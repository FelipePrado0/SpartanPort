//! The joysticks the controller mapper reads (Python `SDLJoysticks` in
//! `launcher/controller_mapper.py`): a seam with SDL3, loaded at run time, as its real adapter.
//! This file holds all of the mapper's FFI.

use std::ffi::{c_char, c_int, c_void, CStr, CString, OsString};
use std::path::{Path, PathBuf};
use std::ptr;
use std::sync::{Mutex, PoisonError};

use indexmap::IndexMap;
use libloading::Library;
use thiserror::Error;

use crate::core::controller_mappings::{configure_environment, State};

/// One joystick SDL sees.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Device {
    pub id: u32,
    /// SDL's GUID, in lower case hex.
    pub guid: String,
    pub name: String,
    pub buttons: usize,
    pub axes: usize,
    pub hats: usize,
    /// Whether SDL already has a gamepad mapping for it.
    pub mapped: bool,
}

#[derive(Debug, Error)]
pub enum JoyError {
    #[error("SDL3 was not found. Use a complete port installation or build the project first.")]
    Missing,
    /// The library did not load, or lacks a function. The text has the system's reason, so the
    /// error is not also its `source` (an error chain would repeat it).
    #[error("{}", loader_error(.0))]
    Library(libloading::Error),
    /// What `SDL_GetError` said.
    #[error("{0}")]
    Sdl(String),
    #[error("Controller disconnected. Reconnect it and choose Refresh.")]
    Disconnected,
}

impl From<libloading::Error> for JoyError {
    fn from(error: libloading::Error) -> JoyError {
        JoyError::Library(error)
    }
}

/// The gamepad buttons the preview names, in SDL's order.
pub const PREVIEW_BUTTONS: [&str; 15] = [
    "Cross / A",
    "Circle / B",
    "Square / X",
    "Triangle / Y",
    "Select / View / Share",
    "Guide",
    "Start / Options",
    "L3",
    "R3",
    "L1 / LB",
    "R1 / RB",
    "D-pad up",
    "D-pad down",
    "D-pad left",
    "D-pad right",
];

