#!/usr/bin/env python3
"""Build a shareable First Light diagnostic report without game files or absolute paths."""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import platform
import re
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
DEFAULT_LOG=ROOT/"user"/"last_run.log"
TARGET=ROOT/"targets"/"CUSA00575.json"

IMPORTANT=("MISSING:","Imports:","STOP:","Fault:","Guest fault","GPU:","Runtime:",
           "Game profile:","Original guest entry","Native libc initialization")

def git(args:list[str])->str|None:
    try:
        return subprocess.check_output(["git",*args],cwd=ROOT,text=True,
                                       stderr=subprocess.DEVNULL).strip()
    except Exception:
        return None

def sanitize(line:str)->str:
    # Do not leak the user's local game/repository path in a report intended for sharing.
    root=str(ROOT)
    if root:
        line=line.replace(root,"<repo>")
    line=re.sub(r'(?i)(?:[A-Z]:\\|/)(?:[^\s"\']+[\\/])+CUSA00575',"<game>/CUSA00575",line)
    return line

def excerpt(lines:list[str])->list[str]:
    selected=[]
    for i,line in enumerate(lines):
        if any(key in line for key in IMPORTANT):
            selected.extend(lines[max(0,i-1):min(len(lines),i+12)])
    if not selected:
        selected=lines[-100:]
    out=[]; seen=set()
    for line in selected:
        clean=sanitize(line)
        if clean not in seen:
            seen.add(clean); out.append(clean)
    return out[-240:]

def main()->int:
    p=argparse.ArgumentParser()
    p.add_argument("log",nargs="?",type=Path,default=DEFAULT_LOG)
    p.add_argument("--output",type=Path,default=ROOT/"user"/"diagnostic.json")
    a=p.parse_args()
    if not a.log.is_file():
        print(f"Log not found: {a.log}",file=sys.stderr)
        return 2

    target=json.loads(TARGET.read_text(encoding="utf-8"))
    lines=a.log.read_text(encoding="utf-8",errors="replace").splitlines()
    imports=None
    blockers=[]
    for line in lines:
        m=re.search(r"Imports:\s*(\d+)\s+missing",line)
        if m:
            imports=int(m.group(1))
        if any(key in line for key in ("STOP:","Fault:","Guest fault")):
            blockers.append(sanitize(line))

    report={
        "schema_version":1,
        "created_utc":datetime.now(timezone.utc).isoformat(),
        "target":{
            "title":target["title"],
            "title_id":target["title_id"],
            "app_ver":target["app_ver"],
            "entry_point":target["executable"]["entry_point"],
            "eboot_sha256":target["executable"]["sha256"],
        },
        "port":{
            "commit":git(["rev-parse","HEAD"]),
            "branch":git(["branch","--show-current"]),
            "dirty":bool(git(["status","--porcelain"])),
        },
        "host":{
            "system":platform.system(),
            "release":platform.release(),
            "machine":platform.machine(),
            "python":platform.python_version(),
        },
        "result":{
            "imports_missing":imports,
            "first_blocker":blockers[0] if blockers else None,
            "blockers":blockers[:16],
        },
        "log_excerpt":excerpt(lines),
        "privacy":"No game binaries, absolute repository path, or full game path are included.",
    }
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    digest=hashlib.sha256(a.output.read_bytes()).hexdigest()
    print(f"Diagnostic: {a.output}")
    print(f"SHA256: {digest}")
    if imports is not None:
        print(f"Imports missing: {imports}")
    if blockers:
        print(f"First blocker: {blockers[0]}")
    return 0

if __name__=="__main__":
    raise SystemExit(main())
