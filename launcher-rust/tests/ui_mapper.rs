#![allow(clippy::unwrap_used, clippy::expect_used)]
//! The "Set up a controller" page (Task 52) over a scripted joystick: the window halves of
//! `tests/test_controller_mapper_sdl.py` and the refusal of `tests/test_controller_mapper_launch.py`.
//! The SDL halves are in `tests/sdl_mapper.rs`.

mod common;
#[path = "common/joysticks.rs"]
mod joysticks;

use std::fs;
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};

use iced::keyboard::key::Named;
use iced::keyboard::{Key, Location, Modifiers};
use iced_test::simulator::Simulator;
use indexmap::IndexMap;
use joysticks::{pad, rest, FakeJoysticks};
use spartanport_launcher::core::controller_mappings::{mapping_line, FILENAME};
use spartanport_launcher::ui::app::{self, Message, State};
use spartanport_launcher::ui::pages::mapper::{self, MapperMessage, MapperRow, MapperState};
use spartanport_launcher::ui::pages::Page;
use spartanport_launcher::ui::play::{self, GameMessage, GameState};
use spartanport_launcher::ui::widgets::modal::ModalKind;

/// The scripted pad's GUID.
const GUID: &str = "03000000faca0000f001000000000000";
const ANY_ORDER: &str = "Configure any control, in any order.";
const KEPT: &str = "Saved changes are kept when you close this window.";

fn map(pairs: &[(&str, &str)]) -> IndexMap<String, String> {
    pairs
        .iter()
        .map(|(key, value)| (key.to_string(), value.to_string()))
        .collect()
}

/// A launcher over `dir` in English.
fn launcher(dir: &Path) -> State {
    let mut state = common::state(dir);
    state.lang = "en".to_string();
    state
}

/// The page opened from Controls over `fake` (the Python's `window()`).
fn open_over(dir: &Path, fake: &FakeJoysticks) -> State {
    let mut state = launcher(dir);
    common::open(&mut state, Page::Controls);
    state.open_mapper_with(Box::new(fake.clone()));
    state
}

/// The page over a fake that lists one raw pad.
fn opened(dir: &Path) -> (State, FakeJoysticks) {
    let fake = FakeJoysticks::new("Windows", vec![pad(3, GUID)]);
    let state = open_over(dir, &fake);
    (state, fake)
}

fn send(state: &mut State, message: MapperMessage) {
    let _ = app::update(state, Message::Mapper(message));
}

fn escape(state: &mut State) {
    let _ = app::update(
        state,
        Message::Key(
            Key::Named(Named::Escape),
            Modifiers::default(),
            Location::Standard,
        ),
    );
}

fn mapper(state: &State) -> &MapperState {
    state.mapper.as_ref().expect("the mapper page is open")
}

fn profiles(state: &State) -> PathBuf {
    state.paths.data_dir.join(FILENAME)
}

fn row(state: &State, key: &str) -> MapperRow {
    mapper::rows(mapper(state), "en")
        .into_iter()
        .find(|row| row.key == key)
        .unwrap()
}

/// What a click on `target` sends, in a window tall enough for the whole page.
fn clicked<S>(state: &State, target: S) -> Vec<Message>
where
    S: iced_test::Selector + Send,
    S::Output: iced_test::selector::Bounded + Clone + Send + Sync + 'static,
{
    let mut ui = Simulator::with_size(
        iced::Settings::default(),
        iced::Size::new(1280.0, 2000.0),
        app::view(state),
    );
    ui.click(target)
        .unwrap_or_else(|error| panic!("click: {error:?}"));
    ui.into_messages().collect()
}

/// Clicks `label` and applies what it sent.
fn click(state: &mut State, label: &str) {
    for message in clicked(state, label) {
        let _ = app::update(state, message);
    }
}

