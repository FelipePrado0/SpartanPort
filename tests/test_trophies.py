# SPDX-License-Identifier: GPL-2.0-or-later
"""Tests of the trophy support of the launcher (launcher/spartanport_trophies.py and its language tables).

They need no game files: the trophy package (trophy00.trp) is built here, byte by byte."""

import io
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'launcher'))

import spartanport_lang  # noqa: E402
import spartanport_trophies  # noqa: E402

try:
    from PIL import Image
except ImportError:  # the colour test is skipped without Pillow
    Image = None

LANGUAGES = ('en', 'ru', 'ar', 'es', 'pt', 'fr', 'de', 'it', 'pl', 'tr', 'zh', 'ja', 'ko')


def build_trp(entries, magic=b'\xdc\xa2\x4d\x00', element=0x40):
    """A trophy package: 0x60-byte header, one entry per (name, bytes), then the files."""
    header_size = 0x60
    offset = header_size + element * len(entries)
    table, body = b'', b''
    for name, data in entries:
        table += name.encode().ljust(0x20, b'\0') + struct.pack('>QQ', offset + len(body), len(data))
        table = table.ljust(len(table) + element - 0x30, b'\0')
        body += data
    header = magic + struct.pack('>I', 3) + struct.pack('>Q', header_size + len(table) + len(body))
    header += struct.pack('>II', len(entries), element)
    return header.ljust(header_size, b'\0') + table + body


def solid_png(rgb):
    out = io.BytesIO()
    Image.new('RGB', (32, 32), rgb).save(out, 'PNG')
    return out.getvalue()


class TrophyTexts(unittest.TestCase):
    def test_every_language_lists_all_trophies(self):
        for language in LANGUAGES:
            texts = spartanport_trophies.TEXTS[language]
            self.assertEqual(len(texts), spartanport_trophies.TROPHY_COUNT, language)
            for number, (name, description) in enumerate(texts):
                self.assertTrue(name.strip() and description.strip(), (language, number))
                self.assertNotIn('|', name + description, (language, number))  # the separator of trophy_texts.txt
                self.assertLessEqual(len(name.encode()), 63, (language, number))  # runtime buffers
                self.assertLessEqual(len(description.encode()), 159, (language, number))

    def test_text_falls_back_to_english(self):
        self.assertEqual(spartanport_trophies.text('xx', 3), spartanport_trophies.TEXTS['en'][3])
        self.assertEqual(spartanport_trophies.text('pt', 0)[0], 'Rei da colina')
        self.assertEqual(spartanport_trophies.text('en', 99), ('', ''))
        self.assertEqual(spartanport_trophies.text('en', -1), ('', ''))


class PageTexts(unittest.TestCase):
    def test_every_language_has_the_same_texts(self):
        keys = set(spartanport_trophies.UI['pt'])
        for language, table in spartanport_trophies.UI.items():
            self.assertEqual(set(table), keys, language)
            for key, text in table.items():
                self.assertTrue(text.strip(), (language, key))
                self.assertEqual(text.count('{}'), key.count('{}'), (language, key))

    def test_launcher_tables_carry_them(self):
        for language in spartanport_trophies.UI:
            table = spartanport_lang.table(language)
            for key, text in spartanport_trophies.UI[language].items():
                self.assertEqual(table.get(key), text, (language, key))


