"""Frame structure from a SPARTAN_CAPTURE_TRIGGER capture (vk_frame_capture.cpp `Write`) and an
exact comparison of two of them.

The structure of a frame is the ordered list of passes and compute dispatches with their shaders
and the format and size of every color and depth target. Guest addresses are left out (they move
between runs); draw counts are kept but compared only when `draws=True` (a static camera).
"""
import re
import sys

HEADER = re.compile(r"^#\d+ (?P<kind>PASS|COMPUTE) (?:draws|dispatches) (?P<count>\d+)"
                    r"(?: \(indices \d+\))?, (?:vs/ps|cs)(?P<shaders>(?: [0-9a-f]{16})*)$")
TARGET = re.compile(r"^  (?P<slot>color\d+|depth)\s+(?:-|0x[0-9a-f]+ (?P<format>\S+) (?P<size>\d+x\d+))$")


def parse(text):
    """Returns the list of entries: {kind, count, shaders, targets: [(slot, format, size)]}."""
    entries = []
    for line in text.splitlines():
        if m := HEADER.match(line):
            entries.append({"kind": m["kind"], "count": int(m["count"]),
                            "shaders": m["shaders"].split(), "targets": []})
        elif entries and (m := TARGET.match(line)):
            entries[-1]["targets"].append((m["slot"], m["format"], m["size"]))
    return entries


def diff(reference, candidate, draws=False):
    """Returns None when the structures match, else a one-line description of the first
    difference."""
    if len(reference) != len(candidate):
        return f"{len(candidate)} passes vs {len(reference)} in the reference"
    for i, (a, b) in enumerate(zip(reference, candidate)):
        for key in ("kind", "shaders", "targets") + (("count",) if draws else ()):
            if a[key] != b[key]:
                return f"entry #{i} {key}: {b[key]} vs {a[key]}"
    return None


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("usage: capstruct.py <reference.txt> <candidate.txt> [--draws]", file=sys.stderr)
        sys.exit(2)
    with open(sys.argv[1], encoding="utf-8") as f:
        ref = parse(f.read())
    with open(sys.argv[2], encoding="utf-8") as f:
        cand = parse(f.read())
    d = diff(ref, cand, "--draws" in sys.argv)
    print(d or f"same structure ({len(ref)} entries)")
    sys.exit(1 if d else 0)
