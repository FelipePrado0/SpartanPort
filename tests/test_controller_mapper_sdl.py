"""Run with GOW3_TEST_SDL3 pointing to SDL3.dll/libSDL3.so; no physical pad/display needed."""
import ctypes as c
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
from controller_mapper import MapperWindow, SDLJoysticks
from controller_mappings import Capture, FILENAME, mapping_line, save_mapping


class VirtualJoystick(c.Structure):
    # SDL_VirtualJoystickDesc (SDL3); version is sizeof this structure, per SDL_INIT_INTERFACE.
    _fields_ = [('version', c.c_uint32), ('type', c.c_uint16), ('padding', c.c_uint16),
                ('vendor', c.c_uint16), ('product', c.c_uint16), ('axes', c.c_uint16),
                ('buttons', c.c_uint16), ('balls', c.c_uint16), ('hats', c.c_uint16),
                ('touchpads', c.c_uint16), ('sensors', c.c_uint16), ('padding2', c.c_uint16*2),
                ('button_mask', c.c_uint32), ('axis_mask', c.c_uint32), ('name', c.c_char_p),
                ('touchpad_desc', c.c_void_p), ('sensor_desc', c.c_void_p), ('userdata', c.c_void_p),
                ('callbacks', c.c_void_p*8)]


@unittest.skipUnless(os.environ.get('GOW3_TEST_SDL3'), 'set GOW3_TEST_SDL3 to test with real SDL3')
class SDLIntegrationTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.old_env = {key: os.environ.get(key) for key in
                        ('SDL_GAMECONTROLLERCONFIG', 'SDL_GAMECONTROLLERCONFIG_FILE')}
        for key in self.old_env:
            os.environ.pop(key, None)
        self.addCleanup(self.restore_env)
        self.backend = self.new_backend()
        self.addCleanup(lambda: self.backend.close())
        self.attach()

    def restore_env(self):
        for key, value in self.old_env.items():
            if value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = value

    def new_backend(self):
        backend = SDLJoysticks(ROOT, self.directory.name, library=os.environ['GOW3_TEST_SDL3'])
        for name, result, args in (
                ('SDL_AttachVirtualJoystick', c.c_uint32, [c.POINTER(VirtualJoystick)]),
                ('SDL_DetachVirtualJoystick', c.c_bool, [c.c_uint32]),
                ('SDL_SetJoystickVirtualButton', c.c_bool, [c.c_void_p, c.c_int, c.c_bool]),
                ('SDL_SetJoystickVirtualAxis', c.c_bool, [c.c_void_p, c.c_int, c.c_int16]),
                ('SDL_SetJoystickVirtualHat', c.c_bool, [c.c_void_p, c.c_int, c.c_uint8]),
        ):
            function = getattr(backend.sdl, name)
            function.restype, function.argtypes = result, args
            backend.functions[name] = function
        return backend

    def attach(self):
        desc = VirtualJoystick(version=c.sizeof(VirtualJoystick), type=4, vendor=0xfaca,
                               product=0xf001, axes=6, buttons=12, hats=1, name=b'GOW3 mapper test')
        self.ident = self.backend.call('SDL_AttachVirtualJoystick', c.byref(desc))
        self.assertNotEqual(self.ident, 0, self.backend.error())
        device = next(item for item in self.backend.devices() if item['id'] == self.ident)
        self.backend.select(device)

    def test_raw_capture_gamepad_preview_inversion_and_analog_trigger(self):
        backend = self.backend
        self.assertFalse(backend.device['mapped'])  # Reproduce an unrecognized raw joystick.
        backend.call('SDL_SetJoystickVirtualAxis', backend.pad, 4, -32768)
        capture = Capture(backend.state())
        backend.call('SDL_SetJoystickVirtualButton', backend.pad, 7, True)
        self.assertEqual([capture.feed(backend.state()) for _ in range(3)][-1], 'b7')
        line = mapping_line(backend.device['guid'], backend.device['name'],
                            {**capture.bindings, 'leftx': 'a0~', 'lefttrigger': 'a4', 'dpup': 'h0.1'}, backend.platform)
        backend.begin_preview(line)
        buttons, axes = backend.preview_state()
        self.assertIn('Cross / A', buttons)
        self.assertEqual(axes[4], 0)
        backend.call('SDL_SetJoystickVirtualButton', backend.pad, 7, False)
        backend.call('SDL_SetJoystickVirtualAxis', backend.pad, 0, -32768)
        backend.call('SDL_SetJoystickVirtualAxis', backend.pad, 4, 32767)
        backend.call('SDL_SetJoystickVirtualHat', backend.pad, 0, 1)
        buttons, axes = backend.preview_state()
        self.assertNotIn('Cross / A', buttons)
        self.assertIn('D-pad up', buttons)
        self.assertEqual(axes[0], 32767)
        self.assertEqual(axes[4], 32767)

    def test_saved_mapping_loads_and_explicit_user_file_wins(self):
        backend = self.backend
        guid, name, platform = backend.device['guid'], backend.device['name'], backend.platform
        save_mapping(Path(self.directory.name) / FILENAME, mapping_line(guid, name, {'a': 'b7'}, platform))
        backend.call('SDL_DetachVirtualJoystick', self.ident)
        backend.close()
        self.backend = backend = self.new_backend()
        self.attach()
        backend.preview = backend.call('SDL_OpenGamepad', self.ident)
        self.assertTrue(backend.preview, backend.error())
        backend.call('SDL_SetJoystickVirtualButton', backend.pad, 7, True)
        self.assertIn('Cross / A', backend.preview_state()[0])
        backend.call('SDL_DetachVirtualJoystick', self.ident)
        backend.close()
        custom = Path(self.directory.name) / 'custom.txt'
        custom.write_text(mapping_line(guid, name, {'a': 'b3'}, platform), encoding='utf-8')
        os.environ['SDL_GAMECONTROLLERCONFIG_FILE'] = str(custom)
        self.backend = backend = self.new_backend()
        self.attach()
        backend.preview = backend.call('SDL_OpenGamepad', self.ident)
        self.assertTrue(backend.preview, backend.error())
        backend.call('SDL_SetJoystickVirtualButton', backend.pad, 7, True)
        self.assertNotIn('Cross / A', backend.preview_state()[0])
        backend.call('SDL_SetJoystickVirtualButton', backend.pad, 3, True)
        self.assertIn('Cross / A', backend.preview_state()[0])

    def test_disconnect_reports_error_instead_of_reading_a_stale_pad(self):
        self.backend.call('SDL_DetachVirtualJoystick', self.ident)
        with self.assertRaisesRegex(OSError, 'disconnected'):
            self.backend.state()

    def window(self):
        import tkinter as tk
        root = tk.Tk()
        root.withdraw()
        self.addCleanup(root.destroy)
        with patch('controller_mapper.SDLJoysticks', return_value=self.backend):
            window = MapperWindow(root, ROOT, self.directory.name)
        window.window.withdraw()
        self.addCleanup(window.close)
        window.choice.current(next(i for i, item in enumerate(window.devices) if item['id'] == self.ident))
        window.choose()
        return window

    def capture_button(self, window, target, button):
        window.begin_edit(target)
        self.backend.call('SDL_SetJoystickVirtualButton', self.backend.pad, button, True)
        for _ in range(3):
            if window.after_id is not None:
                window.window.after_cancel(window.after_id)
            window.poll()
        self.backend.call('SDL_SetJoystickVirtualButton', self.backend.pad, button, False)

    def test_rows_save_out_of_order_and_reload_without_losing_other_bindings(self):
        window = self.window()
        path = Path(self.directory.name) / FILENAME
        self.capture_button(window, 'x', 7)  # Start with Square; Cross need not be mapped first.
        self.assertIn('x:b7,', path.read_text())
        self.assertFalse(window.closed)
        self.capture_button(window, 'b', 2)
        self.capture_button(window, 'x', 4)  # Correct just one row.
        saved = path.read_text()
        self.assertIn('b:b2,', saved)
        self.assertIn('x:b4,', saved)
        self.assertNotIn('x:b7,', saved)
        window.refresh()
        self.assertEqual(window.bindings, {'x': 'b4', 'b': 'b2'})
        self.assertEqual(window.rows['x']['value'].get(), 'Button 5')
        self.assertEqual(window.rows['b']['value'].get(), 'Button 3')

    def test_cancel_capture_and_close_keep_already_saved_rows(self):
        window = self.window()
        path = Path(self.directory.name) / FILENAME
        self.capture_button(window, 'x', 7)
        previous = path.read_text()
        window.begin_edit('b')
        self.backend.call('SDL_SetJoystickVirtualButton', self.backend.pad, 2, True)
        window.capture.feed(self.backend.state())  # Not stable yet.
        window.cancel_capture()
        window.close()
        self.assertEqual(path.read_text(), previous)
        self.assertFalse(self.backend.ready)

    def test_clear_only_one_row_and_allow_an_empty_profile(self):
        window = self.window()
        path = Path(self.directory.name) / FILENAME
        self.capture_button(window, 'a', 7)
        self.capture_button(window, 'b', 2)
        window.clear_binding('a')
        self.assertEqual(window.bindings, {'b': 'b2'})
        self.assertNotIn('a:b7,', path.read_text())
        window.clear_binding('b')
        self.assertEqual(window.bindings, {})
        self.assertIsNone(self.backend.preview)
        self.assertTrue(path.exists())

    def test_failed_save_keeps_previous_profile_and_row_values(self):
        window = self.window()
        path = Path(self.directory.name) / FILENAME
        self.capture_button(window, 'a', 7)
        previous = path.read_text()
        with patch('controller_mapper.save_mapping', side_effect=PermissionError('denied')), \
                patch.object(window.messagebox, 'showerror') as error:
            self.capture_button(window, 'x', 2)
        error.assert_called_once()
        self.assertEqual(path.read_text(), previous)
        self.assertEqual(window.bindings, {'a': 'b7'})
        self.assertEqual(window.rows['x']['value'].get(), 'Not configured')
        self.assertIsNone(window.capture)

    def test_editing_an_existing_sdl_mapping_preserves_guide_and_other_buttons(self):
        line = mapping_line(self.backend.device['guid'], 'USB', {'a': 'b7'}, self.backend.platform) + 'guide:b10,'
        self.backend.begin_preview(line)
        window = self.window()
        self.assertEqual(window.bindings['a'], 'b7')
        self.capture_button(window, 'b', 2)
        text = (Path(self.directory.name) / FILENAME).read_text()
        self.assertIn('a:b7,', text)
        self.assertIn('b:b2,', text)
        self.assertIn('guide:b10,', text)

    def test_parent_destroy_closes_sdl_without_saving(self):
        window = self.window()
        window.window.destroy()
        self.assertTrue(window.closed)
        self.assertFalse(self.backend.ready)
        self.assertFalse((Path(self.directory.name) / FILENAME).exists())


if __name__ == '__main__':
    unittest.main()
