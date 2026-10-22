"""The launcher's monitor choice (SPARTAN_MONITOR): the `spartanport-probe --displays` parser, the Monitor
list of the Graphics page, the saved 'monitor' setting and the environment each launch path builds.
The probe prints seven TAB-separated columns; a monitor it cannot describe is left out, so the
indices the launcher offers may have gaps."""
import json
import os
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

from paths import ROOT
sys.path.insert(0, str(ROOT / 'launcher'))
import spartanport_launcher_win as launcher  # noqa: E402

# `spartanport-probe --displays`: index, name, primary, x, y, width, height. The name keeps spaces.
TWO_DISPLAYS = ('0\tMonitor A\t1\t0\t0\t1920\t1080\n'
                '1\tMonitor B\t0\t1920\t0\t2560\t1440\n')


def added(env):
    return {key for key in env if key not in os.environ}


def page_widgets(parent):
    for child in parent.winfo_children():
        yield child
        yield from page_widgets(child)


def monitor_box(app, first_label='Primary (default)', page='graphics'):
    """The Monitor combobox of a page, found by the label of its first entry (the default)."""
    from tkinter import ttk
    return next(widget for widget in page_widgets(app.pages[page])
                if isinstance(widget, ttk.Combobox) and widget['values'][0] == first_label)


class DisplayParserTest(unittest.TestCase):
    def test_seven_columns_are_read(self):
        self.assertEqual(launcher.parse_display_list(TWO_DISPLAYS),
                         [('0', 'Monitor A', '1', 0, 0, 1920, 1080),
                          ('1', 'Monitor B', '0', 1920, 0, 2560, 1440)])

    def test_names_keep_spaces_quotes_and_utf8(self):
        name = 'Dell U2720Q «à esquerda» , 4K'
        rows = launcher.parse_display_list(f'3\t{name}\t0\t-1920\t0\t3840\t2160\n')
        self.assertEqual(rows, [('3', name, '0', -1920, 0, 3840, 2160)])

    def test_a_name_with_a_straight_quote_is_preserved(self):
        name = 'Monitor "Pro"'
        rows = launcher.parse_display_list(f'3\t{name}\t0\t-1920\t0\t3840\t2160\n')
        self.assertEqual(rows, [('3', name, '0', -1920, 0, 3840, 2160)])

    def test_a_gap_and_an_empty_name_are_kept(self):
        rows = launcher.parse_display_list('0\tA\t1\t0\t0\t1920\t1080\n2\t\t0\t0\t0\t1280\t720\n')
        self.assertEqual(rows, [('0', 'A', '1', 0, 0, 1920, 1080), ('2', '', '0', 0, 0, 1280, 720)])

    def test_a_list_without_a_primary_is_not_a_failure(self):
        rows = launcher.parse_display_list('0\tA\t0\t0\t0\t1920\t1080\n1\tB\t0\t1920\t0\t1920\t1080\n')
        self.assertEqual([row[2] for row in rows], ['0', '0'])

    def test_malformed_lines_are_skipped(self):
        text = ('\n'                                   # a blank line
                '0\tA\t1\t0\t0\t1920\n'                # six columns
                '0\tA\t1\t0\t0\t1920\t1080\tx\n'       # eight columns
                'x\tA\t1\t0\t0\t1920\t1080\n'          # index is not a number
                '-1\tA\t1\t0\t0\t1920\t1080\n'         # index is not a non-negative number
                '\u00b2\tA\t1\t0\t0\t1920\t1080\n'     # index is a "digit" the game cannot read
                '0\tA\t2\t0\t0\t1920\t1080\n'          # primary is not 0 or 1
                '0\tA\t1\t0\t0\t1920\t1080\n')         # the one good line
        self.assertEqual(launcher.parse_display_list(text), [('0', 'A', '1', 0, 0, 1920, 1080)])

    def test_geometry_must_be_integers_with_positive_sizes(self):
        for bad in ('1\tA\t1\t0\t0\t0\t1080',        # zero width
                    '1\tA\t1\t0\t0\t1920\t-1',       # negative height
                    '1\tA\t1\t0\t0\twide\t1080',     # not a number
                    '1\tA\t1\t0\t0\t1_0\t1080',      # underscores are not digits
                    '1\tA\t1\t0\t0\t 5\t1080'):      # a blank is not a number
            with self.subTest(bad=bad):
                self.assertEqual(launcher.parse_display_list(bad), [])
        # Negative coordinates are valid (a monitor to the left of or above the primary one).
        self.assertEqual(launcher.parse_display_list('1\tA\t0\t-1920\t-1080\t1920\t1080\n'),
                         [('1', 'A', '0', -1920, -1080, 1920, 1080)])

    def test_empty_output_is_no_monitors(self):
        self.assertEqual(launcher.parse_display_list(''), [])

    def test_a_record_ends_on_a_line_feed_or_a_crlf(self):
        # The probe writes LF; a Windows C runtime may translate stdout to CRLF. Only those end a row.
        text = '0\tA\t1\t0\t0\t1920\t1080\r\n1\tB\t0\t1920\t0\t1280\t720\n'
        self.assertEqual(launcher.parse_display_list(text),
                         [('0', 'A', '1', 0, 0, 1920, 1080), ('1', 'B', '0', 1920, 0, 1280, 720)])

    def test_a_unicode_line_separator_in_a_name_is_not_a_record_boundary(self):
        # SDL_GetDisplayName may return a name holding U+2028 (LINE SEPARATOR) or another separator the
        # probe does not rewrite; the row must stay whole instead of being split and dropped.
        for sep in ('\u2028', '\u2029', '\u0085', '\u000b', '\u000c'):
            with self.subTest(separator=hex(ord(sep))):
                name = f'Panel{sep}A'
                rows = launcher.parse_display_list(f'0\t{name}\t1\t0\t0\t1920\t1080\n')
                self.assertEqual(rows, [('0', name, '1', 0, 0, 1920, 1080)])


