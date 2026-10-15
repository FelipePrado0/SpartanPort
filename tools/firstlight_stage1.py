#!/usr/bin/env python3
"""Prepare/link CUSA00575 and optionally run the native import checker."""
from __future__ import annotations
import argparse
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/"out"

def run(cmd:list[str],check:bool=True)->int:
    print("+"," ".join(map(str,cmd)),flush=True)
    return subprocess.run(cmd,check=check).returncode

def probe_path()->Path|None:
    for name in ("firstlight-probe.exe","gow3-probe.exe","firstlight-probe","gow3-probe"):
        p=OUT/name
        if p.is_file():
            return p
    return None

def main()->int:
    p=argparse.ArgumentParser()
    p.add_argument("game_dir",type=Path)
    p.add_argument("--skip-hash",action="store_true")
    p.add_argument("--check-runtime",action="store_true")
    a=p.parse_args()
    game=a.game_dir.expanduser().resolve()

    cmd=[sys.executable,str(ROOT/"tools"/"validate_firstlight.py"),str(game)]
    if a.skip_hash:
        cmd.append("--skip-hash")
    run(cmd)

    OUT.mkdir(parents=True,exist_ok=True)
    run([sys.executable,str(ROOT/"scripts"/"prepare.py"),str(game),"--out",str(OUT)])
    run([sys.executable,str(ROOT/"scripts"/"link_modules.py"),str(game),"--out",str(OUT)])
    run([sys.executable,str(ROOT/"scripts"/"content_profile.py"),str(game),"--out",str(OUT),"--sku","full"])
    run([sys.executable,str(ROOT/"tools"/"check_firstlight_link.py"),str(OUT/"link.json")])

    if a.check_runtime:
        probe=probe_path()
        if not probe:
            print("Runtime checker is not built. Run build.ps1 or bash build.sh first.",file=sys.stderr)
            return 2
        return run([str(probe),str(OUT/"boot-linked.bin"),"--cpu-only","--check-imports",
                    "--content-profile",str(OUT/"content.bin"),"--app0",str(game)],check=False)
    return 0

if __name__=="__main__":
    raise SystemExit(main())
