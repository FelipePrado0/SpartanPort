"""Dependency resolution and redistribution boundaries of the Mac package."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from paths import ROOT

spec = importlib.util.spec_from_file_location('macos_bundle', ROOT / 'packaging/macos/bundle_libraries.py')
bundle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bundle)


class MacosPackageTest(unittest.TestCase):
    def test_relocates_transitive_loader_path_dependencies(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            prefix = root / 'deps'
            runtime = root / 'bundle/bin'
            for name in ('spartanport-probe', 'spartanport-gpu-capabilities',
                         'gpu/libspartanportgpu.dylib', 'vulkan/libvulkan_kosmickrisp.dylib'):
                file = runtime / name
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_bytes(b'binary')
            for name in ('libSDL3.dylib', 'libhelper.dylib'):
                file = prefix / 'lib' / name
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_bytes(name.encode())

            def tool(*command):
                if command[0] == 'lipo':
                    return 'x86_64\n'
                file = Path(command[-1])
                if command[1] == '-L':
                    dependency = (str(prefix / 'lib/libSDL3.dylib') if file.name == 'spartanport-probe' else
                                  '@loader_path/libhelper.dylib' if file.name == 'libSDL3.dylib' else
                                  '/usr/lib/libSystem.B.dylib')
                    return f'{file}:\n\t{dependency} (compatibility version 1.0.0)\n'
                return f'{file}:\n'

            with mock.patch.object(bundle, 'output', side_effect=tool), mock.patch.object(bundle.subprocess, 'run') as run:
                bundle.relocate(runtime, prefix)
            self.assertEqual((runtime / 'lib/libhelper.dylib').read_bytes(), b'libhelper.dylib')
            self.assertIn(mock.call(['install_name_tool', '-change', '@loader_path/libhelper.dylib',
                                    '@rpath/libhelper.dylib', str(runtime / 'lib/libSDL3.dylib')], check=True), run.call_args_list)

    def test_arm_only_library_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(bundle, 'output', return_value='arm64\n'):
            with self.assertRaisesRegex(ValueError, 'not x86-64'):
                bundle.relocate(Path(directory), Path(directory))

    def test_resolves_rpath_and_absolute_dependencies(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory)
            library = prefix / 'lib/libSDL3.dylib'
            library.parent.mkdir()
            library.write_bytes(b'library')
            self.assertEqual(bundle.resolve_library('@rpath/libSDL3.dylib', prefix / 'probe', prefix), library.resolve())
            self.assertEqual(bundle.resolve_library(str(library), prefix / 'probe', prefix), library.resolve())
            self.assertIsNone(bundle.resolve_library('/usr/lib/libSystem.B.dylib', prefix / 'probe', prefix))

    def test_missing_or_external_dependency_stops_packaging(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / 'deps'
            prefix.mkdir()
            external = prefix.parent / 'developer-only.dylib'
            external.write_bytes(b'library')
            for name in ('@rpath/missing.dylib', str(external)):
                with self.subTest(name=name), self.assertRaises(ValueError):
                    bundle.resolve_library(name, prefix / 'probe', prefix)

    def test_package_rejects_game_and_user_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'LICENSE').write_text('license')
            bundle.verify_contents(root)
            for name in ('eboot.bin', 'settings.json', 'spartanport.ini', 'last_run.log', 'shader.spv'):
                file = root / name
                file.write_bytes(b'private')
                with self.subTest(name=name), self.assertRaises(ValueError):
                    bundle.verify_contents(root)
                file.unlink()


if __name__ == '__main__':
    unittest.main()
