#!/usr/bin/env python3
"""Extract and replace the MSGS_TXT block in a God of War WAD."""
from __future__ import annotations

import argparse
from pathlib import Path
import struct

HEADER_SIZE = 0x40


def entries(data: bytes):
    """Yield ``(offset, size, name)`` for each valid WAD entry."""
    offset = 0
    while offset + HEADER_SIZE <= len(data):
        size = struct.unpack_from('<I', data, offset + 4)[0]
        end = offset + HEADER_SIZE + size
        if end > len(data):
            raise ValueError(f'WAD entry at {offset} extends beyond the file')
        name = data[offset + 8:offset + HEADER_SIZE].split(b'\0', 1)[0].decode('latin1')
        yield offset, size, name
        offset += HEADER_SIZE + ((size + 15) & ~15)
    if offset != len(data):
        raise ValueError(f'trailing bytes after WAD entries: {len(data) - offset}')


def extract_messages(data: bytes) -> bytes:
    """Return the payload of the sole ``MSGS_TXT`` entry."""
    matches = [(offset, size) for offset, size, name in entries(data) if name == 'MSGS_TXT']
    if len(matches) != 1:
        raise ValueError(f'expected one MSGS_TXT entry, found {len(matches)}')
    offset, size = matches[0]
    return data[offset + HEADER_SIZE:offset + HEADER_SIZE + size]


def replace_messages(data: bytes, messages: bytes) -> bytes:
    """Return a WAD with ``MSGS_TXT`` replaced; all other entries are preserved."""
    found = False
    output = bytearray()
    for offset, size, name in entries(data):
        header = data[offset:offset + HEADER_SIZE]
        payload = data[offset + HEADER_SIZE:offset + HEADER_SIZE + size]
        if name == 'MSGS_TXT':
            if found:
                raise ValueError('multiple MSGS_TXT entries')
            found = True
            payload = messages
            header = bytearray(header)
            struct.pack_into('<I', header, 4, len(payload))
            header = bytes(header)
        output.extend(header)
        output.extend(payload)
        output.extend(b'\0' * ((-len(payload)) % 16))
    if not found:
        raise ValueError('MSGS_TXT entry not found')
    return bytes(output)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    extract = sub.add_parser('extract')
    extract.add_argument('wad', type=Path)
    extract.add_argument('output', type=Path)
    replace = sub.add_parser('replace')
    replace.add_argument('wad', type=Path)
    replace.add_argument('messages', type=Path)
    replace.add_argument('output', type=Path)
    args = parser.parse_args()

    if args.command == 'extract':
        args.output.write_bytes(extract_messages(args.wad.read_bytes()))
    else:
        args.output.write_bytes(replace_messages(args.wad.read_bytes(), args.messages.read_bytes()))


if __name__ == '__main__':
    main()
