//! The top of the Controls page: the live picture of the controller or keyboard, what the pad is
//! doing right now, and the profiles.
//!
//! The page owns SDL3 (`State::live_pad`) while it shows the gamepad. It lets go when the player
//! leaves the page, picks the keyboard, opens the mapper (which owns SDL3 then) or starts the game.
//! SDL3 is not `Send`, so it stays on the window's thread.

use std::path::PathBuf;
use std::time::Duration;

use iced::time::Instant;
use iced::widget::{
    button, column, container, pick_list, row as layout, slider as track, text, text_input,
};
use iced::{Center, Element, Fill, Length, Padding, Task};
use indexmap::IndexMap;
use serde_json::Value;

use crate::core::control_profiles::{
    self, built_in, clean_name, Profile, ProfileError, BUILT_IN, DEFAULT_PROFILE,
};
use crate::core::controller_mappings::{self, load_mapping};
use crate::core::controls::{
    bind_pad, conflicts, control_changes, default_input, table_rows, Kind, Slot,
};
use crate::core::i18n::{tr, tr_fmt};
use crate::core::joysticks::{Device, JoyError, JoystickThread, Joysticks, Sdl3};
use crate::core::pad_model::{
    detect_model, sdl_button, stick_percent, suggest_deadzone, MAX_DEADZONE,
};
use crate::core::paths::Paths;
use crate::ui::app::{apply_set, Message, State};
use crate::ui::pages::controls::{self, ini_number, ControlsMessage, Device as Shown};
use crate::ui::pages::Page;
use crate::ui::theme::{self, palette};
use crate::ui::widgets::pad_diagram::{
    game_icon, input_icon, pad_diagram, pad_icon, parts, picture_width, Live, Picture,
    Shown as ShownInput,
};
use crate::ui::widgets::SELECT_TEXT;

/// How often the page reads the pad.
pub const POLL: Duration = Duration::from_millis(20);
/// A trigger counts as pressed past this, as the game's own L2 and R2 do (about 30 of 255).
const TRIGGER_DOWN: i16 = 3855;
/// Polls between looks for a controller while none is connected: 300 ms.
const SCAN_POLLS: u32 = 15;
/// Polls a drift measure lasts: two seconds.
const DRIFT_POLLS: u32 = 100;
const RUMBLE_MS: u32 = 500;
/// The columns on each side of the controller picture.
const SIDE_WIDTH: f32 = 190.0;
const SIDE_GAP: f32 = 12.0;
/// A pad remap gives up after this long, as a key box does.
const CAPTURE_SECONDS: u64 = 5;

const NO_INPUT: &str = "No game button uses this input.";
const NEED_PAD: &str = "Connect a controller to remap by pressing a button.";
const UNMAPPED: &str = "SDL does not know this controller yet. Use Set up a controller first.";
const NO_PAD: &str = "No controller found. Connect one and it shows up here.";
const IDLE_PAD: &str = "Press any button on {}";
const IDLE_KEYS: &str = "Press a key or click a key to change it";
const PRESSED: &str = "Pressed: {}";
const CAPTURE: &str = "Press the new button for {} ({})";
const CONFLICT: &str = "{} presses more than one game button.";

/// What the sticks did while the player left them alone.
#[derive(Debug, Clone, Copy, Default)]
pub struct Drift {
    polls: u32,
    most: [f32; 2],
}

/// The pad the page reads, and what it read last.
pub struct LivePad {
    joysticks: Box<dyn Joysticks>,
    pub device: Option<Device>,
    /// SDL names of what is held.
    pub pressed: Vec<String>,
    pub axes: [i16; 6],
    /// The last input pressed, for the caption.
    pub last: Option<String>,
    /// SDL has no mapping for this device, so no named buttons to show.
    pub unmapped: bool,
    pub drift: Option<Drift>,
    /// What the last measure suggests: left and right.
    pub suggestion: Option<[u32; 2]>,
    /// The motor under test, left or right, and when it stops.
    motor: Option<(bool, Instant)>,
    scan: u32,
}

fn said(error: impl std::fmt::Display) -> String {
    error.to_string()
}

impl LivePad {
    fn new(joysticks: Box<dyn Joysticks>) -> LivePad {
        LivePad {
            joysticks,
            device: None,
            pressed: Vec::new(),
            axes: [0; 6],
            last: None,
            unmapped: false,
            drift: None,
            suggestion: None,
            motor: None,
            scan: 0,
        }
    }

