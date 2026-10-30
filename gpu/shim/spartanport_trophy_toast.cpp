// SPDX-License-Identifier: GPL-2.0-or-later
#include "spartanport_trophy_toast.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <SDL3/SDL.h>
#include "imgui_internal.h"
#include "spartanport_trophy_media.h"
#include "spartanport_ui_language.h"

// DejaVu Sans, embedded by the overlay (spartanport_overlay.cpp): fills in what the toast's fonts lack.
extern "C" const unsigned char spartanport_font_ttf[];
extern "C" const unsigned char spartanport_font_ttf_end[];

namespace SpartanToast {

namespace {

namespace fs = std::filesystem;

// Themes. Lengths are fractions of the card's height, times are in ms.

enum class Renderer { Card, Corner };

struct Style {
    float height;             // px at 1080p
    float aspect;             // width / height
    float margin_x, margin_y; // px at 1080p
    // In card heights:
    float rounding;
    float art, art_x, art_rounding; // the trophy art: size, centre from the left edge
    float text_x, name_y, sub_y;    // where the text starts; the two lines' centres
    float name_size, sub_size;      // the two lines' font sizes
    float cup, cup_x, name_after_cup; // the grade's cup: height, centre, the name after it
    float trophy;                   // the drawn platinum trophy's height
    float sprite;                   // the platinum's animation, its height
    float badge_r, badge_x;         // the badge: radius, centre from the right edge
    float slide_in, slide_out;
    ImU32 bg_left, bg_right, name, sub;
};

// Cinematic platinum: the platinum's own animation, then light once the trophy has settled.
// Scaled by the intensity option.
struct Cinematic {
    bool enabled;
    float settle;     // card heights: the card also comes down this much as it enters
    float glow_at, glow_len, steady_glow;
    float sweep_at, sweep_len;
    float glint_at, glint_len;
    ImU32 light;
};

struct ThemeDef {
    Renderer renderer;
    const Style* style; // the card's layout (Card themes only)
    Timeline normal, platinum;
    Cinematic cinematic;
};

constexpr Style kCardStyle{
    /*height*/ 80.0f, /*aspect*/ 5.9f, /*margin_x*/ 44.0f, /*margin_y*/ 40.0f,
    /*rounding*/ 0.11f,
    /*art*/ 0.58f, /*art_x*/ 0.50f, /*art_rounding*/ 0.06f,
    /*text_x*/ 0.99f, /*name_y*/ 0.36f, /*sub_y*/ 0.715f,
    /*name_size*/ 0.37f, /*sub_size*/ 0.30f,
    /*cup*/ 0.20f, /*cup_x*/ 1.12f, /*name_after_cup*/ 1.35f,
    /*trophy*/ 0.56f, /*sprite*/ 0.62f,
    /*badge_r*/ 0.17f, /*badge_x*/ 0.42f,
    /*slide_in*/ 0.06f, /*slide_out*/ 0.09f,
    /*bg_left*/ IM_COL32(31, 34, 40, 242), /*bg_right*/ IM_COL32(18, 21, 27, 242),
    /*name*/ IM_COL32(244, 244, 244, 255), /*sub*/ IM_COL32(200, 203, 208, 255)};
constexpr Timeline kCardTimeline{.enter = 60.0f, .hold = 5150.0f, .exit = 200.0f, .gap = 570.0f,
                                 .badge_delay = 60.0f, .badge_fade = 150.0f,
                                 .ring_first = 350.0f, .ring_period = 2030.0f, .ring_len = 1000.0f,
                                 .trophy_at = 370.0f};
constexpr Timeline kCinematicTimeline{.enter = 260.0f, .hold = 7000.0f, .exit = 260.0f, .gap = 570.0f,
                                      .badge_delay = 120.0f, .badge_fade = 180.0f,
                                      .ring_first = 350.0f, .ring_period = 2030.0f, .ring_len = 1000.0f,
                                      .trophy_at = 370.0f};
constexpr Cinematic kNoCinematic{};
constexpr Cinematic kPlatinumCinematic{
    /*enabled*/ true, /*settle*/ 0.16f,
    /*glow_at*/ 3300.0f, /*glow_len*/ 1500.0f, /*steady_glow*/ 0.22f,
    /*sweep_at*/ 3400.0f, /*sweep_len*/ 1100.0f,
    /*glint_at*/ 3800.0f, /*glint_len*/ 700.0f,
    /*light*/ IM_COL32(196, 222, 255, 255)};
// The corner popup rises in 250 ms, stays 5 s and sinks in 300 ms.
constexpr Timeline kCornerTimeline{.enter = 250.0f, .hold = 5000.0f, .exit = 300.0f, .gap = 250.0f};

// The corner popup, in px at 1080p: a plain rectangle against the bottom right corner, the
// artwork on the left in a frame of the grade's colour, the trophy's name and its detail (up to
// two lines) beside it.
struct CornerStyle {
    float width, height, art, frame, pad, name_px, sub_px, line_gap;
    ImU32 bg_top, bg_bottom, edge, name, sub;
};
constexpr CornerStyle kCorner{
    /*width*/ 340.0f, /*height*/ 92.0f, /*art*/ 62.0f, /*frame*/ 2.0f, /*pad*/ 14.0f,
    /*name_px*/ 16.5f, /*sub_px*/ 14.0f, /*line_gap*/ 3.0f,
    /*bg_top*/ IM_COL32(42, 46, 55, 248), /*bg_bottom*/ IM_COL32(25, 28, 34, 248),
    /*edge*/ IM_COL32(255, 255, 255, 22), /*name*/ IM_COL32(240, 242, 245, 255), /*sub*/ IM_COL32(150, 158, 170, 255)};

const ThemeDef kThemes[int(Theme::Count)] = {
    {Renderer::Card, &kCardStyle, kCardTimeline, kCardTimeline, kNoCinematic}, // Original: drawn by the overlay
    {Renderer::Card, &kCardStyle, kCardTimeline, kCardTimeline, kNoCinematic},
    {Renderer::Card, &kCardStyle, kCardTimeline, kCinematicTimeline, kPlatinumCinematic},
    {Renderer::Corner, nullptr, kCornerTimeline, kCornerTimeline, kNoCinematic},
};

const ThemeDef& Def(Theme theme) {
    const int i = int(theme);
    return kThemes[i >= 0 && i < int(Theme::Count) ? i : 0];
}

ImU32 GradeInk(Grade grade, float alpha) {
    const int a = int(255.0f * std::clamp(alpha, 0.0f, 1.0f));
    switch (grade) {
    case Grade::Bronze: return IM_COL32(176, 92, 86, a);
    case Grade::Silver: return IM_COL32(184, 192, 204, a);
    case Grade::Gold: return IM_COL32(228, 178, 72, a);
    case Grade::Platinum: return IM_COL32(170, 202, 238, a);
    default: return IM_COL32(200, 200, 200, a);
    }
}

ImU32 WithAlpha(ImU32 color, float alpha) {
    const float base = float((color >> IM_COL32_A_SHIFT) & 0xFF);
    const ImU32 a = ImU32(std::clamp(base * alpha, 0.0f, 255.0f));
    return (color & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
}

ImU32 Mix(ImU32 a, ImU32 b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto ch = [&](int shift) {
        const float x = float((a >> shift) & 0xFF), y = float((b >> shift) & 0xFF);
        return ImU32(x + (y - x) * t + 0.5f) << shift;
    };
    return ch(IM_COL32_R_SHIFT) | ch(IM_COL32_G_SHIFT) | ch(IM_COL32_B_SHIFT) | ch(IM_COL32_A_SHIFT);
}

// 0 -> 1 -> 0 over [0, 1], smooth at both ends.
float Bump(float t) { return t <= 0.0f || t >= 1.0f ? 0.0f : std::sin(t * 3.14159265f); }
// Quickly in, slowly out: lines and flashes of the platinum animation.
float Flash(float t) {
    if (t <= 0.0f || t >= 1.0f) return 0.0f;
    return std::min(t / 0.2f, 1.0f) * (1.0f - EaseInQuad((t - 0.2f) / 0.8f));
}

// Options (notifications.ini) and paths.

fs::path TrophyDir() {
    const char* user = std::getenv("SPARTAN_USER_DIR");
    return fs::path(user && *user ? user : "user") / "custom_trophy";
}

std::string ReadText(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), {});
}

// Never destroyed: the worker thread may still save them while the process exits.
std::mutex& options_mutex = *new std::mutex;
Options& options = *new Options;
std::atomic<int> theme_override{int(Theme::Count)};

void LoadOptions() {
    const Options o = ParseOptions(ReadText(TrophyDir() / "notifications.ini"));
    std::scoped_lock lock{options_mutex};
    options = o;
}

// Written beside, then renamed: a crash while writing never leaves half a file.
void WriteOptions(const Options& o) {
    std::error_code ec;
    fs::create_directories(TrophyDir(), ec);
    const fs::path file = TrophyDir() / "notifications.ini", part = TrophyDir() / "notifications.ini.part";
    {
        std::ofstream out(part, std::ios::binary | std::ios::trunc);
        out << FormatOptions(o);
        if (!out) return;
    }
    fs::rename(part, file, ec);
}

// Pictures, loaded once at start on the ImGui thread (never during a toast):
//   badge.png                  the disc on the right of the card
//   <grade>_anim.<ext>         the art of that grade's toasts: a GIF or animated PNG (its own
//                              timing), a still picture, or with <grade>_anim.ini a sheet of
//                              frames; trophy_anim.* for every grade without its own
// Without them the badge, the platinum trophy and the trophies' own artwork are drawn.

struct Picture {
    std::unique_ptr<ImTextureData> texture;
    int width = 0, height = 0;
    bool Ready() const { return texture != nullptr; }
    ImTextureRef Ref() const { return texture->GetTexRef(); }
};

Picture MakePicture(const unsigned char* rgba, int width, int height) {
    Picture picture;
    if (!rgba || width <= 0 || height <= 0) return picture;
    picture.texture = std::make_unique<ImTextureData>();
    picture.texture->Create(ImTextureFormat_RGBA32, width, height);
    std::memcpy(picture.texture->Pixels, rgba, size_t(width) * height * 4);
    picture.texture->UseColors = true;
    ImGui::RegisterUserTexture(picture.texture.get());
    picture.width = width;
    picture.height = height;
    return picture;
}

struct Animation {
    Picture sheet;
    int columns = 1, frames = 0;
    std::vector<float> frame_ends; // ms
    bool loop = false;
    float delay_ms = -1.0f;
    std::string source; // the file name, for the menu
    bool Ready() const { return sheet.Ready() && frames > 0; }
};

Picture badge_picture;
std::string badge_source; // the file name, for the menu
std::array<Animation, 5> animations; // by Grade; [0] = trophy_anim, for any grade

// Finds <stem>.<ext> in the folder: the first of the extensions that exists.
fs::path FindFile(const fs::path& dir, const std::string& stem, std::initializer_list<const char*> extensions) {
    for (const char* ext : extensions) {
        std::error_code ec;
        const fs::path path = dir / (stem + ext);
        if (fs::is_regular_file(path, ec)) return path;
    }
    return {};
}

Animation LoadAnimation(const fs::path& dir, const char* grade_name) {
    Animation anim;
    const std::string stem = std::string(grade_name) + "_anim";
    const fs::path path = FindFile(dir, stem, {".gif", ".apng", ".png", ".webp", ".jpg", ".jpeg", ".bmp"});
    if (path.empty()) return anim;
    const AnimationSettings settings = ParseAnimationSettings(ReadText(dir / (stem + ".ini")));
    anim.loop = settings.loop;
    anim.delay_ms = settings.delay_ms;
    anim.source = path.filename().string();
    if (settings.frames > 0) {
        // A sheet of frames at a fixed rate: kept at its own size (the player chose it).
        SpartanToastMedia::AnimationLimits one;
        one.max_side = 4096;
        one.max_frames = 1;
        one.max_pixels = size_t(4096) * 4096;
        const auto image = SpartanToastMedia::DecodeAnimation(path, one);
        if (image.Frames() == 1) {
            anim.sheet = MakePicture(image.pixels.data(), image.width, image.height);
            anim.columns = std::min(settings.columns, settings.frames);
            anim.frames = settings.frames;
            anim.frame_ends = UniformFrameEnds(settings.frames, settings.fps);
        }
    } else {
        const auto decoded = SpartanToastMedia::DecodeAnimation(path);
        const auto sheet = SpartanToastMedia::PackFrames(decoded);
        anim.sheet = MakePicture(sheet.pixels.data(), sheet.width, sheet.height);
        anim.columns = sheet.columns;
        anim.frames = decoded.Frames();
        anim.frame_ends = decoded.frame_ends;
    }
    if (!anim.Ready()) {
        std::printf("Trophy notifications: cannot read %s\n", path.string().c_str());
        anim = {};
    }
    return anim;
}

// The animation of a grade: its own, else trophy_anim; null when there is none.
const Animation* AnimationFor(Grade grade) {
    const int g = int(grade);
    if (g >= 1 && g <= 4 && animations[size_t(g)].Ready()) return &animations[size_t(g)];
    return animations[0].Ready() ? &animations[0] : nullptr;
}

// Audio: one worker thread decodes, keeps the samples and feeds a stream of its own (mixed by
// SDL beside the game's audio, which is never paused or cleared). The present thread only posts.
// No file, no sound.

struct Audio {
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::function<void()>> jobs;
    bool started = false;
    // Worker thread only:
    std::array<std::vector<float>, 5> sounds; // by Grade
    SDL_AudioStream* stream = nullptr;
    bool device_tried = false;
    // Guarded by mutex (read by the menu):
    std::array<std::string, 5> sources;
    bool loaded = false;
};

Audio& audio() {
    static Audio* instance = new Audio; // never destroyed: the worker may outlive static teardown
    return *instance;
}

void Post(std::function<void()> job) {
    Audio& a = audio();
    {
        std::scoped_lock lock{a.mutex};
        if (!a.started) {
            a.started = true;
            std::thread([&a] {
                for (;;) {
                    std::function<void()> next;
                    {
                        std::unique_lock lock{a.mutex};
                        a.wake.wait(lock, [&] { return !a.jobs.empty(); });
                        next = std::move(a.jobs.front());
                        a.jobs.pop_front();
                    }
                    next();
                }
            }).detach();
        }
        a.jobs.push_back(std::move(job));
    }
    a.wake.notify_one();
}

void ApplyGain() { // worker thread
    Audio& a = audio();
    if (!a.stream) return;
    const Options o = GetOptions();
    SDL_SetAudioStreamGain(a.stream, o.muted ? 0.0f : float(o.volume) / 100.0f);
}

void LoadSounds() { // worker thread
    Audio& a = audio();
    const fs::path dir = TrophyDir();
    std::array<std::vector<float>, 5> sounds;
    std::array<std::string, 5> sources;
    for (size_t g = 0; g < 5; ++g) {
        const fs::path path = FindFile(dir, kGradeFileNames[g], {".wav", ".mp3", ".ogg", ".flac", ".m4a", ".opus"});
        if (path.empty()) continue;
        sounds[g] = SpartanToastMedia::DecodeSound(path);
        if (sounds[g].empty()) std::printf("Trophy sound: cannot decode %s\n", path.string().c_str());
        else sources[g] = path.filename().string();
    }
    for (size_t g = 1; g < 5; ++g) // trophy.* for every grade without its own
        if (sounds[g].empty() && !sounds[0].empty()) {
            sounds[g] = sounds[0];
            sources[g] = sources[0];
        }
    const bool any = std::any_of(sounds.begin(), sounds.end(), [](const auto& s) { return !s.empty(); });
    a.sounds = std::move(sounds);
    {
        std::scoped_lock lock{a.mutex};
        a.sources = sources;
        a.loaded = true;
    }
    // The audio device only when there is something to play, and never with SPARTAN_AUDIO=none.
    const char* audio_mode = std::getenv("SPARTAN_AUDIO");
    if (any && !a.device_tried && !(audio_mode && !std::strcmp(audio_mode, "none"))) {
        a.device_tried = true;
        if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, SpartanToastMedia::kAudioRate};
            a.stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
            if (a.stream) SDL_ResumeAudioStreamDevice(a.stream);
        }
        if (!a.stream) std::printf("Trophy sound: no audio device (%s)\n", SDL_GetError());
    }
    ApplyGain();
}

