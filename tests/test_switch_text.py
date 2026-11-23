"""Weapon Switching tutorials for the switch attacks (scripts/switch_text.py) on a synthetic WAD."""
import struct
import tempfile
import unittest
from pathlib import Path

from paths import ROOT  # noqa: F401  (puts scripts/ on the path)
import switch_text
from glyphs import entries


def entry(name, payload=b'', tag=1):
    return (struct.pack('<HHI', tag, 0, len(payload)) + name.encode().ljust(56, b'\0')
            + payload.ljust((len(payload) + 15) & ~15, b'\0'))


OLD = 'Use [L1Button] and [XButton] to rapidly\r\nswitch weapons while attacking.'
TEXT = ('5384 7373\r\n*190*T\r\n[*1]Weapon Switching\r\nPress [RightButton] for Claws.\r\n' + OLD
        + '\r\n*191*\r\nPress [L1Button] to block.\r\n*283*T\r\n[*1]Weapon Switching\r\n' + OLD
        + '\r\n*284*\r\nOnyx\r\n*286*T\r\n[*1]Weapon Switching\r\nPress [LeftButton] for Whip.\r\n'
        + '*5000*\r\nHold to repeat attacks when using weapons.\r\n\0')  # every letter of the new lines


def wad(text=TEXT):
    return bytearray(entry('WAD_R_Lang', b'\0' * 8, tag=0x14) + entry('WAD_R_Lang', tag=0x12)
                     + entry('MSGS_TXT', text.encode(), tag=7) + entry('TXR_GoWIII_en', b'\xee' * 100)
                     + entry('SMF_0', b'SMF2' + b'\x11' * 20, tag=7))


class SwitchTextTest(unittest.TestCase):
    def test_cards_end_with_the_new_lines_and_what_follows_moves(self):
        data = wad()
        self.assertEqual(switch_text.patch(data, switch_text.ENGLISH), 2)  # 286 has no L1 line here
        found = {name: (offset, size) for offset, size, name in entries(data)}
        self.assertEqual(list(found), ['WAD_R_Lang', 'MSGS_TXT', 'TXR_GoWIII_en', 'SMF_0'])
        offset, size = found['MSGS_TXT']
        text = bytes(data[offset + 0x40:offset + 0x40 + size]).decode()
        self.assertEqual(text, TEXT.replace(OLD, '\r\n'.join(switch_text.ENGLISH)))
        self.assertIn('*191*\r\nPress [L1Button] to block.', text)  # not a Weapon Switching card
        self.assertEqual(text.count('\r\n'), TEXT.count('\r\n'))  # the same lines
        self.assertEqual(found['TXR_GoWIII_en'][0] % 16, 0)
        tail = found['SMF_0'][0]
        self.assertEqual(bytes(data[tail + 0x40:tail + 0x44]), b'SMF2')

    def test_any_wording_of_the_old_sentence(self):
        text = '1 1\r\n*1*\r\nHalte gedrückt, um bei Angriffen Wechselangriffe zu wiederholen.\r\n*283*T\r\n[*1]Waffenwechsel\r\nDrücke [DownButton] für den Cestus.\r\nMit [L1Button] und [XButton]\r\nwechselst du schnell\r\nim Angriff.\r\n*284*\r\nOnyx\r\n'
        new, count = switch_text.rewrite(text, switch_text.TEXT['DE'])
        self.assertEqual(count, 1)
        self.assertEqual(new, '1 1\r\n*1*\r\nHalte gedrückt, um bei Angriffen Wechselangriffe zu wiederholen.\r\n*283*T\r\n[*1]Waffenwechsel\r\nDrücke [DownButton] für den Cestus.\r\n'
                         + '\r\n'.join(switch_text.TEXT['DE']) + '\r\n*284*\r\nOnyx\r\n')

    def test_a_language_whose_font_lacks_a_new_character_keeps_its_text(self):
        text = TEXT.replace('rapidly', 'rapidement')  # no ç, é or ' in this "French" text
        self.assertEqual(switch_text.rewrite(text, switch_text.TEXT['FR']), (text, 0))
        data = wad('5384 7373\r\n*1*\r\nEmpty\r\n\0')
        self.assertEqual(switch_text.patch(data, switch_text.ENGLISH), 0)
        self.assertEqual(data, wad('5384 7373\r\n*1*\r\nEmpty\r\n\0'))

    def test_build_mounts_copies_and_never_writes_the_game(self):
        with tempfile.TemporaryDirectory() as temp:
            game, out = Path(temp) / 'game', Path(temp) / 'out'
            folder = game / switch_text.FOLDER
            folder.mkdir(parents=True)
            (folder / 'R_LANG_EN.WAD').write_bytes(wad())
            (folder / 'R_LANG_JA.WAD').write_bytes(wad())  # no Japanese characters: kept
            (folder / 'R_LANG_XX.WAD').write_bytes(wad())  # not a language of the game
            mounts = switch_text.build(game, out)
            self.assertEqual(list(mounts), ['/app0/EXEC/WAD/PS4_LE/R_LANG_EN.WAD'])
            self.assertIn(switch_text.ENGLISH[1].encode(), mounts['/app0/EXEC/WAD/PS4_LE/R_LANG_EN.WAD'].read_bytes())
            self.assertEqual((folder / 'R_LANG_EN.WAD').read_bytes(), wad())
            self.assertEqual(switch_text.build(game, out), mounts)  # cached


if __name__ == '__main__':
    unittest.main()
