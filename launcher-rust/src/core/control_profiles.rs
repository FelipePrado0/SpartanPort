//! Control profiles: named sets of keyboard, gamepad and dead zone choices. The saved ones live in
//! the data folder's `control-profiles.json`; one can be exported to a file and imported on another
//! PC. A file is checked whole before anything of it is used.

use std::fs;
use std::path::Path;

use indexmap::IndexMap;
use serde_json::{json, Map, Value};
use thiserror::Error;

use crate::core::controls::{table_rows, Kind, PAD_STYLES};
use crate::core::fs::atomic_write;
use crate::core::pad_model::MAX_DEADZONE;

/// The saved profiles, in the data folder.
pub const FILENAME: &str = "control-profiles.json";
const VERSION: u64 = 1;
/// What marks an exported profile.
const FORMAT: &str = "spartanport-controls";
/// No real profile comes near this; a bigger file is not one.
const MAX_FILE: u64 = 64 * 1024;
const MAX_NAME: usize = 40;
const MAX_INPUT: usize = 32;

/// The English names of the built-in profiles, which are also their translation keys.
pub const DEFAULT_PROFILE: &str = "Default";
pub const MOUSE_SIDE_BUTTONS: &str = "Mouse with side buttons";
pub const BUILT_IN: [&str; 2] = [DEFAULT_PROFILE, MOUSE_SIDE_BUTTONS];

#[derive(Debug, Error)]
pub enum ProfileError {
    #[error("Could not read the profile file.")]
    Read,
    #[error("Could not save the profile file.")]
    Write,
    #[error("This file is not a valid SpartanPort control profile.")]
    Invalid,
    #[error("Enter a profile name of up to 40 characters.")]
    Name,
}

/// The inputs that differ from the defaults, and the two dead zones.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Profile {
    pub key: IndexMap<String, String>,
    /// Alternative keyboard inputs by game button (`controls.key_alt`).
    pub key_alt: IndexMap<String, String>,
    /// Second alternative keyboard inputs by game button (`controls.key_alt2`).
    pub key_alt2: IndexMap<String, String>,
    pub pad: IndexMap<String, String>,
    pub deadzone_left: u32,
    pub deadzone_right: u32,
}

/// A built-in profile, by its English name. The mouse layout is the one proposed in #616: block
/// and dodge under the thumb, so L1 combos stay on the mouse and Q stays free for magic.
pub fn built_in(name: &str) -> Option<Profile> {
    let mut profile = Profile::default();
    match name {
        DEFAULT_PROFILE => {}
        MOUSE_SIDE_BUTTONS => {
            for (button, input) in [
                ("l1", "Mouse Back"),
                ("dodge", "Mouse Forward"),
                ("r2", "Q"),
                ("l2", "Left Ctrl"),
            ] {
                profile.key.insert(button.to_string(), input.to_string());
            }
        }
        _ => return None,
    }
    Some(profile)
}

fn to_json(profile: &Profile) -> Map<String, Value> {
    let mut out = Map::new();
    out.insert("key".to_string(), json!(profile.key));
    if !profile.key_alt.is_empty() {
        out.insert("key_alt".to_string(), json!(profile.key_alt));
    }
    if !profile.key_alt2.is_empty() {
        out.insert("key_alt2".to_string(), json!(profile.key_alt2));
    }
    out.insert("pad".to_string(), json!(profile.pad));
    out.insert("deadzone_left".to_string(), json!(profile.deadzone_left));
    out.insert("deadzone_right".to_string(), json!(profile.deadzone_right));
    out
}

fn table(value: &Value, kind: Kind) -> Option<IndexMap<String, String>> {
    let rows = table_rows(kind);
    let mut out = IndexMap::new();
    for (button, input) in value.as_object()? {
        let input = input.as_str()?;
        let known = rows.iter().any(|row| row.button == button);
        let clean = !input.trim().is_empty()
            && input.chars().count() <= MAX_INPUT
            && !input.contains([',', '=', '|'])
            && !input.chars().any(char::is_control);
        let usable = kind == Kind::Key
            || input.eq_ignore_ascii_case("None")
            || PAD_STYLES
                .iter()
                .any(|(_, labels)| labels.iter().any(|(name, _)| *name == input));
        if !known || !clean || !usable {
            return None;
        }
        out.insert(button.clone(), input.to_string());
    }
    Some(out)
}

