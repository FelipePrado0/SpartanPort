//! A scripted [`Host`] for the runner's in-process tests: it records every tool the runner asks
//! for and answers from its fields, so no program starts and no wait happens.
#![allow(dead_code)]

use std::ffi::OsString;
use std::io;
use std::path::PathBuf;
use std::sync::{RwLock, RwLockReadGuard, RwLockWriteGuard};
use std::time::Duration;

use indexmap::IndexMap;
use spartanport_launcher::core::platform::tool::{ToolCall, ToolError, ToolOutput, Tools};
use spartanport_launcher::runner::Host;

/// What runs when the game starts.
pub type PlayHook = Box<dyn Fn(&ToolCall) + Send + Sync>;

/// An owned copy of a [`ToolCall`] the runner made.
#[derive(Clone, Debug, PartialEq)]
pub struct Called {
    pub program: OsString,
    pub args: Vec<OsString>,
    /// The environment it got (`None`: this process's).
    pub env: Option<IndexMap<String, String>>,
    pub cwd: Option<PathBuf>,
    pub limit: Option<Duration>,
}

impl Called {
    fn of(call: &ToolCall) -> Called {
        Called {
            program: call.program.clone(),
            args: call.args.clone(),
            env: call.env.cloned(),
            cwd: call.cwd.map(PathBuf::from),
            limit: call.limit,
        }
    }
}

/// A value the host changes behind `&self`, safe to share (a [`Tools`] must be `Sync`).
#[derive(Default)]
pub struct Shared<T>(RwLock<T>);

impl<T> Shared<T> {
    fn new(value: T) -> Shared<T> {
        Shared(RwLock::new(value))
    }

    /// Reads may overlap, as with a `RefCell`.
    pub fn borrow(&self) -> RwLockReadGuard<'_, T> {
        self.0.read().unwrap()
    }

    pub fn borrow_mut(&self) -> RwLockWriteGuard<'_, T> {
        self.0.write().unwrap()
    }
}

impl<T: Copy> Shared<T> {
    pub fn get(&self) -> T {
        *self.borrow()
    }

    pub fn set(&self, value: T) {
        *self.borrow_mut() = value;
    }
}

/// When Ctrl+C comes.
#[derive(Clone, Copy, Default)]
pub enum Interrupt {
    #[default]
    Never,
    /// Before anything runs.
    Always,
    /// While the game runs: `play` gives `None`, and `interrupted` is true from then on.
    DuringGame,
    /// From the runner's `n`-th look on (1: the first).
    FromPoll(usize),
}

pub struct FakeHost {
    /// Every tool and game start, in order.
    pub calls: Shared<Vec<Called>>,
    /// What `--cpu-features` prints.
    pub cpu: String,
    /// The exit code of `--cpu-features`.
    pub cpu_exit: i32,
    /// What `--gamepad` gives.
    pub gamepad: Result<String, ToolError>,
    /// The game's exit code.
    pub exit: i32,
    pub interrupt: Interrupt,
    pub memory_mb: Option<u64>,
    /// Runs when the game starts, before it ends.
    pub on_play: Option<PlayHook>,
    /// Every `(pid, limit)` the runner waited for.
    pub waited: Shared<Vec<(u32, Duration)>>,
    /// How many times the runner had looked for Ctrl+C when the game started; `None` until then.
    pub polls_before_game: Shared<Option<usize>>,
    /// How many times the runner looked for Ctrl+C.
    polls: Shared<usize>,
}

impl Default for FakeHost {
    fn default() -> FakeHost {
        FakeHost {
            calls: Shared::default(),
            cpu: String::new(),
            cpu_exit: 0,
            gamepad: Ok(String::new()),
            exit: 0,
            interrupt: Interrupt::Never,
            memory_mb: None,
            on_play: None,
            waited: Shared::default(),
            polls_before_game: Shared::new(None),
            polls: Shared::new(0),
        }
    }
}

impl Tools for FakeHost {
    fn run(&self, call: &ToolCall) -> Result<ToolOutput, ToolError> {
        self.calls.borrow_mut().push(Called::of(call));
        let (code, stdout) = if call.args.iter().any(|arg| arg == "--gamepad") {
            match &self.gamepad {
                Ok(text) => (0, text.clone()),
                Err(ToolError::TimedOut { command, limit }) => {
                    return Err(ToolError::TimedOut {
                        command: command.clone(),
                        limit: *limit,
                    })
                }
                Err(ToolError::Spawn(error)) => {
                    return Err(ToolError::Spawn(io::Error::new(
                        error.kind(),
                        error.to_string(),
                    )))
                }
                Err(ToolError::Wait(error)) => {
                    return Err(ToolError::Wait(io::Error::new(
                        error.kind(),
                        error.to_string(),
                    )))
                }
            }
        } else {
            (self.cpu_exit, self.cpu.clone())
        };
        Ok(ToolOutput {
            code: Some(code),
            stdout,
        })
    }
}

impl Host for FakeHost {
    fn play(&self, call: &ToolCall, _log: &mut dyn FnMut(String)) -> io::Result<Option<i32>> {
        self.calls.borrow_mut().push(Called::of(call));
        self.polls_before_game.set(Some(self.polls.get()));
        if let Some(on_play) = &self.on_play {
            on_play(call);
        }
        if matches!(self.interrupt, Interrupt::DuringGame) {
            return Ok(None);
        }
        Ok(Some(self.exit))
    }

    fn wait_for_pid(&self, pid: u32, limit: Duration) -> bool {
        self.waited.borrow_mut().push((pid, limit));
        true
    }

    fn interrupted(&self) -> bool {
        let poll = self.polls.get() + 1;
        self.polls.set(poll);
        match self.interrupt {
            Interrupt::Never => false,
            Interrupt::Always => true,
            Interrupt::DuringGame => self.polls_before_game.get().is_some(),
            Interrupt::FromPoll(from) => poll >= from,
        }
    }

    fn this_process(&self) -> (PathBuf, u32) {
        (PathBuf::from("spartanport-launcher"), 4242)
    }

    fn physical_memory_mb(&self) -> Option<u64> {
        self.memory_mb
    }
}
