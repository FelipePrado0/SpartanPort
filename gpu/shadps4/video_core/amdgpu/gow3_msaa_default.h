// SPDX-License-Identifier: GPL-2.0-or-later
// gow3: the Vulkan drivers where the game's multisampled images are created single-sampled
// unless GOW3_NO_MSAA says otherwise (gow3_msaa.h). Plain C: vk_instance.cpp decides with it
// and tools/gpu_capabilities.c prints it per GPU (`--list`) for the launcher to show.
//
// AMD's Windows driver: the RX 6600 family reads the 2-sample depth buffer wrong and the
// lighting shows black lines (#65). Intel's Windows driver: the same lines on Arc A750, B580
// and 140V (#28). NVIDIA, RADV, AMDVLK, Mesa ANV and MoltenVK read it correctly as far as
// known and keep the game's multisampling.
#pragma once

#include <vulkan/vulkan_core.h>

static inline int gow3_no_msaa_default(VkDriverId driver) {
    return driver == VK_DRIVER_ID_AMD_PROPRIETARY || driver == VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS;
}
