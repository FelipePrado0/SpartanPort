#![allow(clippy::unwrap_used, clippy::expect_used)]
//! The runner (`--run`, `--after`, `--play`, the in-game restart) on the synthetic game of the
//! `prepare` fixture: the real game is never started. Most runs are in process on a [`FakeHost`],
//! which records every tool and game start and answers for them; the in-game restart, Ctrl+C, the
//! probe's working folder and `--play` start the launcher, with `fake-probe` in place of
//! `spartanport-probe`.

use std::ffi::OsString;
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

use indexmap::IndexMap;
use serde_json::json;
use spartanport_launcher::core::controller_mappings::{mapping_line, FILENAME};
use spartanport_launcher::core::log::ExitHint;
use spartanport_launcher::core::paths::Paths;
use spartanport_launcher::core::platform::tool::{run_tool, Stderr, ToolCall, ToolError, Window};
use spartanport_launcher::core::settings::Settings;
use spartanport_launcher::runner::{
    isa_rewrite, play_with, restart_command, run_with, Host, RunContext, SystemHost, AFTER_LIMIT,
    GAMEPAD_PROBE_LIMIT,
};
use spartanport_launcher::startup::{start, Context};

#[path = "common/host.rs"]
mod host;
use host::{Called, FakeHost, Interrupt};

/// The launcher program cargo built for these tests.
const LAUNCHER: &str = env!("CARGO_BIN_EXE_spartanport-launcher");
const FAKE_PROBE: &str = env!("CARGO_BIN_EXE_fake-probe");
const FAKE_GAME: &str = env!("CARGO_BIN_EXE_fake-game");
const EXE: &str = if cfg!(windows) { ".exe" } else { "" };

fn manifest() -> &'static Path {
    Path::new(env!("CARGO_MANIFEST_DIR"))
}

/// Python's `Path.resolve()` of something that exists: links followed, long names, no `\\?\`
/// (`\\?\UNC\server\share` is `\\server\share`).
fn resolved(path: &Path) -> PathBuf {
    let path = fs::canonicalize(path).unwrap();
    let text = path.to_string_lossy().into_owned();
    if let Some(rest) = text.strip_prefix(r"\\?\UNC\") {
        return PathBuf::from(format!(r"\\{rest}"));
    }
    match text.strip_prefix(r"\\?\") {
        Some(rest) => PathBuf::from(rest),
        None => path,
    }
}

/// `path`, on a local drive, through this machine's admin share (`\\localhost\C$\...`): a
/// network share without a network. `None` where the share is not served.
#[cfg(windows)]
fn through_admin_share(path: &Path) -> Option<PathBuf> {
    let (drive, rest) = path.to_str()?.split_once(r":\")?;
    let shared = PathBuf::from(format!(r"\\localhost\{drive}$\{rest}"));
    shared.exists().then_some(shared)
}

fn copy_tree(from: &Path, to: &Path) {
    fs::create_dir_all(to).unwrap();
    for entry in fs::read_dir(from).unwrap() {
        let entry = entry.unwrap();
        let target = to.join(entry.file_name());
        if entry.file_type().unwrap().is_dir() {
            copy_tree(&entry.path(), &target);
        } else {
            fs::copy(entry.path(), &target).unwrap();
        }
    }
}

fn copy_into(from: &Path, folder: &Path) {
    fs::create_dir_all(folder).unwrap();
    fs::copy(from, folder.join(from.file_name().unwrap())).unwrap();
}

/// A port folder as a package holds it: `run.py`, an empty probe in `bin/` (an in-process run
/// never starts it; [`Port::with_probe`] puts `fake-probe` there), the icon and the built-in
/// patches, with the synthetic game of the `prepare` fixture (plus the `link` fixture's
/// `libSceFios2.prx`) as `game/`.
struct Port {
    _dir: tempfile::TempDir,
    root: PathBuf,
}

impl Port {
    fn new() -> Port {
        Port::in_folder("")
    }

    /// A port whose probe is `fake-probe`, for the runs that start the launcher.
    fn with_probe() -> Port {
        let port = Port::new();
        fs::copy(FAKE_PROBE, port.probe()).unwrap();
        port
    }

    /// A port in the folder `name` of a temporary folder (`""`: the temporary folder itself).
    fn in_folder(name: &str) -> Port {
        let dir = tempfile::tempdir().unwrap();
        let root = resolved(dir.path()).join(name);
        fs::create_dir_all(&root).unwrap();
        fs::write(root.join("run.py"), b"").unwrap();
        fs::create_dir_all(root.join("bin")).unwrap();
        fs::write(
            root.join("bin").join(format!("spartanport-probe{EXE}")),
            b"",
        )
        .unwrap();
        let repo = manifest().join("..");
        copy_into(
            &repo.join("launcher/spartanport.png"),
            &root.join("launcher"),
        );
        copy_into(&repo.join("patches/CUSA01623.xml"), &root.join("patches"));
        let fixtures = manifest().join("fixtures/runner");
        copy_tree(&fixtures.join("prepare/input"), &root.join("game"));
        copy_into(
            &fixtures.join("link/input/sce_module/libSceFios2.prx"),
            &root.join("game/sce_module"),
        );
        Port { _dir: dir, root }
    }

    fn game(&self) -> PathBuf {
        self.root.join("game")
    }

    fn out(&self) -> PathBuf {
        self.root.join("out")
    }

    fn probe(&self) -> PathBuf {
        self.root
            .join("bin")
            .join(format!("spartanport-probe{EXE}"))
    }

    /// The environment a run in this port gets: [`clean_environment`], the port, its game and no
    /// Discord presence, then `extra`.
    fn environment(&self, extra: &[(&str, &str)]) -> IndexMap<String, String> {
        let mut env = clean_environment();
        let text = |path: PathBuf| path.to_string_lossy().into_owned();
        env.insert("SPARTAN_PORT_DIR".into(), text(self.root.clone()));
        env.insert("SPARTAN_GAME_DIR".into(), text(self.game()));
        env.insert("SPARTAN_DISCORD".into(), "0".into());
        for (key, value) in extra {
            env.insert(key.to_string(), value.to_string());
        }
        env
    }

    /// What the program starts from in this port: [`Port::environment`] as `startup::start` makes
    /// it, with the folders.
    fn program(&self, extra: &[(&str, &str)]) -> Context {
        let vars = self
            .environment(extra)
            .into_iter()
            .map(|(key, value)| (OsString::from(key), OsString::from(value)));
        start(&[OsString::from("--run")], &self.root, vars, cfg!(windows)).1
    }

    /// The context of an in-process run in this port: [`Port::environment`] and no arguments.
    fn context<'a>(program: &'a Context, out: &'a mut dyn Write) -> RunContext<'a> {
        RunContext {
            context: program,
            args: Vec::new(),
            out,
        }
    }

    /// An in-process run in this port on `host`, with `extra` variables and `args` after `--run`.
    fn run_on(&self, host: FakeHost, extra: &[(&str, &str)], args: &[&str]) -> InProcess {
        let mut out = Vec::new();
        let program = self.program(extra);
        let code = run_with(
            RunContext {
                context: &program,
                args: args.iter().map(OsString::from).collect(),
                out: &mut out,
            },
            &host,
        );
        InProcess {
            code,
            out: String::from_utf8_lossy(&out).into_owned(),
            host,
        }
    }

    /// `LAUNCHER --run <args>` in this port with `extra` variables.
    fn run(&self, extra: &[(&str, &str)], args: &[&str]) -> Ran {
        let output = Command::new(LAUNCHER)
            .arg("--run")
            .args(args)
            .env_clear()
            .envs(self.environment(extra))
            .stdin(Stdio::null())
            .output()
            .unwrap();
        Ran {
            code: output.status.code(),
            stdout: String::from_utf8_lossy(&output.stdout).into_owned(),
            stderr: String::from_utf8_lossy(&output.stderr).into_owned(),
        }
    }
}

/// This process's environment without the variables a run reads (`SPARTAN_*`, the old `GOW3_*`
/// names and the fake probe's), so the developer's own settings never reach a test.
fn clean_environment() -> IndexMap<String, String> {
    std::env::vars_os()
        .filter_map(|(key, value)| Some((key.into_string().ok()?, value.into_string().ok()?)))
        .filter(|(key, _)| {
            let key = key.to_uppercase();
            !["SPARTAN_", "GOW3_", "FAKE_PROBE_"]
                .iter()
                .any(|prefix| key.starts_with(prefix))
        })
        .collect()
}

/// What an in-process run gave: its exit code, what it printed and its host's record.
struct InProcess {
    code: i32,
    out: String,
    host: FakeHost,
}

impl InProcess {
    /// The argv of every game start (the calls that are not `--cpu-features` or `--gamepad`), the
    /// program first.
    fn games(&self) -> Vec<Vec<String>> {
        self.host
            .calls
            .borrow()
            .iter()
            .filter(|call| is_game(call))
            .map(argv)
            .collect()
    }

    /// The one game start.
    fn game_call(&self) -> Called {
        let calls = self.host.calls.borrow();
        let games: Vec<&Called> = calls.iter().filter(|call| is_game(call)).collect();
        assert_eq!(games.len(), 1, "{self}");
        games[0].clone()
    }

    /// The argv of the one game start, the program first.
    fn game(&self) -> Vec<String> {
        argv(&self.game_call())
    }

    /// The variable `name` in the environment the game got.
    fn var(&self, name: &str) -> Option<String> {
        let env = self.game_call().env;
        let env = env.unwrap_or_else(|| panic!("the game got no environment\n{self}"));
        env.get(name).cloned()
    }
}

impl std::fmt::Display for InProcess {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        writeln!(f, "code {}\n--- out\n{}--- calls", self.code, self.out)?;
        for call in self.host.calls.borrow().iter() {
            writeln!(f, "{:?}", argv(call))?;
        }
        Ok(())
    }
}