/// What the mapper needs from the joysticks.
pub trait Joysticks {
    /// SDL's name of this system (`Windows`, `Linux`, ...), as mapping lines name it.
    fn platform(&self) -> &str;
    fn devices(&mut self) -> Result<Vec<Device>, JoyError>;
    /// Opens `device` for [`Joysticks::state`]; the previous one is closed.
    fn select(&mut self, device: &Device) -> Result<(), JoyError>;
    /// One poll of the selected device's raw inputs.
    fn state(&mut self) -> Result<State, JoyError>;
    /// SDL's own line for the selected device, with its GUID (SDL may answer a shared one).
    fn current_mapping(&mut self) -> Option<String>;
    /// Hands `line` to SDL and opens the selected device as a gamepad with it.
    fn begin_preview(&mut self, line: &str) -> Result<(), JoyError>;
    /// The pressed buttons of the preview, by [`PREVIEW_BUTTONS`] name, and its 6 axes.
    fn preview_state(&mut self) -> Option<(Vec<&'static str>, [i16; 6])>;
    fn end_preview(&mut self);
    fn close_device(&mut self);
    /// What opening had to say: a profile file that could not be read (the Python printed it).
    fn warnings(&self) -> Vec<String> {
        Vec::new()
    }
}

/// `SDL_INIT_GAMEPAD`, which brings up the joysticks and events too.
const INIT_GAMEPAD: u32 = 0x2000;
/// `SDL_HINT_OVERRIDE`.
const HINT_OVERRIDE: c_int = 2;

/// `SDL_GUID`.
#[repr(C)]
#[derive(Clone, Copy)]
struct Guid {
    data: [u8; 16],
}

/// The SDL functions the mapper calls, as `controller_mapper.py` declares them.
macro_rules! sdl_api {
    ($($field:ident: $name:literal: fn($($arg:ty),*) $(-> $ret:ty)?;)*) => {
        struct Api {
            $($field: unsafe extern "C" fn($($arg),*) $(-> $ret)?,)*
        }

        impl Api {
            /// # Safety
            /// `library` must be SDL3, whose functions have these signatures. It is never
            /// unloaded, so the returned pointers stay valid.
            unsafe fn load(library: &'static Library) -> Result<Api, libloading::Error> {
                Ok(Api {
                    // SAFETY: the caller's: SDL3 has these functions with these signatures.
                    $($field: *unsafe {
                        library.get::<unsafe extern "C" fn($($arg),*) $(-> $ret)?>(
                            concat!($name, "\0").as_bytes(),
                        )
                    }?,)*
                })
            }
        }
    };
}

sdl_api! {
    init_sub_system: "SDL_InitSubSystem": fn(u32) -> bool;
    quit_sub_system: "SDL_QuitSubSystem": fn(u32);
    set_hint_with_priority: "SDL_SetHintWithPriority": fn(*const c_char, *const c_char, c_int) -> bool;
    get_hint: "SDL_GetHint": fn(*const c_char) -> *const c_char;
    reset_hint: "SDL_ResetHint": fn(*const c_char) -> bool;
    get_error: "SDL_GetError": fn() -> *const c_char;
    get_platform: "SDL_GetPlatform": fn() -> *const c_char;
    pump_events: "SDL_PumpEvents": fn();
    update_joysticks: "SDL_UpdateJoysticks": fn();
    get_joysticks: "SDL_GetJoysticks": fn(*mut c_int) -> *mut u32;
    free: "SDL_free": fn(*mut c_void);
    is_gamepad: "SDL_IsGamepad": fn(u32) -> bool;
    get_joystick_name_for_id: "SDL_GetJoystickNameForID": fn(u32) -> *const c_char;
    get_joystick_guid_for_id: "SDL_GetJoystickGUIDForID": fn(u32) -> Guid;
    get_gamepad_mapping_for_guid: "SDL_GetGamepadMappingForGUID": fn(Guid) -> *mut c_char;
    open_joystick: "SDL_OpenJoystick": fn(u32) -> *mut c_void;
    close_joystick: "SDL_CloseJoystick": fn(*mut c_void);
    joystick_connected: "SDL_JoystickConnected": fn(*mut c_void) -> bool;
    get_num_joystick_buttons: "SDL_GetNumJoystickButtons": fn(*mut c_void) -> c_int;
    get_num_joystick_axes: "SDL_GetNumJoystickAxes": fn(*mut c_void) -> c_int;
    get_num_joystick_hats: "SDL_GetNumJoystickHats": fn(*mut c_void) -> c_int;
    get_joystick_button: "SDL_GetJoystickButton": fn(*mut c_void, c_int) -> bool;
    get_joystick_axis: "SDL_GetJoystickAxis": fn(*mut c_void, c_int) -> i16;
    get_joystick_hat: "SDL_GetJoystickHat": fn(*mut c_void, c_int) -> u8;
    add_gamepad_mapping: "SDL_AddGamepadMapping": fn(*const c_char) -> c_int;
    open_gamepad: "SDL_OpenGamepad": fn(u32) -> *mut c_void;
    close_gamepad: "SDL_CloseGamepad": fn(*mut c_void);
    get_gamepad_button: "SDL_GetGamepadButton": fn(*mut c_void, c_int) -> bool;
    get_gamepad_axis: "SDL_GetGamepadAxis": fn(*mut c_void, c_int) -> i16;
}

/// SDL3's joysticks and gamepads. Dropping it closes the device, stops SDL's gamepad subsystem
/// and puts the hints back; the library stays loaded for the rest of the process.
pub struct Sdl3 {
    api: Api,
    platform: String,
    /// The selected device and its `SDL_Joystick` (null: none).
    device: Option<Device>,
    pad: *mut c_void,
    /// The preview's `SDL_Gamepad` (null: none).
    preview: *mut c_void,
    ready: bool,
    /// Each hint this set, with the value it had.
    old_hints: Vec<(CString, Option<CString>)>,
    /// What `configure_environment` said about the profiles while opening.
    warnings: Vec<String>,
    /// `api` and the test functions point into it; it is never unloaded.
    library: &'static Library,
}

impl Sdl3 {
    /// Loads SDL3 (`library`, else the port's, else MSYS2's; on unix the system's) and starts its
    /// gamepads, with the data folder's profiles in `SDL_GAMECONTROLLERCONFIG`.
    pub fn open(port: &Path, data_dir: &Path, library: Option<&Path>) -> Result<Sdl3, JoyError> {
        let search = search_list(port, std::env::var_os("MSYS2_ROOT"));
        let library = load_library(library, search)?;
        // SAFETY: `library` is SDL3, loaded for the rest of the process.
        let api = unsafe { Api::load(library) }?;
        // SAFETY: SDL_GetPlatform has no preconditions and returns a static string.
        let platform = unsafe { text((api.get_platform)()) }.unwrap_or_default();
        let mut env: IndexMap<String, String> = std::env::vars_os()
            .filter_map(|(key, value)| Some((key.into_string().ok()?, value.into_string().ok()?)))
            .collect();
        // Printed as the Python did, and kept for the mapper page, as the window has no console.
        let warnings = configure_environment(&mut env, port, data_dir);
        for line in &warnings {
            eprintln!("{line}");
        }
        let mut hints = vec![("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS", "1".to_string())];
        if let Some(config) = env.shift_remove("SDL_GAMECONTROLLERCONFIG") {
            if !config.is_empty() {
                hints.push(("SDL_GAMECONTROLLERCONFIG", config));
            }
        }
        let hints: Vec<(CString, CString)> = hints
            .into_iter()
            .map(|(name, value)| Ok((c_string(name)?, c_string(&value)?)))
            .collect::<Result<_, JoyError>>()?;
        let old_hints = hints
            .iter()
            .map(|(name, _)| {
                // SAFETY: `name` is a C string; SDL_GetHint's answer is copied at once.
                let old = unsafe { (api.get_hint)(name.as_ptr()) };
                // SAFETY: a non-null answer is a C string SDL owns.
                let old = (!old.is_null()).then(|| unsafe { CStr::from_ptr(old) }.to_owned());
                (name.clone(), old)
            })
            .collect();
        let mut sdl = Sdl3 {
            api,
            platform,
            device: None,
            pad: ptr::null_mut(),
            preview: ptr::null_mut(),
            ready: false,
            old_hints,
            warnings,
            library,
        };
        // From here a failure drops `sdl`, which puts the hints back.
        for (name, value) in &hints {
            // SAFETY: both are C strings, which SDL copies.
            unsafe {
                (sdl.api.set_hint_with_priority)(name.as_ptr(), value.as_ptr(), HINT_OVERRIDE)
            };
        }
        // SAFETY: SDL_InitSubSystem has no preconditions.
        if !unsafe { (sdl.api.init_sub_system)(INIT_GAMEPAD) } {
            return Err(JoyError::Sdl(sdl.error()));
        }
        sdl.ready = true;
        Ok(sdl)
    }

