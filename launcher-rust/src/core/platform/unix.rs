//! Calls for systems other than Windows.

use nix::errno::Errno;
use nix::sys::signal::{kill, killpg, SigSet, Signal};
use nix::unistd::Pid;
use std::fs;
use std::io;
use std::os::unix::process::CommandExt;
use std::path::Path;
use std::process::{Child, Command};
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;

/// Time between SIGTERM and SIGKILL.
const KILL_AFTER: Duration = Duration::from_secs(3);

/// There is no Windows UI language here.
pub fn user_default_ui_language() -> Option<u32> {
    None
}

/// Steam keeps no registry entry here.
pub fn steam_path_from_registry() -> Option<std::path::PathBuf> {
    None
}

/// Programs share the terminal here: nothing to hide.
pub fn hide_window(_cmd: &mut Command) {}

/// The error goes to the terminal here: there is no dialog to block on.
pub fn show_error(title: &str, text: &str) {
    eprintln!("{title}: {text}");
}

/// The output already reaches the terminal here.
pub fn attach_parent_console() {}

/// Console programs never open a window of their own here.
pub fn hide_console_window(_cmd: &mut Command) {}

/// The window manager draws the title bar here.
pub fn dark_title_bar(_hwnd: isize) {}

/// The program runs from a terminal or a launcher that takes its output: it counts as having one.
pub fn has_console() -> bool {
    true
}

/// Sets the execute bits the way the Python does: exactly `0o755`, or the mode plus `0o755`.
pub fn make_executable(path: &Path, how: super::Exec) -> io::Result<()> {
    use std::os::unix::fs::PermissionsExt;
    let mode = match how {
        super::Exec::Exactly => 0o755,
        super::Exec::Adding => fs::metadata(path)?.permissions().mode() | 0o755,
    };
    fs::set_permissions(path, fs::Permissions::from_mode(mode))
}

/// A file with any execute bit set.
pub fn is_executable(path: &Path) -> bool {
    use std::os::unix::fs::PermissionsExt;
    fs::metadata(path).is_ok_and(|m| m.is_file() && m.permissions().mode() & 0o111 != 0)
}

/// Off Windows the installed RAM is not read (the Python returns `None` too): the warning's advice
/// is about the Windows page file and error 1450.
pub fn physical_memory_mb() -> Option<u64> {
    None
}

/// Set once SIGINT came after `catch_interrupt`.
static INTERRUPTED: AtomicBool = AtomicBool::new(false);

/// From now on SIGINT (Ctrl+C) ends nothing: it sets [`interrupted`], which the caller checks to
/// end in order (the Python's `KeyboardInterrupt`). The signal is blocked in the calling thread,
/// and so in the threads it starts afterwards, and taken by a thread of its own; the programs it
/// starts get the usual handling back (the standard library resets their signal mask).
pub fn catch_interrupt() {
    let mut set = SigSet::empty();
    set.add(Signal::SIGINT);
    if set.thread_block().is_ok() {
        std::thread::spawn(move || {
            while set.wait().is_ok() {
                INTERRUPTED.store(true, Ordering::SeqCst);
            }
        });
    }
}

/// Whether Ctrl+C came since `catch_interrupt`.
pub fn interrupted() -> bool {
    INTERRUPTED.load(Ordering::SeqCst)
}

/// Puts the child in its own process group.
pub fn prepare(cmd: &mut Command) {
    cmd.process_group(0);
}

/// The game's process group.
pub struct Job {
    pgid: i32,
}

fn signal_group(pgid: i32, signal: Signal) {
    // A group that is gone (ESRCH) or not ours (EPERM) is not worth reporting; nothing to undo.
    let _ = killpg(Pid::from_raw(pgid), signal);
}

impl Job {
    pub fn attach(child: &Child) -> Job {
        Job {
            pgid: i32::try_from(child.id()).unwrap_or(0),
        }
    }

    /// No group of its own: the game stays in the caller's, and `terminate` does nothing.
    pub fn none() -> Job {
        Job { pgid: 0 }
    }

