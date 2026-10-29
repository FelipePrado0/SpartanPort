// SPDX-License-Identifier: GPL-2.0-or-later
#include "spartanport_overlay.h"
#include "spartanport_ui_font.h"

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include "spartanport_settings.h"
#include "spartanport_orbs.h"
#include "spartanport_actors.h"
#include "spartanport_graphics.h"
#include "spartanport_perf_stats.h"
#include "spartanport_trophy_rarity.h"
#include "spartanport_trophy_texts.h"
#include "spartanport_trophy_toast.h"
#include "../../src/actor_hook.h"

extern SpartanActorSlot spartanport_actor_slots[2];
extern "C" void runtime_restart(void); // src/probe.c: starts run.py again, ends this process
#include "common/elf_info.h"
#include "imgui.h"
#include "imgui_internal.h" // RegisterUserTexture (save pictures)
#include "../spartanportgpu.h"       // spartanportgpu_toast_trophy, spartanportgpu_trophy_setup
#include "imgui_impl_vulkan.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_frame_capture.h"
#include "video_core/renderdoc.h"

// DejaVu Sans (Cyrillic), embedded (third_party/fonts, Bitstream Vera license), and the launcher's
// title face, Cinzel Black (launcher/Cinzel-Black.ttf, SIL OFL 1.1, launcher/OFL-Cinzel.txt), for the
// headings. The offscreen preview (tools/overlay_preview.cpp) compiles this file again and links the
// library's copies.
#ifdef SPARTAN_OVERLAY_PREVIEW
#elif defined(_WIN32)
asm(".section .rdata,\"dr\"\n"
    ".balign 16\n"
    ".global spartanport_font_ttf\n"
    "spartanport_font_ttf:\n"
    ".incbin \"" SPARTAN_FONT_PATH "\"\n"
    ".global spartanport_font_ttf_end\n"
    "spartanport_font_ttf_end:\n"
    ".balign 16\n"
    ".global spartanport_title_ttf\n"
    "spartanport_title_ttf:\n"
    ".incbin \"" SPARTAN_TITLE_FONT_PATH "\"\n"
    ".global spartanport_title_ttf_end\n"
    "spartanport_title_ttf_end:\n"
    ".text\n");
#else
asm(".section .rodata\n"
    ".balign 16\n"
    ".hidden spartanport_font_ttf\n"
    ".global spartanport_font_ttf\n"
    "spartanport_font_ttf:\n"
    ".incbin \"" SPARTAN_FONT_PATH "\"\n"
    ".hidden spartanport_font_ttf_end\n"
    ".global spartanport_font_ttf_end\n"
    "spartanport_font_ttf_end:\n"
    ".balign 16\n"
    ".hidden spartanport_title_ttf\n"
    ".global spartanport_title_ttf\n"
    "spartanport_title_ttf:\n"
    ".incbin \"" SPARTAN_TITLE_FONT_PATH "\"\n"
    ".hidden spartanport_title_ttf_end\n"
    ".global spartanport_title_ttf_end\n"
    "spartanport_title_ttf_end:\n"
    ".previous\n");
#endif
extern "C" const unsigned char spartanport_font_ttf[];
extern "C" const unsigned char spartanport_font_ttf_end[];
extern "C" const unsigned char spartanport_title_ttf[];
extern "C" const unsigned char spartanport_title_ttf_end[];

namespace SpartanOverlay {

namespace {
using SpartanUi::Tr;

std::mutex imgui_mutex; // the ImGui context: window thread (input) and present thread
bool initialized = false;

// The launcher's colours (launcher/spartanport_launcher_win.py: BG, PANEL, CARD, LINE, TEXT, MUTED,
// ACCENT, ACCENT_HI), for the menu and the trophy window; gold marks trophies and section titles.
namespace Ink {
constexpr ImVec4 Rgb(int r, int g, int b, float a = 1.0f) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a); }
constexpr ImVec4 Bg = Rgb(0x0d, 0x0b, 0x0b), Panel = Rgb(0x16, 0x12, 0x12), Card = Rgb(0x1d, 0x17, 0x17);
constexpr ImVec4 Line = Rgb(0x2a, 0x22, 0x22), Raised = Rgb(0x34, 0x2a, 0x2a);
constexpr ImVec4 Text = Rgb(0xe6, 0xe0, 0xd8), Muted = Rgb(0x8a, 0x81, 0x7a);
constexpr ImVec4 Accent = Rgb(0xa4, 0x16, 0x1a), AccentHi = Rgb(0xc8, 0x1e, 0x25), AccentDeep = Rgb(0x3a, 0x14, 0x16);
constexpr ImVec4 Gold = Rgb(0xd4, 0xa8, 0x5a);
// The grades, as the launcher's Trophies page colours them (TROPHY_TIERS).
constexpr ImVec4 Bronze = Rgb(0xcd, 0x7f, 0x32), Silver = Rgb(0xc4, 0xc4, 0xc8), GoldGrade = Rgb(0xe0, 0xb4, 0x3a);
constexpr ImVec4 Platinum = Rgb(0x8f, 0xd3, 0xe8);
inline ImVec4 Grade(SpartanToast::Grade grade) {
    switch (grade) {
    case SpartanToast::Grade::Bronze: return Bronze;
    case SpartanToast::Grade::Silver: return Silver;
    case SpartanToast::Grade::Gold: return GoldGrade;
    case SpartanToast::Grade::Platinum: return Platinum;
    default: return Gold;
    }
}
} // namespace Ink
ImFont* title_font = nullptr; // Cinzel with DejaVu Sans for what it lacks; null: the menu's font
constexpr float kTitleSize = 32.0f, kTabSize = 22.0f, kSectionSize = 19.0f; // px at 1080p
std::atomic<bool> menu_open{false};
bool r3_down = false, l2_down = false, r2_down = false;
std::atomic<bool> loading{false};
std::atomic<u32> loading_done{0}, loading_total{0};
std::atomic<u32> background_done{0}, background_total{0};
std::atomic<u32> prefetch_done{0}, prefetch_total{0};
std::atomic<s64> prefetch_until{0}; // "ready" stays this long after the last program
// Shader translations: in progress, and started since the note last appeared.
std::atomic<u32> compile_pending{0}, compile_count{0};
std::atomic<s64> compile_until{0};
// A short message in a corner (saved screenshot), shown until notice_until.
std::mutex notice_mutex;
std::string notice_text;
std::atomic<s64> notice_until{0}; // steady_clock milliseconds
bool dirty = false; // settings changed while open: saved on close
int requested_tab = -1; // switch on the next frame (L1/R1, Q/E, menu opened)
// The game's text dialog (SetTextEntry), guarded by imgui_mutex.
bool text_entry_active = false;
std::string text_entry_prompt, text_entry_text;
// The game's save list (sceSaveDataDialog): a choice made with the arrows/D-pad.
std::atomic<bool> choice_active{false};
std::string choice_title;
std::string choice_preview_label;
std::vector<std::string> choice_items;
int choice_index = 0;
std::atomic<int> choice_result{-1}; // -1 open, -2 cancelled, else the chosen index
// Save pictures: decoded by BeginChoice, made textures on the present thread (null = none).
// Closed lists' textures wait in choice_retired until the backend has destroyed them.
std::vector<SpartanPng::Image> choice_pictures;
std::vector<ImTextureData*> choice_textures, choice_retired;
std::atomic<bool> choice_retiring{false};
float base_scale = 1.0f;

// Present rate for the FPS counter.
std::chrono::steady_clock::time_point last_present{};
float frame_ms_avg = 0.0f;

void SetOpen(bool value) {
    if (menu_open.exchange(value) == value) {
        return;
    }
    ImGui::GetIO().MouseDrawCursor = value;
    if (value) {
        requested_tab = SpartanSettings::Get().menu_tab;
    }
    if (!value && dirty) {
        dirty = false;
        SpartanSettings::Save();
    }
}

ImGuiKey KeyFromSdl(SDL_Keycode key) {
    switch (key) {
    case SDLK_TAB: return ImGuiKey_Tab;
    case SDLK_LEFT: return ImGuiKey_LeftArrow;
    case SDLK_RIGHT: return ImGuiKey_RightArrow;
    case SDLK_UP: return ImGuiKey_UpArrow;
    case SDLK_DOWN: return ImGuiKey_DownArrow;
    case SDLK_PAGEUP: return ImGuiKey_PageUp;
    case SDLK_PAGEDOWN: return ImGuiKey_PageDown;
    case SDLK_HOME: return ImGuiKey_Home;
    case SDLK_END: return ImGuiKey_End;
    case SDLK_DELETE: return ImGuiKey_Delete;
    case SDLK_BACKSPACE: return ImGuiKey_Backspace;
    case SDLK_SPACE: return ImGuiKey_Space;
    case SDLK_RETURN: return ImGuiKey_Enter;
    case SDLK_KP_ENTER: return ImGuiKey_KeypadEnter;
    case SDLK_ESCAPE: return ImGuiKey_Escape;
    case SDLK_Q: return ImGuiKey_Q;
    case SDLK_E: return ImGuiKey_E;
    case SDLK_LCTRL: return ImGuiKey_LeftCtrl;
    case SDLK_RCTRL: return ImGuiKey_RightCtrl;
    case SDLK_LSHIFT: return ImGuiKey_LeftShift;
    case SDLK_RSHIFT: return ImGuiKey_RightShift;
    case SDLK_LALT: return ImGuiKey_LeftAlt;
    case SDLK_RALT: return ImGuiKey_RightAlt;
    default: return ImGuiKey_None;
    }
}

ImGuiKey KeyFromGamepad(u8 button) {
    const bool swapped = SpartanSettings::Get().menu_confirm_swap.load(std::memory_order_relaxed) != 0;
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return swapped ? ImGuiKey_GamepadFaceRight : ImGuiKey_GamepadFaceDown;
    case SDL_GAMEPAD_BUTTON_EAST: return swapped ? ImGuiKey_GamepadFaceDown : ImGuiKey_GamepadFaceRight;
    case SDL_GAMEPAD_BUTTON_WEST: return ImGuiKey_GamepadFaceLeft;
    case SDL_GAMEPAD_BUTTON_NORTH: return ImGuiKey_GamepadFaceUp;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return ImGuiKey_GamepadDpadUp;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return ImGuiKey_GamepadDpadDown;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return ImGuiKey_GamepadDpadLeft;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return ImGuiKey_GamepadDpadRight;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return ImGuiKey_GamepadL1;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return ImGuiKey_GamepadR1;
    case SDL_GAMEPAD_BUTTON_START: return ImGuiKey_GamepadStart;
    case SDL_GAMEPAD_BUTTON_BACK: return ImGuiKey_GamepadBack;
    default: return ImGuiKey_None;
    }
}

void UiText(const char* text) { ImGui::TextUnformatted(Tr(text)); }
void DisabledText(const char* text) { ImGui::TextDisabled("%s", Tr(text)); }
// A section title: gold, in the title face, over a thin line.
void Separator(const char* text) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, Ink::Gold);
    if (title_font) ImGui::PushFont(title_font, kSectionSize);
    ImGui::SeparatorText(Tr(text));
    if (title_font) ImGui::PopFont();
    ImGui::PopStyleColor();
}
bool Button(const char* text) { return ImGui::Button(Tr(text)); }
// The red rule under a window's heading: bright on the left, fading out on the right.
void HeadingRule() {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float px = base_scale;
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    draw->AddRectFilledMultiColor(ImVec2(at.x, at.y + 2.0f * px), ImVec2(at.x + width * 0.6f, at.y + 4.0f * px),
                                  ImGui::GetColorU32(Ink::AccentHi), ImGui::GetColorU32(Ink::Accent),
                                  ImGui::GetColorU32(Ink::Accent), ImGui::GetColorU32(Ink::AccentHi));
    draw->AddRectFilledMultiColor(ImVec2(at.x + width * 0.6f, at.y + 2.0f * px), ImVec2(at.x + width, at.y + 4.0f * px),
                                  ImGui::GetColorU32(Ink::Accent), ImGui::GetColorU32(ImVec4(0, 0, 0, 0)),
                                  ImGui::GetColorU32(ImVec4(0, 0, 0, 0)), ImGui::GetColorU32(Ink::Accent));
    ImGui::Dummy(ImVec2(width, 14.0f * px));
}

float PixelDensity(SDL_WindowID id) {
    SDL_Window* window = SDL_GetWindowFromID(id);
    const float density = window ? SDL_GetWindowPixelDensity(window) : 1.0f;
    return density > 0.0f ? density : 1.0f;
}

// Marks the settings dirty when a widget changed them.
template <typename T>
void Store(std::atomic<T>& target, T value, bool changed) {
    if (changed) {
        target = value;
        dirty = true;
    }
}

void Checkbox(const char* label, std::atomic<bool>& value) {
    bool v = value;
    const bool changed = ImGui::Checkbox(Tr(label), &v);
    Store(value, v, changed);
}

// A combo over `count` labels storing an index or a value from `values`.
void Choice(const char* label, std::atomic<int>& target, const char* const* labels, const int* values,
            int count) {
    int current = 0;
    for (int i = 0; i < count; ++i) {
        if ((values ? values[i] : i) == target.load()) current = i;
    }
    std::vector<const char*> translated;
    translated.reserve(count);
    for (int i = 0; i < count; ++i) translated.push_back(Tr(labels[i]));
    if (ImGui::Combo(label, &current, translated.data(), count)) {
        Store(target, values ? values[current] : current, true);
    }
}

// Controller button names in the help text follow SPARTAN_GLYPHS (run.py sets xbox or nintendo when
// the glyph feature is on); unset is PlayStation. Read once: the environment is fixed for a run.
static const char* PadText(const char* playstation, const char* xbox, const char* nintendo) {
    static const char* style = std::getenv("SPARTAN_GLYPHS");
    if (!style) return playstation;
    return !std::strcmp(style, "xbox") ? xbox : !std::strcmp(style, "nintendo") ? nintendo : playstation;
}

// The note for the item under the mouse or the gamepad/keyboard focus, shown in the footer.
const char* footer_help = nullptr;

void Help(const char* text) {
    if (text && (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) || ImGui::IsItemFocused())) {
        footer_help = Tr(text);
    }
}

// Two-column rows: the name on the left, the control on the right.
bool BeginRows(const char* id) {
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp)) {
        return false;
    }
    ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch, 0.52f);
    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 0.48f);
    return true;
}

void Row(const char* label) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    UiText(label);
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::PushID(label);
}

void CheckRow(const char* label, std::atomic<bool>& value, const char* help = nullptr) {
    Row(label);
    Checkbox("##value", value);
    Help(help);
    ImGui::PopID();
}

void ChoiceRow(const char* label, std::atomic<int>& target, const char* const* labels, const int* values,
               int count, const char* help = nullptr) {
    Row(label);
    Choice("##value", target, labels, values, count);
    Help(help);
    ImGui::PopID();
}

// A 0.1x to 100x multiplier with a 1x button.
void MultiplierRow(const char* label, std::atomic<float>& value, const char* help) {
    Row(label);
    const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::SetNextItemWidth(-(ImGui::CalcTextSize("1x").x + style.FramePadding.x * 2 + style.ItemSpacing.x));
    float v = value;
    if (ImGui::SliderFloat("##value", &v, 0.1f, 100.0f, "%.2fx",
                           ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp)) {
        value = SpartanOrbs::ClampMultiplier(v);
        dirty = true;
    }
    Help(help);
    ImGui::SameLine();
    if (ImGui::SmallButton("1x")) {
        value = 1.0f;
        dirty = true;
    }
    Help(help);
    ImGui::PopID();
}

void InfoRow(const char* label, const char* text) {
    Row(label);
    DisabledText(text);
    ImGui::PopID();
}

void SliderRow(const char* label, std::atomic<int>& value, int lo, int hi, const char* format, const char* help) {
    Row(label);
    int v = value;
    if (ImGui::SliderInt("##value", &v, lo, hi, format, ImGuiSliderFlags_AlwaysClamp)) {
        Store(value, std::clamp(v, lo, hi), true);
    }
    Help(help);
    ImGui::PopID();
}

const char* UpscalerLabel(int upscaler) {
    switch (upscaler) {
    case SpartanSettings::UpscalerFsr3: return "FSR 3.1";
    case SpartanSettings::UpscalerFsr4: return "FSR 4";
    case SpartanSettings::UpscalerFsr411: return "FSR 4.1.1";
    case SpartanSettings::UpscalerTaa: return "TAA";
    case SpartanSettings::UpscalerDlss: return "DLSS";
    default: return "";
    }
}

const Vulkan::Instance* overlay_instance = nullptr; // for the overlay's measurements

// The performance overlay's text with the current settings (also the menu's preview).
std::string OverlayText() {
    namespace P = SpartanPerfStats;
    const auto& s = SpartanSettings::Get();
    const unsigned items = s.overlay_items;
    if ((items & (P::Gpu | P::Cpu | P::Ram | P::Vram)) && overlay_instance) {
        P::Start(*overlay_instance);
    }
    P::Sample sample = P::Latest();
    sample.fps = frame_ms_avg > 0.0f ? 1000.0f / frame_ms_avg : 0.0f;
    sample.frame_ms = frame_ms_avg;
    sample.upscaler = UpscalerLabel(s.upscaler);
    return P::Format(sample, items, s.overlay_layout == 0);
}

constexpr const char* Unsupported = "Unavailable: game version or code signature mismatch.";

