#!/usr/bin/env python3
"""Build an experimental Persian message WAD from a Unicode-capable base WAD.

This replaces only MSGS_TXT. It does not claim Persian glyph support; use it to
validate message encoding and WAD replacement before the SMF2 atlas patch lands.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from wad_lang import replace_messages  # noqa: E402


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("base_wad", type=Path)
    ap.add_argument("messages", type=Path)
    ap.add_argument("output", type=Path)
    args = ap.parse_args()
    result = replace_messages(args.base_wad.read_bytes(), args.messages.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(result)
    print(f"wrote {args.output} ({len(result)} bytes; MSGS_TXT only)")


if __name__ == "__main__":
    main()
