//! What the window shows from the disk follows the settings it is read from, through one table: a
//! view is read again if and only if one of its settings changed or the cause names it.
//!
//! Out of the table: the Mods page's own reads and edits update the mod resolutions directly (that
//! dependency is on the Mods view, not on a setting), and showing a page reads it again.

use iced::Task;

use crate::core::store::Store;
use crate::ui::app::{refresh_installation, Message, State};
use crate::ui::home;
use crate::ui::pages::mods::{self, ModsMessage};
use crate::ui::pages::{game, performance, trophies};

/// Something the window shows that is read from the disk, or from another setting.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum View {
    /// `State::installation`: the game and update folders.
    Installation,
    /// The Home facts: the saves and the last start's patch report.
    Home,
    /// The game patches of the Game page.
    Patches,
    /// The save backups of the Game page.
    Backups,
    /// The render resolutions the enabled mods add.
    ModResolutions,
    /// The Aspect ratio box, which follows the render resolution.
    Aspect,
    /// The lists of the Mods page.
    Mods,
    /// The list of the Trophies page.
    Trophies,
}

/// Why the views are read again.
#[derive(Debug, Clone, Copy)]
pub enum Cause<'a> {
    /// These settings changed.
    Keys(&'a [&'a str]),
    /// The window opens.
    Start,
    /// The window is rebuilt in another language or zoom.
    Rebuild,
}

/// A line of the table: the view, the settings it is read from, and whether the start and a
/// rebuild read it.
struct Row {
    view: View,
    keys: &'static [&'static str],
    start: bool,
    rebuild: bool,
}

const fn row(view: View, keys: &'static [&'static str], start: bool, rebuild: bool) -> Row {
    Row {
        view,
        keys,
        start,
        rebuild,
    }
}

/// The views in the order they are read: the installation first, since the views after it ask it.
const TABLE: [Row; 8] = [
    row(View::Installation, &["game_dir", "update_dir"], true, true),
    row(
        View::Home,
        &["game_dir", "update_dir", "user_dir"],
        true,
        true,
    ),
    row(View::Patches, &["game_dir", "update_dir"], true, true),
    row(View::Backups, &["user_dir"], true, false),
    row(
        View::ModResolutions,
        &["mods_enabled", "mods_dir", "game_dir"],
        true,
        false,
    ),
    row(View::Aspect, &["render_resolution"], true, false),
    row(
        View::Mods,
        &["mods_dir", "game_dir", "update_dir", "patches_dir"],
        true,
        true,
    ),
    row(
        View::Trophies,
        &["game_dir", "update_dir", "user_dir"],
        false,
        false,
    ),
];

impl Row {
    fn follows(&self, cause: Cause<'_>) -> bool {
        match cause {
            Cause::Keys(keys) => keys.iter().any(|key| self.keys.contains(key)),
            Cause::Start => self.start,
            Cause::Rebuild => self.rebuild,
        }
    }
}

/// Reads again, in table order, every view `cause` reaches; what is read on a thread arrives as
/// the messages of the returned task.
pub fn follow(state: &mut State, cause: Cause<'_>) -> Task<Message> {
    let reads: Vec<Task<Message>> = TABLE
        .iter()
        .filter(|row| row.follows(cause))
        .map(|row| read(state, row.view, cause))
        .collect();
    Task::batch(reads)
}

/// The values of the settings `view` is read from, in table order. A read on a thread carries
/// them, and one made for values since typed over is dropped when it arrives.
pub fn inputs(view: View, store: &Store) -> Vec<String> {
    TABLE
        .iter()
        .filter(|row| row.view == view)
        .flat_map(|row| row.keys)
        .map(|key| store.text(key))
        .collect()
}

/// Reads `view` again: at once, except the Trophies list, and the Mods lists unless the window
/// opens or is rebuilt, which are read on a thread.
fn read(state: &mut State, view: View, cause: Cause<'_>) -> Task<Message> {
    match view {
        View::Installation => refresh_installation(state),
        View::Home => home::read_facts(state),
        View::Patches => game::refresh_game_patches(state),
        View::Backups => game::refresh_backups(state),
        View::ModResolutions => performance::refresh_mod_resolutions(state),
        View::Aspect => performance::resolution_chosen(state),
        View::Mods => {
            return match cause {
                Cause::Start | Cause::Rebuild => {
                    let lists = mods::load(&state.paths, &state.store, &state.installation);
                    mods::update(state, ModsMessage::Loaded(Box::new(lists)))
                }
                Cause::Keys(_) => mods::refresh(state),
            };
        }
        View::Trophies => return trophies::refresh(state),
    }
    Task::none()
}
