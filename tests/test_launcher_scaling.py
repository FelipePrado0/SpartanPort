"""Desktop scaling keeps launcher text, controls and layout in proportion, without game changes."""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import spartanport_launcher_win as launcher  # noqa: E402

try:
    import tkinter as tk
    from tkinter import filedialog, font, messagebox, ttk
except ImportError:
    tk = None


class ScaleSelectionTest(unittest.TestCase):
    def test_manual_factors_are_absolute_even_on_a_scaled_desktop(self):
        for desktop_scale in (1, 1.25, 1.5, 2):
            for mode, expected in (('1', 1.0), ('2', 2.0)):
                self.assertEqual(launcher.launcher_ui_scale(mode, desktop_scale), expected)

    def test_auto_uses_the_desktop_scale_once(self):
        for scale in (1, 1.25, 1.5, 2):
            self.assertEqual(launcher.launcher_ui_scale('auto', scale), scale)
            self.assertEqual(launcher.launcher_ui_scale('invalid', scale), scale)

    def test_ui_scale_does_not_change_the_game_environment(self):
        settings = dict(launcher.APP_DEFAULTS)
        before = launcher.game_environment({**settings, 'ui_scale': '1'})
        self.assertEqual(before, launcher.game_environment({**settings, 'ui_scale': '2'}))


class DesktopScaleTest(unittest.TestCase):
    def test_gdk_scale_is_used_without_querying_xfce(self):
        with mock.patch.dict(os.environ, {'GDK_SCALE': '2'}), \
                mock.patch.object(launcher.subprocess, 'run') as run:
            self.assertEqual(launcher.x11_window_scale(), 2)
        run.assert_not_called()

    def test_xfce_window_scaling_is_read_when_gdk_scale_is_unset(self):
        with mock.patch.object(launcher, 'WINDOWS', False), \
                mock.patch.dict(os.environ, {'GDK_SCALE': '', 'XDG_CURRENT_DESKTOP': 'XFCE'}), \
                mock.patch.object(launcher.subprocess, 'run', return_value=mock.Mock(stdout='2\n')) as run:
            self.assertEqual(launcher.x11_window_scale(), 2)
        self.assertIn('/Gdk/WindowScalingFactor', run.call_args.args[0])

    def test_missing_or_unresponsive_xfconf_is_not_fatal(self):
        for error in (FileNotFoundError(), subprocess.TimeoutExpired('xfconf-query', 1)):
            with mock.patch.object(launcher, 'WINDOWS', False), \
                    mock.patch.dict(os.environ, {'GDK_SCALE': '', 'XDG_CURRENT_DESKTOP': 'XFCE'}), \
                    mock.patch.object(launcher.subprocess, 'run', side_effect=error):
                self.assertEqual(launcher.x11_window_scale(), 1)

    def test_invalid_environment_values_are_ignored(self):
        for value in ('bad', 'nan', 'inf', '-1', '0', '100'):
            with mock.patch.dict(os.environ, {'GDK_SCALE': value}):
                self.assertEqual(launcher.x11_window_scale(), 1, value)


