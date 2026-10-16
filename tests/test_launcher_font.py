"""The God of War title font: an optional launcher asset, never a reason the launcher cannot start."""
import sys
import unittest
from pathlib import Path

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import gow3_launcher_win as launcher  # noqa: E402


class FontFileTest(unittest.TestCase):
    def test_font_file_is_a_launcher_asset_under_port_dir(self):
        # Frozen builds find launcher assets next to the exe (PORT_DIR / 'launcher'), like gow3.ico.
        self.assertEqual(launcher.FONT_FILE, launcher.PORT_DIR / 'launcher' / 'gowfont.ttf')
        self.assertTrue(launcher.FONT_FILE.is_file())

    def test_macos_package_ships_the_font(self):
        script = (ROOT / 'packaging' / 'macos' / 'package.sh').read_text(encoding='utf-8')
        self.assertIn('launcher/gowfont.ttf "$res/launcher/"', script)

    def test_windows_package_ships_the_font(self):
        script = (ROOT / 'packaging' / 'windows' / 'package.sh').read_text(encoding='utf-8')
        copies = [line for line in script.splitlines()
                  if 'launcher/gowfont.ttf' in line and '"$dest/launcher/"' in line]
        self.assertTrue(copies, 'package.sh must copy launcher/gowfont.ttf into $dest/launcher/')


class RegisterFontTest(unittest.TestCase):
    def test_missing_file_is_not_fatal(self):
        self.assertFalse(launcher.register_font(Path('no-such-font.ttf')))

    @unittest.skipUnless(sys.platform == 'win32', 'GDI font registration')
    def test_file_that_is_not_a_font_is_refused(self):
        self.assertFalse(launcher.register_font(Path(launcher.__file__)))

    @unittest.skipUnless(sys.platform == 'win32', 'GDI font registration')
    def test_font_file_registers(self):
        self.assertTrue(launcher.register_font(launcher.FONT_FILE))


    @unittest.skipUnless(sys.platform == 'darwin', 'Core Text font registration')
    def test_core_text_registers_the_font_and_refuses_other_files(self):
        self.assertTrue(launcher.register_font(launcher.FONT_FILE))
        self.assertFalse(launcher.register_font(Path(launcher.__file__)))


class HeadingFontTest(unittest.TestCase):
    def setUp(self):
        self.saved = launcher.LANG, launcher.FONT_LOADED

    def tearDown(self):
        launcher.LANG, launcher.FONT_LOADED = self.saved

    def test_english_titles_use_the_loaded_font(self):
        launcher.LANG, launcher.FONT_LOADED = 'en', True
        self.assertEqual(launcher.heading_font(12), ('GodOfWar', 12))

    def test_other_languages_keep_the_fallback(self):
        launcher.LANG, launcher.FONT_LOADED = 'ru', True
        self.assertEqual(launcher.heading_font(12), ('Georgia', 12))
        self.assertEqual(launcher.heading_font(11, 'Segoe UI'), ('Segoe UI', 11))

    def test_unloaded_font_keeps_the_fallback(self):
        launcher.LANG, launcher.FONT_LOADED = 'en', False
        self.assertEqual(launcher.heading_font(20), ('Georgia', 20))

    def test_game_name_uses_the_font_in_every_language(self):
        launcher.LANG, launcher.FONT_LOADED = 'ru', True
        self.assertEqual(launcher.heading_font(15, every_language=True), ('GodOfWar', 15))
        launcher.FONT_LOADED = False
        self.assertEqual(launcher.heading_font(15, every_language=True), ('Georgia', 15))


if __name__ == '__main__':
    unittest.main()