class TrophyPackage(unittest.TestCase):
    def setUp(self):
        self.folder = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, str(self.folder), True)

    def write(self, name, data):
        path = self.folder / name
        path.write_bytes(data)
        return path

    def test_icons_are_read_by_id(self):
        png = b'\x89PNG-fake'
        path = self.write('trophy00.trp', build_trp([
            ('TROPCONF.ESFM', b'encrypted'), ('TROP000.PNG', png + b'0'), ('TROP034.PNG', png + b'34'),
            ('TROP_00.ESFM', b'x'), ('ICON0.PNG', b'icon')]))
        self.assertEqual(spartanport_trophies.read_trp_icons(path), {0: png + b'0', 34: png + b'34'})

    def test_bad_packages_give_nothing(self):
        good = build_trp([('TROP001.PNG', b'abc')])
        self.assertEqual(spartanport_trophies.read_trp_icons(self.folder / 'missing.trp'), {})
        self.assertEqual(spartanport_trophies.read_trp_icons(self.write('a.trp', b'')), {})
        self.assertEqual(spartanport_trophies.read_trp_icons(self.write('b.trp', b'\0\0\0\0' + good[4:])), {})
        self.assertEqual(spartanport_trophies.read_trp_icons(self.write('c.trp', build_trp([('TROP001.PNG', b'abc')], element=8))), {})
        self.assertEqual(spartanport_trophies.read_trp_icons(self.write('d.trp', good[:-2])), {})  # a file past the end

    def test_unlocked_bits(self):
        flags = [0, 0, 0, 0]
        for trophy in (0, 5, 31, 32, 34, 127):
            flags[trophy // 32] |= 1 << (trophy % 32)
        path = self.write('trophies.dat', struct.pack('<4I', *flags))
        self.assertEqual(spartanport_trophies.read_unlocked_trophies(path), {0, 5, 31, 32, 34, 127})
        self.assertEqual(spartanport_trophies.read_unlocked_trophies(self.folder / 'none.dat'), set())
        self.assertEqual(spartanport_trophies.read_unlocked_trophies(self.write('short.dat', b'\x02')), {1})

    @unittest.skipUnless(Image, 'Pillow is not installed')
    def test_tier_follows_the_colour_of_the_icon(self):
        colours = {'platinum': (139, 167, 167), 'bronze': (137, 89, 52), 'silver': (136, 119, 104),
                   'gold': (141, 104, 27)}  # the average colours of the icons of the game
        for tier, rgb in colours.items():
            self.assertEqual(spartanport_trophies.trophy_tier(solid_png(rgb)), tier)
        self.assertEqual(spartanport_trophies.trophy_tier(b'not a png'), '')
        self.assertEqual(spartanport_trophies.trophy_tier(solid_png((0, 0, 0))), '')


class TrophyEnvironmentAndPersistence(unittest.TestCase):
    def setUp(self):
        self.folder = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, str(self.folder), True)

    def test_game_environment_propagates_default_and_custom_user_dir(self):
        import spartanport_launcher_win as launcher
        # Isolate environment and launcher.DATA_DIR in a temporary directory
        temp_data = self.folder / 'data'
        temp_data.mkdir()
        orig_data = launcher.DATA_DIR
        launcher.DATA_DIR = temp_data
        self.addCleanup(setattr, launcher, 'DATA_DIR', orig_data)

        orig_env = dict(os.environ)
        self.addCleanup(lambda: (os.environ.clear(), os.environ.update(orig_env)))
        os.environ.pop('SPARTAN_USER_DIR', None)

        # 1. Default user_dir when neither settings['user_dir'] nor SPARTAN_USER_DIR is set
        env_default = launcher.game_environment({**launcher.APP_DEFAULTS, 'user_dir': ''})
        self.assertIn('SPARTAN_USER_DIR', env_default)
        self.assertEqual(env_default['SPARTAN_USER_DIR'], str(temp_data / 'user'))
        self.assertTrue((temp_data / 'user' / 'trophy_texts.txt').is_file())

        # 2. Inherited SPARTAN_USER_DIR when settings['user_dir'] is empty
        env_saves = str(self.folder / 'env_saves')
        os.environ['SPARTAN_USER_DIR'] = env_saves
        env_from_os = launcher.game_environment({**launcher.APP_DEFAULTS, 'user_dir': ''})
        self.assertEqual(env_from_os['SPARTAN_USER_DIR'], env_saves)
        self.assertTrue((Path(env_saves) / 'trophy_texts.txt').is_file())

        # 3. Explicit custom user_dir in settings overrides inherited SPARTAN_USER_DIR
        custom_dir = str(self.folder / 'custom_saves')
        env_custom = launcher.game_environment({**launcher.APP_DEFAULTS, 'user_dir': custom_dir})
        self.assertIn('SPARTAN_USER_DIR', env_custom)
        self.assertEqual(env_custom['SPARTAN_USER_DIR'], custom_dir)
        self.assertTrue((Path(custom_dir) / 'trophy_texts.txt').is_file())

    def test_write_trophy_texts_and_launcher_display_consistency(self):
        import spartanport_launcher_win as launcher
        test_user = self.folder / 'deck_user'
        launcher.write_trophy_texts(test_user)

        trophy_file = test_user / 'trophy_texts.txt'
        self.assertTrue(trophy_file.is_file())
        content = trophy_file.read_text(encoding='utf-8')
        self.assertIn('@heading=', content)
        self.assertIn('0=', content)
        self.assertIn('1=', content)

        # Write unlocked trophies to user dir and verify read_unlocked_trophies finds them
        flags = [0, 0, 0, 0]
        flags[0] |= (1 << 1)  # trophy 1
        (test_user / 'trophies.dat').write_bytes(struct.pack('<4I', *flags))
        unlocked = spartanport_trophies.read_unlocked_trophies(test_user / 'trophies.dat')
        self.assertEqual(unlocked, {1})

    @unittest.skipIf(sys.platform == 'win32', 'AppRun is Linux/POSIX only')
    def test_apprun_environment_default_xdg(self):
        import subprocess
        root = Path(__file__).resolve().parent.parent
        appdir = self.folder / 'AppDir'
        share = appdir / 'usr' / 'share' / 'spartanport'
        share.mkdir(parents=True)
        launcher_bin = share / 'spartanport-launcher'
        launcher_bin.write_text('#!/bin/sh\necho SPARTAN_DATA_DIR=$SPARTAN_DATA_DIR\necho SPARTAN_USER_DIR=$SPARTAN_USER_DIR\n')
        launcher_bin.chmod(0o755)

        apprun = appdir / 'AppRun'
        apprun.write_text((root / 'packaging/linux/AppRun').read_text())
        apprun.chmod(0o755)

        env = os.environ.copy()
        fake_home = self.folder / 'fake_home'
        fake_home.mkdir()
        env['HOME'] = str(fake_home)
        env.pop('SPARTAN_DATA_DIR', None)
        env.pop('SPARTAN_USER_DIR', None)
        env.pop('XDG_DATA_HOME', None)

        res = subprocess.run([str(apprun)], env=env, capture_output=True, text=True, check=True)
        lines = dict(l.split('=', 1) for l in res.stdout.strip().splitlines() if '=' in l)

        expected_data = str(fake_home / '.local' / 'share' / 'spartanport')
        expected_user = str(fake_home / '.local' / 'share' / 'spartanport' / 'user')
        self.assertEqual(lines['SPARTAN_DATA_DIR'], expected_data)
        self.assertEqual(lines['SPARTAN_USER_DIR'], expected_user)
        self.assertTrue(Path(expected_user).is_dir())

    @unittest.skipIf(sys.platform == 'win32', 'AppRun is Linux/POSIX only')
    def test_apprun_environment_paths_with_spaces(self):
        import subprocess
        root = Path(__file__).resolve().parent.parent
        space_home = self.folder / 'User With Spaces'
        space_home.mkdir()

        appdir = self.folder / 'AppDir'
        share = appdir / 'usr' / 'share' / 'spartanport'
        share.mkdir(parents=True)
        launcher_bin = share / 'spartanport-launcher'
        launcher_bin.write_text('#!/bin/sh\necho SPARTAN_USER_DIR=$SPARTAN_USER_DIR\n')
        launcher_bin.chmod(0o755)

        apprun = appdir / 'AppRun'
        apprun.write_text((root / 'packaging/linux/AppRun').read_text())
        apprun.chmod(0o755)

        env = os.environ.copy()
        env['HOME'] = str(space_home)
        env.pop('SPARTAN_DATA_DIR', None)
        env.pop('SPARTAN_USER_DIR', None)
        env.pop('XDG_DATA_HOME', None)

        res = subprocess.run([str(apprun)], env=env, capture_output=True, text=True, check=True)
        lines = dict(l.split('=', 1) for l in res.stdout.strip().splitlines() if '=' in l)

        expected_space_user = str(space_home / '.local' / 'share' / 'spartanport' / 'user')
        self.assertEqual(lines['SPARTAN_USER_DIR'], expected_space_user)
        self.assertTrue(Path(expected_space_user).is_dir())

    @unittest.skipIf(sys.platform == 'win32', 'AppRun is Linux/POSIX only')
    def test_apprun_preserves_external_spartan_user_dir(self):
        import subprocess
        root = Path(__file__).resolve().parent.parent
        custom_external = self.folder / 'external_user_dir'
        custom_external.mkdir()

        appdir = self.folder / 'AppDir'
        share = appdir / 'usr' / 'share' / 'spartanport'
        share.mkdir(parents=True)
        launcher_bin = share / 'spartanport-launcher'
        launcher_bin.write_text('#!/bin/sh\necho SPARTAN_USER_DIR=$SPARTAN_USER_DIR\n')
        launcher_bin.chmod(0o755)

        apprun = appdir / 'AppRun'
        apprun.write_text((root / 'packaging/linux/AppRun').read_text())
        apprun.chmod(0o755)

        env = os.environ.copy()
        env['SPARTAN_USER_DIR'] = str(custom_external)

        res = subprocess.run([str(apprun)], env=env, capture_output=True, text=True, check=True)
        lines = dict(l.split('=', 1) for l in res.stdout.strip().splitlines() if '=' in l)
        self.assertEqual(lines['SPARTAN_USER_DIR'], str(custom_external))


if __name__ == '__main__':
    unittest.main()
