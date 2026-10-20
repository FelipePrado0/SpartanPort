import json
import re
import tempfile
import unittest
from pathlib import Path

import paths  # noqa: F401 (puts scripts/ on sys.path)
import graphics_options as g


class GraphicsOptionsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.ini = Path(self.temp.name) / 'spartanport.ini'
        self.state = Path(self.temp.name) / 'state.json'

    def tearDown(self):
        self.temp.cleanup()

    def test_defaults_and_invalid_values(self):
        self.ini.write_text(
            'render_resolution=invalid\nengine_fps=90\nstale_readback=yes\n',
            encoding='utf-8',
        )
        options, reverted = g.prepare(self.ini, self.state)
        self.assertFalse(reverted)
        self.assertEqual(
            options,
            {
                'render_resolution': 'native',
                'engine_fps': '120',
                'deferred_readback': '1',
                'stale_readback': '1',
                'async_shaders': '1',
            },
        )

    def test_corrupt_state_file_preserves_current_settings(self):
        self.ini.write_text(
            'render_resolution=1440p\nengine_fps=240\n',
            encoding='utf-8',
        )
        self.state.write_text('{invalid json', encoding='utf-8')

        options, reverted = g.prepare(self.ini, self.state)

        self.assertFalse(reverted)
        self.assertEqual(options['render_resolution'], '1440p')
        self.assertEqual(options['engine_fps'], '240')

    def test_corrupt_state_file_preserves_current_settings(self):
        self.ini.write_text('render_resolution=1440p\nengine_fps=240\n', encoding='utf-8')
        self.state.write_text('{invalid json', encoding='utf-8')

        options, reverted = g.prepare(self.ini, self.state)

        self.assertFalse(reverted)
        self.assertEqual(options['render_resolution'], '1440p')
        self.assertEqual(options['engine_fps'], '240')

    def test_8k_selection_replaces_previous_resolution(self):
        options = g.startup_options({'render_resolution': '8K'})
        self.assertEqual(options['render_resolution'], '8K')
        self.assertEqual(g.SIZES['8K'], (7680, 4320))
        self.assertEqual(g.resolution_label('8K'), '8K · 7680 × 4320 · 16:9 (beta)')
        names = g.patch_names(['Resolution Patch - 4K', g.TEXTURE_FIX], options)
        self.assertEqual([n for n in names if n.startswith(g.RESOLUTION_PREFIX)],
                         ['Resolution Patch - 8K'])
        self.assertNotIn(g.TEXTURE_FIX, names)

    def test_write_keys_keeps_other_lines(self):
        self.ini.write_text(
            '# comment\nshow_fps=1\nengine_fps=60\n',
            encoding='utf-8',
        )
        g.write_keys(self.ini, {'engine_fps': '240', 'vsync': '0'})
        self.assertEqual(
            self.ini.read_text(encoding='utf-8'),
            '# comment\nshow_fps=1\nengine_fps=240\nvsync=0\n',
        )

    def test_unconfirmed_restart_is_tried_once_then_reverted(self):
        self.ini.write_text(
            'render_resolution=1440p\nengine_fps=120\n',
            encoding='utf-8',
        )
        good, _ = g.prepare(self.ini, self.state)
        g.write_keys(
            self.ini,
            {
                'render_resolution': '4K',
                'engine_fps': '240',
                'restart_unconfirmed': '1',
            },
        )

        tried, reverted = g.prepare(self.ini, self.state)
        self.assertFalse(reverted)
        self.assertEqual(
            (tried['render_resolution'], tried['engine_fps']),
            ('4K', '240'),
        )

        back, reverted = g.prepare(self.ini, self.state)
        self.assertTrue(reverted)
        self.assertEqual(back, good)

        ini = g.read_ini(self.ini)
        self.assertEqual(
            (ini['render_resolution'], ini['restart_unconfirmed']),
            ('1440p', '0'),
        )

    def test_confirmed_restart_becomes_the_last_good(self):
        g.prepare(self.ini, self.state)
        g.write_keys(
            self.ini,
            {'engine_fps': '240', 'restart_unconfirmed': '1'},
        )
        g.prepare(self.ini, self.state)
        g.write_keys(self.ini, {'restart_unconfirmed': '0'})
        g.prepare(self.ini, self.state)

        self.assertEqual(
            json.loads(self.state.read_text())['last_good']['engine_fps'],
            '240',
        )

    def test_patch_names(self):
        launcher = [
            'Bug Fix - Texture Corruption Fix',
            'Frame Rate Patch - 120 FPS',
            'Skip Any Video With X Button',
        ]
        options = g.startup_options(
            {'render_resolution': '1440p', 'engine_fps': '240'}
        )
        self.assertEqual(
            g.patch_names(launcher, options),
            [
                'Skip Any Video With X Button',
                'Resolution Patch - 1440p',
                'Frame Rate Patch - 240 FPS',
            ],
        )

        # The engine rate changes live (in-game menu, R3 + L1 + R1): every launch takes the
        # highest frame rate patch, even for 60 or 100 FPS.
        native = g.startup_options({'engine_fps': '60'})
        self.assertEqual(
            g.patch_names(
                ['Resolution Patch - 4K', 'Skip Any Video With X Button'],
                native,
            ),
            [
                'Bug Fix - Texture Corruption Fix',
                'Skip Any Video With X Button',
                'Frame Rate Patch - 240 FPS',
            ],
        )
        self.assertEqual(g.startup_options({'engine_fps': '100'})['engine_fps'], '100')

    def test_resolution_labels_show_the_size_and_aspect_ratio(self):
        self.assertEqual(g.resolution_label('native'), 'Native · 1920 × 1080 · 16:9')
        self.assertEqual(g.resolution_label('480p'), '480p · 854 × 480 · 16:9')
        self.assertEqual(g.resolution_label('900p'), '900p · 1600 × 900 · 16:9')
        self.assertEqual(g.resolution_label('3440x1440'), '3440 × 1440 · 21:9 (beta)')
        self.assertEqual(g.resolution_label('5120x1440'), '5120 × 1440 · 32:9 (beta)')
        self.assertEqual(g.resolution_label('2560x1600'), '2560 × 1600 · 16:10 (beta)')
        self.assertEqual(g.resolution_label('1280x800'), '1280 × 800 · 16:10')  # the Steam Deck's screen, tested
        self.assertEqual(g.resolution_label('1280x960'), '1280 × 960 · 4:3')
        self.assertEqual(g.resolution_label('1920x1440'), '1920 × 1440 · 4:3 (beta)')
        self.assertEqual(g.resolution_label('3840x1080'), '3840 × 1080 · 32:9')  # a mod's size, not beta
        self.assertEqual(g.resolution_label('1600x1200'), '1600 × 1200 · 4:3')
        self.assertEqual(g.resolution_label('custom'), 'custom')
        # Every ultrawide and 16:10 size is beta; the 16:9 ones are not.
        self.assertEqual([r for r in g.RESOLUTIONS if g.resolution_label(r).endswith('(beta)')],
                         list(g.BETA_RESOLUTIONS))

    def test_every_built_in_resolution_has_a_patch(self):
        import patches
        xml = Path(__file__).resolve().parent.parent / 'patches' / 'CUSA01623.xml'
        self.assertEqual(sorted(r for r in g.RESOLUTIONS if r != 'native'), sorted(patches.resolution_names([xml])))

    def test_900p_is_the_built_in_size_between_720p_and_1440p(self):
        # #238: 900p (1600x900) sits between 720p and 1440p, with its own resolution patch.
        self.assertEqual(g.RESOLUTIONS.index('900p'), 3)
        self.assertEqual(g.SIZES['900p'], (1600, 900))
        import xml.etree.ElementTree as ET
        xml = Path(__file__).resolve().parent.parent / 'patches' / 'CUSA01623.xml'
        names = {m.get('Name') for m in ET.parse(xml).getroot().iter('Metadata')}
        self.assertIn('Resolution Patch - 900p', names)

    def test_resolutions_of_enabled_mods(self):
        # A mod's "Resolution Patch - <size>" is a render resolution only while its mod is on.
        self.assertEqual(g.resolutions(), g.RESOLUTIONS)
        self.assertEqual(g.resolutions(['3840x1080', '4K']), g.RESOLUTIONS + ('3840x1080',))
        ini = {'render_resolution': '3840x1080', 'engine_fps': '60'}
        self.assertEqual(g.startup_options(ini)['render_resolution'], 'native')
        options = g.startup_options(ini, ['3840x1080'])
        self.assertEqual(options['render_resolution'], '3840x1080')
        self.assertEqual(g.patch_names(['Bug Fix - Texture Corruption Fix'], options),
                         ['Resolution Patch - 3840x1080', 'Frame Rate Patch - 240 FPS'])

    def test_a_confirmed_mod_resolution_is_restored_while_its_mod_is_on(self):
        with tempfile.TemporaryDirectory() as folder:
            ini, state = Path(folder) / 'spartanport.ini', Path(folder) / 'state.json'
            g.write_keys(ini, {'render_resolution': '3840x1080', 'engine_fps': '60'})
            tried, _ = g.prepare(ini, state, ['3840x1080'])
            self.assertEqual(tried['render_resolution'], '3840x1080')
            g.write_keys(ini, {'render_resolution': '4K', 'restart_unconfirmed': '1'})
            self.assertEqual(g.prepare(ini, state, ['3840x1080'])[0]['render_resolution'], '4K')  # attempt
            options, reverted = g.prepare(ini, state, ['3840x1080'])  # the restart never confirmed
            self.assertTrue(reverted)
            self.assertEqual(options['render_resolution'], '3840x1080')

    def test_engine_rate_100_for_100hz_displays(self):
        self.assertIn(100, g.ENGINE_FPS)
        options = g.startup_options({'render_resolution': 'native', 'engine_fps': '100'})
        self.assertEqual(options['engine_fps'], '100')
        # 100 is written live over the 240 FPS patch (spartanport_settings.cpp SyncEngineFps).
        self.assertEqual(g.patch_names(['Bug Fix - Texture Corruption Fix'], options),
                         ['Bug Fix - Texture Corruption Fix', 'Frame Rate Patch - 240 FPS'])
        self.assertEqual(g.startup_options({'engine_fps': '90'})['engine_fps'], '120')  # not a choice: the default

    def test_engine_rate_180_for_high_refresh_displays(self):
        self.assertEqual(g.ENGINE_FPS, (60, 100, 120, 144, 180, 240))
        options = g.startup_options({'render_resolution': 'native', 'engine_fps': '180'})
        self.assertEqual(options['engine_fps'], '180')
        self.assertIn('Frame Rate Patch - 240 FPS', g.patch_names([], options))

    def test_engine_rate_144_for_144hz_displays(self):
        self.assertIn(144, g.ENGINE_FPS)
        options = g.startup_options({'render_resolution': 'native', 'engine_fps': '144'})
        self.assertEqual(options['engine_fps'], '144')
        self.assertEqual(g.patch_names(['Bug Fix - Texture Corruption Fix'], options),
                         ['Bug Fix - Texture Corruption Fix', 'Frame Rate Patch - 240 FPS'])

    def test_applied_engine_fps(self):
        self.assertEqual(
            g.applied_engine_fps(
                ['Skip Any Video With X Button', 'Frame Rate Patch - 240 FPS']
            ),
            240,
        )
        self.assertEqual(
            g.applied_engine_fps(['Frame Rate Patch - 120 FPS']),
            120,
        )
        self.assertEqual(
            g.applied_engine_fps(['Frame Rate Patch - 100 FPS']),
            100,
        )
        self.assertEqual(g.applied_engine_fps(['Frame Rate Patch - 144 FPS']), 144)
        self.assertEqual(
            g.applied_engine_fps(['Bug Fix - Texture Corruption Fix']),
            60,
        )
        self.assertEqual(g.applied_engine_fps([]), 60)

    def test_environment(self):
        env = g.environment(
            g.startup_options({'stale_readback': '0'})
        )
        self.assertEqual(
            env,
            {
                'SPARTAN_DEFERRED_READBACK': '1',
                'SPARTAN_STALE_READBACK': '0',
                'SPARTAN_ASYNC_SHADERS': '1',
            },
        )