double SteadyMs() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void PostSound(Grade grade) { // any thread: posts
    const double posted = SteadyMs();
    Post([grade, posted] {
        Audio& a = audio();
        const auto& pcm = a.sounds[size_t(grade)];
        if (!a.stream || pcm.empty() || GetOptions().muted) return;
        if (SteadyMs() - posted > 1000.0) return; // held up (sounds reloading): its toast is gone
        SDL_ClearAudioStream(a.stream); // only this stream: a new toast cuts the previous sound
        SDL_PutAudioStreamData(a.stream, pcm.data(), int(pcm.size() * sizeof(float)));
    });
}

std::atomic<bool> save_posted{false};
void PostSave() {
    if (save_posted.exchange(true)) return; // one write for a burst of changes (a slider drag)
    Post([] {
        save_posted = false;
        WriteOptions(GetOptions());
    });
}

// Queue and the toast on screen.

struct Request {
    int id;
    bool preview;
    Grade grade; // shown as this grade (a preview), else Unknown: the trophy's own
};
std::mutex queue_mutex;
std::deque<Request> queue;
std::atomic<int> pending{0};
std::atomic<bool> showing{false};
std::atomic<double> clock_override{-1.0};
// Set by Start on the present thread; Enqueue (any thread) and the worker read them.
std::atomic<IconSource> icon_source{nullptr};
std::atomic<TrophyLookup> trophy_lookup{nullptr};
// More than this many waiting are not worth showing one by one.
constexpr size_t kQueueLimit = 64;
ImFont* name_font = nullptr;
ImFont* sub_font = nullptr;
float top_inset = 0.0f; // px from the top the card keeps clear (the FPS counter), set by Draw
// SetDefaultTexts: the player's language for what notifications.ini leaves empty.
std::mutex default_mutex;
std::string default_unlocked;
std::array<std::string, 5> default_grades;