void LanguageRow(); // with the trophy texts, below

void GameTab() {
    auto& s = SpartanSettings::Get();
    if (BeginRows("##language")) {
        LanguageRow();
        CheckRow("Pause the game", s.pause_game, "The game stops while this menu or the trophy list is open.");
        ImGui::EndTable();
    }
    Separator("Cheats");
    if (BeginRows("##cheats")) {
        static const char* const names[] = {"Infinite health", "Infinite magic", "Infinite item meter",
                                            "Infinite Rage of Sparta", "Max red orbs",
                                            "Unlock Very Hard", "Infinite Double Jumps"};
        static const char* const helps[] = {
            "Health stays full. Only Kratos is protected. Turning it off does not undo past refills.",
            "Magic stays full.", "The item meter (bow, Helios' head, boots) stays full.",
            "Rage of Sparta stays full.", "Red orbs stay at the maximum.",
            "Enable before opening New Game. Does not change an existing save difficulty.",
            "Repeat air jumps without landing. Turn off for normal movement."};
        for (unsigned cheat = 0; cheat < SPARTAN_CHEAT_COUNT; ++cheat) {
            const bool available = (s.cheats_supported.load() & (1u << cheat)) != 0;
            ImGui::BeginDisabled(!available);
            CheckRow(names[cheat], s.cheats[cheat], available ? helps[cheat] : Unsupported);
            ImGui::EndDisabled();
        }
        ImGui::BeginDisabled(!s.qte_supported.load());
        CheckRow("Automatic QTE", s.auto_qte,
                 s.qte_supported.load() ? "Quick-time prompts answer themselves: single presses, repeated "
                                          "presses, holds and stick rotations."
                                        : Unsupported);
        ImGui::EndDisabled();
        ImGui::EndTable();
    }

    Separator("Controller");
    if (BeginRows("##controller")) {
        SliderRow("Vibration", s.vibration, 0, 100, "%d%%",
                  "Strength of the controller's vibration. 0% turns it off.");
        static const char* const confirm_styles[] = {"Cross confirms, Circle cancels", "Circle confirms, Cross cancels"};
        ChoiceRow("Menu confirm button", s.menu_confirm_swap, confirm_styles, nullptr, 2,
                  "Which controller button confirms choices in save/load lists and confirmation dialogs.");
        SliderRow("Mouse prompt distance", s.mouse_prompt_distance, 16, 200, "%d px",
                  "With Joystick prompts on Adaptive mouse: how far the mouse moves for one step of a rotation.");
        static const char* const aim_styles[] = {"Relative", "Absolute"};
        ChoiceRow("Right stick style", s.mouse_aim_style, aim_styles, nullptr, 2,
                  "L2 aim with the mouse (Head of Helios, Bow of Apollo). Relative: the stick follows the "
                  "movement and goes back to neutral when the mouse stops. Absolute: the stick points from "
                  "where the mouse was when L2 was pressed to where it is.");
        SliderRow("Right stick sensitivity", s.mouse_aim_sensitivity, 10, 200, "%d%%",
                  "L2 aim with the mouse: lower needs more mouse movement for the same stick, higher less.");
        CheckRow("Keep mouse in window", s.mouse_lock,
                 "While you play, the mouse stays in the game window; this menu and other windows get it back.");
        ImGui::EndTable();
    }

    Separator("Audio");
    if (BeginRows("##audio")) {
        SliderRow("Volume", s.master_volume, SpartanSettings::MasterVolumeMin, SpartanSettings::MasterVolumeMax, "%d%%",
                  "Master volume of the game's audio. 100% is the game's own mix; more is louder, with a soft "
                  "limiter so loud scenes do not clip.");
        static const char* const speakers[] = {"Automatic", "Stereo", "5.1 surround", "7.1 surround"};
        ChoiceRow("Speakers", s.audio_channels, speakers, nullptr, SpartanSettings::AudioChannelsCount,
                  "Layout the game's 7.1 mix is sent in. Automatic follows your sound device: stereo, 5.1 or 7.1. "
                  "Applies from the next start.");
        ImGui::EndTable();
    }

    Separator("Multipliers");
    if (Button("Reset all to 1x")) {
        for (auto* m : {&s.red_orb_multiplier, &s.green_orb_multiplier, &s.blue_orb_multiplier,
                        &s.gold_orb_multiplier, &s.damage_dealt, &s.damage_taken}) {
            *m = 1.0f;
        }
        dirty = true;
    }
    Help("Every multiplier back to 1x (the game's normal values).");
    if (BeginRows("##multipliers")) {
        const bool max_orbs = (s.cheats_supported.load() & (1u << 4)) && s.cheats[4].load();
        ImGui::BeginDisabled(!s.red_orbs_supported.load() || max_orbs);
        MultiplierRow("Red orbs", s.red_orb_multiplier,
                      !s.red_orbs_supported.load() ? Unsupported
                      : max_orbs ? "Turn off Max red orbs to use the multiplier."
                                 : "Red orbs gained. Orbs you have and prices stay the same.");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!s.orb_pickup_supported.load());
        const char* pickup = s.orb_pickup_supported.load() ? nullptr : Unsupported;
        MultiplierRow("Green orbs (health)", s.green_orb_multiplier,
                      pickup ? pickup : "Health from each green orb. The bar stays within its maximum.");
        MultiplierRow("Blue orbs (magic)", s.blue_orb_multiplier,
                      pickup ? pickup : "Magic from each blue orb. The bar stays within its maximum.");
        MultiplierRow("Gold orbs (Rage of Sparta)", s.gold_orb_multiplier,
                      pickup ? pickup : "Rage of Sparta from each gold orb. The bar stays within its maximum.");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!s.damage_supported.load());
        const char* damage = s.damage_supported.load() ? nullptr : Unsupported;
        MultiplierRow("Damage dealt", s.damage_dealt, damage ? damage : "Damage Kratos does to enemies.");
        MultiplierRow("Damage taken", s.damage_taken, damage ? damage : "Damage Kratos takes.");
        ImGui::EndDisabled();
        ImGui::EndTable();
    }

    Separator("HUD");
    if (BeginRows("##hud")) {
        ImGui::BeginDisabled(!s.actor_watch_supported.load());
        CheckRow("Enemy health bar", s.enemy_health_bar,
                 s.actor_watch_supported.load() ? "A bar at the top for the last enemy hit." : Unsupported);
        ImGui::EndDisabled();
        ImGui::EndTable();
    }
}

void ImageTab() {
    namespace G = SpartanGraphics;
    auto& s = SpartanSettings::Get();
    Separator("Display");
    if (BeginRows("##display")) {
        ChoiceRow("Display mode", s.display_mode, G::DisplayModeNames.data(), nullptr, G::DisplayModeCount,
                  "Windowed, borderless window or exclusive fullscreen.");
        CheckRow("VSync", s.vsync, "Waits for the monitor: no tearing, a little more input delay.");
        ImGui::EndTable();
    }
    const int iw = s.image_width, ih = s.image_height, ww = s.window_width, wh = s.window_height;
    const bool downscaling = iw > ww || ih > wh;
    const bool rcas_blocked = s.rcas && !s.rcas_applied;
    Separator("Sharpening");
    if (BeginRows("##scaling")) {
        ImGui::BeginDisabled(rcas_blocked);
        CheckRow("Sharpening (RCAS)", s.rcas,
                 rcas_blocked && downscaling ? "No effect: the game's image is larger than the window."
                 : rcas_blocked              ? "No effect: the game's image is smaller than the window."
                                             : "Sharpens the final image.");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!s.rcas || rcas_blocked);
        SliderRow("Sharpening strength", s.rcas_strength, 0, 100, "%d%%", "How strong the sharpening is.");
        ImGui::EndDisabled();
        ImGui::EndTable();
    }
    Separator("Effects");
    if (BeginRows("##effects")) {
        CheckRow("Depth of field", s.depth_of_field,
                 "The game's own blur of distant objects, strongest in cutscenes. Off keeps the whole picture sharp.");
        ImGui::EndTable();
    }
    Separator("Textures (applied on restart)");
    if (BeginRows("##textures")) {
        static const char* const anisotropy_labels[] = {"Game default", "2x", "4x", "8x", "16x"};
        ChoiceRow("Anisotropic filtering", s.anisotropy, anisotropy_labels, G::Anisotropies.data(),
                  int(G::Anisotropies.size()),
                  "Sharper floors and walls seen at an angle. Little cost on most GPUs.");
        ImGui::EndTable();
    }
    DisabledText("F12 saves a screenshot to the user\\screenshots folder.");
    Separator("Information");
    if (BeginRows("##image_info")) {
        char text[96];
        std::snprintf(text, sizeof(text), "%d x %d", iw, ih);
        InfoRow("Game image", text);
        std::snprintf(text, sizeof(text), "%d x %d", ww, wh);
        InfoRow("Window", text);
        InfoRow("Sharpening", s.rcas_applied ? "applied" : "not applied");
        ImGui::EndTable();
    }
}

void PerformanceTab() {
    namespace G = SpartanGraphics;
    auto& s = SpartanSettings::Get();
    Separator("Live");
    if (BeginRows("##live")) {
        static const char* const fps_labels[] = {"30", "60", "100", "120", "144", "180", "240", "Unlimited"};
        ChoiceRow("Frame rate limit", s.fps_limit, fps_labels, G::FpsLimits.data(), int(G::FpsLimits.size()),
                  "Caps the frame rate. The engine frame rate (below) is the highest it can go.");
        static const char* const engine_labels[] = {"60 (original)", "100", "120", "144", "180", "240"};
        static_assert(std::size(engine_labels) == G::EngineFps.size());
        ChoiceRow("Engine frame rate", s.engine_fps, engine_labels, G::EngineFps.data(), int(G::EngineFps.size()),
                  "The highest frame rate the game itself runs at. 60 for spots that break above it (a ledge "
                  "Kratos cannot cross, a button-mash prompt). R3 + L1 + R1 or F9 go through the list.");
        const int effective = G::EffectiveFps(s.fps_limit, SpartanSettings::EngineFpsNow());
        if (s.fps_limit == 0 || effective < s.fps_limit) {
            char text[48];
            std::snprintf(text, sizeof(text), Tr("%d FPS (engine frame rate)"), effective);
            InfoRow("Effective limit", text);
        }
        if (s.applied_engine_fps <= 60 && s.engine_fps > 60) {
            InfoRow("Engine frame rate", "60 FPS: the frame rate patch is not available for this game build");
        }
        static const char* const queued_labels[] = {"1 (lowest latency)", "2", "Unlimited"};
        ChoiceRow("Frames queued", s.frames_queued, queued_labels, G::FramesQueued.data(),
                  int(G::FramesQueued.size()), "More frames queued: steadier FPS, slower response to input.");
        CheckRow("Compile new shaders in the background", s.async_shaders,
                 "Fewer stutters in new areas; an object or effect may appear a moment later.");
        ImGui::EndTable();
    }
    Separator("Applied on restart");
    if (BeginRows("##restart")) {
        // The built-in sizes, then those of the enabled mods' patch files (ultrawide, for one).
        // The same labels as the launcher (graphics_options.resolution_label): name, exact size, aspect ratio.
        static const char* const builtin_labels[] = {
            "Native · 1920 × 1080 · 16:9", "480p · 854 × 480 · 16:9",
            "720p · 1280 × 720 · 16:9", "900p · 1600 × 900 · 16:9",
            "1440p · 2560 × 1440 · 16:9",
            "1800p · 3200 × 1800 · 16:9", "4K · 3840 × 2160 · 16:9",
            "2560 × 1080 · 21:9 (beta)", "3440 × 1440 · 21:9",
            "3840 × 1600 · 21:9", "5120 × 1440 · 32:9",
            "1920 × 1200 · 16:10", "2560 × 1600 · 16:10",
            "1280 × 800 · 16:10", "1440 × 900 · 16:10",
            "1360 × 768 · 16:9",
            "1280 × 960 · 4:3", "1440 × 1080 · 4:3 (beta)", "1920 × 1440 · 4:3",
            "2048 × 1536 · 4:3 (beta)", "2560 × 1920 · 4:3 (beta)", "5120 × 2160 · 21:9",
            "8K · 7680 × 4320 · 16:9 (beta)"};
        static_assert(std::size(builtin_labels) == G::Resolutions.size());
        const std::vector<std::string> resolution_names = G::ResolutionNames();
        std::vector<const char*> resolution_labels;
        for (std::size_t i = 0; i < resolution_names.size(); ++i) {
            resolution_labels.push_back(i < G::Resolutions.size() ? builtin_labels[i] : resolution_names[i].c_str());
        }
        ChoiceRow("Render resolution", s.render_resolution, resolution_labels.data(), nullptr,
                  int(resolution_labels.size()), "The resolution the game draws at. Higher is sharper and slower.");
        CheckRow("Game reads GPU data without waiting", s.stale_readback,
                 "Big FPS gain. Switch it off if lighting or objects look wrong.");
        CheckRow("Deferred GPU readbacks", s.deferred_readback,
                 "Fewer waits for the GPU. Switch it off if textures or shadows look wrong.");
        ImGui::EndTable();
    }
}

