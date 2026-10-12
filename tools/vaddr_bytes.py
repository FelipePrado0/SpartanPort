#!/usr/bin/env python3
"""Read bytes at PS4 vaddrs mapping through ALL PT_LOAD segments (not just the vaddr-0 one).

The data region (vaddr >= 0x3c0000) lives in a second PT_LOAD; a naive vaddr->offset using the
code segment is wrong there. This resolves each vaddr against the segment that contains it.

Usage: python vaddr_bytes.py <eboot.elf> <vaddr> [count]   (vaddr is a PS4 vaddr, base 0x400000)
       python vaddr_bytes.py <eboot.elf> --scan <hex>       (scan whole file, report vaddrs)
"""
import struct
import sys


EBOOT_BASE = 0x400000


def loads(path):
    data = open(path, 'rb').read()
    phoff = struct.unpack_from('<Q', data, 0x20)[0]
    phentsize, phnum = struct.unpack_from('<HH', data, 0x36)
    segs = []
    for i in range(phnum):
        kind, flags, off, vaddr, paddr, filesz, memsz, align = struct.unpack_from(
            '<IIQQQQQQ', data, phoff + i * phentsize)
        if kind == 1 and filesz:
            segs.append((vaddr, off, filesz))
    return data, segs


def vaddr_to_off(segs, image_vaddr):
    for vaddr, off, filesz in segs:
        if vaddr <= image_vaddr < vaddr + filesz:
            return off + (image_vaddr - vaddr)
    return None


def main():
    data, segs = loads(sys.argv[1])
    print('PT_LOAD segments (image vaddr -> file off, size):')
    for vaddr, off, filesz in segs:
        print(f'  vaddr={vaddr:#x} off={off:#x} filesz={filesz:#x}')
    if sys.argv[2] == '--scan':
        needle = bytes.fromhex(sys.argv[3].replace(' ', ''))
        start = 0
        hits = []
        while True:
            i = data.find(needle, start)
            if i < 0:
                break
            # map file offset back to an image vaddr
            iv = None
            for vaddr, off, filesz in segs:
                if off <= i < off + filesz:
                    iv = vaddr + (i - off)
                    break
            hits.append((i, iv))
            start = i + 1
        print(f'scan {needle.hex(" ")}: {len(hits)} hits')
        for fo, iv in hits:
            print(f'  file {fo:#x} image_vaddr={iv:#x if iv is not None else 0} '
                  f'ps4={(iv + EBOOT_BASE) if iv is not None else 0:#x}')
        return
    ps4 = int(sys.argv[2], 0)
    n = int(sys.argv[3], 0) if len(sys.argv) > 3 else 16
    image_vaddr = ps4 - EBOOT_BASE
    fo = vaddr_to_off(segs, image_vaddr)
    if fo is None:
        print(f'ps4 {ps4:#x} (image {image_vaddr:#x}) is not in any PT_LOAD filesz range')
        return
    print(f'ps4 {ps4:#x} -> image_vaddr {image_vaddr:#x} -> file off {fo:#x}')
    print(f'bytes: {data[fo:fo + n].hex(" ")}')


if __name__ == '__main__':
    main()
