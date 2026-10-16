"""link_modules.module on a synthetic SELF: reads the file once and reports its hash."""
import hashlib
import tempfile
import unittest
from pathlib import Path

from paths import ROOT  # noqa: F401  (sys.path for scripts/)
import link_modules
from test_link_libc import self_fixture


class ModuleTests(unittest.TestCase):
    def test_module_reads_the_file_once_and_reports_its_hash(self):
        data = self_fixture()
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'libc.prx'
            path.write_bytes(data)
            info = link_modules.module(path)
        self.assertEqual(info['sha256'], hashlib.sha256(data).hexdigest())
        self.assertEqual(info['missing'], [])
        self.assertEqual(info['symbols'][0]['identity'], ('bzQExy189ZI', ('libc', 1), ('libc', 1)))


if __name__ == '__main__':
    unittest.main()
