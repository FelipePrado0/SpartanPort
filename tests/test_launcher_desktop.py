"""The menu entry that gives the launcher and the game window their icon on Linux."""
import os
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest import mock

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import spartanport_launcher_win as launcher  # noqa: E402


class DesktopIntegrationTest(unittest.TestCase):
    def setUp(self):
        self.home = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.home, True)
        self.icon = self.home / 'source.png'
        self.icon.write_bytes(b'png one')
        self.data = self.home / 'data'
        self.entry = self.home / '.local' / 'share' / 'applications' / 'spartanport.desktop'

    def integrate(self, enabled=True, appimage='/games/spartanport.AppImage', environment=None):
        # The environment is ours, not the shell's: DESKTOPINTEGRATION in the real one must not decide a test.
        return launcher.integrate_desktop(enabled, appimage, self.icon, self.data, home=self.home,
                                          environment={} if environment is None else environment)

    def test_entry_names_the_appimage_the_icon_and_the_window_class(self):
        self.assertEqual(self.integrate(), self.entry)
        text = self.entry.read_text()
        self.assertIn('Exec="/games/spartanport.AppImage"', text)
        self.assertIn(f'Icon={self.data / "spartanport.png"}', text)
        self.assertIn('StartupWMClass=spartanport', text)
        self.assertEqual((self.data / 'spartanport.png').read_bytes(), b'png one')

    def test_a_moved_appimage_or_a_new_icon_updates_the_entry(self):
        self.integrate()
        self.icon.write_bytes(b'png two')
        self.integrate(appimage='/elsewhere/spartanport.AppImage')
        self.assertIn('Exec="/elsewhere/spartanport.AppImage"', self.entry.read_text())
        self.assertEqual((self.data / 'spartanport.png').read_bytes(), b'png two')

    def test_turning_it_off_removes_only_our_entry(self):
        self.integrate()
        self.integrate(enabled=False)
        self.assertFalse(self.entry.exists())
        self.entry.write_text('[Desktop Entry]\nName=made by hand\n')
        self.integrate(enabled=False)
        self.assertTrue(self.entry.exists())

    def test_an_entry_made_by_hand_is_left_alone(self):
        self.entry.parent.mkdir(parents=True)
        self.entry.write_text('[Desktop Entry]\nName=made by hand\n')
        self.assertIsNone(self.integrate())
        self.assertEqual(self.entry.read_text(), '[Desktop Entry]\nName=made by hand\n')

    def other_entry(self, text, name='gearlever.desktop'):
        path = self.home / '.local' / 'share' / 'applications' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('[Desktop Entry]\nName=SpartanPort\n' + text)
        return path

    def test_an_entry_of_another_tool_for_the_same_file_means_none_of_ours(self):
        for exec_line in ('Exec=env DESKTOPINTEGRATION=1 /games/spartanport.AppImage',
                          'Exec="/games/spartanport.AppImage" %U', 'TryExec=/games/spartanport.AppImage'):
            with self.subTest(exec_line):
                other = self.other_entry(exec_line)
                self.assertIsNone(self.integrate())
                self.assertFalse(self.entry.exists())
                other.unlink()

    def test_our_entry_is_removed_once_another_tool_integrates_the_file(self):
        self.integrate()
        self.assertTrue(self.entry.exists())
        self.other_entry('Exec=env DESKTOPINTEGRATION=1 /games/spartanport.AppImage')
        self.assertIsNone(self.integrate())
        self.assertFalse(self.entry.exists())

    def test_an_entry_for_another_file_does_not_count(self):
        self.other_entry('Exec=/games/elsewhere.AppImage')
        self.assertEqual(self.integrate(), self.entry)

    def test_the_variable_gear_lever_sets_skips_it_too(self):
        self.assertIsNone(self.integrate(environment={'DESKTOPINTEGRATION': '1'}))
        self.assertFalse(self.entry.exists())

    def test_an_empty_variable_does_not_count(self):
        self.assertEqual(self.integrate(environment={'DESKTOPINTEGRATION': ''}), self.entry)

    def test_the_shells_variable_does_not_leak_into_these_tests(self):
        with mock.patch.dict(os.environ, {'DESKTOPINTEGRATION': '1'}):
            self.assertEqual(self.integrate(), self.entry)

    def test_without_the_environment_argument_the_real_one_is_read(self):
        with mock.patch.dict(os.environ, {'DESKTOPINTEGRATION': '1'}):
            self.assertIsNone(launcher.integrate_desktop(True, '/games/spartanport.AppImage', self.icon, self.data,
                                                         home=self.home))
        self.assertFalse(self.entry.exists())

    def other_bytes(self, content, name='broken.desktop'):
        path = self.home / '.local' / 'share' / 'applications' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        return path

    def test_a_nul_byte_in_another_entry_is_ignored(self):
        for key in ('Exec', 'TryExec'):
            with self.subTest(key):
                broken = self.other_bytes(f'[Desktop Entry]\n{key}=/games/\x00spartanport.AppImage\n'.encode())
                self.assertEqual(self.integrate(), self.entry)
                self.assertEqual(launcher.other_desktop_entries(self.entry.parent, '/games/spartanport.AppImage'), [])
                self.entry.unlink()
                broken.unlink()

    def test_a_broken_entry_does_not_hide_a_good_one(self):
        self.other_bytes(b'[Desktop Entry]\nExec=/a/\x00b\n', 'a-broken.desktop')
        good = self.other_entry('Exec=env DESKTOPINTEGRATION=1 /games/spartanport.AppImage', 'z-good.desktop')
        self.assertEqual(launcher.other_desktop_entries(self.entry.parent, '/games/spartanport.AppImage'), [good])
        self.assertIsNone(self.integrate())

    def test_entries_that_are_not_text_or_not_files_are_ignored(self):
        self.other_bytes(b'\xff\xfe\x00\x80Exec=\xff', 'binary.desktop')
        self.other_bytes(b'', 'empty.desktop')
        (self.entry.parent / 'folder.desktop').mkdir()
        self.assertEqual(self.integrate(), self.entry)

    def test_a_dangling_symlink_entry_is_ignored(self):
        self.entry.parent.mkdir(parents=True)
        try:
            (self.entry.parent / 'dangling.desktop').symlink_to(self.home / 'nowhere')
        except OSError as error:
            if getattr(error, 'winerror', None) == 1314:
                self.skipTest('Windows symlink privilege unavailable')
            raise
        self.assertEqual(self.integrate(), self.entry)

    def test_lines_that_do_not_parse_are_ignored(self):
        for line in ('Exec="/games/spartanport.AppImage', 'Exec=', 'Exec=env', 'Exec=env A=1 B=2', 'Exec', 'TryExec=   ',
                     "Exec='unbalanced", 'Exec=%U'):
            with self.subTest(line):
                self.other_entry(line, 'odd.desktop')
                self.assertEqual(launcher.other_desktop_entries(self.entry.parent, '/games/spartanport.AppImage'), [])
        self.assertEqual(self.integrate(), self.entry)

    def test_a_path_with_spaces_counts(self):
        real = self.home / 'My Games' / 'spartanport.AppImage'
        real.parent.mkdir()
        real.write_bytes(b'x')
        self.other_entry(f'Exec=env DESKTOPINTEGRATION=1 "{real}" %U', 'spaces.desktop')
        self.assertIsNone(self.integrate(appimage=str(real)))

    def test_a_symlink_to_the_file_counts(self):
        real = self.home / 'My Games' / 'spartanport.AppImage'
        real.parent.mkdir()
        real.write_bytes(b'x')
        link = self.home / 'link.AppImage'
        try:
            link.symlink_to(real)
        except OSError as error:
            if getattr(error, 'winerror', None) == 1314:
                self.skipTest('Windows symlink privilege unavailable')
            raise
        self.other_entry(f'Exec=env DESKTOPINTEGRATION=1 "{real}" %U', 'spaces.desktop')
        self.assertIsNone(self.integrate(appimage=str(real)))
        self.assertIsNone(self.integrate(appimage=str(link)))

    def test_our_own_entry_never_counts_as_another_tool(self):
        self.integrate()
        self.assertEqual(launcher.other_desktop_entries(self.entry.parent, '/games/spartanport.AppImage'), [])
        self.assertEqual(self.integrate(), self.entry)

    def test_a_missing_or_unreadable_applications_folder_means_no_other_tool(self):
        self.assertEqual(launcher.other_desktop_entries(self.home / 'nowhere', '/games/spartanport.AppImage'), [])
        with mock.patch.object(Path, 'glob', side_effect=PermissionError):
            self.assertEqual(launcher.other_desktop_entries(self.entry.parent, '/games/spartanport.AppImage'), [])

    def test_a_broken_file_never_stops_the_start(self):
        self.other_bytes(b'[Desktop Entry]\nExec=/a/\x00b\n')
        with mock.patch.object(launcher, 'other_desktop_entries', side_effect=ValueError('embedded null byte')):
            self.assertIsNone(self.integrate())

    def test_our_entry_survives_a_broken_neighbour_across_starts(self):
        self.other_bytes(b'[Desktop Entry]\nExec=/a/\x00b\n')
        for _ in range(3):
            self.assertEqual(self.integrate(), self.entry)
        self.assertIn(launcher.DESKTOP_MARKER, self.entry.read_text())

    def test_without_an_appimage_nothing_is_written(self):
        self.assertIsNone(self.integrate(appimage=None))
        self.assertFalse(self.entry.exists())


