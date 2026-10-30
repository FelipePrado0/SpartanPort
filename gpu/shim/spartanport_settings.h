// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: user settings changed at run time from the in-game menu (spartanport_overlay.h) and kept
// in spartanport.ini (SPARTAN_CONFIG overrides the path). Environment variables override the file at
// start. Readers load the atomics every frame; writers are the menu and Load().

#pragma once

#include "../../src/cheat_hook.h"

#include <atomic>
#include <cstdint>
#include <utility>
#include <vector>
#include <string_view>

namespace SpartanSettings {

/// Speaker layout the game's 7.1 audio is mixed down to (src/runtime_audio.c): the playback
/// device's own, stereo, 5.1 or 7.1. spartanport.ini and SPARTAN_AUDIO_CHANNELS name them as below.
enum AudioChannels : int { AudioChannelsAuto = 0, AudioStereo, Audio51, Audio71, AudioChannelsCount };
inline constexpr std::string_view AudioChannelKeys[AudioChannelsCount] = {"auto", "stereo", "5.1", "7.1"};
/// A layout key (also "2.0", "6" and "8"); anything else is automatic.
inline int ParseAudioChannels(std::string_view value) {
    for (int c = 0; c < AudioChannelsCount; ++c) {
        if (value == AudioChannelKeys[c]) return c;
    }
    if (value == "2.0" || value == "2") return AudioStereo;
    if (value == "6") return Audio51;
    if (value == "8") return Audio71;
    return AudioChannelsAuto;
}
inline constexpr int MasterVolumeMin = 0, MasterVolumeMax = 200, MasterVolumeDefault = 100;

enum Upscaler : int { UpscalerOff = 0, UpscalerFsr3 = 1, UpscalerFsr4 = 2, UpscalerFsr411 = 3,
                      UpscalerTaa = 4, UpscalerCount };
/// FSR 4 v07 or FSR 4.1.1: the same inputs, settings and placement in the frame.
inline bool IsFsr4(int upscaler) {
    return upscaler == UpscalerFsr4 || upscaler == UpscalerFsr411;
}
// Ultra Quality (FSR 1's 1.3x) is appended: saved preset numbers keep their meaning.
enum Preset : int { NativeAA = 0, Quality, Balanced, Performance, UltraPerformance, UltraQuality, PresetCount };
enum DebugView : int { DebugNone = 0, DebugReactive = 1, DebugMotion = 2, DebugViewCount };

/// Live output resolutions: the upscaler's output and the UI host targets.
inline constexpr int OutputWidths[] = {1280, 1920, 2560, 3840};
inline constexpr int OutputHeights[] = {720, 1080, 1440, 2160};
inline constexpr int OutputCount = 4;
inline constexpr int OutputDefault = 1; ///< 1920x1080, the game's own size

/// Texture Quality steps: their labels, and the extra mip LOD bias each one adds.
inline constexpr int TextureQualityCount = 4;
inline constexpr const char* TextureQualityNames[TextureQualityCount] = {"High", "Medium", "Low", "Very Low"};
inline constexpr const char* TextureQualityNamesPt[TextureQualityCount] = {"Alto", "Médio", "Baixo", "Muito Baixo"};
inline float TextureLodBias(int quality) {
    return quality > 0 && quality < TextureQualityCount ? float(quality) : 0.0f;
}

struct Values {
    std::atomic<int> upscaler{UpscalerOff};
    std::atomic<int> preset{NativeAA};
    /// Frame generation (frame_generation_policy.h Mode): off, fsr in spartanport.ini.
    std::atomic<int> frame_generation{0};
    /// Legacy ini key, normalized to one generated frame per real frame (2x).
    std::atomic<int> frame_generation_frames{1};
    std::atomic<bool> sharpen{true};
    std::atomic<float> sharpness{0.3f};
    std::atomic<bool> jitter{true};
    std::atomic<bool> reactive{false};
    std::atomic<bool> object_motion{true};
    std::atomic<float> reactive_scale{1.0f};
    std::atomic<float> reactive_threshold{0.2f};
    std::atomic<float> reactive_max{0.9f};
    std::atomic<int> debug_view{DebugNone};
    std::atomic<bool> show_fps{false};
    /// Performance overlay (spartanport_perf_stats.h): items shown, font size and background opacity
    /// in percent, corner (0 top left, 1 top right, 2 bottom left, 3 bottom right), layout
    /// (0 one line, 1 one item per line). menu_tab: the menu's last tab.
    std::atomic<unsigned> overlay_items{7};
    std::atomic<int> overlay_scale{100}, overlay_corner{1}, overlay_opacity{50}, overlay_layout{0};
    std::atomic<int> menu_tab{0};
    /// The menu's and the trophies' language chosen in the game (SpartanUi::Languages index), -1 for
    /// the launcher's (SPARTAN_UI_LANGUAGE).
    std::atomic<int> ui_language{-1};
    /// Whether the game stops while the menu or the trophy list is open (SpartanOverlay::GamePaused).
    std::atomic<bool> pause_game{true};
    std::atomic<float> red_orb_multiplier{1.0f};
    std::atomic<bool> red_orbs_supported{false};
    /// Damage and orb pickup multipliers (src/damage_hook.h, src/orb_hook.h), 0.1x to 100x.
    /// The game's code reads these floats directly.
    std::atomic<float> damage_dealt{1.0f}, damage_taken{1.0f};
    std::atomic<float> green_orb_multiplier{1.0f}, blue_orb_multiplier{1.0f}, gold_orb_multiplier{1.0f};
    std::atomic<bool> damage_supported{false}, orb_pickup_supported{false};
    std::atomic<bool> cheats[SPARTAN_CHEAT_COUNT]{};
    std::atomic<uint32_t> cheats_supported{0};
    /// Damage hook (src/actor_hook.h): enemy health bar, off by default.
    std::atomic<bool> enemy_health_bar{false};
    std::atomic<bool> actor_watch_supported{false};
    /// QTE prompts answered in the game's own checks (src/qte_hook.h), off by default. Live.
    std::atomic<bool> auto_qte{false};
    std::atomic<bool> qte_supported{false};
    /// Options > Graphics (src/graphics_hook.h): the game's bloom and motion blur, on by default.
    /// The hooks read these bytes every frame. graphics_supported: bit per SPARTAN_GRAPHICS_* id.
    std::atomic<bool> bloom{true};
    /// The one motion blur setting (ini key motion_blur, SPARTAN_MOTION_BLUR): read by the game hook
    /// and by the renderer, which also skips the blur's draws when it is off (spartanport_motion_blur.h).
    std::atomic<bool> motion_blur{true};
    std::atomic<bool> shadows{true};
    std::atomic<uint32_t> graphics_supported{0};
    /// Options > Graphics > Texture Quality: 0 High (the game's own) .. TextureQualityCount - 1.
    /// Each step reads mipmapped textures one mip level smaller (texture_cache.cpp). Live.
    std::atomic<int> texture_quality{0};
    // Graphics (spartanport_graphics.h). Live: applied on the next frame.
    std::atomic<int> display_mode{0};
    std::atomic<bool> vsync{true};
    std::atomic<int> monitor{-1};
    std::atomic<bool> hdr{false};
    int startup_monitor = -1;
    bool startup_hdr = false;
    std::vector<int> monitors; ///< SDL indices, populated before the guest starts
    std::atomic<int> fps_limit{0};
    std::atomic<bool> async_shaders{true};
    std::atomic<bool> fsr1{false};
    std::atomic<bool> rcas{true};
    std::atomic<int> rcas_strength{75};
    /// The game's depth of field (spartanport_depth_of_field.h); off keeps the whole picture sharp. Live.
    std::atomic<bool> depth_of_field{true};
    /// Set by the presenter: sharpening ran on the last frame (it needs FSR 1 when the game's
    /// image is smaller than the window).
    std::atomic<bool> rcas_applied{false};
    /// Set by the presenter: the game's image and the window it is shown in, in pixels.
    std::atomic<int> image_width{0}, image_height{0}, window_width{0}, window_height{0};
    std::atomic<int> frames_queued{1};
    /// Controller vibration strength in percent; 0 turns it off (runtime_pad.c). Live.
    std::atomic<int> vibration{100};
    /// Menu confirmation button: 0 Cross confirms, Circle cancels (default); 1 Circle confirms, Cross cancels. Live.
    std::atomic<int> menu_confirm_swap{0};
    bool menu_confirm_swap_explicit{false};
    std::atomic<int> glyph_style{0}; ///< PlayStation, Xbox, Nintendo, keyboard, legacy automatic
    int startup_glyph_style = 0;
    /// Master volume of the game's audio in percent, 0 to 200: above 100 boosts the mix behind a
    /// soft limiter (src/runtime_audio.c). Live.
    std::atomic<int> master_volume{MasterVolumeDefault};
    /// Speaker layout of the game's 7.1 audio (AudioChannels). Read when the game opens its
    /// audio ports, so a change applies from the next start.
    std::atomic<int> audio_channels{AudioChannelsAuto};
    /// Mouse movement in px that counts as one step of a stick prompt (runtime_pad.c). Live.
    std::atomic<int> mouse_prompt_distance{48};
    /// Keep the mouse in the game window while the game has it (window.cpp). Live.
    std::atomic<bool> mouse_lock{true};
    /// L2 aim with the mouse (runtime_pad.c): 0 relative (follows movement), 1 absolute. Live.
    std::atomic<int> mouse_aim_style{0};
    /// L2 aim sensitivity in percent, 10 to 200: lower needs more mouse for the same stick. Live.
    std::atomic<int> mouse_aim_sensitivity{100};
    // Startup: run.py turns them into patches and environment variables; "Apply and restart".
    std::atomic<int> render_resolution{0};
    std::atomic<int> engine_fps{120};
    std::atomic<bool> deferred_readback{true};
    std::atomic<bool> stale_readback{true};
    /// SpartanGraphics::Anisotropies; the samplers read startup_anisotropy.
    std::atomic<int> anisotropy{0};
    int startup_anisotropy = 0;
    /// Set before an "Apply and restart"; cleared once the new launch shows the game. run.py
    /// restores the previous startup options when a launch never cleared it.
    std::atomic<bool> restart_unconfirmed{false};
    int startup_render_resolution = 0;
    /// The engine rate this launch really runs at (run.py's SPARTAN_ENGINE_FPS): 60 when the frame
    /// rate patch does not apply to this game build.
    std::atomic<int> applied_engine_fps{120};
    /// The game's maximum frame rate, read every frame (src/probe.c finds it): as a float and
    /// rounded. Null when this eboot.bin's code does not match. engine_fps is written there live.
    std::atomic<float*> engine_fps_global{nullptr};
    std::atomic<int32_t*> engine_fps_int_global{nullptr};
    /// The script global inPowerUpMenu (src/probe.c finds it): non-zero while the upgrade menu is
    /// open. Texture Quality adds no LOD bias then. Null when this eboot.bin's code does not match.
    std::atomic<int32_t*> power_up_menu_global{nullptr};
    bool startup_deferred_readback = true, startup_stale_readback = true;
    // FSR 4 checks (menu): the provider's auto exposure, the jitter sign it is given.
    std::atomic<bool> fsr4_auto_exposure{true};
    std::atomic<bool> fsr4_invert_jitter{false};
    std::atomic<int> active_render_width{1920}, active_render_height{1080};
    std::atomic<int> output_res{OutputDefault}; ///< index into OutputWidths
    /// Live resolution and preset changes (run.sh): 0 off by default (startup patch, fastest
    /// on the Steam Deck and older GPUs), -1 auto (strong discrete GPUs), 1 on. On restart.
    std::atomic<int> live_resolution{0};
    /// Why FSR 4 cannot run (assets, device features), or null. Set by the renderer.
    std::atomic<const char*> fsr4_problem{nullptr};
    std::atomic<bool> fsr4_supported{false}, fsr411_supported{false};
    /// Why frame generation stopped or cannot run, or null; its rates for the Insert menu
    /// ("FSR FG 2x, 60 -> 120 fps"), or null.
    std::atomic<const char*> fg_problem{nullptr};
    std::atomic<const char*> fg_status{nullptr};
    /// Frame generation running: the game's frames per second (the overlay shows both), else 0.
    std::atomic<float> fg_game_fps{0.0f};

