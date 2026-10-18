"""Game language selector: Brazilian Portuguese reaches the game as its system language id."""
import sys
import unittest

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import gow3_lang  # noqa: E402
import gow3_launcher_win as launcher  # noqa: E402


class GameLanguageTest(unittest.TestCase):
    def test_brazilian_portuguese_is_offered_with_its_system_id(self):
        offered = {value: names[0] for value, names in launcher.LANGUAGES}
        self.assertEqual(offered.get('17'), 'Portuguese (Brazil)')

    def test_choosing_brazilian_portuguese_starts_the_game_with_id_17(self):
        env = launcher.game_environment({**launcher.APP_DEFAULTS, 'language': '17'})
        self.assertEqual(env['GOW3_LANGUAGE'], '17')

    def test_brazilian_portuguese_label_is_translated_in_portuguese_and_spanish(self):
        self.assertEqual(gow3_lang.EXTRA['pt']['Portuguese (Brazil)'], 'Português (Brasil)')
        self.assertEqual(gow3_lang.EXTRA['es']['Portuguese (Brazil)'], 'Portugués (Brasil)')


if __name__ == '__main__':
    unittest.main()
