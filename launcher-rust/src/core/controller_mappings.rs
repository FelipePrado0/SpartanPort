//! Generic controller mappings (Python `scripts/controller_mappings.py`): the SDL lines of
//! `gamecontrollerdb.txt` the mapper saves, the raw capture that records one input per prompt and
//! the environment that hands the saved lines to the game.

use std::fs;
use std::io::{self, Write};
use std::path::{Path, PathBuf};

use indexmap::{IndexMap, IndexSet};
use tempfile::{NamedTempFile, TempPath};
use thiserror::Error;

/// The saved profiles, in the data folder.
pub const FILENAME: &str = "gamecontrollerdb.txt";
/// The inputs the mapper asks for, in order: the SDL name and the label. The sticks are asked in
/// their positive direction, so reversed axes can be recorded too.
pub static STEPS: &[(&str, &str)] = &[
    ("a", "Cross / A"),
    ("b", "Circle / B"),
    ("x", "Square / X"),
    ("y", "Triangle / Y"),
    ("leftshoulder", "L1 / LB"),
    ("rightshoulder", "R1 / RB"),
    ("lefttrigger", "L2 / LT"),
    ("righttrigger", "R2 / RT"),
    ("back", "Select / View / Share"),
    ("start", "Start / Options"),
    ("leftstick", "L3"),
    ("rightstick", "R3"),
    ("dpup", "D-pad up"),
    ("dpdown", "D-pad down"),
    ("dpleft", "D-pad left"),
    ("dpright", "D-pad right"),
    ("leftx", "Move the left stick to the right"),
    ("lefty", "Move the left stick down"),
    ("rightx", "Move the right stick to the right"),
    ("righty", "Move the right stick down"),
];
pub static STICK_AXES: &[&str] = &["leftx", "lefty", "rightx", "righty"];
pub static TRIGGERS: &[&str] = &["lefttrigger", "righttrigger"];

/// The platforms SDL names in a mapping line.
const PLATFORMS: [&str; 3] = ["Windows", "Linux", "Mac OS X"];
const CROSS_FIRST: &str = "Map Cross / A before saving.";
const INVALID_BINDING: &str = "Invalid controller binding.";

#[derive(Debug, Error)]
pub enum MappingError {
    /// What the Python raises as `ValueError`: the message the mapper shows.
    #[error("{0}")]
    Invalid(&'static str),
    /// A file or folder that could not be read or written. The text holds the reason, so it is
    /// not also the `source` (an error chain would repeat it).
    #[error("{}: {error}", path.display())]
    Io { path: PathBuf, error: io::Error },
}

/// The [`MappingError::Io`] of a failure on `path`.
fn at(path: &Path) -> impl Fn(io::Error) -> MappingError + '_ {
    move |error| MappingError::Io {
        path: path.to_path_buf(),
        error,
    }
}

/// One poll of a raw joystick: its buttons, its axes (-32768 to 32767) and its hats (SDL's
/// direction bits).
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct State {
    pub buttons: Vec<bool>,
    pub axes: Vec<i16>,
    pub hats: Vec<u8>,
}

/// The mapper's raw capture (Python `Capture`): one input for each prompt of [`STEPS`], from
/// `index` on. An input counts once it is the same for 3 polls in a row, and the next prompt waits
/// until everything is back at rest for 3 polls. Stick noise under 7000 is rest; an axis counts
/// from 18000 away from where it rested.
#[derive(Debug, Clone)]
pub struct Capture {
    /// The prompt of [`STEPS`] being captured.
    pub index: usize,
    /// What was captured so far, by SDL name.
    pub bindings: IndexMap<String, String>,
    /// Whether the last input was refused because another control already has it.
    pub error: bool,
    baseline: State,
    waiting_release: bool,
    candidate: Option<String>,
    frames: u32,
}

impl Capture {
    /// A capture whose rest position is `baseline`; refused while a button or the D-pad is held.
    pub fn new(baseline: &State) -> Result<Capture, MappingError> {
        if baseline.buttons.iter().any(|&pressed| pressed)
            || baseline.hats.iter().any(|&hat| hat != 0)
        {
            return Err(MappingError::Invalid(
                "Release all buttons and the D-pad before starting.",
            ));
        }
        Ok(Capture {
            index: 0,
            bindings: IndexMap::new(),
            error: false,
            baseline: baseline.clone(),
            waiting_release: false,
            candidate: None,
            frames: 0,
        })
    }

    /// Whether every prompt was answered or skipped.
    pub fn done(&self) -> bool {
        self.index >= STEPS.len()
    }

