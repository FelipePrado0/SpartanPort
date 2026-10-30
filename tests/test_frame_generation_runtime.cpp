// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise the production FG lifecycle and HUD comparison with synthetic GPU images.
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "spartanport_settings.h"
#include "video_core/renderer_vulkan/vk_frame_generation.h"
#include "video_core/renderer_vulkan/vk_gpu_profiler.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include <vk_mem_alloc.h>

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace Vulkan;
    Instance instance(0, argc > 2 && std::strcmp(argv[2], "validation") == 0);
    Scheduler scheduler(instance);
    const bool profile = argc > 1 && std::strcmp(argv[1], "profile") == 0;
    if (profile) {
#ifdef _WIN32
        _putenv_s("SPARTAN_GPU_PROFILE", "1");
#else
        setenv("SPARTAN_GPU_PROFILE", "1", 1);
#endif
        GpuProfiler::Init(instance, scheduler);
    }
    auto* profiler = GpuProfiler::Get();
    constexpr u64 parent_key = 0xF6AF0000;

    auto& settings = SpartanSettings::Get();
    settings.frame_generation = 1;
    settings.fg_problem = nullptr;
    static vk::detail::DynamicLoader loader;
    const auto get_instance_proc = loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    const auto get_device_proc = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        get_instance_proc(VkInstance(instance.GetInstance()), "vkGetDeviceProcAddr"));
    const auto device = instance.GetDevice();
    const auto create_view = reinterpret_cast<PFN_vkCreateImageView>(get_device_proc(device, "vkCreateImageView"));
    const auto destroy_view = reinterpret_cast<PFN_vkDestroyImageView>(get_device_proc(device, "vkDestroyImageView"));
    const auto barrier = reinterpret_cast<PFN_vkCmdPipelineBarrier2>(get_device_proc(device, "vkCmdPipelineBarrier2"));
    const auto clear = reinterpret_cast<PFN_vkCmdClearColorImage>(get_device_proc(device, "vkCmdClearColorImage"));
    const auto blit = reinterpret_cast<PFN_vkCmdBlitImage>(get_device_proc(device, "vkCmdBlitImage"));
    const auto read_image = reinterpret_cast<PFN_vkCmdCopyImageToBuffer>(
        get_device_proc(device, "vkCmdCopyImageToBuffer"));
    constexpr u32 W = 128, H = 128;
    std::array<VideoCore::UniqueImage, 4> images;
    std::array<VkImageView, 4> views{};
    const bool decode = argc > 1 && std::strcmp(argv[1], "decode") == 0;
    const bool same = argc > 1 && std::strcmp(argv[1], "same") == 0;
    const std::array formats{decode || same ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm,
                            vk::Format::eR32Sfloat, vk::Format::eR16G16Sfloat,
                            decode ? vk::Format::eR8G8B8A8Unorm : vk::Format::eR8G8B8A8Srgb};
    for (u32 i = 0; i < images.size(); ++i) {
        const u32 size = i == 1 || i == 2 ? 64 : W;
        images[i] = VideoCore::UniqueImage(device, instance.GetAllocator());
        images[i].Create({.flags = vk::ImageCreateFlagBits::eMutableFormat,
            .imageType = vk::ImageType::e2D, .format = formats[i], .extent = {size, size, 1},
            .mipLevels = 1, .arrayLayers = 1, .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc |
                     vk::ImageUsageFlagBits::eTransferDst});
        const VkImageViewCreateInfo view{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = VkImage(vk::Image(images[i])), .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = VkFormat(formats[i]),
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
        assert(create_view(device, &view, nullptr, &views[i]) == VK_SUCCESS);
    }
    auto cmd = scheduler.CommandBuffer();
    for (u32 i = 0; i < images.size(); ++i) {
        const vk::ImageMemoryBarrier2 b{
            .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
            .dstAccessMask = vk::AccessFlagBits2::eMemoryWrite,
            .oldLayout = vk::ImageLayout::eUndefined, .newLayout = vk::ImageLayout::eGeneral,
            .image = vk::Image(images[i]),
            .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
        const VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = reinterpret_cast<const VkImageMemoryBarrier2*>(&b)};
        barrier(cmd, &dep);
        const vk::ClearColorValue color{.float32 = std::array{0.25f, 0.25f, 0.25f, 1.0f}};
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        clear(cmd, VkImage(vk::Image(images[i])), VK_IMAGE_LAYOUT_GENERAL,
              reinterpret_cast<const VkClearColorValue*>(&color), 1, &range);
    }
    // The same transfer used by the display path: linear UNORM scene into an sRGB display.
    const vk::MemoryBarrier2 written{
        .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .srcAccessMask = vk::AccessFlagBits2::eMemoryWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .dstAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite};
    const VkDependencyInfo written_dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1, .pMemoryBarriers = reinterpret_cast<const VkMemoryBarrier2*>(&written)};
    barrier(cmd, &written_dep);
    const vk::ImageBlit region{
        .srcSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .srcOffsets = std::array{vk::Offset3D{}, vk::Offset3D{W, H, 1}},
        .dstSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .dstOffsets = std::array{vk::Offset3D{}, vk::Offset3D{W, H, 1}}};
    blit(cmd, VkImage(vk::Image(images[0])), VK_IMAGE_LAYOUT_GENERAL, VkImage(vk::Image(images[3])),
         VK_IMAGE_LAYOUT_GENERAL, 1, reinterpret_cast<const VkImageBlit*>(&region), VK_FILTER_NEAREST);
    barrier(cmd, &written_dep);
    scheduler.Finish();
    FrameGeneration fg(instance, scheduler);
    FrameGeneration::CaptureInfo info{
        .scene = vk::Image(images[0]), .scene_format = formats[0],
        .output_width = W, .output_height = H, .depth = views[1],
        .motion = vk::Image(images[2]), .motion_view = views[2],
        .width = 64, .height = 64, .frame_ms = 16.6f, .near_plane = 0.1f,
        .vertical_fov = 1.0f};
    const bool pending = argc > 1 && std::strcmp(argv[1], "pending") == 0;
    if (pending) {
        // Two rendered scenes can precede a flip. Every prepare must still get a dispatch.
        fg.Capture(scheduler.CommandBuffer(), info);
        fg.Capture(scheduler.CommandBuffer(), info);
        assert(settings.fg_problem.load() == nullptr);
        fg.Generate(scheduler.CommandBuffer(), true, vk::Image(images[3]), formats[3]);
    } else {
        vk::Image generated{};
        const auto profile_end = std::chrono::steady_clock::now() + std::chrono::milliseconds(5200);
        for (int i = 0; i < 8 || (profile && std::chrono::steady_clock::now() < profile_end); ++i) {
            if (profiler) {
                profiler->BeginFrame();
                profiler->Mark(parent_key, [] { return std::string{"FG test parent"}; });
            }
            fg.Capture(scheduler.CommandBuffer(), info);
            if (profiler && i > 0) assert(profiler->Current() == parent_key);
            const auto result = fg.Generate(scheduler.CommandBuffer(), true,
                                            vk::Image(images[3]), formats[3]);
            if (profiler && i > 0) assert(profiler->Current() == parent_key);
            scheduler.Finish();
            if (i >= 4) generated = result;
            assert(settings.fg_problem.load() == nullptr);
        }
        // No HUD and no screen effect: a color-space conversion must not suppress FG.
        assert(generated);
        // The generated scene and the display must also agree on actual pixel encoding.
        VkBuffer buffer{};
        VmaAllocation allocation{};
        VmaAllocationInfo mapped{};
        const VkBufferCreateInfo buffer_info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = 8, .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT};
        const VmaAllocationCreateInfo memory_info{.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, .usage = VMA_MEMORY_USAGE_AUTO};
        assert(vmaCreateBuffer(instance.GetAllocator(), &buffer_info, &memory_info,
                               &buffer, &allocation, &mapped) == VK_SUCCESS);
        cmd = scheduler.CommandBuffer();
        const std::array sources{VkImage(generated), VkImage(vk::Image(images[3]))};
        for (u32 i = 0; i < sources.size(); ++i) {
            const VkImageMemoryBarrier2 transition{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT, .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
                .oldLayout = i == 0 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL, .image = sources[i],
                .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
            const VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &transition};
            barrier(cmd, &dep);
            const VkBufferImageCopy copy{.bufferOffset = i * 4u,
                .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                .imageOffset = {W / 2, H / 2, 0}, .imageExtent = {1, 1, 1}};
            read_image(cmd, sources[i], VK_IMAGE_LAYOUT_GENERAL, buffer, 1, &copy);
        }
        const VkMemoryBarrier2 host{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT, .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT, .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT};
        const VkDependencyInfo host_dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1, .pMemoryBarriers = &host};
        barrier(cmd, &host_dep);
        scheduler.Finish();
        vmaInvalidateAllocation(instance.GetAllocator(), allocation, 0, VK_WHOLE_SIZE);
        const auto* pixels = static_cast<const unsigned char*>(mapped.pMappedData);
        for (u32 i = 0; i < 3; ++i) {
            std::printf("FG pixel %u: generated %u display %u\n", i, pixels[i], pixels[4+i]);
            assert(std::abs(int(pixels[i]) - int(pixels[4+i])) <= 3);
        }
        vmaDestroyBuffer(instance.GetAllocator(), buffer, allocation);
        if (argc > 1 && std::strcmp(argv[1], "fullscreen") == 0) {
            cmd = scheduler.CommandBuffer();
            const VkClearColorValue black{};
            const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            clear(cmd, VkImage(vk::Image(images[3])), VK_IMAGE_LAYOUT_GENERAL, &black, 1, &range);
            barrier(cmd, &written_dep);
            scheduler.Finish();
            for (int i = 0; i < 4; ++i) {
                fg.Capture(scheduler.CommandBuffer(), info);
                generated = fg.Generate(scheduler.CommandBuffer(), true, vk::Image(images[3]), formats[3]);
                scheduler.Finish();
            }
            assert(!generated);
            assert(settings.fg_problem.load() == nullptr);
            std::puts("FG fullscreen suppression: PASS");
        }
    }
    scheduler.Finish();
    scheduler.PopPendingOperations();
    for (const auto view : views) destroy_view(device, view, nullptr);
    std::puts(pending ? "FG consecutive captures: PASS" : "FG color-space comparison: PASS");
}