void OverlayTab() {
    namespace P = SpartanPerfStats;
    auto& s = SpartanSettings::Get();
    if (BeginRows("##overlay_on")) {
        CheckRow("Show overlay", s.show_fps, "The performance overlay in a corner of the screen.");
        ImGui::EndTable();
    }
    ImGui::BeginDisabled(!s.show_fps);
    Separator("What to show");
    if (ImGui::BeginTable("##overlay_items", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableNextColumn();
        static const char* const names[] = {"FPS", "Frametime", "Upscaler", "GPU (name and use)", "CPU (game)",
                                            "RAM (game / total)", "VRAM (used / available)"};
        for (unsigned item = 0; item < 7; ++item) {
            const unsigned bit = 1u << item;
            bool on = (s.overlay_items & bit) != 0;
            if (ImGui::Checkbox(Tr(names[item]), &on)) {
                s.overlay_items = on ? (s.overlay_items | bit) : (s.overlay_items & ~bit);
                dirty = true;
            }
        }
        ImGui::TableNextColumn();
        DisabledText("Preview");
        ImGui::BeginChild("##preview", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
        ImGui::SetWindowFontScale(s.overlay_scale / 100.0f);
        const std::string text = OverlayText();
        UiText(text.empty() ? "(nothing selected)" : text.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::EndChild();
        ImGui::EndTable();
    }
    Separator("Appearance");
    if (BeginRows("##appearance")) {
        SliderRow("Font size", s.overlay_scale, 50, 300, "%d%%", "Size of the overlay text.");
        static const char* const corners[] = {"Top left", "Top right", "Bottom left", "Bottom right"};
        ChoiceRow("Corner", s.overlay_corner, corners, nullptr, 4, "Where the overlay sits.");
        SliderRow("Background opacity", s.overlay_opacity, 0, 100, "%d%%", "0% is no background.");
        static const char* const layouts[] = {"One line", "One item per line"};
        ChoiceRow("Layout", s.overlay_layout, layouts, nullptr, 2, "Items side by side or stacked.");
        ImGui::EndTable();
    }
    ImGui::EndDisabled();
    Separator("About");
    if (BeginRows("##about")) {
        const auto& elf = Common::ElfInfo::Instance();
        char text[160];
        std::snprintf(text, sizeof(text), "%.*s v%.*s", int(elf.GameSerial().size()), elf.GameSerial().data(),
                      int(elf.AppVer().size()), elf.AppVer().data());
        InfoRow("Game", text);
        const u32 total = background_total;
        if (total) {
            std::snprintf(text, sizeof(text), Tr("loading %u%%"), u32(u64(background_done) * 100 / total));
        } else {
            std::snprintf(text, sizeof(text), "%s", Tr("ready"));
        }
        InfoRow("Shader cache", text);
        ImGui::EndTable();
    }
    // What the game's code lets the port reach in this game version (a version or region the hooks
    // do not recognise has fewer): not whether a feature is on, which each tab sets.
    Separator("Supported features");
    ImGui::PushTextWrapPos(0.0f);
    DisabledText("What this game version can use. Each one is turned on or off in its own tab.");
    ImGui::PopTextWrapPos();
    if (BeginRows("##features")) {
        const std::pair<const char*, bool> features[] = {
            {"Cheats", s.cheats_supported.load() != 0}, {"Red orbs", s.red_orbs_supported.load()},
            {"Orb pickups", s.orb_pickup_supported.load()}, {"Damage", s.damage_supported.load()},
            {"Enemy health bar", s.actor_watch_supported.load()}, {"Automatic QTE", s.qte_supported.load()},
        };
        for (const auto& [name, on] : features) {
            Row(name);
            ImGui::TextColored(on ? Ink::Gold : Ink::Muted, "%s  %s", on ? "\xE2\x9C\x93" : "\xE2\x80\x93",
                               Tr(on ? "supported" : "not supported"));
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

// Startup options changed since this launch (applied by "Apply and restart").
int RestartChanges() {
    const auto& s = SpartanSettings::Get();
    return (s.render_resolution != s.startup_render_resolution) +
           (s.monitor != s.startup_monitor) + (s.hdr != s.startup_hdr) +
           (s.deferred_readback != s.startup_deferred_readback) + (s.stale_readback != s.startup_stale_readback) +
           (s.anisotropy != s.startup_anisotropy);
}

// The menu's tabs: words in the title face, the open one in full colour with a red line under it,
// L1 / R1 (LB / RB, L / R) at both ends. Returns the tab clicked, or -1.
int MenuTabs(const char* const* tabs, int count, int open) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float s = base_scale, gap = 26.0f * s;
    const char* left = PadText("L1", "LB", "L");
    const char* right = PadText("R1", "RB", "R");
    auto badge = [&](const char* text) {
        const ImVec2 size = ImGui::CalcTextSize(text);
        const ImVec2 at = ImGui::GetCursorScreenPos();
        const ImVec2 box(size.x + 12.0f * s, size.y + 4.0f * s);
        draw->AddRect(at, ImVec2(at.x + box.x, at.y + box.y), ImGui::GetColorU32(Ink::Line), 4.0f * s, 0, 1.0f * s);
        draw->AddText(ImVec2(at.x + 6.0f * s, at.y + 2.0f * s), ImGui::GetColorU32(Ink::Muted), text);
        ImGui::Dummy(box);
    };
    int clicked = -1;
    const float row_top = ImGui::GetCursorScreenPos().y;
    badge(left);
    for (int tab = 0; tab < count; ++tab) {
        ImGui::SameLine(0.0f, tab == 0 ? 18.0f * s : gap);
        if (title_font) ImGui::PushFont(title_font, kTabSize);
        const char* label = Tr(tabs[tab]);
        const ImVec2 size = ImGui::CalcTextSize(label);
        const ImVec2 at = ImGui::GetCursorScreenPos();
        ImGui::PushID(tab);
        if (ImGui::InvisibleButton("##tab", ImVec2(size.x, size.y + 8.0f * s))) {
            clicked = tab;
        }
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        const ImVec4 ink = tab == open ? Ink::Text : hovered ? Ink::Text : Ink::Muted;
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), at, ImGui::GetColorU32(ImVec4(ink.x, ink.y, ink.z, tab == open || hovered ? 1.0f : 0.85f)), label);
        if (tab == open) {
            draw->AddRectFilled(ImVec2(at.x, at.y + size.y + 4.0f * s), ImVec2(at.x + size.x, at.y + size.y + 7.0f * s),
                                ImGui::GetColorU32(Ink::AccentHi), 1.5f * s);
        }
        if (title_font) ImGui::PopFont();
    }
    ImGui::SameLine(0.0f, 18.0f * s);
    // R1 at the same height as L1
    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, row_top));
    badge(right);
    return clicked;
}

void Menu() {
    auto& s = SpartanSettings::Get();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float px = base_scale;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                   viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(820.0f * px, viewport->WorkSize.x * 0.95f),
                                    std::min(700.0f * px, viewport->WorkSize.y * 0.9f)),
                             ImGuiCond_Appearing);
    bool keep_open = true;
    if (!ImGui::Begin("##menu", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::End();
        return;
    }
    footer_help = nullptr;
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // The heading: the game's own title (param.sfo) in the title face, the keys that open the menu
    // and a close button on the right, a red rule under them.
    static const std::string title = [] {
        const std::string_view name = Common::ElfInfo::Instance().Title();
        return std::string(name.empty() ? "SpartanPort" : name);
    }();
    if (title_font) ImGui::PushFont(title_font, kTitleSize);
    ImGui::TextColored(Ink::Text, "%s", title.c_str());
    if (title_font) ImGui::PopFont();
    const char* keys = PadText("Insert / R3+L2", "Insert / RS+LT", "Insert / RS+ZL");
    const float close_size = ImGui::GetFrameHeight();
    const float keys_w = ImGui::CalcTextSize(keys).x;
    const float heading_h = ImGui::GetItemRectSize().y;
    const float right = ImGui::GetWindowContentRegionMax().x;
    ImGui::SameLine(right - keys_w - close_size - 14.0f * px);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (heading_h - ImGui::GetTextLineHeight()) * 0.5f);
    ImGui::TextColored(Ink::Muted, "%s", keys);
    ImGui::SameLine(right - close_size);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (heading_h - close_size) * 0.5f - (heading_h - ImGui::GetTextLineHeight()) * 0.5f);
    {
        const ImVec2 at = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##close", ImVec2(close_size, close_size))) {
            keep_open = false;
        }
        const ImU32 ink = ImGui::GetColorU32(ImGui::IsItemHovered() ? Ink::AccentHi : Ink::Muted);
        const float c = close_size * 0.5f, k = close_size * 0.22f;
        draw->AddLine(ImVec2(at.x + c - k, at.y + c - k), ImVec2(at.x + c + k, at.y + c + k), ink, 2.0f * px);
        draw->AddLine(ImVec2(at.x + c + k, at.y + c - k), ImVec2(at.x + c - k, at.y + c + k), ink, 2.0f * px);
    }
    HeadingRule();

    constexpr int TabCount = 4;
    const ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput) {
        const int step = ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false) || ImGui::IsKeyPressed(ImGuiKey_E, false) ? 1
                         : ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false) || ImGui::IsKeyPressed(ImGuiKey_Q, false)
                             ? -1
                             : 0;
        if (step) {
            requested_tab = (s.menu_tab + step + TabCount) % TabCount;
        }
    }
    static const char* const tabs[TabCount] = {"Game", "Image", "Performance", "Overlay"};
    if (requested_tab >= 0) {
        s.menu_tab = requested_tab;
        dirty = true;
        requested_tab = -1;
    }
    const int open = std::clamp(s.menu_tab.load(), 0, TabCount - 1);
    if (const int clicked = MenuTabs(tabs, TabCount, open); clicked >= 0 && clicked != open) {
        s.menu_tab = clicked;
        dirty = true;
    }
    ImGui::Dummy(ImVec2(0, 6.0f * px));

    // The open tab, on a panel; the footer keeps its room.
    const float footer = ImGui::GetFrameHeightWithSpacing() * 2.0f + ImGui::GetTextLineHeightWithSpacing() * 2.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * px, 12.0f * px));
    if (ImGui::BeginChild("##body", ImVec2(0, -footer), ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_Borders)) {
        switch (s.menu_tab.load()) {
        case 0: GameTab(); break;
        case 1: ImageTab(); break;
        case 2: PerformanceTab(); break;
        default: OverlayTab(); break;
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0, 2.0f * px));
    ImGui::PushTextWrapPos(0.0f);
    DisabledText(footer_help ? footer_help : PadText("L1 / R1 or Q / E: switch tabs. Settings are saved in spartanport.ini.", "LB / RB or Q / E: switch tabs. Settings are saved in spartanport.ini.", "L / R or Q / E: switch tabs. Settings are saved in spartanport.ini."));
    ImGui::PopTextWrapPos();
    if (const int changes = RestartChanges()) {
        ImGui::TextColored(Ink::Gold, Tr("%d change%s need%s a restart (unsaved progress is lost)."),
                           changes, changes == 1 ? "" : "s", changes == 1 ? "s" : "");
        ImGui::SameLine();
        ImGui::BeginDisabled(!std::getenv("SPARTAN_RESTART_COMMAND"));
        if (Button("Apply and restart")) {
            // The new launch confirms the options once the game shows; run.py restores the
            // previous ones if it never does (restart_unconfirmed).
            s.restart_unconfirmed = true;
            SpartanSettings::Save();
            runtime_restart();
        }
        ImGui::EndDisabled();
    } else {
        ImGui::NewLine();
    }
    const float close_width = ImGui::CalcTextSize(Tr("Close")).x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - close_width);
    ImGui::PushStyleColor(ImGuiCol_Button, Ink::Accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Ink::AccentHi);
    if (Button("Close")) {
        keep_open = false;
    }
    ImGui::PopStyleColor(2);
    ImGui::End();
    if (!keep_open) {
        SetOpen(false);
    }
}

// Enemy health bar from the damage hook's records (spartanport_actors.cpp), in the menu's look: a dark
// plate centered on the screen's top edge, the bar in the menu's red shrinking toward the center, the
// health just lost in gold draining after it, and the health in numbers below in the title face.
// Only the copied values are read, never the game's memory: an actor may be gone by now.
double health_clock_ms = -1.0; // a fixed clock (tools/overlay_preview.cpp); negative: the real one
void HealthDisplay() {
    using Clock = std::chrono::steady_clock;
    struct Seen {
        uint32_t seq = 0;
        Clock::time_point at{};
    };
    static Seen seen[2];
    // The gold part: from where the bar was before the last hit, down to the health now.
    static uint64_t actor = 0;
    static float trail_from = 0.0f, last_fraction = 0.0f;
    static Clock::time_point shown_at{};
    constexpr std::chrono::milliseconds kShown{4000}, kHold{350};
    constexpr float kDrainMs = 450.0f, kFadeInMs = 150.0f, kFadeOutMs = 500.0f;
    const auto& s = SpartanSettings::Get();
    if (!s.actor_watch_supported.load()) {
        return;
    }
    const auto now = health_clock_ms >= 0.0
                         ? Clock::time_point(std::chrono::microseconds(int64_t(health_clock_ms * 1000.0)))
                         : Clock::now();
    const auto ms = [](Clock::duration d) { return std::chrono::duration<float, std::milli>(d).count(); };
    const auto recent = [&](int i) { return seen[i].seq != 0 && now - seen[i].at < kShown; };
    SpartanActorSlot slot[2];
    for (int i = 0; i < 2; ++i) {
        std::memcpy(&slot[i], const_cast<const SpartanActorSlot*>(&spartanport_actor_slots[i]), sizeof(slot[i]));
        if (slot[i].seq == seen[i].seq) {
            continue;
        }
        if (i == 1) {
            const float fraction = SpartanActors::Fraction(slot[1].health, slot[1].max_health);
            const bool same = recent(1) && slot[1].actor == actor;
            if (!same) {
                shown_at = now; // a new enemy, or the bar had gone: no gold, and it fades in
                trail_from = fraction;
            } else {
                // Hit again while the gold drains: it goes on from where it is.
                const float drain = std::clamp((ms(now - seen[1].at - kHold)) / kDrainMs, 0.0f, 1.0f);
                trail_from = std::max(trail_from + (last_fraction - trail_from) * drain, fraction);
            }
            actor = slot[1].actor;
            last_fraction = fraction;
        }
        seen[i] = {slot[i].seq, now};
    }
    if (!s.enemy_health_bar || !recent(1) || !SpartanActors::Valid(slot[1].health, slot[1].max_health)) {
        return;
    }
    const float since = ms(now - seen[1].at);
    const float alpha = std::min(std::clamp(ms(now - shown_at) / kFadeInMs, 0.0f, 1.0f),
                                 std::clamp((ms(kShown) - since) / kFadeOutMs, 0.0f, 1.0f));
    const auto ink = [&](ImVec4 c, float a = 1.0f) { return ImGui::GetColorU32(ImVec4(c.x, c.y, c.z, c.w * a * alpha)); };
    const float fraction = SpartanActors::Fraction(slot[1].health, slot[1].max_health);
    const float drain = std::clamp((since - ms(kHold)) / kDrainMs, 0.0f, 1.0f);
    const float trail = std::max(trail_from + (fraction - trail_from) * drain, fraction);

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const float px = base_scale;
    const float center = ImGui::GetIO().DisplaySize.x * 0.5f;
    ImFont* font = title_font ? title_font : ImGui::GetFont();
    const float font_size = 16.0f * px;
    char text[48];
    std::snprintf(text, sizeof(text), "%.0f / %.0f", std::ceil(slot[1].health), slot[1].max_health);
    const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text);

    // The plate, with the red rule of the menu's headings on its top edge, bright in the middle.
    const float width = 360.0f * px, pad = 12.0f * px, bar_height = 8.0f * px, top = 28.0f * px;
    const float height = pad * 0.9f + bar_height + 5.0f * px + text_size.y + pad * 0.6f;
    const ImVec2 p0(center - width * 0.5f, top), p1(center + width * 0.5f, top + height);
    const float rounding = 6.0f * px;
    draw->AddRectFilled(p0, p1, ink(Ink::Bg, 0.82f), rounding);
    draw->AddRect(p0, p1, ink(Ink::Line), rounding, 0, 1.0f * px);
    const ImU32 clear = ink(Ink::Accent, 0.0f), red = ink(Ink::AccentHi);
    const float rule = top + 1.0f * px, half = width * 0.5f - rounding;
    draw->AddRectFilledMultiColor(ImVec2(center - half, rule), ImVec2(center, rule + 2.0f * px), clear, red, red, clear);
    draw->AddRectFilledMultiColor(ImVec2(center, rule), ImVec2(center + half, rule + 2.0f * px), red, clear, clear, red);

    // The bar: the track, the gold just lost, then the health left, all shrinking toward the center.
    const float bar_half = width * 0.5f - pad, bar_top = top + pad * 0.9f, bar_bottom = bar_top + bar_height;
    draw->AddRectFilled(ImVec2(center - bar_half, bar_top), ImVec2(center + bar_half, bar_bottom), ink(Ink::Panel), 2.0f * px);
    if (trail > fraction) {
        draw->AddRectFilled(ImVec2(center - bar_half * trail, bar_top), ImVec2(center + bar_half * trail, bar_bottom),
                            ink(Ink::Gold, 0.9f), 2.0f * px);
    }
    if (fraction > 0.0f) {
        const float x = std::max(bar_half * fraction, 1.0f * px);
        draw->AddRectFilledMultiColor(ImVec2(center - x, bar_top), ImVec2(center + x, bar_bottom), ink(Ink::AccentHi),
                                      ink(Ink::AccentHi), ink(Ink::Accent), ink(Ink::Accent));
        draw->AddLine(ImVec2(center - x, bar_top + 0.5f * px), ImVec2(center + x, bar_top + 0.5f * px),
                      ink(ImVec4(1, 1, 1, 1), 0.18f), 1.0f * px);
    }
    draw->AddRect(ImVec2(center - bar_half, bar_top), ImVec2(center + bar_half, bar_bottom), ink(Ink::Raised), 2.0f * px, 0,
                  1.0f * px);

    const ImVec2 at(center - text_size.x * 0.5f, bar_bottom + 5.0f * px);
    draw->AddText(font, font_size, ImVec2(at.x + px, at.y + px), ink(ImVec4(0, 0, 0, 1), 0.6f), text);
    draw->AddText(font, font_size, at, ink(Ink::Text), text);
}

