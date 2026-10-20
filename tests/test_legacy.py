"""Moving players from the project's former name (scripts/legacy.py): nothing a player made is lost,
nothing is overwritten and the old files stay where they were."""
import os
import tempfile
import unittest
from pathlib import Path

from paths import ROOT  # noqa: F401  (puts scripts/ on the path)
import legacy

OLD = legacy.LEGACY_NAME


class LegacyTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.data = self.root / 'port'
        self.data.mkdir()

    def migrate(self, data=None, home=None):
        return legacy.migrate(data or self.data, home=home or self.root / 'home', windows=False)

    def test_game_settings_file_is_copied_and_kept(self):
        (self.data / f'{OLD}.ini').write_text('display_mode=borderless\n', encoding='utf-8')
        self.migrate()
        self.assertEqual((self.data / 'spartanport.ini').read_text(encoding='utf-8'), 'display_mode=borderless\n')
        self.assertTrue((self.data / f'{OLD}.ini').is_file())

    def test_game_settings_file_is_not_overwritten(self):
        (self.data / f'{OLD}.ini').write_text('old', encoding='utf-8')
        (self.data / 'spartanport.ini').write_text('new', encoding='utf-8')
        self.migrate()
        self.assertEqual((self.data / 'spartanport.ini').read_text(encoding='utf-8'), 'new')

    def test_linux_data_folder_with_saves_is_copied(self):
        share = self.root / 'share'
        old = share / OLD
        (old / 'user' / 'savedata' / '1').mkdir(parents=True)
        (old / 'user' / 'savedata' / '1' / 'slot0.bin').write_bytes(b'save')
        (old / f'{OLD}.ini').write_text('engine_fps=60\n', encoding='utf-8')
        new = share / 'spartanport'
        new.mkdir(parents=True)  # AppRun makes it before the launcher starts
        self.migrate(data=new)
        self.assertEqual((new / 'user' / 'savedata' / '1' / 'slot0.bin').read_bytes(), b'save')
        self.assertEqual((new / 'spartanport.ini').read_text(encoding='utf-8'), 'engine_fps=60\n')
        self.assertTrue((old / 'user' / 'savedata' / '1' / 'slot0.bin').is_file())

    def test_linux_data_copy_leaves_caches_and_run_folders(self):
        share = self.root / 'share'
        old = share / OLD
        (old / 'user' / 'cache' / 'shaders').mkdir(parents=True)
        (old / 'user' / 'cache' / 'shaders' / 'pipelines.bin').write_bytes(b'cache')
        (old / 'out' / 'mod-game-abc').mkdir(parents=True)
        (old / 'out' / 'mod-game-abc' / 'eboot.bin').write_bytes(b'overlay')
        (old / 'user' / 'savedata').mkdir(parents=True)
        (old / 'user' / 'savedata' / 'slot0.bin').write_bytes(b'save')
        new = share / 'spartanport'
        new.mkdir(parents=True)
        self.migrate(data=new)
        self.assertEqual((new / 'user' / 'savedata' / 'slot0.bin').read_bytes(), b'save')
        self.assertFalse((new / 'user' / 'cache').exists())
        self.assertFalse((new / 'out').exists())
        self.assertTrue((old / 'user' / 'cache' / 'shaders' / 'pipelines.bin').is_file())

    def test_linux_data_copy_keeps_newer_files(self):
        share = self.root / 'share'
        (share / OLD / 'user').mkdir(parents=True)
        (share / OLD / 'user' / 'a.bin').write_bytes(b'old')
        new = share / 'spartanport'
        (new / 'user').mkdir(parents=True)
        (new / 'user' / 'a.bin').write_bytes(b'new')
        self.migrate(data=new)
        self.assertEqual((new / 'user' / 'a.bin').read_bytes(), b'new')

    def test_runs_once(self):
        """A player who deletes a migrated file later (to reset it) does not get it back."""
        (self.data / f'{OLD}.ini').write_text('old', encoding='utf-8')
        self.migrate()
        (self.data / 'spartanport.ini').unlink()
        self.migrate()
        self.assertFalse((self.data / 'spartanport.ini').exists())

    def test_nothing_to_move_is_quiet(self):
        self.assertEqual(self.migrate(), [])
        self.assertFalse((self.data / legacy.MARKER).exists())

    def test_old_menu_entry_of_ours_is_removed_and_a_hand_made_one_kept(self):
        home = self.root / 'home'
        applications = home / '.local' / 'share' / 'applications'
        applications.mkdir(parents=True)
        ours = applications / f'{OLD}.desktop'
        ours.write_text(f'[Desktop Entry]\n{legacy.LEGACY_DESKTOP_MARKER}\n', encoding='utf-8')
        self.migrate(home=home)
        self.assertFalse(ours.exists())
        ours.write_text('[Desktop Entry]\nExec=by hand\n', encoding='utf-8')
        self.data.joinpath(legacy.MARKER).unlink()
        self.migrate(home=home)
        self.assertTrue(ours.exists())

    def test_environment_is_forwarded_without_overriding(self):
        env = {f'{legacy.LEGACY_ENV}GAME_DIR': 'E:/game', f'{legacy.LEGACY_ENV}FPS_LIMIT': '60',
               'SPARTAN_FPS_LIMIT': '120'}
        legacy.forward_environment(env)
        self.assertEqual(env['SPARTAN_GAME_DIR'], 'E:/game')
        self.assertEqual(env['SPARTAN_FPS_LIMIT'], '120')

    def test_custom_data_folder_wins_over_the_appimage_default(self):
        default = str(Path(os.environ.get('XDG_DATA_HOME') or Path.home() / '.local' / 'share') / 'spartanport')
        env = {f'{legacy.LEGACY_ENV}DATA_DIR': '/mnt/games/data', 'SPARTAN_DATA_DIR': default}
        legacy.forward_environment(env)
        self.assertEqual(env['SPARTAN_DATA_DIR'], '/mnt/games/data')

    def test_leftover_binaries_go_only_when_the_new_ones_exist(self):
        port = self.root / 'install'
        (port / 'bin').mkdir(parents=True)
        old_probe = port / 'bin' / f'{OLD}-probe.exe'
        old_probe.write_bytes(b'old')
        legacy.remove_leftovers(port)
        self.assertTrue(old_probe.exists())  # no new probe: keep the old one
        (port / 'bin' / 'spartanport-probe.exe').write_bytes(b'new')
        legacy.remove_leftovers(port)
        self.assertFalse(old_probe.exists())

    def test_old_executable_copies_stay(self):
        """The launcher under its old name keeps desktop and Steam shortcuts working."""
        port = self.root / 'install'
        port.mkdir()
        (port / legacy.LEGACY_EXE).write_bytes(b'launcher')
        (port / 'SpartanPort.exe').write_bytes(b'launcher')
        legacy.remove_leftovers(port)
        self.assertTrue((port / legacy.LEGACY_EXE).exists())


