// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_vulkan/vk_frame_generation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <vk_mem_alloc.h>

#include "common/scope_exit.h"
#include "video_core/renderer_vulkan/vk_gpu_profiler.h"

#include "ffx_vk_fsr3_3_1_5_bridge.h"
#include "spartanport_settings.h"
#include "video_core/host_shaders/fg_depth_comp.h"
#include "video_core/host_shaders/fg_hud_comp.h"
#include "video_core/renderer_vulkan/frame_generation_policy.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_platform.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"

namespace Vulkan {

namespace {

using Policy = FrameGenerationPolicy::Skip;

constexpr u32 kSlots = 4;
// A HUD pixel differs from the scene by more than dithering and sRGB rounding.
constexpr float kHudThreshold = 2.0f / 255.0f;

bool Rgba8(vk::Format format) {
    return format == vk::Format::eR8G8B8A8Unorm || format == vk::Format::eR8G8B8A8Srgb;
}

void Barrier(vk::CommandBuffer cmdbuf, vk::Image image, vk::ImageLayout old_layout,
             vk::ImageLayout new_layout, vk::PipelineStageFlags2 src_stage,
             vk::AccessFlags2 src_access, vk::PipelineStageFlags2 dst_stage,
             vk::AccessFlags2 dst_access) {
    const vk::ImageMemoryBarrier2 b{
        .srcStageMask = src_stage, .srcAccessMask = src_access,
        .dstStageMask = dst_stage, .dstAccessMask = dst_access,
        .oldLayout = old_layout, .newLayout = new_layout, .image = image,
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
    };
    cmdbuf.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &b});
}

FfxVkFsr3_3_1_6FrameGenerationImage Describe(vk::Image image, vk::Format format, u32 w, u32 h,
                                             vk::ImageLayout layout, vk::ImageUsageFlags usage) {
    return {.image = VkImage(image), .format = VkFormat(format), .width = w, .height = h,
            .layout = VkImageLayout(layout), .usage = VkImageUsageFlags(usage)};
}

} // namespace

FrameGeneration::FrameGeneration(const Instance& instance_, Scheduler& scheduler_)
    : instance{instance_}, scheduler{scheduler_} {}

FrameGeneration::~FrameGeneration() {
    Destroy();
    if (counts) {
        vmaDestroyBuffer(instance.GetAllocator(), VkBuffer(counts), counts_allocation);
    }
}

void FrameGeneration::Destroy() {
    if (context) {
        ffxVkFsr3_3_1_6FrameGenerationContextDestroy(context);
        context = nullptr;
    }
    in_flight.clear();
    generated_views.clear();
    generated_storage_view.reset();
    scene_view.reset();
    depth_view.reset();
    motion_view.reset();
    scene_image = VideoCore::UniqueImage{};
    depth_image = VideoCore::UniqueImage{};
    motion_image = VideoCore::UniqueImage{};
    generated_image = VideoCore::UniqueImage{};
    width = height = out_width = out_height = 0;
    prepared_last_frame = false;
    dispatched_frame_id = frame_id;
}

void FrameGeneration::Fail(const char* reason) {
    failed = true;
    std::printf("FG: failed (%s), off\n", reason);
    SpartanSettings::Get().fg_problem = reason;
}