// The game's text dialog: what is typed, and how to finish (keyboard or controller).
void TextEntryBox() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                   viewport->WorkPos.y + viewport->WorkSize.y * 0.42f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(viewport->WorkSize.x * 0.32f, 0.0f),
                                        ImVec2(viewport->WorkSize.x * 0.8f, FLT_MAX));
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::Begin("##text_entry", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::TextColored(ImVec4(0.85f, 0.72f, 0.45f, 1.0f), "%s", text_entry_prompt.c_str());
    ImGui::Separator();
    ImGui::SetWindowFontScale(1.4f);
    ImGui::Text("%s_", text_entry_text.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Separator();
    DisabledText("Keyboard: type, Backspace to erase, Enter = OK, Esc = cancel");
    const bool text_entry_swapped = SpartanSettings::Get().menu_confirm_swap.load(std::memory_order_relaxed) != 0;
    DisabledText(text_entry_swapped
        ? PadText("Controller: Circle (B) = OK, Cross (A) = cancel", "Controller: B = OK, A = cancel", "Controller: A = OK, B = cancel")
        : PadText("Controller: Cross (A) = OK, Circle (B) = cancel", "Controller: A = OK, B = cancel", "Controller: B = OK, A = cancel"));
    ImGui::End();
}

// One save (or one answer) of the game's save list as a card: its picture on the left when the list
// has pictures, then its lines: the first is the name, the second the place, the rest (date,
// difficulty, play time) in grey. `draw` false only measures. Returns the card's height.
float SaveCard(ImVec2 at, float width, const std::string& text, ImTextureData* picture, bool pictures, bool selected,
               bool draw) {
    const float px = base_scale;
    const float pad = 12.0f * px, gap = 16.0f * px;
    const ImVec2 picture_size(192.0f * px, 108.0f * px);
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize(), name_size = size * 1.15f;
    const float text_x = pad + (pictures ? picture_size.x + gap : 0.0f);
    const float wrap = std::max(width - text_x - pad, 40.0f * px);
    std::vector<std::string_view> lines;
    for (size_t start = 0; start <= text.size();) {
        const size_t end = std::min(text.find('\n', start), text.size());
        lines.emplace_back(text.data() + start, end - start);
        start = end + 1;
    }
    const auto line_gap = [&](size_t i) { return i == 0 && lines.size() > 1 ? 4.0f * px : 1.0f * px; };
    float text_h = 0.0f;
    for (size_t i = 0; i < lines.size(); ++i) {
        const char* begin = lines[i].data();
        text_h += font->CalcTextSizeA(i == 0 ? name_size : size, FLT_MAX, wrap, begin, begin + lines[i].size()).y +
                  line_gap(i);
    }
    const float height = std::max(pictures ? picture_size.y : 0.0f, text_h) + pad * 2.0f;
    if (!draw) {
        return height;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 max(at.x + width, at.y + height);
    const float rounding = 6.0f * px;
    dl->AddRectFilled(at, max, ImGui::GetColorU32(selected ? Ink::AccentDeep : Ink::Card), rounding);
    dl->AddRect(at, max, ImGui::GetColorU32(selected ? Ink::AccentHi : Ink::Line), rounding, 0,
                (selected ? 2.0f : 1.0f) * px);
    if (selected) {
        dl->AddRectFilled(ImVec2(at.x, at.y + rounding), ImVec2(at.x + 4.0f * px, max.y - rounding),
                          ImGui::GetColorU32(Ink::AccentHi));
    }
    if (pictures) {
        const ImVec2 p0(at.x + pad, at.y + (height - picture_size.y) * 0.5f);
        const ImVec2 p1(p0.x + picture_size.x, p0.y + picture_size.y);
        if (picture) {
            dl->AddImage(picture->GetTexRef(), p0, p1);
        } else { // the new save: no picture yet
            dl->AddRectFilled(p0, p1, ImGui::GetColorU32(Ink::Panel), 4.0f * px);
            const ImVec2 c((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
            const float k = 16.0f * px;
            const ImU32 ink = ImGui::GetColorU32(selected ? Ink::AccentHi : Ink::Muted);
            dl->AddLine(ImVec2(c.x - k, c.y), ImVec2(c.x + k, c.y), ink, 3.0f * px);
            dl->AddLine(ImVec2(c.x, c.y - k), ImVec2(c.x, c.y + k), ink, 3.0f * px);
        }
        dl->AddRect(p0, p1, ImGui::GetColorU32(Ink::Line), 4.0f * px, 0, 1.0f * px);
    }
    float y = at.y + (height - text_h) * 0.5f;
    for (size_t i = 0; i < lines.size(); ++i) {
        const float s = i == 0 ? name_size : size;
        const ImVec4 ink = i <= 1 ? Ink::Text : Ink::Muted;
        const char* begin = lines[i].data();
        const char* end = begin + lines[i].size();
        dl->AddText(font, s, ImVec2(at.x + text_x, y), ImGui::GetColorU32(ink), begin, end, wrap);
        y += font->CalcTextSizeA(s, FLT_MAX, wrap, begin, end).y + line_gap(i);
    }
    return height;
}

// The game's save list (sceSaveDataDialog) in the menu's look: the action ("Load game") in the title
// face over the red rule, then a card per save, the chosen one in red. Asked to overwrite, the new
// save is shown first (its picture and the details of the one it replaces), then the answers.
void ChoiceBox() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float px = base_scale;
    const float width = std::min(760.0f * px, viewport->WorkSize.x * 0.92f);
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                   viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.97f);
    ImGui::Begin("##choice", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoFocusOnAppearing);
    const bool confirm = !choice_preview_label.empty();
    const size_t details = confirm ? choice_title.find("\n\n") : std::string::npos;
    const std::string heading = choice_title.substr(0, details);
    ImGui::PushTextWrapPos(0.0f);
    if (title_font) ImGui::PushFont(title_font, kTitleSize * 0.85f);
    ImGui::TextColored(Ink::Text, "%s", heading.c_str());
    if (title_font) ImGui::PopFont();
    ImGui::PopTextWrapPos();
    HeadingRule();
    const float inner = ImGui::GetContentRegionAvail().x;
    const float card_gap = 8.0f * px;
    if (confirm) {
        // The new save: its picture, then the save it would replace.
        std::string text = choice_preview_label;
        if (details != std::string::npos) text += "\n" + choice_title.substr(details + 2);
        ImTextureData* preview = choice_textures.size() > choice_items.size() ? choice_textures.back() : nullptr;
        const float h = SaveCard(ImGui::GetCursorScreenPos(), inner, text, preview, true, false, true);
        ImGui::Dummy(ImVec2(inner, h + card_gap));
    }
    const int count = int(choice_items.size());
    const size_t listed = std::min(choice_textures.size(), choice_items.size());
    const bool pictures = !confirm && std::any_of(choice_textures.begin(), choice_textures.begin() + listed,
                                                  [](ImTextureData* tex) { return tex != nullptr; });
    const auto picture_of = [&](int i) { return pictures && i < int(listed) ? choice_textures[i] : nullptr; };
    float total = 0.0f;
    for (int i = 0; i < count; ++i) {
        total += SaveCard(ImVec2(), inner, choice_items[i], picture_of(i), pictures, false, false) +
                 (i + 1 < count ? card_gap : 0.0f);
    }
    // A long list scrolls in its own area, between the heading and the keys.
    const float most = viewport->WorkSize.y * 0.86f - ImGui::GetCursorPosY() - ImGui::GetFrameHeightWithSpacing() * 2.0f;
    ImGui::BeginChild("##saves", ImVec2(inner, std::min(total, std::max(most, 120.0f * px))), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
    for (int i = 0; i < count; ++i) {
        const bool selected = i == choice_index;
        const float h = SaveCard(ImGui::GetCursorScreenPos(), inner, choice_items[i], picture_of(i), pictures,
                                 selected, true);
        ImGui::Dummy(ImVec2(inner, h));
        if (selected) {
            ImGui::SetScrollHereY(0.5f);
        }
        if (i + 1 < count) {
            ImGui::Dummy(ImVec2(inner, card_gap));
        }
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::Spacing();
    const bool choice_swapped = SpartanSettings::Get().menu_confirm_swap.load(std::memory_order_relaxed) != 0;
    DisabledText(choice_swapped
        ? PadText("Up/Down or D-pad: choose   Enter or Circle (B): OK   Esc or Cross (A): cancel",
                  "Up/Down or D-pad: choose   Enter or B: OK   Esc or A: cancel",
                  "Up/Down or D-pad: choose   Enter or A: OK   Esc or B: cancel")
        : PadText("Up/Down or D-pad: choose   Enter or Cross (A): OK   Esc or Circle (B): cancel",
                  "Up/Down or D-pad: choose   Enter or A: OK   Esc or B: cancel",
                  "Up/Down or D-pad: choose   Enter or B: OK   Esc or A: cancel"));
    ImGui::End();
}

void LoadingScreen() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                   viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x * 0.4f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("##loading", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing);
    const u32 done = loading_done, total = loading_total;
    ImGui::SetWindowFontScale(1.6f);
    UiText(total ? "Loading shader cache..." : "Preparing graphics...");
    ImGui::SetWindowFontScale(1.0f);
    if (total) {
        char count[64];
        std::snprintf(count, sizeof(count), "%u / %u (%u%%)", done, total,
                      u32(u64(done) * 100 / total));
        ImGui::ProgressBar(float(done) / float(total), ImVec2(-1.0f, 0.0f), count);
    }
    DisabledText("The first start after an update takes longer.");
    ImGui::End();
}

void FpsCounter() {
    const auto& s = SpartanSettings::Get();
    const std::string text = OverlayText();
    if (text.empty()) {
        return;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float pad = 12.0f * base_scale;
    const int corner = s.overlay_corner;
    const bool right = corner == 1 || corner == 3, bottom = corner >= 2;
    ImGui::SetNextWindowPos(ImVec2(right ? viewport->WorkPos.x + viewport->WorkSize.x - pad : viewport->WorkPos.x + pad,
                                   bottom ? viewport->WorkPos.y + viewport->WorkSize.y - pad : viewport->WorkPos.y + pad),
                            ImGuiCond_Always, ImVec2(right ? 1.0f : 0.0f, bottom ? 1.0f : 0.0f));
    ImGui::SetNextWindowBgAlpha(s.overlay_opacity / 100.0f);
    ImGui::Begin("##fps", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::SetWindowFontScale(s.overlay_scale / 100.0f);
    UiText(text.c_str());
    ImGui::End();
}

void BackgroundLoading() {
    const u32 done = background_done, total = std::max(background_total.load(), 1u);
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float pad = 12.0f * base_scale;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + pad,
                                   viewport->WorkPos.y + viewport->WorkSize.y - pad),
                            ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::Begin("##background_loading", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::TextDisabled(Tr("Loading shader cache %u%%"), u32(u64(done) * 100 / total));
    ImGui::End();
}

// Trophy texts and list (set once by spartanportgpu_trophy_setup from the runtime, which reads them from the
// launcher's trophy_texts.txt): the labels of the notice and of the in-game trophy window.
struct TrophyLabels {
    std::string heading = "Trophies", notice = "TROPHY UNLOCKED", unlocked = "UNLOCKED", locked = "LOCKED";
    std::string all = "All trophies", only_unlocked = "Unlocked", only_locked = "Locked";
    std::string counter = "{} of {} unlocked";
    std::string hint = "Up/Down: scroll   Left/Right: filter   F8 / R3+R2 / Esc: close";
    std::string notifications = "Notifications";
    std::string order_list = "List order", order_common = "Most earned first", order_rare = "Rarest first";
    std::string rarity = "{}% of players";
    std::string common = "Common", rare = "Rare", very_rare = "Very rare", ultra_rare = "Ultra rare";
    std::string hint_order = "Triangle/Tab: order   L1/R1: notifications";
    std::string hint_notifications = "L1/R1: trophies   F8 / R3+R2 / Esc: close";
};
struct TrophyRow {
    int id = 0;
    std::string name, detail;
    float earned = -1.0f;            // percent of players who earned it (spartanport_trophy_rarity.h); -1 unknown
    SpartanToast::Grade grade = SpartanToast::Grade::Unknown; // by the colour of its icon
    SpartanPng::Image image;         // decoded icon, waiting for its texture
    ImTextureData* tex = nullptr; // render thread, lives for the whole run
};
TrophyLabels trophy_labels;                      // imgui_mutex
std::vector<TrophyRow> trophy_rows;              // imgui_mutex
const volatile uint32_t* trophy_flags = nullptr; // the runtime's unlocked bits, read live
std::atomic<bool> trophy_ready{false}, trophy_started{false};
std::atomic<bool> trophy_view{false};            // the in-game trophy window is open
int trophy_filter = 0;                           // 0 all, 1 unlocked, 2 locked
int trophy_grade = 0;                            // 0 every grade, else only that SpartanToast::Grade
int trophy_order = SpartanTrophyRarity::MostEarned; // SpartanTrophyRarity::Order
int trophy_page = 0;                             // 0 the list, 1 the notification options
bool trophy_focus = false;                       // give the window the focus on its next frame
float trophy_scroll = 0.0f;                      // pixels

// What the notification themes (spartanport_trophy_toast.cpp) show of each trophy, by id: filled
// once by spartanportgpu_trophy_setup's worker, read from any thread.
std::mutex trophy_entry_mutex;
std::vector<SpartanToast::Entry> trophy_entries; // trophy_entry_mutex

SpartanToast::Entry TrophyEntry(int id) {
    std::scoped_lock lock{trophy_entry_mutex};
    return id >= 0 && id < int(trophy_entries.size()) ? trophy_entries[size_t(id)] : SpartanToast::Entry{};
}

// imgui_mutex held (the toast draws inside Render): the list's texture of a trophy's icon.
ImTextureRef TrophyIcon(int id) {
    for (const TrophyRow& row : trophy_rows) {
        if (row.id == id && row.tex) {
            return row.tex->GetTexRef();
        }
    }
    return {};
}

// The launcher's trophy texts in a language (<user>/trophy_texts/<code>.txt); empty when missing.
SpartanTrophyTexts::Texts ReadTrophyTexts(std::string_view code) {
    const char* dir = std::getenv("SPARTAN_USER_DIR");
    const std::string path = std::string(dir && dir[0] ? dir : "user") + "/trophy_texts/" + std::string(code) + ".txt";
    std::ifstream file(path, std::ios::binary);
    return SpartanTrophyTexts::Parse(std::string(std::istreambuf_iterator<char>(file), {}));
}

// The labels, names and descriptions of `texts` over the window's (what the file lacks stays).
void TakeTrophyTexts(const SpartanTrophyTexts::Texts& texts, TrophyLabels& labels, std::vector<TrophyRow>& rows,
                     std::vector<SpartanToast::Entry>& entries) {
    std::string* const fields[] = {&labels.heading, &labels.notice, &labels.unlocked, &labels.locked,
                                   &labels.all, &labels.only_unlocked, &labels.only_locked, &labels.counter,
                                   &labels.hint, &labels.notifications, &labels.order_list, &labels.order_common,
                                   &labels.order_rare, &labels.rarity, &labels.common, &labels.rare,
                                   &labels.very_rare, &labels.ultra_rare, &labels.hint_order,
                                   &labels.hint_notifications};
    static const char* const keys[] = {"heading", "notice", "unlocked", "locked", "all", "only_unlocked",
                                       "only_locked", "counter", "hint", "notifications", "order_list",
                                       "order_common", "order_rare", "rarity", "common", "rare", "very_rare",
                                       "ultra_rare", "hint_order", "hint_notifications"};
    for (const auto& [key, text] : texts.labels) {
        for (size_t k = 0; k < std::size(keys); ++k) {
            if (key == keys[k] && !text.empty()) {
                *fields[k] = text;
            }
        }
    }
    for (TrophyRow& row : rows) {
        const auto& [name, detail] = texts.trophies[size_t(row.id)];
        if (!name.empty()) {
            row.name = name;
            row.detail = detail;
        }
    }
    for (size_t id = 0; id < entries.size() && id < texts.trophies.size(); ++id) {
        if (!texts.trophies[id].first.empty()) {
            entries[id].name = texts.trophies[id].first;
            entries[id].detail = texts.trophies[id].second;
        }
    }
}

// imgui_mutex held: the menu, the trophy window and the notifications in Languages[index] at once.
void ApplyLanguage(int index) {
    index = SpartanUi::SetLanguage(index);
    SpartanToast::SetDefaultTexts(Tr("Trophy unlocked!"), {"", Tr("Bronze"), Tr("Silver"), Tr("Gold"), Tr("Platinum")});
    if (!trophy_ready) {
        return; // spartanportgpu_trophy_setup reads the language's texts itself
    }
    const auto texts = ReadTrophyTexts(SpartanUi::Languages[index].code);
    if (!texts.count) {
        return;
    }
    std::scoped_lock lock{trophy_entry_mutex};
    TakeTrophyTexts(texts, trophy_labels, trophy_rows, trophy_entries);
}

// The language row (the Insert menu's Game tab and the trophy window's Notifications page): "Same as the
// launcher", then every language the overlay can draw (Chinese once its font is loaded).
void LanguageRow() {
    auto& s = SpartanSettings::Get();
    Row(Tr("Language"));
    const char* env = std::getenv("SPARTAN_UI_LANGUAGE");
    const int launcher = SpartanUi::FindLanguage(env ? env : "");
    const std::string same = std::string(Tr("Same as the launcher")) + " (" + SpartanUi::Languages[launcher].name + ")";
    const int chosen = s.ui_language;
    const char* preview = chosen >= 0 ? SpartanUi::Languages[chosen].name : same.c_str();
    if (ImGui::BeginCombo("##value", preview)) {
        if (ImGui::Selectable(same.c_str(), chosen < 0)) {
            s.ui_language = -1;
            ApplyLanguage(launcher);
            SpartanSettings::Save();
        }
        for (int i = 0; i < SpartanUi::LanguageCount; ++i) {
            if (SpartanUi::Languages[i].code == "zh" && !SpartanUi::chinese_font) {
                continue;
            }
            if (ImGui::Selectable(SpartanUi::Languages[i].name, chosen == i)) {
                s.ui_language = i;
                ApplyLanguage(i);
                SpartanSettings::Save();
            }
        }
        ImGui::EndCombo();
    }
    Help(Tr("The language of this menu and of the trophies. Until you choose one, the launcher's is used."));
    ImGui::PopID();
}

// Trophy notices: queued by spartanportgpu_toast/spartanportgpu_toast_icon (any thread), drawn by Render in the
// top-right corner, below the FPS counter.
struct Toast {
    std::string text, detail;     // title (the trophy's name) and the line under it (its description)
    std::string notice;           // the first line; empty: the launcher's label
    std::chrono::steady_clock::time_point start, until;
    SpartanPng::Image image;         // decoded icon, waiting for its texture (empty: none)
    ImTextureData* tex = nullptr; // render thread only
    ImTextureData* shown = nullptr; // a preview's: the trophy list's icon (not owned)
};
std::mutex toast_mutex;
std::vector<Toast> toasts;
std::vector<ImTextureData*> toast_retired; // render thread only; freed once the backend destroyed them
std::atomic<bool> toast_active{false};
constexpr float kToastSeconds = 5.0f;

// Queues a notice (any thread). The notification options can lengthen it and change its first line.
void PushNotice(std::string title, std::string detail, SpartanPng::Image image, ImTextureData* shown = nullptr) {
    const SpartanToast::Options o = SpartanToast::GetOptions();
    const float seconds = o.hold_seconds > 0.0f ? o.hold_seconds : kToastSeconds;
    std::scoped_lock lock{toast_mutex};
    const auto now = std::chrono::steady_clock::now();
    if (toasts.size() >= 6) {
        toasts.front().until = now; // expires now; Render retires its texture
    }
    toasts.push_back({std::move(title), std::move(detail), o.text_banner, now,
                      now + std::chrono::milliseconds(int(seconds * 1000.0f)), std::move(image), nullptr, shown});
    toast_active = true;
}

// The notices' top edge: below the FPS counter when it is showing in the top-right half of the screen.
float NoticeTop(float pad, float gap) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    float y = viewport->WorkPos.y + pad;
    if (ImGuiWindow* fps = ImGui::FindWindowByName("##fps"); fps && fps->Active) {
        const float mid_x = viewport->WorkPos.x + viewport->WorkSize.x * 0.5f;
        const float mid_y = viewport->WorkPos.y + viewport->WorkSize.y * 0.5f;
        if (fps->Pos.y < mid_y && fps->Pos.x + fps->Size.x > mid_x) {
            y = std::max(y, fps->Pos.y + fps->Size.y + gap);
        }
    }
    return y;
}

// The trophy window's second page, the notifications: theme, sound and timing (saved in
// user/custom_trophy/notifications.ini by SpartanToast, not in spartanport.ini), and previews that go
// through the same queue, renderer and sounds as a real unlock (nothing is unlocked or saved).
void NotificationsPage() {
    using SpartanToast::Grade;
    using SpartanToast::Theme;
    const auto T = [](const char* english) { return Tr(english); };
    SpartanToast::Options o = SpartanToast::GetOptions();
    bool changed = false;
    if (BeginRows("##trophy_notifications")) {
        LanguageRow();
        Row(T("Theme"));
        const char* themes[int(Theme::Count)];
        for (int i = 0; i < int(Theme::Count); ++i) {
            themes[i] = T(SpartanToast::ThemeLabel(Theme(i)));
        }
        int theme = int(o.theme);
        if (ImGui::Combo("##value", &theme, themes, int(Theme::Count))) {
            o.theme = Theme(theme);
            changed = true;
        }
        Help(T("How an unlocked trophy is shown. Original is the port's own notice."));
        ImGui::PopID();
        Row(T("Mute the sound"));
        changed |= ImGui::Checkbox("##value", &o.muted);
        Help(T("No sound plays without a sound file in user/custom_trophy."));
        ImGui::PopID();
        Row(T("Volume"));
        ImGui::BeginDisabled(o.muted);
        changed |= ImGui::SliderInt("##value", &o.volume, 0, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp);
        ImGui::EndDisabled();
        ImGui::PopID();
        Row(T("Time on screen"));
        changed |= ImGui::SliderFloat("##value", &o.hold_seconds, 0.0f, 20.0f,
                                      o.hold_seconds < 0.05f ? T("Theme default") : "%.1f s",
                                      ImGuiSliderFlags_AlwaysClamp);
        if (o.hold_seconds < 0.05f) {
            o.hold_seconds = 0.0f;
        }
        ImGui::PopID();
        if (o.theme == Theme::CardCinematic) {
            Row(T("Platinum light"));
            changed |= ImGui::SliderInt("##value", &o.cinematic_intensity, 0, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp);
            ImGui::PopID();
        }
        if (o.theme != Theme::Original) {
            Row(T("Animate the platinum"));
            changed |= ImGui::Checkbox("##value", &o.platinum_animation);
            Help(T("Off: the platinum shows its own picture, like the other trophies."));
            ImGui::PopID();
            Row(T("Write the grade"));
            changed |= ImGui::Checkbox("##value", &o.grade_label);
            Help(T("The grade's name before the second line."));
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (changed) {
        SpartanToast::SetOptions(o);
    }

    // A trophy of each grade, shown as an unlock would be (the first trophy when none has that grade).
    std::vector<SpartanToast::Entry> entries;
    {
        std::scoped_lock lock{trophy_entry_mutex};
        entries = trophy_entries;
    }
    auto preview = [&](Grade grade) {
        int id = -1;
        for (int i = 0; i < int(entries.size()); ++i) {
            if (entries[size_t(i)].name.empty()) {
                continue;
            }
            if (id < 0 || (entries[size_t(i)].grade == grade && entries[size_t(id)].grade != grade)) {
                id = i;
            }
        }
        if (id < 0) {
            return;
        }
        if (o.theme != Theme::Original) {
            SpartanToast::Enqueue(id, true, int(grade));
            return;
        }
        ImTextureData* icon = nullptr;
        for (const TrophyRow& row : trophy_rows) {
            if (row.id == id) {
                icon = row.tex;
            }
        }
        PushNotice(entries[size_t(id)].name, entries[size_t(id)].detail, {}, icon);
        SpartanToast::PlaySound(id, int(grade));
    };
    Separator(T("Preview"));
    ImGui::BeginDisabled(entries.empty());
    static const char* const grade_names[] = {"Bronze", "Silver", "Gold", "Platinum"};
    static const ImVec4 grade_inks[] = {ImVec4(0.80f, 0.50f, 0.42f, 1.0f), ImVec4(0.78f, 0.81f, 0.86f, 1.0f),
                                        ImVec4(0.92f, 0.73f, 0.33f, 1.0f), ImVec4(0.70f, 0.82f, 0.95f, 1.0f)};
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float button_w = (ImGui::GetContentRegionAvail().x - spacing * 3) / 4;
    for (int i = 0; i < 4; ++i) {
        ImGui::PushStyleColor(ImGuiCol_Text, grade_inks[i]);
        if (ImGui::Button(T(grade_names[i]), ImVec2(button_w, 0))) {
            preview(Grade(i + 1));
        }
        ImGui::PopStyleColor();
        Help(T("A preview: it looks and sounds like an unlock; nothing is unlocked or saved."));
        if (i < 3) {
            ImGui::SameLine();
        }
    }
    const float half = (ImGui::GetContentRegionAvail().x - spacing) / 2;
    if (ImGui::Button(T("All four in a row"), ImVec2(half, 0))) {
        for (int i = 1; i <= 4; ++i) {
            preview(Grade(i));
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(T("Three at once"), ImVec2(-FLT_MIN, 0))) {
        for (int i = 0; i < 3; ++i) {
            preview(Grade::Bronze);
        }
    }
    ImGui::EndDisabled();
    Separator(T("Your files"));
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s %s", T("Sounds:"), SpartanToast::SoundSummary().c_str());
    ImGui::TextDisabled("%s %s", T("Pictures:"), SpartanToast::PictureSummary().c_str());
    DisabledText(T("Your own sounds, pictures, animations and texts go in user/custom_trophy "
                   "(docs/TROPHIES.md). Sounds reload here; pictures when the game starts."));
    ImGui::PopTextWrapPos();
    if (Button(T("Reload sounds"))) {
        SpartanToast::ReloadSounds();
    }
}

// imgui_mutex held, before NewFrame: icons that arrived become textures, destroyed ones are freed.
void UpdateToastTextures() {
    std::scoped_lock lock{toast_mutex};
    for (Toast& t : toasts) {
        if (t.tex || t.image.rgba.empty()) {
            continue;
        }
        ImTextureData* tex = IM_NEW(ImTextureData)();
        tex->Create(ImTextureFormat_RGBA32, t.image.width, t.image.height);
        std::memcpy(tex->GetPixels(), t.image.rgba.data(), t.image.rgba.size());
        ImGui::RegisterUserTexture(tex);
        t.tex = tex;
        t.image = {};
    }
    std::erase_if(toast_retired, [](ImTextureData* tex) {
        if (tex->Status != ImTextureStatus_Destroyed) {
            return false;
        }
        ImGui::UnregisterUserTexture(tex);
        IM_DELETE(tex);
        return true;
    });
    toast_active = !toasts.empty() || !toast_retired.empty();
}

// A gold medal with a cup, drawn with shapes: used when a trophy has no icon. a: overall opacity.
void DrawTrophyIcon(ImDrawList* dl, ImVec2 c, float r, float a) {
    auto col = [&](float R, float G, float B, float A) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(R, G, B, A * a));
    };
    dl->AddCircleFilled(c, r, col(0.96f, 0.74f, 0.20f, 1.0f), 48);
    dl->AddCircle(c, r, col(1.0f, 0.92f, 0.60f, 1.0f), 48, std::max(1.5f, r * 0.07f));
    const ImU32 ink = col(0.12f, 0.08f, 0.02f, 1.0f);
    const float u = r / 10.0f;
    dl->AddCircle(ImVec2(c.x - 5.4f * u, c.y - 2.6f * u), 2.2f * u, ink, 20, 1.3f * u);
    dl->AddCircle(ImVec2(c.x + 5.4f * u, c.y - 2.6f * u), 2.2f * u, ink, 20, 1.3f * u);
    dl->AddQuadFilled(ImVec2(c.x - 4.6f * u, c.y - 5.8f * u), ImVec2(c.x + 4.6f * u, c.y - 5.8f * u),
                      ImVec2(c.x + 2.4f * u, c.y + 1.6f * u), ImVec2(c.x - 2.4f * u, c.y + 1.6f * u), ink);
    dl->AddRectFilled(ImVec2(c.x - 0.9f * u, c.y + 1.6f * u), ImVec2(c.x + 0.9f * u, c.y + 4.4f * u), ink);
    dl->AddRectFilled(ImVec2(c.x - 3.2f * u, c.y + 4.4f * u), ImVec2(c.x + 3.2f * u, c.y + 6.0f * u), ink, 0.5f * u);
}

void ToastBox() {
    const auto now = std::chrono::steady_clock::now();
    struct Line {
        std::string text, detail, notice;
        float elapsed, left, seconds;
        ImTextureData* tex;
    };
    std::vector<Line> lines;
    {
        std::scoped_lock lock{toast_mutex};
        for (Toast& t : toasts) {
            if (t.until <= now && t.tex) {
                t.tex->WantDestroyNextFrame = true;
                toast_retired.push_back(t.tex);
                t.tex = nullptr;
            }
        }
        toasts.erase(std::remove_if(toasts.begin(), toasts.end(),
                                    [&](const Toast& t) { return t.until <= now; }),
                     toasts.end());
        for (const Toast& t : toasts) {
            lines.push_back({t.text, t.detail, t.notice, std::chrono::duration<float>(now - t.start).count(),
                             std::chrono::duration<float>(t.until - now).count(),
                             std::chrono::duration<float>(t.until - t.start).count(), t.tex ? t.tex : t.shown});
        }
        toast_active = !toasts.empty() || !toast_retired.empty();
    }
    if (lines.empty()) {
        return;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float s = base_scale;
    const float pad = 20.0f * s, w = 380.0f * s, gap = 10.0f * s;
    const float text_x = 96.0f * s, wrap_w = w - text_x - 16.0f * s;
    const float fs = ImGui::GetFontSize();
    constexpr float kNameScale = 1.3f;
    float y = NoticeTop(pad, gap);
    for (size_t i = 0; i < lines.size(); i++) {
        const Line& l = lines[i];
        // The card grows with the wrapped name and description (never under 88 px).
        const float name_h = ImGui::CalcTextSize(l.text.c_str(), nullptr, false, wrap_w / kNameScale).y * kNameScale;
        const float detail_h = l.detail.empty() ? 0.0f : ImGui::CalcTextSize(l.detail.c_str(), nullptr, false, wrap_w).y;
        const float block = fs * 1.15f + name_h + (detail_h > 0.0f ? 4.0f * s + detail_h : 0.0f);
        const float h = std::max(88.0f * s, block + 28.0f * s);
        const float tin = std::min(1.0f, l.elapsed / 0.4f);
        const float ease = 1.0f - (1.0f - tin) * (1.0f - tin) * (1.0f - tin); // slides in from the right
        const float alpha = std::clamp(std::min(l.left / 0.5f, l.elapsed / 0.2f), 0.0f, 1.0f);
        const float x = viewport->WorkPos.x + viewport->WorkSize.x - pad + (1.0f - ease) * (w + pad);
        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f * s);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f * s);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.05f, 0.08f, 0.92f * alpha));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.96f, 0.74f, 0.20f, 0.9f * alpha));
        char name[40];
        std::snprintf(name, sizeof(name), "##trophy_toast_%zu", i);
        ImGui::Begin(name, nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                         ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetWindowPos();
        const ImU32 gold = ImGui::ColorConvertFloat4ToU32(ImVec4(0.96f, 0.74f, 0.20f, alpha));
        const ImU32 goldSoft = ImGui::ColorConvertFloat4ToU32(ImVec4(0.96f, 0.74f, 0.20f, 0.75f * alpha));
        dl->AddRectFilled(p, ImVec2(p.x + 6.0f * s, p.y + h), gold, 10.0f * s, ImDrawFlags_RoundCornersLeft);
        if (l.tex) {
            const float icon = 64.0f * s;
            ImGui::SetCursorPos(ImVec2(18.0f * s, (h - 8.0f * s - icon) * 0.5f));
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
            ImGui::Image(l.tex->GetTexRef(), ImVec2(icon, icon));
            ImGui::PopStyleVar();
            dl->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), gold, 3.0f * s, 0, 2.0f * s);
        } else {
            DrawTrophyIcon(dl, ImVec2(p.x + 52.0f * s, p.y + (h - 8.0f * s) * 0.5f), 28.0f * s, alpha);
        }
        const float frac = std::clamp(l.left / l.seconds, 0.0f, 1.0f); // time left
        dl->AddRectFilled(ImVec2(p.x + 14.0f * s, p.y + h - 8.0f * s),
                          ImVec2(p.x + 14.0f * s + (w - 28.0f * s) * frac, p.y + h - 5.0f * s), goldSoft, 1.5f * s);
        const float top = std::max(8.0f * s, (h - 8.0f * s - block) * 0.5f);
        ImGui::PushTextWrapPos(w - 16.0f * s);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SetCursorPos(ImVec2(text_x, top));
        ImGui::TextColored(ImVec4(0.96f, 0.74f, 0.20f, alpha), "%s",
                           l.notice.empty() ? trophy_labels.notice.c_str() : l.notice.c_str());
        ImGui::SetCursorPos(ImVec2(text_x, top + fs * 1.15f));
        ImGui::SetWindowFontScale(kNameScale);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, alpha), "%s", l.text.c_str());
        ImGui::SetWindowFontScale(1.0f);
        if (detail_h > 0.0f) {
            ImGui::SetCursorPos(ImVec2(text_x, top + fs * 1.15f + name_h + 4.0f * s));
            ImGui::TextColored(ImVec4(0.80f, 0.83f, 0.90f, alpha), "%s", l.detail.c_str());
        }
        ImGui::PopTextWrapPos();
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
        y += h + gap;
    }
}

