import sys
import unittest

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import gow3_launcher_win as launcher  # noqa: E402


class ControlMapTest(unittest.TestCase):
    def test_new_cheats_default_to_disabled(self):
        self.assertEqual(launcher.INI_DEFAULTS['cheat_double_jumps'], '0')
        self.assertEqual(launcher.INI_DEFAULTS['cheat_very_hard'], '0')
        self.assertIn('cheat_double_jumps', launcher.INI_FLAGS)
        self.assertIn('cheat_very_hard', launcher.INI_FLAGS)

    def test_pipeline_binary_cache_switch(self):
        settings = {**launcher.APP_DEFAULTS}
        self.assertTrue(settings['pipeline_binary_cache'])
        self.assertEqual(launcher.game_environment(settings)['GOW3_PIPELINE_BINARIES'], '1')
        settings['pipeline_binary_cache'] = False
        self.assertEqual(launcher.game_environment(settings)['GOW3_PIPELINE_BINARIES'], '0')

    def test_changed_buttons_in_table_order(self):
        controls = {'key': {'circle': ' Left Shift ', 'cross': 'Q', 'square': ''}, 'pad': {'cross': 'b'}}
        self.assertEqual(launcher.control_map(controls, 'key'), 'cross=Q,circle=Left Shift')
        self.assertEqual(launcher.control_map(controls, 'pad'), 'cross=b')

    def test_rejects_separators_unknown_buttons_and_bad_shapes(self):
        controls = {'key': {'cross': 'a,b', 'circle': 'x=y', 'jump': 'Q', 'square': 5}}
        self.assertEqual(launcher.control_map(controls, 'key'), '')
        self.assertEqual(launcher.control_map([], 'key'), '')
        self.assertEqual(launcher.control_map({}, 'pad'), '')

    def test_only_changes_from_the_defaults_are_kept(self):
        keys = {b: k for b, k, _p in launcher.CONTROLS}
        keys.update(cross='q', circle='LEFT SHIFT', square='  ')
        self.assertEqual(launcher.control_changes(keys, 'key'), {'cross': 'q'})
        pads = {b: p or '' for b, _k, p in launcher.CONTROLS}
        pads.update(cross='b', l2='a')
        self.assertEqual(launcher.control_changes(pads, 'pad'), {'cross': 'b'})  # L2/R2: triggers only

    def test_touchpad_sides_are_separate_buttons(self):
        rows = {b: (k, p) for b, k, p in launcher.CONTROLS}
        self.assertNotIn('touchpad', rows)
        self.assertEqual(rows['touchpad_left'], ('Tab', None))  # photo mode: L1 + the right-click button
        self.assertEqual(rows['touchpad_right'], ('Backspace', 'back'))  # pause menu
        self.assertEqual(launcher.control_map({'pad': {'touchpad_right': 'guide'}}, 'pad'), 'touchpad_right=guide')
        self.assertEqual(launcher.control_map({'key': {'touchpad_right': 'Delete'}}, 'key'), 'touchpad_right=Delete')

    def test_saved_touchpad_key_moves_to_the_left_side(self):
        # Settings saved before the sides were split named the left click 'touchpad'.
        self.assertEqual(launcher.control_map({'key': {'touchpad': 'T'}}, 'key'), 'touchpad_left=T')
        self.assertEqual(launcher.control_map({'key': {'touchpad': 'T', 'touchpad_left': 'Y'}}, 'key'), 'touchpad_left=Y')
        self.assertEqual(launcher.control_values({'key': {'touchpad': 'T'}}, 'key'), {'touchpad_left': 'T'})
        self.assertEqual(launcher.control_values({'pad': {'touchpad': 'misc1'}}, 'pad'), {})  # no gamepad box for it

    def test_both_styles_name_every_default_gamepad_button(self):
        for labels in launcher.PAD_STYLES.values():
            self.assertTrue(all(p in labels for _b, _k, p in launcher.CONTROLS if p))
            self.assertEqual(len(set(labels.values())), len(labels))

    def test_environment(self):
        settings = {**launcher.APP_DEFAULTS, 'controls': {'key': {'cross': 'Q'}}}
        env = launcher.game_environment(settings)
        self.assertEqual(env['GOW3_KEY_MAP'], 'cross=Q')
        self.assertNotIn('GOW3_PAD_MAP', env)
        self.assertEqual(env['GOW3_PARALLEL_WARMUP'], '1')
        self.assertEqual(env['GOW3_BACKGROUND_WARMUP'], '1')
        self.assertEqual(env['GOW3_PERF_DIAG'], '0')
        self.assertNotIn('GOW3_FRAME_CAPTURE_KEY', {k for k in env if k not in __import__('os').environ})
        self.assertEqual(launcher.game_environment({**settings, 'frame_capture_key': True})['GOW3_FRAME_CAPTURE_KEY'], '1')
        # Graphics options live in gow3.ini (run.py and the in-game menu), not in the environment.
        for name in ('GOW3_FULLSCREEN', 'GOW3_PRESENT_MODE', 'GOW3_FPS_LIMIT', 'GOW3_ASYNC_SHADERS',
                     'GOW3_DEFERRED_READBACK', 'GOW3_STALE_READBACK', 'GOW3_FSR1', 'GOW3_RCAS'):
            self.assertNotIn(name, {k for k in env if k not in __import__('os').environ})
        old = launcher.game_environment({**settings, 'extra_env': 'GOW3_DRAW_PIPE=0'})
        self.assertNotIn('GOW3_DRAW_PIPE', old)  # the old free-text field is ignored
        self.assertEqual(launcher.game_environment({**settings, 'parallel_warmup': False})['GOW3_PARALLEL_WARMUP'], '0')
        self.assertEqual(launcher.game_environment({**settings, 'background_warmup': False})['GOW3_BACKGROUND_WARMUP'], '0')

    def test_settings_keys_kept(self):
        # The redesign hides some options (player name) but keeps every saved setting working.
        for key in ('game_dir', 'user_dir', 'mods_dir', 'patches_dir', 'language', 'player_name', 'hdr',
                    'draw_pipe', 'readbacks', 'background_warmup', 'parallel_warmup', 'controls', 'pad_style'):
            self.assertIn(key, launcher.APP_DEFAULTS)
        env = launcher.game_environment({**launcher.APP_DEFAULTS, 'player_name': ' Kratos '})
        self.assertEqual(env['GOW3_USER_NAME'], 'Kratos')
        for key in ('display_mode', 'fps_limit', 'render_resolution', 'engine_fps', 'rcas_strength'):
            self.assertIn(key, launcher.INI_DEFAULTS)

    def test_graphics_defaults_match_the_game(self):
        d = launcher.INI_DEFAULTS
        self.assertEqual((d['display_mode'], d['vsync'], d['fps_limit'], d['engine_fps']), ('windowed', '1', '0', '120'))
        self.assertEqual([v for v, _t in launcher.FPS_LIMITS], ['30', '60', '120', '240', '0'])
        self.assertEqual((d['fsr1'], d['rcas'], d['rcas_strength'], d['frames_queued']), ('0', '1', '75', '1'))
        self.assertEqual([v for v, _t in launcher.FRAMES_QUEUED], ['1', '2', '0'])
        self.assertNotIn('GOW3_FRAMES_AHEAD', launcher.game_environment({**launcher.APP_DEFAULTS, 'frames_ahead': '2'}))