    /// SDL's last error, or "SDL error" without one.
    fn error(&self) -> String {
        // SAFETY: SDL_GetError has no preconditions and returns a C string SDL owns.
        unsafe { text((self.api.get_error)()) }
            .filter(|error| !error.is_empty())
            .unwrap_or_else(|| "SDL error".to_string())
    }

    fn pump(&self) {
        // SAFETY: neither has preconditions.
        unsafe {
            (self.api.pump_events)();
            (self.api.update_joysticks)();
        }
    }

    /// One device from SDL's list, opened just to count its inputs; `None` if it cannot be opened.
    fn describe(&self, id: u32) -> Option<Device> {
        // SAFETY: SDL_OpenJoystick takes any id and answers null for a bad one.
        let pad = unsafe { (self.api.open_joystick)(id) };
        if pad.is_null() {
            return None;
        }
        let count = |count: c_int| usize::try_from(count).unwrap_or(0);
        // SAFETY: `pad` is open until SDL_CloseJoystick below; the name SDL owns is copied.
        let device = unsafe {
            Device {
                id,
                guid: hex(&(self.api.get_joystick_guid_for_id)(id).data),
                name: text((self.api.get_joystick_name_for_id)(id))
                    .filter(|name| !name.is_empty())
                    .unwrap_or_else(|| "Controller".to_string()),
                buttons: count((self.api.get_num_joystick_buttons)(pad)),
                axes: count((self.api.get_num_joystick_axes)(pad)),
                hats: count((self.api.get_num_joystick_hats)(pad)),
                mapped: (self.api.is_gamepad)(id),
            }
        };
        // SAFETY: `pad` was opened above (SDL counts the opens of one device).
        unsafe { (self.api.close_joystick)(pad) };
        Some(device)
    }
}

impl Joysticks for Sdl3 {
    fn platform(&self) -> &str {
        &self.platform
    }