/// The Python's `capture_button`: Configure on `target`, then `button` held for three polls.
fn capture_button(state: &mut State, fake: &FakeJoysticks, target: &str, button: usize) {
    send(state, MapperMessage::Configure(target.to_string()));
    let device = fake.script().selected.clone().expect("a chosen pad");
    let mut held = rest(&device);
    held.buttons[button] = true;
    for _ in 0..3 {
        fake.script().states.push_back(held.clone());
        send(state, MapperMessage::Poll);
    }
}

/// Saving `path` fails while it stays readable: a read-only file on Windows, a read-only folder
/// elsewhere. False when the system writes anyway (a superuser on unix).
fn block_saves(path: &Path) -> bool {
    #[cfg(windows)]
    {
        let mut permissions = fs::metadata(path).unwrap().permissions();
        permissions.set_readonly(true);
        fs::set_permissions(path, permissions).unwrap();
        true
    }
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        let folder = path.parent().unwrap();
        fs::set_permissions(folder, fs::Permissions::from_mode(0o555)).unwrap();
        let probe = folder.join("probe");
        if fs::write(&probe, b"").is_ok() {
            let _ = fs::remove_file(&probe);
            unblock_saves(path);
            return false;
        }
        true
    }
}

fn unblock_saves(path: &Path) {
    #[cfg(windows)]
    {
        let mut permissions = fs::metadata(path).unwrap().permissions();
        // Windows: this clears the read-only attribute, nothing more.
        #[allow(clippy::permissions_set_readonly_false)]
        permissions.set_readonly(false);
        fs::set_permissions(path, permissions).unwrap();
    }
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        let folder = path.parent().unwrap();
        fs::set_permissions(folder, fs::Permissions::from_mode(0o755)).unwrap();
    }
}

// --- A live game, as in tests/ui_play.rs ---

/// A dump with every file PLAY checks.
fn full_dump(folder: &Path) {
    fs::create_dir_all(folder.join("sce_sys")).unwrap();
    fs::write(folder.join("eboot.bin"), b"elf").unwrap();
    fs::write(
        folder.join("sce_sys").join("param.sfo"),
        common::sfo(&[
            ("TITLE_ID", "CUSA01623"),
            ("TITLE", "God of War III"),
            ("APP_VER", "01.02"),
        ]),
    )
    .unwrap();
    fs::create_dir_all(folder.join("sce_module")).unwrap();
    fs::write(folder.join("sce_module").join("libc.prx"), b"prx").unwrap();
    fs::write(folder.join("sce_module").join("libSceFios2.prx"), b"prx").unwrap();
}

/// Applies the fake game's events as the clock does until it ended.
fn wait_for_the_exit(state: &mut State) {
    let started = Instant::now();
    while !matches!(state.game, GameState::Idle) {
        assert!(
            started.elapsed() < Duration::from_secs(10),
            "{:?}",
            state.game
        );
        for message in play::drain_events(state) {
            let _ = app::update(state, Message::Game(message));
        }
        std::thread::sleep(Duration::from_millis(10));
    }
}

#[test]
fn mapper_is_not_opened_over_a_running_game() {
    let dir = tempfile::tempdir().unwrap();
    let dump = dir.path().join("dump");
    full_dump(&dump);
    let mut state = launcher(dir.path());
    let _ = app::update(
        &mut state,
        Message::Set("game_dir".into(), dump.display().to_string().into()),
    );
    state.launch_command = Some(
        [
            env!("CARGO_BIN_EXE_fake-game"),
            "--lines",
            "1000",
            "--delay-ms",
            "50",
            "--exit",
            "0",
        ]
        .map(String::from)
        .to_vec(),
    );
    let _ = app::update(&mut state, Message::Play);
    assert!(state.game.is_live());
    common::open(&mut state, Page::Controls);
    let sent = common::messages(&state, "Set up a controller");
    assert!(
        matches!(sent.as_slice(), [Message::Mapper(MapperMessage::Open)]),
        "{sent:?}"
    );
    for message in sent {
        let _ = app::update(&mut state, message);
    }
    assert!(state.mapper.is_none());
    assert_eq!(state.page, Page::Controls);
    let shown = state.modal.take().expect("a notice");
    assert!(matches!(shown.kind, ModalKind::Info), "{:?}", shown.kind);
    assert_eq!(shown.title, "Set up a controller");
    assert_eq!(shown.text, "Stop the game before setting up a controller.");
    let _ = app::update(&mut state, Message::Stop);
    wait_for_the_exit(&mut state);
}

