//! What every setting is, described once: its file, type, default, choices and `SPARTAN_*` name.
//! Only the table; reading and writing live in `settings` and `store`.

use crate::core::options::{
    Choice, AIM_STYLES, ANISOTROPY, AUDIO_CHANNELS, DISPLAY_MODES, DRAW_PIPE, ENGINE_FPS_CHOICES,
    FPS_LIMITS, FRAMES_QUEUED, GLYPH_STYLES, LANGUAGES, LIVE_RESOLUTION, MENU_CONFIRM_STYLES,
    NO_MSAA, OUTPUTS, PRESETS, READBACKS, STICK_PROMPTS, STREAM_MEMORY, UI_LANGUAGES, UI_SCALES,
    UPSCALERS, VIDEO_BACKENDS,
};

/// The file a setting lives in.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum File {
    /// `settings.json`.
    App,
    /// The game's `spartanport.ini`.
    Ini,
}

#[derive(Debug, Clone, Copy)]
pub enum Kind {
    Flag,
    Text,
    Choice(&'static [Choice]),
    Int,
    Json,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Default {
    Bool(bool),
    Text(&'static str),
    Int(i64),
    /// An empty JSON object.
    Object,
}

impl Default {
    /// The default as the text an ini file or a text field holds; empty for an object.
    pub fn as_text(&self) -> String {
        match self {
            Default::Bool(on) => if *on { "1" } else { "0" }.to_string(),
            Default::Text(text) => (*text).to_string(),
            Default::Int(n) => n.to_string(),
            Default::Object => String::new(),
        }
    }

    /// The default read as a flag: what an absent key means.
    pub fn is_on(&self) -> bool {
        match self {
            Default::Bool(on) => *on,
            Default::Text(text) => !text.is_empty(),
            Default::Int(n) => *n != 0,
            Default::Object => false,
        }
    }
}

/// The environment variable the game reads, and its value when the variable is absent.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Env {
    pub name: &'static str,
    /// What the runner (`run.py`) assumes when the variable is not set.
    pub absent: &'static str,
}

#[derive(Debug, Clone, Copy)]
pub struct Setting {
    pub key: &'static str,
    pub file: File,
    pub kind: Kind,
    pub default: Default,
    /// App: goes into a new settings.json. Ini: a control of the window writes it.
    pub written: bool,
    /// Ini: where the key sits in `INI_DEFAULTS` (a fresh file's key order). Unused for App.
    pub ini_order: u8,
    pub env: Option<Env>,
    /// Ini: the App key whose `settings.json` value the row starts from when the file lacks the key.
    pub seed: Option<&'static str>,
}

const fn env(name: &'static str, absent: &'static str) -> Option<Env> {
    Some(Env { name, absent })
}

const fn app(key: &'static str, kind: Kind, default: Default, env: Option<Env>) -> Setting {
    Setting {
        key,
        file: File::App,
        kind,
        default,
        written: true,
        ini_order: 0,
        env,
        seed: None,
    }
}

/// An App setting a new settings.json does not get.
const fn app_unwritten(key: &'static str, kind: Kind, default: Default) -> Setting {
    Setting {
        written: false,
        ..app(key, kind, default, None)
    }
}

const fn ini(
    key: &'static str,
    kind: Kind,
    default: Default,
    written: bool,
    ini_order: u8,
) -> Setting {
    Setting {
        key,
        file: File::Ini,
        kind,
        default,
        written,
        ini_order,
        env: None,
        seed: None,
    }
}

/// An Ini row the window writes that starts from the App row of the same key while the file lacks
/// it (Python `load_ini`, #980): the App row stays, the ini's own value wins.
const fn seeded(key: &'static str, kind: Kind, default: Default, ini_order: u8) -> Setting {
    Setting {
        seed: Some(key),
        ..ini(key, kind, default, true, ini_order)
    }
}

use self::Default::{Bool, Int, Object, Text};
use Kind::{Choice as Pick, Flag, Json};

/// App rows in the order of `app_defaults`, then the two that are never written; Ini rows in the
/// order the window's controls are collected, then the parked upscaling keys.
pub static SETTINGS: &[Setting] = &[
    app(
        "ui_language",
        Pick(UI_LANGUAGES),
        Text("en"),
        env("SPARTAN_UI_LANGUAGE", ""),
    ),
    app("ui_scale", Pick(UI_SCALES), Text("auto"), None),
    app(
        "game_dir",
        Kind::Text,
        Text(""),
        env("SPARTAN_GAME_DIR", ""),
    ),
    app(
        "update_dir",
        Kind::Text,
        Text(""),
        env("SPARTAN_PATCH_DIR", ""),
    ),
    app(
        "experimental_upscaler",
        Flag,
        Bool(false),
        env("SPARTAN_EXPERIMENTAL_UPSCALER", ""),
    ),
    app("video_backend", Pick(VIDEO_BACKENDS), Text("auto"), None),
    app("gamemode", Flag, Bool(false), env("SPARTAN_GAMEMODE", "")),
    app("mangohud", Flag, Bool(false), env("SPARTAN_MANGOHUD", "")),
    app("desktop_integration", Flag, Bool(true), None),
    app(
        "user_dir",
        Kind::Text,
        Text(""),
        env("SPARTAN_USER_DIR", ""),
    ),
    app(
        "mods_dir",
        Kind::Text,
        Text(""),
        env("SPARTAN_MODS_DIR", ""),
    ),
    app(
        "mods_enabled",
        Flag,
        Bool(true),
        env("SPARTAN_MODS_ENABLED", "1"),
    ),
    app(
        "patches_dir",
        Kind::Text,
        Text(""),
        env("SPARTAN_PATCHES_DIR", ""),
    ),
    app(
        "language",
        Pick(LANGUAGES),
        Text("1"),
        env("SPARTAN_LANGUAGE", "1"),
    ),
    app("hdr", Flag, Bool(false), env("SPARTAN_HDR", "")),
    app(
        "draw_pipe",
        Pick(DRAW_PIPE),
        Text(""),
        env("SPARTAN_DRAW_PIPE", ""),
    ),
    app(
        "readbacks",
        Pick(READBACKS),
        Text(""),
        env("SPARTAN_READBACKS", ""),
    ),
    app(
        "no_msaa",
        Pick(NO_MSAA),
        Text(""),
        env("SPARTAN_NO_MSAA", ""),
    ),
    app(
        "stream_memory",
        Pick(STREAM_MEMORY),
        Text("cached"),
        env("SPARTAN_STREAM_MEMORY", "cached"),
    ),
    app("gpu_id", Kind::Text, Text(""), env("SPARTAN_GPU_ID", "")),
    app("monitor", Kind::Text, Text(""), env("SPARTAN_MONITOR", "")),
    app(
        "partial_height_views",
        Flag,
        Bool(true),
        env("SPARTAN_PARTIAL_HEIGHT_VIEWS", ""),
    ),
    app(
        "disk_shaders",
        Flag,
        Bool(true),
        env("SPARTAN_DISK_SHADERS", ""),
    ),
    app(
        "shader_prefetch",
        Flag,
        Bool(true),
        env("SPARTAN_SHADER_PREFETCH", ""),
    ),
    app(
        "shader_notes",
        Flag,
        Bool(false),
        env("SPARTAN_SHADER_NOTES", ""),
    ),
    app(
        "background_warmup",
        Flag,
        Bool(true),
        env("SPARTAN_BACKGROUND_WARMUP", ""),
    ),
    app(
        "parallel_warmup",
        Flag,
        Bool(true),
        env("SPARTAN_PARALLEL_WARMUP", ""),
    ),
    app(
        "pipeline_binary_cache",
        Flag,
        Bool(true),
        env("SPARTAN_PIPELINE_BINARIES", ""),
    ),
    app("perf_diag", Flag, Bool(false), env("SPARTAN_PERF_DIAG", "")),
    app(
        "frame_capture_key",
        Flag,
        Bool(false),
        env("SPARTAN_FRAME_CAPTURE_KEY", ""),
    ),
    app(
        "frame_stats",
        Flag,
        Bool(false),
        env("SPARTAN_FRAME_STATS", ""),
    ),
    app(
        "gpu_profile",
        Flag,
        Bool(false),
        env("SPARTAN_GPU_PROFILE", ""),
    ),
    app(
        "vk_validation",
        Flag,
        Bool(false),
        env("SPARTAN_VK_VALIDATION", ""),
    ),
    app("close_on_play", Flag, Bool(false), None),
    // The runner reads SPARTAN_DISCORD only to switch it off ("0"), so absent has no default.
    app("discord_rpc", Flag, Bool(true), env("SPARTAN_DISCORD", "")),
    app("check_updates", Flag, Bool(false), None),
    // Derived, not 1:1: `env.rs` builds SPARTAN_PATCHES and SPARTAN_KEY_MAP / SPARTAN_PAD_MAP
    // from the content of these two, so they carry no `env`.
    app("game_patches", Json, Object, None),
    app("controls", Json, Object, None),
    app("pad_style", Kind::Text, Text("playstation"), None),
    app("rage_one_input", Flag, Bool(true), None),
    app(
        "stick_prompts",
        Pick(STICK_PROMPTS),
        Text("keyboard"),
        env("SPARTAN_STICK_PROMPTS", ""),
    ),
    app("controls_layout", Kind::Int, Int(4), None),
    // Off unless set: the runner patches the tutorials only for "1" (`run.py` `switch_text_mounts`).
    app(
        "switch_attacks",
        Flag,
        Bool(false),
        env("SPARTAN_SWITCH_ATTACKS", ""),
    ),
    app(
        "glyph_style",
        Pick(GLYPH_STYLES),
        Text("off"),
        env("SPARTAN_GLYPH_STYLE", "off"),
    ),
    app_unwritten("launcher_effects", Flag, Bool(true)),
    app_unwritten("controls_device", Kind::Text, Text("")),
    ini(
        "display_mode",
        Pick(DISPLAY_MODES),
        Text("windowed"),
        true,
        6,
    ),
    ini("fps_limit", Pick(FPS_LIMITS), Text("0"), true, 8),
    ini(
        "engine_fps",
        Pick(ENGINE_FPS_CHOICES),
        Text("120"),
        true,
        13,
    ),
    ini("render_resolution", Kind::Text, Text("native"), true, 12),
    seeded("monitor", Kind::Text, Text(""), 15),
    ini("vsync", Flag, Bool(true), true, 7),
    seeded("hdr", Flag, Bool(false), 16),
    ini("anisotropy", Pick(ANISOTROPY), Text("0"), true, 20),
    ini("show_fps", Flag, Bool(true), true, 3),
    ini("frames_queued", Pick(FRAMES_QUEUED), Text("1"), true, 14),
    ini("async_shaders", Flag, Bool(true), true, 9),
    ini("stale_readback", Flag, Bool(true), true, 11),
    ini("deferred_readback", Flag, Bool(true), true, 10),
    ini("cheat_double_jumps", Flag, Bool(false), true, 19),
    ini("cheat_very_hard", Flag, Bool(false), true, 18),
    seeded("glyph_style", Pick(GLYPH_STYLES), Text("off"), 17),
    ini("mouse_prompt_distance", Kind::Int, Int(48), true, 22),
    ini("mouse_aim_style", Pick(AIM_STYLES), Text("0"), true, 24),
    ini("mouse_aim_sensitivity", Kind::Int, Int(100), true, 25),
    ini("mouse_lock", Kind::Int, Int(1), true, 23),
    ini("vibration", Kind::Int, Int(100), true, 21),
    ini(
        "menu_confirm_swap",
        Pick(MENU_CONFIRM_STYLES),
        Text("0"),
        true,
        26,
    ),
    // #968: read by the game itself (gpu/shim/spartanport_settings.cpp). motion_blur is the same
    // key as the in-game Options > Graphics > Motion Blur.
    ini("master_volume", Kind::Int, Int(100), true, 27),
    ini(
        "audio_channels",
        Pick(AUDIO_CHANNELS),
        Text("auto"),
        true,
        28,
    ),
    ini("motion_blur", Flag, Bool(true), true, 29),
    ini("depth_of_field", Flag, Bool(true), true, 30),
    ini("upscaler", Pick(UPSCALERS), Text("fsr3"), false, 0),
    ini("preset", Pick(PRESETS), Text("1"), false, 1),
    ini("object_motion", Flag, Bool(true), false, 2),
    ini("output_res", Pick(OUTPUTS), Text("1920x1080"), false, 4),
    ini(
        "live_resolution",
        Pick(LIVE_RESOLUTION),
        Text("auto"),
        false,
        5,
    ),
];

/// The Ini rows in the order of `INI_DEFAULTS`.
pub fn ini_rows() -> Vec<&'static Setting> {
    let mut rows: Vec<_> = SETTINGS.iter().filter(|s| s.file == File::Ini).collect();
    rows.sort_by_key(|s| s.ini_order);
    rows
}

/// The row of `key`, if the launcher knows it: the App row of a key both files hold (App rows come
/// first), whose `env` the game's environment reads.
pub fn setting(key: &str) -> Option<&'static Setting> {
    SETTINGS.iter().find(|s| s.key == key)
}

