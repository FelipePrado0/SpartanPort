"""Exit to Desktop rows (scripts/pause_menu.py). The script is the game's own, so the patch is
checked on the dump's R_UI.WAD when GOW3_GAME_DIR points at one; the refusals need no files."""
import os
import struct
import tempfile
import unittest
from pathlib import Path

from paths import ROOT  # noqa: F401  (puts scripts/ on the path)
import pause_menu
from pause_menu import Script, decode

GAME = Path(os.environ.get('GOW3_GAME_DIR', '')) if os.environ.get('GOW3_GAME_DIR') else None


def wad(name, payload):
    return struct.pack('<HHI', 1, 0, len(payload)) + name.encode().ljust(56, b'\0') + payload.ljust(
        (len(payload) + 15) & ~15, b'\0')


def gotos(data, base, spans):
    """{code offset: index of the state it goes to} for the `47` instructions in `spans`."""
    s = Script(data, base)
    index = {r['at'] - 4: i for i, r in enumerate(s.records)}
    return {at - base - s.lit: index[int.from_bytes(arg, 'big')]
            for a, b in spans for at, op, arg in decode(data, base + s.lit + a, base + s.lit + b) if op == 0x47}


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
    def setUp(self):
        self.original = (GAME / pause_menu.WAD).read_bytes()
        self.data = bytearray(self.original)
        pause_menu.patch(self.data)
        entry = lambda d: next(o + 0x40 for o, _s, n in pause_menu.entries(d) if n == 'SMI_R_UI')  # noqa: E731
        self.base, self.old_base = entry(self.data), entry(self.original)
        self.old, self.new = Script(self.original, self.old_base), Script(self.data, self.base)

    def test_pause_row(self):
        main, row = self.new.one('PauseMain'), self.new.one('WadChoose')
        self.assertEqual(row['parent'], main['at'] - 4)
        self.assertEqual(main['at'] + main['size'], row['at'] + row['size'])
        confirm = next(v for k, v, t in self.new.blocks(row) if k == 3 and t == pause_menu.REPLY)
        self.assertIn(pause_menu.EXIT_CALL, self.new.block_bytes(confirm))
        with self.assertRaises(ValueError):  # a patched copy is refused, not patched twice
            pause_menu.patch(bytearray(self.data))

    def test_exit_asks_quit_games_question(self):
        quit_ = self.new.one('HLQuitGame')
        block = lambda row, trigger: self.new.block_bytes(pause_menu.transition(self.new, row, trigger))  # noqa: E731
        for row in (self.new.one('WadChoose'), self.new.by_name['HLDisplay'][1]):
            self.assertTrue(block(quit_, pause_menu.CONFIRM).startswith(block(row, pause_menu.CONFIRM)))  # the same dialog
            reply = block(row, pause_menu.REPLY)
            at = self.base + self.new.lit + next(v for k, v, t in self.new.blocks(row) if t == pause_menu.REPLY)
            code = decode(self.data, at, at + len(reply))
            jump = next(c for c in code if c[1] == 0x17)
            target = jump[0] + 3 + struct.unpack('>h', jump[2])[0]
            call = [c for c in code if c[1] == 0x13 and c[2] == bytes([0x0f, 0xff])]
            self.assertEqual([c[0] for c in call], [target])  # "yes" (any answer but 0) lands on the exit call
            self.assertEqual(reply[:len(pause_menu.REPLY_TEMPLATE) - 9], pause_menu.REPLY_TEMPLATE[:-9])  # up to the answer test

    def test_options_row(self):
        options, row = self.new.one('PauseOptions'), self.new.by_name['HLDisplay'][1]
        self.assertEqual(len(self.new.records), len(self.old.records) + 1)
        self.assertEqual(row['parent'], options['at'] - 4)
        self.assertEqual(options['size'], self.old.one('PauseOptions')['size'] + pause_menu.ROW_SIZE)
        self.assertEqual(row['at'], options['at'] + options['size'] - pause_menu.ROW_SIZE)
        # The WAD entry, the script header and the padding agree with the new size.
        size = struct.unpack_from('<I', self.data, self.base - 0x40 + 4)[0]
        old_size = struct.unpack_from('<I', self.original, self.old_base - 0x40 + 4)[0]
        self.assertEqual(struct.unpack_from('<I', self.data, self.base + 8)[0], size - 4)
        self.assertEqual(len(self.data) - len(self.original), ((size + 15) & ~15) - ((old_size + 15) & ~15))
        self.assertEqual([n for _o, _s, n in pause_menu.entries(self.data)][-1], 'GroupEnd')

    def test_every_other_goto_is_unchanged(self):
        dead = self.old.one('WadChoose')  # its code is replaced by the pause row's
        lo = min(v for k, v in dead['items'] if k in (1, 2, 3))
        hi = next(r for r in self.old.code_refs() if r > max(v for k, v in dead['items'] if k in (1, 2, 3)))
        spans = [(a, b) for a, b in self.old.spans() if not lo <= a < hi]
        before = gotos(self.original, self.old_base, spans)
        after = gotos(self.data, self.base, spans)
        options = self.new.one('PauseOptions')
        inserted = next(i for i, r in enumerate(self.new.records) if r['at'] == options['at'] + options['size'] - pause_menu.ROW_SIZE)
        moved = lambda i: i + (i >= inserted)  # noqa: E731  (states after the new record shift by one)
        changed = {at: after[at] for at, b in before.items() if after[at] != moved(b)}
        wad_choose = moved(next(i for i, r in enumerate(self.old.records) if r['at'] == dead['at']))
        # Continue up and Quit Game down go to the pause row; Controls up and Display down to the options row.
        self.assertEqual(sorted(changed.values()), sorted([wad_choose, wad_choose, inserted, inserted]))


if __name__ == '__main__':
    unittest.main()
