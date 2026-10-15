#!/usr/bin/env python3
"""Report host-runtime imports still unnamed after libc/Fios2 native linking."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
PAIR=re.compile(r'\{"([^"]+)","([^"]+)"\},?')

def import_names()->dict[str,str]:
    text=(ROOT/"src"/"import_names.inc").read_text(encoding="utf-8")
    return dict(PAIR.findall(text))

def main()->int:
    p=argparse.ArgumentParser()
    p.add_argument("link_json",type=Path)
    a=p.parse_args()
    report=json.loads(a.link_json.read_text(encoding="utf-8"))
    table=import_names()
    unresolved=report.get("unresolved_imports",[])
    unknown=[name for name in unresolved if name not in table]
    print(f"Native module bindings: {report.get('bindings',0)}")
    print(f"Host-runtime imports after linking: {len(unresolved)}")
    print(f"Named host imports: {len(unresolved)-len(unknown)}")
    print(f"Unknown names: {len(unknown)}")
    for name in unknown:
        print("UNKNOWN:",name)
    return 1 if unknown else 0

if __name__=="__main__":
    raise SystemExit(main())