/// The Ini row of `key`, if the ini holds it: where the window reads and writes the key.
pub fn ini_setting(key: &str) -> Option<&'static Setting> {
    SETTINGS
        .iter()
        .find(|s| s.key == key && s.file == File::Ini)
}

/// What the runner assumes for the variable `name` when a direct `--run` does not set it.
pub fn run_default(name: &str) -> Option<&'static str> {
    SETTINGS
        .iter()
        .filter_map(|s| s.env)
        .find(|e| e.name == name)
        .map(|e| e.absent)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::core::ini::{INI_DEFAULTS, INI_FLAGS};
    use crate::core::settings::{app_defaults, Settings};
    use crate::ui::persist::SHOWN_INI_KEYS;
    use serde_json::json;

    #[test]
    fn every_key_is_described_once() {
        let mut seen = std::collections::HashSet::new();
        assert!(SETTINGS.iter().all(|s| seen.insert((s.key, s.file))));
        // A key in both files is an Ini row seeded from its App row.
        for row in SETTINGS.iter().filter(|s| s.file == File::Ini) {
            let in_app = SETTINGS
                .iter()
                .any(|s| s.file == File::App && s.key == row.key);
            assert_eq!(in_app, row.seed.is_some(), "{}", row.key);
        }
        let app: Vec<_> = SETTINGS
            .iter()
            .filter(|s| matches!(s.file, File::App) && s.written)
            .map(|s| s.key)
            .collect();
        let defaults = app_defaults(&|_| None);
        assert_eq!(app, defaults.keys().map(String::as_str).collect::<Vec<_>>());
    }

    #[test]
    fn the_table_holds_the_values_of_the_old_defaults() {
        let defaults = app_defaults(&|_| None);
        for (key, value) in &defaults {
            let row = setting(key).unwrap();
            let same = match (row.default, value) {
                (Default::Bool(a), serde_json::Value::Bool(b)) => a == *b,
                (Default::Text(a), serde_json::Value::String(b)) => a == b,
                (Default::Int(a), serde_json::Value::Number(b)) => b.as_i64() == Some(a),
                (Default::Object, serde_json::Value::Object(b)) => b.is_empty(),
                _ => false,
            };
            assert!(same, "{key}");
        }
    }

    #[test]
    fn seeded_ini_rows_name_their_app_key() {
        let seeded: Vec<_> = SETTINGS
            .iter()
            .filter_map(|s| s.seed.map(|seed| (s.key, seed)))
            .collect();
        assert_eq!(
            seeded,
            [
                ("monitor", "monitor"),
                ("hdr", "hdr"),
                ("glyph_style", "glyph_style")
            ]
        );
        for row in SETTINGS.iter().filter(|s| s.seed.is_some()) {
            assert_eq!(row.file, File::Ini, "{}", row.key);
            assert!(row.written, "{}", row.key);
            // The App row stays, as in the Python's APP_DEFAULTS, with the same default.
            let app = setting(row.key).unwrap();
            assert_eq!(app.file, File::App, "{}", row.key);
            assert!(app.written, "{}", row.key);
            assert_eq!(app.default.as_text(), row.default.as_text(), "{}", row.key);
            assert_eq!(ini_setting(row.key).unwrap().seed, row.seed);
        }
        assert!(SETTINGS
            .iter()
            .filter(|s| s.file == File::App)
            .all(|s| s.seed.is_none()));
        assert!(ini_setting("game_dir").is_none());
    }

    #[test]
    fn ini_lists_follow_the_table() {
        let rows: Vec<_> = SETTINGS.iter().filter(|s| s.file == File::Ini).collect();
        assert_eq!(rows.len(), INI_DEFAULTS.len());
        for (key, value) in INI_DEFAULTS.iter() {
            let row = ini_setting(key).unwrap();
            assert_eq!(row.default.as_text(), *value, "{key}");
            assert_eq!(
                matches!(row.kind, Kind::Flag),
                INI_FLAGS.contains(key),
                "{key}"
            );
        }
        let flags = rows.iter().filter(|s| matches!(s.kind, Kind::Flag)).count();
        assert_eq!(flags, INI_FLAGS.len());
        // The Python's INI_DEFAULTS order: the seeded keys follow frames_queued.
        let keys: Vec<_> = INI_DEFAULTS.iter().map(|(key, _)| *key).collect();
        assert_eq!(
            keys[14..19],
            [
                "frames_queued",
                "monitor",
                "hdr",
                "glyph_style",
                "cheat_very_hard"
            ]
        );
        assert_eq!(keys.last(), Some(&"depth_of_field"));
        let shown: Vec<_> = rows.iter().filter(|s| s.written).map(|s| s.key).collect();
        assert_eq!(shown, SHOWN_INI_KEYS.iter().copied().collect::<Vec<_>>());
    }

    #[test]
    fn shown_ini_keys_are_the_26_controls_of_the_window() {
        assert_eq!(SHOWN_INI_KEYS.len(), 26);
        for seeded in ["monitor", "hdr", "glyph_style"] {
            assert!(SHOWN_INI_KEYS.contains(&seeded), "{seeded}");
        }
        assert!(INI_FLAGS.contains(&"hdr"));
        for parked in [
            "upscaler",
            "preset",
            "object_motion",
            "output_res",
            "live_resolution",
        ] {
            assert!(!SHOWN_INI_KEYS.contains(&parked), "{parked}");
        }
    }

    #[test]
    fn the_unwritten_rows_stay_out_of_a_new_settings_file() {
        for key in ["launcher_effects", "controls_device"] {
            let row = setting(key).unwrap();
            assert!(!row.written, "{key}");
            assert!(!app_defaults(&|_| None).contains_key(key), "{key}");
        }
    }

    #[test]
    fn the_runner_defaults_come_from_the_table() {
        assert_eq!(run_default("SPARTAN_MODS_ENABLED"), Some("1"));
        assert_eq!(run_default("SPARTAN_STREAM_MEMORY"), Some("cached"));
        assert_eq!(run_default("SPARTAN_GLYPH_STYLE"), Some("off"));
        assert_eq!(run_default("SPARTAN_SWITCH_ATTACKS"), Some(""));
        assert_eq!(run_default("SPARTAN_NOT_A_SETTING"), None);
    }

    #[test]
    fn flag_follows_python_truth_and_defaults() {
        let mut s = Settings(serde_json::Map::new());
        assert!(s.flag("mods_enabled")); // default true
        assert!(!s.flag("hdr")); // default false
        for (v, on) in [
            (json!("0"), true),
            (json!("no"), true),
            (json!(2), true),
            (json!("true"), true),
            (json!(""), false),
            (json!(0), false),
            (json!(0.0), false),
            (json!(null), false),
            (json!([]), false),
            (json!({}), false),
            (json!(true), true),
            (json!(false), false),
        ] {
            s.set("hdr", v.clone());
            assert_eq!(s.flag("hdr"), on, "{v}");
        }
    }

    #[test]
    fn text_reads_the_default_of_an_absent_key() {
        let mut s = Settings(serde_json::Map::new());
        assert_eq!(s.text("stream_memory"), "cached");
        assert_eq!(s.text("controls_layout"), "4");
        s.set("stream_memory", json!("host"));
        assert_eq!(s.text("stream_memory"), "host");
    }
}
