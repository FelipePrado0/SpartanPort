#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Pre-flight system checks before starting the game.

Validates that the system has enough virtual memory (physical RAM + page file)
to satisfy the guest direct memory allocation, and gives actionable guidance
when it does not.  Called from run.py after the DMEM requirement is known.
"""
import ctypes
import ctypes.wintypes
import os
import struct
import subprocess
import sys
from pathlib import Path


WINDOWS = sys.platform in ('win32', 'cygwin')

# Win32 structures
if WINDOWS:
    import ctypes.wintypes

    class MEMORYSTATUSEX(ctypes.Structure):
        _fields_ = [
            ('dwLength',                ctypes.wintypes.DWORD),
            ('dwMemoryLoad',            ctypes.wintypes.DWORD),
            ('ullTotalPhys',            ctypes.c_uint64),
            ('ullAvailPhys',            ctypes.c_uint64),
            ('ullTotalPageFile',        ctypes.c_uint64),
            ('ullAvailPageFile',        ctypes.c_uint64),
            ('ullTotalVirtual',         ctypes.c_uint64),
            ('ullAvailVirtual',         ctypes.c_uint64),
            ('ullAvailExtendedVirtual', ctypes.c_uint64),
        ]


def _memory_status():
    """Current memory statistics from GlobalMemoryStatusEx (Windows only)."""
    if not WINDOWS:
        return None
    ms = MEMORYSTATUSEX()
    ms.dwLength = ctypes.sizeof(MEMORYSTATUSEX)
    if not ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(ms)):
        return None
    return ms


def _mib(b):
    """Bytes → MiB string."""
    return f'{b / (1024 * 1024):.0f}'


def _gib(b):
    """Bytes → GiB string with one decimal."""
    return f'{b / (1024 ** 3):.1f}'


# ── Preflight check ────────────────────────────────────────────────────────

# The loader adds its own overhead (image, relocations, stacks, GPU) on top of
# DMEM.  This margin accounts for that so the check does not give a false pass.
_OVERHEAD_MIB = 512


def check_memory(dmem_mib, quiet=False):
    """Check whether the system can likely satisfy a *dmem_mib* MiB allocation.

    Returns True if the launch should proceed, False if the system is very
    likely to fail with ERROR_NO_SYSTEM_RESOURCES (1450).

    When *quiet* is True, only the verdict is returned; nothing is printed.
    """
    ms = _memory_status()
    if ms is None:
        # Cannot query; let the game try and fail with its own message.
        return True

    required_bytes = (dmem_mib + _OVERHEAD_MIB) * 1024 * 1024
    total_phys     = ms.ullTotalPhys
    avail_phys     = ms.ullAvailPhys
    # Total commit limit = physical RAM + page file(s); this is the hard cap
    # on how much virtual memory all processes combined can reserve.
    total_commit   = ms.ullTotalPageFile   # Win32 name is misleading: this is the commit limit
    avail_commit   = ms.ullAvailPageFile   # available commit charge
    avail_virtual  = ms.ullAvailVirtual    # largest contiguous free VA region (approx)

    if not quiet:
        print(f'Memory check: need {_gib(required_bytes)} GB '
              f'(DMEM {dmem_mib} MiB + {_OVERHEAD_MIB} MiB overhead)', flush=True)
        print(f'  Physical RAM:    {_gib(total_phys)} GB total, '
              f'{_gib(avail_phys)} GB free', flush=True)
        print(f'  Commit limit:    {_gib(total_commit)} GB total, '
              f'{_gib(avail_commit)} GB free', flush=True)
        print(f'  Virtual address: {_gib(avail_virtual)} GB available', flush=True)

    # ── Verdict ─────────────────────────────────────────────────────────

    if avail_commit >= required_bytes and avail_virtual >= required_bytes:
        if not quiet:
            print('  Result: OK', flush=True)
        return True

    # ── Not enough: explain why and how to fix ──────────────────────────

    if not quiet:
        print('', flush=True)
        _print_failure(dmem_mib, required_bytes, ms)
    return False


def _print_failure(dmem_mib, required_bytes, ms):
    """Print a detailed, actionable error panel."""
    bar = '=' * 72
    print(bar)
    print('  MEMORY CHECK FAILED -- the game will not be able to start')
    print(bar)
    print()
    print(f'  The game needs approximately {_gib(required_bytes)} GB of contiguous')
    print(f'  virtual memory for its PS4 direct-memory pool ({dmem_mib} MiB)')
    print(f'  plus runtime overhead ({_OVERHEAD_MIB} MiB).')
    print()
    print(f'  Your system:')
    print(f'    Physical RAM ........... {_gib(ms.ullTotalPhys)} GB')
    print(f'    Available RAM .......... {_gib(ms.ullAvailPhys)} GB')
    print(f'    Commit limit (RAM+swap)  {_gib(ms.ullTotalPageFile)} GB')
    print(f'    Available commit ....... {_gib(ms.ullAvailPageFile)} GB')
    print(f'    Available VA space ..... {_gib(ms.ullAvailVirtual)} GB')
    print()

    shortfall = required_bytes - ms.ullAvailPageFile
    if shortfall > 0:
        extra_gib = shortfall / (1024 ** 3)
        print(f'  Shortfall: ~{extra_gib:.1f} GB more commit charge needed.')
    print()
    print('  HOW TO FIX:')
    print()
    print('  1. INCREASE THE PAGE FILE (most common fix)')
    print('     - Press Win+R, type SystemPropertiesAdvanced, press Enter')
    print('     - Performance -> Settings -> Advanced -> Virtual memory -> Change')
    print('     - Uncheck "Automatically manage paging file size"')
    print('     - Select drive C: -> Custom size:')
    print('        Initial size: 16384 MB   Maximum size: 24576 MB')
    print('     - Click Set -> OK -> OK -> RESTART your PC')
    print()
    print('     Or run the included script as Administrator:')
    print('       Right-click scripts\fix_pagefile.ps1 -> Run with PowerShell')
    print()
    print('  2. FREE UP MEMORY')
    print('     - Close browsers (each Chrome/Edge tab uses 100-500 MB)')
    print('     - Close other games, Discord, Spotify, large applications')
    print('     - Restart your PC to clear leaked memory')
    print()
    print('  3. REDUCE GAME MEMORY (last resort, may affect visuals)')
    print(f'     - Set GOW3_DMEM_MB=5056 in your environment, or')
    print(f'     - In gow3.ini set render_resolution=native and')
    print(f'       disable resolution patches to lower the DMEM requirement.')
    print()
    print(bar)


def check_or_warn(env, default_dmem_mib=5056):
    """Read GOW3_DMEM_MB from *env* and run the memory check.

    Returns True to proceed, False to abort.  When the check fails and a
    console is available, waits for the user to acknowledge before returning.
    """
    try:
        dmem_mib = int(env.get('GOW3_DMEM_MB', str(default_dmem_mib)))
    except (TypeError, ValueError):
        dmem_mib = default_dmem_mib

    if check_memory(dmem_mib):
        return True

    # Give the user time to read the message before the window closes.
    if ctypes.windll.kernel32.GetConsoleWindow():
        print()
        try:
            input('Press Enter to exit...')
        except EOFError:
            pass
    return False