fn is_game(call: &Called) -> bool {
    let first = call.args.first().and_then(|arg| arg.to_str());
    !matches!(first, Some("--cpu-features" | "--gamepad"))
}

fn argv(call: &Called) -> Vec<String> {
    std::iter::once(&call.program)
        .chain(&call.args)
        .map(|arg| arg.to_string_lossy().into_owned())
        .collect()
}

/// A host whose game reads `file` under its `--app0` folder, as `FAKE_PROBE_SHOW` did, and the
/// text it read (`missing` without the file) once the game has started.
fn showing(file: &'static str) -> (FakeHost, Arc<Mutex<Option<String>>>) {
    let shown = Arc::new(Mutex::new(None));
    let into = Arc::clone(&shown);
    let mut host = FakeHost::default();
    host.on_play = Some(Box::new(move |call: &ToolCall| {
        let at = call.args.iter().position(|arg| arg == "--app0").unwrap();
        let text = fs::read_to_string(Path::new(&call.args[at + 1]).join(file))
            .unwrap_or_else(|_| "missing".to_string());
        *into.lock().unwrap() = Some(text);
    }));
    (host, shown)
}

/// What a run of the launcher gave.
struct Ran {
    code: Option<i32>,
    stdout: String,
    stderr: String,
}

impl std::fmt::Display for Ran {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "code {:?}\n--- stdout\n{}--- stderr\n{}",
            self.code, self.stdout, self.stderr
        )
    }
}

/// The argv of every game start the fake probe printed, in order.
fn probe_runs(stdout: &str) -> Vec<Vec<String>> {
    let mut runs = Vec::new();
    let mut lines = stdout.lines();
    while lines.any(|line| line == "fake-probe argv:") {
        let argv = lines
            .by_ref()
            .take_while(|line| *line != "fake-probe argv end")
            .map(str::to_string)
            .collect();
        runs.push(argv);
    }
    runs
}

/// Every value the fake probe printed for the variable `name`, in order.
fn printed(stdout: &str, name: &str) -> Vec<String> {
    let prefix = format!("{name}=");
    stdout
        .lines()
        .filter_map(|line| line.strip_prefix(&prefix))
        .map(str::to_string)
        .collect()
}

fn text(path: &Path) -> String {
    path.to_string_lossy().into_owned()
}

/// The folder the probe got after `--app0`.
fn app0(argv: &[String]) -> PathBuf {
    let at = argv.iter().position(|arg| arg == "--app0").unwrap();
    PathBuf::from(&argv[at + 1])
}

/// Asserts that lines holding each of `parts` appear in `stdout` in this order.
#[track_caller]
fn in_order(stdout: &str, parts: &[&str]) {
    let all: Vec<&str> = stdout.lines().collect();
    let mut from = 0;
    for wanted in parts {
        match all[from..].iter().position(|line| line.contains(wanted)) {
            Some(at) => from += at + 1,
            None => panic!("no line {wanted:?} after line {from} in\n{stdout}"),
        }
    }
}

