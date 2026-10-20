// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: one state line per render pass in the frame analyzer's capture (vk_frame_capture.cpp):
// guest colour and depth sample counts, the host sample count and the sample path the pipeline
// cache chose (vk_pipeline_cache.cpp), sample shading, stencil test/write/clear, the depth clear
// source, the depth layout, the attachment feedback loop and the render area against the depth
// target's size. Answers, from one capture, whether the game renders with MSAA or mixed samples,
// whether a pass stencil-tests or samples a stencil the emulator never cleared, and whether a
// render target is smaller than the pass's attachments.

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Vulkan::PassCensus {

enum class SamplePath : std::uint8_t {
    Equal,       ///< every target has the depth target's sample count
    MixedAmd,    ///< colour and depth differ, run through VK_AMD_mixed_attachment_samples
    MixedNv,     ///< colour and depth differ, run through VK_NV_framebuffer_mixed_samples
    ForcedEqual, ///< colour samples were forced to the depth samples (no mixed-sample extension)
};

inline const char* Name(SamplePath path) {
    switch (path) {
    case SamplePath::Equal:
        return "equal";
    case SamplePath::MixedAmd:
        return "mixed-amd";
    case SamplePath::MixedNv:
        return "mixed-nv";
    case SamplePath::ForcedEqual:
        return "forced-equal";
    }
    return "?";
}

/// What vk_pipeline_cache.cpp does with the guest sample counts: `color_samples` are the bound
/// colour targets' counts in order, `depth_samples` the depth target's (used only when
/// `db_enabled`), `mixed_any` whether VK_AMD_mixed_attachment_samples is enabled and
/// `mixed_depth` whether it or VK_NV_framebuffer_mixed_samples is.
inline SamplePath ResolveSamplePath(const std::vector<std::uint8_t>& color_samples,
                                    std::uint8_t depth_samples, bool db_enabled, bool mixed_any,
                                    bool mixed_depth) {
    if (color_samples.empty()) {
        return SamplePath::Equal;
    }
    bool all_same = true;
    for (const auto count : color_samples) {
        all_same &= count == color_samples.front();
    }
    const std::uint8_t depth = db_enabled ? depth_samples : 1;
    const bool depth_mismatch = db_enabled && color_samples.front() != depth;
    if ((!all_same && !mixed_any) || (all_same && depth_mismatch && !mixed_depth)) {
        return SamplePath::ForcedEqual;
    }
    if (!all_same || depth_mismatch) {
        return mixed_any ? SamplePath::MixedAmd : SamplePath::MixedNv;
    }
    return SamplePath::Equal;
}

enum class DepthClear : std::uint8_t {
    None,     ///< depth loaded
    Register, ///< DB_RENDER_CONTROL depth_clear_enable with depth writes on
    Htile,    ///< the HTILE metadata was marked cleared (texture_cache IsMetaCleared)
};

struct State {
    std::vector<std::uint8_t> color_samples; ///< guest counts of the bound colour targets
    std::uint8_t depth_samples{};            ///< guest count of the depth target, 0 without one
    std::uint8_t raster_samples{};           ///< the pipeline's rasterizationSamples
    SamplePath path{SamplePath::Equal};
    bool sample_shading{}; ///< sampleShadingEnable of the pass's first pipeline
    bool stencil_test{};
    bool stencil_write{};
    bool stencil_clear{};
    DepthClear depth_clear{DepthClear::None};
    std::string depth_layout; ///< vk::to_string of the depth attachment layout, empty without one
    bool feedback_loop{};
    std::pair<std::uint32_t, std::uint32_t> area{};       ///< render area
    std::pair<std::uint32_t, std::uint32_t> depth_size{}; ///< the depth image, 0x0 without one
};

inline std::string Format(const State& s) {
    std::string out = "state samples c";
    if (s.color_samples.empty()) {
        out += '0';
    }
    for (std::size_t i = 0; i < s.color_samples.size(); ++i) {
        out += (i ? "," : "") + std::to_string(s.color_samples[i]);
    }
    out += " d" + std::to_string(s.depth_samples) + " r" + std::to_string(s.raster_samples) +
           ' ' + Name(s.path) + " shading " + (s.sample_shading ? '1' : '0');
    out += std::string(" | stencil t") + (s.stencil_test ? '1' : '0') + " w" +
           (s.stencil_write ? '1' : '0') + " c" + (s.stencil_clear ? '1' : '0');
    out += std::string(" | zclear ") + (s.depth_clear == DepthClear::Htile      ? "htile"
                                        : s.depth_clear == DepthClear::Register ? "register"
                                                                                : "none");
    out += " | ds " + (s.depth_layout.empty() ? std::string("-") : s.depth_layout);
    out += std::string(" | fb ") + (s.feedback_loop ? '1' : '0');
    out += " | area " + std::to_string(s.area.first) + 'x' + std::to_string(s.area.second);
    if (s.depth_size.first && s.depth_size != s.area) {
        out += " (depth " + std::to_string(s.depth_size.first) + 'x' +
               std::to_string(s.depth_size.second) + ')';
    }
    return out;
}

} // namespace Vulkan::PassCensus