    /// Takes the first controller SDL lists; the mapper chooses among several.
    fn connect(&mut self, mappings: &std::path::Path) -> Result<(), JoyError> {
        let Some(device) = self.joysticks.devices()?.into_iter().next() else {
            return Ok(());
        };
        self.joysticks.select(&device)?;
        let platform = self.joysticks.platform().to_string();
        let line = load_mapping(mappings, &device.guid, &platform)
            .ok()
            .flatten()
            .or_else(|| self.joysticks.current_mapping());
        self.unmapped = match line {
            Some(line) => self.joysticks.begin_preview(&line).is_err(),
            None => true,
        };
        self.device = Some(device);
        Ok(())
    }

    fn disconnect(&mut self) {
        self.joysticks.close_device();
        self.device = None;
        self.pressed.clear();
        self.axes = [0; 6];
        self.drift = None;
        self.motor = None;
    }

    /// One read. The inputs newly pressed since the last one.
    fn read(&mut self) -> Vec<String> {
        if self.joysticks.state().is_err() {
            self.disconnect();
            return Vec::new();
        }
        let Some((buttons, axes)) = self.joysticks.preview_state() else {
            return Vec::new();
        };
        self.axes = axes;
        let mut pressed: Vec<String> = buttons
            .into_iter()
            .filter_map(sdl_button)
            .map(str::to_string)
            .collect();
        if axes[4] > TRIGGER_DOWN {
            pressed.push("lefttrigger".to_string());
        }
        if axes[5] > TRIGGER_DOWN {
            pressed.push("righttrigger".to_string());
        }
        let fresh: Vec<String> = pressed
            .iter()
            .filter(|name| !self.pressed.contains(name))
            .cloned()
            .collect();
        if let Some(name) = fresh.first() {
            self.last = Some(name.clone());
        }
        self.pressed = pressed;
        fresh
    }

    fn measure(&mut self) {
        let Some(mut drift) = self.drift else {
            return;
        };
        let sticks = [
            stick_percent(self.axes[0], self.axes[1]),
            stick_percent(self.axes[2], self.axes[3]),
        ];
        drift.polls += 1;
        for (most, now) in drift.most.iter_mut().zip(sticks) {
            *most = most.max(now);
        }
        self.drift = (drift.polls < DRIFT_POLLS).then_some(drift);
        if self.drift.is_none() {
            self.suggestion = Some(drift.most.map(suggest_deadzone));
        }
    }
}

impl State {
    /// Starts reading `joysticks` (the page passes SDL3, the tests a scripted one).
    #[doc(hidden)]
    pub fn open_live_pad_with(&mut self, joysticks: Box<dyn Joysticks>) {
        let mut live = LivePad::new(joysticks);
        if let Err(error) = live.connect(&mappings_file(self)) {
            self.controls.live_error = Some(said(error));
        }
        self.live_pad = Some(live);
        follow_model(self);
    }
}

fn mappings_file(state: &State) -> PathBuf {
    state.paths.data_dir.join(controller_mappings::FILENAME)
}

fn profiles_file(state: &State) -> PathBuf {
    state.paths.data_dir.join(control_profiles::FILENAME)
}

/// What the page opens outside the tests.
pub fn open_sdl(paths: &Paths) -> Result<Box<dyn Joysticks>, JoyError> {
    Ok(Box::new(Sdl3::open(
        &paths.port_dir,
        &paths.data_dir,
        None,
    )?))
}

/// With the gamepad shown and no game running: the pad, unless it is already open.
pub fn open_live(state: &mut State) {
    if state.live_pad.is_some()
        || state.controls.device != Shown::Gamepad
        || state.game.is_live()
        || state.mapper.is_some()
    {
        return;
    }
    state.controls.live_error = None;
    let (opener, paths) = (state.joystick_opener, state.paths.clone());
    state.open_live_pad_with(Box::new(JoystickThread::spawn(move || opener(&paths))));
}

/// Lets go of the pad; SDL3 stops as its adapter drops.
pub fn close_live(state: &mut State) {
    if let Some(mut live) = state.live_pad.take() {
        live.disconnect();
    }
    stop_capture(state);
}

