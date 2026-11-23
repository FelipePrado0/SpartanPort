//! The trophy notification sound (#1243): `volume` and `muted` of
//! `<saves folder>/custom_trophy/notifications.ini`, the file the in-game menu's Trophies >
//! Notifications page reads and writes (gpu/shim/spartanport_trophy_toast_model.cpp).

use std::io;
use std::path::{Path, PathBuf};

use indexmap::IndexMap;

use super::ini::write_lines;

/// The game's defaults when the file or a key is missing.
pub const DEFAULT_VOLUME: u32 = 80;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TrophySound {
    /// 0 to 100.
    pub volume: u32,
    pub muted: bool,
}

impl Default for TrophySound {
    fn default() -> Self {
        Self {
            volume: DEFAULT_VOLUME,
            muted: false,
        }
    }
}

pub fn file(user_dir: &Path) -> PathBuf {
    user_dir.join("custom_trophy").join("notifications.ini")
}

/// What the file holds, read as the game reads it: a bad or missing value keeps the default.
pub fn read(user_dir: &Path) -> TrophySound {
    let mut sound = TrophySound::default();
    let Ok(text) = std::fs::read_to_string(file(user_dir)) else {
        return sound;
    };
    for line in text
        .lines()
        .filter(|line| !line.trim_start().starts_with('#'))
    {
        let Some((key, value)) = line.split_once('=') else {
            continue;
        };
        let value = value.trim();
        match key.trim() {
            "volume" => {
                if let Ok(volume) = value.parse::<i64>() {
                    sound.volume = volume.clamp(0, 100) as u32;
                }
            }
            "muted" => match value {
                "1" | "true" => sound.muted = true,
                "0" | "false" => sound.muted = false,
                _ => {}
            },
            _ => {}
        }
    }
    sound
}

/// Writes the two keys in place; the file's other lines (theme, texts, timing) stay.
pub fn write(user_dir: &Path, sound: TrophySound) -> io::Result<()> {
    let path = file(user_dir);
    if let Some(dir) = path.parent() {
        std::fs::create_dir_all(dir)?;
    }
    // Only a missing file starts empty: an unreadable one fails rather than lose its other lines.
    let lines: Vec<String> = match std::fs::read_to_string(&path) {
        Ok(text) => text.lines().map(str::to_string).collect(),
        Err(error) if error.kind() == io::ErrorKind::NotFound => Vec::new(),
        Err(error) => return Err(error),
    };
    let mut edited = IndexMap::new();
    edited.insert("volume".to_string(), sound.volume.min(100).to_string());
    edited.insert("muted".to_string(), u8::from(sound.muted).to_string());
    write_lines(&path, &edited, &lines)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn reads_and_writes_only_the_sound_keys() {
        let dir = tempfile::tempdir().unwrap();
        assert_eq!(read(dir.path()), TrophySound::default());
        std::fs::create_dir_all(dir.path().join("custom_trophy")).unwrap();
        std::fs::write(
            file(dir.path()),
            "theme=card\n# volume: 0-100\nvolume=150\nmuted=1\nhold_seconds=3\n",
        )
        .unwrap();
        assert_eq!(
            read(dir.path()),
            TrophySound {
                volume: 100,
                muted: true
            }
        );
        write(
            dir.path(),
            TrophySound {
                volume: 25,
                muted: false,
            },
        )
        .unwrap();
        assert_eq!(
            std::fs::read_to_string(file(dir.path())).unwrap(),
            "theme=card\n# volume: 0-100\nvolume=25\nmuted=0\nhold_seconds=3\n"
        );
        let latin1 = tempfile::tempdir().unwrap();
        std::fs::create_dir_all(latin1.path().join("custom_trophy")).unwrap();
        std::fs::write(file(latin1.path()), b"title=Tr\xf3feu\nvolume=10\n").unwrap();
        assert!(write(latin1.path(), TrophySound::default()).is_err());
        assert_eq!(
            std::fs::read(file(latin1.path())).unwrap(),
            b"title=Tr\xf3feu\nvolume=10\n"
        );
        let fresh = tempfile::tempdir().unwrap();
        write(
            fresh.path(),
            TrophySound {
                volume: 0,
                muted: true,
            },
        )
        .unwrap();
        assert_eq!(
            read(fresh.path()),
            TrophySound {
                volume: 0,
                muted: true
            }
        );
    }
}
