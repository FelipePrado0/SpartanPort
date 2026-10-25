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
    def test_graphics_anisotropy_and_controls_have_restart_and_reset_confirmation(self):
        for page, apply, reset, confirm, no in (
                ('NativeGraphics', 'NativeGraphicsApply', 'NativeGraphicsReset', 'NativeGraphicsRestart', 'NativeGraphicsRestartNo'),
                ('PauseControls', 'NativeControlsApply', 'NativeControlsReset', 'NativeControlsRestart', 'NativeControlsRestartNo')):
            self.script.one(apply)
            self.script.one(reset)
            entry = self.block(self.script.one(confirm), 1)
            target = next(int.from_bytes(arg, 'big') for _, op, arg in entry if op == 0x47)
            self.assertEqual(target, self.script.one(no)['at'] - 4)
        self.script.one('NativeGraphic4')
        for index in range(6):
            row = self.script.one(f'NativeControl{index}')
            self.assertEqual(row['parent'], self.script.one('PauseControls')['at'] - 4)
            for trigger, function in (([0xe6, 0xf2], 0xfaa), ([0xe2, 0xee], 0xfb1)):
                self.assertIn(function + index, [int.from_bytes(arg, 'big') for _, op, arg
                                               in self.block(row, 3, trigger) if op == 0x13])
        for name in ('HLContDis', 'HLFreeCamera', 'HLFlightControl'):
            self.script.one(name)
        self.assertEqual(self.target('HLFlightControl', pause_menu.DOWN), self.script.one('NativeControl0')['at'] - 4)
        self.assertEqual(self.target('HLContDis', pause_menu.UP), self.script.one('NativeControlsApply')['at'] - 4)
        end = self.block(self.script.one('PauseControls'), 2)
        self.assertIn('sliderBar12', [self.script.string(self.script.lit + int.from_bytes(arg, 'big'))
                                    for _, op, arg in end if op == 0x16])

    def test_new_controls_labels_and_graphics_restore_are_bilingual(self):
        data = bytearray((GAME / pause_menu.WAD).read_bytes())
        pause_menu.patch(data, language='pt')
        for text in ('Tipo de controle', 'Filtragem anisotrópica', 'Restaurar padrões dos controles', 'Aplicar e reiniciar'):
            self.assertIn(text.encode('utf-8') + b'\0', data)
        for name in ('NativeGraphicsReset', 'NativeGraphicsApply'):
            entry = self.block(self.script.one(name), 1)
            texts = [self.script.string(self.script.lit + int.from_bytes(arg, 'big')) for _, op, arg in entry if op == 0x16]
            self.assertIn('Graphics Settings', texts)
            self.assertIn('Anisotropic Filtering', texts)
        starts = {r['at'] - 4 for r in self.script.records}
        for name in self.script.by_name:
            if not name or not name.startswith(('NativeControl', 'NativeGraphicsRestart')):
                continue
            for kind, ref, triggers in self.script.blocks(self.script.one(name)):
                for at, op, arg in self.block(self.script.one(name), kind, triggers):
                    if op == 0x47:
                        self.assertIn(int.from_bytes(arg, 'big'), starts, name)

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

    def test_graphics_and_cheats_follow_game_settings_before_audio(self):
        row = self.script.one('HLNativeCheats')
        graphics = self.script.one('HLNativeGraphics')
        self.assertEqual(self.target('HLGSettings', pause_menu.DOWN), graphics['at'] - 4)
        self.assertEqual(self.target('HLNativeGraphics', pause_menu.UP), self.script.one('HLGSettings')['at'] - 4)
        self.assertEqual(self.target('HLNativeGraphics', pause_menu.DOWN), row['at'] - 4)
        self.assertEqual(self.target('HLAudio', pause_menu.UP), row['at'] - 4)
        self.assertEqual(self.target('HLNativeCheats', pause_menu.UP), graphics['at'] - 4)
        self.assertEqual(self.target('HLNativeCheats', pause_menu.DOWN), self.script.one('HLAudio')['at'] - 4)

    def test_graphics_initializes_only_its_four_controls_and_returns_to_its_link(self):
        page = self.script.one('NativeGraphics')
        calls = [int.from_bytes(arg, 'big') for _, op, arg in self.block(page, 1) if op == 0x13]
        for index in range(4):
            self.assertIn(0xf70 + index, calls)
            self.assertEqual(self.script.one(f'NativeGraphic{index}')['parent'], page['at'] - 4)
        self.assertIn(0xfa3, calls)
        self.assertNotIn(0xfc0, calls)
        self.assertEqual(self.target('NativeGraphics', [0xeb]), self.script.one('HLNativeGraphics')['at'] - 4)
        for name in ('NativeGraphic0', 'NativeGraphic3'):
            index = int(name[-1])
            for trigger, function in (([0xe6, 0xf2], 0xf80), ([0xe2, 0xee], 0xf90)):
                self.assertIn(function + index, [int.from_bytes(arg, 'big') for _, op, arg in
                                                self.block(self.script.one(name), 3, trigger) if op == 0x13])

    def test_graphics_defaults_require_confirmation_and_cancel_preserves_the_selection(self):
        self.assertEqual(self.target('NativeGraphicsReset', pause_menu.CONFIRM),
                         self.script.one('NativeGraphicsConfirm')['at'] - 4)
        reset = self.script.one('NativeGraphicsReset')['at'] - 4
        for name in ('NativeGraphicsResetYes', 'NativeGraphicsResetNo'):
            self.assertEqual(self.target(name, pause_menu.CONFIRM), reset)
            calls = [int.from_bytes(arg, 'big') for _, op, arg in
                     self.block(self.script.one(name), 3, pause_menu.CONFIRM) if op == 0x13]
            self.assertEqual(0xfb0 in calls, name.endswith('Yes'))
        self.assertEqual(self.target('NativeGraphicsConfirm', [0xeb]), reset)

    def test_graphics_labels_follow_the_game_language(self):
        self.assertTrue(b'Graphics Settings\0' in self.data)
        data = bytearray((GAME / pause_menu.WAD).read_bytes())
        pause_menu.patch(data, language=17)
        for text in ('Configurações Gráficas', 'Restaurar padrões gráficos', 'Quadros enfileirados'):
            self.assertTrue(text.encode('utf-8') + b'\0' in data, text)

    def test_graphics_options_label_animation_and_confirmation_focus(self):
        entry = self.block(self.script.one('PauseOptions'), 1)
        assignments = {}
        for previous, instruction in zip(entry, entry[1:]):
            if previous[1] == 0x16 and instruction[1] == 0x09:
                assignments[instruction[2][1]] = self.script.string(
                    self.script.lit + int.from_bytes(previous[2], 'big'))
        self.assertEqual(assignments[3], 'Graphics Settings')
        self.assertEqual(assignments[4], 'Cheats')
        for name, index in (('HLNativeGraphics', 2), ('HLNativeCheats', 3)):
            for kind in (1, 2):
                texts = [self.script.string(self.script.lit + int.from_bytes(arg, 'big'))
                         for _, op, arg in self.block(self.script.one(name), kind) if op == 0x16]
                self.assertIn(f'menuItem{index}Select', texts)
        confirmation = self.block(self.script.one('NativeGraphicsConfirm'), 1)
        self.assertEqual(next(int.from_bytes(arg, 'big') for _, op, arg in confirmation if op == 0x47),
                         self.script.one('NativeGraphicsResetNo')['at'] - 4)

    def test_graphics_rows_wrap_and_only_strength_uses_a_slider(self):
        self.assertEqual(self.target('NativeGraphic0', pause_menu.UP),
                         self.script.one('NativeGraphicsApply')['at'] - 4)
        self.assertEqual(self.target('NativeGraphicsApply', pause_menu.DOWN),
                         self.script.one('NativeGraphic0')['at'] - 4)
        entry = self.block(self.script.one('NativeGraphics'), 1)
        texts = [self.script.string(self.script.lit + int.from_bytes(arg, 'big'))
                 for _, op, arg in entry if op == 0x16]
        self.assertEqual(texts.count('sliderBar7'), 3)  # hide, position, show
        self.assertIn('Restore Graphics Defaults', texts)
        for name in ('NativeGraphic1', 'NativeGraphic2'):
            trigger = next(t for k, v, t in self.script.blocks(self.script.one(name)) if k == 3 and 0xe9 in t)
            self.assertEqual(trigger, [0xe6, 0xe2, 0xee, 0xf2, 0xe9])

    def test_audio_clears_unused_rows_inherited_from_options(self):
        entry = self.block(self.script.one('PauseAudio'), 1)
        assignments = {}
        for previous, instruction in zip(entry, entry[1:]):
            if previous[1] == 0x16 and instruction[1] == 0x09:
                assignments[instruction[2][1]] = self.script.string(
                    self.script.lit + int.from_bytes(previous[2], 'big'))
        for local in (6, 7, 8, 14, 15, 16):
            self.assertEqual(assignments.get(local), '', f'Audio must clear local {local}')
        for local, text in enumerate(('[msg_A[4180]]', '[msg_B[4180]]', '[msg_C[4180]]', '[msg_D[4180]]'), 1):
            self.assertEqual(assignments[local], text)
        original = (GAME / pause_menu.WAD).read_bytes()
        base = next(o + 64 for o, size, name in entries(original) if name == 'SMI_R_UI')
        script = Script(original, base)
        ref = next(v for k, v, t in script.blocks(script.one('PauseAudio')) if k == 1)
        body = script.block_bytes(ref)
        before = decode(original, base + script.lit + ref, base + script.lit + ref + len(body))
        # The cleanup must not change the audio controls or its language-dependent branches.
        self.assertEqual([(op, arg) for _, op, arg in entry[18:] if op not in (0x47, 0x4a)],
                         [(op, arg) for _, op, arg in before if op not in (0x47, 0x4a)])

    def test_display_contains_eight_new_controls_and_keeps_its_original_rows(self):
        page = self.script.one('PauseDisplay')
        for index in range(8):
            self.assertEqual(self.script.one(f'NativeDisplay{index}')['parent'], page['at'] - 4)
        self.assertEqual(self.script.one('NativeDisplayApply')['parent'], page['at'] - 4)
        self.assertEqual(self.target('HLBrightness', pause_menu.UP), self.script.one('NativeDisplayApply')['at'] - 4)
        self.assertEqual(self.target('NativeDisplay0', pause_menu.UP),
                         next(r for r in self.script.by_name['HLSubtitleDis'] if r['parent'] == page['at'] - 4)['at'] - 4)
        for name in ('HLBrightness', 'HLScreenLocX', 'HLScreenLocY'):
            parents = {r['at'] - 4: r for r in self.script.records}
            parent = self.script.one(name)['parent']
            while parent != page['at'] - 4:
                parent = parents[parent]['parent']
            self.assertEqual(parent, page['at'] - 4)

    def test_display_scrolls_without_overwriting_original_setting_values(self):
        for index in range(8):
            entry = self.block(self.script.one(f'NativeDisplay{index}'), 1)
            self.assertTrue(any(op == 0x13 and int.from_bytes(arg, 'big') == 0xff0 + index for _, op, arg in entry))
            self.assertFalse(any(op == 0x09 and 0x11 <= arg[1] <= 0x14 for _, op, arg in entry))
            texts = [self.script.string(self.script.lit + int.from_bytes(arg, 'big'))
                     for _, op, arg in entry if op == 0x16]
            self.assertIn(f'menuItem{min(index + 4, 7)}Select', texts)
        self.assertEqual(self.target('NativeDisplayConfirm', [0xeb]), self.script.one('NativeDisplayApply')['at'] - 4)
        for name in ('NativeDisplayYes', 'NativeDisplayNo'):
            calls = [int.from_bytes(arg, 'big') for _, op, arg in
                     self.block(self.script.one(name), 3, pause_menu.CONFIRM) if op == 0x13]
            self.assertEqual(0xfcb in calls, name.endswith('Yes'))

    def test_graphics_does_not_add_an_informational_row(self):
        names = ['NativeGraphics', 'NativeGraphicsReset', *[f'NativeGraphic{i}' for i in range(4)]]
        for name in names:
            for kind, ref, triggers in self.script.blocks(self.script.one(name)):
                calls = [int.from_bytes(arg, 'big') for _, op, arg in self.block(self.script.one(name), kind, triggers)
                         if op == 0x13]
                self.assertNotIn(0xfc0, calls, name)

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

    def test_cheats_scrollbar_tracks_unpinned_rows_and_hides_on_exit(self):
        objects = {name: (o + 64, size) for o, size, name in entries(self.data) if size}
        self.assertTrue('gosliderbar11' in objects)
        mesh, _ = objects['MG_95eaf70d_0']
        xs = [struct.unpack_from('<f', self.data, mesh + 0x220 + i * 20)[0] for i in range(4)]
        self.assertAlmostEqual(max(xs) - min(xs), 2.98 * 7 / 8, places=4)
        times = []
        for name in ('NativeCheat0', 'NativeCheat7', 'NativeMultipliersLink'):
            code = self.block(self.script.one(name), 1)
            marker = next(i for i, (_, op, arg) in enumerate(code) if op == 0x16
                          and self.script.string(self.script.lit + int.from_bytes(arg, 'big')) == 'sliderBar11')
            times.append(struct.unpack('>f', code[marker + 4][2])[0])
        self.assertLess(times[0], times[1])
        self.assertEqual(times[1], times[2])
        exit_code = self.block(self.script.one('NativeCheats'), 2)
        self.assertIn((0x13, b'\x00\x59'), [(op, arg) for _, op, arg in exit_code])
        self.assertIn('sliderBar11', [self.script.string(self.script.lit + int.from_bytes(arg, 'big'))
                                    for _, op, arg in exit_code if op == 0x16])

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
                     *[f'NativeMultiplier{i}' for i in range(6)], 'HLNativeGraphics', 'NativeGraphics',
                     'NativeGraphicsReset', 'NativeGraphicsConfirm', 'NativeGraphicsResetYes',
                     'NativeGraphicsResetNo', *[f'NativeGraphic{i}' for i in range(4)],
                     *[f'NativeDisplay{i}' for i in range(8)], 'NativeDisplayApply',
                     'NativeDisplayConfirm', 'NativeDisplayYes', 'NativeDisplayNo']:
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
                     *[f'NativeCheat{i}' for i in range(8)], *[f'NativeMultiplier{i}' for i in range(6)],
                     'HLNativeGraphics', 'NativeGraphics', 'NativeGraphicsReset', 'NativeGraphicsConfirm',
                     'NativeGraphicsResetYes', 'NativeGraphicsResetNo', *[f'NativeGraphic{i}' for i in range(4)],
                     *[f'NativeDisplay{i}' for i in range(8)], 'NativeDisplayApply',
                     'NativeDisplayConfirm', 'NativeDisplayYes', 'NativeDisplayNo']:
            for kind, ref, triggers in self.script.blocks(self.script.one(name)):
                for at, op, arg in self.block(self.script.one(name), kind, triggers):
                    if op == 0x47:
                        self.assertIn(int.from_bytes(arg, 'big'), starts)
                    if op == 0x4a:
                        target = at + 5 + struct.unpack('>i', arg)[0]
                        self.assertTrue(self.base + self.script.lit <= target < self.base + self.script.symtab)


if __name__ == '__main__':
    unittest.main()