// Present thread only.
struct Active {
    int id = -1;
    Grade grade = Grade::Unknown;
    double start = 0.0;
    const ThemeDef* theme = nullptr;
    Timeline timeline{};
    bool cinematic = false;
    float intensity = 0.0f;
    bool grade_label = false;
    bool platinum_art = false; // the platinum's animation (or drawn trophy), not its artwork
    std::string title, detail, unlocked_text, grade_name;
    // The title (and the corner's detail) cut to the toast at the last scale (computed once per
    // toast and scale).
    std::string fitted;
    std::vector<std::string> detail_lines;
    float fitted_scale = -1.0f;
} active;
double next_start = 0.0;

std::atomic<double> stat_last{0}, stat_sum{0}, stat_max{0};
std::atomic<unsigned> stat_frames{0}, stat_vertices{0};

double NowMs() {
    const double fixed = clock_override.load();
    if (fixed >= 0.0) return fixed;
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::string Fit(ImFont* font, float size, const std::string& text, float width) {
    if (font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str()).x <= width) return text;
    std::string cut = text;
    while (!cut.empty() &&
           font->CalcTextSizeA(size, FLT_MAX, 0.0f, (cut + "…").c_str()).x > width) {
        // Drop one whole UTF-8 character: its continuation bytes, then its lead byte.
        while (!cut.empty() && (static_cast<unsigned char>(cut.back()) & 0xC0) == 0x80) cut.pop_back();
        if (!cut.empty()) cut.pop_back();
    }
    while (!cut.empty() && cut.back() == ' ') cut.pop_back();
    return cut + "…";
}

// text in lines of at most `width`, broken between words; past `max_lines` the last one is cut.
std::vector<std::string> Wrap(ImFont* font, float size, const std::string& text, float width, int max_lines) {
    std::vector<std::string> lines;
    const char* at = text.c_str();
    const char* end = at + text.size();
    while (at < end && int(lines.size()) < max_lines) {
        while (at < end && *at == ' ') ++at;
        if (at == end) break;
        const char* stop = font->CalcWordWrapPosition(size, at, end, width);
        if (stop == at) stop = at + 1; // a single character wider than the line
        while (stop < end && (static_cast<unsigned char>(*stop) & 0xC0) == 0x80) ++stop;
        if (int(lines.size()) + 1 == max_lines && stop < end) stop = end; // the last line: cut by Fit
        lines.push_back(Fit(font, size, std::string(at, stop), width));
        at = stop;
    }
    return lines;
}

// The grade's small cup (bowl, handles, stem, base); s is about 1.4x its height.
void Cup(ImDrawList* d, ImVec2 c, float s, ImU32 ink) {
    const float x = c.x, y = c.y;
    const ImVec2 bowl[] = {{x - .30f * s, y - .24f * s}, {x + .30f * s, y - .24f * s},
                           {x + .22f * s, y + .13f * s}, {x, y + .25f * s}, {x - .22f * s, y + .13f * s}};
    d->AddConvexPolyFilled(bowl, 5, ink);
    const float w = .07f * s;
    d->AddLine({x - .29f * s, y - .15f * s}, {x - .43f * s, y - .15f * s}, ink, w);
    d->AddLine({x - .43f * s, y - .15f * s}, {x - .33f * s, y + .07f * s}, ink, w);
    d->AddLine({x + .29f * s, y - .15f * s}, {x + .43f * s, y - .15f * s}, ink, w);
    d->AddLine({x + .43f * s, y - .15f * s}, {x + .33f * s, y + .07f * s}, ink, w);
    d->AddRectFilled({x - .06f * s, y + .22f * s}, {x + .06f * s, y + .40f * s}, ink);
    d->AddRectFilled({x - .25f * s, y + .38f * s}, {x + .25f * s, y + .47f * s}, ink, .04f * s);
}

