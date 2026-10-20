// SPDX-License-Identifier: GPL-2.0-or-later
// SPARTAN_AMD_PATHS_OFF=mixed,shaderext (vk_amd_paths.h): runs the shared core without the device
// extensions only AMD exposes, so a tester can tell an AMD-only code path from the driver.
// No game or Vulkan device required.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "video_core/renderer_vulkan/vk_amd_paths.h"

using namespace Vulkan;

int main() {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    using AmdPathsOff::Flags;
    const Flags none{};
    assert(AmdPathsOff::Parse(nullptr) == none);
    assert(AmdPathsOff::Parse("") == none);
    assert(AmdPathsOff::Parse("0") == none);
    assert(!none.Any());

    const Flags both{.mixed = true, .shaderext = true};
    assert(AmdPathsOff::Parse("mixed,shaderext") == both);
    assert(AmdPathsOff::Parse("all") == both);
    assert(AmdPathsOff::Parse("1") == both);
    assert((AmdPathsOff::Parse("shaderext") == Flags{.shaderext = true}));
    assert((AmdPathsOff::Parse("bogus,mixed") == Flags{.mixed = true}));
    assert(AmdPathsOff::Parse("Mixed shaderExt") == both);
    assert(both.Any());

    assert(AmdPathsOff::Marker(both) == "GPU: amd paths off: mixed shaderext");
    assert((AmdPathsOff::Marker(Flags{.mixed = true}) == "GPU: amd paths off: mixed"));
    assert(AmdPathsOff::Marker(none).empty());

    std::puts("amd paths knob OK");
    return EXIT_SUCCESS;
}