void FrameGeneration::CreatePipelines() {
    if (hud_pipeline) {
        return;
    }
    const auto device = instance.GetDevice();
    const auto make = [&](std::span<const vk::DescriptorSetLayoutBinding> bindings, u32 push_size,
                          vk::UniqueDescriptorSetLayout& set_layout,
                          vk::UniquePipelineLayout& layout, std::span<const u32> code) {
        set_layout = Check(device.createDescriptorSetLayoutUnique({
            .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
            .bindingCount = u32(bindings.size()), .pBindings = bindings.data()}));
        const vk::PushConstantRange push{vk::ShaderStageFlagBits::eCompute, 0, push_size};
        layout = Check(device.createPipelineLayoutUnique({
            .setLayoutCount = 1, .pSetLayouts = &*set_layout,
            .pushConstantRangeCount = push_size ? 1u : 0u, .pPushConstantRanges = &push}));
        const auto module = CompileSPV(code, device);
        auto pipeline = Check(device.createComputePipelineUnique(
            {}, vk::ComputePipelineCreateInfo{
                    .stage = {.stage = vk::ShaderStageFlagBits::eCompute, .module = module,
                              .pName = "main"},
                    .layout = *layout}));
        device.destroyShaderModule(module);
        return pipeline;
    };
    const auto binding = [](u32 index, vk::DescriptorType type) {
        return vk::DescriptorSetLayoutBinding{.binding = index, .descriptorType = type,
                                              .descriptorCount = 1,
                                              .stageFlags = vk::ShaderStageFlagBits::eCompute};
    };
    const std::array depth_bindings{binding(0, vk::DescriptorType::eSampledImage),
                                    binding(1, vk::DescriptorType::eStorageImage),
                                    binding(2, vk::DescriptorType::eSampledImage),
                                    binding(3, vk::DescriptorType::eSampledImage),
                                    binding(4, vk::DescriptorType::eStorageImage),
                                    binding(5, vk::DescriptorType::eSampledImage)};
    depth_pipeline = make(depth_bindings, 3 * sizeof(u32), depth_set_layout, depth_layout,
                          FG_DEPTH_COMP);
    const std::array hud_bindings{binding(0, vk::DescriptorType::eSampledImage),
                                  binding(1, vk::DescriptorType::eSampledImage),
                                  binding(2, vk::DescriptorType::eStorageImage),
                                  binding(3, vk::DescriptorType::eStorageBuffer)};
    hud_pipeline = make(hud_bindings, 4 * sizeof(u32), hud_set_layout, hud_layout, FG_HUD_COMP);

    const VkBufferCreateInfo buffer_info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                         .size = kSlots * sizeof(u32),
                                         .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    const VmaAllocationCreateInfo alloc_info{
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO};
    VkBuffer buffer{};
    VmaAllocationInfo info{};
    if (vmaCreateBuffer(instance.GetAllocator(), &buffer_info, &alloc_info, &buffer,
                        &counts_allocation, &info) == VK_SUCCESS) {
        counts = vk::Buffer(buffer);
        counts_mapped = static_cast<u32*>(info.pMappedData);
    }
}

