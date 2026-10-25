#![allow(clippy::unwrap_used, clippy::expect_used)]
//! The SDL3 adapter on the real library, with virtual joysticks (Python
//! `tests/test_controller_mapper_sdl.py`): no pad and no display are needed. Set
//! `SPARTAN_TEST_SDL3` to SDL3's path (`SDL3.dll` or `libSDL3.so`); without it each SDL test says
//! so and passes.

use std::ffi::OsString;
use std::fs;
use std::path::PathBuf;
use std::sync::{Mutex, MutexGuard};

use indexmap::IndexMap;
use spartanport_launcher::core::controller_mappings::{
    edit_mapping, load_mapping, mapping_bindings, mapping_line, save_mapping, Capture, FILENAME,
    STEPS,
};
use spartanport_launcher::core::joysticks::{
    Device, JoyError, Joysticks, Sdl3, VirtualInput, VirtualJoystick,
};
use tempfile::TempDir;

#[path = "common/joysticks.rs"]
mod joysticks;
use joysticks::{pad, rest, FakeJoysticks};

/// SDL's hints, subsystems and virtual joysticks belong to the process: one SDL test at a time.
static SDL: Mutex<()> = Mutex::new(());

/// The variables the tests clear, as the Python's `setUp` does.
const SDL_VARIABLES: [&str; 2] = ["SDL_GAMECONTROLLERCONFIG", "SDL_GAMECONTROLLERCONFIG_FILE"];

fn map(pairs: &[(&str, &str)]) -> IndexMap<String, String> {
    pairs
        .iter()
        .map(|(key, value)| (key.to_string(), value.to_string()))
        .collect()
}

/// The Python's `setUp`: an adapter on a fresh data folder, with a virtual joystick attached and
/// selected. Its fields drop in order: SDL first, the folder and the lock last.
struct Rig {
    sdl: Option<Sdl3>,
    ident: u32,
    device: Device,
    library: PathBuf,
    env: Vec<(&'static str, Option<OsString>)>,
    dir: TempDir,
    _lock: MutexGuard<'static, ()>,
}

impl Rig {
    /// `None` (and a note) without `SPARTAN_TEST_SDL3`.
    fn new() -> Option<Rig> {
        let Some(library) = std::env::var_os("SPARTAN_TEST_SDL3").filter(|path| !path.is_empty())
        else {
            println!("skipped: set SPARTAN_TEST_SDL3");
            return None;
        };
        let lock = SDL.lock().unwrap_or_else(|poisoned| poisoned.into_inner());
        let env = SDL_VARIABLES
            .iter()
            .map(|key| (*key, std::env::var_os(key)))
            .collect();
        for key in SDL_VARIABLES {
            std::env::remove_var(key);
        }
        let mut rig = Rig {
            sdl: None,
            ident: 0,
            device: pad(0, ""),
            library: library.into(),
            env,
            dir: tempfile::tempdir().unwrap(),
            _lock: lock,
        };
        rig.reopen();
        Some(rig)
    }

    fn sdl(&mut self) -> &mut Sdl3 {
        self.sdl.as_mut().unwrap()
    }

    /// The Python's `backend.close()`, `new_backend()` and `attach()`: the old adapter is closed
    /// before the new one starts SDL, so SDL reads the hints again.
    fn reopen(&mut self) {
        if let Some(mut sdl) = self.sdl.take() {
            sdl.detach_virtual(self.ident).unwrap();
        }
        let sdl = Sdl3::open(self.dir.path(), self.dir.path(), Some(&self.library)).unwrap();
        let sdl = self.sdl.insert(sdl);
        let joystick = VirtualJoystick {
            kind: 4,
            vendor: 0xfaca,
            product: 0xf001,
            axes: 6,
            buttons: 12,
            hats: 1,
            name: "GOW3 mapper test".to_string(),
        };
        self.ident = sdl.attach_virtual(&joystick).unwrap();
        assert_ne!(self.ident, 0);
        let device = sdl
            .devices()
            .unwrap()
            .into_iter()
            .find(|device| device.id == self.ident)
            .unwrap();
        assert_eq!(device.name, "GOW3 mapper test");
        assert_eq!((device.buttons, device.axes, device.hats), (12, 6, 1));
        sdl.select(&device).unwrap();
        self.device = device;
    }

    fn set(&mut self, input: VirtualInput) {
        self.sdl().set_virtual(input).unwrap();
    }

    fn pressed(&mut self) -> Vec<&'static str> {
        self.sdl().preview_state().unwrap().0
    }

    fn profiles(&self) -> PathBuf {
        self.dir.path().join(FILENAME)
    }
}

impl Drop for Rig {
    fn drop(&mut self) {
        self.sdl.take();
        for (key, value) in &self.env {
            match value {
                Some(value) => std::env::set_var(key, value),
                None => std::env::remove_var(key),
            }
        }
    }
}

