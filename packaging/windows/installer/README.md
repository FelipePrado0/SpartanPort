# Windows online installer

A small, standalone `God-of-War-III-Setup.exe` that downloads the latest stable
`gow3-windows.zip` from this project's official GitHub Releases. The release
already includes Python and its runtime libraries; players do not install a
compiler, Python, MSYS2, or a separate Visual C++ runtime to use this installer.

The setup UI follows the Windows language (Portuguese or English). Use
`--pt-BR` or `--en` to override it. It installs for the current user, without
administrator privileges, by default in `%LOCALAPPDATA%\Programs\gow3`.

## Player flow

1. Download and open `God-of-War-III-Setup.exe`.
2. Keep the default destination or choose an **empty** folder on another drive.
3. Optionally select your own PS4 game folder (containing `eboot.bin`,
   `sce_sys/param.sfo`, `sce_module/libc.prx` and `sce_module/libSceFios2.prx`).
4. Click **Install now**. Setup downloads, checks SHA-256, and extracts the port.
5. The Start menu shortcuts and optional desktop shortcut are created. The
   launcher opens automatically unless that option is unchecked.
6. In the launcher, choose the game folder if you skipped step 3, then play.

The game is supplied separately. The installer does not download game data,
drivers, or modify Windows' page file. A Vulkan 1.3 GPU with an appropriate
driver is required. The launcher performs its usual game/hardware checks;
installation success alone does not establish gameplay compatibility.

Setup checks x64 Windows 10 1903+ and AVX before downloading. .NET Framework
4.8, included with supported Windows 10/11 updates, runs the setup program.
The executable is unsigned; distribution builds can be signed by the maintainer.

## Existing installs, failures and cancellation

Setup only accepts an empty destination. Use the launcher's **Advanced > Check
for updates** for an existing installation, retaining its saves and settings.

The download is checked against the SHA-256 digest reported by GitHub's release
asset API. A missing digest, wrong length or hash, unsafe archive path, symbolic
link, duplicate path, user-data entry, or incomplete package stops installation.
Every asset redirect is checked before the request: only HTTPS on the official
repository and GitHub's release-asset hosts is accepted. Metadata redirects are
refused. The digest verifies integrity against GitHub's release metadata; it is
not an independent audit or signature of the downloaded port.
The executable never accepts a custom feed or arbitrary download URL from CLI.

Extraction happens in a randomly named `.gow3-setup-*` sibling of the target.
The payload is moved into place only after validation, with a final check that
the destination is still empty. Cancel or retry after a normal failure removes
that staging folder; a new attempt downloads the ZIP again. Downloads do not
resume across attempts. An unexpected shutdown can leave a staging folder;
it can be removed once setup is no longer running.
If temporary cleanup fails after the installation has committed, setup reports
the leftover path in the log and keeps the successful installation available.

If a game folder is selected, setup merges only `game_dir` into the launcher's
settings, preserving existing keys and backing up an existing file. Language is
set only for a new configuration. If this step fails, the installed launcher can
still be opened and configured manually. Shortcuts are also best effort; a
shortcut failure does not remove the installed port.
Existing shortcut files are preserved; setup selects a numbered name if needed.

Setup logs live in `%LOCALAPPDATA%\gow3-setup\logs`. There is no registered
uninstaller in this version. To remove the port, first back up its `user` folder
and any wanted settings, then delete its installation folder and shortcuts.

## Build and verify

From the repository root in Windows PowerShell 5.1:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File packaging/windows/installer/build.ps1 -Test
```

The script uses the .NET Framework C# compiler already installed with Windows;
it needs no additional SDK, packages, game dump, or git submodules. It generates
`dist/installer/God-of-War-III-Setup.exe` and its `.sha256` file. Tests also
generate UI previews in Portuguese and English in that folder.

The local tests exercise release selection, URL/digest validation, destination
protection, archive traversal, duplicate entries, links, cancellation, committing
an installation, concurrent destination changes, settings backup, preserving
existing shortcuts, cleanup failures and UI rendering. Test shortcuts,
configurations and logs are written only below a unique temporary directory.
Offline tests do not download or execute the port or change user settings.

For a real network smoke test:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File packaging/windows/installer/build.ps1 -Integration
```

This additionally exercises the real setup UI with the official download,
configuration and shortcuts in isolated temporary directories, then cancels
another real download. It starts the downloaded launcher's `--script` role to
check bundled Python, Tk, Pillow and iced-x86, and runs the packaged Vulkan GPU
probe. It **does not boot or play the game**. Both test installations are removed
and the tested release/digest are reported. It requires internet access, a
compatible Windows CPU and a Vulkan GPU/driver. No real game dump is needed;
the configuration test uses dummy files that are never passed to the game.

Local validation on Windows 11 with release `windows-v2.3.0` passed the setup
tests and the runtime/GPU smoke tests (NVIDIA RTX 4060 and AMD integrated GPU).
SHA-256: `41eabb665373029cb188e95bb59dd9c038d02ab011de4cfcfef0056784019da3`.
Gameplay, other hardware, Windows 10 and a clean Windows installation have not
been validated by these tests.

The `Windows installer` GitHub Actions workflow builds and runs the offline
tests, then uploads the EXE, checksum and UI previews as an artifact. It does
not publish a release or change the existing Windows ZIP packaging workflow.
Attach the EXE and checksum to a release after review; distribute matching source
under this repository's GPL license.
