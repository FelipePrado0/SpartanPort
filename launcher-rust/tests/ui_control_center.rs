#![allow(clippy::unwrap_used, clippy::expect_used)]
//! The Controls page's live picture, remap by press, dead zones and profiles, over a scripted pad.

mod common;
#[path = "common/joysticks.rs"]
mod joysticks;

use common::{open, state};
use iced::keyboard::key::Named;
use iced::keyboard::{Key, Location, Modifiers};
use joysticks::{pad, FakeJoysticks};
use serde_json::{json, Value};
use spartanport_launcher::core::control_profiles::{self, MOUSE_SIDE_BUTTONS};
use spartanport_launcher::core::controls::{control_map, Kind, Slot};
use spartanport_launcher::core::joysticks::{Device, JoystickThread, Joysticks, PREVIEW_BUTTONS};
use spartanport_launcher::core::pad_model::{sdl_button, PadModel};
use spartanport_launcher::ui::app::{self, Message, State};
use spartanport_launcher::ui::pages::control_center;
use spartanport_launcher::ui::pages::controls::{self, ControlsMessage, Device as Shown};
use spartanport_launcher::ui::pages::Page;
use spartanport_launcher::ui::persist;

const SONY: &str = "030000004c050000e60c000000000000";
const MICROSOFT: &str = "030000005e040000ea02000000000000";
const MAPPING: &str = "030000004c050000e60c000000000000,Pad,a:b0,b:b1,platform:Windows,";

fn send(state: &mut State, message: ControlsMessage) {
    let _ = app::update(state, Message::Controls(message));
}

fn sony() -> Device {
    pad(1, SONY)
}

/// The Controls page on the gamepad, reading `fake`.
fn with_pad(dir: &std::path::Path, fake: &FakeJoysticks) -> State {
    fake.script().mapping = Some(MAPPING.to_string());
    let mut state = state(dir);
    state.lang = "en".into();
    open(&mut state, Page::Controls);
    send(&mut state, ControlsMessage::Device(Shown::Gamepad));
    state.controls.live_error = None;
    state.open_live_pad_with(Box::new(fake.clone()));
    state
}

fn poll(state: &mut State) {
    send(state, ControlsMessage::LivePoll);
}

fn held(state: &State) -> Vec<String> {
    state.live_pad.as_ref().unwrap().pressed.clone()
}

#[test]
fn every_button_shows_as_it_is_pressed() {
    let dir = tempfile::tempdir().unwrap();
    let fake = FakeJoysticks::new("Windows", vec![sony()]);
    let mut state = with_pad(dir.path(), &fake);
    for name in PREVIEW_BUTTONS {
        fake.script().pressed = vec![name];
        poll(&mut state);
        assert_eq!(held(&state), vec![sdl_button(name).unwrap()], "{name}");
        fake.script().pressed = Vec::new();
        poll(&mut state);
        assert!(held(&state).is_empty());
    }
}

#[test]
fn the_triggers_count_as_buttons_past_the_games_threshold() {
    let dir = tempfile::tempdir().unwrap();
    let fake = FakeJoysticks::new("Windows", vec![sony()]);
    let mut state = with_pad(dir.path(), &fake);
    fake.script().axes = [0, 0, 0, 0, 3000, 32767];
    poll(&mut state);
    assert_eq!(held(&state), vec!["righttrigger"]);
}

#[test]
fn a_click_then_a_press_moves_the_game_button_and_swaps() {
    let dir = tempfile::tempdir().unwrap();
    let fake = FakeJoysticks::new("Windows", vec![sony()]);
    let mut state = with_pad(dir.path(), &fake);
    send(&mut state, ControlsMessage::PartPressed("a".into()));
    assert_eq!(state.controls.pad_capturing.as_deref(), Some("cross"));
    fake.script().pressed = vec!["Circle / B"];
    poll(&mut state);
    assert_eq!(state.controls.pad_capturing, None);
    assert_eq!(state.controls.pad["cross"], "b");
    assert_eq!(state.controls.pad["circle"], "a");
    let saved = json!({"pad": {"cross": "b", "circle": "a"}});
    assert_eq!(control_map(&saved, Kind::Pad), "cross=b,circle=a");
}

