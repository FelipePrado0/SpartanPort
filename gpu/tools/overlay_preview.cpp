// SPDX-License-Identifier: GPL-2.0-or-later
// Draws the overlay offscreen into BMP files, through ImGui's SDL software renderer: the Insert menu,
// the trophy window and the trophy notifications, for working on their look without the game (and for
// the pictures in the documentation). The overlay's own code is compiled here under another
// namespace. Nothing of the game is used: the trophies are made up here, their icons drawn, the
// background generated.
//   overlay-preview OUT.bmp SCENE [WIDTH HEIGHT]
// SCENE: menu (SPARTAN_PREVIEW_TAB 0-3), trophies, notifications (the trophy window's pages, with
// SPARTAN_PREVIEW_ORDER 0-2, SPARTAN_PREVIEW_FILTER 0-2 and SPARTAN_PREVIEW_GRADE 0-4), saves (the
// game's save list, SPARTAN_PREVIEW_SAVE=1 to save), overwrite (its question), health (the enemy
// health bar, after a hit down to SPARTAN_PREVIEW_HEALTH percent; with frames, three hits), or toast.
// SPARTAN_PREVIEW_TOAST=1..4 (bronze..platinum, or several: "1234") adds notifications, over any
// scene, in SPARTAN_PREVIEW_THEME (original, card, card_cinematic, corner), drawn
// SPARTAN_PREVIEW_TOAST_MS after they start; with SPARTAN_PREVIEW_FRAMES=N and SPARTAN_PREVIEW_STEP_MS,
// N pictures OUT_000.bmp, OUT_001.bmp... (the Original theme runs on the real clock, the others on a
// fixed one). SPARTAN_PREVIEW_LANG: the language ("pt", "ru"...); the trophy texts in it come from
// SPARTAN_USER_DIR/trophy_texts/<language>.txt when there is one, as in the game.
#define SPARTAN_OVERLAY_PREVIEW 1
#define SpartanOverlay SpartanOverlayPreview
#include "../shim/spartanport_overlay.cpp"
#undef SpartanOverlay

#include <thread>
#include "imgui_impl_sdlrenderer3.h"

using namespace SpartanOverlayPreview;

// ElfInfo's fields are private to Core::Emulator; the game fills them in gpu/shim/spartanportgpu.cpp,
// the preview with a made-up game (the menu's heading and its About section read them).
namespace Core {
class Emulator {
public:
    static void PreviewElfInfo() {
        auto& info = Common::ElfInfo::Instance();
        info.initialized = true;
        info.game_serial = "DEMO00000";
        info.title = "SpartanPort";
        info.app_ver = "01.00";
    }
};
} // namespace Core

// What spartanport-probe provides to the library (as tests/test_gpu_runtime_stubs.cpp, with the
// Windows jump buffer): the preview has no guest process, never restarts and never runs game code.
#include "spartanport_toggles.h"
extern "C" {
__thread sigjmp_buf* runtime_fault_recover = nullptr;
uint64_t runtime_disabled_optimizations = 0;
uint64_t runtime_tsc_frequency() { return 1000000000; }
int runtime_file_translate(const char*, char*, size_t) { std::abort(); }
uint64_t runtime_memory_clamp(uintptr_t, uint64_t) { std::abort(); }
uint64_t runtime_process_time_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
int runtime_memory_region(uintptr_t, uintptr_t*, uintptr_t*, int*) { std::abort(); }
void runtime_memory_set_gpu_hooks(void (*)(uintptr_t, uint64_t), void (*)(uintptr_t, uint64_t),
                                  void (*)(uintptr_t, uint64_t)) { std::abort(); }
void runtime_thread_attach_host(const char*) {}
void runtime_memory_gpu_protect(uintptr_t, uint64_t, int, int) { std::abort(); }
void runtime_restart() { std::abort(); }
uint64_t runtime_process_time_counter() { return runtime_process_time_us() * 1000; }
uint64_t runtime_read_tsc() { return runtime_process_time_counter(); }
int32_t* runtime_errno() { std::abort(); }
int runtime_memory_write_backing(uintptr_t, const void*, uint64_t) { std::abort(); }
void runtime_report() {}
}

