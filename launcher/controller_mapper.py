# SPDX-License-Identifier: GPL-2.0-or-later
"""Edit individual SDL3 joystick bindings with the same mapping format used by the game."""
import ctypes as c
import ctypes.util
import os
from pathlib import Path
import sys

from controller_mappings import (Capture, FILENAME, STEPS, State, configure_environment,
                                 edit_mapping, load_mapping, mapping_bindings, save_mapping)


class GUID(c.Structure):
    _fields_ = [('data', c.c_ubyte * 16)]


class SDLJoysticks:
    """Small ctypes interface to the SDL3 already shipped with the port; no extra dependency."""
    def __init__(self, port, data_dir, library=None):
        self.pad = self.preview = None
        self.device = None
        self.ready = False
        if library is None:
            if sys.platform == 'win32':
                candidates = [Path(port) / 'bin/SDL3.dll', Path(port) / 'out/SDL3.dll',
                              Path(os.environ.get('MSYS2_ROOT', r'C:\msys64')) / 'clang64/bin/SDL3.dll']
                library = next((str(path) for path in candidates if path.is_file()), None)
            library = library or ctypes.util.find_library('SDL3')
        if not library:
            raise OSError('SDL3 was not found. Use a complete port installation or build the project first.')
        self.sdl = c.CDLL(str(library))
        self.functions = {}
        signatures = {
            'SDL_InitSubSystem': (c.c_bool, c.c_uint32),
            'SDL_QuitSubSystem': (None, c.c_uint32),
            'SDL_SetHintWithPriority': (c.c_bool, c.c_char_p, c.c_char_p, c.c_int),
            'SDL_GetHint': (c.c_char_p, c.c_char_p), 'SDL_ResetHint': (c.c_bool, c.c_char_p),
            'SDL_GetError': (c.c_char_p,), 'SDL_GetPlatform': (c.c_char_p,),
            'SDL_PumpEvents': (None,), 'SDL_UpdateJoysticks': (None,),
            'SDL_GetJoysticks': (c.POINTER(c.c_uint32), c.POINTER(c.c_int)),
            'SDL_free': (None, c.c_void_p),
            'SDL_IsGamepad': (c.c_bool, c.c_uint32),
            'SDL_GetJoystickNameForID': (c.c_char_p, c.c_uint32),
            'SDL_GetJoystickGUIDForID': (GUID, c.c_uint32),
            'SDL_GetGamepadMappingForGUID': (c.c_void_p, GUID),
            'SDL_OpenJoystick': (c.c_void_p, c.c_uint32),
            'SDL_CloseJoystick': (None, c.c_void_p),
            'SDL_JoystickConnected': (c.c_bool, c.c_void_p),
            'SDL_GetNumJoystickButtons': (c.c_int, c.c_void_p),
            'SDL_GetNumJoystickAxes': (c.c_int, c.c_void_p),
            'SDL_GetNumJoystickHats': (c.c_int, c.c_void_p),
            'SDL_GetJoystickButton': (c.c_bool, c.c_void_p, c.c_int),
            'SDL_GetJoystickAxis': (c.c_int16, c.c_void_p, c.c_int),
            'SDL_GetJoystickHat': (c.c_uint8, c.c_void_p, c.c_int),
            'SDL_AddGamepadMapping': (c.c_int, c.c_char_p),
            'SDL_OpenGamepad': (c.c_void_p, c.c_uint32),
            'SDL_CloseGamepad': (None, c.c_void_p),
            'SDL_GetGamepadButton': (c.c_bool, c.c_void_p, c.c_int),
            'SDL_GetGamepadAxis': (c.c_int16, c.c_void_p, c.c_int),
        }
        for name, (result, *args) in signatures.items():
            function = getattr(self.sdl, name)
            function.restype, function.argtypes = result, args
            self.functions[name] = function
        self.platform = self.call('SDL_GetPlatform').decode()
        env = dict(os.environ)
        configure_environment(env, data_dir)
        hints = {'SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS': '1'}
        if env.get('SDL_GAMECONTROLLERCONFIG'):
            hints['SDL_GAMECONTROLLERCONFIG'] = env['SDL_GAMECONTROLLERCONFIG']
        self.old_hints = {name: self.call('SDL_GetHint', name.encode()) for name in hints}
        try:
            for name, value in hints.items():
                self.call('SDL_SetHintWithPriority', name.encode(), value.encode(), 2)
            if not self.call('SDL_InitSubSystem', 0x2000):
                raise OSError(self.error())
            self.ready = True
        except Exception:
            self.close()
            raise

    def call(self, name, *args):
        return self.functions[name](*args)

    def error(self):
        return (self.call('SDL_GetError') or b'SDL error').decode('utf-8', 'replace')

    def pump(self):
        self.call('SDL_PumpEvents')
        self.call('SDL_UpdateJoysticks')

    def devices(self):
        self.pump()
        count = c.c_int()
        ids = self.call('SDL_GetJoysticks', c.byref(count))
        result = []
        try:
            for i in range(count.value):
                ident = ids[i]
                pad = self.call('SDL_OpenJoystick', ident)
                if not pad:
                    continue
                try:
                    guid = self.call('SDL_GetJoystickGUIDForID', ident)
                    result.append({
                        'id': ident, 'guid': bytes(guid.data).hex(),
                        'name': (self.call('SDL_GetJoystickNameForID', ident) or b'Controller').decode('utf-8', 'replace'),
                        'buttons': self.call('SDL_GetNumJoystickButtons', pad),
                        'axes': self.call('SDL_GetNumJoystickAxes', pad),
                        'hats': self.call('SDL_GetNumJoystickHats', pad),
                        'mapped': self.call('SDL_IsGamepad', ident),
                    })
                finally:
                    self.call('SDL_CloseJoystick', pad)
        finally:
            self.call('SDL_free', ids)
        return result

    def select(self, device):
        self.close_device()
        self.pad = self.call('SDL_OpenJoystick', device['id'])
        if not self.pad:
            raise OSError(self.error())
        self.device = device

    def state(self):
        self.pump()
        if not self.pad or not self.call('SDL_JoystickConnected', self.pad):
            raise OSError('Controller disconnected. Reconnect it and choose Refresh.')
        return State(
            tuple(self.call('SDL_GetJoystickButton', self.pad, i) for i in range(self.device['buttons'])),
            tuple(self.call('SDL_GetJoystickAxis', self.pad, i) for i in range(self.device['axes'])),
            tuple(self.call('SDL_GetJoystickHat', self.pad, i) for i in range(self.device['hats'])),
        )

    def current_mapping(self):
        guid = self.call('SDL_GetJoystickGUIDForID', self.device['id'])
        pointer = self.call('SDL_GetGamepadMappingForGUID', guid)
        if not pointer:
            return None
        try:
            line = c.string_at(pointer).decode('utf-8', 'replace')
            # SDL may return a shared/canonical GUID, such as an XInput mapping.
            # Save an editable copy scoped to the selected physical device.
            return self.device['guid'] + ',' + line.split(',', 1)[1]
        finally:
            self.call('SDL_free', pointer)

    def begin_preview(self, line):
        self.end_preview()
        if self.call('SDL_AddGamepadMapping', line.encode()) < 0:
            raise OSError(self.error())
        self.preview = self.call('SDL_OpenGamepad', self.device['id'])
        if not self.preview:
            raise OSError(self.error())

    def preview_state(self):
        self.pump()
        names = ('Cross / A', 'Circle / B', 'Square / X', 'Triangle / Y', 'Select / View / Share',
                 'Guide', 'Start / Options', 'L3', 'R3', 'L1 / LB', 'R1 / RB',
                 'D-pad up', 'D-pad down', 'D-pad left', 'D-pad right')
        return ([name for i, name in enumerate(names) if self.call('SDL_GetGamepadButton', self.preview, i)],
                [self.call('SDL_GetGamepadAxis', self.preview, i) for i in range(6)])

    def end_preview(self):
        if self.preview:
            self.call('SDL_CloseGamepad', self.preview)
            self.preview = None

    def close_device(self):
        self.end_preview()
        if self.pad:
            self.call('SDL_CloseJoystick', self.pad)
            self.pad = None
        self.device = None

    def close(self):
        self.close_device()
        if self.ready:
            self.call('SDL_QuitSubSystem', 0x2000)
            self.ready = False
        for name, value in getattr(self, 'old_hints', {}).items():
            if value is None:
                self.call('SDL_ResetHint', name.encode())
            else:
                self.call('SDL_SetHintWithPriority', name.encode(), value, 2)


