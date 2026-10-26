// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <cstdio>
#include "gpu/shadps4/video_core/renderer_vulkan/ui_composition.h"

int main() {
    using namespace Vulkan::UiComposition;
    // Title/loading menus have no depth/camera. UI must still go to the native target.
    assert(Choose(true, true, false, true) == Background::Copy);
    assert(Choose(true, true, false, false) == Background::Copy);
    // In-game HUD: first reconstruct the scene, then draw the native UI over it.
    assert(Choose(true, true, true, true) == Background::Temporal);
    // Runtime FSR disable/failure must not change UI resolution.
    assert(Choose(true, true, true, false) == Background::Copy);
    assert(Choose(true, false, true, true) == Background::None);
    assert(Choose(false, true, true, true) == Background::None);
    assert(NativeViewport(1920, -1080));
    assert(!NativeViewport(960, -540));
    assert(MovieShader(0x34e8a281) && MovieShader(0x24042a9b));
    assert(!MovieShader(0x0b0acf50)); // fullscreen post/tonemap also has a native viewport
    assert(!MovieShader(0x6c62a79f)); // shadow geometry
    const auto display = Scale(960, 540, 1920, 1080, false);
    assert(display[0] == 2 && display[1] == 2);
    const auto ui = Scale(960, 540, 1920, 1080, true);
    assert(ui[0] == 1 && ui[1] == 1); // don't double an already native UI viewport
    const auto larger = Scale(960, 540, 3840, 2160, true);
    assert(larger[0] == 2 && larger[1] == 2);
    // The failing RTX log: DLAA must not upscale the launcher's two-pixel inset.
    const auto dlaa = TemporalOutputSize(1918, 1078, 1920, 1080, true);
    assert(dlaa[0] == 1918 && dlaa[1] == 1078);
    const auto native = TemporalOutputSize(1920, 1080, 1920, 1080, true);
    assert(native[0] == 1920 && native[1] == 1080);
    const auto dlaa_1440 = TemporalOutputSize(2558, 1438, 2560, 1440, true);
    assert(dlaa_1440[0] == 2558 && dlaa_1440[1] == 1438);
    const auto dlaa_4k = TemporalOutputSize(3838, 2158, 3840, 2160, true);
    assert(dlaa_4k[0] == 3838 && dlaa_4k[1] == 2158);
    for (const auto scene : std::array{std::array{1476u, 830u}, std::array{1280u, 720u},
                                      std::array{1130u, 636u}, std::array{960u, 540u},
                                      std::array{640u, 360u}, std::array{1918u, 1078u}}) {
        const auto upscaled = TemporalOutputSize(scene[0], scene[1], 1920, 1080, false);
        assert(upscaled[0] == 1920 && upscaled[1] == 1080);
    }
    std::puts("UI composition: PASS (menu without camera, HUD, FSR off, viewport scaling)");
}