    fn neutral(&self, state: &State) -> bool {
        !state.buttons.iter().any(|&pressed| pressed)
            && !state.hats.iter().any(|&hat| hat != 0)
            && state.axes.len() == self.baseline.axes.len()
            && state
                .axes
                .iter()
                .zip(&self.baseline.axes)
                .all(|(&value, &rest)| (i32::from(value) - i32::from(rest)).abs() < 7000)
    }

    /// The input `state` shows for the current prompt: a button or a hat direction (not for a
    /// stick prompt), else the axis furthest from its rest.
    fn detect(&self, state: &State) -> Option<String> {
        let target = STEPS[self.index].0;
        let stick = STICK_AXES.contains(&target);
        if !stick {
            if let Some(i) = state.buttons.iter().position(|&pressed| pressed) {
                return Some(format!("b{i}"));
            }
            let mut hats = state.hats.iter().enumerate();
            if let Some((i, value)) = hats.find(|&(_, &value)| matches!(value, 1 | 2 | 4 | 8)) {
                return Some(format!("h{i}.{value}"));
            }
        }
        if state.axes.len() != self.baseline.axes.len() {
            return None;
        }
        // Python's `max` of (distance, axis, delta): the furthest axis, the last one on a tie.
        let (magnitude, i, delta) = state
            .axes
            .iter()
            .zip(&self.baseline.axes)
            .enumerate()
            .map(|(i, (&value, &rest))| {
                let delta = i32::from(value) - i32::from(rest);
                (delta.abs(), i, delta)
            })
            .max()?;
        if magnitude < 18000 {
            return None;
        }
        let rest = i32::from(self.baseline.axes[i]).abs();
        let inverted = if delta < 0 { "~" } else { "" };
        if stick {
            // A trigger resting at an endpoint is not a centered stick.
            return (rest <= 12000).then(|| format!("a{i}{inverted}"));
        }
        if TRIGGERS.contains(&target) && rest > 24000 {
            return Some(format!("a{i}{inverted}"));
        }
        Some(format!("{}a{i}", if delta > 0 { '+' } else { '-' }))
    }

    /// One poll: the input just captured for the current prompt, which moves the capture to the
    /// next one; `None` otherwise.
    pub fn feed(&mut self, state: &State) -> Option<String> {
        if self.done() {
            return None;
        }
        if self.waiting_release {
            self.frames = if self.neutral(state) {
                self.frames.saturating_add(1)
            } else {
                0
            };
            if self.frames >= 3 {
                self.waiting_release = false;
                self.frames = 0;
            }
            return None;
        }
        let token = self.detect(state);
        if token != self.candidate {
            self.candidate.clone_from(&token);
            self.frames = 0;
        }
        let token = token?;
        self.frames = self.frames.saturating_add(1);
        if self.frames < 3 {
            return None;
        }
        let target = STEPS[self.index].0;
        // Full axes cannot also act as another control; opposite half axes can be shared.
        let axis = axis_of(&token);
        let taken = self.bindings.values().any(|value| {
            token == *value
                || (axis.is_some()
                    && axis == axis_of(value)
                    && (token.starts_with('a') || value.starts_with('a')))
        });
        if taken {
            self.error = true;
            return None;
        }
        self.bindings.insert(target.to_string(), token.clone());
        self.index += 1;
        self.rearm();
        Some(token)
    }

    /// Leaves the current prompt unmapped. Cross / A cannot be skipped.
    pub fn skip(&mut self) -> Result<(), MappingError> {
        if !self.done() {
            if self.index == 0 {
                return Err(MappingError::Invalid(CROSS_FIRST));
            }
            self.index += 1;
            self.rearm();
        }
        Ok(())
    }

    /// Goes back one prompt, dropping what it and the later ones captured.
    pub fn back(&mut self) {
        if self.index > 0 {
            self.index -= 1;
            let kept = &STEPS[..self.index.min(STEPS.len())];
            self.bindings
                .retain(|key, _| kept.iter().any(|(step, _)| step == key));
            self.rearm();
        }
    }

    /// The next prompt starts once everything is released.
    fn rearm(&mut self) {
        self.waiting_release = true;
        self.frames = 0;
        self.candidate = None;
        self.error = false;
    }
}

/// `[0-9a-fA-F]{32}` (Python `GUID_RE`).
fn is_guid(text: &str) -> bool {
    text.len() == 32 && text.bytes().all(|byte| byte.is_ascii_hexdigit())
}

