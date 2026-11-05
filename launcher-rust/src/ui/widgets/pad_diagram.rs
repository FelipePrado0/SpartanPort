//! The Controls page's live picture: the outline of each controller (`pad_art`), or a keyboard and
//! mouse made of ready icons (`input_icons`), where each input lights up as it is pressed and a
//! click picks it.
//!
//! The parts live in a fixed design space and the picture scales to fit, so hit tests and drawing
//! share one list, [`parts`].

use iced::alignment::{Horizontal, Vertical};
use iced::mouse;
use iced::widget::canvas::{self, stroke, Action, Frame, Geometry, Path, Stroke, Text};
use iced::widget::image::{self as image_widget, Handle};
use iced::widget::stack;
use iced::{Color, Element, Event, Fill, Pixels, Point, Rectangle, Renderer, Size, Theme, Vector};
use std::collections::BTreeMap;
use std::sync::{Mutex, PoisonError};

use crate::core::pad_model::PadModel;
use crate::ui::app::Message;
use crate::ui::pages::controls::ControlsMessage;
use crate::ui::theme::palette;
use crate::ui::widgets::button_icon::game_button;
use crate::ui::widgets::input_icons::{
    Icon, INPUT_ICONS, MOUSE_SIDE, MOUSE_TOP, PLAYSTATION4, PLAYSTATION5, SWITCH,
    XBOX as XBOX_ICONS,
};
use crate::ui::widgets::pad_art::{Art, ArtPart, DUALSENSE, DUALSHOCK4, SWITCH_PRO, XBOX};

/// What the picture shows.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Picture {
    Pad(PadModel),
    Keyboard,
}

/// The shape of one input in the design space.
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Shape {
    Round { x: f32, y: f32, r: f32 },
    Boxed { x: f32, y: f32, w: f32, h: f32 },
}

/// One input: its SDL name, what its cap reads, and where it is.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Part {
    pub id: &'static str,
    pub cap: &'static str,
    pub shape: Shape,
}

const KEYBOARD_SIZE: Size = Size::new(1040.0, 296.0);
/// A key's icon, and the distance from one key to the next.
const KEY: f32 = 44.0;
const KEY_STEP: f32 = 48.0;
/// The two mouse icons beside the keyboard: from above, and with the side buttons.
const MOUSE_X: f32 = 884.0;
const MOUSE_SIZE: f32 = 148.0;
/// A full stick's dead zone reads this many times the stick's well, so a small one still shows.
const ZONE_SCALE: f32 = 2.0;
/// The stick's cap travels this share of its well, so full tilt reads at a glance.
const CAP_TRAVEL: f32 = 0.55;
/// The height of the caption under the picture.
const CAPTION: f32 = 38.0;
/// An icon of the caption.
const CAPTION_ICON: f32 = 30.0;
/// What a controller button's cap reads at, in picture pixels.
const PAD_CAP: f32 = 34.0;

const fn boxed(id: &'static str, cap: &'static str, x: f32, y: f32, w: f32, h: f32) -> Part {
    Part {
        id,
        cap,
        shape: Shape::Boxed { x, y, w, h },
    }
}

fn art(model: PadModel) -> &'static Art {
    match model {
        PadModel::DualSense => &DUALSENSE,
        PadModel::DualShock4 => &DUALSHOCK4,
        PadModel::Xbox => &XBOX,
        PadModel::SwitchPro => &SWITCH_PRO,
    }
}

/// Art images, one handle each, so the renderer keeps them instead of decoding every frame.
fn handle(bytes: &'static [u8]) -> Handle {
    static HANDLES: Mutex<BTreeMap<usize, Handle>> = Mutex::new(BTreeMap::new());
    HANDLES
        .lock()
        .unwrap_or_else(PoisonError::into_inner)
        .entry(bytes.as_ptr() as usize)
        .or_insert_with(|| Handle::from_bytes(bytes))
        .clone()
}

fn pad_parts(model: PadModel) -> Vec<Part> {
    art(model)
        .parts
        .iter()
        .map(|part| {
            let (x, y, w, h) = (part.x as f32, part.y as f32, part.w as f32, part.h as f32);
            let round = matches!(
                part.id,
                "a" | "b" | "x" | "y" | "leftstick" | "rightstick" | "guide"
            );
            let shape = if round {
                Shape::Round {
                    x: x + w / 2.0,
                    y: y + h / 2.0,
                    r: w.max(h) / 2.0,
                }
            } else {
                Shape::Boxed { x, y, w, h }
            };
            Part {
                id: part.id,
                cap: part.cap,
                shape,
            }
        })
        .collect()
}

