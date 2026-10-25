//! The "Set up a controller" page (Python `MapperWindow` in `launcher/controller_mapper.py`): the
//! controller and its details, the table of [`STEPS`] with Configure and Clear, the prompt, the
//! status and the preview, then Cancel capture and Close. Each change is checked by SDL's preview
//! and saved at once, in the data folder's `gamecontrollerdb.txt` the game reads.
//!
//! The Controls page's button opens SDL3 and the page owns it (`State::mapper`) until the page
//! closes: Close, Esc, another tab or closing the window. A 20 ms clock reads the pad only while the
//! page is open. SDL3 is not `Send`, so it stays on the window's thread.

use std::fmt::Display;
use std::path::PathBuf;
use std::time::Duration;

use iced::keyboard::key::Named;
use iced::keyboard::Key;
use iced::widget::{
    button, column, container, pick_list, row as layout, scrollable, space, text, Id,
};
use iced::{Center, Element, Fill, Left, Padding, Subscription, Task};
use indexmap::IndexMap;

use crate::core::controller_mappings::{
    edit_mapping, load_mapping, mapping_bindings, save_mapping, Capture, FILENAME, STEPS,
    STICK_AXES,
};
use crate::core::i18n::{tr, tr_fmt};
use crate::core::joysticks::{Device, JoyError, Joysticks, Sdl3};
use crate::ui::app::{show_page, Message, State};
use crate::ui::pages::Page;
use crate::ui::play::{refuse_while_live, LiveAction};
use crate::ui::theme::{self, mirror_x, palette, CINZEL_BOLD};
use crate::ui::widgets::{Modal, SELECT_TEXT};

/// How often the open page reads the pad (the Python's `after(20, poll)`).
pub const POLL: Duration = Duration::from_millis(20);
/// The page's title, which also titles its dialogs.
const TITLE: &str = "Set up a controller";
const ANY_ORDER: &str = "Configure any control, in any order.";
const SAVE_FAILED: &str = "Could not save mapping";
const CONFLICT: &str =
    "That input is assigned to another control. Clear its row first or choose another input.";
const STICKS: &str = "Left stick: {} / {}   Right stick: {} / {}\nL2: {}   R2: {}";
/// The table scrolls in its own area (the Python's canvas), which takes the window's height less
/// this (the tab bar, the drawer's header and the page's other lines), so the prompt, the status
/// and the buttons under it stay in sight: 340 px in the 800 px window, more in a taller one.
const BESIDE_TABLE: f32 = 460.0;
/// The table's least height, in a window smaller than the launcher's own size.
const MIN_TABLE: f32 = 200.0;
/// Width of the Configure buttons, enough for "Listening…", and of the Clear buttons; the
/// column titles line up with them.
const CONFIGURE_WIDTH: f32 = 130.0;
const CLEAR_WIDTH: f32 = 90.0;

/// The page while it is open: the joysticks it reads and what the Python's window showed.
pub struct MapperState {
    joysticks: Box<dyn Joysticks>,
    /// `gamecontrollerdb.txt` in the data folder.
    pub path: PathBuf,
    /// What the last Refresh listed, and the one chosen in the box.
    pub devices: Vec<Device>,
    pub choice: Option<usize>,
    /// The device the joysticks have open (the Python's `backend.device`); none after a failure.
    pub device: Option<Device>,
    /// The capture of the row being configured, and that row's SDL name.
    pub capture: Option<Capture>,
    pub editing: Option<String>,
    /// The inputs of `profile`, by SDL name.
    pub bindings: IndexMap<String, String>,
    /// The device's line: the saved one, else SDL's own.
    pub profile: Option<String>,
    /// The device's inputs, the line under the box.
    pub details: String,
    pub prompt: String,
    pub status: String,
    /// What the preview reads: the pressed buttons, the sticks and the triggers.
    pub preview_text: String,
}

/// Mapper page events.
#[derive(Debug, Clone)]
pub enum MapperMessage {
    /// The Controls page's "Set up a controller".
    Open,
    /// The device box: the index of the chosen device.
    Choose(usize),
    Refresh,
    /// A row's Configure: listen for an input for this SDL name.
    Configure(String),
    /// A row's Clear: save the line without this SDL name.
    Clear(String),
    CancelCapture,
    Close,
    /// The 20 ms clock.
    Poll,
}

