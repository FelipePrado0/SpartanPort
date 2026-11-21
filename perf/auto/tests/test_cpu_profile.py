import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import cpu_profile  # noqa: E402


class CpuProfileTest(unittest.TestCase):
    def test_aggregates_functions_per_thread(self):
        with tempfile.NamedTemporaryFile("w", suffix=".tsv", delete=False, encoding="utf-8") as f:
            f.write("spartanport:DrawRec\tgame.exe\t100\t30\n"
                    "spartanport:DrawRec\tgame.exe\t104\t10\n"
                    "spartanport:DrawRec\tntdll.dll\t5000\t20\n"
                    "WorkerThread1\t?\t10a640d\t5\n"
                    "spartanport:DrawRec\tvia:game.exe\t100\t20\n"
                    "broken line\n")
        try:
            rows = cpu_profile.read_samples(f.name)
        finally:
            os.unlink(f.name)
        self.assertEqual(len(rows), 5)
        names = {0x100: "Rasterizer::DrawRecord", 0x104: "Rasterizer::DrawRecord"}
        per_thread = cpu_profile.aggregate(rows, names, "game.exe")
        self.assertEqual(per_thread["spartanport:DrawRec"]["Rasterizer::DrawRecord"], 40)
        self.assertEqual(per_thread["spartanport:DrawRec"]["ntdll.dll"], 20)
        self.assertEqual(per_thread["WorkerThread1"]["(guest code)"], 5)
        text = cpu_profile.report(per_thread, 5)
        self.assertIn("== spartanport:DrawRec: 60 samples", text)
        self.assertIn(" 66.7%  Rasterizer::DrawRecord", text)
        self.assertIn(" 33.3%  (called from) Rasterizer::DrawRecord", text)


if __name__ == "__main__":
    unittest.main()
