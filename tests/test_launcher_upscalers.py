"""The launcher offers the upscalers the game can run on this platform, by the names it reads."""
import re
import sys
import unittest

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import gow3_launcher_win as launcher  # noqa: E402


def game_upscaler_names():
    """The gow3.ini upscaler names of gpu/shim/gow3_settings.cpp (UpscalerName)."""
    source = (ROOT / 'gpu' / 'shim' / 'gow3_settings.cpp').read_text(encoding='utf-8')
    names = re.search(r'names\[UpscalerCount\] = \{([^}]*)\}', source)
    return re.findall(r'"(\w+)"', names.group(1))


class LauncherUpscalersTest(unittest.TestCase):
    def test_every_choice_is_a_name_the_game_reads(self):
        game = game_upscaler_names()
        self.assertIn('metalfx', game)
        for name, _labels in launcher.UPSCALERS:
            with self.subTest(name=name):
                self.assertIn(name, game)

    def test_metalfx_on_a_mac_and_dlss_elsewhere(self):
        names = [name for name, _labels in launcher.UPSCALERS]
        self.assertEqual('metalfx' in names, launcher.MACOS)
        self.assertEqual('dlss' in names, not launcher.MACOS)
        if launcher.MACOS:
            self.assertEqual(names[0], 'metalfx')  # Apple's own upscaler first
        self.assertEqual(names[-1], 'off')


if __name__ == '__main__':
    unittest.main()
