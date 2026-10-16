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

    def test_fewer_gpu_waits_switch(self):
        # Off unless chosen; the Display page's switch writes it for run.py's next start.
        self.app.collect()
        self.assertEqual(self.saved()['fewer_gpu_waits'], '0')
        self.app.vars['fewer_gpu_waits'].set(True)
        self.app.collect()
        self.assertEqual(self.saved()['fewer_gpu_waits'], '1')
        sys.path.insert(0, str(ROOT / 'scripts'))
        import graphics_options
        options = graphics_options.startup_options(self.saved())
        self.assertEqual(graphics_options.environment(options)['GOW3_FEWER_GPU_WAITS'], '1')
        box = next(w for w in self.widgets() if isinstance(w, self.app.ttk.Checkbutton)
                   and str(w.cget('variable')) == str(self.app.vars['fewer_gpu_waits']))
        self.app.vars['stale_readback'].set(False)  # what it builds on
        self.assertEqual(str(box.cget('state')), 'disabled')
        self.app.vars['stale_readback'].set(True)
        self.assertEqual(str(box.cget('state')), 'normal')

    def widgets(self, parent=None):
        parent = parent or self.app.pages['display']
        for child in parent.winfo_children():
            yield child
            yield from self.widgets(child)

    def test_upscaling_note(self):
        # Under the Display page's upscaler when it has nothing to enlarge, with the lower render
        # resolutions: each of them is enlarged, none is required.
        self.assertIn('nothing to enlarge', launcher.upscaling_note(True, 'native', 1080))
        self.assertIn('nothing to enlarge', launcher.upscaling_note(True, 'native', 900))  # a laptop
        for upscale, resolution in ((False, 'native'), (True, '720p'), (True, '480p'), (False, '480p')):
            self.assertEqual(launcher.upscaling_note(upscale, resolution, 1080), '')
        # A taller screen (1440p, 4K): FSR 1 or MetalFX enlarges native 1080p to it.
        self.assertEqual(launcher.upscaling_note(True, 'native', 2160), '')
        buttons = {w.cget('text'): w for w in self.widgets() if isinstance(w, self.app.ttk.Button)
                   and w.cget('text') in ('720p', '480p')}
        note = buttons['720p'].master
        self.app.root.winfo_screenheight = lambda: 1080  # the note concerns screens up to 1080p
        self.app.vars['fsr1'].set(True)  # render resolution native (setUp's file)
        self.assertTrue(note.winfo_manager())
        buttons['480p'].invoke()
        self.assertEqual(self.app.vars['render_resolution'].get(), '480p')
        self.assertFalse(note.winfo_manager())
        self.app.vars['render_resolution'].set('native')
        buttons['720p'].invoke()
        self.assertEqual(self.app.vars['render_resolution'].get(), '720p')

    @unittest.skipUnless(launcher.MACOS, 'the screen upscaler choice is on a Mac')
    def test_upscaler_choice_needs_upscaling(self):
        box = next(w for w in self.widgets() if isinstance(w, self.app.ttk.Combobox)
                   and 'MetalFX (Apple)' in w.cget('values'))
        self.app.vars['fsr1'].set(False)
        self.assertEqual(str(box.cget('state')), 'disabled')
        self.app.vars['fsr1'].set(True)
        self.assertEqual(str(box.cget('state')), 'readonly')

    def whole_launcher(self, upscaler_page):
        """The launcher as it opens, with the experimental upscaler switched on in settings.json."""
        from tkinter import filedialog, messagebox
        load_json = launcher.load_json
        saved = lambda path, default: ({'experimental_upscaler': True} if path == launcher.CONFIG_FILE
                                       else load_json(path, default))
        with mock.patch.object(launcher, 'UPSCALER_PAGE', upscaler_page), \
                mock.patch.object(launcher, 'load_json', side_effect=saved):
            # setUp's window: the launcher makes its styles once in a Tk interpreter.
            return launcher.Launcher(self.app.root, self.app.tk, self.app.ttk, filedialog, messagebox)

    def test_a_mac_has_no_upscaler_page(self):
        # The temporal upscalers change nothing on a Mac yet: no page, and the game starts with them off.
        app = self.whole_launcher(upscaler_page=False)
        self.assertNotIn('graphics', app.nav)
        self.assertNotIn('graphics', app.pages)
        self.assertEqual(launcher.game_environment(app.app)['GOW3_EXPERIMENTAL_UPSCALER'], '0')

    def test_the_upscaler_page_elsewhere(self):
        app = self.whole_launcher(upscaler_page=True)
        self.assertIn('graphics', app.nav)
        self.assertIn('graphics', app.pages)
        self.assertEqual(launcher.game_environment(app.app)['GOW3_EXPERIMENTAL_UPSCALER'], '1')

if __name__ == '__main__':
    unittest.main()
