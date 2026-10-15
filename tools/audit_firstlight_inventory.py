#!/usr/bin/env python3
"""Audit a static CUSA00575 inventory against the current First Light port.

This tool never reads game binaries. It consumes the JSON inventory produced by the
static diagnostic tool and classifies eboot imports using this repository's current
runtime/name/GPU tables.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parents[1]
NAME_RE=re.compile(r'\{"([^"]+)","([^"]+)"\},?')
NID_RE=re.compile(r'"([^"]+#[^"]+)"')
EXPORT_RE=re.compile(r'\{"([^"]+)",\s*[A-Za-z_][A-Za-z0-9_]*\}')
GPU_RE=re.compile(r'LIB_FUNCTION\("([^"]+)"')
NATIVE_LIBS={"libc","libSceFios2","libSceLibcInternal"}


def import_names()->dict[str,str]:
    text=(ROOT/"src"/"import_names.inc").read_text(encoding="utf-8")
    return dict(NAME_RE.findall(text))


def runtime_tables()->tuple[set[str],set[str]]:
    direct:set[str]=set()
    symbols:set[str]=set()
    for path in sorted((ROOT/"src").glob("runtime*.c")):
        text=path.read_text(encoding="utf-8",errors="replace")
        direct.update(x for x in NID_RE.findall(text) if "#" in x)
        symbols.update(EXPORT_RE.findall(text))
    return direct,symbols


def gpu_nids()->set[str]:
    nids:set[str]=set()
    for path in (
        ROOT/"gpu"/"shadps4"/"core"/"libraries"/"gnmdriver"/"gnmdriver.cpp",
        ROOT/"gpu"/"shadps4"/"core"/"libraries"/"videoout"/"video_out.cpp",
        ROOT/"gpu"/"shadps4"/"core"/"libraries"/"kernel"/"equeue.cpp",
    ):
        if path.is_file():
            nids.update(GPU_RE.findall(path.read_text(encoding="utf-8",errors="replace")))
    return nids


def canonical(nid:str,library:str)->str:
    if library=="libScePosix":
        library="libkernel"
    return f"{nid}#{library}"


def classify(symbol:dict,names:dict[str,str],direct:set[str],exports:set[str],gpu:set[str])->tuple[str,str|None]:
    nid=symbol["nid"]
    library=symbol["library"]["name"]
    key=canonical(nid,library)
    name=names.get(key) or names.get(f"{nid}#{library}")

    if library in NATIVE_LIBS:
        return "native-module",name
    if nid in gpu and library in {"libSceGnmDriver","libSceVideoOut"}:
        return "gpu",name
    if key in direct:
        return "runtime-direct",name
    if name and name in exports:
        return "runtime-symbol",name
    if library=="libScePosix":
        return "posix-alias-unresolved",name
    if name:
        return "named-unresolved",name
    return "unknown-nid",None


def main()->int:
    p=argparse.ArgumentParser()
    p.add_argument("report",type=Path,help="Static inventory JSON; game binaries are not needed")
    p.add_argument("--json",dest="json_out",type=Path,help="Write machine-readable audit result")
    p.add_argument("--show-all",action="store_true",help="Print resolved imports too")
    a=p.parse_args()

    data=json.loads(a.report.read_text(encoding="utf-8"))
    if data.get("target_title_id")!="CUSA00575":
        print(f"ERROR: expected CUSA00575, got {data.get('target_title_id')!r}",file=sys.stderr)
        return 2
    eboot=next((f for f in data.get("files",[]) if f.get("path")=="eboot.bin"),None)
    if not eboot:
        print("ERROR: report has no eboot.bin entry",file=sys.stderr)
        return 2

    names=import_names()
    direct,exports=runtime_tables()
    gpu=gpu_nids()
    rows=[]
    counts=Counter()
    libs=defaultdict(Counter)

    for symbol in eboot.get("undefined_symbols",[]):
        category,name=classify(symbol,names,direct,exports,gpu)
        library=symbol["library"]["name"]
        key=f"{symbol['nid']}#{library}"
        row={
            "nid":symbol["nid"],
            "library":library,
            "key":key,
            "name":name,
            "category":category,
            "relocation_references":symbol.get("relocation_references",0),
        }
        rows.append(row)
        counts[category]+=1
        libs[library][category]+=1

    expected=eboot.get("referenced_import_count")
    print(f"Target: {data.get('target_title_id')}  eboot imports: {len(rows)}"
          +(f" (report says {expected})" if expected is not None else ""))
    for category,count in sorted(counts.items()):
        print(f"{category:24s} {count:4d}")

    blockers={"unknown-nid","named-unresolved","posix-alias-unresolved"}
    unresolved=[row for row in rows if row["category"] in blockers]
    print(f"\nHost-resolution blockers: {len(unresolved)}")
    for row in unresolved:
        print(f"{row['category']:24s} {row['key']}"
              +(f" -> {row['name']}" if row["name"] else ""))

    if a.show_all:
        print("\nAll imports:")
        for row in rows:
            print(f"{row['category']:24s} {row['key']}"
                  +(f" -> {row['name']}" if row["name"] else ""))

    result={
        "title_id":data.get("target_title_id"),
        "eboot_sha256":eboot.get("sha256"),
        "imports":len(rows),
        "counts":dict(sorted(counts.items())),
        "blockers":unresolved,
        "by_library":{lib:dict(sorted(c.items())) for lib,c in sorted(libs.items())},
    }
    if a.json_out:
        a.json_out.parent.mkdir(parents=True,exist_ok=True)
        a.json_out.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
        print(f"\nJSON: {a.json_out}")
    return 1 if unresolved else 0


if __name__=="__main__":
    raise SystemExit(main())