/// The automatic model follows the connected controller; a generic pad keeps the last one.
fn follow_model(state: &mut State) {
    if !state.store.flag("pad_style_auto") {
        return;
    }
    let found = state
        .live_pad
        .as_ref()
        .and_then(|live| live.device.as_ref())
        .and_then(|device| detect_model(&device.guid, &device.name));
    if let Some(model) = found {
        state.controls.pad_model = model;
        state.controls.pad_style = model.style().to_string();
    }
}

pub fn stop_capture(state: &mut State) {
    state.controls.pad_capturing = None;
    state.controls.pad_capture_started = None;
}

/// Whether a pad remap is waiting.
pub fn capturing(state: &State) -> bool {
    state.controls.pad_capturing.is_some()
}

/// A pad remap gives up `CAPTURE_SECONDS` after it started, and the pad comes back once the game
/// that took it has closed.
pub fn tick(state: &mut State, at: Instant) {
    if state.page == Page::Controls && state.controls.live_error.is_none() {
        open_live(state);
    }
    let expired = state.controls.pad_capture_started.is_some_and(|started| {
        at.saturating_duration_since(started) >= Duration::from_secs(CAPTURE_SECONDS)
    });
    if expired {
        stop_capture(state);
    }
}

/// One read of the pad: the live view, a waiting remap, a drift measure, and a controller that came
/// or went.
pub fn poll(state: &mut State) {
    if state.game.is_live() {
        close_live(state);
        return;
    }
    let mappings = mappings_file(state);
    let Some(mut live) = state.live_pad.take() else {
        return;
    };
    if let Some(error) = live.joysticks.failure() {
        state.controls.live_error = Some(error);
        return;
    }
    if live.device.is_some() {
        let fresh = live.read();
        live.measure();
        if let (Some(button), Some(name)) = (state.controls.pad_capturing.clone(), fresh.first()) {
            bind_pad(&mut state.controls.pad, &button, name);
            stop_capture(state);
        }
        if live.device.is_none() {
            stop_capture(state);
        }
    } else {
        live.scan += 1;
        if live.scan >= SCAN_POLLS {
            live.scan = 0;
            if let Err(error) = live.connect(&mappings) {
                state.controls.live_error = Some(said(error));
            }
        }
    }
    state.live_pad = Some(live);
    follow_model(state);
}

/// A click on the picture: the game button on that input waits for a new one.
pub fn part_pressed(state: &mut State, part: &str) -> Task<Message> {
    let lang = state.lang.clone();
    state.controls.status.clear();
    let matches = |value: &String| value.trim().eq_ignore_ascii_case(part);
    match state.controls.device {
        Shown::Keyboard => {
            let enabled = controls::key_rows(state, &lang);
            let input = enabled
                .into_iter()
                .filter(|row| row.enabled)
                .find_map(|row| {
                    let view = &state.controls;
                    let slot = if view.key.get(row.button).is_some_and(matches) {
                        Slot::Main
                    } else if view.key_alt.get(row.button).is_some_and(matches) {
                        Slot::Alternative
                    } else {
                        return None;
                    };
                    Some((row.button.to_string(), slot))
                });
            match input {
                Some((button, slot)) => {
                    controls::update(state, ControlsMessage::Capture(button, slot))
                }
                None => {
                    state.controls.status = tr(&lang, NO_INPUT);
                    Task::none()
                }
            }
        }
        Shown::Gamepad => {
            let connected = state
                .live_pad
                .as_ref()
                .is_some_and(|live| live.device.is_some() && !live.unmapped);
            let button = state
                .controls
                .pad
                .iter()
                .find(|(_, value)| matches(value))
                .map(|(button, _)| button.clone());
            match (connected, button) {
                (false, _) => state.controls.status = tr(&lang, NEED_PAD),
                (true, None) => state.controls.status = tr(&lang, NO_INPUT),
                (true, Some(button)) => {
                    let now = Instant::now();
                    state.now = now;
                    state.controls.pad_capturing = Some(button);
                    state.controls.pad_capture_started = Some(now);
                }
            }
            Task::none()
        }
    }
}

pub fn measure_drift(state: &mut State) {
    if let Some(live) = state.live_pad.as_mut() {
        live.suggestion = None;
        live.drift = Some(Drift::default());
    }
}

