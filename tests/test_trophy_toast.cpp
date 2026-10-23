// SPDX-License-Identifier: GPL-2.0-or-later
// The trophy notifications without a window: notifications.ini, the animation settings, the
// timeline, the frame an animation shows, the grade told by an icon's colour, and the player's
// files decoded (a GIF, a WAV and a broken file written here).
//   trophy-toast-test SCRATCH_DIR [trophy00.trp]
// With the game's own trophy00.trp, the grades of its 35 icons are checked too (0 platinum, 1-18
// bronze, 19-29 silver, 30-34 gold); nothing of the game is needed otherwise.
#include <algorithm>
#include <cassert>
#include <clocale>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <locale>
#include <stdexcept>
#include <string>
#include <vector>

#include "spartanport_png.h"
#include "spartanport_trophy_media.h"
#include "spartanport_trophy_rarity.h"
#include "spartanport_trophy_toast_model.h"
#include "spartanport_trophy_texts.h"
extern "C" {
#include <libavutil/log.h>
}

namespace fs = std::filesystem;
using namespace SpartanToast;

static bool Near(float a, float b, float eps = 0.01f) { return std::fabs(a - b) <= eps; }

static void Write(const fs::path& path, const std::string& data) {
    std::ofstream out(path, std::ios::binary);
    out.write(data.data(), std::streamsize(data.size()));
    assert(out.good());
}

static void Le16(std::string& s, unsigned v) {
    s.push_back(char(v & 0xFF));
    s.push_back(char((v >> 8) & 0xFF));
}
static void Le32(std::string& s, unsigned v) {
    Le16(s, v & 0xFFFF);
    Le16(s, v >> 16);
}

// A GIF of `colors.size()` frames, each one plain colour (palette index), each with its delay in
// hundredths of a second. LZW without compression: a clear code before every pixel keeps every
// code 3 bits wide.
static std::string Gif(int width, int height, const std::vector<int>& colors, const std::vector<int>& delays) {
    std::string g = "GIF89a";
    Le16(g, unsigned(width));
    Le16(g, unsigned(height));
    g += char(0x81); // a global palette of 4 colours
    g += char(0);
    g += char(0);
    const unsigned char palette[4][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}};
    for (const auto& c : palette) g.append(reinterpret_cast<const char*>(c), 3);
    for (size_t f = 0; f < colors.size(); ++f) {
        g += "\x21\xF9\x04";
        g += char(0x04); // disposal: leave in place, no transparency
        Le16(g, unsigned(delays[f]));
        g += char(0);
        g += char(0);
        g += char(0x2C);
        Le16(g, 0);
        Le16(g, 0);
        Le16(g, unsigned(width));
        Le16(g, unsigned(height));
        g += char(0);
        g += char(2); // minimum code size: clear = 4, end = 5, codes of 3 bits
        std::vector<unsigned char> bytes;
        unsigned acc = 0;
        int bits = 0;
        auto code = [&](unsigned c) {
            acc |= c << bits;
            bits += 3;
            while (bits >= 8) {
                bytes.push_back(static_cast<unsigned char>(acc & 0xFF));
                acc >>= 8;
                bits -= 8;
            }
        };
        for (int p = 0; p < width * height; ++p) {
            code(4);
            code(unsigned(colors[f]));
        }
        code(5);
        if (bits) bytes.push_back(static_cast<unsigned char>(acc & 0xFF));
        for (size_t at = 0; at < bytes.size(); at += 255) {
            const size_t n = std::min<size_t>(255, bytes.size() - at);
            g += char(n);
            g.append(reinterpret_cast<const char*>(bytes.data() + at), n);
        }
        g += char(0);
    }
    g += char(0x3B);
    return g;
}

// 16-bit mono PCM: a quarter of a second of a 440 Hz tone at 8 kHz.
static std::string Wav() {
    const unsigned rate = 8000, samples = rate / 4;
    std::string w = "RIFF";
    Le32(w, 36 + samples * 2);
    w += "WAVEfmt ";
    Le32(w, 16);
    Le16(w, 1);
    Le16(w, 1);
    Le32(w, rate);
    Le32(w, rate * 2);
    Le16(w, 2);
    Le16(w, 16);
    w += "data";
    Le32(w, samples * 2);
    for (unsigned i = 0; i < samples; ++i)
        Le16(w, unsigned(std::int16_t(8000.0 * std::sin(6.2831853 * 440.0 * i / rate))) & 0xFFFF);
    return w;
}