fn keyboard_parts() -> Vec<Part> {
    let mut parts = Vec::new();
    let mut keys = |row: f32, from: f32, names: &[&'static str]| {
        for (index, id) in names.iter().enumerate() {
            let x = (from + index as f32) * KEY_STEP;
            parts.push(boxed(id, "", x, row * KEY_STEP, KEY, KEY));
        }
    };
    keys(0.0, 0.0, &["Escape"]);
    keys(
        0.0,
        2.0,
        &[
            "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
        ],
    );
    keys(0.0, 15.0, &["PrintScreen", "ScrollLock", "Pause"]);
    keys(
        1.0,
        0.0,
        &[
            "`",
            "1",
            "2",
            "3",
            "4",
            "5",
            "6",
            "7",
            "8",
            "9",
            "0",
            "-",
            "Equals",
            "Backspace",
        ],
    );
    keys(1.0, 15.0, &["Insert", "Home", "PageUp"]);
    keys(
        2.0,
        0.0,
        &[
            "Tab", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "\\",
        ],
    );
    keys(2.0, 15.0, &["Delete", "End", "PageDown"]);
    keys(
        3.0,
        0.25,
        &[
            "CapsLock", "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "Return",
        ],
    );
    keys(
        4.0,
        0.5,
        &[
            "Left Shift",
            "Z",
            "X",
            "C",
            "V",
            "B",
            "N",
            "M",
            "Comma",
            ".",
            "/",
            "Right Shift",
        ],
    );
    keys(4.0, 16.0, &["Up"]);
    keys(5.0, 0.0, &["Left Ctrl", "Left GUI", "Left Alt"]);
    keys(5.0, 9.0, &["Right Alt", "Right Ctrl"]);
    keys(5.0, 15.0, &["Left", "Down", "Right"]);
    let space = 6.0 * KEY_STEP - (KEY_STEP - KEY);
    parts.extend([
        boxed("Space", "", 3.0 * KEY_STEP, 5.0 * KEY_STEP, space, KEY),
        boxed("Mouse Left", "", MOUSE_X + 34.0, 22.0, 31.0, 37.0),
        boxed("Mouse Right", "", MOUSE_X + 83.0, 22.0, 31.0, 37.0),
        boxed("Wheel Up", "", MOUSE_X + 65.0, 22.0, 18.0, 12.0),
        boxed("Mouse Middle", "", MOUSE_X + 65.0, 34.0, 18.0, 13.0),
        boxed("Wheel Down", "", MOUSE_X + 65.0, 47.0, 18.0, 12.0),
        boxed("Mouse Forward", "", MOUSE_X + 20.0, 170.0, 18.0, 36.0),
        boxed("Mouse Back", "", MOUSE_X + 20.0, 213.0, 18.0, 33.0),
    ]);
    parts
}

/// The icon of a keyboard or mouse input.
pub fn input_icon(input: &str) -> Option<&'static Icon> {
    let input = input.trim();
    INPUT_ICONS
        .iter()
        .find(|(name, _)| name.eq_ignore_ascii_case(input))
        .map(|(_, icon)| icon)
}

/// The icon of a gamepad input on `model`.
pub fn pad_icon(model: PadModel, input: &str) -> Option<&'static [u8]> {
    let find = |table: &'static [(&str, &[u8])]| {
        table
            .iter()
            .find(|(name, _)| *name == input)
            .map(|(_, icon)| *icon)
    };
    match model {
        PadModel::DualSense => find(PLAYSTATION5),
        PadModel::DualShock4 => find(PLAYSTATION4).or_else(|| find(PLAYSTATION5)),
        PadModel::Xbox => find(XBOX_ICONS),
        PadModel::SwitchPro => find(SWITCH),
    }
}

/// The game's own icon of a game button: always the PlayStation one, as the game was made for it.
pub fn game_icon(button: &str) -> Option<&'static [u8]> {
    pad_icon(PadModel::DualSense, game_button(button)?)
}

/// Every input of a picture.
pub fn parts(picture: Picture) -> Vec<Part> {
    match picture {
        Picture::Pad(model) => pad_parts(model),
        Picture::Keyboard => keyboard_parts(),
    }
}

