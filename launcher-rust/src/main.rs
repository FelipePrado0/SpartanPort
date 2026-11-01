// A windowed program on Windows: no console opens with the launcher. From a terminal it attaches
// to the terminal's console (`platform::attach_parent_console`).
#![windows_subsystem = "windows"]

use std::ffi::OsString;
use std::path::{Path, PathBuf};

use spartanport_launcher::core::i18n::{launcher_language, system_language};
use spartanport_launcher::core::installation::Installation;
use spartanport_launcher::core::paths::Paths;
use spartanport_launcher::core::platform;
use spartanport_launcher::core::settings::load_app_settings;
use spartanport_launcher::core::voice_swap;
use spartanport_launcher::runner::{self, RunContext};
use spartanport_launcher::startup::{self, Route};
use spartanport_launcher::{ui, APP_NAME, VERSION};

fn main() {
    // First, so `--version`, the update installer and start-up errors print in a terminal (and
    // `--play`'s output: the Python's `attach_stdio`).
    platform::attach_parent_console();
    let args: Vec<OsString> = std::env::args_os().skip(1).collect();
    let exe_dir = std::env::current_exe()
        .ok()
        .and_then(|exe| exe.parent().map(Path::to_path_buf))
        .unwrap_or_else(|| PathBuf::from("."));
    // The environment forwarded and the folders derived once, for every route; a packaged port,
    // or the checkout a cargo build sits in (`launcher-rust/target/...`).
    let (route, context) = startup::start(&args, &exe_dir, std::env::vars_os(), cfg!(windows));
    match route {
        Route::Version => println!("{APP_NAME} {VERSION}"),
        Route::Run { args } => {
            let run = RunContext {
                context: &context,
                args,
                out: &mut std::io::stdout(),
            };
            std::process::exit(runner::run(run))
        }
        Route::Play { migrate } => {
            // As the Python `main`: the old name's files and the settings first.
            startup::apply_to_process(&context);
            let paths = &context.paths;
            startup::prepare(paths, migrate);
            let settings = load_app_settings(paths);
            // Without a usable game folder there is nothing to play yet: the window opens instead.
            if Installation::read(
                &settings.get_str("game_dir"),
                &settings.get_str("update_dir"),
            )
            .verdict()
            .passes()
            {
                let run = RunContext {
                    context: &context,
                    // The Python's `--play` starts the runner without arguments.
                    args: Vec::new(),
                    out: &mut std::io::stdout(),
                };
                std::process::exit(runner::play(run, &settings, paths));
            }
            open_window(context.paths);
        }
        Route::InstallUpdate { target, pid } => {
            let lang = launcher_language(
                &load_app_settings(&context.paths).get_str("ui_language"),
                system_language,
            );
            std::process::exit(startup::install_update(&exe_dir, &target, pid, &lang))
        }
        Route::VoiceSwap { args } => {
            // As the AppImage runs from a mount, `OWD` is the folder the player typed paths in.
            let base = std::env::var_os("OWD");
            let code = voice_swap::main(
                &args,
                base.as_deref().map(Path::new),
                &mut std::io::stdout(),
                &mut std::io::stderr(),
            );
            std::process::exit(code)
        }
        Route::Launcher { migrate } => {
            // Before the folders below are read from the environment.
            startup::apply_to_process(&context);
            startup::prepare(&context.paths, migrate);
            open_window(context.paths);
        }
    }
}

/// The launcher window; exit code 1 when it cannot open.
fn open_window(paths: Paths) {
    if let Err(error) = ui::app::run(paths) {
        eprintln!("{APP_NAME}: {error}");
        std::process::exit(1);
    }
}