// A four-pointed star (glints, and the badge without a picture).
void Star(ImDrawList* d, ImVec2 c, float r, ImU32 ink) {
    const float k = r * 0.22f;
    const ImVec2 v[] = {{c.x, c.y - r}, {c.x + k, c.y - k}, {c.x + r, c.y}, {c.x + k, c.y + k},
                        {c.x, c.y + r}, {c.x - k, c.y + k}, {c.x - r, c.y}, {c.x - k, c.y - k}};
    d->AddConcavePolyFilled(v, 8, ink);
}

// The badge on the right: a disc (badge.png, or a light disc with a star), and a ring that leaves
// it every ~2 s: a soft glow at the edge becomes a thin ring that moves out quickly, then slowly,
// to ~1.55x the radius, fading.
void Badge(ImDrawList* d, ImVec2 c, float r, float alpha, float ms, const Timeline& tl) {
    if (alpha <= 0.0f) return;
    if (ms >= tl.ring_first && tl.ring_period > 0.0f && tl.ring_len > 0.0f) {
        const float k = std::fmod(ms - tl.ring_first, tl.ring_period);
        if (k < tl.ring_len) {
            const float p = k / tl.ring_len;
            const float rise = Clamp01(k / 120.0f);
            const float fade = std::pow(1.0f - Clamp01((p - 0.14f) / 0.86f), 1.3f);
            const float radius = r * (1.08f + 0.50f * EaseOutCubic(p));
            const float glow = 1.0f - Clamp01(k / 160.0f); // the soft start
            d->AddCircleFilled(c, radius, IM_COL32(4, 5, 8, int(150 * rise * fade * alpha)), 48);
            for (int i = 0; i < 3 && glow > 0.0f; ++i)
                d->AddCircle(c, r * (1.0f + 0.07f * float(i + 1)), IM_COL32(225, 228, 234, int(80 * glow * rise * alpha)),
                             48, r * 0.12f);
            d->AddCircle(c, radius, IM_COL32(214, 218, 224, int(245 * rise * fade * alpha)), 48, std::max(r * 0.12f, 1.0f));
        }
    }
    if (badge_picture.Ready()) {
        d->AddImage(badge_picture.Ref(), ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), ImVec2(0, 0), ImVec2(1, 1),
                    IM_COL32(255, 255, 255, int(255 * alpha)));
        return;
    }
    d->AddCircleFilled(c, r, IM_COL32(246, 246, 246, int(255 * alpha)), 40);
    Star(d, c, r * 0.62f, IM_COL32(24, 27, 33, int(255 * alpha)));
}

// The platinum trophy, drawn: a globe held by silver hands on a slim stem and a flared foot.
// size is its height (-0.5 .. 0.5 around c). whiteness turns it into a white silhouette (the
// animation's flashes), reveal shows the holder from the bottom up, globe_scale grows the globe.
// Convex pieces and strokes only.
constexpr float kGlobeY = -0.19f, kGlobeR = 0.245f;
void PlatinumTrophy(ImDrawList* d, ImVec2 c, float size, float alpha, float whiteness, float reveal,
                    float globe_scale, float globe_whiteness) {
    if (alpha <= 0.0f) return;
    auto P = [&](float x, float y) { return ImVec2(c.x + x * size, c.y + y * size); };
    const ImU32 white = IM_COL32(255, 255, 255, 255);
    auto ink = [&](ImU32 color) { return WithAlpha(Mix(color, white, whiteness), alpha); };
    if (globe_scale > 0.0f) { // behind the hands
        auto globe_ink = [&](ImU32 color) { return WithAlpha(Mix(color, white, globe_whiteness), alpha); };
        const ImVec2 g = P(0.0f, kGlobeY);
        const float R = kGlobeR * size * globe_scale;
        auto at = [&](float dx, float dy) { return ImVec2(g.x + dx * R, g.y + dy * R); };
        d->AddCircleFilled(g, R, globe_ink(IM_COL32(20, 62, 168, 255)), 32);
        // Lit from below: lighter blues, then cyan, towards the bottom.
        d->AddEllipseFilled(at(0.0f, 0.16f), ImVec2(R * 0.84f, R * 0.78f), globe_ink(IM_COL32(40, 112, 218, 255)), 0.0f, 32);
        d->AddEllipseFilled(at(0.0f, 0.36f), ImVec2(R * 0.66f, R * 0.52f), globe_ink(IM_COL32(92, 176, 242, 255)), 0.0f, 28);
        d->AddEllipseFilled(at(0.0f, 0.55f), ImVec2(R * 0.44f, R * 0.30f), globe_ink(IM_COL32(176, 230, 255, 255)), 0.0f, 24);
        d->AddEllipseFilled(at(-0.30f, -0.50f), ImVec2(R * 0.26f, R * 0.15f), globe_ink(IM_COL32(236, 246, 255, 215)), -0.6f);
        d->AddCircle(g, R, globe_ink(IM_COL32(120, 190, 255, 160)), 32, 0.016f * size);
    }
    if (reveal > 0.0f) {
        const ImU32 silver = ink(IM_COL32(206, 212, 222, 255)), shade = ink(IM_COL32(136, 144, 158, 255)),
                    light = ink(IM_COL32(248, 250, 253, 255));
        const float bottom = c.y + 0.48f * size;
        d->PushClipRect(ImVec2(c.x - size, bottom - reveal * 0.85f * size), ImVec2(c.x + size, bottom + 1.0f), true);
        // Foot and stem.
        d->AddEllipseFilled(P(0.0f, 0.435f), ImVec2(0.19f * size, 0.045f * size), shade);
        d->AddQuadFilled(P(-0.05f, 0.29f), P(0.05f, 0.29f), P(0.17f, 0.42f), P(-0.17f, 0.42f), silver);
        d->AddEllipseFilled(P(0.0f, 0.415f), ImVec2(0.17f * size, 0.03f * size), light);
        d->AddQuadFilled(P(-0.055f, 0.10f), P(0.055f, 0.10f), P(0.04f, 0.31f), P(-0.04f, 0.31f), silver);
        d->AddLine(P(-0.012f, 0.12f), P(-0.010f, 0.30f), light, 0.014f * size);
        // Petals cupping the globe's lower half: outer ones up its sides, inner ones in front.
        d->AddBezierCubic(P(-0.03f, 0.12f), P(-0.16f, 0.08f), P(-0.235f, 0.0f), P(-0.215f, -0.13f), silver, 0.10f * size, 20);
        d->AddBezierCubic(P(0.03f, 0.12f), P(0.16f, 0.08f), P(0.235f, 0.0f), P(0.215f, -0.13f), silver, 0.10f * size, 20);
        d->AddBezierQuadratic(P(-0.02f, 0.11f), P(-0.12f, 0.05f), P(-0.13f, -0.05f), silver, 0.085f * size, 14);
        d->AddBezierQuadratic(P(0.02f, 0.11f), P(0.12f, 0.05f), P(0.13f, -0.05f), silver, 0.085f * size, 14);
        d->AddQuadFilled(P(-0.09f, 0.0f), P(0.09f, 0.0f), P(0.05f, 0.12f), P(-0.05f, 0.12f), silver);
        d->AddBezierCubic(P(-0.04f, 0.11f), P(-0.16f, 0.06f), P(-0.22f, -0.01f), P(-0.20f, -0.11f), light, 0.022f * size, 20);
        d->AddBezierQuadratic(P(-0.03f, 0.10f), P(-0.10f, 0.05f), P(-0.11f, -0.03f), light, 0.018f * size, 14);
        d->AddBezierCubic(P(0.04f, 0.11f), P(0.16f, 0.06f), P(0.22f, -0.01f), P(0.20f, -0.11f), shade, 0.022f * size, 20);
        d->AddLine(P(0.0f, 0.01f), P(0.0f, 0.11f), shade, 0.012f * size);
        d->PopClipRect();
    }
}

