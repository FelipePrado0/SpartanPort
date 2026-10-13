"""Find the Haswell-only instructions (MOVBE, LZCNT, BMI1, F16C) in the linked image.

scan_segment decodes one executable segment with iced-x86 and classifies each
site: JMP (5+ bytes, patched with a jump to a stub), JMP_RELOCATE (4 bytes,
the jump also covers the plain next instruction, copied into the stub), INT3
(4 bytes, trapped at run time) or UNSUPPORTED (an encoding the stubs do not
emulate: segment override, LOCK, 32-bit addressing or F16C).

Usage: isa_rewrite.py out/boot-linked.bin --out out
Reads the G3PROBE5 image, scans its executable segments and writes
<out>/isa-rewrite.bin (format G3ISA1: HEADER, then one RECORD per site).
"""
import argparse
from dataclasses import dataclass
import hashlib
from pathlib import Path
import struct
import sys
from prepare import span, unpack

try:
    import iced_x86
    from iced_x86 import Code, Decoder, FlowControl, Mnemonic, OpKind, Register, RegisterExt
except ImportError:
    iced_x86 = None

(KIND_MOVBE_LOAD, KIND_MOVBE_STORE, KIND_LZCNT, KIND_TZCNT, KIND_ANDN, KIND_BEXTR,
 KIND_BLSI, KIND_BLSMSK, KIND_BLSR, KIND_VCVTPH2PS, KIND_VCVTPS2PH) = range(1, 12)
MODE_JMP, MODE_JMP_RELOCATE, MODE_INT3, MODE_UNSUPPORTED = range(4)
NO_REG = 0xFF
HEADER = struct.Struct('<8sQ32sII8x')                # magic, image size, sha256, sites, unsupported
RECORD = struct.Struct('<QBBBBBBBBBBBB4xq15sB15sx')  # mirrors IsaSite in the C loader
SET_OF_KIND = {KIND_MOVBE_LOAD: 'MOVBE', KIND_MOVBE_STORE: 'MOVBE', KIND_LZCNT: 'LZCNT',
               KIND_TZCNT: 'BMI1', KIND_ANDN: 'BMI1', KIND_BEXTR: 'BMI1', KIND_BLSI: 'BMI1',
               KIND_BLSMSK: 'BMI1', KIND_BLSR: 'BMI1', KIND_VCVTPH2PS: 'F16C', KIND_VCVTPS2PH: 'F16C'}


@dataclass
class Site:
    offset: int
    length: int
    kind: int
    width: int
    mode: int
    dst: int
    src1: int
    src2: int
    mem_slot: int   # 0 none, 1 dst, 2 src
    base: int
    index: int
    scale: int
    mem_flags: int  # bit 0: RIP-relative, disp is the absolute target
    disp: int
    original: bytes
    relocated: bytes


def kind_of(ins):
    m = ins.mnemonic
    if m == Mnemonic.MOVBE:
        return KIND_MOVBE_LOAD if ins.op0_kind == OpKind.REGISTER else KIND_MOVBE_STORE
    return {Mnemonic.LZCNT: KIND_LZCNT, Mnemonic.TZCNT: KIND_TZCNT, Mnemonic.ANDN: KIND_ANDN,
            Mnemonic.BEXTR: KIND_BEXTR, Mnemonic.BLSI: KIND_BLSI, Mnemonic.BLSMSK: KIND_BLSMSK,
            Mnemonic.BLSR: KIND_BLSR, Mnemonic.VCVTPH2PS: KIND_VCVTPH2PS,
            Mnemonic.VCVTPS2PH: KIND_VCVTPS2PH}.get(m)


def register_number(reg):
    return NO_REG if reg == Register.NONE else RegisterExt.number(reg)


def site_of(ins, kind, original):
    """Operands in order fill dst, src1, src2; the memory operand leaves its slot NO_REG."""
    regs, mem_slot, width = [NO_REG] * 3, 0, 0
    for i in range(ins.op_count):
        if ins.op_kind(i) == OpKind.REGISTER:
            regs[i] = register_number(ins.op_register(i))
            width = width or RegisterExt.size(ins.op_register(i)) * 8
        elif ins.op_kind(i) == OpKind.MEMORY:
            mem_slot = 1 if i == 0 else 2
    if SET_OF_KIND[kind] == 'F16C':
        width = 0
    base = index = NO_REG
    scale = mem_flags = disp = 0
    unsupported = SET_OF_KIND[kind] == 'F16C' or ins.segment_prefix != Register.NONE or ins.has_lock_prefix
    if mem_slot:
        unsupported |= any(r != Register.NONE and RegisterExt.size(r) != 8 for r in (ins.memory_base, ins.memory_index))
        scale = ins.memory_index_scale
        if ins.is_ip_rel_memory_operand:
            mem_flags, disp = 1, ins.ip_rel_memory_address
        else:
            base, index = register_number(ins.memory_base), register_number(ins.memory_index)
            disp = ins.memory_displacement  # iced reports it as u64; the table stores int64
            disp -= (1 << 64) if disp >= 1 << 63 else 0
    mode = MODE_UNSUPPORTED if unsupported else MODE_JMP if ins.len >= 5 else MODE_INT3
    return Site(ins.ip, ins.len, kind, width, mode, *regs, mem_slot, base, index, scale, mem_flags, disp, original, b'')


