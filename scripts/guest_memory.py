#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Memory check for the guest memory pool (src/runtime_memory.c) before the game starts.

On Windows the pool is one SEC_COMMIT section of SPARTAN_DMEM_MB plus the 1 GiB flexible span,
charged in full at start. The 4K patch asks for about 11 GiB of direct memory: on a 16 GB PC
with a small system managed page file the start then depends on Windows growing the page file
in time, and the game closes ("cannot create ... MiB of guest memory (error 1450)") when it
does not. The page file may grow, so the commit free before the start says little; the
installed RAM is compared instead.
"""
import sys

DMEM_RETAIL_MB = 5056  # runtime_memory.c pool_size_bytes default
FLEX_SPAN_MB = 1024    # runtime_memory.c FLEX_SPAN
# The texture fix (on by default) and the 480p/720p patches ask for DMEM + 2048 MB: games that
# run at those sizes are not warned, only the resolution patches above 1080p.
DMEM_DEFAULT_MB = DMEM_RETAIL_MB + 2048
# The probe's own memory besides the pool. Measured on one PC (RTX 5070, 16 GB, 4K): about
# 2 GiB with stream buffers in GPU memory, more with them in system RAM (cached/host).
HEADROOM_MB = {'device': 2048, 'cached': 4096, 'host': 4096}


def pool_mb(dmem_mb):
    """The section runtime_memory.c commits for SPARTAN_DMEM_MB (out of range: the retail size)."""
    try:
        mb = int(dmem_mb) if dmem_mb else 0
    except ValueError:
        mb = 0
    if mb < DMEM_RETAIL_MB or mb > 16384:
        mb = DMEM_RETAIL_MB
    return mb + FLEX_SPAN_MB


def physical_memory_mb():
    """The installed RAM (GlobalMemoryStatusEx ullTotalPhys), or None off Windows."""
    if sys.platform != 'win32':
        return None
    import ctypes

    class MemoryStatus(ctypes.Structure):
        _fields_ = [('dwLength', ctypes.c_uint32), ('dwMemoryLoad', ctypes.c_uint32),
                    ('ullTotalPhys', ctypes.c_uint64), ('ullAvailPhys', ctypes.c_uint64),
                    ('ullTotalPageFile', ctypes.c_uint64), ('ullAvailPageFile', ctypes.c_uint64),
                    ('ullTotalVirtual', ctypes.c_uint64), ('ullAvailVirtual', ctypes.c_uint64),
                    ('ullAvailExtendedVirtual', ctypes.c_uint64)]

    status = MemoryStatus(dwLength=ctypes.sizeof(MemoryStatus))
    if not ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status)):
        return None
    return status.ullTotalPhys >> 20


def memory_warning(dmem_mb, physical_mb, stream_memory='cached'):
    """A warning when the pool and the probe's headroom need more than the installed RAM."""
    pool = pool_mb(dmem_mb)
    if physical_mb is None or pool <= DMEM_DEFAULT_MB + FLEX_SPAN_MB:
        return None
    stream = stream_memory if stream_memory in HEADROOM_MB else 'cached'
    needed = pool + HEADROOM_MB[stream]
    if needed <= physical_mb:
        return None
    advice = 'set Performance -> Stream buffer memory to GPU memory, ' if stream != 'device' else ''
    return (f'Memory: the guest memory pool commits {pool} MiB and the game needs about {needed} MiB '
            f'in all, more than the {physical_mb} MiB of RAM. The game may close at start (error 1450): '
            f'{advice}make sure the Windows page file can grow, or choose a lower render resolution.')