// The drawn platinum's animation, u ms after it starts: the base grows (0.04-0.60 s), a flat
// halo opens (0.25-0.58), the globe lands blue while the hands are still white (0.48-0.76), a
// triangle of light turns around it (0.60-0.90), a bar and short rays (0.90-1.20), the trophy
// flashes white over a plane of light (1.30-1.85), then rests.
void PlatinumAnimation(ImDrawList* d, ImVec2 c, float size, float alpha, float u) {
    if (u < 0.0f) return; // the slot is empty until it starts
    auto P = [&](float x, float y) { return ImVec2(c.x + x * size, c.y + y * size); };
    const ImU32 white = IM_COL32(255, 255, 255, 255);
    const float reveal = EaseInOutSine((u - 40.0f) / 560.0f);
    const float globe = u < 480.0f ? 0.0f : EaseOutBack((u - 480.0f) / 280.0f);
    const float flash = 0.95f * Bump((u - 1300.0f) / 550.0f);
    // The hands: white, then silver from ~0.9 s; the flash; colours once it has passed.
    float hands = 1.0f - 0.85f * Clamp01((u - 900.0f) / 400.0f);
    hands = std::max(hands * Clamp01((1850.0f - u) / 150.0f), flash);
    const float globe_white = std::max(0.25f * Clamp01((900.0f - u) / 300.0f), flash);
    PlatinumTrophy(d, c, size, alpha, hands, reveal, globe, globe_white);
    // Halo: a flat ring opening around the top.
    if (const float h = Flash((u - 250.0f) / 330.0f); h > 0.0f) {
        const float open = EaseOutCubic((u - 250.0f) / 220.0f);
        d->AddEllipse(P(0.0f, kGlobeY + 0.06f), ImVec2((0.26f + 0.20f * open) * size, (0.045f + 0.03f * open) * size),
                      WithAlpha(white, h * alpha), 0.0f, 0, 0.05f * size);
    }
    // A triangle of light turning around the globe.
    if (const float t = Flash((u - 600.0f) / 300.0f); t > 0.0f) {
        const float turn = (u - 600.0f) / 300.0f * 3.6f;
        ImVec2 v[3];
        for (int i = 0; i < 3; ++i) {
            const float a = turn + float(i) * 2.0944f;
            v[i] = P(0.40f * std::cos(a), kGlobeY + 0.17f * std::sin(a) - 0.08f * std::cos(a));
        }
        d->AddTriangle(v[0], v[1], v[2], WithAlpha(white, t * alpha), 0.03f * size);
    }
    // A bar across the globe, then short rays.
    if (const float b = Flash((u - 880.0f) / 130.0f); b > 0.0f)
        d->AddLine(P(-0.40f, kGlobeY + 0.08f), P(0.40f, kGlobeY + 0.08f), WithAlpha(white, b * alpha), 0.04f * size);
    if (const float r = Flash((u - 980.0f) / 220.0f); r > 0.0f) {
        const float reach = EaseOutCubic((u - 980.0f) / 220.0f);
        for (int i = 0; i < 6; ++i) {
            const float a = 0.35f + float(i) * 1.0472f;
            const float r0 = 0.30f, r1 = 0.30f + 0.15f * reach;
            d->AddLine(P(r0 * std::cos(a), kGlobeY + r0 * std::sin(a)), P(r1 * std::cos(a), kGlobeY + r1 * std::sin(a)),
                       WithAlpha(white, r * alpha), 0.035f * size);
        }
    }
    // A plane of light under the base as the trophy flashes.
    if (const float p = Flash((u - 1480.0f) / 380.0f); p > 0.0f) {
        const float wide = 0.20f + 0.40f * EaseOutCubic((u - 1480.0f) / 380.0f);
        const ImVec2 plane[] = {P(-wide, 0.45f), P(0.0f, 0.40f), P(wide, 0.45f), P(0.0f, 0.50f)};
        d->AddConvexPolyFilled(plane, 4, WithAlpha(white, 0.55f * p * alpha));
        d->AddPolyline(plane, 4, WithAlpha(white, p * alpha), ImDrawFlags_Closed, 0.02f * size);
    }
}

// The player's animation in the art slot, `u` ms after it starts; true when something was drawn.
bool DrawAnimation(ImDrawList* d, const Animation& anim, ImVec2 c, float height, float alpha, float u) {
    const int frame = AnimationFrame(anim.frame_ends, u, anim.loop);
    if (frame < 0) return u < 0.0f; // before it starts the slot stays empty
    const int rows = (anim.frames + anim.columns - 1) / anim.columns;
    const float sw = float(anim.sheet.width), sh = float(anim.sheet.height);
    const float fw = sw / float(anim.columns), fh = sh / float(rows);
    const ImVec2 uv0(float(frame % anim.columns) * fw / sw, float(frame / anim.columns) * fh / sh);
    const ImVec2 uv1(uv0.x + fw / sw, uv0.y + fh / sh);
    const float width = height * fw / fh;
    d->AddImage(anim.sheet.Ref(), ImVec2(c.x - width * 0.5f, c.y - height * 0.5f),
                ImVec2(c.x + width * 0.5f, c.y + height * 0.5f), uv0, uv1, IM_COL32(255, 255, 255, int(255 * alpha)));
    return true;
}

// The animation the active toast plays: none for a platinum shown with its own artwork.
const Animation* ToastAnimation() {
    if (active.grade == Grade::Platinum && !active.platinum_art) return nullptr;
    return AnimationFor(active.grade);
}