bool TrophyUnlocked(int id) {
    return trophy_flags && id >= 0 && id < 128 && ((trophy_flags[id / 32] >> (id % 32)) & 1u);
}

void SetTrophyPage(int page) {
    trophy_page = page;
    trophy_focus = page == 1;
    ImGui::GetIO().MouseDrawCursor = page == 1 || menu_open;
}

// The grade filter's order: every grade, then platinum, gold, silver and bronze.
constexpr SpartanToast::Grade kGradeFilters[] = {SpartanToast::Grade::Unknown, SpartanToast::Grade::Platinum,
                                                 SpartanToast::Grade::Gold, SpartanToast::Grade::Silver,
                                                 SpartanToast::Grade::Bronze};

void NextGradeFilter() {
    int at = 0;
    for (int i = 0; i < int(std::size(kGradeFilters)); ++i) {
        if (int(kGradeFilters[i]) == trophy_grade) {
            at = i;
        }
    }
    trophy_grade = int(kGradeFilters[(at + 1) % int(std::size(kGradeFilters))]);
    trophy_scroll = 0.0f;
}

// A small trophy cup (bowl, handles, stem, base) of height `h` centred on `c`: the grade's mark.
void SmallCup(ImDrawList* dl, ImVec2 c, float h, ImU32 ink) {
    const float x = c.x, y = c.y, u = h;
    const ImVec2 bowl[] = {{x - .34f * u, y - .42f * u}, {x + .34f * u, y - .42f * u}, {x + .24f * u, y + .02f * u},
                           {x, y + .12f * u}, {x - .24f * u, y + .02f * u}};
    dl->AddConvexPolyFilled(bowl, 5, ink);
    const float w = std::max(1.0f, .08f * u);
    dl->AddLine({x - .32f * u, y - .32f * u}, {x - .48f * u, y - .32f * u}, ink, w);
    dl->AddLine({x - .48f * u, y - .32f * u}, {x - .36f * u, y - .08f * u}, ink, w);
    dl->AddLine({x + .32f * u, y - .32f * u}, {x + .48f * u, y - .32f * u}, ink, w);
    dl->AddLine({x + .48f * u, y - .32f * u}, {x + .36f * u, y - .08f * u}, ink, w);
    dl->AddRectFilled({x - .06f * u, y + .08f * u}, {x + .06f * u, y + .32f * u}, ink);
    dl->AddRectFilled({x - .26f * u, y + .30f * u}, {x + .26f * u, y + .42f * u}, ink, .04f * u);
}

void SetTrophyView(bool open) {
    trophy_view = open;
    trophy_scroll = 0.0f;
    trophy_filter = 0;
    trophy_grade = 0;
    SetTrophyPage(0);
}

// imgui_mutex held, before NewFrame: the icons of the list become textures (once).
void UpdateTrophyTextures() {
    for (TrophyRow& row : trophy_rows) {
        if (row.tex || row.image.rgba.empty()) {
            continue;
        }
        ImTextureData* tex = IM_NEW(ImTextureData)();
        tex->Create(ImTextureFormat_RGBA32, row.image.width, row.image.height);
        std::memcpy(tex->GetPixels(), row.image.rgba.data(), row.image.rgba.size());
        ImGui::RegisterUserTexture(tex);
        row.tex = tex;
        row.image = {};
    }
}

constexpr float kTrophyRow = 80.0f, kTrophyGap = 6.0f; // layout units (x base_scale)