bool FrameGeneration::EnsureContext(u32 w, u32 h, u32 ow, u32 oh) {
    if (context && w == width && h == height && ow == out_width && oh == out_height) {
        return true;
    }
    // Its shaders read and write storage images without a declared format.
    const auto features = instance.GetPhysicalDevice().getFeatures();
    if (!features.shaderStorageImageReadWithoutFormat ||
        !features.shaderStorageImageWriteWithoutFormat) {
        Fail("the graphics card cannot read storage images without a format");
        return false;
    }
    CreatePipelines();
    if (!counts_mapped) {
        Fail("no counter buffer");
        return false;
    }
    scheduler.Finish();
    Destroy();
    const auto device = instance.GetDevice();
    const auto allocator = instance.GetAllocator();
    const auto make_image = [&](VideoCore::UniqueImage& image, vk::Format format, u32 iw, u32 ih,
                                vk::ImageUsageFlags usage, bool mutable_format) {
        image = VideoCore::UniqueImage(device, allocator);
        image.Create(vk::ImageCreateInfo{
            .flags = mutable_format ? vk::ImageCreateFlagBits::eMutableFormat
                                    : vk::ImageCreateFlags{},
            .imageType = vk::ImageType::e2D, .format = format, .extent = {iw, ih, 1},
            .mipLevels = 1, .arrayLayers = 1, .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal, .usage = usage,
            .initialLayout = vk::ImageLayout::eUndefined});
    };
    const auto view = [&](vk::Image image, vk::Format format, vk::ImageUsageFlags usage) {
        const vk::ImageViewUsageCreateInfo usage_info{.usage = usage};
        return Check(device.createImageViewUnique({
            .pNext = &usage_info, .image = image, .viewType = vk::ImageViewType::e2D,
            .format = format, .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}}));
    };
    using Usage = vk::ImageUsageFlagBits;
    make_image(scene_image, vk::Format::eR8G8B8A8Unorm, ow, oh,
               Usage::eSampled | Usage::eStorage | Usage::eTransferDst | Usage::eTransferSrc, false);
    make_image(depth_image, vk::Format::eR32Sfloat, w, h, Usage::eSampled | Usage::eStorage, false);
    make_image(motion_image, vk::Format::eR16G16Sfloat, w, h, Usage::eSampled | Usage::eStorage,
               false);
    make_image(generated_image, vk::Format::eR8G8B8A8Unorm, ow, oh,
               Usage::eSampled | Usage::eStorage | Usage::eTransferSrc, true);
    scene_view = view(vk::Image(scene_image), vk::Format::eR8G8B8A8Unorm, Usage::eSampled);
    depth_view = view(vk::Image(depth_image), vk::Format::eR32Sfloat, Usage::eStorage);
    motion_view = view(vk::Image(motion_image), vk::Format::eR16G16Sfloat, Usage::eStorage);
    generated_storage_view =
        view(vk::Image(generated_image), vk::Format::eR8G8B8A8Unorm, Usage::eStorage);

    const FfxVkFsr3_3_1_6FrameGenerationCreateInfo create{
        .physicalDevice = VkPhysicalDevice(instance.GetPhysicalDevice()),
        .device = VkDevice(device), .maxRenderWidth = w, .maxRenderHeight = h,
        .displayWidth = ow, .displayHeight = oh, .colorFormat = VK_FORMAT_R8G8B8A8_UNORM};
    if (ffxVkFsr3_3_1_6FrameGenerationContextCreate(&create, &context) !=
        FFX_VK_FSR3_3_1_6_FRAMEGEN_OK) {
        context = nullptr;
        Fail("FSR 3.1 frame generation context creation failed");
        return false;
    }
    width = w;
    height = h;
    out_width = ow;
    out_height = oh;
    std::printf("FG: FSR 3.1 frame generation %ux%u -> %ux%u\n", w, h, ow, oh);
    return true;
}

