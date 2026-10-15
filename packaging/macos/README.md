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

## MetalFX upscaling (experimental)

On a Mac the temporal upscaler can also be Apple's MetalFX (its temporal scaler). Like FSR 3.1,
FSR 4 and TAA it stays off unless the experimental switch is on: in the launcher, **Graphics
(experimental) → Enable the upscaler**, then **Upscaler → MetalFX (Apple)**; without the
launcher, `GOW3_EXPERIMENTAL_UPSCALER=1 GOW3_UPSCALER=metalfx ./play-mac.sh` (or `upscaler=metalfx`
in `gow3.ini`). The preset and output resolution work as with FSR: Native AA is anti-aliasing at
1:1, Ultra Performance (3x) is MetalFX's largest scale on Apple GPUs. MetalFX has no sharpening
of its own; the upscaler sharpening setting runs the port's RCAS pass on its output, as for TAA.
The log says `Upscaler: MetalFX temporal <input> -> <output>` when it runs; when it cannot, it
says why and the game uses FSR 3.1 (`Upscaler: MetalFX unavailable (...)` or `... failed (...)`).

**Status:** in God of War III the temporal upscalers do not run yet on any system: the scene,
depth and camera detection they rely on is still the one of the Bloodborne port this one comes
from (`docs/gow3/ROADMAP.md`: kept in the code, off, until this game's frame is mapped), so
MetalFX, like FSR 3.1, changes nothing in the game for now. Its whole path was checked inside the
game on the game's own frames (a 1280x720 crop upscaled to 1920x1080, inputs and output read
back), and with tests outside it. FSR 3.1 does not start on KosmicKrisp (its backend cannot
create the context), so on a Mac falling back to FSR 3.1 means no temporal upscaling.

How it works: MetalFX gets FSR 3.1's inputs (scene color, depth, the port's motion vectors and
jitter) and its output replaces the scene color the same way (`vk_metalfx.mm`). KosmicKrisp backs
every `VkDeviceMemory` with an `MTLHeap` and exports it (`VK_EXT_external_memory_metal`), so the
upscaler's images get memory of their own and a Metal texture is placed at the start of each
heap with the descriptor KosmicKrisp gives the image: Vulkan and Metal see the same pixels, with
no copy through the CPU. MetalFX writes only private textures, so a blit in the same Metal command
buffer moves its result into the shared output image. KosmicKrisp exports no `MTLSharedEvent`
(no `VK_EXT_metal_objects`), so the host relays the synchronization: the upscale is committed at
once behind a wait for an event of its own, a thread releases it when the Vulkan submission that
copied the inputs has completed, and its completion handler signals a Vulkan timeline semaphore
that the next submission waits for. `GOW3_FRAME_STATS=1` adds a line every 5 s with MetalFX's GPU
time and the relay's gaps.

Cost on an M4 Pro: outside the game, MetalFX takes 2.1 ms of GPU time at Native AA for a 1080p
output, 1.5 ms at Quality, 1.1 ms at Performance and 0.7 ms at Ultra Performance (4K output: 6.9,
5.7, 4.6 and 2.8 ms). In the game, upscaling 1280x720 to 1920x1080 took 1.7 ms on the GPU, the
relay about 0.4 ms before it and 0.06 ms after it; with the copies and the sharpening pass the
frame took about 2.5 to 3 ms longer. Under Rosetta, MetalFX logs an `appleneuralengine ... Bad
argument error` when it creates a scaler (an arm64 build does not); its output matched the arm64
output in a test outside the game (64 to 74 dB PSNR).