namespace {

// The made-up trophies: id, name, what it asks, grade, share of players, unlocked.
struct Demo {
    const char* name;
    const char* detail;
    SpartanToast::Grade grade;
    float earned;
    bool unlocked;
};
using G = SpartanToast::Grade;
constexpr Demo kDemo[] = {
    {"Legend of the Arena", "Unlock every other trophy", G::Platinum, 9.8f, true},
    {"First Blood", "Defeat your first enemy", G::Bronze, 97.4f, true},
    {"Treasure Hunter", "Open 20 chests", G::Bronze, 71.2f, true},
    {"Sure Footed", "Cross the long bridge without falling", G::Bronze, 64.5f, true},
    {"Sharp Eye", "Hit 50 targets from afar", G::Bronze, 55.0f, false},
    {"Marathon", "Run 42 kilometres", G::Bronze, 47.3f, true},
    {"Night Watch", "Light every brazier in the old fort", G::Bronze, 38.9f, false},
    {"Combo Artist", "Land a 200-hit combo", G::Bronze, 22.3f, false},
    {"Collector", "Find every hidden relic", G::Silver, 30.1f, true},
    {"Unbroken", "Finish a chapter without healing", G::Silver, 14.6f, false},
    {"Speed Runner", "Finish the story in under six hours", G::Gold, 4.2f, true},
    {"Untouchable", "Defeat the last boss without being hit", G::Gold, 2.7f, false},
};
constexpr int kDemoCount = int(std::size(kDemo));

// An icon: a dark rounded tile with a cup in the grade's colour (bluish platinum, grey silver, muted
// bronze, saturated gold: the colours SpartanToast::GradeFromIcon tells apart).
SpartanPng::Image DemoIcon(G grade, int seed) {
    constexpr int n = 64;
    SpartanPng::Image image;
    image.width = image.height = n;
    image.rgba.resize(size_t(n) * n * 4);
    unsigned char ink[3] = {200, 200, 200};
    switch (grade) {
    case G::Platinum: ink[0] = 150, ink[1] = 190, ink[2] = 240; break;
    case G::Gold: ink[0] = 235, ink[1] = 175, ink[2] = 35; break;
    case G::Silver: ink[0] = 190, ink[1] = 192, ink[2] = 186; break;
    default: ink[0] = 190, ink[1] = 112, ink[2] = 78; break;
    }
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            unsigned char* p = image.rgba.data() + (size_t(y) * n + x) * 4;
            const float fx = (x - 31.5f) / 32.0f, fy = (y - 31.5f) / 32.0f;
            // a soft vignette, a touch different per trophy
            const float glow = std::max(0.0f, 1.0f - (fx * fx + fy * fy)) * (0.35f + 0.05f * float(seed % 4));
            float r = 14 + 40 * glow, g = 16 + 40 * glow, b = 22 + 46 * glow;
            const bool bowl = fy > -0.55f && fy < 0.05f && std::abs(fx) < 0.42f - (fy + 0.55f) * 0.35f;
            const bool stem = fy >= 0.05f && fy < 0.35f && std::abs(fx) < 0.08f;
            const bool base = fy >= 0.35f && fy < 0.5f && std::abs(fx) < 0.3f;
            const bool handle = fy > -0.45f && fy < -0.15f && std::abs(fx) > 0.42f && std::abs(fx) < 0.56f;
            if (bowl || stem || base || handle) {
                const float shade = 0.75f + 0.25f * (1.0f - (fy + 0.6f));
                r = ink[0] * shade, g = ink[1] * shade, b = ink[2] * shade;
            }
            p[0] = (unsigned char)std::min(r, 255.0f), p[1] = (unsigned char)std::min(g, 255.0f);
            p[2] = (unsigned char)std::min(b, 255.0f), p[3] = 255;
        }
    return image;
}

