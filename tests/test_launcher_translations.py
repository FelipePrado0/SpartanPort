"""Brazilian Portuguese covers every text the launcher shows (issue #451)."""
import ast
import re
import sys
import unittest
import xml.etree.ElementTree as ET

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import spartanport_lang  # noqa: E402
import spartanport_launcher_win as launcher  # noqa: E402

# Button names that read the same in Portuguese.
SAME = {'Options', 'Share / Create', 'Menu', 'View', 'Share'}


class PortugueseTest(unittest.TestCase):
    def setUp(self):
        self.pt = spartanport_lang.table('pt')

    def test_every_literal_text_is_translated(self):
        tree = ast.parse((ROOT / 'launcher' / 'spartanport_launcher_win.py').read_text(encoding='utf-8'))
        texts = {n.args[0].value for n in ast.walk(tree)
                 if isinstance(n, ast.Call) and getattr(n.func, 'id', None) == '_' and n.args
                 and isinstance(n.args[0], ast.Constant) and isinstance(n.args[0].value, str)}
        self.assertEqual(sorted(t for t in texts if t not in self.pt), [])

    def test_button_names_are_translated(self):
        labels = set(launcher.PS_BUTTON_NAMES.values())
        for style in launcher.PAD_STYLES.values():
            labels |= set(style.values())
        missing = [l for l in labels if l not in self.pt and l not in SAME and not re.fullmatch(r'[A-Z0-9]{1,2}', l)]
        self.assertEqual(sorted(missing), [])

    def test_the_built_in_patch_notes_are_translated(self):
        root = ET.parse(ROOT / 'patches' / 'CUSA01623.xml').getroot()
        notes = {m.get('Note') for m in root.iter('Metadata')
                 if not m.get('Name').startswith(('Resolution Patch', 'Frame Rate Patch'))}
        self.assertEqual(sorted(n for n in notes if n not in self.pt), [])

    def test_resolution_labels(self):
        self.assertEqual(self.pt[launcher.RESOLUTION_CHOICES[0][1][0]], 'Nativa · 1920 × 1080 · 16:9')



class SpanishTest(PortugueseTest):
    """Spanish covers the same texts as Brazilian Portuguese."""

    def setUp(self):
        self.pt = spartanport_lang.table('es')

    def test_resolution_labels(self):
        self.assertEqual(self.pt[launcher.RESOLUTION_CHOICES[0][1][0]], 'Nativa · 1920 × 1080 · 16:9')


class ChineseTest(PortugueseTest):
    """Simplified Chinese covers the current launcher, including dynamic choices."""

    def setUp(self):
        self.pt = spartanport_lang.table('zh')

    def test_resolution_labels(self):
        self.assertEqual(self.pt[launcher.RESOLUTION_CHOICES[0][1][0]], '原生 · 1920 × 1080 · 16:9')

    def test_all_known_labels_and_format_fields(self):
        from string import Formatter
        fields = lambda text: [(field, spec, conversion) for _literal, field, spec, conversion in
                               Formatter().parse(text) if field is not None]
        known = set(spartanport_lang.KEYS) | set().union(*(set(v) for v in spartanport_lang.EXTRA.values()))
        self.assertEqual(sorted(key for key in known if not self.pt.get(key)), [])
        for english, chinese in self.pt.items():
            with self.subTest(text=english):
                self.assertEqual(fields(english), fields(chinese))

    def test_menu_language_is_independent_from_game_language(self):
        settings = {**launcher.APP_DEFAULTS, 'ui_language': 'zh', 'language': '1'}
        env = launcher.game_environment(settings)
        self.assertEqual(env['SPARTAN_UI_LANGUAGE'], 'zh')
        self.assertEqual(env['SPARTAN_LANGUAGE'], '1')


class IndonesianTest(PortugueseTest):
    """Indonesian covers the same texts as Brazilian Portuguese."""

    def setUp(self):
        self.pt = spartanport_lang.table('id')

    def test_resolution_labels(self):
        self.assertEqual(self.pt[launcher.RESOLUTION_CHOICES[0][1][0]], 'Asli · 1920 × 1080 · 16:9')


if __name__ == '__main__':
    unittest.main()