/// One row of the table as the page draws it.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct MapperRow {
    /// The SDL name.
    pub key: &'static str,
    pub label: String,
    /// The bound input, or "Not configured".
    pub assigned: String,
    /// "Configure", or "Listening…" while this row captures.
    pub configure: String,
    pub configure_enabled: bool,
    pub clear_enabled: bool,
}

/// An error as the page shows it.
fn said(error: impl Display) -> String {
    error.to_string()
}

impl MapperState {
    fn new(joysticks: Box<dyn Joysticks>, path: PathBuf) -> MapperState {
        MapperState {
            joysticks,
            path,
            devices: Vec::new(),
            choice: None,
            device: None,
            capture: None,
            editing: None,
            bindings: IndexMap::new(),
            profile: None,
            details: String::new(),
            prompt: String::new(),
            status: String::new(),
            preview_text: String::new(),
        }
    }

    fn close_device(&mut self) {
        self.joysticks.close_device();
        self.device = None;
    }

    /// Python `refresh`: lists the controllers again and chooses the same one, else the first.
    fn refresh(&mut self, lang: &str) {
        let previous = self.device.as_ref().map(|device| device.guid.clone());
        self.close_device();
        self.capture = None;
        self.editing = None;
        self.bindings.clear();
        self.profile = None;
        self.status.clear();
        self.preview_text.clear();
        match self.joysticks.devices() {
            Ok(devices) if devices.is_empty() => {
                self.devices = devices;
                self.choice = None;
                self.details.clear();
                self.prompt = tr(lang, "No controller found. Connect it and choose Refresh.");
            }
            Ok(devices) => {
                let same = devices
                    .iter()
                    .position(|device| Some(&device.guid) == previous.as_ref());
                self.devices = devices;
                self.choice = Some(same.unwrap_or(0));
                self.choose(lang);
            }
            Err(error) => self.fail(lang, &said(error)),
        }
    }

    /// Python `choose`: opens the chosen controller and reads its line.
    fn choose(&mut self, lang: &str) {
        self.capture = None;
        self.editing = None;
        if let Err(error) = self.open_choice(lang) {
            self.fail(lang, &error);
        }
    }

    fn open_choice(&mut self, lang: &str) -> Result<(), String> {
        let Some(device) = self
            .choice
            .and_then(|index| self.devices.get(index))
            .cloned()
        else {
            return Ok(());
        };
        // Choosing closes the previous controller first, so a failure leaves none.
        self.device = None;
        self.joysticks.select(&device).map_err(said)?;
        self.device = Some(device.clone());
        let saved = load_mapping(&self.path, &device.guid, self.joysticks.platform());
        self.profile = saved
            .map_err(said)?
            .or_else(|| self.joysticks.current_mapping());
        self.bindings = mapping_bindings(self.profile.as_deref());
        let counts = [device.buttons, device.axes, device.hats].map(|count| count.to_string());
        self.details = tr_fmt(
            lang,
            "{} buttons, {} axes, {} D-pad hats",
            &[&counts[0], &counts[1], &counts[2]],
        );
        self.prompt = tr(lang, ANY_ORDER);
        self.status = tr(lang, "Saved changes are kept when you close this window.");
        self.preview_text.clear();
        self.update_preview().map_err(said)
    }

    /// Python `update_preview`: SDL previews the line while it binds anything.
    fn update_preview(&mut self) -> Result<(), JoyError> {
        match &self.profile {
            Some(line) if !self.bindings.is_empty() => self.joysticks.begin_preview(line),
            _ => {
                self.joysticks.end_preview();
                Ok(())
            }
        }
    }

