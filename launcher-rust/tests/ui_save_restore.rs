#![allow(clippy::unwrap_used, clippy::expect_used)]
mod common;

use common::{browsed, dump_with_patches, state};
use spartanport_launcher::ui::app::{self, Message};
use spartanport_launcher::ui::pages::game::BackupMessage;

fn put(path: &std::path::Path, text: &str) {
    std::fs::create_dir_all(path.parent().unwrap()).unwrap();
    std::fs::write(path, text).unwrap();
}

#[test]
fn lists_legacy_and_safety_and_warns_for_a_different_title() {
    let temp = tempfile::tempdir().unwrap();
    let mut state = state(temp.path());
    state.lang = "en".into();
    browsed(&mut state, "game_dir", &dump_with_patches(temp.path()));
    let user = state.paths.user_dir("");
    put(
        &temp
            .path()
            .join("savedata_backups/20261020-100000/1/CUSA01740/a"),
        "legacy",
    );
    put(
        &user.join("save-restore-safety/20261021-100000/savedata/1/CUSA01623/a"),
        "safety",
    );
    let _ = app::update(&mut state, Message::Backup(BackupMessage::Refresh));
    assert!(state.backups.contains(&"legacy/20261020-100000".into()));
    assert!(state.backups.contains(&"safety/20261021-100000".into()));
    let _ = app::update(
        &mut state,
        Message::Backup(BackupMessage::Choose("legacy/20261020-100000".into())),
    );
    let _ = app::update(&mut state, Message::Backup(BackupMessage::Restore));
    let text = &state.modal.as_ref().unwrap().text;
    assert!(text.contains("different game ID"));
    assert!(text.contains("savedata_backups"));
    assert!(text.contains("20261020-100000"));
    // Merely opening and confirming the offer has not restored anything.
    assert!(!user.join("savedata").exists());
}

#[test]
fn pending_restore_blocks_play_and_selection_changes_and_recovers_from_failure() {
    let temp = tempfile::tempdir().unwrap();
    let mut state = state(temp.path());
    state.lang = "zh".into();
    state.backup_choice = "old".into();
    state.restoring_saves = true;
    let _ = app::update(
        &mut state,
        Message::Backup(BackupMessage::Choose("other".into())),
    );
    let _ = app::update(&mut state, Message::Backup(BackupMessage::Refresh));
    assert_eq!(state.backup_choice, "old");
    let _ = app::update(&mut state, Message::Play);
    assert!(state.game.is_idle());
    assert!(state.modal.as_ref().unwrap().text.contains("恢复完成"));
    let _ = app::update(
        &mut state,
        Message::Backup(BackupMessage::Restored(Err("disk full".into()))),
    );
    assert!(!state.restoring_saves);
    assert!(state.modal.as_ref().unwrap().text.contains("disk full"));
}
