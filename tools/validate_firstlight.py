#!/usr/bin/env python3
"""Validate the known CUSA00575 dump before the native-port tools touch it."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from scripts.prepare import load_sfo, parse_self, read_self  # noqa: E402

TARGET=json.loads((ROOT/"targets"/"CUSA00575.json").read_text(encoding="utf-8"))

def sha256(path:Path)->str:
    h=hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda:f.read(1024*1024),b""):
            h.update(chunk)
    return h.hexdigest()

def check_file(root:Path,spec:dict,skip_hash:bool)->list[str]:
    path=root/spec["path"]
    errors=[]
    if not path.is_file():
        return [f"missing: {spec['path']}"]
    if "size" in spec and path.stat().st_size != spec["size"]:
        errors.append(f"{spec['path']}: size {path.stat().st_size}, expected {spec['size']}")
    if not skip_hash:
        got=sha256(path)
        if got.lower()!=spec["sha256"].lower():
            errors.append(f"{spec['path']}: SHA256 {got}, expected {spec['sha256']}")
    return errors

def main()->int:
    p=argparse.ArgumentParser()
    p.add_argument("game_dir",type=Path)
    p.add_argument("--skip-hash",action="store_true",
                   help="Only verify identity/layout; useful while comparing a different update")
    a=p.parse_args()
    game=a.game_dir.expanduser().resolve()
    errors=[]

    try:
        sfo=load_sfo(game/"sce_sys"/"param.sfo")
        if sfo.get("TITLE_ID") != TARGET["title_id"]:
            errors.append(f"TITLE_ID={sfo.get('TITLE_ID')!r}, expected {TARGET['title_id']}")
        if sfo.get("APP_VER") != TARGET["app_ver"]:
            errors.append(f"APP_VER={sfo.get('APP_VER')!r}, expected {TARGET['app_ver']}")
    except Exception as e:
        errors.append(f"param.sfo: {e}")

    errors += check_file(game,TARGET["executable"],a.skip_hash)
    for module in TARGET["modules"]:
        errors += check_file(game,module,a.skip_hash)

    eboot=game/TARGET["executable"]["path"]
    if eboot.is_file():
        try:
            _elf,header,_ph,_segments,_missing=parse_self(read_self(eboot))
            entry=header[4]
            expected=int(TARGET["executable"]["entry_point"],16)
            if entry != expected:
                errors.append(f"eboot entry={entry:#x}, expected {expected:#x}")
        except Exception as e:
            errors.append(f"eboot SELF: {e}")

    if errors:
        print("CUSA00575 validation FAILED:")
        for error in errors:
            print(" -",error)
        return 1

    print("CUSA00575 validation OK")
    print(f"Title: {TARGET['title']} ({TARGET['title_id']})")
    print(f"APP_VER: {TARGET['app_ver']}")
    print(f"Entry: {TARGET['executable']['entry_point']}")
    print("Known TLS relocations: 16 (Fios2=2, libc=14)")
    if a.skip_hash:
        print("WARNING: hashes were not checked")
    return 0

if __name__=="__main__":
    raise SystemExit(main())