    /// Python `begin_edit`: the row `target` listens for an input.
    fn begin_edit(&mut self, lang: &str, target: &str) {
        if self.capture.is_some() || self.device.is_none() {
            return;
        }
        let Some(index) = STEPS.iter().position(|(key, _)| *key == target) else {
            return;
        };
        let started = self
            .joysticks
            .state()
            .map_err(said)
            .and_then(|rest| Capture::new(&rest).map_err(said));
        let mut capture = match started {
            Ok(capture) => capture,
            Err(error) => {
                self.status = tr(lang, &error);
                return;
            }
        };
        capture.index = index;
        // Replacing this input is allowed, but conflicting with another row is not.
        capture.bindings = self
            .bindings
            .iter()
            .filter(|(key, _)| *key != target)
            .map(|(key, value)| (key.clone(), value.clone()))
            .collect();
        self.joysticks.end_preview();
        self.capture = Some(capture);
        self.editing = Some(target.to_string());
        self.preview_text.clear();
        let label = STEPS[index].1;
        self.prompt = if STICK_AXES.contains(&target) {
            tr(lang, label)
        } else {
            tr_fmt(lang, "Press {}", &[&tr(lang, label)])
        };
        self.status = tr(
            lang,
            "Hold the input briefly. It will be saved for this control only.",
        );
    }

    /// Python `commit`: the saved line read again (so every other saved row stays), with `target`
    /// bound to `token` (or unbound), checked by SDL's preview and only then saved.
    fn commit(&mut self, lang: &str, target: &str, token: Option<&str>) -> Result<(), String> {
        let Some(device) = self.device.clone() else {
            return Ok(());
        };
        let platform = self.joysticks.platform().to_string();
        let previous = load_mapping(&self.path, &device.guid, &platform)
            .map_err(said)?
            .or_else(|| self.profile.clone());
        let line = edit_mapping(
            previous.as_deref(),
            &device.guid,
            &device.name,
            &platform,
            target,
            token,
        )
        .map_err(said)?;
        self.joysticks.begin_preview(&line).map_err(said)?;
        if let Err(error) = save_mapping(&self.path, &line) {
            self.joysticks.end_preview();
            return Err(said(error));
        }
        self.bindings = mapping_bindings(Some(&line));
        self.profile = Some(line);
        if self.bindings.is_empty() {
            self.joysticks.end_preview();
        }
        self.status = tr_fmt(lang, "Saved: {}", &[&row_label(lang, target)]);
        Ok(())
    }

    /// Python `clear_binding`; the dialog's text when the change could not be saved.
    fn clear(&mut self, lang: &str, target: &str) -> Option<String> {
        if self.capture.is_some() || self.device.is_none() {
            return None;
        }
        self.commit(lang, target, None).err()
    }

    /// Python `cancel_capture`.
    fn cancel_capture(&mut self, lang: &str) {
        self.capture = None;
        self.editing = None;
        self.prompt = tr(lang, ANY_ORDER);
        self.status = tr(lang, "Capture canceled. Previously saved changes are kept.");
        if let Err(error) = self.update_preview() {
            self.fail(lang, &said(error));
        }
    }

    /// Python `poll`: one read of the pad, for the capture or the preview; the dialog's text when
    /// a captured input could not be saved.
    fn poll(&mut self, lang: &str) -> Option<String> {
        // Nothing to read once the controller was let go.
        self.device.as_ref()?;
        let now = match self.joysticks.state() {
            Ok(now) => now,
            Err(error) => {
                self.fail(lang, &said(error));
                return None;
            }
        };
        if let Some(capture) = self.capture.as_mut() {
            if let Some(token) = capture.feed(&now) {
                let target = self.editing.take().unwrap_or_default();
                self.capture = None;
                let failed = self.commit(lang, &target, Some(&token)).err();
                if failed.is_some() {
                    self.status = tr(
                        lang,
                        "The change was not saved. Configure the control again to retry.",
                    );
                    if let Err(error) = self.update_preview() {
                        self.fail(lang, &said(error));
                        return failed;
                    }
                }
                self.prompt = tr(lang, ANY_ORDER);
                return failed;
            }
            if capture.error {
                self.status = tr(lang, CONFLICT);
            }
        } else if let Some((buttons, axes)) = self.joysticks.preview_state() {
            let pressed: Vec<String> = buttons.iter().map(|name| tr(lang, name)).collect();
            let pressed = if pressed.is_empty() {
                "-".to_string()
            } else {
                pressed.join(", ")
            };
            // As whole numbers, as Python's `round` gives them (so no "-0").
            let percent =
                axes.map(|value| ((f64::from(value) / 32767.0 * 100.0).round() as i64).to_string());
            let percent: Vec<&str> = percent.iter().map(String::as_str).collect();
            self.preview_text = tr_fmt(lang, "Pressed: {}", &[&pressed]) + "\n";
            self.preview_text.push_str(&tr_fmt(lang, STICKS, &percent));
        }
        None
    }