#[test]
fn a_play_after_a_copy_closes_the_mapper_before_the_game_starts() {
    // The copy of the system files ends in PLAY; the mapper, opened while it ran, lets go of the
    // pad before the game starts.
    let dir = tempfile::tempdir().unwrap();
    let dump = dir.path().join("dump");
    full_dump(&dump);
    let mut state = launcher(dir.path());
    let _ = app::update(
        &mut state,
        Message::Set("game_dir".into(), dump.display().to_string().into()),
    );
    state.launch_command = Some(
        [
            env!("CARGO_BIN_EXE_fake-game"),
            "--lines",
            "1000",
            "--delay-ms",
            "50",
            "--exit",
            "0",
        ]
        .map(String::from)
        .to_vec(),
    );
    let fake = FakeJoysticks::new("Windows", vec![pad(3, GUID)]);
    common::open(&mut state, Page::Controls);
    state.open_mapper_with(Box::new(fake.clone()));
    assert_eq!(state.page, Page::Mapper);
    state.game = GameState::Copying;
    let closes = fake.script().closed;
    let _ = app::update(&mut state, Message::Game(GameMessage::AdoptDone(Ok(()))));
    assert!(state.game.is_live());
    assert!(state.mapper.is_none());
    assert!(fake.script().closed > closes);
    assert_eq!(state.page, Page::Controls);
    let _ = app::update(&mut state, Message::Stop);
    wait_for_the_exit(&mut state);
}

#[test]
fn rows_save_out_of_order_and_reload_without_losing_other_bindings() {
    let dir = tempfile::tempdir().unwrap();
    let (mut state, fake) = opened(dir.path());
    assert_eq!(state.page, Page::Mapper);
    assert_eq!(mapper(&state).prompt, ANY_ORDER);
    assert_eq!(mapper(&state).status, KEPT);
    assert_eq!(mapper(&state).details, "12 buttons, 6 axes, 1 D-pad hats");
    let path = profiles(&state);
    // Square first: Cross need not be mapped first.
    capture_button(&mut state, &fake, "x", 7);
    assert!(fs::read_to_string(&path).unwrap().contains("x:b7,"));
    assert_eq!(state.page, Page::Mapper);
    assert_eq!(mapper(&state).status, "Saved: Square / X");
    capture_button(&mut state, &fake, "b", 2);
    // Correct just one row.
    capture_button(&mut state, &fake, "x", 4);
    let saved = fs::read_to_string(&path).unwrap();
    assert!(saved.contains("b:b2,"), "{saved}");
    assert!(saved.contains("x:b4,"), "{saved}");
    assert!(!saved.contains("x:b7,"), "{saved}");
    send(&mut state, MapperMessage::Refresh);
    assert_eq!(mapper(&state).bindings, map(&[("x", "b4"), ("b", "b2")]));
    assert_eq!(row(&state, "x").assigned, "Button 5");
    assert_eq!(row(&state, "b").assigned, "Button 3");
    // Each change was checked by SDL before it was saved.
    assert!(fake
        .script()
        .previewed
        .iter()
        .any(|line| line.contains("x:b7,")));
}