    fn devices(&mut self) -> Result<Vec<Device>, JoyError> {
        self.pump();
        let mut count: c_int = 0;
        // SAFETY: `count` is a valid out pointer.
        let ids = unsafe { (self.api.get_joysticks)(&mut count) };
        if ids.is_null() {
            return Ok(Vec::new());
        }
        let count = usize::try_from(count).unwrap_or(0);
        // SAFETY: SDL_GetJoysticks answered an array of `count` ids; it is copied, then released
        // with SDL_free once.
        let copied: Vec<u32> = unsafe {
            let copied = std::slice::from_raw_parts(ids, count).to_vec();
            (self.api.free)(ids.cast());
            copied
        };
        Ok(copied
            .into_iter()
            .filter_map(|id| self.describe(id))
            .collect())
    }

    fn select(&mut self, device: &Device) -> Result<(), JoyError> {
        self.close_device();
        // SAFETY: SDL_OpenJoystick takes any id and answers null for a bad one.
        self.pad = unsafe { (self.api.open_joystick)(device.id) };
        if self.pad.is_null() {
            return Err(JoyError::Sdl(self.error()));
        }
        self.device = Some(device.clone());
        Ok(())
    }

    fn state(&mut self) -> Result<State, JoyError> {
        self.pump();
        // SAFETY: a non-null `pad` stays open until `close_device`; an unplugged one is still
        // valid, only no longer connected.
        let connected = !self.pad.is_null() && unsafe { (self.api.joystick_connected)(self.pad) };
        let Some(device) = self.device.as_ref().filter(|_| connected) else {
            return Err(JoyError::Disconnected);
        };
        let (pad, api) = (self.pad, &self.api);
        let inputs = |count: usize| 0..c_int::try_from(count).unwrap_or(c_int::MAX);
        // SAFETY: `pad` is open; SDL answers 0 for an index past the device's inputs.
        let state = unsafe {
            State {
                buttons: inputs(device.buttons)
                    .map(|i| (api.get_joystick_button)(pad, i))
                    .collect(),
                axes: inputs(device.axes)
                    .map(|i| (api.get_joystick_axis)(pad, i))
                    .collect(),
                hats: inputs(device.hats)
                    .map(|i| (api.get_joystick_hat)(pad, i))
                    .collect(),
            }
        };
        Ok(state)
    }

    fn current_mapping(&mut self) -> Option<String> {
        let device = self.device.as_ref()?;
        // SAFETY: both take any id or GUID; the answer is null or a C string to SDL_free.
        let line = unsafe {
            let guid = (self.api.get_joystick_guid_for_id)(device.id);
            let pointer = (self.api.get_gamepad_mapping_for_guid)(guid);
            let line = text(pointer);
            if !pointer.is_null() {
                (self.api.free)(pointer.cast());
            }
            line?
        };
        // SDL may answer a shared GUID (an XInput mapping, say): the copy is this device's.
        let (_, rest) = line.split_once(',')?;
        Some(format!("{},{rest}", device.guid))
    }

    fn begin_preview(&mut self, line: &str) -> Result<(), JoyError> {
        self.end_preview();
        let Some(id) = self.device.as_ref().map(|device| device.id) else {
            return Err(JoyError::Disconnected);
        };
        let line = c_string(line)?;
        // SAFETY: `line` is a C string, which SDL copies.
        if unsafe { (self.api.add_gamepad_mapping)(line.as_ptr()) } < 0 {
            return Err(JoyError::Sdl(self.error()));
        }
        // SAFETY: SDL_OpenGamepad takes any id and answers null when it cannot open it.
        self.preview = unsafe { (self.api.open_gamepad)(id) };
        if self.preview.is_null() {
            return Err(JoyError::Sdl(self.error()));
        }
        Ok(())
    }

