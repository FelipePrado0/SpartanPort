#!/usr/bin/env python3
"""Inspect GoW III SMF2 font records inside a WAD."""
from __future__ import annotations
import argparse
import struct
from collections import Counter
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import glyphs  # noqa: E402


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("wad", type=Path)
    args = ap.parse_args()
    data = args.wad.read_bytes()
    entries = [(off, size, name) for off, size, name in glyphs.entries(data) if name == "SMF_0"]
    if not entries:
        raise SystemExit("SMF_0 not found")
    off, size, _ = entries[0]
    smf = data[off + 64: off + 64 + size]
    if smf[:4] != b"SMF2":
        raise SystemExit(f"unexpected magic: {smf[:4]!r}")
    record_offset = 100
    record_size = 20
    records = []
    for pos in range(record_offset, len(smf) - record_size + 1, record_size):
        values = struct.unpack_from("<5I", smf, pos)
        if values[0] == 0:
            continue
        records.append((values[0], values))
    cps = [cp for cp, _ in records]
    print(f"SMF size: {len(smf)} bytes")
    print(f"record size: {record_size} bytes")
    print(f"records: {len(records)}")
    print(f"codepoint range: U+{min(cps):04X}..U+{max(cps):04X}")
    for name, pred in {
        "ASCII": lambda cp: cp < 0x80,
        "Arabic/Persian": lambda cp: 0x0600 <= cp <= 0x06FF,
        "CJK Unified": lambda cp: 0x4E00 <= cp <= 0x9FFF,
    }.items():
        selected = [cp for cp in cps if pred(cp)]
        print(f"{name}: {len(selected)}")
    print("first records:")
    for cp, values in records[:10]:
        print(f"  U+{cp:04X}: " + " ".join(f"0x{x:08X}" for x in values[1:]))


if __name__ == "__main__":
    main()
