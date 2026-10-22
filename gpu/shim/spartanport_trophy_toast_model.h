// SPDX-License-Identifier: GPL-2.0-or-later
// SpartanPort: the parts of the trophy notifications that need no window, sound or texture: the
// options in <user>/custom_trophy/notifications.ini, the animation files' settings, the
// timeline of a toast and the frame an animation shows. Kept apart for the tests.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace SpartanToast {

enum class Grade : int { Unknown, Bronze, Silver, Gold, Platinum };

/// The grade of a trophy by the colour of its icon (RGBA, `width` x `height`), the way the
/// launcher's Trophies page tells them apart (launcher/spartanport_trophies.py, trophy_tier): the
/// grades in the trophy package are encrypted. Unknown for an empty or all-dark icon.
Grade GradeFromIcon(const unsigned char* rgba, int width, int height);

enum class Theme : int {
    Original,      ///< the port's own notice at the top right (the default)
    Card,          ///< a dark card at the top right, with the trophy's art and an animated badge
    CardCinematic, ///< Card, and the platinum is held longer, with light around it
    Corner,        ///< a popup that rises in the bottom right corner: the artwork, name and detail
    Count
};

/// Indexed by Grade: 0 unknown, 1 bronze, 2 silver, 3 gold, 4 platinum.
constexpr std::array<const char*, 5> kGradeFileNames{"trophy", "bronze", "silver", "gold", "platinum"};

struct Options {
    Theme theme = Theme::Original;
    int volume = 80;              ///< 0..100, the notification sound only
    bool muted = false;
    float hold_seconds = 0.0f;    ///< time fully shown, 0..20; 0 = the theme's own
    int cinematic_intensity = 70; ///< 0..100, CardCinematic only
    bool grade_label = false;     ///< the grade's name before the second line ("Gold  ·  ...")
    bool platinum_animation = true; ///< false: the platinum shows its own artwork, like the others
    // Texts (empty: the built-in English). The player's own words, in any language (UTF-8).
    std::string text_unlocked;          ///< the card's second line, "Trophy unlocked!"
    std::string text_banner;            ///< the Original notice's first line (the launcher's own)
    std::array<std::string, 5> grade_names; ///< by grade, for grade_label (index 0 unused)

    bool operator==(const Options&) const = default;
};

const char* ThemeKey(Theme theme);   ///< in notifications.ini: original, card, card_cinematic, corner
const char* ThemeLabel(Theme theme); ///< in the menu
/// notifications.ini's text: unknown keys, bad values and comments are ignored, so a hand-edited
/// file never stops the game; a missing key keeps its default.
Options ParseOptions(std::string_view text);
/// The file the menu writes back, with a comment for every key.
std::string FormatOptions(const Options& options);
/// The first `max_bytes` bytes of a UTF-8 text, never ending inside a character.
std::string CutUtf8(std::string_view text, size_t max_bytes);
/// The text a toast shows: the option, else the English default.
std::string UnlockedText(const Options& options);
std::string GradeName(const Options& options, int grade);

/// <name>_anim.ini, beside an animation file. frames > 0 makes <name>_anim.png a sheet of that
/// many frames, `columns` wide, read left to right and top to bottom at `fps`. For a GIF or an
/// animated PNG the file's own timing is used and only `loop` and `delay_ms` apply.
struct AnimationSettings {
    int frames = 0;
    int columns = 1;
    float fps = 30.0f;
    bool loop = false;     ///< false: played once, then its last frame stays
    float delay_ms = -1.0f; ///< from the toast's start; < 0: the theme's own moment
};
AnimationSettings ParseAnimationSettings(std::string_view text);
/// End times (ms from the animation's start) of `frames` frames at `fps`.
std::vector<float> UniformFrameEnds(int frames, float fps);
/// The frame shown `ms` after the animation started; -1 before it starts or without frames.
int AnimationFrame(const std::vector<float>& frame_ends, float ms, bool loop);

// The timeline of one toast; times in ms from its start.
struct Timeline {
    float enter = 0.0f, hold = 0.0f, exit = 0.0f; // coming in, fully shown, going out
    float gap = 0.0f;                              // the pause before the next toast
    float badge_delay = 0.0f, badge_fade = 0.0f;   // the card's badge appears after the card
    float ring_first = 0.0f, ring_period = 0.0f, ring_len = 0.0f; // the ring around the badge
    float trophy_at = 0.0f;                        // the platinum's art starts its animation
    float Total() const { return enter + hold + exit; }
};

// Easing, shared by the timeline and the drawings: t in [0, 1].
inline float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
inline float EaseOutCubic(float t) { t = 1.0f - Clamp01(t); return 1.0f - t * t * t; }
inline float EaseInQuad(float t) { t = Clamp01(t); return t * t; }
inline float EaseInOutSine(float t) { return 0.5f - 0.5f * std::cos(Clamp01(t) * 3.14159265f); }
inline float EaseOutBack(float t) {
    t = Clamp01(t) - 1.0f;
    return 1.0f + t * t * (2.2f * t + 1.2f);
}

enum class Phase { Enter, Hold, Exit, Done };

struct Frame {
    Phase phase;
    float alpha;  ///< the card's opacity
    float offset; ///< card heights to the right of its place
    float drop;   ///< card heights above its place (the cinematic entrance)
    float badge;  ///< the badge's opacity
};

/// Everything follows from the time since the toast started, so a toast looks the same at 30,
/// 60, 120 or 144 FPS. settle: how far the card comes down as it enters (0: it does not).
Frame Evaluate(const Timeline& timeline, float settle, float ms);

} // namespace SpartanToast