class MonitorOptionsTest(unittest.TestCase):
    def test_the_default_is_first_and_the_rest_come_by_index(self):
        options = launcher.monitor_options(launcher.parse_display_list(TWO_DISPLAYS))
        self.assertEqual([value for value, _text in options], ['', '0', '1'])
        self.assertEqual(options[0], ('', ('Primary (default)',)))
        self.assertEqual(options[1][1], ('Monitor A · 1920 × 1080 · (primary)',))
        self.assertEqual(options[2][1], ('Monitor B · 2560 × 1440 · (1920, 0)',))

    def test_a_gap_in_the_indices_is_kept(self):
        options = launcher.monitor_options(launcher.parse_display_list('0\tA\t1\t0\t0\t1920\t1080\n'
                                                                       '2\tB\t0\t0\t0\t1280\t720\n'))
        self.assertEqual([value for value, _text in options], ['', '0', '2'])

    def test_no_monitors_leaves_only_the_default(self):
        self.assertEqual(launcher.monitor_options([]), [('', ('Primary (default)',))])

    def test_primary_at_nonzero_index(self):
        # Case A: Primary monitor at index 1 (not 0)
        displays = launcher.parse_display_list('0\tA\t0\t0\t0\t1920\t1080\n1\tB\t1\t1920\t0\t2560\t1440\n')
        options = launcher.monitor_options(displays)
        self.assertEqual([value for value, _text in options], ['', '0', '1'])
        self.assertEqual(options[0], ('', ('Primary (default)',)))
        self.assertEqual(options[1][1], ('A · 1920 × 1080',))  # index 0, not primary
        self.assertEqual(options[2][1], ('B · 2560 × 1440 · (1920, 0) · (primary)',))  # index 1, primary
        # Selection of '0' should work
        self.assertEqual(launcher.monitor_selection(displays, '0'), '0')
        self.assertEqual(launcher.monitor_selection(displays, '1'), '1')
        # Empty selection falls back to system default (empty string)
        self.assertEqual(launcher.monitor_selection(displays, ''), '')

    def test_negative_coordinates(self):
        # Case B: Monitor with negative coordinates (to the left of primary)
        displays = launcher.parse_display_list('0\tLeft\t1\t-1920\t0\t1920\t1080\n')
        self.assertEqual(displays, [('0', 'Left', '1', -1920, 0, 1920, 1080)])
        self.assertEqual(launcher.display_label(displays[0]), 'Left · 1920 × 1080 · (-1920, 0) · (primary)')


class ListDisplaysTest(unittest.TestCase):
    def test_a_missing_tool_lists_nothing(self):
        with mock.patch.object(launcher, 'find_tool', return_value=None):
            self.assertEqual(launcher.list_displays(), [])

    def test_failures_and_a_bad_exit_list_nothing(self):
        with mock.patch.object(launcher, 'find_tool', return_value='spartanport-probe'):
            with mock.patch.object(launcher.subprocess, 'run', side_effect=subprocess.TimeoutExpired('x', 5)):
                self.assertEqual(launcher.list_displays(), [])
            with mock.patch.object(launcher.subprocess, 'run', side_effect=OSError):
                self.assertEqual(launcher.list_displays(), [])
            done = mock.Mock(returncode=1, stdout='')
            with mock.patch.object(launcher.subprocess, 'run', return_value=done):
                self.assertEqual(launcher.list_displays(), [])

    def test_a_good_run_is_parsed_and_captured_as_utf8(self):
        done = mock.Mock(returncode=0, stdout=TWO_DISPLAYS)
        with mock.patch.object(launcher, 'find_tool', return_value='spartanport-probe'), \
                mock.patch.object(launcher.subprocess, 'run', return_value=done) as run:
            self.assertEqual(launcher.list_displays(), launcher.parse_display_list(TWO_DISPLAYS))
        self.assertEqual(run.call_args.args[0], ['spartanport-probe', '--displays'])
        self.assertEqual(run.call_args.kwargs['timeout'], 5)
        self.assertEqual(run.call_args.kwargs['encoding'], 'utf-8')
        self.assertEqual(run.call_args.kwargs['errors'], 'replace')

    def test_the_game_backend_sets_the_sdl_driver_for_the_probe(self):
        done = mock.Mock(returncode=0, stdout='')
        with mock.patch.object(launcher, 'WINDOWS', False), \
                mock.patch.object(launcher, 'find_tool', return_value='spartanport-probe'), \
                mock.patch.object(launcher.subprocess, 'run', return_value=done) as run, \
                mock.patch.dict(os.environ, {'SDL_VIDEODRIVER': 'sentinel', 'SDL_VIDEO_DRIVER': 'sentinel'}):
            launcher.list_displays('x11')
            self.assertEqual(run.call_args.kwargs['env']['SDL_VIDEODRIVER'], 'x11')
            self.assertEqual(run.call_args.kwargs['env']['SDL_VIDEO_DRIVER'], 'x11')
            launcher.list_displays('wayland')
            self.assertEqual(run.call_args.kwargs['env']['SDL_VIDEODRIVER'], 'wayland')
            launcher.list_displays('auto')  # Automatic lets SDL choose, as the game does
            self.assertEqual(run.call_args.kwargs['env']['SDL_VIDEODRIVER'], 'sentinel')


