#!/usr/bin/env python3
"""Extract the useful stopping point from user/last_run.log."""
from __future__ import annotations
import argparse
from pathlib import Path

IMPORTANT=("MISSING:","Imports:","STOP:","Fault:","Guest fault","GPU:","Runtime:","Game profile:")

def main()->int:
    p=argparse.ArgumentParser()
    p.add_argument("log",nargs="?",type=Path,default=Path("user/last_run.log"))
    p.add_argument("--output",type=Path,default=Path("user/last_run_summary.txt"))
    a=p.parse_args()
    if not a.log.is_file():
        print(f"Log not found: {a.log}")
        return 2
    lines=a.log.read_text(encoding="utf-8",errors="replace").splitlines()
    chosen=[]
    for i,line in enumerate(lines):
        if any(key in line for key in IMPORTANT):
            start=max(0,i-1)
            end=min(len(lines),i+(30 if ("Fault:" in line or "Guest fault" in line) else 4))
            chosen.extend(lines[start:end])
    out=[]; seen=set()
    for line in chosen:
        if line not in seen:
            seen.add(line); out.append(line)
    if not out:
        out=lines[-80:]
    text="\n".join(out).rstrip()+"\n"
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(text,encoding="utf-8")
    print(text,end="")
    print(f"Summary: {a.output}")
    return 0

if __name__=="__main__":
    raise SystemExit(main())
