#![allow(clippy::unwrap_used, clippy::expect_used)]
//! The views read from the disk follow the settings they are read from (`ui::follow`).

mod common;

use std::collections::BTreeSet;
use std::path::Path;

use common::{
    dump_with_patches, load_mods, load_trophies, make_mod, state, state_with_trophies, trp_package,
    write_mod_patch,
};
use iced::futures::executor::block_on;
use iced::futures::StreamExt;
use iced::Task;
use iced_test::runtime::task::into_stream;
use iced_test::runtime::Action;
use serde_json::{json, Value};
use spartanport_launcher::core::launch::Exit;
use spartanport_launcher::core::paths::Paths;
use spartanport_launcher::core::trophies::TROPHY_COUNT;
use spartanport_launcher::ui::app::{self, Message, State};
use spartanport_launcher::ui::follow::{inputs, View};
use spartanport_launcher::ui::home;
use spartanport_launcher::ui::pages::game::BackupMessage;
use spartanport_launcher::ui::pages::mods::{self, ModsMessage};
use spartanport_launcher::ui::pages::trophies::{self, TrophiesMessage};
use spartanport_launcher::ui::persist;
use spartanport_launcher::ui::play::{GameMessage, GameState};
use spartanport_launcher::ui::setup::SetupMessage;

/// Runs what `task` reads in the background and hands each message back to `update`, with what
/// those start in turn, as the window's runtime does. Each action is taken as it comes; one that
/// is not a message (a window or widget request, which nothing here answers) is dropped at once.
fn settle(state: &mut State, task: Task<Message>) {
    let Some(mut stream) = into_stream(task) else {
        return;
    };
    while let Some(action) = block_on(stream.next()) {
        if let Action::Output(message) = action {
            let next = app::update(state, message);
            settle(state, next);
        }
    }
}

/// `Message::Set`, with what it reads in the background.
fn set(state: &mut State, key: &str, value: Value) {
    let task = app::update(state, Message::Set(key.into(), value));
    settle(state, task);
}

/// `Message::Browsed`, as the dialog or a keystroke in the folder field sends it, with what it
/// reads in the background.
fn browse(state: &mut State, key: &str, path: &Path) {
    let task = app::update(
        state,
        Message::Browsed(key.into(), Some(path.to_path_buf())),
    );
    settle(state, task);
}

/// A setting changed as the window changes it: a folder through its field (`Browsed`), anything
/// else through `Set`.
fn change(state: &mut State, key: &str, value: Value) {
    match value.as_str() {
        Some(folder) if key.ends_with("_dir") => browse(state, key, Path::new(folder)),
        _ => set(state, key, value),
    }
}

/// Every view, in the table's order.
const VIEWS: [View; 8] = [
    View::Installation,
    View::Home,
    View::Patches,
    View::Backups,
    View::ModResolutions,
    View::Aspect,
    View::Mods,
    View::Trophies,
];

fn text(path: &Path) -> String {
    path.display().to_string()
}

fn folder(path: &Path) -> Value {
    Value::String(text(path))
}

/// What `view` shows, as text to compare.
fn look(state: &State, view: View) -> String {
    match view {
        View::Installation => format!(
            "{:?} {:?}",
            state.installation.title_id(),
            state.installation.has_game()
        ),
        View::Home => format!("{:?}", state.home),
        View::Patches => format!(
            "{:?} {:?}",
            state.patches.title_id,
            state
                .patches
                .selectable
                .iter()
                .map(|meta| &meta.name)
                .collect::<Vec<_>>()
        ),
        // The choice too: listing the backups again chooses the newest.
        View::Backups => format!("{:?} chosen {:?}", state.backups, state.backup_choice),
        View::ModResolutions => format!("{:?}", state.mod_resolutions),
        View::Aspect => state.aspect.clone(),
        View::Mods => format!("{:?}", state.mods.order),
        View::Trophies => format!(
            "{} {:?}",
            state.trophies.icons.len(),
            state.trophies.unlocked
        ),
    }
}