/// `b<n>`, `h<n>.<1|2|4|8>` or `[+-]a<n>[~]` (Python `INPUT_RE`); the digits are ASCII, the only
/// ones SDL reads.
fn is_input(text: &str) -> bool {
    /// What follows one or more digits at the start of `text`.
    fn number(text: &str) -> Option<&str> {
        let end = text
            .find(|c: char| !c.is_ascii_digit())
            .unwrap_or(text.len());
        (end > 0).then(|| &text[end..])
    }
    if let Some(rest) = text.strip_prefix('b') {
        return number(rest) == Some("");
    }
    if let Some(rest) = text.strip_prefix('h') {
        return matches!(number(rest), Some(".1" | ".2" | ".4" | ".8"));
    }
    let text = text.strip_prefix(['+', '-']).unwrap_or(text);
    text.strip_prefix('a')
        .is_some_and(|rest| matches!(number(rest), Some("" | "~")))
}

/// The first `a<n>` of a binding (Python `re.search(r'a\d+', token)`).
fn axis_of(token: &str) -> Option<&str> {
    token.match_indices('a').find_map(|(at, _)| {
        let rest = &token[at + 1..];
        let digits = rest
            .find(|c: char| !c.is_ascii_digit())
            .unwrap_or(rest.len());
        (digits > 0).then(|| &token[at..at + 1 + digits])
    })
}

fn is_step(key: &str) -> bool {
    STEPS.iter().any(|(step, _)| *step == key)
}

/// One saved line (Python `mapping_line`): `<guid>,<name>,<bindings in STEPS order>,platform:<p>,`.
/// The name loses its commas and control characters. With `require_cross` a line without Cross / A
/// is refused.
pub fn mapping_line(
    guid: &str,
    name: &str,
    bindings: &IndexMap<String, String>,
    platform: &str,
    require_cross: bool,
) -> Result<String, MappingError> {
    if !is_guid(guid) || (require_cross && bindings.get("a").is_none_or(String::is_empty)) {
        return Err(MappingError::Invalid(CROSS_FIRST));
    }
    if !PLATFORMS.contains(&platform) {
        return Err(MappingError::Invalid("Unsupported controller platform."));
    }
    if bindings
        .iter()
        .any(|(key, value)| !is_step(key) || !is_input(value))
    {
        return Err(MappingError::Invalid(INVALID_BINDING));
    }
    let clean: String = name
        .chars()
        .map(|c| if c == ',' || c <= '\u{1f}' { ' ' } else { c })
        .collect();
    let clean = match py_strip(&clean) {
        "" => "Custom controller",
        clean => clean,
    };
    let mut fields = vec![guid.to_ascii_lowercase(), clean.to_string()];
    fields.extend(
        STEPS
            .iter()
            .filter_map(|(key, _)| bindings.get(*key).map(|value| format!("{key}:{value}"))),
    );
    fields.extend([format!("platform:{platform}"), String::new()]);
    Ok(fields.join(","))
}

/// The device and platform a line is for (Python `mapping_key`): the lower case GUID and the value
/// of its `platform:` field (empty without one); `None` for a comment or any other line.
pub fn mapping_key(line: &str) -> Option<(String, String)> {
    let parts: Vec<&str> = py_strip(line).split(',').collect();
    if parts.len() < 3 || !is_guid(parts[0]) {
        return None;
    }
    let platform = parts[2..]
        .iter()
        .find_map(|part| part.strip_prefix("platform:"))
        .unwrap_or("");
    Some((parts[0].to_ascii_lowercase(), platform.to_string()))
}

/// The saved line of device `guid` (Python `load_mapping`): the last one for `platform`, else the
/// last one without a platform. A missing file has none; one that cannot be read is an error.
pub fn load_mapping(
    path: &Path,
    guid: &str,
    platform: &str,
) -> Result<Option<String>, MappingError> {
    if !path.is_file() {
        return Ok(None);
    }
    let text = read_text(path).map_err(at(path))?;
    let lines = splitlines(&text);
    let guid = guid.to_ascii_lowercase();
    Ok(
        [(guid.clone(), platform.to_string()), (guid, String::new())]
            .into_iter()
            .find_map(|key| {
                let found = lines
                    .iter()
                    .rev()
                    .find(|line| mapping_key(line).as_ref() == Some(&key));
                found.map(|line| py_strip(line).to_string())
            }),
    )
}

/// The inputs a line binds (Python `mapping_bindings`), by SDL name; the other fields (the GUID,
/// the name, `platform:`, `guide:` and whatever SDL adds) are left out.
pub fn mapping_bindings(line: Option<&str>) -> IndexMap<String, String> {
    let mut bindings = IndexMap::new();
    for field in line.unwrap_or("").split(',').skip(2) {
        if let Some((key, value)) = field.split_once(':') {
            if is_step(key) && is_input(value) {
                bindings.insert(key.to_string(), value.to_string());
            }
        }
    }
    bindings
}

