"""The launcher and the in-game menu both write gow3.ini: Play and closing the launcher keep what
the game saved meanwhile, a change made in the launcher wins, and the launcher shows the file as
the game left it once the game exits."""
import os
from pathlib import Path
import queue
import sys
import tempfile
import unittest
from unittest import mock

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import gow3_launcher_win as launcher  # noqa: E402


class GameSettingsTest(unittest.TestCase):
    def setUp(self):
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
        self.ini = Path(folder.name) / 'gow3.ini'
        self.ini.write_text('# kept\nvsync=1\nrender_resolution=native\ndisplay_mode=fullscreen\n'
                            'rcas_strength=75\nsharpness=0.50\noverlay_scale=150\n', encoding='utf-8')
        for patch in (mock.patch.object(launcher, 'CONFIG_DIR', Path(folder.name)),
                      mock.patch.object(launcher, 'CONFIG_FILE', Path(folder.name) / 'settings.json'),
                      mock.patch.dict(os.environ, {'GOW3_CONFIG': str(self.ini)})):
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
        app.build_graphics()
        app.build_display()
        app.link_sharpening()
        self.app = app

    def game_saves(self, **changes):
        """The in-game menu's save (gow3_settings.cpp Save): the whole file again, every key."""
        values = launcher.load_ini()[0]
        values.update(changes)
        self.ini.write_text('# gow3 settings (in-game menu: Insert / R3+L2)\n' +
                            ''.join(f'{key}={value}\n' for key, value in values.items()), encoding='utf-8')

    def saved(self):
        return launcher.load_ini()[0]

    def game_exits(self):
        app = self.app
        app.output, app.ui_calls = queue.Queue(), queue.Queue()
        app.output.put((0,))
        app.append, app.refresh_status = mock.Mock(), mock.Mock()
        app.job = app.process = None
        app.drain_output()
        app.refresh_status.assert_called_once()

    def test_play_keeps_what_the_game_saved(self):
        # Render resolution has no control on these pages; the others do.
        game = dict(vsync='0', render_resolution='720p', rcas_strength='40', sharpness='1.25', fsr1='1')
        self.game_saves(**game)
        before = self.saved()
        self.app.collect()
        self.assertEqual(self.saved(), before)
        self.assertFalse(self.app.vars['vsync'].get())  # the controls show it now
        self.assertTrue(self.app.vars['fsr1'].get())
        self.assertEqual(self.app.vars['rcas_strength'].get(), '40')
        self.assertEqual(self.app.vars['sharpness'].get(), 1.25)

    def test_a_change_made_here_wins(self):
        self.app.vars['display_mode'].set('borderless')
        self.game_saves(vsync='0', display_mode='windowed')
        self.app.collect()
        saved = self.saved()
        self.assertEqual((saved['display_mode'], saved['vsync']), ('borderless', '0'))

    def test_a_key_saved_here_follows_the_game_again(self):
        self.app.vars['vsync'].set(False)
        self.app.collect()  # Play
        self.assertEqual(self.saved()['vsync'], '0')
        self.game_saves(vsync='1')
        self.app.collect()  # closing the launcher
        self.assertEqual(self.saved()['vsync'], '1')
        self.assertTrue(self.app.vars['vsync'].get())

    def test_the_controls_show_the_file_after_the_game(self):
        self.app.vars['display_mode'].set('borderless')  # changed here, not saved yet
        self.game_saves(vsync='0', rcas_strength='40', display_mode='windowed')
        self.game_exits()
        self.assertFalse(self.app.vars['vsync'].get())
        self.assertEqual(self.app.vars['rcas_strength'].get(), '40')
        self.assertEqual(self.app.vars['display_mode'].get(), 'borderless')
        self.app.collect()
        saved = self.saved()
        self.assertEqual((saved['display_mode'], saved['vsync'], saved['rcas_strength']), ('borderless', '0', '40'))

    def test_comments_other_keys_and_missing_keys(self):
        self.app.collect()
        text = self.ini.read_text(encoding='utf-8')
        self.assertTrue(text.startswith('# kept\n'))
        self.assertIn('\noverlay_scale=150\n', text)
        self.assertIn('\nrender_resolution=native\n', text)
        for key in launcher.INI_DEFAULTS:
            self.assertIn(f'\n{key}=', text)


if __name__ == '__main__':
    unittest.main()