void FrameGeneration::Capture(vk::CommandBuffer cmdbuf, const CaptureInfo& info) {
    captured = false;
    if (SpartanSettings::Get().frame_generation != FrameGenerationPolicy::FgFsr || failed) {
        return;
    }
    if (!Rgba8(info.scene_format)) {
        Fail("the upscaled frame is not RGBA8");
        return;
    }
    if (!EnsureContext(info.width, info.height, info.output_width, info.output_height)) {
        return;
    }
    // A new context waits for the GPU (Scheduler::Finish), which submits `cmdbuf`.
    cmdbuf = scheduler.CommandBuffer();
    const auto all = vk::PipelineStageFlagBits2::eAllCommands;
    const auto compute = vk::PipelineStageFlagBits2::eComputeShader;
    const auto General = vk::ImageLayout::eGeneral;
    const auto Read = vk::ImageLayout::eShaderReadOnlyOptimal;

    auto* profiler = GpuProfiler::Get();
    if (profiler && !profiler->Records(&scheduler)) profiler = nullptr;
    const u64 resume = profiler ? profiler->Current() : 0;
    SCOPE_EXIT { if (profiler) profiler->Resume(resume); };
    if (profiler) {
        profiler->Mark(0xF6A00001, [] { return std::string{"FG scene copy"}; });
        cmdbuf = scheduler.CommandBuffer();
    }
    // The scene without the HUD: the HUD draws into the same image next.
    Barrier(cmdbuf, info.scene, General, General, all, vk::AccessFlagBits2::eMemoryWrite,
            vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferRead);
    Barrier(cmdbuf, vk::Image(scene_image), vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal, all, vk::AccessFlagBits2::eNone,
            vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferWrite);
    const vk::ImageCopy region{
        .srcSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .dstSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .extent = {info.output_width, info.output_height, 1}};
    cmdbuf.copyImage(info.scene, General, vk::Image(scene_image),
                     vk::ImageLayout::eTransferDstOptimal, region);
    Barrier(cmdbuf, info.scene, General, General, vk::PipelineStageFlagBits2::eCopy,
            vk::AccessFlagBits2::eTransferRead, all,
            vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite);
    Barrier(cmdbuf, vk::Image(scene_image), vk::ImageLayout::eTransferDstOptimal, Read,
            vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferWrite, all,
            vk::AccessFlagBits2::eShaderRead);

    if (profiler) {
        profiler->Mark(0xF6A00002, [] { return std::string{"FG depth and motion"}; });
        cmdbuf = scheduler.CommandBuffer();
    }
    // The depth as an R32 float color image, with valid object vectors kept on characters.
    for (const vk::Image image : {vk::Image(depth_image), vk::Image(motion_image)}) {
        Barrier(cmdbuf, image, vk::ImageLayout::eUndefined, General, all,
                vk::AccessFlagBits2::eNone, compute, vk::AccessFlagBits2::eShaderWrite);
    }
    Barrier(cmdbuf, info.motion, General, General, all, vk::AccessFlagBits2::eMemoryWrite, compute,
            vk::AccessFlagBits2::eShaderRead);
    const vk::DescriptorImageInfo depth_in{.imageView = info.depth, .imageLayout = General};
    const vk::DescriptorImageInfo depth_out{.imageView = *depth_view, .imageLayout = General};
    const vk::DescriptorImageInfo motion_in{.imageView = info.motion_view, .imageLayout = General};
    // Without a mask the motion view stands in for it (the shader then ignores it).
    const vk::DescriptorImageInfo mask_in{
        .imageView = info.character_mask ? info.character_mask : info.motion_view,
        .imageLayout = General};
    const vk::DescriptorImageInfo motion_out{.imageView = *motion_view, .imageLayout = General};
    const vk::DescriptorImageInfo object_in{
        .imageView = info.object_motion ? info.object_motion : info.motion_view,
        .imageLayout = General};
    const std::array depth_writes{
        vk::WriteDescriptorSet{.dstBinding = 0, .descriptorCount = 1,
                               .descriptorType = vk::DescriptorType::eSampledImage,
                               .pImageInfo = &depth_in},
        vk::WriteDescriptorSet{.dstBinding = 1, .descriptorCount = 1,
                               .descriptorType = vk::DescriptorType::eStorageImage,
                               .pImageInfo = &depth_out},
        vk::WriteDescriptorSet{.dstBinding = 2, .descriptorCount = 1,
                               .descriptorType = vk::DescriptorType::eSampledImage,
                               .pImageInfo = &motion_in},
        vk::WriteDescriptorSet{.dstBinding = 3, .descriptorCount = 1,
                               .descriptorType = vk::DescriptorType::eSampledImage,
                               .pImageInfo = &mask_in},
        vk::WriteDescriptorSet{.dstBinding = 4, .descriptorCount = 1,
                               .descriptorType = vk::DescriptorType::eStorageImage,
                               .pImageInfo = &motion_out},
        vk::WriteDescriptorSet{.dstBinding = 5, .descriptorCount = 1,
                               .descriptorType = vk::DescriptorType::eSampledImage,
                               .pImageInfo = &object_in}};
    const std::array<u32, 3> depth_params{info.character_mask ? 1u : 0u,
        info.object_motion ? 1u : 0u, std::bit_cast<u32>(info.projection_z)};
    cmdbuf.bindPipeline(vk::PipelineBindPoint::eCompute, *depth_pipeline);
    cmdbuf.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *depth_layout, 0, depth_writes);
    cmdbuf.pushConstants(*depth_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(depth_params),
                         depth_params.data());
    cmdbuf.dispatch((info.width + 7) / 8, (info.height + 7) / 8, 1);
    for (const vk::Image image : {vk::Image(depth_image), vk::Image(motion_image)}) {
        Barrier(cmdbuf, image, General, Read, compute, vk::AccessFlagBits2::eShaderWrite, all,
                vk::AccessFlagBits2::eShaderRead);
    }

    near_plane = std::isfinite(info.near_plane) && info.near_plane > 0.0f ? info.near_plane : 0.1f;
    vertical_fov = std::isfinite(info.vertical_fov) && info.vertical_fov > 0.0f
                       ? info.vertical_fov : 1.0f;
    capture_frame_ms = std::isfinite(info.frame_ms) && info.frame_ms > 0.0f ? info.frame_ms : 16.6f;
    capture_reset = info.reset || !prepared_last_frame;
    ++frame_id;

    if (profiler) {
        profiler->Mark(0xF6A00003, [] { return std::string{"FG prepare"}; });
        cmdbuf = scheduler.CommandBuffer();
    }
    using Usage = vk::ImageUsageFlagBits;
    FfxVkFsr3_3_1_6FrameGenerationPrepareInfo prepare{};
    prepare.commandBuffer = VkCommandBuffer(cmdbuf);
    prepare.color = Describe(vk::Image(scene_image), vk::Format::eR8G8B8A8Unorm, out_width,
                             out_height, Read, Usage::eSampled | Usage::eStorage |
                             Usage::eTransferSrc | Usage::eTransferDst);
    prepare.depth = Describe(vk::Image(depth_image), vk::Format::eR32Sfloat, width, height, Read,
                             Usage::eSampled | Usage::eStorage);
    prepare.motionVectors = Describe(vk::Image(motion_image), vk::Format::eR16G16Sfloat, width,
                                     height, Read, Usage::eSampled | Usage::eStorage);
    prepare.renderWidth = width;
    prepare.renderHeight = height;
    prepare.jitterOffsetX = std::isfinite(info.jitter_x) ? info.jitter_x : 0.0f;
    prepare.jitterOffsetY = std::isfinite(info.jitter_y) ? info.jitter_y : 0.0f;
    // Same convention as the upscaler's FSR 3.1 dispatch: motion in render pixels.
    prepare.motionVectorScaleX = 1.0f;
    prepare.motionVectorScaleY = 1.0f;
    prepare.frameTimeMilliseconds = capture_frame_ms;
    prepare.minLuminance = 0.0f;
    prepare.maxLuminance = 1.0f;
    prepare.transferFunction = FFX_VK_FSR3_3_1_6_FRAMEGEN_TRANSFER_SRGB;
    prepare.cameraNear = near_plane;
    prepare.cameraFar = 3000.0f;
    prepare.viewSpaceToMeters = 1.0f;
    prepare.cameraVerticalFovRadians = vertical_fov;
    prepare.cameraUp[1] = 1.0f;
    prepare.cameraRight[0] = 1.0f;
    prepare.cameraForward[2] = -1.0f;
    prepare.frameId = frame_id;
    prepare.reset = capture_reset ? VK_TRUE : VK_FALSE;
    if (const auto result = ffxVkFsr3_3_1_6FrameGenerationContextRecordPrepare(context, &prepare);
        result != FFX_VK_FSR3_3_1_6_FRAMEGEN_OK) {
        std::printf("FG: prepare error %d frame %llu previous_dispatch %llu render %ux%u "
                    "output %ux%u near %.6g far %.6g fov %.6g ms %.6g reset %u "
                    "previous_pair_complete %u camera_range_valid %u\n",
                    int(result), static_cast<unsigned long long>(frame_id),
                    static_cast<unsigned long long>(dispatched_frame_id), width, height,
                    out_width, out_height, double(prepare.cameraNear), double(prepare.cameraFar),
                    double(prepare.cameraVerticalFovRadians), double(capture_frame_ms),
                    unsigned(prepare.reset), unsigned(frame_id == dispatched_frame_id + 1),
                    unsigned(prepare.cameraNear < prepare.cameraFar));
        Fail("FSR 3.1 frame generation prepare failed");
        return;
    }
    if (profiler) {
        profiler->Mark(0xF6A00004, [] { return std::string{"FG interpolation"}; });
        cmdbuf = scheduler.CommandBuffer();
    }
    // A scene can be rendered without a flip. Complete the API pair before another capture.
    Barrier(cmdbuf, vk::Image(generated_image), vk::ImageLayout::eUndefined, General, all,
            vk::AccessFlagBits2::eNone, all,
            vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite);
    {
        using Usage = vk::ImageUsageFlagBits;
        FfxVkFsr3_3_1_6FrameGenerationDispatchInfo dispatch{};
        dispatch.commandBuffer = VkCommandBuffer(cmdbuf);
        dispatch.color = Describe(vk::Image(scene_image), vk::Format::eR8G8B8A8Unorm, out_width,
                                  out_height, vk::ImageLayout::eShaderReadOnlyOptimal,
                                  Usage::eSampled | Usage::eStorage | Usage::eTransferSrc |
                                  Usage::eTransferDst);
        dispatch.output = Describe(vk::Image(generated_image), vk::Format::eR8G8B8A8Unorm,
                                   out_width, out_height, General,
                                   Usage::eSampled | Usage::eStorage | Usage::eTransferSrc);
        dispatch.displayWidth = out_width;
        dispatch.displayHeight = out_height;
        dispatch.frameTimeMilliseconds = capture_frame_ms;
        dispatch.cameraNear = near_plane;
        dispatch.cameraFar = 3000.0f;
        dispatch.viewSpaceToMeters = 1.0f;
        dispatch.cameraVerticalFovRadians = vertical_fov;
        dispatch.minLuminance = 0.0f;
        dispatch.maxLuminance = 1.0f;
        dispatch.transferFunction = FFX_VK_FSR3_3_1_6_FRAMEGEN_TRANSFER_SRGB;
        dispatch.frameId = frame_id;
        dispatch.reset = capture_reset ? VK_TRUE : VK_FALSE;
        if (const auto result =
                ffxVkFsr3_3_1_6FrameGenerationContextRecordDispatch(context, &dispatch);
            result != FFX_VK_FSR3_3_1_6_FRAMEGEN_OK) {
            std::printf("FG: dispatch error %d\n", int(result));
            Fail("FSR 3.1 frame generation dispatch failed");
            ++skips[u32(Policy::Failed)];
            Report();
            return;
        }
        Barrier(cmdbuf, vk::Image(generated_image), General, General, all,
                vk::AccessFlagBits2::eMemoryWrite, compute,
                vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite);
    }

    dispatched_frame_id = frame_id;
    in_flight.emplace_back(frame_id, scheduler.CurrentTick());
    while (!in_flight.empty() && scheduler.IsFree(in_flight.front().second)) {
        ffxVkFsr3_3_1_6FrameGenerationContextRetireFrame(context, in_flight.front().first);
        in_flight.pop_front();
    }
    scene_srgb = info.scene_format == vk::Format::eR8G8B8A8Srgb;
    captured = true;
}

