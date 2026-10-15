#!/usr/bin/env python3
"""CUSA00575 diagnostic launcher for firstlight_pc."""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parent
OUT=ROOT/"out"
USER=ROOT/"user"
LOG=USER/"last_run.log"

def safe_env()->dict[str,str]:
    env=dict(os.environ)
    if os.name=="nt":
        for extra in (Path(r"C:\msys64\clang64\bin"),Path(r"C:\msys64\usr\bin")):
            if extra.is_dir() and str(extra).lower() not in env.get("PATH","").lower():
                env["PATH"]=str(extra)+os.pathsep+env.get("PATH","")
    defaults={
        # Upstream compatibility variable names remain GOW3_* internally during bring-up.
        "GOW3_NO_MSAA":"0",
        "GOW3_UPSCALER":"none",
        "GOW3_FSR1":"0",
        "GOW3_RCAS":"0",
        "GOW3_JITTER":"0",
        "GOW3_REACTIVE":"0",
        "GOW3_OBJECT_MOTION":"0",
        "GOW3_DEFERRED_READBACK":"0",
        "GOW3_STALE_READBACK":"0",
        "GOW3_ASYNC_SHADERS":"0",
        "GOW3_ENGINE_FPS":"60",
        "GOW3_GPU_USER_DIR":str(USER.resolve()),
        "GOW3_CONFIG":str((USER/"firstlight.ini").resolve()),
        "FIRSTLIGHT_GAME_PROFILE":"firstlight",
    }
    for k,v in defaults.items():
        env.setdefault(k,v)
    return env

def run(cmd:list[str],env=None)->int:
    USER.mkdir(parents=True,exist_ok=True)
    rendered="+ "+" ".join(map(str,cmd))
    print(rendered,flush=True)
    with LOG.open("a",encoding="utf-8",errors="replace") as log:
        log.write(rendered+"\n")
        p=subprocess.Popen(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                           text=True,encoding="utf-8",errors="replace")
        assert p.stdout is not None
        for line in p.stdout:
            print(line,end="")
            log.write(line)
        code=p.wait()
        log.write(f"[exit {code}]\n")
    return code

def content_args()->list[str]:
    profile=OUT/"content.bin"
    return ["--content-profile",str(profile)] if profile.is_file() else []

def probe_path()->Path|None:
    for name in ("firstlight-probe.exe","gow3-probe.exe","firstlight-probe","gow3-probe"):
        p=OUT/name
        if p.is_file():
            return p
    return None

def prepare(game:Path,check_runtime:bool)->int:
    cmd=[sys.executable,str(ROOT/"tools"/"firstlight_stage1.py"),str(game)]
    if check_runtime:
        cmd.append("--check-runtime")
    return run(cmd)

def main()->int:
    p=argparse.ArgumentParser()
    p.add_argument("game_dir",type=Path)
    p.add_argument("--mode",choices=("check","cpu","gpu"),default="check")
    p.add_argument("--skip-prepare",action="store_true")
    p.add_argument("--timeout",type=int,default=10)
    a=p.parse_args()
    game=a.game_dir.expanduser().resolve()
    USER.mkdir(parents=True,exist_ok=True)
    LOG.write_text(f"infamous_firstlight diagnostic run\nmode={a.mode}\ngame={game}\n",
                   encoding="utf-8")
    print(f"Log: {LOG}")

    if a.mode=="check" and not a.skip_prepare:
        return prepare(game,True)

    probe=probe_path()
    if not probe:
        print("Build the runtime first with build.ps1 or bash build.sh.",file=sys.stderr)
        return 2

    if a.mode=="check":
        return run([str(probe),str(OUT/"boot-linked.bin"),"--cpu-only","--check-imports",
                    "--content-profile",str(OUT/"content.bin"),
                    "--app0",str(game),"--user",str(USER)],env=safe_env())

    if not a.skip_prepare:
        code=prepare(game,False)
        if code:
            return code

    base=[str(probe),str(OUT/"boot-linked.bin"),"--content-profile",str(OUT/"content.bin"),"--app0",str(game),
          "--user",str(USER),"--timeout",str(max(0,a.timeout))]
    env=safe_env()

    if a.mode=="cpu":
        print("CPU diagnostic boot: Vulkan disabled.")
        return run([*base,"--cpu-only"],env=env)

    print("Checking imports before experimental Vulkan boot...")
    code=run([str(probe),str(OUT/"boot-linked.bin"),"--cpu-only","--check-imports",
              "--content-profile",str(OUT/"content.bin"),
              "--app0",str(game),"--user",str(USER)],env=env)
    if code:
        print("GPU boot blocked until Imports: 0 missing.",file=sys.stderr)
        return code
    print("Experimental First Light Vulkan boot: GOW3 gameplay hooks are disabled.")
    return run(base,env=env)

if __name__=="__main__":
    raise SystemExit(main())