static void TestOptions() {
    const Options defaults = ParseOptions("");
    assert(defaults == Options{});
    assert(defaults.theme == Theme::Original && defaults.volume == 80 && !defaults.muted);

    // Values, clamping, comments, a BOM, CRLF and spaces around '='.
    const Options o = ParseOptions("\xEF\xBB\xBF# comment\r\n"
                                   "theme = card_cinematic\r\n"
                                   "volume=250\r\n"
                                   "muted=yes\n"
                                   "hold_seconds=99\n"
                                   "cinematic_intensity=-5\n"
                                   "grade_label=1\n"
                                   "platinum_animation=off\n"
                                   "text_unlocked=Troféu conquistado!\n"
                                   "name_gold=Ouro\n"
                                   "unknown_key=1\n"
                                   "no equals sign here\n");
    assert(o.theme == Theme::CardCinematic);
    assert(o.volume == 100 && o.muted && Near(o.hold_seconds, 20.0f) && o.cinematic_intensity == 0);
    assert(o.grade_label && !o.platinum_animation && Options{}.platinum_animation);
    assert(o.text_unlocked == "Troféu conquistado!");
    assert(o.grade_names[3] == "Ouro" && o.grade_names[1].empty());

    // A bad value keeps the default; an unknown theme too.
    const Options bad = ParseOptions("theme=nonexistent\nvolume=loud\nmuted=maybe\nhold_seconds=1.5s\n"
                                     "cinematic_intensity=50x\nvolume=7%\nplatinum_animation=2\n");
    assert(bad == Options{});

    // What the menu writes reads back the same.
    Options all;
    all.theme = Theme::Card;
    all.volume = 35;
    all.muted = true;
    all.hold_seconds = 7.5f;
    all.cinematic_intensity = 42;
    all.grade_label = true;
    all.platinum_animation = false;
    all.text_unlocked = "Unlocked";
    all.text_banner = "Done";
    all.grade_names = {"", "B", "S", "G", "P"};
    assert(ParseOptions(FormatOptions(all)) == all);
    assert(ParseOptions(FormatOptions(Options{})) == Options{});

    // Texts: the English defaults, or the player's.
    assert(UnlockedText(Options{}) == "Trophy unlocked!");
    assert(GradeName(Options{}, 4) == "Platinum" && GradeName(all, 1) == "B");
    assert(GradeName(Options{}, 0).empty() && GradeName(Options{}, 9).empty());
    // A text never spans lines and never runs on, and is never cut inside a character: 199 bytes
    // of 'x', then a two-byte 'é' that would straddle the 200-byte limit.
    const Options long_text = ParseOptions("text_unlocked=" + std::string(500, 'x') + "\n");
    assert(long_text.text_unlocked.size() == 200);
    const Options accented = ParseOptions("text_unlocked=" + std::string(199, 'x') + "\xC3\xA9 more\n");
    assert(accented.text_unlocked == std::string(199, 'x'));
    assert(CutUtf8("Troféu", 5) == "Trof" && CutUtf8("Troféu", 6) == "Trof\xC3\xA9" && CutUtf8("ab", 9) == "ab");

    for (int t = 0; t < int(Theme::Count); ++t)
        assert(ParseOptions(std::string("theme=") + ThemeKey(Theme(t))).theme == Theme(t));
}

// The game's own code can switch the process to a locale with a decimal comma, in C (setlocale)
// and C++ (std::locale::global): "7.5" still reads as 7.5 and is written back the same.
static bool CommaLocale() {
    for (const char* name : {"Portuguese_Brazil.1252", "pt_BR.UTF-8", "de_DE.UTF-8", "German_Germany.1252"}) {
        if (!std::setlocale(LC_ALL, name)) continue;
        try {
            std::locale::global(std::locale(name));
        } catch (const std::runtime_error&) {
            // the C library knows it, the C++ library does not: the C locale alone is still a check
        }
        return true;
    }
    return false;
}