vk::Image FrameGeneration::Generate(vk::CommandBuffer cmdbuf, bool upscaled, vk::Image final_image,
                                    vk::Format final_format) {
    auto& settings = SpartanSettings::Get();
    const bool enabled = settings.frame_generation == FrameGenerationPolicy::FgFsr;
    // Capture already completed the prepare/dispatch pair, even if no flip followed it.
    const bool prepared = std::exchange(captured, false) && context;
    const bool did_capture = prepared && upscaled;
    prepared_last_frame = did_capture;
    if (!enabled) {
        settings.fg_status = nullptr;
        return {};
    }
    if (did_capture && !Rgba8(final_format)) {
        Fail("the display buffer is not RGBA8");
    }
    // The share of pixels changed after the upscale, a few frames back (the GPU has it by now).
    for (u32 i = 0; i < kSlots; ++i) {
        if (slot_pending[i] && scheduler.IsFree(slot_ticks[i])) {
            slot_pending[i] = false;
            // Host memory may be cached and not coherent.
            vmaInvalidateAllocation(instance.GetAllocator(), counts_allocation, i * sizeof(u32),
                                    sizeof(u32));
            ui_coverage = float(counts_mapped[i]) / float(std::max(1u, out_width * out_height));
        }
    }
    const Policy skip = FrameGenerationPolicy::Decide(
        {.enabled = true, .upscaled = did_capture, .reset = capture_reset,
         .ui_coverage = ui_coverage, .frame_ms = capture_frame_ms, .failed = failed});
    ++real;
    if (skip != Policy::None) {
        ++skips[std::min<u32>(u32(skip), u32(Policy::Failed))];
    }
    if (!prepared || failed) {
        Report();
        return {};
    }

    const auto all = vk::PipelineStageFlagBits2::eAllCommands;
    const auto compute = vk::PipelineStageFlagBits2::eComputeShader;
    const auto General = vk::ImageLayout::eGeneral;
    const bool generate = skip == Policy::None;
    const bool composite = did_capture && final_image;

    if (!composite) {
        Report();
        return {};
    }
    auto* profiler = GpuProfiler::Get();
    if (profiler && !profiler->Records(&scheduler)) profiler = nullptr;
    const u64 resume = profiler ? profiler->Current() : 0;
    SCOPE_EXIT { if (profiler) profiler->Resume(resume); };
    if (profiler) {
        profiler->Mark(0xF6A00005, [] { return std::string{"FG HUD composition"}; });
        cmdbuf = scheduler.CommandBuffer();
    }
    // The HUD (and anything else drawn after the upscale): counted every frame, copied from
    // the real frame over the generated one.
    const auto device = instance.GetDevice();
    const vk::ImageViewUsageCreateInfo sampled{.usage = vk::ImageUsageFlagBits::eSampled};
    const vk::ImageView final_view = Check(device.createImageView({
        .pNext = &sampled, .image = final_image, .viewType = vk::ImageViewType::e2D,
        .format = vk::Format::eR8G8B8A8Unorm,
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}}));
    scheduler.DeferOperation([device, final_view] { device.destroyImageView(final_view); });
    Barrier(cmdbuf, final_image, General, General, all, vk::AccessFlagBits2::eMemoryWrite, compute,
            vk::AccessFlagBits2::eShaderRead);
    slot = (slot + 1) % kSlots;
    cmdbuf.fillBuffer(counts, slot * sizeof(u32), sizeof(u32), 0);
    const vk::BufferMemoryBarrier2 cleared{
        .srcStageMask = vk::PipelineStageFlagBits2::eClear,
        .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .dstStageMask = compute,
        .dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
        .buffer = counts, .offset = 0, .size = VK_WHOLE_SIZE};
    cmdbuf.pipelineBarrier2({.bufferMemoryBarrierCount = 1, .pBufferMemoryBarriers = &cleared});
    const vk::DescriptorImageInfo final_info{.imageView = final_view, .imageLayout = General};
    const vk::DescriptorImageInfo scene_info{.imageView = *scene_view,
                                             .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
    const vk::DescriptorImageInfo out_info{.imageView = *generated_storage_view,
                                           .imageLayout = General};
    const vk::DescriptorBufferInfo counts_info{.buffer = counts, .offset = 0,
                                               .range = VK_WHOLE_SIZE};
    const std::array writes{
        vk::WriteDescriptorSet{.dstBinding = 0, .descriptorCount = 1,
                               .descriptorType = vk::DescriptorType::eSampledImage,
                               .pImageInfo = &final_info},
        vk::WriteDescriptorSet{.dstBinding = 1, .descriptorCount = 1,
                               .descriptorType = vk::DescriptorType::eSampledImage,
                               .pImageInfo = &scene_info},
        vk::WriteDescriptorSet{.dstBinding = 2, .descriptorCount = 1,
                               .descriptorType = vk::DescriptorType::eStorageImage,
                               .pImageInfo = &out_info},
        vk::WriteDescriptorSet{.dstBinding = 3, .descriptorCount = 1,
                               .descriptorType = vk::DescriptorType::eStorageBuffer,
                               .pBufferInfo = &counts_info}};
    const u32 color_spaces = (scene_srgb ? 1u : 0u) |
                             (final_format == vk::Format::eR8G8B8A8Srgb ? 2u : 0u);
    const std::array<u32, 4> push{std::bit_cast<u32>(kHudThreshold), slot, generate ? 1u : 0u,
                                  color_spaces};
    cmdbuf.bindPipeline(vk::PipelineBindPoint::eCompute, *hud_pipeline);
    cmdbuf.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *hud_layout, 0, writes);
    cmdbuf.pushConstants(*hud_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(push),
                         push.data());
    cmdbuf.dispatch((out_width + 7) / 8, (out_height + 7) / 8, 1);
    const vk::BufferMemoryBarrier2 counted{
        .srcStageMask = compute, .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eHost,
        .dstAccessMask = vk::AccessFlagBits2::eHostRead,
        .buffer = counts, .offset = 0, .size = VK_WHOLE_SIZE};
    cmdbuf.pipelineBarrier2({.bufferMemoryBarrierCount = 1, .pBufferMemoryBarriers = &counted});
    slot_ticks[slot] = scheduler.CurrentTick();
    slot_pending[slot] = true;
    Barrier(cmdbuf, final_image, General, General, compute, vk::AccessFlagBits2::eShaderRead, all,
            vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite);
    if (!generate) {
        Report();
        return {};
    }
    Barrier(cmdbuf, vk::Image(generated_image), General, vk::ImageLayout::eShaderReadOnlyOptimal,
            compute, vk::AccessFlagBits2::eShaderWrite, all, vk::AccessFlagBits2::eShaderRead);
    ++generated;
    Report();
    return vk::Image(generated_image);
}

