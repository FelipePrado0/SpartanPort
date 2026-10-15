import unittest

import paths  # noqa: F401  (puts scripts/ on sys.path)
import guest_memory

RAM_16_GB = 16303  # GlobalMemoryStatusEx on the reported 16 GB PC


class GuestMemoryTest(unittest.TestCase):
    def test_pool_matches_runtime_memory(self):
        self.assertEqual(guest_memory.pool_mb(None), 5056 + 1024)
        self.assertEqual(guest_memory.pool_mb('11200'), 11200 + 1024)
        # runtime_memory.c ignores values out of range and keeps the retail size.
        self.assertEqual(guest_memory.pool_mb('4000'), 5056 + 1024)
        self.assertEqual(guest_memory.pool_mb('20000'), 5056 + 1024)
        self.assertEqual(guest_memory.pool_mb('abc'), 5056 + 1024)

    def test_reported_pc(self):
        # Black screen: 4K with stream buffers in system RAM.
        warning = guest_memory.memory_warning('11200', RAM_16_GB, 'cached')
        self.assertIn('12224 MiB', warning)
        self.assertIn('GPU memory', warning)
        # Ran: 4K with stream buffers in GPU memory, 1440p and native with the default.
        self.assertIsNone(guest_memory.memory_warning('11200', RAM_16_GB, 'device'))
        self.assertIsNone(guest_memory.memory_warning('8128', RAM_16_GB, 'cached'))
        self.assertIsNone(guest_memory.memory_warning(None, RAM_16_GB, 'cached'))

    def test_device_advice_is_not_repeated(self):
        warning = guest_memory.memory_warning('11200', 12000, 'device')
        self.assertNotIn('GPU memory', warning)
        self.assertIn('page file', warning)

    def test_enough_ram_is_silent(self):
        self.assertIsNone(guest_memory.memory_warning('11200', 32000, 'host'))

    def test_default_patches_are_silent_on_8_gb(self):
        # The texture fix (on by default) and the 480p/720p patches ask for 5056 + 2048 MiB.
        self.assertIsNone(guest_memory.memory_warning(None, 8000, 'cached'))
        self.assertIsNone(guest_memory.memory_warning('7104', 8000, 'cached'))

    def test_1440p_warns_on_8_gb(self):
        self.assertIn('9152 MiB', guest_memory.memory_warning('8128', 8000, 'cached'))

    def test_unknown_ram_is_silent(self):
        self.assertIsNone(guest_memory.memory_warning('11200', None))

    def test_unknown_stream_memory_counts_as_cached(self):
        self.assertEqual(guest_memory.memory_warning('11200', RAM_16_GB, 'other'),
                         guest_memory.memory_warning('11200', RAM_16_GB, 'cached'))


if __name__ == '__main__':
    unittest.main()
