// SPDX-License-Identifier: GPL-2.0-or-later
#include "spartanport_trophy_toast_model.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <locale>
#include <sstream>

namespace SpartanToast {

namespace {

constexpr std::array<const char*, int(Theme::Count)> kThemeKeys{"original", "card", "card_cinematic", "corner"};
constexpr std::array<const char*, int(Theme::Count)> kThemeLabels{"Original", "Card",
                                                                  "Card, cinematic platinum", "Corner popup"};
constexpr std::array<const char*, 5> kGradeNames{"", "Bronze", "Silver", "Gold", "Platinum"};

std::string_view Trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

// Calls `on(key, value)` for every "key=value" line; '#' starts a comment line.
template <typename F>
void ForEachLine(std::string_view text, F&& on) {
    if (text.size() >= 3 && text.substr(0, 3) == "\xEF\xBB\xBF") text.remove_prefix(3); // UTF-8 BOM
    while (!text.empty()) {
        const size_t end = std::min(text.find('\n'), text.size());
        const std::string_view line = Trim(text.substr(0, end));
        text.remove_prefix(std::min(end + 1, text.size()));
        if (line.empty() || line.front() == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string_view::npos) continue;
        on(Trim(line.substr(0, eq)), Trim(line.substr(eq + 1)));
    }
}

bool ToInt(std::string_view v, int& out) {
    const auto [end, ec] = std::from_chars(v.data(), v.data() + v.size(), out);
    return ec == std::errc() && end == v.data() + v.size();
}

// "1.5" whatever the process's locale (the game's own code may change it).
bool ToFloat(std::string_view v, float& out) {
    std::istringstream in{std::string(v)};
    in.imbue(std::locale::classic());
    in >> std::noskipws >> out;
    return !v.empty() && in && in.peek() == std::char_traits<char>::eof() && std::isfinite(out);
}

bool ToBool(std::string_view v, bool& out) {
    if (v == "1" || v == "true" || v == "yes" || v == "on") return out = true, true;
    if (v == "0" || v == "false" || v == "no" || v == "off") return out = false, true;
    return false;
}

// One line of text: a value never spans lines, and a toast never shows more than this.
std::string TextValue(std::string_view v) {
    std::string s = CutUtf8(v, 200);
    std::replace(s.begin(), s.end(), '\t', ' ');
    return s;
}

} // namespace

std::string CutUtf8(std::string_view text, size_t max_bytes) {
    if (text.size() <= max_bytes) return std::string(text);
    size_t end = max_bytes;
    // Back to the start of the character the cut falls in (continuation bytes are 10xxxxxx).
    while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) --end;
    return std::string(text.substr(0, end));
}

const char* ThemeKey(Theme theme) {
    const int i = int(theme);
    return i >= 0 && i < int(Theme::Count) ? kThemeKeys[i] : kThemeKeys[0];
}

const char* ThemeLabel(Theme theme) {
    const int i = int(theme);
    return i >= 0 && i < int(Theme::Count) ? kThemeLabels[i] : "";
}

Options ParseOptions(std::string_view text) {
    Options o;
    ForEachLine(text, [&](std::string_view key, std::string_view value) {
        int i = 0;
        float f = 0.0f;
        bool b = false;
        if (key == "theme") {
            for (int t = 0; t < int(Theme::Count); ++t)
                if (value == kThemeKeys[t]) o.theme = Theme(t);
        } else if (key == "volume") {
            if (ToInt(value, i)) o.volume = std::clamp(i, 0, 100);
        } else if (key == "muted") {
            if (ToBool(value, b)) o.muted = b;
        } else if (key == "hold_seconds") {
            if (ToFloat(value, f)) o.hold_seconds = std::clamp(f, 0.0f, 20.0f);
        } else if (key == "cinematic_intensity") {
            if (ToInt(value, i)) o.cinematic_intensity = std::clamp(i, 0, 100);
        } else if (key == "grade_label") {
            if (ToBool(value, b)) o.grade_label = b;
        } else if (key == "platinum_animation") {
            if (ToBool(value, b)) o.platinum_animation = b;
        } else if (key == "text_unlocked") {
            o.text_unlocked = TextValue(value);
        } else if (key == "text_banner") {
            o.text_banner = TextValue(value);
        } else {
            for (int g = 1; g < 5; ++g)
                if (key == std::string("name_") + kGradeFileNames[g]) o.grade_names[g] = TextValue(value);
        }
    });
    return o;
}

std::string FormatOptions(const Options& o) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "# Trophy notifications. The game's menu (Insert / R3+L2 > Overlay) writes this file;\n"
        << "# it can also be edited by hand while the game is closed. Sounds, pictures and\n"
        << "# animations go beside it, see docs/TROPHIES.md (Notifications).\n"
        << "# theme: original, card, card_cinematic, corner\n"
        << "theme=" << ThemeKey(o.theme) << "\n"
        << "# volume: 0-100; muted: 0 or 1\n"
        << "volume=" << o.volume << "\n"
        << "muted=" << int(o.muted) << "\n"
        << "# hold_seconds: how long a toast stays, 0 = the theme's own\n"
        << "hold_seconds=" << o.hold_seconds << "\n"
        << "# cinematic_intensity: 0-100, the light of card_cinematic's platinum\n"
        << "cinematic_intensity=" << o.cinematic_intensity << "\n"
        << "# grade_label: 1 writes the grade's name before the second line\n"
        << "grade_label=" << int(o.grade_label) << "\n"
        << "# platinum_animation: 0 shows the platinum trophy's own artwork instead of an animation\n"
        << "platinum_animation=" << int(o.platinum_animation) << "\n"
        << "# Texts, empty for the defaults: the card's second line, the first line of the\n"
        << "# Original notice, and the grade names grade_label writes.\n"
        << "text_unlocked=" << o.text_unlocked << "\n"
        << "text_banner=" << o.text_banner << "\n";
    for (int g = 1; g < 5; ++g) out << "name_" << kGradeFileNames[g] << "=" << o.grade_names[g] << "\n";
    return out.str();
}