vk::ImageView FrameGeneration::GeneratedView(vk::Format format) {
    for (const auto& [view_format, view] : generated_views) {
        if (view_format == format) {
            return *view;
        }
    }
    const vk::ImageViewUsageCreateInfo sampled{.usage = vk::ImageUsageFlagBits::eSampled};
    auto view = Check(instance.GetDevice().createImageViewUnique({
        .pNext = &sampled, .image = vk::Image(generated_image), .viewType = vk::ImageViewType::e2D,
        .format = format,
        .components = {vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
                       vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eOne},
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}}));
    const vk::ImageView handle = *view;
    generated_views.emplace_back(format, std::move(view));
    return handle;
}

void FrameGeneration::Report() {
    const auto now = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(now - report_start).count();
    if (seconds < 1.0) {
        return;
    }
    const int real_fps = int(std::lround(real / seconds));
    SpartanSettings::Get().fg_game_fps = generated ? float(real / seconds) : 0.0f;
    const int shown_fps = int(std::lround((real + generated) / seconds));
    auto& status = this->status[status_index ^= 1];
    status = failed ? std::string("FSR FG off: ") + SpartanSettings::Get().fg_problem.load()
         : generated ? "FSR FG 2x, " + std::to_string(real_fps) + " -> " +
                           std::to_string(shown_fps) + " fps"
                     : "FSR FG 2x, inactive (" +
                           std::string(skips[u32(Policy::NoUpscale)] ? "no upscaler" : "skipping") + ")";
    SpartanSettings::Get().fg_status = status.c_str();
    report_start = now;
    real = generated = 0;
    std::fill(std::begin(skips), std::end(skips), 0u);
}

} // namespace Vulkan
