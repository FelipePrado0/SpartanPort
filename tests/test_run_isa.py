from paths import ROOT
import sys
import unittest

sys.path.insert(0, str(ROOT))
import run


class RunIsaTests(unittest.TestCase):
    def test_parse_cpu_features(self):
        self.assertEqual(run.parse_cpu_features('CPU: missing MOVBE, BMI1, LZCNT\n'), ['MOVBE', 'BMI1', 'LZCNT'])
        self.assertEqual(run.parse_cpu_features('CPU: complete\n'), [])

    def test_rewrite_wanted_follows_cpu_by_default(self):
        self.assertTrue(run.isa_rewrite_wanted(['MOVBE'], {}))
        self.assertFalse(run.isa_rewrite_wanted([], {}))

    def test_rewrite_forced(self):
        self.assertTrue(run.isa_rewrite_wanted([], {'SPARTAN_ISA_REWRITE': '1'}))

    def test_rewrite_disabled(self):
        self.assertFalse(run.isa_rewrite_wanted(['MOVBE', 'BMI1'], {'SPARTAN_ISA_REWRITE': '0'}))


if __name__ == '__main__':
    unittest.main()
