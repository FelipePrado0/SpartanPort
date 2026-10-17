# Mouse input

The port exposes SDL mouse input through `libSceMouse`. Physical motion is read in relative mode while the game is active; the launcher controls whether it is enabled, its sensitivity, and Y inversion. The left analog stick can optionally add relative mouse motion.

## Build and unit tests

On Windows, configure the GPU build with the normal project setup and build the focused target from an MSYS2 shell:

The CMake project also requires the repository submodules and the header-only Vulkan Memory Allocator. If this checkout was received without Git metadata, clone the repositories listed in `.gitmodules` into their listed paths and download VMA into `out/vma/vk_mem_alloc.h`.

With the MSYS2 CLANG64 packages installed, configure with:

```sh
export PATH="/c/msys64/clang64/bin:/c/msys64/usr/bin:$PATH"
export PKG_CONFIG_PATH=/c/msys64/clang64/lib/pkgconfig
cmake -S gpu -B out/gpu -G Ninja \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo -DGOW3_LTO=OFF \
	-DCMAKE_PREFIX_PATH=/c/msys64/clang64 \
	-DCMAKE_C_COMPILER=/c/msys64/clang64/bin/clang.exe \
	-DCMAKE_CXX_COMPILER=/c/msys64/clang64/bin/clang++.exe \
	-DVMA_INCLUDE_DIR="$PWD/out/vma" \
	-DVulkan_INCLUDE_DIR=/c/msys64/clang64/include \
	-DVulkan_LIBRARY=/c/msys64/clang64/lib/libvulkan-1.dll.a
ninja -C out/gpu mouse-test
out/gpu/mouse-test.exe
```

For a source-tree launcher run, build the full probe and copy it to the location `run.py` searches:

```sh
ninja -C out/gpu gow3-probe
cp out/gpu/gow3-probe.exe out/gow3-probe.exe
```

The regular test build also runs the mouse test:

```sh
./build.sh --test
```

The launcher control test can be run from PowerShell with:

```powershell
$env:PYTHONPATH = 'tests'
python -m unittest tests/test_launcher_controls.py
```

## Testing through the launcher

1. Start the launcher and open `Game > Controls`.
2. Leave `Enable mouse input` enabled. Set `Mouse sensitivity` to `1.0` and leave `Invert mouse and stick Y` disabled for the baseline test.
3. Start the game. Move the mouse and verify that the camera responds without the pointer escaping the game window.
4. Open the port menu with `Insert`, move and click the mouse over the menu, and verify that the game does not react. Close the menu and verify that movement resumes without a stuck button or jump.
5. Alt-tab away and back. Verify that held mouse buttons are released and that the camera does not jump when focus returns.
6. Enable `Map left analog stick to mouse movement`. Start the game and verify that a centered stick produces no movement, partial deflection produces slow movement, and full deflection produces faster movement.
7. Repeat the stick test with `Invert mouse and stick Y` enabled and confirm only vertical movement reverses.
8. With mouse-left-stick mode enabled, verify that WASD no longer moves the left stick. Mouse movement should pan wherever the game uses the left stick, including camera mode.
9. Set the four **Right analog stick** key fields to custom SDL names and verify those keys pan the right stick independently.
10. Set the four **Left analog stick** key fields to custom SDL names and verify those keys move the character. These keys are ignored while mouse-left-stick mode is enabled.
11. Assign the left, middle, and right mouse buttons to Cross, Triangle, and Circle, then verify each button in-game.
12. Change both sensitivity fields, save, restart the game, and confirm the values persist and affect the next launch.

The game log should show the normal runtime startup and no `sceMouse` stub message. If the game has no response, confirm that `GOW3_MOUSE=1` appears in the launcher-created process environment and that the game imported `sceMouseInit` and `sceMouseOpen`.