class MonitorEnvironmentTest(unittest.TestCase):
    def test_the_default_is_the_primary_and_sends_no_variable(self):
        self.assertEqual(launcher.APP_DEFAULTS['monitor'], '')
        self.assertNotIn('SPARTAN_MONITOR', added(launcher.game_environment(dict(launcher.APP_DEFAULTS))))

    def test_a_saved_index_reaches_the_game(self):
        env = launcher.game_environment({**launcher.APP_DEFAULTS, 'monitor': '1'})
        self.assertEqual(env['SPARTAN_MONITOR'], '1')
        # The first monitor is a real choice, not the default: it must be sent as well.
        self.assertEqual(launcher.game_environment({**launcher.APP_DEFAULTS, 'monitor': '0'})['SPARTAN_MONITOR'], '0')
        self.assertEqual(launcher.game_environment({**launcher.APP_DEFAULTS, 'monitor': 2})['SPARTAN_MONITOR'], '2')

    def test_an_invalid_choice_sends_nothing(self):
        for bad in ('', 'bad', '-1', '1.5', ' 1', '²', '  ', None):
            with self.subTest(bad=bad):
                env = launcher.game_environment({**launcher.APP_DEFAULTS, 'monitor': bad})
                self.assertNotIn('SPARTAN_MONITOR', added(env))

    def test_an_old_setting_without_the_key_is_the_default(self):
        legacy = dict(launcher.APP_DEFAULTS)
        legacy.pop('monitor')
        self.assertNotIn('SPARTAN_MONITOR', added(launcher.game_environment(legacy)))

    def test_an_inherited_variable_is_overridden(self):
        with mock.patch.dict(os.environ, {'SPARTAN_MONITOR': '3'}):
            self.assertNotIn('SPARTAN_MONITOR', added(launcher.game_environment(dict(launcher.APP_DEFAULTS))))
            self.assertEqual(launcher.game_environment({**launcher.APP_DEFAULTS, 'monitor': '1'})['SPARTAN_MONITOR'], '1')


class MonitorSelectionTest(unittest.TestCase):
    """The saved index the Monitor list lands on: the same index while the list still holds it, the
    primary default otherwise. The decision is by index, never by the position an option used to sit."""
    def test_a_saved_index_the_list_holds_is_kept(self):
        displays = launcher.parse_display_list(TWO_DISPLAYS)
        self.assertEqual(launcher.monitor_selection(displays, '1'), '1')
        self.assertEqual(launcher.monitor_selection(displays, '0'), '0')

    def test_an_index_the_list_lost_is_the_primary_default(self):
        displays = launcher.parse_display_list('1\tB\t0\t0\t0\t1280\t720\n')  # index 1 only
        self.assertEqual(launcher.monitor_selection(displays, '0'), '')
        self.assertEqual(launcher.monitor_selection(displays, '9'), '')

    def test_an_empty_or_unknown_choice_is_the_default(self):
        displays = launcher.parse_display_list(TWO_DISPLAYS)
        self.assertEqual(launcher.monitor_selection(displays, ''), '')
        self.assertEqual(launcher.monitor_selection([], '1'), '')

    def test_a_shorter_list_never_lands_on_a_monitor_by_position(self):
        # '0' was the first monitor; the new list starts at index 1, so the old position is another screen.
        displays = launcher.parse_display_list('1\tB\t0\t0\t0\t1280\t720\n')
        self.assertNotEqual(launcher.monitor_selection(displays, '0'), '1')