static void TestLocale() {
    if (!CommaLocale()) {
        std::printf("trophy toast: no locale with a decimal comma here, locale check skipped\n");
        return;
    }
    char decimal[8];
    std::snprintf(decimal, sizeof(decimal), "%.1f", 7.5);
    assert(std::string(decimal) == "7,5"); // the locale is really in effect
    const Options o = ParseOptions("hold_seconds=7.5\n");
    assert(Near(o.hold_seconds, 7.5f));
    assert(FormatOptions(o).find("hold_seconds=7.5\n") != std::string::npos);
    assert(Near(ParseAnimationSettings("fps=12.5\n").fps, 12.5f));
    std::locale::global(std::locale::classic());
    std::setlocale(LC_ALL, "C");
}

static void TestAnimationSettings() {
    const AnimationSettings none = ParseAnimationSettings("");
    assert(none.frames == 0 && none.columns == 1 && Near(none.fps, 30.0f) && !none.loop && none.delay_ms < 0.0f);
    const AnimationSettings a = ParseAnimationSettings("frames=93\ncolumns=10\nfps=30\nloop=1\ndelay_ms=250\n");
    assert(a.frames == 93 && a.columns == 10 && Near(a.fps, 30.0f) && a.loop && Near(a.delay_ms, 250.0f));
    const AnimationSettings clamped = ParseAnimationSettings("frames=999999\ncolumns=0\nfps=10000\n");
    assert(clamped.frames == 4096 && clamped.columns == 1 && Near(clamped.fps, 240.0f));

    const auto ends = UniformFrameEnds(4, 20.0f); // 50 ms each
    assert(ends.size() == 4 && Near(ends[0], 50.0f) && Near(ends[3], 200.0f));
    assert(UniformFrameEnds(0, 30.0f).empty() && UniformFrameEnds(3, 0.0f).empty());

    assert(AnimationFrame(ends, -1.0f, false) == -1); // not started
    assert(AnimationFrame({}, 10.0f, false) == -1);  // no frames
    assert(AnimationFrame(ends, 0.0f, false) == 0);
    assert(AnimationFrame(ends, 49.9f, false) == 0);
    assert(AnimationFrame(ends, 50.0f, false) == 1);
    assert(AnimationFrame(ends, 199.0f, false) == 3);
    assert(AnimationFrame(ends, 5000.0f, false) == 3); // once: the last frame stays
    assert(AnimationFrame(ends, 210.0f, true) == 0);   // looping: from the start again
    assert(AnimationFrame(ends, 360.0f, true) == 3);
}

static void TestTimeline() {
    const Timeline tl{.enter = 100.0f, .hold = 1000.0f, .exit = 200.0f, .gap = 50.0f, .badge_delay = 50.0f,
                      .badge_fade = 100.0f};
    assert(Near(tl.Total(), 1300.0f));
    const Frame start = Evaluate(tl, 0.0f, 0.0f);
    assert(start.phase == Phase::Enter && Near(start.alpha, 0.0f) && Near(start.offset, 1.0f));
    const Frame mid_in = Evaluate(tl, 0.0f, 50.0f);
    assert(mid_in.phase == Phase::Enter && mid_in.alpha > 0.0f && mid_in.alpha < 1.0f);
    const Frame hold = Evaluate(tl, 0.0f, 600.0f);
    assert(hold.phase == Phase::Hold && Near(hold.alpha, 1.0f) && Near(hold.offset, 0.0f) && Near(hold.badge, 1.0f));
    const Frame out = Evaluate(tl, 0.0f, 1200.0f);
    assert(out.phase == Phase::Exit && out.alpha < 1.0f && out.offset > 0.0f);
    assert(Evaluate(tl, 0.0f, 1300.0f).phase == Phase::Done);
    assert(Evaluate(tl, 0.0f, -50.0f).phase == Phase::Enter); // a clock behind the start: as at 0
    // The entrance only brightens and the card only settles in place.
    float last_alpha = -1.0f, last_offset = 2.0f;
    for (float t = 0.0f; t <= 100.0f; t += 5.0f) {
        const Frame f = Evaluate(tl, 0.0f, t);
        assert(f.alpha >= last_alpha - 1e-6f && f.offset <= last_offset + 1e-6f);
        last_alpha = f.alpha;
        last_offset = f.offset;
    }
    // The opacity never rises during the exit.
    float last = 2.0f;
    for (float t = 1100.0f; t < 1300.0f; t += 5.0f) {
        const float a = Evaluate(tl, 0.0f, t).alpha;
        assert(a <= last + 1e-6f);
        last = a;
    }
    // The cinematic entrance comes down from above, and is in place once entered.
    assert(Evaluate(tl, 0.2f, 10.0f).drop > 0.0f && Near(Evaluate(tl, 0.2f, 100.0f).drop, 0.0f));
    // A timeline without an entrance or exit is not divided by zero.
    const Timeline instant{.hold = 100.0f};
    assert(Evaluate(instant, 0.0f, 0.0f).phase == Phase::Hold && Near(Evaluate(instant, 0.0f, 0.0f).badge, 1.0f));
}