pub fn apply_drift(state: &mut State) -> Task<Message> {
    let Some([left, right]) = state
        .live_pad
        .as_mut()
        .and_then(|live| live.suggestion.take())
    else {
        return Task::none();
    };
    Task::batch([
        apply_set(state, "deadzone_left", Value::from(left)),
        apply_set(state, "deadzone_right", Value::from(right)),
    ])
}

pub fn rumble(state: &mut State, left: bool) {
    let strength = (ini_number(state, "vibration") / 100.0 * 65535.0) as u16;
    let (low, high) = if left { (strength, 0) } else { (0, strength) };
    let now = state.now;
    if let Some(live) = state.live_pad.as_mut().filter(|live| live.device.is_some()) {
        if live.joysticks.rumble(low, high, RUMBLE_MS) {
            live.motor = Some((left, now + Duration::from_millis(u64::from(RUMBLE_MS))));
        }
    }
}

/// The left and right motors under test now.
pub fn motors(state: &State) -> [bool; 2] {
    match state.live_pad.as_ref().and_then(|live| live.motor) {
        Some((left, until)) if state.now < until => [left, !left],
        _ => [false, false],
    }
}

/// The page's saved profiles, read each time it opens.
pub fn load_profiles(state: &mut State) {
    match control_profiles::load(&profiles_file(state)) {
        Ok(profiles) => {
            state.controls.profiles = profiles;
            state.controls.profiles_damaged = false;
        }
        Err(error) => {
            state.controls.profiles_damaged = true;
            state.controls.status = tr(&state.lang, &said(error));
        }
    }
}

fn current(state: &State) -> Profile {
    let number = |key: &str| ini_number(state, key) as u32;
    Profile {
        key: control_changes(&state.controls.key, Kind::Key),
        key_alt: state.controls.key_alt.clone(),
        pad: control_changes(&state.controls.pad, Kind::Pad),
        deadzone_left: number("deadzone_left"),
        deadzone_right: number("deadzone_right"),
    }
}

fn store_profiles(state: &mut State) -> Result<(), ProfileError> {
    // A damaged file was never read, so saving now would lose what it holds.
    if state.controls.profiles_damaged {
        return Err(ProfileError::Read);
    }
    control_profiles::save(&profiles_file(state), &state.controls.profiles)
}

fn fail(state: &mut State, error: &ProfileError) {
    state.controls.status = tr(&state.lang, &said(error));
}

pub fn apply_profile(state: &mut State, profile: &Profile) -> Task<Message> {
    state.controls.key_alt = profile.key_alt.clone();
    for kind in [Kind::Key, Kind::Pad] {
        let (values, chosen) = match kind {
            Kind::Key => (&mut state.controls.key, &profile.key),
            Kind::Pad => (&mut state.controls.pad, &profile.pad),
        };
        for row in table_rows(kind) {
            let value = chosen
                .get(row.button)
                .map(String::as_str)
                .or_else(|| default_input(row, kind))
                .unwrap_or_default();
            values.insert(row.button.to_string(), value.to_string());
        }
    }
    let (left, right) = (profile.deadzone_left, profile.deadzone_right);
    Task::batch([
        apply_set(state, "deadzone_left", Value::from(left)),
        apply_set(state, "deadzone_right", Value::from(right)),
    ])
}

pub fn pick_profile(state: &mut State, name: &str) -> Task<Message> {
    let profile = built_in(name).or_else(|| state.controls.profiles.get(name).cloned());
    let Some(profile) = profile else {
        return Task::none();
    };
    state.controls.profile = name.to_string();
    state.controls.status.clear();
    apply_profile(state, &profile)
}

pub fn save_profile(state: &mut State) {
    match clean_name(&state.controls.profile_name) {
        Ok(name) => {
            let profile = current(state);
            state.controls.profiles.insert(name.clone(), profile);
            match store_profiles(state) {
                Ok(()) => {
                    state.controls.status = tr_fmt(
                        &state.lang,
                        "Saved: {}",
                        std::slice::from_ref(&name.as_str()),
                    );
                    state.controls.profile = name;
                    state.controls.profile_name.clear();
                    state.controls.profile_naming = false;
                }
                Err(error) => fail(state, &error),
            }
        }
        Err(error) => fail(state, &error),
    }
}