    fn preview_state(&mut self) -> Option<(Vec<&'static str>, [i16; 6])> {
        if self.preview.is_null() {
            return None;
        }
        self.pump();
        let (preview, api) = (self.preview, &self.api);
        // SAFETY: `preview` is open until `end_preview`; the indexes are SDL's buttons and axes.
        unsafe {
            let buttons = (0..)
                .zip(PREVIEW_BUTTONS)
                .filter(|&(i, _)| (api.get_gamepad_button)(preview, i))
                .map(|(_, name)| name)
                .collect();
            let mut axes = [0; 6];
            for (i, axis) in (0..).zip(axes.iter_mut()) {
                *axis = (api.get_gamepad_axis)(preview, i);
            }
            Some((buttons, axes))
        }
    }

    fn end_preview(&mut self) {
        if !self.preview.is_null() {
            // SAFETY: `preview` was opened by SDL_OpenGamepad and is closed once.
            unsafe { (self.api.close_gamepad)(self.preview) };
            self.preview = ptr::null_mut();
        }
    }

    fn close_device(&mut self) {
        self.end_preview();
        if !self.pad.is_null() {
            // SAFETY: `pad` was opened by SDL_OpenJoystick and is closed once.
            unsafe { (self.api.close_joystick)(self.pad) };
            self.pad = ptr::null_mut();
        }
        self.device = None;
    }

    fn warnings(&self) -> Vec<String> {
        self.warnings.clone()
    }
}

impl Drop for Sdl3 {
    fn drop(&mut self) {
        self.close_device();
        if self.ready {
            // SAFETY: the subsystem was started by this `Sdl3`.
            unsafe { (self.api.quit_sub_system)(INIT_GAMEPAD) };
            self.ready = false;
        }
        for (name, old) in &self.old_hints {
            // SAFETY: both are C strings, which SDL copies.
            unsafe {
                match old {
                    None => (self.api.reset_hint)(name.as_ptr()),
                    Some(old) => (self.api.set_hint_with_priority)(
                        name.as_ptr(),
                        old.as_ptr(),
                        HINT_OVERRIDE,
                    ),
                };
            }
        }
    }
}

/// A virtual joystick for the tests, as `tests/test_controller_mapper_sdl.py` describes it.
#[doc(hidden)]
#[derive(Debug, Clone, Default)]
pub struct VirtualJoystick {
    /// `SDL_JoystickType`.
    pub kind: u16,
    pub vendor: u16,
    pub product: u16,
    pub axes: u16,
    pub buttons: u16,
    pub hats: u16,
    pub name: String,
}

/// An input of the selected virtual joystick, for the tests.
#[doc(hidden)]
#[derive(Debug, Clone, Copy)]
pub enum VirtualInput {
    Button(i32, bool),
    Axis(i32, i16),
    Hat(i32, u8),
}

/// `SDL_VirtualJoystickDesc`; `version` is its size, as `SDL_INIT_INTERFACE` sets it.
#[repr(C)]
struct VirtualJoystickDesc {
    version: u32,
    kind: u16,
    padding: u16,
    vendor: u16,
    product: u16,
    axes: u16,
    buttons: u16,
    balls: u16,
    hats: u16,
    touchpads: u16,
    sensors: u16,
    padding2: [u16; 2],
    button_mask: u32,
    axis_mask: u32,
    name: *const c_char,
    touchpad_desc: *const c_void,
    sensor_desc: *const c_void,
    userdata: *mut c_void,
    callbacks: [*const c_void; 8],
}

impl Sdl3 {
    /// The test function `name` of SDL (the virtual joysticks).
    ///
    /// # Safety
    /// `T` must be the function's signature.
    unsafe fn test_function<T: Copy>(&self, name: &str) -> Result<T, JoyError> {
        let name = c_string(name)?;
        // SAFETY: the caller gives the signature; `self.library` is never unloaded.
        Ok(*unsafe { self.library.get::<T>(name.as_bytes_with_nul()) }?)
    }

