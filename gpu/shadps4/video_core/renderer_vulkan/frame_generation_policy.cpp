// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_vulkan/frame_generation_policy.h"

namespace Vulkan::FrameGenerationPolicy {

// Above this share of changed pixels a pass after the HUD rewrote the frame (the water attack
// distorts the whole screen): interpolating it would smear the HUD into the scene.
constexpr float kMaxUiCoverage = 0.30f;

Skip Decide(const Input& in) {
    if (!in.enabled) return Skip::Off;
    if (in.failed) return Skip::Failed;
    if (!in.upscaled) return Skip::NoUpscale;
    if (in.reset) return Skip::Reset;
    if (in.ui_coverage > kMaxUiCoverage) return Skip::FullScreenPass;
    return Skip::None;
}

int GeneratedFrames(int mode) {
    return mode == FgFsr ? 1 : 0;
}

Pacing::Clock::time_point Pacing::RealDeadline(Clock::time_point ready) {
    // Menus, pauses and clock discontinuities must not carry a long wait into gameplay.
    if (!has_previous || ready <= previous || ready - previous > std::chrono::milliseconds(250)) {
        interval = Clock::duration::zero();
        has_previous = true;
    } else {
        const auto sample = ready - previous;
        if (interval == Clock::duration::zero() || sample < interval) {
            interval = sample;
        } else {
            interval += (sample - interval) / 10;
        }
    }
    previous = ready;
    return ready + interval / 2;
}

} // namespace Vulkan::FrameGenerationPolicy
