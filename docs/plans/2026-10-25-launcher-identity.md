# Launcher Identity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The Rust launcher knows which release it is from the tag compiled into it, with no `release.json` stamp; the version list keeps only releases from 2.8.0 on; a tester on `issue1-test1` is offered `issue1-test2`.

**Architecture:** `build.rs` forwards `SPARTAN_RELEASE_TAG` into the binary; `core/release.rs` turns it into the `ReleaseId` the whole launcher uses (`ReleaseId::this()`), filters the list by a version floor and adds the "next build of the series" rule. `core/update.rs` loses the stamp reader. The bottom bar, the version button and `--version` all show that one identity. The two release workflows pass the tag to `cargo build` instead of writing and shipping a stamp; the smoke tests check `--version` against the tag.

**Tech Stack:** Rust stable (`rust-toolchain.toml`), iced 0.14 with `iced_test`, `serde_json`; GitHub Actions; bash and PowerShell smoke tests.

**Spec:** `docs/specs/2026-10-25-launcher-identity-design.md`. Context: `docs/specs/2026-10-22-version-menu-design.md` (the menu, the stamp and the rule this plan replaces).

## Global Constraints

- Work in the worktree `.claude/worktrees/launcher-identity` on branch `worktree-launcher-identity`. Rust tasks run in `launcher-rust/`; packaging tasks at the worktree root. Stage files by name, never `git add -A`.
- `src/core/` stays free of iced. No `unwrap`/`expect` outside tests (`clippy.toml` denies them). Each Rust task passes `cargo fmt --check` and `cargo test` in `launcher-rust/` before its commit; Task 7 runs `cargo clippy --all-targets -- -D warnings` as the final gate.
- The PC nearly freezes on cargo builds: keep `~/.cargo/config.toml` (jobs=8) and `RUST_TEST_THREADS=4`; never run two cargo commands at once.
- Commit subjects follow the branch: `Feat(Launcher): …`, `Refactor(Launcher): …`, `Test(Launcher): …`, `Ci(Packaging): …`, `Docs(Launcher): …`; imperative, ≤ 72 characters. The message comes from the `caveman:caveman-commit` skill; no AI attribution lines, ever.
- Spec values: build-time variable `SPARTAN_RELEASE_TAG` (empty counts as absent); `pub const VERSION: &str = env!("CARGO_PKG_VERSION")`; `pub const FIRST_RUST_VERSION: &str = "2.8.0"`; channel from the tag (`build.is_some()` → `PreRelease`); `--version` prints `SpartanPort 2.8.0` or `SpartanPort 2.8.0-beta2`; the bar shows `v2.8.0 · Windows` or `v2.8.0 · beta2 · Linux`; the series offer shows `· issue1-test2 available` (build only); no new i18n keys.
- Tests never compare against literal text for this binary's identity: they use `ReleaseId::this()`, so they pass whether or not the variable was set at build time.
- Code and comments in English, as the crate; the spec notes added to the two older specs in Portuguese, as those files.

## Review Focus

- A macOS or Linux tag (`macos-v2.8.0-apple-silicon-test2`, `linux-v2.8.1-test1`) compiled in: the system prefix is dropped and the build kept. Pinned in Task 1.
- A whitespace-only `SPARTAN_RELEASE_TAG`: no `-v<digit>`, so the identity is the fallback, never a panic. Pinned in Task 1.
- `issue1-test10` offered over `issue1-test9`: the number compares numerically, not as text. Pinned in Task 3.
- A build whose trailing digits overflow `u64` (`t99999999999999999999`): `series()` returns `u64::MAX`, no panic. Pinned in Task 3.
- The Windows smoke test's `version.log` ends in CRLF: the comparison trims before matching the tag. Pinned in Task 7.

---

### Task 1: The identity compiled from the tag (`build.rs`, `lib.rs`, `core/release.rs`)

**Files:**
- Modify: `launcher-rust/build.rs` (`main`, lines 4-9)
- Modify: `launcher-rust/src/lib.rs:7`
- Modify: `launcher-rust/src/core/release.rs` (`impl ReleaseId`, after `fallback` at line 90-97; `tests` after line 310)
- Delete test: `launcher-rust/tests/core_boundary.rs:35-39` (`version_is_the_crates`)

