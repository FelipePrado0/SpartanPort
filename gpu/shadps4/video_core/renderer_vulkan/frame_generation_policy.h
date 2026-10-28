// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: when frame generation adds a frame. No Vulkan: unit tested
// (tests/test_frame_generation.cpp).
#pragma once

namespace Vulkan::FrameGenerationPolicy {

/// spartanport.ini frame_generation: off, fsr, dlss.
enum Mode : int { FgOff = 0, FgFsr = 1, FgDlss = 2 };

enum class Skip { None, Off, NoUpscale, Reset, FullScreenPass, Failed };

struct Input {
    bool enabled;      ///< frame generation on and supported
    bool upscaled;     ///< the upscaler made this frame (not a menu, movie or loading screen)
    bool reset;        ///< upscaler reset, scene cut, preset or menu change
    float ui_coverage; ///< share of the frame drawn after the upscale, 0..1
    float frame_ms;    ///< real frame time
    bool failed;       ///< the context or a dispatch failed before
};

Skip Decide(const Input& in);

/// Generated frames per real frame. `requested` is frame_generation_frames (1..3); DLSS also
/// needs the card to allow them (`dlss_max`: 1 on an RTX 40, 3 on an RTX 50).
int GeneratedFrames(int mode, int requested, int dlss_max);

} // namespace Vulkan::FrameGenerationPolicy
