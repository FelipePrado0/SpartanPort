//! Runs an external tool and returns what it printed, with a time limit.

use std::ffi::OsString;
use std::fmt;
use std::io::{self, Read};
use std::path::Path;
use std::process::{Command, Stdio};
use std::time::{Duration, Instant};

use indexmap::IndexMap;

use super::{hide_console_window, hide_window};
use crate::core::python::py_list;

/// How often a running tool is checked for its end.
const POLL: Duration = Duration::from_millis(20);

/// Where the tool's error output goes.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Stderr {
    Inherit,
    Null,
}

/// Whether the tool gets a console window of its own.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Window {
    /// Never.
    Hidden,
    /// Only when this program has a console to share.
    LikeConsole,
}

/// One tool to run: no input, its output captured.
pub struct ToolCall<'a> {
    pub program: OsString,
    pub args: Vec<OsString>,
    /// The whole environment of the tool; `None` leaves it this program's.
    pub env: Option<&'a IndexMap<String, String>>,
    pub cwd: Option<&'a Path>,
    /// `None` waits for as long as the tool runs.
    pub limit: Option<Duration>,
    pub stderr: Stderr,
    pub window: Window,
}

/// What a tool that ended printed.
#[derive(Debug)]
pub struct ToolOutput {
    /// `None` when a signal ended it.
    pub code: Option<i32>,
    /// UTF-8, lossy.
    pub stdout: String,
}

#[derive(Debug)]
pub enum ToolError {
    Spawn(io::Error),
    TimedOut {
        command: Vec<String>,
        limit: Duration,
    },
    Wait(io::Error),
}

impl fmt::Display for ToolError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Spawn(error) | Self::Wait(error) => error.fmt(f),
            // subprocess.TimeoutExpired's text (run.py:132).
            Self::TimedOut { command, limit } => write!(
                f,
                "Command '{}' timed out after {} seconds",
                py_list(command),
                limit.as_secs()
            ),
        }
    }
}

impl std::error::Error for ToolError {}

/// Where the program runs its external tools: the real system, or a fake in the tests.
pub trait Tools: Send + Sync {
    /// Runs `call` and waits for it.
    fn run(&self, call: &ToolCall) -> Result<ToolOutput, ToolError>;
}

/// The real system.
pub struct SystemTools;

impl Tools for SystemTools {
    fn run(&self, call: &ToolCall) -> Result<ToolOutput, ToolError> {
        run_tool(call)
    }
}

/// This process's environment with `extra` set over it (a name that differs only in case is
/// replaced, as Windows would); for a tool that gets the same environment plus a few entries.
pub fn inherited_env_with(extra: &[(&str, String)]) -> IndexMap<String, String> {
    let mut env: IndexMap<String, String> = std::env::vars_os()
        .filter_map(|(key, value)| Some((key.into_string().ok()?, value.into_string().ok()?)))
        .collect();
    for (key, value) in extra {
        env.retain(|name, _| !name.eq_ignore_ascii_case(key));
        env.insert((*key).to_string(), value.clone());
    }
    env
}

/// Runs `call` and waits for it; a tool still running at its limit is stopped.
pub fn run_tool(call: &ToolCall) -> Result<ToolOutput, ToolError> {
    let mut cmd = Command::new(&call.program);
    cmd.args(&call.args)
        .stdin(Stdio::null())
        .stdout(Stdio::piped());
    if let Some(env) = call.env {
        cmd.env_clear().envs(env);
    }
    if let Some(cwd) = call.cwd {
        cmd.current_dir(cwd);
    }
    cmd.stderr(match call.stderr {
        Stderr::Inherit => Stdio::inherit(),
        Stderr::Null => Stdio::null(),
    });
    match call.window {
        Window::Hidden => hide_window(&mut cmd),
        Window::LikeConsole => hide_console_window(&mut cmd),
    }
    let mut child = cmd.spawn().map_err(ToolError::Spawn)?;
    let Some(mut stdout) = child.stdout.take() else {
        let _ = child.kill();
        let _ = child.wait();
        return Err(ToolError::Wait(io::Error::other("no output")));
    };
    let reader = std::thread::spawn(move || {
        let mut text = Vec::new();
        let _ = stdout.read_to_end(&mut text);
        text
    });
    let deadline = call.limit.map(|limit| Instant::now() + limit);
    let status = loop {
        match child.try_wait() {
            Ok(Some(status)) => break status,
            Ok(None) if deadline.is_none_or(|at| Instant::now() < at) => {
                std::thread::sleep(POLL);
            }
            Ok(None) => {
                let _ = child.kill();
                let _ = child.wait();
                let command = std::iter::once(&call.program)
                    .chain(&call.args)
                    .map(|part| part.to_string_lossy().into_owned())
                    .collect();
                return Err(ToolError::TimedOut {
                    command,
                    limit: call.limit.unwrap_or_default(),
                });
            }
            Err(error) => {
                let _ = child.kill();
                let _ = child.wait();
                return Err(ToolError::Wait(error));
            }
        }
    };
    let text = reader.join().unwrap_or_default();
    Ok(ToolOutput {
        code: status.code(),
        stdout: String::from_utf8_lossy(&text).into_owned(),
    })
}

/// How a [`FakeTools`] answers a call.
#[cfg(test)]
type Answer = Box<dyn Fn(&ToolCall) -> Result<ToolOutput, ToolError> + Send + Sync>;

/// A [`Tools`] for the tests: it records every call and answers from a program, so nothing
/// starts and no wait happens.
#[cfg(test)]
pub struct FakeTools {
    calls: std::sync::Mutex<Vec<Called>>,
    answer: Answer,
}

/// An owned copy of a [`ToolCall`] a [`FakeTools`] got.
#[cfg(test)]
#[derive(Clone, Debug, PartialEq)]
pub struct Called {
    pub program: OsString,
    pub args: Vec<OsString>,
    pub env: Option<IndexMap<String, String>>,
    pub limit: Option<Duration>,
    pub stderr: Stderr,
    pub window: Window,
}

#[cfg(test)]
impl FakeTools {
    /// Answers every call with `answer`.
    pub fn answering(
        answer: impl Fn(&ToolCall) -> Result<ToolOutput, ToolError> + Send + Sync + 'static,
    ) -> FakeTools {
        FakeTools {
            calls: std::sync::Mutex::default(),
            answer: Box::new(answer),
        }
    }

    /// Every call that ended with the same `code` and `stdout`.
    pub fn printing(code: i32, stdout: &str) -> FakeTools {
        let stdout = stdout.to_string();
        FakeTools::answering(move |_| {
            Ok(ToolOutput {
                code: Some(code),
                stdout: stdout.clone(),
            })
        })
    }

    /// Every call ran past its limit, without waiting for it.
    pub fn timing_out() -> FakeTools {
        FakeTools::answering(|call| {
            Err(ToolError::TimedOut {
                command: vec![call.program.to_string_lossy().into_owned()],
                limit: call.limit.unwrap_or_default(),
            })
        })
    }

    /// Every call, in order.
    pub fn calls(&self) -> Vec<Called> {
        self.calls.lock().unwrap().clone()
    }
}

#[cfg(test)]
impl Tools for FakeTools {
    fn run(&self, call: &ToolCall) -> Result<ToolOutput, ToolError> {
        self.calls.lock().unwrap().push(Called {
            program: call.program.clone(),
            args: call.args.clone(),
            env: call.env.cloned(),
            limit: call.limit,
            stderr: call.stderr,
            window: call.window,
        });
        (self.answer)(call)
    }
}