uint32_t demo_flags[4];

// The trophy window and the notifications get the made-up trophies, as spartanportgpu_trophy_setup does.
void LoadDemoTrophies() {
    std::vector<TrophyRow> rows;
    std::vector<SpartanToast::Entry> entries;
    for (int id = 0; id < kDemoCount; ++id) {
        TrophyRow row;
        row.id = id;
        row.name = kDemo[id].name;
        row.detail = kDemo[id].detail;
        row.earned = kDemo[id].earned;
        row.image = DemoIcon(kDemo[id].grade, id);
        const G grade = SpartanToast::GradeFromIcon(row.image.rgba.data(), row.image.width, row.image.height);
        row.grade = grade;
        if (grade != kDemo[id].grade) std::printf("Overlay preview: demo icon %d reads as grade %d\n", id, int(grade));
        entries.push_back({row.name, row.detail, grade});
        rows.push_back(std::move(row));
        if (kDemo[id].unlocked) demo_flags[id / 32] |= 1u << (id % 32);
    }
    TrophyLabels labels;
    const char* env = std::getenv("SPARTAN_UI_LANGUAGE");
    const std::string_view code = SpartanUi::Languages[SpartanUi::FindLanguage(env ? env : "")].code;
    const auto texts = ReadTrophyTexts(code);
    if (texts.count) TakeTrophyTexts(texts, labels, rows, entries);
    const char* user = std::getenv("SPARTAN_USER_DIR");
    std::printf("Overlay preview: trophy texts %s/trophy_texts/%.*s.txt: %d trophies\n", user ? user : "user",
                int(code.size()), code.data(), texts.count);
    {
        std::scoped_lock lock{trophy_entry_mutex};
        trophy_entries = std::move(entries);
    }
    trophy_rows = std::move(rows);
    trophy_labels = std::move(labels);
    trophy_flags = demo_flags;
    trophy_ready = true;
    trophy_started = true;
}

// A generated background: a dusky gradient with a few soft warm lights (no picture of the game),
// computed once per size.
void Background(SDL_Renderer* renderer, int width, int height) {
    static SDL_Texture* texture = nullptr;
    if (!texture) {
        std::vector<Uint32> pixels(size_t(width) * height);
        const float lights[][4] = {{0.16f, 0.72f, 0.30f, 0.9f}, {0.70f, 0.38f, 0.38f, 0.7f}, {0.48f, 0.95f, 0.25f, 0.8f}};
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                const float t = float(y) / float(height);
                float r = 30 + 26 * t, g = 26 + 12 * t, b = 40 - 16 * t;
                for (const auto& l : lights) {
                    const float dx = (float(x) / float(width) - l[0]) * float(width) / float(height);
                    const float dy = float(y) / float(height) - l[1];
                    const float k = std::exp(-(dx * dx + dy * dy) / (l[2] * l[2])) * l[3];
                    r += 90 * k, g += 46 * k, b += 18 * k;
                }
                pixels[size_t(y) * width + x] = 0xFF000000u | Uint32(std::min(r, 255.0f)) << 16 |
                                                Uint32(std::min(g, 255.0f)) << 8 | Uint32(std::min(b, 255.0f));
            }
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, width, height);
        SDL_UpdateTexture(texture, nullptr, pixels.data(), width * 4);
    }
    SDL_RenderTexture(renderer, texture, nullptr, nullptr);
}

int Env(const char* name, int fallback) {
    const char* value = std::getenv(name);
    return value && *value ? std::atoi(value) : fallback;
}