#[test]
fn raw_capture_gamepad_preview_inversion_and_analog_trigger() {
    let Some(mut rig) = Rig::new() else { return };
    let device = rig.device.clone();
    assert!(!device.mapped); // An unrecognized raw joystick.
    rig.set(VirtualInput::Axis(4, -32768));
    let mut capture = Capture::new(&rig.sdl().state().unwrap()).unwrap();
    rig.set(VirtualInput::Button(7, true));
    let fed: Vec<Option<String>> = (0..3)
        .map(|_| capture.feed(&rig.sdl().state().unwrap()))
        .collect();
    assert_eq!(fed[2].as_deref(), Some("b7"));
    let mut bindings = capture.bindings.clone();
    bindings.extend(map(&[
        ("leftx", "a0~"),
        ("lefttrigger", "a4"),
        ("dpup", "h0.1"),
    ]));
    let platform = rig.sdl().platform().to_string();
    let line = mapping_line(&device.guid, &device.name, &bindings, &platform, true).unwrap();
    rig.sdl().begin_preview(&line).unwrap();
    let (buttons, axes) = rig.sdl().preview_state().unwrap();
    assert!(buttons.contains(&"Cross / A"), "{buttons:?}");
    assert_eq!(axes[4], 0);
    rig.set(VirtualInput::Button(7, false));
    rig.set(VirtualInput::Axis(0, -32768));
    rig.set(VirtualInput::Axis(4, 32767));
    rig.set(VirtualInput::Hat(0, 1));
    let (buttons, axes) = rig.sdl().preview_state().unwrap();
    assert!(!buttons.contains(&"Cross / A"), "{buttons:?}");
    assert!(buttons.contains(&"D-pad up"), "{buttons:?}");
    assert_eq!(axes[0], 32767);
    assert_eq!(axes[4], 32767);
}

#[test]
fn saved_mapping_loads_and_explicit_user_file_wins() {
    let Some(mut rig) = Rig::new() else { return };
    let device = rig.device.clone();
    let platform = rig.sdl().platform().to_string();
    let saved = mapping_line(
        &device.guid,
        &device.name,
        &map(&[("a", "b7")]),
        &platform,
        true,
    );
    save_mapping(&rig.profiles(), &saved.unwrap()).unwrap();
    rig.reopen();
    // SDL learned the saved profile from the hint: the pad is a gamepad now. The preview opens it
    // with SDL's own line, as the Python opens the gamepad directly.
    assert!(rig.device.mapped);
    let line = rig.sdl().current_mapping().unwrap();
    rig.sdl().begin_preview(&line).unwrap();
    rig.set(VirtualInput::Button(7, true));
    assert!(rig.pressed().contains(&"Cross / A"));
    let custom = rig.dir.path().join("custom.txt");
    let explicit = mapping_line(
        &device.guid,
        &device.name,
        &map(&[("a", "b3")]),
        &platform,
        true,
    );
    fs::write(&custom, explicit.unwrap()).unwrap();
    std::env::set_var("SDL_GAMECONTROLLERCONFIG_FILE", &custom);
    rig.reopen();
    assert!(rig.device.mapped);
    let line = rig.sdl().current_mapping().unwrap();
    rig.sdl().begin_preview(&line).unwrap();
    rig.set(VirtualInput::Button(7, true));
    assert!(!rig.pressed().contains(&"Cross / A"));
    rig.set(VirtualInput::Button(3, true));
    assert!(rig.pressed().contains(&"Cross / A"));
}

#[test]
fn disconnect_reports_error_instead_of_reading_a_stale_pad() {
    let Some(mut rig) = Rig::new() else { return };
    let ident = rig.ident;
    rig.sdl().detach_virtual(ident).unwrap();
    let error = rig.sdl().state().unwrap_err();
    assert!(matches!(error, JoyError::Disconnected), "{error:?}");
    assert!(error.to_string().contains("disconnected"), "{error}");
}