class WindowSizeTest(unittest.TestCase):
    def test_the_game_knows_the_launcher_sizes(self):
        """gpu/shim/spartanport_graphics.h ResolutionSizes, which sizes the window, matches SIZES."""
        header = (paths.ROOT / 'gpu' / 'shim' / 'spartanport_graphics.h').read_text(encoding='utf-8')
        found = {name: (int(w), int(h)) for name, w, h in re.findall(r'\{"(\w+)", \{(\d+), (\d+)\}\}', header)}
        self.assertEqual(found, g.SIZES)



class AspectRatioTest(unittest.TestCase):
    """The launcher's Aspect ratio filter: the render resolutions of one aspect ratio."""

    def test_aspect_of_a_resolution(self):
        self.assertEqual([g.aspect_of(r) for r in ('native', '720p', '1360x768', '1280x800', '3440x1440',
                                                   '5120x1440', '1280x960', '5120x2160')],
                         ['16:9', '16:9', '16:9', '16:10', '21:9', '32:9', '4:3', '21:9'])
        self.assertEqual(g.aspect_of('1280x1024'), '5:4')  # a mod's size outside the named ones

    def test_aspects_in_a_fixed_order_with_the_others_last(self):
        self.assertEqual(g.aspects(g.RESOLUTIONS), ['16:9', '16:10', '21:9', '32:9', '4:3'])
        self.assertEqual(g.aspects(g.resolutions(('1280x1024',))), ['16:9', '16:10', '21:9', '32:9', '4:3', '5:4'])

    def test_resolutions_of_an_aspect_smallest_first_native_on_top(self):
        self.assertEqual(g.resolutions_for('16:9', g.RESOLUTIONS),
                         ['native', '480p', '720p', '1360x768', '900p', '1440p', '1800p', '4K', '8K'])
        self.assertEqual(g.resolutions_for('16:10', g.RESOLUTIONS), ['1280x800', '1440x900', '1920x1200', '2560x1600'])
        self.assertEqual(g.resolutions_for('4:3', g.RESOLUTIONS),
                         ['1280x960', '1440x1080', '1920x1440', '2048x1536', '2560x1920'])

    def test_switching_aspect_keeps_the_closest_size(self):
        self.assertEqual(g.closest_resolution('16:10', '1440p', g.RESOLUTIONS), '2560x1600')
        self.assertEqual(g.closest_resolution('4:3', 'native', g.RESOLUTIONS), '1440x1080')
        self.assertEqual(g.closest_resolution('16:9', '1280x800', g.RESOLUTIONS), '1360x768')
        self.assertEqual(g.closest_resolution('16:10', '1440x900', g.RESOLUTIONS), '1440x900')  # already of it

    def test_labels_without_the_aspect_ratio_for_the_filtered_list(self):
        self.assertEqual(g.resolution_label('1800p', aspect=False), '1800p · 3200 × 1800')
        self.assertEqual(g.resolution_label('3440x1440', aspect=False), '3440 × 1440 (beta)')
        self.assertEqual(g.resolution_label('1800p'), '1800p · 3200 × 1800 · 16:9')  # elsewhere it stays

if __name__ == '__main__':
    unittest.main()
