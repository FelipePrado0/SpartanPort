#!/usr/bin/env python3
"""Signatures for patch and hook sites, from the reference eboot.bin (needs capstone).

Each site gets the shortest window of original bytes around it that occurs exactly once in the
image, with "??" where another build of the game may differ: rip-relative displacements and rel32
branch targets in code (the code moves against the data it references). The written bytes stay
fixed, so a match also proves the original bytes are there.

  python tools/make_signatures.py EBOOT ADDR:LEN[:data] ... [--ref ADDR]
    ADDR:LEN       a site (PS4 vaddr, base 0x400000) and how many bytes are written there
    ADDR:LEN:data  the same in constant data (raw bytes, no disassembly)
    --ref ADDR     the instruction at ADDR whose rip-relative or rel32 operand gives a Target:
                   prints the Pattern, Ref (operand offset) and Tail (bytes after it)
"""
import argparse
import sys
from pathlib import Path

import capstone
from capstone import x86

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'scripts'))
from patches import EBOOT_BASE, eboot_image, find_pattern  # noqa: E402
from prepare import parse_self, read_self  # noqa: E402

BRANCHES = {x86.X86_GRP_JUMP, x86.X86_GRP_CALL}
# Unique in one build is not proof for another: a longer window keeps a chance match unlikely.
MIN_LENGTH = 16


def load(path):
    elf, _, ph, _, _ = parse_self(read_self(path))
    text = next(p for p in ph if p['type'] == 1 and p['flags'] & 1)
    return eboot_image(bytes(elf)), (text['vaddr'], text['vaddr'] + text['filesz'])


def instructions(image, text, at):
    """Decoded instructions around image offset at; a linear sweep from well before it has
    resynchronized by the window."""
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    md.skipdata = True  # constants between functions decode as .byte and the sweep goes on
    start = max(text[0], at - 160)
    return [i for i in md.disasm(image[start:min(text[1], at + 160)], start) if i.address >= at - 96]


def variable(ins):
    """Offsets inside ins of bytes another build may change."""
    skip = set()
    if ins.id == 0:  # SKIPDATA bytes: no operands
        return skip
    if ins.disp_offset and any(op.type == x86.X86_OP_MEM and op.mem.base == x86.X86_REG_RIP
                               for op in ins.operands):
        skip |= set(range(ins.disp_offset, ins.disp_offset + ins.disp_size))
    if ins.imm_offset and ins.imm_size == 4 and set(ins.groups) & BRANCHES:
        skip |= set(range(ins.imm_offset, ins.imm_offset + 4))
    return skip


def pattern_text(pattern):
    return ' '.join('??' if b is None else f'{b:02x}' for b in pattern)


def code_signature(image, text, at, length):
    code = instructions(image, text, at)
    mask = {}
    for ins in code:
        for k in variable(ins): mask[ins.address + k] = None
    first = next(n for n, i in enumerate(code) if i.address + i.size > at)
    last = next(n for n, i in enumerate(code) if i.address + i.size >= at + length)
    while True:
        start, end = code[first].address, code[last].address + code[last].size
        pattern = [mask.get(a, image[a]) for a in range(start, end)]
        if end - start >= MIN_LENGTH and len(find_pattern(image, pattern)) == 1:
            return pattern, at - start
        # Grow towards the side that adds fewer bytes; stop when nothing is left to add.
        if first and (last + 1 >= len(code) or code[first - 1].size <= code[last + 1].size):
            first -= 1
        elif last + 1 < len(code):
            last += 1
        else:
            raise SystemExit(f'{at + EBOOT_BASE:#x}: no unique signature')


def data_signature(image, at, length):
    start, end = at, at + length
    while True:
        pattern = list(image[start:end])
        if end - start >= MIN_LENGTH and len(find_pattern(image, pattern)) == 1:
            return pattern, at - start
        start, end = start - 4, end + 4


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('eboot')
    p.add_argument('sites', nargs='*')
    p.add_argument('--ref', action='append', default=[], type=lambda v: int(v, 0))
    a = p.parse_args()
    image, text = load(a.eboot)
    for site in a.sites:
        address, length, *kind = site.split(':')
        at, length = int(address, 0) - EBOOT_BASE, int(length, 0)
        pattern, offset = (data_signature(image, at, length) if kind == ['data'] else
                           code_signature(image, text, at, length))
        print(f'{at + EBOOT_BASE:#x} Pattern="{pattern_text(pattern)}" At="{offset}"')
    for address in a.ref:
        at = address - EBOOT_BASE
        ins = next(i for i in instructions(image, text, at) if i.address == at)
        operand = ins.disp_offset if ins.disp_offset else ins.imm_offset
        pattern, offset = code_signature(image, text, at, ins.size)
        tail = ins.size - operand - 4
        target = at + operand + 4 + tail + int.from_bytes(image[at + operand:at + operand + 4], 'little', signed=True)
        print(f'{address:#x} -> {target + EBOOT_BASE:#x} Pattern="{pattern_text(pattern)}" '
              f'At="{offset + operand}" Ref="1" Tail="{tail}"')


if __name__ == '__main__':
    main()
