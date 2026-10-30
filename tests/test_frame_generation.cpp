// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <cstdio>
#include <chrono>
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
    assert(GeneratedFrames(2) == 0);
    assert(GeneratedFrames(FgOff) == 0);
    assert(GeneratedFrames(-1) == 0);
    using namespace std::chrono;
    using Clock = Pacing::Clock;
    // Complete real-frame cycles, including rates whose half-cycle is below 4 ms.
    for (const int fps : {30, 60, 120, 200, 240, 500}) {
        Pacing pacing;
        auto ready = Clock::time_point{};
        const auto period = duration_cast<Clock::duration>(nanoseconds(1000000000 / fps));
        assert(pacing.RealDeadline(ready) == ready);
        for (int i = 0; i < 60; ++i) {
            ready += period;
            const auto deadline = pacing.RealDeadline(ready);
            assert(deadline == ready + period / 2);
            // A generated present taking longer than half a cycle leaves no future wait.
            assert(deadline < ready + period * 3 / 4);
        }
    }
    Pacing pacing;
    auto ready = Clock::time_point{};
    assert(pacing.RealDeadline(ready) == ready);
    ready += milliseconds(20);
    assert(pacing.RealDeadline(ready) == ready + milliseconds(10));
    // A faster producer must not inherit the old long wait.
    ready += milliseconds(4);
    assert(pacing.RealDeadline(ready) == ready + milliseconds(2));
    // A slower producer may smooth its new interval but never exceed half its cycle.
    ready += milliseconds(20);
    const auto slower = pacing.RealDeadline(ready);
    assert(slower > ready + milliseconds(2) && slower <= ready + milliseconds(10));
    // A pause or an invalid clock sample discards the old estimate, not the generated frame.
    ready += seconds(1);
    assert(pacing.RealDeadline(ready) == ready);
    ready += milliseconds(4);
    assert(pacing.RealDeadline(ready) == ready + milliseconds(2));
    assert(pacing.RealDeadline(ready) == ready);
    ready -= milliseconds(1);
    assert(pacing.RealDeadline(ready) == ready);
    std::puts("Frame generation: decisions, frame counts and pacing PASS");
    return 0;
}
