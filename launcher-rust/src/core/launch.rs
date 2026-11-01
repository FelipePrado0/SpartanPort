//! Starting the game, reading its merged output and stopping it.

use super::i18n::tr;
use super::log::{exit_hint, ExitHint, RunLog};
use super::platform;
use indexmap::IndexMap;
use std::ffi::OsString;
use std::io::{BufRead, BufReader, Read};
use std::path::Path;
use std::process::{Command, ExitStatus, Stdio};
use std::sync::mpsc::{channel, Receiver};

/// The longest piece of output delivered at once. A line longer than this is split into chunks
/// of at most this many bytes, so a huge line without a newline never grows the buffer unbounded.
const MAX_CHUNK: u64 = 64 * 1024;

/// What the reader thread reports.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum GameEvent {
    /// A piece of output: normally one whole line including its `\n`.
    Output(String),
    /// The process ended.
    Exited(Exit),
}

/// How the game ended.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Exit {
    /// The `returncode`: on Linux one killed by a signal N gives `-N`, as Python's.
    pub code: i32,
    /// The advice of the last output line that asked for one.
    pub hint: Option<ExitHint>,
}

/// Whether the game gets a process group of its own.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Group {
    /// PLAY: its own group (a job on Windows); STOP ends it with everything it started.
    Own,
    /// `--play`: the caller's group, no job; Ctrl+C reaches the game.
    Caller,
}

/// What `spawn_game` starts and how.
pub struct Launch<'a> {
    pub command: &'a [String],
    pub cwd: &'a Path,
    /// Exactly the environment of the game.
    pub env: &'a IndexMap<String, String>,
    pub run_log: RunLog,
    /// The language of the exit advice written to the run log.
    pub lang: &'a str,
    pub group: Group,
}

#[derive(Debug, thiserror::Error)]
pub enum LaunchError {
    #[error("the command is empty")]
    EmptyCommand,
    #[error("could not start the game: {0}")]
    Spawn(#[from] std::io::Error),
}

/// The command that runs the game: the launcher's own `exe` with `--run` and `args`.
pub fn run_command(exe: &Path, args: &[OsString]) -> Vec<String> {
    let mut command = vec![exe.to_string_lossy().into_owned(), "--run".to_string()];
    command.extend(args.iter().map(|arg| arg.to_string_lossy().into_owned()));
    command
}

/// The exit code of a process; on Linux one ended by a signal N gives `-N`, as Python's
/// `Popen.returncode`.
pub fn returncode(status: ExitStatus) -> i32 {
    #[cfg(unix)]
    {
        use std::os::unix::process::ExitStatusExt;
        status
            .code()
            .unwrap_or_else(|| -status.signal().unwrap_or(1))
    }
    #[cfg(not(unix))]
    {
        status.code().unwrap_or(-1)
    }
}

/// A running game: its events and the means to stop it.
pub struct GameHandle {
    pub pid: u32,
    pub events: Receiver<GameEvent>,
    job: platform::Job,
}

impl GameHandle {
    /// Stops the game and everything it started.
    pub fn terminate(&mut self) {
        self.job.terminate();
    }

    /// Releases the job control resources; the game keeps running if it was not terminated.
    pub fn close(&mut self) {
        self.job.close();
    }
}

/// Starts the game with stdin closed and stdout and stderr merged into one pipe. A thread reads
/// that pipe and sends the events; it writes the run log, and when the game fails after printing
/// a line with a known cause it adds the advice for it to the log.
pub fn spawn_game(launch: Launch<'_>) -> Result<GameHandle, LaunchError> {
    let Launch {
        command,
        cwd,
        env,
        run_log,
        lang,
        group,
    } = launch;
    let lang = lang.to_string();
    let (program, args) = command.split_first().ok_or(LaunchError::EmptyCommand)?;
    let (reader, writer) = std::io::pipe()?;
    let mut cmd = Command::new(program);
    cmd.args(args)
        .current_dir(cwd)
        .env_clear()
        .envs(env)
        .stdin(Stdio::null())
        .stdout(Stdio::from(writer.try_clone()?))
        .stderr(Stdio::from(writer));
    match group {
        Group::Own => platform::prepare(&mut cmd),
        Group::Caller => platform::hide_window(&mut cmd),
    }
    // `cmd` owns the writer ends; dropping it after the spawn closes them in this process, so the
    // reader sees EOF once the child and any descendant holding the pipe are gone.
    let child = cmd.spawn();
    drop(cmd);
    let mut child = child?;
    let pid = child.id();
    let job = match group {
        Group::Own => platform::Job::attach(&child),
        Group::Caller => platform::Job::none(),
    };
    let (tx, events) = channel();
    std::thread::spawn(move || {
        let mut run_log = run_log;
        let mut reader = BufReader::new(reader);
        let mut buf = Vec::new();
        let mut hint = None;
        loop {
            buf.clear();
            match reader.by_ref().take(MAX_CHUNK).read_until(b'\n', &mut buf) {
                Ok(0) | Err(_) => break,
                Ok(_) => {}
            }
            let text = String::from_utf8_lossy(&buf).into_owned();
            run_log.write(&text);
            hint = exit_hint(&text).or(hint);
            let _ = tx.send(GameEvent::Output(text));
        }
        let code = child.wait().map_or(-1, returncode);
        if let Some(hint) = hint.filter(|_| code != 0) {
            run_log.write(&format!("\n{}\n", tr(&lang, hint.text_en())));
        }
        drop(run_log);
        let _ = tx.send(GameEvent::Exited(Exit { code, hint }));
    });
    Ok(GameHandle { pid, events, job })
}
