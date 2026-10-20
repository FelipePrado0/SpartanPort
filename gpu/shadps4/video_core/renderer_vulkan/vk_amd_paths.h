// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: SPARTAN_AMD_PATHS_OFF=mixed,shaderext (or `all`). Runs the shared core without the device
// extensions only AMD drivers expose, so a tester on an RDNA2 card (#65) can tell an AMD-only
// code path of the port from the driver itself:
//   mixed      VK_AMD_mixed_attachment_samples is not enabled: passes whose colour and depth
//              sample counts differ get equal counts, as on Intel (vk_pipeline_cache.cpp)
//   shaderext  VK_AMD_shader_explicit_vertex_parameter, VK_AMD_gcn_shader and
//              VK_AMD_shader_trinary_minmax are not enabled: fragment shaders use
//              VK_KHR_fragment_shader_barycentric and the generic cube and min/max code
//              (changes the SPIR-V; folded into the shader binary cache version)

#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace Vulkan::AmdPathsOff {

struct Flags {
    bool mixed{};
    bool shaderext{};

    bool Any() const {
        return mixed || shaderext;
    }
    bool operator==(const Flags&) const = default;
};

/// Tokens separated by commas or spaces, case-insensitive; `all` or `1` turn both on, unknown
/// tokens are ignored.
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
            flags = Flags{.mixed = true, .shaderext = true};
        } else if (token == "mixed") {
            flags.mixed = true;
        } else if (token == "shaderext") {
            flags.shaderext = true;
        }
    }
    return flags;
}

/// The line a tester's last_run.log must contain; empty when nothing is on.
inline std::string Marker(Flags flags) {
    if (!flags.Any()) {
        return {};
    }
    std::string line = "GPU: amd paths off:";
    if (flags.mixed) {
        line += " mixed";
    }
    if (flags.shaderext) {
        line += " shaderext";
    }
    return line;
}

/// SPARTAN_AMD_PATHS_OFF, read once; the marker goes to stdout (last_run.log) the first time.
inline Flags Current() {
    static const Flags flags = [] {
        const Flags parsed = Parse(std::getenv("SPARTAN_AMD_PATHS_OFF"));
        if (parsed.Any()) {
            std::printf("%s\n", Marker(parsed).c_str());
        }
        return parsed;
    }();
    return flags;
}

} // namespace Vulkan::AmdPathsOff