    /// SIGTERM to the group, then SIGKILL after three seconds.
    pub fn terminate(&mut self) {
        if self.pgid <= 0 {
            return;
        }
        let pgid = self.pgid;
        signal_group(pgid, Signal::SIGTERM);
        std::thread::spawn(move || {
            std::thread::sleep(KILL_AFTER);
            signal_group(pgid, Signal::SIGKILL);
        });
    }

    pub fn close(&mut self) {}
}

/// Points this process's stdout and stderr at `/dev/null`, which closes the pipe the game's
/// reader holds the other end of. Errors are ignored.
pub fn close_std_streams() {
    if let Ok(null) = std::fs::File::options().write(true).open("/dev/null") {
        let _ = nix::unistd::dup2_stdout(&null);
        let _ = nix::unistd::dup2_stderr(&null);
    }
}

/// Junctions exist only on Windows: links here are symlinks.
pub fn create_junction(_link: &Path, _target: &Path) -> io::Result<()> {
    Err(io::Error::new(
        io::ErrorKind::Unsupported,
        "junctions exist only on Windows",
    ))
}

/// Waits up to `limit` for process `pid` to end, polling every 100 ms. `true` when it ended or
/// does not exist, `false` when it still runs after `limit`.
pub fn wait_for_process(pid: u32, limit: Duration) -> bool {
    let Ok(raw) = i32::try_from(pid) else {
        return true;
    };
    let deadline = std::time::Instant::now() + limit;
    loop {
        // Signal 0 only checks the process; a process of another user (EPERM) still runs.
        if kill(Pid::from_raw(raw), None) == Err(Errno::ESRCH) {
            return true;
        }
        if std::time::Instant::now() >= deadline {
            return false;
        }
        std::thread::sleep(Duration::from_millis(100));
    }
}

#[cfg(test)]
mod tests {
    use super::super::{which, Exec};
    use super::*;

    fn file_with_mode(dir: &Path, name: &str, mode: u32) -> std::path::PathBuf {
        use std::os::unix::fs::PermissionsExt;
        let file = dir.join(name);
        std::fs::write(&file, b"x").unwrap();
        std::fs::set_permissions(&file, std::fs::Permissions::from_mode(mode)).unwrap();
        file
    }

    fn mode_of(file: &Path) -> u32 {
        use std::os::unix::fs::PermissionsExt;
        std::fs::metadata(file).unwrap().permissions().mode() & 0o777
    }

    #[test]
    fn adding_keeps_other_mode_bits() {
        let temp = tempfile::tempdir().unwrap();
        // Group write is outside 0o755: Adding keeps it, Exactly drops it.
        let added = file_with_mode(temp.path(), "added", 0o664);
        make_executable(&added, Exec::Adding).unwrap();
        assert_eq!(mode_of(&added), 0o775);
        let exact = file_with_mode(temp.path(), "exact", 0o640);
        make_executable(&exact, Exec::Exactly).unwrap();
        assert_eq!(mode_of(&exact), 0o755);
    }

    #[test]
    fn which_finds_an_executable_on_the_path_only() {
        use std::os::unix::fs::PermissionsExt;
        let temp = tempfile::tempdir().unwrap();
        let path = std::env::join_paths([temp.path()]).unwrap();
        let program = file_with_mode(temp.path(), "mangohud", 0o644);
        assert_eq!(which("mangohud", &path), None);
        std::fs::set_permissions(&program, std::fs::Permissions::from_mode(0o755)).unwrap();
        assert_eq!(which("mangohud", &path), Some(program));
        assert_eq!(which("gamemoderun", &path), None);
        assert!(!is_executable(temp.path()));
    }

    #[test]
    fn physical_memory_is_unknown_off_windows() {
        assert_eq!(physical_memory_mb(), None);
    }

    #[test]
    fn a_unix_program_always_has_a_console() {
        assert!(has_console());
    }

    #[test]
    fn there_are_no_junctions_here() {
        let temp = tempfile::tempdir().unwrap();
        let link = temp.path().join("link");
        let error = create_junction(&link, temp.path()).unwrap_err();
        assert_eq!(error.kind(), std::io::ErrorKind::Unsupported);
        assert!(std::fs::symlink_metadata(&link).is_err());
    }
}