void DrawCard(ImDrawList* d, const Frame& f, float ms, float s) {
    const ThemeDef& th = *active.theme;
    const Style& st = *th.style;
    const Timeline& tl = active.timeline;
    const Cinematic& cin = th.cinematic;
    const bool cinematic = active.cinematic;
    // The platinum's own layout (its animation or drawn trophy, no cup) unless it shows its artwork.
    const bool platinum = active.grade == Grade::Platinum && active.platinum_art;
    const float k = active.intensity;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float h = std::floor(st.height * s);
    const float top = std::max(st.margin_y * s, top_inset);
    const float w = std::floor(std::min(h * st.aspect, vp->WorkSize.x - 2.0f * st.margin_x * s));
    const float offset = f.offset * (f.phase == Phase::Exit ? st.slide_out : st.slide_in) * h;
    const float x = std::floor(vp->WorkPos.x + vp->WorkSize.x - st.margin_x * s - w + offset);
    const float y = std::floor(vp->WorkPos.y + top - f.drop * h);
    const ImVec2 min(x, y), max(x + w, y + h);
    const float a = f.alpha, r = st.rounding * h;

    float glow = 0.0f; // cinematic platinum: one swell of light, then a faint steady glow
    if (cinematic) {
        const float swell = Bump((ms - cin.glow_at) / cin.glow_len);
        const float steady = cin.steady_glow * Clamp01((ms - cin.glow_at - cin.glow_len * 0.5f) / 300.0f);
        glow = std::max(swell, steady) * k * a;
        for (int i = 1; i <= 3 && glow > 0.0f; ++i) {
            const float e = float(i) * 2.0f * s;
            d->AddRect(ImVec2(min.x - e, min.y - e), ImVec2(max.x + e, max.y + e),
                       WithAlpha(cin.light, glow * 0.16f / float(i)), r + e, 0, 2.0f * s);
        }
    }

    // The card: lighter on the left.
    const int first = d->VtxBuffer.Size;
    d->AddRectFilled(min, max, WithAlpha(st.bg_right, a), r);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(d, first, d->VtxBuffer.Size, min, ImVec2(max.x, min.y),
                                                  st.bg_left | IM_COL32_A_MASK, st.bg_right | IM_COL32_A_MASK);
    if (cinematic)
        d->AddRect(min, max, GradeInk(Grade::Platinum, 0.25f * a + 0.55f * glow), r, 0, 1.3f * s);
    if (cinematic && k > 0.0f) { // a soft highlight crossing once, inside the corners
        const float p = (ms - cin.sweep_at) / cin.sweep_len;
        if (p > 0.0f && p < 1.0f) {
            const float band = w * 0.30f, cx = min.x - band + (w + 2.0f * band) * EaseInOutSine(p);
            const ImU32 clear = WithAlpha(cin.light, 0.0f), lit = WithAlpha(cin.light, 0.10f * k * a);
            d->PushClipRect(ImVec2(min.x + r, min.y + 1.0f), ImVec2(max.x - r, max.y - 1.0f), true);
            d->AddRectFilledMultiColor(ImVec2(cx - band * 0.5f, min.y), ImVec2(cx, max.y), clear, lit, lit, clear);
            d->AddRectFilledMultiColor(ImVec2(cx, min.y), ImVec2(cx + band * 0.5f, max.y), lit, clear, clear, lit);
            d->PopClipRect();
        }
    }

    // The art: the player's animation for the grade, else the drawn platinum, else the artwork.
    const ImVec2 art_c(std::floor(min.x + st.art_x * h), std::floor(min.y + 0.5f * h));
    const float art = std::floor(st.art * h);
    const ImVec2 art_min(art_c.x - art * 0.5f, art_c.y - art * 0.5f), art_max(art_min.x + art, art_min.y + art);
    if (cinematic && glow > 0.0f)
        for (int i = 1; i <= 4; ++i) {
            const float e = float(i) * 1.8f * s;
            d->AddCircle(art_c, art * 0.55f + e, WithAlpha(cin.light, glow * 0.30f / float(i)), 0, 1.8f * s);
        }
    const Animation* anim = ToastAnimation();
    const float anim_start = anim && anim->delay_ms >= 0.0f ? anim->delay_ms : platinum ? tl.trophy_at : 0.0f;
    if (anim && DrawAnimation(d, *anim, art_c, (platinum ? st.sprite : st.art) * h, a, ms - anim_start)) {
    } else if (platinum) {
        PlatinumAnimation(d, art_c, st.trophy * h, a, ms - tl.trophy_at);
    } else {
        const IconSource icons = icon_source.load();
        const ImTextureRef texture = icons ? icons(active.id) : ImTextureRef{};
        if (texture._TexData || texture._TexID != ImTextureID_Invalid)
            d->AddImageRounded(texture, art_min, art_max, ImVec2(0, 0), ImVec2(1, 1),
                               IM_COL32(255, 255, 255, int(255 * a)), st.art_rounding * h);
        else
            Cup(d, ImVec2(art_c.x, art_c.y - 0.08f * art), art * 0.9f, GradeInk(active.grade, a));
    }
    if (cinematic) { // glints on the trophy once it has settled
        const float g1 = Bump((ms - cin.glint_at) / cin.glint_len) * k;
        const float g2 = Bump((ms - cin.glint_at - cin.glint_len * 0.35f) / (cin.glint_len * 0.8f)) * k;
        const ImVec2 at(art_c.x + 0.16f * h, art_c.y - 0.20f * h);
        if (g1 > 0.0f) Star(d, at, (0.04f + 0.09f * g1) * h, IM_COL32(255, 255, 255, int(255 * g1 * a)));
        if (g2 > 0.0f)
            Star(d, ImVec2(at.x - 0.20f * h, at.y + 0.14f * h), (0.03f + 0.05f * g2) * h, WithAlpha(cin.light, g2 * a));
    }

    // Text: the cup and the name, then the second line ("Trophy unlocked!" or the player's).
    ImFont* nf = name_font ? name_font : ImGui::GetFont();
    ImFont* sf = sub_font ? sub_font : nf;
    const float name_px = st.name_size * h, sub_px = st.sub_size * h;
    const float badge_left = max.x - (st.badge_x + st.badge_r + 0.12f) * h;
    const float name_x = std::floor(min.x + (platinum ? st.text_x : st.name_after_cup) * h);
    if (active.fitted_scale != s) {
        active.fitted = Fit(nf, name_px, active.title, badge_left - name_x);
        active.fitted_scale = s;
    }
    // A line's visual centre sits about 0.56 of the font size below its top.
    const float name_top = std::floor(min.y + st.name_y * h - 0.56f * name_px);
    const float sub_top = std::floor(min.y + st.sub_y * h - 0.56f * sub_px);
    if (!platinum) {
        const float cs = st.cup * h / 0.71f;
        Cup(d, ImVec2(min.x + st.cup_x * h, min.y + st.name_y * h - 0.115f * cs), cs, GradeInk(active.grade, a));
    }
    d->AddText(nf, name_px, ImVec2(name_x, name_top), WithAlpha(st.name, a), active.fitted.c_str());
    const float sub_x = std::floor(min.x + st.text_x * h);
    const ImU32 sub_ink = WithAlpha(st.sub, a);
    const ImVec4 sub_clip(sub_x, min.y, badge_left, max.y);
    if (active.grade_label && !active.grade_name.empty()) {
        const char* g = active.grade_name.c_str();
        d->AddText(sf, sub_px, ImVec2(sub_x, sub_top), GradeInk(active.grade, a), g, nullptr, 0.0f, &sub_clip);
        const float gx = sub_x + sf->CalcTextSizeA(sub_px, FLT_MAX, 0.0f, g).x;
        const std::string rest = "  ·  " + active.unlocked_text;
        d->AddText(sf, sub_px, ImVec2(gx, sub_top), sub_ink, rest.c_str(), nullptr, 0.0f, &sub_clip);
    } else {
        d->AddText(sf, sub_px, ImVec2(sub_x, sub_top), sub_ink, active.unlocked_text.c_str(), nullptr, 0.0f, &sub_clip);
    }

    Badge(d, ImVec2(std::floor(max.x - st.badge_x * h), std::floor(min.y + 0.5f * h)), st.badge_r * h,
          a * f.badge, ms, tl);
}