/// `line` with only `target` changed (Python `edit_mapping`): bound to `token`, or unbound for
/// `None`. Every other field stays, SDL's own and unknown ones too; the GUID and name are this
/// device's, and `platform:` moves to the end. A line of another device is refused.
pub fn edit_mapping(
    line: Option<&str>,
    guid: &str,
    name: &str,
    platform: &str,
    target: &str,
    token: Option<&str>,
) -> Result<String, MappingError> {
    if !is_step(target) || token.is_some_and(|token| !is_input(token)) {
        return Err(MappingError::Invalid(INVALID_BINDING));
    }
    let header = mapping_line(guid, name, &IndexMap::new(), platform, false)?;
    let mut fields: Vec<String> = header.split(',').take(2).map(str::to_string).collect();
    if let Some(line) = line.filter(|line| !line.is_empty()) {
        let guid = guid.to_ascii_lowercase();
        let own = mapping_key(line)
            .is_some_and(|(id, saved)| id == guid && (saved == platform || saved.is_empty()));
        if !own {
            return Err(MappingError::Invalid(
                "The saved mapping belongs to another controller.",
            ));
        }
        let replaced = format!("{target}:");
        fields.extend(
            line.split(',')
                .skip(2)
                .filter(|part| {
                    !part.is_empty()
                        && !part.starts_with(&replaced)
                        && !part.starts_with("platform:")
                })
                .map(str::to_string),
        );
    }
    if let Some(token) = token {
        fields.push(format!("{target}:{token}"));
    }
    fields.extend([format!("platform:{platform}"), String::new()]);
    Ok(fields.join(","))
}

/// Saves `line` in the profiles file `path` (Python `save_mapping`): it replaces only the line of
/// the same device and platform; comments and the other lines stay. The new file is written beside
/// it and then moved over it, so a failure leaves the old one whole.
pub fn save_mapping(path: &Path, line: &str) -> Result<(), MappingError> {
    save_mapping_with(path, line, |temporary, path| {
        temporary.persist(path).map_err(|error| error.error)
    })
}

/// [`save_mapping`] moving the written file over `path` with `replace` (the tests make it fail).
/// The written file is removed when `replace` fails.
fn save_mapping_with(
    path: &Path,
    line: &str,
    replace: impl FnOnce(TempPath, &Path) -> io::Result<()>,
) -> Result<(), MappingError> {
    let key = mapping_key(line);
    if key.is_none() || line.contains(['\n', '\r']) {
        return Err(MappingError::Invalid("Invalid controller mapping."));
    }
    let old = if path.exists() {
        read_text(path).map_err(at(path))?
    } else {
        String::new()
    };
    let mut lines: Vec<&str> = splitlines(&old)
        .into_iter()
        .filter(|row| mapping_key(row) != key)
        .collect();
    lines.push(line);
    let folder = path
        .parent()
        .filter(|folder| !folder.as_os_str().is_empty())
        .unwrap_or(Path::new("."));
    fs::create_dir_all(folder).map_err(at(folder))?;
    let mut temporary = NamedTempFile::new_in(folder).map_err(at(folder))?;
    temporary
        .write_all((lines.join("\n") + "\n").as_bytes())
        .map_err(at(path))?;
    replace(temporary.into_temp_path(), path).map_err(at(path))
}

/// Hands the saved profiles to SDL (Python `configure_environment`). Without a
/// `gamecontrollerdb.txt` in `data_dir` nothing changes. With one, `SDL_GAMECONTROLLERCONFIG`
/// becomes its lines, then those of the file `SDL_GAMECONTROLLERCONFIG_FILE` names, then the
/// variable's own, without blanks, comments or repeats. A repeated line keeps its last place, so
/// an explicit mapping still wins on the same GUID and a restarted game gets the same value.
/// A relative `SDL_GAMECONTROLLERCONFIG_FILE` is in `port` (run.py had moved there).
/// Returns what to print for a file that could not be read (it counts as empty).
pub fn configure_environment(
    env: &mut IndexMap<String, String>,
    port: &Path,
    data_dir: &Path,
) -> Vec<String> {
    let path = data_dir.join(FILENAME);
    if !path.is_file() {
        return Vec::new();
    }
    let mut said = Vec::new();
    let mut read = |source: &Path| {
        read_text(source).unwrap_or_else(|error| {
            let shown = source.display();
            said.push(format!(
                "Controller mappings: could not read {shown}: {error}"
            ));
            String::new()
        })
    };
    let local = read(&path);
    let explicit = env
        .get("SDL_GAMECONTROLLERCONFIG_FILE")
        .filter(|file| !file.is_empty())
        .map(|file| port.join(file))
        .filter(|file| file.is_file())
        .map_or_else(String::new, |file| read(&file));
    let hint = env
        .get("SDL_GAMECONTROLLERCONFIG")
        .cloned()
        .unwrap_or_default();
    let rows: Vec<&str> = [local.as_str(), explicit.as_str(), hint.as_str()]
        .into_iter()
        .flat_map(splitlines)
        .map(py_strip)
        .filter(|row| !row.is_empty() && !row.starts_with('#'))
        .collect();
    // The last copy of each row, in their order.
    let mut kept = IndexSet::new();
    for row in rows.iter().rev() {
        kept.insert(*row);
    }
    let rows: Vec<&str> = kept.into_iter().rev().collect();
    if !rows.is_empty() {
        env.insert("SDL_GAMECONTROLLERCONFIG".to_string(), rows.join("\n"));
    }
    said
}

