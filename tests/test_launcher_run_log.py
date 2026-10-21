"""The game's output goes to <user folder>/last_run.log from both launch paths."""
import io
import queue
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest import mock

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import spartanport_launcher_win as launcher  # noqa: E402


class RunLogTest(unittest.TestCase):
    def test_play_passes_the_run_log_to_the_output_reader(self):
        owner = launcher.Launcher.__new__(launcher.Launcher)
        owner.process = None
        owner.app = {**launcher.APP_DEFAULTS, 'game_dir': 'synthetic-game'}
        for name in ('collect', 'offer_system_files', 'set_log', 'play_button', 'status'):
            setattr(owner, name, mock.Mock(return_value=True))
        process, log = mock.Mock(), launcher._NullLog()
        with mock.patch.object(launcher, 'game_info', return_value=('SpartanPort', '01.02')), \
                mock.patch.object(launcher, 'missing_game_files', return_value=[]), \
                mock.patch.object(launcher, 'open_play_log', return_value=log), \
                mock.patch.object(launcher.subprocess, 'Popen', return_value=process), \
                mock.patch.object(launcher, 'GameJob'), \
                mock.patch.object(launcher.threading, 'Thread') as thread:
            owner.play()
        self.assertEqual(thread.call_args.kwargs['args'], (process, log))
        self.assertEqual(thread.call_args.kwargs['target'], owner.read_output)

    def test_log_path_follows_the_saves_folder_setting(self):
        self.assertEqual(launcher.run_log_path({'user_dir': ''}),
                         launcher.DATA_DIR / 'user' / 'last_run.log')
        self.assertEqual(launcher.run_log_path({'user_dir': r'D:\saves'}),
                         Path(r'D:\saves') / 'last_run.log')
        self.assertEqual(launcher.run_log_path({}), launcher.DATA_DIR / 'user' / 'last_run.log')

    def test_open_run_log_creates_the_folder_and_truncates(self):
        with tempfile.TemporaryDirectory() as tmp:
            settings = {'user_dir': str(Path(tmp) / 'nested' / 'user')}
            with launcher.open_run_log(settings) as log:
                log.write('first\n')
            path = launcher.run_log_path(settings)
            self.assertEqual(path.read_text(encoding='utf-8'), 'first\n')
            with launcher.open_run_log(settings) as log:
                log.write('second\n')
            self.assertEqual(path.read_text(encoding='utf-8'), 'second\n')

    def test_open_run_log_failure_returns_a_null_log(self):
        # A read-only or invalid folder must not stop the game from starting.
        settings = {'user_dir': str(Path(tempfile.gettempdir()) / 'nul:\x00invalid')}
        with launcher.open_run_log(settings) as log:
            log.write('ignored\n')  # no exception

    def test_read_output_writes_every_line_then_closes_and_reports_the_exit_code(self):
        reader = _launcher_class().read_output
        with tempfile.TemporaryDirectory() as tmp:
            settings = {'user_dir': tmp}
            log = launcher.open_run_log(settings)
            process = types.SimpleNamespace(stdout=io.BytesIO(b'a\nb\n'), wait=lambda: 7)
            owner = types.SimpleNamespace(output=queue.Queue())
            reader(owner, process, log)
            self.assertTrue(log.closed)
            self.assertEqual(launcher.run_log_path(settings).read_text(encoding='utf-8'), 'a\nb\n')
            self.assertEqual([owner.output.get_nowait() for _ in range(3)], ['a\n', 'b\n', (7,)])

    def test_read_output_keeps_draining_when_the_log_fails(self):
        # A write error (disk full, folder gone) must not kill the reader thread: the game
        # would block on a full pipe and the PLAY button would stay at RUNNING.
        class BrokenLog:
            closed = False

            def write(self, text):
                raise OSError('disk full')

            def close(self):
                self.closed = True

        log = BrokenLog()
        process = types.SimpleNamespace(stdout=io.BytesIO(b'a\nb\n'), wait=lambda: 0)
        owner = types.SimpleNamespace(output=queue.Queue())
        _launcher_class().read_output(owner, process, log)
        self.assertTrue(log.closed)
        self.assertEqual([owner.output.get_nowait() for _ in range(3)], ['a\n', 'b\n', (0,)])

    def test_play_log_is_kept_when_the_launcher_hides_on_play(self):
        with tempfile.TemporaryDirectory() as tmp:
            for close_on_play in (True, False):
                with self.subTest(close_on_play=close_on_play):
                    with launcher.open_play_log({'user_dir': tmp, 'close_on_play': close_on_play}) as log:
                        log.write('x\n')
                    self.assertEqual(launcher.run_log_path({'user_dir': tmp}).read_text(encoding='utf-8'), 'x\n')


def _launcher_class():
    """The launcher window class, found by its method so the test does not build a Tk window."""
    return next(c for c in vars(launcher).values() if isinstance(c, type) and hasattr(c, 'read_output'))


if __name__ == '__main__':
    unittest.main()
