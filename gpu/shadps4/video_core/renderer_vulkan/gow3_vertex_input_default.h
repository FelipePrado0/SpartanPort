// SPDX-License-Identifier: GPL-2.0-or-later
// gow3: the Vulkan drivers where VK_EXT_vertex_input_dynamic_state stays off unless
// GOW3_VERTEX_INPUT_DYNAMIC says otherwise. Plain C: vk_instance.cpp decides with it and
// tools/gpu_capabilities.c prints the same result per GPU for the launcher to show.
//
// Intel's Windows driver: on an HD 630 every 3D object is drawn black or blue with the
// extension and correctly without it (#35). ANGLE and yuzu turn the extension off on every
// Intel Windows driver; Arc follows the same default until someone measures otherwise.
// NVIDIA, AMD, RADV, Mesa ANV and MoltenVK keep it on.
#pragma once

#include <vulkan/vulkan_core.h>

static inline int gow3_vertex_input_dynamic_default(VkDriverId driver) {
    return driver != VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS;
}

// GOW3_VERTEX_INPUT_DYNAMIC: unset or empty keeps the driver default, "1" turns the extension
// on, any other value turns it off.
static inline int gow3_vertex_input_dynamic(const char* env, VkDriverId driver) {
    return env && *env ? *env == '1' : gow3_vertex_input_dynamic_default(driver);
}
