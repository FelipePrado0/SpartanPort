# SpartanPort launcher (Rust)

Rust rewrite of the Python/Tk launcher. `src/core/` holds the launcher logic and never depends on the UI crate; `src/ui/` holds the graphical front end.

## Toolchain

Install Rust with [rustup](https://rustup.rs). `rust-toolchain.toml` pins the toolchain version; rustup downloads it on the first build.

- Windows: install Visual Studio Build Tools with the "Desktop development with C++" workload.
- Ubuntu: `sudo apt-get install build-essential pkg-config libwayland-dev libxkbcommon-dev`
- macOS: see below.

## macOS

On macOS you can build and run the launcher, but not the game: `main` doesn't build the game's runtime for macOS yet, so PLAY doesn't start anything. Apple Silicon support is in progress in [#798](https://github.com/FelipePrado0/SpartanPort/pull/798).

1. Install the Xcode Command Line Tools if they are missing: `xcode-select --install`.
2. Install Rust with [rustup](https://rustup.rs). Use the official installer rather than Homebrew's `rustup`, which puts `cargo` outside your `PATH`:

   ```bash
   curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
   ```

   Choose the standard installation. It adds `~/.cargo/bin` to your `PATH` through `~/.zshenv`.
3. Open a new terminal and check that `cargo` is available:

   ```bash
   cargo --version
   ```

   If the command is not found, add `. "$HOME/.cargo/env"` to `~/.zshenv` and open a new terminal.
4. Build and run from the repository root. The first build downloads the pinned toolchain and takes a few minutes:

   ```bash
   cd launcher-rust
   cargo build --release
   cd ..
   ./launcher-rust/target/release/spartanport-launcher
   ```

   `cargo run --release` in `launcher-rust/` builds and runs in one step. `cargo clean` there removes the build output (`launcher-rust/target/`).

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

- `SPARTAN_PORT_DIR`: the port folder (with `bin/`) and, unless `SPARTAN_DATA_DIR` is set, the data folder. Defaults to the executable's folder; point it at the repository root to run a `cargo` build.
- `SPARTANPORT_PYTHON`: the Python the dump tests compare against (the frozen Python launcher and runner). Defaults to `py -3` on Windows and `python3` elsewhere. The exe itself never runs Python.