// A trophy_texts.txt as the launcher writes it: labels, names and descriptions, the runtime's limits.
static void TestTrophyTexts() {
    const std::string text = "\xEF\xBB\xBF# comment\r\n@heading=Trof\xC3\xA9us\r\n@rarity={}% dos jogadores\n"
                             "0=Rei da colina | Desbloqueie todos os trof\xC3\xA9us\n"
                             "3 = Sr. M\xC3\xA3o |  \n12=Despeda\xC3\xA7\x61\x64o\n200=Too far | x\nx=bad\nno equals\n"
                             "7= | empty name\n34=" + std::string(80, 'n') + " | " + std::string(158, 'd') + "\xC3\xA9\n";
    const auto t = SpartanTrophyTexts::Parse(text);
    assert(t.labels.size() == 2 && t.labels[0].first == "heading" && t.labels[0].second == "Trof\xC3\xA9us");
    assert(t.labels[1].second == "{}% dos jogadores");
    assert(t.count == 4); // 0, 3, 12 and 34: not 200 (beyond the runtime's 128), not 7 (no name)
    assert(t.trophies[0].first == "Rei da colina" && t.trophies[0].second == "Desbloqueie todos os trof\xC3\xA9us");
    assert(t.trophies[3].first == "Sr. M\xC3\xA3o" && t.trophies[3].second.empty());
    assert(t.trophies[12].first == "Despeda\xC3\xA7\x61\x64o" && t.trophies[7].first.empty());
    // Cut as the runtime keeps them, never inside a character: 158 bytes of 'd', then an 'é' that
    // would end at byte 160.
    assert(t.trophies[34].first.size() == SpartanTrophyTexts::kMaxName && t.trophies[34].second == std::string(158, 'd'));
    assert(SpartanTrophyTexts::Parse("").count == 0);
}

// The trophy window's orders over the game's real numbers, and the tiers.
static void TestRarity() {
    using namespace SpartanTrophyRarity;
    assert(Earned(1, 35) > 96.0f && Earned(0, 35) > 17.0f && Earned(35, 35) < 0.0f && Earned(1, 36) < 0.0f);
    std::vector<int> ids;
    for (int id = 0; id < 35; ++id) ids.push_back(id);
    auto sorted = [&](int order, int count) {
        std::vector<int> list = ids;
        std::stable_sort(list.begin(), list.end(),
                         [&](int a, int b) { return Before(Earned(a, count), Earned(b, count), order); });
        return list;
    };
    const auto most = sorted(MostEarned, 35), rarest = sorted(Rarest, 35);
    assert(most.front() == 1 && rarest.front() == 0); // Kill Poseidon 96.17%; the platinum 17.58%
    for (size_t i = 1; i < most.size(); ++i) {
        assert(Earned(most[i - 1], 35) >= Earned(most[i], 35));
        assert(Earned(rarest[i - 1], 35) <= Earned(rarest[i], 35));
    }
    assert(sorted(ListOrder, 35) == ids);
    assert(sorted(MostEarned, 36) == ids); // another package: no numbers, the list's order
    // Unknown ones come last in both orders.
    assert(Before(10.0f, -1.0f, MostEarned) && Before(10.0f, -1.0f, Rarest) && !Before(-1.0f, 10.0f, Rarest));
    assert(Tier(96.0f) == 0 && Tier(50.0f) == 1 && Tier(15.1f) == 1 && Tier(15.0f) == 2 && Tier(5.0f) == 3);
}

