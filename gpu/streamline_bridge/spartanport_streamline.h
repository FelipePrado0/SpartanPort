// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <vulkan/vulkan.h>

#define SPARTAN_STREAMLINE_ABI 1u

// No SDK C++ types cross the MSVC/MinGW boundary.
struct SpartanSlCamera {
    float view[12], inverse_view[12], previous_view[12];
    float projection[4], previous_projection[4];
    float jitter[2], near_plane, far_plane, fov;
    uint32_t reset;
};
struct SpartanSlImage {
    VkImage image;
    VkImageView view;
    VkFormat format;
    VkImageLayout layout;
    VkImageUsageFlags usage;
    uint32_t width, height;
};
struct SpartanSlState {
    uint32_t presented, status, minimum_dimension, vsync_supported;
};
struct SpartanSlApi {
    uint32_t abi, size;
    int (*Configure)(const wchar_t* directory);
    int (*Supported)(VkPhysicalDevice physical, const uint8_t* luid, uint32_t luid_size);
    PFN_vkGetInstanceProcAddr (*Resolver)();
    int (*Initialize)();
    // Tickets encode factory frame indices; no SDK-owned C++ object escapes this DLL.
    void* (*BeginFrame)(uint32_t frame_id);
    int (*Marker)(void* token, uint32_t marker);
    int (*Inputs)(void* token, VkCommandBuffer cmd, const SpartanSlCamera* camera,
                  const SpartanSlImage* depth, const SpartanSlImage* motion);
    int (*Color)(void* token, VkCommandBuffer cmd, const SpartanSlImage* hudless,
                 const SpartanSlImage* alpha);
    int (*SetEnabled)(int enabled);
    int (*GetState)(SpartanSlState* state);
    const char* (*Problem)();
    void (*Shutdown)();
};
using SpartanSlGetApiFn = const SpartanSlApi* (*)();
