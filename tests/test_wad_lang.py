import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import wad_lang  # noqa: E402


def entry(name, payload, tag=7, flags=0):
    name_bytes = name.encode('ascii')
    header = struct.pack('<HHI', tag, flags, len(payload)) + name_bytes.ljust(56, b'\0')
    return header + payload + b'\0' * ((-len(payload)) % 16)


class WadLangTests(unittest.TestCase):
    def test_extract_and_replace_message_block_preserves_other_entries(self):
        original = entry('Before', b'keep') + entry('MSGS_TXT', b'English\r\n') + entry('After', b'keep2')
        self.assertEqual(wad_lang.extract_messages(original), b'English\r\n')

        changed = wad_lang.replace_messages(original, b'\xd9\x85\xd8\xaa\xd9\x86\r\n')
        self.assertEqual(wad_lang.extract_messages(changed), b'\xd9\x85\xd8\xaa\xd9\x86\r\n')
        self.assertEqual(list(wad_lang.entries(changed))[0][1:], (4, 'Before'))
        self.assertEqual(list(wad_lang.entries(changed))[2][1:], (5, 'After'))

    def test_missing_message_block_is_rejected(self):
        with self.assertRaises(ValueError):
            wad_lang.extract_messages(entry('Other', b'x'))

    def test_persian_launcher_language_is_available(self):
        sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'launcher'))
        import gow3_lang
        self.assertIn(('fa', ('فارسی',)), gow3_lang.LANGUAGE_NAMES)
        self.assertEqual(gow3_lang.table('fa')['Play'], 'بازی')


if __name__ == '__main__':
    unittest.main()
