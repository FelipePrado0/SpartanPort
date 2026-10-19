import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from paths import ROOT  # adds scripts/ to sys.path
from controller_mappings import (Capture, FILENAME, STEPS, State, configure_environment, edit_mapping,
                                 load_mapping, mapping_bindings, mapping_line, save_mapping)

GUID = '0300457e790000000600000000000000'


class CaptureTest(unittest.TestCase):
    def setUp(self):
        self.rest = State((False,)*12, (0, 0, 0, 0, -32768, 0), (0,))
        self.capture = Capture(self.rest)

    def held(self, state):
        return [self.capture.feed(state) for _ in range(3)][-1]

    def test_release_gate_stability_and_noise(self):
        noise = State(self.rest.buttons, (2000, -1000, 0, 0, -32768, 0), (0,))
        self.assertIsNone(self.held(noise))
        pressed = State((False, True)+(False,)*10, self.rest.axes, (0,))
        self.assertIsNone(self.capture.feed(pressed))
        self.assertIsNone(self.capture.feed(self.rest))  # brief press does not count
        self.assertEqual(self.held(pressed), 'b1')
        self.assertEqual(self.capture.index, 1)
        for _ in range(8):
            self.assertIsNone(self.capture.feed(pressed))  # cannot map the held button twice
        self.held(self.rest)
        self.assertFalse(self.capture.waiting_release)
        self.assertIsNone(self.held(pressed))
        self.assertIn('already mapped', self.capture.error)
        self.assertEqual(self.capture.index, 1)

    def test_hats_diagonals_axes_triggers_and_inversion(self):
        self.capture.index = 12  # D-pad up
        self.assertIsNone(self.held(State(self.rest.buttons, self.rest.axes, (3,))))
        self.assertEqual(self.held(State(self.rest.buttons, self.rest.axes, (1,))), 'h0.1')
        for index, axes, expected in (
                (16, (-32768, 0, 0, 0, -32768, 0), 'a0~'),
                (17, (0, 32767, 0, 0, -32768, 0), 'a1'),
                (6, (0, 0, 0, 0, 32767, 0), 'a4'),
                (7, (0, 0, 0, 0, -32768, -32768), '-a5')):
            capture = Capture(self.rest)
            capture.index = index
            state = State(self.rest.buttons, axes, (0,))
            self.assertEqual([capture.feed(state) for _ in range(3)][-1], expected)

    def test_stick_prompt_ignores_buttons_and_endpoint_trigger(self):
        self.capture.index = 16
        pressed = State((True,)+(False,)*11, self.rest.axes, (1,))
        self.assertIsNone(self.held(pressed))
        trigger = State(self.rest.buttons, (0, 0, 0, 0, 32767, 0), (0,))
        self.assertIsNone(self.held(trigger))

    def test_full_axes_conflict_but_opposite_halves_can_be_shared(self):
        self.capture.index = 16
        self.capture.bindings['dpup'] = '-a0'
        self.assertIsNone(self.held(State(self.rest.buttons, (32767, 0, 0, 0, -32768, 0), (0,))))
        self.assertIn('already mapped', self.capture.error)
        self.capture.index = 7
        self.capture.bindings = {'lefttrigger': '+a5'}
        self.capture.candidate, self.capture.frames = None, 0
        self.assertEqual(self.held(State(self.rest.buttons, (0, 0, 0, 0, -32768, -32768), (0,))), '-a5')

    def test_back_skip_and_cross_requirement(self):
        with self.assertRaises(ValueError):
            self.capture.skip()
        self.capture.bindings = {'a': 'b0', 'b': 'b1'}
        self.capture.index = 2
        self.capture.back()
        self.assertEqual(self.capture.bindings, {'a': 'b0'})
        self.assertEqual(self.capture.index, 1)
        self.capture.skip()
        self.assertEqual(self.capture.index, 2)
        self.capture.index = len(STEPS)-1
        self.capture.skip()
        self.assertTrue(self.capture.done)

    def test_start_with_a_held_button_is_rejected(self):
        with self.assertRaises(ValueError):
            Capture(State((True,), (), ()))


