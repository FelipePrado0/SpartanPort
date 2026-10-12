import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'scripts'))
from pkg_source import PkgPath, pkg_metadata
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'launcher'))
import gow3_launcher_win as launcher


def package_fixture(fields):
    keys, values, entries = bytearray(), bytearray(), []
    for name, value in fields.items():
        encoded = value.encode('utf-8') + b'\0'
        entries.append((len(keys), 0x204, len(encoded), len(encoded), len(values)))
        keys.extend(name.encode('ascii') + b'\0')
        values.extend(encoded)
    key_offset = 20 + 16 * len(entries)
    sfo = struct.pack('<4sIIII', b'\0PSF', 0x101, key_offset, key_offset + len(keys), len(entries))
    sfo += b''.join(struct.pack('<HHIII', *entry) for entry in entries) + keys + values
    header = bytearray(32)
    header[:4] = b'\x7fCNT'
    struct.pack_into('>I', header, 16, 1)
    struct.pack_into('>I', header, 24, 32)
    entry = struct.pack('>6I', 0x1000, 0, 0, 0, 64, len(sfo)) + bytes(8)
    return bytes(header) + entry + sfo


class PackageMetadataTests(unittest.TestCase):
    def test_launcher_recognizes_pkg_without_a_helper_session(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'game.pkg'
            path.write_bytes(package_fixture({'TITLE': 'God of War III™', 'APP_VER': '01.00'}))
            self.assertEqual(launcher.game_info(path), ('God of War III', '01.00'))
            path.write_bytes(b'not a package')
            self.assertIsNone(launcher.game_info(path))

    def test_reads_title_version_and_non_ascii_title(self):
        fields = {'TITLE_ID': 'CUSA01715', 'APP_VER': '01.00', 'TITLE': 'ゴッド・オブ・ウォー III'}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'game.pkg'
            path.write_bytes(package_fixture(fields))
            self.assertEqual(pkg_metadata(path), fields)

    def test_rejects_truncated_metadata_payload(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'game.pkg'
            path.write_bytes(package_fixture({'TITLE_ID': 'CUSA01715'})[:-1])
            with self.assertRaisesRegex(ValueError, 'Truncated PKG metadata'):
                pkg_metadata(path)

    def test_read_bytes_chunks_large_members_and_rejects_early_eof(self):
        payload = b'0123456789'
        reads = []
        class Client:
            def request(self, op, path, offset=0, count=0):
                if op == 1:
                    return struct.pack('<Iq', 8, len(payload))
                reads.append((offset, count))
                return payload[offset:offset + count]
        with patch('pkg_source.MAX_READ', 4):
            self.assertEqual((PkgPath(Client()) / 'eboot.bin').read_bytes(), payload)
        self.assertEqual(reads, [(0, 4), (4, 4), (8, 2)])
        class TruncatedClient(Client):
            def request(self, op, path, offset=0, count=0):
                return super().request(op, path, offset, count) if op == 1 else b''
        with self.assertRaisesRegex(OSError, 'Unexpected EOF'):
            (PkgPath(TruncatedClient()) / 'eboot.bin').read_bytes()

    def test_rejects_non_package_and_truncated_table(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'bad.pkg'
            path.write_bytes(b'not a package')
            with self.assertRaises(ValueError):
                pkg_metadata(path)
            header = bytearray(32)
            header[:4] = b'\x7fCNT'
            struct.pack_into('>I', header, 16, 2)
            struct.pack_into('>I', header, 24, 32)
            path.write_bytes(header)
            with self.assertRaisesRegex(ValueError, 'Truncated'):
                pkg_metadata(path)

    def test_virtual_path_and_relative_resources(self):
        root = PkgPath(None)
        file = root / 'EXEC/WAD/LEVEL.WAD'
        self.assertEqual(file.name, 'LEVEL.WAD')
        self.assertEqual(file.relative_to(root / 'EXEC').parts, ('WAD', 'LEVEL.WAD'))


if __name__ == '__main__':
    unittest.main()