    /// Python `fail`: the controller is let go, and the status says why.
    fn fail(&mut self, lang: &str, error: &str) {
        self.close_device();
        self.capture = None;
        self.editing = None;
        self.status = tr(lang, error);
    }
}

/// A row's name: the sticks by their axis (the Python's `axis_labels`), the others by their prompt.
fn row_label(lang: &str, key: &str) -> String {
    let label = match key {
        "leftx" => "Left stick horizontal",
        "lefty" => "Left stick vertical",
        "rightx" => "Right stick horizontal",
        "righty" => "Right stick vertical",
        _ => STEPS
            .iter()
            .find(|(step, _)| *step == key)
            .map_or(key, |(_, label)| *label),
    };
    tr(lang, label)
}

/// How the table names a bound input (Python `input_name`): `Button 3`, `D-pad 1: D-pad up` or
/// `Axis 2 (-)`, from SDL's `b2`, `h0.1` or `-a1` (`a1~` is reversed, so `-` too).
fn input_name(token: Option<&str>, lang: &str) -> String {
    let Some(token) = token.filter(|token| !token.is_empty()) else {
        return tr(lang, "Not configured");
    };
    let number = |digits: &str| {
        digits
            .parse::<u32>()
            .map_or(0, |index| index + 1)
            .to_string()
    };
    if let Some(button) = token.strip_prefix('b') {
        return tr_fmt(lang, "Button {}", &[&number(button)]);
    }
    if let Some(hat) = token.strip_prefix('h') {
        let (hat, direction) = hat.split_once('.').unwrap_or((hat, ""));
        let label = match direction {
            "1" => "D-pad up",
            "2" => "D-pad right",
            "4" => "D-pad down",
            _ => "D-pad left",
        };
        return tr_fmt(lang, "D-pad {}: {}", &[&number(hat), &tr(lang, label)]);
    }
    let axis = token.trim_start_matches(['+', '-']);
    let axis = axis.get(1..).unwrap_or_default().trim_end_matches('~');
    let direction = if token.starts_with('-') || token.ends_with('~') {
        "-"
    } else {
        "+"
    };
    tr_fmt(lang, "Axis {} ({})", &[&number(axis), direction])
}

/// The rows of the table (Python `update_rows`); `view` draws them.
pub fn rows(mapper: &MapperState, lang: &str) -> Vec<MapperRow> {
    let enabled = mapper.device.is_some() && mapper.capture.is_none();
    STEPS
        .iter()
        .map(|(key, _)| {
            let listening = mapper.editing.as_deref() == Some(*key);
            MapperRow {
                key,
                label: row_label(lang, key),
                assigned: input_name(mapper.bindings.get(*key).map(String::as_str), lang),
                configure: tr(
                    lang,
                    if listening {
                        "Listening…"
                    } else {
                        "Configure"
                    },
                ),
                configure_enabled: enabled,
                clear_enabled: enabled && mapper.bindings.contains_key(*key),
            }
        })
        .collect()
}

impl State {
    /// Opens the page over `joysticks` (the Controls button passes SDL3, the tests a scripted
    /// one): it lists the controllers and chooses the first. What opening could not read goes in
    /// the status, as the window has no console for it.
    #[doc(hidden)]
    pub fn open_mapper_with(&mut self, joysticks: Box<dyn Joysticks>) {
        let warnings = joysticks.warnings();
        let mut mapper = MapperState::new(joysticks, self.paths.data_dir.join(FILENAME));
        mapper.refresh(&self.lang);
        if !warnings.is_empty() {
            let status = std::mem::take(&mut mapper.status);
            mapper.status = warnings
                .into_iter()
                .chain(Some(status).filter(|status| !status.is_empty()))
                .collect::<Vec<_>>()
                .join("\n");
        }
        self.mapper = Some(mapper);
        // Showing this page starts nothing.
        let _ = show_page(self, Page::Mapper);
    }
}

