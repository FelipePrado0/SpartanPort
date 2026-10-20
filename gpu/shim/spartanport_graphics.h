// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: graphics options of the in-game menu (Insert / R3+L2), kept in spartanport.ini. The live ones
// apply on the next frame; the startup ones (patches, GPU readback modes) are read by run.py and
// apply through "Apply and restart". The launcher reads and writes the same keys.
#pragma once

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace SpartanGraphics {

enum DisplayMode : int { Windowed = 0, Borderless = 1, Fullscreen = 2, DisplayModeCount };
inline constexpr std::array<std::string_view, DisplayModeCount> DisplayModeKeys = {
    "windowed", "borderless", "fullscreen"};
inline constexpr std::array<const char*, DisplayModeCount> DisplayModeNames = {
    "Windowed", "Borderless", "Fullscreen"};

/// Present rate limits; 0 = unlimited.
inline constexpr std::array<int, 8> FpsLimits = {30, 60, 100, 120, 144, 180, 240, 0};

/// Game patch suffixes ("Resolution Patch - <name>"); "native" is no resolution patch.
/// The ones after 900p are the Ultrawide mod's sizes (#202) and the 16:10 and 4:3 ones, built in as beta
/// (scripts/graphics_options.py).
inline constexpr std::array<std::string_view, 23> Resolutions = {
    "native", "480p", "720p", "900p", "1440p", "1800p", "4K",
    "2560x1080", "3440x1440", "3840x1600", "5120x1440", "1920x1200", "2560x1600", "1280x800",
    "1440x900", "1360x768",
    "1280x960", "1440x1080", "1920x1440", "2048x1536", "2560x1920",
    "5120x2160", "8K"};
/// The resolutions to choose from: the built-in ones, then those of the enabled mods' patch files
/// (SPARTAN_EXTRA_RESOLUTIONS, ";"-separated sizes such as "3440x1440"; run.py sets it at every start).
inline std::vector<std::string> ResolutionNames() {
    std::vector<std::string> names(Resolutions.begin(), Resolutions.end());
    const char* extra = std::getenv("SPARTAN_EXTRA_RESOLUTIONS");
    for (std::string_view rest = extra ? extra : ""; !rest.empty();) {
        const std::size_t end = rest.find(';');
        const std::string_view name = rest.substr(0, end);
        if (!name.empty() && std::find(names.begin(), names.end(), name) == names.end()) names.emplace_back(name);
        rest = end == std::string_view::npos ? std::string_view() : rest.substr(end + 1);
    }
    return names;
}
/// The size in pixels of a resolution name: the built-in ones (as scripts/graphics_options.py SIZES)
/// or a mod's "<width>x<height>"; anything else is native.
inline constexpr std::array<std::pair<std::string_view, std::pair<int, int>>, 8> ResolutionSizes = {{
    {"native", {1920, 1080}}, {"480p", {854, 480}}, {"720p", {1280, 720}},
    {"900p", {1600, 900}}, {"1440p", {2560, 1440}}, {"1800p", {3200, 1800}}, {"4K", {3840, 2160}},
    {"8K", {7680, 4320}}}};
inline std::pair<int, int> ResolutionSize(std::string_view name) {
    for (const auto& [key, size] : ResolutionSizes) {
        if (key == name) return size;
    }
    const std::size_t x = name.find('x');
    if (x != std::string_view::npos) {
        const int w = std::atoi(std::string(name.substr(0, x)).c_str());
        const int h = std::atoi(std::string(name.substr(x + 1)).c_str());
        if (w > 0 && h > 0) return {w, h};
    }
    return ResolutionSizes[0].second;
}
/// Engine frame rates: 60 is the game's own (59.94). The launch takes the 240 FPS patch and the
/// rate is written live (SpartanSettings::SyncEngineFps); 100, 144 and 180 fit common high-refresh displays.
inline constexpr std::array<int, 6> EngineFps = {60, 100, 120, 144, 180, 240};

template <typename T, std::size_t N>
constexpr int IndexOf(const std::array<T, N>& values, const T& value, int fallback) {
    for (std::size_t i = 0; i < N; ++i) {
        if (values[i] == value) return int(i);
    }
    return fallback;
}

inline int ParseDisplayMode(std::string_view text) {
    return IndexOf(DisplayModeKeys, text, Windowed);
}
/// A listed limit, else unlimited.
inline int ParseFpsLimit(int value) {
    return IndexOf(FpsLimits, value, -1) >= 0 ? value : 0;
}
/// Index into ResolutionNames(); unknown text is native.
inline int ParseResolution(std::string_view text) {
    const std::vector<std::string> names = ResolutionNames();
    const auto found = std::find(names.begin(), names.end(), text);
    return found == names.end() ? 0 : int(found - names.begin());
}
/// A listed ceiling, else 120 (the default patch).
inline int ParseEngineFps(int value) {
    return IndexOf(EngineFps, value, -1) >= 0 ? value : 120;
}

/// Anisotropic filtering forced on the game's mipmapped textures; 0 = the game's own. Samplers
/// are cached, so it applies on restart.
inline constexpr std::array<int, 5> Anisotropies = {0, 2, 4, 8, 16};
inline int ParseAnisotropy(int value) {
    return IndexOf(Anisotropies, value, -1) >= 0 ? value : 0;
}

/// Frames the GPU command thread may queue ahead of the GPU; 0 = unbounded.
inline constexpr std::array<int, 3> FramesQueued = {1, 2, 0};
inline int ParseFramesQueued(int value) {
    return IndexOf(FramesQueued, value, -1) >= 0 ? value : 1;
}
/// RCAS strength in percent; invalid values fall back to 75.
inline int ParseRcasStrength(int value) {
    return value >= 0 && value <= 100 ? value : 75;
}
/// FSR RCAS attenuation for a strength: 0 is the sharpest, 2 the softest.
inline float RcasAttenuation(int strength) {
    return 2.0f * (1.0f - float(ParseRcasStrength(strength)) / 100.0f);
}
/// Whether the FSR pass runs: FSR 1 when the window is larger than the game's image, RCAS
/// alone when they are the same size. A smaller window is left to the regular scaling.
inline bool FsrPassRuns(bool fsr1, bool rcas, int in_w, int in_h, int out_w, int out_h) {
    const bool upscale = in_w < out_w && in_h < out_h;
    const bool same = in_w == out_w && in_h == out_h;
    return (fsr1 && upscale) || (rcas && same);
}

/// What the present limit really allows: the engine ceiling caps it.
inline int EffectiveFps(int limit, int engine_fps) {
    return limit == 0 || limit > engine_fps ? engine_fps : limit;
}

/// The present thread's frame limit: never above the engine rate, since the engine steps a fixed
/// time per frame and faster flips speed the game up. 0 stays vblank paced while the vblank is
/// the engine rate; a live rate below it (the vblank runs at the highest one) caps it.
inline int PresentLimit(int limit, int engine_fps, int vblank_hz) {
    return limit == 0 && vblank_hz <= engine_fps ? 0 : EffectiveFps(limit, engine_fps);
}

/// R3 + L1 + R1 / F9: the engine rate after `current`, up to `highest` (this launch's patch);
/// back to the first after the last.
inline int NextEngineFps(int current, int highest) {
    for (const int fps : EngineFps) {
        if (fps > current && fps <= highest) {
            return fps;
        }
    }
    return EngineFps[0];
}

} // namespace SpartanGraphics
