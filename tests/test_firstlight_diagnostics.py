import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]

class FirstLightDiagnosticsTests(unittest.TestCase):
    def test_collector_sanitizes_and_extracts_blocker(self):
        with tempfile.TemporaryDirectory() as td:
            td=Path(td)
            log=td/"run.log"
            out=td/"diag.json"
            log.write_text(
                "game=C:\\Games\\CUSA00575\n"
                "Imports: 0 missing\n"
                "Fault: read access to 0x0 at guest offset 0x1234\n",
                encoding="utf-8")
            p=subprocess.run(
                [sys.executable,str(ROOT/"tools"/"collect_firstlight_diagnostics.py"),
                 str(log),"--output",str(out)],
                cwd=ROOT,text=True,capture_output=True)
            self.assertEqual(p.returncode,0,p.stdout+p.stderr)
            data=json.loads(out.read_text(encoding="utf-8"))
            self.assertEqual(data["target"]["title_id"],"CUSA00575")
            self.assertEqual(data["result"]["imports_missing"],0)
            self.assertIn("guest offset 0x1234",data["result"]["first_blocker"])
            rendered=json.dumps(data)
            self.assertNotIn("C:\\\\Games",rendered)

    def test_triage_defaults_cpu_only(self):
        text=(ROOT/"tools"/"firstlight_triage.py").read_text(encoding="utf-8")
        self.assertIn('"--mode","check"',text)
        self.assertIn('"--mode","cpu"',text)
        self.assertIn("if a.gpu:",text)

if __name__=="__main__":
    unittest.main()