// The list takes the keys and buttons that scroll, filter and close; the rest of the input stays
// with the game (held neutral by CapturesInput).
bool HandleTrophyEvent(const SDL_Event& event) {
    const float step = (kTrophyRow + kTrophyGap) * base_scale;
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN: {
        const SDL_Keycode key = event.key.key;
        if (key == SDLK_UP || key == SDLK_W) {
            trophy_scroll -= step;
        } else if (key == SDLK_DOWN || key == SDLK_S) {
            trophy_scroll += step;
        } else if (key == SDLK_PAGEUP) {
            trophy_scroll -= 4.0f * step;
        } else if (key == SDLK_PAGEDOWN) {
            trophy_scroll += 4.0f * step;
        } else if (key == SDLK_HOME) {
            trophy_scroll = 0.0f;
        } else if (key == SDLK_END) {
            trophy_scroll = 1.0e6f; // clamped by the window
        } else if (key == SDLK_LEFT || key == SDLK_A || key == SDLK_RIGHT || key == SDLK_D) {
            trophy_filter = (trophy_filter + (key == SDLK_LEFT || key == SDLK_A ? 2 : 1)) % 3;
            trophy_scroll = 0.0f;
        } else if (key == SDLK_G) {
            NextGradeFilter();
        } else if (key == SDLK_TAB) {
            trophy_order = (trophy_order + 1) % 3;
            trophy_scroll = 0.0f;
        } else if (!event.key.repeat && (key == SDLK_Q || key == SDLK_E)) {
            SetTrophyPage(1);
        } else if (!event.key.repeat && (key == SDLK_F8 || key == SDLK_ESCAPE || key == SDLK_BACKSPACE ||
                                         key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_INSERT)) {
            SetTrophyView(false);
        }
        return true;
    }
    case SDL_EVENT_KEY_UP:
        return true;
    case SDL_EVENT_MOUSE_WHEEL:
        trophy_scroll -= event.wheel.y * step;
        return true;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
        const bool down = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        const u8 button = event.gbutton.button;
        if (button == SDL_GAMEPAD_BUTTON_RIGHT_STICK) {
            r3_down = down;
            if (down && r2_down) {
                SetTrophyView(false);
            }
        } else if (down) {
            if (button == SDL_GAMEPAD_BUTTON_DPAD_UP) {
                trophy_scroll -= step;
            } else if (button == SDL_GAMEPAD_BUTTON_DPAD_DOWN) {
                trophy_scroll += step;
            } else if (button == SDL_GAMEPAD_BUTTON_DPAD_LEFT || button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) {
                trophy_filter = (trophy_filter + (button == SDL_GAMEPAD_BUTTON_DPAD_LEFT ? 2 : 1)) % 3;
                trophy_scroll = 0.0f;
            } else if (button == SDL_GAMEPAD_BUTTON_WEST) {
                NextGradeFilter();
            } else if (button == SDL_GAMEPAD_BUTTON_NORTH) {
                trophy_order = (trophy_order + 1) % 3;
                trophy_scroll = 0.0f;
            } else if (button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER || button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) {
                SetTrophyPage(1);
            } else if (button == SDL_GAMEPAD_BUTTON_EAST || button == SDL_GAMEPAD_BUTTON_SOUTH) {
                SetTrophyView(false);
            }
        }
        return true;
    }
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
            const bool down = event.gaxis.value > 16000;
            if (down && !r2_down && r3_down) {
                r2_down = true;
                SetTrophyView(false);
                return true;
            }
            r2_down = down;
        } else if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
            l2_down = event.gaxis.value > 16000;
        }
        return false;
    default:
        return false;
    }
}

// The options page: switching back to the list and closing the window are handled here; the rest
// goes to ImGui as in the menu (false).
bool HandleTrophyOptionsEvent(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN: {
        const SDL_Keycode key = event.key.key;
        if (event.key.repeat) {
            return false;
        }
        if (key == SDLK_Q || key == SDLK_E) {
            SetTrophyPage(0);
            return true;
        }
        if (key == SDLK_F8 || key == SDLK_ESCAPE || key == SDLK_INSERT) {
            SetTrophyView(false);
            return true;
        }
        return false;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
        const bool down = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        const u8 button = event.gbutton.button;
        if (button == SDL_GAMEPAD_BUTTON_RIGHT_STICK) {
            r3_down = down;
            if (down && r2_down) {
                SetTrophyView(false);
            }
            return true;
        }
        if (button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER || button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) {
            if (down) {
                SetTrophyPage(0);
            }
            return true;
        }
        return false;
    }
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
            const bool down = event.gaxis.value > 16000;
            if (down && !r2_down && r3_down) {
                SetTrophyView(false);
            }
            r2_down = down;
        } else if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
            l2_down = event.gaxis.value > 16000;
        }
        return true;
    default:
        return false;
    }
}

std::string TrophyCounter(int have, int total) {
    std::string text = trophy_labels.counter;
    for (const int value : {have, total}) {
        const size_t at = text.find("{}");
        if (at == std::string::npos) {
            break;
        }
        text.replace(at, 2, std::to_string(value));
    }
    return text;
}

// The trophy window, centered over the game, in two pages: the list (icon, name, description, state
// and how many players earned each trophy, in the chosen order) and the notification options.
void TrophyView() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float s = base_scale;
    const ImVec2 size(std::min(780.0f * s, viewport->WorkSize.x - 40.0f * s),
                      std::min(660.0f * s, viewport->WorkSize.y - 60.0f * s));
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                   viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    if (trophy_focus) {
        ImGui::SetNextWindowFocus();
        trophy_focus = false;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f * s);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f * s);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(Ink::Bg.x, Ink::Bg.y, Ink::Bg.z, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_Border, Ink::Line);
    // The list takes its keys itself (HandleTrophyEvent); the options page is driven like the menu.
    ImGui::Begin("##trophy_list", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoFocusOnAppearing |
                     (trophy_page == 0 ? ImGuiWindowFlags_NoNav : 0));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetWindowPos();
    ImGui::SetWindowFontScale(1.0f);
    const float fs = ImGui::GetFontSize();
    // The menu's colours (Ink): gold marks what is unlocked, red what is chosen.
    const ImVec4 gold = Ink::Gold, white = Ink::Text, muted = Ink::Muted;
    auto color = [](const ImVec4& c, float a = 1.0f) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, c.w * a));
    };

    int have = 0;
    int grade_have[5] = {}, grade_total[5] = {}; // by SpartanToast::Grade
    std::vector<const TrophyRow*> shown;
    for (const TrophyRow& row : trophy_rows) {
        const bool got = TrophyUnlocked(row.id);
        have += got ? 1 : 0;
        grade_have[int(row.grade)] += got ? 1 : 0;
        ++grade_total[int(row.grade)];
        const bool state = trophy_filter == 0 || (trophy_filter == 1 && got) || (trophy_filter == 2 && !got);
        if (state && (trophy_grade == 0 || int(row.grade) == trophy_grade)) {
            shown.push_back(&row);
        }
    }
    // Most earned or rarest first; trophies without a number keep the list's order, after the others.
    std::stable_sort(shown.begin(), shown.end(), [](const TrophyRow* a, const TrophyRow* b) {
        return SpartanTrophyRarity::Before(a->earned, b->earned, trophy_order);
    });

    // header: the two pages in the title face (the open one underlined in red), the counter, then the
    // filter and the order
    if (title_font) ImGui::PushFont(title_font, kTitleSize * 0.85f);
    const float title_h = ImGui::GetFontSize();
    if (title_font) ImGui::PopFont();
    const bool list = trophy_page == 0;
    const float grades_h = list ? fs + 12.0f * s : 0.0f; // the grade filter's line
    const float header_h = 16.0f * s + title_h + 10.0f * s + fs + 16.0f * s + grades_h;
    const float footer_h = (list ? 2.0f : 1.0f) * (fs + 4.0f * s) + 18.0f * s;
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + header_h), color(Ink::Panel), 12.0f * s, ImDrawFlags_RoundCornersTop);
    dl->AddRectFilledMultiColor(ImVec2(p.x, p.y + header_h - 2.0f * s), ImVec2(p.x + size.x * 0.6f, p.y + header_h),
                                color(Ink::AccentHi), color(Ink::Accent), color(Ink::Accent), color(Ink::AccentHi));
    dl->AddRectFilledMultiColor(ImVec2(p.x + size.x * 0.6f, p.y + header_h - 2.0f * s), ImVec2(p.x + size.x, p.y + header_h),
                                color(Ink::Accent), color(Ink::Accent, 0.0f), color(Ink::Accent, 0.0f), color(Ink::Accent));
    if (title_font) ImGui::PushFont(title_font, kTitleSize * 0.85f);
    else ImGui::SetWindowFontScale(1.5f);
    const std::string* pages[2] = {&trophy_labels.heading, &trophy_labels.notifications};
    float px = 24.0f * s;
    for (int i = 0; i < 2; i++) {
        const float w = ImGui::CalcTextSize(pages[i]->c_str()).x;
        ImGui::SetCursorPos(ImVec2(px, 16.0f * s));
        ImGui::TextColored(i == trophy_page ? white : muted, "%s", pages[i]->c_str());
        if (i == trophy_page) {
            dl->AddRectFilled(ImVec2(p.x + px, p.y + 16.0f * s + title_h + 1.0f * s),
                              ImVec2(p.x + px + w, p.y + 16.0f * s + title_h + 4.0f * s), color(Ink::AccentHi), 1.5f * s);
        }
        px += w + 34.0f * s;
    }
    if (title_font) ImGui::PopFont();
    else ImGui::SetWindowFontScale(1.0f);
    const std::string counter = TrophyCounter(have, int(trophy_rows.size()));
    ImGui::SetCursorPos(ImVec2(size.x - 24.0f * s - ImGui::CalcTextSize(counter.c_str()).x, 16.0f * s + (title_h - fs) * 0.5f));
    ImGui::TextColored(white, "%s", counter.c_str());
    const float fy = 16.0f * s + title_h + 10.0f * s;
    if (list) {
        const std::string* filters[3] = {&trophy_labels.all, &trophy_labels.only_unlocked, &trophy_labels.only_locked};
        float fx = 24.0f * s;
        for (int i = 0; i < 3; i++) {
            const float w = ImGui::CalcTextSize(filters[i]->c_str()).x;
            ImGui::SetCursorPos(ImVec2(fx, fy));
            ImGui::TextColored(i == trophy_filter ? white : muted, "%s", filters[i]->c_str());
            if (i == trophy_filter) {
                dl->AddRectFilled(ImVec2(p.x + fx, p.y + fy + fs + 3.0f * s), ImVec2(p.x + fx + w, p.y + fy + fs + 5.0f * s),
                                  color(Ink::AccentHi), 1.0f * s);
            }
            fx += w + 28.0f * s;
        }
        const std::string& order = trophy_order == SpartanTrophyRarity::MostEarned ? trophy_labels.order_common
                                   : trophy_order == SpartanTrophyRarity::Rarest   ? trophy_labels.order_rare
                                                                                   : trophy_labels.order_list;
        ImGui::SetCursorPos(ImVec2(size.x - 24.0f * s - ImGui::CalcTextSize(order.c_str()).x, fy));
        ImGui::TextColored(gold, "%s", order.c_str());
        // The grades: every one, then each with its cup and how many of them are unlocked.
        float gx = 24.0f * s;
        const float gy = fy + fs + 12.0f * s;
        for (const SpartanToast::Grade grade : kGradeFilters) {
            const bool chosen = int(grade) == trophy_grade;
            const bool every = grade == SpartanToast::Grade::Unknown;
            static const char* const names[] = {"All grades", "Bronze", "Silver", "Gold", "Platinum"};
            char text[96];
            if (every) {
                std::snprintf(text, sizeof(text), "%s", Tr(names[0]));
            } else {
                std::snprintf(text, sizeof(text), "%s %d/%d", Tr(names[int(grade)]), grade_have[int(grade)],
                              grade_total[int(grade)]);
            }
            const float cup = every ? 0.0f : fs * 0.95f;
            const float w = cup + (every ? 0.0f : 5.0f * s) + ImGui::CalcTextSize(text).x;
            if (!every) {
                SmallCup(dl, ImVec2(p.x + gx + cup * 0.5f, p.y + gy + fs * 0.5f), cup, color(Ink::Grade(grade), chosen ? 1.0f : 0.8f));
            }
            ImGui::SetCursorPos(ImVec2(gx + cup + (every ? 0.0f : 5.0f * s), gy));
            ImGui::TextColored(chosen ? white : muted, "%s", text);
            if (chosen) {
                dl->AddRectFilled(ImVec2(p.x + gx, p.y + gy + fs + 3.0f * s), ImVec2(p.x + gx + w, p.y + gy + fs + 5.0f * s),
                                  color(Ink::AccentHi), 1.0f * s);
            }
            gx += w + 22.0f * s;
        }
    }

    const ImVec2 list_min(p.x, p.y + header_h), list_max(p.x + size.x, p.y + size.y - footer_h);
    if (!list) {
        ImGui::SetCursorPos(ImVec2(24.0f * s, header_h + 12.0f * s));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f * s, 4.0f * s));
        if (ImGui::BeginChild("##trophy_notifications", ImVec2(size.x - 48.0f * s, list_max.y - list_min.y - 18.0f * s),
                              ImGuiChildFlags_AlwaysUseWindowPadding)) {
            NotificationsPage();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
    } else {
        // list, drawn by hand so it scrolls by pixels
        const float step = (kTrophyRow + kTrophyGap) * s;
        const float content_h = float(shown.size()) * step + 12.0f * s;
        const float max_scroll = std::max(0.0f, content_h - (list_max.y - list_min.y));
        trophy_scroll = std::clamp(trophy_scroll, 0.0f, max_scroll);
        // The right column: the state, then the rarity tier and the share of players.
        auto earned_text = [&](float percent) {
            char number[16];
            std::snprintf(number, sizeof(number), "%.1f", percent);
            std::string text = trophy_labels.rarity;
            const size_t at = text.find("{}");
            return at == std::string::npos ? text : text.replace(at, 2, number);
        };
        const std::string* tiers[4] = {&trophy_labels.common, &trophy_labels.rare, &trophy_labels.very_rare,
                                       &trophy_labels.ultra_rare};
        static const ImVec4 tier_inks[4] = {ImVec4(0.72f, 0.75f, 0.80f, 1.0f), ImVec4(0.95f, 0.78f, 0.38f, 1.0f),
                                            ImVec4(0.82f, 0.58f, 0.96f, 1.0f), ImVec4(0.42f, 0.80f, 1.0f, 1.0f)};
        float status_w = std::max(ImGui::CalcTextSize(trophy_labels.unlocked.c_str()).x,
                                  ImGui::CalcTextSize(trophy_labels.locked.c_str()).x);
        for (const TrophyRow* row : shown) {
            if (row->earned >= 0.0f) {
                status_w = std::max({status_w, ImGui::CalcTextSize(earned_text(row->earned).c_str()).x,
                                     ImGui::CalcTextSize(tiers[SpartanTrophyRarity::Tier(row->earned)]->c_str()).x});
            }
        }
        ImGui::PushClipRect(list_min, list_max, true);
        for (size_t k = 0; k < shown.size(); k++) {
            const TrophyRow& row = *shown[k];
            const float row_h = kTrophyRow * s;
            const float y = list_min.y + 8.0f * s + float(k) * step - trophy_scroll;
            if (y + row_h < list_min.y || y > list_max.y) {
                continue;
            }
            const bool got = TrophyUnlocked(row.id);
            const ImVec2 a(p.x + 16.0f * s, y), b(p.x + size.x - 16.0f * s - (max_scroll > 0.0f ? 8.0f * s : 0.0f), y + row_h);
            // Unlocked: framed in the grade's colour (the launcher's); the platinum also glows, its frame
            // shades from white to blue and a soft light crosses it now and then. Locked: plain.
            const ImVec4 grade_ink = Ink::Grade(row.grade);
            const bool platinum = got && row.grade == SpartanToast::Grade::Platinum;
            if (platinum) {
                for (int ring = 3; ring >= 1; --ring) {
                    const float e = float(ring) * 2.5f * s;
                    dl->AddRect(ImVec2(a.x - e, a.y - e), ImVec2(b.x + e, b.y + e), color(grade_ink, 0.10f / float(ring)),
                                8.0f * s + e, 0, 2.5f * s);
                }
            }
            dl->AddRectFilled(a, b, color(got ? Ink::Card : Ink::Panel), 8.0f * s);
            if (platinum) {
                const int first = dl->VtxBuffer.Size;
                dl->AddRectFilled(a, b, color(grade_ink, 0.10f), 8.0f * s);
                ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, first, dl->VtxBuffer.Size, a, b,
                                                              color(grade_ink, 1.0f), color(Ink::Card, 1.0f));
                // a soft light crossing the row every few seconds
                const float t = std::fmod(float(ImGui::GetTime()), 4.5f) / 1.2f;
                if (t < 1.0f) {
                    const float band = (b.x - a.x) * 0.22f, x = a.x - band + (b.x - a.x + 2.0f * band) * t;
                    dl->PushClipRect(a, b, true);
                    dl->AddRectFilledMultiColor(ImVec2(x - band * 0.5f, a.y), ImVec2(x, b.y), color(grade_ink, 0.0f),
                                                color(grade_ink, 0.12f), color(grade_ink, 0.12f), color(grade_ink, 0.0f));
                    dl->AddRectFilledMultiColor(ImVec2(x, a.y), ImVec2(x + band * 0.5f, b.y), color(grade_ink, 0.12f),
                                                color(grade_ink, 0.0f), color(grade_ink, 0.0f), color(grade_ink, 0.12f));
                    dl->PopClipRect();
                }
                const int border = dl->VtxBuffer.Size;
                dl->AddRect(a, b, color(grade_ink), 8.0f * s, 0, 2.0f * s);
                ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, border, dl->VtxBuffer.Size, a, b,
                                                              color(Ink::Rgb(0xf4, 0xfa, 0xff)), color(grade_ink));
            } else {
                dl->AddRect(a, b, color(got ? ImVec4(grade_ink.x, grade_ink.y, grade_ink.z, 0.85f) : Ink::Line), 8.0f * s, 0,
                            1.5f * s);
            }
            ImGui::PushClipRect(a, b, true);
            const float icon = 56.0f * s;
            const ImVec2 ip(a.x + 12.0f * s, a.y + (row_h - icon) * 0.5f);
            ImGui::SetCursorScreenPos(ip);
            if (row.tex) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, got ? 1.0f : 0.35f);
                ImGui::Image(row.tex->GetTexRef(), ImVec2(icon, icon));
                ImGui::PopStyleVar();
            } else {
                ImGui::Dummy(ImVec2(icon, icon));
            }
            dl->AddRect(ip, ImVec2(ip.x + icon, ip.y + icon), color(grade_ink, got ? 1.0f : 0.35f), 3.0f * s, 0, 1.5f * s);
            const float tx = ip.x + icon + 14.0f * s;
            const float status_x = b.x - 16.0f * s - status_w;
            ImGui::SetWindowFontScale(1.15f);
            // the grade's cup, then the name
            const float cup = fs * 1.05f;
            SmallCup(dl, ImVec2(tx + cup * 0.5f, a.y + 9.0f * s + fs * 0.6f), cup, color(grade_ink, got ? 1.0f : 0.55f));
            ImGui::SetCursorScreenPos(ImVec2(tx + cup + 7.0f * s, a.y + 9.0f * s));
            ImGui::PushTextWrapPos(status_x - 12.0f * s - p.x);
            ImGui::TextColored(got ? white : muted, "%s", row.name.c_str());
            ImGui::PopTextWrapPos();
            ImGui::SetWindowFontScale(1.0f);
            if (!row.detail.empty()) {
                ImGui::SetCursorScreenPos(ImVec2(tx, a.y + 9.0f * s + fs * 1.15f + 4.0f * s));
                ImGui::PushTextWrapPos(status_x - 12.0f * s - p.x);
                ImGui::TextColored(got ? Ink::Rgb(0xcf, 0xc6, 0xbc) : ImVec4(muted.x, muted.y, muted.z, 0.85f), "%s",
                                   row.detail.c_str());
                ImGui::PopTextWrapPos();
            }
            const std::string& state = got ? trophy_labels.unlocked : trophy_labels.locked;
            if (row.earned < 0.0f) {
                ImGui::SetCursorScreenPos(ImVec2(status_x, a.y + (row_h - fs) * 0.5f));
                ImGui::TextColored(got ? grade_ink : muted, "%s", state.c_str());
            } else {
                const float line = fs + 3.0f * s, top = a.y + (row_h - 3.0f * line + 3.0f * s) * 0.5f;
                ImGui::SetCursorScreenPos(ImVec2(status_x, top));
                ImGui::TextColored(got ? grade_ink : muted, "%s", state.c_str());
                const int tier = SpartanTrophyRarity::Tier(row.earned);
                ImGui::SetCursorScreenPos(ImVec2(status_x, top + line));
                ImGui::TextColored(tier_inks[tier], "%s", tiers[tier]->c_str());
                ImGui::SetCursorScreenPos(ImVec2(status_x, top + 2.0f * line));
                ImGui::TextColored(muted, "%s", earned_text(row.earned).c_str());
            }
            ImGui::PopClipRect();
        }
        ImGui::PopClipRect();
        if (max_scroll > 0.0f) { // scroll bar
            const float track = list_max.y - list_min.y - 16.0f * s;
            const float thumb = std::max(24.0f * s, track * (list_max.y - list_min.y) / content_h);
            const float top = list_min.y + 8.0f * s + (track - thumb) * (trophy_scroll / max_scroll);
            dl->AddRectFilled(ImVec2(p.x + size.x - 14.0f * s, top), ImVec2(p.x + size.x - 8.0f * s, top + thumb),
                              color(Ink::Raised), 3.0f * s);
        }
    }
    // footer: the keys
    const std::string order_keys = trophy_labels.hint_order + "   " + Tr("Square/G: grade");
    const std::string* hints[2] = {list ? &trophy_labels.hint : &trophy_labels.hint_notifications, &order_keys};
    for (int i = 0; i < (list ? 2 : 1); i++) {
        const float hint_w = ImGui::CalcTextSize(hints[i]->c_str()).x;
        ImGui::SetCursorPos(ImVec2(std::max(12.0f * s, (size.x - hint_w) * 0.5f),
                                   size.y - footer_h + 9.0f * s + float(i) * (fs + 4.0f * s)));
        ImGui::TextColored(muted, "%s", hints[i]->c_str());
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

s64 NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

bool NoticeActive() {
    return notice_until.load(std::memory_order_relaxed) > NowMs();
}

// Lower-right notes stack upward from `bottom`: the screenshot notice, then the shader notes.
template <typename Body>
void CornerNote(const char* id, float& bottom, float alpha, Body&& body) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float pad = 12.0f * base_scale;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - pad, bottom),
                            ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(alpha);
    ImGui::Begin(id, nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoFocusOnAppearing);
    body();
    bottom -= ImGui::GetWindowHeight() + pad / 2;
    ImGui::End();
}

