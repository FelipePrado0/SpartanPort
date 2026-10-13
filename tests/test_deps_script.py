"""packaging/linux/deps.sh: distribution detection, the requirement checks, the install and the diagnosis of a failed build."""
import os
import shutil
import stat
import subprocess
import tempfile
import unittest
from pathlib import Path

from paths import ROOT

SCRIPT = ROOT / 'packaging' / 'linux' / 'deps.sh'
BASH = shutil.which('bash')
# What the script itself calls; everything else is absent from the PATH of the tests, so the checks do not
# depend on what the machine running them has installed.
TOOLS = ('bash', 'awk', 'sed', 'grep', 'tr', 'sort', 'cut', 'tail', 'cat', 'ls', 'env', 'uname', 'id', 'dirname', 'basename')

# No pacman package database: no -Si lookups, so the calls the stubs log are only the installs.
NO_SYNC = {'GOW3_PACMAN_SYNC': '/nonexistent'}


def run(*args, family=None, env=None, bin_dir=None):
    environment = dict(os.environ)
    for name in ('GOW3_SKIP_DEPS', 'GOW3_YES', 'IN_NIX_SHELL', 'GOW3_IN_NIX_SHELL', 'GOW3_DEPS_SECOND_PASS',
                 'CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'CMAKE_PREFIX_PATH'):
        environment.pop(name, None)
    environment.update(env or {})
    environment.setdefault('GOW3_SYSTEM_INCLUDE', '/nonexistent')
    if bin_dir:
        environment['PATH'] = str(bin_dir)
    if family:
        environment['GOW3_FAMILY'] = family
    return subprocess.run([BASH, str(SCRIPT), *args], capture_output=True, text=True, env=environment, stdin=subprocess.DEVNULL)


class Sandbox:
    """A PATH holding only the tools the script needs plus the stubs a test adds."""

    def __init__(self, test):
        self.root = Path(tempfile.mkdtemp())
        test.addCleanup(shutil.rmtree, self.root, True)
        self.bin = self.root / 'bin'
        self.bin.mkdir()
        self.log = self.root / 'calls.log'
        self.log.touch()
        for tool in TOOLS:
            real = shutil.which(tool)
            if real:
                (self.bin / tool).symlink_to(real)

    def with_pkg_config(self, found=False):
        """pkg-config present; every module it is asked about is missing (or found)."""
        self.stub('pkg-config', 'exit 0' if found else 'exit 1')

    def stub(self, name, body='exit 0'):
        path = self.bin / name
        path.write_text(f'#!{BASH}\n{body}\n')
        path.chmod(path.stat().st_mode | stat.S_IEXEC)

    def logging_stub(self, name, status=0):
        self.stub(name, f'echo "{name} $*" >> "{self.log}"\nexit {status}')

    def calls(self):
        return self.log.read_text()


@unittest.skipUnless(BASH, 'needs bash')
class DepsScriptTests(unittest.TestCase):
    def test_family_decides_the_package_manager(self):
        for family, manager in (('arch', 'pacman'), ('debian', 'apt'), ('fedora', 'dnf'), ('suse', 'zypper')):
            self.assertIn(f'package manager {manager}', run('--list', family=family).stdout)

    def test_unknown_option_is_refused(self):
        self.assertEqual(run('--nonsense').returncode, 2)

    def test_skip_variable_ends_the_check_at_once(self):
        self.assertEqual(run('--build', env={'GOW3_SKIP_DEPS': '1'}).returncode, 0)

    def test_a_variable_set_to_zero_is_off(self):
        box = Sandbox(self)
        result = run('--build', family='debian', env={'GOW3_SKIP_DEPS': '0'}, bin_dir=box.bin)
        self.assertEqual(result.returncode, 1)
        self.assertIn('Missing on', result.stdout)

    def test_a_nix_shell_skips_the_check(self):
        box = Sandbox(self)
        result = run('--build', family='debian', env={'IN_NIX_SHELL': 'impure'}, bin_dir=box.bin)
        self.assertEqual(result.returncode, 0)
        self.assertIn('shell.nix', result.stdout)

    def test_diagnose_without_a_log_is_refused(self):
        self.assertEqual(run('--diagnose').returncode, 2)

    def test_missing_items_are_listed_once_with_the_install_command(self):
        box = Sandbox(self)
        box.with_pkg_config()
        result = run('--build', family='debian', bin_dir=box.bin)
        self.assertEqual(result.returncode, 1)
        self.assertIn('  - cmake', result.stdout)
        self.assertIn('apt-get install -y', result.stdout)
        install_line = next(line for line in result.stdout.splitlines() if line.startswith('Install with'))
        packages = install_line.split()
        self.assertEqual(len(packages), len(set(packages)))
        self.assertIn('libsdl3-dev', packages)
        self.assertIn('--install', result.stdout)
        self.assertIn('GOW3_SKIP_DEPS=1', result.stdout)

    def test_without_pkg_config_the_modules_wait_for_it(self):
        box = Sandbox(self)
        result = run('--build', family='debian', bin_dir=box.bin)
        self.assertIn('Not checked until pkg-config is installed', result.stdout)
        self.assertNotIn('  - SDL3', result.stdout)
        self.assertIn('  - pkg-config', result.stdout)

    def test_what_cmake_downloads_does_not_fail_the_check(self):
        box = Sandbox(self)
        for tool in ('gcc', 'g++', 'cmake', 'ninja', 'git', 'python3', 'objdump', 'glslang', 'curl', 'file', 'ldd'):
            box.stub(tool)
        box.with_pkg_config(found=True)
        (box.root / 'include' / 'boost').mkdir(parents=True)
        (box.root / 'include' / 'boost' / 'version.hpp').touch()
        (box.root / 'include' / 'Zydis').mkdir()
        (box.root / 'include' / 'Zydis' / 'Zydis.h').touch()
        (box.root / 'include' / 'vulkan').mkdir()
        (box.root / 'include' / 'vulkan' / 'vulkan_core.h').touch()
        result = run('--build', family='debian', env={'GOW3_SYSTEM_INCLUDE': str(box.root / 'include')}, bin_dir=box.bin)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('Downloaded by CMake when absent', result.stdout)
        self.assertIn('robin-map', result.stdout)

    def test_a_distribution_without_a_package_says_so(self):
        box = Sandbox(self)
        box.with_pkg_config()
        result = run('--build', family='suse', bin_dir=box.bin)
        self.assertIn('No package for this distribution: Zydis', result.stdout)

    def test_arch_does_not_list_the_aur_only_allocator(self):
        box = Sandbox(self)
        box.with_pkg_config()
        install_line = next(line for line in run('--build', family='arch', bin_dir=box.bin).stdout.splitlines()
                            if line.startswith('Install with'))
        self.assertNotIn('vulkan-memory-allocator', install_line)
        self.assertIn('pacman -S --needed', install_line)
        self.assertNotIn('-Syu', install_line)

    def test_apt_packages_missing_from_the_repositories_are_split_off(self):
        box = Sandbox(self)
        box.with_pkg_config()
        lists = box.root / 'lists'
        lists.mkdir()
        (lists / 'deb.debian.org_dists_trixie_main_binary-amd64_Packages.lz4').touch()
        box.stub('apt-cache', 'case "$2" in libzydis-dev) exit 100 ;; esac\nexit 0')
        result = run('--build', family='debian', env={'GOW3_APT_LISTS': str(lists)}, bin_dir=box.bin)
        self.assertIn("Not in this release's repositories: libzydis-dev", result.stdout)
        install_line = next(line for line in result.stdout.splitlines() if line.startswith('Install with'))
        self.assertNotIn('libzydis-dev', install_line)
        self.assertIn('libsdl3-dev', install_line)

    def test_install_without_confirmation_and_without_a_terminal_is_refused(self):
        box = Sandbox(self)
        box.logging_stub('sudo')
        result = run('--build', '--install', family='debian', bin_dir=box.bin)
        self.assertEqual(result.returncode, 1)
        self.assertIn('pass --yes', result.stderr)
        self.assertEqual(box.calls(), '')

    def test_install_updates_the_index_then_installs_what_the_repositories_have(self):
        box = Sandbox(self)
        box.with_pkg_config()
        box.logging_stub('sudo')
        box.logging_stub('apt-get')
        box.stub('apt-cache', 'case "$2" in libzydis-dev) exit 100 ;; esac\nexit 0')
        lists = box.root / 'lists'
        lists.mkdir()
        (lists / 'x_Packages').touch()
        result = run('--build', '--install', '--yes', family='debian', env={'GOW3_APT_LISTS': str(lists)}, bin_dir=box.bin)
        calls = box.calls().splitlines()
        self.assertTrue(any('apt-get update' in call for call in calls), calls)
        install = next(call for call in calls if 'install -y' in call)
        self.assertIn('libsdl3-dev', install)
        self.assertNotIn('libzydis-dev', install)
        self.assertIn("Not in this release's repositories: libzydis-dev", result.stdout)

    def test_a_failing_apt_update_does_not_stop_the_install(self):
        box = Sandbox(self)
        box.stub('sudo', f'echo "sudo $*" >> "{box.log}"\ncase "$*" in *update*) exit 100 ;; esac\nexit 0')
        box.stub('apt-get', 'exit 0')
        result = run('--build', '--install', '--yes', family='debian', bin_dir=box.bin)
        self.assertIn('apt-get update failed', result.stderr)
        self.assertIn('install -y', box.calls())

    def test_pacman_installs_without_a_system_update_and_retries_with_one_only_on_failure(self):
        box = Sandbox(self)
        box.with_pkg_config()
        box.stub('pacman', f'echo "pacman $*" >> "{box.log}"\nexit 0')
        box.stub('sudo', 'exec "$@"')
        run('--build', '--install', '--yes', family='arch', env=NO_SYNC, bin_dir=box.bin)
        calls = box.calls()
        self.assertIn('pacman -S --needed', calls)
        self.assertNotIn('-Syu', calls)

        retry = Sandbox(self)
        retry.with_pkg_config()
        retry.stub('pacman', f'echo "pacman $*" >> "{retry.log}"\ncase "$*" in *-Syu*) exit 0 ;; esac\nexit 1')
        retry.stub('sudo', 'exec "$@"')
        result = run('--build', '--install', '--yes', family='arch', env=NO_SYNC, bin_dir=retry.bin)
        lines = retry.calls().splitlines()
        self.assertIn('pacman -S --needed', lines[0])
        self.assertIn('pacman -Syu --needed', lines[1])
        self.assertIn('Retrying with pacman -Syu', result.stderr)

    def test_pacman_retry_asks_before_updating_the_system_without_yes(self):
        box = Sandbox(self)
        box.with_pkg_config()
        box.stub('pacman', f'echo "pacman $*" >> "{box.log}"\nexit 1')
        box.stub('sudo', 'exec "$@"')
        # A terminal is needed for the question; without --yes and without one the install is refused earlier.
        result = run('--build', '--install', family='arch', env=NO_SYNC, bin_dir=box.bin)
        self.assertEqual(result.returncode, 1)
        self.assertNotIn('-Syu', box.calls())

    def test_diagnosis_names_the_package_of_each_distribution(self):
        with tempfile.TemporaryDirectory() as folder:
            log = Path(folder) / 'build.log'
            log.write_text('CMake Error: Could not find a package configuration file provided by "Zydis" with any\n'
                           'Package \'sdl3\', required by \'virtual:world\', not found\n'
                           'x.cpp:3:10: fatal error: fmt/core.h: No such file or directory\n'
                           '/usr/bin/ld: cannot find -lavcodec: No such file or directory\n'
                           '/usr/bin/ld: cannot find -lfmt: No such file or directory\n'
                           'build.sh: glslangValidator: command not found\n')
            expected = {'debian': ('libzydis-dev', 'libsdl3-dev', 'libfmt-dev', 'glslang-tools'),
                        'fedora': ('zydis-devel', 'SDL3-devel', 'fmt-devel', 'glslang'),
                        'arch': ('zydis', 'sdl3', 'fmt', 'glslang')}
            for family, packages in expected.items():
                text = run('--diagnose', str(log), family=family).stderr
                for package in packages:
                    self.assertIn(f'install {package}', text, f'{family}: {package}')
                self.assertNotIn("library 'libfmt': find the package", text, family)

    def test_diagnosis_reads_a_log_with_the_color_codes_of_the_compiler(self):
        with tempfile.TemporaryDirectory() as folder:
            log = Path(folder) / 'build.log'
            log.write_text('\x1b[01m\x1b[Kx.cpp:3:10:\x1b[m\x1b[K \x1b[01;31m\x1b[Kfatal error: \x1b[m\x1b[Kfmt/core.h: No such file or directory\n')
            self.assertIn('install libfmt-dev', run('--diagnose', str(log), family='debian').stderr)

    def test_diagnosis_reads_cmake_could_not_find_messages(self):
        with tempfile.TemporaryDirectory() as folder:
            log = Path(folder) / 'configure.log'
            log.write_text('-- Could NOT find Vulkan (missing: Vulkan_INCLUDE_DIR)\n'
                           'CMake Error at CMakeLists.txt:104 (find_program):\n'
                           '  Could not find GLSLANG_COMPILER using the following names: glslang, glslangValidator\n')
            text = run('--diagnose', str(log), family='debian').stderr
            self.assertIn('libvulkan-dev', text)
            self.assertIn('glslang-tools', text)

    def test_diagnosis_of_a_distribution_it_does_not_know_does_not_stop_early(self):
        with tempfile.TemporaryDirectory() as folder:
            log = Path(folder) / 'build.log'
            log.write_text('x.cpp: fatal error: X11/Xlib.h: No such file or directory\n')
            result = run('--diagnose', str(log), family='unknown')
            self.assertEqual(result.returncode, 0)
            self.assertIn('Logs:', result.stderr)
            self.assertIn('your package manager', result.stderr)

    def test_diagnosis_knows_the_full_disk_the_killed_compiler_and_a_failed_download(self):
        with tempfile.TemporaryDirectory() as folder:
            log = Path(folder) / 'build.log'
            log.write_text('error: No space left on device\n'
                           'c++: fatal error: Killed signal terminated program cc1plus\n'
                           'Could not resolve host: github.com\n')
            text = run('--diagnose', str(log), family='debian').stderr
            self.assertIn('the disk is full', text)
            self.assertIn('lack of memory', text)
            self.assertIn('check the network', text)

    def test_diagnosis_of_an_unknown_error_shows_the_last_error_lines(self):
        with tempfile.TemporaryDirectory() as folder:
            log = Path(folder) / 'build.log'
            log.write_text('foo.cpp:1:1: error: something nobody planned for\n')
            text = run('--diagnose', str(log), family='debian').stderr
            self.assertIn('no known cause', text)
            self.assertIn('something nobody planned for', text)

    def test_diagnosis_of_a_log_without_error_words_still_ends_with_the_log_list(self):
        with tempfile.TemporaryDirectory() as folder:
            log = Path(folder) / 'build.log'
            log.write_text('all quiet\n')
            result = run('--diagnose', str(log), family='debian')
            self.assertEqual(result.returncode, 0)
            self.assertIn('Logs:', result.stderr)


if __name__ == '__main__':
    unittest.main()