#[test]
fn run_writes_every_derived_file_and_starts_the_probe_with_the_python_command_line() {
    let port = Port::new();
    let ran = port.run_on(
        FakeHost::default(),
        &[("SPARTAN_TIMEOUT", "5")],
        &["--fullscreen", "two words"],
    );
    assert_eq!(ran.code, 0, "{ran}");
    let out = port.out();
    for name in [
        "boot.bin",
        "eboot.elf",
        "entry.bin",
        "analysis.json",
        "boot-libc.bin",
        "libc-link.json",
        "boot-linked.bin",
        "link.json",
        "content.bin",
        "content-profile.json",
        "patches.bin",
        "patch_status.json",
        "graphics_state.json",
        "window_icon.bmp",
    ] {
        assert!(out.join(name).is_file(), "out/{name}\n{ran}");
    }
    // run.py's command line (spec §5 step 16), the arguments after --run at its end.
    let user = resolved(&port.root.join("user"));
    let expected: Vec<String> = [
        text(&port.probe()),
        text(&out.join("boot-linked.bin")),
        "--content-profile".into(),
        text(&out.join("content.bin")),
        "--patches".into(),
        text(&out.join("patches.bin")),
        "--app0".into(),
        text(&resolved(&port.game())),
        "--user".into(),
        text(&user),
        "--timeout".into(),
        "5".into(),
        "--fullscreen".into(),
        "two words".into(),
    ]
    .into();
    assert_eq!(ran.games(), [expected], "{ran}");
    // What this run derived, so "Apply and restart" derives it again from the new options.
    let var = |name: &str| ran.var(name);
    assert_eq!(
        var("SPARTAN_DERIVED").as_deref(),
        Some("SPARTAN_EXTRA_RESOLUTIONS,SPARTAN_VBLANK_HZ,SPARTAN_ENGINE_FPS")
    );
    assert_eq!(var("SPARTAN_EXTRA_RESOLUTIONS").as_deref(), Some(""));
    assert_eq!(var("SPARTAN_VBLANK_HZ").as_deref(), Some("60"));
    assert_eq!(var("SPARTAN_ENGINE_FPS").as_deref(), Some("60"));
    assert_eq!(var("SPARTAN_DMEM_MB"), None, "{ran}");
    assert_eq!(var("SPARTAN_GLYPHS"), None, "{ran}");
    assert_eq!(
        var("SPARTAN_CONFIG"),
        Some(text(&port.root.join("spartanport.ini")))
    );
    // The game reads spartanport.ini's upscaler (Off by default); the runner no longer forces it.
    assert_eq!(var("SPARTAN_UPSCALER"), None);
    assert_eq!(var("SPARTAN_READBACK_LINEAR").as_deref(), Some("1"));
    assert_eq!(
        var("SPARTAN_WINDOW_ICON"),
        Some(text(&out.join("window_icon.bmp")))
    );
    assert_eq!(var("SPARTAN_GPU_USER_DIR"), Some(text(&user)));
    assert_eq!(var("SPARTAN_PATCHES_ONLY").as_deref(), Some("1"));
    for name in [
        "SPARTAN_DEFERRED_READBACK",
        "SPARTAN_STALE_READBACK",
        "SPARTAN_ASYNC_SHADERS",
    ] {
        assert_eq!(var(name).as_deref(), Some("1"), "{name}");
    }
    // The lines of the Python's steps, in its order, before the game's.
    in_order(
        &ran.out,
        &[
            "ゴッド・オブ・ウォーIII リマスタード | entry=0x",
            " imported symbols; ",
            "Unavailable non-loadable metadata headers: ",
            &format!("Output: {}", out.display()),
            "libc _init_env verified RET: ",
            "Linked native libc: base=0x",
            "Linked modules: libc.prx@0x",
            "AppContent profile: SKU=full (probe setting), user params=[",
            "Patches: CUSA01623 01.02, eboot.bin sha256 ",
            "Patches: CUSA01623; ",
            "Engine frame rate: 60 FPS, live up to 60 (the frame rate patch was not found in this eboot.bin)",
        ],
    );
}

#[test]
fn a_restarted_run_derives_the_inherited_values_again() {
    // "Apply and restart": the new runner inherits the old one's environment, its derived values
    // included. The keys SPARTAN_DERIVED lists are derived again from the new options; the same
    // keys set by the player (not listed) are kept, as run.py's `setdefault`s keep them.
    let port = Port::new();
    let first = port.run_on(FakeHost::default(), &[], &[]);
    assert_eq!(first.code, 0, "{first}");
    let derived = first.var("SPARTAN_DERIVED").unwrap();
    assert_eq!(
        derived,
        "SPARTAN_EXTRA_RESOLUTIONS,SPARTAN_VBLANK_HZ,SPARTAN_ENGINE_FPS"
    );
    // What a start with other options left: a mod's size, a patch's memory and VBlank needs and
    // its frame rate (SPARTAN_DMEM_MB is derived only when a patch needs it).
    let stale = [
        ("SPARTAN_EXTRA_RESOLUTIONS", "3440x1440"),
        ("SPARTAN_VBLANK_HZ", "144"),
        ("SPARTAN_DMEM_MB", "12288"),
        ("SPARTAN_ENGINE_FPS", "120"),
    ];
    let listed = format!("{derived},SPARTAN_DMEM_MB");
    let restarted = port.run_on(
        FakeHost::default(),
        &[&stale[..], &[("SPARTAN_DERIVED", &listed)]].concat(),
        &[],
    );
    assert_eq!(restarted.code, 0, "{restarted}");
    let var = |ran: &InProcess, name: &str| ran.var(name);
    assert_eq!(var(&restarted, "SPARTAN_DERIVED"), Some(derived));
    assert_eq!(
        var(&restarted, "SPARTAN_EXTRA_RESOLUTIONS").as_deref(),
        Some("")
    );
    assert_eq!(var(&restarted, "SPARTAN_VBLANK_HZ").as_deref(), Some("60"));
    assert_eq!(var(&restarted, "SPARTAN_ENGINE_FPS").as_deref(), Some("60"));
    assert_eq!(var(&restarted, "SPARTAN_DMEM_MB"), None, "{restarted}");
    // Not listed: the player's own VBlank rate and memory stay.
    let own = port.run_on(FakeHost::default(), &stale, &[]);
    assert_eq!(own.code, 0, "{own}");
    assert_eq!(var(&own, "SPARTAN_VBLANK_HZ").as_deref(), Some("144"));
    assert_eq!(var(&own, "SPARTAN_DMEM_MB").as_deref(), Some("12288"));
    assert_eq!(var(&own, "SPARTAN_EXTRA_RESOLUTIONS").as_deref(), Some(""));
    assert_eq!(var(&own, "SPARTAN_ENGINE_FPS").as_deref(), Some("60"));
}

