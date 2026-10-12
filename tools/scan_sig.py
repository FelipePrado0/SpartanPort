#!/usr/bin/env python3
"""Scan the prepared eboot.elf for the Very Hard cheat signature and show context.

Usage: python scan_sig.py <eboot.elf> <hexsig> [offset]
Builds the needle at runtime (bytes.fromhex) so no opcode literals sit in the source.
"""
import sys


def main():
    path = sys.argv[1]
    sig = bytes.fromhex(sys.argv[2].replace(' ', ''))
    data = open(path, 'rb').read()
    print(f'file: {path}  size: {len(data)}')
    print(f'needle ({len(sig)} bytes): {sig.hex(" ")}')

    if len(sys.argv) > 3:
        off = int(sys.argv[3], 0)
        window = data[off:off + max(16, len(sig))]
        print(f'bytes at {off:#x}: {window.hex(" ")}')

    hits = []
    start = 0
    while True:
        i = data.find(sig, start)
        if i < 0:
            break
        hits.append(i)
        start = i + 1
    print(f'exact matches: {len(hits)}')
    for i in hits:
        ctx = data[max(0, i - 8):i + len(sig) + 8]
        print(f'  at {i:#x}: ...{ctx.hex(" ")}...')


if __name__ == '__main__':
    main()