class UpgradeFromFormerNameTest(unittest.TestCase):
    """A player of the last release under the former name starts SpartanPort: the launcher settings
    (then in AppData or ~/.config) land next to the game settings, and saves stay or are copied."""

    def setUp(self):
        import sys
        sys.path.insert(0, str(ROOT / 'launcher'))
        import spartanport_launcher_win as launcher
        self.launcher = launcher
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.old_config = self.root / 'config' / f'{OLD}-launcher'
        self.old_config.mkdir(parents=True)
        (self.old_config / 'settings.json').write_text('{"game_dir": "E:/game"}', encoding='utf-8')
        (self.old_config / 'game_icon.ico').write_bytes(b'icon')

    def start(self, data, windows):
        from unittest import mock
        with mock.patch.multiple(self.launcher, DATA_DIR=data, CONFIG_DIR=data, CONFIG_FILE=data / 'settings.json',
                                 LEGACY_CONFIG_FILE=self.old_config / 'settings.json', WINDOWS=windows,
                                 FROZEN=False):
            self.launcher.move_from_legacy_name()
            return self.launcher.load_app_settings()

    def test_the_old_settings_folder_name_is_the_former_one(self):
        self.assertEqual(self.launcher.LEGACY_CONFIG_FILE.parent.name, f'{OLD}-launcher')

    def test_windows_settings_move_and_saves_stay(self):
        port = self.root / 'port'  # the data folder is the port folder; an update keeps it
        (port / 'user' / 'savedata').mkdir(parents=True)
        (port / 'user' / 'savedata' / 'slot0.bin').write_bytes(b'save')
        (port / f'{OLD}.ini').write_text('engine_fps=60\n', encoding='utf-8')
        settings = self.start(port, windows=True)
        self.assertEqual(settings['game_dir'], 'E:/game')
        self.assertEqual((port / 'game_icon.ico').read_bytes(), b'icon')
        self.assertEqual((port / 'spartanport.ini').read_text(encoding='utf-8'), 'engine_fps=60\n')
        self.assertEqual((port / 'user' / 'savedata' / 'slot0.bin').read_bytes(), b'save')
        self.assertTrue((self.old_config / 'settings.json').is_file())

    def test_linux_appimage_settings_and_saves_are_copied(self):
        share = self.root / 'share'
        old = share / OLD
        (old / 'user' / 'savedata').mkdir(parents=True)
        (old / 'user' / 'savedata' / 'slot0.bin').write_bytes(b'save')
        (old / f'{OLD}.ini').write_text('engine_fps=60\n', encoding='utf-8')
        new = share / 'spartanport'
        new.mkdir()  # AppRun makes it
        settings = self.start(new, windows=False)
        self.assertEqual(settings['game_dir'], 'E:/game')
        self.assertEqual((new / 'game_icon.ico').read_bytes(), b'icon')
        self.assertEqual((new / 'user' / 'savedata' / 'slot0.bin').read_bytes(), b'save')
        self.assertEqual((new / 'spartanport.ini').read_text(encoding='utf-8'), 'engine_fps=60\n')
        self.assertTrue((old / 'user' / 'savedata' / 'slot0.bin').is_file())

    def test_settings_already_in_the_old_data_folder_win(self):
        """A build between the release and the rename kept settings.json in the data folder: it is
        newer than the one in ~/.config."""
        share = self.root / 'share'
        (share / OLD).mkdir(parents=True)
        (share / OLD / 'settings.json').write_text('{"game_dir": "/newer"}', encoding='utf-8')
        new = share / 'spartanport'
        new.mkdir()
        self.assertEqual(self.start(new, windows=False)['game_dir'], '/newer')

    def test_start_moves_before_reading_settings(self):
        import sys
        from unittest import mock
        calls = []
        with mock.patch.object(self.launcher, 'move_from_legacy_name', lambda: calls.append('move')), \
                mock.patch.object(self.launcher, 'load_app_settings',
                                  side_effect=lambda: calls.append('load') or sys.exit(0)), \
                mock.patch.object(sys, 'argv', ['launcher']), self.assertRaises(SystemExit):
            self.launcher.main()
        self.assertEqual(calls, ['move', 'load'])


