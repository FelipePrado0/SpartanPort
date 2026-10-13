from paths import ROOT
import struct
import unittest
import tempfile
from pathlib import Path
from prepare import parse_self, inspect_libc, load_sfo, nid, prepare, read_self, read_sfo, sfo, write_report


def fixture():
    data = bytearray(0x210)
    data[:4] = b'O\x15=\x1d'
    struct.pack_into('<H', data, 24, 1)
    struct.pack_into('<QQQQ', data, 32, 0x800, 0x200, 16, 16)
    struct.pack_into('<16sHHIQQQIHHHHHH', data, 64,
                     b'\x7fELF\x02\x01\x01\x09', 0xfe10, 62, 1, 0, 64, 0, 0, 64, 56, 1, 0, 0, 0)
    struct.pack_into('<IIQQQQQQ', data, 128, 1, 5, 0x1000, 0, 0, 16, 32, 0x1000)
    data[0x200:] = bytes(range(16))
    return data


class SelfTests(unittest.TestCase):
    def test_libc_ret_contract_is_verified_from_symbol_and_code(self):
        def libc(instruction):
            data=bytearray(0x320)
            data[:4]=b'O\x15=\x1d'
            struct.pack_into('<H',data,24,2)
            struct.pack_into('<QQQQ',data,32,0x800,0x200,16,16)
            struct.pack_into('<QQQQ',data,64,0x100800,0x220,256,256)
            struct.pack_into('<16sHHIQQQIHHHHHH',data,96,b'\x7fELF\x02\x01\x01\x09',0xfe18,62,1,0,64,0,0,64,56,3,0,0,0)
            struct.pack_into('<IIQQQQQQ',data,160,1,5,0x1000,0,0,16,16,0x1000)
            struct.pack_into('<IIQQQQQQ',data,216,0x61000000,4,0x2000,0,0,256,0,16)
            struct.pack_into('<IIQQQQQQ',data,272,2,4,0x20b0,0,0,80,80,8)
            data[0x200]=instruction
            name=b'bzQExy189ZI#C#A\0'
            data[0x220:0x220+len(name)]=name
            struct.pack_into('<IBBHQQ',data,0x260,0,0x12,0,1,0,1)
            for i,(tag,value) in enumerate([(0x61000035,0),(0x61000037,32),(0x61000039,64),(0x6100003f,24),(0,0)]):
                struct.pack_into('<QQ',data,0x2d0+16*i,tag,value)
            return data
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'libc.prx'
            path.write_bytes(libc(0xc3))
            proof=inspect_libc(path)
            self.assertTrue(proof['init_env_is_ret'])
            self.assertEqual(proof['bytes'],'c3')
            path.write_bytes(libc(0x90))
            self.assertFalse(inspect_libc(path)['init_env_is_ret'])
        self.assertEqual(nid('_init_env'),'bzQExy189ZI')

    def test_report_with_non_latin_title_is_written_as_utf8(self):
        title = 'ゴッド・オブ・ウォーIII リマスタード'
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'analysis.json'
            write_report(path, dict(sfo=dict(TITLE=title)))
            self.assertIn(title, path.read_bytes().decode('utf-8'))

    def test_file_offset_is_not_virtual_address(self):
        elf, header, ph, segments, missing = parse_self(fixture())
        self.assertEqual(elf[0x1000:0x1010], bytes(range(16)))
        self.assertEqual(ph[0]['vaddr'], 0)
        self.assertEqual(missing, [])

    def test_truncated_payload_rejected(self):
        with self.assertRaisesRegex(ValueError, 'out of bounds'):
            parse_self(fixture()[:-1])

    def test_encrypted_or_compressed_payload_rejected(self):
        for flag in (2, 8):
            data = fixture()
            struct.pack_into('<Q', data, 32, 0x800 | flag)
            with self.assertRaisesRegex(ValueError, 'encrypted/compressed'):
                parse_self(data)

    def test_missing_required_segment_rejected(self):
        data = fixture()
        struct.pack_into('<Q', data, 32, 0)
        with self.assertRaisesRegex(ValueError, 'required segment'):
            parse_self(data)

    def test_bad_program_header_index_rejected(self):
        data = fixture()
        struct.pack_into('<Q', data, 32, 0x100800)
        with self.assertRaisesRegex(ValueError, 'segment index'):
            parse_self(data)

    def test_empty_game_files_are_named(self):
        # An empty eboot.bin (a broken copy, or a placeholder made by hand) failed as
        # "out of bounds: 0x0+0x4 / 0x0", which names neither the file nor the cause.
        with tempfile.TemporaryDirectory() as tmp:
            game = Path(tmp)
            (game / 'eboot.bin').write_bytes(b'')
            with self.assertRaisesRegex(ValueError, 'eboot.bin is empty'):
                prepare(game, game / 'out')
            # Every SELF the port loads goes through read_self, so a prx is named the same way.
            (game / 'libc.prx').write_bytes(b'')
            with self.assertRaisesRegex(ValueError, 'libc.prx is empty'):
                inspect_libc(game / 'libc.prx')
            with self.assertRaisesRegex(ValueError, 'libc.prx is empty'):
                read_self(game / 'libc.prx')
            # load_sfo tells a file that cannot be read (OSError) from one that is not a param.sfo
            # (ValueError); read_sfo answers None for both instead of raising.
            (game / 'param.sfo').write_bytes(b'x')
            with self.assertRaises(ValueError):
                load_sfo(game / 'param.sfo')
            with self.assertRaises(OSError):
                load_sfo(game / 'none.sfo')
            self.assertIsNone(read_sfo(game / 'param.sfo'))
            self.assertIsNone(read_sfo(game / 'none.sfo'))
            self.assertIsNone(read_sfo(game))
        with self.assertRaisesRegex(ValueError, 'param.sfo is empty'):
            sfo(b'')
        with self.assertRaisesRegex(ValueError, 'empty SELF'):
            parse_self(b'')


if __name__ == '__main__':
    unittest.main()