/// The dialog `modal` under the page's `title` (the Python's message boxes had their own).
fn titled(mut modal: Modal, lang: &str, title: &str) -> Modal {
    modal.title = tr(lang, title);
    modal
}

/// The Controls page's button (Python `open_controller_mapper`): refused while the game runs; an
/// open page is shown again; otherwise SDL3 opens, or the dialog says why it did not.
fn open(state: &mut State) -> Task<Message> {
    if refuse_while_live(state, LiveAction::SetUpController) {
        if let Some(modal) = state.modal.take() {
            state.modal = Some(titled(modal, &state.lang, TITLE));
        }
        return Task::none();
    }
    if state.mapper.is_some() {
        return show_page(state, Page::Mapper);
    }
    match Sdl3::open(&state.paths.port_dir, &state.paths.data_dir, None) {
        Ok(sdl) => state.open_mapper_with(Box::new(sdl)),
        Err(error) => {
            let text = tr(&state.lang, &error.to_string());
            state.modal = Some(titled(Modal::error(text), &state.lang, TITLE));
        }
    }
    Task::none()
}

/// Lets go of the joysticks without saving (Python `close`): the pad closes, and SDL3's gamepads
/// stop and its hints come back as the adapter drops. Leaving the page and closing the window
/// call it.
pub fn close(state: &mut State) {
    if let Some(mut mapper) = state.mapper.take() {
        mapper.close_device();
    }
}

pub fn update(state: &mut State, message: MapperMessage) -> Task<Message> {
    match message {
        MapperMessage::Open => return open(state),
        // Leaving the page closes it.
        MapperMessage::Close => return show_page(state, Page::Controls),
        _ => {}
    }
    let Some(mapper) = state.mapper.as_mut() else {
        return Task::none();
    };
    let lang = state.lang.as_str();
    let failed = match message {
        MapperMessage::Choose(index) => {
            mapper.choice = Some(index);
            mapper.choose(lang);
            None
        }
        MapperMessage::Refresh => {
            mapper.refresh(lang);
            None
        }
        MapperMessage::Configure(target) => {
            mapper.begin_edit(lang, &target);
            None
        }
        MapperMessage::Clear(target) => mapper.clear(lang, &target),
        MapperMessage::CancelCapture => {
            mapper.cancel_capture(lang);
            None
        }
        MapperMessage::Poll => mapper.poll(lang),
        MapperMessage::Open | MapperMessage::Close => None,
    };
    if let Some(error) = failed {
        state.modal = Some(titled(Modal::error(error), &state.lang, SAVE_FAILED));
    }
    Task::none()
}

/// A key while the page is open: Esc cancels the capture, or else closes the page. The page takes
/// every other key, as the Python's window held the grab.
pub fn key_pressed(state: &mut State, key: &Key) -> Task<Message> {
    if *key != Key::Named(Named::Escape) {
        return Task::none();
    }
    let capturing = state
        .mapper
        .as_ref()
        .is_some_and(|mapper| mapper.capture.is_some());
    let message = if capturing {
        MapperMessage::CancelCapture
    } else {
        MapperMessage::Close
    };
    update(state, message)
}

/// The pad's clock, only while the page is open.
pub fn subscription(state: &State) -> Subscription<Message> {
    if state.mapper.is_none() {
        return Subscription::none();
    }
    iced::time::every(POLL).map(|_| Message::Mapper(MapperMessage::Poll))
}

/// The id of the Configure button of the row `key`.
pub fn configure_id(key: &str) -> Id {
    Id::from(format!("mapper-configure: {key}"))
}

/// The id of the Clear button of the row `key`.
pub fn clear_id(key: &str) -> Id {
    Id::from(format!("mapper-clear: {key}"))
}

fn heading<'a>(label: String) -> Element<'a, Message> {
    text(label)
        .font(CINZEL_BOLD)
        .size(13)
        .color(palette::MUTED)
        .into()
}