fn design_size(picture: Picture) -> Size {
    match picture {
        Picture::Pad(model) => {
            let art = art(model);
            Size::new(art.width as f32, art.height as f32)
        }
        Picture::Keyboard => KEYBOARD_SIZE,
    }
}

impl Shape {
    fn holds(self, point: Point) -> bool {
        match self {
            Shape::Round { x, y, r } => (point.x - x).hypot(point.y - y) <= r,
            Shape::Boxed { x, y, w, h } => {
                point.x >= x && point.x <= x + w && point.y >= y && point.y <= y + h
            }
        }
    }
}

/// The input at a point of the design space. Later parts sit on top, as they are drawn.
pub fn part_at(picture: Picture, point: Point) -> Option<Part> {
    parts(picture)
        .into_iter()
        .rev()
        .find(|part| part.shape.holds(point))
}

/// The picture's scale and top-left corner in a widget of `bounds`, keeping its proportions.
fn placement(picture: Picture, bounds: Size) -> (f32, Vector) {
    let size = design_size(picture);
    let room = Size::new(bounds.width, (bounds.height - CAPTION).max(1.0));
    let scale = (room.width / size.width).min(room.height / size.height);
    let left = (room.width - size.width * scale) / 2.0;
    let top = (room.height - size.height * scale) / 2.0;
    (scale, Vector::new(left, top))
}

/// What the picture shows right now.
#[derive(Debug, Clone, Default)]
pub struct Live {
    /// SDL names of what is held.
    pub pressed: Vec<String>,
    /// Left x, y, right x, y, as -1 to 1; left and right trigger as 0 to 1.
    pub axes: [f32; 6],
    /// The two dead zones, as 0 to 1.
    pub deadzones: [f32; 2],
    /// Inputs with a game button, and which: the caption names them.
    pub bound: Vec<(String, String)>,
    /// Inputs moved off their default, marked gold.
    pub changed: Vec<String>,
    /// Inputs that press two game buttons, marked red.
    pub conflicts: Vec<String>,
    /// The input waiting for its replacement.
    pub selected: Option<String>,
    /// The line under the picture when the mouse is elsewhere.
    pub caption: String,
    /// The input just pressed and the game button it presses, shown as icons over the caption.
    pub shown: Option<Shown>,
    /// The left and right motors while they run: that grip turns red.
    pub motors: [bool; 2],
    /// Whether a click picks an input.
    pub clickable: bool,
}

/// What the caption shows of the input just pressed.
#[derive(Debug, Clone, Default, PartialEq)]
pub struct Shown {
    pub words: String,
    pub input: Option<&'static [u8]>,
    pub game: Option<&'static [u8]>,
    pub game_name: String,
}

pub struct PadDiagram {
    pub picture: Picture,
    pub live: Live,
}

fn shape_path(shape: Shape) -> Path {
    match shape {
        Shape::Round { x, y, r } => Path::circle(Point::new(x, y), r),
        Shape::Boxed { x, y, w, h } => {
            let radius = (w.min(h) / 4.0).min(7.0);
            Path::rounded_rectangle(Point::new(x, y), Size::new(w, h), radius.into())
        }
    }
}

fn centre(shape: Shape) -> Point {
    match shape {
        Shape::Round { x, y, .. } => Point::new(x, y),
        Shape::Boxed { x, y, w, h } => Point::new(x + w / 2.0, y + h / 2.0),
    }
}

fn line(path: &Path, frame: &mut Frame, color: Color, width: f32) {
    frame.stroke(
        path,
        Stroke {
            style: stroke::Style::Solid(color),
            width,
            ..Stroke::default()
        },
    );
}

fn cap_text(frame: &mut Frame, cap: &str, at: Point, ink: Color, size: f32) {
    frame.fill_text(Text {
        content: cap.to_string(),
        position: at,
        color: ink,
        size: Pixels(size),
        align_x: Horizontal::Center.into(),
        align_y: Vertical::Center,
        ..Text::default()
    });
}

fn caption(frame: &mut Frame, words: &str, width: f32, top: f32) {
    frame.fill_text(Text {
        content: words.to_string(),
        position: Point::new(width / 2.0, top + CAPTION / 2.0),
        color: palette::TEXT,
        size: Pixels(14.0),
        align_x: Horizontal::Center.into(),
        align_y: Vertical::Center,
        ..Text::default()
    });
}

