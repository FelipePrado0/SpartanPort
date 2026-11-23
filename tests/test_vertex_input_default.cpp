// SPDX-License-Identifier: GPL-2.0-or-later
// Default and SPARTAN_VERTEX_INPUT_DYNAMIC override for VK_EXT_vertex_input_dynamic_state
// (spartanport_vertex_input_default.h). No game, Vulkan device or window required.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

#include <vulkan/vulkan_core.h>

#include "video_core/renderer_vulkan/spartanport_vertex_input_default.h"

int main() {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    // Intel's Windows driver draws 3D objects black or blue with the extension (#861): off.
    assert(spartanport_vertex_input_dynamic_default(VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS) == 0);
    // Every other driver, including an unknown one, keeps it on.
    for (VkDriverId d : {VK_DRIVER_ID_NVIDIA_PROPRIETARY, VK_DRIVER_ID_AMD_PROPRIETARY, VK_DRIVER_ID_MESA_RADV,
                         VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA, VK_DRIVER_ID_MOLTENVK, (VkDriverId)0})
        assert(spartanport_vertex_input_dynamic_default(d) == 1);

    // Unset or empty variable: the driver default.
    assert(spartanport_vertex_input_dynamic(nullptr, VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS) == 0);
    assert(spartanport_vertex_input_dynamic("", VK_DRIVER_ID_NVIDIA_PROPRIETARY) == 1);
    // "1" turns it on everywhere; any other value, a blank included, turns it off.
    assert(spartanport_vertex_input_dynamic("1", VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS) == 1);
    assert(spartanport_vertex_input_dynamic("0", VK_DRIVER_ID_NVIDIA_PROPRIETARY) == 0);
    assert(spartanport_vertex_input_dynamic("2", VK_DRIVER_ID_NVIDIA_PROPRIETARY) == 0);
    assert(spartanport_vertex_input_dynamic(" ", VK_DRIVER_ID_NVIDIA_PROPRIETARY) == 0);

    std::puts("vertex-input-default-test: ok");
    return 0;
}