void Notice(float& bottom) {
    std::string text;
    {
        std::scoped_lock lock{notice_mutex};
        text = notice_text;
    }
    CornerNote("##notice", bottom, 0.6f, [&] { UiText(text.c_str()); });
}

// Launcher: "Shader notes in the corner" (SPARTAN_SHADER_NOTES), off by default.
const bool shader_notes = [] {
    const char* env = std::getenv("SPARTAN_SHADER_NOTES");
    return env && *env != '0';
}();

bool PrefetchActive() {
    const u32 total = prefetch_total;
    return shader_notes && total && (prefetch_done < total || prefetch_until > NowMs());
}

void ShaderPrefetch(float& bottom) {
    const u32 done = prefetch_done, total = std::max(prefetch_total.load(), 1u);
    CornerNote("##shader_prefetch", bottom, 0.35f, [&] {
        if (done >= total) {
            ImGui::TextDisabled("Shader cache ready (%u)", total);
            return;
        }
        ImGui::TextDisabled("Reading shader cache %u%% (%u/%u)", u32(u64(done) * 100 / total), done,
                            total);
        ImGui::ProgressBar(float(done) / float(total),
                           ImVec2(ImGui::GetContentRegionAvail().x, 4.0f * base_scale), "");
    });
}

bool CompileNoteActive() {
    return shader_notes && (compile_pending || compile_until > NowMs());
}

void ShaderCompiles(float& bottom) {
    const u32 pending = compile_pending, count = compile_count;
    CornerNote("##shader_compiles", bottom, 0.35f, [&] {
        if (pending) {
            ImGui::TextDisabled("Compiling shaders (%u left)", pending);
        } else {
            ImGui::TextDisabled("Compiled %u new shader%s", count, count == 1 ? "" : "s");
        }
    });
}

} // namespace

void ShowNotice(const std::string& text, int milliseconds) {
    {
        std::scoped_lock lock{notice_mutex};
        notice_text = text;
    }
    notice_until.store(NowMs() + milliseconds, std::memory_order_relaxed);
}

void CycleEngineFps() {
    const int fps = SpartanSettings::CycleEngineFps();
    ShowNotice(fps ? std::string(Tr("Engine frame rate: ")) + std::to_string(fps) + Tr(" FPS")
                   : std::string(Tr("Engine frame rate: 60 FPS only (no frame rate patch for this game build)")));
}

// The ImGui context's options, style, fonts and language (Init; also tools/overlay_preview.cpp).
void ConfigureContext() {
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // window positions are not kept
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    io.BackendPlatformName = "spartanport";

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 5.0f;
    style.GrabRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.WindowPadding = ImVec2(20.0f, 16.0f);
    style.FramePadding = ImVec2(10.0f, 5.0f);
    style.ItemSpacing = ImVec2(10.0f, 7.0f);
    style.ScrollbarSize = 10.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.SeparatorTextBorderSize = 1.0f;
    style.SeparatorTextPadding = ImVec2(12.0f, 4.0f);
    // The launcher's colours (Ink): warm blacks, cream text, the red of the launcher's buttons.
    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = Ink::Text;
    c[ImGuiCol_TextDisabled] = Ink::Muted;
    c[ImGuiCol_WindowBg] = ImVec4(Ink::Bg.x, Ink::Bg.y, Ink::Bg.z, 0.97f);
    c[ImGuiCol_ChildBg] = ImVec4(Ink::Panel.x, Ink::Panel.y, Ink::Panel.z, 0.9f);
    c[ImGuiCol_PopupBg] = Ink::Panel;
    c[ImGuiCol_Border] = Ink::Line;
    c[ImGuiCol_FrameBg] = Ink::Card;
    c[ImGuiCol_FrameBgHovered] = Ink::Line;
    c[ImGuiCol_FrameBgActive] = Ink::Raised;
    c[ImGuiCol_TitleBg] = c[ImGuiCol_TitleBgActive] = c[ImGuiCol_TitleBgCollapsed] = Ink::Panel;
    c[ImGuiCol_CheckMark] = Ink::Rgb(0xf4, 0xec, 0xe0); // the launcher's check: light on red
    c[ImGuiCol_CheckboxSelectedBg] = Ink::AccentHi;
    c[ImGuiCol_TextLink] = Ink::Gold;
    c[ImGuiCol_SliderGrab] = Ink::Accent;
    c[ImGuiCol_SliderGrabActive] = Ink::AccentHi;
    c[ImGuiCol_Button] = Ink::Line;
    c[ImGuiCol_ButtonHovered] = Ink::Accent;
    c[ImGuiCol_ButtonActive] = Ink::AccentHi;
    c[ImGuiCol_Header] = Ink::AccentDeep;
    c[ImGuiCol_HeaderHovered] = ImVec4(Ink::Accent.x, Ink::Accent.y, Ink::Accent.z, 0.55f);
    c[ImGuiCol_HeaderActive] = Ink::Accent;
    c[ImGuiCol_Separator] = c[ImGuiCol_SeparatorHovered] = c[ImGuiCol_SeparatorActive] = Ink::Line;
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = Ink::Line;
    c[ImGuiCol_ScrollbarGrabHovered] = Ink::Raised;
    c[ImGuiCol_ScrollbarGrabActive] = Ink::Accent;
    c[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ResizeGripHovered] = Ink::Line;
    c[ImGuiCol_ResizeGripActive] = Ink::Accent;
    c[ImGuiCol_Tab] = c[ImGuiCol_TabHovered] = c[ImGuiCol_TabSelected] = Ink::Card;
    c[ImGuiCol_TabSelectedOverline] = Ink::AccentHi;
    c[ImGuiCol_TableBorderLight] = c[ImGuiCol_TableBorderStrong] = Ink::Line;
    c[ImGuiCol_NavCursor] = Ink::AccentHi;
    c[ImGuiCol_TextSelectedBg] = ImVec4(Ink::Accent.x, Ink::Accent.y, Ink::Accent.z, 0.45f);

    ImFontConfig font_config;
    font_config.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(spartanport_font_ttf),
                                   int(spartanport_font_ttf_end - spartanport_font_ttf), 18.0f, &font_config);
    const char* ui_language = std::getenv("SPARTAN_UI_LANGUAGE");
    // The language chosen in the game, else the launcher's.
    const int chosen = SpartanSettings::Get().ui_language;
    const int wanted = chosen >= 0 ? chosen : SpartanUi::FindLanguage(ui_language ? ui_language : "");
    const std::string_view wanted_code = SpartanUi::Languages[wanted].code;
    if (!SpartanUi::ConfigureChineseFont(io.Fonts, wanted_code, SpartanUi::FindChineseFont()) &&
        SpartanUi::WantsChinese(wanted_code)) {
        std::printf("Overlay: no usable system CJK font; keeping English menu text\n");
    }
    SpartanUi::SetLanguage(wanted);
    if (!SpartanUi::chinese) {
        ImFontConfig title_config;
        title_config.FontDataOwnedByAtlas = false;
        title_font = io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(spartanport_title_ttf),
                                                    int(spartanport_title_ttf_end - spartanport_title_ttf), kTitleSize,
                                                    &title_config);
        if (title_font) { // what Cinzel lacks (Cyrillic, some accents) from DejaVu Sans
            ImFontConfig merge;
            merge.MergeMode = true;
            merge.FontDataOwnedByAtlas = false;
            io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(spartanport_font_ttf),
                                           int(spartanport_font_ttf_end - spartanport_font_ttf), kTitleSize, &merge);
        }
    }
}

// The trophy notifications' fonts, pictures and sounds, once the context has its renderer.
void StartNotifications() {
    SpartanToast::Start(ImGui::GetIO().Fonts, &TrophyIcon, &TrophyEntry);
    ApplyLanguage(SpartanUi::language);
}

void Init(const Vulkan::Instance& instance, vk::Format format, u32 image_count) {
    std::scoped_lock lock{imgui_mutex};
    if (initialized) {
        return;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ConfigureContext();

    const vk::Instance vk_instance = instance.GetInstance();
    ImGui_ImplVulkan_LoadFunctions(
        instance.ApiVersion(),
        [](const char* name, void* user) {
            return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(
                *static_cast<const vk::Instance*>(user), name);
        },
        const_cast<vk::Instance*>(&vk_instance));

    const VkFormat color_format = static_cast<VkFormat>(format);
    ImGui_ImplVulkan_InitInfo info{};
    info.ApiVersion = instance.ApiVersion();
    info.Instance = vk_instance;
    info.PhysicalDevice = instance.GetPhysicalDevice();
    info.Device = instance.GetDevice();
    info.QueueFamily = instance.GetGraphicsQueueFamilyIndex();
    info.Queue = instance.GetGraphicsQueue();
    // Font atlas plus the save list pictures (up to 33, and a closed list's until freed), the trophy
    // icons (35) and the notifications' own pictures (a badge and up to 5 animations).
    info.DescriptorPoolSize = 160;
    info.MinImageCount = std::max(image_count, 2u);
    info.ImageCount = std::max(image_count, 2u);
    info.UseDynamicRendering = true;
    info.PipelineInfoMain.PipelineRenderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &color_format,
    };
    if (!ImGui_ImplVulkan_Init(&info)) {
        std::printf("Overlay: ImGui Vulkan backend init failed\n");
        ImGui::DestroyContext();
        return;
    }
    overlay_instance = &instance;
    StartNotifications();
    initialized = true;
    std::printf("Overlay: menu ready (Insert or R3+L2)\n");
}

void UpdateTextInput(SDL_Window* window) {
    bool want = false;
    {
        std::scoped_lock lock{imgui_mutex};
        want = initialized && menu_open && ImGui::GetIO().WantTextInput;
    }
    if (want != SDL_TextInputActive(window)) {
        if (want) {
            SDL_StartTextInput(window);
        } else {
            SDL_StopTextInput(window);
        }
    }
}

// imgui_mutex held. The backend frees a texture some frames after its last use.
void RetireChoiceTextures() {
    for (ImTextureData* tex : choice_textures) {
        if (tex) {
            tex->WantDestroyNextFrame = true;
            choice_retired.push_back(tex);
        }
    }
    choice_textures.clear();
    choice_pictures.clear();
    choice_retiring = !choice_retired.empty();
}

// imgui_mutex held, before NewFrame: new pictures become textures, destroyed ones are freed.
void UpdateChoiceTextures() {
    if (!choice_pictures.empty()) {
        choice_textures.assign(choice_pictures.size(), nullptr);
        for (size_t i = 0; i < choice_pictures.size(); ++i) {
            const SpartanPng::Image& picture = choice_pictures[i];
            if (picture.rgba.empty()) {
                continue;
            }
            ImTextureData* tex = IM_NEW(ImTextureData)();
            tex->Create(ImTextureFormat_RGBA32, picture.width, picture.height);
            std::memcpy(tex->GetPixels(), picture.rgba.data(), picture.rgba.size());
            ImGui::RegisterUserTexture(tex);
            choice_textures[i] = tex;
        }
        choice_pictures.clear();
    }
    std::erase_if(choice_retired, [](ImTextureData* tex) {
        if (tex->Status != ImTextureStatus_Destroyed) {
            return false;
        }
        ImGui::UnregisterUserTexture(tex);
        IM_DELETE(tex);
        return true;
    });
    choice_retiring = !choice_retired.empty();
}

