import contextlib
import io
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

from paths import ROOT
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / 'launcher'))
import run
import spartanport_launcher_win as launcher
from controller_mappings import FILENAME, mapping_line


class LaunchTest(unittest.TestCase):
    def test_direct_run_loads_profile_before_native_detection(self):
        with tempfile.TemporaryDirectory() as folder:
            profile = mapping_line('0300457e790000000600000000000000', 'USB', {'a': 'b2'}, 'Windows')
            (Path(folder) / FILENAME).write_text(profile, encoding='utf-8')
            seen = []

            def cpu_check(*_args, **_kwargs):
                seen.append(os.environ.get('SDL_GAMECONTROLLERCONFIG'))
                return mock.Mock(returncode=20, stdout='CPU check stopped for test\n')

            with mock.patch.dict(os.environ, {'SPARTAN_DATA_DIR': folder}, clear=True), \
                    mock.patch.object(sys, 'argv', ['run.py']), \
                    mock.patch.object(run, 'find_executable', return_value=Path(sys.executable)), \
                    mock.patch('graphics_options.prepare', return_value=({}, False)), \
                    mock.patch('graphics_options.environment', return_value={}), \
                    mock.patch.object(run.subprocess, 'run', side_effect=cpu_check), \
                    mock.patch.object(run.os, 'chdir'), contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    run.main()
            self.assertEqual(error.exception.code, 20)
            self.assertEqual(seen, [profile])

    def test_updater_preserves_profiles(self):
        self.assertIn(FILENAME, launcher.USER_FILES)

    def test_mapper_is_not_opened_over_a_running_game(self):
        app = launcher.Launcher.__new__(launcher.Launcher)
        app.process = mock.Mock()
        app.process.poll.return_value = None
        app.messagebox = mock.Mock()
        with mock.patch.object(launcher.controller_mapper, 'MapperWindow') as window:
            app.open_controller_mapper()
        window.assert_not_called()
        app.messagebox.showinfo.assert_called_once()


if __name__ == '__main__':
    unittest.main()