// A save picture (228 x 128, as the game's): a made-up dusk over hills, its colours by `seed`.
SpartanPng::Image DemoSavePicture(int seed) {
    SpartanPng::Image image;
    image.width = 228;
    image.height = 128;
    image.rgba.resize(size_t(image.width) * image.height * 4);
    const float hue[][3] = {{0.95f, 0.45f, 0.25f}, {0.35f, 0.55f, 0.95f}, {0.85f, 0.75f, 0.35f}, {0.55f, 0.85f, 0.65f}};
    const float* sky = hue[seed % 4];
    for (int y = 0; y < image.height; ++y)
        for (int x = 0; x < image.width; ++x) {
            const float u = float(x) / float(image.width), v = float(y) / float(image.height);
            const float ridge = 0.62f + 0.12f * std::sin(u * 7.0f + float(seed)) + 0.06f * std::sin(u * 19.0f + float(seed) * 2.0f);
            const float hills = 0.52f + 0.08f * std::sin(u * 4.0f - float(seed));
            float k = 0.25f + 0.75f * (1.0f - v);
            float r = 30 + 200 * sky[0] * k, g = 20 + 170 * sky[1] * k, b = 30 + 150 * sky[2] * k;
            if (v > hills) r *= 0.55f, g *= 0.55f, b *= 0.6f;
            if (v > ridge) r *= 0.3f, g *= 0.3f, b *= 0.35f;
            unsigned char* p = &image.rgba[(size_t(y) * image.width + x) * 4];
            p[0] = (unsigned char)std::min(r, 255.0f), p[1] = (unsigned char)std::min(g, 255.0f);
            p[2] = (unsigned char)std::min(b, 255.0f), p[3] = 255;
        }
    return image;
}

// The game's save list with made-up saves: "saves" loads (SPARTAN_PREVIEW_SAVE=1: saves, with a new
// save first), "overwrite" asks before saving over one. Its texts are the runtime's (English or
// Portuguese, src/runtime_services.c).
void DemoSaves(const std::string& scene, bool portuguese) {
    const auto t = [&](const char* en, const char* pt) { return std::string(portuguese ? pt : en); };
    const std::string places[] = {"Storm Peak", "Ember Halls", "Sunken Archive"};
    const char* times[] = {"2026-10-26 21:14", "2026-10-25 23:02", "2026-10-24 19:47"};
    const char* played[] = {"6:42", "4:18", "2:05"};
    std::vector<std::string> items;
    std::vector<SpartanPng::Image> pictures;
    const bool saving = scene == "overwrite" || Env("SPARTAN_PREVIEW_SAVE", 0) != 0;
    if (saving && scene != "overwrite") {
        items.push_back(t("New save", "Novo save"));
        pictures.emplace_back();
    }
    for (int i = 0; i < 3; ++i) {
        items.push_back("SpartanPort demo\n" + places[i] + "\n" + times[i] + "\n" +
                        t("Difficulty: Spartan  ·  Play time ", "Dificuldade: Espartano  ·  Tempo de jogo ") + played[i]);
        pictures.push_back(DemoSavePicture(i));
    }
    if (scene == "overwrite") {
        const std::string question = t("Overwrite this save?", "Sobrescrever este save?") + "\n\n" + places[0] + "\n" +
                                     times[0] + "\n" +
                                     t("Difficulty: Spartan  ·  Play time ", "Dificuldade: Espartano  ·  Tempo de jogo ") +
                                     played[0];
        BeginChoice(question, {t("Overwrite", "Sobrescrever"), t("Cancel", "Cancelar")}, {}, 1,
                    t("New save", "Novo save"), DemoSavePicture(3));
        return;
    }
    BeginChoice(saving ? t("Save game", "Salvar jogo") : t("Load game", "Carregar jogo"), items, std::move(pictures),
                saving ? 2 : 1);
}

} // namespace