/// #156: the triggers on the shoulders and the shoulders on the triggers.
#[test]
fn the_triggers_can_swap_with_the_shoulders() {
    let dir = tempfile::tempdir().unwrap();
    let fake = FakeJoysticks::new("Windows", vec![sony()]);
    let mut state = with_pad(dir.path(), &fake);
    send(
        &mut state,
        ControlsMessage::PartPressed("lefttrigger".into()),
    );
    assert_eq!(state.controls.pad_capturing.as_deref(), Some("l2"));
    fake.script().pressed = vec!["L1 / LB"];
    poll(&mut state);
    assert_eq!(state.controls.pad["l2"], "leftshoulder");
    assert_eq!(state.controls.pad["l1"], "lefttrigger");
}

/// #1098: photo mode on a button of its own.
#[test]
fn touchpad_left_takes_a_button_and_starts_on_none() {
    let dir = tempfile::tempdir().unwrap();
    let fake = FakeJoysticks::new("Windows", vec![sony()]);
    let mut state = with_pad(dir.path(), &fake);
    assert_eq!(state.controls.pad["touchpad_left"], "");
    send(
        &mut state,
        ControlsMessage::PadPick("touchpad_left".into(), "guide".into()),
    );
    let saved = json!({"pad": {"touchpad_left": "guide"}});
    assert_eq!(control_map(&saved, Kind::Pad), "touchpad_left=guide");
}

#[test]
fn a_remap_gives_up_on_esc_and_after_five_seconds() {
    let dir = tempfile::tempdir().unwrap();
    let fake = FakeJoysticks::new("Windows", vec![sony()]);
    let mut state = with_pad(dir.path(), &fake);
    send(&mut state, ControlsMessage::PartPressed("a".into()));
    let _ = app::update(
        &mut state,
        Message::Key(
            Key::Named(Named::Escape),
            Modifiers::empty(),
            Location::Standard,
        ),
    );
    assert_eq!(state.controls.pad_capturing, None);
    assert_eq!(state.page, Page::Controls);

    send(&mut state, ControlsMessage::PartPressed("a".into()));
    let started = state.controls.pad_capture_started.unwrap();
    control_center::tick(&mut state, started + std::time::Duration::from_secs(4));
    assert!(state.controls.pad_capturing.is_some());
    control_center::tick(&mut state, started + std::time::Duration::from_secs(5));
    assert_eq!(state.controls.pad_capturing, None);
    assert_eq!(state.controls.pad["cross"], "a");
}

#[test]
fn a_controller_can_come_and_go() {
    let dir = tempfile::tempdir().unwrap();
    let fake = FakeJoysticks::new("Windows", Vec::new());
    let mut state = with_pad(dir.path(), &fake);
    assert!(state.live_pad.as_ref().unwrap().device.is_none());
    fake.script().devices = vec![sony()];
    for _ in 0..15 {
        poll(&mut state);
    }
    assert!(state.live_pad.as_ref().unwrap().device.is_some());
    send(&mut state, ControlsMessage::PartPressed("a".into()));
    fake.script().unplugged = true;
    poll(&mut state);
    assert!(state.live_pad.as_ref().unwrap().device.is_none());
    assert_eq!(state.controls.pad_capturing, None);
}

#[test]
fn the_model_follows_the_controller_until_the_player_picks_one() {
    let dir = tempfile::tempdir().unwrap();
    let fake = FakeJoysticks::new("Windows", vec![pad(1, MICROSOFT)]);
    let mut state = with_pad(dir.path(), &fake);
    assert_eq!(state.controls.pad_model, PadModel::Xbox);
    assert_eq!(state.controls.pad_style, "xbox");
    send(&mut state, ControlsMessage::PadModel(PadModel::SwitchPro));
    poll(&mut state);
    assert_eq!(state.controls.pad_model, PadModel::SwitchPro);
    assert_eq!(state.controls.pad_style, "nintendo");
    send(&mut state, ControlsMessage::PadStyleAuto);
    poll(&mut state);
    assert_eq!(state.controls.pad_model, PadModel::Xbox);
    assert_eq!(state.controls.pad_style, "xbox");
}