pub fn delete_profile(state: &mut State) {
    let name = state.controls.profile.clone();
    if state.controls.profiles.shift_remove(&name).is_none() {
        return;
    }
    state.controls.profile = DEFAULT_PROFILE.to_string();
    if let Err(error) = store_profiles(state) {
        fail(state, &error);
    }
}

pub fn import_dialog() -> Task<Message> {
    let dialog = rfd::AsyncFileDialog::new()
        .add_filter("JSON", &["json"])
        .pick_file();
    Task::perform(dialog, |handle| {
        Message::Controls(ControlsMessage::ProfileImported(
            handle.map(|h| h.path().to_path_buf()),
        ))
    })
}

pub fn export_dialog(state: &State) -> Task<Message> {
    let dialog = rfd::AsyncFileDialog::new()
        .add_filter("JSON", &["json"])
        .set_file_name(format!(
            "{}.json",
            shown_profile(state, &state.controls.profile)
        ))
        .save_file();
    Task::perform(dialog, |handle| {
        Message::Controls(ControlsMessage::ProfileExported(
            handle.map(|h| h.path().to_path_buf()),
        ))
    })
}

pub fn imported(state: &mut State, path: Option<PathBuf>) -> Task<Message> {
    let Some(path) = path else {
        return Task::none();
    };
    match control_profiles::import(&path) {
        Ok((name, profile)) => {
            state
                .controls
                .profiles
                .insert(name.clone(), profile.clone());
            if let Err(error) = store_profiles(state) {
                fail(state, &error);
                return Task::none();
            }
            state.controls.profile = name;
            state.controls.status.clear();
            apply_profile(state, &profile)
        }
        // A bad file changes nothing: the choices on screen stay as they were.
        Err(error) => {
            fail(state, &error);
            Task::none()
        }
    }
}

pub fn exported(state: &mut State, path: Option<PathBuf>) {
    let Some(path) = path else {
        return;
    };
    let name = shown_profile(state, &state.controls.profile);
    if let Err(error) = control_profiles::export(&path, &name, &current(state)) {
        fail(state, &error);
    }
}

/// A profile's name as the box shows it: the built-ins are translated.
fn shown_profile(state: &State, name: &str) -> String {
    if BUILT_IN.contains(&name) {
        tr(&state.lang, name)
    } else {
        name.to_string()
    }
}

/// `input` as the picture names it: the tables keep what the player typed, in any case.
fn part_id(picture: Picture, input: &str) -> Option<&'static str> {
    parts(picture)
        .into_iter()
        .find(|part| part.id.eq_ignore_ascii_case(input.trim()))
        .map(|part| part.id)
}

fn pad_label(state: &State, lang: &str, name: &str) -> String {
    controls::pad_labels(&state.controls.pad_style, lang)
        .into_iter()
        .find(|(known, _)| known == name)
        .map_or_else(|| name.to_string(), |(_, label)| label)
}

/// The inputs of the table on show that press more than one game button.
fn shown_conflicts(view: &controls::ControlsView) -> Vec<String> {
    match view.device {
        Shown::Keyboard => conflicts(view.key.values().chain(view.key_alt.values())),
        Shown::Gamepad => conflicts(view.pad.values()),
    }
}

