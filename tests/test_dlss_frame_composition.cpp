// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstdio>
#include <cstdlib>
#include <cstring>
// Vulkan-Hpp's dispatcher layout must match the production library, built with NDEBUG.
#define NDEBUG
#include "video_core/renderer_vulkan/vk_dlss_frame_composition.h"
#include "video_core/renderer_vulkan/vk_presenter.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include <vk_mem_alloc.h>
#undef NDEBUG
#include <cassert>

namespace {
vk::detail::DispatchLoaderDynamic dispatch;
void Barrier(vk::CommandBuffer cmd, vk::Image image, vk::ImageLayout from, vk::ImageLayout to) {
    const vk::ImageMemoryBarrier2 b{
        .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .srcAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
        .oldLayout = from,
        .newLayout = to,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
    cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &b}, dispatch);
}
} // namespace
int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace Vulkan;
    using L = vk::ImageLayout;
    Instance instance(0, argc > 2);
    static vk::detail::DynamicLoader loader;
    dispatch.init(loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));
    dispatch.init(instance.GetInstance());
    dispatch.init(instance.GetDevice());
    Scheduler scheduler(instance);
    const auto device = instance.GetDevice();
    constexpr u32 W = 320, H = 144;
    const bool mixed = argc > 1 && std::strcmp(argv[1], "mixed-hdr") == 0;
    const bool hdr = mixed || (argc > 1 && std::strcmp(argv[1], "hdr") == 0);
    const auto format = hdr ? vk::Format::eA2B10G10R10UnormPack32 : vk::Format::eB8G8R8A8Unorm;
    NvidiaFrame fg(instance, 128, 128, {W, H}, mixed ? vk::Format::eB8G8R8A8Unorm : format, format);
    fg.reset = false;
    VideoCore::UniqueImage final_image(device, instance.GetAllocator());
    final_image.Create({.imageType = vk::ImageType::e2D,
                        .format = format,
                        .extent = {W, H, 1},
                        .mipLevels = 1,
                        .arrayLayers = 1,
                        .samples = vk::SampleCountFlagBits::e1,
                        .tiling = vk::ImageTiling::eOptimal,
                        .usage = vk::ImageUsageFlagBits::eColorAttachment |
                                 vk::ImageUsageFlagBits::eSampled |
                                 vk::ImageUsageFlagBits::eTransferDst});
    auto final_view = device
                          .createImageViewUnique(
                              {.image = vk::Image(final_image),
                               .viewType = vk::ImageViewType::e2D,
                               .format = format,
                               .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}},
                              nullptr, dispatch)
                          .value;
    const auto render_scene = [&] {
        auto cmd = scheduler.CommandBuffer();
        fg.Begin(cmd);
        auto target = fg.Target();
        const vk::RenderingAttachmentInfo attachment{
            .imageView = target.image_view,
            .imageLayout = L::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = {.color = {.float32 = std::array{0.25f, 0.25f, 0.25f, 1.0f}}}};
        cmd.beginRendering({.renderArea = {{0, 0}, {128, 128}},
                            .layerCount = 1,
                            .colorAttachmentCount = 1,
                            .pColorAttachments = &attachment},
                           dispatch);
        cmd.endRendering(dispatch);
        Barrier(cmd, target.image, L::eColorAttachmentOptimal, L::eGeneral);
    };
    render_scene();
    auto cmd = scheduler.CommandBuffer();
    Barrier(cmd, vk::Image(final_image), L::eUndefined, L::eTransferDstOptimal);
    const vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    cmd.clearColorImage(vk::Image(final_image), L::eTransferDstOptimal,
                        vk::ClearColorValue{std::array{0.0f, 0.0f, 0.0f, 1.0f}}, range, dispatch);
    Barrier(cmd, vk::Image(final_image), L::eTransferDstOptimal, L::eTransferDstOptimal);
    auto target = fg.Target();
    Barrier(cmd, target.image, L::eGeneral, L::eTransferSrcOptimal);
    cmd.blitImage(target.image, L::eTransferSrcOptimal, vk::Image(final_image),
                  L::eTransferDstOptimal, MakeImageBlitFit(128, 128, W, H), vk::Filter::eLinear,
                  dispatch);
    Barrier(cmd, target.image, L::eTransferSrcOptimal, L::eGeneral);
    Barrier(cmd, vk::Image(final_image), L::eTransferDstOptimal, L::eColorAttachmentOptimal);
    const vk::RenderingAttachmentInfo attachment{.imageView = *final_view,
                                                 .imageLayout = L::eColorAttachmentOptimal,
                                                 .loadOp = vk::AttachmentLoadOp::eLoad,
                                                 .storeOp = vk::AttachmentStoreOp::eStore};
    cmd.beginRendering({.renderArea = {{0, 0}, {W, H}},
                        .layerCount = 1,
                        .colorAttachmentCount = 1,
                        .pColorAttachments = &attachment},
                       dispatch);
    const vk::ClearAttachment hud{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .colorAttachment = 0,
        .clearValue = {.color = {.float32 = std::array{1.0f, 0.0f, 0.0f, 1.0f}}}};
    const vk::ClearRect rect{.rect = {{100, 40}, {8, 8}}, .baseArrayLayer = 0, .layerCount = 1};
    cmd.clearAttachments(hud, rect, dispatch);
    cmd.endRendering(dispatch);
    Barrier(cmd, vk::Image(final_image), L::eColorAttachmentOptimal, L::eGeneral);
    assert(fg.Composite(cmd, vk::Image(final_image), *final_view, L::eGeneral));
    const auto resources = fg.Resources();
    assert(resources[0].format == VkFormat(format));
    assert(resources[0].width == W && resources[0].height == H);
    assert(resources[1].format == VK_FORMAT_R8_UNORM);
    VkBuffer buffer{};
    VmaAllocation allocation{};
    VmaAllocationInfo memory{};
    const VkBufferCreateInfo info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                  .size = W * H,
                                  .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    const VmaAllocationCreateInfo alloc{.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                                 VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                                        .usage = VMA_MEMORY_USAGE_AUTO};
    assert(vmaCreateBuffer(instance.GetAllocator(), &info, &alloc, &buffer, &allocation, &memory) ==
           VK_SUCCESS);
    Barrier(cmd, vk::Image(resources[1].image), L::eShaderReadOnlyOptimal, L::eTransferSrcOptimal);
    const vk::BufferImageCopy copy{.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                                   .imageExtent = {W, H, 1}};
    cmd.copyImageToBuffer(vk::Image(resources[1].image), L::eTransferSrcOptimal, vk::Buffer(buffer),
                          copy, dispatch);
    const vk::MemoryBarrier2 host{.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                                  .srcAccessMask = vk::AccessFlagBits2::eMemoryWrite,
                                  .dstStageMask = vk::PipelineStageFlagBits2::eHost,
                                  .dstAccessMask = vk::AccessFlagBits2::eHostRead};
    cmd.pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &host}, dispatch);
    scheduler.Finish();
    vmaInvalidateAllocation(instance.GetAllocator(), allocation, 0, VK_WHOLE_SIZE);
    const auto* pixels = static_cast<const u8*>(memory.pMappedData);
    for (u32 y = 0; y < H; ++y)
        for (u32 x = 0; x < W; ++x) {
            const bool ui = x >= 100 && x < 108 && y >= 40 && y < 48;
            assert(pixels[y * W + x] == (ui ? 255 : 0));
        }
    vmaDestroyBuffer(instance.GetAllocator(), buffer, allocation);
    fg.ReadCoverage();
    for (int i = 0; i < 2; ++i) {
        render_scene();
        cmd = scheduler.CommandBuffer();
        Barrier(cmd, vk::Image(final_image), L::eGeneral, L::eTransferDstOptimal);
        cmd.clearColorImage(vk::Image(final_image), L::eTransferDstOptimal,
                            vk::ClearColorValue{std::array{0.0f, 0.0f, 1.0f, 1.0f}}, range,
                            dispatch);
        Barrier(cmd, vk::Image(final_image), L::eTransferDstOptimal, L::eGeneral);
        const bool eligible = fg.Composite(cmd, vk::Image(final_image), *final_view, L::eGeneral);
        assert(eligible == (i == 0));
        scheduler.Finish();
        fg.ReadCoverage();
    }
    std::puts("NVIDIA composition: HUD, letterbox, full-screen guard PASS");
}
