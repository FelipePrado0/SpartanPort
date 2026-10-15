#!/usr/bin/env python3
"""Audit a static CUSA00575 inventory against the current firstlight_pc runtime.

This does not execute or modify the game. It classifies eboot imports as:
- native-linked: expected to bind to the game's libc/Fios2;
- gpu: NID registered by the vendored GNM/VideoOut tables;
- hle: named import with a matching runtime export;
- named-only: the NID is named but no runtime export was found;
- unknown-name: no NID->name mapping is present yet.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
IMPORT_PAIR=re.compile(r'\{"([^"]+)","([^"]+)"\},?')
RUNTIME_EXPORT=re.compile(r'\{"([^"]+)",\s*[A-Za-z_][A-Za-z0-9_]*\}')
GPU_NID=re.compile(r'LIB_FUNCTION\("([^"]+)"\s*,\s*"(libSce(?:GnmDriver|VideoOut))"')

NATIVE_LIBS={"libc","libSceFios2","libSceLibcInternal"}
GPU_LIBS={"libSceGnmDriver","libSceVideoOut"}

def import_names()->dict[str,str]:
    text=(ROOT/"src"/"import_names.inc").read_text(encoding="utf-8")
    return dict(IMPORT_PAIR.findall(text))

def runtime_exports()->set[str]:
    names=set()
    for path in (ROOT/"src").glob("runtime*.c"):
        names.update(RUNTIME_EXPORT.findall(path.read_text(encoding="utf-8",errors="replace")))
    return names

def gpu_nids()->set[tuple[str,str]]:
    out=set()
    for path in (
        ROOT/"gpu"/"shadps4"/"core"/"libraries"/"gnmdriver"/"gnmdriver.cpp",
        ROOT/"gpu"/"shadps4"/"core"/"libraries"/"videoout"/"video_out.cpp",
    ):
        for nid,lib in GPU_NID.findall(path.read_text(encoding="utf-8",errors="replace")):
            out.add((nid,lib))
    return out

def eboot_from(report:dict)->dict:
    for item in report.get("files",[]):
        if item.get("path")=="eboot.bin":
            return item
    raise ValueError("report has no eboot.bin entry")

def canonical(item:dict)->tuple[str,str,str]:
    nid=item.get("nid") or str(item.get("symbol","")).split("#",1)[0]
    lib=((item.get("library") or {}).get("name") or "")
    return nid,lib,f"{nid}#{lib}"

def main()->int:
    p=argparse.ArgumentParser()
    p.add_argument("report",type=Path,help="Static relatorio.json produced for CUSA00575")
    p.add_argument("--json",type=Path,help="Optional machine-readable audit output")
    a=p.parse_args()

    report=json.loads(a.report.read_text(encoding="utf-8"))
    if report.get("target_title_id")!="CUSA00575":
        raise SystemExit(f"expected CUSA00575 report, got {report.get('target_title_id')!r}")

    eboot=eboot_from(report)
    mappings=import_names()
    hle=runtime_exports()
    gpu=gpu_nids()

    rows=[]
    counts=Counter()
    by_library=defaultdict(Counter)

    for item in eboot.get("undefined_symbols",[]):
        nid,lib,key=canonical(item)
        name=mappings.get(key)
        if lib in NATIVE_LIBS:
            status="native-linked"
        elif lib in GPU_LIBS and (nid,lib) in gpu:
            status="gpu"
        elif not name:
            status="unknown-name"
        elif name in hle:
            status="hle"
        else:
            status="named-only"
        counts[status]+=1
        by_library[lib][status]+=1
        rows.append({
            "nid":nid,
            "library":lib,
            "key":key,
            "name":name,
            "status":status,
            "relocation_references":item.get("relocation_references",0),
        })

    print(f"Target: {report.get('target_title_id')}  imports={len(rows)}")
    for status in ("native-linked","gpu","hle","named-only","unknown-name"):
        print(f"{status:13s}: {counts[status]}")

    blockers=[r for r in rows if r["status"] in ("named-only","unknown-name")]
    if blockers:
        print("\nPotential Stage 1 blockers:")
        for row in blockers:
            label=row["name"] or row["key"]
            print(f" - {row['library']}: {label} [{row['status']}]")
    else:
        print("\nStatic audit found no obvious name/HLE gaps.")

    print("\nLibrary coverage:")
    for lib in sorted(by_library):
        summary=", ".join(f"{k}={v}" for k,v in sorted(by_library[lib].items()))
        print(f" - {lib}: {summary}")

    result={
        "target_title_id":report.get("target_title_id"),
        "total_imports":len(rows),
        "counts":dict(counts),
        "blockers":blockers,
        "imports":rows,
    }
    if a.json:
        a.json.parent.mkdir(parents=True,exist_ok=True)
        a.json.write_text(json.dumps(result,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
        print(f"\nJSON: {a.json}")

    return 1 if blockers else 0

if __name__=="__main__":
    raise SystemExit(main())