/// The picture's state: the tables say what each input does, the pad what it is doing now.
fn live_view(state: &State, lang: &str, picture: Picture) -> Live {
    let view = &state.controls;
    let (kind, values) = match picture {
        Picture::Keyboard => (Kind::Key, &view.key),
        Picture::Pad(_) => (Kind::Pad, &view.pad),
    };
    let mut bound: Vec<(String, String)> = Vec::new();
    let mut changed = Vec::new();
    for row in table_rows(kind) {
        let alternative = match picture {
            Picture::Keyboard => view.key_alt.get(row.button),
            Picture::Pad(_) => None,
        };
        for (input, extra) in [(values.get(row.button), false), (alternative, true)] {
            let Some(input) = input.filter(|value| !value.is_empty()) else {
                continue;
            };
            let Some(id) = part_id(picture, input) else {
                continue;
            };
            let name = tr(lang, controls::button_name(row.button));
            match bound.iter_mut().find(|(known, _)| known == id) {
                Some((_, words)) => {
                    words.push_str(" + ");
                    words.push_str(&name);
                }
                None => {
                    let shown = match picture {
                        Picture::Keyboard => input.clone(),
                        Picture::Pad(_) => pad_label(state, lang, id),
                    };
                    bound.push((id.to_string(), format!("{shown}: {name}")));
                }
            }
            let default = default_input(row, kind).unwrap_or_default();
            if extra || !input.eq_ignore_ascii_case(default) {
                changed.push(id.to_string());
            }
        }
    }
    let conflicting = shown_conflicts(view)
        .into_iter()
        .filter_map(|input| part_id(picture, &input).map(str::to_string))
        .collect();
    let mut live = Live {
        bound,
        changed,
        conflicts: conflicting,
        clickable: true,
        shown: pad_shown(state, lang, picture),
        motors: motors(state),
        ..Live::default()
    };
    match picture {
        Picture::Keyboard => {
            live.pressed = view
                .held
                .iter()
                .filter_map(|input| part_id(picture, input).map(str::to_string))
                .collect();
            live.selected = view
                .capturing
                .as_ref()
                .and_then(|button| match view.capturing_slot {
                    Slot::Main => view.key.get(button),
                    Slot::Alternative => view.key_alt.get(button),
                })
                .and_then(|input| part_id(picture, input).map(str::to_string));
            live.caption = match view.held.last() {
                Some(input) => tr_fmt(lang, PRESSED, &[input]),
                None => tr(lang, IDLE_KEYS),
            };
            if view.capturing.is_none() {
                live.shown = view.held.last().map(|input| {
                    let same = |table: &IndexMap<String, String>, button: &str| {
                        table
                            .get(button)
                            .is_some_and(|key| key.trim().eq_ignore_ascii_case(input))
                    };
                    let game = table_rows(Kind::Key)
                        .into_iter()
                        .find(|row| same(&view.key, row.button) || same(&view.key_alt, row.button));
                    let icon = input_icon(input).map(|icon| icon.plain);
                    pressed_line(lang, icon, game.map(|row| row.button))
                });
            }
        }
        Picture::Pad(_) => {
            live.selected = view
                .pad_capturing
                .as_ref()
                .and_then(|button| view.pad.get(button))
                .and_then(|input| part_id(picture, input).map(str::to_string));
            live.deadzones = [
                ini_number(state, "deadzone_left") / 100.0,
                ini_number(state, "deadzone_right") / 100.0,
            ];
            live.caption = pad_caption(state, lang);
            if let Some(pad) = &state.live_pad {
                live.pressed = pad.pressed.clone();
                let a = pad.axes.map(|axis| f32::from(axis) / 32767.0);
                live.axes = a;
            }
        }
    }
    live
}

/// The caption's icons: what was pressed, then the game's own button.
fn pressed_line(lang: &str, input: Option<&'static [u8]>, game: Option<&str>) -> ShownInput {
    ShownInput {
        words: tr_fmt(lang, PRESSED, &[""]).trim_end().to_string(),
        input,
        game: game.and_then(game_icon),
        game_name: game
            .map(|button| tr(lang, controls::button_name_of(button)))
            .unwrap_or_default(),
    }
}

/// The last pad input with the game button it presses, while nothing waits for a press.
fn pad_shown(state: &State, lang: &str, picture: Picture) -> Option<ShownInput> {
    let view = &state.controls;
    let Picture::Pad(model) = picture else {
        return None;
    };
    let live = state
        .live_pad
        .as_ref()
        .filter(|live| live.device.is_some())?;
    let name = live
        .last
        .as_ref()
        .filter(|_| view.pad_capturing.is_none())?;
    let game = view
        .pad
        .iter()
        .find(|(_, value)| value.eq_ignore_ascii_case(name))
        .map(|(button, _)| button.as_str());
    Some(pressed_line(lang, pad_icon(model, name), game))
}

