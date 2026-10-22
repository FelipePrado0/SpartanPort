import os
from pathlib import Path
import struct
import unittest

from paths import ROOT  # noqa: F401
import pause_menu
from pause_menu import Script, decode, entries

GAME = Path(os.environ['SPARTAN_GAME_DIR']) if os.environ.get('SPARTAN_GAME_DIR') else None


@unittest.skipUnless(GAME and (GAME / pause_menu.WAD).is_file(), 'needs SPARTAN_GAME_DIR')
class NativeCheats(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = bytearray((GAME / pause_menu.WAD).read_bytes())
        pause_menu.patch(cls.data, language='en')
        cls.base = next(o + 64 for o, size, name in entries(cls.data) if name == 'SMI_R_UI')
        cls.script = Script(cls.data, cls.base)

    def block(self, record, kind, triggers=None):
        ref = next(v for k, v, t in self.script.blocks(record) if k == kind and (triggers is None or t == triggers))
        end = next((v for v in self.script.code_refs() if v > ref), self.script.symtab - self.script.lit)
        return decode(self.data, self.base + self.script.lit + ref, self.base + self.script.lit + end)

    def target(self, name, triggers):
        return next(int.from_bytes(arg, 'big') for _, op, arg in self.block(self.script.one(name), 3, triggers) if op == 0x47)

    def test_cheats_is_between_game_settings_and_audio(self):
        row = self.script.one('HLNativeCheats')
        self.assertEqual(self.target('HLGSettings', pause_menu.DOWN), row['at'] - 4)
        self.assertEqual(self.target('HLAudio', pause_menu.UP), row['at'] - 4)
        self.assertEqual(self.target('HLNativeCheats', pause_menu.UP), self.script.one('HLGSettings')['at'] - 4)
        self.assertEqual(self.target('HLNativeCheats', pause_menu.DOWN), self.script.one('HLAudio')['at'] - 4)

    def test_every_cheat_has_native_selection_and_initial_value(self):
        page = self.script.one('NativeCheats')
        initial = self.block(page, 1)
        queries = [int.from_bytes(arg, 'big') for _, op, arg in initial if op == 0x13]
        for index in range(8):
            self.assertIn(0xf00 + index, queries)
            row = self.script.one(f'NativeCheat{index}')
            self.assertEqual(row['parent'], page['at'] - 4)
            entry = self.block(row, 1)
            texts = [self.script.string(self.script.lit + int.from_bytes(arg, 'big')) for _, op, arg in entry if op == 0x16]
            self.assertIn(f'menuItem{index - (index >= 6)}Select', texts)
            toggle = next(t for k, v, t in self.script.blocks(row) if k == 3 and 0xe9 in t)
            self.assertIn(0xf10 + index, [int.from_bytes(arg, 'big') for _, op, arg in self.block(row, 3, toggle) if op == 0x13])

    def test_qte_precedes_multipliers_and_last_row_scrolls(self):
        self.assertTrue(b'Automatic QTE\0' in self.data)
        self.assertEqual(self.target('NativeCheat7', pause_menu.DOWN), self.script.one('NativeMultipliersLink')['at'] - 4)
        self.assertEqual(self.target('NativeMultipliersLink', pause_menu.UP), self.script.one('NativeCheat7')['at'] - 4)
        entry = self.block(self.script.one('NativeMultipliersLink'), 1)
        texts = [self.script.string(self.script.lit + int.from_bytes(arg, 'big')) for _, op, arg in entry if op == 0x16]
        self.assertIn('Infinite Magic', texts)
        self.assertIn('Multipliers', texts)
        self.assertIn('menuItem7Select', texts)
        for _, op, arg in entry:
            if op == 0x09:
                self.assertLessEqual(arg[1], 0x18)

    def test_multipliers_stays_visible_and_qte_uses_a_visible_value_slot(self):
        for name in ('NativeCheat0', 'NativeCheat6', 'NativeCheat7', 'NativeMultipliersLink'):
            entry = self.block(self.script.one(name), 1)
            texts = [self.script.string(self.script.lit + int.from_bytes(arg, 'big')) for _, op, arg in entry if op == 0x16]
            self.assertIn('Multipliers', texts, name)
            if name != 'NativeCheat0':
                self.assertIn('Automatic QTE', texts, name)
        entry = self.block(self.script.one('NativeCheat7'), 1)
        texts = [self.script.string(self.script.lit + int.from_bytes(arg, 'big')) for _, op, arg in entry if op == 0x16]
        self.assertIn('[ArrowSelectionL][PauseMenu:optSet6][ArrowSelectionR]', texts)

    def test_toggling_scrolled_cheats_updates_their_visible_row(self):
        for index in range(8):
            row = self.script.one(f'NativeCheat{index}')
            trigger = next(t for k, v, t in self.script.blocks(row) if k == 3 and 0xe9 in t)
            instructions = self.block(row, 3, trigger)
            slot = index - (index >= 6)
            setters = [arg[1] for _, op, arg in instructions if op == 0x09]
            self.assertEqual(setters, [0x11 + slot, 9 + slot], f'cheat {index}')
            self.assertIn(0xf10 + index, [int.from_bytes(arg, 'big') for _, op, arg in instructions if op == 0x13])

    def test_six_multipliers_have_native_bars_and_directional_controls(self):
        page = self.script.one('NativeMultipliers')
        queries = [int.from_bytes(arg, 'big') for _, op, arg in self.block(page, 1) if op == 0x13]
        for index in range(6):
            self.assertIn(0xf20 + index, queries)
            row = self.script.one(f'NativeMultiplier{index}')
            self.assertEqual(row['parent'], page['at'] - 4)
            for trigger, function in (([0xe6, 0xf2], 0xf30), ([0xe2, 0xee], 0xf40)):
                calls = [int.from_bytes(arg, 'big') for _, op, arg in self.block(row, 3, trigger) if op == 0x13]
                self.assertIn(function + index, calls)
        objects = {name: (o, size) for o, size, name in entries(self.data) if size}
        for index in range(4, 10):
            offset, size = objects[f'gosliderbar{index}']
            self.assertEqual(size, 148)
            self.assertEqual(struct.unpack_from('<H', self.data, offset + 64 + 0x76)[0], index)

    def test_custom_pages_preserve_the_native_footer(self):
        for name in ['HLNativeCheats', 'NativeCheats', 'NativeMultipliers', 'NativeMultipliersLink',
                     'NativeMultiplierReset', *[f'NativeCheat{i}' for i in range(8)],
                     *[f'NativeMultiplier{i}' for i in range(6)]]:
            for kind, ref, triggers in self.script.blocks(self.script.one(name)):
                self.assertNotIn((0x09, b'\xe0\x1d'),
                                 [(op, arg) for _, op, arg in self.block(self.script.one(name), kind, triggers)], name)
        for text in (b'Confirm: open.\0', b'Left/Right: adjust. Changes are saved.\0',
                     b'Show the health of the last enemy hit.\0'):
            self.assertNotIn(text, self.data)

    def test_multiplier_bars_are_separate_from_numbers_and_original_sliders(self):
        original = (GAME / pause_menu.WAD).read_bytes()
        originals = {name: (o, size) for o, size, name in entries(original) if size}
        objects = {name: (o, size) for o, size, name in entries(self.data) if size}
        first = self.block(self.script.one('NativeMultipliers'), 1)
        texts = [self.script.string(self.script.lit + int.from_bytes(arg, 'big')) for _, op, arg in first if op == 0x16]
        for row in range(6):
            self.assertIn(f'sliderBar{row + 4}', texts)
            offset, _ = objects[f'gosliderbar{row + 4}']
            # Dedicated controls end left of the original value column, including the selection scale.
            x = struct.unpack_from('<f', self.data, offset + 64 + 0x64)[0]
            scale = struct.unpack_from('<f', self.data, offset + 64 + 0x40)[0]
            self.assertLess(x, 403.0)
            self.assertLess(scale, 1.0)
        for row in range(4):
            name = f'gosliderbar{row}'
            before, size = originals[name]
            after, _ = objects[name]
            self.assertEqual(original[before + 64:before + 64 + size], self.data[after + 64:after + 64 + size])

    def test_every_state_reference_and_relative_call_stays_in_the_script(self):
        starts = {r['at'] - 4 for r in self.script.records}
        for name in ['HLNativeCheats', 'NativeCheats', 'NativeMultipliers', 'NativeMultiplierReset',
                     *[f'NativeCheat{i}' for i in range(8)], *[f'NativeMultiplier{i}' for i in range(6)]]:
            for kind, ref, triggers in self.script.blocks(self.script.one(name)):
                for at, op, arg in self.block(self.script.one(name), kind, triggers):
                    if op == 0x47:
                        self.assertIn(int.from_bytes(arg, 'big'), starts)
                    if op == 0x4a:
                        target = at + 5 + struct.unpack('>i', arg)[0]
                        self.assertTrue(self.base + self.script.lit <= target < self.base + self.script.symtab)


if __name__ == '__main__':
    unittest.main()
