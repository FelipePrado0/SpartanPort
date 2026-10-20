"""Fixes the community found for a game that stays black or closes at start (#263, #175, #208)."""
import os
import subprocess
import sys
import unittest
from unittest import mock

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import spartanport_launcher_win as launcher  # noqa: E402


class ExitHintTest(unittest.TestCase):
    def test_guest_memory_failure_points_to_the_page_file(self):
        hint = launcher.exit_hint('Runtime: cannot create 8128 MiB of guest memory (error 1450); '
                                  'is the page file large enough?\n')
        self.assertIn('page file', hint)

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
