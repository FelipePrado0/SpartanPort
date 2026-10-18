"""Expanded diagnostics must not share the Log section's grid row (#522)."""
import contextlib
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from test_launcher_controls import launcher

try:
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk
except ImportError:
    tk = None


def descendants(widget):
    for child in widget.winfo_children():
        yield child
        yield from descendants(child)


@unittest.skipUnless(tk, 'requires Tk')
class DiagnosticsLayoutTest(unittest.TestCase):
    @contextlib.contextmanager
    def window(self, language, scale):
        with tempfile.TemporaryDirectory() as directory, contextlib.ExitStack() as stack:
            folder = Path(directory)
            config = folder / 'settings.json'
            config.write_text(json.dumps({'game_dir': '', 'check_updates': False}), encoding='utf-8')
            for name, value in (('CONFIG_DIR', folder), ('CONFIG_FILE', config),
                                ('DATA_DIR', folder), ('LANG', language)):
                stack.enter_context(mock.patch.object(launcher, name, value))
            stack.enter_context(mock.patch.dict(os.environ, {'GOW3_CONFIG': str(folder / 'gow3.ini')}))
            stack.enter_context(mock.patch.object(launcher, 'list_gpus', return_value=[]))
            try:
                root = tk.Tk()
            except tk.TclError as error:
                self.skipTest(f'Tk display unavailable: {error}')
            root.withdraw()
            try:
                root.tk.call('tk', 'scaling', scale)
                app = launcher.Launcher(root, tk, ttk, filedialog, messagebox)
                app.show('advanced')
                root.deiconify()
                root.update()
                yield root, app
            finally:
                try:
                    exists = root.winfo_exists()
                except tk.TclError:
                    exists = False
                if exists:
                    for pending in root.tk.call('after', 'info'):
                        root.after_cancel(pending)
                    root.destroy()

    def check_layout(self, language, scale):
        with self.window(language, scale) as (root, app):
            widgets = list(descendants(app.pages['advanced']))
            title = launcher._('Diagnostics (for development)').upper()
            toggle = next(w for w in widgets if w.winfo_class() == 'TLabel'
                          and str(w.cget('text')).endswith(title))
            heading = launcher._('Log').upper()
            log = next(w for w in widgets if w.winfo_class() == 'TLabel' and w.cget('text') == heading)
            hint = launcher._('Shows where the frame waits for the GPU and which reads cause it. For performance tests.')
            diagnostics_hint = next(w for w in widgets if w.winfo_class() == 'TLabel' and w.cget('text') == hint)
            body = diagnostics_hint.master
            self.assertFalse(body.winfo_ismapped())
            collapsed_y = log.winfo_rooty()
            for _ in range(3):
                toggle.event_generate('<Button-1>')
                root.update()
                self.assertTrue(body.winfo_ismapped())
                self.assertGreaterEqual(log.winfo_rooty(), body.winfo_rooty() + body.winfo_height())
                toggle.event_generate('<Button-1>')
                root.update()
                self.assertFalse(body.winfo_ismapped())
                self.assertEqual(log.winfo_rooty(), collapsed_y)

    def test_english(self):
        self.check_layout('en', 1.0)

    def test_portuguese(self):
        self.check_layout('pt', 1.0)

    def test_scaled_fonts(self):
        self.check_layout('en', 1.5)


if __name__ == '__main__':
    unittest.main()
