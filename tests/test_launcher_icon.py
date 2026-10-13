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
import gow3_launcher_win as launcher  # noqa: E402

try:
    import tkinter as tk
except ImportError:  # pragma: no cover
    tk = None
try:
    import PIL  # noqa: F401
    PILLOW = True
except ImportError:
    PILLOW = False

OWN_ICON = ROOT / 'launcher' / 'gow3.png'
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

    def test_own_png_is_the_window_icon_without_a_game(self):
        kept = launcher.window_icon(self.root, tk, self.tmp / 'missing' / 'icon0.png', OWN_ICON, self.tmp / 'game.ico')
        self.assertIsNotNone(kept)  # the PhotoImage the window must keep alive
        big, small = self.window_icons()
        self.assertTrue(big and small, (big, small))

    @unittest.skipUnless(PILLOW, 'needs Pillow')
    def test_game_icon_becomes_an_ico_and_the_window_icon(self):
        ico = self.tmp / 'icons' / 'game.ico'
        launcher.window_icon(self.root, tk, OWN_ICON, OWN_ICON, ico)
        self.assertTrue(ico.is_file())
        big, small = self.window_icons()
        self.assertTrue(big and small, (big, small))


if __name__ == '__main__':
    unittest.main()
