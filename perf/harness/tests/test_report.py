"""L0 tests of perf/harness/report.py with synthetic results: python -m unittest discover -s perf/harness/tests"""
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import report  # noqa: E402


def result(scenario, cache, fps, lo, hi, target=None):
    d = {"scenario": scenario, "cache": cache, "runs": 3,
         "summary": {"fps_median": fps, "p99_frametime_ms": 9.0, "frames_over_40ms": 0, "window_compiles": 2},
         "spread": {"fps_median": [lo, hi]}, "machine": {"gpu": "GPU A"}, "build": "abc1234"}
    if target:
        d["target"] = target
    return d


class Report(unittest.TestCase):
    def test_table_ratio_and_missing_target(self):
        with tempfile.TemporaryDirectory() as tmp:
            for name, d in {"a.json": result("cena", "warm", 190.0, 188.0, 193.0),
                            "b.json": result("cena", "warm", 200.0, 199.0, 201.0, "rehooked"),
                            "c.json": result("troca", "warm", 120.0, 110.0, 130.0)}.items():
                (Path(tmp) / name).write_text(json.dumps(d))
            text = report.render(report.load([Path(tmp)]))
        self.assertIn("| cena | warm | 190.0 (188.0 a 193.0) | 200.0 (199.0 a 201.0) | 0.95 |", text)
        self.assertIn("| troca | warm | 120.0 (110.0 a 130.0) | sem dados | - |", text)
        self.assertIn("GPU A", text)

    def test_partial_spread_and_missing_cache(self):
        d = result("x", "warm", 100.0, 90.0, None)
        self.assertEqual(report._cell(d), "100.0")
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / "a.json").write_text(json.dumps({"scenario": "x", "summary": {}}))
            self.assertEqual(report.load([Path(tmp)]), [])


if __name__ == "__main__":
    unittest.main()
