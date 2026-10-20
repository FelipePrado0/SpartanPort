import io
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
sys.path.insert(0, str(ROOT))
import spartanport_launcher_win as launcher
import run


class UnicodePreparationTest(unittest.TestCase):
    def test_frozen_script_prints_unicode_paths_with_gbk_streams(self):
        output = io.BytesIO()
        errors = io.BytesIO()
        stdout = io.TextIOWrapper(output, encoding='gbk', newline='\n')
        stderr = io.TextIOWrapper(errors, encoding='gbk', newline='\n')
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / 'prepare.py'
            script.write_text("import sys\nprint('Game\\u00ae/\\u00e7')\n"
                              "print('Log\\u00ae', file=sys.stderr)\n", encoding='utf-8')
            with patch.object(sys, 'stdout', stdout), patch.object(sys, 'stderr', stderr), \
                    patch.object(sys, 'argv', []):
                self.assertEqual(launcher.run_role(['--script', str(script)]), 0)
            stdout.flush()
            stderr.flush()
            self.assertEqual(output.getvalue().decode('utf-8'), 'Game\u00ae/\u00e7\n')
            self.assertEqual(errors.getvalue().decode('utf-8'), 'Log\u00ae\n')
        stdout.close()
        stderr.close()

    def test_captured_preparation_output_uses_utf8_despite_gbk_environment(self):
        with tempfile.TemporaryDirectory() as directory:
            scripts = Path(directory) / 'scripts'
            scripts.mkdir()
            (scripts / 'mods.py').write_text("print('Game\\u00ae/\\u00e7')\n", encoding='utf-8')
            with patch.object(run, 'PORT', Path(directory)), \
                    patch.dict(os.environ, {'PYTHONIOENCODING': 'gbk'}):
                self.assertEqual(run.run_script('mods.py', capture=True), 'Game\u00ae/\u00e7\n')


if __name__ == '__main__':
    unittest.main()