/// The input, a small arrow and the game button it presses, side by side.
fn icon_caption(frame: &mut Frame, shown: &Shown, width: f32, top: f32) {
    let middle = width / 2.0;
    let y = top + (CAPTION - CAPTION_ICON) / 2.0;
    let centre_y = top + CAPTION / 2.0;
    let edge = middle - CAPTION_ICON - 16.0;
    frame.fill_text(Text {
        content: shown.words.clone(),
        position: Point::new(edge - 6.0, centre_y),
        color: palette::TEXT,
        size: Pixels(14.0),
        align_x: Horizontal::Right.into(),
        align_y: Vertical::Center,
        ..Text::default()
    });
    let square = |x: f32| Rectangle {
        x,
        y,
        width: CAPTION_ICON,
        height: CAPTION_ICON,
    };
    if let Some(input) = shown.input {
        frame.draw_image(square(edge), canvas::Image::new(handle(input)));
    }
    let head = Path::new(|path| {
        path.move_to(Point::new(middle - 5.0, centre_y - 6.0));
        path.line_to(Point::new(middle + 5.0, centre_y));
        path.line_to(Point::new(middle - 5.0, centre_y + 6.0));
        path.close();
    });
    frame.fill(&head, palette::SILVER);
    if let Some(game) = shown.game {
        frame.draw_image(square(middle + 16.0), canvas::Image::new(handle(game)));
    }
    frame.fill_text(Text {
        content: shown.game_name.clone(),
        position: Point::new(middle + CAPTION_ICON + 22.0, centre_y),
        color: palette::TEXT,
        size: Pixels(14.0),
        align_y: Vertical::Center,
        ..Text::default()
    });
}

impl PadDiagram {
    fn is(&self, list: &[String], id: &str) -> bool {
        list.iter().any(|known| known == id)
    }

    fn axis_of(&self, id: &str) -> Option<(f32, f32, f32)> {
        let a = &self.live.axes;
        match id {
            "leftstick" => Some((a[0], a[1], self.live.deadzones[0])),
            "rightstick" => Some((a[2], a[3], self.live.deadzones[1])),
            _ => None,
        }
    }

    fn trigger_of(&self, id: &str) -> Option<f32> {
        match id {
            "lefttrigger" => Some(self.live.axes[4]),
            "righttrigger" => Some(self.live.axes[5]),
            _ => None,
        }
    }

    /// The keyboard and mouse: every key's own icon, brighter when a game button uses it.
    fn draw_keyboard(
        &self,
        frame: &mut Frame,
        hovered: Option<&str>,
        place: impl Fn(f32, f32, f32, f32) -> Rectangle,
    ) {
        let (x, side) = (MOUSE_X, MOUSE_SIZE);
        let top = place(x, 0.0, side, side);
        let below = place(x, side, side, side);
        frame.draw_image(top, canvas::Image::new(handle(MOUSE_TOP)));
        frame.draw_image(below, canvas::Image::new(handle(MOUSE_SIDE)));
        for part in parts(self.picture) {
            let Shape::Boxed { x, y, w, h } = part.shape else {
                continue;
            };
            let Some(icon) = input_icon(part.id) else {
                continue;
            };
            let pressed = self.is(&self.live.pressed, part.id);
            let over = hovered == Some(part.id);
            if x >= MOUSE_X {
                // Each mouse icon is the whole mouse with that input in red.
                if pressed || over {
                    let at = if y < MOUSE_SIZE { top } else { below };
                    frame.draw_image(at, canvas::Image::new(handle(icon.plain)));
                }
                continue;
            }
            let bound = self.live.bound.iter().any(|(id, _)| id == part.id);
            let (bytes, opacity) = match (pressed, over, bound) {
                (true, _, _) => (icon.lit, 1.0_f32),
                (_, true, _) => (icon.plain, 1.0),
                (_, _, true) => (icon.plain, 0.9),
                _ => (icon.plain, 0.4),
            };
            // The bar of the keyboard reads across more than one key.
            let size = if w > KEY { KEY * 2.0 } else { KEY };
            frame.draw_image(
                place(x + (w - size) / 2.0, y + (h - size) / 2.0, size, size),
                canvas::Image::new(handle(bytes)).opacity(opacity),
            );
        }
    }

