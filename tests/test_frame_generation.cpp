// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <cstdio>
#include "gpu/shadps4/video_core/renderer_vulkan/frame_generation_policy.h"

int main() {
    using namespace Vulkan::FrameGenerationPolicy;
    const Input happy{.enabled = true, .upscaled = true, .reset = false, .ui_coverage = 0.05f,
                      .frame_ms = 16.6f, .failed = false};
    assert(Decide(happy) == Skip::None);

    Input in = happy;
    in.enabled = false;
    assert(Decide(in) == Skip::Off);
    // Menus, movies and loading screens: the upscaler did not run.
    in = happy;
    in.upscaled = false;
    assert(Decide(in) == Skip::NoUpscale);
    in = happy;
    in.reset = true;
    assert(Decide(in) == Skip::Reset);
    // The water attack's second display pass rewrites the whole frame after the HUD.
    in = happy;
    in.ui_coverage = 0.31f;
    assert(Decide(in) == Skip::FullScreenPass);
    in.ui_coverage = 0.30f;
    assert(Decide(in) == Skip::None);
    in = happy;
    in.frame_ms = 25.1f;
    assert(Decide(in) == Skip::None);
    in.frame_ms = 100.0f;
    assert(Decide(in) == Skip::None);
    in.frame_ms = 25.0f;
    assert(Decide(in) == Skip::None);
    in = happy;
    in.failed = true;
    assert(Decide(in) == Skip::Failed);
    // A failure wins over everything but off.
    in.reset = true;
    assert(Decide(in) == Skip::Failed);

    // Both providers generate exactly one frame per real frame (2x).
    assert(GeneratedFrames(FgFsr) == 1);
    assert(GeneratedFrames(FgDlss) == 1);
    assert(GeneratedFrames(FgOff) == 0);
    assert(GeneratedFrames(-1) == 0);
    std::puts("Frame generation: decisions and frame counts PASS");
    return 0;
}