**Interfaces:**
- Produces: `ReleaseId::from_tag(tag: Option<&str>) -> ReleaseId`, `ReleaseId::this() -> ReleaseId`, `ReleaseId::plain(&self) -> String`. `ReleaseId::fallback()` keeps its meaning (`Release · VERSION`, no build). `VERSION` keeps its name and type.

- [ ] **Step 1: Write the failing tests** in the `tests` module of `src/core/release.rs`, after `the_fallback_is_this_version`

```rust
#[test]
fn the_identity_comes_from_the_tag() {
    for tag in [None, Some(""), Some("   "), Some("nonsense"), Some("windows-2.8.0")] {
        assert_eq!(ReleaseId::from_tag(tag), ReleaseId::fallback(), "{tag:?}");
    }
    assert_eq!(
        ReleaseId::from_tag(Some("windows-v2.8.0")),
        id(Channel::Release, "2.8.0", None)
    );
    assert_eq!(
        ReleaseId::from_tag(Some("windows-v2.8.0-beta2")),
        id(Channel::PreRelease, "2.8.0", Some("beta2"))
    );
    assert_eq!(
        ReleaseId::from_tag(Some("linux-v2.8.1-issue1-test2")),
        id(Channel::PreRelease, "2.8.1", Some("issue1-test2"))
    );
    assert_eq!(
        ReleaseId::from_tag(Some("macos-v2.8.0-apple-silicon-test2")),
        id(Channel::PreRelease, "2.8.0", Some("apple-silicon-test2"))
    );
}

#[test]
fn this_binary_is_the_tag_it_was_built_with() {
    assert_eq!(
        ReleaseId::this(),
        ReleaseId::from_tag(option_env!("SPARTAN_RELEASE_TAG"))
    );
}

#[test]
fn plain_is_the_tag_without_its_system() {
    assert_eq!(id(Channel::Release, "2.8.0", None).plain(), "2.8.0");
    assert_eq!(
        id(Channel::PreRelease, "2.8.0", Some("beta2")).plain(),
        "2.8.0-beta2"
    );
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run (in `launcher-rust/`): `cargo test --lib release::tests::`
Expected: compile error, `no function or associated item named `from_tag``.

- [ ] **Step 3: Implement**

- `src/lib.rs:7`: `pub const VERSION: &str = env!("CARGO_PKG_VERSION");`
- `build.rs`, at the start of `main` (before the icon):
  ```rust
  println!("cargo:rerun-if-env-changed=SPARTAN_RELEASE_TAG");
  if let Ok(tag) = std::env::var("SPARTAN_RELEASE_TAG") {
      if !tag.is_empty() {
          println!("cargo:rustc-env=SPARTAN_RELEASE_TAG={tag}");
      }
  }
  ```
  Update the module doc: the file also forwards the release tag into the binary.
- `src/core/release.rs`, in `impl ReleaseId`:
  - `pub fn from_tag(tag: Option<&str>) -> ReleaseId`: `tag.and_then(|t| ReleaseId::parse(t, false))`, then set `channel = PreRelease` when `build.is_some()`; `None` → `fallback()`. Doc: "The identity of `tag`; None, empty or invalid is `fallback()`."
  - `pub fn this() -> ReleaseId`: `Self::from_tag(option_env!("SPARTAN_RELEASE_TAG"))`. Doc: "The release this binary is: the tag it was built with, else `fallback()`."
  - `pub fn plain(&self) -> String`: `version` or `version-build`. Doc: "`2.8.0`, `2.8.0-beta2`: the tag without its system prefix."
- Delete `version_is_the_crates` from `tests/core_boundary.rs` (lines 35-39): it became tautological.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cargo test --lib release::tests::` then `cargo test --test core_boundary`
Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
cargo fmt --check
git add build.rs src/lib.rs src/core/release.rs tests/core_boundary.rs
git commit   # message from caveman:caveman-commit, e.g. "Feat(Launcher): compile the release tag into the binary"
```

---

### Task 2: The installed release is this binary (`core/update.rs`)

**Files:**
- Modify: `launcher-rust/src/core/update.rs` (imports lines 11 and 17; `stamp_path`/`installed_release`/`read_stamp` lines 79-111; `GitHubReleases` lines 405-440; tests lines 738-784)
- Modify: `launcher-rust/src/ui/update.rs:76-79` (doc of `installed`)
- Modify: `launcher-rust/tests/common/releases.rs:38`
- Modify: `launcher-rust/tests/ui_home.rs:592-599`
- Modify: `launcher-rust/tests/ui_pages.rs` (lines 2418, 2465, 2479, 2484, 2557)

**Interfaces:**
- Consumes: `ReleaseId::this()` (Task 1).
- Produces: `GitHubReleases::installed()` returns `ReleaseId::this()`. `stamp_path`, `installed_release` and the `appdir` field are gone.

- [ ] **Step 1: Write the failing test** in the `tests` module of `src/core/update.rs`, replacing `the_stamp_names_the_installed_release`, `a_missing_or_broken_stamp_is_this_version` and `the_stamp_sits_beside_the_launcher` (delete those three)

```rust
#[test]
fn the_host_is_this_binary() {
    let host = GitHubReleases::from_env(Path::new("port"));
    assert_eq!(host.installed(), ReleaseId::this());
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cargo test --lib update::tests::the_host_is_this_binary`
Expected: FAIL only if `SPARTAN_RELEASE_TAG` was set at build time; otherwise it passes by accident. Go on either way: Step 3 removes the stamp code the three deleted tests covered, and the compile must stay clean.

- [ ] **Step 3: Implement**

- `GitHubReleases::installed` returns `ReleaseId::this()`. Remove `stamp_path`, `installed_release`, `read_stamp`, the `appdir` field and its `var("APPDIR")` line, and `APPDIR` from the struct doc. Drop the now-unused imports `serde_json::Value` and `Channel` (keep `ReleaseId` and `release`).
- `src/ui/update.rs:76`: doc of `Updater::installed` becomes "The release this binary is."
- `tests/common/releases.rs:38`: `installed: ReleaseId::this()`.
- `tests/ui_home.rs:592-599`: `state_installed(dir.path(), ReleaseId::this())` and `ui.find(format!("{} ▾", ReleaseId::this().label("en")).as_str())`.
- `tests/ui_pages.rs`: replace every `VERSION` use (lines 2465, 2479, 2484, 2557) with `ReleaseId::this().version` (line 2557: `let installed = Release { id: ReleaseId::this(), ..release("x", None, false) }`), and drop the `use spartanport_launcher::VERSION;` at 2418. The comment at 2566 says "The channel compiled in is frozen; the list's follows the API."

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cargo test --lib update::tests::` then `cargo test --test ui_home --test ui_pages`
Expected: all PASS; no warnings about unused imports.

- [ ] **Step 5: Commit**

```bash
cargo fmt --check
git add src/core/update.rs src/ui/update.rs tests/common/releases.rs tests/ui_home.rs tests/ui_pages.rs
git commit   # e.g. "Refactor(Launcher): read the installed release from the binary"
```

---

### Task 3: The next build of the installed series (`core/release.rs`, `ui/update.rs`)

**Files:**
- Modify: `launcher-rust/src/core/release.rs` (`impl ReleaseId`; after `newer_stable` at lines 176-182; `tests`)
- Modify: `launcher-rust/src/ui/update.rs:14` (import) and `:74-77` (`Updater::newer`)

**Interfaces:**
- Produces: `ReleaseId::series(&self) -> Option<(&str, u64)>`; `pub fn newer<'a>(releases: &'a [Release], installed: &ReleaseId) -> Option<&'a Release>`. `newer_stable` stays as the first rule.

- [ ] **Step 1: Write the failing tests** in the `tests` module of `src/core/release.rs`

```rust
#[test]
fn a_build_has_a_series_and_a_number() {
    let pre = |build| id(Channel::PreRelease, "2.8.0", Some(build));
    assert_eq!(pre("issue1-test2").series(), Some(("issue1-test", 2)));
    assert_eq!(pre("hotfix").series(), Some(("hotfix", 0)));
    assert_eq!(pre("teste3").series(), Some(("teste", 3)));
    assert_eq!(pre("t99999999999999999999").series(), Some(("t", u64::MAX)));
    assert_eq!(id(Channel::Release, "2.8.0", None).series(), None);
}

#[test]
fn the_next_build_of_the_installed_series_is_newer() {
    let list = vec![
        rel("windows-v2.8.0-issue2-test5", true, "d"),
        rel("windows-v2.8.0-issue1-test2", true, "d"),
        rel("windows-v2.8.0-issue1-test1", true, "d"),
        rel("windows-v2.8.0", false, "d"),
    ];
    let test = |n: u64| {
        let build = format!("issue1-test{n}");
        id(Channel::PreRelease, "2.8.0", Some(build.as_str()))
    };
    assert_eq!(newer(&list, &test(1)).unwrap().id.build.as_deref(), Some("issue1-test2"));
    assert!(newer(&list, &test(2)).is_none());
    assert!(newer(&list, &test(3)).is_none());
    assert!(newer(&list, &id(Channel::Release, "2.8.0", None)).is_none());
    assert!(newer(&[], &test(1)).is_none());
    let ten = vec![rel("windows-v2.8.0-issue1-test10", true, "d")];
    assert_eq!(newer(&ten, &test(9)).unwrap().id.build.as_deref(), Some("issue1-test10"));
}

#[test]
fn a_newer_stable_beats_the_next_build() {
    let list = vec![
        rel("windows-v2.9.0", false, "d"),
        rel("windows-v2.8.0-issue1-test2", true, "d"),
    ];
    let test1 = id(Channel::PreRelease, "2.8.0", Some("issue1-test1"));
    assert_eq!(newer(&list, &test1).unwrap().id.version, "2.9.0");
}

#[test]
fn the_series_goes_on_to_a_higher_version_and_never_back() {
    let test1 = id(Channel::PreRelease, "2.8.0", Some("issue1-test1"));
    let up = vec![rel("windows-v2.9.0-issue1-test2", true, "d")];
    assert_eq!(newer(&up, &test1).unwrap().id.version, "2.9.0");
    let back = vec![rel("windows-v2.7.0-issue1-test9", true, "d")];
    assert!(newer(&back, &test1).is_none());
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cargo test --lib release::tests::`
Expected: compile error, `cannot find function `newer`` / `no method named `series``.

- [ ] **Step 3: Implement**

- `ReleaseId::series`: `build.trim_end_matches(|c: char| c.is_ascii_digit())` is the series; the trimmed tail parses as `u64` (empty → 0, overflow → `u64::MAX`, as `version_tuple` does). Doc: "The series and number of a build: `issue1-test2` is `("issue1-test", 2)`, `hotfix` is `("hotfix", 0)`. None for a stable."
- `pub fn newer<'a>(releases: &'a [Release], installed: &ReleaseId) -> Option<&'a Release>`: `newer_stable` first; else, with `installed.series()` as `(series, n)`, the first release whose `series()` has the same series and whose `(version_tuple(&version), number)` is greater than `(version_tuple(&installed.version), n)`; whatever its channel. Doc: "The stable above the installed one; else the next build of the installed series."
- `src/ui/update.rs`: import `newer` instead of `newer_stable`; `Updater::newer` calls `newer(self.releases(), &self.installed)`; its doc: "The newest listed stable above the installed release, else the next build of its series."

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cargo test --lib release::tests::` then `cargo test --test ui_home --test ui_pages`
Expected: all PASS (the UI tests see no change yet: their installed ids are stables).

- [ ] **Step 5: Commit**

```bash
cargo fmt --check
git add src/core/release.rs src/ui/update.rs
git commit   # e.g. "Feat(Launcher): offer the next build of the installed series"
```

---

### Task 4: The list keeps releases from 2.8.0 on (`core/release.rs`)

**Files:**
- Modify: `launcher-rust/src/core/release.rs` (`STAMP_FILE` lines 8-9, `stamp_asset` lines 11-19, `parse_entry` line 130, `tests` lines 211 and 312-428)

**Interfaces:**
- Produces: `pub const FIRST_RUST_VERSION: &str = "2.8.0"`. `STAMP_FILE` and `stamp_asset` are gone; `parse_entry` no longer needs a stamp asset.

- [ ] **Step 1: Rewrite the list tests** in the `tests` module of `src/core/release.rs`

- `const WIN: [&str; 1] = ["spartanport-windows.zip"];`
- `mixed(extra_linux_asset)`: `stable` assets are `spartanport-windows.zip`, `spartanport-x86_64.AppImage` (+ `.sha256` when asked); entries become `windows-v2.9.0` (stable, 2026-05-01), `windows-v2.8.0-issue209-test1` (pre, 2026-04-01, `&WIN`), the draft `windows-v2.8.0-test1`, `python_era` = `windows-v2.7.0` (stable, zip only, 2026-01-01), `linux-v2.8.1-test1` (pre, `["spartanport-x86_64.AppImage"]`), `nonsense`, `no_date` = `windows-v2.8.0` without `published_at`.
- Delete `with_the_other_stamp`. `windows_takes_the_zip` parses `mixed(false)` for Windows and expects 2 entries: `2.9.0` (url `https://x.test/spartanport-windows.zip`, page `https://x.test/windows-v2.9.0`) then `PreRelease 2.8.0 issue209-test1`. `linux_takes_the_appimage_not_its_checksum` parses `mixed(true)` for Linux and expects `2.9.0` (url `https://x.test/spartanport-x86_64.AppImage`) then `PreRelease 2.8.1 test1`.
- `linux_has_no_package_while_the_release_has_no_appimage`: tag `windows-v2.8.0`, assets `["spartanport-windows.zip"]`.
- `the_list_is_newest_first`: tags `windows-v2.8.0` (01-01), `windows-v2.10.0` (03-01), `windows-v2.9.0` (02-01); expected `["2.10.0", "2.9.0", "2.8.0"]`.
- New test:

```rust
#[test]
fn releases_below_the_first_rust_one_are_not_listed() {
    let body = json!([
        entry("windows-v2.8.0", false, "2026-10-25T00:00:00Z", &WIN),
        entry(
            "windows-v2.7.0-rust-launcher-test1",
            true,
            "2026-10-24T00:00:00Z",
            &["spartanport-windows.zip", "release-windows.json"]
        ),
        entry("windows-v2.7.0", false, "2026-10-22T00:00:00Z", &WIN),
    ]);
    let versions: Vec<_> = parse_releases(&body, true)
        .unwrap()
        .into_iter()
        .map(|r| r.id.version)
        .collect();
    assert_eq!(versions, ["2.8.0"]);
    assert_eq!(FIRST_RUST_VERSION, "2.8.0");
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cargo test --lib release::tests::`
Expected: compile error on `FIRST_RUST_VERSION`; after a stub, `releases_below_the_first_rust_one_are_not_listed` and the `mixed` tests FAIL (2.7.0 listed, 2.9.0 without a stamp asset dropped).

- [ ] **Step 3: Implement**

- `pub const FIRST_RUST_VERSION: &str = "2.8.0";` with the doc "The first package with the Rust launcher: nothing below it is listed, so no Python-era release is."
- `parse_entry`: replace line 130 (`asset_url(assets, |name| name == stamp_asset(windows))?;`) with a return of `None` when `version_tuple(&id.version) < version_tuple(FIRST_RUST_VERSION)`.
- Delete `STAMP_FILE` and `stamp_asset`; update the module doc (line 1) to "the tag's identity, the list GitHub returns, the version floor and the newer rules".

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cargo test --lib release::tests::` then `cargo test`
Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
cargo fmt --check
git add src/core/release.rs
git commit   # e.g. "Feat(Launcher): list releases from 2.8.0 on instead of stamped ones"
```

---

### Task 5: The button and the menu show the series offer (`ui/version_menu.rs`)

**Files:**
- Modify: `launcher-rust/src/ui/version_menu.rs` (`button_text` lines 93-97; the pre-release rows around lines 206-208)
- Test: `launcher-rust/tests/ui_home.rs` (after `the_version_button_offers_a_newer_stable`, line 632)

**Interfaces:**
- Consumes: `Updater::newer()` (Task 3), `Channel`.

- [ ] **Step 1: Write the failing test** in `tests/ui_home.rs`

```rust
#[test]
fn the_version_button_offers_the_next_build_of_the_series() {
    let dir = tempfile::tempdir().unwrap();
    let installed = ReleaseId {
        channel: Channel::PreRelease,
        version: "2.8.0".into(),
        build: Some("issue1-test1".into()),
    };
    let list = vec![
        release(Channel::PreRelease, "2.8.0", Some("issue2-test5")),
        release(Channel::PreRelease, "2.8.0", Some("issue1-test2")),
        release(Channel::Release, "2.8.0", None),
    ];
    let mut state = state_installed(dir.path(), installed);
    ready_with(&mut state, list.clone());
    let mut ui = simulator(app::view(&state));
    assert!(ui
        .find("Pre-release · 2.8.0 · issue1-test1 · issue1-test2 available ▾")
        .is_ok());
    drop(ui);
    open_menu(&mut state);
    let mut ui = simulator(app::view(&state));
    assert!(ui.find("new").is_ok());
    drop(ui);

    // A stable user sees the same list with nothing new.
    let mut state = state_installed(dir.path(), release(Channel::Release, "2.8.0", None).id);
    ready_with(&mut state, list);
    open_menu(&mut state);
    let mut ui = simulator(app::view(&state));
    assert!(ui.find("Release · 2.8.0 ▾").is_ok());
    assert!(ui.find("new").is_err());
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cargo test --test ui_home the_version_button_offers_the_next_build_of_the_series`
Expected: FAIL at the first `find` (the button says `· 2.8.0 available` and the row carries no "new").

- [ ] **Step 3: Implement**

- `button_text`: the offered text is `newer.id.version` when `newer.id.channel == Channel::Release`, else `newer.id.build` (falling back to the version when there is none); still through `tr_fmt(lang, "{} available", ..)`.
- `menu`: the pre-release rows pass `newer == Some(&r.id)` instead of `false`, as the stable rows do.
- Update the doc of `button_text` ("with the newer stable or the next build of the series noted").

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cargo test --test ui_home`
Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
cargo fmt --check
git add src/ui/version_menu.rs tests/ui_home.rs
git commit   # e.g. "Feat(Launcher): mark the next build of the series in the menu"
```

---

### Task 6: The bar and `--version` show the identity (`ui/app.rs`, `main.rs`)

**Files:**
- Modify: `launcher-rust/src/ui/app.rs:749-755` (`version_label`) and `:767` (the bar's call); drop `use crate::VERSION;` at line 53 if nothing else uses it
- Modify: `launcher-rust/src/main.rs:16` and `:31`
- Test: `launcher-rust/tests/ui_navigation.rs:336-347`, `launcher-rust/tests/startup.rs:149-161`

**Interfaces:**
- Produces: `pub fn version_label(installed: &ReleaseId, windows: bool) -> String`.

- [ ] **Step 1: Write the failing tests**

`tests/ui_navigation.rs`, replacing the body of `the_bar_names_the_version_and_the_system` (keep the test name; import `ReleaseId` and `Channel` from `spartanport_launcher::core::release`, drop `VERSION`):

```rust
let stable = ReleaseId { channel: Channel::Release, version: "2.8.0".into(), build: None };
let beta = ReleaseId { channel: Channel::PreRelease, version: "2.8.0".into(), build: Some("beta2".into()) };
assert_eq!(app::version_label(&stable, true), "v2.8.0 · Windows");
assert_eq!(app::version_label(&stable, false), "v2.8.0 · Linux");
assert_eq!(app::version_label(&beta, false), "v2.8.0 · beta2 · Linux");
let dir = tempfile::tempdir().unwrap();
let state = state_in(dir.path());
let mut ui = simulator(app::view(&state));
let system = if cfg!(windows) { "Windows" } else { "Linux" };
let shown = format!("v{} · {system}", ReleaseId::this().short());
// ...the rest of the test unchanged (it finds `shown` in the bar).
```

`tests/startup.rs:149-161`, `version_prints_the_name_and_the_version`: the expected line becomes `format!("SpartanPort {}", ReleaseId::this().plain())`; delete the comment about `core_boundary.rs`.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cargo test --test ui_navigation the_bar_names --test startup version_prints`
Expected: compile error on `version_label`'s arity (ui_navigation); `startup` passes by accident without a tag. Go on.

- [ ] **Step 3: Implement**

- `app.rs`: `pub fn version_label(installed: &ReleaseId, windows: bool) -> String` returns `format!("v{} · {system}", installed.short())`; the doc's example becomes `v2.8.0 · Windows` or `v2.8.0 · beta2 · Windows`; `bar` passes `state.updater.installed()`.
- `main.rs`: `Route::Version => println!("{APP_NAME} {}", ReleaseId::this().plain())`; import `ReleaseId` from `spartanport_launcher::core::release` and drop `VERSION` from the `use`.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cargo test --test ui_navigation --test startup`
Expected: all PASS.

- [ ] **Step 5: Verify the tag reaches the binary** (PowerShell, in `launcher-rust/`; each line rebuilds only this crate)

```powershell
$env:SPARTAN_RELEASE_TAG = 'windows-v2.8.0-beta2'; cargo run -q -- --version
Remove-Item Env:SPARTAN_RELEASE_TAG; cargo run -q -- --version
```

Expected: `SpartanPort 2.8.0-beta2`, then `SpartanPort 2.8.0`.

- [ ] **Step 6: Commit**

```bash
cargo fmt --check
git add src/ui/app.rs src/main.rs tests/ui_navigation.rs tests/startup.rs
git commit   # e.g. "Feat(Launcher): show the compiled release in the bar and --version"
```

---

### Task 7: Workflows, packaging, smoke tests and docs

**Files:**
- Modify: `.github/workflows/windows-zip.yml` (input `stamp` lines 15-18; build step lines 58-62; lines 63-82 `setup-python` + Stamp; artifact line 92; smoke env line 107; publish line 129)
- Modify: `.github/workflows/linux-appimage.yml` (lines 15-18; Stamp lines 40-53; AppImage step lines 54-57; artifact line 63; smoke env line 83; publish line 107)
- Modify: `packaging/windows/package.sh:43`, `packaging/linux/build.sh:41`
- Modify: `packaging/windows/smoke-test.ps1` (lines 14-25 and 59-61), `packaging/linux/smoke-test.sh` (lines 28-34 and 45)
- Delete: `packaging/release_stamp.py`, `tests/test_release_stamp.py`
- Modify: `launcher-rust/README.md` (Environment section), `docs/specs/2026-10-22-version-menu-design.md` (§3.3, §3.4, §5), `docs/specs/2026-10-23-rust-cutover-design.md:159-160`

- [ ] **Step 1: Workflows**

Both workflows: delete the `stamp` input and the Stamp step; delete the `release-*.json` line from the artifact `path` and the `$( [[ -f dist/release-*.json ]] && echo … )` from the `gh release upload` line; the smoke job's `SPARTAN_RELEASE_TAG` becomes `${{ inputs.tag }}`. `windows-zip.yml`: delete the `setup-python` step and its two comment lines; the "Build the launcher" step gains

```yaml
        env:
          SPARTAN_RELEASE_TAG: ${{ inputs.tag }}
```

`linux-appimage.yml`: the same `env` on the "AppImage" step (its `packaging/linux/build.sh` runs `cargo build`).

- [ ] **Step 2: Packaging scripts**

Delete `packaging/windows/package.sh:43` and `packaging/linux/build.sh:41` (the `release.json` copies). `git rm packaging/release_stamp.py tests/test_release_stamp.py`.

- [ ] **Step 3: Smoke tests**

`packaging/windows/smoke-test.ps1`: delete the stamp block (lines 14-25, comment included). Replace the `-notmatch` branch at lines 59-61 with

```powershell
} else {
    $printed = (Get-Content -Raw (Join-Path $work 'version.log')).Trim()
    if ($env:SPARTAN_RELEASE_TAG) {
        $expected = 'SpartanPort ' + ($env:SPARTAN_RELEASE_TAG -replace '^[a-z]+-v', '')
        if ($printed -ne $expected) {
            $failed += "SpartanPort.exe --version printed '$printed', not '$expected'"
        }
    } elseif ($printed -notmatch '^SpartanPort \d+\.\d+\.\d+') {
        $failed += 'SpartanPort.exe --version did not print "SpartanPort <version>"'
    }
}
```

Update the header comment (line 3): "the Rust launcher answers --version with the release tag it was built with".

`packaging/linux/smoke-test.sh`: delete lines 28-34 (the `release.json` block). Replace line 45 with

```bash
if [[ -n ${SPARTAN_RELEASE_TAG:-} ]]; then
    expected="SpartanPort ${SPARTAN_RELEASE_TAG#*-v}"
    [[ $version == "$expected" ]] || { echo "FAIL: --version printed '$version', not '$expected'" >&2; exit 1; }
else
    grep -qE '^SpartanPort [0-9]+\.[0-9]+\.[0-9]+$' <<<"$version" || { echo 'FAIL: --version printed no version line' >&2; exit 1; }
fi
```

- [ ] **Step 4: Docs**

- `launcher-rust/README.md`, Environment: add `- SPARTAN_RELEASE_TAG: read at build time, not at run time. The release tag the binary is (`windows-v2.8.0-beta2`); without it the binary is the stable of `Cargo.toml`. The release workflows set it from their tag. A local test build: `$env:SPARTAN_RELEASE_TAG = 'windows-v2.8.0-beta2'; cargo build --release --locked` (PowerShell) or `SPARTAN_RELEASE_TAG=windows-v2.8.0-beta2 cargo build --release --locked`.`
- `docs/specs/2026-10-22-version-menu-design.md`: one line under each of §3.3, §3.4 and §5: `Substituído em 2026-10-25 por `docs/specs/2026-10-25-launcher-identity-design.md`: a identidade é compilada da tag, sem carimbo; a lista usa o piso 2.8.0; a próxima build da série também é "nova".`
- `docs/specs/2026-10-23-rust-cutover-design.md:159-160`: append `Desde 2026-10-25 o filtro é o piso de versão 2.8.0 (`docs/specs/2026-10-25-launcher-identity-design.md`).`
- `docs/specs/2026-10-25-launcher-identity-design.md`: "Estado: implementado (branch worktree-launcher-identity, 2026-10-25)" plus a "## 10. Desvios" section listing what the code does differently, if anything.

- [ ] **Step 5: Verify**

Run at the worktree root:

```bash
grep -rn "stamp\|release\.json\|release-windows\|release-linux\|setup-python\|release_stamp" .github/workflows packaging tests launcher-rust/src launcher-rust/tests launcher-rust/README.md
bash -n packaging/linux/smoke-test.sh packaging/linux/build.sh packaging/windows/package.sh
SPARTAN_RELEASE_TAG=windows-v2.8.0-beta2 bash -c 'echo "SpartanPort ${SPARTAN_RELEASE_TAG#*-v}"'
```

Expected: the grep prints nothing (the `tests/dump.rs` `glyphs/stamp.json` lines are a different stamp and may appear; nothing else); `bash -n` prints nothing; the echo prints `SpartanPort 2.8.0-beta2`.

```powershell
$e = $null; [void][System.Management.Automation.Language.Parser]::ParseFile((Resolve-Path packaging/windows/smoke-test.ps1), [ref]$null, [ref]$e); $e.Count
$env:SPARTAN_RELEASE_TAG = 'windows-v2.8.0-beta2'; 'SpartanPort ' + ($env:SPARTAN_RELEASE_TAG -replace '^[a-z]+-v', ''); Remove-Item Env:SPARTAN_RELEASE_TAG
```

Expected: `0`, then `SpartanPort 2.8.0-beta2`.

Final gate, in `launcher-rust/`: `cargo fmt --check`, `cargo clippy --all-targets -- -D warnings`, `cargo test`. Expected: clean, all PASS.

- [ ] **Step 6: Commit**

```bash
git add .github/workflows/windows-zip.yml .github/workflows/linux-appimage.yml packaging/windows/package.sh packaging/linux/build.sh packaging/windows/smoke-test.ps1 packaging/linux/smoke-test.sh launcher-rust/README.md docs/specs/2026-10-22-version-menu-design.md docs/specs/2026-10-23-rust-cutover-design.md docs/specs/2026-10-25-launcher-identity-design.md
git commit   # e.g. "Ci(Packaging): compile the release tag in instead of stamping"
```
