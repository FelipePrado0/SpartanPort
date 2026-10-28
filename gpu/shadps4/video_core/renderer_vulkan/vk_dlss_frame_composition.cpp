// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_vulkan/vk_dlss_frame_composition.h"
#include "video_core/host_shaders/fg_ui_comp.h"
#include "video_core/renderer_vulkan/vk_dlss_frame_generation.h"
#include "video_core/renderer_vulkan/vk_platform.h"
#include "video_core/renderer_vulkan/vk_presenter.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include <vk_mem_alloc.h>

namespace Vulkan {
namespace {
void Transition(vk::CommandBuffer cmd, vk::Image image, vk::ImageLayout from, vk::ImageLayout to) {
    const vk::ImageMemoryBarrier2 barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .srcAccessMask = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
        .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
        .oldLayout = from,
        .newLayout = to,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
    cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier});
}
} // namespace
NvidiaFrame::NvidiaFrame(const Instance& instance_, u32 w, u32 h, vk::Extent2D e, vk::Format f,
                         vk::Format backbuffer)
    : instance{instance_}, width{w}, height{h}, extent{e}, format{f}, backbuffer_format{backbuffer} {
    const auto device = instance.GetDevice();
    const auto make = [&](VideoCore::UniqueImage& image, vk::UniqueImageView& view, u32 iw, u32 ih,
                          vk::Format image_format, vk::ImageUsageFlags usage) {
        image = VideoCore::UniqueImage(device, instance.GetAllocator());
        image.Create({.imageType = vk::ImageType::e2D,
                      .format = image_format,
                      .extent = {iw, ih, 1},
                      .mipLevels = 1,
                      .arrayLayers = 1,
                      .samples = vk::SampleCountFlagBits::e1,
                      .tiling = vk::ImageTiling::eOptimal,
                      .usage = usage});
        view = Check(device.createImageViewUnique(
            {.image = vk::Image(image),
             .viewType = vk::ImageViewType::e2D,
             .format = image_format,
             .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}}));
    };
    using U = vk::ImageUsageFlagBits;
    make(processed, processed_view, w, h, f, U::eColorAttachment | U::eTransferSrc);
    make(hudless, hudless_view, e.width, e.height, backbuffer,
         U::eTransferSrc | U::eTransferDst | U::eSampled);
    make(alpha, alpha_view, e.width, e.height, vk::Format::eR8Unorm,
         U::eStorage | U::eSampled | U::eTransferSrc);
    const std::array bindings{
        vk::DescriptorSetLayoutBinding{.binding = 0,
                                       .descriptorType = vk::DescriptorType::eSampledImage,
                                       .descriptorCount = 1,
                                       .stageFlags = vk::ShaderStageFlagBits::eCompute},
        vk::DescriptorSetLayoutBinding{.binding = 1,
                                       .descriptorType = vk::DescriptorType::eSampledImage,
                                       .descriptorCount = 1,
                                       .stageFlags = vk::ShaderStageFlagBits::eCompute},
        vk::DescriptorSetLayoutBinding{.binding = 2,
                                       .descriptorType = vk::DescriptorType::eStorageImage,
                                       .descriptorCount = 1,
                                       .stageFlags = vk::ShaderStageFlagBits::eCompute},
        vk::DescriptorSetLayoutBinding{.binding = 3,
                                       .descriptorType = vk::DescriptorType::eStorageBuffer,
                                       .descriptorCount = 1,
                                       .stageFlags = vk::ShaderStageFlagBits::eCompute}};
    descriptors = Check(device.createDescriptorSetLayoutUnique(
        {.flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
         .bindingCount = u32(bindings.size()),
         .pBindings = bindings.data()}));
    layout = Check(
        device.createPipelineLayoutUnique({.setLayoutCount = 1, .pSetLayouts = &*descriptors}));
    const auto shader = CompileSPV(FG_UI_COMP, device);
    pipeline = Check(device.createComputePipelineUnique(
        {},
        {.stage = {.stage = vk::ShaderStageFlagBits::eCompute, .module = shader, .pName = "main"},
         .layout = *layout}));
    device.destroyShaderModule(shader);
    const VkBufferCreateInfo buffer_info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                         .size = sizeof(u32),
                                         .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    const VmaAllocationCreateInfo allocation_info{
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO};
    VkBuffer buffer{};
    VmaAllocationInfo info{};
    if (vmaCreateBuffer(instance.GetAllocator(), &buffer_info, &allocation_info, &buffer,
                        &counter_allocation, &info) == VK_SUCCESS) {
        counter = vk::Buffer(buffer);
        mapped = static_cast<u32*>(info.pMappedData);
    }
}
NvidiaFrame::~NvidiaFrame() {
    if (counter)
        vmaDestroyBuffer(instance.GetAllocator(), VkBuffer(counter), counter_allocation);
}
bool NvidiaFrame::Matches(u32 w, u32 h, vk::Extent2D e, vk::Format f, vk::Format backbuffer) const {
    return width == w && height == h && extent == e && format == f && backbuffer_format == backbuffer;
}
Frame NvidiaFrame::Target() const {
    Frame frame{};
    frame.width = width;
    frame.height = height;
    frame.image = vk::Image(processed);
    frame.image_view = *processed_view;
    frame.format = format;
    return frame;
}
void NvidiaFrame::Begin(vk::CommandBuffer cmd) {
    Transition(cmd, vk::Image(processed), vk::ImageLayout::eUndefined,
               vk::ImageLayout::eColorAttachmentOptimal);
}
void NvidiaFrame::ReadCoverage() {
    if (mapped && counted) {
        vmaInvalidateAllocation(instance.GetAllocator(), counter_allocation, 0, sizeof(u32));
        coverage = float(*mapped) / float(extent.width * extent.height);
        counted = false;
    }
}
bool NvidiaFrame::Composite(vk::CommandBuffer cmd, vk::Image final_image, vk::ImageView final_view,
                            vk::ImageLayout final_layout) {
    if (!mapped) {
        DlssFrameGeneration::Get().Fail("NVIDIA UI counter allocation failed");
        return false;
    }
    using L = vk::ImageLayout;
    Transition(cmd, vk::Image(processed), L::eGeneral, L::eTransferSrcOptimal);
    Transition(cmd, vk::Image(hudless), L::eUndefined, L::eTransferDstOptimal);
    const vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    cmd.clearColorImage(vk::Image(hudless), L::eTransferDstOptimal,
                        vk::ClearColorValue{std::array{0.0f, 0.0f, 0.0f, 1.0f}}, range);
    Transition(cmd, vk::Image(hudless), L::eTransferDstOptimal, L::eTransferDstOptimal);
    // Same fit and black border as the real frame's swapchain blit.
    const auto blit = MakeImageBlitFit(width, height, extent.width, extent.height);
    cmd.blitImage(vk::Image(processed), L::eTransferSrcOptimal, vk::Image(hudless),
                  L::eTransferDstOptimal, blit, vk::Filter::eLinear);
    Transition(cmd, vk::Image(hudless), L::eTransferDstOptimal, L::eShaderReadOnlyOptimal);
    Transition(cmd, final_image, final_layout, L::eShaderReadOnlyOptimal);
    Transition(cmd, vk::Image(alpha), L::eUndefined, L::eGeneral);
    cmd.fillBuffer(counter, 0, sizeof(u32), 0);
    const vk::MemoryBarrier2 clear{.srcStageMask = vk::PipelineStageFlagBits2::eClear,
                                   .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                                   .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
                                   .dstAccessMask = vk::AccessFlagBits2::eShaderRead |
                                                    vk::AccessFlagBits2::eShaderWrite};
    cmd.pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &clear});
    const std::array images{
        vk::DescriptorImageInfo{.imageView = final_view, .imageLayout = L::eShaderReadOnlyOptimal},
        vk::DescriptorImageInfo{.imageView = *hudless_view,
                                .imageLayout = L::eShaderReadOnlyOptimal},
        vk::DescriptorImageInfo{.imageView = *alpha_view, .imageLayout = L::eGeneral}};
    const vk::DescriptorBufferInfo buffer{counter, 0, sizeof(u32)};
    std::array<vk::WriteDescriptorSet, 4> writes{};
    for (u32 i = 0; i < 4; ++i) {
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = i < 2    ? vk::DescriptorType::eSampledImage
                                   : i == 2 ? vk::DescriptorType::eStorageImage
                                            : vk::DescriptorType::eStorageBuffer;
        if (i < 3)
            writes[i].pImageInfo = &images[i];
        else
            writes[i].pBufferInfo = &buffer;
    }
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
    cmd.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *layout, 0, writes);
    cmd.dispatch((extent.width + 7) / 8, (extent.height + 7) / 8, 1);
    const vk::MemoryBarrier2 counted_barrier{.srcStageMask =
                                                 vk::PipelineStageFlagBits2::eComputeShader,
                                             .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
                                             .dstStageMask = vk::PipelineStageFlagBits2::eHost,
                                             .dstAccessMask = vk::AccessFlagBits2::eHostRead};
    cmd.pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &counted_barrier});
    counted = true;
    Transition(cmd, vk::Image(alpha), L::eGeneral, L::eShaderReadOnlyOptimal);
    Transition(cmd, final_image, L::eShaderReadOnlyOptimal, final_layout);
    return !reset && coverage <= 0.30f;
}
std::array<SpartanSlImage, 2> NvidiaFrame::Resources() const {
    const auto usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    const SpartanSlImage color{VkImage(vk::Image(hudless)),
                               VkImageView(*hudless_view),
                               VkFormat(backbuffer_format),
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                               usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                               extent.width,
                               extent.height};
    const SpartanSlImage ui{VkImage(vk::Image(alpha)),
                            VkImageView(*alpha_view),
                            VK_FORMAT_R8_UNORM,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            usage | VK_IMAGE_USAGE_STORAGE_BIT,
                            extent.width,
                            extent.height};
    return {color, ui};
}
} // namespace Vulkan
