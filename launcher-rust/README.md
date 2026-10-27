# SpartanPort launcher (Rust)

Rust rewrite of the Python/Tk launcher. `src/core/` holds the launcher logic and never depends on the UI crate; `src/ui/` holds the graphical front end.

## Toolchain

Install Rust with [rustup](https://rustup.rs). The stable channel is selected by `rust-toolchain.toml`.

- Windows: install Visual Studio Build Tools with the "Desktop development with C++" workload.
- Ubuntu: `sudo apt-get install build-essential pkg-config libwayland-dev libxkbcommon-dev`

## Tests

```
cd launcher-rust
cargo test
```

The tests in `tests/dump.rs` need a real game folder (the patches of the real `eboot.bin`, the pause menu and the glyph WADs against the Python's) and return at once without one. Point `SPARTAN_GAME_DIR` at the dump to run them; the dump is only read:

```
cd launcher-rust
SPARTAN_GAME_DIR=<game folder> cargo test --test dump
```

On Windows PowerShell: `$env:SPARTAN_GAME_DIR = "<game folder>"; cargo test --test dump`. Drop `--test dump` to run the whole suite with the dump. These tests also run the Python scripts (the interpreter is set under Environment below) to compare their files with the Rust ones.

## Environment

- `SPARTAN_RELEASE_TAG`: read at build time, not at run time. The release tag the binary is (`windows-v2.8.0-issue1-test2`); without it the binary is the stable of `Cargo.toml`. The release workflows set it from their tag. Name test tags `<system>-v<version>-<issue>-test<N>`: the launcher offers the next build of the same series. A local test build: `$env:SPARTAN_RELEASE_TAG = 'windows-v2.8.0-issue1-test2'; cargo build --release --locked` (PowerShell) or `SPARTAN_RELEASE_TAG=windows-v2.8.0-issue1-test2 cargo build --release --locked`.
- `SPARTAN_PORT_DIR`: the port folder (with `bin/`) and, unless `SPARTAN_DATA_DIR` is set, the data folder. Defaults to the executable's folder; point it at the repository root to run a `cargo` build.
- `SPARTANPORT_PYTHON`: the Python the dump tests compare against (the frozen Python launcher and runner). Defaults to `py -3` on Windows and `python3` elsewhere. The exe itself never runs Python.
