"""Arch source preparation, without makepkg, submodule downloads or a GPU build."""
from paths import ROOT
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


PKGBUILD = ROOT / 'packaging/linux/PKGBUILD'
OLD_BUFFER = 'char path[64], line[1024];'
FIXED_BUFFER = 'char path[sizeof(d->d_name)+sizeof("/proc/self/task//stat")], line[1024];'
RELATIVE_RPATH = "-Wl,-rpath,'$ORIGIN/gpu'"
BUILD_RPATH = ' -Wl,-rpath,"$PWD/out/gpu"'
SOURCE_FILES = ('build.sh', 'gpu/CMakeLists.txt', 'src/runtime_thread.c')


@unittest.skipUnless(sys.platform.startswith('linux') and shutil.which('bash'),
                     'the Arch PKGBUILD uses Linux and Bash')
class ArchPreparationTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.srcdir = Path(tmp.name)
        self.source = self.srcdir / 'spartanport'
        for name in SOURCE_FILES:
            target = self.source / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, target)
        self.runtime = self.source / 'src/runtime_thread.c'
        self.legacy_runtime = self.runtime.read_text().replace(FIXED_BUFFER, OLD_BUFFER)
        self.assertEqual(self.legacy_runtime.count(OLD_BUFFER), 1)

    def prepare(self):
        # Only submodule setup is stubbed; run the actual PKGBUILD transformations.
        return subprocess.run(
            ['bash', '-c', 'set -e; source "$1"; srcdir=$2; git() { :; }; prepare',
             'test-arch-prepare', str(PKGBUILD), str(self.srcdir)],
            text=True, capture_output=True)

    def assert_prepared(self):
        result = self.prepare()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        build = (self.source / 'build.sh').read_text()
        self.assertIn(RELATIVE_RPATH, build)
        self.assertNotIn(BUILD_RPATH, build)
        self.assertIn('cmake --build out/gpu --target spartanportgpu', build)
        self.assertNotIn('ninja -C out/gpu spartanportgpu', build)
        cmake = (self.source / 'gpu/CMakeLists.txt').read_text()
        for dependency in ('magic_enum', 'miniz'):
            self.assertIn(f'set({dependency}_FOUND FALSE)', cmake)
            self.assertNotIn(f'find_package({dependency} QUIET)', cmake)
        self.assertIn(FIXED_BUFFER, self.runtime.read_text())
        self.assertNotIn(OLD_BUFFER, self.runtime.read_text())

    def test_legacy_buffer_is_fixed(self):
        self.runtime.write_text(self.legacy_runtime)
        self.assert_prepared()

    def test_upstream_buffer_fix_is_preserved(self):
        fixed = self.legacy_runtime.replace(OLD_BUFFER, FIXED_BUFFER)
        self.runtime.write_text(fixed)
        self.assert_prepared()
        self.assertEqual(self.runtime.read_text(), fixed)

    def test_repeated_preparation_preserves_files(self):
        self.assert_prepared()
        before = {name: (self.source / name).read_bytes() for name in SOURCE_FILES}
        self.assert_prepared()
        self.assertEqual(before, {name: (self.source / name).read_bytes() for name in SOURCE_FILES})

    def test_missing_duplicate_and_mixed_buffer_layouts_fail(self):
        for declaration in ('', 'char path[128], line[1024];', OLD_BUFFER * 2,
                            FIXED_BUFFER * 2, OLD_BUFFER + FIXED_BUFFER):
            with self.subTest(declaration=declaration):
                source = self.legacy_runtime.replace(OLD_BUFFER, declaration)
                self.runtime.write_text(source)
                result = self.prepare()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('src/runtime_thread.c: expected one original or one replacement',
                              result.stderr)
                self.assertEqual(self.runtime.read_text(), source)

    def test_upstream_removal_of_build_rpath_is_accepted(self):
        build = self.source / 'build.sh'
        build.write_text(build.read_text().replace(BUILD_RPATH, ''))
        self.assert_prepared()

    def test_missing_relative_rpath_is_rejected(self):
        build = self.source / 'build.sh'
        source = build.read_text().replace(RELATIVE_RPATH, '')
        build.write_text(source)
        result = self.prepare()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('build.sh: expected one original or one replacement', result.stderr)
        self.assertEqual(build.read_text(), source)
