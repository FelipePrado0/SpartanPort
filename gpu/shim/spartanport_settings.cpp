// SPDX-License-Identifier: GPL-2.0-or-later
#include "spartanport_settings.h"
#include "spartanport_orbs.h"
#include "spartanport_graphics.h"
#include "../spartanportgpu.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

namespace SpartanSettings {

namespace {

constexpr const char* GlyphStyles[] = {"off", "xbox", "nintendo", "keyboard", "auto"};

const char* Path() {
    const char* env = std::getenv("SPARTAN_CONFIG");
    return env && env[0] ? env : "spartanport.ini";
}

float Clamp(float v, float lo, float hi) {
    return std::clamp(v, lo, hi);
}

void Set(Values& v, const std::string& key, const std::string& value) {
    const float f = float(std::atof(value.c_str()));
    const int i = std::atoi(value.c_str());
    static const char* const overlay_keys[] = {"overlay_fps", "overlay_frametime", "overlay_upscaler",
                                               "overlay_gpu", "overlay_cpu", "overlay_ram", "overlay_vram"};
    for (unsigned item = 0; item < 7; ++item) {
        if (key == overlay_keys[item]) {
            const unsigned bit = 1u << item;
            v.overlay_items = i ? (v.overlay_items | bit) : (v.overlay_items & ~bit);
            return;
        }
    }
    if (key == "monitor") {
        v.monitor = SpartanGraphics::ParseMonitor(value);
    } else if (key == "hdr") {
        v.hdr = i != 0;
    } else if (key == "overlay_scale") {
        v.overlay_scale = std::clamp(i, 50, 300);
        return;
    }
    if (key == "overlay_corner") {
        v.overlay_corner = i >= 0 && i <= 3 ? i : 1;
        return;
    }
    if (key == "overlay_opacity") {
        v.overlay_opacity = std::clamp(i, 0, 100);
        return;
    }
    if (key == "overlay_layout") {
        v.overlay_layout = i == 1 ? 1 : 0;
        return;
    }
    if (key == "menu_tab") {
        v.menu_tab = i >= 0 && i <= 3 ? i : 0;
        return;
    }
    const char* cheat_keys[] = {"cheat_health", "cheat_magic", "cheat_item", "cheat_rage", "cheat_orbs",
                                "cheat_very_hard", "cheat_double_jumps"};
    for (unsigned cheat = 0; cheat < SPARTAN_CHEAT_COUNT; ++cheat) {
        if (key == cheat_keys[cheat]) {
            v.cheats[cheat] = i != 0;
            return;
        }
    }
    if (key == "upscaler") {
        for (int u = 0; u < UpscalerCount; ++u) {
            if (value == UpscalerName(u)) {
                v.upscaler = u;
            }
        }
    } else if (key == "preset") {
        v.preset = std::clamp(i, 0, PresetCount - 1);
    } else if (key == "sharpen") {
        v.sharpen = i != 0;
    } else if (key == "sharpness") {
        v.sharpness = Clamp(f, 0.0f, 2.0f);
    } else if (key == "jitter") {
        v.jitter = i != 0;
    } else if (key == "reactive") {
        v.reactive = i != 0;
    } else if (key == "object_motion") {
        v.object_motion = i != 0;
    } else if (key == "reactive_scale") {
        v.reactive_scale = Clamp(f, 0.0f, 16.0f);
    } else if (key == "reactive_threshold") {
        v.reactive_threshold = Clamp(f, 0.0f, 1.0f);
    } else if (key == "reactive_max") {
        v.reactive_max = Clamp(f, 0.0f, 1.0f);
    } else if (key == "debug_view") {
        v.debug_view = std::clamp(i, 0, DebugViewCount - 1);
    } else if (key == "show_fps") {
        v.show_fps = i != 0;
    } else if (key == "display_mode") {
        v.display_mode = SpartanGraphics::ParseDisplayMode(value);
    } else if (key == "vsync") {
        v.vsync = i != 0;
    } else if (key == "fps_limit") {
        v.fps_limit = SpartanGraphics::ParseFpsLimit(i);
    } else if (key == "async_shaders") {
        v.async_shaders = i != 0;
    } else if (key == "fsr1") {
        // FSR 1 left the launcher and the in-game menu: a value saved by an older version has no
        // control left to turn it off, so it stays off.
        v.fsr1 = false;
    } else if (key == "rcas") {
        v.rcas = i != 0;
    } else if (key == "rcas_strength") {
        v.rcas_strength = SpartanGraphics::ParseRcasStrength(value.empty() ? -1 : i);
    } else if (key == "frames_queued") {
        v.frames_queued = SpartanGraphics::ParseFramesQueued(value.empty() ? -1 : i);
    } else if (key == "render_resolution") {
        v.render_resolution = SpartanGraphics::ParseResolution(value);
    } else if (key == "engine_fps") {
        v.engine_fps = SpartanGraphics::ParseEngineFps(i);
    } else if (key == "deferred_readback") {
        v.deferred_readback = i != 0;
    } else if (key == "stale_readback") {
        v.stale_readback = i != 0;
    } else if (key == "anisotropy") {
        v.anisotropy = SpartanGraphics::ParseAnisotropy(i);
    } else if (key == "glyph_style") {
        v.glyph_style = 0;
        for (int style = 0; style < 5; ++style) if (value == GlyphStyles[style]) v.glyph_style = style;
    } else if (key == "vibration") {
        v.vibration = value.empty() ? 100 : std::clamp(i, 0, 100);
    } else if (key == "mouse_prompt_distance") {
        v.mouse_prompt_distance = value.empty() ? 48 : std::clamp(i, 16, 200);
    } else if (key == "mouse_lock") {
        v.mouse_lock = value.empty() || i != 0;
    } else if (key == "mouse_aim_style") {
        v.mouse_aim_style = std::clamp(i, 0, 1);
    } else if (key == "mouse_aim_sensitivity") {
        v.mouse_aim_sensitivity = value.empty() ? 100 : std::clamp(i, 10, 200);
    } else if (key == "restart_unconfirmed") {
        v.restart_unconfirmed = i != 0;
    } else if (key == "enemy_health_bar") {
        v.enemy_health_bar = i != 0;
    } else if (key == "auto_qte") {
        v.auto_qte = i != 0;
    } else if (key == "red_orb_multiplier") {
        v.red_orb_multiplier = SpartanOrbs::ClampMultiplier(f);
    } else if (key == "damage_dealt") {
        v.damage_dealt = SpartanOrbs::ClampMultiplier(f);
    } else if (key == "damage_taken") {
        v.damage_taken = SpartanOrbs::ClampMultiplier(f);
    } else if (key == "green_orb_multiplier") {
        v.green_orb_multiplier = SpartanOrbs::ClampMultiplier(f);
    } else if (key == "blue_orb_multiplier") {
        v.blue_orb_multiplier = SpartanOrbs::ClampMultiplier(f);
    } else if (key == "gold_orb_multiplier") {
        v.gold_orb_multiplier = SpartanOrbs::ClampMultiplier(f);
    } else if (key == "fsr4_auto_exposure") {
        v.fsr4_auto_exposure = i != 0;
    } else if (key == "fsr4_invert_jitter") {
        v.fsr4_invert_jitter = i != 0;
    } else if (key == "live_resolution") {
        v.live_resolution = value == "auto" ? -1 : std::clamp(i, 0, 1);
    } else if (key == "output_res") {
        for (int r = 0; r < OutputCount; ++r) {
            if (value == std::to_string(OutputWidths[r]) + "x" + std::to_string(OutputHeights[r])) {
                v.output_res = r;
            }
        }
    }
}

} // namespace

Values& Get() {
    static Values values;
    return values;
}

void Load() {
    auto& v = Get();
    if (FILE* file = std::fopen(Path(), "r")) {
        char line[256];
        while (std::fgets(line, sizeof(line), file)) {
            std::string text{line};
            text.erase(text.find_last_not_of(" \t\r\n") + 1);
            const auto eq = text.find('=');
            if (text.empty() || text[0] == '#' || eq == std::string::npos) {
                continue;
            }
            Set(v, text.substr(0, eq), text.substr(eq + 1));
        }
        std::fclose(file);
        std::printf("Settings: %s\n", Path());
    }
    // Environment overrides (scripts, A/B tests).
    if (const char* env = std::getenv("SPARTAN_UPSCALER")) {
        v.upscaler = UpscalerOff;
        for (int u = 0; u < UpscalerCount; ++u) {
            if (std::strcmp(env, UpscalerName(u)) == 0) v.upscaler = u;
        }
    }
    const std::pair<const char*, const char*> env_keys[] = {
        {"SPARTAN_FSR_SHARPNESS", "sharpness"},        {"SPARTAN_JITTER", "jitter"},
        {"SPARTAN_REACTIVE", "reactive"},              {"SPARTAN_REACTIVE_SCALE", "reactive_scale"},
        {"SPARTAN_REACTIVE_THRESHOLD", "reactive_threshold"}, {"SPARTAN_REACTIVE_MAX", "reactive_max"},
        {"SPARTAN_UPSCALE_PRESET", "preset"},            {"SPARTAN_OBJECT_MOTION", "object_motion"},
        {"SPARTAN_ASYNC_SHADERS", "async_shaders"},      {"SPARTAN_DEFERRED_READBACK", "deferred_readback"},
        {"SPARTAN_STALE_READBACK", "stale_readback"},     {"SPARTAN_FSR1", "fsr1"},
        {"SPARTAN_RCAS", "rcas"},                        {"SPARTAN_FRAMES_AHEAD", "frames_queued"},
        {"SPARTAN_ANISOTROPY", "anisotropy"},            {"SPARTAN_VIBRATION", "vibration"},
        {"SPARTAN_MOUSE_PROMPT_DISTANCE", "mouse_prompt_distance"}, {"SPARTAN_MOUSE_LOCK", "mouse_lock"},
        {"SPARTAN_MOUSE_AIM_STYLE", "mouse_aim_style"}, {"SPARTAN_MOUSE_AIM_SENSITIVITY", "mouse_aim_sensitivity"},
        {"SPARTAN_AUTO_QTE", "auto_qte"},
        {"SPARTAN_MONITOR", "monitor"}, {"SPARTAN_HDR", "hdr"},
        {"SPARTAN_GLYPH_STYLE", "glyph_style"},
    };
    for (const auto& [env, key] : env_keys) {
        if (const char* value = std::getenv(env); value && value[0]) {
            Set(v, key, value);
        }
    }
    v.startup_preset = v.preset;
    v.startup_upscaler = v.upscaler;
    v.startup_object_motion = v.object_motion;
    v.startup_output_res = v.output_res;
    v.startup_live_resolution = v.live_resolution;
    v.startup_render_resolution = v.render_resolution;
    v.startup_monitor = v.monitor;
    v.startup_hdr = v.hdr;
    v.startup_glyph_style = v.glyph_style;
    const char* applied = std::getenv("SPARTAN_ENGINE_FPS");
    v.applied_engine_fps = applied && applied[0] ? SpartanGraphics::ParseEngineFps(std::atoi(applied))
                                                 : v.engine_fps.load();
    v.startup_deferred_readback = v.deferred_readback;
    v.startup_stale_readback = v.stale_readback;
    v.startup_anisotropy = v.anisotropy;
}

int EngineFpsNow() {
    const auto& v = Get();
    return std::min(v.engine_fps.load(), v.applied_engine_fps.load());
}

void SyncEngineFps() {
    const auto& v = Get();
    volatile float* fps = v.engine_fps_global.load();
    volatile int32_t* rounded = v.engine_fps_int_global.load();
    // 0 until the game's setter ran; 60 when no frame rate patch applied (nothing to change).
    if (!fps || !rounded || *fps <= 0.0f || v.applied_engine_fps <= 60) {
        return;
    }
    const int now = EngineFpsNow();
    const float value = now == 60 ? 59.94f : float(now); // 59.94: the game's own constant
    if (*fps == value) {
        return;
    }
    // The game's frame timing reads both every frame (rounded: int(fps + 0.5), as its setter).
    *fps = value;
    *rounded = int32_t(value + 0.5f);
    std::printf("Engine FPS: %d\n", now);
}

int CycleEngineFps() {
    auto& v = Get();
    if (!v.engine_fps_global || v.applied_engine_fps <= 60) {
        return 0;
    }
    v.engine_fps = SpartanGraphics::NextEngineFps(EngineFpsNow(), v.applied_engine_fps);
    Save();
    return v.engine_fps;
}

bool GraphicsNeedRestart() {
    const auto& v = Get();
    return v.render_resolution != v.startup_render_resolution ||
           v.monitor != v.startup_monitor || v.hdr != v.startup_hdr ||
           v.deferred_readback != v.startup_deferred_readback ||
           v.stale_readback != v.startup_stale_readback ||
           v.anisotropy != v.startup_anisotropy || v.glyph_style != v.startup_glyph_style;
}

void ConfigureUpscalerSupport(bool fsr4, bool fsr411) {
    auto& v = Get();
    v.fsr4_supported = fsr4;
    v.fsr411_supported = fsr4 && fsr411;
    const int requested = v.upscaler;
    if ((requested == UpscalerFsr4 && !v.fsr4_supported) ||
        (requested == UpscalerFsr411 && !v.fsr411_supported)) {
        v.fsr4_problem = "GPU does not support the selected FSR 4 shaders; using FSR 3.1";
        std::printf("Upscaler: %s unsupported on this GPU; falling back to FSR 3.1 before the first frame\n",
                    UpscalerName(requested));
        v.upscaler = UpscalerFsr3;
    }
}

void ConfigureDlssSupport(bool available, const char* problem) {
    auto& v = Get();
    v.dlss_supported = available;
    static std::string kept;
    kept = problem ? problem : "";
    v.dlss_problem = available || kept.empty() ? nullptr : kept.c_str();
    if (v.upscaler == UpscalerDlss && !available) {
        std::printf("Upscaler: DLSS unavailable (%s); falling back to FSR 3.1\n",
                    kept.empty() ? "spartanport_dlss.dll or nvngx_dlss.dll missing" : kept.c_str());
        v.upscaler = UpscalerFsr3;
    }
}

bool FixedRenderSession() {
    const char* size = std::getenv("SPARTAN_RENDER_RES");
    return size && size[0];
}

int RenderPreset() {
    const auto& v = Get();
    return FixedRenderSession() ? v.startup_preset :
        v.upscaler == UpscalerTaa ? NativeAA : v.preset.load();
}

bool ResolutionNeedsRestart() {
    const auto& v = Get();
    // TAA needs the live path (native guest targets): run.sh selects it on restart.
    return FixedRenderSession() &&
        (v.preset != v.startup_preset || v.output_res != v.startup_output_res ||
         (v.upscaler == UpscalerOff) != (v.startup_upscaler == UpscalerOff) ||
         (v.upscaler == UpscalerTaa) != (v.startup_upscaler == UpscalerTaa));
}

void Save() {
    static std::mutex mutex; // the menu and the pad thread (CycleEngineFps)
    std::scoped_lock lock{mutex};
    const auto& v = Get();
    FILE* file = std::fopen(Path(), "w");
    if (!file) {
        std::printf("Settings: cannot write %s\n", Path());
        return;
    }
    std::fprintf(file,
                 "# spartanport settings (in-game menu: Insert / R3+L2)\n"
                 "upscaler=%s\npreset=%d\nsharpen=%d\nsharpness=%.2f\njitter=%d\n"
                 "reactive=%d\nobject_motion=%d\nreactive_scale=%.2f\nreactive_threshold=%.2f\nreactive_max=%.2f\n"
                 "debug_view=%d\nshow_fps=%d\nfsr4_auto_exposure=%d\nfsr4_invert_jitter=%d\n",
                 UpscalerName(v.upscaler), v.preset.load(), int(v.sharpen.load()),
                 v.sharpness.load(), int(v.jitter.load()), int(v.reactive.load()),
                 int(v.object_motion.load()),
                 v.reactive_scale.load(), v.reactive_threshold.load(), v.reactive_max.load(),
                 v.debug_view.load(), int(v.show_fps.load()),
                 int(v.fsr4_auto_exposure.load()), int(v.fsr4_invert_jitter.load()));
    std::fprintf(file, "output_res=%dx%d\n", OutputWidths[v.output_res], OutputHeights[v.output_res]);
    std::fprintf(file, "red_orb_multiplier=%.3f\n", v.red_orb_multiplier.load());
    std::fprintf(file, "damage_dealt=%.3f\n", v.damage_dealt.load());
    std::fprintf(file, "damage_taken=%.3f\n", v.damage_taken.load());
    std::fprintf(file, "green_orb_multiplier=%.3f\n", v.green_orb_multiplier.load());
    std::fprintf(file, "blue_orb_multiplier=%.3f\n", v.blue_orb_multiplier.load());
    std::fprintf(file, "gold_orb_multiplier=%.3f\n", v.gold_orb_multiplier.load());
    std::fprintf(file, "cheat_health=%d\ncheat_magic=%d\ncheat_item=%d\ncheat_rage=%d\ncheat_orbs=%d\n"
                       "cheat_very_hard=%d\ncheat_double_jumps=%d\n",
                 int(v.cheats[0].load()), int(v.cheats[1].load()), int(v.cheats[2].load()),
                 int(v.cheats[3].load()), int(v.cheats[4].load()),
                 int(v.cheats[5].load()), int(v.cheats[6].load()));
    std::fprintf(file, "enemy_health_bar=%d\n", int(v.enemy_health_bar.load()));
    std::fprintf(file, "auto_qte=%d\n", int(v.auto_qte.load()));
    std::fprintf(file,
                 "overlay_fps=%d\noverlay_frametime=%d\noverlay_upscaler=%d\noverlay_gpu=%d\n"
                 "overlay_cpu=%d\noverlay_ram=%d\noverlay_vram=%d\noverlay_scale=%d\noverlay_corner=%d\n"
                 "overlay_opacity=%d\noverlay_layout=%d\nmenu_tab=%d\n",
                 int(v.overlay_items >> 0 & 1), int(v.overlay_items >> 1 & 1), int(v.overlay_items >> 2 & 1),
                 int(v.overlay_items >> 3 & 1), int(v.overlay_items >> 4 & 1), int(v.overlay_items >> 5 & 1),
                 int(v.overlay_items >> 6 & 1), v.overlay_scale.load(), v.overlay_corner.load(),
                 v.overlay_opacity.load(), v.overlay_layout.load(), v.menu_tab.load());
    const std::vector<std::string> resolution_names = SpartanGraphics::ResolutionNames();
    std::fprintf(file,
                 "display_mode=%s\nvsync=%d\nfps_limit=%d\nasync_shaders=%d\n"
                 "fsr1=%d\nrcas=%d\nrcas_strength=%d\nframes_queued=%d\n"
                 "render_resolution=%s\nengine_fps=%d\ndeferred_readback=%d\nstale_readback=%d\n"
                 "restart_unconfirmed=%d\n",
                 SpartanGraphics::DisplayModeKeys[std::clamp(v.display_mode.load(), 0, 2)].data(),
                 int(v.vsync.load()), v.fps_limit.load(), int(v.async_shaders.load()),
                 int(v.fsr1.load()), int(v.rcas.load()), v.rcas_strength.load(), v.frames_queued.load(),
                 resolution_names[std::clamp(v.render_resolution.load(), 0, int(resolution_names.size()) - 1)].c_str(),
                 v.engine_fps.load(), int(v.deferred_readback.load()), int(v.stale_readback.load()),
                 int(v.restart_unconfirmed.load()));
    std::fprintf(file, "anisotropy=%d\nvibration=%d\nmouse_prompt_distance=%d\nmouse_lock=%d\nmouse_aim_style=%d\n"
                       "mouse_aim_sensitivity=%d\n",
                 v.anisotropy.load(), v.vibration.load(), v.mouse_prompt_distance.load(), int(v.mouse_lock.load()),
                 v.mouse_aim_style.load(), v.mouse_aim_sensitivity.load());
    // Read by run.sh at start.
    std::fprintf(file, "live_resolution=%s\n", v.live_resolution < 0 ? "auto"
                                                  : v.live_resolution ? "1" : "0");
    std::fprintf(file, "monitor=%s\nhdr=%d\n", v.monitor < 0 ? "" : std::to_string(v.monitor.load()).c_str(),
                 int(v.hdr.load()));
    std::fprintf(file, "glyph_style=%s\n", GlyphStyles[std::clamp(v.glyph_style.load(), 0, 4)]);
    std::fclose(file);
}

float PresetScale(int preset) {
    static constexpr float scales[PresetCount] = {1.0f, 1.5f, 1.7f, 2.0f, 3.0f};
    return scales[std::clamp(preset, 0, PresetCount - 1)];
}

const char* PresetName(int preset) {
    static constexpr const char* names[PresetCount] = {"Native AA", "Quality", "Balanced",
                                                       "Performance", "Ultra Performance"};
    return names[std::clamp(preset, 0, PresetCount - 1)];
}

const char* UpscalerName(int upscaler) {
    static constexpr const char* names[UpscalerCount] = {"off", "fsr3", "fsr4", "fsr411", "taa", "dlss"};
    return names[std::clamp(upscaler, 0, UpscalerCount - 1)];
}

} // namespace SpartanSettings