class NativeScaleTest(unittest.TestCase):
    def root(self, backend, dpi=96, percent=None):
        root = mock.Mock()
        def call(*args):
            if args == ('tk', 'windowingsystem'):
                return backend
            if args == ('tk', 'scaling'):
                return dpi / 72
            if args == ('info', 'exists', 'tk::scalingPct'):
                return percent is not None
        root.tk.call.side_effect = call
        root.tk.getvar.return_value = percent
        return root

    def test_windows_uses_native_scale_without_x11_queries(self):
        for percent in (None, 150):
            root = self.root('win32', dpi=144, percent=percent)
            with mock.patch.object(launcher, 'x11_font_dpi') as xft, \
                    mock.patch.object(launcher, 'x11_window_scale') as fallback:
                self.assertEqual(launcher.desktop_ui_scale(root), 1.5)
            xft.assert_not_called()
            fallback.assert_not_called()
            self.assertFalse(any(len(c.args) == 3 and c.args[:2] == ('tk', 'scaling')
                                 for c in root.tk.call.call_args_list))

    def test_aqua_leaves_retina_to_macos_and_never_changes_native_scaling(self):
        root = self.root('aqua', dpi=192, percent=200)
        with mock.patch.object(launcher, 'x11_font_dpi') as xft:
            self.assertEqual(launcher.desktop_ui_scale(root), 1)
        xft.assert_not_called()
        root.tk.call.assert_called_once_with('tk', 'windowingsystem')

    def test_x11_uses_font_dpi_instead_of_physical_screen_size(self):
        for dpi in (96, 120, 144, 192):
            root = self.root('x11', dpi=110)
            with mock.patch.object(launcher, 'x11_font_dpi', return_value=dpi), \
                    mock.patch.object(launcher, 'x11_window_scale', return_value=1):
                self.assertAlmostEqual(launcher.desktop_ui_scale(root), dpi / 96)
            root.tk.call.assert_any_call('tk', 'scaling', dpi / 72)
            root.winfo_screenwidth.assert_not_called()

    def test_tk9_native_scale_is_not_lost_when_font_dpi_is_smaller(self):
        root = self.root('x11', percent=200)
        with mock.patch.object(launcher, 'x11_font_dpi', return_value=96), \
                mock.patch.object(launcher, 'x11_window_scale') as fallback:
            self.assertEqual(launcher.desktop_ui_scale(root), 2)
        fallback.assert_not_called()

    def test_missing_resources_use_tk_and_then_desktop_fallback(self):
        for dpi, fallback, expected in ((144, 1, 1.5), (96, 2, 2), (96, 1, 1)):
            with mock.patch.object(launcher, 'x11_font_dpi', return_value=None), \
                    mock.patch.object(launcher, 'x11_window_scale', return_value=fallback):
                self.assertEqual(launcher.desktop_ui_scale(self.root('x11', dpi)), expected)


@unittest.skipUnless(tk and (os.environ.get('DISPLAY') or sys.platform in ('win32', 'darwin')),
                     'needs a graphical desktop')
class LauncherScalingTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        folder = Path(self.tmp.name)
        patchers = [
            mock.patch.multiple(launcher, CONFIG_DIR=folder / 'config', CONFIG_FILE=folder / 'settings.json',
                                DATA_DIR=folder / 'data', LANG='en', UI_FONT=launcher.UI_FONT,
                                MONO_FONT=launcher.MONO_FONT, SERIF_FONT=launcher.SERIF_FONT,
                                FONT_LOADED=launcher.FONT_LOADED),
            mock.patch.dict(os.environ, {'SPARTAN_CONFIG': str(folder / 'spartanport.ini'),
                                        'GDK_SCALE': '', 'XDG_CURRENT_DESKTOP': '', 'DESKTOP_SESSION': ''}),
            mock.patch.object(launcher, 'monitor_under_pointer', return_value=(0, 0, 3840, 2160)),
        ]
        for patcher in patchers:
            patcher.start()
            self.addCleanup(patcher.stop)
        launcher.CONFIG_FILE.write_text('{"ui_scale": "1", "check_updates": false}', encoding='utf-8')
        self.root = None
        self.addCleanup(self.close_window)
        self.open_window()

    def open_window(self):
        # Exercise the same registration and font selection as main(), without substituting fonts.
        launcher.FONT_LOADED = launcher.register_font(launcher.FONT_FILE)
        self.root = tk.Tk()
        launcher.pick_fonts(font.families(self.root))
        self.native_fonts = {name: self.root.tk.call('font', 'configure', name)
                             for name in self.root.tk.call('font', 'names') if str(name).startswith('Tk')}
        self.app = launcher.Launcher(self.root, tk, ttk, filedialog, messagebox)
        self.app.show('advanced')
        self.root.update()

    def close_window(self):
        if self.root is None:
            return
        for pending in self.root.tk.call('after', 'info'):
            self.root.after_cancel(pending)
        self.root.destroy()
        self.root = None

    def text_height(self):
        return font.Font(self.root, font=ttk.Style(self.root).lookup('TLabel', 'font')).metrics('linespace')

    def widgets(self, parent=None):
        for child in (parent or self.root).winfo_children():
            yield child
            yield from self.widgets(child)

    def measurements(self):
        widgets = list(self.widgets())
        combo = next(w for w in widgets if w.winfo_class() == 'TCombobox')
        scroll = next(w for w in widgets if w.winfo_class() == 'TScrollbar' and str(w['orient']) == 'vertical')
        return {'text': self.text_height(), 'checkbox': self.app.check_on.width(),
                'heading': self.root.tk.call('font', 'metrics', self.app.side_title['font'], '-linespace'),
                'sidebar': self.app.side.winfo_width(), 'scrollbar': scroll.winfo_reqwidth(),
                'combobox_height': combo.winfo_reqheight(), 'combobox_width': combo.winfo_reqwidth(),
                'button_height': self.app.save_button.winfo_reqheight(),
                'padding': self.root.tk.getint(self.app.side_title.pack_info()['padx'])}

    def test_save_scales_the_whole_interface_and_preserves_game_settings(self):
        before_sizes = self.measurements()
        image_name = str(self.app.check_on)
        before = self.app.gather()['ini']
        before_environment = launcher.game_environment(self.app.app)
        native_scale = self.root.tk.call('tk', 'scaling')
        self.app.var('ui_scale', 'app').set('2')
        self.app.save()
        self.root.update()
        self.assertEqual(self.app.dpi, 2)
        after_sizes = self.measurements()
        for key, size in before_sizes.items():
            # Fixed borders and per-character raster rounding aren't exact multiples.
            self.assertAlmostEqual(after_sizes[key], size * 2, delta=max(6, size * .12), msg=key)
        self.assertEqual(str(self.app.check_on), image_name)
        self.assertEqual(self.root.tk.call('tk', 'scaling'), native_scale)
        for name, config in self.native_fonts.items():
            self.assertEqual(self.root.tk.call('font', 'configure', name), config, name)
        self.assertEqual(self.app.current_page, 'advanced')
        self.assertEqual(launcher.load_json(launcher.CONFIG_FILE, {})['ui_scale'], '2')
        self.assertEqual(self.app.gather()['ini'], before)
        self.assertEqual(launcher.game_environment(self.app.app), before_environment)
        self.assertEqual(str(self.app.save_button['state']), 'disabled')
        # Switching back must not accumulate the previous factor.
        self.app.var('ui_scale', 'app').set('1')
        self.app.save()
        self.root.update()
        self.assertEqual(self.measurements(), before_sizes)
        self.app.var('ui_scale', 'app').set('auto')
        self.app.save()
        self.root.update()
        self.assertEqual(self.app.dpi, self.app.desktop_scale)

    def test_saved_scale_is_applied_when_a_new_window_starts(self):
        self.app.var('ui_scale', 'app').set('2')
        self.app.save()
        self.root.update()
        sizes = self.measurements()
        self.close_window()
        self.open_window()
        self.assertEqual(self.app.dpi, 2)
        self.assertEqual(self.measurements(), sizes)

    def test_entry_and_combobox_fonts_belong_to_the_launcher(self):
        owned = set(map(str, self.app.fonts.values()))
        for widget in self.widgets():
            if widget.winfo_class() in ('TEntry', 'TCombobox', 'TSpinbox'):
                self.assertIn(str(widget['font']), owned, widget)

    def test_hint_labels_keep_their_smaller_font(self):
        style = ttk.Style(self.root)
        for widget in self.widgets():
            if widget.winfo_class() == 'TLabel' and str(widget['style']) == 'Muted.TLabel':
                effective_font = widget['font'] or style.lookup('Muted.TLabel', 'font')
                height = self.root.tk.call('font', 'metrics', effective_font, '-linespace')
                self.assertLess(height, self.text_height())
                return
        self.fail('No hint label found')

    def test_failed_save_keeps_the_current_scale(self):
        self.app.var('ui_scale', 'app').set('2')
        with mock.patch.object(self.app, 'collect', return_value=False):
            self.app.save()
        self.assertEqual(self.app.dpi, 1)


if __name__ == '__main__':
    unittest.main()