    /// Attaches `joystick` (`SDL_AttachVirtualJoystick`) and answers its id.
    #[doc(hidden)]
    pub fn attach_virtual(&mut self, joystick: &VirtualJoystick) -> Result<u32, JoyError> {
        type Attach = unsafe extern "C" fn(*const VirtualJoystickDesc) -> u32;
        // SAFETY: the signature of SDL_AttachVirtualJoystick.
        let attach: Attach = unsafe { self.test_function("SDL_AttachVirtualJoystick") }?;
        let name = c_string(&joystick.name)?;
        let desc = VirtualJoystickDesc {
            version: std::mem::size_of::<VirtualJoystickDesc>() as u32,
            kind: joystick.kind,
            padding: 0,
            vendor: joystick.vendor,
            product: joystick.product,
            axes: joystick.axes,
            buttons: joystick.buttons,
            balls: 0,
            hats: joystick.hats,
            touchpads: 0,
            sensors: 0,
            padding2: [0; 2],
            button_mask: 0,
            axis_mask: 0,
            name: name.as_ptr(),
            touchpad_desc: ptr::null(),
            sensor_desc: ptr::null(),
            userdata: ptr::null_mut(),
            callbacks: [ptr::null(); 8],
        };
        // SAFETY: `desc` and `name` live through the call; SDL copies what it keeps.
        match unsafe { attach(&desc) } {
            0 => Err(JoyError::Sdl(self.error())),
            id => Ok(id),
        }
    }

    /// Unplugs the virtual joystick `id` (`SDL_DetachVirtualJoystick`).
    #[doc(hidden)]
    pub fn detach_virtual(&mut self, id: u32) -> Result<(), JoyError> {
        type Detach = unsafe extern "C" fn(u32) -> bool;
        // SAFETY: the signature of SDL_DetachVirtualJoystick, which takes any id.
        let detach: Detach = unsafe { self.test_function("SDL_DetachVirtualJoystick") }?;
        // SAFETY: as above.
        self.checked(unsafe { detach(id) })
    }

    /// Moves an input of the selected virtual joystick (`SDL_SetJoystickVirtual*`).
    #[doc(hidden)]
    pub fn set_virtual(&mut self, input: VirtualInput) -> Result<(), JoyError> {
        type Button = unsafe extern "C" fn(*mut c_void, c_int, bool) -> bool;
        type Axis = unsafe extern "C" fn(*mut c_void, c_int, i16) -> bool;
        type Hat = unsafe extern "C" fn(*mut c_void, c_int, u8) -> bool;
        if self.pad.is_null() {
            return Err(JoyError::Disconnected);
        }
        let pad = self.pad;
        // SAFETY: the signatures of SDL's functions; `pad` is open, and SDL refuses a joystick
        // that is not virtual.
        let done = unsafe {
            match input {
                VirtualInput::Button(i, down) => {
                    self.test_function::<Button>("SDL_SetJoystickVirtualButton")?(pad, i, down)
                }
                VirtualInput::Axis(i, value) => {
                    self.test_function::<Axis>("SDL_SetJoystickVirtualAxis")?(pad, i, value)
                }
                VirtualInput::Hat(i, value) => {
                    self.test_function::<Hat>("SDL_SetJoystickVirtualHat")?(pad, i, value)
                }
            }
        };
        self.checked(done)
    }

    fn checked(&self, done: bool) -> Result<(), JoyError> {
        if done {
            Ok(())
        } else {
            Err(JoyError::Sdl(self.error()))
        }
    }
}

/// Where SDL3 is looked for when no library is given, in order (Python `SDLJoysticks`): the
/// port's `bin/` and `out/`, then MSYS2's (`MSYS2_ROOT`, else `C:\msys64`); on unix the system's.
fn search_list(port: &Path, msys2_root: Option<OsString>) -> Vec<PathBuf> {
    if cfg!(windows) {
        let msys2 = PathBuf::from(msys2_root.unwrap_or_else(|| r"C:\msys64".into()));
        vec![
            port.join("bin").join("SDL3.dll"),
            port.join("out").join("SDL3.dll"),
            msys2.join("clang64").join("bin").join("SDL3.dll"),
        ]
    } else {
        vec!["libSDL3.so.0".into(), "libSDL3.so".into()]
    }
}

/// `library`, else the first of `search`: on Windows the first that exists, on unix the first
/// the loader finds.
fn load_library(
    library: Option<&Path>,
    search: Vec<PathBuf>,
) -> Result<&'static Library, JoyError> {
    if let Some(library) = library {
        return load(library);
    }
    if cfg!(windows) {
        match search.into_iter().find(|path| path.is_file()) {
            Some(path) => load(&path),
            None => Err(JoyError::Missing),
        }
    } else {
        // The first the loader finds. Names it did not find leave `Missing`; the error of one it
        // found but could not load (a bad file, a missing dependency) is kept, as Python's CDLL
        // raised it.
        let mut failed = None;
        for name in &search {
            match load(name) {
                Ok(library) => return Ok(library),
                Err(JoyError::Library(error)) if !not_found(name, &error.to_string()) => {
                    failed = Some(error);
                }
                Err(_) => {}
            }
        }
        Err(failed.map_or(JoyError::Missing, JoyError::Library))
    }
}