class MonitorRefreshTest(unittest.TestCase):
    """refresh_monitors: after a save the Monitor list is enumerated again only when the window system
    changed, and the choice the interface shows follows the new indices (or the primary default)."""
    def refresh(self, listed, pending, monitor, displays, selectable=True, visible=None):
        """A stand-in Launcher: `listed` is the backend the list was enumerated under (self.app),
        `pending` the one the Window system row now holds, `monitor` the value collected earlier,
        `visible` the index the Monitor row shows (the collected one when it is not given),
        `selectable` False for Windows, where that row is not built at all."""
        app = {**launcher.APP_DEFAULTS, 'video_backend': listed, 'monitor': monitor}
        fake = mock.Mock(monitor_box=mock.Mock(), monitor_pairs=None, app=app)
        fake.vars = {'monitor': _ValueVar(monitor if visible is None else visible)}
        if selectable:
            fake.vars['video_backend'] = _ValueVar(pending)
        with mock.patch.object(launcher, 'list_displays', return_value=displays) as detect:
            changed = launcher.Launcher.refresh_monitors(fake)
        return changed, fake, detect

    def test_an_unchanged_backend_refreshes_nothing(self):
        displays = launcher.parse_display_list(TWO_DISPLAYS)
        changed, fake, detect = self.refresh('x11', 'x11', '1', displays)
        self.assertFalse(changed)
        detect.assert_not_called()
        fake.monitor_box.refresh.assert_not_called()
        self.assertEqual(fake.vars['monitor'].sets, [])
        self.assertEqual(fake.app['monitor'], '1')

    def test_without_a_window_system_choice_nothing_happens(self):
        changed, fake, detect = self.refresh('auto', 'x11', '1', launcher.parse_display_list(TWO_DISPLAYS),
                                             selectable=False)  # as on Windows, the row is not built
        self.assertFalse(changed)
        detect.assert_not_called()
        self.assertEqual(fake.vars['monitor'].sets, [])

    def test_a_changed_backend_lists_under_the_new_one(self):
        displays = launcher.parse_display_list(TWO_DISPLAYS)
        changed, fake, detect = self.refresh('wayland', 'x11', '', displays)
        self.assertTrue(changed)
        detect.assert_called_once_with('x11')
        self.assertEqual(fake.monitor_pairs, launcher.monitor_options(displays))
        fake.monitor_box.refresh.assert_called_once_with()

    def test_a_choice_the_new_list_still_holds_is_kept(self):
        changed, fake, _detect = self.refresh('wayland', 'x11', '1', launcher.parse_display_list(TWO_DISPLAYS))
        self.assertTrue(changed)
        self.assertEqual(fake.vars['monitor'].sets, ['1'])
        self.assertEqual(fake.app['monitor'], '1')

    def test_a_choice_the_new_list_lost_is_the_default(self):
        displays = launcher.parse_display_list('1\tB\t0\t0\t0\t1280\t720\n')  # index 0 is gone
        changed, fake, _detect = self.refresh('wayland', 'x11', '0', displays)
        self.assertTrue(changed)
        self.assertEqual(fake.vars['monitor'].sets, [''])
        self.assertEqual(fake.app['monitor'], '')

    def test_another_monitor_at_the_old_position_is_not_selected(self):
        displays = launcher.parse_display_list('1\tB\t0\t0\t0\t1280\t720\n')
        _changed, fake, _detect = self.refresh('wayland', 'x11', '0', displays)
        self.assertEqual(fake.app['monitor'], '')  # not '1', the option now at the old position

    def test_an_unsaved_choice_survives_the_backend_change(self):
        # The user picked index 1 and then switched the window system, without saving in between: the
        # collected value is still '' but the choice on screen must be the one the save carries.
        displays = launcher.parse_display_list(TWO_DISPLAYS)
        changed, fake, _detect = self.refresh('auto', 'x11', '', displays, visible='1')
        self.assertTrue(changed)
        self.assertEqual(fake.vars['monitor'].sets, ['1'])
        self.assertEqual(fake.app['monitor'], '1')

    def test_the_unsaved_choice_beats_the_different_collected_one(self):
        displays = launcher.parse_display_list(TWO_DISPLAYS)
        _changed, fake, _detect = self.refresh('auto', 'x11', '0', displays, visible='1')
        self.assertEqual(fake.app['monitor'], '1')  # the index on screen, not the collected 0

    def test_an_unsaved_choice_lost_by_the_new_list_falls_back_to_the_collected_one(self):
        # The user picked index 0, which the x11 list does not have; the value collected earlier (1) is
        # a safe, documented stand-in and is still a real option.
        displays = launcher.parse_display_list('1\tB\t0\t0\t0\t1280\t720\n')
        _changed, fake, _detect = self.refresh('auto', 'x11', '1', displays, visible='0')
        self.assertEqual(fake.vars['monitor'].sets, ['1'])
        self.assertEqual(fake.app['monitor'], '1')  # not '', and not the position of index 0

    def test_with_neither_choice_in_the_new_list_it_is_the_default(self):
        displays = launcher.parse_display_list('1\tB\t0\t0\t0\t1280\t720\n')
        _changed, fake, _detect = self.refresh('auto', 'x11', '0', displays, visible='2')
        self.assertEqual(fake.vars['monitor'].sets, [''])
        self.assertEqual(fake.app['monitor'], '')

    def test_only_the_monitor_setting_changes(self):
        app = {**launcher.APP_DEFAULTS, 'video_backend': 'wayland', 'monitor': '0', 'gpu_id': '2'}
        example = dict(app)
        fake = mock.Mock(monitor_box=mock.Mock(), monitor_pairs=None, app=app)
        fake.vars = {'monitor': _ValueVar('0'), 'video_backend': _ValueVar('x11')}
        with mock.patch.object(launcher, 'list_displays',
                               return_value=launcher.parse_display_list('1\tB\t0\t0\t0\t1280\t720\n')):
            launcher.Launcher.refresh_monitors(fake)
        self.assertEqual({k: v for k, v in fake.app.items() if k != 'monitor'},
                         {k: v for k, v in example.items() if k != 'monitor'})
        self.assertEqual(fake.app['monitor'], '')  # index 0 is gone in the new list

    def test_the_refreshed_choice_is_what_a_launch_sends(self):
        _changed, fake, _detect = self.refresh('wayland', 'x11', '1', launcher.parse_display_list(TWO_DISPLAYS))
        self.assertEqual(launcher.game_environment(fake.app)['SPARTAN_MONITOR'], '1')


class _ValueVar:
    """A stand-in Tk variable (get/set) so the monitor flow runs without a display. `sets` keeps what
    was written to it, the way a Mock's set.assert_called_once_with would."""
    store = 'app'

    def __init__(self, value):
        self.value = value
        self.sets = []

    def get(self):
        return self.value

    def set(self, value):
        self.value = value
        self.sets.append(value)


class _LauncherStub:
    """The little of Launcher that collect() and play() touch, without Tk or real files. `gather` reads
    the app back from the variables; the settings file writers and `refresh_save_button` are replaced by
    the tests. `refresh_monitors` and `collect` are the real methods, so the centralised refresh runs the
    way the launcher runs it."""
    def __init__(self, listed, pending, monitor, visible=None):
        self.app = {**launcher.APP_DEFAULTS, 'video_backend': listed, 'monitor': monitor}
        self.vars = {'video_backend': _ValueVar(pending),
                     'monitor': _ValueVar(monitor if visible is None else visible)}
        self.monitor_box = mock.Mock()
        self.quick_monitor_box = mock.Mock()
        self.monitor_pairs = None
        self.patch_vars = {}
        self.messagebox = mock.Mock()
        self.ini, self.ini_lines = {}, []
        self.saved = {'ini': {}}
        self.process = None
        self.copying_system_files = False

    def refresh_monitors(self):
        return launcher.Launcher.refresh_monitors(self)

    def gather(self):
        return {'app': {**self.app, 'video_backend': self.vars['video_backend'].get(),
                        'monitor': self.vars['monitor'].get()},
                'ini': {}, 'mods': None, 'patches': None}

    def collect(self):
        return launcher.Launcher.collect(self)

    def refresh_save_button(self):
        pass

    def show(self, *_args):  # play() reaches this only with an unset game folder, which the tests force
        pass