extern "C" int spartanportgpu_enemy_health_bar(int toggle) {
    auto& v = SpartanSettings::Get();
    if (!v.actor_watch_supported.load()) return 2;
    if (toggle) {
        v.enemy_health_bar = !v.enemy_health_bar.load();
        SpartanSettings::Save();
    }
    return v.enemy_health_bar.load();
}

extern "C" uint64_t spartanportgpu_native_setting(unsigned function) {
    auto& v = SpartanSettings::Get();
    const unsigned group = function & ~0xfu, index = function & 0xfu;
    const auto text = [](const char* value) { return reinterpret_cast<uint64_t>(value); };
    if ((function >= 0xfa4 && function <= 0xfaf) || (function >= 0xfb1 && function <= 0xfbd)) {
        if (function == 0xfb7) {
            v.glyph_style = 0;
            v.vibration = 100;
            v.mouse_aim_sensitivity = 100;
            v.mouse_aim_style = 0;
            v.mouse_prompt_distance = 48;
            v.mouse_lock = true;
            SpartanSettings::Save();
            return 0;
        }
        const unsigned row = function >= 0xfb8 ? function - 0xfb8 : function >= 0xfb1 ? function - 0xfb1
                           : function >= 0xfaa ? function - 0xfaa : function - 0xfa4;
        const int direction = function >= 0xfb8 ? 0 : function >= 0xfb1 ? 1 : function >= 0xfaa ? -1 : 0;
        if (direction) {
            if (row == 0) v.glyph_style = ((v.glyph_style < 4 ? v.glyph_style.load() : 0) + direction + 4) % 4;
            else if (row == 1) v.vibration = std::clamp(v.vibration + direction * 5, 0, 100);
            else if (row == 2) v.mouse_aim_sensitivity = std::clamp(v.mouse_aim_sensitivity + direction * 5, 10, 200);
            else if (row == 3) v.mouse_aim_style = !v.mouse_aim_style;
            else if (row == 4) v.mouse_prompt_distance = std::clamp(v.mouse_prompt_distance + direction * 4, 16, 200);
            else if (row == 5) v.mouse_lock = !v.mouse_lock;
            SpartanSettings::Save();
        }
        const char* language = std::getenv("SPARTAN_LANGUAGE");
        const bool pt = language && (std::strcmp(language, "7") == 0 || std::strcmp(language, "17") == 0
                                    || std::strncmp(language, "pt", 2) == 0 || std::strcmp(language, "br") == 0);
        std::string value;
        if (row == 0) {
            const char* names[] = {"PlayStation", "Xbox", "Nintendo", "Keyboard", pt ? "Automático" : "Automatic"};
            value = names[std::clamp(v.glyph_style.load(), 0, 4)];
        } else if (row == 1) value = std::to_string(v.vibration.load()) + "%";
        else if (row == 2) value = std::to_string(v.mouse_aim_sensitivity.load()) + "%";
        else if (row == 3) value = v.mouse_aim_style ? (pt ? "Absoluto" : "Absolute") : (pt ? "Relativo" : "Relative");
        else if (row == 4) value = std::to_string(v.mouse_prompt_distance.load()) + " px";
        else value = v.mouse_lock ? (pt ? "Ligado" : "On") : (pt ? "Desligado" : "Off");
        static thread_local std::string labels[6];
        const bool selected = direction || function >= 0xfb8;
        labels[row] = std::string(selected ? "[ArrowSelectionL]" : "[ArrowSpacer]") + value
                    + (selected ? "[ArrowSelectionR]" : "[ArrowSpacer]");
        return text(labels[row].c_str());
    }
    if (function >= 0xfc0 && function <= 0xff7) {
        const bool restart = std::getenv("SPARTAN_RESTART_COMMAND") &&
                             std::getenv("SPARTAN_RESTART_COMMAND")[0] && SpartanSettings::GraphicsNeedRestart();
        if (function == 0xfc9) return restart;
        if (function == 0xfcb) {
            if (!restart) return 0;
            v.restart_unconfirmed = true;
            SpartanSettings::Save();
            return 1;
        }
        if (index >= 8) return 0;
        const char* language = std::getenv("SPARTAN_LANGUAGE");
        const bool pt = language && (std::strcmp(language, "7") == 0 || std::strcmp(language, "17") == 0
                                    || std::strncmp(language, "pt", 2) == 0 || std::strcmp(language, "br") == 0);
        const int direction = group == 0xfd0 ? -1 : group == 0xfe0 ? 1 : 0;
        const auto names = SpartanGraphics::ResolutionNames();
        const auto aspect = [](const std::string& name) {
            const auto [w, h] = SpartanGraphics::ResolutionSize(name);
            const char* labels[] = {"16:9", "16:10", "21:9", "32:9", "4:3"};
            const double ratios[] = {16.0/9, 1.6, 2.37, 32.0/9, 4.0/3};
            for (unsigned i = 0; i < 5; ++i) if (std::abs(double(w)/h - ratios[i]) < 0.05)
                return std::string(labels[i]);
            const int divisor = std::gcd(w, h);
            return std::to_string(w/divisor) + ":" + std::to_string(h/divisor);
        };
        const auto pixels = [&](int i) {
            const auto [w, h] = SpartanGraphics::ResolutionSize(names[i]);
            return int64_t(w) * h;
        };
        int resolution = std::clamp(v.render_resolution.load(), 0, int(names.size()) - 1);
        const bool engine_supported = v.engine_fps_global && v.engine_fps_int_global && v.applied_engine_fps > 60;
        if (direction && (index != 3 || engine_supported)) {
            const auto cycle = [&](const auto& values, int current) {
                const auto found = std::find(values.begin(), values.end(), current);
                const int pos = found == values.end() ? 0 : int(found - values.begin());
                return values[(pos + direction + int(values.size())) % values.size()];
            };
            if (index <= 1) {
                std::vector<std::string> aspects;
                for (const char* a : {"16:9", "16:10", "21:9", "32:9", "4:3"}) aspects.emplace_back(a);
                for (const auto& name : names) {
                    const auto a = aspect(name);
                    if (std::find(aspects.begin(), aspects.end(), a) == aspects.end()) aspects.push_back(a);
                }
                auto desired = aspect(names[resolution]);
                if (index == 1) {
                    const int current = int(std::find(aspects.begin(), aspects.end(), desired) - aspects.begin());
                    desired = aspects[(current + direction + int(aspects.size())) % aspects.size()];
                }
                std::vector<int> choices;
                for (unsigned i = 0; i < names.size(); ++i) if (aspect(names[i]) == desired) choices.push_back(int(i));
                std::stable_sort(choices.begin(), choices.end(), [&](int a, int b) {
                    return a == 0 ? b != 0 : b == 0 ? false : pixels(a) < pixels(b);
                });
                if (!choices.empty()) resolution = index == 0 ? cycle(choices, resolution)
                    : *std::min_element(choices.begin(), choices.end(), [&](int a, int b) {
                        return std::abs(pixels(a) - pixels(resolution)) < std::abs(pixels(b) - pixels(resolution));
                    });
                v.render_resolution = resolution;
            } else if (index == 2) v.fps_limit = cycle(SpartanGraphics::FpsLimits, v.fps_limit);
            else if (index == 3) {
                std::vector<int> rates;
                for (int rate : SpartanGraphics::EngineFps) if (rate <= v.applied_engine_fps) rates.push_back(rate);
                v.engine_fps = cycle(rates, v.engine_fps);
            } else if (index == 4) {
                std::vector<int> monitors{-1};
                monitors.insert(monitors.end(), v.monitors.begin(), v.monitors.end());
                v.monitor = cycle(monitors, v.monitor);
            } else if (index == 5) v.display_mode = (v.display_mode + direction + 3) % 3;
            else if (index == 6) v.vsync = !v.vsync;
            else if (index == 7) v.hdr = !v.hdr;
            SpartanSettings::Save();
        }
        std::string value;
        if (index == 0) value = names[resolution] == "native" ? "1920x1080" : names[resolution];
        else if (index == 1) value = aspect(names[resolution]);
        else if (index == 2) value = v.fps_limit ? std::to_string(v.fps_limit.load()) : pt ? "Sem limite" : "Unlimited";
        else if (index == 3) value = engine_supported ? std::to_string(v.engine_fps.load()) : "-";
        else if (index == 4) value = v.monitor < 0 ? (pt ? "Principal" : "Primary") : std::to_string(v.monitor + 1);
        else if (index == 5) {
            const char* en[] = {"Windowed", "Borderless", "Fullscreen"};
            const char* br[] = {"Janela", "Sem bordas", "Tela cheia"};
            value = (pt ? br : en)[std::clamp(v.display_mode.load(), 0, 2)];
        } else value = (index == 6 ? v.vsync.load() : v.hdr.load()) ? (pt ? "Ligado" : "On") : (pt ? "Desligado" : "Off");
        const bool selected = group != 0xfc0 && (index != 3 || engine_supported);
        static thread_local std::string labels[8];
        labels[index] = std::string(selected ? "[ArrowSelectionL]" : "[ArrowSpacer]") + value
                      + (selected ? "[ArrowSelectionR]" : "[ArrowSpacer]");
        return text(labels[index].c_str());
    }
    if (function >= 0xf70 && function <= 0xfb0) {
        const char* language = std::getenv("SPARTAN_LANGUAGE");
        const bool portuguese = language && (std::strcmp(language, "7") == 0 || std::strcmp(language, "17") == 0
            || std::strcmp(language, "pt") == 0 || std::strcmp(language, "pt-BR") == 0
            || std::strcmp(language, "pt-br") == 0 || std::strcmp(language, "br") == 0);
        const auto label = [&](const char* en, const char* pt) { return text(portuguese ? pt : en); };
        const bool rcas_available = v.image_width > 0 && v.image_height > 0 && v.window_width > 0 && v.window_height > 0
            && SpartanGraphics::FsrPassRuns(v.fsr1, true, v.image_width, v.image_height, v.window_width, v.window_height);
        if (function == 0xfb0) {
            v.frames_queued = 1;
            v.async_shaders = true;
            v.rcas = true;
            v.rcas_strength = 75;
            v.anisotropy = 0;
            SpartanSettings::Save();
            return 0;
        }
        if (index >= 5 || group > 0xfa0) return 0;
        if (group == 0xfa0) return index == 3 ? std::clamp(v.rcas_strength.load(), 0, 100) : 0;
        if (index == 3 && (!v.rcas || !rcas_available))
            return text("-");
        if (group == 0xf80 || group == 0xf90) {
            if (index == 0) {
                const auto& values = SpartanGraphics::FramesQueued;
                const int current = SpartanGraphics::IndexOf(values, v.frames_queued.load(), 0);
                v.frames_queued = values[(current + (group == 0xf90 ? 1 : 2)) % values.size()];
            } else if (index == 1) {
                v.async_shaders = !v.async_shaders.load();
            } else if (index == 2) {
                v.rcas = !v.rcas.load();
            } else if (index == 4) {
                const auto& values = SpartanGraphics::Anisotropies;
                const int current = SpartanGraphics::IndexOf(values, v.anisotropy.load(), 0);
                v.anisotropy = values[(current + (group == 0xf90 ? 1 : 4)) % values.size()];
            } else {
                v.rcas_strength = std::clamp(v.rcas_strength.load() + (group == 0xf90 ? 5 : -5), 0, 100);
            }
            SpartanSettings::Save();
        }
        if (index == 0) {
            if (v.frames_queued == 0) return label("Unlimited", "Sem limite");
            return text(v.frames_queued == 2 ? "2" : "1");
        }
        if (index == 1 || index == 2) {
            const bool enabled = index == 1 ? v.async_shaders.load() : v.rcas.load();
            return enabled ? label("On", "Ligada") : label("Off", "Desligada");
        }
        if (index == 4) {
            static thread_local std::string anisotropy;
            anisotropy = v.anisotropy ? std::to_string(v.anisotropy.load()) + "x" : portuguese ? "Padrão do jogo" : "Game default";
            return text(anisotropy.c_str());
        }
        static thread_local char strength[8];
        std::snprintf(strength, sizeof(strength), "%d%%", v.rcas_strength.load());
        return text(strength);
    }
    if ((group == 0xf00 || group == 0xf10) && index == SPARTAN_CHEAT_COUNT) {
        if (!v.qte_supported.load()) return text("Unavailable");
        if (group == 0xf10) {
            v.auto_qte = !v.auto_qte.load();
            SpartanSettings::Save();
        }
        return text(v.auto_qte ? "On" : "Off");
    }
    if ((group == 0xf00 || group == 0xf10) && index < SPARTAN_CHEAT_COUNT) {
        if (!(v.cheats_supported.load() & (1u << index))) return text("Unavailable");
        if (group == 0xf10) {
            v.cheats[index] = !v.cheats[index].load();
            SpartanSettings::Save();
        }
        return text(v.cheats[index] ? "On" : "Off");
    }
    std::atomic<float>* const multipliers[] = {&v.red_orb_multiplier, &v.green_orb_multiplier,
        &v.blue_orb_multiplier, &v.gold_orb_multiplier, &v.damage_dealt, &v.damage_taken};
    if (function == 0xf60) {
        for (auto* value : multipliers) *value = 1.0f;
        SpartanSettings::Save();
        return 0;
    }
    if (group < 0xf20 || group > 0xf50 || index >= 6) return 0;
    const bool supported = index == 0 ? v.red_orbs_supported.load()
        : index < 4 ? v.orb_pickup_supported.load() : v.damage_supported.load();
    const bool blocked = index == 0 && (v.cheats_supported.load() & (1u << SPARTAN_CHEAT_ORBS))
        && v.cheats[SPARTAN_CHEAT_ORBS].load();
    if (!supported || blocked) return group == 0xf50 ? 0 : text(supported ? "Disabled" : "Unavailable");
    auto& setting = *multipliers[index];
    float value = SpartanOrbs::ClampMultiplier(setting.load());
    if (group == 0xf30 || group == 0xf40) {
        // Snap arbitrary GUI values onto the native control's grid in the requested direction.
        const double units = std::round(value * 100.0) / 10.0;
        const bool increase = group == 0xf40;
        const int step = (increase ? units < 20 : units <= 20) ? 1
            : (increase ? units < 100 : units <= 100) ? 5 : 10;
        const double next = increase ? std::floor(units / step + 0.00001) + 1
                                     : std::ceil(units / step - 0.00001) - 1;
        value = SpartanOrbs::ClampMultiplier(float(next * step / 10.0));
        setting = value;
        SpartanSettings::Save();
    }
    if (group == 0xf50) {
        const float position = value <= 2 ? (value - 0.1f) * 10
            : value <= 10 ? 19 + (value - 2) * 2 : 35 + value - 10;
        return static_cast<uint64_t>(std::lround(position * 100 / 125));
    }
    // Each row owns its buffer: assigning another row must not change an earlier label.
    static thread_local char labels[6][24];
    std::snprintf(labels[index], sizeof(labels[index]), "%.2fx", value);
    return text(labels[index]);
}
