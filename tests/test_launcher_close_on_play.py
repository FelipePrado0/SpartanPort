"""Keep the game output reader alive after its launcher window disappears (#528)."""
import queue
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest import mock

from test_launcher_controls import launcher


class CloseOnPlayTest(unittest.TestCase):
    def owner(self):
        owner = launcher.Launcher.__new__(launcher.Launcher)
        owner.root, owner.status = mock.Mock(), mock.Mock()
        owner.output, owner.ui_calls = queue.Queue(), queue.Queue()
        owner.append, owner.refresh_status = mock.Mock(), mock.Mock()
        owner.job, owner.exit_hint = mock.Mock(), None
        return owner

    def test_active_game_hides_window_without_destroying_the_reader_owner(self):
        owner = self.owner()
        owner.process = None
        owner.app = {**launcher.APP_DEFAULTS, 'game_dir': 'fixture', 'close_on_play': True}
        for name in ('collect', 'offer_system_files', 'set_log', 'play_button'):
            setattr(owner, name, mock.Mock(return_value=True))
        process = mock.Mock()
        process.poll.return_value = None
        with mock.patch.object(launcher, 'game_info', return_value=('SpartanPort', '01.02')), \
                mock.patch.object(launcher, 'missing_game_files', return_value=[]), \
                mock.patch.object(launcher, 'open_play_log', return_value=launcher._NullLog()), \
                mock.patch.object(launcher.subprocess, 'Popen', return_value=process), \
                mock.patch.object(launcher, 'GameJob'), \
                mock.patch.object(launcher.threading, 'Thread'):
            owner.play()
        delay, callback, *args = owner.root.after.call_args.args
        self.assertEqual(delay, 5000)
        callback(*args)
        owner.root.withdraw.assert_called_once()
        owner.root.destroy.assert_not_called()
        self.assertTrue(owner.hidden_for_game)

    def test_early_failure_and_stale_timer_do_not_hide_the_launcher(self):
        for poll, current in ((3, True), (None, False)):
            with self.subTest(poll=poll, current=current):
                owner = self.owner()
                process = mock.Mock()
                process.poll.return_value = poll
                owner.process = process if current else None
                owner.hide_for_game(process)
                owner.root.withdraw.assert_not_called()
                owner.root.destroy.assert_not_called()

    def test_hidden_launcher_exits_after_game_output_is_finished(self):
        owner = self.owner()
        owner.process, owner.hidden_for_game = mock.Mock(), True
        owner.output.put((0,))
        owner.drain_output()
        owner.job.close.assert_called_once()
        owner.root.destroy.assert_called_once()
        owner.root.after.assert_not_called()

    def test_visible_launcher_stays_open_after_game_exits(self):
        owner = self.owner()
        owner.process = mock.Mock()
        owner.output.put((3,))
        owner.drain_output()
        owner.root.destroy.assert_not_called()
        owner.refresh_status.assert_called_once()
        owner.root.after.assert_called_once()

    def test_real_child_keeps_writing_after_window_is_hidden(self):
        # More than a pipe buffer of output after the window disappears. The
        # reader must remain alive, save all lines, and report the child's exit.
        code = ("import sys; sys.stdout.buffer.write(b'ready\\n'); sys.stdout.buffer.flush(); "
                "sys.stdin.readline(); sys.stdout.buffer.write((b'x'*100+b'\\n')*2000); sys.stdout.buffer.flush()")
        process = subprocess.Popen([sys.executable, '-u', '-c', code], stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        self.addCleanup(process.stdout.close)
        self.addCleanup(process.stdin.close)
        self.addCleanup(lambda: process.wait(timeout=5))
        self.addCleanup(lambda: process.kill() if process.poll() is None else None)
        owner = self.owner()
        owner.process = process
        with tempfile.TemporaryDirectory() as directory:
            settings = {'user_dir': directory, 'close_on_play': True}
            log = launcher.open_play_log(settings)
            reader = threading.Thread(target=owner.read_output, args=(process, log), daemon=True)
            reader.start()
            self.assertEqual(owner.output.get(timeout=5), 'ready\n')
            owner.hide_for_game(process)
            owner.root.destroy.assert_not_called()
            process.stdin.write(b'go\n')
            process.stdin.flush()
            self.assertEqual(process.wait(timeout=10), 0)
            reader.join(timeout=5)
            self.assertFalse(reader.is_alive())
            saved = launcher.run_log_path(settings).read_text(encoding='utf-8')
            self.assertEqual(saved, 'ready\n' + ('x' * 100 + '\n') * 2000)
            while owner.process is not None:
                owner.drain_output()
            owner.root.destroy.assert_called_once()


if __name__ == '__main__':
    unittest.main()