#[test]
fn the_probes_exit_code_is_returned() {
    // tests/test_mods.py::test_run_uses_overlay_propagates_exit_and_cleans_view: the probe gets
    // the overlay with the mod's file, its exit code is the runner's and the overlay goes after.
    let port = Port::new();
    let wad = port.game().join("dvdroot_ps4/PART1.WAD");
    let original = fs::read(&wad).unwrap();
    let mod_wad = port.root.join("mods/A/dvdroot_ps4/PART1.WAD");
    fs::create_dir_all(mod_wad.parent().unwrap()).unwrap();
    fs::write(&mod_wad, b"mod").unwrap();
    let (mut host, shown) = showing("dvdroot_ps4/PART1.WAD");
    host.exit = 7;
    let ran = port.run_on(host, &[], &[]);
    assert_eq!(ran.code, 7, "{ran}");
    let overlay = app0(&ran.game());
    assert!(
        overlay
            .file_name()
            .unwrap()
            .to_string_lossy()
            .starts_with("mod-game-"),
        "{ran}"
    );
    assert_eq!(overlay.parent(), Some(port.out().as_path()));
    assert_eq!(shown.lock().unwrap().as_deref(), Some("mod"), "{ran}");
    assert!(!overlay.exists(), "the overlay is left in out/");
    assert_eq!(fs::read(&wad).unwrap(), original);
}

#[test]
fn an_unsupported_cpu_stops_with_20() {
    let port = Port::new();
    let mut host = FakeHost::default();
    host.cpu = "CPU: missing AVX2\n".into();
    host.cpu_exit = 20;
    let ran = port.run_on(host, &[], &[]);
    assert_eq!(ran.code, 20, "{ran}");
    assert!(ran.out.contains("CPU: missing AVX2\n"), "{ran}");
    assert!(ran.games().is_empty(), "{ran}");
    assert!(!port.out().join("boot.bin").exists());
}

#[test]
fn direct_run_loads_profile_before_native_detection() {
    // The CPU check, `--gamepad` and the game all see the data folder's controller profiles.
    let port = Port::new();
    let data = port.root.join("profiles");
    fs::create_dir_all(&data).unwrap();
    let bindings = [("a".to_string(), "b2".to_string())].into_iter().collect();
    let profile = mapping_line(
        "0300457e790000000600000000000000",
        "USB",
        &bindings,
        "Windows",
        true,
    )
    .unwrap();
    fs::write(data.join(FILENAME), &profile).unwrap();
    let mut host = FakeHost::default();
    host.cpu = "CPU check stopped for test\n".into();
    host.cpu_exit = 20;
    let mut program = port.program(&[("SPARTAN_DATA_DIR", &text(&data))]);
    for key in ["SDL_GAMECONTROLLERCONFIG", "SDL_GAMECONTROLLERCONFIG_FILE"] {
        program.env.shift_remove(key);
    }
    let mut out = Vec::new();
    let code = run_with(Port::context(&program, &mut out), &host);
    let out = String::from_utf8_lossy(&out);
    assert_eq!(code, 20, "{out}");
    let calls = host.calls.borrow();
    let seen: Vec<(String, Option<String>)> = calls
        .iter()
        .map(|call| {
            let first = argv(call).get(1).cloned().unwrap_or_default();
            let env = call.env.as_ref();
            (
                first,
                env.and_then(|env| env.get("SDL_GAMECONTROLLERCONFIG").cloned()),
            )
        })
        .collect();
    assert_eq!(seen, [("--cpu-features".to_string(), Some(profile))]);
}

#[test]
fn a_relative_controller_file_is_the_ports() {
    // run.py moved to the port folder first, so a relative SDL_GAMECONTROLLERCONFIG_FILE named a
    // file there; the explicit file still comes after the data folder's profile.
    let port = Port::new();
    let data = port.root.join("profiles");
    fs::create_dir_all(&data).unwrap();
    let guid = "0300457e790000000600000000000000";
    let line = |button: &str| {
        let bindings = [("a".to_string(), button.to_string())]
            .into_iter()
            .collect();
        mapping_line(guid, "USB", &bindings, "Windows", true).unwrap()
    };
    let (profile, custom) = (line("b2"), line("b3"));
    fs::write(data.join(FILENAME), &profile).unwrap();
    fs::write(port.root.join("custom.txt"), &custom).unwrap();
    let mut host = FakeHost::default();
    host.cpu = "CPU check stopped for test\n".into();
    host.cpu_exit = 20;
    let mut program = port.program(&[
        ("SPARTAN_DATA_DIR", &text(&data)),
        ("SDL_GAMECONTROLLERCONFIG_FILE", "custom.txt"),
    ]);
    program.env.shift_remove("SDL_GAMECONTROLLERCONFIG");
    let mut out = Vec::new();
    let code = run_with(Port::context(&program, &mut out), &host);
    let out = String::from_utf8_lossy(&out);
    assert_eq!(code, 20, "{out}");
    let calls = host.calls.borrow();
    let env = calls[0].env.as_ref().unwrap();
    assert_eq!(
        env.get("SDL_GAMECONTROLLERCONFIG"),
        Some(&format!("{profile}\n{custom}"))
    );
}

#[test]
fn a_missing_eboot_fails_with_the_python_message() {
    let port = Port::new();
    let empty = port.root.join("not a game");
    fs::create_dir_all(&empty).unwrap();
    let ran = port.run_on(
        FakeHost::default(),
        &[("SPARTAN_GAME_DIR", &text(&empty))],
        &[],
    );
    assert_eq!(ran.code, 1, "{ran}");
    let message = format!(
        "No eboot.bin in {} (set SPARTAN_GAME_DIR).",
        empty.display()
    );
    assert!(ran.out.contains(&message), "{ran}");
    assert!(ran.games().is_empty(), "{ran}");
}