fn deadzone(value: &Value) -> Option<u32> {
    u32::try_from(value.as_u64()?)
        .ok()
        .filter(|percent| *percent <= MAX_DEADZONE)
}

/// An optional table of alternative inputs; profiles from before it have none.
fn alternatives(value: &Value, field: &str) -> Option<IndexMap<String, String>> {
    value
        .get(field)
        .map_or_else(|| Some(IndexMap::new()), |saved| table(saved, Kind::Key))
}

fn from_json(value: &Value) -> Option<Profile> {
    Some(Profile {
        key: table(value.get("key")?, Kind::Key)?,
        key_alt: alternatives(value, "key_alt")?,
        key_alt2: alternatives(value, "key_alt2")?,
        pad: table(value.get("pad")?, Kind::Pad)?,
        deadzone_left: deadzone(value.get("deadzone_left")?)?,
        deadzone_right: deadzone(value.get("deadzone_right")?)?,
    })
}

/// A usable profile name: trimmed, short, printable and not a built-in's.
pub fn clean_name(name: &str) -> Result<String, ProfileError> {
    let name = name.trim();
    let taken = BUILT_IN
        .iter()
        .any(|known| known.eq_ignore_ascii_case(name));
    if name.is_empty()
        || name.chars().count() > MAX_NAME
        || name.chars().any(char::is_control)
        || taken
    {
        return Err(ProfileError::Name);
    }
    Ok(name.to_string())
}

fn read_limited(path: &Path) -> Result<Vec<u8>, ProfileError> {
    let size = fs::metadata(path).map_err(|_| ProfileError::Read)?.len();
    if size > MAX_FILE {
        return Err(ProfileError::Invalid);
    }
    fs::read(path).map_err(|_| ProfileError::Read)
}

/// The saved profiles; a missing file has none. A damaged one is an error, so a later save does
/// not replace it unseen.
pub fn load(path: &Path) -> Result<IndexMap<String, Profile>, ProfileError> {
    if !path.exists() {
        return Ok(IndexMap::new());
    }
    let bytes = read_limited(path)?;
    let value: Value = serde_json::from_slice(&bytes).map_err(|_| ProfileError::Invalid)?;
    let saved = value
        .get("profiles")
        .and_then(Value::as_object)
        .ok_or(ProfileError::Invalid)?;
    saved
        .iter()
        .map(|(name, profile)| {
            let name = clean_name(name)?;
            Ok((name, from_json(profile).ok_or(ProfileError::Invalid)?))
        })
        .collect()
}

pub fn save(path: &Path, profiles: &IndexMap<String, Profile>) -> Result<(), ProfileError> {
    let saved: Map<String, Value> = profiles
        .iter()
        .map(|(name, profile)| (name.clone(), Value::Object(to_json(profile))))
        .collect();
    let text = serde_json::to_vec_pretty(&json!({"version": VERSION, "profiles": saved}))
        .map_err(|_| ProfileError::Write)?;
    atomic_write(path, &text).map_err(|_| ProfileError::Write)
}

/// Writes one profile as a file for another PC.
pub fn export(path: &Path, name: &str, profile: &Profile) -> Result<(), ProfileError> {
    let mut out = to_json(profile);
    out.insert("format".to_string(), json!(FORMAT));
    out.insert("version".to_string(), json!(VERSION));
    out.insert("name".to_string(), json!(name));
    let text = serde_json::to_vec_pretty(&out).map_err(|_| ProfileError::Write)?;
    atomic_write(path, &text).map_err(|_| ProfileError::Write)
}

/// The name and profile of an exported file. Anything wrong in it refuses the whole file.
pub fn import(path: &Path) -> Result<(String, Profile), ProfileError> {
    let bytes = read_limited(path)?;
    let value: Value = serde_json::from_slice(&bytes).map_err(|_| ProfileError::Invalid)?;
    let marked = value.get("format").and_then(Value::as_str) == Some(FORMAT)
        && value.get("version").and_then(Value::as_u64) == Some(VERSION);
    if !marked {
        return Err(ProfileError::Invalid);
    }
    let name = value
        .get("name")
        .and_then(Value::as_str)
        .ok_or(ProfileError::Invalid)?;
    let name = clean_name(name).map_err(|_| ProfileError::Invalid)?;
    let profile = from_json(&value).ok_or(ProfileError::Invalid)?;
    Ok((name, profile))
}

#[cfg(test)]
mod tests {
    #![allow(clippy::unwrap_used)]

