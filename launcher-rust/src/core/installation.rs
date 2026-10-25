//! The installed game: the game folder and the update folder, and everything the launcher asks
//! of them.

use std::path::{Path, PathBuf};

use super::game::{
    missing_game_files, same_folder, FileState, GameFolder, GameInfo, GameProfile,
    REQUIRED_GAME_FILES, REQUIRED_UPDATE_FILES,
};
use super::store::Store;

/// What the setup and the Play button say when both folders are one.
pub const SEPARATE_FOLDERS: &str =
    "Choose two separate folders for the game and its update, or leave the update folder empty.";

/// Whether the folders are ready to play, as the files each lacks.
#[derive(Clone, Debug, PartialEq)]
pub struct Verdict {
    pub game_missing: Vec<(String, FileState)>,
    pub same_folder: bool,
    pub update_missing: Vec<(String, FileState)>,
}

impl Verdict {
    /// Whether `--play` may start the game at once; anything the Play button would refuse needs
    /// the launcher window instead.
    pub fn passes(&self) -> bool {
        self.game_missing.is_empty() && !self.same_folder && self.update_missing.is_empty()
    }
}

/// A snapshot of the game and update folders: stale after changes on disk until it is read again.
#[derive(Clone, Debug)]
pub struct Installation {
    game: String,
    update: String,
    game_path: PathBuf,
    meta: GameFolder,
    meta_path: PathBuf,
    info: Option<GameInfo>,
    has_game: bool,
    verdict: Verdict,
}

/// An empty game folder field means the current folder.
fn or_here<'a>(folder: &'a str, here: &'a Path) -> &'a Path {
    if folder.is_empty() {
        here
    } else {
        Path::new(folder)
    }
}

impl Installation {
    pub fn read(game_dir: &str, update_dir: &str) -> Self {
        Self::read_from(game_dir, update_dir, Path::new("."))
    }

    /// [`Installation::read`] with the folder an empty game field means; tests pass a temp folder.
    fn read_from(game_dir: &str, update_dir: &str, here: &Path) -> Self {
        let game_path = or_here(game_dir, here);
        let update_has_eboot =
            !update_dir.is_empty() && Path::new(update_dir).join("eboot.bin").is_file();
        let meta_path = if update_has_eboot {
            PathBuf::from(update_dir)
        } else {
            game_path.to_path_buf()
        };
        let meta = GameFolder::read(&meta_path);
        let verdict = Verdict {
            game_missing: missing_game_files(game_path, REQUIRED_GAME_FILES),
            same_folder: !update_dir.is_empty() && same_folder(update_dir, game_path),
            update_missing: if update_dir.is_empty() {
                Vec::new()
            } else {
                missing_game_files(Path::new(update_dir), REQUIRED_UPDATE_FILES)
            },
        };
        Installation {
            game: game_dir.to_string(),
            update: update_dir.to_string(),
            game_path: game_path.to_path_buf(),
            info: meta.info(),
            meta,
            meta_path,
            has_game: game_path.join("eboot.bin").is_file(),
            verdict,
        }
    }

    fn typed(&self) -> bool {
        !self.game.is_empty()
    }

    pub fn of(store: &Store) -> Self {
        Self::read(&store.text("game_dir"), &store.text("update_dir"))
    }

    /// The game folder: the current folder while the field is empty.
    pub fn game_folder(&self) -> &Path {
        &self.game_path
    }

    /// The folder the title, version and patches are read from: the update when it has an
    /// `eboot.bin`.
    pub fn meta_folder(&self) -> &Path {
        &self.meta_path
    }

    /// Title and app version of the meta folder, or `None` without its `eboot.bin`.
    pub fn info(&self) -> Option<&GameInfo> {
        self.info.as_ref()
    }

    /// The facts below need a game folder typed in: an empty field answers nothing, as the
    /// Python does.
    pub fn title_id(&self) -> Option<&str> {
        self.typed().then(|| self.meta.title_id()).flatten()
    }