// The enemy health bar's made-up enemy: the hits so far at `ms` on the bar's clock. One picture: a hit
// at 0 ms down to SPARTAN_PREVIEW_HEALTH percent; frames: full, then a hit to half, then one to almost none.
void DemoHealth(double ms, bool frames) {
    struct Hit {
        double at;
        float health;
    };
    constexpr float kMax = 1200.0f;
    const float health = kMax * float(std::clamp(Env("SPARTAN_PREVIEW_HEALTH", 100), 0, 100)) / 100.0f;
    const Hit one[] = {{0.0, health}};
    const Hit three[] = {{0.0, kMax}, {700.0, kMax * 0.5f}, {1900.0, kMax * 0.08f}};
    const Hit* hits = frames ? three : one;
    const int count = frames ? 3 : 1;
    int last = 0;
    while (last + 1 < count && hits[last + 1].at <= ms) ++last;
    SpartanActorSlot& slot = spartanport_actor_slots[1];
    slot.actor = 1;
    slot.health = hits[last].health;
    slot.max_health = kMax;
    slot.seq = uint32_t(last + 1);
    health_clock_ms = ms;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "overlay-preview OUT.bmp menu|trophies|notifications|saves|overwrite|health|toast [WIDTH HEIGHT]\n");
        return 2;
    }
    const std::string out = argv[1], scene = argv[2];
    const int width = argc > 4 ? std::atoi(argv[3]) : 1920, height = argc > 4 ? std::atoi(argv[4]) : 1080;
    if (const char* language = std::getenv("SPARTAN_PREVIEW_LANG")) {
        SpartanSettings::Get().ui_language = SpartanUi::FindLanguage(language);
        static std::string env = std::string("SPARTAN_UI_LANGUAGE=") + language;
        putenv(env.data());
    }

    Core::Emulator::PreviewElfInfo();
    SDL_Surface* surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_XRGB8888);
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ConfigureContext();
    io.IniFilename = nullptr;
    // The software renderer draws ImGui's textured thin lines with gaps; plain geometry instead.
    ImGui::GetStyle().AntiAliasedLinesUseTex = false;
    ImGui_ImplSDLRenderer3_Init(renderer);
    LoadDemoTrophies();
    StartNotifications();
    initialized = true;
    const float scale = std::max(float(height) / 1080.0f, 0.75f);
    ImGui::GetStyle().ScaleAllSizes(scale);
    ImGui::GetStyle().FontScaleMain = scale;
    base_scale = scale;

    // As in a game version the hooks recognise (SPARTAN_PREVIEW_HOOKS=0: none of them).
    if (Env("SPARTAN_PREVIEW_HOOKS", 1)) {
        auto& settings = SpartanSettings::Get();
        settings.cheats_supported = (1u << SPARTAN_CHEAT_COUNT) - 1;
        settings.red_orbs_supported = settings.orb_pickup_supported = settings.damage_supported = true;
        settings.actor_watch_supported = settings.qte_supported = true;
    }
    if (scene == "menu") {
        SetOpen(true);
        io.MouseDrawCursor = false; // no pointer in the pictures
        requested_tab = std::clamp(Env("SPARTAN_PREVIEW_TAB", 0), 0, 3);
    } else if (scene == "trophies" || scene == "notifications") {
        SetTrophyView(true);
        trophy_order = std::clamp(Env("SPARTAN_PREVIEW_ORDER", SpartanTrophyRarity::MostEarned), 0, 2);
        trophy_filter = std::clamp(Env("SPARTAN_PREVIEW_FILTER", 0), 0, 2);
        trophy_grade = std::clamp(Env("SPARTAN_PREVIEW_GRADE", 0), 0, 4); // 1 bronze .. 4 platinum
        if (scene == "notifications") SetTrophyPage(1);
    } else if (scene == "saves" || scene == "overwrite") {
        const char* language = std::getenv("SPARTAN_PREVIEW_LANG");
        DemoSaves(scene, language && std::string(language) == "pt");
    } else if (scene == "health") {
        SpartanSettings::Get().enemy_health_bar = true;
    }
    // One grade, or several in a row ("1234": bronze, silver, gold, platinum).
    const char* grades = std::getenv("SPARTAN_PREVIEW_TOAST");
    const std::string toasts = grades ? grades : "";
    const int grade = toasts.empty() ? 0 : toasts[0] - '0';
    SpartanToast::Theme theme = SpartanToast::Theme::Card;
    if (const char* name = std::getenv("SPARTAN_PREVIEW_THEME"))
        for (int i = 0; i < int(SpartanToast::Theme::Count); ++i)
            if (std::string(name) == SpartanToast::ThemeKey(SpartanToast::Theme(i))) theme = SpartanToast::Theme(i);
    const bool original = theme == SpartanToast::Theme::Original;
    const double toast_ms = Env("SPARTAN_PREVIEW_TOAST_MS", 1500);
    const int frames = std::max(1, Env("SPARTAN_PREVIEW_FRAMES", 1));
    const double step_ms = Env("SPARTAN_PREVIEW_STEP_MS", 33);
    if (grade >= 1 && grade <= 4) { // any scene: a notification over the menu or the trophy window too
        SpartanToast::SetThemeOverride(theme);
        if (!original) SpartanToast::SetClockOverride(0.0);
        for (const char g : toasts) {
            const int shown = g - '0';
            int id = -1;
            for (int i = 0; i < kDemoCount && id < 0; ++i)
                if (int(kDemo[i].grade) == shown) id = i;
            if (id < 0) continue;
            if (original) PushNotice(kDemo[id].name, kDemo[id].detail, DemoIcon(kDemo[id].grade, id));
            else SpartanToast::Enqueue(id, true, shown);
        }
    }

    auto draw = [&](double ms) {
        if (grade >= 1 && !original) SpartanToast::SetClockOverride(ms);
        UpdateToastTextures();
        UpdateTrophyTextures();
        UpdateChoiceTextures();
        if (scene == "health") DemoHealth(ms, frames > 1);
        io.DisplaySize = ImVec2(float(width), float(height));
        io.DeltaTime = frames > 1 ? float(step_ms) / 1000.0f : 1.0f / 60.0f; // ImGui's clock (the platinum's light)
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui::NewFrame();
        DrawWindows();
        ImGui::Render();
        Background(renderer, width, height);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    };
    std::this_thread::sleep_for(std::chrono::milliseconds(400)); // the notifications' worker loads its files
    // SPARTAN_PREVIEW_WHEEL=N: the mouse wheel turned N notches down over the middle of the screen
    // (the menu's lower sections).
    const int wheel = Env("SPARTAN_PREVIEW_WHEEL", 0);
    for (int warm = 0; warm < 3 + (wheel ? 3 : 0); ++warm) { // layouts and textures settle over a frame or two
        if (wheel && warm >= 2) {
            io.AddMousePosEvent(float(width) * 0.5f, float(height) * 0.5f);
            io.AddMouseWheelEvent(0.0f, -float(wheel));
        }
        draw(0.0);
    }
    bool saved = true;
    if (frames == 1) {
        if (grade >= 1 && original) std::this_thread::sleep_for(std::chrono::milliseconds(int(toast_ms)));
        draw(toast_ms);
        saved = SDL_SaveBMP(surface, out.c_str());
    } else {
        const auto start = std::chrono::steady_clock::now();
        for (int f = 0; f < frames && saved; ++f) {
            const double ms = f * step_ms;
            if (original) std::this_thread::sleep_until(start + std::chrono::microseconds(int64_t(ms * 1000.0)));
            draw(ms);
            char name[32];
            std::snprintf(name, sizeof(name), "_%03d.bmp", f);
            const std::string path = out.substr(0, out.rfind('.')) + name;
            saved = SDL_SaveBMP(surface, path.c_str());
        }
    }
    if (scene == "toast") {
        const auto stats = SpartanToast::Stats();
        std::printf("Overlay preview: toast busy %d, %u frames drawn, last %u vertices\n", int(SpartanToast::Busy()),
                    stats.frames, stats.vertices);
    }
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    std::printf("Overlay preview: %s %s\n", out.c_str(), saved ? "written" : SDL_GetError());
    return saved ? 0 : 1;
}
