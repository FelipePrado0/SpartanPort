# gow3 on macOS (experimental)

The port builds and runs on Macs with Apple Silicon from the same sources as on Linux. The game's
own code is x86-64 and runs inside the loader's process, so the whole process is x86-64 and runs
under Rosetta 2; the graphics go through the same shadPS4 video core to Vulkan, which Mesa's
KosmicKrisp driver implements on Metal.

**Status:** built and tested on an M4 Pro with macOS 27: `bash build.sh --test` (runtime tests and
a GPU start: window, Vulkan device, presenter) and `python3 -m unittest discover -s tests`
(including the loader tests with synthetic x86-64 images). The game itself has not been run on
macOS yet, so expect issues; send `user/last_run.log` with a report.

## Requirements

- A Mac with Apple Silicon (M1 or newer) and **macOS 26 (Tahoe) or newer**: KosmicKrisp needs
  both. Intel Macs are not supported.
- Rosetta 2: `softwareupdate --install-rosetta --agree-to-license`.
- The Xcode command line tools (`xcode-select --install`) and, from Homebrew,
  `brew install cmake ninja pkg-config`.
- Python 3 with Tk and Pillow for the launcher (`brew install python python-tk`, then
  `python3 -m pip install pillow`).
- Your own dump of the game (CUSA01623 or CUSA01715, versions 01.00 or 01.02), a folder with
  `eboot.bin`, `sce_module` and `sce_sys`.

## Build

```bash
git clone --recursive <this repository> && cd gow3_pc
bash build.sh            # first run: builds the x86-64 libraries (about 5 minutes)
bash build.sh --test     # optional: runtime tests and the GPU start test (opens a window briefly)
```

`build.sh` runs `packaging/macos/deps.sh` first. It downloads pinned sources (checked by SHA-256)
and builds, for x86-64, everything the port links into `out/macos-deps`: SDL3, FFmpeg, fmt, Zydis,
xxHash, mimalloc, the Khronos Vulkan loader, and header-only Boost, robin-map, VMA and
Vulkan-Headers. It also builds the host tools glslang and nasm. Homebrew's Intel bottles are no
longer offered on current macOS, so nothing comes from Homebrew except cmake, ninja and
pkg-config. The Vulkan driver, `libvulkan_kosmickrisp.dylib`, is taken from shadPS4's macOS
release (v0.19.0, the same video core); `build.sh` copies it to `out/vulkan/`. A package is built
again when its stamp in `out/macos-deps/stamps` is deleted; `GOW3_MACOS_DEPS` moves the folder.

## Play

```bash
./play-mac.sh                                         # finds the dump and starts the game
./play-mac.sh /path/to/CUSA01623 [/path/to/update]    # that folder (and an update dumped apart)
./play-mac.sh --launcher                              # the launcher: choose the game folder, then PLAY
GOW3_GAME_DIR=/path/to/CUSA01623 python3 run.py       # without the script (after build.sh)
```

`play-mac.sh` builds what changed (quietly once built; `out/play-build.log`), then looks for the
dump, a folder with `eboot.bin`, `sce_module` and `sce_sys`, in `~/Downloads`, `~/Desktop`,
`~/Documents`, `~/Games` and next to the repository, and for an update dumped apart from it next
to the game. It cannot read `.pkg` files and says so when it finds only those.

On a Mac keyboard the port's menu opens with **`** (the key left of 1); Insert and R3+L2 work
too. The first start takes longer: Rosetta translates the game's code and the shaders are compiled.
Saves, `gow3.ini`, the shader cache and `user/last_run.log` are in the repository folder, as on
Linux; the launcher keeps its own settings in `~/.config/gow3-launcher/settings.json`.
**Advanced → Desktop shortcut** puts `God of War III.app` on the desktop, which opens the launcher.

## What is different from Linux

| Linux | macOS |
|---|---|
| `mov rax, fs:[0]` rewritten to `gs:[0]`, GS base set with `arch_prctl` | GS is the thread's pthread TSD array (Rosetta keeps it so) and cannot move. As on Windows, the loader rewrites the displacement: `gs:[8*key]`, the slot of a pthread key whose value is the thread's guest TCB. |
| One memfd for direct and flexible memory | A POSIX shared memory object; the host's own view of it lies above 1 TiB. |
| PS4 user range 64 GiB–1 TiB | On Apple Silicon, [63 GiB, 448 GiB) cannot be mapped (commpage, GPU carveout), so the user range is 448 GiB–1008 GiB, as in shadPS4's macOS build. It and the low range for the image and stacks (48–63 GiB) are reserved first thing in `main`; unmapping returns a range to the reservation. |
| `-no-pie`, glibc's brk heap keeps host objects below 1 TiB | macOS's allocator places heaps above 1 TiB. The loader links mimalloc as the process's malloc zone (`MI_NO_ALIGNED_HINT`): it takes memory with plain `mmap`, which macOS places bottom-up from 4 GiB, so every heap object (thread handles, mutexes, TLS, C++ objects) stays below 1 TiB. |
| The window on a thread of its own, the loader on the main thread | Cocoa windows and their events belong to the main thread: the loader runs on a second thread and the main thread hosts the window (`gow3gpu_main_loop`). |
| SIGSEGV for GPU page tracking | Protection faults arrive as SIGBUS; both go to the GPU's handler. Write faults are told apart by `__es.__err` bit 1, which Rosetta fills in. |
| CPUID lists AVX, F16C, BMI1, MOVBE, LZCNT | Rosetta runs them but lists them only for processes started with `ROSETTA_ADVERTISE_AVX=1`: the loader starts itself again with it set. |
| `nanosleep`, `clock_nanosleep`, monotonic condition variables, timed mutex/rwlock waits | Ordinary macOS timers fire up to a third of their length late (coalescing): sleeps use kqueue timers marked `NOTE_CRITICAL` (`compat_sleep_ns`). Semaphore deadlines use `CLOCK_REALTIME` and timed lock waits poll (`src/compat_mac.c`). |
| Vulkan loader and the system's drivers | The loader shipped in `out/macos-deps` and KosmicKrisp: `VK_DRIVER_FILES` points at `out/vulkan/kosmickrisp_mesa_icd.json` unless set. MoltenVK cannot run the video core: it lacks `robustBufferAccess2` and `nullDescriptor`. |
| `libgow3gpu.so` with `--allow-shlib-undefined` | `libgow3gpu.dylib` with `-undefined dynamic_lookup`; the libraries are found through run paths (`@executable_path/gpu`, `@executable_path/lib`, then the build folders). |

Overrides: `VK_DRIVER_FILES` (another Vulkan driver), `GOW3_VULKAN_LIBRARY` (another Vulkan
loader or library), `GOW3_LTO=OFF bash build.sh` (faster builds of the GPU library).

Not available on macOS: Add to Steam, the Linux options (Wayland/X11, GameMode, MangoHud),
DLSS, userfaultfd tracking (`GOW3_UFFD`), PGO, and a packaged build (the port runs from the
repository). The performance overlay shows the GPU name only, as on Linux.