#[test]
fn a_dualshock_4_shows_its_own_picture() {
    let dir = tempfile::tempdir().unwrap();
    let fake = FakeJoysticks::new("Windows", vec![pad(1, "030000004c050000cc09000000000000")]);
    let state = with_pad(dir.path(), &fake);
    assert_eq!(state.controls.pad_model, PadModel::DualShock4);
    assert_eq!(state.controls.pad_style, "playstation");
}

/// The arrows light up on the keyboard picture instead of changing the page.
#[test]
fn the_arrows_stay_on_the_page_while_the_keyboard_shows() {
    let dir = tempfile::tempdir().unwrap();
    let mut state = state(dir.path());
    open(&mut state, Page::Controls);
    for key in [Named::ArrowLeft, Named::ArrowRight] {
        let _ = app::update(
            &mut state,
            Message::Key(Key::Named(key), Modifiers::empty(), Location::Standard),
        );
        assert_eq!(state.page, Page::Controls);
    }
    send(&mut state, ControlsMessage::Device(Shown::Gamepad));
    let _ = app::update(
        &mut state,
        Message::Key(
            Key::Named(Named::ArrowRight),
            Modifiers::empty(),
            Location::Standard,
        ),
    );
    assert_ne!(state.page, Page::Controls);
}

#[test]
fn a_generic_pad_keeps_the_saved_model() {
    let dir = tempfile::tempdir().unwrap();
    let fake = FakeJoysticks::new("Windows", vec![pad(1, "03000000ad1b00000000000000000000")]);
    let state = with_pad(dir.path(), &fake);
    assert_eq!(state.controls.pad_model, PadModel::DualSense);
}

#[test]
fn drift_is_measured_and_its_dead_zone_applied() {
    let dir = tempfile::tempdir().unwrap();
    let fake = FakeJoysticks::new("Windows", vec![sony()]);
    let mut state = with_pad(dir.path(), &fake);
    fake.script().axes = [1311, 0, 0, 0, 0, 0]; // 4% on the left stick
    send(&mut state, ControlsMessage::MeasureDrift);
    for _ in 0..100 {
        poll(&mut state);
    }
    assert_eq!(state.live_pad.as_ref().unwrap().suggestion, Some([7, 0]));
    send(&mut state, ControlsMessage::ApplyDrift);
    assert_eq!(state.store.text("deadzone_left"), "7");
    assert_eq!(state.store.text("deadzone_right"), "0");
}

/// The new choices stay out of settings.json until set, then survive a restart.
#[test]
fn dead_zones_and_the_model_choice_are_saved() {
    let dir = tempfile::tempdir().unwrap();
    let mut first = state(dir.path());
    open(&mut first, Page::Controls);
    let _ = app::update(
        &mut first,
        Message::Set("deadzone_left".into(), Value::from(9)),
    );
    send(&mut first, ControlsMessage::PadModel(PadModel::Xbox));
    persist::collect(&mut first).unwrap();
    let second = state(dir.path());
    assert_eq!(second.store.text("deadzone_left"), "9");
    assert!(!second.store.flag("pad_style_auto"));
    assert_eq!(second.controls.pad_style, "xbox");
    assert_eq!(second.controls.pad_model, PadModel::Xbox);
}

/// The window never waits for SDL: a slow start lists nothing, then the pad shows up.
#[test]
fn a_slow_start_does_not_hold_the_page() {
    let fake = FakeJoysticks::new("Windows", vec![sony()]);
    fake.script().mapping = Some(MAPPING.to_string());
    let (go, wait) = std::sync::mpsc::channel::<()>();
    let adapter = fake.clone();
    let mut thread = JoystickThread::spawn(move || {
        let _ = wait.recv();
        Ok(Box::new(adapter) as Box<dyn Joysticks>)
    });
    assert_eq!(thread.failure(), None);
    assert!(thread.devices().unwrap().is_empty());
    go.send(()).unwrap();
    let mut listed = Vec::new();
    for _ in 0..2000 {
        let _ = thread.failure();
        listed = thread.devices().unwrap();
        if !listed.is_empty() {
            break;
        }
        std::thread::sleep(std::time::Duration::from_millis(1));
    }
    assert_eq!(listed, vec![sony()]);
    assert_eq!(thread.platform(), "Windows");
    thread.select(&sony()).unwrap();
    thread.begin_preview(MAPPING).unwrap();
    fake.script().pressed = vec!["Cross / A"];
    assert_eq!(thread.preview_state().unwrap().0, vec!["Cross / A"]);
    assert!(thread.rumble(1, 2, 10));
    assert_eq!(fake.script().rumbles, vec![(1, 2)]);
}

