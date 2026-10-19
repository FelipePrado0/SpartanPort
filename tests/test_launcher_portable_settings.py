"""Portable launcher settings: store settings.json in DATA_DIR next to spartanport.ini (#561)."""
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest import mock

import sys
from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import spartanport_launcher_win as launcher


class PortableSettingsPathsTest(unittest.TestCase):
    def test_config_dir_is_data_dir(self):
        self.assertEqual(launcher.CONFIG_DIR, launcher.DATA_DIR)
        self.assertEqual(launcher.CONFIG_FILE, launcher.DATA_DIR / 'settings.json')

    def test_user_files_includes_settings_and_icon(self):
        self.assertIn('settings.json', launcher.USER_FILES)
        self.assertIn('game_icon.ico', launcher.USER_FILES)


class PortableSettingsMigrationTest(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.root = Path(self.temp_dir.name)
        self.data_dir = self.root / 'data'
        self.legacy_dir = self.root / 'legacy'
        self.data_dir.mkdir()
        self.legacy_dir.mkdir()
        self.target = self.data_dir / 'settings.json'
        self.legacy = self.legacy_dir / 'settings.json'

    def test_copies_legacy_config_and_icon_when_portable_missing(self):
        self.legacy.write_text('{"game_dir": "D:/Games/SpartanPort", "ui_scale": "2"}', encoding='utf-8')
        legacy_icon = self.legacy_dir / 'game_icon.ico'
        legacy_icon.write_bytes(b'synthetic icon data')

        launcher.migrate_portable_settings(config_file=self.target, legacy_file=self.legacy)

        self.assertTrue(self.target.is_file())
        self.assertEqual(json.loads(self.target.read_text(encoding='utf-8')),
                         {'game_dir': 'D:/Games/SpartanPort', 'ui_scale': '2'})
        target_icon = self.data_dir / 'game_icon.ico'
        self.assertTrue(target_icon.is_file())
        self.assertEqual(target_icon.read_bytes(), b'synthetic icon data')

    def test_does_not_overwrite_existing_portable_config(self):
        self.target.write_text('{"game_dir": "D:/Games/Portable"}', encoding='utf-8')
        self.legacy.write_text('{"game_dir": "D:/Games/OldAppData"}', encoding='utf-8')

        launcher.migrate_portable_settings(config_file=self.target, legacy_file=self.legacy)

        self.assertEqual(json.loads(self.target.read_text(encoding='utf-8')),
                         {'game_dir': 'D:/Games/Portable'})

    def test_missing_legacy_config_is_a_noop(self):
        launcher.migrate_portable_settings(config_file=self.target, legacy_file=self.legacy)
        self.assertFalse(self.target.is_file())

    def test_load_app_settings_migrates_and_merges_defaults(self):
        self.legacy.write_text('{"game_dir": "C:/Games/SpartanPort"}', encoding='utf-8')
        with mock.patch.object(launcher, 'CONFIG_FILE', self.target), \
                mock.patch.object(launcher, 'LEGACY_CONFIG_FILE', self.legacy):
            settings = launcher.load_app_settings(self.target)

        self.assertEqual(settings['game_dir'], 'C:/Games/SpartanPort')
        self.assertEqual(settings['ui_scale'], launcher.APP_DEFAULTS['ui_scale'])
        self.assertTrue(self.target.is_file())

    def test_packaging_scripts_preserve_portable_files(self):
        package_sh = (ROOT / 'packaging/windows/package.sh').read_text(encoding='utf-8')
        self.assertIn('! -name settings.json ! -name game_icon.ico', package_sh)
        self.assertIn('-o -iname settings.json \\\n    -o -iname game_icon.ico', package_sh)

        smoke_test = (ROOT / 'packaging/windows/smoke-test.ps1').read_text(encoding='utf-8')
        self.assertIn("'settings.json', 'game_icon.ico'", smoke_test)


if __name__ == '__main__':
    unittest.main()
