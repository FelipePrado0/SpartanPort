// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace Vulkan::UiComposition {

enum class Background { None, Copy, Temporal };

template <typename Blend>
constexpr uint32_t ColorMask(uint64_t shader, uint32_t mask, const Blend& blend) {
    // Verified backdrop PS exports (0,0,0,0) to RT1; omit only a destination-preserving blend.
    if (shader != 0xbcb456ff6dd871a8ull || mask != 3 || !blend.enable) {
        return mask;
    }
    using Factor = typename Blend::BlendFactor;
    using Function = typename Blend::BlendFunc;
    const auto preserves = [](Function function, Factor destination) {
        return function == Function::Add &&
               (destination == Factor::One || destination == Factor::OneMinusSrcAlpha);
    };
    const auto alpha_function = blend.separate_alpha_blend ? blend.alpha_func : blend.color_func;
    const auto alpha_destination =
        blend.separate_alpha_blend ? blend.alpha_dst_factor : blend.color_dst_factor;
    if (preserves(blend.color_func, blend.color_dst_factor) &&
        preserves(alpha_function, alpha_destination)) {
        return 1;
    }
    return mask;
}

struct DisplayDrawPlan {
    bool seed;
    bool discard;
};

constexpr DisplayDrawPlan PlanDisplayDraw(uint64_t shader, bool samples_ui, bool replaces_color,
                                          bool has_depth, bool seeded) {
    // Verified display-copy PS: samples pixel coordinates / actual image size, then decodes sRGB.
    const bool copy = shader == 0x6d2dc8a47f6d9c23ull && samples_ui && replaces_color && !has_depth;
    return {copy && !seeded, copy};
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
