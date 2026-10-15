import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]

class InventoryAuditTests(unittest.TestCase):
    def test_minimal_report_classification(self):
        report={
            "target_title_id":"CUSA00575",
            "files":[{
                "path":"eboot.bin",
                "undefined_symbols":[
                    {"nid":"BtqmpTRXHgk","library":{"name":"libSceRtc"},"relocation_references":1},
                    {"nid":"Up36PTk687E","library":{"name":"libSceVideoOut"},"relocation_references":1},
                    {"nid":"Q3VBxCXhUHs","library":{"name":"libc"},"relocation_references":1},
                ]
            }]
        }
        with tempfile.TemporaryDirectory() as td:
            path=Path(td)/"report.json"
            out=Path(td)/"audit.json"
            path.write_text(json.dumps(report),encoding="utf-8")
            p=subprocess.run(
                [sys.executable,str(ROOT/"tools"/"audit_firstlight_report.py"),str(path),"--json",str(out)],
                cwd=ROOT,text=True,capture_output=True
            )
            self.assertEqual(p.returncode,0,p.stdout+p.stderr)
            data=json.loads(out.read_text(encoding="utf-8"))
            statuses={row["library"]:row["status"] for row in data["imports"]}
            self.assertEqual(statuses["libSceRtc"],"hle")
            self.assertEqual(statuses["libSceVideoOut"],"gpu")
            self.assertEqual(statuses["libc"],"native-linked")

if __name__=="__main__":
    unittest.main()
