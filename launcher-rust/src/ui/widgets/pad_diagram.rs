//! The Controls page's live picture: the real outline of each controller (`pad_art`), or a keyboard
//! and mouse, where each input lights up as it is pressed and a click picks it.
//!
//! The parts live in a fixed design space and the picture scales to fit, so hit tests and drawing
//! share one list, [`parts`].

use iced::alignment::{Horizontal, Vertical};
use iced::mouse;
use iced::widget::canvas::{self, stroke, Action, Frame, Geometry, Path, Stroke, Text};
use iced::widget::image::Handle;
use iced::{Color, Element, Event, Fill, Pixels, Point, Rectangle, Renderer, Size, Theme, Vector};
use std::collections::BTreeMap;
use std::sync::{Mutex, PoisonError};

use crate::core::pad_model::PadModel;
use crate::ui::app::Message;
use crate::ui::pages::controls::ControlsMessage;
use crate::ui::theme::palette;
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

const KEYBOARD_SIZE: Size = Size::new(860.0, 268.0);
/// The stick's cap travels this share of its well, so full tilt reads at a glance.
const CAP_TRAVEL: f32 = 0.55;
/// The height of the caption under the picture.
const CAPTION: f32 = 26.0;
/// What a controller button's cap reads at, in picture pixels.
const PAD_CAP: f32 = 34.0;
const WHITE_KEY: Color = Color::from_rgb(0.145, 0.118, 0.118);

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

/// Rows of the keyboard: SDL name, cap and width. Only what a compact board shows; the table still
/// binds every key.
const KEY_ROWS: [&[(&str, &str, f32)]; 5] = [
    &[
        ("Escape", "Esc", 44.0),
        ("1", "1", 44.0),
        ("2", "2", 44.0),
        ("3", "3", 44.0),
        ("4", "4", 44.0),
        ("5", "5", 44.0),
        ("6", "6", 44.0),
        ("7", "7", 44.0),
        ("8", "8", 44.0),
        ("9", "9", 44.0),
        ("0", "0", 44.0),
        ("Backspace", "Back", 76.0),
    ],
    &[
        ("Tab", "Tab", 60.0),
        ("Q", "Q", 44.0),
        ("W", "W", 44.0),
        ("E", "E", 44.0),
        ("R", "R", 44.0),
        ("T", "T", 44.0),
        ("Y", "Y", 44.0),
        ("U", "U", 44.0),
        ("I", "I", 44.0),
        ("O", "O", 44.0),
        ("P", "P", 44.0),
        ("[", "[", 44.0),
        ("]", "]", 12.0),
    ],
    &[
        ("CapsLock", "Caps", 72.0),
        ("A", "A", 44.0),
        ("S", "S", 44.0),
        ("D", "D", 44.0),
        ("F", "F", 44.0),
        ("G", "G", 44.0),
        ("H", "H", 44.0),
        ("J", "J", 44.0),
        ("K", "K", 44.0),
        ("L", "L", 44.0),
        ("Return", "Enter", 92.0),
    ],
    &[
        ("Left Shift", "Shift", 92.0),
        ("Z", "Z", 44.0),
        ("X", "X", 44.0),
        ("C", "C", 44.0),
        ("V", "V", 44.0),
        ("B", "B", 44.0),
        ("N", "N", 44.0),
        ("M", "M", 44.0),
        ("Comma", ",", 44.0),
        (".", ".", 44.0),
    ],
    &[
        ("Left Ctrl", "Ctrl", 60.0),
        ("Left Alt", "Alt", 60.0),
        ("Space", "Space", 230.0),
        ("Right Alt", "Alt", 60.0),
        ("Right Ctrl", "Ctrl", 60.0),
    ],
];
const KEY_HEIGHT: f32 = 44.0;
const KEY_GAP: f32 = 4.0;

