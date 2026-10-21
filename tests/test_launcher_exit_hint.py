import json
import tempfile
from pathlib import Path
"""Fixes the community found for a game that stays black or closes at start (#263, #175, #208)."""
import os
import subprocess
import sys
import io
import unittest
from unittest import mock

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import spartanport_launcher_win as launcher  # noqa: E402


class ExitHintTest(unittest.TestCase):
    def test_guest_memory_failure_points_to_the_page_file_on_windows(self):
        hint = launcher.exit_hint('Runtime: cannot create 8128 MiB of guest memory (error 1450); '
                                  'is the page file large enough?\n')
        self.assertIn('page file', hint)
        self.assertIn('Windows could not reserve', hint)

    def test_guest_memory_failure_points_to_linux_memory_on_linux(self):
        hint = launcher.exit_hint('Runtime: cannot create 6080 MiB of guest memory (error 12); '
                                  'is /dev/shm or RAM large enough?\n')
        self.assertIn('Linux could not allocate', hint)
        self.assertIn('/dev/shm', hint)
        self.assertNotIn('page file', hint)

    def test_guest_memory_mapping_failure_on_linux(self):
        with mock.patch.object(launcher, 'WINDOWS', False):
            hint = launcher.exit_hint('Runtime: cannot map 6080 MiB of guest memory (error 12)\n')
            self.assertIn('Linux could not allocate', hint)
            self.assertIn('RAM, swap space, or /dev/shm', hint)

    def test_windows_environment_uses_windows_advice_for_generic_error(self):
        with mock.patch.object(launcher, 'WINDOWS', True):
            hint = launcher.exit_hint('Runtime: cannot create 6080 MiB of guest memory (error 1450)\n')
            self.assertIn('Windows could not reserve', hint)
            self.assertIn('page file', hint)

    def test_linux_environment_uses_linux_advice_for_generic_error(self):
        with mock.patch.object(launcher, 'WINDOWS', False):
            hint = launcher.exit_hint('Runtime: cannot create 6080 MiB of guest memory (error 12)\n')
            self.assertIn('Linux could not allocate', hint)
            self.assertIn('/dev/shm', hint)

    def test_debug_exception_while_reading_files_points_to_the_dump(self):
        hint = launcher.exit_hint('STOP: guest raised debug exception code=0xa0020001 argument=0x0\n')
        self.assertIn('pkg', hint)

    def test_other_lines_have_no_hint(self):
        for line in ('The game exited (code 3).\n',
                     'STOP: guest raised debug exception code=0xa0020014 argument=0x0\n',
                     'Runtime: guest thread \'my_decode_thread\' created\n'):
            self.assertIsNone(launcher.exit_hint(line))

    def test_the_hint_follows_the_exit_line(self):
        owner = launcher.Launcher.__new__(launcher.Launcher)
        owner.ui_calls = launcher.queue.Queue()
        owner.output = launcher.queue.Queue()
        owner.process, owner.job, owner.exit_hint = object(), None, None
        owner.root, owner.status = mock.Mock(), mock.Mock()
        owner.refresh_status = mock.Mock()
        shown = []
        owner.append = shown.append
        owner.output.put('Runtime: cannot create 8128 MiB of guest memory (error 1450); is the page file large enough?\n')
        owner.output.put('Runtime: memory operations=0\n')
        owner.output.put((3,))
        owner.drain_output()
        self.assertIn('exited (code 3)', shown[-2])
        self.assertIn('page file', shown[-1])



    def test_exit_hint_switches_to_advanced_page_and_scrolls_log(self):
        owner = launcher.Launcher.__new__(launcher.Launcher)
        owner.ui_calls = launcher.queue.Queue()
        owner.output = launcher.queue.Queue()
        owner.process, owner.job, owner.exit_hint = object(), None, None
        owner.root, owner.status = mock.Mock(), mock.Mock()
        owner.refresh_status = mock.Mock()
        owner.append = mock.Mock()
        mock_canvas = mock.Mock()
        mock_page = mock.Mock()
        mock_page.canvas = mock_canvas
        owner.pages = {'advanced': mock_page}
        owner.show = mock.Mock()
        owner.log = mock.Mock()
        owner.output.put('Runtime: cannot create 8128 MiB of guest memory (error 1450); is the page file large enough?\n')
        owner.output.put((21,))
        owner.drain_output()
        owner.show.assert_called_with('advanced')
        mock_canvas.yview_moveto.assert_called_with(1.0)
        owner.log.see.assert_called_with('end')

    def test_exit_hint_unhides_launcher_when_hidden_for_game(self):
        owner = launcher.Launcher.__new__(launcher.Launcher)
        owner.ui_calls = launcher.queue.Queue()
        owner.output = launcher.queue.Queue()
        owner.process, owner.job, owner.exit_hint = object(), None, None
        owner.root, owner.status = mock.Mock(), mock.Mock()
        owner.refresh_status = mock.Mock()
        owner.append = mock.Mock()
        owner.hidden_for_game = True
        owner.output.put('Runtime: cannot create 8128 MiB of guest memory (error 1450); is the page file large enough?\n')
        owner.output.put((21,))
        owner.drain_output()
        owner.root.deiconify.assert_called_once()
        owner.root.destroy.assert_not_called()
        self.assertFalse(owner.hidden_for_game)

    def test_clean_exit_destroys_root_when_hidden_for_game(self):
        owner = launcher.Launcher.__new__(launcher.Launcher)
        owner.ui_calls = launcher.queue.Queue()
        owner.output = launcher.queue.Queue()
        owner.process, owner.job, owner.exit_hint = object(), None, None
        owner.root, owner.status = mock.Mock(), mock.Mock()
        owner.refresh_status = mock.Mock()
        owner.append = mock.Mock()
        owner.hidden_for_game = True
        owner.output.put('The game exited normally.\n')
        owner.output.put((0,))
        owner.drain_output()
        owner.root.destroy.assert_called_once()

    @unittest.skipUnless(sys.platform == 'win32', 'MessageBoxW and windll are Windows only')
    def test_play_without_window_logs_hint_and_shows_messagebox(self):
        fake_log = io.StringIO()
        fake_log.close = mock.Mock()
        fake_proc = mock.Mock()
        fake_proc.stdout.readline.side_effect = [
            b'Runtime: cannot create 8128 MiB of guest memory (error 1450); is the page file large enough?\n',
            b''
        ]
        fake_proc.wait.return_value = 21

        with mock.patch('spartanport_launcher_win.game_environment', return_value={}), \
             mock.patch('spartanport_launcher_win.open_run_log', return_value=fake_log), \
             mock.patch('subprocess.Popen', return_value=fake_proc), \
             mock.patch('ctypes.windll.kernel32.GetConsoleWindow', return_value=0), \
             mock.patch('ctypes.windll.user32.MessageBoxW') as mock_msgbox:
            code = launcher.play_without_window({})
            self.assertEqual(code, 21)
            self.assertIn('Windows could not reserve the memory the game needs', fake_log.getvalue())
            mock_msgbox.assert_called_once()
            args = mock_msgbox.call_args[0]
            self.assertIn('page file', args[1])


    @unittest.skipUnless(sys.platform == 'win32', 'MessageBoxW and windll are Windows only')
    def test_play_without_window_clean_exit_does_not_show_messagebox(self):
        fake_log = io.StringIO()
        fake_log.close = mock.Mock()
        fake_proc = mock.Mock()
        fake_proc.stdout.readline.side_effect = [
            b'The game is starting normally.\n',
            b''
        ]
        fake_proc.wait.return_value = 0

        with mock.patch('spartanport_launcher_win.game_environment', return_value={}), \
             mock.patch('spartanport_launcher_win.open_run_log', return_value=fake_log), \
             mock.patch('subprocess.Popen', return_value=fake_proc), \
             mock.patch('ctypes.windll.kernel32.GetConsoleWindow', return_value=0), \
             mock.patch('ctypes.windll.user32.MessageBoxW') as mock_msgbox:
            code = launcher.play_without_window({})
            self.assertEqual(code, 0)
            mock_msgbox.assert_not_called()
            self.assertNotIn('Windows could not reserve the memory', fake_log.getvalue())

