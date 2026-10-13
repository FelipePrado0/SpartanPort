"""packaging/linux/deps.sh: distribution detection and the diagnosis of a failed build."""
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

from paths import ROOT

SCRIPT = ROOT / 'packaging' / 'linux' / 'deps.sh'


def run(*args, family=None, env=None):
    environment = dict(os.environ, **(env or {}))
    environment.pop('GOW3_SKIP_DEPS', None)
    if family:
        environment['GOW3_FAMILY'] = family
    return subprocess.run(['bash', str(SCRIPT), *args], capture_output=True, text=True, env=environment)


@unittest.skipUnless(os.name == 'posix' and Path('/bin/bash').exists() or Path('/usr/bin/bash').exists(), 'needs bash')
class DepsScriptTests(unittest.TestCase):
    def test_family_decides_the_package_manager(self):
        for family, manager in (('arch', 'pacman'), ('debian', 'apt'), ('fedora', 'dnf'), ('suse', 'zypper')):
            self.assertIn(f'package manager {manager}', run('--list', family=family).stdout)

    def test_unknown_option_is_refused(self):
        self.assertEqual(run('--nonsense').returncode, 2)

    def test_skip_variable_ends_the_check_at_once(self):
        self.assertEqual(run('--build', env={'GOW3_SKIP_DEPS': '1'}).returncode, 0)

    def test_diagnosis_names_the_package_of_each_distribution(self):
        with tempfile.TemporaryDirectory() as folder:
            log = Path(folder) / 'build.log'
            log.write_text('CMake Error: Could not find a package configuration file provided by "Zydis" with any\n'
                           'Package \'sdl3\', required by \'virtual:world\', not found\n'
                           'x.cpp:3:10: fatal error: fmt/core.h: No such file or directory\n'
                           '/usr/bin/ld: cannot find -lavcodec: No such file or directory\n'
                           'build.sh: glslangValidator: command not found\n')
            expected = {'debian': ('libzydis-dev', 'libsdl3-dev', 'libfmt-dev', 'glslang-tools'),
                        'fedora': ('zydis-devel', 'SDL3-devel', 'fmt-devel', 'glslang'),
                        'arch': ('zydis', 'sdl3', 'fmt', 'glslang')}
            for family, packages in expected.items():
                text = run('--diagnose', str(log), family=family).stderr
                for package in packages:
                    self.assertIn(f'install {package}', text, f'{family}: {package}')

    def test_diagnosis_of_an_unknown_error_shows_the_last_error_lines(self):
        with tempfile.TemporaryDirectory() as folder:
            log = Path(folder) / 'build.log'
            log.write_text('foo.cpp:1:1: error: something nobody planned for\n')
            text = run('--diagnose', str(log), family='debian').stderr
            self.assertIn('no known cause', text)
            self.assertIn('something nobody planned for', text)


if __name__ == '__main__':
    unittest.main()