fn keyboard_parts() -> Vec<Part> {
    let mut parts = Vec::new();
    for (index, row) in KEY_ROWS.iter().enumerate() {
        let y = 16.0 + index as f32 * (KEY_HEIGHT + KEY_GAP);
        let mut x = 8.0;
        for (id, cap, width) in *row {
            parts.push(boxed(id, cap, x, y, *width, KEY_HEIGHT));
            x += width + KEY_GAP;
        }
    }
    let bottom = 16.0 + 4.0 * (KEY_HEIGHT + KEY_GAP);
    parts.extend([
        boxed("Up", "", 548.0, bottom - 48.0, KEY_HEIGHT, KEY_HEIGHT),
        boxed("Left", "", 500.0, bottom, KEY_HEIGHT, KEY_HEIGHT),
        boxed("Down", "", 548.0, bottom, KEY_HEIGHT, KEY_HEIGHT),
        boxed("Right", "", 596.0, bottom, KEY_HEIGHT, KEY_HEIGHT),
        // The mouse.
        boxed("Mouse Left", "", 686.0, 22.0, 66.0, 92.0),
        boxed("Mouse Right", "", 776.0, 22.0, 66.0, 92.0),
        boxed("Wheel Up", "", 756.0, 26.0, 16.0, 26.0),
        boxed("Mouse Middle", "", 756.0, 56.0, 16.0, 26.0),
        boxed("Wheel Down", "", 756.0, 86.0, 16.0, 26.0),
        boxed("Mouse Forward", "", 666.0, 122.0, 14.0, 34.0),
        boxed("Mouse Back", "", 666.0, 160.0, 14.0, 34.0),
    ]);
    parts
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
    /// Whether a click picks an input.
    pub clickable: bool,
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

/// A small triangle on an arrow key.
fn arrow(frame: &mut Frame, id: &str, at: Point, color: Color) {
    const S: f32 = 5.0;
    let (dx, dy) = match id {
        "Up" | "Wheel Up" => (0.0, -1.0),
        "Down" | "Wheel Down" => (0.0, 1.0),
        "Left" => (-1.0, 0.0),
        "Right" => (1.0, 0.0),
        _ => return,
    };
    let tip = at + Vector::new(dx * S, dy * S);
    let path = Path::new(|p| {
        p.move_to(tip);
        p.line_to(at + Vector::new(-dx * S - dy * S, -dy * S - dx * S));
        p.line_to(at + Vector::new(-dx * S + dy * S, -dy * S + dx * S));
        p.close();
    });
    frame.fill(&path, color);
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

    fn draw_part(&self, frame: &mut Frame, part: &Part, hovered: bool) {
        let id = part.id;
        let pressed = self.is(&self.live.pressed, id);
        let selected = self.live.selected.as_deref() == Some(id);
        let path = shape_path(part.shape);
        let bound = self.live.bound.iter().any(|(known, _)| known == id);
        let rest = if bound {
            Color::from_rgb(0.2, 0.16, 0.16)
        } else {
            WHITE_KEY
        };
        let fill = if pressed {
            palette::RED_HI
        } else if hovered {
            palette::LINE
        } else {
            rest
        };
        frame.fill(&path, fill);
        self.draw_ring(frame, part, selected);
        let ink = if pressed {
            palette::TEXT
        } else if bound {
            palette::SILVER
        } else {
            palette::MUTED
        };
        let at = centre(part.shape);
        if part.cap.is_empty() {
            arrow(frame, id, at, ink);
        } else {
            cap_text(
                frame,
                part.cap,
                at,
                ink,
                if part.cap.len() > 3 { 11.0 } else { 14.0 },
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
        } else if self.picture == Picture::Keyboard {
            line(&path, frame, palette::LINE, 1.0);
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
            let scale = rect.width / part.w as f32;
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
            let middle = rect.center();
            if zone > 0.0 {
                line(
                    &Path::circle(middle, reach * zone),
                    frame,
                    palette::GOLD,
                    1.5,
                );
            }
            frame.fill(
                &Path::circle(
                    middle + Vector::new(sx * reach, sy * reach),
                    6.0 * scale.max(0.5),
                ),
                palette::TEXT,
            );
        }
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
        frame.with_save(|frame| {
            frame.translate(offset);
            frame.scale(scale);
            if self.picture == Picture::Keyboard {
                // The mouse's shell under its buttons.
                let shell = Path::rounded_rectangle(
                    Point::new(680.0, 16.0),
                    Size::new(168.0, 230.0),
                    60.0.into(),
                );
                frame.fill(&shell, palette::CARD);
                line(&shell, frame, palette::LINE, 2.0);
                for part in parts(self.picture) {
                    let over = hovered.is_some_and(|hover| hover.id == part.id);
                    self.draw_part(frame, &part, over);
                }
            }
        });
        if let Picture::Pad(model) = self.picture {
            let art = art(model);
            let place = |x: f32, y: f32, w: f32, h: f32| Rectangle {
                x: offset.x + x * scale,
                y: offset.y + y * scale,
                width: w * scale,
                height: h * scale,
            };
            let (w, h) = (art.width as f32, art.height as f32);
            frame.draw_image(place(0.0, 0.0, w, h), canvas::Image::new(handle(art.base)));
            for part in art.parts {
                let over = hovered.is_some_and(|hover| hover.id == part.id);
                self.draw_pad_part(&mut frame, part, over, place);
            }
            frame.with_save(|frame| {
                frame.translate(offset);
                frame.scale(scale);
                for part in parts(self.picture) {
                    let selected = self.live.selected.as_deref() == Some(part.id);
                    self.draw_ring(frame, &part, selected);
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
        let words = hovered
            .as_ref()
            .and_then(|part| self.hover_words(part))
            .unwrap_or_else(|| self.live.caption.clone());
        caption(&mut frame, &words, bounds.width, bounds.height - CAPTION);
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

/// The picture at the page's width.
pub fn pad_diagram<'a>(picture: Picture, live: Live, height: f32) -> Element<'a, Message> {
    canvas::Canvas::new(PadDiagram { picture, live })
        .width(Fill)
        .height(height)
        .into()
}

/// The little mouse beside a mouse input in the key table: the part it names is lit.
struct MouseGlyph(&'static str);

impl canvas::Program<Message> for MouseGlyph {
    type State = ();

    fn draw(
        &self,
        _state: &(),
        renderer: &Renderer,
        _theme: &Theme,
        bounds: Rectangle,
        _cursor: mouse::Cursor,
    ) -> Vec<Geometry> {
        const W: f32 = 18.0;
        const H: f32 = 26.0;
        let mut frame = Frame::new(renderer, bounds.size());
        let shell = Path::rounded_rectangle(
            Point::new(4.0, 1.0),
            Size::new(W - 2.0, H - 2.0),
            8.0.into(),
        );
        line(&shell, &mut frame, palette::SILVER, 1.5);
        let lit = match self.0 {
            "Mouse Left" => Some((Point::new(5.0, 2.0), Size::new(7.0, 10.0))),
            "Mouse Right" => Some((Point::new(12.0, 2.0), Size::new(7.0, 10.0))),
            "Mouse Middle" => Some((Point::new(10.5, 4.0), Size::new(3.0, 7.0))),
            "Wheel Up" => Some((Point::new(10.5, 2.0), Size::new(3.0, 5.0))),
            "Wheel Down" => Some((Point::new(10.5, 7.0), Size::new(3.0, 5.0))),
            "Mouse Back" => Some((Point::new(0.0, 14.0), Size::new(3.0, 5.0))),
            "Mouse Forward" => Some((Point::new(0.0, 8.0), Size::new(3.0, 5.0))),
            _ => None,
        };
        if let Some((at, size)) = lit {
            frame.fill(
                &Path::rounded_rectangle(at, size, 1.5.into()),
                palette::RED_HI,
            );
        }
        vec![frame.into_geometry()]
    }
}

/// The mouse glyph of an input name, or `None` for a key.
pub fn mouse_glyph<'a>(input: &str) -> Option<Element<'a, Message>> {
    const NAMES: [&str; 7] = [
        "Mouse Left",
        "Mouse Right",
        "Mouse Middle",
        "Mouse Back",
        "Mouse Forward",
        "Wheel Up",
        "Wheel Down",
    ];
    let name = NAMES
        .into_iter()
        .find(|name| name.eq_ignore_ascii_case(input.trim()))?;
    Some(
        canvas::Canvas::new(MouseGlyph(name))
            .width(24.0)
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
    fn the_picture_scales_to_fit() {
        let (scale, offset) = placement(Picture::Keyboard, Size::new(430.0, 400.0));
        assert_eq!(scale, 0.5);
        assert_eq!(offset.x, 0.0);
    }

    #[test]
    fn only_mouse_inputs_get_the_glyph() {
        assert!(mouse_glyph("mouse left").is_some());
        assert!(mouse_glyph("Q").is_none());
    }
}