class _StubbedLauncherTest(unittest.TestCase):
    """Runs the real collect()/play() over _LauncherStub with the probe and the settings files stubbed,
    so the monitor refresh centralised in collect() is exercised as the launcher exercises it."""
    def run_collect(self, stub, displays):
        """(ok, probe spy, {path: text} written) for the real collect() over `stub`."""
        written = {}
        with mock.patch.object(launcher, 'list_displays',
                               return_value=launcher.parse_display_list(displays)) as detect, \
                mock.patch.object(launcher, 'load_ini', lambda: ({}, [])), \
                mock.patch.object(launcher, 'save_ini'), \
                mock.patch.object(launcher, 'atomic_write',
                                  lambda path, text: written.__setitem__(path, text)):
            ok = launcher.Launcher.collect(stub)
        return ok, detect, written

    def run_play(self, stub, displays):
        """(probe spy, {path: text} written) for the real play() over `stub`, stopped after its collect."""
        written = {}
        with mock.patch.object(launcher, 'list_displays',
                               return_value=launcher.parse_display_list(displays)) as detect, \
                mock.patch.object(launcher, 'load_ini', lambda: ({}, [])), \
                mock.patch.object(launcher, 'save_ini'), \
                mock.patch.object(launcher, 'atomic_write',
                                  lambda path, text: written.__setitem__(path, text)), \
                mock.patch.object(launcher, 'game_info', return_value=None):  # stops play() after collect()
            launcher.Launcher.play(stub)
        return detect, written

    def settings(self, written):
        return json.loads(written[launcher.CONFIG_FILE])


class CollectMonitorConsistencyTest(_StubbedLauncherTest):
    """collect() is the one place the monitor choice is validated: it lists the monitors again (only for
    a changed window system) before it gathers and writes, so every caller stores one coherent pair."""
    def test_an_unchanged_backend_lists_nothing(self):
        stub = _LauncherStub('x11', 'x11', '1')
        ok, detect, written = self.run_collect(stub, TWO_DISPLAYS)
        self.assertTrue(ok)
        detect.assert_not_called()
        self.assertEqual(self.settings(written)['monitor'], '1')

    def test_a_changed_backend_lists_again_before_gathering(self):
        stub = _LauncherStub('wayland', 'x11', '1')
        ok, detect, written = self.run_collect(stub, TWO_DISPLAYS)
        self.assertTrue(ok)
        detect.assert_called_once_with('x11')
        saved = self.settings(written)
        self.assertEqual((saved['video_backend'], saved['monitor']), ('x11', '1'))

    def test_a_choice_the_new_list_lost_becomes_the_default_in_the_file(self):
        stub = _LauncherStub('wayland', 'x11', '0')
        ok, detect, written = self.run_collect(stub, '1\tB\t0\t0\t0\t1280\t720\n')
        self.assertTrue(ok)
        detect.assert_called_once_with('x11')
        # The reset happens before the gather: the file gets '' and never '1' (the old position).
        self.assertEqual(self.settings(written)['monitor'], '')

    def test_a_second_collect_lists_nothing_more(self):
        stub = _LauncherStub('wayland', 'x11', '1')
        self.run_collect(stub, TWO_DISPLAYS)
        _ok, detect_again, _written = self.run_collect(stub, TWO_DISPLAYS)
        detect_again.assert_not_called()  # the backend is stored now: no repeated probe

    def test_a_choice_not_saved_yet_is_what_collect_writes(self):
        # Picked monitor index 1 (still unsaved), switched the window system, then Save: the file must
        # carry the choice on screen, not the '' collected before it.
        stub = _LauncherStub('auto', 'x11', '', visible='1')
        ok, detect, written = self.run_collect(stub, TWO_DISPLAYS)
        self.assertTrue(ok)
        detect.assert_called_once_with('x11')
        saved = self.settings(written)
        self.assertEqual((saved['video_backend'], saved['monitor']), ('x11', '1'))
        self.assertEqual(stub.vars['monitor'].get(), '1')

    def test_a_choice_not_saved_yet_that_the_new_list_lost_is_the_default(self):
        stub = _LauncherStub('auto', 'x11', '', visible='3')  # 3 is not in the x11 list
        ok, _detect, written = self.run_collect(stub, TWO_DISPLAYS)
        self.assertTrue(ok)
        self.assertEqual(self.settings(written)['monitor'], '')


