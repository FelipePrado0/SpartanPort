// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise production FG motion filtering: valid objects, stale depth and camera fallback.
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <limits>
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>
#include "video_core/host_shaders/fg_depth_comp.h"
#define VMA_IMPLEMENTATION
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#include <vk_mem_alloc.h>
#pragma clang diagnostic pop

int main() {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using u32 = uint32_t;
    using u8 = uint8_t;
    static vk::detail::DynamicLoader loader;
    vk::detail::DispatchLoaderDynamic d;
    d.init(loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));
    const vk::ApplicationInfo app{.pApplicationName = "SpartanPort FG motion test",
                                  .apiVersion = VK_API_VERSION_1_3};
    auto instance = vk::createInstanceUnique({.pApplicationInfo = &app}, nullptr, d).value;
    d.init(*instance);
    const auto physical = instance->enumeratePhysicalDevices(d).value.at(0);
    const auto families = physical.getQueueFamilyProperties(d);
    u32 family = 0;
    while (family < families.size() && !(families[family].queueFlags & vk::QueueFlagBits::eCompute)) ++family;
    assert(family < families.size());
    const float priority = 1;
    const vk::DeviceQueueCreateInfo queue_info{.queueFamilyIndex = family, .queueCount = 1,
                                              .pQueuePriorities = &priority};
    const char* extension = VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME;
    const vk::PhysicalDeviceFeatures features{.shaderStorageImageExtendedFormats = true};
    const vk::PhysicalDeviceVulkan13Features features13{.synchronization2 = true};
    auto owned_device = physical.createDeviceUnique({.pNext = &features13,
        .queueCreateInfoCount = 1, .pQueueCreateInfos = &queue_info, .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = &extension, .pEnabledFeatures = &features}, nullptr, d).value;
    const auto device = *owned_device;
    d.init(device);
    const auto queue = device.getQueue(family, 0, d);
    auto pool = device.createCommandPoolUnique({.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                               .queueFamilyIndex = family}, nullptr, d).value;
    auto commands = device.allocateCommandBuffersUnique({.commandPool = *pool,
        .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1}, d).value;
    VmaAllocator allocator{};
    const VmaAllocatorCreateInfo allocator_info{.physicalDevice = physical, .device = device,
                                                .instance = *instance, .vulkanApiVersion = VK_API_VERSION_1_3};
    assert(vmaCreateAllocator(&allocator_info, &allocator) == VK_SUCCESS);
    constexpr u32 W = 9, H = 3, N = W * H;
    constexpr std::array<u32, 6> offsets{0, 2560, 512, 1024, 2048, 1536};
    const std::array formats{vk::Format::eR32Sfloat, vk::Format::eR32Sfloat,
        vk::Format::eR16G16Sfloat, vk::Format::eR8Unorm, vk::Format::eR16G16Sfloat,
        vk::Format::eR32G32B32A32Sfloat};
    std::array<VkImage, 6> images{};
    std::array<VmaAllocation, 6> image_allocations{};
    std::array<vk::UniqueHandle<vk::ImageView, vk::detail::DispatchLoaderDynamic>, 6> views;
    std::array<vk::DescriptorSetLayoutBinding, 6> bindings{};
    for (u32 i = 0; i < images.size(); ++i) {
        const auto format = formats[i];
        const vk::ImageCreateInfo image_info{.imageType = vk::ImageType::e2D, .format = format,
            .extent = {W, H, 1}, .mipLevels = 1, .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1, .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc |
                     vk::ImageUsageFlagBits::eTransferDst |
                     (i == 1 || i == 4 ? vk::ImageUsageFlagBits::eStorage : vk::ImageUsageFlags{})};
        const VmaAllocationCreateInfo image_memory{.usage = VMA_MEMORY_USAGE_AUTO};
        assert(vmaCreateImage(allocator, reinterpret_cast<const VkImageCreateInfo*>(&image_info),
                              &image_memory, &images[i], &image_allocations[i], nullptr) == VK_SUCCESS);
        views[i] = device.createImageViewUnique({.image = vk::Image(images[i]),
            .viewType = vk::ImageViewType::e2D, .format = format,
            .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}}, nullptr, d).value;
        bindings[i] = {.binding = i,
            .descriptorType = i == 1 || i == 4 ? vk::DescriptorType::eStorageImage : vk::DescriptorType::eSampledImage,
            .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eCompute};
    }
    auto desc = device.createDescriptorSetLayoutUnique({
        .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
        .bindingCount = 6, .pBindings = bindings.data()}, nullptr, d).value;
    const vk::PushConstantRange push{vk::ShaderStageFlagBits::eCompute, 0, 12};
    auto layout = device.createPipelineLayoutUnique({.setLayoutCount = 1, .pSetLayouts = &*desc,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &push}, nullptr, d).value;
    auto module = device.createShaderModuleUnique({.codeSize = sizeof(FG_DEPTH_COMP),
        .pCode = FG_DEPTH_COMP}, nullptr, d).value;
    auto pipeline = device.createComputePipelineUnique({}, {
        .stage = {.stage = vk::ShaderStageFlagBits::eCompute, .module = *module, .pName = "main"},
        .layout = *layout}, nullptr, d).value;
    VkBuffer staging{};
    VmaAllocation allocation{};
    VmaAllocationInfo mapped{};
    const VkBufferCreateInfo bi{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = 3072, .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    const VmaAllocationCreateInfo ac{.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
    assert(vmaCreateBuffer(allocator, &bi, &ac, &staging, &allocation, &mapped) == VK_SUCCESS);
    const auto barrier = [&](vk::CommandBuffer cmd, vk::PipelineStageFlags2 src, vk::AccessFlags2 access,
                             vk::PipelineStageFlags2 dst, vk::AccessFlags2 next) {
        const vk::MemoryBarrier2 b{.srcStageMask = src, .srcAccessMask = access,
                                  .dstStageMask = dst, .dstAccessMask = next};
        cmd.pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &b}, d);
    };
    for (const auto flags : {std::array{1u, 1u}, std::array{1u, 0u}, std::array{0u, 1u}}) {
        auto* pixels = static_cast<u8*>(mapped.pMappedData);
        std::memset(pixels, 0, 3072);
        auto* depth = reinterpret_cast<float*>(pixels);
        auto* motion = reinterpret_cast<uint16_t*>(pixels + offsets[2]);
        auto* objects = reinterpret_cast<float*>(pixels + offsets[5]);
        for (u32 p = 0; p < N; ++p) {
            depth[p] = 0.5f;
            motion[p*2] = 0x4000; // 2.0 in half precision
            motion[p*2+1] = 0xbc00; // -1.0
            pixels[offsets[3]+p] = p == 7 ? 0 : 255;
            objects[p*4] = 2.f;
            objects[p*4+1] = -1.f;
            objects[p*4+2] = 1.f;
            objects[p*4+3] = 0.5f;
        }
        objects[1*4+2] = 0.f; // no object vector
        objects[2*4+3] = 0.9f; // another surface
        objects[3*4] = std::numeric_limits<float>::quiet_NaN();
        objects[4*4+1] = std::numeric_limits<float>::infinity();
        objects[5*4+3] = 0.506f; // just outside the camera motion depth tolerance
        objects[6*4+3] = 0.504f; // inside the same tolerance
        motion[8*2] = motion[8*2+1] = 0; // valid stationary object
        objects[8*4] = objects[8*4+1] = 0.f;
        depth[9] = depth[10] = 0.99999f;
        objects[9*4+3] = 1.f; // distant surfaces still reject stale vectors
        objects[10*4+3] = 0.9999899f; // within the full-precision minimum tolerance
        motion[11*2] = 0x7c00; // finite object data can overflow the composed half vector
        motion[12*2+1] = 0x7e00; // reject a non-finite composed vector as well
        const auto cmd = *commands[0];
        assert(cmd.reset({}, d) == vk::Result::eSuccess);
        assert(cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit}, d) == vk::Result::eSuccess);
        std::array<vk::ImageMemoryBarrier2, 6> transitions{};
        for (u32 i = 0; i < images.size(); ++i) {
            transitions[i] = {.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                .srcAccessMask = vk::AccessFlagBits2::eNone, .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
                .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
                .oldLayout = vk::ImageLayout::eUndefined, .newLayout = vk::ImageLayout::eGeneral,
                .image = vk::Image(images[i]), .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
        }
        cmd.pipelineBarrier2({.imageMemoryBarrierCount = 6, .pImageMemoryBarriers = transitions.data()}, d);
        for (u32 i : {0u, 2u, 3u, 5u}) {
            const vk::BufferImageCopy copy{.bufferOffset = offsets[i],
                .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1}, .imageExtent = {W, H, 1}};
            cmd.copyBufferToImage(staging, vk::Image(images[i]), vk::ImageLayout::eGeneral, copy, d);
        }
        barrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite);
        std::array<vk::DescriptorImageInfo, 6> infos{};
        std::array<vk::WriteDescriptorSet, 6> writes{};
        for (u32 i = 0; i < images.size(); ++i) {
            infos[i] = {.imageView = *views[i], .imageLayout = vk::ImageLayout::eGeneral};
            writes[i] = {.dstBinding = i, .descriptorCount = 1,
                .descriptorType = bindings[i].descriptorType, .pImageInfo = &infos[i]};
        }
        struct Params { u32 mask, objects; float projection_z; } params{flags[0], flags[1], 1.0f};
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline, d);
        cmd.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *layout, 0, writes, d);
        cmd.pushConstants(*layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(params), &params, d);
        cmd.dispatch((W + 7) / 8, (H + 7) / 8, 1, d);
        barrier(cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite,
                vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);
        for (u32 i : {1u, 4u}) {
            const vk::BufferImageCopy read{.bufferOffset = offsets[i],
                .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1}, .imageExtent = {W, H, 1}};
            cmd.copyImageToBuffer(vk::Image(images[i]), vk::ImageLayout::eGeneral, staging, read, d);
        }
        barrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eHost, vk::AccessFlagBits2::eHostRead);
        assert(cmd.end(d) == vk::Result::eSuccess);
        const vk::SubmitInfo submit{.commandBufferCount = 1, .pCommandBuffers = &cmd};
        assert(queue.submit(submit, {}, d) == vk::Result::eSuccess);
        assert(queue.waitIdle(d) == vk::Result::eSuccess);
        const auto* result = reinterpret_cast<const uint16_t*>(pixels + offsets[4]);
        const auto* result_depth = reinterpret_cast<const float*>(pixels + offsets[1]);
        for (u32 p = 0; p < N; ++p) {
            const bool valid = p == 0 || p == 6 || p == 8 || p == 10 || p >= 13;
            const bool keep = !flags[0] || p == 7 || (flags[1] && valid);
            const uint16_t x = keep ? motion[p*2] : 0;
            const uint16_t y = keep ? motion[p*2+1] : 0;
            if (result[p*2] != x || result[p*2+1] != y) {
                std::fprintf(stderr, "FG motion pixel %u mask %u objects %u: got %04x/%04x expected %04x/%04x\n",
                    p, flags[0], flags[1], result[p*2], result[p*2+1], x, y);
            }
            assert(result[p*2] == x && result[p*2+1] == y);
            assert(result_depth[p] == depth[p]);
        }
    }
    vmaDestroyBuffer(allocator, staging, allocation);
    for (u32 i = 0; i < images.size(); ++i) {
        views[i].reset();
        vmaDestroyImage(allocator, images[i], image_allocations[i]);
    }
    vmaDestroyAllocator(allocator);
    std::puts("FG motion: valid objects preserved, invalid/stale objects rejected, camera fallback PASS");
}
