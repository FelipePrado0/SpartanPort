"""Game language selector: Brazilian Portuguese reaches the game as its system language id."""
import sys
from pathlib import Path
import unittest

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import spartanport_lang  # noqa: E402
import spartanport_launcher_win as launcher  # noqa: E402


class GameLanguageTest(unittest.TestCase):
    def test_brazilian_portuguese_is_offered_with_its_system_id(self):
        offered = {value: names[0] for value, names in launcher.LANGUAGES}
        self.assertEqual(offered.get('17'), 'Portuguese (Brazil)')

    def test_choosing_brazilian_portuguese_starts_the_game_with_id_17(self):
        env = launcher.game_environment({**launcher.APP_DEFAULTS, 'language': '17'})
        self.assertEqual(env['SPARTAN_LANGUAGE'], '17')

    def test_brazilian_portuguese_label_is_translated_in_portuguese_and_spanish(self):
        self.assertEqual(spartanport_lang.EXTRA['pt']['Portuguese (Brazil)'], 'Português (Brasil)')
        self.assertEqual(spartanport_lang.EXTRA['es']['Portuguese (Brazil)'], 'Portugués (Brasil)')
        self.assertEqual(spartanport_lang.EXTRA['id']['Portuguese (Brazil)'], 'Portugis (Brasil)')


class GameLanguageNoteTest(unittest.TestCase):
    """The game keeps its own text and speech languages after the first start (#842): the launcher says so,
    in the game's own menu names."""

    def test_the_note_names_the_game_menus(self):
        note = spartanport_lang._GAME_LANGUAGE_NOTE
        self.assertIn('Options → Game Settings → Game Text', note)
        self.assertIn('Options → Audio → Speech', note)
        source = Path(launcher.__file__).read_text(encoding='utf-8')
        self.assertIn("Used on the game's first start.", source)  # the row shows this note

    def test_the_note_uses_the_game_menu_names_of_each_language(self):
        names = {'pt': ('Texto de jogo', 'Falas'), 'es': ('Texto del juego', 'Diálogos'),
                 'pl': ('Tekst gry', 'Dialogi'), 'fr': ('Texte du jeu', 'Voix'),
                 'de': ('Spieltext', 'Sprachausgabe'), 'it': ('Testo di gioco', 'Dialoghi')}
        for language, (text, speech) in names.items():
            with self.subTest(language=language):
                translated = spartanport_lang.table(language)[spartanport_lang._GAME_LANGUAGE_NOTE]
                self.assertIn(text, translated)
                self.assertIn(speech, translated)


if __name__ == '__main__':
    unittest.main()
