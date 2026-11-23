// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise the production stencil shader on the GPU without loading the game.
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>
#include "video_core/host_shaders/stencil_reactive_comp.h"
#define VMA_IMPLEMENTATION
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#include <vk_mem_alloc.h>
#pragma clang diagnostic pop

int main() {
    using u32 = uint32_t;
    using u8 = uint8_t;
    static vk::detail::DynamicLoader loader;
    vk::detail::DispatchLoaderDynamic d;
    d.init(loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));
    const vk::ApplicationInfo app{.pApplicationName = "SpartanPort reactive shader test",
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
    constexpr u32 W = 9, H = 3, N = W * H, mask_offset = 32;
    std::array<VkImage, 2> images{};
    std::array<VmaAllocation, 2> image_allocations{};
    std::array<vk::UniqueHandle<vk::ImageView, vk::detail::DispatchLoaderDynamic>, 2> views;
    std::array<vk::DescriptorSetLayoutBinding, 2> bindings{};
    for (u32 i = 0; i < images.size(); ++i) {
        const auto format = i == 0 ? vk::Format::eR8Uint : vk::Format::eR8Unorm;
        const vk::ImageCreateInfo image_info{.imageType = vk::ImageType::e2D, .format = format,
            .extent = {W, H, 1}, .mipLevels = 1, .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1, .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc |
                     vk::ImageUsageFlagBits::eTransferDst |
                     (i == 1 ? vk::ImageUsageFlagBits::eStorage : vk::ImageUsageFlags{})};
        const VmaAllocationCreateInfo image_memory{.usage = VMA_MEMORY_USAGE_AUTO};
        assert(vmaCreateImage(allocator, reinterpret_cast<const VkImageCreateInfo*>(&image_info),
                              &image_memory, &images[i], &image_allocations[i], nullptr) == VK_SUCCESS);
        views[i] = device.createImageViewUnique({.image = vk::Image(images[i]),
            .viewType = vk::ImageViewType::e2D, .format = format,
            .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}}, nullptr, d).value;
        bindings[i] = {.binding = i,
            .descriptorType = i == 0 ? vk::DescriptorType::eSampledImage : vk::DescriptorType::eStorageImage,
            .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eCompute};
    }
    auto desc = device.createDescriptorSetLayoutUnique({
        .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
        .bindingCount = 2, .pBindings = bindings.data()}, nullptr, d).value;
    const vk::PushConstantRange push{vk::ShaderStageFlagBits::eCompute, 0, 12};
    auto layout = device.createPipelineLayoutUnique({.setLayoutCount = 1, .pSetLayouts = &*desc,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &push}, nullptr, d).value;
    auto module = device.createShaderModuleUnique({.codeSize = sizeof(STENCIL_REACTIVE_COMP),
        .pCode = STENCIL_REACTIVE_COMP}, nullptr, d).value;
    auto pipeline = device.createComputePipelineUnique({}, {
        .stage = {.stage = vk::ShaderStageFlagBits::eCompute, .module = *module, .pName = "main"},
        .layout = *layout}, nullptr, d).value;
    VkBuffer staging{};
    VmaAllocation allocation{};
    VmaAllocationInfo mapped{};
    const VkBufferCreateInfo bi{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = mask_offset + N, .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};
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
    for (u32 preserve : {1u, 0u}) {
        auto* pixels = static_cast<u8*>(mapped.pMappedData);
        std::memset(pixels, 129, N);
        std::memset(pixels + mask_offset, 230, N);
        pixels[1] = 0; // character
        pixels[2] = 128; // moving object
        pixels[mask_offset + 1] = pixels[mask_offset + 2] = pixels[mask_offset + 3] = 0;
        const auto cmd = *commands[0];
        assert(cmd.reset({}, d) == vk::Result::eSuccess);
        assert(cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit}, d) == vk::Result::eSuccess);
        std::array<vk::ImageMemoryBarrier2, 2> transitions{};
        for (u32 i = 0; i < images.size(); ++i) {
            transitions[i] = {.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                .srcAccessMask = vk::AccessFlagBits2::eNone, .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
                .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
                .oldLayout = vk::ImageLayout::eUndefined, .newLayout = vk::ImageLayout::eGeneral,
                .image = vk::Image(images[i]), .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
        }
        cmd.pipelineBarrier2({.imageMemoryBarrierCount = 2, .pImageMemoryBarriers = transitions.data()}, d);
        for (u32 i = 0; i < images.size(); ++i) {
            const vk::BufferImageCopy copy{.bufferOffset = i == 0 ? 0u : mask_offset,
                .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1}, .imageExtent = {W, H, 1}};
            cmd.copyBufferToImage(staging, vk::Image(images[i]), vk::ImageLayout::eGeneral, copy, d);
        }
        barrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite);
        std::array<vk::DescriptorImageInfo, 2> infos{};
        std::array<vk::WriteDescriptorSet, 2> writes{};
        for (u32 i = 0; i < images.size(); ++i) {
            infos[i] = {.imageView = *views[i], .imageLayout = vk::ImageLayout::eGeneral};
            writes[i] = {.dstBinding = i, .descriptorCount = 1,
                .descriptorType = bindings[i].descriptorType, .pImageInfo = &infos[i]};
        }
        struct Params { u32 world; float value; u32 preserve; } params{129, 1.0f, preserve};
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline, d);
        cmd.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *layout, 0, writes, d);
        cmd.pushConstants(*layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(params), &params, d);
        cmd.dispatch((W + 7) / 8, (H + 7) / 8, 1, d);
        barrier(cmd, vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderWrite,
                vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferRead);
        const vk::BufferImageCopy read{.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                                      .imageExtent = {W, H, 1}};
        cmd.copyImageToBuffer(vk::Image(images[1]), vk::ImageLayout::eGeneral, staging, read, d);
        barrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eHost, vk::AccessFlagBits2::eHostRead);
        assert(cmd.end(d) == vk::Result::eSuccess);
        const vk::SubmitInfo submit{.commandBufferCount = 1, .pCommandBuffers = &cmd};
        assert(queue.submit(submit, {}, d) == vk::Result::eSuccess);
        assert(queue.waitIdle(d) == vk::Result::eSuccess);
        for (u32 p = 0; p < N; ++p) {
            const u8 expected = p == 1 || p == 2 ? 255 : preserve && p != 3 ? 230 : 0;
            if (pixels[p] != expected) std::fprintf(stderr, "Mask pixel %u: got %u, expected %u\n", p, pixels[p], expected);
            assert(pixels[p] == expected);
        }
    }
    vmaDestroyBuffer(allocator, staging, allocation);
    for (u32 i = 0; i < images.size(); ++i) {
        views[i].reset();
        vmaDestroyImage(allocator, images[i], image_allocations[i]);
    }
    vmaDestroyAllocator(allocator);
    std::puts("Reactive shader: particles preserved, characters retained, stale mask discarded PASS");
}