    /// Gold for an input waiting or moved off its default, red for one that presses two buttons.
    fn draw_ring(&self, frame: &mut Frame, part: &Part, selected: bool) {
        let path = shape_path(part.shape);
        if selected {
            line(&path, frame, palette::GOLD, 3.0 / self.line_scale());
        } else if self.is(&self.live.conflicts, part.id) {
            line(&path, frame, palette::RED_HI, 2.5 / self.line_scale());
        } else if self.is(&self.live.changed, part.id) {
            line(&path, frame, palette::GOLD, 1.5 / self.line_scale());
        }
    }

    /// Rings keep their width on the big controller pictures.
    fn line_scale(&self) -> f32 {
        match self.picture {
            Picture::Pad(_) => 0.5,
            Picture::Keyboard => 1.0,
        }
    }

    /// One input of a controller: its real shape lit over the picture.
    fn draw_pad_part(
        &self,
        frame: &mut Frame,
        part: &ArtPart,
        hovered: bool,
        place: impl Fn(f32, f32, f32, f32) -> Rectangle,
    ) {
        let id = part.id;
        let pressed = self.is(&self.live.pressed, id);
        let rect = place(part.x as f32, part.y as f32, part.w as f32, part.h as f32);
        let mask = |opacity: f32| canvas::Image::new(handle(part.mask)).opacity(opacity);
        // A trigger lights as far as it is pulled.
        let lit = match self.trigger_of(id) {
            Some(amount) => amount.clamp(0.0, 1.0),
            None if pressed => 1.0,
            None => 0.0,
        };
        if lit > 0.0 {
            frame.draw_image(rect, mask(lit));
        } else if hovered {
            frame.draw_image(rect, mask(0.35));
        }
        if let Some((sx, sy, zone)) = self.axis_of(id) {
            let reach = rect.width / 2.0;
            let tilted = sx.hypot(sy) > zone;
            if tilted && !pressed {
                let moved = Rectangle {
                    x: rect.x + sx * reach * CAP_TRAVEL,
                    y: rect.y + sy * reach * CAP_TRAVEL,
                    ..rect
                };
                frame.draw_image(moved, mask(0.55));
            }
        }
    }

    /// A stick's dead zone as a gold disc with how much it is, and where the stick is now.
    fn draw_stick(&self, frame: &mut Frame, part: &Part) {
        let (Some((sx, sy, zone)), Shape::Round { x, y, r }) = (self.axis_of(part.id), part.shape)
        else {
            return;
        };
        let middle = Point::new(x, y);
        let tip = Path::circle(middle + Vector::new(sx * r, sy * r), 12.0);
        if zone <= 0.0 {
            frame.fill(&tip, palette::TEXT);
            return;
        }
        let disc = Path::circle(middle, r * ZONE_SCALE * zone);
        frame.fill(
            &disc,
            Color {
                a: 0.45,
                ..palette::GOLD
            },
        );
        line(&disc, frame, palette::GOLD, 3.0 / self.line_scale());
        let percent = format!("{}%", (zone * 100.0).round() as u32);
        let below = middle + Vector::new(0.0, r + PAD_CAP * 0.75);
        cap_text(frame, &percent, below, palette::GOLD, PAD_CAP);
        frame.fill(&tip, palette::TEXT);
    }

    /// What sits over the picture.
    fn draw_marks(&self, frame: &mut Frame, scale: f32, offset: Vector) {
        frame.with_save(|frame| {
            frame.translate(offset);
            frame.scale(scale);
            for part in parts(self.picture) {
                let selected = self.live.selected.as_deref() == Some(part.id);
                self.draw_ring(frame, &part, selected);
                self.draw_stick(frame, &part);
            }
        });
    }

    fn hovered(&self, bounds: Rectangle, cursor: mouse::Cursor) -> Option<Part> {
        let spot = cursor.position_in(bounds)?;
        let (scale, offset) = placement(self.picture, bounds.size());
        let design = Point::new((spot.x - offset.x) / scale, (spot.y - offset.y) / scale);
        part_at(self.picture, design)
    }

    fn hover_words(&self, part: &Part) -> Option<String> {
        self.live
            .bound
            .iter()
            .find(|(id, _)| id == part.id)
            .map(|(_, words)| words.clone())
    }
}

impl canvas::Program<Message> for PadDiagram {
    type State = ();