// An icon of `width` x `height`: the left `lit` columns in one colour, the rest black.
static std::vector<unsigned char> Icon(int width, int height, int lit, unsigned char r, unsigned char g,
                                       unsigned char b, unsigned char a = 255) {
    std::vector<unsigned char> rgba(size_t(width) * height * 4);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            unsigned char* p = rgba.data() + (size_t(y) * width + x) * 4;
            const bool on = x < lit;
            p[0] = on ? r : 0;
            p[1] = on ? g : 0;
            p[2] = on ? b : 0;
            p[3] = a;
        }
    return rgba;
}

static void TestGrades() {
    auto grade = [](const std::vector<unsigned char>& rgba, int w, int h) { return GradeFromIcon(rgba.data(), w, h); };
    assert(GradeFromIcon(nullptr, 4, 4) == Grade::Unknown);
    assert(grade(Icon(4, 4, 4, 200, 200, 200), 0, 4) == Grade::Unknown);
    assert(grade(Icon(48, 48, 48, 40, 50, 60), 48, 48) == Grade::Unknown); // all dark
    // Bluish is platinum; grey silver; a muted orange bronze; a saturated yellow gold.
    assert(grade(Icon(48, 48, 48, 120, 150, 210), 48, 48) == Grade::Platinum);
    assert(grade(Icon(48, 48, 48, 185, 185, 178), 48, 48) == Grade::Silver);
    assert(grade(Icon(48, 48, 48, 180, 110, 80), 48, 48) == Grade::Bronze);
    assert(grade(Icon(48, 48, 48, 230, 170, 30), 48, 48) == Grade::Gold);
    // Dark cells do not count, alpha is ignored, and an icon smaller than the 24 x 24 grid works.
    assert(grade(Icon(48, 48, 12, 230, 170, 30), 48, 48) == Grade::Gold);
    assert(grade(Icon(48, 48, 48, 185, 185, 178, 0), 48, 48) == Grade::Silver);
    assert(grade(Icon(7, 5, 7, 180, 110, 80), 7, 5) == Grade::Bronze);
}

// The game's own icons, when the test is given its trophy00.trp: the grades the launcher shows.
static void TestGameGrades(const fs::path& trp) {
    std::ifstream in(trp, std::ios::binary);
    const std::string data((std::istreambuf_iterator<char>(in)), {});
    auto be32 = [&](size_t at) {
        return unsigned(static_cast<unsigned char>(data[at])) << 24 | unsigned(static_cast<unsigned char>(data[at + 1])) << 16 |
               unsigned(static_cast<unsigned char>(data[at + 2])) << 8 | unsigned(static_cast<unsigned char>(data[at + 3]));
    };
    assert(data.size() > 0x60 && be32(0) == 0xDCA24D00u);
    const unsigned files = be32(0x10), elem = be32(0x14);
    int checked = 0;
    for (unsigned i = 0; i < files; ++i) {
        const size_t entry = 0x60 + size_t(i) * elem;
        const std::string name = data.substr(entry, 32).c_str();
        int id = -1;
        if (std::sscanf(name.c_str(), "TROP%3d.PNG", &id) != 1 || name.size() != 11) continue;
        const size_t offset = size_t(be32(entry + 0x24)), size = size_t(be32(entry + 0x2C));
        SpartanPng::Image image;
        assert(SpartanPng::DecodeRgba(reinterpret_cast<const unsigned char*>(data.data()) + offset, size, image));
        const Grade want = id == 0 ? Grade::Platinum : id <= 18 ? Grade::Bronze : id <= 29 ? Grade::Silver : Grade::Gold;
        const Grade got = GradeFromIcon(image.rgba.data(), image.width, image.height);
        if (got != want) std::printf("trophy %d: grade %d, expected %d\n", id, int(got), int(want));
        assert(got == want);
        ++checked;
    }
    assert(checked == 35);
    std::printf("trophy toast: the grades of the game's 35 icons passed\n");
}

