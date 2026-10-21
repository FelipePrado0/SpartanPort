"""scripts/zarchive.py: the listing, the extraction view and its cache.

The native reader has its own test (tests/test_zarchive.c, tests/test_archive_fs.c). Here the
probe's answers are simulated, because what the preparation scripts read is the extracted folder.
"""
from paths import ROOT
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

import zarchive


class StandInProbe:
    """The probe's --zar-list and --zar-extract answers for {path: bytes}."""

    def __init__(self, files):
        self.files = dict(files)
        self.calls = []

    def __call__(self, probe, *arguments, capture=False, **kwargs):
        self.calls.append(arguments)
        if arguments[0] == '--zar-list':
            folders = set()
            for name in self.files:
                parts = name.split('/')[:-1]
                for depth in range(len(parts)):
                    folders.add('/'.join(parts[:depth + 1]))
            lines = [f'd\t{name}' for name in sorted(folders)]
            lines += [f'f\t{name}\t{len(data)}' for name, data in sorted(self.files.items())]
            return '\n'.join(lines) + '\n'
        if arguments[0] == '--zar-extract':
            destination = Path(arguments[3])
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(self.files[arguments[2]])
            return ''
        raise AssertionError(arguments)

    def extracted(self):
        return [arguments[2] for arguments in self.calls if arguments[0] == '--zar-extract']


GAME = {
    'eboot.bin': b'executable',
    'sce_module/libc.prx': b'libc',
    'sce_module/libSceFios2.prx': b'fios',
    'sce_sys/param.sfo': b'\0PSF' + bytes(60),
    'sce_sys/icon0.png': b'icon',
    'EXEC/WAD/PS4_LE/R_UI.WAD': b'wad' * 1000,
    'EXEC/WAD/PS4_LE/R_PERM.WAD': b'perm' * 100,
    'EXEC/WAD/PS4_LE/OLY150.WAD': b'oly' * 100,
    'dvdroot_ps4/PORTUGUESE/SOUNDS/X.XVAG': b'voice',
}


class ArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.archive = self.root / 'CUSA01623.zar'
        self.archive.write_bytes(b'archive placeholder')
        self.probe = StandInProbe(GAME)
        self.patch = mock.patch('zarchive._run', self.probe)
        self.patch.start()
        self.addCleanup(self.patch.stop)

    def view(self, extra=()):
        return zarchive.view(self.archive, self.root / 'cache', self.probe, extra=extra)

    def test_only_a_zar_file_is_an_image(self):
        self.assertTrue(zarchive.is_archive(self.archive))
        self.assertTrue(zarchive.is_archive(Path(str(self.archive).upper())))
        self.assertFalse(zarchive.is_archive(self.root))
        folder = self.root / 'folder.zar'
        folder.mkdir()
        self.assertFalse(zarchive.is_archive(folder))

    def test_listing_keeps_folders_and_file_sizes(self):
        listing = zarchive.listing(self.archive, self.probe)
        self.assertIn('sce_sys', listing.directories)
        self.assertIn('EXEC/WAD/PS4_LE', listing.directories)
        self.assertEqual(listing.files['sce_module/libc.prx'], 4)
        self.assertEqual(sorted(listing.files_in('sce_module/')),
                         sorted(['sce_module/libc.prx', 'sce_module/libSceFios2.prx']))
        self.assertEqual(listing.files_in('eboot.bin'), ['eboot.bin'])
        self.assertEqual(listing.files_in('sce_sys/missing.png'), [])

    def test_view_holds_the_files_the_preparation_scripts_read(self):
        folder = self.view()
        self.assertEqual(folder, self.root / 'cache' / 'game')
        for name in ('eboot.bin', 'sce_module/libc.prx', 'sce_module/libSceFios2.prx',
                     'sce_sys/param.sfo', 'sce_sys/icon0.png'):
            self.assertEqual((folder / name).read_bytes(), GAME[name], name)
        # The data files stay in the archive: the game reads them there.
        self.assertFalse((folder / 'EXEC/WAD/PS4_LE/R_UI.WAD').exists())
        self.assertTrue((folder / 'dvdroot_ps4/PORTUGUESE/SOUNDS').is_dir())  # the layout is visible
        self.assertIn('CUSA01623.zar', (self.root / 'cache' / 'view.json').read_text())

    def test_a_second_start_reuses_the_view(self):
        self.view()
        self.probe.calls.clear()
        folder = self.view()
        self.assertEqual(self.probe.extracted(), [])
        self.assertTrue((folder / 'eboot.bin').is_file())
        self.assertEqual([call[0] for call in self.probe.calls], [])  # not even listed again

    def test_extra_files_are_added_to_the_view(self):
        folder = self.view(extra=(zarchive.PAUSE_MENU_WAD,))
        self.assertEqual((folder / zarchive.PAUSE_MENU_WAD).read_bytes(), GAME[zarchive.PAUSE_MENU_WAD])
        self.assertFalse((folder / zarchive.GLYPH_WADS[1]).exists())
        self.view(extra=zarchive.GLYPH_WADS)
        self.assertTrue((folder / zarchive.GLYPH_WADS[1]).is_file())
        self.assertTrue((folder / zarchive.GLYPH_WADS[2]).is_file())  # the lyre's WAD

    def test_another_archive_or_a_repacked_one_rebuilds_the_view(self):
        self.view()
        self.archive.write_bytes(b'archive placeholder, repacked')
        os.utime(self.archive, (1, 1))
        self.probe.calls.clear()
        folder = self.view()
        self.assertIn('eboot.bin', self.probe.extracted())
        self.assertEqual((folder / 'eboot.bin').read_bytes(), b'executable')

    def test_a_missing_entry_is_skipped(self):
        others = {name: data for name, data in GAME.items() if name != 'sce_sys/icon0.png'}
        self.probe.files = others
        folder = self.view()
        self.assertFalse((folder / 'sce_sys/icon0.png').exists())
        self.assertTrue((folder / 'eboot.bin').is_file())

    def test_a_probe_failure_is_an_archive_error(self):
        def refuse(*arguments, **kwargs):
            raise zarchive.ArchiveError('ZArchive support needs libzstd, which was not found')
        with mock.patch('zarchive._run', refuse):
            with self.assertRaises(zarchive.ArchiveError):
                zarchive.listing(self.archive, self.probe)

    def test_listing_json_for_the_resource_census(self):
        path = zarchive.listing(self.archive, self.probe).write(self.root / 'out' / 'listing.json')
        self.assertEqual(json.loads(path.read_text())['eboot.bin'], 10)

    def test_the_probe_is_found_beside_the_port(self):
        port = self.root / 'port'
        (port / 'bin').mkdir(parents=True)
        name = 'spartanport-probe.exe' if sys.platform == 'win32' else 'spartanport-probe'
        (port / 'bin' / name).write_bytes(b'')
        with mock.patch.dict(os.environ, {'SPARTAN_PROBE': ''}):
            self.assertEqual(zarchive.probe_executable(port), port / 'bin' / name)
        chosen = self.root / 'my-probe'
        chosen.write_bytes(b'')
        with mock.patch.dict(os.environ, {'SPARTAN_PROBE': str(chosen)}):
            self.assertEqual(zarchive.probe_executable(port), chosen)


if __name__ == '__main__':
    unittest.main()