    pub fn profile(&self) -> Option<&'static GameProfile> {
        self.typed().then(|| self.meta.profile()).flatten()
    }

    pub fn patchable(&self) -> bool {
        self.profile().is_some()
    }

    /// Whether the game folder holds an `eboot.bin`.
    pub fn has_game(&self) -> bool {
        self.has_game
    }

    pub fn verdict(&self) -> &Verdict {
        &self.verdict
    }

    /// A file of `sce_sys`: the update's when it has it, else the game's.
    pub fn system_file(&self, rel: &str) -> Option<PathBuf> {
        if !self.typed() {
            return None;
        }
        let update = (!self.update.is_empty()).then(|| Path::new(self.update.as_str()));
        update
            .into_iter()
            .chain([Path::new(&self.game)])
            .map(|folder| folder.join("sce_sys").join(rel))
            .find(|path| path.is_file())
    }

    pub fn trophy_package(&self) -> Option<PathBuf> {
        self.system_file("trophy/trophy00.trp")
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    fn sfo(entries: &[(&str, &str)]) -> Vec<u8> {
        let keys_off = 20 + 16 * entries.len();
        let (mut keys, mut values, mut table) = (Vec::new(), Vec::new(), Vec::new());
        for (key, value) in entries {
            let mut raw = value.as_bytes().to_vec();
            raw.push(0);
            table.extend_from_slice(&(keys.len() as u16).to_le_bytes());
            table.extend_from_slice(&0x204u16.to_le_bytes());
            table.extend_from_slice(&(raw.len() as u32).to_le_bytes());
            table.extend_from_slice(&(raw.len() as u32).to_le_bytes());
            table.extend_from_slice(&(values.len() as u32).to_le_bytes());
            keys.extend_from_slice(key.as_bytes());
            keys.push(0);
            values.extend_from_slice(&raw);
        }
        let values_off = keys_off + keys.len();
        let mut out = b"\0PSF".to_vec();
        out.extend_from_slice(&0x101u32.to_le_bytes());
        out.extend_from_slice(&(keys_off as u32).to_le_bytes());
        out.extend_from_slice(&(values_off as u32).to_le_bytes());
        out.extend_from_slice(&(entries.len() as u32).to_le_bytes());
        out.extend(table);
        out.extend(keys);
        out.extend(values);
        out
    }

    fn gow(version: &str) -> Vec<u8> {
        sfo(&[
            ("TITLE_ID", "CUSA01623"),
            ("APP_VER", version),
            ("TITLE", "God of War III"),
        ])
    }

    fn put(root: &Path, name: &str, bytes: &[u8]) {
        let path = root.join(name);
        fs::create_dir_all(path.parent().unwrap()).unwrap();
        fs::write(path, bytes).unwrap();
    }

    fn complete_dump(root: &Path) {
        put(root, "eboot.bin", b"elf");
        put(root, "sce_sys/param.sfo", &gow("01.00"));
        put(root, "sce_module/libc.prx", b"prx");
        put(root, "sce_module/libSceFios2.prx", b"prx");
    }

    fn complete_update(root: &Path) {
        put(root, "eboot.bin", b"elf");
        put(root, "sce_sys/param.sfo", &gow("01.02"));
    }

    fn s(path: &Path) -> &str {
        path.to_str().unwrap()
    }

    #[test]
    fn meta_folder_is_the_update_only_with_its_eboot() {
        let game = tempfile::tempdir().unwrap();
        let update = tempfile::tempdir().unwrap();
        assert_eq!(
            Installation::read(s(game.path()), "").meta_folder(),
            game.path()
        );
        assert_eq!(
            Installation::read(s(game.path()), s(update.path())).meta_folder(),
            game.path()
        );
        put(update.path(), "eboot.bin", b"elf");
        assert_eq!(
            Installation::read(s(game.path()), s(update.path())).meta_folder(),
            update.path()
        );
    }

    #[test]
    fn facts_come_from_the_meta_folder() {
        let game = tempfile::tempdir().unwrap();
        let update = tempfile::tempdir().unwrap();
        complete_dump(game.path());
        let base = Installation::read(s(game.path()), s(update.path()));
        assert_eq!(base.title_id(), Some("CUSA01623"));
        assert_eq!(base.info().map(|i| i.version.as_str()), Some("01.00"));
        assert!(base.patchable());
        assert!(base.has_game());
        complete_update(update.path());
        let both = Installation::read(s(game.path()), s(update.path()));
        assert_eq!(both.info().map(|i| i.version.as_str()), Some("01.02"));
        assert!(both.profile().is_some());
        let empty = tempfile::tempdir().unwrap();
        let nothing = Installation::read(s(empty.path()), "");
        assert!(nothing.info().is_none() && !nothing.patchable() && !nothing.has_game());
    }

    #[test]
    fn system_files_prefer_the_update_file_by_file() {
        let game = tempfile::tempdir().unwrap();
        let update = tempfile::tempdir().unwrap();
        put(game.path(), "sce_sys/icon0.png", b"g");
        put(game.path(), "sce_sys/pic1.png", b"g");
        put(update.path(), "sce_sys/icon0.png", b"u");
        let installation = Installation::read(s(game.path()), s(update.path()));
        assert_eq!(
            installation.system_file("icon0.png"),
            Some(update.path().join("sce_sys/icon0.png"))
        );
        assert_eq!(
            installation.system_file("pic1.png"),
            Some(game.path().join("sce_sys/pic1.png"))
        );
        assert_eq!(installation.system_file("none.png"), None);
        assert_eq!(installation.trophy_package(), None);
        put(game.path(), "sce_sys/trophy/trophy00.trp", b"g");
        assert_eq!(
            Installation::read(s(game.path()), s(update.path())).trophy_package(),
            Some(game.path().join("sce_sys/trophy/trophy00.trp"))
        );
        put(update.path(), "sce_sys/trophy/trophy00.trp", b"u");
        assert_eq!(
            Installation::read(s(game.path()), s(update.path())).trophy_package(),
            Some(update.path().join("sce_sys/trophy/trophy00.trp"))
        );
        assert_eq!(Installation::read("", "").trophy_package(), None);
    }

    #[test]
    fn verdict_lists_missing_files_per_folder() {
        let game = tempfile::tempdir().unwrap();
        let update = tempfile::tempdir().unwrap();
        complete_dump(game.path());
        put(game.path(), "eboot.bin", b"");
        put(update.path(), "sce_sys/param.sfo", b"junk");
        let verdict = Installation::read(s(game.path()), s(update.path()))
            .verdict()
            .clone();
        assert_eq!(
            verdict.game_missing,
            vec![("eboot.bin".to_string(), FileState::Empty)]
        );
        assert_eq!(
            verdict.update_missing,
            vec![
                ("eboot.bin".to_string(), FileState::Missing),
                ("sce_sys/param.sfo".to_string(), FileState::Invalid)
            ]
        );
        assert!(!verdict.same_folder);
        let without = Installation::read(s(game.path()), "");
        assert!(without.verdict().update_missing.is_empty());
    }

    #[test]
    fn an_empty_game_folder_reads_here_for_files_but_gives_no_facts() {
        let here = tempfile::tempdir().unwrap();
        complete_dump(here.path());
        put(here.path(), "sce_sys/trophy/trophy00.trp", b"t");
        let installation = Installation::read_from("", "", here.path());
        assert_eq!(installation.title_id(), None);
        assert!(installation.profile().is_none() && !installation.patchable());
        assert_eq!(installation.system_file("param.sfo"), None);
        assert_eq!(installation.trophy_package(), None);
        assert!(installation.info().is_some() && installation.has_game());
        assert!(installation.verdict().passes());
        let same = Installation::read_from("", s(here.path()), here.path());
        assert!(same.verdict().same_folder);
    }

    #[test]
    fn the_game_folder_is_here_for_an_empty_field() {
        let here = tempfile::tempdir().unwrap();
        let game = tempfile::tempdir().unwrap();
        assert_eq!(
            Installation::read_from("", "", here.path()).game_folder(),
            here.path()
        );
        assert_eq!(
            Installation::read_from(s(game.path()), "", here.path()).game_folder(),
            game.path()
        );
    }

    #[test]
    fn same_folder_counts_an_empty_game_folder_as_here() {
        assert!(Installation::read("", ".").verdict().same_folder);
        assert!(!Installation::read("", "").verdict().same_folder);
    }

    #[test]
    fn passes_is_ready_to_play() {
        let dir = tempfile::tempdir().unwrap();
        complete_dump(dir.path());
        assert!(Installation::read(s(dir.path()), "").verdict().passes());

        let empty_eboot = tempfile::tempdir().unwrap();
        complete_dump(empty_eboot.path());
        put(empty_eboot.path(), "eboot.bin", b"");
        assert!(!Installation::read(s(empty_eboot.path()), "")
            .verdict()
            .passes());

        assert!(!Installation::read(s(dir.path()), s(dir.path()))
            .verdict()
            .passes());
        assert!(same_folder(dir.path(), dir.path().join(".")));

        let update = tempfile::tempdir().unwrap();
        assert!(!Installation::read(s(dir.path()), s(update.path()))
            .verdict()
            .passes());
        complete_update(update.path());
        assert!(Installation::read(s(dir.path()), s(update.path()))
            .verdict()
            .passes());
        assert!(!same_folder(dir.path(), update.path()));
    }
}