class GameFilesTest(unittest.TestCase):
    def test_missing_game_files(self):
        import tempfile
        from pathlib import Path
        with tempfile.TemporaryDirectory() as folder:
            folder = Path(folder)
            (folder / 'sce_module').mkdir()
            (folder / 'sce_module/libSceFios2.prx').write_bytes(b'x')
            self.assertEqual(launcher.missing_game_files(folder),
                             ['sce_sys/param.sfo', 'sce_module/libc.prx'])
            (folder / 'sce_sys').mkdir()
            (folder / 'sce_sys/param.sfo').write_bytes(b'x')
            (folder / 'sce_module/libc.prx').write_bytes(b'x')
            self.assertEqual(launcher.missing_game_files(folder), [])


class GpuChoiceTest(unittest.TestCase):
    def test_parse_gpu_list(self):
        text = '0\tAMD Radeon(TM) Graphics\tintegrated\nbroken line\n1\tNVIDIA GeForce RTX 2070 SUPER\tdiscrete\n'
        self.assertEqual(launcher.parse_gpu_list(text),
                         [('0', 'AMD Radeon(TM) Graphics', 'integrated'),
                          ('1', 'NVIDIA GeForce RTX 2070 SUPER', 'discrete')])
        self.assertEqual(launcher.parse_gpu_list(''), [])

    def test_list_gpus_failures_leave_only_automatic(self):
        import subprocess
        from unittest import mock
        with mock.patch.object(launcher, 'find_tool', return_value=None):
            self.assertEqual(launcher.list_gpus(), [])
        with mock.patch.object(launcher, 'find_tool', return_value='gow3-gpu-capabilities.exe'):
            with mock.patch.object(launcher.subprocess, 'run', side_effect=subprocess.TimeoutExpired('x', 5)):
                self.assertEqual(launcher.list_gpus(), [])
            with mock.patch.object(launcher.subprocess, 'run', side_effect=OSError):
                self.assertEqual(launcher.list_gpus(), [])
            done = mock.Mock(returncode=0, stdout='0\tRX 6700 XT\tdiscrete\n')
            with mock.patch.object(launcher.subprocess, 'run', return_value=done) as run:
                self.assertEqual(launcher.list_gpus(), [('0', 'RX 6700 XT', 'discrete')])
                self.assertEqual(run.call_args.kwargs['timeout'], 5)

    def test_gpu_choice_reaches_the_game(self):
        from unittest import mock
        with mock.patch.dict(launcher.os.environ, {'GOW3_GPU_ID': '7'}):
            env = launcher.game_environment({**launcher.APP_DEFAULTS, 'game_dir': 'x'})
            self.assertIsNone(env.get('GOW3_GPU_ID'))  # Automatic wins over an inherited value
            env = launcher.game_environment({**launcher.APP_DEFAULTS, 'game_dir': 'x', 'gpu_id': '1'})
            self.assertEqual(env['GOW3_GPU_ID'], '1')
            env = launcher.game_environment({**launcher.APP_DEFAULTS, 'game_dir': 'x', 'gpu_id': 'bad'})
            self.assertIsNone(env.get('GOW3_GPU_ID'))
        self.assertEqual(launcher.APP_DEFAULTS['gpu_id'], '')


class ReleaseVersionTest(unittest.TestCase):
    def test_god_of_war_release_is_newer_than_the_inherited_tags(self):
        self.assertEqual(launcher.VERSION, '2.1.7')
        self.assertGreater(launcher.version_tuple(launcher.VERSION), launcher.version_tuple('windows-v1.5'))
        self.assertGreater(launcher.version_tuple('windows-v2.2'), launcher.version_tuple(launcher.VERSION))

    def test_first_start_is_in_english(self):
        # The translations are partial; 'System' stays a choice in Advanced.
        self.assertEqual(launcher.APP_DEFAULTS['ui_language'], 'en')


if __name__ == '__main__':
    unittest.main()
