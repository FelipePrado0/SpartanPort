//! The environment the game is started with (`SPARTAN_*`) and the trophy texts it reads.

use std::ffi::OsStr;
use std::fs;
use std::path::Path;

use indexmap::IndexMap;
use serde_json::Value;

use crate::core::catalog::{run_default, setting, Kind};
use crate::core::controls::{control_map, keyboard_map, Kind as ControlKind};
use crate::core::i18n::{tr, trophy_text};
use crate::core::installation::Installation;
use crate::core::legacy::forward_environment;
use crate::core::options::Choice;
use crate::core::paths::Paths;
use crate::core::settings::Settings;

/// Reads one environment variable; tests pass a closure over a map instead of the process.
pub type Lookup<'a> = &'a dyn Fn(&str) -> Option<String>;

const TROPHY_COUNT: usize = 35;

/// AMD hybrid laptops: this Vulkan layer hides the NVIDIA GPU and the game stays black (#263).
const VULKAN_LAYERS_DISABLE: (&str, &str) = (
    "VK_LOADER_LAYERS_DISABLE",
    "VK_LAYER_AMD_switchable_graphics",
);

/// What the launcher changes in its own environment before anything reads it, as the Python does
/// on import: the old `GOW3_*` names are forwarded and, on Windows, the AMD switchable-graphics
/// layer is disabled unless the player set the variable. The game and the probes inherit it.
pub fn start_up_environment(env: &mut IndexMap<String, String>, windows: bool) {
    if windows {
        let (key, value) = VULKAN_LAYERS_DISABLE;
        env.entry(key.to_string())
            .or_insert_with(|| value.to_string());
    }
    forward_environment(env);
}

fn one_or_zero(on: bool) -> String {
    if on { "1" } else { "0" }.to_string()
}

/// `user/trophy_texts.txt`: the trophy names, descriptions and labels the game shows, in the
/// launcher's language. A failure is harmless.
pub fn write_trophy_texts(folder: &Path, lang: &str) {
    let labels = [
        ("heading", "Trophies"),
        ("notice", "TROPHY UNLOCKED"),
        ("unlocked", "UNLOCKED"),
        ("locked", "LOCKED"),
        ("all", "All trophies"),
        ("only_unlocked", "Unlocked"),
        ("only_locked", "Locked"),
        ("counter", "{} of {} unlocked"),
        (
            "hint",
            "Up/Down: scroll   Left/Right: filter   F8 / R3+R2 / Esc: close",
        ),
    ];
    let mut lines = vec![format!(
        "# Written by the launcher on every start (language: {lang}); do not edit."
    )];
    for (key, en) in labels {
        let text = tr(lang, en);
        let collapsed = text.split_whitespace().collect::<Vec<_>>().join(" ");
        lines.push(format!("@{key}={collapsed}"));
    }
    for trophy in 0..TROPHY_COUNT {
        let (name, description) = trophy_text(lang, trophy);
        lines.push(format!("{trophy}={name} | {description}"));
    }
    if fs::create_dir_all(folder).is_ok() {
        let _ = fs::write(folder.join("trophy_texts.txt"), lines.join("\n") + "\n");
    }
}

/// An environment as text, in its order (lossily where it is not Unicode): what the game starts
/// from is the launcher's whole environment, `environment_of(std::env::vars_os())`, as the
/// Python passes `dict(os.environ)`.
pub fn environment_of<K: AsRef<OsStr>, V: AsRef<OsStr>>(
    vars: impl IntoIterator<Item = (K, V)>,
) -> IndexMap<String, String> {
    vars.into_iter()
        .map(|(key, value)| {
            (
                key.as_ref().to_string_lossy().into_owned(),
                value.as_ref().to_string_lossy().into_owned(),
            )
        })
        .collect()
}

/// The `SPARTAN_*` name of a catalog row; empty when the row has none (a slip the contract test in
/// `tests/env_contract.rs` catches).
fn var(key: &str) -> &'static str {
    setting(key).and_then(|row| row.env).map_or("", |e| e.name)
}

/// The choices of a Choice row.
fn choices(key: &str) -> &'static [Choice] {
    match setting(key).map(|row| row.kind) {
        Some(Kind::Choice(list)) => list,
        _ => &[],
    }
}

/// `1` or `0` for the flag `key`, always written.
fn put_switch(env: &mut IndexMap<String, String>, settings: &Settings, key: &str) {
    env.insert(var(key).into(), one_or_zero(settings.flag(key)));
}