fn table_line<'a>(row: MapperRow, lang: &str) -> Element<'a, Message> {
    let configure = button(text(row.configure))
        .width(CONFIGURE_WIDTH)
        .style(theme::button_flat)
        .on_press_maybe(
            row.configure_enabled
                .then(|| Message::Mapper(MapperMessage::Configure(row.key.to_string()))),
        );
    let clear = button(text(tr(lang, "Clear")))
        .width(CLEAR_WIDTH)
        .style(theme::button_flat)
        .on_press_maybe(
            row.clear_enabled
                .then(|| Message::Mapper(MapperMessage::Clear(row.key.to_string()))),
        );
    layout![
        text(row.label).width(Fill),
        text(row.assigned).width(Fill),
        container(configure).id(configure_id(row.key)),
        container(clear).id(clear_id(row.key)),
    ]
    .spacing(12)
    .align_y(Center)
    .into()
}

/// The page (Python `MapperWindow.__init__`); the drawer's header has its title and note.
pub fn view(state: &State) -> Element<'_, Message> {
    let Some(mapper) = &state.mapper else {
        return space().into();
    };
    let lang = state.lang.as_str();
    let labels: Vec<String> = mapper
        .devices
        .iter()
        .map(|device| format!("{} [{}]", device.name, device.id))
        .collect();
    let selected = mapper.choice.and_then(|index| labels.get(index).cloned());
    let options = labels.clone();
    let choice = pick_list(labels, selected, move |label: String| {
        let index = options.iter().position(|known| *known == label);
        Message::Mapper(MapperMessage::Choose(index.unwrap_or_default()))
    })
    .width(Fill)
    .text_size(SELECT_TEXT)
    .style(theme::pick_list_dark);
    let refresh = button(text(tr(lang, "Refresh")))
        .style(theme::button_flat)
        .on_press(Message::Mapper(MapperMessage::Refresh));
    let titles = layout![
        container(heading(tr(lang, "Game control"))).width(Fill),
        container(heading(tr(lang, "Assigned input"))).width(Fill),
        space().width(CONFIGURE_WIDTH),
        space().width(CLEAR_WIDTH),
    ]
    .spacing(12);
    let table = rows(mapper, lang)
        .into_iter()
        .fold(column![].spacing(6), |table, row| {
            table.push(table_line(row, lang))
        });
    let cancel = button(text(tr(lang, "Cancel capture")))
        .style(theme::button_flat)
        .on_press_maybe(
            mapper
                .capture
                .is_some()
                .then_some(Message::Mapper(MapperMessage::CancelCapture)),
        );
    let close = button(text(tr(lang, "Close")))
        .style(theme::button_flat)
        .on_press(Message::Mapper(MapperMessage::Close));
    // Right to left the device row and the buttons are mirrored; the table is not, as the
    // Controls page's.
    let (device_row, buttons) = if state.rtl {
        (
            layout![refresh, choice],
            layout![close, space::horizontal(), cancel],
        )
    } else {
        (
            layout![choice, refresh],
            layout![cancel, space::horizontal(), close],
        )
    };
    let line = |body: &str, size: f32| {
        container(text(body.to_string()).size(size))
            .width(Fill)
            .align_x(mirror_x(Left, state.rtl))
    };
    let table_height = (state.window_size.height - BESIDE_TABLE).max(MIN_TABLE);
    column![
        device_row.spacing(8).align_y(Center),
        line(&mapper.details, 13.0),
        container(titles).padding(Padding::ZERO.top(8)),
        scrollable(table).spacing(8).height(table_height),
        container(
            text(mapper.prompt.clone())
                .font(CINZEL_BOLD)
                .size(16)
                .color(palette::TEXT)
        )
        .padding(Padding::ZERO.top(8))
        .width(Fill)
        .align_x(mirror_x(Left, state.rtl)),
        line(&mapper.status, 14.0),
        line(&mapper.preview_text, 13.0),
        container(buttons.align_y(Center)).padding(Padding::ZERO.top(8)),
    ]
    .spacing(8)
    .into()
}