def is_plain(ins, end):
    """Candidate to be moved into a 4-byte site's stub (branch targets are checked later)."""
    return (ins.code != Code.INVALID and ins.flow_control == FlowControl.NEXT and not ins.is_ip_rel_memory_operand
            and not ins.has_rep_prefix and not ins.has_repne_prefix and kind_of(ins) is None and ins.next_ip <= end)


def scan_segment(code, address):
    """Sites of one executable segment loaded at address, in address order."""
    if iced_x86 is None:
        raise RuntimeError('iced_x86 is not installed')
    end = address + len(code)
    sites, targets, candidates = [], set(), []
    pending = None  # 4-byte site waiting for the instruction after it
    for ins in Decoder(64, code, ip=address):
        if ins.flow_control in (FlowControl.UNCONDITIONAL_BRANCH, FlowControl.CONDITIONAL_BRANCH, FlowControl.CALL) \
                and ins.op0_kind == OpKind.NEAR_BRANCH64:
            targets.add(ins.near_branch_target)
        if pending is not None and is_plain(ins, end):
            candidates.append((pending, ins.ip, code[ins.ip - address:ins.next_ip - address]))
        pending = None
        kind = kind_of(ins)
        if kind is None:
            continue
        site = site_of(ins, kind, code[ins.ip - address:ins.next_ip - address])
        sites.append(site)
        if site.mode == MODE_INT3 and ins.len == 4:
            pending = site
    for site, ip, relocated in candidates:
        if ip not in targets:
            site.mode, site.relocated = MODE_JMP_RELOCATE, relocated
    return sites


def read_boot_linked(data):
    """Segments (address, size, flags) and image of a G3PROBE5 file, in link_modules.py's order."""
    magic, size, entry, ns, nr, ni, flags = unpack('<8s6Q', data, 0)
    if magic != b'G3PROBE5':
        raise ValueError('not a G3PROBE5 image')
    pos = 56 + 8 + 32   # header, procparam, main TLS
    nmodules, = unpack('<Q', data, pos)
    pos += 8 + nmodules * 56
    nbindings, = unpack('<Q', data, pos)
    pos += 8 + nbindings * 24
    segments = [unpack('<3Q', data, pos + i * 24) for i in range(ns)]
    pos += ns * 24 + ni * 128 + nr * 32
    return segments, span(data, pos, size)


def encode_table(image, sites):
    unsupported = sum(s.mode == MODE_UNSUPPORTED for s in sites)
    table = [HEADER.pack(b'G3ISA1\0\0', len(image), hashlib.sha256(image).digest(), len(sites), unsupported)]
    for s in sorted(sites, key=lambda s: s.offset):   # the C loader rejects records out of order
        table.append(RECORD.pack(s.offset, s.length, s.kind, s.width, s.mode, s.dst, s.src1, s.src2, s.mem_slot,
                                 s.base, s.index, s.scale, s.mem_flags, s.disp, s.original, len(s.relocated),
                                 s.relocated))
    return b''.join(table)


def summary(sites):
    sets = [SET_OF_KIND[s.kind] for s in sites]
    modes = [s.mode for s in sites]
    per_set = ', '.join(f'{name} {sets.count(name)}' for name in ('MOVBE', 'BMI1', 'LZCNT', 'F16C'))
    return (f'ISA: {len(sites)} sites ({per_set}); {modes.count(MODE_JMP_RELOCATE)} relocated, '
            f'{modes.count(MODE_INT3)} int3, {modes.count(MODE_UNSUPPORTED)} unsupported')


def fixture_code():
    """Code behind tests/data/isa_fixture.bin, loaded by the C loader test."""
    return b''.join(bytes.fromhex(h) for h in (
        'c4e270f3c9',              # blsr ecx, ecx                 JMP
        '0f38f108', '90',          # movbe [rax], ecx; nop         JMP_RELOCATE
        'c4e270f7050c000000',      # bextr eax, [rip+0xc], ecx     JMP
        '66f30fbdc1',              # lzcnt ax, cx                  JMP
        'f3480fbcc1',              # tzcnt rax, rcx                JMP
        '0f38f108', 'ebfe',        # movbe [rax], ecx; jmp $       INT3
        'c3'))                     # ret


def main(argv):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('boot', type=Path)
    p.add_argument('--out', type=Path, default=Path(__file__).resolve().parent.parent / 'out')
    a = p.parse_args(argv)
    try:
        if iced_x86 is None:
            raise RuntimeError('iced_x86 is not installed')
        segments, image = read_boot_linked(a.boot.read_bytes())
        sites = [site for address, size, flags in segments if flags & 1
                 for site in scan_segment(span(image, address, size), address)]
        (a.out / 'isa-rewrite.bin').write_bytes(encode_table(image, sites))
    except (OSError, ValueError, RuntimeError) as e:
        print(f'ISA: rewrite table unavailable ({e})')
        return 1
    print(summary(sites))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