class HybridGpuTest(unittest.TestCase):
    @unittest.skipUnless(sys.platform == 'win32', 'the AMD switchable graphics layer is Windows only')
    def test_windows_hides_the_amd_switchable_layer(self):
        self.assertEqual(os.environ['VK_LOADER_LAYERS_DISABLE'], 'VK_LAYER_AMD_switchable_graphics')

    @unittest.skipUnless(sys.platform == 'win32', 'the AMD switchable graphics layer is Windows only')
    def test_a_value_set_by_the_player_is_kept(self):
        env = {**os.environ, 'VK_LOADER_LAYERS_DISABLE': 'VK_LAYER_custom'}
        code = (f'import sys; sys.path.insert(0, {str(ROOT / "launcher")!r})\n'
                'import os, spartanport_launcher_win\n'
                'print(os.environ["VK_LOADER_LAYERS_DISABLE"])')
        done = subprocess.run([sys.executable, '-c', code], env=env, capture_output=True, text=True)
        self.assertEqual(done.stdout.strip().splitlines()[-1], 'VK_LAYER_custom')


if __name__ == '__main__':
    unittest.main()


class RealLauncherExitTest(unittest.TestCase):
    def setUp(self):
        import tkinter as tk
        from tkinter import filedialog, messagebox, ttk
        try:
            self.root = tk.Tk()
        except tk.TclError as error:
            self.skipTest(f'Tk display unavailable: {error}')
        self.root.withdraw()
        self.addCleanup(self.destroy_root)
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.folder = Path(temporary.name)
        self.game = self.folder / 'Image0'
        (self.game / 'sce_module').mkdir(parents=True)
        (self.game / 'eboot.bin').write_bytes(b'dummy executable')
        config = self.folder / 'config'
        config.mkdir()
        config_file = config / 'settings.json'
        config_file.write_text(json.dumps({**launcher.APP_DEFAULTS, 'game_dir': str(self.game),
                                           'ui_language': 'en', 'check_updates': False}), encoding='utf-8')
        for name, value in [('CONFIG_DIR', config), ('CONFIG_FILE', config_file), ('DATA_DIR', self.folder)]:
            patch = mock.patch.object(launcher, name, value)
            patch.start()
            self.addCleanup(patch.stop)
        gpu = mock.patch.object(launcher, 'list_gpus', return_value=[])
        gpu.start()
        self.addCleanup(gpu.stop)
        self.app = launcher.Launcher(self.root, tk, ttk, filedialog, messagebox)
        self.app.messagebox = mock.Mock()
        self.root.update()

    def destroy_root(self):
        for callback in self.root.tk.call('after', 'info'):
            self.root.after_cancel(callback)
        self.root.destroy()

    def test_real_launcher_memory_failure_navigates_to_advanced_without_key_error(self):
        self.assertEqual(self.app.current_page, 'home')
        self.app.output.put('Runtime: cannot create 8128 MiB of guest memory (error 1450); is the page file large enough?\n')
        self.app.output.put((21,))
        self.app.drain_output()
        self.root.update()
        self.assertEqual(self.app.current_page, 'advanced')
        log_text = self.app.log.get('1.0', 'end')
        self.assertIn('page file', log_text)
        self.assertIn('cannot create 8128 MiB of guest memory', log_text)

    def test_real_launcher_restores_from_hidden_state_on_memory_failure(self):
        self.app.hidden_for_game = True
        self.root.withdraw()
        self.app.output.put('Runtime: cannot create 8128 MiB of guest memory (error 1450); is the page file large enough?\n')
        self.app.output.put((21,))
        self.app.drain_output()
        self.root.update()
        self.assertFalse(self.app.hidden_for_game)
        self.assertEqual(self.root.state(), 'normal')
        self.assertEqual(self.app.current_page, 'advanced')