#[test]
fn cancel_capture_and_close_keep_already_saved_rows() {
    let dir = tempfile::tempdir().unwrap();
    let (mut state, fake) = opened(dir.path());
    let path = profiles(&state);
    capture_button(&mut state, &fake, "x", 7);
    let previous = fs::read_to_string(&path).unwrap();
    send(&mut state, MapperMessage::Configure("b".into()));
    assert_eq!(row(&state, "b").configure, "Listening…");
    assert!(!row(&state, "a").configure_enabled);
    assert_eq!(mapper(&state).prompt, "Press Circle / B");
    let mut held = rest(&pad(3, GUID));
    held.buttons[2] = true;
    fake.script().states.push_back(held);
    // Not stable yet.
    send(&mut state, MapperMessage::Poll);
    assert!(mapper(&state).capture.is_some());
    click(&mut state, "Cancel capture");
    assert!(mapper(&state).capture.is_none());
    assert_eq!(
        mapper(&state).status,
        "Capture canceled. Previously saved changes are kept."
    );
    assert_eq!(row(&state, "b").configure, "Configure");
    let closes = fake.script().closed;
    click(&mut state, "Close");
    assert_eq!(fs::read_to_string(&path).unwrap(), previous);
    // Closed: SDL let go of the pad, and the Controls page is back.
    assert!(state.mapper.is_none());
    assert!(fake.script().closed > closes);
    assert_eq!(state.page, Page::Controls);
}

#[test]
fn clear_only_one_row_and_allow_an_empty_profile() {
    let dir = tempfile::tempdir().unwrap();
    let (mut state, fake) = opened(dir.path());
    let path = profiles(&state);
    capture_button(&mut state, &fake, "a", 7);
    capture_button(&mut state, &fake, "b", 2);
    assert!(row(&state, "a").clear_enabled);
    assert!(!row(&state, "x").clear_enabled);
    send(&mut state, MapperMessage::Clear("a".into()));
    assert_eq!(mapper(&state).bindings, map(&[("b", "b2")]));
    assert!(!fs::read_to_string(&path).unwrap().contains("a:b7,"));
    assert_eq!(row(&state, "a").assigned, "Not configured");
    send(&mut state, MapperMessage::Clear("b".into()));
    assert_eq!(mapper(&state).bindings, map(&[]));
    assert!(fake.script().preview.is_none());
    assert!(path.exists());
}

#[test]
fn failed_save_keeps_previous_profile_and_row_values() {
    let dir = tempfile::tempdir().unwrap();
    let (mut state, fake) = opened(dir.path());
    let path = profiles(&state);
    capture_button(&mut state, &fake, "a", 7);
    let previous = fs::read_to_string(&path).unwrap();
    if !block_saves(&path) {
        return;
    }
    capture_button(&mut state, &fake, "x", 2);
    unblock_saves(&path);
    let shown = state.modal.take().expect("an error dialog");
    assert!(matches!(shown.kind, ModalKind::Error), "{:?}", shown.kind);
    assert_eq!(shown.title, "Could not save mapping");
    // The error names what could not be written: the read-only file on Windows, its read-only
    // folder elsewhere (block_saves), where the temporary file cannot be created.
    let failed = if cfg!(windows) {
        path.clone()
    } else {
        path.parent().unwrap().to_path_buf()
    };
    assert!(
        shown.text.contains(&failed.display().to_string()),
        "{}",
        shown.text
    );
    assert_eq!(fs::read_to_string(&path).unwrap(), previous);
    assert_eq!(mapper(&state).bindings, map(&[("a", "b7")]));
    assert_eq!(row(&state, "x").assigned, "Not configured");
    assert!(mapper(&state).capture.is_none());
    assert_eq!(
        mapper(&state).status,
        "The change was not saved. Configure the control again to retry."
    );
    assert_eq!(mapper(&state).prompt, ANY_ORDER);
}

