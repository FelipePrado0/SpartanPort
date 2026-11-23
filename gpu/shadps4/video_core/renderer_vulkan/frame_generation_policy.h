// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: when frame generation adds a frame. No Vulkan: unit tested
// (tests/test_frame_generation.cpp).
#pragma once

#include <chrono>

namespace Vulkan::FrameGenerationPolicy {

/// spartanport.ini frame_generation: off, fsr.
enum Mode : int { FgOff = 0, FgFsr = 1 };

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

/// Exactly one generated frame per real frame (2x) for AMD FSR.
int GeneratedFrames(int mode);

class Pacing {
public:
    using Clock = std::chrono::steady_clock;
    /// Called once per new real frame, before presenting either member of its pair.
    Clock::time_point RealDeadline(Clock::time_point ready);

private:
    Clock::time_point previous{};
    Clock::duration interval{};
    bool has_previous = false;
};

} // namespace Vulkan::FrameGenerationPolicy
