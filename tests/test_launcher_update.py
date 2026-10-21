"""The launcher updating itself: the AppImage on Linux, with the checksum the release publishes."""
import hashlib
import io
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest import mock

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import spartanport_launcher_win as launcher  # noqa: E402

NEW_APPIMAGE = b'\x7fELF new appimage'


class ReplaceAppImageTest(unittest.TestCase):
    def setUp(self):
        self.folder = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.folder, True)
        self.target = self.folder / 'games' / 'spartanport.AppImage'
        self.target.parent.mkdir()
        self.target.write_bytes(b'old appimage')
        self.target.chmod(0o644)
        self.release = self.folder / 'release'
        self.release.mkdir()
        self.package = self.release / 'spartanport-x86_64.AppImage'
        self.package.write_bytes(NEW_APPIMAGE)

    def publish_checksum(self, text=None):
        digest = hashlib.sha256(NEW_APPIMAGE).hexdigest()
        (self.release / 'spartanport-x86_64.AppImage.sha256').write_text(
            text if text is not None else f'{digest} *spartanport-x86_64.AppImage\n')

    def replace(self, progress=lambda percent: None):
        launcher.replace_appimage(self.package.as_uri(), self.target, progress)

    def leftovers(self):
        return sorted(p.name for p in self.target.parent.iterdir())

    def test_a_matching_download_replaces_the_appimage_and_makes_it_executable(self):
        self.publish_checksum()
        self.replace()
        self.assertEqual(self.target.read_bytes(), NEW_APPIMAGE)
        self.assertTrue(self.target.stat().st_mode & 0o111)
        self.assertEqual(self.leftovers(), ['spartanport.AppImage'])

    def test_a_symlink_to_the_appimage_replaces_the_file_behind_it(self):
        self.publish_checksum()
        link = self.folder / 'link.AppImage'
        link.symlink_to(self.target)
        launcher.replace_appimage(self.package.as_uri(), link)
        self.assertTrue(link.is_symlink())
        self.assertEqual(self.target.read_bytes(), NEW_APPIMAGE)
        self.assertEqual(sorted(p.name for p in self.folder.iterdir() if p.suffix == '.update'), [])

    def test_progress_reaches_one_hundred_percent(self):
        self.publish_checksum()
        seen = []
        self.replace(seen.append)
        self.assertEqual(seen[-1], 100)

    def test_a_download_that_does_not_match_the_checksum_leaves_the_appimage_alone(self):
        self.publish_checksum('0' * 64 + ' *spartanport-x86_64.AppImage\n')
        with self.assertRaisesRegex(OSError, 'checksum'):
            self.replace()
        self.assertEqual(self.target.read_bytes(), b'old appimage')
        self.assertEqual(self.leftovers(), ['spartanport.AppImage'])

    def test_a_release_without_a_checksum_is_not_installed(self):
        with self.assertRaises(OSError):
            self.replace()
        self.assertEqual(self.target.read_bytes(), b'old appimage')
        self.assertEqual(self.leftovers(), ['spartanport.AppImage'])

    def test_a_garbled_checksum_file_is_not_installed(self):
        self.publish_checksum('not a hash at all\n')
        with self.assertRaises(OSError):
            self.replace()
        self.assertEqual(self.target.read_bytes(), b'old appimage')

    def test_a_missing_download_leaves_the_appimage_alone(self):
        self.publish_checksum()
        self.package.unlink()
        with self.assertRaises(OSError):
            self.replace()
        self.assertEqual(self.target.read_bytes(), b'old appimage')
        self.assertEqual(self.leftovers(), ['spartanport.AppImage'])


class RelaunchTest(unittest.TestCase):
    def relaunch(self, environment):
        with mock.patch.object(launcher.subprocess, 'Popen') as popen:
            launcher.relaunch_appimage('/games/spartanport.AppImage', environment)
        return popen.call_args

    def test_the_old_mount_does_not_leak_into_the_new_appimage(self):
        call = self.relaunch({'APPIMAGE': '/games/spartanport.AppImage', 'APPDIR': '/tmp/.mount_spartanportabc', 'ARGV0': 'x',
                              'OWD': '/home', 'LD_LIBRARY_PATH': '/tmp/.mount_spartanportabc/usr/lib:/opt/lib', 'HOME': '/h'})
        environment = call.kwargs['env']
        for name in ('APPIMAGE', 'APPDIR', 'ARGV0', 'OWD'):
            self.assertNotIn(name, environment)
        self.assertEqual(environment['LD_LIBRARY_PATH'], '/opt/lib')
        self.assertEqual(environment['HOME'], '/h')
        self.assertEqual(call.args[0], ['/games/spartanport.AppImage'])
        self.assertTrue(call.kwargs['start_new_session'])

    def test_a_library_path_that_was_only_the_mount_is_dropped(self):
        environment = self.relaunch({'APPDIR': '/tmp/.mount_spartanportabc',
                                     'LD_LIBRARY_PATH': '/tmp/.mount_spartanportabc/usr/lib'}).kwargs['env']
        self.assertNotIn('LD_LIBRARY_PATH', environment)

    def test_a_library_path_is_kept_without_an_appdir(self):
        environment = self.relaunch({'LD_LIBRARY_PATH': '/opt/lib'}).kwargs['env']
        self.assertEqual(environment['LD_LIBRARY_PATH'], '/opt/lib')


class LatestReleaseTest(unittest.TestCase):
    RELEASE = {'tag_name': 'windows-v2.6.0', 'html_url': 'https://example.test/release', 'assets': [
        {'name': 'spartanport-windows.zip', 'browser_download_url': 'https://example.test/spartanport-windows.zip'},
        {'name': 'spartanport-x86_64.AppImage', 'browser_download_url': 'https://example.test/spartanport-x86_64.AppImage'},
        {'name': 'spartanport-x86_64.AppImage.sha256', 'browser_download_url': 'https://example.test/a.sha256'}]}

    def latest(self, windows, release=None):
        body = io.BytesIO(json.dumps(release or self.RELEASE).encode())
        with mock.patch.object(launcher, 'WINDOWS', windows), \
                mock.patch.object(launcher.urllib.request, 'urlopen', return_value=body):
            return launcher.latest_release()

    def test_windows_takes_the_zip(self):
        self.assertEqual(self.latest(True), ('2.6.0', 'https://example.test/spartanport-windows.zip',
                                              'https://example.test/release'))

    def test_linux_takes_the_appimage_not_its_checksum(self):
        self.assertEqual(self.latest(False)[1], 'https://example.test/spartanport-x86_64.AppImage')

    def test_linux_has_no_package_while_the_release_has_no_appimage(self):
        release = {**self.RELEASE, 'assets': self.RELEASE['assets'][:1]}
        version, url, _page = self.latest(False, release)
        self.assertEqual((version, url), ('2.6.0', None))


class CanSelfUpdateTest(unittest.TestCase):
    def test_linux_updates_only_as_an_appimage(self):
        with mock.patch.object(launcher, 'WINDOWS', False):
            with mock.patch.dict(launcher.os.environ, {'APPIMAGE': '/games/spartanport.AppImage'}):
                self.assertTrue(launcher.can_self_update())
            with mock.patch.dict(launcher.os.environ, {}, clear=True):
                self.assertFalse(launcher.can_self_update())

    def test_windows_always_updates(self):
        with mock.patch.object(launcher, 'WINDOWS', True), mock.patch.dict(launcher.os.environ, {}, clear=True):
            self.assertTrue(launcher.can_self_update())


if __name__ == '__main__':
    unittest.main()