#[test]
fn a_line_sdl_refuses_is_never_saved() {
    // Python `commit`: SDL checks the line before it is saved. One it refuses leaves the file with
    // the bytes it had (here another pad's profile).
    let dir = tempfile::tempdir().unwrap();
    let mut state = launcher(dir.path());
    let path = profiles(&state);
    let other = mapping_line(
        "03000000aaaa0000bbbb000000000000",
        "Other pad",
        &map(&[("a", "b0")]),
        "Windows",
        true,
    )
    .unwrap();
    fs::write(&path, format!("{other}\n")).unwrap();
    let before = fs::read_to_string(&path).unwrap();
    let fake = FakeJoysticks::new("Windows", vec![pad(3, GUID)]);
    common::open(&mut state, Page::Controls);
    state.open_mapper_with(Box::new(fake.clone()));
    fake.script().preview_error = Some("Couldn't parse mapping".to_string());
    capture_button(&mut state, &fake, "a", 7);
    // SDL was asked about the new line, and said no.
    assert!(fake
        .script()
        .previewed
        .iter()
        .any(|line| line.contains("a:b7,")));
    assert_eq!(fs::read_to_string(&path).unwrap(), before);
    let shown = state.modal.take().expect("an error dialog");
    assert!(matches!(shown.kind, ModalKind::Error), "{:?}", shown.kind);
    assert_eq!(shown.title, "Could not save mapping");
    assert_eq!(shown.text, "Couldn't parse mapping");
    assert_eq!(mapper(&state).bindings, map(&[]));
    assert_eq!(row(&state, "a").assigned, "Not configured");
    assert_eq!(
        mapper(&state).status,
        "The change was not saved. Configure the control again to retry."
    );
}

#[cfg(windows)]
#[test]
fn an_sdl3_that_does_not_load_says_why_in_a_dialog() {
    // Python `open_controller_mapper`: an error opening SDL3 shows in a dialog titled as the page,
    // and no page opens. The port's broken DLL fails in the loader, before SDL holds any state.
    let dir = tempfile::tempdir().unwrap();
    let mut state = launcher(dir.path());
    let broken = state.paths.port_dir.join("bin").join("SDL3.dll");
    fs::create_dir_all(broken.parent().unwrap()).unwrap();
    fs::write(&broken, b"not a library").unwrap();
    common::open(&mut state, Page::Controls);
    send(&mut state, MapperMessage::Open);
    assert!(state.mapper.is_none());
    assert_eq!(state.page, Page::Controls);
    let shown = state.modal.take().expect("an error dialog");
    assert!(matches!(shown.kind, ModalKind::Error), "{:?}", shown.kind);
    assert_eq!(shown.title, "Set up a controller");
    // The system's reason, as the loader gave it.
    assert!(shown.text.contains("(os error "), "{}", shown.text);
}

#[test]
fn parent_destroy_closes_sdl_without_saving() {
    let dir = tempfile::tempdir().unwrap();
    let (mut state, fake) = opened(dir.path());
    let closes = fake.script().closed;
    let task = app::update(
        &mut state,
        Message::CloseRequested(iced::window::Id::unique()),
    );
    assert_eq!(task.units(), 1, "the window closes");
    assert!(state.mapper.is_none());
    assert!(fake.script().closed > closes);
    assert!(!profiles(&state).exists());
}

#[test]
fn a_conflicting_input_asks_to_clear_the_other_row() {
    let dir = tempfile::tempdir().unwrap();
    let (mut state, fake) = opened(dir.path());
    let path = profiles(&state);
    capture_button(&mut state, &fake, "a", 7);
    let saved = fs::read_to_string(&path).unwrap();
    // Circle on the button Cross already has.
    capture_button(&mut state, &fake, "b", 7);
    assert_eq!(
        mapper(&state).status,
        "That input is assigned to another control. Clear its row first or choose another input."
    );
    // Still listening for Circle; nothing changed.
    assert!(mapper(&state).capture.is_some());
    assert_eq!(mapper(&state).editing.as_deref(), Some("b"));
    assert_eq!(fs::read_to_string(&path).unwrap(), saved);
    assert_eq!(mapper(&state).bindings, map(&[("a", "b7")]));
}

