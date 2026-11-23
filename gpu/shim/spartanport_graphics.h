// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: graphics options of the in-game menu (Insert / R3+L2), kept in spartanport.ini. The live ones
// apply on the next frame; the startup ones (patches, GPU readback modes) are read by run.py and
// apply through "Apply and restart". The launcher reads and writes the same keys.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
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
inline constexpr std::array<std::string_view, 26> Resolutions = {
    "native", "480p", "720p", "900p", "1440p", "1800p", "4K",
    "2560x1080", "3440x1440", "3840x1600", "5120x1440", "1920x1200", "2560x1600", "1280x800",
    "1440x900", "1360x768",
    "1280x960", "1440x1080", "1920x1440", "2048x1536", "2560x1920",
    "5120x2160", "2880x1800", "5120x2880", "11520x2160", "8K"};
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
/// Engine frame rates: any whole rate from 30 to 240 (#831); 60 is the game's own (59.94). The
/// launch takes the 240 FPS patch and the rate is written live (SpartanSettings::SyncEngineFps).
inline constexpr int MinEngineFps = 30, MaxEngineFps = 240, EngineFpsStep = 5;

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
/// The primary display, the default when the launcher saved no monitor choice. -1 keeps it distinct
/// from an explicit index 0, which names the first display SDL3 lists even when it is not the primary.
inline constexpr int PrimaryMonitor = -1;
/// The display the window opens on: an index into the list window.cpp reads from SDL3, or
/// PrimaryMonitor. The value comes from the launcher (SPARTAN_MONITOR), so it is untrusted text:
/// empty, negative, non-numeric or too large for an int falls back to PrimaryMonitor.
inline int ParseMonitor(std::string_view text) {
    if (text.empty()) return PrimaryMonitor;
    int index = 0;
    for (const char digit : text) {
        if (digit < '0' || digit > '9') return PrimaryMonitor;
        if (index > (std::numeric_limits<int>::max() - (digit - '0')) / 10) return PrimaryMonitor;
        index = index * 10 + (digit - '0');
    }
    return index;
}
/// Whether HDR is on for the window's display (Windows HDR), from SDL's window properties
/// SDL_PROP_WINDOW_HDR_ENABLED_BOOLEAN and SDL_PROP_WINDOW_SDR_WHITE_LEVEL_FLOAT. SDL sets the first
/// only when the display's maximum is above its SDR white, so a display whose SDR white is at or
/// above its maximum (a 400-nit display with the Windows SDR slider at 400 nits or more) reports it
/// off. SDL leaves the SDR white level at 1 (80 nits) unless Windows drives the display in HDR10, so a
/// level above 1 says HDR is on too.
inline bool HdrOnForDisplay(bool sdl_hdr_enabled, float sdr_white_level) {
    return sdl_hdr_enabled || sdr_white_level > 1.0f;
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
/// A rate from 30 to 240, else 120 (the default).
inline int ParseEngineFps(int value) {
    return value >= MinEngineFps && value <= MaxEngineFps ? value : 120;
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
/// Picture controls of the finished SDR frame (post_process.frag, picture_adjust()). Live.
/// Gamma is the display gamma the picture is shaped for, in hundredths: 2.20 is a PC monitor and
/// leaves the picture as the game made it, 2.40 is the TVs the game was made for. Contrast scales
/// the encoded value around mid grey, in percent. Brightness sets the black level like a TV's
/// Brightness, at 1/400 of the range per step: +50 lifts black to 12.5% of the range and -50 crushes
/// the darkest 12.5% to black, while white stays white.
inline constexpr int PictureBrightnessMin = -50, PictureBrightnessMax = 50, PictureBrightnessNeutral = 0;
inline constexpr int PictureContrastMin = 50, PictureContrastMax = 150, PictureContrastNeutral = 100;
inline constexpr int PictureGammaMin = 180, PictureGammaMax = 280, PictureGammaNeutral = 220;
/// An ini or environment gamma: hundredths ("240") or the plain value ("2.4"); empty is neutral.
inline int ParsePictureGamma(std::string_view text) {
    if (text.empty()) return PictureGammaNeutral;
    const double value = std::atof(std::string{text}.c_str());
    if (std::isnan(value)) return PictureGammaNeutral;
    const double hundredths = value < 10.0 ? value * 100.0 : value;
    // Clamped while still a double, so an infinite or huge value never reaches the int conversion.
    return int(std::lround(std::clamp(hundredths, double(PictureGammaMin), double(PictureGammaMax))));
}
/// The power applied to the encoded value: 1 at 2.20.
inline float PicturePower(int gamma) {
    return float(std::clamp(gamma, PictureGammaMin, PictureGammaMax)) / float(PictureGammaNeutral);
}
inline float PictureContrast(int contrast) {
    return float(std::clamp(contrast, PictureContrastMin, PictureContrastMax)) / 100.0f;
}
inline float PictureBrightness(int brightness) {
    return float(std::clamp(brightness, PictureBrightnessMin, PictureBrightnessMax)) / 400.0f;
}
/// Whether the three controls leave the picture as the game made it. The post pass then skips
/// them, so the default picture is the same, bit for bit, as without the controls.
inline bool PictureNeutral(int brightness, int contrast, int gamma) {
    return PictureBrightness(brightness) == 0.0f && PictureContrast(contrast) == 1.0f &&
           PicturePower(gamma) == 1.0f;
}

/// HDR output (post_process.frag, pp.hdr 2), in nits. Live. Paper white is where the SDR picture's
/// white is shown; the highlights above it are boosted up to 1.5 times and fitted under the peak,
/// which should be the display's own peak brightness.
inline constexpr int HdrPaperWhiteMin = 80, HdrPaperWhiteMax = 500, HdrPaperWhiteDefault = 200;
inline constexpr int HdrPeakMin = 400, HdrPeakMax = 10000, HdrPeakDefault = 1000;
/// The highlight boost's maximum (post_process.frag highlight_gain): SDR white is shown at paper
/// white x 1.5, the brightest the expanded picture gets.
inline constexpr double HdrHighlightGain = 1.5;

/// "Match Windows HDR settings" (hdr_auto): paper white and peak from what SDL reports for the window's
/// display, read once at start. SDL's SDR white level (SDL_PROP_WINDOW_SDR_WHITE_LEVEL_FLOAT) is in
/// units of 80 nits and is Windows' SDR content brightness; its HDR headroom
/// (SDL_PROP_WINDOW_HDR_HEADROOM_FLOAT) is the display's maximum luminance as a multiple of SDR white.
/// SDL raises both to at least 1 and gives 1 when it reports nothing, so:
/// - paper white is known when either is above 1 (a white level of 1 with a headroom above 1 is the
///   Windows slider at 80 nits);
/// - the peak is known only when the headroom is above 1. At 1 the display's maximum is lost: SDL
///   also gives 1 when Windows' SDR white is at or above the maximum (a 400-nit display with SDR
///   white at 480 nits).
/// A value that is not known keeps its default here, and the sliders' value is used for it.
struct HdrNits {
    int paper_white = HdrPaperWhiteDefault;
    int peak = HdrPeakDefault;
    bool paper_white_known = false;
    bool peak_known = false;
};
inline HdrNits HdrFromDisplay(float sdr_white_level, float hdr_headroom) {
    // Anything not a finite number above 1 counts as not reported (1, SDL's floor).
    const auto reported = [](float value) { return std::isfinite(value) && value > 1.0f ? double(value) : 1.0; };
    const double white = reported(sdr_white_level), headroom = reported(hdr_headroom);
    HdrNits nits;
    nits.paper_white_known = white > 1.0 || headroom > 1.0;
    nits.peak_known = headroom > 1.0;
    if (nits.paper_white_known) {
        nits.paper_white = int(std::lround(std::clamp(white * 80.0, double(HdrPaperWhiteMin), double(HdrPaperWhiteMax))));
    }
    if (nits.peak_known) {
        nits.peak = int(std::lround(std::clamp(white * 80.0 * headroom, double(HdrPeakMin), double(HdrPeakMax))));
    }
    return nits;
}

/// SMPTE ST 2084 (PQ): nits to the PQ value in [0, 1], and back.
inline double HdrPq(double nits) {
    const double m1 = 0.1593017578125, m2 = 78.84375, c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
    const double y = std::pow(std::clamp(nits / 10000.0, 0.0, 1.0), m1);
    return std::pow((c1 + c2 * y) / (1.0 + c3 * y), m2);
}
inline double HdrPqToNits(double pq) {
    const double m1 = 0.1593017578125, m2 = 78.84375, c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
    const double p = std::pow(std::clamp(pq, 0.0, 1.0), 1.0 / m2);
    return 10000.0 * std::pow(std::max(p - c1, 0.0) / (c2 - c3 * p), 1.0 / m1);
}

/// The highlight roll-off of HDR output (post_process.frag sdr_to_hdr10()), for F12 screenshots to
/// undo: ITU-R BT.2390's EETF on a pixel's brightest channel in PQ, from the source maximum LW =
/// paper white x 1.5 to the peak, black at 0. With E the PQ value over PQ(LW) and maxLum =
/// PQ(peak) / PQ(LW), it is the identity up to KS = 1.5 maxLum - 0.5, then the Hermite spline
/// P(T) = (2T^3 - 3T^2 + 1) KS + (T^3 - 2T^2 + T)(1 - KS) + (-2T^3 + 3T^2) maxLum, T = (E - KS) / (1 - KS).
/// With that KS, 1 - KS = 3 (maxLum - KS), so P(T) = KS + (maxLum - KS)(1 - (1 - T)^3), which
/// Undo() solves for T in closed form. When LW fits under the peak (maxLum >= 1) nothing is rolled off.
class HdrRollOff {
public:
    HdrRollOff(double paper_white, double peak)
        : source_pq{HdrPq(paper_white * HdrHighlightGain)}, max_lum{HdrPq(peak) / source_pq},
          ks{1.5 * max_lum - 0.5},
          knee_nits{max_lum < 1.0 ? HdrPqToNits(ks * source_pq) : std::numeric_limits<double>::infinity()} {}

    /// The brightest channel before the roll-off, from the one after it (nits). An input at or
    /// above the peak gives LW.
    double Undo(double out_nits) const {
        if (!(out_nits > knee_nits)) {
            return out_nits;
        }
        const double e2 = HdrPq(out_nits) / source_pq;
        const double rest = std::max(1.0 - (e2 - ks) / (max_lum - ks), 0.0); // (1 - T)^3
        const double t = 1.0 - std::cbrt(rest);
        return HdrPqToNits((ks + t * (1.0 - ks)) * source_pq);
    }

private:
    double source_pq, max_lum, ks, knee_nits;
};

/// Whether the FSR pass runs: FSR 1 when the window is larger than the game's image, RCAS
/// alone when they are the same size. A smaller window is left to the regular scaling.
inline bool FsrPassRuns(bool fsr1, bool rcas, int in_w, int in_h, int out_w, int out_h) {
    const bool upscale = in_w < out_w && in_h < out_h;
    const bool same = in_w == out_w && in_h == out_h;
    return (fsr1 && upscale) || (rcas && same);
}

/// Anti-aliasing of the finished frame (host_passes/aa_pass.cpp), applied before the sharpening. Live.
/// FXAA is one cheap pass that softens edges a little; SMAA 1x takes three and keeps the picture
/// sharper. Off by default: it changes the picture.
enum AntiAliasing : int { AaOff = 0, AaFxaa = 1, AaSmaa = 2, AntiAliasingCount };
inline constexpr std::array<std::string_view, AntiAliasingCount> AntiAliasingKeys = {"off", "fxaa", "smaa"};
inline constexpr std::array<const char*, AntiAliasingCount> AntiAliasingNames = {"Off", "FXAA", "SMAA"};
/// A key of spartanport.ini or SPARTAN_ANTI_ALIASING (also the menu's index and the old on/off
/// spelling); anything else is off.
inline int ParseAntiAliasing(std::string_view text) {
    if (const int found = IndexOf(AntiAliasingKeys, text, -1); found >= 0) return found;
    if (text == "none" || text == "0" || text == "false") return AaOff;
    if (text == "1" || text == "true") return AaFxaa;
    if (text == "2") return AaSmaa;
    return AaOff;
}
/// Whether the anti-aliasing pass runs on an image of this size: a mode is chosen and the image
/// is big enough for the filters to read their neighbourhood.
inline bool AaPassRuns(int mode, int width, int height) {
    return (mode == AaFxaa || mode == AaSmaa) && width >= 16 && height >= 16;
}
/// Whether the colour values the presenter reads are linear light (an sRGB view or a plain 10-bit
/// buffer, which the post pass encodes) or already perceptual (10-bit sRGB values, HDR PQ): the edge
/// tests work on perceptual luma either way.
inline bool AaInputIsLinear(bool srgb_encoded_10bit, bool hdr) {
    return !srgb_encoded_10bit && !hdr;
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

/// In-game arrows, R3 + R1 / L1 and F9 + arrows: the next stop from `current` toward `direction`
/// (1 up, -1 down), stops every EngineFpsStep plus 144 (common displays). Both ends stop, and
/// never above `highest` (this launch's patch).
inline int StepEngineFps(int current, int direction, int highest) {
    const int top = std::clamp(highest, MinEngineFps, MaxEngineFps);
    current = std::clamp(current, MinEngineFps, top);
    int next = direction > 0 ? (current / EngineFpsStep + 1) * EngineFpsStep
                             : (current - 1) / EngineFpsStep * EngineFpsStep;
    if (direction > 0 ? current < 144 && next > 144 : current > 144 && next < 144) next = 144;
    return std::clamp(next, MinEngineFps, top);
}

} // namespace SpartanGraphics
