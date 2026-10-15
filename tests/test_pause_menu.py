"""Exit to Desktop row (scripts/pause_menu.py). The script is the game's own, so the patch is
checked on the dump's R_UI.WAD when GOW3_GAME_DIR points at one; the refusals need no files."""
import os
import struct
import tempfile
import unittest
from pathlib import Path

from paths import ROOT  # noqa: F401  (puts scripts/ on the path)
import pause_menu

GAME = Path(os.environ.get('GOW3_GAME_DIR', '')) if os.environ.get('GOW3_GAME_DIR') else None


def wad(name, payload):
    return struct.pack('<HHI', 1, 0, len(payload)) + name.encode().ljust(56, b'\0') + payload.ljust(
        (len(payload) + 15) & ~15, b'\0')


class Refusals(unittest.TestCase):
    def test_wad_without_the_script(self):
        with self.assertRaises(ValueError):
            pause_menu.patch(bytearray(wad('TXRX_R_UI', b'x' * 8)))

    def test_build_leaves_no_copy(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / 'R_UI.WAD'
            source.write_bytes(wad('TXRX_R_UI', b'x' * 8))
            self.assertEqual(pause_menu.build(folder, folder, source), {})
            self.assertFalse((Path(folder) / 'pause_menu' / 'R_UI.WAD').exists())


@unittest.skipUnless(GAME and (GAME / pause_menu.WAD).is_file(), 'needs GOW3_GAME_DIR with the game')
class GameScript(unittest.TestCase):
    def test_exit_row(self):
        data = bytearray((GAME / pause_menu.WAD).read_bytes())
        size = len(data)
        pause_menu.patch(data)
        self.assertEqual(len(data), size)  # nothing moves
        base = next(o + 0x40 for o, _s, n in pause_menu.entries(data) if n == 'SMI_R_UI')
        script = pause_menu.Script(data, base)  # the state tree still parses up to the code
        main, row = script.one('PauseMain'), script.one('WadChoose')
        self.assertEqual(row['parent'], main['at'] - 4)
        self.assertEqual(main['at'] + main['size'], row['at'] + row['size'])
        confirm = next(v for k, v, t in script.blocks(row) if k == 3 and t[0] == pause_menu.CONFIRM[0])
        self.assertIn(pause_menu.EXIT_CALL, script.block_bytes(confirm))
        with self.assertRaises(ValueError):  # a patched copy is refused, not patched twice
            pause_menu.patch(data)


if __name__ == '__main__':
    unittest.main()
