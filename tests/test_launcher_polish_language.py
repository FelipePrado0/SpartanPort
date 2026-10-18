"""Game language selector: Polish (Orbis language ID 16) support (#493)."""
import re
import sys
import unittest

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import gow3_lang  # noqa: E402
import gow3_launcher_win as launcher  # noqa: E402


class PolishLanguageTest(unittest.TestCase):
    def test_polish_is_offered_with_system_id_16(self):
        offered = {value: names[0] for value, names in launcher.LANGUAGES}
        self.assertEqual(offered.get('16'), 'Polish')
        russian_labels = {value: names[1] for value, names in launcher.LANGUAGES if len(names) > 1}
        self.assertEqual(russian_labels.get('16'), 'Польский')

    def test_choosing_polish_starts_the_game_with_id_16(self):
        env = launcher.game_environment({**launcher.APP_DEFAULTS, 'language': '16'})
        self.assertEqual(env['GOW3_LANGUAGE'], '16')

    def test_polish_label_is_translated_across_ui_languages(self):
        expected_translations = {
            'pt': 'Polonês',
            'es': 'Polaco',
            'pl': 'Polski',
            'fr': 'Polonais',
            'de': 'Polnisch',
            'it': 'Polacco',
            'tr': 'Lehçe',
            'ar': 'البولندية',
            'zh': '波兰语',
            'ja': 'ポーランド語',
            'ko': '폴란드어',
        }
        for lang, expected in expected_translations.items():
            with self.subTest(lang=lang):
                self.assertEqual(gow3_lang.table(lang).get('Polish'), expected)

    def test_russian_and_english_fallbacks(self):
        saved = launcher.LANG
        try:
            launcher.LANG = 'ru'
            self.assertEqual(launcher._('Polish', 'Польский'), 'Польский')
            launcher.LANG = 'en'
            self.assertEqual(launcher._('Polish', 'Польский'), 'Polish')
        finally:
            launcher.LANG = saved

    def test_system_service_contract_maps_parameter_1_to_gow3_language(self):
        services_src = (ROOT / 'src' / 'runtime_services.c').read_text(encoding='utf-8')
        # SystemService system_param: case 1 returns language()
        self.assertRegex(services_src, r'case\s+1:\s*\*value\s*=\s*language\(\);')
        # language() parses getenv("GOW3_LANGUAGE")
        self.assertRegex(services_src, r'getenv\("GOW3_LANGUAGE"\)')


if __name__ == '__main__':
    unittest.main()
