# Drop Pre-2.8.0 Compatibility Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The Rust launcher and the Windows package carry nothing for installs older than 2.8.0: no `core::legacy`, no old-name migrations, no Python cleanup, no `God of War III.exe` / `Play God of War III.exe` copies.

**Architecture:** Pure removal, one boundary per task: the package (Task 1), the environment (Task 2), the launcher settings and `Paths` (Task 3), Steam/patches/runner (Task 4), then startup and the module itself with the docs (Task 5). Each task first changes a test to the new behavior, sees it fail, removes the code, sees it pass. `core::legacy` stays compiled (all `pub`, so no dead-code warnings) until Task 5 deletes it.

**Tech Stack:** Rust stable (`rust-toolchain.toml`), iced 0.14 with `iced_test`; bash packaging script.

**Spec:** `docs/specs/2026-10-25-drop-legacy-compat-design.md`.

## Global Constraints

- Work in the worktree `.claude/worktrees/launcher-identity` on branch `worktree-launcher-identity`. Rust commands run in `launcher-rust/`. Stage files by name, never `git add -A`.
- `src/core/` stays free of iced. No `unwrap`/`expect` outside tests (`clippy.toml`). Each task passes `cargo fmt --check` and `cargo test` in `launcher-rust/` before its commit, with no warnings (an import left unused by a removal is removed in the same task). Task 5 also runs `cargo clippy --all-targets -- -D warnings`.
- The PC nearly freezes on cargo builds: `RUST_TEST_THREADS=4`, never two cargo commands at once.
- The launcher Python (`launcher/`, `run.py`, `scripts/*.py`), `packaging/linux/PKGBUILD`, and the data-format migrations of spec §6 (`mods.rs` `<game>-mods`, `controls.rs` `RAGE_LEGACY_INPUT`, `graphics_options.rs` launch choices) do not change.
- `--install-update` (route, `startup::install_update`, `core::update`) does not change.
- Commit subjects: `Refactor(Launcher): …`, `Test(Launcher): …`, `Ci(Packaging): …`, `Docs(Launcher): …`; imperative, ≤ 72 characters; message from `caveman:caveman-commit`; no AI attribution lines, ever.
- Code and comments in English, as the crate; the notes in the two specs in Portuguese, as those files. Plan line numbers may be off by a few lines: locate by symbol.
- Commit steps of Tasks 2-4 run in `launcher-rust/` (their paths are relative to it); Tasks 1 and 5 commit from the worktree root.

## Review Focus

