# God of War III Remastered for PC 

<div align="center">
  <a href="https://ko-fi.com/S7O428PBCC" target="_blank"><img src="https://ko-fi.com/img/githubbutton_sm.svg" alt="ko-fi"></a>
</div>

**English** | [Português (Brasil)](README.pt-BR.md)

A PC port of **God of War III Remastered** (PlayStation 4) for Windows 10 and 11.

The CPU side is native: the game's original executable runs directly on the PC's processor (the
PS4 uses the same x86-64 architecture), and a runtime written for this game replaces the PS4
system libraries, so there is no emulator. The graphics are translated: they run on the
[shadPS4](https://github.com/shadps4-emu/shadPS4) video core, which converts the PS4 GPU commands
and shaders to Vulkan, with the God of War III rendering fixes from
[cuesta4's `eltutz` branch](https://github.com/cuesta4/shadPS4/tree/eltutz). Running the game code
directly on the CPU is also how shadPS4 works; what this project changes is the system layer and
the game-specific work (patches, background shader cache, launcher and in-game menu).
It is a fork of [Supermedo's Windows port](https://github.com/Supermedo/bloodborne_pc) of
[deadinside28's bloodborne_pc](https://github.com/deadinside28/bloodborne_pc) (see
[Credits](#credits)).

> **No game files are included.** You need a dump from your own PS4 (tested: CUSA01623 and
> CUSA01715, versions 01.00 and 01.02).
> This project is not affiliated with Sony Interactive Entertainment or Santa Monica Studio.
> God of War is a trademark of Sony Interactive Entertainment.

On the test machine (AMD Radeon RX 6700 XT, Ryzen 5 5600X, 32 GB) the game runs at 144 FPS on
average (55 to 240) at 1080p with the 240 FPS engine frame rate, and at 108 FPS on average at 4K
with FSR 1.

**Tested and playable from start to finish** at native 1080p with the 240 FPS engine frame
rate. At other render resolutions some textures look broken; a fix is in progress.

Progress is tracked (in Portuguese) in [docs/gow3/ROADMAP.md](docs/gow3/ROADMAP.md).

## What works

- Opening, menus, video cutscenes (H.264) and gameplay.
- Audio: music, voices and effects (ATRAC9 and MP3).
- Controller (DualSense, Xbox and others, through SDL3) and keyboard at the same time, both
  remappable.
- Saving and loading: the save list is shown on screen, as on the PS4. Autosave works too. The
  launcher backs the saves up before every start.
- Patches: render resolution of 480p, 720p, 1440p, 1800p or 4K, corrupted texture fix and skip
  videos with the X button (community); engine frame rate of 120 FPS (kvicken) or 240 FPS
  (adapted by Felipe Prado).
- **Instant start:** the saved shader cache loads in the background while you play, and new
  shaders compile in the background too, so the game does not stop to build them.
- **Launcher** in the game's colors, in English with partial translations: quick settings on the home page, display,
  performance, game folder and save backups, patches, mods (order, presets and conflicts),
  controls with PlayStation or Xbox button icons, updates and diagnostics.
- **In-game menu** (Insert on the keyboard, R3+L2 on the controller), with four tabs:
  - **Game:** cheats (infinite health, magic, item meter, Rage of Sparta, max red orbs);
    multipliers from 0.1x to 100x for red, green (health), blue (magic) and gold (Rage of
    Sparta) orbs and for damage dealt and taken; an enemy health bar.
  - **Image:** display mode (windowed, borderless, fullscreen), VSync, FSR 1 upscaling and
    RCAS sharpening.
  - **Performance:** frame rate limit (30 to unlimited), frames queued, background shader
    compiling; render resolution, engine frame rate and GPU readback options apply with one
    "Apply and restart" button.
  - **Overlay:** a performance overlay with FPS, frametime, upscaler, GPU (name and use), CPU,
    RAM and VRAM, each optional, with font size, corner, background opacity and layout. The
    numbers are the game's own use, not the whole system's.

## Download and play

**You need**
- Windows 10 (1903 or newer) or Windows 11, 64-bit.
- A graphics card with Vulkan 1.3 and an up-to-date driver.
- Your own copy of God of War III Remastered, as a folder that contains `eboot.bin`,
  `sce_module` and `sce_sys`. Tested: **CUSA01623 (US) and CUSA01715 (EU), versions 01.00 and
  01.02** (full update). Other regions and versions may work but are untested: if you try one,
  please open an issue with your `user/last_run.log`. No game files are included in this download.

**Steps**
1. Download `gow3-windows.zip` from the
   [Releases page](https://github.com/FelipePrado0/gow3_pc/releases/latest).
2. Extract it to a folder of your choice, for example `C:\Games\gow3-windows`. Do not run it from
   inside the zip.
3. Open **`God of War III.exe`**. If Windows shows "Windows protected your PC", click
   **More info**, then **Run anyway**: the program is not digitally signed.
4. Click **Choose the game folder**. It takes you to the **Game** page. Next to **Game folder**,
   click **Browse…** and select the folder with `eboot.bin`.
5. Back on **Home**, the cover art and "CUSA01623 · version 01.02" appear. Adjust **Quick
   settings** if you like: display mode, frame rate limit, render resolution and engine frame
   rate.
6. Click **PLAY**. The first start takes a little longer while the game image is prepared. The
   next starts are quick.

Patches and fixes are included and applied automatically: texture fix, skip videos with X and
120 FPS are on by default. Change the resolution and frame rate on **Home** or **Performance**.

**While playing**
- **Insert** on the keyboard, or **R3 + L2** on the controller, opens the port's menu: cheats,
  multipliers, image, performance and the performance overlay.
- Saved shaders load in the background. A small "Loading shader cache" note in the corner shows
  the progress.

**Next time**
- **`Play God of War III.exe`** starts the game straight away with your saved settings. You can
  make a desktop shortcut to it, or add it to Steam with **Add a Non-Steam Game**.
- Your saves, shader cache and log are in the `user` folder next to `God of War III.exe`. Keep
  that folder when you update.
- **Advanced → Check for updates** downloads and installs a new version. Your saves and settings
  are kept.

**If something goes wrong**
- Black screen at start: **Advanced → Clear shader cache**, then start again.
- The game does not open: check the **Log** page, or send `user\last_run.log` with your report.

## Linux (AppImage, experimental)

The Linux build runs the same native runtime and the same launcher. It was tested on Arch Linux
with a Radeon RX 7800 XT (RADV) in a Wayland session.

**You need:** a Vulkan 1.3 driver and your own dump of the game (CUSA01623, version 01.02).

**Build and run**
```
bash build.sh
bash packaging/linux/build.sh   # needs PyInstaller, Tk, Pillow and iced-x86 for Python, plus patchelf and curl
./dist/gow3-x86_64.AppImage
```
Choose the game folder in the launcher. `--play` starts the game with the saved options and no
window. From a source tree: `GOW3_GAME_DIR=/path/to/CUSA01623 python3 run.py` after `bash build.sh`.

- **Files:** saves, settings, shader cache and the log are in `~/.local/share/gow3`; the launcher's
  own settings in `~/.config/gow3-launcher`.
- **Update folder:** if the update was dumped apart from the game, choose it under **Game → Update
  folder**. It is laid over the game folder without changing either.
- **Advanced → Linux:** window system (automatic, Wayland or X11), GameMode and MangoHud. Only the
  tools installed on the system can be turned on.
- **Steam:** **Advanced → Add to Steam** adds "God of War III Remastered" as a non-Steam game, with
  the icon, the vertical and horizontal capsules, the library background and the logo made from the
  dump's pictures. Close Steam first (it rewrites its shortcuts when it exits). The button is also in
  the Windows launcher (not tested there yet).
- **Menu entry:** the AppImage writes `~/.local/share/applications/gow3.desktop` when it starts, so
  the launcher and the game window show their icon in the dock; **Advanced → Linux** turns it off and
  removes the entry.
- **Icon:** the AppImage, the launcher and the game window use `packaging/linux/gow3.png` when that
  file exists at build time, and `launcher/gow3.png` otherwise.
- **Patches:** the same patch file serves the full 01.02 update (`eboot.bin` sha256 `6e72330e…`) and
  the European title (CUSA01715): the patches and cheats find their sites by signature.
- **Graphics (experimental):** the temporal upscalers (FSR 3.1, FSR 4, TAA, DLSS) are off by default;
  the switch is at the top of that page.

## Requirements

- Windows 10 (1903 or newer) or Windows 11, 64-bit.
- A 64-bit processor with AVX: Intel Sandy Bridge (2011) or newer, AMD FX or newer. On CPUs without
  MOVBE, BMI1 or LZCNT (Sandy/Ivy Bridge, AMD FX) the loader rewrites the game's instructions that
  use them to emulated stubs and logs an `ISA:` line; performance on those CPUs has not been
  measured yet. Older CPUs stop at start with a `STOP: this CPU lacks` line in the log.
- A graphics card with Vulkan 1.3 and a current driver.
- Your copy of the game (the folder with `eboot.bin`, `sce_module` and `sce_sys`). Tested:
  CUSA01623 and CUSA01715, versions 01.00 and 01.02.
- The patches and the in-game hooks (cheats, multipliers, health bar) find their sites by
  signature: the original bytes around each one, wherever a release placed that code
  (`tools/make_signatures.py`). A site that is not found leaves only that patch or hook off; the
  game starts without it, and the log and the menu say why.
- To build from source: [MSYS2](https://www.msys2.org) and [Python 3](https://www.python.org) with Pillow
  (`pip install pillow`, used for the game's icon and cover art); on a CPU without MOVBE, BMI1 or LZCNT
  also iced-x86 (`pip install iced-x86`, used for the instruction rewrite).

## Building from source

Development happens on the `gow3` branch; `main` receives tested merges.

1. Install MSYS2 (`winget install MSYS2.MSYS2`) and, in the **MSYS2 CLANG64** shell, the
   packages listed in [packaging/windows/README.md](packaging/windows/README.md).
2. Clone the repository with its submodules and build:

   ```bash
   git clone --recursive https://github.com/FelipePrado0/gow3_pc
   cd gow3_pc
   git checkout gow3   # the newest work; skip it to build main
   bash build.sh
   ```

3. Start the launcher (in PowerShell, in the project folder):

   ```powershell
   python launcher/gow3_launcher_win.py
   ```

4. On **Game**, choose the game folder. Adjust **Display**, **Performance** and **Patches** if
   you like, then press **PLAY**.

To play with the saved settings without opening the launcher's window (for a shortcut or Steam):

```powershell
python launcher/gow3_launcher_win.py --play
```

Or without the launcher at all:

```powershell
$env:GOW3_GAME_DIR = 'D:\path\CUSA01623'
python run.py
```

Settings are kept in `gow3.ini` (the game and the in-game menu) and in
`%APPDATA%\gow3-launcher\settings.json` (the launcher). Saves, the shader cache and
`last_run.log` go to the project's `user` folder.

On **Performance → GPU data → Stream buffer memory**, choose **System RAM (cached, default)**,
**System RAM (uncached)** or **GPU memory (previous behavior)**. System RAM can improve frame
rates on NVIDIA laptops. The choice is saved and applies on the next game launch, including
when using `Play God of War III.exe` or `--play`. Choose GPU memory to restore the previous
allocation behavior. For direct launches without the launcher, `GOW3_STREAM_MEMORY` accepts
`cached`, `host` or `device`; the launcher's saved choice takes precedence when using it.

### Importing shadPS4 saves

Only **shadPS4 saves for God of War III Remastered** are compatible. RPCS3 saves are
from the PS3 game and will not work. Use the same title ID (this example uses `CUSA01623`).

1. Close the game and shadPS4. Back up both save folders.
2. Find the source in shadPS4's data folder: `user\savedata\1\CUSA01623\` in older
   versions, or `home\<user-id>\savedata\CUSA01623\` in newer versions.
3. The destination is **`user\savedata\1\CUSA01623\` next to `God of War III.exe`**.
   Example: `C:\Games\gow3-windows\user\savedata\1\CUSA01623\`. With a custom
   **Saves folder** in the launcher, use `<custom folder>\savedata\1\CUSA01623\`.
4. Copy each slot folder there, keeping its name and game files. Do not nest another
   `CUSA01623` folder. Exclude the slot's internal `sce_sys` folder from the game data.
5. Beside each slot, create `<slot name>.sce_sys` if it does not exist. You can copy
   shadPS4's `sce_sys\icon0.png` there. **Do not rename `param.sfo` to `param.bin`**:
   the formats differ. Preserve any existing port `param.bin`. Without one, the slot
   appears by folder name; saving again in the port writes its metadata and refreshes
   the displayed information.
6. Start the port and select **Load Game**. Completed saves can be imported too.

The port's save dialog displays at most **32 entries**. If the imported slot is missing
from a long list, test it with a separate **Saves folder** containing just that slot
under `savedata\1\CUSA01623\`, with its sibling metadata folder. Keep the original saves
and restore the previous Saves folder setting after testing.

### Third-party overlays (Windows)

The game process is **`bin\gow3-probe.exe`**, not `God of War III.exe` (source builds
use `out\gow3-probe.exe`). Add that executable to RTSS and enable Afterburner's on-screen
display. In the ReShade installer, select that same executable and the **Vulkan** API.

If the overlay still does not appear, report the port and overlay versions, the last
working port version, and attach `user\last_run.log`. These checks do not confirm a fix
for every overlay configuration.

### Keyboard controls

Defaults; change them on the launcher's **Controls** page.

| Key | Button |
|---|---|
| WASD | left stick |
| Arrows | right stick (camera) |
| Space | Cross |
| Left Shift | Circle |
| E | Square |
| Q | Triangle |
| 1 and 3 | L1 and R1 |
| R and F | L2 and R2 |
| Z and C | L3 and R3 |
| I, K, J, L | d-pad |
| Enter | Options |
| Tab | left touchpad click (photo mode) |
| Backspace | right touchpad click (pause menu) |
| Insert | the port's menu (R3+L2 on the controller) |

On a controller without a touchpad (Xbox and others), View opens the pause menu (the right
touchpad click) and L1 + View opens photo mode (the left click). On a DualSense the touchpad
works as on the PS4, and Share/Create works like View.

In the menu: L1/R1 or Q/E switch tabs. In the save list: arrows or the d-pad choose, Enter or
Cross confirm, Esc or Circle cancel.

## Tests

```bash
python -m unittest discover -s tests
bash build.sh --test
ninja -C out/gpu stat-multipliers-test perf-overlay-test warmup-inbox-test graphics-settings-test
out/gpu/stat-multipliers-test.exe out/eboot.elf
out/gpu/perf-overlay-test.exe
out/gpu/warmup-inbox-test.exe
out/gpu/graphics-settings-test.exe
```

The C++ tests are targets in [gpu/CMakeLists.txt](gpu/CMakeLists.txt); the ones that check the
game's code take the prepared executable (`out/eboot.elf`) as their argument.

To check that every system function the game uses is implemented, without starting the game:

```bash
out/gow3-probe.exe out/boot-linked.bin --app0 <game folder> --check-imports
```

## How it works

| Part | Where | What it does |
|---|---|---|
| Preparation | `scripts/` | Reads `eboot.bin`, applies its relocations and links the game's own `libc` and `libSceFios2` into one image |
| Loader | `src/probe.c` | Loads the image, installs the validated hooks (cheats, multipliers, health bar) and starts the game's original code |
| Runtime | `src/runtime_*.c` | Reimplements the PS4 system functions: memory, threads, files, audio, video, controller, saves and services |
| GPU | `gpu/shadps4/` | The shadPS4 video core, translating the PS4 GPU commands and shaders to Vulkan, with cuesta4's God of War III fixes (`eltutz` branch) and this port's speed work (background shader cache, deferred readbacks) |
| In-game menu | `gpu/shim/gow3_overlay.cpp` | The Dear ImGui menu and performance overlay; settings in `gpu/shim/gow3_settings.cpp` |
| Patches | `patches/God_of_War_III_Remastered.xml`, `scripts/patches.py` | Applied in memory at start, without changing the game files |
| Launcher | `launcher/gow3_launcher_win.py` | Settings, patches, mods, controls and the play button |

More documentation: [mods](docs/MODS.md), [frame map and upscaler](docs/upscaler.md),
[parallel GPU work](docs/parallel_gpu.md) and the [roadmap](docs/gow3/ROADMAP.md).

## Credits

This project is a fork of Supermedo's Windows port, which builds on deadinside28's original:

- **deadinside28**: [bloodborne_pc](https://github.com/deadinside28/bloodborne_pc), the original
  native port.
- **Supermedo** (Mohammed Albarghouthi): [bloodborne_pc for Windows](https://github.com/Supermedo/bloodborne_pc),
  the fork this project comes from.
- **cuesta4**: [shadPS4 fork focused on God of War III Remastered](https://github.com/cuesta4/shadPS4/tree/eltutz)
  (branch `eltutz`). The game's rendering in this port depends on its fixes: the GNM blend
  rewrite, depth/stencil state and image aliasing. cuesta4 is also a co-author of the
  resolution, texture fix, 120 FPS and skip videos patches (below). If you play on shadPS4,
  this is the build to use for God of War III.
- **shadPS4 team**: the [shadPS4](https://github.com/shadps4-emu/shadPS4) video core, which does
  all the GPU and shader translation to Vulkan in this port.

Patches: kvicken and cuesta4 (resolution, texture fix, 120 FPS and skip videos, building on the
PS3 patches by illusion0001), published for shadPS4 in
[shadps4-emu/ps4_cheats#145](https://github.com/shadps4-emu/ps4_cheats/pull/145) and adapted to
this executable. The 240 FPS patch was adapted by Felipe Prado from the 120 FPS patch.
Cheats: adapted from Celogamez's [GoldHEN cheats](https://github.com/GoldHEN/GoldHEN_Cheat_Repository).

Also used: [LibAtrac9](https://github.com/Thealexbarney/LibAtrac9), [FFmpeg](https://ffmpeg.org),
[SDL3](https://github.com/libsdl-org/SDL), [Dear ImGui](https://github.com/ocornut/imgui),
[sirit](https://github.com/shadps4-emu/sirit), [magic_enum](https://github.com/Neargye/magic_enum),
[miniz](https://github.com/richgel999/miniz), [xbyak](https://github.com/herumi/xbyak),
[Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator),
[FSR-Vulkan](https://github.com/FireBurn/FSR-Vulkan) and the AMD FidelityFX SDK,
[MSYS2](https://www.msys2.org), [LLVM](https://llvm.org) and [Pillow](https://python-pillow.org).

## License

GNU GPL v2 or later ([LICENSE](LICENSE)). Third-party components keep their own licenses.