    /// Startup settings for the explicit SPARTAN_RENDER_RES compatibility patch only.
    int startup_preset = NativeAA;
    int startup_upscaler = UpscalerOff;
    int startup_frame_generation = 0;
    bool startup_object_motion = true;
    int startup_output_res = OutputDefault;
    int startup_live_resolution = 0;
};

Values& Get();

/// Reads the file, then the environment overrides. Called once at start.
void Load();
/// Checks the loaded choice before the first frame; unsupported FSR 4 uses FSR 3.1.
void ConfigureUpscalerSupport(bool fsr4, bool fsr411, const char* fsr411_problem = nullptr);
/// Startup-patched scene dimensions cannot change until run.sh prepares a new image.
bool FixedRenderSession();
int RenderPreset();
bool ResolutionNeedsRestart();
/// A startup graphics option differs from the one this launch started with.
bool GraphicsNeedRestart();
/// The engine rate in effect: engine_fps, up to what this launch's patch allows.
int EngineFpsNow();
/// Present thread, every vblank: writes EngineFpsNow() into the game once the game has set its
/// own rate (a frame rate patch applied), when they differ.
void SyncEngineFps();
/// R3 + L1 + R1 / F9: the next engine rate (saved); 0 when this launch cannot change it.
int CycleEngineFps();
/// Writes the file (menu changes).
void Save();

/// Render resolution divisor of a preset (1.0 native, 1.5 quality, ...).
float PresetScale(int preset);
const char* PresetName(int preset);
/// The game's render size for a preset at an output size (tests/upscale_render_sizes.txt, the
/// launcher's upscale_render_size): even, never the game's 1920x1080 nor the output size.
std::pair<int, int> UpscaleRenderSize(int preset, int width, int height);
const char* UpscalerName(int upscaler);

} // namespace SpartanSettings
