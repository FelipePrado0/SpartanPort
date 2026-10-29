"""tools/game_manifest.py on a synthetic game tree (docs/specs/2026-10-29-load-game-manifest.md)."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import paths

TOOL = paths.ROOT / 'tools/game_manifest.py'


def run(*args):
    return subprocess.run([sys.executable, '-I', str(TOOL), *map(str, args)],
                          capture_output=True, text=True, timeout=60)


def sha(data):
    return hashlib.sha256(data).hexdigest()


class GameManifestTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.tmp = Path(temporary.name)
        self.game = self.tmp / 'CUSA01623'
        self.files = {
            'eboot.bin': b'synthetic executable',
            'sce_sys/param.sfo': b'synthetic sfo',
            'GAMEDATA/HDS690.WAD': bytes(range(256)) * 9000,  # > 1 MiB: several read blocks
            'GAMEDATA/empty.wad': b'',
        }
        for name, data in self.files.items():
            path = self.game / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)

    def create(self, *extra, root=None):
        result = run('create', root or self.game, *extra)
        self.assertIn(result.returncode, (0, 1), result.stderr)
        return result

    def test_create_lists_every_file_with_size_and_sha256(self):
        result = self.create()
        self.assertEqual(result.returncode, 0, result.stderr)
        manifest = json.loads(result.stdout)
        self.assertEqual(manifest['format'], 'spartanport-game-manifest')
        self.assertEqual(manifest['version'], 1)
        self.assertEqual(manifest['root'], 'CUSA01623')
        self.assertNotIn(str(self.tmp), result.stdout)  # no absolute (personal) path
        expected = [{'path': name, 'size': len(data), 'sha256': sha(data)}
                    for name, data in sorted(self.files.items())]
        self.assertEqual(manifest['files'], expected)
        self.assertEqual(manifest['totals'], {'files': 4, 'bytes': sum(map(len, self.files.values())), 'errors': 0})
        self.assertEqual(manifest['errors'], [])

    def test_create_writes_output_file_and_text_format(self):
        out = self.tmp / 'manifest.txt'
        result = self.create('--format', 'text', '-o', out)
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = [line for line in out.read_text(encoding='utf-8').splitlines() if not line.startswith('#')]
        self.assertEqual(lines[0], f"{sha(self.files['GAMEDATA/HDS690.WAD'])} {len(self.files['GAMEDATA/HDS690.WAD'])} GAMEDATA/HDS690.WAD")
        self.assertEqual(len(lines), 4)

    def test_exclude_and_quick(self):
        manifest = json.loads(self.create('--quick', '--exclude', 'sce_sys/*').stdout)
        self.assertIsNone(manifest['hash'])
        self.assertEqual([f['path'] for f in manifest['files']], ['GAMEDATA/HDS690.WAD', 'GAMEDATA/empty.wad', 'eboot.bin'])
        self.assertTrue(all(f['sha256'] is None for f in manifest['files']))

    @unittest.skipIf(os.name == 'nt' or (hasattr(os, 'geteuid') and os.geteuid() == 0), 'needs POSIX permissions without root')
    def test_unreadable_file_is_recorded_and_exit_is_1(self):
        locked = self.game / 'locked.wad'
        locked.write_bytes(b'x')
        locked.chmod(0)
        self.addCleanup(locked.chmod, 0o644)
        result = self.create()
        self.assertEqual(result.returncode, 1)
        manifest = json.loads(result.stdout)
        self.assertEqual([e['path'] for e in manifest['errors']], ['locked.wad'])
        self.assertEqual(manifest['totals']['errors'], 1)

    def test_read_error_is_recorded_in_process(self):
        # Same path as above for runs as root (permissions do not stop root's reads).
        import importlib.util
        from unittest import mock
        spec = importlib.util.spec_from_file_location('game_manifest', TOOL)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        real = module.file_sha256

        def flaky(path):
            if path.endswith('HDS690.WAD'):
                raise PermissionError(13, 'Permission denied', path)
            return real(path)
        with mock.patch.object(module, 'file_sha256', flaky):
            manifest = module.create(str(self.game))
        self.assertEqual(manifest['errors'], [{'path': 'GAMEDATA/HDS690.WAD', 'error': 'Permission denied'}])
        self.assertEqual(manifest['totals']['files'], 3)

    def test_compare_reports_missing_extra_different_and_case(self):
        copy = self.tmp / 'reextracted'
        for name, data in self.files.items():
            path = copy / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        os.remove(copy / 'GAMEDATA/empty.wad')                      # missing in B
        (copy / 'GAMEDATA/new.wad').write_bytes(b'new')              # extra in B
        (copy / 'GAMEDATA/HDS690.WAD').write_bytes(b'truncated')     # different
        os.rename(copy / 'eboot.bin', copy / 'EBOOT.BIN')            # case only
        a = self.tmp / 'a.json'
        a.write_text(self.create().stdout, encoding='utf-8')
        result = run('compare', a, copy, '--format', 'json')
        self.assertEqual(result.returncode, 1, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report['missing'], ['GAMEDATA/empty.wad'])
        self.assertEqual(report['extra'], ['GAMEDATA/new.wad'])
        self.assertEqual([d['path'] for d in report['different']], ['GAMEDATA/HDS690.WAD'])
        self.assertEqual(report['case_only'], [{'a': 'eboot.bin', 'b': 'EBOOT.BIN'}])
        text = run('compare', a, copy)
        self.assertEqual(text.returncode, 1)
        self.assertIn('missing GAMEDATA/empty.wad', text.stdout)
        self.assertIn('different GAMEDATA/HDS690.WAD', text.stdout)

    def test_compare_identical_and_quick(self):
        a = self.tmp / 'a.txt'
        a.write_text(self.create('--format', 'text').stdout, encoding='utf-8')
        same = run('compare', a, self.game)
        self.assertEqual(same.returncode, 0, same.stdout + same.stderr)
        self.assertIn('identical', same.stdout)
        quick = self.tmp / 'quick.json'
        quick.write_text(self.create('--quick').stdout, encoding='utf-8')
        report = run('compare', a, quick, '--format', 'json')
        self.assertEqual(report.returncode, 0)
        self.assertEqual(len(json.loads(report.stdout)['unhashed']), 4)

    def test_invalid_input_exits_2(self):
        bad = self.tmp / 'bad.json'
        bad.write_text('{"format": "something else"}', encoding='utf-8')
        result = run('compare', bad, self.game)
        self.assertEqual(result.returncode, 2)
        self.assertIn('not a game manifest', result.stderr)
        self.assertEqual(run('create', self.tmp / 'nothing-here').returncode, 2)


if __name__ == '__main__':
    unittest.main()
