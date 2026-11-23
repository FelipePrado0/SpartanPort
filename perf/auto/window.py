"""The game window without global input (Windows only): find it by process, keep it shown without
taking focus, send keys with PostMessage and capture it with PrintWindow.

Never SendInput: the person at the PC keeps their keyboard and mouse, and a key can never land in
another application.
"""
import ctypes
import struct
import sys
import zlib

if sys.platform != "win32":
    raise ImportError("perf/auto/window.py is Windows only")

from ctypes import wintypes

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)

WM_KEYDOWN, WM_KEYUP = 0x0100, 0x0101
SW_SHOWNOACTIVATE = 4
HWND_BOTTOM = 1
SWP_NOSIZE, SWP_NOMOVE, SWP_NOACTIVATE = 0x1, 0x2, 0x10
PW_RENDERFULLCONTENT = 3
# virtual key, scan code
KEYS = {"enter": (13, 28), "tab": (9, 15), "space": (32, 57), "s": (0x53, 31), "w": (0x57, 17),
        "escape": (27, 1)}

EnumWindowsProc = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
user32.PrintWindow.argtypes = [wintypes.HWND, wintypes.HDC, wintypes.UINT]
user32.GetDC.argtypes = [wintypes.HWND]
user32.GetDC.restype = wintypes.HDC
user32.ReleaseDC.argtypes = [wintypes.HWND, wintypes.HDC]
gdi32.CreateCompatibleDC.argtypes = [wintypes.HDC]
gdi32.CreateCompatibleDC.restype = wintypes.HDC
gdi32.CreateCompatibleBitmap.argtypes = [wintypes.HDC, ctypes.c_int, ctypes.c_int]
gdi32.CreateCompatibleBitmap.restype = wintypes.HBITMAP
gdi32.SelectObject.argtypes = [wintypes.HDC, wintypes.HGDIOBJ]
gdi32.SelectObject.restype = wintypes.HGDIOBJ
gdi32.DeleteObject.argtypes = [wintypes.HGDIOBJ]
gdi32.DeleteDC.argtypes = [wintypes.HDC]
gdi32.GetDIBits.argtypes = [wintypes.HDC, wintypes.HBITMAP, wintypes.UINT, wintypes.UINT,
                            ctypes.c_void_p, ctypes.c_void_p, wintypes.UINT]


def find(pids):
    """The largest visible top-level window owned by one of `pids`, or None."""
    found = []

    def visit(hwnd, _):
        pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value in pids and user32.IsWindowVisible(hwnd):
            r = wintypes.RECT()
            user32.GetClientRect(hwnd, ctypes.byref(r))
            found.append((r.right * r.bottom, hwnd))
        return True

    user32.EnumWindows(EnumWindowsProc(visit), 0)
    return max(found)[1] if found else None


def show(hwnd):
    """Restores a minimized window without activating it and puts it under the others: a minimized
    window neither captures nor reacts reliably."""
    if user32.IsIconic(hwnd):
        user32.ShowWindow(hwnd, SW_SHOWNOACTIVATE)
    user32.SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE)


def key(hwnd, name, hold=0.08):
    import time
    vk, scan = KEYS[name]
    show(hwnd)
    user32.PostMessageW(hwnd, WM_KEYDOWN, vk, 1 | scan << 16)
    time.sleep(hold)
    user32.PostMessageW(hwnd, WM_KEYUP, vk, 1 | scan << 16 | 1 << 30 | 1 << 31)


def _png(width, height, rgb_rows):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))
    raw = b"".join(b"\0" + row for row in rgb_rows)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def capture(hwnd, path):
    """Writes the client area to `path` as PNG; returns (width, height) or None."""
    show(hwnd)
    r = wintypes.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(r))
    w, h = r.right, r.bottom
    if w <= 0 or h <= 0:
        return None
    screen = user32.GetDC(hwnd)
    dc = gdi32.CreateCompatibleDC(screen)
    bmp = gdi32.CreateCompatibleBitmap(screen, w, h)
    old = gdi32.SelectObject(dc, bmp)
    try:
        if not user32.PrintWindow(hwnd, dc, PW_RENDERFULLCONTENT):
            return None
        # BITMAPINFOHEADER, top-down 32-bit BGRX
        header = struct.pack("<IiiHHIIiiII", 40, w, -h, 1, 32, 0, 0, 0, 0, 0, 0)
        info = ctypes.create_string_buffer(header + b"\0" * 16)
        pixels = ctypes.create_string_buffer(w * h * 4)
        if gdi32.GetDIBits(dc, bmp, 0, h, pixels, info, 0) != h:
            return None
        data = pixels.raw
        rows = []
        for y in range(h):
            row = data[y * w * 4:(y + 1) * w * 4]
            rgb = bytearray(w * 3)
            rgb[0::3], rgb[1::3], rgb[2::3] = row[2::4], row[1::4], row[0::4]
            rows.append(bytes(rgb))
        with open(path, "wb") as f:
            f.write(_png(w, h, rows))
        return w, h
    finally:
        gdi32.SelectObject(dc, old)
        gdi32.DeleteObject(bmp)
        gdi32.DeleteDC(dc)
        user32.ReleaseDC(hwnd, screen)