/// `1` for the flag `key` when it is on, nothing otherwise.
fn put_if_on(env: &mut IndexMap<String, String>, settings: &Settings, key: &str) {
    if settings.flag(key) {
        env.insert(var(key).into(), "1".into());
    }
}

/// `base` (the launcher's own environment) plus everything the game reads from it, and the trophy
/// texts file the game reads with it.
pub fn game_environment(
    settings: &Settings,
    paths: &Paths,
    lang: &str,
    base: &IndexMap<String, String>,
    windows: bool,
) -> IndexMap<String, String> {
    write_trophy_texts(&paths.user_dir(&settings.get_str("user_dir")), lang);
    environment(settings, paths, lang, base, windows)
}

/// The game's environment: pure, it writes no file. The order of the keys is the Python's.
pub fn environment(
    settings: &Settings,
    paths: &Paths,
    lang: &str,
    base: &IndexMap<String, String>,
    windows: bool,
) -> IndexMap<String, String> {
    let mut env = base.clone();
    env.insert(var("game_dir").into(), settings.get_str("game_dir"));
    env.shift_remove(var("update_dir"));
    // Temporal upscaling is parked: a switch saved on earlier has no control left to turn it off.
    env.insert(var("experimental_upscaler").into(), "0".into());
    let update_dir = settings.get_str("update_dir");
    if !update_dir.is_empty() {
        env.insert(var("update_dir").into(), update_dir.clone());
    }
    if !windows {
        let backend = settings.text("video_backend");
        if backend == "wayland" || backend == "x11" {
            env.insert("SDL_VIDEO_DRIVER".into(), backend.clone());
            env.insert("SDL_VIDEODRIVER".into(), backend);
        }
        put_switch(&mut env, settings, "gamemode");
        put_switch(&mut env, settings, "mangohud");
    }
    // The launcher's patch selection is the whole list (derived: no 1:1 catalog row).
    let game_dir = settings.get_str("game_dir");
    let installation = Installation::read(&game_dir, &update_dir);
    let (title_id, profile) = (installation.title_id(), installation.profile());
    let chosen = title_id.and_then(|id| settings.get("game_patches")?.get(id)?.as_array());
    if let (Some(_), Some(chosen)) = (profile, chosen) {
        let names: Vec<&str> = chosen.iter().filter_map(Value::as_str).collect();
        env.insert("SPARTAN_PATCHES".into(), names.join(";"));
        env.insert("SPARTAN_PATCHES_ONLY".into(), "1".into());
    }
    let user_dir = settings.get_str("user_dir");
    if !user_dir.is_empty() {
        env.insert(var("user_dir").into(), user_dir);
    }
    if !windows {
        env.insert(
            "SPARTAN_FSR4_DIR".into(),
            paths.fsr4_folder().to_string_lossy().into_owned(),
        );
    }
    let path_text = |p: std::path::PathBuf| p.to_string_lossy().into_owned();
    env.insert(
        var("mods_dir").into(),
        path_text(paths.mods_dir(&settings.get_str("mods_dir"))),
    );
    env.insert("SPARTAN_MODS_CONFIG".into(), path_text(paths.mods_json()));
    put_switch(&mut env, settings, "mods_enabled");
    env.insert(
        var("patches_dir").into(),
        path_text(paths.patches_dir(&settings.get_str("patches_dir"))),
    );
    env.insert(
        "SPARTAN_PATCHES_CONFIG".into(),
        path_text(paths.patches_json()),
    );
    env.insert(var("language").into(), settings.get_str("language"));
    // Menu language is independent from the game's text and voice language.
    let ui_language = settings.get_str("ui_language");
    env.insert(
        var("ui_language").into(),
        if ui_language.is_empty() {
            lang.to_string()
        } else {
            ui_language
        },
    );
    // On by default (R91), also in a file without the key.
    put_switch(&mut env, settings, "discord_rpc");
    // Always written (#980): the runner copies it into the ini once, as `0` too.
    put_switch(&mut env, settings, "hdr");
    for key in ["draw_pipe", "readbacks", "no_msaa"] {
        if settings.flag(key) {
            env.insert(var(key).into(), settings.get_str(key));
        }
    }
    // The saved UI choice wins over inherited diagnostic overrides, including when reverting.
    let stream_memory = settings.text("stream_memory");
    let stream_memory = if choices("stream_memory")
        .iter()
        .any(|choice| choice.value == stream_memory)
    {
        stream_memory
    } else {
        run_default(var("stream_memory")).unwrap_or_default().into()
    };
    env.insert(var("stream_memory").into(), stream_memory);
    // Only ASCII digits: the game cannot read the other Unicode digits.
    let digits = |text: &str| !text.is_empty() && text.chars().all(|c| c.is_ascii_digit());
    let gpu_id = settings.get_str("gpu_id");
    env.shift_remove(var("gpu_id"));
    if digits(&gpu_id) {
        env.insert(var("gpu_id").into(), gpu_id);
    }
    // The monitor index the probe enumerated; an empty choice is the primary display.
    let monitor = settings.get_str("monitor");
    env.shift_remove(var("monitor"));
    if digits(&monitor) {
        env.insert(var("monitor").into(), monitor);
    }
    for key in [
        "background_warmup",
        "parallel_warmup",
        "pipeline_binary_cache",
        "perf_diag",
        "partial_height_views",
        "disk_shaders",
        "shader_prefetch",
        "shader_notes",
    ] {
        put_switch(&mut env, settings, key);
    }
    for key in [
        "frame_stats",
        "gpu_profile",
        "vk_validation",
        "frame_capture_key",
    ] {
        put_if_on(&mut env, settings, key);
    }
    // The key and pad maps are derived from the saved bindings (no 1:1 catalog row).
    let no_controls = Value::Object(serde_json::Map::new());
    let controls = settings.get("controls").unwrap_or(&no_controls);
    let value = keyboard_map(controls, settings.flag("rage_one_input"));
    if !value.is_empty() {
        env.insert("SPARTAN_KEY_MAP".into(), value);
    }
    let value = control_map(controls, ControlKind::Pad);
    if !value.is_empty() {
        env.insert("SPARTAN_PAD_MAP".into(), value);
    }
    // A saved value that is not a choice, or is the runner's own default, is left out.
    let put_choice = |env: &mut IndexMap<String, String>, key: &str| {
        let name = var(key);
        let value = settings.get_str(key);
        env.shift_remove(name);
        let listed = choices(key).iter().any(|choice| choice.value == value);
        if listed && run_default(name) != Some(value.as_str()) {
            env.insert(name.into(), value);
        }
    };
    put_choice(&mut env, "stick_prompts");
    put_choice(&mut env, "glyph_style");
    env.insert("PYTHONUNBUFFERED".into(), "1".into());
    env.insert("PYTHONIOENCODING".into(), "utf-8".into());
    env
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::core::settings::app_defaults;
    use serde_json::json;

    fn variables(pairs: &[(&str, &str)]) -> IndexMap<String, String> {
        pairs
            .iter()
            .map(|(key, value)| (key.to_string(), value.to_string()))
            .collect()
    }

    #[test]
    fn windows_hides_the_amd_switchable_layer() {
        let mut env = variables(&[("GOW3_DATA_DIR", "/old")]);
        start_up_environment(&mut env, true);
        assert_eq!(
            env.get("VK_LOADER_LAYERS_DISABLE").map(String::as_str),
            Some("VK_LAYER_AMD_switchable_graphics")
        );
        // The old names are forwarded in the same step.
        assert_eq!(
            env.get("SPARTAN_DATA_DIR").map(String::as_str),
            Some("/old")
        );
        let mut env = IndexMap::new();
        start_up_environment(&mut env, false);
        assert!(!env.contains_key("VK_LOADER_LAYERS_DISABLE"));
    }

    #[test]
    fn a_value_set_by_the_player_is_kept() {
        let mut env = variables(&[("VK_LOADER_LAYERS_DISABLE", "VK_LAYER_custom")]);
        start_up_environment(&mut env, true);
        assert_eq!(env["VK_LOADER_LAYERS_DISABLE"], "VK_LAYER_custom");
    }

    #[test]
    fn the_game_starts_from_the_whole_environment() {
        // Python `dict(os.environ)`: the variables a bundle sets for itself are passed too (a
        // newer Mesa through LD_LIBRARY_PATH reaches the game).
        let pairs = [
            ("LD_LIBRARY_PATH", "/opt/mesa"),
            ("PYTHONPATH", "/py"),
            ("TCL_LIBRARY", "/tcl"),
            ("PATH", "/bin"),
        ];
        let env = environment_of(pairs);
        assert_eq!(env, variables(&pairs));
    }

    #[test]
    fn the_game_gets_the_vulkan_layer_setting() {
        let (settings, paths, _dir) = setup(json!({}));
        let mut base = IndexMap::new();
        start_up_environment(&mut base, true);
        let env = game_environment(&settings, &paths, "en", &base, true);
        assert_eq!(
            env.get("VK_LOADER_LAYERS_DISABLE").map(String::as_str),
            Some("VK_LAYER_AMD_switchable_graphics")
        );
    }

    fn setup(edits: Value) -> (Settings, Paths, tempfile::TempDir) {
        let dir = tempfile::tempdir().unwrap();
        let data = dir.path().to_string_lossy().into_owned();
        let env = move |key: &str| (key == "SPARTAN_DATA_DIR").then(|| data.clone());
        let paths = Paths::from_env(dir.path(), &env);
        let mut settings = Settings(app_defaults(&|_| None));
        if let Value::Object(map) = edits {
            for (key, value) in map {
                settings.set(&key, value);
            }
        }
        (settings, paths, dir)
    }

    fn base(pairs: &[(&str, &str)]) -> IndexMap<String, String> {
        pairs
            .iter()
            .map(|(k, v)| (k.to_string(), v.to_string()))
            .collect()
    }

    #[test]
    fn inherited_gpu_id_and_monitor_are_dropped_when_not_digits() {
        let (settings, paths, _dir) = setup(json!({"gpu_id": "abc", "monitor": "-1"}));
        let inherited = base(&[("SPARTAN_GPU_ID", "7"), ("SPARTAN_MONITOR", "3")]);
        let env = game_environment(&settings, &paths, "en", &inherited, true);
        assert!(!env.contains_key("SPARTAN_GPU_ID"));
        assert!(!env.contains_key("SPARTAN_MONITOR"));

        let (settings, paths, _dir) = setup(json!({"gpu_id": "2", "monitor": "1"}));
        let env = game_environment(&settings, &paths, "en", &inherited, true);
        assert_eq!(env["SPARTAN_GPU_ID"], "2");
        assert_eq!(env["SPARTAN_MONITOR"], "1");
    }

    #[test]
    fn discord_rich_presence_is_on_by_default() {
        // R91: the default turns it on, and a file without the key (an older launcher's) too.
        let (settings, paths, _dir) = setup(json!({}));
        let env = game_environment(&settings, &paths, "en", &IndexMap::new(), true);
        assert_eq!(env["SPARTAN_DISCORD"], "1");
        let mut without = settings.clone();
        without.0.shift_remove("discord_rpc");
        let env = game_environment(&without, &paths, "en", &IndexMap::new(), true);
        assert_eq!(env["SPARTAN_DISCORD"], "1");
        let (off, paths, _dir) = setup(json!({"discord_rpc": false}));
        let env = game_environment(&off, &paths, "en", &IndexMap::new(), true);
        assert_eq!(env["SPARTAN_DISCORD"], "0");
    }

    #[test]
    fn shader_notes_are_off_without_a_saved_choice() {
        let (settings, paths, _dir) = setup(json!({}));
        assert_eq!(settings.get("shader_notes"), Some(&json!(false)));
        let mut without = settings.clone();
        without.0.shift_remove("shader_notes");
        let env = game_environment(&without, &paths, "en", &IndexMap::new(), true);
        assert_eq!(env["SPARTAN_SHADER_NOTES"], "0");
    }

    #[test]
    fn saved_shader_notes_choice_is_preserved() {
        for enabled in [true, false] {
            let dir = tempfile::tempdir().unwrap();
            let data = dir.path().to_string_lossy().into_owned();
            let env = move |key: &str| (key == "SPARTAN_DATA_DIR").then(|| data.clone());
            let paths = Paths::from_env(dir.path(), &env);
            std::fs::write(
                dir.path().join("settings.json"),
                format!(r#"{{"shader_notes": {enabled}}}"#),
            )
            .unwrap();
            let settings = crate::core::settings::load_app_settings(&paths);
            assert_eq!(settings.get("shader_notes"), Some(&json!(enabled)));
            let env = game_environment(&settings, &paths, "en", &IndexMap::new(), true);
            assert_eq!(env["SPARTAN_SHADER_NOTES"], if enabled { "1" } else { "0" });
        }
    }

    #[test]
    fn shader_cache_switches_reach_the_game() {
        let (settings, paths, _dir) = setup(json!({}));
        let env = game_environment(&settings, &paths, "en", &IndexMap::new(), true);
        assert_eq!(
            (
                env["SPARTAN_DISK_SHADERS"].as_str(),
                env["SPARTAN_SHADER_PREFETCH"].as_str(),
                env["SPARTAN_SHADER_NOTES"].as_str()
            ),
            ("1", "1", "0")
        );
        // In the Python's order: right after the screen effects switch.
        let keys: Vec<&str> = env.keys().map(String::as_str).collect();
        let at = keys
            .iter()
            .position(|key| *key == "SPARTAN_PARTIAL_HEIGHT_VIEWS")
            .unwrap();
        assert_eq!(
            &keys[at + 1..at + 4],
            [
                "SPARTAN_DISK_SHADERS",
                "SPARTAN_SHADER_PREFETCH",
                "SPARTAN_SHADER_NOTES"
            ]
        );
        let (off, paths, _dir) = setup(json!({
            "disk_shaders": false, "shader_prefetch": false, "shader_notes": false
        }));
        let env = game_environment(&off, &paths, "en", &IndexMap::new(), true);
        assert_eq!(
            (
                env["SPARTAN_DISK_SHADERS"].as_str(),
                env["SPARTAN_SHADER_PREFETCH"].as_str(),
                env["SPARTAN_SHADER_NOTES"].as_str()
            ),
            ("0", "0", "0")
        );
    }

    #[test]
    fn stream_memory_falls_back_to_cached() {
        let (settings, paths, _dir) = setup(json!({"stream_memory": "weird"}));
        let inherited = base(&[("SPARTAN_STREAM_MEMORY", "device")]);
        let env = game_environment(&settings, &paths, "en", &inherited, true);
        assert_eq!(env["SPARTAN_STREAM_MEMORY"], "cached");
        let (settings, paths, _dir) = setup(json!({"stream_memory": "host"}));
        let env = game_environment(&settings, &paths, "en", &inherited, true);
        assert_eq!(env["SPARTAN_STREAM_MEMORY"], "host");
    }

    #[test]
    fn trophy_texts_file_has_35_rows_and_the_labels() {
        let dir = tempfile::tempdir().unwrap();
        let folder = dir.path().join("user");
        write_trophy_texts(&folder, "pt");
        let text = fs::read_to_string(folder.join("trophy_texts.txt")).unwrap();
        assert!(text.ends_with('\n'));
        let lines: Vec<&str> = text.lines().collect();
        assert_eq!(lines.len(), 45);
        assert!(lines[0].starts_with("# Written by the launcher on every start (language: pt)"));
        assert_eq!(lines[1], format!("@heading={}", tr("pt", "Trophies")));
        let (name, _) = trophy_text("pt", 0);
        assert!(lines[10].starts_with(&format!("0={name} | ")));
        assert!(lines[44].starts_with("34="));
    }

    #[test]
    fn game_environment_honors_inherited_spartan_user_dir() {
        let dir = tempfile::tempdir().unwrap();
        let user_folder = dir.path().join("inherited_user");
        let user_str = user_folder.to_string_lossy().into_owned();
        let user_clone = user_str.clone();
        let env_fn = move |key: &str| (key == "SPARTAN_USER_DIR").then(|| user_clone.clone());
        let paths = Paths::from_env(dir.path(), &env_fn);
        let settings = Settings(app_defaults(&|_| None));
        let inherited = base(&[("SPARTAN_USER_DIR", &user_str)]);

        let env = game_environment(&settings, &paths, "en", &inherited, true);
        assert_eq!(
            env.get("SPARTAN_USER_DIR").map(String::as_str),
            Some(user_str.as_str())
        );
        assert!(user_folder.join("trophy_texts.txt").is_file());
    }

    #[test]
    fn game_environment_honors_explicit_user_dir_over_inherited_env() {
        let dir = tempfile::tempdir().unwrap();
        let inherited_folder = dir.path().join("inherited_user");
        let custom_folder = dir.path().join("custom_user");
        let inherited_str = inherited_folder.to_string_lossy().into_owned();
        let custom_str = custom_folder.to_string_lossy().into_owned();
        let inh_clone = inherited_str.clone();
        let env_fn = move |key: &str| (key == "SPARTAN_USER_DIR").then(|| inh_clone.clone());
        let paths = Paths::from_env(dir.path(), &env_fn);

        let (mut settings, _, _temp) = setup(json!({}));
        settings.set("user_dir", json!(custom_str));
        let inherited = base(&[("SPARTAN_USER_DIR", &inherited_str)]);

        let env = game_environment(&settings, &paths, "en", &inherited, true);
        assert_eq!(
            env.get("SPARTAN_USER_DIR").map(String::as_str),
            Some(custom_str.as_str())
        );
        assert!(custom_folder.join("trophy_texts.txt").is_file());
        assert!(!inherited_folder.join("trophy_texts.txt").exists());
    }
}
