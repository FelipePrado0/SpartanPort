// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
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

int GeneratedFrames(int mode, int requested, int dlss_max) {
    if (mode == FgFsr) return 1;
    if (mode == FgDlss) return std::clamp(std::min(requested, dlss_max), 1, 3);
    return 0;
}

} // namespace Vulkan::FrameGenerationPolicy
