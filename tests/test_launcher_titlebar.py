"""Tests for the native Windows dark title bar (DWMWA_USE_IMMERSIVE_DARK_MODE, DWMWA_CAPTION_COLOR, Issue #914)."""
import ctypes
import shutil
import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import spartanport_launcher_win as launcher  # noqa: E402

try:
    import tkinter as tk
except ImportError:  # pragma: no cover
    tk = None

OWN_ICO = ROOT / 'launcher' / 'spartanport.ico'
OWN_PNG = ROOT / 'launcher' / 'spartanport.png'


class DarkTitlebarUnitTest(unittest.TestCase):
    """Platform-independent unit tests and error-handling tests with mocking."""

    def test_non_windows_guard(self):
        with mock.patch('spartanport_launcher_win.WINDOWS', False):
            self.assertIsNone(launcher.toplevel_hwnd(mock.MagicMock()))
            self.assertFalse(launcher.native_dark_titlebar(mock.MagicMock()))

    def test_dwm_attribute_20_success(self):
        fake_root = mock.MagicMock()
        fake_root.winfo_id.return_value = 0x1234
        calls = []

        def fake_dwm_set(hwnd, attr, pv, cb):
            calls.append((hwnd, attr))
            return 0  # S_OK

        with mock.patch('spartanport_launcher_win.WINDOWS', True), \
             mock.patch('spartanport_launcher_win.toplevel_hwnd', return_value=0x9999), \
             mock.patch('ctypes.windll.dwmapi.DwmSetWindowAttribute', side_effect=fake_dwm_set):
            res = launcher.native_dark_titlebar(fake_root, '#0d0b0b')
            self.assertTrue(res)
            # Must set attribute 20 (immersive dark mode), attribute 35 (caption color), attribute 36 (text color)
            attrs = [attr for hwnd, attr in calls]
            self.assertIn(20, attrs)
            self.assertIn(35, attrs)
            self.assertIn(36, attrs)
            self.assertNotIn(19, attrs)

    def test_dwm_attribute_20_fail_fallback_19(self):
        fake_root = mock.MagicMock()
        calls = []

        def fake_dwm_set(hwnd, attr, pv, cb):
            calls.append((hwnd, attr))
            if attr == 20:
                return -1  # fail attr 20 (e.g. Windows 10 1809/1903)
            return 0  # S_OK for attr 19

        with mock.patch('spartanport_launcher_win.WINDOWS', True), \
             mock.patch('spartanport_launcher_win.toplevel_hwnd', return_value=0x9999), \
             mock.patch('ctypes.windll.dwmapi.DwmSetWindowAttribute', side_effect=fake_dwm_set):
            res = launcher.native_dark_titlebar(fake_root, '#0d0b0b')
            self.assertTrue(res)
            attrs = [attr for hwnd, attr in calls]
            self.assertIn(20, attrs)
            self.assertIn(19, attrs)

    def test_dwm_exception_handled_gracefully(self):
        fake_root = mock.MagicMock()
        with mock.patch('spartanport_launcher_win.WINDOWS', True), \
             mock.patch('spartanport_launcher_win.toplevel_hwnd', return_value=0x9999), \
             mock.patch('ctypes.windll.dwmapi.DwmSetWindowAttribute', side_effect=OSError('DWM not running')):
            res = launcher.native_dark_titlebar(fake_root, '#0d0b0b')
            self.assertFalse(res)

    def test_invalid_bg_hex_does_not_crash(self):
        fake_root = mock.MagicMock()
        calls = []

        def fake_dwm_set(hwnd, attr, pv, cb):
            calls.append((hwnd, attr))
            return 0

        with mock.patch('spartanport_launcher_win.WINDOWS', True), \
             mock.patch('spartanport_launcher_win.toplevel_hwnd', return_value=0x9999), \
             mock.patch('ctypes.windll.dwmapi.DwmSetWindowAttribute', side_effect=fake_dwm_set):
            res = launcher.native_dark_titlebar(fake_root, 'invalid-hex')
            self.assertTrue(res)
            attrs = [attr for hwnd, attr in calls]
            self.assertIn(20, attrs)
            self.assertNotIn(35, attrs)


