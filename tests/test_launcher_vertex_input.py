"""Launcher setting for Vulkan's dynamic vertex input (GOW3_VERTEX_INPUT_DYNAMIC): environment,
page and texts. Automatic leaves the decision to the game (off on Intel's Windows driver,
gow3_vertex_input_default.h) and names the chosen card and that decision, read from the sixth
column of `gow3-gpu-capabilities --list`."""
import os
import sys
import unittest
from unittest import mock

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import gow3_lang  # noqa: E402
import gow3_launcher_win as launcher  # noqa: E402

# `gow3-gpu-capabilities --list`: index, name, type, multisampling default, the game's pick,
# dynamic vertex input default.
HD630_PC = [('0', 'Intel(R) HD Graphics 630', 'integrated', 'off', '0', 'off'),
            ('1', 'NVIDIA GeForce GTX 1050 Ti', 'discrete', 'on', '1', 'on')]
FIVE_COLUMNS = [('0', 'Intel(R) HD Graphics 630', 'integrated', 'off', '0'),
                ('1', 'NVIDIA GeForce GTX 1050 Ti', 'discrete', 'on', '1')]


def added(env):
    return {key for key in env if key not in os.environ}


class VertexInputEnvironmentTest(unittest.TestCase):
    def test_auto_leaves_the_game_default_and_keeps_an_inherited_value(self):
        self.assertEqual(launcher.APP_DEFAULTS['vertex_input'], '')
        self.assertNotIn('GOW3_VERTEX_INPUT_DYNAMIC', added(launcher.game_environment(dict(launcher.APP_DEFAULTS))))
        # A GOW3_VERTEX_INPUT_DYNAMIC set by a .bat before the launcher still reaches the game on Auto.
        with mock.patch.dict(os.environ, {'GOW3_VERTEX_INPUT_DYNAMIC': '0'}):
            self.assertEqual(launcher.game_environment(dict(launcher.APP_DEFAULTS))['GOW3_VERTEX_INPUT_DYNAMIC'], '0')

    def test_on_and_off_set_the_variable(self):
        for value in ('1', '0'):
            with self.subTest(value=value):
                env = launcher.game_environment({**launcher.APP_DEFAULTS, 'vertex_input': value})
                self.assertEqual(env['GOW3_VERTEX_INPUT_DYNAMIC'], value)

    def test_choices_and_legacy_settings(self):
        self.assertEqual([value for value, _text in launcher.VERTEX_INPUT], ['', '1', '0'])
        legacy = dict(launcher.APP_DEFAULTS)
        legacy.pop('vertex_input')
        self.assertNotIn('GOW3_VERTEX_INPUT_DYNAMIC', added(launcher.game_environment(legacy)))


class VertexInputOptionsTest(unittest.TestCase):
    def test_auto_label_names_the_card_and_the_decision(self):
        options = launcher.vertex_input_options(HD630_PC, '')
        self.assertEqual([value for value, _text in options], ['', '1', '0'])
        self.assertEqual(options[0][1], ('Automatic (NVIDIA GeForce GTX 1050 Ti: on)',))
        self.assertEqual(launcher.vertex_input_options(HD630_PC, '0')[0][1],
                         ('Automatic (Intel(R) HD Graphics 630: off)',))
        self.assertEqual(options[1:], launcher.VERTEX_INPUT[1:])
        # A tool without the sixth column, none at all, or a spelling this launcher does not know.
        self.assertEqual(launcher.vertex_input_options(FIVE_COLUMNS, '0')[0][1], ('Automatic',))
        self.assertEqual(launcher.vertex_input_options([], '')[0][1], ('Automatic',))
        self.assertEqual(launcher.vertex_input_options([('0', 'x', 'discrete', 'on', '1', 'OFF')], '')[0][1],
                         ('Automatic',))
        with mock.patch.object(launcher, 'LANG', 'pt'):
            self.assertEqual(launcher.vertex_input_options(HD630_PC, '0')[0][1],
                             ('Automático (Intel(R) HD Graphics 630: desligado)',))
        with mock.patch.object(launcher, 'LANG', 'es'):
            self.assertEqual(launcher.vertex_input_options(HD630_PC, '0')[0][1],
                             ('Automático (Intel(R) HD Graphics 630: desactivado)',))

    def test_portuguese_texts(self):
        table = gow3_lang.table('pt')
        texts = ('Dynamic vertex input', launcher.VERTEX_INPUT_HINT)
        for text in texts + tuple(text[0] for _value, text in launcher.VERTEX_INPUT):
            with self.subTest(text=text[:40]):
                self.assertTrue(table.get(text), f'no Portuguese text for {text!r}')

    def test_spanish_texts(self):
        table = gow3_lang.table('es')
        texts = ('Dynamic vertex input', launcher.VERTEX_INPUT_HINT)
        for text in texts + tuple(text[0] for _value, text in launcher.VERTEX_INPUT):
            with self.subTest(text=text[:40]):
                self.assertTrue(table.get(text), f'no Spanish text for {text!r}')
                self.assertNotEqual(table[text], text)