/// Every library [`load`] loaded, by the name it was loaded with. None is ever unloaded, as
/// Python's `CDLL` never unloads SDL3: `Sdl3` stops only SDL's gamepad subsystem, and SDL's own
/// global state goes only with `SDL_Quit`. A second `Sdl3` on the same name reuses the library.
static LOADED: Mutex<Vec<(PathBuf, &'static Library)>> = Mutex::new(Vec::new());

/// Loads one library, once for the process (see [`LOADED`]). On Windows, as Python's ctypes: a
/// name with a folder is made absolute (which also turns `/` into `\`), and that folder is
/// searched for the DLL's own DLLs.
fn load(name: &Path) -> Result<&'static Library, JoyError> {
    let in_folder = name.components().count() > 1;
    let name = if cfg!(windows) && in_folder {
        std::path::absolute(name).unwrap_or_else(|_| name.to_path_buf())
    } else {
        name.to_path_buf()
    };
    let mut loaded = LOADED.lock().unwrap_or_else(PoisonError::into_inner);
    if let Some(&(_, library)) = loaded.iter().find(|(known, _)| *known == name) {
        return Ok(library);
    }
    #[cfg(windows)]
    let library: Library = {
        use libloading::os::windows::{
            Library as WindowsLibrary, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR,
        };
        let flags = if in_folder {
            LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR
        } else {
            LOAD_LIBRARY_SEARCH_DEFAULT_DIRS
        };
        // SAFETY: loading SDL3 runs only its own initialisers, which have no preconditions.
        unsafe { WindowsLibrary::load_with_flags(&name, flags) }?.into()
    };
    // SAFETY: as above.
    #[cfg(not(windows))]
    let library = unsafe { Library::new(&name) }?;
    // Leaked on purpose: it stays loaded for the rest of the process.
    let library: &'static Library = Box::leak(Box::new(library));
    loaded.push((name, library));
    Ok(library)
}

/// Whether the loader's message `said`, for `name`, means it did not find `name` itself. A path
/// is looked at directly. For a bare name glibc's message starts with that name only then: a
/// library it found is named by its full path, and a missing dependency by its own name.
fn not_found(name: &Path, said: &str) -> bool {
    if name.components().count() > 1 {
        !name.exists()
    } else {
        said.starts_with(&format!("{}:", name.display()))
    }
}

/// libloading's words for a failed load, then the system's reason (on Windows libloading says
/// only "LoadLibraryExW failed").
fn loader_error(error: &libloading::Error) -> String {
    match std::error::Error::source(error) {
        Some(reason) => format!("{error}: {reason}"),
        None => error.to_string(),
    }
}

fn c_string(text: &str) -> Result<CString, JoyError> {
    CString::new(text).map_err(|_| JoyError::Sdl("Invalid controller mapping.".to_string()))
}

/// A C string as text (invalid UTF-8 replaced); `None` for null.
///
/// # Safety
/// `pointer` is null or a valid C string.
unsafe fn text(pointer: *const c_char) -> Option<String> {
    if pointer.is_null() {
        return None;
    }
    // SAFETY: the caller's.
    let text = unsafe { CStr::from_ptr(pointer) };
    Some(text.to_string_lossy().into_owned())
}

fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|byte| format!("{byte:02x}")).collect()
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    #[test]
    fn a_missing_library_names_sdl3() {
        let port = tempfile::tempdir().unwrap();
        let msys2 = tempfile::tempdir().unwrap();
        let search = search_list(port.path(), Some(msys2.path().into()));
        #[cfg(windows)]
        {
            assert_eq!(
                search,
                [
                    port.path().join("bin").join("SDL3.dll"),
                    port.path().join("out").join("SDL3.dll"),
                    msys2.path().join("clang64").join("bin").join("SDL3.dll"),
                ]
            );
            assert_eq!(
                search_list(port.path(), None)[2],
                Path::new(r"C:\msys64\clang64\bin\SDL3.dll")
            );
        }
        #[cfg(not(windows))]
        assert_eq!(
            search,
            [PathBuf::from("libSDL3.so.0"), PathBuf::from("libSDL3.so")]
        );
        // None of the Windows places has it; the loader's own search is left out on unix.
        let absent = if cfg!(windows) {
            search
        } else {
            vec![port.path().join("libSDL3.so.0")]
        };
        let error = load_library(None, absent).err().unwrap();
        assert!(matches!(error, JoyError::Missing), "{error:?}");
        assert_eq!(
            error.to_string(),
            "SDL3 was not found. Use a complete port installation or build the project first."
        );
    }

    #[test]
    fn sdl3_is_loaded_once_and_kept_for_the_process() {
        // SDL's global state outlives SDL_QuitSubSystem, so the library is never unloaded (as
        // Python's CDLL), and opening the mapper again reuses it.
        let Some(path) = std::env::var_os("SPARTAN_TEST_SDL3").filter(|path| !path.is_empty())
        else {
            println!("skipped: set SPARTAN_TEST_SDL3");
            return;
        };
        let path = PathBuf::from(path);
        let first: &'static Library = load_library(Some(&path), Vec::new()).unwrap();
        let again = load_library(Some(&path), Vec::new()).unwrap();
        assert!(ptr::eq(first, again));
        #[cfg(windows)]
        {
            // The same file, however its separators are written.
            let spelled = PathBuf::from(path.to_string_lossy().replace('/', r"\"));
            assert!(ptr::eq(first, load(&spelled).unwrap()));
        }
    }

    #[test]
    fn a_library_that_does_not_load_says_why() {
        let port = tempfile::tempdir().unwrap();
        let msys2 = tempfile::tempdir().unwrap();
        let (broken, search) = if cfg!(windows) {
            let broken = port.path().join("bin").join("SDL3.dll");
            let search = search_list(port.path(), Some(msys2.path().into()));
            (broken, search)
        } else {
            let broken = port.path().join("libSDL3.so.0");
            (broken.clone(), vec![broken])
        };
        fs::create_dir_all(broken.parent().unwrap()).unwrap();
        fs::write(&broken, b"not a library").unwrap();
        let error = load_library(None, search).err().unwrap();
        assert!(matches!(error, JoyError::Library(_)), "{error:?}");
        let said = error.to_string();
        // The system's reason, not only libloading's "LoadLibraryExW failed".
        #[cfg(windows)]
        assert!(said.contains("(os error "), "{said}");
        #[cfg(not(windows))]
        assert!(said.contains(&broken.display().to_string()), "{said}");
        // The text already holds the reason: as a source too, an error chain would repeat it.
        assert!(std::error::Error::source(&error).is_none(), "{error:?}");
    }

    #[test]
    fn only_a_name_the_loader_did_not_find_is_missing() {
        // glibc names a bare name it did not find; a library it found is named by its path, and a
        // missing dependency by the dependency's name.
        let name = Path::new("libSDL3.so.0");
        let missing = "cannot open shared object file: No such file or directory";
        assert!(not_found(name, &format!("libSDL3.so.0: {missing}")));
        assert!(!not_found(
            name,
            "/usr/lib/libSDL3.so.0: invalid ELF header"
        ));
        assert!(!not_found(name, &format!("libdecor-0.so.0: {missing}")));
        // A path is looked at directly.
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("libSDL3.so.0");
        assert!(not_found(&path, &format!("{}: {missing}", path.display())));
        fs::write(&path, b"not a library").unwrap();
        let said = format!("{}: invalid ELF header", path.display());
        assert!(!not_found(&path, &said));
    }
}