@unittest.skipUnless(sys.platform == 'win32' and tk, 'needs Windows and Tk')
class NativeDarkTitlebarWindowsTest(unittest.TestCase):
    """Actual native Windows DWM tests executing against the Windows desktop window manager."""

    def setUp(self):
        self.root = tk.Tk()
        self.root.title('SpartanPort Dark Titlebar Test')
        self.root.geometry('300x150+50+50')
        self.root.configure(bg=launcher.BG)

    def tearDown(self):
        self.root.destroy()

    def test_toplevel_hwnd_returns_valid_frame(self):
        self.root.update_idletasks()
        wid = self.root.winfo_id()
        self.assertNotEqual(wid, 0)
        hwnd = launcher.toplevel_hwnd(self.root)
        self.assertIsNotNone(hwnd)
        self.assertNotEqual(hwnd, 0)
        user32 = ctypes.windll.user32
        self.assertTrue(bool(user32.IsWindow(hwnd)))
        # Top level frame wraps TkChild
        expected = user32.GetAncestor(wid, 2)
        self.assertEqual(hwnd, expected)

    def test_native_dark_titlebar_applies_immersive_dark_mode(self):
        self.root.update_idletasks()
        success = launcher.native_dark_titlebar(self.root, launcher.BG)
        self.assertTrue(success)
        self.root.update()

        hwnd = launcher.toplevel_hwnd(self.root)
        dwmapi = ctypes.windll.dwmapi
        # Read back DWMWA_USE_IMMERSIVE_DARK_MODE (20)
        val = ctypes.c_int()
        dwmapi.DwmGetWindowAttribute.argtypes = (ctypes.c_void_p, ctypes.c_uint, ctypes.c_void_p, ctypes.c_uint)
        dwmapi.DwmGetWindowAttribute.restype = ctypes.c_long
        hr = dwmapi.DwmGetWindowAttribute(hwnd, 20, ctypes.byref(val), ctypes.sizeof(val))
        self.assertEqual(hr, 0)
        self.assertEqual(val.value, 1)

    def test_native_titlebar_visual_pixel_contrast(self):
        """Visually verify title bar pixel color via Win32 PrintWindow into a DIB."""
        import ctypes.wintypes as wt
        self.root.update_idletasks()
        launcher.native_dark_titlebar(self.root, launcher.BG)
        self.root.update()

        user32 = ctypes.windll.user32
        gdi32 = ctypes.windll.gdi32
        hwnd = launcher.toplevel_hwnd(self.root)

        rect = wt.RECT()
        user32.GetWindowRect(hwnd, ctypes.byref(rect))
        w = max(10, rect.right - rect.left)
        h = max(10, rect.bottom - rect.top)

        hwnd_dc = user32.GetWindowDC(hwnd)
        mem_dc = gdi32.CreateCompatibleDC(hwnd_dc)
        bitmap = gdi32.CreateCompatibleBitmap(hwnd_dc, w, h)
        gdi32.SelectObject(mem_dc, bitmap)
        user32.PrintWindow(hwnd, mem_dc, 2)  # PW_RENDERFULLCONTENT

        class BITMAPINFOHEADER(ctypes.Structure):
            _pack_ = 1
            _fields_ = [('biSize', wt.DWORD), ('biWidth', wt.LONG), ('biHeight', wt.LONG), ('biPlanes', wt.WORD),
                        ('biBitCount', wt.WORD), ('biCompression', wt.DWORD), ('biSizeImage', wt.DWORD),
                        ('biXPelsPerMeter', wt.LONG), ('biYPelsPerMeter', wt.LONG), ('biClrUsed', wt.DWORD),
                        ('biClrImportant', wt.DWORD)]

        bmi = BITMAPINFOHEADER()
        bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
        bmi.biWidth = w
        bmi.biHeight = -h
        bmi.biPlanes = 1
        bmi.biBitCount = 32
        bmi.biCompression = 0
        data_size = w * 4 * h
        buffer = (ctypes.c_char * data_size)()
        gdi32.GetDIBits(mem_dc, bitmap, 0, h, buffer, ctypes.byref(bmi), 0)

        # Title bar pixel near center top
        x, y = w // 2, 15
        idx = (y * w + x) * 4
        b = buffer[idx] if isinstance(buffer[idx], int) else ord(buffer[idx])
        g = buffer[idx + 1] if isinstance(buffer[idx + 1], int) else ord(buffer[idx + 1])
        r = buffer[idx + 2] if isinstance(buffer[idx + 2], int) else ord(buffer[idx + 2])

        gdi32.DeleteObject(bitmap)
        gdi32.DeleteDC(mem_dc)
        user32.ReleaseDC(hwnd, hwnd_dc)

        # On dark title bar, background is dark (R,G,B all <= 30), not default light gray (> 220)
        self.assertLess(r, 40, f'Expected dark titlebar red component, got {r}')
        self.assertLess(g, 40, f'Expected dark titlebar green component, got {g}')
        self.assertLess(b, 40, f'Expected dark titlebar blue component, got {b}')

    def test_window_icon_applies_dark_titlebar(self):
        self.root.update_idletasks()
        launcher.window_icon(self.root, tk, OWN_ICO, OWN_PNG)
        self.root.update()
        hwnd = launcher.toplevel_hwnd(self.root)
        dwmapi = ctypes.windll.dwmapi
        val = ctypes.c_int()
        dwmapi.DwmGetWindowAttribute.argtypes = (ctypes.c_void_p, ctypes.c_uint, ctypes.c_void_p, ctypes.c_uint)
        dwmapi.DwmGetWindowAttribute.restype = ctypes.c_long
        hr = dwmapi.DwmGetWindowAttribute(hwnd, 20, ctypes.byref(val), ctypes.sizeof(val))
        self.assertEqual(hr, 0)
        self.assertEqual(val.value, 1)

    def test_window_lifecycle_transitions(self):
        """Test minimize, restore, focus, and resizing with dark titlebar."""
        self.root.update_idletasks()
        launcher.native_dark_titlebar(self.root, launcher.BG)
        self.root.update()

        # Minimize
        self.root.iconify()
        self.root.update()
        self.assertEqual(self.root.state(), 'iconic')

        # Restore
        self.root.deiconify()
        self.root.update()
        self.assertEqual(self.root.state(), 'normal')

        # Focus
        hwnd = launcher.toplevel_hwnd(self.root)
        user32 = ctypes.windll.user32
        user32.SetFocus(hwnd)
        self.root.update()

        # Resize
        self.root.geometry('400x250')
        self.root.update()
        self.assertIn('400x250', self.root.geometry())


if __name__ == '__main__':
    unittest.main()
