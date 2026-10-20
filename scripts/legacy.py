"""The project's former name, kept only to move players to the new one without losing anything.
Delete this file (and its callers) three releases after the rename: it is the only place in the
repository that still spells the old name.

Everything here copies; nothing a player made is moved, overwritten or deleted. Each step runs once
(MARKER in the launcher's settings folder)."""
import os
import shutil
import subprocess
from pathlib import Path

LEGACY_NAME = 'gow3'
LEGACY_TITLE = 'God of War III'
LEGACY_EXE = f'{LEGACY_TITLE}.exe'               # the launcher; installed updaters look for this name
LEGACY_PLAY_EXE = f'Play {LEGACY_TITLE}.exe'     # the windowless start that shortcuts point to
LEGACY_ENV = 'GOW3_'
LEGACY_PATCH_FILE = 'God_of_War_III_Remastered.xml'  # an older build's patches, left in patches/ by an update
LEGACY_DESKTOP_MARKER = 'X-GOW3-Integration=true'
LEGACY_CONFIG_DIR = f'{LEGACY_NAME}-launcher'  # the launcher's settings, in AppData or ~/.config
NEW_NAME, NEW_ENV = 'spartanport', 'SPARTAN_'
MARKER = 'legacy-migrated'
SHORTCUT_MARKER = 'shortcut-migrated'
DATA_SKIP = ('user/cache', 'out')
# Old binaries an update leaves behind, with the file that replaced each one.
LEFTOVERS = {
    f'bin/{LEGACY_NAME}-probe.exe': f'bin/{NEW_NAME}-probe.exe',
    f'bin/{LEGACY_NAME}-gpu-capabilities.exe': f'bin/{NEW_NAME}-gpu-capabilities.exe',
    f'launcher/{LEGACY_NAME}.ico': f'launcher/{NEW_NAME}.ico',
    f'launcher/{LEGACY_NAME}.png': f'launcher/{NEW_NAME}.png',
}


def forward_environment(env=None):
    """Old GOW3_* variables a player set by hand become SPARTAN_* ones; a SPARTAN_* already set wins,
    except the AppImage's default data folder, which only stands in for a choice not made."""
    env = os.environ if env is None else env
    for key, value in list(env.items()):
        if key.startswith(LEGACY_ENV):
            new = NEW_ENV + key[len(LEGACY_ENV):]
            if new not in env or (new == 'SPARTAN_DATA_DIR' and env[new] == _appimage_data_dir(env)):
                env[new] = value
    return env


def _appimage_data_dir(env):
    """The data folder AppRun picks when none is set ($XDG_DATA_HOME/spartanport)."""
    try:
        return str(Path(env.get('XDG_DATA_HOME') or Path.home() / '.local' / 'share') / NEW_NAME)
    except RuntimeError:  # no home folder
        return None


def _merge(old, new, skip=()):
    """Copies the files of folder `old` into `new` that `new` does not have yet, leaving out the
    folders in `skip` (relative to `old`)."""
    copied = 0
    for source in old.rglob('*'):
        relative = source.relative_to(old)
        if any(relative == Path(s) or Path(s) in relative.parents for s in skip):
            continue
        target = new / relative
        if source.is_dir():
            target.mkdir(parents=True, exist_ok=True)
        elif not target.exists():
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
            copied += 1
    return copied


def copy_game_settings(data_dir):
    """The old game settings file copied to the new name when that one does not exist yet (run.py
    started without the launcher). Returns whether it copied."""
    old, new = Path(data_dir) / f'{LEGACY_NAME}.ini', Path(data_dir) / f'{NEW_NAME}.ini'
    try:
        if old.is_file() and not new.exists():
            shutil.copy2(old, new)
            return True
    except OSError:
        pass  # the game still starts (with defaults); the copy is tried again next time
    return False


def migrate(data_dir, home=None, windows=os.name == 'nt'):
    """Copies the data folder (Linux) and the game settings file from their old names. The launcher's
    settings come from LEGACY_CONFIG_DIR afterwards (migrate_portable_settings in the launcher).
    Returns what was done, as text lines for the log."""
    data_dir = Path(data_dir)
    home = Path(home or Path.home())
    if (data_dir / MARKER).is_file():
        return []
    done = []
    old_data = data_dir.parent / LEGACY_NAME
    if data_dir.name == NEW_NAME and old_data.is_dir():
        # The GPU caches (rebuilt while playing, can be gigabytes) and the run views in out/ (links
        # into the game dump) stay behind: copying them would stall the first start.
        done.append(f'data: {_merge(old_data, data_dir, DATA_SKIP)} files from {old_data}')
    old_ini = data_dir / f'{LEGACY_NAME}.ini'
    if copy_game_settings(data_dir):
        done.append(f'game settings: {old_ini} copied to {NEW_NAME}.ini')
    if not windows:
        entry = home / '.local' / 'share' / 'applications' / f'{LEGACY_NAME}.desktop'
        try:
            if entry.is_file() and LEGACY_DESKTOP_MARKER in entry.read_text(encoding='utf-8', errors='replace'):
                entry.unlink()  # the launcher writes the new menu entry itself
                done.append(f'menu: removed {entry}')
        except OSError:
            pass
    if done or old_data.is_dir() or old_ini.is_file():
        data_dir.mkdir(parents=True, exist_ok=True)
        (data_dir / MARKER).write_text('\n'.join(done) + '\n', encoding='utf-8')
    return done


