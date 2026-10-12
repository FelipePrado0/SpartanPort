#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""God of War III Remastered — Linux Launcher.

Runs the same Tkinter launcher as Windows (gow3_launcher_win.py) with a thin
compatibility layer that replaces the handful of Windows-specific calls:
- ctypes.windll → no-ops or Linux equivalents
- %APPDATA% → $XDG_CONFIG_HOME (~/.config)
- subprocess.CREATE_NO_WINDOW → 0 (not needed on Linux)
- run.py → run.sh (the Linux game runner)
- .ico icon → .png icon
- DPI awareness → already handled by X11/Wayland
- Win32 job objects → os.killpg (process groups)
"""
import ctypes
import os
import signal
import subprocess
import sys
from pathlib import Path

# ---- Compatibility shims applied before importing the launcher ----

# 1. Fake ctypes.windll so imports don't crash
class _FakeWindll:
    """Absorbs any attribute access on ctypes.windll without errors."""
    class _Lib:
        def __getattr__(self, name):
            return lambda *a, **kw: 0
    def __getattr__(self, name):
        return self._Lib()

if not hasattr(ctypes, 'windll'):
    ctypes.windll = _FakeWindll()

# 2. Set APPDATA to XDG_CONFIG_HOME for the config path
if 'APPDATA' not in os.environ:
    os.environ['APPDATA'] = os.environ.get('XDG_CONFIG_HOME',
                                            str(Path.home() / '.config'))

# 3. subprocess.CREATE_NO_WINDOW doesn't exist on Linux
if not hasattr(subprocess, 'CREATE_NO_WINDOW'):
    subprocess.CREATE_NO_WINDOW = 0

# 4. Redirect to run.sh instead of run.py for Linux
PORT_DIR = Path(__file__).resolve().parent.parent

# Now import and patch the launcher
sys.path.insert(0, str(PORT_DIR / 'launcher'))
sys.path.insert(0, str(PORT_DIR / 'scripts'))

# Patch the module-level constants before the Launcher class reads them
import gow3_launcher_win as launcher

# Fix config directory for Linux
launcher.CONFIG_DIR = Path(os.environ.get('XDG_CONFIG_HOME',
                            Path.home() / '.config')) / 'gow3-launcher'
launcher.CONFIG_FILE = launcher.CONFIG_DIR / 'settings.json'
launcher.VERSION = launcher.VERSION + '-linux'

# Fix: game_environment should use run.sh not run.py
_original_run_command = launcher.run_command
def _linux_run_command():
    """On Linux, run.sh is the game runner (not run.py)."""
    return ['bash', str(PORT_DIR / 'run.sh')]
launcher.run_command = _linux_run_command

# Fix: no_console() doesn't apply on Linux
launcher.no_console = lambda: 0

# Fix: run_script needs to not pass creationflags
_original_run_script = getattr(launcher, 'run_script', None)

# Fix: windows_language → use LANG env
def _linux_language():
    lang = os.environ.get('LANG', 'en_US.UTF-8').split('_')[0].lower()
    lang_map = {'en': 'en', 'es': 'es', 'fr': 'fr', 'de': 'de', 'it': 'it',
                'pt': 'pt', 'ru': 'ru', 'ja': 'ja'}
    return lang_map.get(lang, 'en')
launcher.windows_language = _linux_language

# Fix: The Launcher.play method uses a Win32 JobObject wrapper. Replace with
# a simple process group approach for Linux.
class LinuxJobObject:
    """Linux replacement for Win32 job objects: uses process groups."""
    def __init__(self, process):
        self.pid = process.pid
        self.handle = None

    def terminate(self):
        try:
            os.killpg(os.getpgid(self.pid), signal.SIGTERM)
        except (ProcessLookupError, PermissionError, OSError):
            pass

    def close(self):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

# Patch JobObject if it exists
if hasattr(launcher, 'JobObject'):
    launcher.JobObject = lambda proc: LinuxJobObject(proc)

# Fix: attach_stdio is Windows-only (msvcrt)
launcher.attach_stdio = lambda: None

# Fix: run_role should use run.sh
def _linux_run_role(argv):
    """Linux: --run starts run.sh, --script runs a preparation script."""
    if argv[0] == '--script':
        path = argv[1]
        sys.argv = argv[1:]
    else:
        # Run the game via run.sh
        env = dict(os.environ)
        env['PYTHONUNBUFFERED'] = '1'
        result = subprocess.run(['bash', str(PORT_DIR / 'run.sh')] + argv[1:], env=env)
        return result.returncode
    try:
        import runpy
        runpy.run_path(path, run_name='__main__')
    except SystemExit as stop:
        return stop.code if isinstance(stop.code, int) else (0 if stop.code is None else 1)
    return 0
launcher.run_role = _linux_run_role


def main():
    # The launcher's main() equivalent
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox

    root = tk.Tk()
    # Use the PNG icon instead of .ico
    icon_png = PORT_DIR / 'launcher' / 'gow3.png'
    if icon_png.exists():
        try:
            icon = tk.PhotoImage(file=str(icon_png))
            root.iconphoto(True, icon)
        except tk.TclError:
            pass

    app = launcher.Launcher(root, tk, ttk, filedialog, messagebox)
    root.mainloop()


if __name__ == '__main__':
    main()