/// Python's `read_text(encoding='utf-8-sig')`: UTF-8, without its BOM.
fn read_text(path: &Path) -> io::Result<String> {
    let mut bytes = fs::read(path)?;
    if bytes.starts_with(b"\xEF\xBB\xBF") {
        bytes.drain(..3);
    }
    String::from_utf8(bytes).map_err(|error| io::Error::new(io::ErrorKind::InvalidData, error))
}

/// The line ends of Python's `str.splitlines()`, besides `\r\n`.
const LINE_BREAKS: &str = "\n\r\u{b}\u{c}\u{1c}\u{1d}\u{1e}\u{85}\u{2028}\u{2029}";

/// Python's `str.splitlines()`: the lines without their ends ([`LINE_BREAKS`] and `\r\n`).
fn splitlines(text: &str) -> Vec<&str> {
    let mut lines = Vec::new();
    let mut start = 0;
    let mut chars = text.char_indices().peekable();
    while let Some((at, c)) = chars.next() {
        if LINE_BREAKS.contains(c) {
            lines.push(&text[start..at]);
            start = at + c.len_utf8();
            if c == '\r' && chars.next_if(|&(_, next)| next == '\n').is_some() {
                start += 1;
            }
        }
    }
    if start < text.len() {
        lines.push(&text[start..]);
    }
    lines
}