fn pad_caption(state: &State, lang: &str) -> String {
    let view = &state.controls;
    if let Some(button) = &view.pad_capturing {
        let waited = view.pad_capture_started.map_or(0, |started| {
            state.now.saturating_duration_since(started).as_secs()
        });
        let left = CAPTURE_SECONDS.saturating_sub(waited).max(1).to_string();
        let name = tr(lang, controls::button_name_of(button));
        return tr_fmt(lang, CAPTURE, &[&name, &left]);
    }
    let Some(live) = &state.live_pad else {
        return tr(lang, NO_PAD);
    };
    let Some(device) = &live.device else {
        return tr(lang, NO_PAD);
    };
    match &live.last {
        Some(name) => {
            let shown = pad_label(state, lang, name);
            let game = view
                .pad
                .iter()
                .find(|(_, value)| value.eq_ignore_ascii_case(name))
                .map(|(button, _)| tr(lang, controls::button_name_of(button)));
            let words = match game {
                Some(game) if game != shown => format!("{shown}: {game}"),
                _ => shown,
            };
            tr_fmt(lang, PRESSED, &[&words])
        }
        None => tr_fmt(lang, IDLE_PAD, &[&device.name]),
    }
}

/// The height of the picture: a good share of the window, as it is the page's main subject.
fn picture_height(state: &State, picture: Picture) -> Length {
    let share = match picture {
        Picture::Pad(_) => 0.46,
        Picture::Keyboard => 0.34,
    };
    Length::Fixed((state.window_size.height * share).clamp(240.0, 460.0))
}

fn flat<'a>(label: String, message: Option<Message>) -> Element<'a, Message> {
    button(text(label))
        .style(theme::button_flat)
        .on_press_maybe(message)
        .into()
}

fn notice<'a>(words: String, alert: bool) -> Element<'a, Message> {
    let color = if alert {
        palette::RED_HI
    } else {
        palette::MUTED
    };
    container(text(words).size(13).color(color))
        .padding(Padding::ZERO.top(4))
        .into()
}

/// The live picture, with whatever needs saying under it.
pub fn picture_panel<'a>(state: &State, lang: &str) -> Element<'a, Message> {
    let view = &state.controls;
    let picture = match view.device {
        Shown::Keyboard => Picture::Keyboard,
        Shown::Gamepad => Picture::Pad(view.pad_model),
    };
    let height = picture_height(state, picture);
    let Length::Fixed(height) = height else {
        return column![].into();
    };
    let diagram = pad_diagram(picture, live_view(state, lang, picture), height);
    let top: Element<'a, Message> = match picture {
        // The sides stay beside the controller, however wide the page is.
        Picture::Pad(_) => {
            let wide = picture_width(picture, height) + 2.0 * (SIDE_WIDTH + SIDE_GAP);
            container(
                container(
                    layout![
                        stick_side(state, lang, true),
                        diagram,
                        stick_side(state, lang, false)
                    ]
                    .spacing(SIDE_GAP)
                    .align_y(Center),
                )
                .max_width(wide),
            )
            .center_x(Fill)
            .into()
        }
        Picture::Keyboard => diagram,
    };
    let mut panel = column![top].spacing(2);
    for input in shown_conflicts(view) {
        let shown = match picture {
            Picture::Keyboard => input,
            Picture::Pad(_) => pad_label(state, lang, &input),
        };
        panel = panel.push(notice(tr_fmt(lang, CONFLICT, &[&shown]), true));
    }
    if view.device == Shown::Gamepad {
        if let Some(error) = &view.live_error {
            panel = panel.push(notice(tr(lang, error), true));
        }
        if state.live_pad.as_ref().is_some_and(|live| live.unmapped) {
            panel = panel.push(notice(tr(lang, UNMAPPED), true));
        }
    }
    if !view.status.is_empty() {
        panel = panel.push(notice(view.status.clone(), false));
    }
    panel.into()
}

fn pad_ready(state: &State) -> bool {
    state
        .live_pad
        .as_ref()
        .is_some_and(|live| live.device.is_some() && !live.unmapped)
}

/// One side of the controller: its motor test and the dead zone of the stick on that side.
fn stick_side<'a>(state: &State, lang: &str, left: bool) -> Element<'a, Message> {
    let (motor, name, key) = if left {
        ("Test left motor", "Left stick dead zone", "deadzone_left")
    } else {
        (
            "Test right motor",
            "Right stick dead zone",
            "deadzone_right",
        )
    };
    let value = ini_number(state, key);
    let zone = track(0.0..=MAX_DEADZONE as f32, value, move |value| {
        Message::Set(key.to_string(), Value::from(value as i64))
    })
    .step(1.0_f32);
    column![
        flat(
            tr(lang, motor),
            pad_ready(state).then_some(Message::Controls(ControlsMessage::Rumble(left)))
        ),
        text(tr(lang, name)).size(13),
        layout![zone, text(format!("{} %", value as i64)).size(13)]
            .spacing(8)
            .align_y(Center),
    ]
    .spacing(8)
    .width(SIDE_WIDTH)
    .into()
}

