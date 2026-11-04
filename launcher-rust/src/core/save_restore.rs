//! Selectable automatic, legacy and permanent pre-restore snapshots.
//!
//! Source snapshots are read only. Copies are hashed before publishing; the old savedata is
//! renamed aside until publication succeeds. Safety snapshots never enter automatic retention.

use std::collections::{BTreeMap, BTreeSet};
use std::fs::{self, File, OpenOptions};
use std::io::{self, Read};
use std::path::{Component, Path, PathBuf};
use std::time::SystemTime;

use chrono::{DateTime, Local};
use fs2::FileExt;
use serde::Serialize;
use sha2::{Digest, Sha256};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Source {
    Automatic,
    Legacy,
    Safety,
}

impl Source {
    pub fn label(self) -> &'static str {
        match self {
            Self::Automatic => "Automatic backup",
            Self::Legacy => "Legacy backup",
            Self::Safety => "Before-restore backup",
        }
    }
}

#[derive(Clone, Debug)]
pub struct Snapshot {
    pub key: String,
    pub path: PathBuf,
    pub source: Source,
    pub files: usize,
    pub bytes: u64,
    pub titles: BTreeSet<String>,
    pub modified: SystemTime,
}

impl Snapshot {
    pub fn time_label(&self) -> String {
        let name = self.key.rsplit('/').next().unwrap_or("");
        name.get(..15)
            .and_then(|s| chrono::NaiveDateTime::parse_from_str(s, "%Y%m%d-%H%M%S").ok())
            .map(|time| time.format("%Y-%m-%d %H:%M:%S").to_string())
            .unwrap_or_else(|| {
                DateTime::<Local>::from(self.modified)
                    .format("%Y-%m-%d %H:%M:%S")
                    .to_string()
            })
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
struct Fingerprint {
    bytes: u64,
    sha256: String,
}

type Inventory = BTreeMap<PathBuf, Fingerprint>;

fn invalid(message: impl Into<String>) -> io::Error {
    io::Error::other(message.into())
}

/// Refuse links at this path; callers check every component below an approved root.
fn checked(path: &Path) -> io::Result<()> {
    let meta = match fs::symlink_metadata(path) {
        Ok(meta) => meta,
        Err(e) if e.kind() == io::ErrorKind::NotFound => return Ok(()),
        Err(e) => return Err(e),
    };
    #[cfg(windows)]
    let reparse = {
        use std::os::windows::fs::MetadataExt;
        meta.file_attributes() & 0x400 != 0
    };
    #[cfg(not(windows))]
    let reparse = false;
    if meta.file_type().is_symlink() || reparse {
        return Err(invalid(format!(
            "Linked save paths are not supported: {}",
            path.display()
        )));
    }
    Ok(())
}

/// Lists relative file sizes without opening save contents or hashing them.
fn file_sizes(root: &Path) -> io::Result<BTreeMap<PathBuf, u64>> {
    fn visit(root: &Path, folder: &Path, result: &mut BTreeMap<PathBuf, u64>) -> io::Result<()> {
        checked(folder)?;
        for entry in fs::read_dir(folder)? {
            let path = entry?.path();
            checked(&path)?;
            let meta = fs::symlink_metadata(&path)?;
            if meta.is_dir() {
                visit(root, &path, result)?;
            } else if meta.is_file() {
                let relative = path
                    .strip_prefix(root)
                    .map_err(|e| invalid(e.to_string()))?;
                result.insert(relative.into(), meta.len());
            } else {
                return Err(invalid(
                    "Only regular save files and directories are supported",
                ));
            }
        }
        Ok(())
    }
    let mut result = BTreeMap::new();
    visit(root, root, &mut result)?;
    Ok(result)
}

fn inventory(root: &Path) -> io::Result<Inventory> {
    fn visit(root: &Path, folder: &Path, result: &mut Inventory) -> io::Result<()> {
        checked(folder)?;
        for entry in fs::read_dir(folder)? {
            let path = entry?.path();
            checked(&path)?;
            let meta = fs::symlink_metadata(&path)?;
            if meta.is_dir() {
                visit(root, &path, result)?;
            } else if meta.is_file() {
                let mut file = File::open(&path)?;
                let mut hash = Sha256::new();
                let mut bytes = 0;
                let mut buffer = [0; 64 * 1024];
                loop {
                    let n = file.read(&mut buffer)?;
                    if n == 0 {
                        break;
                    }
                    hash.update(&buffer[..n]);
                    bytes += n as u64;
                }
                let relative = path
                    .strip_prefix(root)
                    .map_err(|e| invalid(e.to_string()))?;
                result.insert(
                    relative.into(),
                    Fingerprint {
                        bytes,
                        sha256: format!("{:x}", hash.finalize()),
                    },
                );
            } else {
                return Err(invalid(
                    "Only regular save files and directories are supported",
                ));
            }
        }
        Ok(())
    }
    let mut result = BTreeMap::new();
    visit(root, root, &mut result)?;
    Ok(result)
}

fn copy_verified(source: &Path, target: &Path) -> io::Result<Inventory> {
    fn copy(from: &Path, to: &Path) -> io::Result<()> {
        checked(from)?;
        fs::create_dir(to)?;
        for entry in fs::read_dir(from)? {
            let path = entry?.path();
            checked(&path)?;
            let name = path
                .file_name()
                .ok_or_else(|| invalid("Missing file name"))?;
            if fs::symlink_metadata(&path)?.is_dir() {
                copy(&path, &to.join(name))?;
            } else {
                fs::copy(&path, to.join(name))?;
            }
        }
        Ok(())
    }
    let original = inventory(source)?;
    copy(source, target)?;
    if original != inventory(target)? || original != inventory(source)? {
        return Err(invalid(
            "Save copy checksum mismatch; no saves were replaced",
        ));
    }
    Ok(original)
}

fn roots(user: &Path, port: &Path) -> [(Source, PathBuf); 3] {
    [
        (Source::Automatic, user.join("savedata_backups")),
        (Source::Legacy, port.join("savedata_backups")),
        (Source::Safety, user.join("save-restore-safety")),
    ]
}

/// Snapshot keys remain plain names for automatic backups, preserving existing selections.
pub fn list(user: &Path, port: &Path) -> Vec<Snapshot> {
    let mut seen = BTreeSet::new();
    let mut result = Vec::new();
    for (source, root) in roots(user, port) {
        if checked(root.parent().unwrap_or(&root)).is_err() || checked(&root).is_err() {
            continue;
        }
        let Ok(entries) = fs::read_dir(&root) else {
            continue;
        };
        for entry in entries.flatten() {
            let name = entry.file_name().to_string_lossy().into_owned();
            if name.starts_with('.') {
                continue;
            }
            if checked(&entry.path()).is_err() {
                continue;
            }
            let path = if source == Source::Safety {
                entry.path().join("savedata")
            } else {
                entry.path()
            };
            let Ok(files) = file_sizes(&path) else {
                continue;
            };
            if files.is_empty() {
                continue;
            }
            let Ok(canonical) = path.canonicalize() else {
                continue;
            };
            if !seen.insert(canonical) {
                continue;
            }
            let titles = files
                .keys()
                .flat_map(|p| p.components())
                .filter_map(|part| {
                    let text = part.as_os_str().to_str()?;
                    (text.len() == 9
                        && text.starts_with("CUSA")
                        && text[4..].bytes().all(|b| b.is_ascii_digit()))
                    .then(|| text.to_string())
                })
                .collect();
            let modified = fs::metadata(&path)
                .and_then(|m| m.modified())
                .unwrap_or(SystemTime::UNIX_EPOCH);
            let key = match source {
                Source::Automatic => name,
                Source::Legacy => format!("legacy/{name}"),
                Source::Safety => format!("safety/{name}"),
            };
            result.push(Snapshot {
                key,
                path,
                source,
                files: files.len(),
                bytes: files.values().sum(),
                titles,
                modified,
            });
        }
    }
    result.sort_by(|a, b| {
        b.time_label()
            .cmp(&a.time_label())
            .then_with(|| b.modified.cmp(&a.modified))
            .then_with(|| b.key.cmp(&a.key))
    });
    result
}

/// Shared with automatic backup: an OS lock is released even after a crashed launcher.
pub(crate) fn lock(user: &Path) -> io::Result<File> {
    checked(user)?;
    fs::create_dir_all(user)?;
    let path = user.join(".save-restore.lock");
    checked(&path)?;
    let file = OpenOptions::new()
        .read(true)
        .write(true)
        .create(true)
        .truncate(false)
        .open(path)?;
    file.try_lock_exclusive()
        .map_err(|_| invalid("Another save backup or restore is in progress"))?;
    Ok(file)
}

/// Check real native processes as well as the launcher's own game handle.
pub fn ensure_game_closed() -> io::Result<()> {
    #[cfg(windows)]
    {
        let output = process_output("powershell.exe", &["-NoProfile", "-NonInteractive", "-Command", "$ErrorActionPreference='Stop'; if (@(Get-Process | Where-Object {$_.ProcessName -in @('spartanport-probe','gow3-probe')}).Count) { Write-Output 'GAME_RUNNING' }"])?;
        if output.contains("GAME_RUNNING") {
            return Err(invalid("Close the game before restoring saves."));
        }
    }
    #[cfg(target_os = "linux")]
    {
        for entry in fs::read_dir("/proc")?.flatten() {
            if !entry
                .file_name()
                .to_string_lossy()
                .bytes()
                .all(|b| b.is_ascii_digit())
            {
                continue;
            }
            match fs::read_link(entry.path().join("exe")) {
                Ok(path)
                    if path
                        .file_name()
                        .is_some_and(|n| n == "spartanport-probe" || n == "gow3-probe") =>
                {
                    return Err(invalid("Close the game before restoring saves."))
                }
                Err(e) if e.kind() == io::ErrorKind::PermissionDenied => {
                    // Other users' games cannot share a normally permissioned user directory.
                }
                _ => {}
            }
        }
    }
    #[cfg(not(any(windows, target_os = "linux")))]
    {
        let output = process_output("ps", &["-axo", "comm="])?;
        if output.lines().any(|line| {
            Path::new(line.trim())
                .file_name()
                .is_some_and(|n| n == "spartanport-probe" || n == "gow3-probe")
        }) {
            return Err(invalid("Close the game before restoring saves."));
        }
    }
    Ok(())
}

#[cfg(not(target_os = "linux"))]
fn process_output(program: &str, args: &[&str]) -> io::Result<String> {
    use crate::core::platform::tool::{run_tool, Stderr, ToolCall, Window};
    let output = run_tool(&ToolCall {
        program: program.into(),
        args: args.iter().map(|arg| (*arg).into()).collect(),
        env: None,
        cwd: None,
        limit: Some(std::time::Duration::from_secs(15)),
        stderr: Stderr::Null,
        window: Window::Hidden,
    })
    .map_err(|e| invalid(format!("Could not check the game process: {e}")))?;
    if output.code != Some(0) {
        return Err(invalid("Could not check the game process"));
    }
    Ok(output.stdout)
}

/// Resolve only keys discovered beneath one of the three approved roots.
fn resolve(user: &Path, port: &Path, key: &str) -> io::Result<PathBuf> {
    let (root, name, safety) = if let Some(name) = key.strip_prefix("legacy/") {
        (port.join("savedata_backups"), name, false)
    } else if let Some(name) = key.strip_prefix("safety/") {
        (user.join("save-restore-safety"), name, true)
    } else {
        (user.join("savedata_backups"), key, false)
    };
    let mut parts = Path::new(name).components();
    if !matches!(parts.next(), Some(Component::Normal(_)))
        || parts.next().is_some()
        || name.starts_with('.')
    {
        return Err(invalid("Invalid backup selection"));
    }
    checked(root.parent().unwrap_or(&root))?;
    checked(&root)?;
    let folder = root.join(name);
    checked(&folder)?;
    let path = if safety {
        folder.join("savedata")
    } else {
        folder
    };
    checked(&path)?;
    if !path.is_dir() {
        return Err(invalid("Selected backup no longer exists"));
    }
    Ok(path)
}

pub fn restore(
    user: &Path,
    port: &Path,
    key: &str,
    now: Option<SystemTime>,
) -> io::Result<Option<PathBuf>> {
    restore_checked(user, port, key, now, ensure_game_closed)
}

fn publish(staging: &Path, savedata: &Path, previous: &Path) -> io::Result<()> {
    let had_saves = savedata.exists();
    if had_saves {
        fs::rename(savedata, previous)?;
    }
    if let Err(error) = fs::rename(staging, savedata) {
        if had_saves {
            fs::rename(previous, savedata).map_err(|rollback| invalid(format!("Restore failed: {error}; rollback failed: {rollback}. Original saves kept at {}", previous.display())))?;
        }
        return Err(error);
    }
    Ok(())
}

fn restore_checked(
    user: &Path,
    port: &Path,
    key: &str,
    now: Option<SystemTime>,
    idle: impl Fn() -> io::Result<()>,
) -> io::Result<Option<PathBuf>> {
    let _lock = lock(user)?;
    idle()?;
    let source = resolve(user, port, key)?;
    let savedata = user.join("savedata");
    checked(&savedata)?;
    let transaction = tempfile::Builder::new()
        .prefix(".save-restore-")
        .tempdir_in(user)?;
    let staging = transaction.path().join("staged");
    if copy_verified(&source, &staging)?.is_empty() {
        return Err(invalid("Selected backup is empty"));
    }
    let safety = if savedata.exists() {
        let root = user.join("save-restore-safety");
        checked(&root)?;
        fs::create_dir_all(&root)?;
        let pending = tempfile::Builder::new()
            .prefix(".pending-")
            .tempdir_in(&root)?;
        let files = copy_verified(&savedata, &pending.path().join("savedata"))?;
        fs::write(
            pending.path().join("sha256.json"),
            serde_json::to_vec_pretty(&files)?,
        )?;
        let moment: DateTime<Local> = now.map_or_else(Local::now, DateTime::from);
        let unique = pending
            .path()
            .file_name()
            .ok_or_else(|| invalid("Missing staging name"))?
            .to_string_lossy();
        let final_path = root.join(format!(
            "{}-{}",
            moment.format("%Y%m%d-%H%M%S"),
            unique.trim_start_matches(".pending-")
        ));
        fs::rename(pending.path(), &final_path)?;
        Some(final_path.join("savedata"))
    } else {
        None
    };
    // A failed recheck leaves the verified safety copy intact.
    idle()?;
    let previous = transaction.path().join("previous");
    if let Err(error) = publish(&staging, &savedata, &previous) {
        if previous.exists() {
            let _ = transaction.keep();
        }
        return Err(error);
    }
    Ok(safety)
}

#[cfg(test)]
mod tests {
    use super::*;
    fn put(path: &Path, content: &str) {
        fs::create_dir_all(path.parent().unwrap()).unwrap();
        fs::write(path, content).unwrap();
    }
    #[test]
    fn lists_all_sources_with_details_and_deduplicates() {
        let temp = tempfile::tempdir().unwrap();
        let user = temp.path().join("user");
        put(
            &user.join("savedata_backups/20261020-100000/1/CUSA01740/data"),
            "auto",
        );
        put(
            &temp.path().join("savedata_backups/older/1/CUSA01623/data"),
            "legacy",
        );
        put(
            &user.join("save-restore-safety/safe/savedata/1/CUSA01740/data"),
            "safety",
        );
        let entries = list(&user, temp.path());
        assert_eq!(entries.len(), 3);
        assert!(entries.iter().any(|e| e.source == Source::Legacy
            && e.bytes == 6
            && e.files == 1
            && e.titles.contains("CUSA01623")));
        let legacy = entries.iter().find(|e| e.source == Source::Legacy).unwrap();
        assert_eq!(legacy.path, temp.path().join("savedata_backups/older"));
        assert_eq!(list(&user, &user).len(), 2);
    }
    #[test]
    fn restores_legacy_and_safety_without_pruning_or_changing_source() {
        let temp = tempfile::tempdir().unwrap();
        let user = temp.path().join("user");
        let save = user.join("savedata/1/CUSA01740/data");
        let source = temp.path().join("savedata_backups/old/1/CUSA01740/data");
        put(&save, "current");
        put(&source, "old");
        let safety = restore_checked(&user, temp.path(), "legacy/old", None, || Ok(()))
            .unwrap()
            .unwrap();
        assert_eq!(fs::read_to_string(&save).unwrap(), "old");
        assert_eq!(fs::read_to_string(&source).unwrap(), "old");
        assert_eq!(
            fs::read_to_string(safety.join("1/CUSA01740/data")).unwrap(),
            "current"
        );
        let key = list(&user, temp.path())
            .into_iter()
            .find(|s| s.source == Source::Safety)
            .unwrap()
            .key;
        restore_checked(&user, temp.path(), &key, None, || Ok(())).unwrap();
        assert_eq!(fs::read_to_string(&save).unwrap(), "current");
        for _ in 0..12 {
            crate::core::save_backup::backup(&user, None).unwrap();
        }
        assert!(safety.exists());
    }
    #[test]
    fn refuses_busy_and_unsafe_sources_without_replacing_saves() {
        let temp = tempfile::tempdir().unwrap();
        let save = temp.path().join("savedata/1/CUSA01740/data");
        put(&save, "current");
        for key in [
            "../savedata",
            "legacy/../../savedata",
            "safety/../savedata",
            "",
            "missing",
        ] {
            assert!(restore_checked(temp.path(), temp.path(), key, None, || Ok(())).is_err());
        }
        assert!(
            restore_checked(temp.path(), temp.path(), "anything", None, || Err(invalid(
                "game running"
            )))
            .is_err()
        );
        assert_eq!(fs::read_to_string(save).unwrap(), "current");
    }
    #[test]
    fn rechecks_game_after_copy_and_keeps_safety_on_failure() {
        let temp = tempfile::tempdir().unwrap();
        put(&temp.path().join("savedata/a"), "current");
        put(&temp.path().join("savedata_backups/old/a"), "old");
        let checks = std::cell::Cell::new(0);
        let result = restore_checked(temp.path(), temp.path(), "old", None, || {
            checks.set(checks.get() + 1);
            if checks.get() == 1 {
                Ok(())
            } else {
                Err(invalid("game started"))
            }
        });
        assert!(result.is_err());
        assert_eq!(
            fs::read_to_string(temp.path().join("savedata/a")).unwrap(),
            "current"
        );
        assert!(list(temp.path(), temp.path())
            .iter()
            .any(|s| s.source == Source::Safety));
    }
    #[test]
    fn concurrent_restore_lock_is_released_on_drop() {
        let temp = tempfile::tempdir().unwrap();
        let first = lock(temp.path()).unwrap();
        assert!(lock(temp.path()).is_err());
        drop(first);
        assert!(lock(temp.path()).is_ok());
    }
    #[test]
    fn publication_failure_rolls_back_original_directory() {
        let temp = tempfile::tempdir().unwrap();
        let savedata = temp.path().join("savedata");
        put(&savedata.join("a"), "current");
        let previous = temp.path().join("previous");
        assert!(publish(&temp.path().join("missing-stage"), &savedata, &previous).is_err());
        assert_eq!(fs::read_to_string(savedata.join("a")).unwrap(), "current");
        assert!(!previous.exists());
    }
    #[test]
    fn empty_snapshot_cannot_erase_current_saves() {
        let temp = tempfile::tempdir().unwrap();
        put(&temp.path().join("savedata/a"), "current");
        fs::create_dir_all(temp.path().join("savedata_backups/empty")).unwrap();
        assert!(restore_checked(temp.path(), temp.path(), "empty", None, || Ok(())).is_err());
        assert_eq!(
            fs::read_to_string(temp.path().join("savedata/a")).unwrap(),
            "current"
        );
    }
    fn link_dir(link: &Path, target: &Path) {
        #[cfg(windows)]
        crate::core::platform::create_junction(link, target).unwrap();
        #[cfg(unix)]
        std::os::unix::fs::symlink(target, link).unwrap();
    }

    /// OS-managed links above the selected user root do not disable backups or restores.
    #[test]
    fn allows_linked_ancestors_above_the_user_root() {
        let temp = tempfile::tempdir().unwrap();
        let real = temp.path().join("real");
        put(&real.join("user/savedata/a"), "current");
        let alias = temp.path().join("alias");
        link_dir(&alias, &real);
        let user = alias.join("user");
        let backup = crate::core::save_backup::backup(&user, None)
            .unwrap()
            .unwrap();
        let snapshots = list(&user, temp.path());
        assert_eq!(snapshots.len(), 1);
        assert_eq!(snapshots[0].path, backup);
        put(&user.join("savedata/a"), "changed");
        restore_checked(&user, temp.path(), &snapshots[0].key, None, || Ok(())).unwrap();
        assert_eq!(
            fs::read_to_string(user.join("savedata/a")).unwrap(),
            "current"
        );
    }

    /// Root and intermediate safety-directory links must still be rejected.
    #[test]
    fn rejects_linked_backup_roots_and_safety_parents() {
        for safety in [false, true] {
            let temp = tempfile::tempdir().unwrap();
            let user = temp.path().join("user");
            put(&user.join("savedata/a"), "current");
            let external = temp.path().join("external");
            let (link, key) = if safety {
                put(&external.join("savedata/a"), "old");
                (user.join("save-restore-safety/linked"), "safety/linked")
            } else {
                put(&external.join("old/a"), "old");
                (user.join("savedata_backups"), "old")
            };
            fs::create_dir_all(link.parent().unwrap()).unwrap();
            link_dir(&link, &external);
            assert!(list(&user, temp.path()).is_empty());
            assert!(restore_checked(&user, temp.path(), key, None, || Ok(())).is_err());
            assert_eq!(
                fs::read_to_string(user.join("savedata/a")).unwrap(),
                "current"
            );
        }
    }

    /// Listing can inspect metadata even when save contents cannot be opened.
    #[cfg(windows)]
    #[test]
    fn listing_does_not_open_snapshot_contents() {
        use std::os::windows::fs::OpenOptionsExt;
        let temp = tempfile::tempdir().unwrap();
        let source = temp.path().join("savedata_backups/old/1/CUSA01623/data");
        put(&source, "save");
        let _held = OpenOptions::new()
            .read(true)
            .share_mode(0)
            .open(&source)
            .unwrap();
        assert!(inventory(source.parent().unwrap()).is_err());
        let snapshots = list(temp.path(), temp.path());
        assert_eq!(snapshots.len(), 1);
        assert_eq!(snapshots[0].files, 1);
        assert_eq!(snapshots[0].bytes, 4);
        assert!(snapshots[0].titles.contains("CUSA01623"));
    }

    #[test]
    fn rejects_linked_snapshot() {
        let temp = tempfile::tempdir().unwrap();
        let external = temp.path().join("external");
        put(&external.join("a"), "do not touch");
        let snapshots = temp.path().join("savedata_backups");
        fs::create_dir(&snapshots).unwrap();
        #[cfg(windows)]
        crate::core::platform::create_junction(&snapshots.join("linked"), &external).unwrap();
        #[cfg(unix)]
        std::os::unix::fs::symlink(&external, snapshots.join("linked")).unwrap();
        assert!(list(temp.path(), temp.path()).is_empty());
        assert!(restore_checked(temp.path(), temp.path(), "linked", None, || Ok(())).is_err());
        assert_eq!(
            fs::read_to_string(external.join("a")).unwrap(),
            "do not touch"
        );
    }
}