class PlayMonitorConsistencyTest(_StubbedLauncherTest):
    """PLAY goes through collect() too, so it can never start the game with a backend and a monitor from
    two different lists; the guard that skips a second launch is kept."""
    def test_play_without_a_backend_change_lists_nothing(self):
        stub = _LauncherStub('x11', 'x11', '1')
        detect, _written = self.run_play(stub, TWO_DISPLAYS)
        detect.assert_not_called()
        self.assertEqual(stub.app['monitor'], '1')

    def test_play_lists_again_when_the_backend_changed(self):
        stub = _LauncherStub('wayland', 'x11', '1')
        detect, _written = self.run_play(stub, TWO_DISPLAYS)
        detect.assert_called_once_with('x11')
        self.assertEqual(stub.app['monitor'], '1')

    def test_play_validates_the_choice_before_the_game_reads_it(self):
        stub = _LauncherStub('wayland', 'x11', '0')
        detect, _written = self.run_play(stub, '1\tB\t0\t0\t0\t1280\t720\n')
        detect.assert_called_once_with('x11')
        self.assertEqual(stub.app['monitor'], '')  # index 0 is gone in the new list

    def test_play_delivers_the_backend_and_the_monitor_it_kept(self):
        stub = _LauncherStub('wayland', 'x11', '1')
        with mock.patch.object(launcher, 'WINDOWS', False):
            self.run_play(stub, TWO_DISPLAYS)
            env = launcher.game_environment(stub.app)
        self.assertEqual(env['SDL_VIDEODRIVER'], 'x11')
        self.assertEqual(env['SDL_VIDEO_DRIVER'], 'x11')
        self.assertEqual(env['SPARTAN_MONITOR'], '1')

    def test_play_delivers_the_default_when_the_choice_was_reset(self):
        stub = _LauncherStub('wayland', 'x11', '0')
        with mock.patch.object(launcher, 'WINDOWS', False):
            self.run_play(stub, '1\tB\t0\t0\t0\t1280\t720\n')
            env = launcher.game_environment(stub.app)
        self.assertEqual(env['SDL_VIDEODRIVER'], 'x11')
        self.assertNotIn('SPARTAN_MONITOR', added(env))  # the reset choice is the primary default

    def test_play_keeps_a_choice_not_saved_yet(self):
        # Picked index 1, switched the window system, then PLAY: the game must open on that monitor,
        # not on the primary the stale collected '' would have sent.
        stub = _LauncherStub('auto', 'x11', '', visible='1')
        with mock.patch.object(launcher, 'WINDOWS', False):
            detect, _written = self.run_play(stub, TWO_DISPLAYS)
            env = launcher.game_environment(stub.app)
        detect.assert_called_once_with('x11')
        self.assertEqual(env['SDL_VIDEODRIVER'], 'x11')
        self.assertEqual(env['SPARTAN_MONITOR'], '1')

    def test_play_drops_a_choice_not_saved_yet_that_the_new_list_lost(self):
        stub = _LauncherStub('auto', 'x11', '', visible='9')  # 9 is not in the x11 list
        with mock.patch.object(launcher, 'WINDOWS', False):
            self.run_play(stub, TWO_DISPLAYS)
            env = launcher.game_environment(stub.app)
        self.assertNotIn('SPARTAN_MONITOR', added(env))

    def test_a_persisted_pair_reaches_a_play_as_one_environment(self):
        # --play (play_without_window) builds its environment the same way, from the saved settings.
        settings = {**launcher.APP_DEFAULTS, 'video_backend': 'x11', 'monitor': '1'}
        with mock.patch.object(launcher, 'WINDOWS', False):
            env = launcher.game_environment(settings)
        self.assertEqual(env['SDL_VIDEODRIVER'], 'x11')
        self.assertEqual(env['SPARTAN_MONITOR'], '1')


class CollectCallersTest(unittest.TestCase):
    """Every UI path that persists runs through collect(), so they all get the refresh centralised there;
    each one keeps the guard it already had (a running game, a failed save)."""
    def test_save_lets_collect_do_the_refresh(self):
        app = mock.Mock()
        launcher.Launcher.save(app)
        app.collect.assert_called_once_with()
        app.refresh_monitors.assert_not_called()  # no second enumeration from the caller

    def test_play_lets_collect_do_the_refresh(self):
        app = mock.Mock(process=None, copying_system_files=False)
        app.collect.return_value = False  # stop play() right after its collect
        launcher.Launcher.play(app)
        app.collect.assert_called_once_with()
        app.refresh_monitors.assert_not_called()

    def test_play_still_skips_collect_while_a_game_runs(self):
        app = mock.Mock(process=object())
        launcher.Launcher.play(app)
        app.collect.assert_not_called()

    def test_play_still_skips_collect_while_the_system_files_copy(self):
        app = mock.Mock(process=None, copying_system_files=True)
        launcher.Launcher.play(app)
        app.collect.assert_not_called()

    def test_close_collects_then_destroys_the_window(self):
        app = mock.Mock()
        launcher.Launcher.close(app)
        app.collect.assert_called_once_with()
        app.root.destroy.assert_called_once_with()

    def test_close_keeps_the_window_when_the_save_fails(self):
        app = mock.Mock()
        app.collect.return_value = False
        launcher.Launcher.close(app)
        app.root.destroy.assert_not_called()

    def test_open_path_collects_before_opening_a_settings_file(self):
        app = mock.Mock()
        app.collect.return_value = False
        with mock.patch.object(launcher, 'WINDOWS', False), \
                mock.patch.object(launcher.subprocess, 'Popen') as start:
            launcher.Launcher.open_path(app, 'spartanport.ini')
        app.collect.assert_called_once_with()
        start.assert_not_called()  # the save failed: nothing opens

    def test_open_path_opens_a_folder_without_collecting(self):
        app = mock.Mock()
        with tempfile.TemporaryDirectory() as folder, \
                mock.patch.object(launcher, 'WINDOWS', False), \
                mock.patch.object(launcher, 'host_environment', return_value={}), \
                mock.patch.object(launcher.subprocess, 'Popen') as start:
            launcher.Launcher.open_path(app, folder)
        app.collect.assert_not_called()
        start.assert_called_once()

    def test_add_to_steam_collects_first(self):
        app = mock.Mock()
        app.collect.return_value = False
        launcher.Launcher.add_to_steam(app)
        app.collect.assert_called_once_with()
        app.messagebox.showerror.assert_not_called()  # stopped at the failed collect


