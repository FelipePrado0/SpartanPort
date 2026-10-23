#ifndef SPARTANPORT_RUNTIME_ZARCHIVE_H
#define SPARTANPORT_RUNTIME_ZARCHIVE_H
#include <stddef.h>
#include <stdint.h>

/* Read-only reader for ZArchive (.zar): the packed game image format of Exzap's ZArchive, which
 * shadPS4 mounts as /app0. The archive holds the game folder at its root (eboot.bin, sce_sys/, ...)
 * and compresses it in 64 KiB blocks with zstd, so a guest read of any range of any file touches
 * only the blocks it needs.
 *
 * Layout (all integers big-endian; see https://github.com/Exzap/ZArchive):
 *   compressed data, offset records (18 bytes per 16 blocks), names, file tree, metadata, footer.
 * The footer at the end holds the section offsets, the archive size, the version and the magic.
 * File-tree records are 16 bytes: a name offset with the file flag, then either (offset, size) for
 * a file or (first child, count) for a directory. The root directory is record 0 and has no name.
 *
 * libzstd is loaded at run time (a DLL or .so, never a build-time link): without it, compressed
 * archives open fails with a message; blocks stored uncompressed would still read.
 *
 * Thread-safe: reads from one archive may come from several guest threads. */

typedef struct ZArchive ZArchive;

#define ZARCHIVE_NONE (-1)

/* Opens a .zar; NULL when the file is missing, not a ZArchive, or needs libzstd and cannot find
 * it. zarchive_error() then names the reason. */
ZArchive *zarchive_open(const char *path);

void zarchive_close(ZArchive *archive);

/* Bytes of the archive file itself. */
uint64_t zarchive_size(const ZArchive *archive);

/* File-tree records, the root included. */
uint32_t zarchive_entries(const ZArchive *archive);

/* Why the last zarchive_open() failed, or "" after a success. */
const char *zarchive_error(void);

/* Node handle of a path inside the archive ("/sce_sys/param.sfo", "eboot.bin", "" for the root);
 * ZARCHIVE_NONE when it does not exist. Names are matched without regard to ASCII case, as the
 * format defines. */
int64_t zarchive_find(ZArchive *archive, const char *path);

int zarchive_is_file(const ZArchive *archive, int64_t node);
uint64_t zarchive_entry_size(const ZArchive *archive, int64_t node);

/* Listing of a directory node: writes the child's name (NUL-terminated, truncated to the buffer)
 * into name and returns 1, or returns 0 past the end. */
int zarchive_child(ZArchive *archive, int64_t node, uint32_t index, char *name, size_t size,
                   int *is_file);

/* Reads up to length bytes at offset; returns the bytes read (short only at the end of the file)
 * or 0 when the range or the blocks cannot be read. */
uint64_t zarchive_read(ZArchive *archive, int64_t node, uint64_t offset, void *buffer,
                       uint64_t length);

/* Whole file into buffer of at most size bytes; its length, or -1 when missing or too large. */
int64_t zarchive_read_all(ZArchive *archive, const char *path, void *buffer, uint64_t size);

/* 1 when libzstd could be loaded (needed for every compressed block). */
int zarchive_library_available(void);

#endif