    use super::*;
    use crate::core::controls::{bind_key, control_changes, default_input, table_rows, Kind, Slot};

    fn sample() -> Profile {
        let mut profile = built_in(MOUSE_SIDE_BUTTONS).unwrap();
        profile.pad.insert("l1".into(), "lefttrigger".into());
        profile.pad.insert("l2".into(), "leftshoulder".into());
        profile.key_alt.insert("cross".into(), "K".into());
        profile.key_alt2.insert("cross".into(), "Keypad 2".into());
        profile.deadzone_left = 7;
        profile
    }

    #[test]
    fn saved_profiles_come_back_the_same() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join(FILENAME);
        assert!(load(&path).unwrap().is_empty());
        let profiles = IndexMap::from([("Mine".to_string(), sample())]);
        save(&path, &profiles).unwrap();
        assert_eq!(load(&path).unwrap(), profiles);
    }

    #[test]
    fn a_damaged_file_is_an_error_and_not_an_empty_list() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join(FILENAME);
        fs::write(&path, "{oops").unwrap();
        assert!(matches!(load(&path), Err(ProfileError::Invalid)));
    }

    #[test]
    fn an_exported_profile_imports_on_another_pc() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("mine.json");
        export(&path, "Mine", &sample()).unwrap();
        assert_eq!(import(&path).unwrap(), ("Mine".to_string(), sample()));
    }

    #[test]
    fn a_bad_file_is_refused_whole() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("bad.json");
        let base = json!({
            "format": FORMAT, "version": 1, "name": "X", "key": {"cross": "Q"}, "pad": {},
            "deadzone_left": 0, "deadzone_right": 0,
        });
        let cases: [(&str, Value); 9] = [
            ("format", json!("other")),
            ("version", json!(2)),
            ("name", json!("")),
            ("key", json!({"nothing": "Q"})),
            ("key", json!({"cross": "Q,square=E"})),
            ("key_alt2", json!({"cross": "Q|E"})),
            ("pad", json!({"cross": "not-a-button"})),
            ("deadzone_left", json!(51)),
            ("deadzone_right", json!(-1)),
        ];
        for (field, bad) in cases {
            let mut value = base.clone();
            value[field] = bad;
            fs::write(&path, serde_json::to_vec(&value).unwrap()).unwrap();
            assert!(
                matches!(import(&path), Err(ProfileError::Invalid)),
                "{field}"
            );
        }
        fs::write(&path, vec![b' '; MAX_FILE as usize + 1]).unwrap();
        assert!(matches!(import(&path), Err(ProfileError::Invalid)));
        fs::write(&path, b"not json").unwrap();
        assert!(matches!(import(&path), Err(ProfileError::Invalid)));
    }

    #[test]
    fn names_are_checked() {
        assert_eq!(clean_name("  Mine ").unwrap(), "Mine");
        assert!(clean_name("  ").is_err());
        assert!(clean_name("default").is_err());
        assert!(clean_name(&"x".repeat(41)).is_err());
        assert!(clean_name("a\nb").is_err());
    }

    /// The mouse layout on top of the defaults: no input presses two rows, so the player never
    /// finds a conflict the preset made.
    #[test]
    fn the_mouse_layout_leaves_no_input_on_two_rows() {
        let mut values: IndexMap<String, String> = table_rows(Kind::Key)
            .into_iter()
            .map(|row| {
                let input = default_input(row, Kind::Key).unwrap_or_default();
                (row.button.to_string(), input.to_string())
            })
            .collect();
        let preset = built_in(MOUSE_SIDE_BUTTONS).unwrap();
        for (button, input) in &preset.key {
            values.insert(button.clone(), input.clone());
        }
        let mut seen: Vec<String> = values.values().map(|v| v.to_lowercase()).collect();
        seen.sort();
        let total = seen.len();
        seen.dedup();
        assert_eq!(seen.len(), total, "{values:?}");
        assert_eq!(control_changes(&values, Kind::Key), preset.key);
        // Still editable row by row.
        let mut alternatives = IndexMap::new();
        assert_eq!(
            bind_key(
                [&mut values, &mut alternatives, &mut IndexMap::new()],
                "l1",
                Slot::Main,
                "G"
            ),
            None
        );
    }

    #[test]
    fn built_ins_are_known_by_name_only() {
        assert_eq!(built_in(DEFAULT_PROFILE), Some(Profile::default()));
        assert_eq!(built_in("Mine"), None);
    }
}