class MonitorLauncherTest(unittest.TestCase):
    def test_graphics_page_offers_the_choice(self):
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

        def build(language='en', monitor='', backend='auto', displays=TWO_DISPLAYS):
            with mock.patch.dict(os.environ, {'SPARTAN_CONFIG': os.devnull}), \
                    mock.patch.object(launcher, 'LANG', language), \
                    mock.patch.object(launcher, 'list_displays',
                                      return_value=launcher.parse_display_list(displays)) as detect:
                app = launcher.Launcher.__new__(launcher.Launcher)
                app.tk, app.ttk, app.root, app.dpi = tk, ttk, root, 1.0
                app.fonts = {}
                app.content, app.pages, app.vars = ttk.Frame(root), {}, {}
                app.app = {**launcher.APP_DEFAULTS, 'monitor': monitor, 'video_backend': backend}
                app.ini, app.ini_lines = launcher.load_ini()
                app.build_graphics()
            return app, detect

        app, detect = build()
        self.assertEqual(detect.call_args.args[0], 'auto')  # the saved backend, as the game uses it
        box = monitor_box(app)
        self.assertEqual(tuple(box['values']),
                         ('Primary (default)', 'Monitor A · 1920 × 1080 · (primary)', 'Monitor B · 2560 × 1440 · (1920, 0)'))
        self.assertEqual(app.vars['monitor'].get(), '')
        box.current(2)
        box.event_generate('<<ComboboxSelected>>')
        self.assertEqual(app.vars['monitor'].get(), '1')

        # A gap in the indices is offered as it is, never renumbered.
        app, _ = build(displays='0\tA\t1\t0\t0\t1920\t1080\n2\tB\t0\t0\t0\t1280\t720\n')
        box = monitor_box(app)
        self.assertEqual(tuple(box['values'])[1:], ('A · 1920 × 1080 · (primary)', 'B · 1280 × 720'))
        box.current(2)
        box.event_generate('<<ComboboxSelected>>')
        self.assertEqual(app.vars['monitor'].get(), '2')

        # A saved choice comes back; one whose monitor is gone falls back to the primary default.
        app, _ = build(monitor='1')
        self.assertEqual(monitor_box(app).current(), 2)
        app, _ = build(monitor='1', displays='')
        self.assertEqual(tuple(monitor_box(app)['values']), ('Primary (default)',))
        self.assertEqual(app.vars['monitor'].get(), '')

        # The saved video backend is what the probe enumerates with.
        app, detect = build(backend='x11')
        self.assertEqual(detect.call_args.args[0], 'x11')

        # The Portuguese page shows the translated title and default.
        app, _ = build('pt')
        box = monitor_box(app, first_label='Principal (padrão)')
        self.assertEqual(box['values'][0], 'Principal (padrão)')
        texts = {widget.cget('text') for widget in page_widgets(app.pages['graphics'])
                 if isinstance(widget, ttk.Label)}
        self.assertIn('Monitor', texts)

        # A new window system lists the monitors again; a choice the new list still holds comes back.
        app, _ = build(monitor='1')
        app.vars['video_backend'] = tk.StringVar(value='x11')  # as the Window system row would hold it
        with mock.patch.object(launcher, 'list_displays',
                               return_value=launcher.parse_display_list(TWO_DISPLAYS)):
            self.assertTrue(app.refresh_monitors())
        self.assertEqual(monitor_box(app).current(), 2)
        self.assertEqual(app.vars['monitor'].get(), '1')

        # A choice picked on the row but not saved yet survives the switch: the collected value is still
        # '', so the index on screen is the one the refresh must keep.
        app, _ = build(monitor='')
        box = monitor_box(app)
        box.current(2)
        box.event_generate('<<ComboboxSelected>>')  # the user picks the second monitor, no Save yet
        self.assertEqual(app.vars['monitor'].get(), '1')
        app.vars['video_backend'] = tk.StringVar(value='x11')
        with mock.patch.object(launcher, 'list_displays',
                               return_value=launcher.parse_display_list(TWO_DISPLAYS)):
            self.assertTrue(app.refresh_monitors())
        self.assertEqual(app.vars['monitor'].get(), '1')
        self.assertEqual(monitor_box(app).current(), 2)

        # A choice the new list no longer holds goes back to the primary default, not to another screen.
        app, _ = build(monitor='0')
        app.vars['video_backend'] = tk.StringVar(value='x11')
        with mock.patch.object(launcher, 'list_displays',
                               return_value=launcher.parse_display_list('1\tB\t0\t0\t0\t1280\t720\n')):
            self.assertTrue(app.refresh_monitors())
        self.assertEqual(tuple(monitor_box(app)['values']), ('Primary (default)', 'B · 1280 × 720'))
        self.assertEqual(monitor_box(app).current(), 0)
        self.assertEqual(app.vars['monitor'].get(), '')

        # Saving without changing the window system leaves the list alone.
        app, _ = build(monitor='1')
        app.vars['video_backend'] = tk.StringVar(value='auto')  # the backend the list was built under
        self.assertFalse(app.refresh_monitors())
        self.assertEqual(monitor_box(app).current(), 2)


