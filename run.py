#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Prepares the game image and starts gow3-probe (Windows and Linux; run.sh wraps it on Linux).

Environment variables (GOW3_GAME_DIR, GOW3_DATA_DIR, GOW3_PATCHES, GOW3_MODS_*,
GOW3_USER_DIR, ...). Extra arguments go to gow3-probe.
The in-game "Apply and restart" runs this script again through GOW3_RESTART_COMMAND;
`--after PID` waits for the previous game process to end first (its GPU device and memory).
"""
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import time

WINDOWS = sys.platform == 'win32'
EXE_SUFFIX = '.exe' if WINDOWS else ''
if WINDOWS:
    import ctypes

PORT = Path(__file__).resolve().parent


def fail(message):
    print(message, file=sys.stderr)
    sys.exit(1)


def wait_for(pid):
    if not WINDOWS:
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                return
            except PermissionError:
                pass
            time.sleep(0.1)
        return
    kernel32 = ctypes.windll.kernel32
    SYNCHRONIZE = 0x00100000
    handle = kernel32.OpenProcess(SYNCHRONIZE, False, pid)
    if handle:
        kernel32.WaitForSingleObject(handle, 30000)
        kernel32.CloseHandle(handle)


def no_console():
    """Started without a console (the launcher): console programs must not open their own."""
    if not WINDOWS:
        return 0
    return 0 if ctypes.windll.kernel32.GetConsoleWindow() else subprocess.CREATE_NO_WINDOW


def run_script(name, *args, capture=False):
    # Inside the packaged launcher executable (PyInstaller) the executable runs scripts itself.
    script = ['--script'] if getattr(sys, 'frozen', False) else []
    command = [sys.executable, *script, str(PORT / 'scripts' / name), *map(str, args)]
    if capture:
        result = subprocess.run(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, text=True,
                                creationflags=no_console())
        if result.returncode:
            sys.exit(result.returncode)
        return result.stdout
    # stdin given: the output handles are passed explicitly (a windowed launcher child would
    # get none otherwise).
    result = subprocess.run(command, stdin=subprocess.DEVNULL, creationflags=no_console())
    if result.returncode:
        sys.exit(result.returncode)
    return None


def launch_prefix(env):
    """Linux integrations chosen in the launcher: MangoHud is its Vulkan layer (an environment
    variable), GameMode a command that starts the game."""
    if WINDOWS:
        return []
    if env.get('GOW3_MANGOHUD') == '1':
        env.setdefault('MANGOHUD', '1')
    if env.get('GOW3_GAMEMODE') == '1':
        if gamemode := shutil.which('gamemoderun'):
            return [gamemode]
        print('GameMode: gamemoderun not found, starting without it', flush=True)
    return []


def find_executable(name):
    """bin/ (packaged), else out/ (built with build.sh)."""
    for candidate in (PORT / 'bin' / name, PORT / 'out' / name):
        if candidate.is_file():
            return candidate
    return None


def main():
    args = sys.argv[1:]
    if len(args) >= 2 and args[0] == '--after':
        wait_for(int(args[1]))
        args = args[2:]
    # As run.sh: relative paths (GOW3_GAME_DIR, GOW3_DATA_DIR, fsr4_shaders/) are from the port.
    os.chdir(PORT)
    env = os.environ
    data = Path(env.get('GOW3_DATA_DIR', PORT))
    out = data / 'out'
    out.mkdir(parents=True, exist_ok=True)
    env.setdefault('GOW3_CONFIG', str(data / 'gow3.ini'))
    # Values this script derived on the previous start ("Apply and restart" inherits them) are
    # derived again from the new options.
    for key in env.pop('GOW3_DERIVED', '').split(','):
        env.pop(key, None)
    derived = []
    sys.path.insert(0, str(PORT / 'scripts'))
    import graphics_options
    graphics, reverted = graphics_options.prepare(env['GOW3_CONFIG'], out / 'graphics_state.json')
    if reverted:
        print('Graphics: the last restart never reached the game; previous options restored', flush=True)
    env.update(graphics_options.environment(graphics))
    if not env.get('GOW3_FSR411_DIR') and not (PORT / 'fsr4_411').is_dir() and (data / 'fsr4_411').is_dir():
        env['GOW3_FSR411_DIR'] = str(data / 'fsr4_411')

    probe = Path(env['GOW3_PROBE']) if env.get('GOW3_PROBE') else find_executable('gow3-probe' + EXE_SUFFIX)
    if not probe or not probe.is_file():
        fail(f'gow3-probe{EXE_SUFFIX} not found: build it with build.sh or use a packaged build.')
    # Development builds run from the MSYS2 tree: its CLANG64 DLLs (SDL3, FFmpeg, ...).
    if WINDOWS and not (probe.parent / 'SDL3.dll').is_file():
        clang64 = Path(env.get('MSYS2_ROOT', r'C:\msys64')) / 'clang64' / 'bin'
        if clang64.is_dir():
            env['PATH'] = f'{clang64}{os.pathsep}{env.get("PATH", "")}'

    game = Path(env.get('GOW3_GAME_DIR', PORT.parent / 'CUSA01623'))
    if not (game / 'eboot.bin').is_file():
        fail(f'No eboot.bin in {game} (set GOW3_GAME_DIR).')
    update_view = None
    if env.get('GOW3_PATCH_DIR'):
        import game_update
        try:
            update_view = game_update.create_view(game, env['GOW3_PATCH_DIR'], out)
        except (ValueError, OSError) as error:
            fail(str(error))
        game = update_view
        print(f'Game and update combined from {env["GOW3_PATCH_DIR"]}', flush=True)
    original_game = game.resolve()
    game = Path(run_script('mods.py', game, '--out', out,
                           '--mods-dir', env.get('GOW3_MODS_DIR', data / 'mods'),
                           '--config', env.get('GOW3_MODS_CONFIG', data / 'mods.json'),
                           '--enabled', env.get('GOW3_MODS_ENABLED', '1'), capture=True).strip())
    mod_view = game if game.resolve() != original_game else None
    try:
        run_script('prepare.py', game, '--out', out)
        run_script('link_libc.py', game, '--out', out)
        run_script('link_modules.py', game, '--out', out)
        run_script('content_profile.py', game, '--out', out, '--sku', env.get('GOW3_CONTENT_SKU', 'full'))

        import patches
        from patches import game_profile_for, patch_requirements, patch_status, selected_patches
        profile = game_profile_for(game)
        engine_fps = 60
        # The window shows the game's icon (BMP: the format SDL loads without extra libraries); on
        # Linux the launcher's own icon (the AppImage ships the one chosen when it was built) and
        # an application id the desktop entry matches, so the shell shows the same icon.
        icon_file = game / 'sce_sys' / 'icon0.png'
        if not WINDOWS:
            env.setdefault('SDL_APP_ID', 'gow3')
            if (PORT / 'launcher' / 'gow3.png').is_file():
                icon_file = PORT / 'launcher' / 'gow3.png'
        try:
            from PIL import Image
            with Image.open(icon_file) as icon:
                icon.convert('RGBA').resize((64, 64), Image.LANCZOS).save(out / 'window_icon.bmp')
            env.setdefault('GOW3_WINDOW_ICON', str(out / 'window_icon.bmp'))
        except (ImportError, OSError):
            pass
        # The temporal upscalers and motion vectors are not calibrated for this game yet: they stay
        # off unless the launcher's experimental switch is on (then gow3.ini decides); linear
        # image readbacks fix its corrupted textures.
        if env.get('GOW3_EXPERIMENTAL_UPSCALER') != '1':
            env.setdefault('GOW3_UPSCALER', 'none')
        env.setdefault('GOW3_READBACK_LINEAR', '1')
        if profile:
            names = selected_patches(profile[1], profile[2], env.get('GOW3_PATCHES', ''), env.get('GOW3_PATCHES_ONLY') == '1')
            # gow3.ini decides the resolution and frame rate patches (in-game menu, launcher).
            names = graphics_options.patch_names(names, graphics)
            names = graphics_options.restrict_to_available(names, patches.available_patches(profile[1], profile[2]))
            env['GOW3_PATCHES'] = ';'.join(names)
            env['GOW3_PATCHES_ONLY'] = '1'
        (out / 'patch_status.json').unlink(missing_ok=True)
        run_script('patches.py', '--out', out, '--extra', env.get('GOW3_PATCHES', ''), '--game-dir', game,
                   '--patches-dir', env.get('GOW3_PATCHES_DIR', data / 'patches'),
                   '--patches-config', env.get('GOW3_PATCHES_CONFIG', data / 'patches.json'))
        if profile:
            # Only the patches found in this eboot.bin count: a frame rate patch that was skipped
            # leaves the engine at 60 FPS, and the notes' memory and VBlank needs follow the same list.
            applied = (patch_status(out) or {}).get('applied', [])
            engine_fps = graphics_options.applied_engine_fps(applied)
            dmem, vblank = patch_requirements(profile[1], applied, profile[2])
            for key, value in (('GOW3_DMEM_MB', dmem), ('GOW3_VBLANK_HZ', vblank)):
                if value and key not in env:
                    env[key] = str(value)
                    derived.append(key)
        if 'GOW3_VBLANK_HZ' not in env:
            env['GOW3_VBLANK_HZ'] = '60'
            derived.append('GOW3_VBLANK_HZ')
        # Without the frame rate patch the engine steps 1/60 s per frame: the present thread must
        # not flip faster (gow3_graphics.h PresentLimit), or the game speeds up.
        env['GOW3_ENGINE_FPS'] = str(engine_fps)
        derived.append('GOW3_ENGINE_FPS')
        print(f'Engine frame rate: {engine_fps} FPS'
              + ('' if engine_fps > 60 or graphics['engine_fps'] == '60' else
                 ' (the frame rate patch was not found in this eboot.bin)'), flush=True)
        env['GOW3_DERIVED'] = ','.join(derived)

        # The in-game restart starts this script again once this process is gone.
        # GPU caches (shaders, pipelines) beside the saves; read before main(), so set here.
        user_dir = Path(env.get('GOW3_USER_DIR', data / 'user')).resolve()
        user_dir.mkdir(parents=True, exist_ok=True)
        env.setdefault('GOW3_GPU_USER_DIR', str(user_dir))
        from save_backup import backup
        try:
            if made := backup(user_dir):
                print(f'Saves backed up to {made}', flush=True)
        except OSError as error:
            print(f'Save backup failed, starting anyway: {error}', flush=True)
        this = ['--run'] if getattr(sys, 'frozen', False) else [str(Path(__file__).resolve())]
        # Windows: the probe starts the new launch and ends, so it waits for this process. Linux:
        # the probe replaces itself with the new launch while this process still waits for it.
        after = ['--after', str(os.getpid())] if WINDOWS else []
        restart = [sys.executable, *this, *after, *args]
        env['GOW3_RESTART_COMMAND'] = subprocess.list2cmdline(restart) if WINDOWS else shlex.join(restart)
        command = [*launch_prefix(env), str(probe), str(out / 'boot-linked.bin'), '--content-profile', str(out / 'content.bin'),
                   '--patches', str(out / 'patches.bin'), '--app0', str(game),
                   '--user', str(user_dir),
                   '--timeout', env.get('GOW3_TIMEOUT', '0'), *args]
        # No stdin: an inherited pipe (shells such as Git Bash) cost the game its console output.
        return subprocess.run(command, stdin=subprocess.DEVNULL, creationflags=no_console()).returncode
    finally:
        if mod_view:
            shutil.rmtree(mod_view, ignore_errors=True)
        if update_view:
            game_update.remove_view(update_view)


if __name__ == '__main__':
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