Grade GradeFromIcon(const unsigned char* rgba, int width, int height) {
    if (!rgba || width <= 0 || height <= 0) return Grade::Unknown;
    // The icon shrunk to 24 x 24 (each cell the mean of its pixels), then the mean colour of the
    // cells that are not dark: bluish is platinum; otherwise the spread of its channels tells grey
    // silver, muted bronze and saturated gold apart. Alpha is ignored, as the launcher drops it.
    constexpr int kCells = 24;
    double sum[3] = {0.0, 0.0, 0.0};
    int lit = 0;
    for (int cy = 0; cy < kCells; ++cy) {
        const int y0 = cy * height / kCells, y1 = std::max(y0 + 1, (cy + 1) * height / kCells);
        for (int cx = 0; cx < kCells; ++cx) {
            const int x0 = cx * width / kCells, x1 = std::max(x0 + 1, (cx + 1) * width / kCells);
            double cell[3] = {0.0, 0.0, 0.0};
            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x)
                    for (int c = 0; c < 3; ++c) cell[c] += rgba[(size_t(y) * width + x) * 4 + c];
            const double n = double(y1 - y0) * double(x1 - x0);
            for (double& v : cell) v = std::round(v / n);
            if (std::max({cell[0], cell[1], cell[2]}) <= 60.0) continue;
            for (int c = 0; c < 3; ++c) sum[c] += cell[c];
            ++lit;
        }
    }
    if (!lit) return Grade::Unknown;
    const double r = sum[0] / lit, g = sum[1] / lit, b = sum[2] / lit;
    const double high = std::max({r, g, b}), spread = (high - std::min({r, g, b})) / high;
    if (b >= r) return Grade::Platinum;
    return spread < 0.4 ? Grade::Silver : spread < 0.72 ? Grade::Bronze : Grade::Gold;
}

std::string UnlockedText(const Options& o) {
    return o.text_unlocked.empty() ? "Trophy unlocked!" : o.text_unlocked;
}

std::string GradeName(const Options& o, int grade) {
    if (grade < 1 || grade > 4) return "";
    return o.grade_names[grade].empty() ? kGradeNames[grade] : o.grade_names[grade];
}

AnimationSettings ParseAnimationSettings(std::string_view text) {
    AnimationSettings a;
    ForEachLine(text, [&](std::string_view key, std::string_view value) {
        int i = 0;
        float f = 0.0f;
        bool b = false;
        if (key == "frames") {
            if (ToInt(value, i)) a.frames = std::clamp(i, 0, 4096);
        } else if (key == "columns") {
            if (ToInt(value, i)) a.columns = std::clamp(i, 1, 256);
        } else if (key == "fps") {
            if (ToFloat(value, f)) a.fps = std::clamp(f, 1.0f, 240.0f);
        } else if (key == "loop") {
            if (ToBool(value, b)) a.loop = b;
        } else if (key == "delay_ms") {
            if (ToFloat(value, f)) a.delay_ms = std::clamp(f, 0.0f, 20000.0f);
        }
    });
    return a;
}

std::vector<float> UniformFrameEnds(int frames, float fps) {
    std::vector<float> ends;
    if (frames <= 0 || fps <= 0.0f) return ends;
    ends.reserve(size_t(frames));
    for (int i = 1; i <= frames; ++i) ends.push_back(float(i) * 1000.0f / fps);
    return ends;
}

int AnimationFrame(const std::vector<float>& ends, float ms, bool loop) {
    if (ends.empty() || ms < 0.0f) return -1;
    const float total = ends.back();
    if (ms >= total) {
        if (!loop || total <= 0.0f) return int(ends.size()) - 1;
        ms = std::fmod(ms, total);
    }
    return int(std::upper_bound(ends.begin(), ends.end(), ms) - ends.begin());
}

Frame Evaluate(const Timeline& tl, float settle, float ms) {
    ms = std::max(ms, 0.0f);
    if (ms >= tl.Total()) return {Phase::Done, 0.0f, 0.0f, 0.0f, 0.0f};
    const float badge = tl.badge_fade > 0.0f ? EaseOutCubic((ms - tl.badge_delay) / tl.badge_fade) : 1.0f;
    if (ms >= tl.enter + tl.hold) { // exit: fades while drifting right
        const float q = tl.exit > 0.0f ? (ms - tl.enter - tl.hold) / tl.exit : 1.0f;
        return {Phase::Exit, 1.0f - EaseInQuad(q), EaseInQuad(q), 0.0f, badge};
    }
    if (ms >= tl.enter) return {Phase::Hold, 1.0f, 0.0f, 0.0f, badge};
    const float p = ms / tl.enter, e = EaseOutCubic(p);
    return {Phase::Enter, e, 1.0f - e, (1.0f - EaseOutBack(p)) * settle, badge};
}

} // namespace SpartanToast
