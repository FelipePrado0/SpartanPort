"""The launcher window's icon on Windows. Tk 9 applies the -default forms of wm iconphoto and
wm iconbitmap only to toplevels created later, so the window needs the icon set on itself."""
import ctypes
import shutil
import sys
import tempfile
import unittest
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
WM_GETICON, ICON_SMALL, ICON_BIG = 0x7F, 0, 1


@unittest.skipUnless(sys.platform == 'win32' and tk, 'needs Windows and Tk')
class WindowIconTest(unittest.TestCase):
    def setUp(self):
        self.root = tk.Tk()
        self.root.geometry('200x100+0+0')
        self.tmp = Path(tempfile.mkdtemp())

    def tearDown(self):
        self.root.destroy()
        shutil.rmtree(self.tmp, ignore_errors=True)

    def window_icons(self):
        self.root.update()
        user32 = ctypes.windll.user32
        user32.GetAncestor.argtypes, user32.GetAncestor.restype = (ctypes.c_void_p, ctypes.c_uint), ctypes.c_void_p
        user32.SendMessageW.argtypes = (ctypes.c_void_p, ctypes.c_uint, ctypes.c_void_p, ctypes.c_void_p)
        user32.SendMessageW.restype = ctypes.c_void_p  # an HICON, 64-bit
        hwnd = user32.GetAncestor(self.root.winfo_id(), 2)
        return user32.SendMessageW(hwnd, WM_GETICON, ICON_BIG, 0), user32.SendMessageW(hwnd, WM_GETICON, ICON_SMALL, 0)

    def test_own_ico_is_the_window_icon(self):
        self.assertIsNone(launcher.window_icon(self.root, tk, OWN_ICO, OWN_PNG))
        big, small = self.window_icons()
        self.assertTrue(big and small, (big, small))

    def test_own_png_when_the_ico_is_missing(self):
        kept = launcher.window_icon(self.root, tk, self.tmp / 'missing.ico', OWN_PNG)
        self.assertIsNotNone(kept)  # the PhotoImage the window must keep alive
        big, small = self.window_icons()
        self.assertTrue(big and small, (big, small))

    def test_icon_has_every_windows_size(self):
        sizes = [int.from_bytes(OWN_ICO.read_bytes()[6 + 16 * i:7 + 16 * i], 'little') or 256
                 for i in range(int.from_bytes(OWN_ICO.read_bytes()[4:6], 'little'))]
        self.assertEqual(sorted(sizes), [16, 24, 32, 48, 64, 128, 256])


if __name__ == '__main__':
    unittest.main()