#[test]
fn editing_an_existing_sdl_mapping_preserves_guide_and_other_buttons() {
    let Some(mut rig) = Rig::new() else { return };
    let device = rig.device.clone();
    let platform = rig.sdl().platform().to_string();
    let path = rig.profiles();
    let line = mapping_line(&device.guid, "USB", &map(&[("a", "b7")]), &platform, true).unwrap()
        + "guide:b10,";
    rig.sdl().begin_preview(&line).unwrap();
    // The page's `choose`: the saved profile, else SDL's own line.
    let profile = load_mapping(&path, &device.guid, &platform)
        .unwrap()
        .or_else(|| rig.sdl().current_mapping());
    let bindings = mapping_bindings(profile.as_deref());
    assert_eq!(bindings["a"], "b7");
    // `begin_edit("b")`, then three polls with button 2 held (the Python's `capture_button`).
    let mut capture = Capture::new(&rig.sdl().state().unwrap()).unwrap();
    capture.index = STEPS.iter().position(|(key, _)| *key == "b").unwrap();
    capture.bindings = bindings.into_iter().filter(|(key, _)| key != "b").collect();
    rig.sdl().end_preview();
    rig.set(VirtualInput::Button(2, true));
    let token = (0..3)
        .map(|_| capture.feed(&rig.sdl().state().unwrap()))
        .last()
        .flatten();
    assert_eq!(token.as_deref(), Some("b2"));
    // `commit`: the saved profile again, edited in one field, checked by SDL, then saved.
    let previous = load_mapping(&path, &device.guid, &platform)
        .unwrap()
        .or(profile);
    let edited = edit_mapping(
        previous.as_deref(),
        &device.guid,
        &device.name,
        &platform,
        "b",
        token.as_deref(),
    )
    .unwrap();
    rig.sdl().begin_preview(&edited).unwrap();
    save_mapping(&path, &edited).unwrap();
    let text = fs::read_to_string(&path).unwrap();
    assert!(text.contains("a:b7,"), "{text}");
    assert!(text.contains("b:b2,"), "{text}");
    assert!(text.contains("guide:b10,"), "{text}");
}

#[test]
fn opening_again_after_a_close_still_reads_the_pad() {
    // The page can open and close SDL many times in a session; SDL3 stays loaded throughout.
    let Some(mut rig) = Rig::new() else { return };
    for _ in 0..3 {
        rig.reopen();
        rig.set(VirtualInput::Button(5, true));
        assert!(rig.sdl().state().unwrap().buttons[5]);
    }
}

#[test]
fn an_unreadable_profile_is_a_warning_of_opening() {
    // The Python printed it to a console the windowed launcher does not have: the mapper page
    // shows it instead.
    let Some(mut rig) = Rig::new() else { return };
    assert_eq!(rig.sdl().warnings(), Vec::<String>::new());
    let profiles = rig.profiles();
    fs::write(&profiles, b"\xff\xfe not UTF-8").unwrap();
    rig.reopen();
    let warnings = rig.sdl().warnings();
    assert_eq!(warnings.len(), 1, "{warnings:?}");
    let prefix = format!(
        "Controller mappings: could not read {}: ",
        profiles.display()
    );
    assert!(warnings[0].starts_with(&prefix), "{warnings:?}");
}

#[test]
fn the_fake_answers_like_the_sdl_adapter() {
    // The UI tests' stand-in: the same errors as SDL, its script in order, then rest.
    let mut fake = FakeJoysticks::new("Windows", vec![pad(7, "03000000faca0000f001000000000000")]);
    assert!(matches!(fake.state(), Err(JoyError::Disconnected)));
    let device = fake.devices().unwrap().remove(0);
    fake.select(&device).unwrap();
    // Only the launcher closes the device: choosing one is not a close.
    assert_eq!(fake.script().closed, 0);
    let mut pressed = rest(&device);
    pressed.buttons[7] = true;
    fake.script().states.push_back(pressed.clone());
    assert_eq!(fake.state().unwrap(), pressed);
    assert_eq!(fake.state().unwrap(), rest(&device));
    assert_eq!(fake.current_mapping(), None);
    fake.script().mapping = Some("line".to_string());
    assert_eq!(fake.current_mapping().as_deref(), Some("line"));
    fake.script().preview_error = Some("Couldn't parse mapping".to_string());
    let error = fake.begin_preview("bad").unwrap_err();
    assert_eq!(error.to_string(), "Couldn't parse mapping");
    assert_eq!(fake.preview_state(), None);
    fake.script().preview_error = None;
    fake.begin_preview("good").unwrap();
    assert_eq!(fake.preview_state(), Some((Vec::new(), [0; 6])));
    // Choosing again ends the preview, as SDL's adapter does, and still closes nothing.
    fake.select(&device).unwrap();
    assert_eq!(fake.preview_state(), None);
    assert_eq!(fake.script().closed, 0);
    fake.begin_preview("good").unwrap();
    fake.script().unplugged = true;
    let error = fake.state().unwrap_err();
    assert_eq!(
        error.to_string(),
        "Controller disconnected. Reconnect it and choose Refresh."
    );
    fake.close_device();
    assert_eq!(fake.preview_state(), None);
    assert_eq!(fake.script().closed, 1);
    assert_eq!(fake.script().selected, None);
    assert_eq!(fake.script().previewed, ["bad", "good", "good"]);
    assert_eq!(fake.platform(), "Windows");
    assert!(fake.warnings().is_empty());
    fake.script().warnings = vec!["Controller mappings: could not read x: y".to_string()];
    assert_eq!(
        fake.warnings(),
        ["Controller mappings: could not read x: y"]
    );
}
