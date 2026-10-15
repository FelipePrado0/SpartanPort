#!/usr/bin/env python3
"""Run the safe CUSA00575 check + CPU bring-up sequence and emit a diagnostic report."""
from __future__ import annotations
import argparse
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]

def call(args:list[str])->int:
    print("+"," ".join(args),flush=True)
    return subprocess.run(args,cwd=ROOT).returncode

def main()->int:
    p=argparse.ArgumentParser()
    p.add_argument("game_dir",type=Path)
    p.add_argument("--timeout",type=int,default=10)
    p.add_argument("--gpu",action="store_true",
                   help="After CPU triage, also attempt the guarded experimental Vulkan boot")
    a=p.parse_args()

    launcher=[sys.executable,str(ROOT/"run_firstlight.py"),str(a.game_dir)]
    code=call([*launcher,"--mode","check"])
    if code:
        call([sys.executable,str(ROOT/"tools"/"collect_firstlight_diagnostics.py")])
        return code

    code=call([*launcher,"--mode","cpu","--skip-prepare","--timeout",str(max(0,a.timeout))])
    call([sys.executable,str(ROOT/"tools"/"collect_firstlight_diagnostics.py")])
    if code and not a.gpu:
        return code

    if a.gpu:
        gpu=call([*launcher,"--mode","gpu","--skip-prepare","--timeout",str(max(0,a.timeout))])
        call([sys.executable,str(ROOT/"tools"/"collect_firstlight_diagnostics.py")])
        return gpu
    return code

if __name__=="__main__":
    raise SystemExit(main())
