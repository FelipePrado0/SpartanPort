"""The menu entry that gives the launcher and the game window their icon on Linux."""
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import gow3_launcher_win as launcher  # noqa: E402


class DesktopIntegrationTest(unittest.TestCase):
    def setUp(self):
        self.home = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.home, True)
        self.icon = self.home / 'source.png'
        self.icon.write_bytes(b'png one')
        self.data = self.home / 'data'
        self.entry = self.home / '.local' / 'share' / 'applications' / 'gow3.desktop'

    def integrate(self, enabled=True, appimage='/games/gow3.AppImage'):
        return launcher.integrate_desktop(enabled, appimage, self.icon, self.data, home=self.home)

    def test_entry_names_the_appimage_the_icon_and_the_window_class(self):
        self.assertEqual(self.integrate(), self.entry)
        text = self.entry.read_text()
        self.assertIn('Exec="/games/gow3.AppImage"', text)
        self.assertIn(f'Icon={self.data / "gow3.png"}', text)
        self.assertIn('StartupWMClass=gow3', text)
        self.assertEqual((self.data / 'gow3.png').read_bytes(), b'png one')

    def test_a_moved_appimage_or_a_new_icon_updates_the_entry(self):
        self.integrate()
        self.icon.write_bytes(b'png two')
        self.integrate(appimage='/elsewhere/gow3.AppImage')
        self.assertIn('Exec="/elsewhere/gow3.AppImage"', self.entry.read_text())
        self.assertEqual((self.data / 'gow3.png').read_bytes(), b'png two')

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
        path.write_text('[Desktop Entry]\nName=God of War III Remastered\n' + text)
        return path

    def test_an_entry_of_another_tool_for_the_same_file_means_none_of_ours(self):
        for exec_line in ('Exec=env DESKTOPINTEGRATION=1 /games/gow3.AppImage',
                          'Exec="/games/gow3.AppImage" %U', 'TryExec=/games/gow3.AppImage'):
            with self.subTest(exec_line):
                other = self.other_entry(exec_line)
                self.assertIsNone(self.integrate())
                self.assertFalse(self.entry.exists())
                other.unlink()

    def test_our_entry_is_removed_once_another_tool_integrates_the_file(self):
        self.integrate()
        self.assertTrue(self.entry.exists())
        self.other_entry('Exec=env DESKTOPINTEGRATION=1 /games/gow3.AppImage')
        self.assertIsNone(self.integrate())
        self.assertFalse(self.entry.exists())

    def test_an_entry_for_another_file_does_not_count(self):
        self.other_entry('Exec=/games/elsewhere.AppImage')
        self.assertEqual(self.integrate(), self.entry)

    def test_the_variable_gear_lever_sets_skips_it_too(self):
        self.assertIsNone(launcher.integrate_desktop(True, '/games/gow3.AppImage', self.icon, self.data,
                                                     home=self.home, environment={'DESKTOPINTEGRATION': '1'}))
        self.assertFalse(self.entry.exists())

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
        self.appimage = '/games/gow3.AppImage'

    def click(self, environment=None):
        return launcher.write_linux_shortcut(f'"{self.appimage}"', self.appimage, self.icon, self.desktop,
                                             home=self.home, environment=environment or {})

    def test_it_makes_the_menu_entry_and_the_desktop_one_again_after_they_are_deleted(self):
        self.desktop.mkdir()
        for _ in range(2):
            created = self.click()
            self.assertEqual(created, [str(self.applications / 'gow3.desktop'), str(self.desktop / 'God of War III.desktop')])
            shutil.rmtree(self.applications)
            (self.desktop / 'God of War III.desktop').unlink()
        self.assertTrue((self.home / '.local' / 'share' / 'icons' / 'gow3.png').is_file())

    def test_the_desktop_folder_is_not_made_when_it_does_not_exist(self):
        self.assertEqual(self.click(), [str(self.applications / 'gow3.desktop')])
        self.assertFalse(self.desktop.exists())

    def test_another_tools_entry_keeps_the_menu_free_of_a_duplicate(self):
        self.applications.mkdir(parents=True)
        (self.applications / 'tool.desktop').write_text(f'[Desktop Entry]\nExec=env DESKTOPINTEGRATION=1 {self.appimage}\n')
        self.desktop.mkdir()
        self.assertEqual(self.click(), [str(self.desktop / 'God of War III.desktop')])
        self.assertFalse((self.applications / 'gow3.desktop').exists())

    def test_the_environment_variable_of_gear_lever_does_the_same(self):
        self.assertEqual(self.click({'DESKTOPINTEGRATION': '1'}), [str(self.desktop / 'God of War III.desktop')])
        self.assertTrue(self.desktop.is_dir())
        self.assertFalse((self.applications / 'gow3.desktop').exists())


if __name__ == '__main__':
    unittest.main()
