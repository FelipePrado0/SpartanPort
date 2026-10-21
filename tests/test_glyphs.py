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
    image = Image.new('RGBA', (4, 4), (255, 0, 255, 255))
    header = io.BytesIO()
    image.save(header, 'DDS', pixel_format=fourcc)
    dds = bytearray(header.getvalue()[:128])
    struct.pack_into('<II', dds, 12, height, width)
    for level in range(0 if length else levels):
        blocks = -(-max(1, width >> level) // 4) * -(-max(1, height >> level) // 4)
        length += blocks * (16 if fourcc == 'DXT5' else 8)
    return entry(name, b'\7\0\1\0' + name.encode().ljust(0x48, b'\0') + dds + b'\xee' * length)


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

    def test_entity_count_marker_carries_no_data(self):
        # level WADs: the marker's size field is a count; the next entry follows its header at once
        data = entry('WAD_Oly150', b'\0' * 8, tag=0x14) + entry('EntityCount', tag=0)[:0x40]
        data = bytearray(data[:-4] + struct.pack('<I', 86) + entry('SBP_OLY150', b'\1' * 40) + entry('GroupEnd', tag=3))
        struct.pack_into('<I', data, 0x40 + 16 + 4, 86)  # the count where the size goes
        self.assertEqual([name for _o, _s, name in glyphs.entries(data)], ['WAD_Oly150', 'EntityCount', 'SBP_OLY150', 'GroupEnd'])

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
        self.assertEqual(set(button for form, names in glyphs.TEXTURES.values() if form not in glyphs.LYRE_FORMS
                             for button in ([names] if isinstance(names, str) else names)) - set(glyphs.DEFAULT_MAP), set())
        self.assertEqual(set(glyphs.LYRE_DISCS) - set(glyphs.DEFAULT_MAP), set())

    def test_key_map_moves_the_key(self):
        keys = glyphs.key_buttons('Cross = Q, square=Left Shift,l2=G,nonsense=Y,circle=')
        self.assertEqual((keys['cross'], keys['square'], keys['l2']), ('Q', 'Left Shift', 'G'))
        self.assertEqual(keys['circle'], 'Left Shift')  # empty: the default
        self.assertEqual(glyphs.key_buttons('l3=None')['l3'], 'Z')
        self.assertEqual(glyphs.key_buttons(''), {**glyphs.DEFAULT_KEYS, **glyphs.STICK_KEYS})
        self.assertEqual(set(glyphs.DEFAULT_KEYS) - set(glyphs.DEFAULT_MAP), {'up'})  # the keyboard-only D-pad icon

    def test_rage_on_one_input_draws_its_key_on_l3_and_r3(self):
        keys = glyphs.key_buttons('l3=None,r3=None,rage=Mouse Middle,grab=C')
        self.assertEqual((keys['l3'], keys['r3']), ('Mouse Middle', 'Mouse Middle'))
        keys = glyphs.key_buttons('l3=None,r3=V')  # no rage key: an unbound L3 keeps the default
        self.assertEqual((keys['l3'], keys['r3']), ('Z', 'V'))

    def test_key_caps(self):
        self.assertEqual(glyphs.glyph('Space', 'keyboard'), ('plate', 'SPACE', glyphs.INK))
        self.assertEqual(glyphs.glyph('Left Shift', 'keyboard')[:2], ('plate', 'SHIFT'))
        self.assertEqual(glyphs.glyph('e', 'keyboard')[:2], ('key', 'E'))
        self.assertEqual(glyphs.glyph('a', 'keyboard')[:2], ('key', 'A'))  # a key, not the south button
        self.assertEqual(glyphs.glyph('F10', 'keyboard')[:2], ('plate', 'F10'))
        self.assertEqual(glyphs.glyph('Up', 'keyboard')[:2], ('key', 'dpup'))
        self.assertEqual(glyphs.glyph('Keypad 4', 'keyboard')[:2], ('plate', 'NUM 4'))
        self.assertEqual(glyphs.glyph('Return', 'keyboard')[:2], ('plate', 'ENTER'))
        self.assertEqual(glyphs.glyph('Mouse Left', 'keyboard')[:2], ('plate', 'LMB'))
        self.assertEqual(glyphs.glyph('Mouse Back', 'keyboard')[:2], ('key', 'M4'))

    def test_gamepad_line(self):
        line = 'CPU: complete\nGamepad: type=xboxone south=a east=b west=x north=y name=Xbox One Controller\n'
        self.assertEqual(glyphs.parse_gamepad(line),
                         ('xbox', {'south': 'a', 'east': 'b', 'west': 'x', 'north': 'y'}, 'Xbox One Controller'))
        self.assertEqual(glyphs.parse_gamepad('Gamepad: type=ps5 south=cross east=circle west=square north=triangle name=x')[0],
                         'playstation')
        self.assertEqual(glyphs.parse_gamepad('Gamepad: type=switchpro south=b east=a west=y north=x name=Pro')[0], 'nintendo')
        self.assertEqual(glyphs.parse_gamepad('Gamepad: type=standard south=a east=b west=x north=y name=')[0], 'xbox')
        self.assertIsNone(glyphs.parse_gamepad('Gamepad: none\n'))
        self.assertIsNone(glyphs.parse_gamepad('Usage: spartanport-probe boot.bin\n'))  # a probe without --gamepad

    def test_resolve(self):
        pad = 'Gamepad: type=switchpro south=b east=a west=y north=x name=Pro Controller'
        self.assertEqual(glyphs.resolve('auto', pad)[::2], ('nintendo', 'Pro Controller'))
        self.assertEqual(glyphs.resolve('auto', 'Gamepad: none'), ('keyboard', None, ''))  # no gamepad: the keys
        self.assertEqual(glyphs.resolve('keyboard', pad), ('keyboard', None, ''))
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

    def test_lyre_discs_are_repainted_in_place(self):
        original = (texture('TX2CDFA5F8.F2C4FECB.dds', 2048, 2048, 'DXT1', 12)
                    + texture('TX4158CBF9.70725051.dds', 2048, 2048, 'DXT1', 12)
                    + texture('TX0C09B6DB.7485CA2D.dds', 2048, 2048, 'DXT1', 12)
                    + texture('TXF489D97F.2CBA0C02.dds', 2048, 2048, 'DXT5', 12)
                    + texture('TXEACA5E71.B9CB5700.dds', 512, 512, 'DXT1', 10)
                    + texture('TXBD54B283.5551218B.dds', 2048, 2048, 'DXT1', 12)
                    + texture('TXCEE0EE2B.BC09F12D.dds', 2048, 2048, 'DXT1', 12))
        keyboard = bytearray(original)
        self.assertEqual(glyphs.patch(keyboard, 'keyboard'), 0)  # the keys are the player's: left alone
        self.assertEqual(keyboard, original)
        copies = []
        for style in ('xbox', 'nintendo'):
            data = bytearray(original)
            self.assertEqual(glyphs.patch(data, style), 7)
            self.assertEqual(len(data), len(original))
            self.assertEqual(list(glyphs.entries(data)), list(glyphs.entries(original)))
            for offset, size, name in glyphs.entries(original):
                end = offset + 0x40 + size
                dds = original.find(b'DDS ', offset + 0x40, end)
                self.assertEqual(data[offset:dds + 128], original[offset:dds + 128], name)
                width = struct.unpack_from('<I', original, dds + 16)[0]
                corner = glyphs.LYRE_CORNER if width == 2048 else tuple(v // 4 for v in glyphs.LYRE_CORNER)
                # top level: blocks outside the corner are untouched, blocks inside are redrawn
                blocks, block = width // 4, 16 if original[dds + 84:dds + 88] == b'DXT5' else 8
                for by in range(blocks):
                    row = dds + 128 + by * blocks * block
                    for bx in range(blocks):
                        inside = corner[0] <= bx * 4 < corner[2] and corner[1] <= by * 4 < corner[3]
                        same = data[row + bx * block:row + (bx + 1) * block] == original[row + bx * block:row + (bx + 1) * block]
                        if not inside:
                            self.assertTrue(same, (name, bx, by))
                self.assertNotEqual(data[dds + 128:end], original[dds + 128:end], name)
                image = Image.open(io.BytesIO(bytes(data[dds:end])))
                image.load()
                self.assertEqual(image.size, (width, width))
            copies.append(bytes(data))
        self.assertEqual(len(set(copies)), 2)

    def test_encode_region_touches_only_changed_blocks(self):
        original = texture('TX2CDFA5F8.F2C4FECB.dds', 64, 64, 'DXT1', 7)
        data = bytearray(original)
        dds = original.find(b'DDS ')
        before = Image.open(io.BytesIO(bytes(original[dds:]))).convert('RGB')
        before.load()
        painted = before.copy()
        painted.paste((255, 255, 255), (8, 8, 16, 16))  # four blocks of the 16 x 16 region at (0, 0)
        glyphs.encode_region(data, dds, len(original), painted, 'DXT1', (0, 0, 16, 16), before)
        self.assertEqual(len(data), len(original))
        top = dds + 128
        changed = [(i - top) // 8 for i in range(top, top + 16 * 16 * 8, 8) if data[i:i + 8] != original[i:i + 8]]
        self.assertEqual(changed, [2 * 16 + 2, 2 * 16 + 3, 3 * 16 + 2, 3 * 16 + 3])
        # the smaller levels: only the blocks the white square reaches, nothing beyond the region
        rest = [i for i in range(top + 16 * 16 * 8, len(original), 8) if data[i:i + 8] != original[i:i + 8]]
        self.assertTrue(0 < len(rest) <= 12, rest)  # 3 at level 1, then the tiny levels
        self.assertEqual(data[top + 16 * 16 * 8 + 2 * 8 * 8:top + 16 * 16 * 8 + 8 * 8 * 8],
                         original[top + 16 * 16 * 8 + 2 * 8 * 8:top + 16 * 16 * 8 + 8 * 8 * 8])  # level 1, rows 2+

    def test_remap_and_style_change_the_pixels(self):
        drawn = []
        for style, pad_map, key_map in (('xbox', '', ''), ('nintendo', '', ''), ('xbox', 'cross=b', ''),
                                        ('keyboard', '', ''), ('keyboard', '', 'cross=Q,l1=G')):
            data = wad()
            glyphs.patch(data, style, None, pad_map, key_map)
            drawn.append(bytes(data))
        self.assertEqual(len(set(drawn)), 5)

    def test_controller_diagram_follows_the_gamepad_style(self):
        original = texture('TXCE352DC5.12345678.dds', 1024, 512, 'DXT5', 3)
        copies = []
        for style in ('xbox', 'nintendo'):
            data = bytearray(original)
            self.assertEqual(glyphs.patch(data, style), 1)
            self.assertEqual(list(glyphs.entries(data)), list(glyphs.entries(original)))
            pixels = original.find(b'DDS ') + 128
            self.assertEqual(data[:pixels], original[:pixels])
            self.assertNotEqual(data[pixels:], original[pixels:])
            image = Image.open(io.BytesIO(bytes(data[pixels - 128:])))
            image.load()
            self.assertEqual(image.size, (1024, 512))
            copies.append(bytes(data))
        self.assertNotEqual(*copies)

    def test_keyboard_keeps_the_original_controller_diagram(self):
        original = texture('TXCE352DC5.12345678.dds', 1024, 512, 'DXT5')
        data = bytearray(original)
        self.assertEqual(glyphs.patch(data, 'keyboard'), 0)
        self.assertEqual(data, original)

    def test_diagram_is_mounted_and_the_original_is_untouched(self):
        original = texture('TXCE352DC5.12345678.dds', 1024, 512, 'DXT5')
        self.source.write_bytes(original)
        for style in ('xbox', 'nintendo'):
            copies = glyphs.build(self.game, self.out, style)
            self.assertNotEqual(copies['/app0/EXEC/WAD/PS4_LE/R_UI.WAD'].read_bytes(), original)
            self.assertEqual(self.source.read_bytes(), original)
        self.assertEqual(glyphs.build(self.game, self.out, 'playstation'), {})

    def test_missing_diagram_art_keeps_the_prompts_and_is_retried_when_restored(self):
        original = wad() + texture('TXCE352DC5.12345678.dds', 1024, 512, 'DXT5')
        self.source.write_bytes(original)
        with mock.patch.object(glyphs, 'CONTROLLER_ART', self.out / 'missing-art'):
            copies = glyphs.build(self.game, self.out, 'xbox')
            incomplete = copies['/app0/EXEC/WAD/PS4_LE/R_UI.WAD'].read_bytes()
        copies = glyphs.build(self.game, self.out, 'xbox')
        self.assertNotEqual(copies['/app0/EXEC/WAD/PS4_LE/R_UI.WAD'].read_bytes(), incomplete)
        self.assertNotEqual(incomplete, original)
        self.assertEqual(self.source.read_bytes(), original)

    def test_the_keyboard_follows_only_the_key_map(self):
        copy = glyphs.build(self.game, self.out, 'keyboard')['/app0/EXEC/WAD/PS4_LE/R_UI.WAD']
        first = copy.read_bytes()
        with mock.patch.object(glyphs, 'patch', side_effect=AssertionError('redrawn')):
            glyphs.build(self.game, self.out, 'keyboard', None, 'cross=b')  # the gamepad map: no change
        glyphs.build(self.game, self.out, 'keyboard', None, '', 'cross=Q')
        self.assertNotEqual(copy.read_bytes(), first)
        self.assertEqual(self.source.read_bytes(), wad())

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
    """run.py: the option is off unless SPARTAN_GLYPH_STYLE asks for it."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.out = Path(self.temp.name) / 'out'
        (self.out / 'glyphs').mkdir(parents=True)
        (self.out / 'glyphs/R_UI.WAD').write_bytes(b'old')

    def test_off_runs_nothing_and_removes_old_copies(self):
        for env in ({}, {'SPARTAN_GLYPH_STYLE': 'off'}, {'SPARTAN_GLYPH_STYLE': 'playstation'}):
            (self.out / 'glyphs').mkdir(exist_ok=True)
            with mock.patch.object(run.subprocess, 'run') as started, mock.patch.object(glyphs, 'build') as build:
                self.assertEqual(run.glyph_mounts(env, Path('probe'), Path('game'), self.out), ([], None))
            started.assert_not_called()
            build.assert_not_called()
            self.assertFalse((self.out / 'glyphs').exists())

    def test_auto_asks_the_probe_and_mounts_the_copies(self):
        pad = mock.Mock(stdout='Gamepad: type=xboxone south=a east=b west=x north=y name=Xbox Controller\n')
        copies = {'/app0/EXEC/WAD/PS4_LE/R_UI.WAD': self.out / 'glyphs/R_UI.WAD'}
        env = {'SPARTAN_GLYPH_STYLE': 'auto', 'SPARTAN_PAD_MAP': 'cross=b'}
        with mock.patch.object(run.subprocess, 'run', return_value=pad) as started, \
                mock.patch.object(glyphs, 'build', return_value=copies) as build:
            arguments, style = run.glyph_mounts(env, Path('probe'), Path('game'), self.out)
        self.assertEqual(started.call_args.args[0], ['probe', '--gamepad'])
        self.assertEqual(build.call_args.args[2:], ('xbox', {'south': 'a', 'east': 'b', 'west': 'x', 'north': 'y'}, 'cross=b', ''))
        self.assertEqual(arguments, ['--mount', f'/app0/EXEC/WAD/PS4_LE/R_UI.WAD={self.out / "glyphs/R_UI.WAD"}'])
        self.assertEqual(style, 'xbox')

    def test_a_chosen_style_does_not_ask_the_probe(self):
        with mock.patch.object(run.subprocess, 'run') as started, mock.patch.object(glyphs, 'build', return_value={}) as build:
            self.assertEqual(run.glyph_mounts({'SPARTAN_GLYPH_STYLE': 'nintendo'}, Path('probe'), Path('game'), self.out), ([], None))
        started.assert_not_called()
        self.assertEqual(build.call_args.args[2:], ('nintendo', None, '', ''))

    def test_keyboard_passes_the_key_map(self):
        copies = {'/app0/EXEC/WAD/PS4_LE/R_UI.WAD': self.out / 'glyphs/R_UI.WAD'}
        env = {'SPARTAN_GLYPH_STYLE': 'keyboard', 'SPARTAN_KEY_MAP': 'cross=Q'}
        with mock.patch.object(run.subprocess, 'run') as started, \
                mock.patch.object(glyphs, 'build', return_value=copies) as build:
            self.assertEqual(run.glyph_mounts(env, Path('probe'), Path('game'), self.out)[1], 'keyboard')
        started.assert_not_called()
        self.assertEqual(build.call_args.args[2:], ('keyboard', None, '', 'cross=Q'))

    def test_auto_without_a_gamepad_shows_the_keys(self):
        pad = mock.Mock(stdout='Gamepad: none\n')
        with mock.patch.object(run.subprocess, 'run', return_value=pad), \
                mock.patch.object(glyphs, 'build', return_value={'/app0/x': self.out / 'x'}) as build:
            self.assertEqual(run.glyph_mounts({'SPARTAN_GLYPH_STYLE': 'auto', 'SPARTAN_KEY_MAP': 'square=G'}, Path('probe'),
                                              Path('game'), self.out)[1], 'keyboard')
        self.assertEqual(build.call_args.args[2:], ('keyboard', None, '', 'square=G'))

    def test_a_failure_starts_the_game_with_its_own_glyphs(self):
        with mock.patch.object(run.subprocess, 'run', side_effect=OSError('no probe')):
            self.assertEqual(run.glyph_mounts({'SPARTAN_GLYPH_STYLE': 'auto'}, Path('probe'), Path('game'), self.out), ([], None))
        with mock.patch.object(glyphs, 'build', side_effect=ValueError('bad WAD')):
            self.assertEqual(run.glyph_mounts({'SPARTAN_GLYPH_STYLE': 'xbox'}, Path('probe'), Path('game'), self.out), ([], None))


if __name__ == '__main__':
    unittest.main()


class KeyboardStickTest(unittest.TestCase):
    """The keyboard style also draws the movement and camera keys on the game's stick icons and the
    D-pad key on its D-pad icon; the gamepad styles leave those textures alone."""

    def test_stick_and_dpad_textures_are_keyboard_only(self):
        self.assertEqual({b for _f, b in glyphs.KEYBOARD_TEXTURES.values()}, {'lstick', 'rstick', 'up'})
        self.assertFalse(set(glyphs.KEYBOARD_TEXTURES) & set(glyphs.TEXTURES))
        self.assertEqual(glyphs.textures('xbox'), glyphs.TEXTURES)
        prompts = {name: entry for name, entry in glyphs.TEXTURES.items() if entry[0] not in glyphs.LYRE_FORMS}
        self.assertEqual(glyphs.textures('keyboard'), {**prompts, **glyphs.KEYBOARD_TEXTURES})  # the lyre keeps its letters

    def test_sticks_show_the_movement_and_camera_keys(self):
        keys = glyphs.key_buttons('')
        self.assertEqual(glyphs.glyph(keys['lstick'], 'keyboard')[:2], ('plate', 'WASD'))
        self.assertEqual(glyphs.glyph(keys['rstick'], 'keyboard')[:2], ('key', 'arrows'))

    def test_dpad_follows_the_weapon_key(self):
        self.assertEqual(glyphs.key_buttons('')['up'], 'I')
        self.assertEqual(glyphs.key_buttons('up=1')['up'], '1')

    def test_arrows_render(self):
        image = glyphs.render('inline', 'rstick', 'keyboard', None, '', 64, 64)
        self.assertEqual(image.size, (64, 64))
        self.assertGreater(len(set(image.getdata())), 4)


@unittest.skipUnless(Image, 'needs Pillow')
class AtlasAlignmentTest(unittest.TestCase):
    """The shoulder-button atlas (TXFB91136E for L2/L1, TX0FFF3CCB for R2/R1) places its prompts
    centered ~52 px from the midline (y=128), matching the in-game HUD quad mapping, rather than
    in the center of each 128 px half (y=64, which puts them 64 px from the midline)."""

    def test_atlas_prompts_are_centered_towards_midline(self):
        for style in ('xbox', 'nintendo', 'keyboard'):
            for pair in (('r2', 'r1'), ('l2', 'l1')):
                img = glyphs.render('atlas', pair, style, None, '', 256, 256)
                self.assertEqual(img.size, (256, 256))
                w, _h = img.size
                top_y = [y for y in range(128) if any(img.getpixel((x, y))[0] > 30 for x in range(w))]
                bot_y = [y for y in range(128, 256) if any(img.getpixel((x, y))[0] > 30 for x in range(w))]
                self.assertTrue(top_y, f"{style} {pair} top half should contain pixels")
                self.assertTrue(bot_y, f"{style} {pair} bottom half should contain pixels")
                top_center = (min(top_y) + max(top_y)) / 2
                bot_center = (min(bot_y) + max(bot_y)) / 2
                # In each half, the prompt should be ~52 px from midline (y=128), within 3 px tolerance.
                # Without the offset, distance was 64 px, causing prompts to touch the HUD border.
                self.assertAlmostEqual(128 - top_center, 52, delta=3)
                self.assertAlmostEqual(bot_center - 128, 52, delta=3)

    def test_qte_face_buttons_remain_centered(self):
        img = glyphs.render('qte', 'cross', 'xbox', None, '', 128, 128)
        self.assertEqual(img.size, (128, 128))
        qw, qh = img.size
        qx = [x for x in range(qw) if any(img.getpixel((x, y))[0] > 30 for y in range(qh))]
        qy = [y for y in range(qh) if any(img.getpixel((x, y))[0] > 30 for x in range(qw))]
        self.assertAlmostEqual((min(qx) + max(qx)) / 2, 63.5, delta=2)
        self.assertAlmostEqual((min(qy) + max(qy)) / 2, 63.5, delta=2)

