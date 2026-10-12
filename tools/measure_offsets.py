#!/usr/bin/env python3
"""Measure where the fork's fixed hook/patch anchors land in a given prepared eboot.elf.

For each anchor we know the expected image vaddr and the original bytes. We check the bytes at
the expected vaddr, and if they do not match we scan the whole code segment for the signature and
report every offset where it is found. This tells us the per-anchor delta for this dump.

Usage: python measure_offsets.py <eboot.elf>
"""
import struct
import sys


def load_code_segment(path):
    data = open(path, 'rb').read()
    phoff = struct.unpack_from('<Q', data, 0x20)[0]
    phentsize, phnum = struct.unpack_from('<HH', data, 0x36)
    # The probe's `image` points at vaddr 0; the first PT_LOAD (code) has vaddr 0.
    for i in range(phnum):
        kind, flags, off, vaddr, paddr, filesz, memsz, align = struct.unpack_from(
            '<IIQQQQQQ', data, phoff + i * phentsize)
        if kind == 1 and vaddr == 0:
            return data, off, filesz
    raise SystemExit('no vaddr-0 PT_LOAD segment found')


def at_vaddr(data, seg_off, vaddr, length):
    """Bytes the probe would see at image+vaddr (file offset = seg_off + vaddr)."""
    fo = seg_off + vaddr
    return data[fo:fo + length]


def scan(data, seg_off, seg_size, needle):
    hits = []
    start = seg_off
    end = seg_off + seg_size
    while True:
        i = data.find(needle, start, end)
        if i < 0:
            break
        hits.append(i - seg_off)  # report as image vaddr
        start = i + 1
    return hits


# (name, expected image vaddr, original signature bytes as hex)
HOOK_ANCHORS = [
    ('cheat health 0x5040e', 0x5040e, 'c4c17a1185cc0100'),
    ('cheat magic 0x72cdc', 0x72cdc, 'c5fa1183d0010000'),
    ('cheat item 0x72d97', 0x72d97, 'c5fa1187d4010000'),
    ('cheat rage 0x6a9b8', 0x6a9b8, 'c5fa118bd8010000'),
    ('cheat rage 0x6a6a0', 0x6a6a0, 'c5fa118bd8010000'),
    ('cheat orbs 0x6a9fc', 0x6a9fc, 'c5fa1187e8010000'),
    ('cheat very_hard 0xc77ea', 0xc77ea, 'b803000000f6c108'),
    ('cheat double_jumps 0x95d51', 0x95d51, '41f68424ac04000004'),
    ('actor hook 0x50417', 0x50417, '498b45004c89ef'),
    ('damage hook 0x503f3', 0x503f3, 'c5fa1045a4c5fa5c45a0'),
    ('orb pickup 0x6a480', 0x6a480, '554889e35348 83ec18'.replace(' ', '')),
    ('orb setter 0x6a9f0', 0x6a9f0, '554889e5c5fa5d0520a42800c5fa1187e8010000'),
]

# Patch addresses are PS4 vaddr (eboot base 0x400000). We read the current bytes there so we can
# see whether the resolution/FPS write targets still hold their expected original values.
PATCH_PROBES = [
    ('res width @0x5f0b3b', 0x5f0b3b, 4),
    ('res height @0x5f0b43', 0x5f0b43, 4),
    ('res @0x5643f3', 0x5643f3, 4),
    ('res @0x564400', 0x564400, 4),
    ('res @0x5ff769', 0x5ff769, 4),
    ('res @0x56bb82', 0x56bb82, 4),
    ('res @0x60953d', 0x60953d, 4),
    ('fps lea @0x48ff1a', 0x48ff1a, 7),
    ('fps const @0x75dbe4', 0x75dbe4, 4),
    ('texture arena @0x7cd918', 0x7cd918, 4),
    ('skip video @0x41b2a3', 0x41b2a3, 2),
    ('skip video @0x41b2e7', 0x41b2e7, 8),
]
EBOOT_BASE = 0x400000


def main():
    path = sys.argv[1]
    data, seg_off, seg_size = load_code_segment(path)
    print(f'file: {path}')
    print(f'code segment: file_off={seg_off:#x} size={seg_size:#x}')
    print()
    print('=== C hooks (image vaddr) ===')
    for name, vaddr, sighex in HOOK_ANCHORS:
        needle = bytes.fromhex(sighex)
        here = at_vaddr(data, seg_off, vaddr, len(needle))
        match = here == needle
        line = f'{name:30s} expected@{vaddr:#x} '
        if match:
            line += 'MATCH at expected offset (delta 0)'
        else:
            hits = scan(data, seg_off, seg_size, needle)
            if hits:
                deltas = ', '.join(f'{h:#x} (delta {h - vaddr:+#x})' for h in hits)
                line += f'moved -> {deltas}'
            else:
                line += f'NOT FOUND (bytes here: {here.hex(" ")})'
        print(line)
    print()
    print('=== patch target bytes (PS4 vaddr) ===')
    for name, vaddr, n in PATCH_PROBES:
        fo = seg_off + (vaddr - EBOOT_BASE)
        cur = data[fo:fo + n]
        print(f'{name:28s} vaddr={vaddr:#x} file_off={fo:#x} bytes={cur.hex(" ")}')


if __name__ == '__main__':
    main()
