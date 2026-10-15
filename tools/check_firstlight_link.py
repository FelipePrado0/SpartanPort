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
    native_libraries=("libc","libSceFios2","libSceLibcInternal")
    native_leaks=[
        name for name in unresolved
        if any(name.endswith("#"+library) for library in native_libraries)
    ]
    expected_tls={"libc.prx":14,"libSceFios2.prx":2}
    tls_mismatch=[]
    for module in report.get("modules",[]):
        filename=module.get("file")
        if filename in expected_tls and module.get("tls_relocations")!=expected_tls[filename]:
            tls_mismatch.append((filename,module.get("tls_relocations"),expected_tls[filename]))

    print(f"Native module bindings: {report.get('bindings',0)}")
    print(f"Host-runtime imports after linking: {len(unresolved)}")
    print(f"Named host imports: {len(unresolved)-len(unknown)}")
    print(f"Unknown names: {len(unknown)}")
    print(f"Native imports left unresolved: {len(native_leaks)}")
    for name in unknown:
        print("UNKNOWN:",name)
    for name in native_leaks:
        print("NATIVE-UNRESOLVED:",name)
    for filename,actual,expected in tls_mismatch:
        print(f"TLS-MISMATCH: {filename}: {actual}, expected {expected}")
    return 1 if unknown or native_leaks or tls_mismatch else 0

if __name__=="__main__":
    raise SystemExit(main())