def remove_leftovers(port_dir):
    """Deletes the old binaries an update left next to their replacements. The launcher's copies
    under the old names (LEGACY_EXE, LEGACY_PLAY_EXE) stay: shortcuts still point to them."""
    port_dir = Path(port_dir)
    for old, new in LEFTOVERS.items():
        try:
            if (port_dir / new).is_file() and (port_dir / old).is_file():
                (port_dir / old).unlink()
        except OSError:
            pass  # in use or read-only: tried again at the next start


def _windows_desktop():
    try:
        import ctypes.wintypes
        buf = ctypes.create_unicode_buffer(ctypes.wintypes.MAX_PATH)
        ctypes.windll.shell32.SHGetFolderPathW(None, 0x0010, None, 0, buf)
        if buf.value:
            return Path(buf.value)
    except (AttributeError, OSError):
        pass
    return Path(os.environ.get('USERPROFILE', Path.home())) / 'Desktop'


def retarget_desktop_shortcut(port_dir, desktop=None, data_dir=None):
    """Windows: the desktop shortcut the old launcher made (named after the old title, pointing to
    God of War III.exe with no arguments) is migrated non-destructively by creating SpartanPort.lnk
    pointing to SpartanPort.exe. User-created, renamed, or customized shortcuts are preserved. Runs once."""
    if os.name != 'nt':
        return False
    port_dir = Path(port_dir)
    data_dir = Path(data_dir or port_dir)
    marker = data_dir / SHORTCUT_MARKER
    if marker.is_file():
        return False
    desktop = Path(desktop or _windows_desktop())
    old, new = desktop / f'{LEGACY_TITLE}.lnk', desktop / 'SpartanPort.lnk'
    target = port_dir / 'SpartanPort.exe'
    legacy_target = port_dir / LEGACY_EXE
    if new.exists() or not old.is_file():
        try:
            data_dir.mkdir(parents=True, exist_ok=True)
            marker.write_text('', encoding='utf-8')
        except OSError:
            pass
        return False
    if not target.is_file():
        return False
    script = (
        '$w = New-Object -ComObject WScript.Shell;'
        '$s = $w.CreateShortcut($env:SP_OLD);'
        'if (![string]::IsNullOrWhiteSpace($s.TargetPath)) {'
        '  $target = [System.IO.Path]::GetFullPath($s.TargetPath);'
        '  $legacy = [System.IO.Path]::GetFullPath($env:SP_LEGACY);'
        '  $port = [System.IO.Path]::GetFullPath($env:SP_PORT);'
        '  $work = if ([string]::IsNullOrWhiteSpace($s.WorkingDirectory)) { $port } else { [System.IO.Path]::GetFullPath($s.WorkingDirectory) };'
        '  $args = if ($s.Arguments) { $s.Arguments.Trim() } else { "" };'
        '  if (($target -ieq $legacy) -and ($args -eq "") -and ($work -ieq $port)) {'
        '    $n = $w.CreateShortcut($env:SP_NEW);'
        '    $n.TargetPath = $env:SP_TARGET;'
        '    $n.WorkingDirectory = $env:SP_PORT;'
        '    $n.IconLocation = $env:SP_TARGET + ",0";'
        '    $n.Save();'
        '    if (!(Test-Path -LiteralPath $env:SP_NEW)) { exit 1; }'
        '  }'
        '}'
    )
    env = {
        **os.environ,
        'SP_OLD': str(old),
        'SP_NEW': str(new),
        'SP_PORT': str(port_dir.resolve()),
        'SP_TARGET': str(target.resolve()),
        'SP_LEGACY': str(legacy_target.resolve()),
    }
    try:
        proc = subprocess.run(
            ['powershell', '-NoProfile', '-NonInteractive', '-Command', script],
            env=env,
            capture_output=True,
            timeout=20,
            creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0)
        )
    except (OSError, subprocess.SubprocessError):
        return False
    if proc.returncode != 0:
        return False
    try:
        data_dir.mkdir(parents=True, exist_ok=True)
        marker.write_text('', encoding='utf-8')
    except OSError:
        pass
    return new.is_file()
