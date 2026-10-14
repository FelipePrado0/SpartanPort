<div align="center">

# God of War III Remastered for PC

A native Windows port of the PlayStation 4 version. No emulator: the game's own executable runs on your CPU.

[![Latest release](https://img.shields.io/github/v/release/FelipePrado0/gow3_pc?style=for-the-badge&color=b3202a&label=release)](https://github.com/FelipePrado0/gow3_pc/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/FelipePrado0/gow3_pc/total?style=for-the-badge&color=444&label=downloads)](https://github.com/FelipePrado0/gow3_pc/releases)
[![License](https://img.shields.io/badge/license-GPL--2.0--or--later-blue?style=for-the-badge)](LICENSE)

![Windows 10/11](https://img.shields.io/badge/Windows-10%20%7C%2011-0078D6?logo=windows&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-AppImage%20(experimental)-FCC624?logo=linux&logoColor=black)
![Vulkan 1.3](https://img.shields.io/badge/Vulkan-1.3-AC162C?logo=vulkan&logoColor=white)
![Status](https://img.shields.io/badge/status-playable%20start%20to%20finish-2ea44f)
![Stars](https://img.shields.io/github/stars/FelipePrado0/gow3_pc?style=flat&logo=github)
![Last commit](https://img.shields.io/github/last-commit/FelipePrado0/gow3_pc/gow3)

**English** | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md)

<a href="https://ko-fi.com/S7O428PBCC" target="_blank"><img src="https://ko-fi.com/img/githubbutton_sm.svg" alt="Support on Ko-fi"></a>

</div>

---

> [!IMPORTANT]
> **No game files are included.** You need a dump from your own PS4 (tested: CUSA01623 and CUSA01715, versions 01.00 and 01.02).
> This project is not affiliated with Sony Interactive Entertainment or Santa Monica Studio. God of War is a trademark of Sony Interactive Entertainment.

## Overview

The CPU side is native. The PS4 uses the same x86-64 architecture as a PC, so the game's original executable runs directly on the processor, with a runtime written for this game standing in for the PS4 system libraries. The GPU side is translated: the [shadPS4](https://github.com/shadps4-emu/shadPS4) video core converts PS4 GPU commands and shaders to Vulkan, with the God of War III rendering fixes from [cuesta4's `eltutz` branch](https://github.com/cuesta4/shadPS4/tree/eltutz).

What this project adds on top is the system layer and the game-specific work: patches, a background shader cache, a launcher and an in-game menu.

It is a fork of [Supermedo's Windows port](https://github.com/Supermedo/bloodborne_pc) of [deadinside28's bloodborne_pc](https://github.com/deadinside28/bloodborne_pc). See [Credits](#credits).

| | |
|---|---|
| **Tested hardware** | Radeon RX 6700 XT, Ryzen 5 5600X, 32 GB RAM |
| **Performance** | 144 FPS average (55 to 240) at 1080p, 240 FPS engine frame rate |
| **Status** | Playable from start to finish at native 1080p |
| **Known issue** | At other render resolutions some textures look broken; a fix is in progress |

Progress is tracked (in Portuguese) in the [roadmap](docs/gow3/ROADMAP.md).

## Features

- **Complete game:** opening, menus, H.264 cutscenes and gameplay, with music, voices and effects (ATRAC9 and MP3).
- **Controls:** DualSense, Xbox and other controllers through SDL3, plus keyboard at the same time. Both are remappable.
- **Button icons (opt-in):** swap the PlayStation icons for your controller's, Xbox or Nintendo ones.
- **Saves:** the save list is shown on screen as on PS4, autosave works, and the launcher backs saves up before every start.
- **Patches:** render resolution (480p, 720p, 1440p, 1800p, 4K), corrupted texture fix, skip videos with X, and engine frame rate of 120 FPS (kvicken) or 240 FPS (adapted by Felipe Prado).
- **Instant start:** saved shaders load in the background while you play, and new shaders compile in the background, so the game doesn't stop to build them.
- **Launcher:** quick settings, display, performance, game folder, save backups, patches, mods (order, presets, conflicts), controls, updates and diagnostics.

### In-game menu

Open it with **Insert** on the keyboard or **R3 + L2** on the controller.

| Tab | Contents |
|---|---|
| **Game** | Cheats (infinite health, magic, item meter, Rage of Sparta, max red orbs). Multipliers from 0.1x to 100x for red, green, blue and gold orbs and for damage dealt and taken. Enemy health bar. |
| **Image** | Windowed, borderless or fullscreen; VSync; RCAS sharpening. |
| **Performance** | Frame rate limit (30 to unlimited), frames queued, background shader compiling. Render resolution, engine frame rate and GPU readback apply with one "Apply and restart" button. |
| **Overlay** | FPS, frametime, upscaler, GPU (name and use), CPU, RAM and VRAM, each optional, with font size, corner, background opacity and layout. Numbers are the game's own use, not the whole system's. |

## Quick start (Windows)

**You need**

- Windows 10 (1903 or newer) or Windows 11, 64-bit
- A graphics card with Vulkan 1.3 and an up-to-date driver
- Your own copy of the game as a folder containing `eboot.bin`, `sce_module` and `sce_sys`. Tested: **CUSA01623 (US)** and **CUSA01715 (EU)**, versions 01.00 and 01.02 (full update). Other regions and versions may work but are untested. If you try one, please open an issue with your `user/last_run.log`.

**Steps**

1. Download `gow3-windows.zip` from the [Releases page](https://github.com/FelipePrado0/gow3_pc/releases/latest).
2. Extract it somewhere permanent, for example `C:\Games\gow3-windows`. Don't run it from inside the zip.
3. Open **`God of War III.exe`**. If Windows shows "Windows protected your PC", click **More info**, then **Run anyway**. The program is not digitally signed.
4. Click **Choose the game folder**, then next to **Game folder** click **Browse…** and select the folder with `eboot.bin`.
5. Back on **Home**, the cover art and a line like "CUSA01623 · version 01.02" appear. Adjust **Quick settings** if you like.
6. Click **PLAY**. The first start takes a little longer while the game image is prepared. Later starts are quick.

The texture fix, skip videos with X and 120 FPS are on by default. Change resolution and frame rate on **Home** or **Performance**.

**Afterwards**

- `Play God of War III.exe` starts the game straight away with your saved settings. Make a desktop shortcut, or add it to Steam with **Add a Non-Steam Game**.
- Saves, shader cache and log live in the `user` folder next to `God of War III.exe`. Keep it when you update.
- **Advanced → Check for updates** downloads and installs new versions. Saves and settings are kept.

> [!TIP]
> **Black screen at start?** Go to **Advanced → Clear shader cache** and start again.
> **Game won't open?** Check **Advanced → Log**, or attach `user\last_run.log` to your report.

## Requirements

- Windows 10 (1903 or newer) or Windows 11, 64-bit.
- A 64-bit CPU with AVX: Intel Sandy Bridge (2011) or newer, AMD FX or newer. On CPUs without MOVBE, BMI1 or LZCNT (Sandy/Ivy Bridge, AMD FX), the loader rewrites the affected instructions to emulated stubs and logs an `ISA:` line. Performance there hasn't been measured. Older CPUs stop at start with a `STOP: this CPU lacks` line in the log.
- A graphics card with Vulkan 1.3 and a current driver.
- Your copy of the game (see above).

The patches and in-game hooks (cheats, multipliers, health bar) find their sites by signature, matching the original bytes around each one wherever a release placed that code (`tools/make_signatures.py`). If a site isn't found, only that patch or hook is left off; the game still starts, and the log and menu say why.

## Linux (AppImage, experimental)

Same native runtime, same launcher. Tested on Arch Linux with a Radeon RX 7800 XT (RADV) in a Wayland session.

**You need:** a Vulkan 1.3 driver and your own dump of the game (CUSA01623, version 01.02).

```bash
bash build.sh
bash packaging/linux/build.sh   # needs PyInstaller, Tk, Pillow, iced-x86 (Python), plus patchelf and curl
./dist/gow3-x86_64.AppImage
```

Choose the game folder in the launcher. `--play` starts the game with saved options and no window. From a source tree: `GOW3_GAME_DIR=/path/to/CUSA01623 python3 run.py` after `bash build.sh`.

<details>
<summary><b>Linux details</b></summary>

- **Files:** saves, settings, shader cache and log are in `~/.local/share/gow3`; launcher settings in `~/.config/gow3-launcher`.
- **Update folder:** if the update was dumped apart from the game, choose it under **Game → Update folder**. It is laid over the game folder without changing either.
- **Advanced → Linux:** window system (automatic, Wayland or X11), GameMode and MangoHud. Only tools installed on the system can be turned on.
- **Steam:** **Advanced → Add to Steam** adds the game as a non-Steam game, with icon, vertical and horizontal capsules, library background and logo made from the dump's pictures. Close Steam first, since it rewrites its shortcuts on exit. The button also exists in the Windows launcher but is untested there.
- **Menu entry:** the AppImage writes `~/.local/share/applications/gow3.desktop` on start so the launcher and game window show their icon in the dock. **Advanced → Linux** turns this off and removes the entry.
- **Icon:** uses `packaging/linux/gow3.png` if present at build time, otherwise `launcher/gow3.png`.
- **Patches:** the same patch file serves the full 01.02 update (`eboot.bin` sha256 `6e72330e…`) and the European title (CUSA01715).
- **Graphics:** the temporal upscalers (FSR 3.1, FSR 4, TAA, DLSS) are temporarily removed while a better implementation is worked on.

</details>

## Controls

Defaults. Change them in the launcher under **Game → Controls**.

| Key | Button | | Key | Button |
|---|---|---|---|---|
| W A S D | Left stick | | Q | Triangle |
| Arrow keys | Right stick (camera) | | 1 / 3 | L1 / R1 |
| Space | Cross | | R / F | L2 / R2 |
| Left Shift | Circle | | Z / C | L3 / R3 |
| E | Square | | I K J L | D-pad |
| Enter | Options | | Insert | Port menu (R3 + L2 on controller) |
| Tab | Left touchpad click (photo mode) | | Backspace | Right touchpad click (pause menu) |

On a controller without a touchpad (Xbox and others), **View** opens the pause menu and **L1 + View** opens photo mode. On a DualSense the touchpad works as on PS4, and Share/Create works like View.

In menus: L1/R1 or Q/E switch tabs. In the save list: arrows or d-pad choose, Enter or Cross confirm, Esc or Circle cancel.

<details>
<summary><b>In-game button icons</b></summary>

Off by default. Pick *Match my controller*, *Xbox* or *Nintendo* in the launcher under **Game → Controls → In-game button icons**. They replace the PlayStation icons in tutorial, move-list and quick-time prompts; the pause-menu controller drawing stays a DualShock 4. The launcher writes a patched copy of two game files into the port's data folder and never touches your game folder. A new controller is picked up after a restart.

Without the launcher, set `GOW3_GLYPH_STYLE` to `auto`, `xbox` or `nintendo` before starting `run.py`.

</details>

## Advanced settings

Settings are kept in `gow3.ini` (game and in-game menu) and `%APPDATA%\gow3-launcher\settings.json` (launcher). Saves, shader cache and `last_run.log` go to the `user` folder.

<details>
<summary><b>Stream buffer memory</b> (Performance → GPU data)</summary>

Choose **System RAM (cached, default)**, **System RAM (uncached)** or **GPU memory (previous behavior)**. System RAM can improve frame rates on NVIDIA laptops. The choice is saved and applies on the next launch, including through `Play God of War III.exe` or `--play`. Pick GPU memory to restore the previous allocation behavior.

For direct launches without the launcher, `GOW3_STREAM_MEMORY` accepts `cached`, `host` or `device`; the launcher's saved choice takes precedence when it is used.

</details>

<details>
<summary><b>Game multisampling</b> (Performance → GPU data)</summary>

**Automatic** lets the game decide by graphics card. On AMD and Intel Windows drivers it creates the game's multisampled images with a single sample, because reading the 2-sample depth buffer as a texture produced black lines in the lighting (Radeon RX 6600 family, Arc). It keeps them multisampled elsewhere (NVIDIA). The label names the card and the decision, e.g. "Automatic (Intel(R) Arc(TM) A750 Graphics: off)", and follows the **Graphics card** choice. The rule covers every GPU on those two drivers, integrated ones included. The log line `GPU: guest multisampling off` or `on` shows what was applied.

Choose **Off** if the lines appear on another card, or **On** to keep multisampling anyway. Off and On set `GOW3_NO_MSAA=1` or `0`; Automatic passes an inherited `GOW3_NO_MSAA` through. Applies on the next launch.

</details>

<details>
<summary><b>Importing shadPS4 saves</b></summary>

Only **shadPS4 saves for God of War III Remastered** are compatible. RPCS3 saves are from the PS3 game and won't work. Use the same title ID (this example uses `CUSA01623`).

1. Close the game and shadPS4. Back up both save folders.
2. Find the source in shadPS4's data folder: `user\savedata\1\CUSA01623\` in older versions, or `home\<user-id>\savedata\CUSA01623\` in newer ones.
3. The destination is `user\savedata\1\CUSA01623\` next to `God of War III.exe`, for example `C:\Games\gow3-windows\user\savedata\1\CUSA01623\`. With a custom **Saves folder** in the launcher, use `<custom folder>\savedata\1\CUSA01623\`.
4. Copy each slot folder there, keeping its name and game files. Don't nest another `CUSA01623` folder. Exclude the slot's internal `sce_sys` folder from the game data.
5. Beside each slot, create `<slot name>.sce_sys` if it doesn't exist. You can copy shadPS4's `sce_sys\icon0.png` there. **Do not rename `param.sfo` to `param.bin`**: the formats differ. Keep any existing port `param.bin`. Without one, the slot appears by folder name; saving again in the port writes its metadata.
6. Start the port and select **Load Game**. Completed saves can be imported too.

The save dialog shows at most **32 entries**. If an imported slot is missing from a long list, test it with a separate **Saves folder** containing just that slot under `savedata\1\CUSA01623\` and its sibling metadata folder. Keep the originals and restore the previous Saves folder setting afterwards.

</details>

<details>
<summary><b>Third-party overlays (RTSS, Afterburner, ReShade)</b></summary>

The game process is **`bin\gow3-probe.exe`**, not `God of War III.exe` (source builds use `out\gow3-probe.exe`). Add that executable to RTSS and enable Afterburner's on-screen display. In the ReShade installer, select the same executable and the **Vulkan** API.

If the overlay still doesn't appear, report the port and overlay versions, the last working port version, and attach `user\last_run.log`. These checks don't confirm a fix for every configuration.

</details>

## Building from source

Development happens on the `gow3` branch; `main` receives tested merges.

**Extra requirements:** [MSYS2](https://www.msys2.org) and [Python 3](https://www.python.org) with Pillow (`pip install pillow`, for the icon and cover art). On a CPU without MOVBE, BMI1 or LZCNT, also `pip install iced-x86` for the instruction rewrite.

1. Install MSYS2 (`winget install MSYS2.MSYS2`) and, in the **MSYS2 CLANG64** shell, the packages listed in [packaging/windows/README.md](packaging/windows/README.md).
2. Clone with submodules and build:

   ```bash
   git clone --recursive https://github.com/FelipePrado0/gow3_pc
   cd gow3_pc
   git checkout gow3   # newest work; skip to build main
   bash build.sh
   ```

3. Start the launcher from PowerShell in the project folder:

   ```powershell
   python launcher/gow3_launcher_win.py
   ```

4. On **Game**, choose the game folder, adjust **Display**, **Performance** and **Patches**, then press **PLAY**.

To play with saved settings without the launcher window (shortcut or Steam):

```powershell
python launcher/gow3_launcher_win.py --play
```

Or skip the launcher entirely:

```powershell
$env:GOW3_GAME_DIR = 'D:\path\CUSA01623'
python run.py
```

### Tests

```bash
python -m unittest discover -s tests
bash build.sh --test
ninja -C out/gpu stat-multipliers-test perf-overlay-test warmup-inbox-test graphics-settings-test
out/gpu/stat-multipliers-test.exe out/eboot.elf
out/gpu/perf-overlay-test.exe
out/gpu/warmup-inbox-test.exe
out/gpu/graphics-settings-test.exe
```

The C++ tests are targets in [gpu/CMakeLists.txt](gpu/CMakeLists.txt). Those that check the game's code take the prepared executable (`out/eboot.elf`) as an argument.

To verify that every system function the game uses is implemented, without starting the game:

```bash
out/gow3-probe.exe out/boot-linked.bin --app0 <game folder> --check-imports
```

## How it works

| Part | Where | What it does |
|---|---|---|
| Preparation | `scripts/` | Reads `eboot.bin`, applies its relocations, links the game's own `libc` and `libSceFios2` into one image |
| Loader | `src/probe.c` | Loads the image, installs the validated hooks (cheats, multipliers, health bar), starts the game's original code |
| Runtime | `src/runtime_*.c` | Reimplements PS4 system functions: memory, threads, files, audio, video, controller, saves, services |
| GPU | `gpu/shadps4/` | The shadPS4 video core, translating PS4 GPU commands and shaders to Vulkan, with cuesta4's fixes and this port's speed work (background shader cache, deferred readbacks) |
| In-game menu | `gpu/shim/gow3_overlay.cpp` | Dear ImGui menu and performance overlay; settings in `gpu/shim/gow3_settings.cpp` |
| Patches | `patches/God_of_War_III_Remastered.xml`, `scripts/patches.py` | Applied in memory at start, game files are never modified |
| Launcher | `launcher/gow3_launcher_win.py` | Settings, patches, mods, controls and the play button |

Further reading: [mods](docs/MODS.md), [frame map and upscaler](docs/upscaler.md), [parallel GPU work](docs/parallel_gpu.md), [roadmap](docs/gow3/ROADMAP.md).

## Credits

- **deadinside28**: [bloodborne_pc](https://github.com/deadinside28/bloodborne_pc), the original native port.
- **Supermedo** (Mohammed Albarghouthi): [bloodborne_pc for Windows](https://github.com/Supermedo/bloodborne_pc), the fork this project comes from.
- **cuesta4**: [shadPS4 fork for God of War III Remastered](https://github.com/cuesta4/shadPS4/tree/eltutz) (`eltutz` branch). This port's rendering depends on its fixes: the GNM blend rewrite, depth/stencil state and image aliasing. cuesta4 also co-authored the resolution, texture fix, 120 FPS and skip videos patches. If you play on shadPS4, this is the build to use for God of War III.
- **shadPS4 team**: the [shadPS4](https://github.com/shadps4-emu/shadPS4) video core, which handles all GPU and shader translation to Vulkan.

**Patches:** kvicken and cuesta4 (resolution, texture fix, 120 FPS, skip videos), building on illusion0001's PS3 patches, published for shadPS4 in [shadps4-emu/ps4_cheats#145](https://github.com/shadps4-emu/ps4_cheats/pull/145) and adapted to this executable. The 240 FPS patch was adapted by Felipe Prado from the 120 FPS patch.
**Cheats:** adapted from Celogamez's [GoldHEN cheats](https://github.com/GoldHEN/GoldHEN_Cheat_Repository).

**Also used:** [LibAtrac9](https://github.com/Thealexbarney/LibAtrac9), [FFmpeg](https://ffmpeg.org), [SDL3](https://github.com/libsdl-org/SDL), [Dear ImGui](https://github.com/ocornut/imgui), [sirit](https://github.com/shadps4-emu/sirit), [magic_enum](https://github.com/Neargye/magic_enum), [miniz](https://github.com/richgel999/miniz), [xbyak](https://github.com/herumi/xbyak), [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator), [FSR-Vulkan](https://github.com/FireBurn/FSR-Vulkan) with the AMD FidelityFX SDK, [MSYS2](https://www.msys2.org), [LLVM](https://llvm.org) and [Pillow](https://python-pillow.org).

## License

GNU GPL v2 or later, see [LICENSE](LICENSE). Third-party components keep their own licenses.