/// For one view: what is in place before it is looked at, what then changes on the disk it reads,
/// a setting it does not read and one it reads, each with a new value. Had the view not followed
/// the setting it reads, it would still show what it showed before the disk changed.
struct Case {
    setup: fn(&mut State),
    disk: fn(&State, &Path),
    outside: (&'static str, fn(&State, &Path) -> Value),
    inside: (&'static str, fn(&State, &Path) -> Value),
}

/// The backups the Backups case starts with; the older one is chosen.
const OLDER: &str = "20261008-180000";
const NEWER: &str = "20261009-180000";
/// A backup made after those two were listed.
const NEWEST: &str = "20261010-180000";

fn backups_folder(state: &State) -> std::path::PathBuf {
    state.paths.user_dir("").join("savedata_backups")
}

/// Lists two backups of the saves folder and chooses the older one, as the player does on the Game
/// page.
fn choose_the_older_backup(state: &mut State) {
    for name in [OLDER, NEWER] {
        std::fs::create_dir_all(backups_folder(state).join(name)).unwrap();
    }
    let _ = app::update(state, Message::Backup(BackupMessage::Refresh));
    let _ = app::update(state, Message::Backup(BackupMessage::Choose(OLDER.into())));
}

/// A backup is made on the disk after the list was read.
fn a_newer_backup_appears(state: &State) {
    std::fs::create_dir_all(backups_folder(state).join(NEWEST)).unwrap();
}

fn case(view: View) -> Case {
    // An empty folder or a missing one: nothing the views read is there.
    let saves = |_: &State, dir: &Path| folder(&dir.join("saves"));
    let mods_folder = |_: &State, dir: &Path| folder(&dir.join("other-mods"));
    let update = |_: &State, dir: &Path| folder(&dir.join("update"));
    let nothing = |_: &mut State| {};
    match view {
        View::Installation => Case {
            setup: nothing,
            disk: |_, dir| {
                dump_with_patches(dir);
            },
            outside: ("user_dir", saves),
            inside: ("update_dir", update),
        },
        View::Home => Case {
            setup: nothing,
            disk: |state, _| {
                let savedata = state.paths.user_dir("").join("savedata");
                std::fs::create_dir_all(&savedata).unwrap();
                std::fs::write(savedata.join("slot0"), b"x").unwrap();
            },
            outside: ("mods_dir", mods_folder),
            inside: ("update_dir", update),
        },
        View::Patches => Case {
            setup: nothing,
            disk: |_, dir| {
                dump_with_patches(dir);
            },
            outside: ("user_dir", saves),
            inside: ("update_dir", update),
        },
        View::Backups => Case {
            setup: choose_the_older_backup,
            disk: |state, _| a_newer_backup_appears(state),
            // Spec §4/06 (changed): a new game folder neither lists the backups again nor
            // moves the choice to the newest.
            outside: ("game_dir", |_, dir| folder(&dir.join("other-game"))),
            // The same saves folder, spelled out.
            inside: ("user_dir", |state, _| folder(&state.paths.user_dir(""))),
        },
        View::ModResolutions => Case {
            setup: |state| {
                let patches = state.paths.mods_dir("").join("Ultrawide").join("patches");
                std::fs::create_dir_all(&patches).unwrap();
                write_mod_patch(&patches, "3840x1080");
                let mods = state.paths.mods_dir("");
                browse(state, "mods_dir", &mods);
                assert_eq!(state.mod_resolutions, ["3840x1080"]);
            },
            // The mod offers another size.
            disk: |state, _| {
                let patches = state.paths.mods_dir("").join("Ultrawide").join("patches");
                write_mod_patch(&patches, "3000x1000");
            },
            // Not a setting of the Mods page either: a read of its lists also reads the sizes.
            outside: ("user_dir", saves),
            // Only this row reads it.
            inside: ("mods_enabled", |_, _| json!(false)),
        },
        View::Aspect => Case {
            setup: nothing,
            // It reads only the render resolution, no file.
            disk: |_, _| {},
            outside: ("user_dir", saves),
            inside: ("render_resolution", |_, _| json!("3440x1440")),
        },
        View::Mods => Case {
            setup: nothing,
            disk: |state, _| make_mod(state, "Ultrawide", "a.bin"),
            outside: ("user_dir", saves),
            inside: ("patches_dir", |_, dir| folder(&dir.join("other-patches"))),
        },
        View::Trophies => Case {
            setup: nothing,
            disk: |_, dir| {
                let trophy = dir.join("dump").join("sce_sys").join("trophy");
                std::fs::create_dir_all(&trophy).unwrap();
                std::fs::write(trophy.join("trophy00.trp"), trp_package(TROPHY_COUNT)).unwrap();
            },
            outside: ("mods_dir", mods_folder),
            inside: ("update_dir", update),
        },
    }
}

#[test]
fn each_view_follows_exactly_its_keys() {
    let mut wrong = Vec::new();
    for view in VIEWS {
        let dir = tempfile::tempdir().unwrap();
        let mut state = state(dir.path());
        // A game folder, still empty, as every view reads it.
        let game = dir.path().join("dump");
        std::fs::create_dir_all(&game).unwrap();
        browse(&mut state, "game_dir", &game);
        let Case {
            setup,
            disk,
            outside,
            inside,
        } = case(view);
        setup(&mut state);
        let before = look(&state, view);
        disk(&state, dir.path());
        let value = (outside.1)(&state, dir.path());
        change(&mut state, outside.0, value);
        if look(&state, view) != before {
            wrong.push(format!("{view:?} followed {}", outside.0));
        }
        let value = (inside.1)(&state, dir.path());
        change(&mut state, inside.0, value);
        if look(&state, view) == before {
            wrong.push(format!("{view:?} did not follow {}", inside.0));
        }
    }
    assert!(wrong.is_empty(), "{wrong:#?}");
}

/// Everything the views show, as text to compare.
fn views(state: &State) -> String {
    let shown: Vec<String> = VIEWS.iter().map(|view| look(state, *view)).collect();
    format!(
        "{shown:?} {:?} {:?}",
        state.mods.inputs, state.trophies.inputs
    )
}

#[test]
fn typing_and_browsing_a_folder_give_the_same_views() {
    // A folder chosen in the dialog or typed into its field is `Browsed`; `Set` of the same path
    // is the same change.
    let dir = tempfile::tempdir().unwrap();
    let mut typed = state(dir.path());
    let mut browsed = state(dir.path());
    let game = dump_with_patches(dir.path());
    std::fs::create_dir_all(game.join("sce_sys/trophy")).unwrap();
    std::fs::write(
        game.join("sce_sys/trophy/trophy00.trp"),
        trp_package(TROPHY_COUNT),
    )
    .unwrap();
    let saves = dir.path().join("saves");
    std::fs::create_dir_all(saves.join("savedata")).unwrap();
    std::fs::write(saves.join("savedata").join("slot0"), b"x").unwrap();
    std::fs::create_dir_all(saves.join("savedata_backups").join(NEWER)).unwrap();
    let mods_dir = dir.path().join("my-mods");
    let patches = mods_dir.join("Ultrawide").join("patches");
    std::fs::create_dir_all(&patches).unwrap();
    write_mod_patch(&patches, "3840x1080");
    let folders = [
        ("game_dir", game.clone()),
        ("update_dir", dir.path().join("update")),
        ("user_dir", saves),
        ("mods_dir", mods_dir),
        ("patches_dir", dir.path().join("my-patches")),
    ];
    for (key, path) in folders {
        let before = views(&typed);
        set(&mut typed, key, folder(&path));
        browse(&mut browsed, key, &path);
        assert_eq!(views(&typed), views(&browsed), "{key}");
        assert_ne!(views(&typed), before, "nothing followed {key}");
    }
    assert_eq!(typed.installation.title_id(), Some("CUSA01623"));
    assert_eq!(typed.mods.order, ["Ultrawide"]);
    assert_eq!(typed.trophies.icons.len(), TROPHY_COUNT);
}

/// A state whose game folder is `dump`, still empty, and holds a copy of the dump made afterwards,
/// as a file manager would copy it into the folder already chosen.
fn copied_into_the_chosen_folder(dir: &Path) -> State {
    let mut state = state(dir);
    state.lang = "en".into();
    let game = dir.join("dump");
    std::fs::create_dir_all(&game).unwrap();
    browse(&mut state, "game_dir", &game);
    assert_eq!(state.installation.title_id(), None);
    dump_with_patches(dir);
    state
}

#[test]
fn setting_the_same_value_again_reads_nothing() {
    let dir = tempfile::tempdir().unwrap();
    let mut state = copied_into_the_chosen_folder(dir.path());
    let before = views(&state);
    let same = folder(&dir.path().join("dump"));
    let task = app::update(&mut state, Message::Set("game_dir".into(), same));
    assert_eq!(task.units(), 0, "a read started");
    assert_eq!(views(&state), before);
    assert_eq!(state.installation.title_id(), None);
}

#[test]
fn browsing_the_same_folder_again_reads_it_again() {
    // Ruling R49: choosing the folder already chosen is how the player has it read again (the
    // Python's variable trace fires on an unchanged value too).
    let dir = tempfile::tempdir().unwrap();
    let mut state = copied_into_the_chosen_folder(dir.path());
    browse(&mut state, "game_dir", &dir.path().join("dump"));
    assert_eq!(state.installation.title_id(), Some("CUSA01623"));
    assert_eq!(home::chips(&state)[1], "✓ CUSA01623 · v01.02");
    assert_eq!(state.patches.title_id.as_deref(), Some("CUSA01623"));
    let names: Vec<&str> = state
        .patches
        .selectable
        .iter()
        .map(|meta| meta.name.as_str())
        .collect();
    assert_eq!(names, ["Fix Alpha"]);
}

#[test]
fn typing_a_game_folder_letter_by_letter_keeps_the_last_read() {
    // Review Focus 3: each keystroke sends `Browsed` and starts a read of the Mods and Trophies
    // lists. They finish in any order; only the read for the folder as typed last is kept.
    let dir = tempfile::tempdir().unwrap();
    let mut state = state(dir.path());
    // Only the whole folder holds the game: its trophies, and the data folder the mod's files go
    // into, without which the mod is not one for this game.
    let game = dir.path().join("game");
    std::fs::create_dir_all(game.join("special")).unwrap();
    std::fs::create_dir_all(game.join("sce_sys/trophy")).unwrap();
    std::fs::write(
        game.join("sce_sys/trophy/trophy00.trp"),
        trp_package(TROPHY_COUNT),
    )
    .unwrap();
    let mod_files = state.paths.mods_dir("").join("Ultrawide").join("special");
    std::fs::create_dir_all(&mod_files).unwrap();
    std::fs::write(mod_files.join("a.bin"), b"data").unwrap();
    let (mut stale_mods, mut stale_trophies) = (Vec::new(), Vec::new());
    for typed in ["g", "ga", "game"] {
        let _ = app::update(
            &mut state,
            Message::Browsed("game_dir".into(), Some(dir.path().join(typed))),
        );
        stale_mods.push(mods::load(&state.paths, &state.store, &state.installation));
        stale_trophies.push(trophies::load(
            &state.paths,
            &state.store,
            &state.installation,
        ));
    }
    assert!(stale_mods[0].order.is_empty() && stale_mods[1].order.is_empty());
    assert!(stale_trophies[0].icons.is_empty() && stale_trophies[1].icons.is_empty());
    // The read for the whole folder finishes first, the older ones after it, newest first.
    stale_mods.pop();
    stale_trophies.pop();
    load_mods(&mut state);
    load_trophies(&mut state);
    for (mods, trophies) in stale_mods.into_iter().zip(stale_trophies).rev() {
        let _ = app::update(
            &mut state,
            Message::Mods(ModsMessage::Loaded(Box::new(mods))),
        );
        let _ = app::update(
            &mut state,
            Message::Trophies(TrophiesMessage::Loaded(Box::new(trophies))),
        );
    }
    assert_eq!(state.store.text("game_dir"), text(&game));
    assert_eq!(state.mods.order, ["Ultrawide"]);
    assert_eq!(state.trophies.icons.len(), TROPHY_COUNT);
    assert_eq!(state.mods.inputs, inputs(View::Mods, &state.store));
    assert_eq!(state.trophies.inputs, inputs(View::Trophies, &state.store));
}

#[test]
fn a_read_for_a_folder_typed_over_is_dropped() {
    // N2: each keystroke in a folder field starts a read of the Trophies and Mods lists, and the
    // reads finish in any order: one made for a path since typed over must not replace the list.
    use spartanport_launcher::core::installation::Installation;
    let dir = tempfile::tempdir().unwrap();
    let mut state = state_with_trophies(dir.path());
    make_mod(&state, "Ultrawide", "a.bin");
    let game = state.store.text("game_dir");
    let mods_dir = state.paths.mods_dir("").display().to_string();
    // The reads started while the fields held one character less.
    state
        .store
        .set("game_dir", game[..game.len() - 1].to_string().into());
    state.store.set(
        "mods_dir",
        mods_dir[..mods_dir.len() - 1].to_string().into(),
    );
    let stale = Installation::of(&state.store);
    let stale_trophies = trophies::load(&state.paths, &state.store, &stale);
    let stale_mods = mods::load(&state.paths, &state.store, &stale);
    assert!(stale_trophies.icons.is_empty() && stale_mods.order.is_empty());
    state.store.set("game_dir", game.into());
    state.store.set("mods_dir", mods_dir.into());
    assert_ne!(stale_trophies.inputs, inputs(View::Trophies, &state.store));
    assert_ne!(stale_mods.inputs, inputs(View::Mods, &state.store));
    // The reads for the whole paths finish first, the older ones after them.
    load_trophies(&mut state);
    load_mods(&mut state);
    let _ = app::update(
        &mut state,
        Message::Trophies(TrophiesMessage::Loaded(Box::new(stale_trophies))),
    );
    let _ = app::update(
        &mut state,
        Message::Mods(ModsMessage::Loaded(Box::new(stale_mods))),
    );
    assert_eq!(state.trophies.icons.len(), 35);
    assert_eq!(state.mods.order, ["Ultrawide"]);
}

#[test]
fn a_cancelled_folder_dialog_reads_nothing() {
    // The dialog closed without a folder: the setting keeps its value and nothing is read, not even
    // a dump copied into the folder meanwhile.
    let dir = tempfile::tempdir().unwrap();
    let mut state = copied_into_the_chosen_folder(dir.path());
    let before = views(&state);
    let task = app::update(&mut state, Message::Browsed("game_dir".into(), None));
    assert_eq!(task.units(), 0, "a read started");
    assert_eq!(views(&state), before);
    assert_eq!(state.installation.title_id(), None);
    assert_eq!(state.store.text("game_dir"), text(&dir.path().join("dump")));
}

#[test]
fn a_game_folder_change_keeps_the_chosen_backup() {
    // Spec §4/06 (changed): as the Python's `game_changed`, a new game folder does not list the
    // backups again, so typing the folder does not move the choice to the newest backup.
    let dir = tempfile::tempdir().unwrap();
    let mut state = state(dir.path());
    choose_the_older_backup(&mut state);
    a_newer_backup_appears(&state);
    for typed in ["d", "du", "dump"] {
        browse(&mut state, "game_dir", &dir.path().join(typed));
    }
    set(&mut state, "game_dir", folder(&dir.path().join("other")));
    assert_eq!(state.backups, [NEWER, OLDER]);
    assert_eq!(state.backup_choice, OLDER);
}

/// A state over `dir` as the program opens it: the setup shows when `dir` has no settings file.
fn opened(dir: &Path) -> State {
    let mut state = State::open(paths(dir));
    state.splash.done = true;
    state
}

/// The folders `common::state` gives a state over `dir`.
fn paths(dir: &Path) -> Paths {
    let root = dir.display().to_string();
    let env = move |key: &str| match key {
        "APPDATA" | "USERPROFILE" | "HOME" | "XDG_CONFIG_HOME" => Some(root.clone()),
        _ => None,
    };
    Paths::from_env(dir, &env)
}

/// Puts a 35-trophy package in the game folder `game`.
fn trophies_in(game: &Path) {
    let trophy = game.join("sce_sys").join("trophy");
    std::fs::create_dir_all(&trophy).unwrap();
    std::fs::write(trophy.join("trophy00.trp"), trp_package(TROPHY_COUNT)).unwrap();
}

/// A mods folder `mods_dir` with one mod, `Ultrawide`, whose patch adds the size 3840x1080.
fn size_mod_in(mods_dir: &Path) {
    let patches = mods_dir.join("Ultrawide").join("patches");
    std::fs::create_dir_all(&patches).unwrap();
    write_mod_patch(&patches, "3840x1080");
}

fn patch_names(state: &State) -> Vec<&str> {
    state
        .patches
        .selectable
        .iter()
        .map(|meta| meta.name.as_str())
        .collect()
}

#[test]
fn skip_follows_the_restored_folders() {
    // Skip puts back what the files held when the setup opened. The views follow every folder it
    // put back, and only those: the saves folder was not changed, so the backups are not listed
    // again and the chosen one stays chosen.
    let dir = tempfile::tempdir().unwrap();
    let mut state = opened(dir.path());
    assert!(state.setup.is_some());
    choose_the_older_backup(&mut state);
    let game = dump_with_patches(dir.path());
    trophies_in(&game);
    let mods_dir = dir.path().join("my-mods");
    size_mod_in(&mods_dir);
    browse(&mut state, "game_dir", &game);
    browse(&mut state, "mods_dir", &mods_dir);
    assert_eq!(state.installation.title_id(), Some("CUSA01623"));
    assert_eq!(state.patches.title_id.as_deref(), Some("CUSA01623"));
    assert_eq!(state.mods.order, ["Ultrawide"]);
    assert_eq!(state.mod_resolutions, ["3840x1080"]);
    assert_eq!(state.trophies.icons.len(), TROPHY_COUNT);
    a_newer_backup_appears(&state);

    let task = app::update(&mut state, Message::Setup(SetupMessage::Skip));
    settle(&mut state, task);
    assert!(state.setup.is_none());
    assert_eq!(state.store.text("game_dir"), "");
    assert_eq!(state.store.text("mods_dir"), "");
    assert_eq!(state.installation.title_id(), None);
    assert_eq!(state.patches.title_id, None);
    assert!(state.mods.order.is_empty(), "{:?}", state.mods.order);
    assert!(state.mod_resolutions.is_empty());
    assert!(state.trophies.icons.is_empty());
    assert_eq!(state.mods.inputs, inputs(View::Mods, &state.store));
    assert_eq!(state.trophies.inputs, inputs(View::Trophies, &state.store));
    assert_eq!(state.backups, [NEWER, OLDER]);
    assert_eq!(state.backup_choice, OLDER);
}

#[test]
fn collect_follows_a_resolution_changed_on_disk() {
    // A save reads the ini back: a size changed in it outside the launcher reaches the Aspect box.
    let dir = tempfile::tempdir().unwrap();
    let mut state = state(dir.path());
    let _ = app::update(&mut state, Message::Save);
    assert_eq!(state.aspect, "16:9");
    let ini = state.paths.ini_path();
    let lines: Vec<String> = std::fs::read_to_string(&ini)
        .unwrap()
        .lines()
        .map(|line| match line.split_once('=') {
            Some(("render_resolution", _)) => "render_resolution=3440x1440".to_string(),
            _ => line.to_string(),
        })
        .collect();
    assert!(lines
        .iter()
        .any(|line| line == "render_resolution=3440x1440"));
    std::fs::write(&ini, lines.join("\n") + "\n").unwrap();
    let _ = app::update(&mut state, Message::Save);
    assert_eq!(state.store.text("render_resolution"), "3440x1440");
    assert_eq!(state.aspect, "21:9");
}

/// A launcher over `dir` whose game, the `fake-game` test binary, has just started.
fn playing(dir: &Path) -> State {
    let mut state = state(dir);
    let game = dump_with_patches(dir);
    std::fs::create_dir_all(game.join("sce_module")).unwrap();
    for name in ["libc.prx", "libSceFios2.prx"] {
        std::fs::write(game.join("sce_module").join(name), b"prx").unwrap();
    }
    browse(&mut state, "game_dir", &game);
    state.launch_command = Some(
        [
            env!("CARGO_BIN_EXE_fake-game"),
            "--delay-ms",
            "50",
            "--exit",
            "0",
        ]
        .map(str::to_string)
        .to_vec(),
    );
    let _ = app::update(&mut state, Message::Play);
    assert!(
        matches!(state.game, GameState::Preparing { .. }),
        "{:?}",
        state.game
    );
    state
}

#[test]
fn the_game_exit_lists_the_new_backup() {
    // Spec §4/06 (changed): the runner backs the saves up before the game starts; when the game
    // exits, that backup is listed and chosen.
    let dir = tempfile::tempdir().unwrap();
    let mut state = playing(dir.path());
    assert!(state.backups.is_empty());
    std::fs::create_dir_all(backups_folder(&state).join(NEWER)).unwrap();
    let task = app::update(
        &mut state,
        Message::Game(GameMessage::Exited(Exit {
            code: 0,
            hint: None,
        })),
    );
    settle(&mut state, task);
    assert!(matches!(state.game, GameState::Idle), "{:?}", state.game);
    assert_eq!(state.backups, [NEWER]);
    assert_eq!(state.backup_choice, NEWER);
}

#[test]
fn a_finished_copy_of_the_system_files_keeps_the_chosen_backup() {
    // The copy of the system files reads the game folders again, so what was copied shows; the
    // saves folder is not one of them, as with the Python's `game_changed`.
    let dir = tempfile::tempdir().unwrap();
    let mut state = state(dir.path());
    state.lang = "en".into();
    let game = dir.path().join("dump");
    std::fs::create_dir_all(&game).unwrap();
    browse(&mut state, "game_dir", &game);
    choose_the_older_backup(&mut state);
    a_newer_backup_appears(&state);
    // What the copy put in place before it failed.
    dump_with_patches(dir.path());
    let task = app::update(
        &mut state,
        Message::Game(GameMessage::AdoptDone(Err("disk full".into()))),
    );
    settle(&mut state, task);
    assert_eq!(
        state.modal.as_ref().map(|modal| modal.text.as_str()),
        Some("Could not copy the files: disk full")
    );
    assert_eq!(state.installation.title_id(), Some("CUSA01623"));
    assert_eq!(patch_names(&state), ["Fix Alpha"]);
    assert_eq!(state.backups, [NEWER, OLDER]);
    assert_eq!(state.backup_choice, OLDER);
}

#[test]
fn the_focus_reads_the_game_folders_again_while_the_setup_is_open() {
    // Files copied from a file manager while the setup is open: when the window gets the focus
    // back, every view of the game folders follows, not only the setup's own check.
    let dir = tempfile::tempdir().unwrap();
    let mut state = opened(dir.path());
    state.lang = "en".into();
    let game = dir.path().join("dump");
    std::fs::create_dir_all(&game).unwrap();
    browse(&mut state, "game_dir", &game);
    dump_with_patches(dir.path());
    trophies_in(&game);
    let task = app::update(&mut state, Message::WindowFocused(true));
    settle(&mut state, task);
    assert_eq!(state.installation.title_id(), Some("CUSA01623"));
    assert_eq!(patch_names(&state), ["Fix Alpha"]);
    assert_eq!(home::chips(&state)[1], "✓ CUSA01623 · v01.02");
    assert_eq!(state.trophies.icons.len(), TROPHY_COUNT);

    // Without the setup, the focus reads nothing.
    let dir = tempfile::tempdir().unwrap();
    let mut state = copied_into_the_chosen_folder(dir.path());
    let before = views(&state);
    let task = app::update(&mut state, Message::WindowFocused(true));
    assert_eq!(task.units(), 0, "a read started");
    assert_eq!(views(&state), before);
}

#[test]
fn a_restore_reads_the_saves_folder_again() {
    // Spec §4/06: after a restore every view of the saves folder follows, the trophies the restored
    // saves unlock included.
    let dir = tempfile::tempdir().unwrap();
    let mut state = state_with_trophies(dir.path());
    load_trophies(&mut state);
    assert_eq!(state.trophies.unlocked, BTreeSet::from([0, 3]));
    assert!(!state.home.saves);
    // What the restore put in place: other saves, and the backup of the replaced ones.
    let user = state.paths.user_dir("");
    std::fs::write(user.join("trophies.dat"), [0b0110u8, 0, 0, 0]).unwrap();
    std::fs::create_dir_all(user.join("savedata")).unwrap();
    std::fs::write(user.join("savedata").join("slot0"), b"x").unwrap();
    std::fs::create_dir_all(backups_folder(&state).join(NEWER)).unwrap();
    let task = app::update(
        &mut state,
        Message::Backup(BackupMessage::Restored(Ok(Some(NEWER.into())))),
    );
    settle(&mut state, task);
    assert_eq!(state.backups, [NEWER]);
    assert_eq!(state.backup_choice, NEWER);
    assert!(state.home.saves);
    assert_eq!(state.trophies.unlocked, BTreeSet::from([1, 2]));
}

#[test]
fn the_start_reads_every_view_but_the_trophies() {
    // The Start column: everything on the disk before the window opens shows at once, the Mods
    // lists included (read then, not on a thread); the Trophies list waits for its page.
    let dir = tempfile::tempdir().unwrap();
    let game = dump_with_patches(dir.path());
    trophies_in(&game);
    let paths = paths(dir.path());
    std::fs::create_dir_all(&paths.data_dir).unwrap();
    std::fs::create_dir_all(paths.ini_path().parent().unwrap()).unwrap();
    std::fs::write(
        paths.config_file(),
        json!({"game_dir": text(&game)}).to_string(),
    )
    .unwrap();
    std::fs::write(paths.ini_path(), "render_resolution=3440x1440\n").unwrap();
    let user = paths.user_dir("");
    std::fs::create_dir_all(user.join("savedata")).unwrap();
    std::fs::write(user.join("savedata").join("slot0"), b"x").unwrap();
    std::fs::create_dir_all(user.join("savedata_backups").join(NEWER)).unwrap();
    size_mod_in(&paths.mods_dir(""));

    let state = state(dir.path());
    assert_eq!(state.installation.title_id(), Some("CUSA01623"));
    assert!(state.home.saves);
    assert_eq!(patch_names(&state), ["Fix Alpha"]);
    assert_eq!(state.backups, [NEWER]);
    assert_eq!(state.backup_choice, NEWER);
    assert_eq!(state.mod_resolutions, ["3840x1080"]);
    assert_eq!(state.aspect, "21:9");
    assert_eq!(state.mods.order, ["Ultrawide"]);
    assert_eq!(state.mods.inputs, inputs(View::Mods, &state.store));
    assert!(state.trophies.icons.is_empty());
    assert!(!state.trophies.loading, "a Trophies read started");
}

#[test]
fn a_rebuild_reads_the_game_and_the_mods_again_and_nothing_else() {
    // The Rebuild column: another language or zoom reads the game folders, the Home facts, the game
    // patches and the Mods lists again (those at once), and nothing else.
    let dir = tempfile::tempdir().unwrap();
    let mut state = state(dir.path());
    let game = dir.path().join("dump");
    std::fs::create_dir_all(&game).unwrap();
    browse(&mut state, "game_dir", &game);
    // The disk of every view changes, and the render resolution without its follow.
    dump_with_patches(dir.path());
    trophies_in(&game);
    let user = state.paths.user_dir("");
    std::fs::create_dir_all(user.join("savedata")).unwrap();
    std::fs::write(user.join("savedata").join("slot0"), b"x").unwrap();
    std::fs::create_dir_all(backups_folder(&state).join(NEWER)).unwrap();
    make_mod(&state, "Ultrawide", "a.bin");
    state.store.set("render_resolution", json!("3440x1440"));

    let scale = state.scale;
    let task = persist::rebuild(&mut state, "pt".into(), scale);
    assert_eq!(state.installation.title_id(), Some("CUSA01623"));
    assert!(state.home.saves);
    assert_eq!(patch_names(&state), ["Fix Alpha"]);
    // At once, before any background read arrives.
    assert_eq!(state.mods.order, ["Ultrawide"]);
    settle(&mut state, task);
    assert!(state.backups.is_empty(), "{:?}", state.backups);
    assert_eq!(state.aspect, "16:9");
    assert!(state.trophies.icons.is_empty());
    assert!(!persist::dirty(&state));
}