#[test]
fn the_motors_follow_the_vibration_setting() {
    let dir = tempfile::tempdir().unwrap();
    let fake = FakeJoysticks::new("Windows", vec![sony()]);
    let mut state = with_pad(dir.path(), &fake);
    let _ = app::update(
        &mut state,
        Message::Set("vibration".into(), Value::from(50)),
    );
    send(&mut state, ControlsMessage::Rumble(true));
    send(&mut state, ControlsMessage::Rumble(false));
    assert_eq!(fake.script().rumbles, vec![(32767, 0), (0, 32767)]);
}

#[test]
fn leaving_the_page_or_picking_the_keyboard_lets_the_pad_go() {
    let dir = tempfile::tempdir().unwrap();
    let fake = FakeJoysticks::new("Windows", vec![sony()]);
    let mut state = with_pad(dir.path(), &fake);
    open(&mut state, Page::Game);
    assert!(state.live_pad.is_none());
    assert_eq!(fake.script().closed, 1);

    let mut state = with_pad(dir.path(), &fake);
    send(&mut state, ControlsMessage::Device(Shown::Keyboard));
    assert!(state.live_pad.is_none());
}

#[test]
fn without_sdl_the_page_says_so_and_the_keyboard_keeps_working() {
    let dir = tempfile::tempdir().unwrap();
    let mut state = state(dir.path());
    state.lang = "en".into();
    open(&mut state, Page::Controls);
    send(&mut state, ControlsMessage::Device(Shown::Gamepad));
    // SDL starts on its own thread, so the page finds out on a later read.
    for _ in 0..2000 {
        poll(&mut state);
        if state.controls.live_error.is_some() {
            break;
        }
        std::thread::sleep(std::time::Duration::from_millis(1));
    }
    assert!(state.controls.live_error.is_some());
    assert!(state.live_pad.is_none());
    send(&mut state, ControlsMessage::PartPressed("a".into()));
    assert_eq!(state.controls.pad_capturing, None);
    assert!(!state.controls.status.is_empty());

    send(&mut state, ControlsMessage::Device(Shown::Keyboard));
    send(&mut state, ControlsMessage::PartPressed("Space".into()));
    assert_eq!(state.controls.capturing.as_deref(), Some("cross"));
}

#[test]
fn a_click_on_a_key_waits_on_its_row() {
    let dir = tempfile::tempdir().unwrap();
    let mut state = state(dir.path());
    state.lang = "en".into();
    open(&mut state, Page::Controls);
    send(
        &mut state,
        ControlsMessage::PartPressed("Mouse Left".into()),
    );
    assert_eq!(state.controls.capturing.as_deref(), Some("square"));
    send(&mut state, ControlsMessage::CaptureTimeout);
    send(&mut state, ControlsMessage::PartPressed("Y".into()));
    assert_eq!(state.controls.capturing, None);
    assert_eq!(state.controls.status, "No game button uses this input.");
}

#[test]
fn held_keys_and_buttons_track_down_and_up() {
    let dir = tempfile::tempdir().unwrap();
    let mut state = state(dir.path());
    open(&mut state, Page::Controls);
    send(&mut state, ControlsMessage::Held("Q".into(), true));
    send(&mut state, ControlsMessage::Held("Q".into(), true));
    assert_eq!(state.controls.held, vec!["Q"]);
    send(&mut state, ControlsMessage::Held("Q".into(), false));
    assert!(state.controls.held.is_empty());
}