class LinuxShortcutTest(unittest.TestCase):
    def setUp(self):
        self.home = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.home, True)
        self.icon = self.home / 'source.png'
        self.icon.write_bytes(b'png')
        self.desktop = self.home / 'Desktop'
        self.applications = self.home / '.local' / 'share' / 'applications'
        self.appimage = '/games/spartanport.AppImage'

    def click(self, environment=None):
        return launcher.write_linux_shortcut(f'"{self.appimage}"', self.appimage, self.icon, self.desktop,
                                             home=self.home, environment=environment or {})

    def test_it_makes_the_menu_entry_and_the_desktop_one_again_after_they_are_deleted(self):
        self.desktop.mkdir()
        for _ in range(2):
            created = self.click()
            self.assertEqual(created, [str(self.applications / 'spartanport.desktop'), str(self.desktop / 'SpartanPort.desktop')])
            shutil.rmtree(self.applications)
            (self.desktop / 'SpartanPort.desktop').unlink()
        self.assertTrue((self.home / '.local' / 'share' / 'icons' / 'spartanport.png').is_file())

    def test_the_desktop_folder_is_not_made_when_it_does_not_exist(self):
        self.assertEqual(self.click(), [str(self.applications / 'spartanport.desktop')])
        self.assertFalse(self.desktop.exists())

    def test_another_tools_entry_keeps_the_menu_free_of_a_duplicate(self):
        self.applications.mkdir(parents=True)
        (self.applications / 'tool.desktop').write_text(f'[Desktop Entry]\nExec=env DESKTOPINTEGRATION=1 {self.appimage}\n')
        self.desktop.mkdir()
        self.assertEqual(self.click(), [str(self.desktop / 'SpartanPort.desktop')])
        self.assertFalse((self.applications / 'spartanport.desktop').exists())

    def test_a_broken_entry_of_another_program_does_not_stop_the_button(self):
        self.applications.mkdir(parents=True)
        (self.applications / 'broken.desktop').write_bytes(b'[Desktop Entry]\nExec=/a/\x00b\n')
        self.desktop.mkdir()
        self.assertEqual(self.click(), [str(self.applications / 'spartanport.desktop'), str(self.desktop / 'SpartanPort.desktop')])

    def test_the_files_run_the_appimage_with_its_icon(self):
        self.desktop.mkdir()
        for path in self.click():
            text = Path(path).read_text()
            self.assertIn(f'Exec="{self.appimage}"', text)
            self.assertIn(f'Icon={self.home / ".local" / "share" / "icons" / "spartanport.png"}', text)
            self.assertIn('StartupWMClass=spartanport', text)
            self.assertNotIn(launcher.DESKTOP_MARKER, text)

    @unittest.skipUnless(sys.platform.startswith('linux'), 'Linux executable permissions')
    def test_the_files_are_executable(self):
        self.desktop.mkdir()
        for path in self.click():
            self.assertEqual(Path(path).stat().st_mode & 0o777, 0o755)

    def test_a_missing_icon_falls_back_to_the_theme_name(self):
        self.icon.unlink()
        created = self.click()
        self.assertIn('Icon=spartanport\n', Path(created[0]).read_text())

    def test_clicking_again_rewrites_the_same_files(self):
        self.desktop.mkdir()
        first = self.click()
        self.assertEqual(self.click(), first)

    def test_the_shells_variable_is_not_read_when_the_environment_is_given(self):
        with mock.patch.dict(os.environ, {'DESKTOPINTEGRATION': '1'}):
            self.assertEqual(self.click(), [str(self.applications / 'spartanport.desktop')])

    def test_the_environment_variable_of_gear_lever_does_the_same(self):
        self.assertEqual(self.click({'DESKTOPINTEGRATION': '1'}), [str(self.desktop / 'SpartanPort.desktop')])
        self.assertTrue(self.desktop.is_dir())
        self.assertFalse((self.applications / 'spartanport.desktop').exists())


if __name__ == '__main__':
    unittest.main()
