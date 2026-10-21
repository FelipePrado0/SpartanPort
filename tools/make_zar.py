#!/usr/bin/env python3
"""Packs a folder as a .zar (ZArchive): the packed game image shadPS4 mounts as /app0.

  python tools/make_zar.py <game folder> [OUT.zar]

The folder's contents go at the archive root (eboot.bin, sce_sys/, ...), the layout the port
reads with no difference to an unpacked dump. Data is stored in 64 KiB blocks compressed with
zstd; a block that does not get smaller is stored as it is, which is what ZArchiveWriter does.

The format (big-endian, from https://github.com/Exzap/ZArchive): compressed data, block offset
records, name table, file tree, empty metadata sections, footer. A file's offset counts from the
start of the compressed data; the file tree stores, for each directory, the first child record and
its count, so children are laid out contiguously (breadth first here, as the reference writer).

zstd compression comes from Python 3.14's compression.zstd or the `zstandard` package.
"""
import argparse
import hashlib
import os
from pathlib import Path
import struct
import sys

BLOCK = 64 * 1024
RECORDS_PER_OFFSET = 16
FOOTER_SIZE = 144
MAGIC = 0x169F52D6
VERSION = 0x61BF3A01
LEVEL = 6


def compressor():
    """(compress, decompress unavailable) callable for one 64 KiB block."""
    try:
        from compression import zstd
    except ImportError:
        try:
            import zstandard
        except ImportError:
            raise SystemExit('make_zar: needs Python 3.14 or the zstandard package to compress')
        return zstandard.ZstdCompressor(level=LEVEL).compress
    return lambda data: zstd.compress(data, LEVEL)


class Node:
    def __init__(self, name, is_file, source=None):
        self.name = name          # bytes
        self.is_file = is_file
        self.source = source      # Path for files
        self.children = []
        self.size = 0
        self.offset = 0
        self.first = 0
        self.count = 0


def walk(folder):
    """The folder as nodes; names are sorted without regard to case, as the reference writer."""
    root = Node(b'', False)

    def fill(node, directory):
        entries = sorted(directory.iterdir(), key=lambda p: p.name.casefold())
        for entry in entries:
            name = entry.name.encode('windows-1252', 'replace')[:0x7FFF]
            if entry.is_dir() and not entry.is_symlink():
                child = Node(name, False)
                node.children.append(child)
                fill(child, entry)
            elif entry.is_file():
                node.children.append(Node(name, True, entry))

    fill(root, Path(folder))
    return root


def flatten(root):
    """Breadth first, children of one directory contiguous; the root is record 0."""
    flat = [root]
    queue = [root]
    while queue:
        directory = queue.pop(0)
        directory.first = len(flat)
        directory.count = len(directory.children)
        flat.extend(directory.children)
        queue.extend(child for child in directory.children if not child.is_file)
    return flat


def name_table(flat):
    """(table bytes, offsets): a 7-bit length with bit 7 set when a second byte holds bits 7-14."""
    table = bytearray()
    offsets = {}
    for node in flat:
        if node is flat[0]:
            offsets[id(node)] = 0x7FFFFFFF
            continue
        offsets[id(node)] = len(table)
        size = len(node.name)
        if size >= 0x80:
            table += bytes([(size & 0x7F) | 0x80, size >> 7])
        else:
            table += bytes([size])
        table += node.name
    return table, offsets


def pack(folder, out):
    compress = compressor()
    root = walk(folder)
    flat = flatten(root)
    files = [node for node in flat if node.is_file]
    digest = hashlib.sha256()

    def write(stream, data):
        stream.write(data)
        digest.update(data)

    records = []      # (base offset of the group, compressed size - 1 of each block)
    blocks = 0
    input_offset = 0  # uncompressed offset: a file's offset counts these bytes, not compressed ones

    def store(stream, block):
        nonlocal blocks, input_offset
        offset = stream.tell()
        packed = compress(block)
        if len(packed) >= BLOCK:
            packed = block
        write(stream, packed)
        if blocks % RECORDS_PER_OFFSET == 0:
            records.append((offset, []))
        records[-1][1].append(len(packed) - 1)
        blocks += 1
        input_offset += BLOCK

    with out.open('wb') as stream:
        buffer = bytearray()
        for node in files:
            node.offset = input_offset
            node.size = node.source.stat().st_size
            with node.source.open('rb') as source:
                while chunk := source.read(BLOCK):
                    buffer += chunk
                    if len(buffer) == BLOCK:
                        store(stream, bytes(buffer))
                        buffer.clear()
            if buffer:  # each file ends on a block boundary, padded with zeros
                buffer += bytes(BLOCK - len(buffer))
                store(stream, bytes(buffer))
                buffer.clear()
        data_size = stream.tell()
        # 8-byte alignment before the tables, as the reference writer leaves it
        if data_size % 8:
            write(stream, bytes(8 - data_size % 8))
        records_at = stream.tell()
        for base, sizes in records:
            write(stream, struct.pack('>Q', base) + struct.pack('>16H', *(sizes + [0] * (16 - len(sizes)))))
        records_size = stream.tell() - records_at
        names_at = stream.tell()
        names, name_offsets = name_table(flat)
        write(stream, names)
        tree_at = stream.tell()
        for node in flat:
            name_offset = name_offsets[id(node)]
            if node.is_file:
                high = ((node.offset >> 32) & 0xFFFF) | ((node.size >> 16) & 0xFFFF0000)
                write(stream, struct.pack('>IIII', 0x80000000 | name_offset, node.offset & 0xFFFFFFFF,
                                          node.size & 0xFFFFFFFF, high))
            else:
                write(stream, struct.pack('>IIII', name_offset, node.first, node.count, 0))
        meta_at = stream.tell()
        footer = struct.pack('>12Q', 0, data_size, records_at, records_size,
                             names_at, len(names), tree_at, len(flat) * 16, meta_at, 0, meta_at, 0)
        footer += bytes(32) + struct.pack('>QII', stream.tell() + FOOTER_SIZE, VERSION, MAGIC)
        assert len(footer) == FOOTER_SIZE
        digest.update(footer)  # the hash covers the footer with the hash field zeroed
        write(stream, footer[:96] + digest.digest() + footer[128:])
    return root, len(flat), len(files), data_size


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('folder', type=Path, help='game folder (the one with eboot.bin)')
    parser.add_argument('out', type=Path, nargs='?', help='the .zar to write')
    args = parser.parse_args()
    if not args.folder.is_dir():
        parser.exit(1, f'make_zar: {args.folder} is not a folder\n')
    out = args.out or args.folder.with_suffix('.zar')
    root, entries, files, data = pack(args.folder, out)
    print(f'{out}: {entries:,} entries ({files:,} files), {data:,} bytes of data, '
          f'{out.stat().st_size:,} bytes on disk')


if __name__ == '__main__':
    sys.exit(main())