class PersistenceTest(unittest.TestCase):
    def test_edit_one_input_preserves_other_inputs_and_unknown_sdl_fields(self):
        original = mapping_line(GUID, 'USB', {'a': 'b2', 'b': 'b1', 'leftx': 'a0'}, 'Windows') + 'guide:b10,type:standard,'
        edited = edit_mapping(original, GUID, 'USB', 'Windows', 'a', 'b5')
        self.assertEqual(mapping_bindings(edited), {'a': 'b5', 'b': 'b1', 'leftx': 'a0'})
        self.assertIn('guide:b10,', edited)
        self.assertIn('type:standard,', edited)
        self.assertEqual(edited.count('a:'), 1)
        cleared = edit_mapping(edited, GUID, 'USB', 'Windows', 'a', None)
        self.assertEqual(mapping_bindings(cleared), {'b': 'b1', 'leftx': 'a0'})
        self.assertIn('guide:b10,', cleared)

    def test_partial_profile_can_start_with_any_button(self):
        line = edit_mapping(None, GUID, 'USB', 'Windows', 'x', 'b3')
        self.assertEqual(mapping_bindings(line), {'x': 'b3'})
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / FILENAME
            save_mapping(path, line)
            self.assertEqual(load_mapping(path, GUID, 'Windows'), line)
            self.assertIsNone(load_mapping(path, GUID, 'Linux'))
            save_mapping(path, edit_mapping(line, GUID, 'USB', 'Windows', 'b', 'b1'))
            self.assertEqual(mapping_bindings(load_mapping(path, GUID, 'Windows')), {'x': 'b3', 'b': 'b1'})

    def test_platform_specific_saved_mapping_wins_over_generic(self):
        generic = f'{GUID},USB,a:b0,'
        specific = mapping_line(GUID, 'USB', {'a': 'b2'}, 'Windows')
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / FILENAME
            path.write_text(specific + '\n' + generic + '\n', encoding='utf-8')
            self.assertEqual(load_mapping(path, GUID, 'Windows'), specific)
            self.assertEqual(load_mapping(path, GUID, 'Linux'), generic)

    def test_replace_only_this_guid_and_platform_preserving_comments(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / FILENAME
            windows = mapping_line(GUID, 'USB,\npad', {'a': 'b2'}, 'Windows')
            linux = mapping_line(GUID, 'USB pad', {'a': 'b3'}, 'Linux')
            path.write_text('# user note\n' + linux + '\n', encoding='utf-8')
            save_mapping(path, windows)
            replacement = mapping_line(GUID, 'USB pad', {'a': 'b1', 'leftx': 'a0~'}, 'Windows')
            save_mapping(path, replacement)
            text = path.read_text(encoding='utf-8')
            self.assertIn('# user note\n', text)
            self.assertIn(linux, text)
            self.assertIn(replacement, text)
            self.assertNotIn(windows, text)
            self.assertEqual(len(list(Path(directory).iterdir())), 1)

    def test_failed_commit_preserves_profile_and_cleans_temporary_file(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / FILENAME
            path.write_text('old profile\n', encoding='utf-8')
            with patch('controller_mappings.os.replace', side_effect=PermissionError):
                with self.assertRaises(PermissionError):
                    save_mapping(path, mapping_line(GUID, 'USB', {'a': 'b2'}, 'Windows'))
            self.assertEqual(path.read_text(), 'old profile\n')
            self.assertEqual(list(Path(directory).iterdir()), [path])

    def test_invalid_mapping_does_not_write(self):
        for guid, bindings in (('invalid', {'a': 'b0'}), (GUID, {}), (GUID, {'a': 'b0\n'})):
            with self.assertRaises(ValueError):
                mapping_line(guid, 'USB', bindings, 'Windows')

    def test_explicit_overrides_have_priority_and_restarts_do_not_duplicate(self):
        with tempfile.TemporaryDirectory() as directory:
            local = mapping_line(GUID, 'USB', {'a': 'b0'}, 'Windows')
            external_file = mapping_line(GUID, 'USB', {'a': 'b1'}, 'Windows')
            external_hint = mapping_line(GUID, 'USB', {'a': 'b2'}, 'Windows')
            (Path(directory) / FILENAME).write_text(local + '\n', encoding='utf-8')
            custom = Path(directory) / 'custom.txt'
            custom.write_text(external_file, encoding='utf-8')
            env = {'SDL_GAMECONTROLLERCONFIG_FILE': str(custom), 'SDL_GAMECONTROLLERCONFIG': external_hint}
            configure_environment(env, directory)
            self.assertEqual(env['SDL_GAMECONTROLLERCONFIG'].splitlines(), [local, external_file, external_hint])
            before = dict(env)
            configure_environment(env, directory)
            self.assertEqual(env, before)
            self.assertEqual(env['SDL_GAMECONTROLLERCONFIG_FILE'], str(custom))

    def test_no_profile_leaves_environment_untouched(self):
        with tempfile.TemporaryDirectory() as directory:
            env = {'SDL_GAMECONTROLLERCONFIG': 'user value'}
            configure_environment(env, directory)
            self.assertEqual(env, {'SDL_GAMECONTROLLERCONFIG': 'user value'})

    def test_reserved_xinput_override_is_not_discarded(self):
        with tempfile.TemporaryDirectory() as directory:
            local = mapping_line(GUID, 'USB', {'a': 'b0'}, 'Windows')
            (Path(directory) / FILENAME).write_text(local, encoding='utf-8')
            custom = 'xinput,Custom Xbox,a:b1,b:b0,platform:Windows,'
            env = {'SDL_GAMECONTROLLERCONFIG': custom}
            configure_environment(env, directory)
            self.assertEqual(env['SDL_GAMECONTROLLERCONFIG'].splitlines(), [local, custom])


if __name__ == '__main__':
    unittest.main()
