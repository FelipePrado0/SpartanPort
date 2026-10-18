from paths import ROOT  # noqa: F401  (puts scripts/ on the path)
import contextlib
import shutil
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest import mock

import game_update
from mods import is_link


def write(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)


def no_links_but_hard_links():
    """Simulates a network share (SMB): symlinks and junctions are refused, hard links work.
    CreateJunction creates the directory and only then refuses the reparse point."""
    def create_junction(src, dst):
        Path(dst).mkdir()
        raise PermissionError(5, 'Access is denied')
    return (mock.patch.object(Path, 'symlink_to', side_effect=PermissionError(5, 'Access is denied')),
            mock.patch('mods.os.name', 'nt'),
            mock.patch.dict(sys.modules, {'_winapi': types.SimpleNamespace(CreateJunction=create_junction)}))


class GameUpdateTests(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.root, True)
        self.base, self.update, self.out = self.root / 'base', self.root / 'update', self.root / 'out'
        write(self.base / 'eboot.bin', 'base eboot')
        write(self.base / 'EXEC/WAD/A.WAD', 'wad')
        write(self.base / 'sce_sys/icon0.png', 'old icon')
        write(self.update / 'eboot.bin', 'update eboot')
        write(self.update / 'SCE_SYS/param.sfo', 'new sfo')
        write(self.update / 'SCE_SYS/icon0.png', 'new icon')

    def view(self, ids=('CUSA01623', 'CUSA01623')):
        with mock.patch('patches.game_title_id', side_effect=list(ids)):
            return game_update.create_view(self.base, self.update, self.out)

    def test_update_files_replace_base_files_and_the_rest_stays(self):
        view = self.view()
        self.assertEqual((view / 'eboot.bin').read_text(), 'update eboot')
        self.assertEqual((view / 'EXEC/WAD/A.WAD').read_text(), 'wad')
        self.assertEqual((view / 'sce_sys/icon0.png').read_text(), 'new icon')
        self.assertEqual((view / 'sce_sys/param.sfo').read_text(), 'new sfo')
        self.assertEqual([p.name for p in view.iterdir() if p.name.lower() == 'sce_sys'], ['sce_sys'])

    def test_folders_the_update_leaves_alone_are_linked_whole(self):
        view = self.view()
        self.assertTrue(is_link(view / 'EXEC'))
        self.assertFalse(is_link(view / 'sce_sys'))  # merged: base and update files together

    def test_removing_the_view_leaves_the_dumps_whole(self):
        view = self.view()
        game_update.remove_view(view)
        self.assertFalse(view.exists())
        self.assertEqual((self.base / 'EXEC/WAD/A.WAD').read_text(), 'wad')
        self.assertEqual((self.base / 'sce_sys/icon0.png').read_text(), 'old icon')
        self.assertEqual((self.update / 'SCE_SYS/param.sfo').read_text(), 'new sfo')

    def test_dumps_are_not_modified(self):
        self.view()
        self.assertEqual((self.base / 'eboot.bin').read_text(), 'base eboot')
        self.assertEqual((self.base / 'sce_sys/icon0.png').read_text(), 'old icon')

    def test_a_network_share_without_junctions_mirrors_the_tree(self):
        # A network share (SMB) refuses symlinks and junctions alike; only hard links work.
        # create_view must still build a view of real files and leave the dumps untouched.
        with contextlib.ExitStack() as stack:
            for patcher in no_links_but_hard_links():
                stack.enter_context(patcher)
            view = self.view()
        self.assertEqual((view / 'eboot.bin').read_text(), 'update eboot')
        self.assertEqual((view / 'EXEC/WAD/A.WAD').read_text(), 'wad')          # mirrored, a real file
        self.assertEqual((view / 'sce_sys/icon0.png').read_text(), 'new icon')  # merged over the base
        self.assertEqual((view / 'sce_sys/param.sfo').read_text(), 'new sfo')   # added by the update
        # the mirrored files are hard links, not copies: the dumps stay exactly as they were
        self.assertEqual((self.base / 'EXEC/WAD/A.WAD').read_text(), 'wad')
        self.assertEqual((self.base / 'sce_sys/icon0.png').read_text(), 'old icon')
        game_update.remove_view(view)
        self.assertFalse(view.exists())
        self.assertEqual((self.base / 'EXEC/WAD/A.WAD').read_text(), 'wad')
        self.assertEqual((self.base / 'sce_sys/icon0.png').read_text(), 'old icon')

    def test_different_titles_and_nested_folders_are_refused(self):
        with self.assertRaises(ValueError):
            self.view(('CUSA01623', 'CUSA01715'))
        with mock.patch('patches.game_title_id', return_value='CUSA01623'):
            with self.assertRaises(ValueError):
                game_update.create_view(self.base, self.base / 'EXEC', self.out)
        self.assertEqual(list(self.out.iterdir()) if self.out.exists() else [], [])


if __name__ == '__main__':
    unittest.main()
