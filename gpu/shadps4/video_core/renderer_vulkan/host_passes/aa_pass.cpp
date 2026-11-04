// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: see aa_pass.h.

#include <algorithm>
#include <cstring>
#include <miniz.h>

#include "common/assert.h"
#include "common/logging/log.h"
#include "spartanport_graphics.h"
#include "video_core/host_shaders/fxaa_comp.h"
#include "video_core/host_shaders/smaa_blend_comp.h"
#include "video_core/host_shaders/smaa_edges_comp.h"
#include "video_core/host_shaders/smaa_weights_comp.h"
#include "video_core/renderer_vulkan/host_passes/aa_pass.h"
#include "video_core/renderer_vulkan/host_passes/smaa_tables.h"
#include "video_core/renderer_vulkan/vk_platform.h"

#include <vk_mem_alloc.h>

namespace Vulkan::HostPasses {

namespace {

// Shader bindings (fxaa.comp, smaa.comp): 0 colour, 1 edges or weights, 2 area, 3 search, 4 output.
constexpr u32 BindColor = 0, BindAux = 1, BindArea = 2, BindSearch = 3, BindOutput = 4;
constexpr u32 GroupSize = 8;

// FXAA: an edge is a luma step of 1/8 of the local maximum, and at least 1/32 (dark areas stay as they
// are); 3/4 of an isolated pixel is blended with its surroundings.
constexpr float FxaaEdgeThreshold = 0.125f;
constexpr float FxaaEdgeThresholdMin = 0.0312f;
constexpr float FxaaSubpixel = 0.75f;
// SMAA: the reference's HIGH preset uses 0.1 and ULTRA 0.05; this game is dark and soft, so a bit lower.
constexpr float SmaaEdgeThreshold = 0.075f;

constexpr vk::ImageSubresourceRange ColorRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

} // namespace

AaPass::~AaPass() {
    if (device) {
        (void)device.waitIdle();
    }
    if (staging_buffer) {
        vmaDestroyBuffer(allocator, staging_buffer, staging_allocation);
    }
}

void AaPass::Create(vk::Device device_, VmaAllocator allocator_, u32 num_images) {
    device = device_;
    allocator = allocator_;
    outputs.resize(std::max(num_images, 1u));

    const auto make_sampler = [&](vk::Filter filter) {
        return Check<"create anti-aliasing sampler">(device.createSamplerUnique(vk::SamplerCreateInfo{
            .magFilter = filter,
            .minFilter = filter,
            .mipmapMode = vk::SamplerMipmapMode::eNearest,
            .addressModeU = vk::SamplerAddressMode::eClampToEdge,
            .addressModeV = vk::SamplerAddressMode::eClampToEdge,
            .addressModeW = vk::SamplerAddressMode::eClampToEdge,
            .maxAnisotropy = 1.0f,
            .minLod = 0.0f,
            .maxLod = 0.0f,
        }));
    };
    linear_sampler = make_sampler(vk::Filter::eLinear);
    point_sampler = make_sampler(vk::Filter::eNearest);

    // Samplers are part of the layout: the shaders use combined image samplers, and a push
    // descriptor only has to name the image views.
    const std::array<vk::DescriptorSetLayoutBinding, 5> bindings{{
        {.binding = BindColor,
         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute,
         .pImmutableSamplers = &linear_sampler.get()},
        {.binding = BindAux,
         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute,
         .pImmutableSamplers = &linear_sampler.get()},
        {.binding = BindArea,
         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute,
         .pImmutableSamplers = &point_sampler.get()},
        {.binding = BindSearch,
         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute,
         .pImmutableSamplers = &point_sampler.get()},
        {.binding = BindOutput,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},
    }};
    set_layout = Check<"create anti-aliasing descriptor set layout">(
        device.createDescriptorSetLayoutUnique({
            .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
            .bindingCount = bindings.size(),
            .pBindings = bindings.data(),
        }));
    const vk::PushConstantRange push_constants{
        .stageFlags = vk::ShaderStageFlagBits::eCompute,
        .offset = 0,
        .size = sizeof(Constants),
    };
    pipeline_layout = Check<"create anti-aliasing pipeline layout">(device.createPipelineLayoutUnique({
        .setLayoutCount = 1,
        .pSetLayouts = &set_layout.get(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constants,
    }));
}

vk::UniquePipeline AaPass::MakePipeline(std::span<const u32> code, const char* name) {
    auto module = device.createShaderModuleUnique({.codeSize = code.size_bytes(), .pCode = code.data()});
    if (module.result != vk::Result::eSuccess) {
        LOG_ERROR(Render_Vulkan, "Anti-aliasing: shader module {} failed: {}", name,
                  vk::to_string(module.result));
        return {};
    }
    auto pipeline = device.createComputePipelineUnique(
        {}, {.stage = {.stage = vk::ShaderStageFlagBits::eCompute, .module = *module.value, .pName = "main"},
             .layout = *pipeline_layout});
    if (pipeline.result != vk::Result::eSuccess) {
        LOG_ERROR(Render_Vulkan, "Anti-aliasing: pipeline {} failed: {}", name,
                  vk::to_string(pipeline.result));
        return {};
    }
    SetObjectName(device, pipeline.value.get(), name);
    return std::move(pipeline.value);
}

bool AaPass::EnsureFxaa() {
    if (fxaa_pipeline) {
        return true;
    }
    if (!fxaa_failed) {
        fxaa_pipeline = MakePipeline(FXAA_COMP, "FXAA");
        fxaa_failed = !fxaa_pipeline;
    }
    return !fxaa_failed;
}

bool AaPass::EnsureSmaa() {
    if (smaa_blend_pipeline) {
        return true;
    }
    if (!smaa_failed) {
        smaa_edges_pipeline = MakePipeline(SMAA_EDGES_COMP, "SMAA edges");
        smaa_weights_pipeline = MakePipeline(SMAA_WEIGHTS_COMP, "SMAA weights");
        smaa_blend_pipeline = MakePipeline(SMAA_BLEND_COMP, "SMAA blend");
        smaa_failed = !smaa_edges_pipeline || !smaa_weights_pipeline || !smaa_blend_pipeline;
        if (smaa_failed) {
            smaa_edges_pipeline.reset();
            smaa_weights_pipeline.reset();
            smaa_blend_pipeline.reset();
        }
    }
    return !smaa_failed;
}

void AaPass::MakeTarget(Target& target, vk::Extent2D size, vk::Format format,
                        vk::ImageUsageFlags usage, const char* name) {
    target.view.reset();
    target.image = VideoCore::UniqueImage(device, allocator); // frees the previous image
    target.image.Create({
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {size.width, size.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .usage = usage,
        .initialLayout = vk::ImageLayout::eUndefined,
    });
    SetObjectName(device, static_cast<vk::Image>(target.image), name);
    target.view = Check<"create anti-aliasing image view">(device.createImageViewUnique({
        .image = target.image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = ColorRange,
    }));
}

void AaPass::RecreateTargets(vk::Extent2D size, bool smaa) {
    // The images being replaced may still be read by frames in flight.
    (void)device.waitIdle();
    constexpr auto output_usage =
        vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;
    for (auto& output : outputs) {
        // TransferSrc: frame dumps and the test read the result back.
        MakeTarget(output, size, vk::Format::eR16G16B16A16Sfloat,
                   output_usage | vk::ImageUsageFlagBits::eTransferSrc, "AA output");
    }
    if (smaa) {
        // Storage images of a format every device supports for it (the edges use two channels).
        MakeTarget(edges, size, vk::Format::eR8G8B8A8Unorm, output_usage, "SMAA edges");
        MakeTarget(weights, size, vk::Format::eR8G8B8A8Unorm, output_usage, "SMAA weights");
    }
    current_output = 0;
    target_size = size;
    targets_have_smaa = smaa;
}

bool AaPass::UploadTables(vk::CommandBuffer cmdbuf) {
    if (tables_uploaded) {
        return true;
    }
    using namespace SmaaTables;
    std::vector<u8> area(AreaBytes), search(SearchBytes);
    mz_ulong area_size = AreaBytes, search_size = SearchBytes;
    if (mz_uncompress(area.data(), &area_size, AreaDeflated, sizeof(AreaDeflated)) != MZ_OK ||
        area_size != AreaBytes ||
        mz_uncompress(search.data(), &search_size, SearchDeflated, sizeof(SearchDeflated)) != MZ_OK ||
        search_size != SearchBytes) {
        LOG_ERROR(Render_Vulkan, "Anti-aliasing: the SMAA tables do not unpack");
        smaa_failed = true;
        return false;
    }

    const VkBufferCreateInfo buffer_ci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = AreaBytes + SearchBytes,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };
    const VmaAllocationCreateInfo alloc_ci{
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };
    VmaAllocationInfo alloc_info{};
    if (vmaCreateBuffer(allocator, &buffer_ci, &alloc_ci, &staging_buffer, &staging_allocation,
                        &alloc_info) != VK_SUCCESS ||
        !alloc_info.pMappedData) {
        LOG_ERROR(Render_Vulkan, "Anti-aliasing: no staging buffer for the SMAA tables");
        smaa_failed = true;
        return false;
    }
    std::memcpy(alloc_info.pMappedData, area.data(), AreaBytes);
    std::memcpy(static_cast<u8*>(alloc_info.pMappedData) + AreaBytes, search.data(), SearchBytes);
    vmaFlushAllocation(allocator, staging_allocation, 0, VK_WHOLE_SIZE);

    constexpr auto usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
    MakeTarget(area_texture, {AreaWidth, AreaHeight}, vk::Format::eR8G8Unorm, usage, "SMAA area");
    MakeTarget(search_texture, {SearchWidth, SearchHeight}, vk::Format::eR8Unorm, usage, "SMAA search");

    const auto transition = [&](vk::Image image, vk::ImageLayout from, vk::ImageLayout to,
                                vk::PipelineStageFlags2 src_stage, vk::AccessFlags2 src_access,
                                vk::PipelineStageFlags2 dst_stage, vk::AccessFlags2 dst_access) {
        return vk::ImageMemoryBarrier2{
            .srcStageMask = src_stage,
            .srcAccessMask = src_access,
            .dstStageMask = dst_stage,
            .dstAccessMask = dst_access,
            .oldLayout = from,
            .newLayout = to,
            .image = image,
            .subresourceRange = ColorRange,
        };
    };
    const std::array to_copy{
        transition(area_texture.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                   vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone,
                   vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferWrite),
        transition(search_texture.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                   vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone,
                   vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferWrite),
    };
    cmdbuf.pipelineBarrier2({.imageMemoryBarrierCount = to_copy.size(), .pImageMemoryBarriers = to_copy.data()});

    const vk::BufferImageCopy area_copy{
        .bufferOffset = 0,
        .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .imageExtent = {AreaWidth, AreaHeight, 1},
    };
    const vk::BufferImageCopy search_copy{
        .bufferOffset = AreaBytes,
        .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .imageExtent = {SearchWidth, SearchHeight, 1},
    };
    cmdbuf.copyBufferToImage(staging_buffer, area_texture.image, vk::ImageLayout::eTransferDstOptimal,
                             area_copy);
    cmdbuf.copyBufferToImage(staging_buffer, search_texture.image, vk::ImageLayout::eTransferDstOptimal,
                             search_copy);

    const std::array to_read{
        transition(area_texture.image, vk::ImageLayout::eTransferDstOptimal,
                   vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eCopy,
                   vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eComputeShader,
                   vk::AccessFlagBits2::eShaderSampledRead),
        transition(search_texture.image, vk::ImageLayout::eTransferDstOptimal,
                   vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eCopy,
                   vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eComputeShader,
                   vk::AccessFlagBits2::eShaderSampledRead),
    };
    cmdbuf.pipelineBarrier2({.imageMemoryBarrierCount = to_read.size(), .pImageMemoryBarriers = to_read.data()});
    tables_uploaded = true;
    return true;
}

void AaPass::Dispatch(vk::CommandBuffer cmdbuf, vk::Pipeline pipeline, vk::Extent2D size,
                      const Constants& constants, std::span<const vk::DescriptorImageInfo> images,
                      std::span<const u32> bindings) {
    std::array<vk::WriteDescriptorSet, 5> writes{};
    for (size_t i = 0; i < images.size(); ++i) {
        writes[i] = {
            .dstBinding = bindings[i],
            .descriptorCount = 1,
            .descriptorType = bindings[i] == BindOutput ? vk::DescriptorType::eStorageImage
                                                        : vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &images[i],
        };
    }
    cmdbuf.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
    cmdbuf.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *pipeline_layout, 0,
                                vk::ArrayProxy<const vk::WriteDescriptorSet>(u32(images.size()), writes.data()));
    cmdbuf.pushConstants(*pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(Constants),
                         &constants);
    cmdbuf.dispatch((size.width + GroupSize - 1) / GroupSize, (size.height + GroupSize - 1) / GroupSize, 1);
}

vk::ImageView AaPass::Render(vk::CommandBuffer cmdbuf, vk::ImageView input, vk::Extent2D size, int mode,
                             bool linear_input) {
    if (!device || !SpartanGraphics::AaPassRuns(mode, int(size.width), int(size.height)) || Failed(mode)) {
        return input;
    }
    const bool smaa = mode == ModeSmaa;
    if (!(smaa ? EnsureSmaa() : EnsureFxaa())) {
        return input;
    }
    if (smaa && !UploadTables(cmdbuf)) {
        return input;
    }
    if (size != target_size || (smaa && !targets_have_smaa) || !bool(outputs.front().image)) {
        RecreateTargets(size, smaa);
    }
    Target& output = outputs[current_output];
    current_output = (current_output + 1) % u32(outputs.size());

    // The game's image was just drawn, and the barrier that made it readable only names the fragment
    // stage; and the images used here may still be read by the previous frame.
    const vk::MemoryBarrier2 input_ready{
        .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .srcAccessMask = vk::AccessFlagBits2::eMemoryWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
        .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
    };
    const auto to_general = [](vk::Image image) {
        return vk::ImageMemoryBarrier2{
            .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
            .srcAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
            .dstAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eGeneral,
            .image = image,
            .subresourceRange = ColorRange,
        };
    };
    std::array<vk::ImageMemoryBarrier2, 3> entry{to_general(output.image)};
    u32 entry_count = 1;
    if (smaa) {
        entry[entry_count++] = to_general(edges.image);
        entry[entry_count++] = to_general(weights.image);
    }
    cmdbuf.pipelineBarrier2({
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &input_ready,
        .imageMemoryBarrierCount = entry_count,
        .pImageMemoryBarriers = entry.data(),
    });

    Constants constants{
        .metrics = {1.0f / float(size.width), 1.0f / float(size.height), float(size.width),
                    float(size.height)},
        .edge_threshold = smaa ? SmaaEdgeThreshold : FxaaEdgeThreshold,
        .edge_threshold_min = FxaaEdgeThresholdMin,
        .subpixel = FxaaSubpixel,
        .linear_input = linear_input ? 1u : 0u,
    };
    const auto sampled = [](vk::ImageView view, vk::ImageLayout layout) {
        return vk::DescriptorImageInfo{.imageView = view, .imageLayout = layout};
    };
    // A compute write of one pass, read by the next as a sampled or storage image.
    const auto pass_barrier = [&] {
        const vk::MemoryBarrier2 barrier{
            .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
            .srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
            .dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead | vk::AccessFlagBits2::eShaderStorageRead,
        };
        cmdbuf.pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &barrier});
    };
    const auto read_only = vk::ImageLayout::eShaderReadOnlyOptimal;
    const auto general = vk::ImageLayout::eGeneral;

    if (!smaa) {
        const std::array images{sampled(input, read_only), sampled(*output.view, general)};
        const std::array bindings{BindColor, BindOutput};
        Dispatch(cmdbuf, *fxaa_pipeline, size, constants, images, bindings);
    } else {
        {
            const std::array images{sampled(input, read_only), sampled(*edges.view, general)};
            const std::array bindings{BindColor, BindOutput};
            Dispatch(cmdbuf, *smaa_edges_pipeline, size, constants, images, bindings);
        }
        pass_barrier();
        {
            const std::array images{sampled(*edges.view, general), sampled(*area_texture.view, read_only),
                                    sampled(*search_texture.view, read_only),
                                    sampled(*weights.view, general)};
            const std::array bindings{BindAux, BindArea, BindSearch, BindOutput};
            Dispatch(cmdbuf, *smaa_weights_pipeline, size, constants, images, bindings);
        }
        pass_barrier();
        {
            const std::array images{sampled(input, read_only), sampled(*weights.view, general),
                                    sampled(*output.view, general)};
            const std::array bindings{BindColor, BindAux, BindOutput};
            Dispatch(cmdbuf, *smaa_blend_pipeline, size, constants, images, bindings);
        }
    }

    // Hand the result to the next pass (sharpening, then the post pass) as a sampled image.
    const vk::ImageMemoryBarrier2 to_read{
        .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
        .srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
        .oldLayout = vk::ImageLayout::eGeneral,
        .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        .image = output.image,
        .subresourceRange = ColorRange,
    };
    cmdbuf.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &to_read});
    return *output.view;
}

} // namespace Vulkan::HostPasses