#[test]
fn profiles_save_load_and_a_bad_file_changes_nothing() {
    let dir = tempfile::tempdir().unwrap();
    let mut state = state(dir.path());
    state.lang = "en".into();
    open(&mut state, Page::Controls);
    send(
        &mut state,
        ControlsMessage::ProfilePick(MOUSE_SIDE_BUTTONS.into()),
    );
    assert_eq!(state.controls.key["l1"], "Mouse Back");
    assert_eq!(state.controls.key["r2"], "Q");

    send(&mut state, ControlsMessage::ProfileName("Mine".into()));
    send(&mut state, ControlsMessage::ProfileSave);
    assert_eq!(state.controls.profile, "Mine");
    let file = state.paths.data_dir.join(control_profiles::FILENAME);
    assert!(control_profiles::load(&file).unwrap().contains_key("Mine"));

    send(&mut state, ControlsMessage::ProfilePick("Default".into()));
    assert_eq!(state.controls.key["l1"], "Q");

    let bad = dir.path().join("bad.json");
    std::fs::write(&bad, r#"{"format":"spartanport-controls","version":1}"#).unwrap();
    let before = state.controls.key.clone();
    send(&mut state, ControlsMessage::ProfileImported(Some(bad)));
    assert_eq!(state.controls.key, before);
    assert_eq!(state.controls.profile, "Default");
    assert!(state.controls.status.contains("not a valid"));

    send(&mut state, ControlsMessage::ProfilePick("Mine".into()));
    assert_eq!(state.controls.key["l1"], "Mouse Back");
    send(&mut state, ControlsMessage::ProfileDelete);
    assert!(!control_profiles::load(&file).unwrap().contains_key("Mine"));
    assert_eq!(state.controls.profile, "Default");
}

#[test]
fn alternative_keys_stay_with_the_profile_and_the_picture() {
    let dir = tempfile::tempdir().unwrap();
    let mut state = state(dir.path());
    state.lang = "en".into();
    open(&mut state, Page::Controls);
    state.controls.key_alt.insert("cross".into(), "K".into());

    send(&mut state, ControlsMessage::PartPressed("K".into()));
    assert_eq!(state.controls.capturing.as_deref(), Some("cross"));
    assert_eq!(state.controls.capturing_slot, Slot::Alternative);
    send(&mut state, ControlsMessage::CaptureTimeout);

    send(&mut state, ControlsMessage::ProfileName("Mine".into()));
    send(&mut state, ControlsMessage::ProfileSave);
    let file = state.paths.data_dir.join(control_profiles::FILENAME);
    let saved = control_profiles::load(&file).unwrap();
    assert_eq!(saved["Mine"].key_alt["cross"], "K");

    send(&mut state, ControlsMessage::ProfilePick("Default".into()));
    assert!(state.controls.key_alt.is_empty());
    send(&mut state, ControlsMessage::ProfilePick("Mine".into()));
    assert_eq!(state.controls.key_alt["cross"], "K");
}

#[test]
fn a_damaged_profile_file_is_not_replaced() {
    let dir = tempfile::tempdir().unwrap();
    let mut state = state(dir.path());
    let file = state.paths.data_dir.join(control_profiles::FILENAME);
    std::fs::create_dir_all(file.parent().unwrap()).unwrap();
    std::fs::write(&file, "{oops").unwrap();
    open(&mut state, Page::Controls);
    send(&mut state, ControlsMessage::ProfileName("Mine".into()));
    send(&mut state, ControlsMessage::ProfileSave);
    assert_eq!(std::fs::read_to_string(&file).unwrap(), "{oops");
}

#[test]
fn two_game_buttons_on_one_input_are_flagged() {
    let dir = tempfile::tempdir().unwrap();
    let fake = FakeJoysticks::new("Windows", vec![sony()]);
    let mut state = with_pad(dir.path(), &fake);
    send(
        &mut state,
        ControlsMessage::PadPick("cross".into(), "b".into()),
    );
    assert_eq!(
        spartanport_launcher::core::controls::conflicts(state.controls.pad.values()),
        vec!["b"]
    );
}

#[test]
fn the_view_draws_every_model_and_the_keyboard() {
    let dir = tempfile::tempdir().unwrap();
    let fake = FakeJoysticks::new("Windows", vec![sony()]);
    let mut state = with_pad(dir.path(), &fake);
    for model in PadModel::ALL {
        send(&mut state, ControlsMessage::PadModel(model));
        let _ = controls::view(&state);
    }
}