// The save list takes the keys and buttons that move and confirm; the rest stays with the game
// (held neutral by CapturesInput).
bool HandleChoiceEvent(const SDL_Event& event) {
    int move = 0, finish = 0; // finish: 1 accept, 2 cancel
    if (event.type == SDL_EVENT_KEY_DOWN) {
        const SDL_Keycode key = event.key.key;
        move = key == SDLK_UP || key == SDLK_W ? -1 : key == SDLK_DOWN || key == SDLK_S ? 1 : 0;
        finish = key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE ? 1
                 : key == SDLK_ESCAPE || key == SDLK_BACKSPACE                   ? 2
                                                                                 : 0;
    } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        const u8 button = event.gbutton.button;
        move = button == SDL_GAMEPAD_BUTTON_DPAD_UP ? -1 : button == SDL_GAMEPAD_BUTTON_DPAD_DOWN ? 1 : 0;
        const bool swapped = SpartanSettings::Get().menu_confirm_swap.load(std::memory_order_relaxed) != 0;
        const u8 confirm_btn = swapped ? SDL_GAMEPAD_BUTTON_EAST : SDL_GAMEPAD_BUTTON_SOUTH;
        const u8 cancel_btn = swapped ? SDL_GAMEPAD_BUTTON_SOUTH : SDL_GAMEPAD_BUTTON_EAST;
        finish = button == confirm_btn ? 1 : button == cancel_btn ? 2 : 0;
    } else {
        return event.type == SDL_EVENT_KEY_UP || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP;
    }
    const int count = int(choice_items.size());
    if (move && count) {
        choice_index = (choice_index + move + count) % count;
    }
    if (finish) {
        choice_result = finish == 1 && count ? choice_index : -2;
        choice_active = false;
        RetireChoiceTextures();
    }
    return true;
}

bool HandleEvent(const SDL_Event& event) {
    std::scoped_lock lock{imgui_mutex};
    if (!initialized) {
        return false;
    }
    if (choice_active && !menu_open) {
        return HandleChoiceEvent(event);
    }
    if (trophy_view && !menu_open) {
        if (trophy_page == 0) {
            return HandleTrophyEvent(event);
        }
        if (HandleTrophyOptionsEvent(event)) {
            return true;
        }
    }
    ImGuiIO& io = ImGui::GetIO();
    const bool is_open = menu_open || (trophy_view && trophy_page == 1);
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        const bool down = event.type == SDL_EVENT_KEY_DOWN;
        static const bool capture_key = [] {
            const char* env = std::getenv("SPARTAN_FRAME_CAPTURE_KEY");
            return env && *env == '1';
        }();
        if (event.key.key == SDLK_F9) {
            if (down && !event.key.repeat) {
                CycleEngineFps();
            }
            return true;
        }
        if (event.key.key == SDLK_F12) {
            if (down && !event.key.repeat) {
                VideoCore::RequestScreenshot(VideoCore::ScreenshotRequest::GameOnly);
            }
            return true;
        }
        if (down && !event.key.repeat && capture_key && event.key.key == SDLK_F11) {
            Vulkan::FrameCapture::Request();
            return true;
        }
        if (down && !event.key.repeat && event.key.key == SDLK_F8 && !is_open && !text_entry_active && trophy_ready) {
            SetTrophyView(true); // the trophy list
            return true;
        }
        if (down && !event.key.repeat &&
            (event.key.key == SDLK_INSERT || (is_open && event.key.key == SDLK_ESCAPE))) {
            SetOpen(event.key.key == SDLK_INSERT ? !is_open : false);
            return true;
        }
        if (!is_open) {
            return false;
        }
        io.AddKeyEvent(ImGuiMod_Ctrl, (event.key.mod & SDL_KMOD_CTRL) != 0);
        io.AddKeyEvent(ImGuiMod_Shift, (event.key.mod & SDL_KMOD_SHIFT) != 0);
        io.AddKeyEvent(ImGuiMod_Alt, (event.key.mod & SDL_KMOD_ALT) != 0);
        if (const ImGuiKey key = KeyFromSdl(event.key.key); key != ImGuiKey_None) {
            io.AddKeyEvent(key, down);
        }
        return true;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
        const bool down = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        const u8 button = event.gbutton.button;
        // R3+L2 opens the menu: L3+R3 is a game action.
        if (button == SDL_GAMEPAD_BUTTON_RIGHT_STICK) {
            r3_down = down;
            if (down && l2_down) {
                SetOpen(!is_open);
                return true;
            }
            if (down && r2_down && !is_open && trophy_ready) { // R3+R2: the trophy list
                SetTrophyView(true);
                return true;
            }
        }
        if (!is_open) {
            return false;
        }
        if (const ImGuiKey key = KeyFromGamepad(button); key != ImGuiKey_None) {
            io.AddKeyEvent(key, down);
        }
        return true;
    }
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
        if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
            const bool down = event.gaxis.value > 16000;
            if (down && !l2_down && r3_down) {
                l2_down = true;
                SetOpen(!is_open);
                return true;
            }
            l2_down = down;
        } else if (event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
            const bool down = event.gaxis.value > 16000;
            if (down && !r2_down && r3_down && !is_open && trophy_ready) {
                r2_down = true;
                SetTrophyView(true);
                return true;
            }
            r2_down = down;
        }
        return false;
    }
    case SDL_EVENT_TEXT_INPUT: {
        // Typed characters (Ctrl+click on a slider, a text field): key events alone erase but
        // do not type. SDL sends them while text input is on (UpdateTextInput).
        if (!is_open) {
            return false;
        }
        io.AddInputCharactersUTF8(event.text.text);
        return true;
    }
    case SDL_EVENT_MOUSE_MOTION: {
        if (!is_open) {
            return false;
        }
        const float density = PixelDensity(event.motion.windowID);
        io.AddMousePosEvent(event.motion.x * density, event.motion.y * density);
        return true;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (!is_open) {
            return false;
        }
        const int button = event.button.button == SDL_BUTTON_LEFT    ? 0
                           : event.button.button == SDL_BUTTON_RIGHT  ? 1
                           : event.button.button == SDL_BUTTON_MIDDLE ? 2
                                                                      : -1;
        if (button >= 0) {
            io.AddMouseButtonEvent(button, event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
        }
        return true;
    }
    case SDL_EVENT_MOUSE_WHEEL:
        if (!is_open) {
            return false;
        }
        io.AddMouseWheelEvent(event.wheel.x, event.wheel.y);
        return true;
    default:
        return false;
    }
}

// After "Apply and restart", the new options are kept once the game has shown about ten
// seconds of frames after loading (run.py restores the previous ones otherwise).
void ConfirmRestart() {
    static u32 frames = 0;
    auto& s = SpartanSettings::Get();
    if (!s.restart_unconfirmed || loading || ++frames < 600) {
        return;
    }
    s.restart_unconfirmed = false;
    SpartanSettings::Save();
    std::puts("Settings: graphics options confirmed after restart");
}

void SetBackgroundLoading(u32 done, u32 total) {
    background_done = done;
    background_total = total;
}

void SetShaderPrefetch(u32 done, u32 total) {
    if (total && done >= total) {
        prefetch_until = NowMs() + 2000;
    }
    prefetch_done = done;
    prefetch_total = total;
}

void NoteShaderCompiles(u32 pending, u32 started) {
    const s64 now = NowMs();
    if (!compile_pending && compile_until <= now) {
        compile_count = 0; // a new burst
    }
    compile_count += started;
    compile_pending = pending;
    compile_until = now + 2000;
}

void SetLoading(bool active, u32 done, u32 total) {
    loading_done = done;
    loading_total = total;
    loading = active;
}

bool Visible() {
    const auto& s = SpartanSettings::Get();
    return initialized && (loading || background_total || PrefetchActive() || CompileNoteActive() ||
                           menu_open || text_entry_active || choice_active || choice_retiring ||
                           toast_active || SpartanToast::Busy() || trophy_view || s.show_fps || NoticeActive() ||
                           (s.actor_watch_supported && s.enemy_health_bar));
}

bool CapturesInput() {
    return menu_open || text_entry_active || choice_active || trophy_view;
}

bool GamePaused() {
    return SpartanSettings::Get().pause_game && (menu_open || trophy_view) && !loading;
}

void WaitWhilePaused() {
    // The game's own thread stops here, between two frames: its clock goes on, and the game takes
    // the gap as one long frame, as after a load.
    while (GamePaused()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void BeginChoice(const std::string& title, const std::vector<std::string>& items,
                 std::vector<SpartanPng::Image> pictures, int focus,
                 const std::string& preview_label, SpartanPng::Image preview) {
    std::scoped_lock lock{imgui_mutex};
    RetireChoiceTextures();
    choice_pictures = std::move(pictures);
    choice_pictures.resize(items.size());
    choice_preview_label = preview_label;
    if (!preview_label.empty()) choice_pictures.push_back(std::move(preview));
    choice_title = title;
    choice_items = items;
    choice_index = focus >= 0 && focus < int(items.size()) ? focus : 0;
    choice_result = -1;
    choice_active = true;
}

int PollChoice() {
    return choice_result;
}

void SetTextEntry(bool active, const std::string& prompt, const std::string& text) {
    std::scoped_lock lock{imgui_mutex};
    text_entry_active = active;
    text_entry_prompt = prompt;
    text_entry_text = text;
}

// Every window of a frame, between NewFrame and Render (Render; also tools/overlay_preview.cpp).
void DrawWindows() {
    if (loading) {
        LoadingScreen();
    }
    if (menu_open) {
        Menu();
    }
    if (SpartanSettings::Get().show_fps && !menu_open) {
        FpsCounter();
    }
    float corner = ImGui::GetMainViewport()->WorkPos.y + ImGui::GetMainViewport()->WorkSize.y -
                   12.0f * base_scale;
    if (NoticeActive()) {
        Notice(corner);
    }
    if (PrefetchActive() && !loading && !menu_open) {
        ShaderPrefetch(corner);
    }
    if (CompileNoteActive() && !loading && !menu_open) {
        ShaderCompiles(corner);
    }
    if (!loading && !menu_open) {
        HealthDisplay();
    }
    if (background_total && !loading && !menu_open) {
        BackgroundLoading();
    }
    if (text_entry_active) {
        TextEntryBox();
    }
    if (choice_active && !menu_open) {
        ChoiceBox();
    }
    if (trophy_view && !menu_open) {
        TrophyView();
    }
    if (toast_active) {
        ToastBox();
    }
    if (SpartanToast::Busy()) {
        SpartanToast::Draw(base_scale, NoticeTop(0.0f, 10.0f * base_scale) - ImGui::GetMainViewport()->WorkPos.y);
    }
}

void Render(vk::CommandBuffer cmdbuf, vk::ImageView view, vk::Extent2D extent) {
    // Present interval for the FPS readout (measured also while nothing is drawn).
    const auto now = std::chrono::steady_clock::now();
    const float ms = std::chrono::duration<float, std::milli>(now - last_present).count();
    last_present = now;
    if (ms > 0.0f && ms < 1000.0f) {
        frame_ms_avg = frame_ms_avg == 0.0f ? ms : frame_ms_avg * 0.95f + ms * 0.05f;
    }
    ConfirmRestart();
    if (!Visible()) {
        return;
    }
    std::scoped_lock lock{imgui_mutex};
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(float(extent.width), float(extent.height));
    io.DeltaTime = ms > 0.0f && ms < 1000.0f ? ms / 1000.0f : 1.0f / 60.0f;
    // UI scale follows the display height (1080p = 1).
    const float scale = std::max(float(extent.height) / 1080.0f, 0.75f);
    if (std::abs(scale - base_scale) > 0.01f) {
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(scale / base_scale);
        style.FontScaleMain = scale;
        base_scale = scale;
    }

    UpdateChoiceTextures();
    UpdateToastTextures();
    UpdateTrophyTextures();
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
    DrawWindows();
    ImGui::Render();

    const vk::RenderingAttachmentInfo attachment{
        .imageView = view,
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eLoad,
        .storeOp = vk::AttachmentStoreOp::eStore,
    };
    cmdbuf.beginRendering(vk::RenderingInfo{
        .renderArea = {{0, 0}, extent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachment,
    });
    {
        // Font atlas uploads submit to the graphics queue themselves.
        std::scoped_lock submit_lock{Vulkan::Scheduler::submit_mutex};
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdbuf);
    }
    cmdbuf.endRendering();
}

} // namespace SpartanOverlay

#ifndef SPARTAN_OVERLAY_PREVIEW // the preview links the library's
// Called by the runtime (src/runtime_services.c) when the game unlocks a trophy. id: the trophy's
// (-1: a plain notice), title: its name, detail: its description (may be empty), png: the trophy's
// icon (the PNG bytes from trophy00.trp) or null for the drawn medal. The theme chosen in the menu
// shows it; the Original notice is drawn here.
extern "C" void spartanportgpu_toast_trophy(int id, const char* title, const char* detail, const unsigned char* png,
                                     uint64_t png_size) {
    if (!title || !title[0]) {
        return;
    }
    using namespace SpartanOverlay;
    if (id >= 0 && SpartanToast::GetOptions().theme != SpartanToast::Theme::Original) {
        SpartanToast::Enqueue(id, false);
        return;
    }
    // The name and description in the language chosen in the game, when it has them.
    const SpartanToast::Entry entry = id >= 0 ? TrophyEntry(id) : SpartanToast::Entry{};
    if (!entry.name.empty()) {
        title = entry.name.c_str();
        detail = entry.detail.c_str();
    }
    SpartanPng::Image image;
    if (png && png_size) {
        SpartanPng::DecodeRgba(png, size_t(png_size), image); // empty on failure: the medal is drawn
    }
    PushNotice(title, detail ? detail : "", std::move(image));
    if (id >= 0) {
        SpartanToast::PlaySound(id);
    }
}

extern "C" void spartanportgpu_toast_icon(const char* text, const unsigned char* png, uint64_t png_size) {
    spartanportgpu_toast_trophy(-1, text, nullptr, png, png_size);
}

extern "C" void spartanportgpu_toast(const char* text) {
    spartanportgpu_toast_trophy(-1, text, nullptr, nullptr, 0);
}

// Called once by the runtime (src/runtime_services.c) with the trophy texts: the in-game list
// (F8 / R3+R2) and the notice labels. The icons are decoded on a worker thread; unlocked is read live.
extern "C" void spartanportgpu_trophy_setup(const SpartanGpuTrophyLabels* labels, const SpartanGpuTrophyInfo* items, int count,
                                     const volatile uint32_t* unlocked) {
    using namespace SpartanOverlay;
    if (!items || count <= 0 || trophy_started.exchange(true)) {
        return;
    }
    struct Item {
        int id;
        std::string name, detail;
        std::vector<unsigned char> png;
    };
    std::vector<Item> list;
    for (int i = 0; i < count; i++) {
        list.push_back({items[i].id, items[i].name ? items[i].name : "", items[i].detail ? items[i].detail : "",
                        items[i].png && items[i].png_size
                            ? std::vector<unsigned char>(items[i].png, items[i].png + items[i].png_size)
                            : std::vector<unsigned char>()});
    }
    TrophyLabels copy;
    if (labels) {
        auto take = [](std::string& to, const char* from) {
            if (from && from[0]) {
                to = from;
            }
        };
        take(copy.heading, labels->heading);
        take(copy.notice, labels->notice);
        take(copy.unlocked, labels->status_unlocked);
        take(copy.locked, labels->status_locked);
        take(copy.all, labels->filter_all);
        take(copy.only_unlocked, labels->filter_unlocked);
        take(copy.only_locked, labels->filter_locked);
        take(copy.counter, labels->counter);
        take(copy.hint, labels->hint);
        take(copy.notifications, labels->notifications);
        take(copy.order_list, labels->order_list);
        take(copy.order_common, labels->order_common);
        take(copy.order_rare, labels->order_rare);
        take(copy.rarity, labels->rarity);
        take(copy.common, labels->common);
        take(copy.rare, labels->rare);
        take(copy.very_rare, labels->very_rare);
        take(copy.ultra_rare, labels->ultra_rare);
        take(copy.hint_order, labels->hint_order);
        take(copy.hint_notifications, labels->hint_notifications);
    }
    std::thread([list = std::move(list), copy = std::move(copy), unlocked]() mutable {
        std::vector<TrophyRow> rows;
        std::vector<SpartanToast::Entry> entries;
        for (Item& item : list) {
            TrophyRow row;
            row.id = item.id;
            row.name = std::move(item.name);
            row.detail = std::move(item.detail);
            row.earned = SpartanTrophyRarity::Earned(row.id, int(list.size()));
            if (!item.png.empty()) {
                SpartanPng::DecodeRgba(item.png.data(), item.png.size(), row.image); // empty on failure
            }
            row.grade = SpartanToast::GradeFromIcon(row.image.rgba.data(), row.image.width, row.image.height);
            if (row.id >= 0 && row.id < 128) {
                if (int(entries.size()) <= row.id) {
                    entries.resize(size_t(row.id) + 1);
                }
                entries[size_t(row.id)] = {row.name, row.detail, row.grade};
            }
            rows.push_back(std::move(row));
        }
        // The language chosen in the game, when it is not the launcher's (whose texts these are).
        const char* env = std::getenv("SPARTAN_UI_LANGUAGE");
        const int language = SpartanUi::language;
        if (language != SpartanUi::FindLanguage(env ? env : "")) {
            const auto texts = ReadTrophyTexts(SpartanUi::Languages[language].code);
            if (texts.count) {
                TakeTrophyTexts(texts, copy, rows, entries);
            }
        }
        {
            std::scoped_lock lock{trophy_entry_mutex};
            trophy_entries = std::move(entries);
        }
        std::scoped_lock lock{imgui_mutex};
        trophy_rows = std::move(rows);
        trophy_labels = std::move(copy);
        trophy_flags = unlocked;
        trophy_ready = true;
    }).detach();
}
#endif // SPARTAN_OVERLAY_PREVIEW