class VertexInputLauncherTest(unittest.TestCase):
    def test_performance_page_offers_the_choice(self):
        try:
            import tkinter as tk
            from tkinter import ttk
            root = tk.Tk()
        except ImportError as error:
            self.skipTest(f'Tk unavailable: {error}')
        except tk.TclError as error:
            self.skipTest(f'Tk display unavailable: {error}')
        root.withdraw()
        self.addCleanup(root.destroy)

        def build(language='en', gpu_id=''):
            with mock.patch.dict(os.environ, {'GOW3_CONFIG': os.devnull}), \
                    mock.patch.object(launcher, 'LANG', language), \
                    mock.patch.object(launcher, 'list_gpus', return_value=HD630_PC):
                app = launcher.Launcher.__new__(launcher.Launcher)
                app.tk, app.ttk, app.root, app.dpi = tk, ttk, root, 1.0
                app.content, app.pages, app.vars = ttk.Frame(root), {}, {}
                app.app = {**launcher.APP_DEFAULTS, 'gpu_id': gpu_id}
                app.ini, app.ini_lines = launcher.load_ini()
                app.control_vars, app.mod_order, app.patch_vars = {}, [], {}
                app.pad_style = tk.StringVar(root, value='playstation')
                app.build_performance()
            return app

        def widgets(parent):
            for child in parent.winfo_children():
                yield child
                yield from widgets(child)

        def boxes_with(page, on_label, off_label):  # On then Off: the multisampling row lists Off first
            return [widget for widget in widgets(page)
                    if isinstance(widget, ttk.Combobox) and tuple(widget['values'])[1:] == (on_label, off_label)]

        app = build()
        boxes = boxes_with(app.pages['performance'], 'On', 'Off')
        self.assertEqual(len(boxes), 1)
        self.assertEqual(tuple(boxes[0]['values']), ('Automatic (NVIDIA GeForce GTX 1050 Ti: on)', 'On', 'Off'))
        # As wide as the multisampling box: 'Automatic (Intel(R) HD Graphics 630: off)' is 41 characters.
        self.assertEqual(int(boxes[0].cget('width')), 46)
        self.assertEqual(app.vars['vertex_input'].get(), '')
        boxes[0].current(2)
        boxes[0].event_generate('<<ComboboxSelected>>')
        self.assertEqual(app.vars['vertex_input'].get(), '0')
        # Choosing the Intel card relabels Automatic in both GPU data rows at once.
        cards = [widget for widget in widgets(app.pages['performance'])
                 if isinstance(widget, ttk.Combobox) and widget['values'][0] == 'Automatic']
        cards[0].current(1)
        cards[0].event_generate('<<ComboboxSelected>>')
        self.assertEqual(app.vars['gpu_id'].get(), '0')
        self.assertEqual(boxes[0]['values'][0], 'Automatic (Intel(R) HD Graphics 630: off)')
        self.assertEqual(boxes[0].current(), 2)
        self.assertEqual(app.vars['vertex_input'].get(), '0')
        msaa = [widget for widget in widgets(app.pages['performance'])
                if isinstance(widget, ttk.Combobox) and widget['values'][1] == 'Off (fixes black lines)']
        self.assertEqual(msaa[0]['values'][0], 'Automatic (Intel(R) HD Graphics 630: off)')
        # A saved Graphics card choice is what the label starts from.
        app = build(gpu_id='0')
        boxes = boxes_with(app.pages['performance'], 'On', 'Off')
        self.assertEqual(boxes[0]['values'][0], 'Automatic (Intel(R) HD Graphics 630: off)')

        # The Portuguese page shows the translated label and hint.
        table = gow3_lang.table('pt')
        app = build('pt')
        texts = {widget.cget('text') for widget in widgets(app.pages['performance'])
                 if isinstance(widget, ttk.Label)}
        self.assertIn(table[launcher.VERTEX_INPUT_HINT], texts)
        self.assertTrue(any(text.startswith(table['Dynamic vertex input']) for text in texts))
        boxes = boxes_with(app.pages['performance'], table['On'], table['Off'])
        self.assertEqual(tuple(boxes[0]['values']),
                         ('Automático (NVIDIA GeForce GTX 1050 Ti: ligado)', table['On'], table['Off']))


if __name__ == '__main__':
    unittest.main()
