//! A scripted [`Joysticks`] for the mapper's tests: the devices it lists, the polls it answers,
//! SDL's own mapping and a preview that can fail are fields; what the launcher did is recorded.
//! Clones share one [`Script`], so a test keeps one and hands the other to the launcher.
#![allow(dead_code)]

use std::collections::VecDeque;
use std::sync::{Arc, Mutex, MutexGuard};

use spartanport_launcher::core::controller_mappings::State;
use spartanport_launcher::core::joysticks::{Device, JoyError, Joysticks};

#[derive(Debug, Default)]
pub struct Script {
    /// What `devices` lists.
    pub devices: Vec<Device>,
    /// The next polls, in order; once they run out, the selected device is at rest.
    pub states: VecDeque<State>,
    /// SDL's own line for the selected device (`current_mapping`).
    pub mapping: Option<String>,
    /// `begin_preview` fails with this message (SDL refused the line).
    pub preview_error: Option<String>,
    /// The selected device was unplugged: `state` fails as SDL's adapter does.
    pub unplugged: bool,
    /// What the preview reports while it is open.
    pub pressed: Vec<&'static str>,
    pub axes: [i16; 6],
    /// What opening said (`warnings`): the profiles SDL's adapter could not read.
    pub warnings: Vec<String>,
    // What the launcher did.
    pub selected: Option<Device>,
    /// The line being previewed.
    pub preview: Option<String>,
    /// Every line `begin_preview` was given, refused ones too.
    pub previewed: Vec<String>,
    /// How many times `close_device` ran.
    pub closed: usize,
}

#[derive(Debug, Clone)]
pub struct FakeJoysticks {
    platform: String,
    script: Arc<Mutex<Script>>,
}

impl FakeJoysticks {
    /// A fake on `platform` that lists `devices`.
    pub fn new(platform: &str, devices: Vec<Device>) -> FakeJoysticks {
        let script = Script {
            devices,
            ..Script::default()
        };
        FakeJoysticks {
            platform: platform.to_string(),
            script: Arc::new(Mutex::new(script)),
        }
    }

    /// The shared script, to change what comes next or to read what was done.
    pub fn script(&self) -> MutexGuard<'_, Script> {
        self.script
            .lock()
            .unwrap_or_else(|poisoned| poisoned.into_inner())
    }
}

/// A raw pad like the SDL tests' virtual one: 12 buttons, 6 axes and a hat, with no mapping.
pub fn pad(id: u32, guid: &str) -> Device {
    Device {
        id,
        guid: guid.to_string(),
        name: "USB pad".to_string(),
        buttons: 12,
        axes: 6,
        hats: 1,
        mapped: false,
    }
}

/// `device` with nothing pressed.
pub fn rest(device: &Device) -> State {
    State {
        buttons: vec![false; device.buttons],
        axes: vec![0; device.axes],
        hats: vec![0; device.hats],
    }
}

impl Joysticks for FakeJoysticks {
    fn platform(&self) -> &str {
        &self.platform
    }

    fn devices(&mut self) -> Result<Vec<Device>, JoyError> {
        Ok(self.script().devices.clone())
    }

    fn select(&mut self, device: &Device) -> Result<(), JoyError> {
        // The previous device goes, as with SDL, but `closed` counts only the launcher's closes.
        let mut script = self.script();
        script.preview = None;
        script.selected = Some(device.clone());
        Ok(())
    }

    fn state(&mut self) -> Result<State, JoyError> {
        let mut script = self.script();
        let Some(device) = script.selected.clone().filter(|_| !script.unplugged) else {
            return Err(JoyError::Disconnected);
        };
        Ok(script.states.pop_front().unwrap_or_else(|| rest(&device)))
    }

    fn current_mapping(&mut self) -> Option<String> {
        let script = self.script();
        script.selected.as_ref()?;
        script.mapping.clone()
    }

    fn begin_preview(&mut self, line: &str) -> Result<(), JoyError> {
        self.end_preview();
        let mut script = self.script();
        script.previewed.push(line.to_string());
        if let Some(error) = script.preview_error.clone() {
            return Err(JoyError::Sdl(error));
        }
        if script.selected.is_none() {
            return Err(JoyError::Disconnected);
        }
        script.preview = Some(line.to_string());
        Ok(())
    }

    fn preview_state(&mut self) -> Option<(Vec<&'static str>, [i16; 6])> {
        let script = self.script();
        script.preview.as_ref()?;
        Some((script.pressed.clone(), script.axes))
    }

    fn end_preview(&mut self) {
        self.script().preview = None;
    }

    fn close_device(&mut self) {
        let mut script = self.script();
        script.preview = None;
        script.selected = None;
        script.closed += 1;
    }

    fn warnings(&self) -> Vec<String> {
        self.script().warnings.clone()
    }
}