- A `--install-update` call with the wrong argument count or on Linux is now the plain window (`Route::Launcher`), and `prepare` runs its remaining steps for it; nothing in them moves player files. Pinned by the route test in Task 5.
- A developer shell that still exports `GOW3_DATA_DIR` sees the launcher ignore it (the data folder stays the port's). Pinned in Task 2.
- A `settings.json` only in `%APPDATA%/gow3-launcher` is not read and not copied: the launcher starts on defaults and writes no file until a save. Pinned in Task 3.
- A Steam library holding a pre-rename "God of War III" non-Steam shortcut gets a second, "SpartanPort", entry and keeps the old one untouched. Pinned in Task 4.
- A data folder holding `gow3.ini` but no `spartanport.ini`: a run does not copy it. Pinned in Task 4.

---

### Task 1: The Windows package carries only the new names

**Files:**
- Modify: `launcher-rust/tests/packaging.rs:3` (import) and `the_windows_package_ships_the_rust_launcher` (lines 10-30)
- Modify: `packaging/windows/package.sh:22-26`
- Modify: `docs/specs/2026-10-23-rust-cutover-design.md` §4.1 (lines ~227-231)

**Interfaces:**
- Produces: nothing in Rust; `tests/packaging.rs` no longer imports `core::legacy`.

- [ ] **Step 1: Change the test.** Delete `use spartanport_launcher::core::legacy::{LEGACY_EXE, LEGACY_PLAY_EXE};`. In the `needed` list remove `"legacy::LEGACY_EXE"`, `"legacy::LEGACY_PLAY_EXE"`, `LEGACY_EXE`, `LEGACY_PLAY_EXE`. In the `banned` list add `"God of War III"`.

- [ ] **Step 2: Run it to see it fail**

Run (in `launcher-rust/`): `cargo test --test packaging`
Expected: FAIL `package.sh still has God of War III`.

- [ ] **Step 3: Remove the copies.** In `package.sh` delete the three comment lines starting `# The same launcher and windowless start under their former names` and the two `cp "$dest/…" "$dest/… God of War III.exe"` lines. In the cutover spec §4.1, after the bullet about `Play SpartanPort.exe`, add: `- Desde 2026-10-25 o pacote não grava mais as cópias com os nomes antigos (`docs/specs/2026-10-25-drop-legacy-compat-design.md`).`

- [ ] **Step 4: Run to see it pass**

Run: `cargo test --test packaging`, then `bash -n packaging/windows/package.sh` at the worktree root.
Expected: PASS; `bash -n` prints nothing.

- [ ] **Step 5: Commit**

```bash
git add launcher-rust/tests/packaging.rs packaging/windows/package.sh docs/specs/2026-10-23-rust-cutover-design.md
git commit   # e.g. "Ci(Packaging): stop shipping the old-name launcher copies"
```

---

### Task 2: The old `GOW3_*` variables are not forwarded

**Files:**
- Modify: `launcher-rust/src/core/env.rs` (import line 14; doc and body of `start_up_environment`, lines 30-40; test `windows_hides_the_amd_switchable_layer`, ~line 302)
- Modify: `launcher-rust/tests/startup.rs` (delete `every_route_forwards_old_names_before_paths`, ~line 416, and `vars` if nothing else uses it)
- Modify: `launcher-rust/tests/runner.rs:216-227` (`clean_environment`)

**Interfaces:**
- Produces: `start_up_environment(env: &mut IndexMap<String, String>, windows: bool)` keeps its signature; it only sets the AMD layer default.

- [ ] **Step 1: Write the failing test** in `env.rs`'s `tests`, and trim `windows_hides_the_amd_switchable_layer` to its `VK_LOADER_LAYERS_DISABLE` assertion (start it from `variables(&[])`, drop the "old names are forwarded" part)

```rust
#[test]
fn the_old_gow3_names_are_not_forwarded() {
    for windows in [true, false] {
        let mut env = variables(&[("GOW3_DATA_DIR", "/old")]);
        start_up_environment(&mut env, windows);
        assert_eq!(env.get("SPARTAN_DATA_DIR"), None, "{windows}");
        assert_eq!(env.get("GOW3_DATA_DIR").map(String::as_str), Some("/old"));
    }
}
```

- [ ] **Step 2: Run it to see it fail**

Run: `cargo test --lib env::tests::`
Expected: FAIL in `the_old_gow3_names_are_not_forwarded` (`SPARTAN_DATA_DIR` is `Some("/old")`).

- [ ] **Step 3: Remove the forwarding.** Drop the `forward_environment` import and call. The doc of `start_up_environment` reads: "What the launcher changes in its own environment before anything reads it: on Windows, the AMD switchable-graphics layer is disabled unless the player set the variable. The game and the probes inherit it." Delete the `tests/startup.rs` test named above (and `vars` when unused). In `tests/runner.rs` the prefix list becomes `["SPARTAN_", "FAKE_PROBE_"]` and the doc "(`SPARTAN_*` and the fake probe's)".

- [ ] **Step 4: Run to see it pass**

Run: `cargo test --lib env::tests::`, then `cargo test`.
Expected: all PASS, no warnings.

- [ ] **Step 5: Commit**

```bash
git add src/core/env.rs tests/startup.rs tests/runner.rs
git commit   # e.g. "Refactor(Launcher): stop forwarding the GOW3_ variables"
```

---

### Task 3: The launcher settings no longer come from `gow3-launcher`

**Files:**
- Modify: `launcher-rust/src/core/settings.rs` (module doc line 1; `migrate_portable_settings` and `copy_if_missing`, lines 82-99; `SettingsFile` doc ~106; the call in `read_app_settings` ~119; tests `a_legacy_settings_file_counts_as_read` and `portable_migration_copies_only_settings`, ~274-321)
- Modify: `launcher-rust/src/core/paths.rs` (`LEGACY_CONFIG_DIR` lines 7-8; field `legacy_config_dir`; `home`/`base` in `from_env` ~60-67; `legacy_config_file` ~87-89; tests `legacy_config_file_is_appdata_gow3_launcher_on_windows`, `an_empty_home_falls_back_to_the_current_folder`, `legacy_config_file_is_xdg_config_gow3_launcher_on_unix`)
- Modify: `launcher-rust/src/ui/app.rs:172` (comment)

**Interfaces:**
- Produces: `Paths` without `legacy_config_dir` / `legacy_config_file()`; `read_app_settings(paths: &Paths) -> (Settings, SettingsFile)` unchanged in signature, reads only `paths.config_file()`.

- [ ] **Step 1: Write the failing test.** Replace `a_legacy_settings_file_counts_as_read` with the test below (same setup) and delete `portable_migration_copies_only_settings`.

```rust
#[test]
fn a_settings_file_at_the_old_location_is_ignored() {
    let dir = tempfile::tempdir().unwrap();
    let data = dir.path().join("data");
    let legacy = dir.path().join("legacy").join("gow3-launcher");
    fs::create_dir_all(&legacy).unwrap();
    fs::write(legacy.join("settings.json"), r#"{"ui_language": "de"}"#).unwrap();
    let config_home = dir.path().join("legacy").to_string_lossy().into_owned();
    let paths = Paths::from_env(dir.path(), &|k| match k {
        "SPARTAN_DATA_DIR" => Some(data.to_string_lossy().into_owned()),
        "APPDATA" | "XDG_CONFIG_HOME" => Some(config_home.clone()),
        _ => None,
    });
    let (settings, file) = read_app_settings(&paths);
    assert_eq!(file, SettingsFile::Missing);
    assert_ne!(settings.get_str("ui_language"), "de");
    assert!(!paths.config_file().exists());
}
```

- [ ] **Step 2: Run it to see it fail**

Run: `cargo test --lib settings::tests::`
Expected: FAIL (`file` is `Read`; the old file was copied).

- [ ] **Step 3: Remove the migration.** Delete `migrate_portable_settings`, `copy_if_missing` and the call. Module doc: "The launcher's own settings (`settings.json`): defaults, load and save." `SettingsFile` doc: "What `read_app_settings` found at `paths.config_file()`." In `paths.rs` delete the constant, the field, the `home`/`base` lines (only the field used them), `legacy_config_file` and its three tests; drop `platform` from the imports if it becomes unused. `app.rs:172`: "What the load found at `config_file`."

- [ ] **Step 4: Run to see it pass**

Run: `cargo test --lib -- settings::tests:: paths::tests::`, then `cargo test`.
Expected: all PASS, no warnings.

- [ ] **Step 5: Commit**

```bash
git add src/core/settings.rs src/core/paths.rs src/ui/app.rs
git commit   # e.g. "Refactor(Launcher): read settings only from the data folder"
```

---

### Task 4: Steam, patches and the runner forget the old name

**Files:**
- Modify: `launcher-rust/src/core/steam.rs:334-336` (`named` in `add_to_steam`) and test `a_shortcut_from_before_the_rename_is_updated_not_doubled` (~line 734)
- Modify: `launcher-rust/src/core/patches.rs:28` (`LEGACY_PATCH_FILE`), `:277` (the exclusion), test `an_older_builds_patch_file_left_by_an_update_is_not_external` (~line 2364)
- Modify: `launcher-rust/src/runner/mod.rs:36` (`use crate::core::legacy;`) and `:342` (`legacy::copy_game_settings(data);`)
- Test: `launcher-rust/tests/runner.rs` (new test after `a_relative_data_dir_is_from_the_port`)

**Interfaces:**
- Produces: no new names; after this task nothing outside `src/core/legacy.rs`, `src/startup.rs` and `src/core/mod.rs` names `legacy`.

- [ ] **Step 1: Write the failing tests**

`steam.rs`: rename the test to `a_shortcut_named_after_the_old_title_is_left_alone`; same setup; replace the assertions after `add(&f)` with

```rust
assert!(!added.updated);
// …names collected as today…
assert_eq!(names, ["God of War III", "SpartanPort"]);
```

`patches.rs`: rename the test to `a_file_named_like_an_older_builds_patch_is_external`; write the file by its literal name and expect both:

```rust
fs::write(dir.path().join("God_of_War_III_Remastered.xml"), EXTERNAL).unwrap();
fs::write(dir.path().join("extra.xml"), EXTERNAL).unwrap();
// …found / files as today…
assert_eq!(files, HashSet::from(["God_of_War_III_Remastered.xml", "extra.xml"]));
```

`tests/runner.rs`:

```rust
#[test]
fn a_run_does_not_take_the_old_game_settings() {
    let port = Port::in_folder("port");
    let host = FakeHost::default();
    let mut out = Vec::new();
    let mut program = port.program(&[]);
    program.env.insert("SPARTAN_DATA_DIR".into(), "data2".into());
    fs::create_dir_all(port.root.join("data2")).unwrap();
    fs::write(port.root.join("data2/gow3.ini"), "[OldName]\nmarker=gow3\n").unwrap();
    let code = run_with(Port::context(&program, &mut out), &host);
    assert_eq!(code, 0, "{}", String::from_utf8_lossy(&out));
    let ini = fs::read_to_string(port.root.join("data2/spartanport.ini")).unwrap_or_default();
    assert!(!ini.contains("marker=gow3"), "{ini}");
}
```

- [ ] **Step 2: Run them to see them fail**

Run: `cargo test --lib -- steam::tests::a_shortcut_named patches::tests::a_file_named`, then `cargo test --test runner a_run_does_not_take`.
Expected: the three FAIL (old shortcut reused; the old patch file excluded; `marker=gow3` copied into `spartanport.ini`).

- [ ] **Step 3: Remove the old-name handling.** `steam.rs`: `named` compares `AppName` with `crate::APP_NAME` only. `patches.rs`: delete `LEGACY_PATCH_FILE` and `|| name == LEGACY_PATCH_FILE`; `CUSA01623.xml` stays excluded. `runner/mod.rs`: delete the call and the `use`.

- [ ] **Step 4: Run to see them pass**

Run: the Step 2 commands, then `cargo test`.
Expected: all PASS, no warnings.

- [ ] **Step 5: Commit**

```bash
git add src/core/steam.rs src/core/patches.rs src/runner/mod.rs tests/runner.rs
git commit   # e.g. "Refactor(Launcher): drop the old name from Steam, patches and runs"
```

---

### Task 5: Startup without migration; `core::legacy` deleted; docs

**Files:**
- Modify: `launcher-rust/src/startup.rs` (`Route::Play`/`Route::Launcher` lines 29 and 36; `route_on` lines 73-77; `migrates` lines 80-85; `prepare` lines 171-181; `move_from_legacy_name` lines 202-219; the `legacy` import)
- Modify: `launcher-rust/src/main.rs:41-45` and `:83-86`
- Modify: `launcher-rust/src/core/mod.rs:18`
- Delete: `launcher-rust/src/core/legacy.rs`
- Modify: `launcher-rust/tests/startup.rs` (import line 12; `arguments_route_to_their_roles`, `run_and_play_are_their_own_routes`; delete `the_update_installer_does_not_migrate`)
- Modify: `README.md:22-26`, `README.pt-BR.md:21-25`, `README.es.md:22-26`; `docs/specs/2026-10-25-drop-legacy-compat-design.md` (state line 3)

**Interfaces:**
- Consumes: Tasks 2-4 (no other `legacy::` users left).
- Produces: `Route::Play` and `Route::Launcher` as unit variants; `pub fn prepare(paths: &Paths)`; `migrates` gone.

- [ ] **Step 1: Change the route tests.** Drop `migrates` from the import. Every `Route::Launcher { migrate: … }` becomes `Route::Launcher` and every `Route::Play { migrate: true }` becomes `Route::Play`; the comment "and it still does not migrate" becomes "Not the installer elsewhere, so it is the window." Delete `the_update_installer_does_not_migrate`.

- [ ] **Step 2: Run them to see them fail**

Run: `cargo test --test startup`
Expected: compile error, `expected value, found struct variant `Route::Launcher``.

- [ ] **Step 3: Remove the migration and the module.** `Play` and `Launcher` lose their field (doc of `Launcher` stays "Anything else: the window."); `route_on` returns them without `migrate`; delete `migrates`, `move_from_legacy_name` and the `use` of `legacy`; `prepare(paths: &Paths)` keeps `installed`, the settings load, the Linux desktop integration and the update-folder cleanup in today's order, and its doc drops "the old name's files". `main.rs` matches `Route::Play` / `Route::Launcher` and calls `startup::prepare(paths)` / `startup::prepare(&context.paths)`. Delete `pub mod legacy;` and `src/core/legacy.rs`.

- [ ] **Step 4: Run to see them pass**

Run: `cargo test --test startup`, then `cargo test`.
Expected: all PASS, no warnings.

- [ ] **Step 5: Docs.** Replace the "Formerly gow3_pc" blockquote in the three READMEs with (English; pt-BR and es carry the same content in their language, as the current notes do):

```markdown
> **Formerly gow3_pc.** Versions before 2.8.0 do not carry over: download 2.8.0 from the
> [releases page](https://github.com/FelipePrado0/SpartanPort/releases) and install it fresh. On
> Windows, copy `user/`, `mods/`, `spartanport.ini` and `settings.json` from the old folder to keep
> your saves and settings. On Linux, replace the AppImage; data from before the rename
> (`~/.local/share/gow3`) is not moved.
```

In the spec, line 3 becomes `Data: 2026-10-25. Estado: implementado (branch worktree-launcher-identity, 2026-10-26).`; add a `## 9. Desvios` section listing what the code does differently, or "Nenhum."

- [ ] **Step 6: Final gate**

Run in `launcher-rust/`: `cargo fmt --check`, `cargo clippy --all-targets -- -D warnings`, `cargo test`. At the worktree root:

```bash
grep -rn "legacy::\|GOW3_\|gow3-launcher\|God of War III\.exe\|migrate_portable\|copy_game_settings" launcher-rust/src launcher-rust/tests packaging/windows
```

Expected: clean, all PASS; the grep prints nothing.

- [ ] **Step 7: Commit**

At the worktree root:

```bash
git add launcher-rust/src/startup.rs launcher-rust/src/main.rs launcher-rust/src/core/mod.rs launcher-rust/tests/startup.rs README.md README.pt-BR.md README.es.md docs/specs/2026-10-25-drop-legacy-compat-design.md
git rm launcher-rust/src/core/legacy.rs
git commit   # e.g. "Refactor(Launcher): delete the old-name compatibility module"
```
