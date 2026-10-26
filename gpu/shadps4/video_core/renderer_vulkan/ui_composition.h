// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace Vulkan::UiComposition {

enum class Background { None, Copy, Temporal };

// DLAA writes at the scene size; only the final copy stretches the two-pixel inset.
constexpr std::array<uint32_t, 2> TemporalOutputSize(uint32_t render_width, uint32_t render_height,
                                                  uint32_t output_width, uint32_t output_height,
                                                  bool native_aa) {
    return native_aa ? std::array{render_width, render_height}
                     : std::array{output_width, output_height};
}

// UI composition must not require scene depth/camera or an active FSR context.
constexpr Background Choose(bool scaled, bool ui_draw, bool scene_ready, bool fsr_active) {
    if (!scaled || !ui_draw) {
        return Background::None;
    }
    return scene_ready && fsr_active ? Background::Temporal : Background::Copy;
}

inline bool NativeViewport(float width, float height) {
    return std::abs(std::abs(width) - 1920.0f) < 0.5f &&
           std::abs(std::abs(height) - 1080.0f) < 0.5f;
}

// Scaleform draws, including the first stencil/movie pass. A native-size viewport
// alone also matches every fullscreen post pass when guest targets stay at 1080p.
constexpr bool MovieShader(uint64_t hash) {
    return hash == 0x34e8a281 || hash == 0x81d336ce || hash == 0x09957251 ||
           hash == 0x24042a9b || hash == 0xa400228b;
}

inline std::array<float, 2> Scale(uint32_t guest_width, uint32_t guest_height,
                                uint32_t output_width, uint32_t output_height,
                                bool native_coordinates) {
    return {float(output_width) / float(native_coordinates ? 1920 : guest_width),
            float(output_height) / float(native_coordinates ? 1080 : guest_height)};
}

} // namespace Vulkan::UiComposition
