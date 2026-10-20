// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: SPARTAN_LIGHTING_TEST=shading,msdepth,dref,bias (or `all`). The four changes of the
// KrachDev/shadPS4_GOW3 build "God of War III Remastered - Lighting, Scanline & AMD Fix" (commit
// eaa1027f), which its author reports removes the black scanlines on an RX 6600, each behind its
// own token so a tester can bisect them on the RDNA2 black-lines issue (#65):
//   shading  sampleShadingEnable always false (vk_graphics_pipeline.cpp)
//   msdepth  colour-to-MSAA-depth blit falls back to the red channel for samples that read 0
//            (host_shaders/color_to_ms_depth.frag)
//   dref     shadow comparisons broadcast the result to all four channels instead of
//            (v, 0, 0, 0) (shader_recompiler emit_spirv_image.cpp; changes the SPIR-V)
//   bias     slope-scaled depth bias passed as the guest gives it, not divided by 16
//            (vk_rasterizer.cpp)
// The tokens that change recompiled shaders are folded into the shader binary cache version
// (vk_pipeline_serialization.cpp), so no cache clearing is needed between runs.

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace Vulkan::LightingTest {

struct Flags {
    bool shading{};
    bool msdepth{};
    bool dref{};
    bool bias{};

    bool Any() const {
        return shading || msdepth || dref || bias;
    }
    bool operator==(const Flags&) const = default;
};

/// Tokens separated by commas or spaces, case-insensitive; `all` or `1` turn every token on,
/// unknown tokens are ignored.
inline Flags Parse(const char* text) {
    Flags flags;
    if (!text) {
        return flags;
    }
    std::string_view rest{text};
    while (!rest.empty()) {
        const auto end = rest.find_first_of(", ");
        std::string token{rest.substr(0, end)};
        rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end + 1);
        for (auto& c : token) {
            c = char(std::tolower(static_cast<unsigned char>(c)));
        }
        if (token == "all" || token == "1") {
            flags = Flags{.shading = true, .msdepth = true, .dref = true, .bias = true};
        } else if (token == "shading") {
            flags.shading = true;
        } else if (token == "msdepth") {
            flags.msdepth = true;
        } else if (token == "dref") {
            flags.dref = true;
        } else if (token == "bias") {
            flags.bias = true;
        }
    }
    return flags;
}

/// The line a tester's last_run.log must contain; empty when nothing is on.
inline std::string Marker(Flags flags) {
    if (!flags.Any()) {
        return {};
    }
    std::string line = "GPU: lighting test on:";
    if (flags.shading) {
        line += " shading";
    }
    if (flags.msdepth) {
        line += " msdepth";
    }
    if (flags.dref) {
        line += " dref";
    }
    if (flags.bias) {
        line += " bias";
    }
    return line;
}

/// Bits for the knobs that change recompiled SPIR-V: `dref` here and `shaderext` of
/// vk_amd_paths.h.
inline std::uint32_t ShaderBinarySalt(bool dref, bool shaderext) {
    return (dref ? 1u : 0u) | (shaderext ? 2u : 0u);
}

/// The shader binary cache version for a salt: distinct from every plain version bump.
inline std::uint32_t ShaderBinaryVersion(std::uint32_t base, std::uint32_t salt) {
    return base + 16u * salt;
}

/// SPARTAN_LIGHTING_TEST, read once; the marker goes to stdout (last_run.log) the first time.
inline Flags Current() {
    static const Flags flags = [] {
        const Flags parsed = Parse(std::getenv("SPARTAN_LIGHTING_TEST"));
        if (parsed.Any()) {
            std::printf("%s\n", Marker(parsed).c_str());
        }
        return parsed;
    }();
    return flags;
}

} // namespace Vulkan::LightingTest