#[cfg(windows)]
#[test]
fn a_game_on_a_share_reaches_the_probe_as_python_spells_it() {
    // \\server\share\..., as Python's resolve() gives it: the probe opens "<app0>/..." paths, and
    // Windows does not normalize a / under \\?\UNC\.
    let port = Port::new();
    let Some(game) = through_admin_share(&port.game()) else {
        eprintln!(r"skipped: \\localhost\ is not reachable");
        return;
    };
    let ran = port.run_on(
        FakeHost::default(),
        &[("SPARTAN_GAME_DIR", &text(&game))],
        &[],
    );
    assert_eq!(ran.code, 0, "{ran}");
    let app0 = app0(&ran.game());
    assert!(text(&app0).starts_with(r"\\localhost\"), "{ran}");
    assert_eq!(app0, resolved(&game), "{ran}");
}

#[test]
fn a_broken_mods_json_stops_with_the_mods_message() {
    // run.py:192 reads the selection before anything else and stops on its error.
    let port = Port::new();
    fs::write(port.root.join("mods.json"), "{").unwrap();
    let host = FakeHost::default();
    let mut out = Vec::new();
    let code = run_with(Port::context(&port.program(&[]), &mut out), &host);
    let out = String::from_utf8_lossy(&out);
    assert_eq!(code, 1, "{out}");
    assert!(out.lines().any(|line| line.starts_with("Mods: ")), "{out}");
    // It stopped before the probe.
    assert!(host.calls.borrow().is_empty());
}

#[test]
fn a_relative_data_dir_is_from_the_port() {
    // Review Focus 2: the port has a space in its name, and no run changes the working folder.
    let port = Port::in_folder("port with space");
    let host = FakeHost::default();
    let mut out = Vec::new();
    let mut program = port.program(&[]);
    program
        .env
        .insert("SPARTAN_DATA_DIR".into(), "data2".into());
    let code = run_with(Port::context(&program, &mut out), &host);
    let out = String::from_utf8_lossy(&out);
    assert_eq!(code, 0, "{out}");
    assert!(port.root.join("data2/out/boot.bin").is_file(), "{out}");
    let calls = host.calls.borrow();
    assert!(!calls.is_empty());
    for call in calls.iter() {
        assert_eq!(call.cwd.as_deref(), Some(port.root.as_path()), "{call:?}");
    }
}

#[test]
fn run_starts_the_probe_in_the_port() {
    let port = Port::with_probe();
    let file = port.out().join("cwd.txt");
    let ran = port.run(&[("FAKE_PROBE_CWD", &text(&file))], &[]);
    assert_eq!(ran.code, Some(0), "{ran}");
    let cwd = fs::read_to_string(&file).unwrap();
    assert_eq!(resolved(Path::new(&cwd)), port.root, "{ran}");
}

#[test]
fn spartan_probe_is_the_program_started() {
    // run.py: SPARTAN_PROBE, taken as is, before the port's bin/ and out/.
    let port = Port::new();
    let probe = port.root.join("tools").join(format!("my-probe{EXE}"));
    fs::create_dir_all(probe.parent().unwrap()).unwrap();
    fs::write(&probe, b"").unwrap();
    // The port's bin/ has a probe too.
    let ran = port.run_on(
        FakeHost::default(),
        &[("SPARTAN_PROBE", &text(&probe))],
        &[],
    );
    assert_eq!(ran.code, 0, "{ran}");
    assert_eq!(ran.games().len(), 1, "{ran}");
    for call in ran.host.calls.borrow().iter() {
        assert_eq!(call.program, probe.clone().into_os_string(), "{ran}");
    }
    // Without it, and with no probe in the port, nothing starts.
    fs::remove_file(port.probe()).unwrap();
    let ran = port.run_on(FakeHost::default(), &[], &[]);
    assert_eq!(ran.code, 1, "{ran}");
    let message = format!("spartanport-probe{EXE} not found: build it with build.sh");
    assert!(ran.out.contains(&message), "{ran}");
    assert!(ran.host.calls.borrow().is_empty(), "{ran}");
}

/// Whether a run whose `--cpu-features` prints `cpu`, with `extra` variables, starts the game with
/// the ISA rewrite table.
fn gets_the_isa_table(cpu: &str, extra: &[(&str, &str)]) -> bool {
    let port = Port::new();
    let mut host = FakeHost::default();
    host.cpu = cpu.into();
    let ran = port.run_on(host, extra, &[]);
    assert_eq!(ran.code, 0, "{ran}");
    ran.game().iter().any(|arg| arg == "--isa-rewrite")
}

#[test]
fn a_missing_movbe_gets_the_isa_table() {
    // run.py's parse_cpu_features and isa_rewrite_wanted: by default the CPU decides.
    assert!(gets_the_isa_table("CPU: missing MOVBE, BMI1, LZCNT\n", &[]));
    assert!(!gets_the_isa_table("CPU: complete\n", &[]));
}

#[test]
fn isa_rewrite_0_skips_it() {
    let off = [("SPARTAN_ISA_REWRITE", "0")];
    assert!(!gets_the_isa_table("CPU: missing MOVBE, BMI1\n", &off));
}

#[test]
fn isa_rewrite_1_forces_it() {
    let on = [("SPARTAN_ISA_REWRITE", "1")];
    assert!(gets_the_isa_table("CPU: complete\n", &on));
}

#[test]
fn a_stale_isa_table_is_replaced() {
    // Review Focus: a table left by an older image (a game update) is rewritten from the current
    // boot-linked.bin, since the C loader refuses a table whose SHA-256 differs.
    let port = Port::new();
    fs::create_dir_all(port.out()).unwrap();
    let table = port.out().join("isa-rewrite.bin");
    fs::write(&table, b"the table of an older image").unwrap();
    // A CPU without MOVBE asks for the table.
    let mut host = FakeHost::default();
    host.cpu = "CPU: missing MOVBE\n".into();
    let ran = port.run_on(host, &[], &[]);
    assert_eq!(ran.code, 0, "{ran}");
    let check = tempfile::tempdir().unwrap();
    let sites = isa_rewrite::rewrite(&port.out().join("boot-linked.bin"), check.path()).unwrap();
    let fresh = fs::read(check.path().join("isa-rewrite.bin")).unwrap();
    assert_eq!(fs::read(&table).unwrap(), fresh);
    assert!(
        ran.out
            .lines()
            .any(|line| line == isa_rewrite::summary(&sites)),
        "{ran}"
    );
    let argv = ran.game();
    let at = argv.iter().position(|arg| arg == "--isa-rewrite").unwrap();
    assert_eq!(argv[at + 1], text(&table));
    assert_eq!(argv[at - 2], "--timeout");
}

/// A run's output that adds a byte past the image of `boot_linked` once the runner prints the
/// `link_modules` line: after the file is written and before the ISA rewrite reads it.
struct CorruptingBootLinked {
    printed: Vec<u8>,
    boot_linked: PathBuf,
    corrupted: bool,
}

impl Write for CorruptingBootLinked {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        self.printed.extend_from_slice(buf);
        let linked = String::from_utf8_lossy(&self.printed)
            .lines()
            .any(|line| line.starts_with("Linked modules: "));
        if linked && !self.corrupted {
            let mut file = fs::OpenOptions::new()
                .append(true)
                .open(&self.boot_linked)?;
            file.write_all(b"\0")?;
            self.corrupted = true;
        }
        Ok(buf.len())
    }

    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

#[test]
fn a_corrupt_boot_linked_skips_the_isa_table() {
    // Review Focus 5: a boot-linked.bin the ISA rewrite refuses (a trailer, which the strict
    // decode rejects) prints the rewrite's error, and the run goes on without the table: the game
    // starts and its exit code is the run's.
    let port = Port::new();
    let boot_linked = port.out().join("boot-linked.bin");
    // A CPU without MOVBE asks for the table.
    let mut host = FakeHost::default();
    host.cpu = "CPU: missing MOVBE\n".into();
    host.exit = 7;
    let mut out = CorruptingBootLinked {
        printed: Vec::new(),
        boot_linked: boot_linked.clone(),
        corrupted: false,
    };
    let code = run_with(Port::context(&port.program(&[]), &mut out), &host);
    let ran = InProcess {
        code,
        out: String::from_utf8_lossy(&out.printed).into_owned(),
        host,
    };
    assert!(out.corrupted, "{ran}");
    assert_eq!(ran.code, 7, "{ran}");
    let check = tempfile::tempdir().unwrap();
    let error = isa_rewrite::rewrite(&boot_linked, check.path()).unwrap_err();
    assert_eq!(error.to_string(), "unexpected boot trailer");
    let line = format!("ISA: rewrite table unavailable ({error})");
    assert!(ran.out.lines().any(|printed| printed == line), "{ran}");
    // The steps after the rewrite ran, and the game got no table.
    in_order(
        &ran.out,
        &[&line, "AppContent profile: ", "Engine frame rate: "],
    );
    assert!(
        !ran.game().iter().any(|arg| arg == "--isa-rewrite"),
        "{ran}"
    );
    assert!(!port.out().join("isa-rewrite.bin").exists(), "{ran}");
}

#[test]
fn a_ctrl_c_stops_before_the_game() {
    // The runner looks for Ctrl+C after --cpu-features, after the image steps, after the glyph
    // probe and just before the start. One at any of them ends the run with 130, and no game runs
    // on the folders the cleanup removes.
    let port = Port::new();
    let ran = port.run_on(FakeHost::default(), &[], &[]);
    assert_eq!(ran.code, 0, "{ran}");
    let checks = ran.host.polls_before_game.get().unwrap();
    assert_eq!(checks, 4, "{ran}");
    for poll in 1..=checks {
        let mut host = FakeHost::default();
        host.interrupt = Interrupt::FromPoll(poll);
        let ran = port.run_on(host, &[], &[]);
        assert_eq!(ran.code, 130, "Ctrl+C from check {poll}\n{ran}");
        assert!(ran.games().is_empty(), "Ctrl+C from check {poll}\n{ran}");
    }
}

#[test]
fn a_pool_above_ram_warns() {
    // guest_memory: an 11200 MiB pool and its flex span, plus the probe's own 4096 MiB, on a PC
    // with 4096 MiB of RAM. The game still starts.
    let port = Port::new();
    let mut host = FakeHost::default();
    host.memory_mb = Some(4096);
    let ran = port.run_on(host, &[("SPARTAN_DMEM_MB", "11200")], &[]);
    assert_eq!(ran.code, 0, "{ran}");
    let warning = "Memory: the guest memory pool holds up to 12224 MiB and the game needs about \
                   16320 MiB in all, more than the 4096 MiB of RAM.";
    assert!(
        ran.out.lines().any(|line| line.starts_with(warning)),
        "{ran}"
    );
    assert_eq!(ran.games().len(), 1, "{ran}");
}

/// `CommandLineToArgvW`: how Windows programs read their command line.
#[cfg(windows)]
fn command_line_to_argv(line: &str) -> Vec<String> {
    use windows_sys::Win32::Foundation::LocalFree;
    use windows_sys::Win32::UI::Shell::CommandLineToArgvW;
    let wide: Vec<u16> = line.encode_utf16().chain([0]).collect();
    let mut count = 0i32;
    // SAFETY: `wide` is NUL-terminated and outlives the call; `count` is a valid i32 to write.
    let argv = unsafe { CommandLineToArgvW(wide.as_ptr(), &mut count) };
    assert!(!argv.is_null());
    let args = (0..usize::try_from(count).unwrap())
        .map(|index| {
            // SAFETY: CommandLineToArgvW returned `count` NUL-terminated strings.
            unsafe {
                let arg = *argv.add(index);
                let length = (0..).take_while(|&at| *arg.add(at) != 0).count();
                String::from_utf16(std::slice::from_raw_parts(arg, length)).unwrap()
            }
        })
        .collect();
    // SAFETY: the array was allocated by CommandLineToArgvW and is freed once.
    unsafe { LocalFree(argv.cast()) };
    args
}

/// The words `sh -c` makes of `line`.
#[cfg(not(windows))]
fn command_line_to_argv(line: &str) -> Vec<String> {
    let output = Command::new("sh")
        .arg("-c")
        .arg(format!("printf '%s\\n' {line}"))
        .output()
        .unwrap();
    assert!(output.status.success());
    String::from_utf8(output.stdout)
        .unwrap()
        .lines()
        .map(str::to_string)
        .collect()
}

#[test]
fn restart_command_quotes_spaces_and_quotes() {
    // Review Focus: the in-game restart's command line, with a space in the program's folder.
    let dir = tempfile::tempdir().unwrap();
    let os = |args: &[&str]| -> Vec<OsString> { args.iter().map(OsString::from).collect() };
    if cfg!(windows) {
        // Windows paths cannot hold a quote; the arguments do.
        let exe = dir.path().join("Spartan Port").join("SpartanPort.exe");
        let args = [
            "--timeout",
            "5",
            "a\"b",
            "c\\",
            "d e\\",
            "",
            "tab\there",
            "back\\\\\"q",
            "x\\y",
        ];
        let command = restart_command(&exe, 42, &os(&args));
        let mut expected = vec![text(&exe), "--run".into(), "--after".into(), "42".into()];
        expected.extend(args.iter().map(|arg| arg.to_string()));
        assert_eq!(command_line_to_argv(&command), expected);
        // subprocess.list2cmdline's text.
        assert_eq!(
            restart_command(
                Path::new(r"C:\Spartan Port\SpartanPort.exe"),
                42,
                &os(&args)
            ),
            "\"C:\\Spartan Port\\SpartanPort.exe\" --run --after 42 --timeout 5 a\\\"b c\\ \
             \"d e\\\\\" \"\" \"tab\there\" back\\\\\\\\\\\"q x\\y"
        );
    } else {
        let exe = dir.path().join("Spartan \"Port's\"").join("spartanport");
        let args = [
            "--timeout",
            "5",
            "a\"b",
            "",
            "it's",
            "$HOME",
            "*",
            "x=y,z:a.b/c-d_e@f%g+h",
            "café",
        ];
        let command = restart_command(&exe, 42, &os(&args));
        // No --after: on Linux the probe replaces itself with the new launch.
        let mut expected = vec![text(&exe), "--run".into()];
        expected.extend(args.iter().map(|arg| arg.to_string()));
        assert_eq!(command_line_to_argv(&command), expected);
        // shlex.join's text.
        assert_eq!(
            restart_command(
                Path::new("/tmp/Spartan \"Port's\"/spartanport"),
                42,
                &os(&args)
            ),
            "'/tmp/Spartan \"Port'\"'\"'s\"/spartanport' --run --timeout 5 'a\"b' '' \
             'it'\"'\"'s' '$HOME' '*' x=y,z:a.b/c-d_e@f%g+h 'café'"
        );
    }
}

#[test]
fn after_waits_for_the_old_process() {
    // `--after <pid>` waits for that process first, then the run goes on with the other arguments.
    let port = Port::new();
    let ran = port.run_on(FakeHost::default(), &[], &["--after", "42", "--fullscreen"]);
    assert_eq!(ran.code, 0, "{ran}");
    assert_eq!(*ran.host.waited.borrow(), [(42, AFTER_LIMIT)]);
    let game = ran.game();
    assert_eq!(game.last().map(String::as_str), Some("--fullscreen"));
    assert!(!game.iter().any(|arg| arg == "--after"), "{ran}");
}

#[test]
fn wait_for_pid_returns_when_the_process_ends() {
    // `--after`'s wait: a process that sleeps 500 ms once started is waited for until it ends.
    let mut child = Command::new(FAKE_GAME)
        .args(["--exit", "0", "--lines", "1", "--delay-ms", "500"])
        .stdout(Stdio::null())
        .spawn()
        .unwrap();
    // From the start on: the first start of a fresh build can itself take longer than the sleep
    // (the antivirus scan), which would hide a wait that returns at once.
    let started = Instant::now();
    // On Linux an exited child stays a zombie until it is reaped, and `kill(pid, 0)` still
    // succeeds on it: a thread reaps it, as the real caller's parent would.
    let pid = child.id();
    let reaper = std::thread::spawn(move || child.wait());
    assert!(SystemHost.wait_for_pid(pid, Duration::from_secs(10)));
    let waited = started.elapsed();
    let _ = reaper.join();
    assert!(waited >= Duration::from_millis(400), "{waited:?}");
    assert!(waited < Duration::from_secs(10), "{waited:?}");
    // A pid that never existed: true at once.
    let started = Instant::now();
    assert!(SystemHost.wait_for_pid(u32::MAX - 1, Duration::from_secs(10)));
    assert!(started.elapsed() < Duration::from_secs(2));
}

#[test]
fn a_restart_from_the_game_starts_the_exe_again() {
    let port = Port::with_probe();
    let child = Command::new(LAUNCHER)
        .arg("--run")
        .env_clear()
        .envs(port.environment(&[
            ("FAKE_PROBE_RESTART", "1"),
            ("FAKE_PROBE_PRINT", "SPARTAN_RESTART_COMMAND"),
        ]))
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .unwrap();
    let pid = child.id();
    // The output ends once the restarted launch, which inherits it, has ended too.
    let output = child.wait_with_output().unwrap();
    let stdout = String::from_utf8_lossy(&output.stdout).into_owned();
    assert_eq!(output.status.code(), Some(0), "{stdout}");
    assert_eq!(
        stdout.matches("Runtime: restarting through run.py").count(),
        1,
        "{stdout}"
    );
    // Two starts of the game, with the same command line (--after is the runner's own).
    let runs = probe_runs(&stdout);
    assert_eq!(runs.len(), 2, "{stdout}");
    assert_eq!(runs[0], runs[1]);
    // The first launch's restart command is this launcher's, waiting for the first runner on
    // Windows.
    let commands = printed(&stdout, "SPARTAN_RESTART_COMMAND");
    assert_eq!(commands.len(), 2, "{stdout}");
    let argv = command_line_to_argv(&commands[0]);
    assert_eq!(resolved(Path::new(&argv[0])), resolved(Path::new(LAUNCHER)));
    if cfg!(windows) {
        assert_eq!(argv[1..], ["--run", "--after", &pid.to_string()]);
    } else {
        assert_eq!(argv[1..], ["--run"]);
    }
}

/// The settings for `--play`: a game folder, `user_dir`, English and no Discord presence.
fn play_settings(game: &Path, user: &Path) -> Settings {
    let values = json!({
        "game_dir": text(game),
        "user_dir": text(user),
        "ui_language": "en",
        "discord_rpc": false,
        "mods_enabled": false,
    });
    Settings(values.as_object().unwrap().clone())
}

/// `play_with` running the fake game with `args`; gives the exit code, the console, the run log
/// and what `notify` was told.
fn played(
    settings: &Settings,
    root: &Path,
    user: &Path,
    env: IndexMap<String, String>,
    args: &[&str],
) -> (i32, String, String, Vec<String>) {
    let paths = Paths::from_env(root, &|_| None);
    let mut command = vec![FAKE_GAME.to_string()];
    command.extend(args.iter().map(|arg| arg.to_string()));
    let mut console = Vec::new();
    let mut told = Vec::new();
    let program = Context {
        paths: paths.clone(),
        env,
    };
    let context = RunContext {
        context: &program,
        args: Vec::new(),
        out: &mut console,
    };
    let code = play_with(context, settings, &paths, &command, &mut |hint| {
        told.push(hint.to_string())
    });
    let log = fs::read_to_string(user.join("last_run.log")).unwrap();
    (code, String::from_utf8(console).unwrap(), log, told)
}

#[test]
fn play_writes_last_run_log_with_the_advice() {
    let dir = tempfile::tempdir().unwrap();
    let user = dir.path().join("saves");
    let settings = play_settings(dir.path(), &user);
    let line = "Runtime: cannot create 6080 MiB of guest memory (error 12)";
    let play = |exit: &str| {
        let args = ["--say", line, "--lines", "1", "--exit", exit];
        played(&settings, dir.path(), &user, clean_environment(), &args)
    };
    let (code, console, log, told) = play("21");
    assert_eq!(code, 21, "{log}");
    assert!(log.contains(&format!("{line}\n")), "{log}");
    let advice = if cfg!(windows) {
        ExitHint::GuestMemory
    } else {
        ExitHint::LinuxMemory
    }
    .text_en();
    // The game's output on the console and in the log; the advice after it, in the log only.
    assert_eq!(log, format!("{console}\n{advice}\n"));
    assert_eq!(told, [advice]);
    // A game that ends well needs no advice.
    let (code, console, log, told) = play("0");
    assert_eq!(code, 0, "{log}");
    assert_eq!(log, console);
    assert!(told.is_empty());
}

#[test]
fn play_starts_the_game_with_the_saved_stream_memory() {
    // The saved choice, whatever the launcher inherited.
    let dir = tempfile::tempdir().unwrap();
    let user = dir.path().join("saves");
    for value in ["cached", "host", "device"] {
        let mut settings = play_settings(dir.path(), &user);
        settings.0.insert("stream_memory".into(), json!(value));
        let mut env = clean_environment();
        env.insert("SPARTAN_STREAM_MEMORY".into(), "inherited".into());
        let args = ["--env", "SPARTAN_STREAM_MEMORY"];
        let (code, _, log, _) = played(&settings, dir.path(), &user, env, &args);
        assert_eq!(code, 0, "{log}");
        assert_eq!(log.lines().next(), Some(value), "{log}");
    }
}

#[test]
fn a_hanging_gamepad_probe_keeps_the_game_icons() {
    // run.py: a probe that does not answer within 15 s is a glyph failure; the game starts with
    // its own button icons.
    let port = Port::new();
    let probe = text(&port.probe());
    let mut host = FakeHost::default();
    host.gamepad = Err(ToolError::TimedOut {
        command: vec![probe.clone(), "--gamepad".into()],
        limit: GAMEPAD_PROBE_LIMIT,
    });
    let ran = port.run_on(host, &[("SPARTAN_GLYPH_STYLE", "auto")], &[]);
    assert_eq!(ran.code, 0, "{ran}");
    let calls = ran.host.calls.borrow();
    let asked = calls
        .iter()
        .find(|call| call.args == ["--gamepad"])
        .expect("no --gamepad call");
    assert_eq!(asked.program, OsString::from(&probe));
    assert_eq!(asked.limit, Some(GAMEPAD_PROBE_LIMIT));
    assert!(!ran.game().iter().any(|arg| arg == "--mount"), "{ran}");
    // subprocess.TimeoutExpired's text, the command as Python's repr of the list.
    let line = format!(
        "Glyphs: Command '['{}', '--gamepad']' timed out after 15 seconds; the game keeps its \
         own button icons",
        probe.replace('\\', "\\\\")
    );
    assert!(ran.out.lines().any(|l| l == line), "{line}\n{ran}");
    assert_eq!(ran.var("SPARTAN_GLYPHS"), None, "{ran}");
}

#[test]
fn the_gamepad_probe_answers_the_auto_glyphs() {
    // No gamepad: the keyboard keys; this synthetic game has no button icons to repaint.
    let port = Port::new();
    let mut host = FakeHost::default();
    host.gamepad = Ok("Gamepad: none\n".into());
    let ran = port.run_on(host, &[("SPARTAN_GLYPH_STYLE", "auto")], &[]);
    assert_eq!(ran.code, 0, "{ran}");
    assert!(
        ran.out
            .lines()
            .any(|l| l == "Glyphs: no button icons found in this game folder"),
        "{ran}"
    );
}

#[test]
fn an_update_folder_is_combined_into_a_view() {
    let port = Port::new();
    let update = port.root.join("update");
    copy_into(&port.game().join("eboot.bin"), &update);
    copy_into(
        &port.game().join("sce_sys/param.sfo"),
        &update.join("sce_sys"),
    );
    fs::create_dir_all(update.join("dvdroot_ps4")).unwrap();
    fs::write(update.join("dvdroot_ps4/PART1.WAD"), b"update").unwrap();
    let (host, shown) = showing("dvdroot_ps4/PART1.WAD");
    let ran = port.run_on(host, &[("SPARTAN_PATCH_DIR", &text(&update))], &[]);
    assert_eq!(ran.code, 0, "{ran}");
    let line = format!("Game and update combined from {}", update.display());
    assert!(ran.out.lines().any(|l| l == line), "{ran}");
    let view = app0(&ran.game());
    assert!(
        view.file_name()
            .unwrap()
            .to_string_lossy()
            .starts_with("game-update-"),
        "{ran}"
    );
    assert_eq!(view.parent(), Some(port.out().as_path()));
    assert_eq!(shown.lock().unwrap().as_deref(), Some("update"), "{ran}");
    assert!(!view.exists(), "the view is left in out/");
    assert_ne!(
        fs::read(port.game().join("dvdroot_ps4/PART1.WAD")).unwrap(),
        b"update"
    );
}

/// Ctrl+C (SIGINT) while the game runs: the overlay goes and the runner ends with 130, as run.py's
/// `KeyboardInterrupt`.
#[cfg(unix)]
#[test]
fn ctrl_c_cleans_up_and_ends_with_130() {
    use nix::sys::signal::{kill, Signal};
    use nix::unistd::Pid;
    use std::io::{BufRead, BufReader};
    let port = Port::with_probe();
    let mod_wad = port.root.join("mods/A/dvdroot_ps4/PART1.WAD");
    fs::create_dir_all(mod_wad.parent().unwrap()).unwrap();
    fs::write(&mod_wad, b"mod").unwrap();
    let mut child = Command::new(LAUNCHER)
        .arg("--run")
        .env_clear()
        .envs(port.environment(&[("FAKE_PROBE_SLEEP_MS", "3000")]))
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .spawn()
        .unwrap();
    let mut stdout = BufReader::new(child.stdout.take().unwrap());
    let mut seen = String::new();
    while !seen.contains("fake-probe argv end\n") {
        let mut line = String::new();
        assert_ne!(stdout.read_line(&mut line).unwrap(), 0, "{seen}");
        seen.push_str(&line);
    }
    let pid = Pid::from_raw(i32::try_from(child.id()).unwrap());
    kill(pid, Signal::SIGINT).unwrap();
    assert_eq!(child.wait().unwrap().code(), Some(130), "{seen}");
    let overlay = app0(&probe_runs(&seen)[0]);
    assert!(!overlay.exists());
}

#[test]
fn a_hanging_tool_is_stopped_at_its_limit() {
    let call = ToolCall {
        program: FAKE_PROBE.into(),
        args: vec!["hang".into()],
        env: None,
        cwd: None,
        limit: Some(Duration::from_secs(1)),
        stderr: Stderr::Null,
        window: Window::Hidden,
    };
    let start = Instant::now();
    let err = run_tool(&call).unwrap_err();
    assert!(matches!(err, ToolError::TimedOut { .. }));
    assert!(err.to_string().ends_with("timed out after 1 seconds"));
    assert!(start.elapsed() < Duration::from_secs(5));
}
