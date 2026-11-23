#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Lists the game's code that reads the engine frame rate (high FPS roadmap, F4.0).

Usage: python -I tools/find_rate_readers.py <eboot.elf or eboot.bin> [--objdump PATH] [--no-objdump]

It finds the engine rate setter by the signature src/probe.c uses (engine_fps_setter), takes the two
globals it writes (the float and the rounded int), finds the 59.94, 60.0 and 1/60 float constants
in the data, and lists every rip-relative instruction ([rip+disp32]) whose target is one of them,
grouped by function. The swing energy reader (0x48ff1a in v01.02, the frame rate patches' first
line) must be among them. Pure Python 3, no dependencies; GNU objdump, if on PATH (or --objdump), adds
the disassembly around each reader. Prints only addresses and short excerpts, never the file.
"""
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "scripts"))
from prepare import parse_self  # noqa: E402

# src/probe.c engine_fps_setter: lea rax,[rip+fps]; vmovss [rax],xmm0; vaddss xmm1,xmm0,[rip+half];
# vcvttss2si eax,xmm1; lea rcx,[rip+rounded]; mov [rcx],eax.
SETTER = "48 8d 05 ?? ?? ?? ?? c5 fa 11 00 c5 fa 58 0d ?? ?? ?? ?? c5 fa 2c c1 48 8d 0d ?? ?? ?? ?? 89 01"
SETTER_LENGTH = 32
# patches/CUSA01623.xml, "Frame Rate Patch": the swing energy reader, at 8 in this pattern.
SWING = "c5 fa 11 85 98 fb ff ff 48 8d 05 ?? ?? ?? ?? c5 fa 10 00"
SWING_AT = 8
SWING_V0102 = 0x48FF1A
CONSTANTS = (("59.94", 0x426FC28F), ("60.0", 0x42700000), ("1/60", 0x3C888889))
CONSTANT_MAX = 64  # copies of one constant listed; more means it is not a constant of interest
FUNCTION_SEARCH = 0x10000
FUNCTION_START = b"\x55\x48\x89\xe5"  # push rbp; mov rbp, rsp

PT_LOAD, PT_SCE_RELRO, PF_X, PF_R = 1, 0x61000010, 1, 4
RIP_MODRM = re.compile(rb"[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]")


class Image:
    """The file laid out at its virtual addresses (the image offsets the port's hooks use)."""

    def __init__(self, data):
        self.segments = []  # (vaddr, memory size, ELF file offset, file size, executable)
        self.source_offsets = {}
        self.constant_ranges = []
        self.is_self = data[:4] == b"O\x15=\x1d"
        if self.is_self:
            data, _, headers, segments, _ = parse_self(data)
            for segment in segments:
                flags = int(segment["flags"], 16)
                if flags & 0x800:
                    header = headers[(flags >> 20) & 4095]
                    self.source_offsets[header["offset"]] = segment["offset"]
        self.flat = False
        if data[:4] == b"\x7fELF" and data[4] == 2 and data[5] == 1:
            phoff, = struct.unpack_from("<Q", data, 0x20)
            phentsize, phnum = struct.unpack_from("<HH", data, 0x36)
            for i in range(phnum):
                p_type, p_flags, p_offset, p_vaddr, _, p_filesz, p_memsz, _ = struct.unpack_from(
                    "<IIQQQQQQ", data, phoff + i * phentsize)
                if p_type in (PT_LOAD, PT_SCE_RELRO) and p_memsz:
                    self.segments.append((p_vaddr, p_memsz, p_offset, p_filesz, bool(p_flags & PF_X)))
                    if p_flags & PF_R:
                        self.constant_ranges.append((p_vaddr, p_vaddr + p_filesz))
        if not self.segments:
            # Not an ELF (or no loadable segment): the file itself, every byte both code and data.
            self.flat = True
            self.segments.append((0, len(data), 0, len(data), True))
        end = max(v + m for v, m, _, _, _ in self.segments)
        self.bytes = bytearray(end)
        for vaddr, _, offset, filesz, _ in self.segments:
            self.bytes[vaddr:vaddr + filesz] = data[offset:offset + filesz]

    def code(self):
        return [(v, v + m) for v, m, _, _, x in self.segments if x]

    def data(self):
        if self.flat:
            return [(0, len(self.bytes))]
        return self.constant_ranges

    def mapped(self, address, size=4):
        return any(v <= address and address + size <= v + m for v, m, _, _, _ in self.segments)

    def file_offset(self, address):
        for vaddr, _, offset, filesz, _ in self.segments:
            if vaddr <= address < vaddr + filesz:
                return self.source_offsets.get(offset, offset) + address - vaddr
        return None

    def disp(self, at):
        return struct.unpack_from("<i", self.bytes, at)[0]


def parse_pattern(text):
    """src/signature.h: hex bytes, "??" for any."""
    return [None if b == "??" else int(b, 16) for b in text.split()]


def find_all(image, pattern, start=0, end=None):
    """Every offset of the pattern (None: any byte) in image.bytes[start:end]."""
    end = len(image.bytes) if end is None else end
    fixed = max((i for i in range(len(pattern))), key=lambda i: run_length(pattern, i))
    anchor = bytes(pattern[fixed:fixed + run_length(pattern, fixed)])
    found = []
    at = image.bytes.find(anchor, start + fixed, end)
    while at >= 0:
        base = at - fixed
        if base + len(pattern) <= end and all(
                b is None or image.bytes[base + i] == b for i, b in enumerate(pattern)):
            found.append(base)
        at = image.bytes.find(anchor, at + 1, end)
    return found


def run_length(pattern, i):
    n = 0
    while i + n < len(pattern) and pattern[i + n] is not None:
        n += 1
    return n


def find_unique(image, text):
    """src/signature.h: the offset when the pattern occurs exactly once, else None."""
    hits = find_all(image, parse_pattern(text))
    return hits[0] if len(hits) == 1 else None


def find_targets(image):
    """{address: name} of what the readers read, the setter's offset, and notes."""
    targets, notes = {}, []
    setter = find_unique(image, SETTER)
    if setter is None:
        notes.append("engine rate setter: not found (src/probe.c engine_fps_setter), globals unknown")
    else:
        fps = setter + 7 + image.disp(setter + 3)
        rounded = setter + 30 + image.disp(setter + 26)
        notes.append("engine rate setter: 0x%x; globals: float 0x%x, rounded 0x%x" % (setter, fps, rounded))
        for address, name in ((fps, "rate (float global)"), (rounded, "rate (rounded global)")):
            if image.mapped(address):
                targets[address] = name
            else:
                notes.append("global 0x%x outside the image: skipped" % address)
    for name, value in CONSTANTS:
        raw = struct.pack("<I", value)
        hits = []
        for lo, hi in image.data():
            at = image.bytes.find(raw, lo, hi)
            while at >= 0 and len(hits) <= CONSTANT_MAX:
                if at % 4 == 0:
                    hits.append(at)
                at = image.bytes.find(raw, at + 1, hi)
        if len(hits) > CONSTANT_MAX:
            notes.append("constant %s: more than %d copies, skipped" % (name, CONSTANT_MAX))
            continue
        notes.append("constant %s: %s" % (name, ", ".join("0x%x" % h for h in hits) or "not found"))
        for h in hits:
            targets.setdefault(h, "constant " + name)
    return targets, setter, notes


def classify(b, modrm_at):
    """(instruction start, mnemonic) of a [rip+disp32] whose ModRM byte is at modrm_at, or None."""
    op = b[modrm_at - 1]
    two = b[modrm_at - 2] if modrm_at >= 2 else -1
    three = b[modrm_at - 3] if modrm_at >= 3 else -1
    # VEX: c5 vv op modrm / c4 rr mm op modrm (0f map), scalar float forms.
    vex = {0x10: "vmovss", 0x11: "vmovss", 0x58: "vaddss", 0x59: "vmulss", 0x5c: "vsubss", 0x5e: "vdivss",
           0x5d: "vminss", 0x5f: "vmaxss", 0x2a: "vcvtsi2ss", 0x2e: "vucomiss", 0x2f: "vcomiss",
           0x5a: "vcvtss2sd", 0x51: "vsqrtss", 0x18: "vbroadcastss"}
    if op in vex and modrm_at >= 3 and b[modrm_at - 3] == 0xC5:
        return modrm_at - 3, vex[op]
    if op in vex and modrm_at >= 4 and b[modrm_at - 4] == 0xC4 and b[modrm_at - 3] & 0x1F in (1, 2):
        return modrm_at - 4, vex[op]
    # SSE: f3 [rex] 0f op modrm.
    sse = {0x10: "movss", 0x11: "movss", 0x58: "addss", 0x59: "mulss", 0x5c: "subss", 0x5e: "divss",
           0x2a: "cvtsi2ss", 0x5d: "minss", 0x5f: "maxss", 0x5a: "cvtss2sd", 0x51: "sqrtss"}
    if op in sse and two == 0x0F:
        if three == 0xF3:
            return modrm_at - 3, sse[op]
        if 0x40 <= three <= 0x4F and modrm_at >= 4 and b[modrm_at - 4] == 0xF3:
            return modrm_at - 4, sse[op]
    if op in (0x2e, 0x2f) and two == 0x0F:  # ucomiss / comiss
        return (modrm_at - 3 if 0x40 <= three <= 0x4F else modrm_at - 2), "ucomiss" if op == 0x2e else "comiss"
    # lea / mov loads and stores, with an optional REX.
    gp = {0x8D: "lea", 0x8B: "mov", 0x89: "mov", 0x3B: "cmp", 0x39: "cmp", 0x03: "add", 0x2B: "sub"}
    if op in gp and two != 0x0F:
        return (modrm_at - 2 if 0x40 <= two <= 0x4F else modrm_at - 1), gp[op]
    return None


def find_readers(image, targets, setter):
    """[(start, disp offset, target, mnemonic or None)] for every [rip+disp32] reaching a target."""
    readers, seen = [], set()
    skip = range(setter, setter + SETTER_LENGTH) if setter is not None else range(0)
    b = image.bytes
    for lo, hi in image.code():
        # Every [rip+disp32] has its ModRM (mod 00, rm 101) right before the displacement.
        for modrm in RIP_MODRM.finditer(b, max(lo, 0), max(lo, hi - 4)):
            at = modrm.start() + 1
            disp = struct.unpack_from("<i", b, at)[0]
            for imm in (0, 1, 4):  # the instruction's immediate after the displacement
                target = at + 4 + imm + disp
                if target not in targets:
                    continue
                known = classify(b, at - 1) if imm == 0 else None
                start = known[0] if known else at - 1
                if at in seen or start in skip:
                    continue
                seen.add(at)
                readers.append((start, at, target, known[1] if known else None))
    readers.sort()
    return readers


def function_of(image, address):
    """The nearest push rbp; mov rbp, rsp before address (within FUNCTION_SEARCH), else None."""
    at = image.bytes.rfind(FUNCTION_START, max(0, address - FUNCTION_SEARCH), address + 1)
    return at if at >= 0 else None


def function_end(image, start, limit):
    """The first ret (c3) after start, or None."""
    at = image.bytes.find(b"\xc3", start, limit)
    return at if at >= 0 else None


def objdump_tool(choice):
    if choice == "":
        return None
    if choice:
        return choice
    return shutil.which("objdump")  # GNU objdump: llvm-objdump has no raw binary input


def disassemble(tool, image, start, reader, lines=16):
    """+-lines instructions around reader, starting the decode at start (a function start when known)."""
    begin = max(start, reader - 160)
    chunk = bytes(image.bytes[begin:reader + 160])
    with tempfile.TemporaryDirectory() as folder:
        path = os.path.join(folder, "chunk.bin")
        with open(path, "wb") as f:
            f.write(chunk)
        args = [tool, "-D", "-b", "binary", "-mi386:x86-64", "--adjust-vma=0x%x" % begin, path]
        try:
            out = subprocess.run(args, capture_output=True, text=True, timeout=30).stdout
        except (OSError, subprocess.SubprocessError) as error:
            return ["    (objdump failed: %s)" % error]
    rows = [line for line in out.splitlines() if ":\t" in line]
    at = next((i for i, line in enumerate(rows) if int(line.split(":")[0], 16) >= reader), None)
    if at is None:
        return ["    (no disassembly)"]
    return ["    " + ("=> " if i == at else "   ") + rows[i].strip()
            for i in range(max(0, at - lines), min(len(rows), at + lines + 1))]


def hex_around(image, address, size=48):
    lo = max(0, address - size // 2)
    return " ".join("%02x" % x for x in image.bytes[lo:lo + size]), lo


def report(path, tool=None, out=sys.stdout):
    """Writes the listing; returns (readers, swing energy check passed)."""
    with open(path, "rb") as f:
        image = Image(f.read())
    write = lambda text="": print(text, file=out)
    write("find_rate_readers: %s (%s)" % (os.path.basename(path),
                                          "SELF" if image.is_self else
                                          "flat file: not an ELF" if image.flat else "ELF"))
    targets, setter, notes = find_targets(image)
    for note in notes:
        write("  " + note)
    readers = find_readers(image, targets, setter)
    swing = find_unique(image, SWING)
    swing = swing + SWING_AT if swing is not None else None
    found_swing = swing is not None and any(start == swing for start, _, _, _ in readers)
    groups = {}
    for reader in readers:
        groups.setdefault(function_of(image, reader[0]), []).append(reader)
    write("readers: %d in %d functions" % (len(readers), len(groups)))
    for function in sorted(groups, key=lambda f: -1 if f is None else f):
        members = groups[function]
        if function is None:
            write("\nfunction ?: %d readers (no push rbp; mov rbp, rsp within 0x%x bytes)"
                  % (len(members), FUNCTION_SEARCH))
        else:
            end = function_end(image, function, len(image.bytes))
            write("\nfunction 0x%x..%s: %d readers" % (function, "0x%x" % end if end else "?", len(members)))
        for start, _, target, mnemonic in members:
            offset = image.file_offset(start)
            write("  0x%x (file offset %s) %s %s -> 0x%x%s%s" % (
                start, "0x%x" % offset if offset is not None else "?", mnemonic or "?", targets[target],
                target, "" if mnemonic else "  [a confirmar]",
                "  [swing energy, known case]" if start == swing else ""))
            data, lo = hex_around(image, start)
            write("    bytes at 0x%x: %s" % (lo, data))
            if tool:
                for line in disassemble(tool, image, function if function is not None else start, start):
                    write(line)
    write("")
    if swing is None:
        write("WARNING: the swing energy reader's pattern (frame rate patch, 0x%x in v01.02) is not in this "
              "file: check the input" % SWING_V0102)
    elif not found_swing:
        write("WARNING: the swing energy reader at 0x%x is not in the list: the tool is wrong for this build"
              % swing)
    else:
        write("known case: the swing energy reader at 0x%x is listed%s" % (
            swing, "" if swing == SWING_V0102 else " (0x%x in v01.02)" % SWING_V0102))
    return readers, found_swing


def main(argv):
    args = [a for a in argv if not a.startswith("--")]
    tool = None
    if "--no-objdump" in argv:
        tool = ""
    for i, a in enumerate(argv):
        if a == "--objdump" and i + 1 < len(argv):
            tool = argv[i + 1]
            args.remove(argv[i + 1])
    if len(args) != 1 or not os.path.isfile(args[0]):
        print(__doc__.strip().splitlines()[2], file=sys.stderr)
        return 2
    report(args[0], objdump_tool(tool))  # a missing known case is a warning in the output
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