static void TestMedia(const fs::path& dir) {
    // A GIF of three frames: red 100 ms, green 200 ms, blue 300 ms.
    const fs::path gif = dir / "gold_anim.gif";
    Write(gif, Gif(4, 2, {0, 1, 2}, {10, 20, 30}));
    const auto a = SpartanToastMedia::DecodeAnimation(gif);
    assert(a.Frames() == 3 && a.width == 4 && a.height == 2);
    assert(a.pixels.size() == size_t(3) * 4 * 2 * 4);
    assert(Near(a.frame_ends[0], 100.0f, 1.0f) && Near(a.frame_ends[1], 300.0f, 1.0f) &&
           Near(a.frame_ends[2], 600.0f, 1.0f));
    auto pixel = [&](int frame, int i) { return a.pixels.data() + (size_t(frame) * 8 + size_t(i)) * 4; };
    assert(pixel(0, 0)[0] == 255 && pixel(0, 0)[1] == 0 && pixel(0, 7)[3] == 255); // red, opaque
    assert(pixel(1, 3)[1] == 255 && pixel(1, 3)[0] == 0);                           // green
    assert(pixel(2, 5)[2] == 255 && pixel(2, 5)[1] == 0);                           // blue
    assert(AnimationFrame(a.frame_ends, 350.0f, false) == 2);

    // The frames as one texture: two per row.
    const auto sheet = SpartanToastMedia::PackFrames(a);
    assert(sheet.columns == 2 && sheet.width == 8 && sheet.height == 4);
    auto at = [&](int x, int y) { return sheet.pixels.data() + (size_t(y) * sheet.width + size_t(x)) * 4; };
    assert(at(0, 0)[0] == 255);                 // frame 0 at the top left
    assert(at(4, 1)[1] == 255);                 // frame 1 beside it
    assert(at(0, 2)[2] == 255);                 // frame 2 below
    assert(at(4, 2)[3] == 0);                   // the empty cell stays transparent
    assert(SpartanToastMedia::PackFrames({}).pixels.empty());

    // Limits: large frames are scaled down, frames beyond the limits are dropped.
    const fs::path wide = dir / "wide.gif";
    Write(wide, Gif(300, 20, {3, 3}, {10, 10}));
    SpartanToastMedia::AnimationLimits small;
    small.max_side = 64;
    small.max_frames = 1;
    const auto scaled = SpartanToastMedia::DecodeAnimation(wide, small);
    assert(scaled.Frames() == 1 && scaled.width == 64 && scaled.height == 4);
    SpartanToastMedia::AnimationLimits few_pixels;
    few_pixels.max_pixels = size_t(4) * 2; // one 4x2 frame
    assert(SpartanToastMedia::DecodeAnimation(gif, few_pixels).Frames() == 1);
    SpartanToastMedia::AnimationLimits short_time;
    short_time.max_ms = 250.0f; // frames starting at 0 and 100 ms; the one at 300 ms is dropped
    assert(SpartanToastMedia::DecodeAnimation(gif, short_time).Frames() == 2);

    // A sound: 48 kHz stereo, about as long as the file.
    const fs::path wav = dir / "bronze.wav";
    Write(wav, Wav());
    const auto pcm = SpartanToastMedia::DecodeSound(wav);
    const float seconds = float(pcm.size()) / 2.0f / float(SpartanToastMedia::kAudioRate);
    assert(pcm.size() % 2 == 0 && Near(seconds, 0.25f, 0.02f));
    float peak = 0.0f;
    for (float v : pcm) peak = std::max(peak, std::fabs(v));
    assert(peak > 0.15f && peak < 0.35f); // 8000 / 32768, resampled

    // Broken and missing files decode to nothing (and never stop the game).
    const fs::path broken = dir / "silver_anim.gif";
    Write(broken, "GIF89a not really");
    assert(SpartanToastMedia::DecodeAnimation(broken).Frames() == 0);
    assert(SpartanToastMedia::DecodeAnimation(dir / "missing.gif").Frames() == 0);
    assert(SpartanToastMedia::DecodeSound(broken).empty());
    assert(SpartanToastMedia::DecodeSound(dir / "missing.mp3").empty());
}

int main(int argc, char** argv) {
    assert(argc == 2 || argc == 3);
    av_log_set_level(AV_LOG_QUIET); // the broken file below is meant to fail
    const fs::path dir = fs::path(argv[1]) / "trophy-toast-fixture";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    TestOptions();
    TestLocale();
    TestAnimationSettings();
    TestTimeline();
    TestGrades();
    TestTrophyTexts();
    TestRarity();
    if (argc == 3) TestGameGrades(argv[2]);
    TestMedia(dir);
    fs::remove_all(dir, ec);
    std::printf("trophy toast: options, animation settings and frames, timeline, grades, trophy texts, rarity orders, GIF/WAV decoding and limits passed\n");
    return 0;
}
