#!/usr/bin/env python3
"""Locate the original values the resolution/FPS patches overwrite, to measure per-region deltas.

The reference XML overwrites known original values. For the resolution sites the original is the
native 1920x1080 (and 960x540 for one pair); for 120 FPS it is the 59.94 float. By scanning for
those originals near the reference addresses we find where they moved to in this dump.

Usage: python find_originals.py <eboot.elf>
"""
import struct
import sys


def load_code_segment(path):
    data = open(path, 'rb').read()
    phoff = struct.unpack_from('<Q', data, 0x20)[0]
    phentsize, phnum = struct.unpack_from('<HH', data, 0x36)
    for i in range(phnum):
        rec = struct.unpack_from('<IIQQQQQQ', data, phoff + i * phentsize)
        kind, vaddr, off, filesz = rec[0], rec[3], rec[2], rec[5]
        if kind == 1 and vaddr == 0:
            return data, off, filesz
    raise SystemExit('no vaddr-0 PT_LOAD segment')


EBOOT_BASE = 0x400000


def scan(data, seg_off, seg_size, needle):
    hits = []
    start, end = seg_off, seg_off + seg_size
    while True:
        i = data.find(needle, start, end)
        if i < 0:
            break
        hits.append(i - seg_off)  # as image/eboot offset from vaddr 0
        start = i + 1
    return hits


def near(ref_vaddr, hits, window=0x400):
    return [h for h in hits if abs((h + EBOOT_BASE) - ref_vaddr) <= window]


def main():
    path = sys.argv[1]
    data, seg_off, seg_size = load_code_segment(path)
    print(f'file: {path}  code file_off={seg_off:#x} size={seg_size:#x}')
    # value -> list of reference vaddrs where the XML writes over it
    probes = [
        ('width 1920 (80070000)', '80070000',
         [0x5f0b3b, 0x564400, 0x5ff769, 0x7c46b4]),
        ('height 1080 (38040000)', '38040000',
         [0x5f0b43, 0x5643f3, 0x5ff76f, 0x7c46b8, 0x7f2a40]),
        ('width 1920 word (8007)', '8007', [0x5f0b4c]),
        ('height 1080 word (3804)', '3804', [0x5f0b53]),
        ('fps 59.94 float (23460c3f-ish)', '23460c3f', [0x75dbe4]),
    ]
    for name, hexval, refs in probes:
        needle = bytes.fromhex(hexval)
        hits = scan(data, seg_off, seg_size, needle)
        print(f'\n{name}: total {len(hits)} occurrences')
        for ref in refs:
            local = near(ref, hits)
            if local:
                for h in local:
                    found_vaddr = h + EBOOT_BASE
                    print(f'   ref {ref:#x}: found {found_vaddr:#x} (delta {found_vaddr - ref:+#x})')
            else:
                print(f'   ref {ref:#x}: no occurrence within +-0x400')


if __name__ == '__main__':
    main()