def input_name(token, translate):
    if not token:
        return translate('Not configured')
    if token.startswith('b'):
        return translate('Button {}').format(int(token[1:]) + 1)
    if token.startswith('h'):
        hat, direction = token[1:].split('.')
        label = {'1': 'D-pad up', '2': 'D-pad right', '4': 'D-pad down', '8': 'D-pad left'}[direction]
        return translate('D-pad {}: {}').format(int(hat) + 1, translate(label))
    axis = int(token.lstrip('+-')[1:].rstrip('~')) + 1
    direction = '-' if token.startswith('-') or token.endswith('~') else '+'
    return translate('Axis {} ({})').format(axis, direction)


class MapperWindow:
    def __init__(self, parent, port, data_dir, translate=lambda text: text):
        import tkinter as tk
        from tkinter import ttk, messagebox
        self.tr, self.messagebox = translate, messagebox
        self.backend = SDLJoysticks(port, data_dir)
        self.path = Path(data_dir) / FILENAME
        self.capture = self.editing = None
        self.bindings = {}
        self.profile = None
        self.after_id = None
        self.closed = False
        self.window = tk.Toplevel(parent)
        self.window.title(self.tr('Set up a controller'))
        self.window.transient(parent)
        self.window.protocol('WM_DELETE_WINDOW', self.close)
        self.window.bind('<Destroy>', self.destroyed, add='+')
        self.window.bind('<Escape>', lambda _event: self.cancel_capture() if self.capture else self.close())
        self.window.minsize(700, 540)
        self.window.geometry(f'780x{min(760, self.window.winfo_screenheight()-100)}')
        frame = ttk.Frame(self.window, padding=20)
        frame.pack(fill='both', expand=True)
        ttk.Label(frame, text=self.tr('Set up a controller'), font=('', 16, 'bold')).pack(anchor='w')
        ttk.Label(frame, text=self.tr(
            'Choose Configure next to a control. Each change is saved immediately.'),
            wraplength=720).pack(anchor='w', pady=(6, 12))
        row = ttk.Frame(frame)
        row.pack(fill='x')
        self.choice = ttk.Combobox(row, state='readonly', width=46)
        self.choice.pack(side='left', fill='x', expand=True)
        self.choice.bind('<<ComboboxSelected>>', lambda _event: self.choose())
        ttk.Button(row, text=self.tr('Refresh'), command=self.refresh).pack(side='left', padx=(8, 0))
        self.details = tk.StringVar()
        ttk.Label(frame, textvariable=self.details).pack(anchor='w', pady=6)

        area = ttk.Frame(frame)
        area.pack(fill='both', expand=True, pady=8)
        background = ttk.Style(parent).lookup('TFrame', 'background') or parent.cget('background')
        canvas = tk.Canvas(area, highlightthickness=0, bg=background, height=380)
        scrollbar = ttk.Scrollbar(area, orient='vertical', command=canvas.yview)
        canvas.configure(yscrollcommand=scrollbar.set)
        scrollbar.pack(side='right', fill='y')
        canvas.pack(side='left', fill='both', expand=True)
        table = ttk.Frame(canvas)
        item = canvas.create_window((0, 0), window=table, anchor='nw')
        table.bind('<Configure>', lambda _event: canvas.configure(scrollregion=canvas.bbox('all')))
        canvas.bind('<Configure>', lambda event: canvas.itemconfigure(item, width=event.width))

        def scroll(event):
            direction = -1 if getattr(event, 'num', None) == 4 or getattr(event, 'delta', 0) > 0 else 1
            canvas.yview_scroll(direction * 3, 'units')
            return 'break'

        def wheel(widget):
            for event in ('<MouseWheel>', '<Button-4>', '<Button-5>'):
                widget.bind(event, scroll)

        wheel(canvas)
        for column, text in enumerate(('Game control', 'Assigned input', '', '')):
            ttk.Label(table, text=self.tr(text), font=('', 10, 'bold')).grid(
                row=0, column=column, sticky='w', padx=6, pady=(4, 10))
        table.columnconfigure(0, weight=1)
        table.columnconfigure(1, weight=1)
        self.rows = {}
        axis_labels = {'leftx': 'Left stick horizontal', 'lefty': 'Left stick vertical',
                       'rightx': 'Right stick horizontal', 'righty': 'Right stick vertical'}
        for index, (key, label) in enumerate(STEPS, 1):
            label = self.tr(axis_labels.get(key, label))
            control = ttk.Label(table, text=label)
            value = tk.StringVar()
            assigned = ttk.Label(table, textvariable=value)
            configure = ttk.Button(table, text=self.tr('Configure'), command=lambda target=key: self.begin_edit(target))
            clear = ttk.Button(table, text=self.tr('Clear'), command=lambda target=key: self.clear_binding(target))
            for column, widget in enumerate((control, assigned, configure, clear)):
                widget.grid(row=index, column=column, sticky='w', padx=6, pady=4)
                wheel(widget)
            self.rows[key] = {'value': value, 'configure': configure, 'clear': clear, 'label': label}

        self.prompt = tk.StringVar()
        ttk.Label(frame, textvariable=self.prompt, font=('', 12, 'bold'), wraplength=720).pack(anchor='w', pady=(8, 2))
        self.status = tk.StringVar()
        ttk.Label(frame, textvariable=self.status, wraplength=720).pack(anchor='w', pady=4)
        self.preview_text = tk.StringVar()
        ttk.Label(frame, textvariable=self.preview_text, wraplength=720).pack(anchor='w', pady=4)
        row = ttk.Frame(frame)
        row.pack(fill='x', pady=(8, 0))
        self.cancel_button = ttk.Button(row, text=self.tr('Cancel capture'), command=self.cancel_capture, state='disabled')
        self.cancel_button.pack(side='left')
        ttk.Button(row, text=self.tr('Close'), command=self.close).pack(side='right')
        self.refresh()
        self.window.grab_set()
        self.after_id = self.window.after(20, self.poll)

    def update_rows(self):
        enabled = bool(self.backend.device) and self.capture is None
        for key, row in self.rows.items():
            row['value'].set(input_name(self.bindings.get(key), self.tr))
            row['configure'].configure(state='normal' if enabled else 'disabled',
                                       text=self.tr('Listening…' if key == self.editing else 'Configure'))
            row['clear'].configure(state='normal' if enabled and key in self.bindings else 'disabled')
        self.cancel_button.configure(state='normal' if self.capture else 'disabled')

    def refresh(self):
        previous = self.backend.device and self.backend.device['guid']
        self.backend.close_device()
        self.capture = self.editing = None
        self.bindings, self.profile = {}, None
        self.status.set('')
        self.preview_text.set('')
        try:
            self.devices = self.backend.devices()
            self.choice.configure(values=[f'{item["name"]} [{item["id"]}]' for item in self.devices])
            if self.devices:
                self.choice.current(next((i for i, device in enumerate(self.devices)
                                          if device['guid'] == previous), 0))
                self.choose()
            else:
                self.choice.set('')
                self.details.set('')
                self.prompt.set(self.tr('No controller found. Connect it and choose Refresh.'))
                self.update_rows()
        except OSError as error:
            self.fail(error)

    def choose(self):
        self.capture = self.editing = None
        try:
            self.backend.select(self.devices[self.choice.current()])
            device = self.backend.device
            self.profile = load_mapping(self.path, device['guid'], self.backend.platform) or self.backend.current_mapping()
            self.bindings = mapping_bindings(self.profile)
            self.details.set(self.tr('{} buttons, {} axes, {} D-pad hats').format(
                device['buttons'], device['axes'], device['hats']))
            self.prompt.set(self.tr('Configure any control, in any order.'))
            self.status.set(self.tr('Saved changes are kept when you close this window.'))
            self.preview_text.set('')
            self.update_rows()
            self.update_preview()
        except (OSError, ValueError, IndexError) as error:
            self.fail(error)

    def update_preview(self):
        if self.bindings:
            self.backend.begin_preview(self.profile)
        else:
            self.backend.end_preview()

    def begin_edit(self, target):
        if self.capture or not self.backend.device:
            return
        try:
            capture = Capture(self.backend.state())
            capture.index = next(i for i, (key, _label) in enumerate(STEPS) if key == target)
            # Replacing this input is allowed, but conflicting with another row is not.
            capture.bindings = {key: value for key, value in self.bindings.items() if key != target}
            self.backend.end_preview()
            self.capture, self.editing = capture, target
            self.preview_text.set('')
            label = dict(STEPS)[target]
            self.prompt.set(self.tr(label) if target in ('leftx', 'lefty', 'rightx', 'righty')
                            else self.tr('Press {}').format(self.tr(label)))
            self.status.set(self.tr('Hold the input briefly. It will be saved for this control only.'))
            self.update_rows()
        except (OSError, ValueError) as error:
            self.status.set(self.tr(str(error)))

    def commit(self, target, token):
        device = self.backend.device
        # Re-read the saved profile so changing one row retains every other saved input.
        previous = load_mapping(self.path, device['guid'], self.backend.platform) or self.profile
        line = edit_mapping(previous, device['guid'], device['name'], self.backend.platform, target, token)
        self.backend.begin_preview(line)  # Validate with SDL before touching the saved profile.
        try:
            save_mapping(self.path, line)
        except (OSError, ValueError):
            self.backend.end_preview()
            raise
        self.profile, self.bindings = line, mapping_bindings(line)
        if not self.bindings:
            self.backend.end_preview()
        self.status.set(self.tr('Saved: {}').format(self.rows[target]['label']))

    def clear_binding(self, target):
        if self.capture or not self.backend.device:
            return
        try:
            self.commit(target, None)
            self.update_rows()
        except (OSError, ValueError) as error:
            self.messagebox.showerror(self.tr('Could not save mapping'), str(error), parent=self.window)

    def cancel_capture(self):
        self.capture = self.editing = None
        self.prompt.set(self.tr('Configure any control, in any order.'))
        self.status.set(self.tr('Capture canceled. Previously saved changes are kept.'))
        self.update_rows()
        try:
            self.update_preview()
        except OSError as error:
            self.fail(error)

    def poll(self):
        self.after_id = None
        try:
            if self.backend.device:
                state = self.backend.state()
                if self.capture:
                    token = self.capture.feed(state)
                    if token:
                        target = self.editing
                        self.capture = self.editing = None
                        try:
                            self.commit(target, token)
                        except (OSError, ValueError) as error:
                            self.messagebox.showerror(self.tr('Could not save mapping'), str(error), parent=self.window)
                            self.status.set(self.tr('The change was not saved. Configure the control again to retry.'))
                            self.update_preview()
                        self.prompt.set(self.tr('Configure any control, in any order.'))
                        self.update_rows()
                    elif self.capture.error:
                        self.status.set(self.tr(
                            'That input is assigned to another control. Clear its row first or choose another input.'))
                elif self.backend.preview:
                    buttons, axes = self.backend.preview_state()
                    self.preview_text.set(self.tr('Pressed: {}').format(', '.join(self.tr(name) for name in buttons) or '-') +
                                          '\n' + self.tr('Left stick: {} / {}   Right stick: {} / {}\nL2: {}   R2: {}').format(
                                              *(round(value / 32767 * 100) for value in axes)))
        except (OSError, ValueError) as error:
            self.fail(error)
        if not self.closed:
            self.after_id = self.window.after(20, self.poll)

    def fail(self, error):
        self.backend.close_device()
        self.capture = self.editing = None
        self.update_rows()
        self.status.set(self.tr(str(error)))

    def close(self):
        if self.closed:
            return
        self.closed = True
        if self.after_id is not None:
            self.window.after_cancel(self.after_id)
            self.after_id = None
        self.backend.close()
        self.window.destroy()

    def destroyed(self, event):
        if event.widget is self.window and not self.closed:
            self.closed = True
            if self.after_id is not None:
                self.window.after_cancel(self.after_id)
                self.after_id = None
            self.backend.close()