    fn update(
        &self,
        _state: &mut (),
        event: &Event,
        bounds: Rectangle,
        cursor: mouse::Cursor,
    ) -> Option<Action<Message>> {
        match event {
            Event::Mouse(mouse::Event::ButtonPressed(mouse::Button::Left))
                if self.live.clickable =>
            {
                let part = self.hovered(bounds, cursor)?;
                Some(
                    Action::publish(Message::Controls(ControlsMessage::PartPressed(
                        part.id.to_string(),
                    )))
                    .and_capture(),
                )
            }
            Event::Mouse(mouse::Event::CursorMoved { .. }) if cursor.is_over(bounds) => {
                Some(Action::request_redraw())
            }
            _ => None,
        }
    }

    fn draw(
        &self,
        _state: &(),
        renderer: &Renderer,
        _theme: &Theme,
        bounds: Rectangle,
        cursor: mouse::Cursor,
    ) -> Vec<Geometry> {
        let mut frame = Frame::new(renderer, bounds.size());
        let (scale, offset) = placement(self.picture, bounds.size());
        let hovered = self.hovered(bounds, cursor).filter(|_| self.live.clickable);
        let place = |x: f32, y: f32, w: f32, h: f32| Rectangle {
            x: offset.x + x * scale,
            y: offset.y + y * scale,
            width: w * scale,
            height: h * scale,
        };
        if self.picture == Picture::Keyboard {
            self.draw_keyboard(&mut frame, hovered.as_ref().map(|part| part.id), place);
        }
        if let Picture::Pad(model) = self.picture {
            let art = art(model);
            let (w, h) = (art.width as f32, art.height as f32);
            frame.draw_image(place(0.0, 0.0, w, h), canvas::Image::new(handle(art.base)));
            for (grip, running) in art.grips.into_iter().zip(self.live.motors) {
                if running {
                    frame.draw_image(place(0.0, 0.0, w, h), canvas::Image::new(handle(grip)));
                }
            }
            for part in art.parts {
                let over = hovered.is_some_and(|hover| hover.id == part.id);
                self.draw_pad_part(&mut frame, part, over, place);
            }
            frame.with_save(|frame| {
                frame.translate(offset);
                frame.scale(scale);
                for part in parts(self.picture) {
                    if !part.cap.is_empty() {
                        let ink = if self.is(&self.live.pressed, part.id) {
                            palette::TEXT
                        } else {
                            palette::SILVER
                        };
                        cap_text(frame, part.cap, centre(part.shape), ink, PAD_CAP);
                    }
                }
            });
        }
        let top = bounds.height - CAPTION;
        let words = hovered.as_ref().and_then(|part| self.hover_words(part));
        match (words, &self.live.shown) {
            (Some(words), _) => caption(&mut frame, &words, bounds.width, top),
            (None, Some(shown)) => icon_caption(&mut frame, shown, bounds.width, top),
            (None, None) => caption(&mut frame, &self.live.caption, bounds.width, top),
        }
        vec![frame.into_geometry()]
    }

    fn mouse_interaction(
        &self,
        _state: &(),
        bounds: Rectangle,
        cursor: mouse::Cursor,
    ) -> mouse::Interaction {
        if self.live.clickable && self.hovered(bounds, cursor).is_some() {
            mouse::Interaction::Pointer
        } else {
            mouse::Interaction::default()
        }
    }
}

/// What sits over the picture, on a layer of its own: inside one canvas images cover shapes.
struct PadMarks(PadDiagram);

impl canvas::Program<Message> for PadMarks {
    type State = ();

    fn draw(
        &self,
        _state: &(),
        renderer: &Renderer,
        _theme: &Theme,
        bounds: Rectangle,
        _cursor: mouse::Cursor,
    ) -> Vec<Geometry> {
        let mut frame = Frame::new(renderer, bounds.size());
        let (scale, offset) = placement(self.0.picture, bounds.size());
        self.0.draw_marks(&mut frame, scale, offset);
        vec![frame.into_geometry()]
    }
}

/// The picture at the page's width.
pub fn pad_diagram<'a>(picture: Picture, live: Live, height: f32) -> Element<'a, Message> {
    let marks = PadMarks(PadDiagram {
        picture,
        live: live.clone(),
    });
    stack![
        canvas::Canvas::new(PadDiagram { picture, live })
            .width(Fill)
            .height(height),
        canvas::Canvas::new(marks).width(Fill).height(height),
    ]
    .into()
}

/// How wide the picture is at `height`.
pub fn picture_width(picture: Picture, height: f32) -> f32 {
    let size = design_size(picture);
    (height - CAPTION) * size.width / size.height
}

