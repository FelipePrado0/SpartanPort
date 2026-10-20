"""The launcher on macOS: open instead of xdg-open, no Linux options, the .app desktop shortcut."""
from pathlib import Path
import io
import json
import plistlib
import shlex
import shutil
import sys
import tempfile
import unittest
from unittest import mock

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import spartanport_launcher_win as launcher  # noqa: E402
import run as runner  # noqa: E402


class MacosAppTest(unittest.TestCase):
    def setUp(self):
        self.desktop = Path(tempfile.mkdtemp()) / 'Desktop folder'
        self.addCleanup(shutil.rmtree, self.desktop.parent, True)
        self.command = ['/opt/homebrew/bin/python3', '/Users/me/God of War 3/launcher/spartanport_launcher_win.py']

    def make(self):
        return launcher.write_macos_app(self.command, ROOT / 'launcher' / 'spartanport.png', self.desktop)

    def test_bundle_names_its_script_and_icon(self):
        app = self.make()
        self.assertEqual(app, self.desktop / 'SpartanPort.app')
        info = plistlib.loads((app / 'Contents' / 'Info.plist').read_bytes())
        self.assertEqual(info['CFBundleExecutable'], 'spartanport')
        self.assertEqual(info['CFBundleIconFile'], 'spartanport')
        self.assertEqual(info['CFBundlePackageType'], 'APPL')
        self.assertEqual(info['CFBundleShortVersionString'], launcher.VERSION)

    def test_script_starts_the_launcher_from_the_port_with_quoted_paths(self):
        script = (self.make() / 'Contents' / 'MacOS' / 'spartanport').read_text()
        self.assertTrue(script.startswith('#!/bin/bash\n'))
        self.assertIn('/opt/homebrew/bin', script)
        self.assertIn(f'cd {shlex.quote(str(launcher.PORT_DIR))} ', script)
        self.assertIn("exec /opt/homebrew/bin/python3 '/Users/me/God of War 3/launcher/spartanport_launcher_win.py' \"$@\"",
                      script)

    @unittest.skipIf(sys.platform == 'win32', 'POSIX executable permissions')
    def test_script_is_executable(self):
        script = self.make() / 'Contents' / 'MacOS' / 'spartanport'
        self.assertEqual(script.stat().st_mode & 0o777, 0o755)

    def test_without_sips_there_is_no_icon_file(self):
        with mock.patch.object(launcher.subprocess, 'run', side_effect=OSError('no sips')):
            app = self.make()
        self.assertFalse((app / 'Contents' / 'Resources' / 'spartanport.icns').exists())

    def test_clicking_again_rewrites_the_bundle(self):
        self.assertEqual(self.make(), self.make())


class MacosBehaviourTest(unittest.TestCase):
    def test_runtime_requirements_report_old_macos_and_missing_rosetta(self):
        with mock.patch.object(runner.platform, 'mac_ver', return_value=('25.0', (), '')):
            self.assertIn('macOS 26', runner.macos_runtime_error())
        with mock.patch.object(runner.platform, 'mac_ver', return_value=('26.0', (), '')), \
                mock.patch.object(runner.subprocess, 'run', side_effect=[
                    mock.Mock(stdout='1\n', returncode=0), mock.Mock(returncode=1)]):
            self.assertIn('Rosetta 2', runner.macos_runtime_error())

    def test_mac_updates_select_disk_image_instead_of_windows_zip(self):
        release = {'tag_name': 'windows-v2.6.1', 'html_url': 'https://example.com/release', 'assets': [
            {'name': 'spartanport-windows.zip', 'browser_download_url': 'https://example.com/windows.zip'},
            {'name': 'spartanport-macos.dmg', 'browser_download_url': 'https://example.com/mac.dmg'}]}
        with mock.patch.object(launcher, 'MACOS', True), \
                mock.patch.object(launcher.urllib.request, 'urlopen', return_value=io.BytesIO(json.dumps(release).encode())):
            self.assertEqual(launcher.latest_release(), ('2.6.1', 'https://example.com/mac.dmg', 'https://example.com/release'))

    def test_frozen_app_reads_resources_and_keeps_user_data_outside_bundle(self):
        executable = Path('/Applications/SpartanPort.app/Contents/MacOS/SpartanPort').resolve()
        home = Path.home()
        with mock.patch.object(launcher, 'MACOS', True), mock.patch.object(launcher, 'FROZEN', True), \
                mock.patch.object(launcher.sys, 'executable', str(executable)), \
                mock.patch.dict(launcher.os.environ, {'HOME': str(home), 'USERPROFILE': str(home)}, clear=True):
            self.assertEqual(launcher.port_directory(), executable.parent.parent / 'Resources')
            self.assertEqual(launcher.data_directory(), home / 'Library' / 'Application Support' / 'SpartanPort')

    def test_explicit_data_directory_is_preserved(self):
        with mock.patch.dict(launcher.os.environ, {'SPARTAN_DATA_DIR': '/custom/data'}):
            self.assertEqual(launcher.data_directory(), Path('/custom/data'))

    def test_opener(self):
        with mock.patch.object(launcher, 'MACOS', True):
            self.assertEqual(launcher.opener(), 'open')
        with mock.patch.object(launcher, 'MACOS', False):
            self.assertEqual(launcher.opener(), 'xdg-open')

    def test_linux_switches_are_not_passed_on_macos(self):
        settings = {**launcher.APP_DEFAULTS, 'game_dir': '', 'video_backend': 'x11', 'gamemode': True,
                    'mangohud': True}
        with mock.patch.object(launcher, 'WINDOWS', False), mock.patch.object(launcher, 'MACOS', True), \
                mock.patch.dict(launcher.os.environ, {}, clear=True):
            env = launcher.game_environment(settings)
        for name in ('SDL_VIDEO_DRIVER', 'SDL_VIDEODRIVER', 'SPARTAN_GAMEMODE', 'SPARTAN_MANGOHUD'):
            self.assertNotIn(name, env)
        with mock.patch.object(launcher, 'WINDOWS', False), mock.patch.object(launcher, 'MACOS', False), \
                mock.patch.dict(launcher.os.environ, {}, clear=True):
            env = launcher.game_environment(settings)
        self.assertEqual((env['SDL_VIDEO_DRIVER'], env['SPARTAN_GAMEMODE'], env['SPARTAN_MANGOHUD']), ('x11', '1', '1'))


if __name__ == '__main__':
    unittest.main()