/// Python's `str.strip()`: its whitespace also counts U+001C to U+001F.
fn py_strip(text: &str) -> &str {
    text.trim_matches(|c: char| c.is_whitespace() || ('\u{1c}'..='\u{1f}').contains(&c))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::PathBuf;

    const GUID: &str = "0300457e790000000600000000000000";

    fn map(pairs: &[(&str, &str)]) -> IndexMap<String, String> {
        pairs
            .iter()
            .map(|(key, value)| (key.to_string(), value.to_string()))
            .collect()
    }

    /// `CaptureTest.setUp`'s rest: 12 buttons, the fifth axis at its endpoint (a trigger), a hat.
    fn rest() -> State {
        State {
            buttons: vec![false; 12],
            axes: vec![0, 0, 0, 0, -32768, 0],
            hats: vec![0],
        }
    }

    fn with_axes(axes: [i16; 6]) -> State {
        State {
            axes: axes.to_vec(),
            ..rest()
        }
    }

    fn with_hat(hat: u8) -> State {
        State {
            hats: vec![hat],
            ..rest()
        }
    }

    fn with_button(button: usize) -> State {
        let mut state = rest();
        state.buttons[button] = true;
        state
    }

    /// `CaptureTest.held`: three polls of `state`, the last one's answer.
    fn held(capture: &mut Capture, state: &State) -> Option<String> {
        let mut last = None;
        for _ in 0..3 {
            last = capture.feed(state);
        }
        last
    }

    fn capture() -> Capture {
        Capture::new(&rest()).unwrap()
    }

    fn entries(folder: &Path) -> Vec<PathBuf> {
        fs::read_dir(folder)
            .unwrap()
            .map(|entry| entry.unwrap().path())
            .collect()
    }

    #[test]
    fn release_gate_stability_and_noise() {
        let mut capture = capture();
        let noise = with_axes([2000, -1000, 0, 0, -32768, 0]);
        assert_eq!(held(&mut capture, &noise), None);
        let pressed = with_button(1);
        assert_eq!(capture.feed(&pressed), None);
        // A brief press does not count.
        assert_eq!(capture.feed(&rest()), None);
        assert_eq!(held(&mut capture, &pressed).as_deref(), Some("b1"));
        assert_eq!(capture.index, 1);
        for _ in 0..8 {
            // The held button cannot be mapped twice.
            assert_eq!(capture.feed(&pressed), None);
        }
        held(&mut capture, &rest());
        assert!(!capture.waiting_release);
        assert_eq!(held(&mut capture, &pressed), None);
        assert!(capture.error);
        assert_eq!(capture.index, 1);
    }

    #[test]
    fn hats_diagonals_axes_triggers_and_inversion() {
        let mut capture = capture();
        capture.index = 12; // D-pad up
        assert_eq!(held(&mut capture, &with_hat(3)), None);
        assert_eq!(held(&mut capture, &with_hat(1)).as_deref(), Some("h0.1"));
        for (index, axes, expected) in [
            (16, [-32768, 0, 0, 0, -32768, 0], "a0~"),
            (17, [0, 32767, 0, 0, -32768, 0], "a1"),
            (6, [0, 0, 0, 0, 32767, 0], "a4"),
            (7, [0, 0, 0, 0, -32768, -32768], "-a5"),
        ] {
            let mut capture = Capture::new(&rest()).unwrap();
            capture.index = index;
            let state = with_axes(axes);
            assert_eq!(held(&mut capture, &state).as_deref(), Some(expected));
        }
    }

    #[test]
    fn stick_prompt_ignores_buttons_and_endpoint_trigger() {
        let mut capture = capture();
        capture.index = 16;
        let pressed = State {
            hats: vec![1],
            ..with_button(0)
        };
        assert_eq!(held(&mut capture, &pressed), None);
        let trigger = with_axes([0, 0, 0, 0, 32767, 0]);
        assert_eq!(held(&mut capture, &trigger), None);
    }

    #[test]
    fn full_axes_conflict_but_opposite_halves_can_be_shared() {
        let mut capture = capture();
        capture.index = 16;
        capture.bindings.insert("dpup".into(), "-a0".into());
        let state = with_axes([32767, 0, 0, 0, -32768, 0]);
        assert_eq!(held(&mut capture, &state), None);
        assert!(capture.error);
        capture.index = 7;
        capture.bindings = map(&[("lefttrigger", "+a5")]);
        capture.candidate = None;
        capture.frames = 0;
        let state = with_axes([0, 0, 0, 0, -32768, -32768]);
        assert_eq!(held(&mut capture, &state).as_deref(), Some("-a5"));
    }

    #[test]
    fn back_skip_and_cross_requirement() {
        let mut capture = capture();
        assert!(matches!(capture.skip(), Err(MappingError::Invalid(_))));
        capture.bindings = map(&[("a", "b0"), ("b", "b1")]);
        capture.index = 2;
        capture.back();
        assert_eq!(capture.bindings, map(&[("a", "b0")]));
        assert_eq!(capture.index, 1);
        capture.skip().unwrap();
        assert_eq!(capture.index, 2);
        capture.index = STEPS.len() - 1;
        capture.skip().unwrap();
        assert!(capture.done());
    }

    #[test]
    fn start_with_a_held_button_is_rejected() {
        let held = State {
            buttons: vec![true],
            axes: Vec::new(),
            hats: Vec::new(),
        };
        assert!(matches!(Capture::new(&held), Err(MappingError::Invalid(_))));
    }

    #[test]
    fn edit_one_input_preserves_other_inputs_and_unknown_sdl_fields() {
        let bindings = map(&[("a", "b2"), ("b", "b1"), ("leftx", "a0")]);
        let original = mapping_line(GUID, "USB", &bindings, "Windows", true).unwrap()
            + "guide:b10,type:standard,";
        let edited =
            edit_mapping(Some(&original), GUID, "USB", "Windows", "a", Some("b5")).unwrap();
        assert_eq!(
            mapping_bindings(Some(&edited)),
            map(&[("a", "b5"), ("b", "b1"), ("leftx", "a0")])
        );
        assert!(edited.contains("guide:b10,"), "{edited}");
        assert!(edited.contains("type:standard,"), "{edited}");
        assert_eq!(edited.matches("a:").count(), 1, "{edited}");
        let cleared = edit_mapping(Some(&edited), GUID, "USB", "Windows", "a", None).unwrap();
        assert_eq!(
            mapping_bindings(Some(&cleared)),
            map(&[("b", "b1"), ("leftx", "a0")])
        );
        assert!(cleared.contains("guide:b10,"), "{cleared}");
    }

    #[test]
    fn partial_profile_can_start_with_any_button() {
        let line = edit_mapping(None, GUID, "USB", "Windows", "x", Some("b3")).unwrap();
        assert_eq!(mapping_bindings(Some(&line)), map(&[("x", "b3")]));
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join(FILENAME);
        save_mapping(&path, &line).unwrap();
        assert_eq!(
            load_mapping(&path, GUID, "Windows").unwrap(),
            Some(line.clone())
        );
        assert_eq!(load_mapping(&path, GUID, "Linux").unwrap(), None);
        let edited = edit_mapping(Some(&line), GUID, "USB", "Windows", "b", Some("b1")).unwrap();
        save_mapping(&path, &edited).unwrap();
        assert_eq!(
            mapping_bindings(load_mapping(&path, GUID, "Windows").unwrap().as_deref()),
            map(&[("x", "b3"), ("b", "b1")])
        );
    }

    #[test]
    fn platform_specific_saved_mapping_wins_over_generic() {
        let generic = format!("{GUID},USB,a:b0,");
        let specific = mapping_line(GUID, "USB", &map(&[("a", "b2")]), "Windows", true).unwrap();
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join(FILENAME);
        fs::write(&path, format!("{specific}\n{generic}\n")).unwrap();
        assert_eq!(
            load_mapping(&path, GUID, "Windows").unwrap(),
            Some(specific)
        );
        assert_eq!(load_mapping(&path, GUID, "Linux").unwrap(), Some(generic));
    }

    #[test]
    fn replace_only_this_guid_and_platform_preserving_comments() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join(FILENAME);
        let windows =
            mapping_line(GUID, "USB,\npad", &map(&[("a", "b2")]), "Windows", true).unwrap();
        let linux = mapping_line(GUID, "USB pad", &map(&[("a", "b3")]), "Linux", true).unwrap();
        fs::write(&path, format!("# user note\n{linux}\n")).unwrap();
        save_mapping(&path, &windows).unwrap();
        let bindings = map(&[("a", "b1"), ("leftx", "a0~")]);
        let replacement = mapping_line(GUID, "USB pad", &bindings, "Windows", true).unwrap();
        save_mapping(&path, &replacement).unwrap();
        let text = fs::read_to_string(&path).unwrap();
        assert!(text.contains("# user note\n"), "{text}");
        assert!(text.contains(&linux), "{text}");
        assert!(text.contains(&replacement), "{text}");
        assert!(!text.contains(&windows), "{text}");
        assert_eq!(entries(dir.path()).len(), 1);
    }

    #[test]
    fn failed_commit_preserves_profile_and_cleans_temporary_file() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join(FILENAME);
        fs::write(&path, "old profile\n").unwrap();
        let line = mapping_line(GUID, "USB", &map(&[("a", "b2")]), "Windows", true).unwrap();
        // Python patches `os.replace` to raise PermissionError.
        let error = save_mapping_with(&path, &line, |_, _| {
            Err(io::Error::from(io::ErrorKind::PermissionDenied))
        })
        .unwrap_err();
        assert!(
            matches!(&error, MappingError::Io { path: failed, error }
                if *failed == path && error.kind() == io::ErrorKind::PermissionDenied),
            "{error:?}"
        );
        // The text names the file, for the mapper's error dialog (only a load failure, in `choose`,
        // reaches its status line); the reason is not also a source.
        assert!(
            error
                .to_string()
                .starts_with(&format!("{}: ", path.display())),
            "{error}"
        );
        assert!(std::error::Error::source(&error).is_none());
        assert_eq!(fs::read_to_string(&path).unwrap(), "old profile\n");
        assert_eq!(entries(dir.path()), [path]);
    }

    #[test]
    fn invalid_mapping_does_not_write() {
        for (guid, bindings) in [
            ("invalid", map(&[("a", "b0")])),
            (GUID, map(&[])),
            (GUID, map(&[("a", "b0\n")])),
        ] {
            let line = mapping_line(guid, "USB", &bindings, "Windows", true);
            assert!(matches!(line, Err(MappingError::Invalid(_))), "{line:?}");
        }
    }

    #[test]
    fn explicit_overrides_have_priority_and_restarts_do_not_duplicate() {
        let dir = tempfile::tempdir().unwrap();
        let local = mapping_line(GUID, "USB", &map(&[("a", "b0")]), "Windows", true).unwrap();
        let external_file =
            mapping_line(GUID, "USB", &map(&[("a", "b1")]), "Windows", true).unwrap();
        let external_hint =
            mapping_line(GUID, "USB", &map(&[("a", "b2")]), "Windows", true).unwrap();
        fs::write(dir.path().join(FILENAME), format!("{local}\n")).unwrap();
        let custom = dir.path().join("custom.txt");
        fs::write(&custom, &external_file).unwrap();
        let custom = custom.to_string_lossy().into_owned();
        let mut env = map(&[
            ("SDL_GAMECONTROLLERCONFIG_FILE", custom.as_str()),
            ("SDL_GAMECONTROLLERCONFIG", external_hint.as_str()),
        ]);
        configure_environment(&mut env, dir.path(), dir.path());
        assert_eq!(
            env["SDL_GAMECONTROLLERCONFIG"].lines().collect::<Vec<_>>(),
            [
                local.as_str(),
                external_file.as_str(),
                external_hint.as_str()
            ]
        );
        let before = env.clone();
        configure_environment(&mut env, dir.path(), dir.path());
        assert_eq!(env, before);
        assert_eq!(env["SDL_GAMECONTROLLERCONFIG_FILE"], custom);
    }

    #[test]
    fn no_profile_leaves_environment_untouched() {
        let dir = tempfile::tempdir().unwrap();
        let mut env = map(&[("SDL_GAMECONTROLLERCONFIG", "user value")]);
        configure_environment(&mut env, dir.path(), dir.path());
        assert_eq!(env, map(&[("SDL_GAMECONTROLLERCONFIG", "user value")]));
    }

    #[test]
    fn reserved_xinput_override_is_not_discarded() {
        let dir = tempfile::tempdir().unwrap();
        let local = mapping_line(GUID, "USB", &map(&[("a", "b0")]), "Windows", true).unwrap();
        fs::write(dir.path().join(FILENAME), &local).unwrap();
        let custom = "xinput,Custom Xbox,a:b1,b:b0,platform:Windows,";
        let mut env = map(&[("SDL_GAMECONTROLLERCONFIG", custom)]);
        configure_environment(&mut env, dir.path(), dir.path());
        assert_eq!(
            env["SDL_GAMECONTROLLERCONFIG"].lines().collect::<Vec<_>>(),
            [local.as_str(), custom]
        );
    }

    #[test]
    fn an_unreadable_profile_is_reported_and_skipped() {
        // Python prints the error and goes on without the file.
        let dir = tempfile::tempdir().unwrap();
        fs::write(dir.path().join(FILENAME), b"\xff\xfe not UTF-8").unwrap();
        let custom = "xinput,Custom Xbox,a:b1,b:b0,platform:Windows,";
        let mut env = map(&[("SDL_GAMECONTROLLERCONFIG", custom)]);
        let said = configure_environment(&mut env, dir.path(), dir.path());
        assert_eq!(env, map(&[("SDL_GAMECONTROLLERCONFIG", custom)]));
        assert_eq!(said.len(), 1, "{said:?}");
        let shown = dir.path().join(FILENAME);
        let prefix = format!("Controller mappings: could not read {}: ", shown.display());
        assert!(said[0].starts_with(&prefix), "{said:?}");
    }

    #[test]
    fn a_bom_and_python_line_breaks_are_read_as_python_does() {
        // `utf-8-sig` and `str.splitlines()`.
        assert_eq!(
            splitlines("a\rb\r\nc\u{b}d\u{1e}e\u{85}f\u{2028}g\n\nh\n"),
            ["a", "b", "c", "d", "e", "f", "g", "", "h"]
        );
        assert!(splitlines("").is_empty());
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join(FILENAME);
        let line = mapping_line(GUID, "USB", &map(&[("a", "b2")]), "Windows", true).unwrap();
        fs::write(&path, format!("\u{feff}{line}\r# note")).unwrap();
        assert_eq!(load_mapping(&path, GUID, "Windows").unwrap(), Some(line));
    }

    #[test]
    fn an_unreadable_saved_mapping_is_an_error() {
        // Python's `read_text` raises, and the mapper shows the error.
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join(FILENAME);
        assert_eq!(load_mapping(&path, GUID, "Windows").unwrap(), None);
        fs::write(&path, b"\xff\xfe not UTF-8").unwrap();
        let error = load_mapping(&path, GUID, "Windows").unwrap_err();
        assert!(
            matches!(&error, MappingError::Io { path: failed, error }
                if *failed == path && error.kind() == io::ErrorKind::InvalidData),
            "{error:?}"
        );
        assert!(
            error
                .to_string()
                .starts_with(&format!("{}: ", path.display())),
            "{error}"
        );
    }

    #[test]
    fn a_relative_explicit_file_is_read_from_the_port() {
        // run.py had moved to the port folder, so a relative SDL_GAMECONTROLLERCONFIG_FILE was
        // the port's.
        let port = tempfile::tempdir().unwrap();
        let data = tempfile::tempdir().unwrap();
        let local = mapping_line(GUID, "USB", &map(&[("a", "b0")]), "Windows", true).unwrap();
        let external = mapping_line(GUID, "USB", &map(&[("a", "b1")]), "Windows", true).unwrap();
        fs::write(data.path().join(FILENAME), &local).unwrap();
        fs::write(port.path().join("custom.txt"), &external).unwrap();
        let mut env = map(&[("SDL_GAMECONTROLLERCONFIG_FILE", "custom.txt")]);
        let said = configure_environment(&mut env, port.path(), data.path());
        assert!(said.is_empty(), "{said:?}");
        assert_eq!(
            env["SDL_GAMECONTROLLERCONFIG"].lines().collect::<Vec<_>>(),
            [local.as_str(), external.as_str()]
        );
    }
}