// Theme Corner: rises from the bottom edge as it fades in, sinks back as it fades out.
void DrawCorner(ImDrawList* d, const Frame& f, float ms, float s) {
    const CornerStyle& st = kCorner;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float w = std::floor(std::min(st.width * s, vp->WorkSize.x));
    const float h = std::floor(st.height * s);
    const float x = std::floor(vp->WorkPos.x + vp->WorkSize.x - w);
    const float y = std::floor(vp->WorkPos.y + vp->WorkSize.y - h + f.offset * 0.6f * h);
    const ImVec2 min(x, y), max(x + w, y + h);
    const float a = f.alpha;
    d->AddRectFilledMultiColor(min, max, WithAlpha(st.bg_top, a), WithAlpha(st.bg_top, a), WithAlpha(st.bg_bottom, a),
                               WithAlpha(st.bg_bottom, a));
    // A hairline on the two edges away from the screen's.
    const float line = std::max(1.0f, std::floor(s));
    d->AddRectFilled(min, ImVec2(max.x, min.y + line), WithAlpha(st.edge, a));
    d->AddRectFilled(ImVec2(min.x, min.y + line), ImVec2(min.x + line, max.y), WithAlpha(st.edge, a));

    // The artwork, in a frame of the grade's colour: the player's animation for the grade, else
    // the trophy's own, else its cup.
    const float pad = st.pad * s, art = std::floor(st.art * s), frame = std::max(1.0f, std::floor(st.frame * s));
    const ImVec2 art_min(std::floor(min.x + pad), std::floor(min.y + (h - art) * 0.5f));
    const ImVec2 art_max(art_min.x + art, art_min.y + art);
    const ImVec2 art_c((art_min.x + art_max.x) * 0.5f, (art_min.y + art_max.y) * 0.5f);
    d->AddRectFilled(ImVec2(art_min.x - frame, art_min.y - frame), ImVec2(art_max.x + frame, art_max.y + frame),
                     GradeInk(active.grade, a));
    d->AddRectFilled(art_min, art_max, WithAlpha(st.bg_bottom, a));
    const Animation* anim = ToastAnimation();
    const float anim_start = anim && anim->delay_ms >= 0.0f ? anim->delay_ms : 0.0f;
    if (!(anim && DrawAnimation(d, *anim, art_c, art, a, ms - anim_start))) {
        const IconSource icons = icon_source.load();
        const ImTextureRef texture = icons ? icons(active.id) : ImTextureRef{};
        if (texture._TexData || texture._TexID != ImTextureID_Invalid)
            d->AddImage(texture, art_min, art_max, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, int(255 * a)));
        else
            Cup(d, ImVec2(art_c.x, art_c.y - 0.08f * art), art * 0.9f, GradeInk(active.grade, a));
    }

    // Text: the name on top, then the trophy's detail in up to two lines ("Trophy unlocked!"
    // without one), with the grade's name before it when asked.
    ImFont* nf = name_font ? name_font : ImGui::GetFont();
    ImFont* sf = sub_font ? sub_font : nf;
    const float name_px = std::floor(st.name_px * s), sub_px = std::floor(st.sub_px * s);
    const float text_x = art_max.x + frame + pad, text_right = max.x - pad;
    const std::string grade = active.grade_label && !active.grade_name.empty() ? active.grade_name + "  ·  " : "";
    if (active.fitted_scale != s) {
        active.fitted = Fit(nf, name_px, active.title, text_right - text_x);
        const std::string& detail = active.detail.empty() ? active.unlocked_text : active.detail;
        active.detail_lines = Wrap(sf, sub_px, grade + detail, text_right - text_x, 2);
        active.fitted_scale = s;
    }
    const float gap = std::floor(st.line_gap * s);
    const float lines = float(active.detail_lines.size());
    const float block = name_px + lines * (gap + sub_px);
    const float name_top = std::floor(min.y + (h - block) * 0.5f);
    d->AddText(nf, name_px, ImVec2(text_x, name_top), WithAlpha(st.name, a), active.fitted.c_str());
    float line_top = name_top + name_px + gap;
    for (size_t i = 0; i < active.detail_lines.size(); ++i, line_top += sub_px + gap) {
        const std::string& text = active.detail_lines[i];
        float line_x = text_x;
        size_t from = 0;
        if (i == 0 && !grade.empty() && text.compare(0, grade.size(), grade) == 0) {
            d->AddText(sf, sub_px, ImVec2(line_x, line_top), GradeInk(active.grade, a), grade.c_str());
            line_x += sf->CalcTextSizeA(sub_px, FLT_MAX, 0.0f, grade.c_str()).x;
            from = grade.size();
        }
        d->AddText(sf, sub_px, ImVec2(line_x, line_top), WithAlpha(st.sub, a), text.c_str() + from);
    }
}

Theme ShownTheme(const Options& o) {
    const int forced = theme_override.load();
    return forced < int(Theme::Count) ? Theme(forced) : o.theme;
}

Entry Find(int id) {
    const TrophyLookup lookup = trophy_lookup.load();
    return lookup ? lookup(id) : Entry{};
}

bool Activate(double now) {
    Request request;
    {
        std::scoped_lock lock{queue_mutex};
        if (queue.empty()) return false;
        request = queue.front();
        queue.pop_front();
        --pending;
    }
    const Entry entry = Find(request.id);
    const Options o = GetOptions();
    const ThemeDef& th = Def(ShownTheme(o));
    const Grade grade = request.grade != Grade::Unknown ? request.grade : entry.grade;
    const bool platinum = grade == Grade::Platinum;
    active.id = request.id;
    active.grade = grade;
    active.start = now;
    active.theme = &th;
    active.timeline = platinum ? th.platinum : th.normal;
    if (o.hold_seconds > 0.0f) active.timeline.hold = o.hold_seconds * 1000.0f;
    active.cinematic = platinum && th.cinematic.enabled;
    active.intensity = float(o.cinematic_intensity) / 100.0f;
    active.grade_label = o.grade_label;
    active.platinum_art = o.platinum_animation;
    active.title = entry.name;
    active.detail = CutUtf8(entry.detail, 300);
    {
        std::scoped_lock lock{default_mutex};
        active.unlocked_text = o.text_unlocked.empty() && !default_unlocked.empty() ? default_unlocked : UnlockedText(o);
        const int g = int(grade);
        active.grade_name = g >= 1 && g <= 4 && o.grade_names[size_t(g)].empty() && !default_grades[size_t(g)].empty()
                                ? default_grades[size_t(g)]
                                : GradeName(o, g);
    }
    active.fitted_scale = -1.0f;
    showing = true;
    PostSound(grade);
    return true;
}

} // namespace