/// The icon beside a key or mouse input in the key table.
pub fn input_glyph<'a>(input: &str) -> Option<Element<'a, Message>> {
    let icon = input_icon(input)?;
    Some(
        image_widget::Image::new(handle(icon.plain))
            .width(28.0)
            .height(28.0)
            .into(),
    )
}

#[cfg(test)]
mod tests {
    #![allow(clippy::unwrap_used)]

    use super::*;
    use crate::core::controls::{table_rows, Kind, PAD_STYLES};
    use std::collections::HashSet;

    const PADS: [PadModel; 4] = PadModel::ALL;

    #[test]
    fn every_gamepad_input_has_a_part_on_every_model() {
        for model in PADS {
            let ids: HashSet<&str> = parts(Picture::Pad(model)).iter().map(|p| p.id).collect();
            let (_, labels) = PAD_STYLES
                .iter()
                .find(|(style, _)| *style == model.style())
                .unwrap();
            for (name, _) in *labels {
                // Not every controller has these: only the PlayStation ones have a touchpad, and
                // the extra button is the DualSense's mute and the Pro Controller's capture.
                let optional = match *name {
                    "touchpad" => model.style() != "playstation",
                    "misc1" => !matches!(model, PadModel::DualSense | PadModel::SwitchPro),
                    _ => false,
                };
                assert!(optional || ids.contains(name), "{model:?} {name}");
            }
        }
    }

    #[test]
    fn a_click_on_a_part_finds_it_and_no_two_parts_share_an_id() {
        for picture in PADS
            .map(Picture::Pad)
            .into_iter()
            .chain([Picture::Keyboard])
        {
            let all = parts(picture);
            let ids: HashSet<&str> = all.iter().map(|p| p.id).collect();
            assert_eq!(ids.len(), all.len(), "{picture:?}");
            for part in &all {
                let found = part_at(picture, centre(part.shape)).unwrap();
                assert_eq!(found.id, part.id, "{picture:?}");
            }
            assert!(part_at(picture, Point::new(-5.0, -5.0)).is_none());
        }
    }

    #[test]
    fn nothing_leaves_the_design_space() {
        for picture in PADS
            .map(Picture::Pad)
            .into_iter()
            .chain([Picture::Keyboard])
        {
            let size = design_size(picture);
            for part in parts(picture) {
                let (right, bottom) = match part.shape {
                    Shape::Round { x, y, r } => (x + r, y + r),
                    Shape::Boxed { x, y, w, h } => (x + w, y + h),
                };
                assert!(right <= size.width && bottom <= size.height, "{}", part.id);
            }
        }
    }

    #[test]
    fn the_keyboard_shows_every_default_input_it_has_room_for() {
        let ids: HashSet<String> = parts(Picture::Keyboard)
            .iter()
            .map(|p| p.id.to_lowercase())
            .collect();
        for row in table_rows(Kind::Key) {
            let input = crate::core::controls::default_input(row, Kind::Key).unwrap_or_default();
            assert!(ids.contains(&input.to_lowercase()), "{input}");
        }
    }

    #[test]
    fn every_key_of_the_picture_has_an_icon() {
        for part in parts(Picture::Keyboard) {
            assert!(input_icon(part.id).is_some(), "{}", part.id);
        }
    }

    #[test]
    fn every_input_of_a_controller_has_an_icon_and_the_game_is_always_playstation() {
        for model in PADS {
            for part in parts(Picture::Pad(model)) {
                // The pack has no PS button.
                let playstation = model.style() == "playstation";
                if !(playstation && part.id == "guide") {
                    assert!(pad_icon(model, part.id).is_some(), "{model:?} {}", part.id);
                }
            }
        }
        assert_eq!(game_icon("cross"), pad_icon(PadModel::DualSense, "a"),);
        assert_ne!(game_icon("cross"), pad_icon(PadModel::Xbox, "a"));
    }

    #[test]
    fn the_picture_scales_to_fit() {
        let (scale, offset) = placement(Picture::Keyboard, Size::new(520.0, 400.0));
        assert_eq!(scale, 0.5);
        assert_eq!(offset.x, 0.0);
    }

    #[test]
    fn keys_and_mouse_inputs_get_their_icon() {
        assert!(input_glyph("mouse left").is_some());
        assert!(input_glyph(" q ").is_some());
        assert!(input_glyph("None").is_none());
    }
}