class LauncherStartTest(unittest.TestCase):
    def test_the_update_installer_does_not_migrate(self):
        """--install-update runs from the unpacked download: migrating there marks the move as done
        without the player's files, and the installed launcher then skips them."""
        import sys
        from unittest import mock
        sys.path.insert(0, str(ROOT / 'launcher'))
        import spartanport_launcher_win as launcher
        with mock.patch.object(launcher, 'move_from_legacy_name') as move, \
                mock.patch.object(launcher, 'install_update', return_value=0), \
                mock.patch.object(launcher, 'WINDOWS', True), \
                mock.patch.object(sys, 'argv', ['launcher', '--install-update', 'C:/game', '1']), \
                self.assertRaises(SystemExit):
            launcher.main()
        move.assert_not_called()


class CrossPlatformShortcutMarkerTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def test_shortcut_migration_skips_when_marker_exists(self):
        (self.root / legacy.SHORTCUT_MARKER).write_text('', encoding='utf-8')
        self.assertFalse(legacy.retarget_desktop_shortcut(self.root))

    def test_shortcut_migration_returns_false_on_non_windows(self):
        if os.name != 'nt':
            self.assertFalse(legacy.retarget_desktop_shortcut(self.root))


@unittest.skipUnless(os.name == 'nt', 'Windows shortcuts require Windows and PowerShell')
class WindowsDesktopShortcutTest(unittest.TestCase):
    def setUp(self):
        import subprocess
        self.subprocess = subprocess
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.port = self.root / 'port'
        self.port.mkdir()
        self.desktop = self.root / 'desktop'
        self.desktop.mkdir()
        (self.port / 'SpartanPort.exe').write_bytes(b'new launcher')
        (self.port / legacy.LEGACY_EXE).write_bytes(b'old launcher copy')
        (self.port / legacy.LEGACY_PLAY_EXE).write_bytes(b'play copy')

    def create_shortcut(self, path, target, arguments='', working_dir=''):
        script = (
            '$w = New-Object -ComObject WScript.Shell;'
            f'$s = $w.CreateShortcut("{path}");'
            f'$s.TargetPath = "{target}";'
            f'$s.Arguments = "{arguments}";'
            + (f'$s.WorkingDirectory = "{working_dir}";' if working_dir else '') +
            '$s.Save()'
        )
        self.subprocess.run(['powershell', '-NoProfile', '-NonInteractive', '-Command', script], check=True)

    def test_user_created_shortcut_with_custom_name(self):
        custom = self.desktop / 'God of War 3.lnk'
        self.create_shortcut(custom, self.port / legacy.LEGACY_PLAY_EXE)
        self.assertFalse(legacy.retarget_desktop_shortcut(self.port, desktop=self.desktop))
        self.assertTrue(custom.is_file())
        self.assertFalse((self.desktop / 'SpartanPort.lnk').exists())

    def test_shortcut_manually_renamed_to_legacy_title(self):
        old = self.desktop / f'{legacy.LEGACY_TITLE}.lnk'
        self.create_shortcut(old, self.port / 'SpartanPort.exe')
        self.assertFalse(legacy.retarget_desktop_shortcut(self.port, desktop=self.desktop))
        self.assertTrue(old.is_file())
        self.assertFalse((self.desktop / 'SpartanPort.lnk').exists())

    def test_user_shortcut_targeting_play_legacy_executable(self):
        # Issue #824 reproduction: user creates shortcut to 'Play God of War III.exe' named 'God of War III.lnk'
        old = self.desktop / f'{legacy.LEGACY_TITLE}.lnk'
        self.create_shortcut(old, self.port / legacy.LEGACY_PLAY_EXE)
        self.assertFalse(legacy.retarget_desktop_shortcut(self.port, desktop=self.desktop))
        self.assertTrue(old.is_file())
        self.assertFalse((self.desktop / 'SpartanPort.lnk').exists())

    def test_shortcut_with_custom_arguments_is_preserved(self):
        old = self.desktop / f'{legacy.LEGACY_TITLE}.lnk'
        self.create_shortcut(old, self.port / legacy.LEGACY_EXE, arguments='--play',
                             working_dir=str(self.port.resolve()))
        self.assertFalse(legacy.retarget_desktop_shortcut(self.port, desktop=self.desktop))
        self.assertTrue(old.is_file())
        self.assertFalse((self.desktop / 'SpartanPort.lnk').exists())

    def test_existing_spartanport_shortcut_is_preserved(self):
        sp_lnk = self.desktop / 'SpartanPort.lnk'
        self.create_shortcut(sp_lnk, self.port / 'SpartanPort.exe')
        old = self.desktop / f'{legacy.LEGACY_TITLE}.lnk'
        self.create_shortcut(old, self.port / legacy.LEGACY_EXE, working_dir=str(self.port.resolve()))
        self.assertFalse(legacy.retarget_desktop_shortcut(self.port, desktop=self.desktop))
        self.assertTrue(sp_lnk.is_file())
        self.assertTrue(old.is_file())

    def test_repeated_launcher_starts_do_not_retrigger_migration(self):
        old = self.desktop / f'{legacy.LEGACY_TITLE}.lnk'
        self.create_shortcut(old, self.port / legacy.LEGACY_EXE, working_dir=str(self.port.resolve()))
        self.assertTrue(legacy.retarget_desktop_shortcut(self.port, desktop=self.desktop))
        self.assertTrue((self.desktop / 'SpartanPort.lnk').is_file())
        self.assertTrue(old.is_file(), 'Original shortcut must be preserved non-destructively')

        # Player deletes SpartanPort.lnk
        (self.desktop / 'SpartanPort.lnk').unlink()

        # Subsequent starts: migration is a no-op, user's shortcut stays untouched
        self.assertFalse(legacy.retarget_desktop_shortcut(self.port, desktop=self.desktop))
        self.assertTrue(old.is_file())
        self.assertFalse((self.desktop / 'SpartanPort.lnk').exists())

    def test_first_launch_after_upgrading_from_legacy_name(self):
        old = self.desktop / f'{legacy.LEGACY_TITLE}.lnk'
        self.create_shortcut(old, self.port / legacy.LEGACY_EXE, working_dir=str(self.port.resolve()))
        self.assertTrue(legacy.retarget_desktop_shortcut(self.port, desktop=self.desktop))
        self.assertTrue(old.is_file(), 'Original shortcut must be preserved non-destructively')
        self.assertTrue((self.desktop / 'SpartanPort.lnk').is_file())

    def test_ambiguous_ownership_preserves_existing_shortcut(self):
        # A shortcut manually created with the exact same target, arguments and working dir
        # as the legacy launcher shortcut cannot be conclusively distinguished.
        # It must be migrated non-destructively: SpartanPort.lnk is created, but God of War III.lnk is kept.
        old = self.desktop / f'{legacy.LEGACY_TITLE}.lnk'
        self.create_shortcut(old, self.port / legacy.LEGACY_EXE, arguments="", working_dir=str(self.port.resolve()))
        res = legacy.retarget_desktop_shortcut(self.port, desktop=self.desktop)
        self.assertTrue(res)
        self.assertTrue(old.is_file(), "Ambiguous shortcut must never be deleted")
        self.assertTrue((self.desktop / 'SpartanPort.lnk').is_file())
        self.assertTrue((self.port / legacy.SHORTCUT_MARKER).is_file())

    def test_failed_shortcut_creation_preserves_old_shortcut_and_does_not_write_marker(self):
        from unittest import mock
        import subprocess
        old = self.desktop / f'{legacy.LEGACY_TITLE}.lnk'
        self.create_shortcut(old, self.port / legacy.LEGACY_EXE, working_dir=str(self.port.resolve()))
        # Simulate PowerShell failing to create the new shortcut (Test-Path fails -> exit 1)
        failed_proc = subprocess.CompletedProcess(args=['powershell'], returncode=1, stdout=b'', stderr=b'Save failed')
        with mock.patch('subprocess.run', return_value=failed_proc):
            res = legacy.retarget_desktop_shortcut(self.port, desktop=self.desktop)
        self.assertFalse(res)
        self.assertTrue(old.is_file(), "Old shortcut must be preserved when shortcut creation fails")
        self.assertFalse((self.desktop / 'SpartanPort.lnk').exists())
        self.assertFalse((self.port / legacy.SHORTCUT_MARKER).exists(), "Marker must not be written on failed creation")

    def test_powershell_error_does_not_write_marker_and_allows_recovery(self):
        from unittest import mock
        import subprocess
        old = self.desktop / f'{legacy.LEGACY_TITLE}.lnk'
        self.create_shortcut(old, self.port / legacy.LEGACY_EXE, working_dir=str(self.port.resolve()))
        # Step 1: PowerShell fails with error
        error_proc = subprocess.CompletedProcess(args=['powershell'], returncode=127, stdout=b'', stderr=b'Crash')
        with mock.patch('subprocess.run', return_value=error_proc):
            res1 = legacy.retarget_desktop_shortcut(self.port, desktop=self.desktop)
        self.assertFalse(res1)
        self.assertTrue(old.is_file())
        self.assertFalse((self.port / legacy.SHORTCUT_MARKER).exists(), "Marker must not be written on PowerShell error")

        # Step 2: Recovery run without error succeeds
        res2 = legacy.retarget_desktop_shortcut(self.port, desktop=self.desktop)
        self.assertTrue(res2)
        self.assertTrue((self.desktop / 'SpartanPort.lnk').is_file())
        self.assertTrue(old.is_file(), "Old shortcut still preserved")
        self.assertTrue((self.port / legacy.SHORTCUT_MARKER).is_file())

    def test_shortcut_targeting_unrelated_executable(self):
        unrelated = self.root / 'other' / 'tool.exe'
        unrelated.parent.mkdir()
        unrelated.write_bytes(b'tool')
        old = self.desktop / f'{legacy.LEGACY_TITLE}.lnk'
        self.create_shortcut(old, unrelated)
        self.assertFalse(legacy.retarget_desktop_shortcut(self.port, desktop=self.desktop))
        self.assertTrue(old.is_file())
        self.assertFalse((self.desktop / 'SpartanPort.lnk').exists())

    def test_missing_shortcuts_and_missing_targets(self):
        # Target missing (SpartanPort.exe does not exist)
        (self.port / 'SpartanPort.exe').unlink()
        old = self.desktop / f'{legacy.LEGACY_TITLE}.lnk'
        self.create_shortcut(old, self.port / legacy.LEGACY_EXE)
        self.assertFalse(legacy.retarget_desktop_shortcut(self.port, desktop=self.desktop))
        self.assertTrue(old.is_file())

        # Shortcut missing (no shortcut on desktop)
        (self.port / 'SpartanPort.exe').write_bytes(b'exe')
        old.unlink()
        self.assertFalse(legacy.retarget_desktop_shortcut(self.port, desktop=self.desktop))
        self.assertTrue((self.port / legacy.SHORTCUT_MARKER).is_file())

    def test_windows_paths_containing_spaces(self):
        space_port = self.root / 'Port With Spaces' / 'God of War III'
        space_port.mkdir(parents=True)
        (space_port / 'SpartanPort.exe').write_bytes(b'exe')
        (space_port / legacy.LEGACY_EXE).write_bytes(b'exe')
        space_desktop = self.root / 'Desktop With Spaces'
        space_desktop.mkdir()
        old = space_desktop / f'{legacy.LEGACY_TITLE}.lnk'
        self.create_shortcut(old, space_port / legacy.LEGACY_EXE, working_dir=str(space_port.resolve()))
        self.assertTrue(legacy.retarget_desktop_shortcut(space_port, desktop=space_desktop))
        self.assertTrue(old.is_file(), 'Old shortcut must be preserved non-destructively')
        self.assertTrue((space_desktop / 'SpartanPort.lnk').is_file())

    def test_launcher_move_from_legacy_name_integration(self):
        import sys
        sys.path.insert(0, str(ROOT / 'launcher'))
        import spartanport_launcher_win as launcher
        from unittest import mock
        old = self.desktop / f'{legacy.LEGACY_TITLE}.lnk'
        self.create_shortcut(old, self.port / legacy.LEGACY_PLAY_EXE)
        with mock.patch.multiple(launcher, PORT_DIR=self.port, DATA_DIR=self.port,
                                 UPDATE_DIR=self.root / 'updates', WINDOWS=True, FROZEN=True):
            with mock.patch.object(legacy, '_windows_desktop', return_value=self.desktop):
                launcher.move_from_legacy_name()
        # Custom Play shortcut is preserved, SpartanPort.lnk is not created
        self.assertTrue(old.is_file())
        self.assertFalse((self.desktop / 'SpartanPort.lnk').exists())
        self.assertTrue((self.port / legacy.SHORTCUT_MARKER).is_file())


if __name__ == '__main__':
    unittest.main()