Options GetOptions() {
    std::scoped_lock lock{options_mutex};
    return options;
}

void SetOptions(const Options& o) {
    {
        std::scoped_lock lock{options_mutex};
        options = o;
        if (int(options.theme) < 0 || int(options.theme) >= int(Theme::Count)) options.theme = Theme::Original;
    }
    Post(ApplyGain);
    PostSave();
}

void Start(ImFontAtlas* atlas, IconSource icons, TrophyLookup lookup) {
    icon_source = icons;
    trophy_lookup = lookup;
    LoadOptions();
    if (atlas) {
        // Fonts: <custom_trophy>/font.ttf (and font-light.ttf for the second line) when the player
        // provides them, else Segoe UI and Segoe UI Semilight on Windows, else the overlay's font.
        const fs::path dir = TrophyDir();
        // Each with the overlay's DejaVu Sans merged in, for the characters the face lacks.
        auto add = [&](const fs::path& path) -> ImFont* {
            std::error_code ec;
            if (path.empty() || !fs::is_regular_file(path, ec)) return nullptr;
            const auto name = path.u8string(); // ImGui opens UTF-8 paths
            ImFont* font = atlas->AddFontFromFileTTF(reinterpret_cast<const char*>(name.c_str()), 20.0f);
            if (!font) return nullptr;
            ImFontConfig merge;
            merge.MergeMode = true;
            merge.FontDataOwnedByAtlas = false;
            atlas->AddFontFromMemoryTTF(const_cast<unsigned char*>(spartanport_font_ttf), int(spartanport_font_ttf_end - spartanport_font_ttf),
                                        20.0f, &merge);
            return font;
        };
        name_font = add(FindFile(dir, "font", {".ttf", ".otf"}));
        sub_font = add(FindFile(dir, "font-light", {".ttf", ".otf"}));
        if (name_font && !sub_font) sub_font = name_font;
#ifdef _WIN32
        if (const char* windows = std::getenv("WINDIR")) {
            const fs::path fonts = fs::path(windows) / "Fonts";
            if (!name_font) name_font = add(fonts / "segoeui.ttf");
            if (!sub_font) sub_font = add(fonts / "segoeuisl.ttf");
        }
#endif
        if (!sub_font) sub_font = name_font;
        // Pictures need the ImGui context (textures): here, once.
        {
            const fs::path badge = FindFile(dir, "badge", {".png", ".webp", ".jpg", ".jpeg", ".bmp"});
            SpartanToastMedia::AnimationLimits one;
            one.max_side = 512;
            one.max_frames = 1;
            if (!badge.empty()) {
                const auto image = SpartanToastMedia::DecodeAnimation(badge, one);
                badge_picture = MakePicture(image.pixels.data(), image.width, image.height);
                if (badge_picture.Ready()) badge_source = badge.filename().string();
                if (!badge_picture.Ready())
                    std::printf("Trophy notifications: cannot read %s\n", badge.string().c_str());
            }
        }
        for (size_t g = 0; g < 5; ++g) animations[g] = LoadAnimation(dir, kGradeFileNames[g]);
    }
    Post(LoadSounds);
}

void SetDefaultTexts(const std::string& unlocked, const std::array<std::string, 5>& grade_names) {
    std::scoped_lock lock{default_mutex};
    default_unlocked = unlocked;
    default_grades = grade_names;
}

void ReloadSounds() { Post(LoadSounds); }

void PlaySound(int id, int grade) {
    PostSound(grade >= 1 && grade <= 4 ? Grade(grade) : Find(id).grade);
}

// A grade's name for the menu: notifications.ini's, else the player's language (SetDefaultTexts).
static std::string ShownGradeName(const Options& o, int g) {
    std::scoped_lock lock{default_mutex};
    return o.grade_names[size_t(g)].empty() && !default_grades[size_t(g)].empty() ? default_grades[size_t(g)]
                                                                                 : GradeName(o, g);
}

std::string SoundSummary() {
    const Options o = GetOptions();
    std::array<std::string, 5> sources;
    {
        Audio& a = audio();
        std::scoped_lock lock{a.mutex};
        if (!a.loaded) return "...";
        sources = a.sources;
    }
    std::string out;
    for (int g = 1; g < 5; ++g) {
        if (g > 1) out += "  ·  ";
        out += ShownGradeName(o, g) + ": " + (sources[size_t(g)].empty() ? SpartanUi::Tr("none") : sources[size_t(g)]);
    }
    return out;
}

std::string PictureSummary() {
    using SpartanUi::Tr;
    const Options o = GetOptions();
    std::string out = std::string(Tr("Badge")) + ": " + (badge_picture.Ready() ? badge_source : std::string(Tr("drawn")));
    for (int g = 1; g < 5; ++g) {
        const Animation* anim = AnimationFor(Grade(g));
        const bool artwork = g == 4 && !o.platinum_animation;
        out += "  ·  " + ShownGradeName(o, g) + ": " +
               (artwork ? Tr("artwork") : anim ? anim->source.c_str() : g == 4 ? Tr("drawn") : Tr("artwork"));
    }
    return out;
}

void Enqueue(int id, bool preview, int grade) {
    const Grade forced = grade >= 1 && grade <= 4 ? Grade(grade) : Grade::Unknown;
    std::scoped_lock lock{queue_mutex};
    if (queue.size() >= kQueueLimit) return;
    queue.push_back({id, preview, forced});
    ++pending;
}

void Clear() {
    std::scoped_lock lock{queue_mutex};
    pending -= int(queue.size());
    queue.clear();
    showing = false;
}

bool Busy() { return showing.load() || pending.load() > 0; }

void Draw(float scale, float top) {
    const auto t0 = std::chrono::steady_clock::now();
    const double now = NowMs();
    if (!showing.load()) {
        if (now < next_start || !Activate(now)) return;
    }
    const float ms = float(now - active.start);
    const Timeline& tl = active.timeline;
    ImDrawList* d = ImGui::GetForegroundDrawList();
    const int before = d->VtxBuffer.Size;
    const Frame f = Evaluate(tl, active.cinematic ? active.theme->cinematic.settle : 0.0f, ms);
    if (f.phase == Phase::Done) {
        showing = false;
        next_start = now + tl.gap;
        return;
    }
    top_inset = top;
    if (active.theme->renderer == Renderer::Corner) DrawCorner(d, f, ms, scale);
    else DrawCard(d, f, ms, scale);
    stat_vertices = unsigned(d->VtxBuffer.Size - before);
    const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    stat_last = us;
    stat_sum = stat_sum.load() + us;
    stat_max = std::max(stat_max.load(), us);
    ++stat_frames;
}

void SetClockOverride(double ms) { clock_override = ms; }
void SetThemeOverride(Theme theme) { theme_override = int(theme); }

DrawStats Stats() {
    const unsigned n = stat_frames.load();
    return {stat_last.load(), n ? stat_sum.load() / n : 0.0, stat_max.load(), n, stat_vertices.load()};
}

void ResetStats() {
    stat_last = 0;
    stat_sum = 0;
    stat_max = 0;
    stat_frames = 0;
}

} // namespace SpartanToast