#[test]
fn no_controller_says_connect_it() {
    let dir = tempfile::tempdir().unwrap();
    let fake = FakeJoysticks::new("Windows", Vec::new());
    let mut state = open_over(dir.path(), &fake);
    assert_eq!(
        mapper(&state).prompt,
        "No controller found. Connect it and choose Refresh."
    );
    assert_eq!(mapper(&state).details, "");
    let rows = mapper::rows(mapper(&state), "en");
    assert_eq!(rows.len(), 20);
    for row in &rows {
        assert_eq!(row.assigned, "Not configured", "{row:?}");
        assert!(!row.configure_enabled && !row.clear_enabled, "{row:?}");
    }
    let configure = || iced_test::selector::id(mapper::configure_id("a"));
    assert!(clicked(&state, configure()).is_empty());
    send(&mut state, MapperMessage::Configure("a".into()));
    assert!(mapper(&state).capture.is_none());
    // Plugged in later: Refresh finds it.
    fake.script().devices = vec![pad(3, GUID)];
    click(&mut state, "Refresh");
    assert_eq!(mapper(&state).details, "12 buttons, 6 axes, 1 D-pad hats");
    assert_eq!(mapper(&state).prompt, ANY_ORDER);
    assert!(row(&state, "a").configure_enabled);
    let sent = clicked(&state, configure());
    assert!(
        matches!(sent.as_slice(), [Message::Mapper(MapperMessage::Configure(key))] if key == "a"),
        "{sent:?}"
    );
}

#[test]
fn escape_cancels_the_capture_then_closes_the_page() {
    let dir = tempfile::tempdir().unwrap();
    let (mut state, fake) = opened(dir.path());
    send(&mut state, MapperMessage::Configure("a".into()));
    assert!(mapper(&state).capture.is_some());
    escape(&mut state);
    assert!(mapper(&state).capture.is_none());
    assert_eq!(state.page, Page::Mapper);
    let closes = fake.script().closed;
    escape(&mut state);
    assert!(state.mapper.is_none());
    assert!(fake.script().closed > closes);
    assert_eq!(state.page, Page::Controls);
}

#[test]
fn another_tab_closes_the_page() {
    let dir = tempfile::tempdir().unwrap();
    let (mut state, fake) = opened(dir.path());
    let closes = fake.script().closed;
    common::open(&mut state, Page::Graphics);
    assert!(state.mapper.is_none());
    assert!(fake.script().closed > closes);
}

#[test]
fn what_opening_could_not_read_shows_in_the_status() {
    // The Python printed it to a console the windowed launcher does not have.
    let dir = tempfile::tempdir().unwrap();
    let fake = FakeJoysticks::new("Windows", vec![pad(3, GUID)]);
    let warning = "Controller mappings: could not read C:\\data\\gamecontrollerdb.txt: bad";
    fake.script().warnings = vec![warning.to_string()];
    let state = open_over(dir.path(), &fake);
    assert_eq!(mapper(&state).status, format!("{warning}\n{KEPT}"));
}

#[test]
fn an_sdl_mapping_is_previewed_and_keeps_its_other_fields() {
    // Python `test_editing_an_existing_sdl_mapping_preserves_guide_and_other_buttons`, on the page.
    let dir = tempfile::tempdir().unwrap();
    let fake = FakeJoysticks::new("Windows", vec![pad(3, GUID)]);
    let line =
        mapping_line(GUID, "USB", &map(&[("a", "b7")]), "Windows", true).unwrap() + "guide:b10,";
    fake.script().mapping = Some(line.clone());
    let mut state = open_over(dir.path(), &fake);
    assert_eq!(mapper(&state).bindings, map(&[("a", "b7")]));
    assert_eq!(fake.script().preview.as_deref(), Some(line.as_str()));
    {
        let mut script = fake.script();
        script.pressed = vec!["Cross / A", "D-pad up"];
        // Python's `round` gives 0, not -0, for a stick just off the centre.
        script.axes = [32767, -1, 0, -32768, 0, 16384];
    }
    send(&mut state, MapperMessage::Poll);
    assert_eq!(
        mapper(&state).preview_text,
        "Pressed: Cross / A, D-pad up\nLeft stick: 100 / 0   Right stick: 0 / -100\nL2: 0   R2: 50"
    );
    capture_button(&mut state, &fake, "b", 2);
    let text = fs::read_to_string(profiles(&state)).unwrap();
    assert!(text.contains("a:b7,"), "{text}");
    assert!(text.contains("b:b2,"), "{text}");
    assert!(text.contains("guide:b10,"), "{text}");
}
