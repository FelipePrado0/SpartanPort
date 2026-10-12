#!/usr/bin/env python3
"""Print the PT_LOAD program headers of an ELF so we can map a file offset to a vaddr."""
import struct
import sys


def main():
    data = open(sys.argv[1], 'rb').read()
    phoff = struct.unpack_from('<Q', data, 0x20)[0]
    phentsize, phnum = struct.unpack_from('<HH', data, 0x36)
    print(f'phoff={phoff:#x} phnum={phnum}')
    for i in range(phnum):
        kind, flags, off, vaddr, paddr, filesz, memsz, align = struct.unpack_from(
            '<IIQQQQQQ', data, phoff + i * phentsize)
        print(f'type={kind} flags={flags} off={off:#x} vaddr={vaddr:#x} '
              f'filesz={filesz:#x} memsz={memsz:#x}')

    if len(sys.argv) > 2:
        file_off = int(sys.argv[2], 0)
        for i in range(phnum):
            kind, flags, off, vaddr, paddr, filesz, memsz, align = struct.unpack_from(
                '<IIQQQQQQ', data, phoff + i * phentsize)
            if kind == 1 and off <= file_off < off + filesz:
                print(f'file offset {file_off:#x} -> vaddr {vaddr + (file_off - off):#x} '
                      f'(in segment off={off:#x} vaddr={vaddr:#x})')


if __name__ == '__main__':
    main()