/// The drift measure, under the mapping.
pub fn pad_tools<'a>(state: &State, lang: &str) -> Element<'a, Message> {
    let live = state.live_pad.as_ref();
    let measuring = live.is_some_and(|live| live.drift.is_some());
    let measure_label = if measuring {
        "Leave the sticks alone..."
    } else {
        "Measure stick drift"
    };
    let mut tools = layout![flat(
        tr(lang, measure_label),
        (pad_ready(state) && !measuring)
            .then_some(Message::Controls(ControlsMessage::MeasureDrift))
    )]
    .spacing(8)
    .align_y(Center);
    if let Some([left, right]) = live.and_then(|live| live.suggestion) {
        let words = tr_fmt(
            lang,
            "Suggested dead zone: left {}%, right {}%",
            &[&left.to_string(), &right.to_string()],
        );
        tools = tools.push(text(words).size(13));
        tools = tools.push(flat(
            tr(lang, "Apply"),
            Some(Message::Controls(ControlsMessage::ApplyDrift)),
        ));
    }
    container(tools).padding(Padding::ZERO.top(8)).into()
}

/// The profile row.
pub fn profile_section<'a>(state: &State, lang: &str) -> Element<'a, Message> {
    let view = &state.controls;
    let names: Vec<String> = BUILT_IN
        .iter()
        .map(|name| (*name).to_string())
        .chain(view.profiles.keys().cloned())
        .collect();
    let labels: IndexMap<String, String> = names
        .iter()
        .map(|name| (shown_profile(state, name), name.clone()))
        .collect();
    let shown: Vec<String> = labels.keys().cloned().collect();
    let chosen = shown_profile(state, &view.profile);
    let picker = pick_list(shown, Some(chosen), move |label: String| {
        let name = labels.get(&label).cloned().unwrap_or(label);
        Message::Controls(ControlsMessage::ProfilePick(name))
    })
    .text_size(SELECT_TEXT)
    .style(theme::pick_list_dark);
    let naming = view.profile_naming;
    let can_save = !naming || !view.profile_name.trim().is_empty();
    let can_delete = view.profiles.contains_key(&view.profile);
    let save = if naming {
        ControlsMessage::ProfileSave
    } else {
        ControlsMessage::ProfileNaming
    };
    let mut actions = layout![
        text(tr(lang, "Control Profiles"))
            .size(13)
            .color(palette::MUTED),
        picker
    ]
    .spacing(8)
    .align_y(Center);
    if naming {
        actions = actions.push(
            text_input(&tr(lang, "New profile name"), &view.profile_name)
                .on_input(|name| Message::Controls(ControlsMessage::ProfileName(name)))
                .on_submit(Message::Controls(ControlsMessage::ProfileSave))
                .width(180)
                .size(SELECT_TEXT),
        );
    }
    let actions = actions
        .push(flat(
            tr(lang, "Save"),
            can_save.then_some(Message::Controls(save)),
        ))
        .push(flat(
            tr(lang, "Delete"),
            can_delete.then_some(Message::Controls(ControlsMessage::ProfileDelete)),
        ))
        .push(flat(
            tr(lang, "Import"),
            Some(Message::Controls(ControlsMessage::ProfileImport)),
        ))
        .push(flat(
            tr(lang, "Export"),
            Some(Message::Controls(ControlsMessage::ProfileExport)),
        ));
    container(actions).padding(Padding::ZERO.top(18)).into()
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::core::pad_model::PadModel;

    #[test]
    fn the_picture_knows_inputs_in_any_case() {
        assert_eq!(
            part_id(Picture::Keyboard, " left shift "),
            Some("Left Shift")
        );
        assert_eq!(part_id(Picture::Keyboard, "f5"), Some("F5"));
        assert_eq!(part_id(Picture::Keyboard, "Keypad 7"), None);
        assert_eq!(part_id(Picture::Pad(PadModel::Xbox), "A"), Some("a"));
        assert_eq!(part_id(Picture::Pad(PadModel::Xbox), "touchpad"), None);
    }
}
