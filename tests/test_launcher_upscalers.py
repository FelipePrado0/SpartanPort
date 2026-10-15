"""The launcher offers the upscalers the game can run on this platform, by the names it reads."""
import os
from pathlib import Path
import re
import sys
import tempfile
import unittest
from unittest import mock

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import gow3_launcher_win as launcher  # noqa: E402


def game_upscaler_names():
    """The gow3.ini upscaler names of gpu/shim/gow3_settings.cpp (UpscalerName)."""
    source = (ROOT / 'gpu' / 'shim' / 'gow3_settings.cpp').read_text(encoding='utf-8')
    names = re.search(r'names\[UpscalerCount\] = \{([^}]*)\}', source)
    return re.findall(r'"(\w+)"', names.group(1))


def game_screen_upscaler_names():
    """The gow3.ini screen_upscaler values of gpu/shim/gow3_graphics.h (ScreenUpscalerKeys)."""
    source = (ROOT / 'gpu' / 'shim' / 'gow3_graphics.h').read_text(encoding='utf-8')
    keys = re.search(r'ScreenUpscalerKeys = \{([^}]*)\}', source)
    return re.findall(r'"(\w+)"', keys.group(1))


class LauncherUpscalersTest(unittest.TestCase):
    def test_every_choice_is_a_name_the_game_reads(self):
        game = game_upscaler_names()
        self.assertIn('metalfx', game)
        for name, _labels in launcher.UPSCALERS:
            with self.subTest(name=name):
                self.assertIn(name, game)

    def test_metalfx_on_a_mac_and_dlss_elsewhere(self):
        names = [name for name, _labels in launcher.UPSCALERS]
        self.assertEqual('metalfx' in names, launcher.MACOS)
        self.assertEqual('dlss' in names, not launcher.MACOS)
        if launcher.MACOS:
            self.assertEqual(names[0], 'metalfx')  # Apple's own upscaler first
        self.assertEqual(names[-1], 'off')

    def test_screen_upscalers_are_the_names_the_game_reads(self):
        # FSR 1 first: the game's default, and what it uses for anything it does not know.
        self.assertEqual([name for name, _labels in launcher.SCREEN_UPSCALERS], game_screen_upscaler_names())
        self.assertEqual(launcher.SCREEN_UPSCALERS[0][0], 'fsr1')

    def test_screen_upscaler_is_kept_in_gow3_ini_on_a_mac_only(self):
        self.assertEqual('screen_upscaler' in launcher.INI_DEFAULTS, launcher.MACOS)
        if launcher.MACOS:
            self.assertEqual(launcher.INI_DEFAULTS['screen_upscaler'], 'fsr1')


class ScreenUpscalerPageTest(unittest.TestCase):
    """The Display page with the real Tk widgets, on a Mac and elsewhere (both simulated)."""

    def build_display(self, macos):
        try:
            import tkinter as tk
            from tkinter import ttk
            root = tk.Tk()
        except ImportError as error:
            self.skipTest(f'Tk unavailable: {error}')
        except tk.TclError as error:
            self.skipTest(f'Tk display unavailable: {error}')
        root.withdraw()
        self.addCleanup(root.destroy)
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        defaults = {key: value for key, value in launcher.INI_DEFAULTS.items() if key != 'screen_upscaler'}
        if macos:
            defaults['screen_upscaler'] = 'fsr1'
        for patch in (mock.patch.object(launcher, 'MACOS', macos),
                      mock.patch.dict(launcher.INI_DEFAULTS, defaults, clear=True),
                      mock.patch.object(launcher, 'CONFIG_DIR', Path(folder.name)),
                      mock.patch.object(launcher, 'CONFIG_FILE', Path(folder.name) / 'settings.json'),
                      mock.patch.dict(os.environ, {'GOW3_CONFIG': str(Path(folder.name) / 'gow3.ini')})):
            patch.start()
            self.addCleanup(patch.stop)
        app = launcher.Launcher.__new__(launcher.Launcher)
        app.tk, app.ttk, app.root, app.dpi = tk, ttk, root, 1.0
        app.content, app.pages, app.vars = ttk.Frame(root), {}, {}
        app.app = dict(launcher.APP_DEFAULTS)
        app.ini, app.ini_lines = launcher.load_ini()
        app.control_vars, app.mod_order, app.patch_vars = {}, [], {}
        app.pad_style = tk.StringVar(root, value='playstation')
        app.store_game_patches = mock.Mock()
        app.build_display()

        def widgets(parent):
            for child in parent.winfo_children():
                yield child
                yield from widgets(child)

        labels = tuple(launcher._(*text) for _value, text in launcher.SCREEN_UPSCALERS)
        boxes = [widget for widget in widgets(app.pages['display'])
                 if isinstance(widget, ttk.Combobox) and tuple(widget['values']) == labels]
        texts = [widget['text'] for widget in widgets(app.pages['display']) if isinstance(widget, ttk.Label)]
        return app, boxes, texts

    def test_mac_offers_metalfx_and_saves_it(self):
        app, boxes, texts = self.build_display(macos=True)
        self.assertEqual(len(boxes), 1)
        self.assertIn('Upscale to the window', texts)
        box = boxes[0]
        self.assertEqual(app.vars['screen_upscaler'].get(), 'fsr1')
        box.current(1)
        box.event_generate('<<ComboboxSelected>>')
        self.assertEqual(app.vars['screen_upscaler'].get(), 'metalfx')
        app.collect()
        saved = launcher.load_ini()[0]
        self.assertEqual((saved['screen_upscaler'], saved['fsr1']), ('metalfx', '0'))

    def test_elsewhere_the_page_is_unchanged(self):
        app, boxes, texts = self.build_display(macos=False)
        self.assertEqual(boxes, [])
        self.assertIn('Upscale with FSR 1', texts)
        app.collect()
        self.assertNotIn('screen_upscaler=', launcher.ini_path().read_text(encoding='utf-8'))


if __name__ == '__main__':
    unittest.main()
