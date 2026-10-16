"""Controller glyphs (scripts/glyphs.py) on a synthetic WAD: no game files."""
import io
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from paths import ROOT
import glyphs

try:
    from PIL import Image
except ImportError:
    Image = None
sys.path.insert(0, str(ROOT))
import run  # noqa: E402


def entry(name, payload=b'', tag=1):
    return (struct.pack('<HHI', tag, 0, len(payload)) + name.encode().ljust(56, b'\0')
            + payload.ljust((len(payload) + 15) & ~15, b'\0'))


def texture(name, width, height, fourcc, levels=1, length=0):
    """A texture entry as the game stores it: a 0x4C prefix and a DDS with `levels` mip levels
    (or `length` bytes of pixels)."""
    image = Image.new('RGBA', (width, height), (255, 0, 255, 255))
    header = io.BytesIO()
    image.save(header, 'DDS', pixel_format=fourcc)
    for level in range(0 if length else levels):
        blocks = -(-max(1, width >> level) // 4) * -(-max(1, height >> level) // 4)
        length += blocks * (16 if fourcc == 'DXT5' else 8)
    return entry(name, b'\7\0\1\0' + name.encode().ljust(0x48, b'\0') + header.getvalue()[:128] + b'\xee' * length)


def wad():
    return bytearray(entry('WAD_R_UI', b'\0' * 8, tag=0x14) + entry('GroupStart', tag=2)
                     + entry('MAT_macroxicon', b'TX1BBACF4D.2E0253CF' + b'\1' * 30)
                     + texture('TX1BBACF4D.2E0253CF.dds', 64, 64, 'DXT5')        # cross, inline
                     + texture('TX7C8D1BEB.350710D4.dds', 64, 64, 'DXT5')        # not a glyph
                     + texture('TX7FF61BF7.CC842B4F.dds', 128, 128, 'DXT1', 7)   # cross, QTE
                     + texture('TXD43D1443.2B12201C.dds', 64, 40, 'DXT5', 3)     # select
                     + texture('TXFB91136E.B22AD3A4.dds', 256, 256, 'DXT1', 8)   # L2/L1 atlas
                     + entry('GroupEnd', tag=3))


class GlyphTableTest(unittest.TestCase):
    def test_entries_walk_the_whole_file_and_stop_at_a_truncated_entry(self):
        data = entry('A', b'x' * 5) + entry('B') + entry('C', b'y' * 16)
        self.assertEqual([(offset, size, name) for offset, size, name in glyphs.entries(data)],
                         [(0, 5, 'A'), (0x50, 0, 'B'), (0x90, 16, 'C')])
        self.assertEqual([name for _o, _s, name in glyphs.entries(data[:-8])], ['A', 'B'])

    def test_glyph_of_each_style(self):
        self.assertEqual(glyphs.glyph('a', 'xbox'), ('disc', 'A', glyphs.STYLES['xbox']['colors']['a']))
        self.assertEqual(glyphs.glyph('a', 'nintendo')[:2], ('disc', 'B'))  # south on a Switch pad
        self.assertEqual(glyphs.glyph('leftshoulder', 'xbox')[:2], ('plate', 'LB'))
        self.assertEqual(glyphs.glyph('righttrigger', 'nintendo')[:2], ('plate', 'ZR'))
        self.assertEqual(glyphs.glyph('leftstick', 'xbox')[:2], ('disc', 'LS'))
        self.assertEqual(glyphs.glyph('start', 'xbox')[:2], ('plate', 'menu'))
        self.assertEqual(glyphs.glyph('back', 'nintendo')[:2], ('disc', 'minus'))
        self.assertEqual(glyphs.glyph('dpup', 'xbox')[:2], ('plate', 'dpup'))
        self.assertEqual(glyphs.glyph('paddle1', 'xbox')[:2], ('plate', 'PA'))

    def test_the_letters_of_the_controller_win_over_the_style(self):
        faces = {'south': 'b', 'east': 'a', 'west': 'unknown', 'north': None}
        self.assertEqual(glyphs.glyph('a', 'xbox', faces)[1], 'B')
        self.assertEqual(glyphs.glyph('x', 'xbox', faces)[1], 'X')
        self.assertEqual(glyphs.glyph('y', 'xbox', faces)[1], 'Y')

    def test_pad_map_moves_the_glyph(self):
        buttons = glyphs.pad_buttons('cross=b, Circle = a,l2=x,nonsense=y,square=')
        self.assertEqual((buttons['cross'], buttons['circle']), ('b', 'a'))
        self.assertEqual((buttons['l2'], buttons['square']), ('lefttrigger', 'x'))
        self.assertEqual(glyphs.pad_buttons(''), glyphs.DEFAULT_MAP)
        self.assertEqual(set(button for _form, names in glyphs.TEXTURES.values()
                             for button in ([names] if isinstance(names, str) else names)) - set(glyphs.DEFAULT_MAP), set())

    def test_gamepad_line(self):
        line = 'CPU: complete\nGamepad: type=xboxone south=a east=b west=x north=y name=Xbox One Controller\n'
        self.assertEqual(glyphs.parse_gamepad(line),
                         ('xbox', {'south': 'a', 'east': 'b', 'west': 'x', 'north': 'y'}, 'Xbox One Controller'))
        self.assertEqual(glyphs.parse_gamepad('Gamepad: type=ps5 south=cross east=circle west=square north=triangle name=x')[0],
                         'playstation')
        self.assertEqual(glyphs.parse_gamepad('Gamepad: type=switchpro south=b east=a west=y north=x name=Pro')[0], 'nintendo')
        self.assertEqual(glyphs.parse_gamepad('Gamepad: type=standard south=a east=b west=x north=y name=')[0], 'xbox')
        self.assertIsNone(glyphs.parse_gamepad('Gamepad: none\n'))
        self.assertIsNone(glyphs.parse_gamepad('Usage: gow3-probe boot.bin\n'))  # a probe without --gamepad

    def test_resolve(self):
        pad = 'Gamepad: type=switchpro south=b east=a west=y north=x name=Pro Controller'
        self.assertEqual(glyphs.resolve('auto', pad)[::2], ('nintendo', 'Pro Controller'))
        self.assertEqual(glyphs.resolve('auto', 'Gamepad: none'), ('playstation', None, ''))
        self.assertEqual(glyphs.resolve('xbox', pad), ('xbox', None, ''))  # chosen: its own letters
        self.assertEqual(glyphs.resolve('off', pad)[0], 'playstation')
        self.assertEqual(glyphs.resolve('', pad)[0], 'playstation')


@unittest.skipUnless(Image, 'needs Pillow')
class GlyphPatchTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.game = Path(self.temp.name) / 'game'
        self.out = Path(self.temp.name) / 'out'
        (self.game / 'EXEC/WAD/PS4_LE').mkdir(parents=True)
        self.source = self.game / 'EXEC/WAD/PS4_LE/R_UI.WAD'
        self.source.write_bytes(wad())

    def test_patch_keeps_every_size_and_only_touches_glyph_pixels(self):
        original = wad()
        data = bytearray(original)
        self.assertEqual(glyphs.patch(data, 'xbox'), 4)
        self.assertEqual(len(data), len(original))
        self.assertEqual(list(glyphs.entries(data)), list(glyphs.entries(original)))
        for offset, size, name in glyphs.entries(original):
            before, after = original[offset:offset + 0x40 + size], data[offset:offset + 0x40 + size]
            if name[:10] in glyphs.TEXTURES and name.endswith('.dds'):
                pixels = before.find(b'DDS ') + 128
                self.assertEqual(before[:pixels], after[:pixels], name)  # header and DDS header
                self.assertNotIn(b'\xee' * 64, after[pixels:], name)      # every level redrawn
                Image.open(io.BytesIO(bytes(after[pixels - 128:]))).load()
            else:
                self.assertEqual(before, after, name)

    def test_remap_and_style_change_the_pixels(self):
        drawn = []
        for style, pad_map in (('xbox', ''), ('nintendo', ''), ('xbox', 'cross=b')):
            data = wad()
            glyphs.patch(data, style, None, pad_map)
            drawn.append(bytes(data))
        self.assertEqual(len(set(drawn)), 3)

    def test_a_texture_of_another_size_is_left_alone(self):
        data = bytearray(texture('TX7FF61BF7.CC842B4F.dds', 128, 128, 'DXT1', length=1000))
        before = bytes(data)
        with mock.patch('builtins.print') as log:
            self.assertEqual(glyphs.patch(data, 'xbox'), 0)
        self.assertIn('left as it is', log.call_args.args[0])
        self.assertEqual(bytes(data), before)

    def test_encode_rejects_a_length_it_cannot_fill(self):
        image = Image.new('RGBA', (64, 64))
        self.assertEqual(len(glyphs.encode(image, 'DXT5', 4096)), 4096)
        self.assertEqual(len(glyphs.encode(image, 'DXT1', 2048 + 512)), 2560)
        with self.assertRaises(ValueError):
            glyphs.encode(image, 'DXT5', 4000)

    def test_build_writes_copies_and_leaves_the_game_alone(self):
        before = self.source.read_bytes()
        mounts = glyphs.build(self.game, self.out, 'xbox')
        self.assertEqual(list(mounts), ['/app0/EXEC/WAD/PS4_LE/R_UI.WAD'])  # no R_PERM.WAD in this game
        copy = mounts['/app0/EXEC/WAD/PS4_LE/R_UI.WAD']
        self.assertEqual(copy, self.out / 'glyphs/R_UI.WAD')
        self.assertEqual(copy.stat().st_size, len(before))
        self.assertNotEqual(copy.read_bytes(), before)
        self.assertEqual(self.source.read_bytes(), before)

    def test_build_reuses_the_copies_until_something_changes(self):
        copy = glyphs.build(self.game, self.out, 'xbox')['/app0/EXEC/WAD/PS4_LE/R_UI.WAD']
        with mock.patch.object(glyphs, 'patch', side_effect=AssertionError('redrawn')):
            self.assertEqual(glyphs.build(self.game, self.out, 'xbox'), {'/app0/EXEC/WAD/PS4_LE/R_UI.WAD': copy})
        first = copy.read_bytes()
        glyphs.build(self.game, self.out, 'xbox', None, 'cross=b')
        self.assertNotEqual(copy.read_bytes(), first)
        glyphs.build(self.game, self.out, 'nintendo')
        self.assertNotEqual(copy.read_bytes(), first)

    def test_playstation_and_off_leave_no_copies(self):
        glyphs.build(self.game, self.out, 'xbox')
        self.assertEqual(glyphs.build(self.game, self.out, 'playstation'), {})
        self.assertFalse((self.out / 'glyphs').exists())

    def test_a_wad_without_glyphs_is_not_mounted(self):
        (self.game / 'EXEC/WAD/PS4_LE/R_PERM.WAD').write_bytes(entry('WAD_R_Perm', tag=0x14) + entry('MAT_x', b'1234'))
        self.assertEqual(list(glyphs.build(self.game, self.out, 'xbox')), ['/app0/EXEC/WAD/PS4_LE/R_UI.WAD'])
        self.assertEqual(list(glyphs.build(self.game, self.out, 'xbox')), ['/app0/EXEC/WAD/PS4_LE/R_UI.WAD'])


class RunGlyphsTest(unittest.TestCase):
    """run.py: the option is off unless GOW3_GLYPH_STYLE asks for it."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.out = Path(self.temp.name) / 'out'
        (self.out / 'glyphs').mkdir(parents=True)
        (self.out / 'glyphs/R_UI.WAD').write_bytes(b'old')

    def test_off_runs_nothing_and_removes_old_copies(self):
        for env in ({}, {'GOW3_GLYPH_STYLE': 'off'}, {'GOW3_GLYPH_STYLE': 'playstation'}):
            (self.out / 'glyphs').mkdir(exist_ok=True)
            with mock.patch.object(run.subprocess, 'run') as started, mock.patch.object(glyphs, 'build') as build:
                self.assertEqual(run.glyph_mounts(env, Path('probe'), Path('game'), self.out), ([], None))
            started.assert_not_called()
            build.assert_not_called()
            self.assertFalse((self.out / 'glyphs').exists())

    def test_auto_asks_the_probe_and_mounts_the_copies(self):
        pad = mock.Mock(stdout='Gamepad: type=xboxone south=a east=b west=x north=y name=Xbox Controller\n')
        copies = {'/app0/EXEC/WAD/PS4_LE/R_UI.WAD': self.out / 'glyphs/R_UI.WAD'}
        env = {'GOW3_GLYPH_STYLE': 'auto', 'GOW3_PAD_MAP': 'cross=b'}
        with mock.patch.object(run.subprocess, 'run', return_value=pad) as started, \
                mock.patch.object(glyphs, 'build', return_value=copies) as build:
            arguments, style = run.glyph_mounts(env, Path('probe'), Path('game'), self.out)
        self.assertEqual(started.call_args.args[0], ['probe', '--gamepad'])
        self.assertEqual(build.call_args.args[2:], ('xbox', {'south': 'a', 'east': 'b', 'west': 'x', 'north': 'y'}, 'cross=b'))
        self.assertEqual(arguments, ['--mount', f'/app0/EXEC/WAD/PS4_LE/R_UI.WAD={self.out / "glyphs/R_UI.WAD"}'])
        self.assertEqual(style, 'xbox')

    def test_a_chosen_style_does_not_ask_the_probe(self):
        with mock.patch.object(run.subprocess, 'run') as started, mock.patch.object(glyphs, 'build', return_value={}) as build:
            self.assertEqual(run.glyph_mounts({'GOW3_GLYPH_STYLE': 'nintendo'}, Path('probe'), Path('game'), self.out), ([], None))
        started.assert_not_called()
        self.assertEqual(build.call_args.args[2:], ('nintendo', None, ''))

    def test_a_failure_starts_the_game_with_its_own_glyphs(self):
        with mock.patch.object(run.subprocess, 'run', side_effect=OSError('no probe')):
            self.assertEqual(run.glyph_mounts({'GOW3_GLYPH_STYLE': 'auto'}, Path('probe'), Path('game'), self.out), ([], None))
        with mock.patch.object(glyphs, 'build', side_effect=ValueError('bad WAD')):
            self.assertEqual(run.glyph_mounts({'GOW3_GLYPH_STYLE': 'xbox'}, Path('probe'), Path('game'), self.out), ([], None))


if __name__ == '__main__':
    unittest.main()