class QuickSettingsMonitorTest(unittest.TestCase):
    """The Monitor choice of Quick settings: the same 'monitor' setting and the same spartanport-probe list
    as the Graphics page (one variable, one enumeration), so either row shows the screen the other
    one picks and the game receives the same index wherever it was chosen."""
    def setUp(self):
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
        self.tk, self.ttk, self.root = tk, ttk, root

    def build(self, language='en', monitor='', backend='auto', displays=TWO_DISPLAYS):
        """A Launcher with the Home and Graphics pages built the way build() builds them: the Home
        page lists the monitors first and the Graphics page reads that same list."""
        with mock.patch.dict(os.environ, {'SPARTAN_CONFIG': os.devnull}), \
                mock.patch.object(launcher, 'LANG', language), \
                mock.patch.object(launcher, 'list_displays',
                                  return_value=launcher.parse_display_list(displays)) as detect:
            app = launcher.Launcher.__new__(launcher.Launcher)
            app.tk, app.ttk, app.root, app.dpi = self.tk, self.ttk, self.root, 1.0
            app.fonts = {}
            app.content, app.pages, app.vars = self.ttk.Frame(self.root), {}, {}
            app.app = {**launcher.APP_DEFAULTS, 'monitor': monitor, 'video_backend': backend}
            app.ini, app.ini_lines = launcher.load_ini()
            app.draw_banner = lambda: None  # the cover art needs the fonts and the game folder
            app.monitor_pairs = None  # as build() leaves it before build_home()
            app.mod_order, app.mod_vars, app.patch_vars = [], {}, {}
            app.game_patch_title, app.game_patch_vars, app.control_vars = None, {}, {}
            app.pad_style = app.var('pad_style', 'app')  # gather() reads it back
            app.build_home()
            app.build_graphics()
        return app, detect

    def boxes(self, app):
        return [monitor_box(app, page=page) for page in ('home', 'graphics')]

    def test_quick_settings_offers_the_monitor_choice(self):
        app, detect = self.build()
        quick, graphics = self.boxes(app)
        self.assertEqual(tuple(quick['values']),
                         ('Primary (default)', 'Monitor A · 1920 × 1080 · (primary)', 'Monitor B · 2560 × 1440 · (1920, 0)'))
        self.assertEqual(tuple(quick['values']), tuple(graphics['values']))  # one list for the two rows
        self.assertEqual(app.vars['monitor'].get(), '')  # the primary default
        self.assertEqual(quick.current(), 0)
        detect.assert_called_once()  # one enumeration serves both rows
        self.assertEqual(detect.call_args.args[0], 'auto')  # under the saved window system

    def test_the_indices_of_the_probe_are_what_quick_settings_carries(self):
        # A gap in the indices is offered as it is, never renumbered: index 2 stays the third entry.
        app, _ = self.build(displays='0\tA\t1\t0\t0\t1920\t1080\n2\tB\t0\t0\t0\t1280\t720\n')
        for box in self.boxes(app):
            self.assertEqual(tuple(box['values']), ('Primary (default)', 'A · 1920 × 1080 · (primary)', 'B · 1280 × 720'))
            box.current(2)
            box.event_generate('<<ComboboxSelected>>')
            self.assertEqual(app.vars['monitor'].get(), '2')

    def test_picking_in_quick_settings_follows_on_the_graphics_page(self):
        app, _ = self.build()
        quick, graphics = self.boxes(app)
        self.assertEqual(graphics.current(), 0)
        quick.current(2)
        quick.event_generate('<<ComboboxSelected>>')
        self.assertEqual(app.vars['monitor'].get(), '1')
        self.assertEqual(graphics.current(), 2)  # the Graphics row shows the choice made above

    def test_picking_on_the_graphics_page_follows_in_quick_settings(self):
        app, _ = self.build()
        quick, graphics = self.boxes(app)
        graphics.current(1)
        graphics.event_generate('<<ComboboxSelected>>')
        self.assertEqual(app.vars['monitor'].get(), '0')
        self.assertEqual(quick.current(), 1)

    def test_the_choice_is_saved_and_comes_back_on_reopening(self):
        app, _ = self.build()
        quick, _graphics = self.boxes(app)
        quick.current(2)
        quick.event_generate('<<ComboboxSelected>>')
        saved = app.gather()  # the real gather, as collect() writes it to settings.json
        self.assertEqual(saved['app']['monitor'], '1')  # one setting, whichever row was used
        app, _ = self.build(monitor=saved['app']['monitor'])  # the launcher again on that file
        for box in self.boxes(app):
            self.assertEqual(box.current(), 2)

    def test_a_new_window_system_drops_an_index_the_new_list_lost(self):
        app, _ = self.build(monitor='0')
        app.vars['video_backend'] = self.tk.StringVar(value='x11')
        with mock.patch.object(launcher, 'list_displays',
                               return_value=launcher.parse_display_list('1\tB\t0\t0\t0\t1280\t720\n')):
            self.assertTrue(app.refresh_monitors())
        for box in self.boxes(app):
            self.assertEqual(tuple(box['values']), ('Primary (default)', 'B · 1280 × 720'))
            self.assertEqual(box.current(), 0)
        self.assertEqual(app.vars['monitor'].get(), '')
        self.assertNotIn('SPARTAN_MONITOR', added(launcher.game_environment(app.app)))

    def test_both_rows_follow_a_reordered_list_that_still_holds_the_index(self):
        # The index stays valid but sits at another position (another window system): the write trace
        # re-reads the list for the Quick settings row, which nothing else refreshes explicitly.
        app, _ = self.build(monitor='1')  # TWO_DISPLAYS: index 1 is the second entry
        for box in self.boxes(app):
            self.assertEqual(box.current(), 2)
        app.vars['video_backend'] = self.tk.StringVar(value='x11')
        with mock.patch.object(launcher, 'list_displays',
                               return_value=launcher.parse_display_list('1\tB\t0\t0\t0\t1280\t720\n')):
            self.assertTrue(app.refresh_monitors())
        self.assertEqual(app.vars['monitor'].get(), '1')  # the index is still one of the options
        for box in self.boxes(app):
            self.assertEqual(tuple(box['values']), ('Primary (default)', 'B · 1280 × 720'))
            self.assertEqual(box.current(), 1)  # the entry that index sits at now

    def test_the_choice_of_quick_settings_reaches_the_game(self):
        app, _ = self.build()
        quick, _graphics = self.boxes(app)
        quick.current(2)
        quick.event_generate('<<ComboboxSelected>>')
        with mock.patch.object(launcher, 'WINDOWS', False):
            # gather() is what save and PLAY hand to the game, so the row reaches it exactly as saved.
            self.assertEqual(launcher.game_environment(app.gather()['app'])['SPARTAN_MONITOR'], '1')

    def test_without_monitors_quick_settings_still_builds(self):
        app, _ = self.build(displays='')
        for box in self.boxes(app):
            self.assertEqual(tuple(box['values']), ('Primary (default)',))
            self.assertEqual(box.current(), 0)
        self.assertEqual(app.vars['monitor'].get(), '')


if __name__ == '__main__':
    unittest.main()
