/* ZArchive (.zar) reader against tests/data/zarchive.zar, which tools/make_zar.py packs from the
 * fixture folder below. No game dump is needed: the archive holds text, a compressible file and a
 * stored (incompressible) one that crosses block boundaries, plus a name longer than 127 bytes.
 *
 *   zarchive-test [tests/data/zarchive.zar]
 *
 * Skipped (exit 0) when libzstd is not installed: the reader then refuses compressed archives,
 * which the message says. */
#include "../src/runtime_zarchive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WAD_SIZE 140000u
#define COMPRESSIBLE_SIZE (11u * 20000u)
#define LONG_NAME_LENGTH 130u

static int failures;

static void check(int condition, const char *what) {
    if (!condition) {
        fprintf(stderr, "zarchive-test: FAILED %s\n", what);
        ++failures;
    }
}

/* The stored block's bytes, as the fixture was built: a 31-bit LCG's high byte. */
static unsigned int lcg_next(unsigned int x) {
    return (unsigned int)((x * 1103515245ull + 12345ull) & 0x7fffffffull);
}

static unsigned char lcg_byte(unsigned index) {
    unsigned int x = 1;
    for (unsigned i = 0; i <= index; ++i) x = lcg_next(x);
    return (unsigned char)((x >> 16) & 0xffu);
}

static void check_range(ZArchive *archive, int64_t node, const char *what) {
    unsigned char chunk[4096];
    uint64_t offset = 0;
    while (offset < WAD_SIZE) {
        uint64_t want = WAD_SIZE - offset < sizeof(chunk) ? WAD_SIZE - offset : sizeof(chunk);
        uint64_t got = zarchive_read(archive, node, offset, chunk, want);
        if (got != want) {
            fprintf(stderr, "zarchive-test: FAILED %s: read %llu bytes at %llu\n", what,
                    (unsigned long long)got, (unsigned long long)offset);
            ++failures;
            return;
        }
        unsigned int x = 1;
        for (unsigned i = 0; i < (unsigned)offset; ++i) x = lcg_next(x);
        for (uint64_t i = 0; i < want; ++i) {
            x = lcg_next(x);
            if (chunk[i] != (unsigned char)((x >> 16) & 0xffu)) {
                fprintf(stderr, "zarchive-test: FAILED %s: byte %llu\n", what,
                        (unsigned long long)(offset + i));
                ++failures;
                return;
            }
        }
        offset += want;
    }
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "tests/data/zarchive.zar";
    ZArchive *archive = zarchive_open(path);
    if (!archive) {
        const char *reason = zarchive_error();
        if (strstr(reason, "libzstd")) {
            printf("zarchive-test: skipped, %s\n", reason);
            return 0;
        }
        fprintf(stderr, "zarchive-test: FAILED to open %s: %s\n", path, reason);
        return 1;
    }
    check(zarchive_size(archive) > 0, "archive size");
    check(zarchive_entries(archive) >= 14, "entry count");

    /* Root and case-insensitive paths. */
    int64_t root = zarchive_find(archive, "");
    check(root == 0, "empty path is the root");
    check(zarchive_find(archive, "/") == 0, "slash is the root");
    int64_t hello = zarchive_find(archive, "hello.txt");
    check(hello != ZARCHIVE_NONE && zarchive_is_file(archive, hello), "hello.txt is a file");
    check(zarchive_entry_size(archive, hello) == 17, "hello.txt size");
    check(zarchive_find(archive, "/HELLO.TXT") == hello, "case-insensitive lookup");
    check(zarchive_find(archive, "\\hello.txt") == hello, "backslash separators");
    char text[32] = {0};
    check(zarchive_read(archive, hello, 0, text, 17) == 17, "hello.txt read");
    check(!strcmp(text, "SpartanPort .zar\n"), "hello.txt content");
    check(zarchive_read(archive, hello, 0, text, sizeof(text)) == 17, "read stops at EOF");
    check(zarchive_read(archive, hello, 17, text, 1) == 0, "read past EOF");

    /* Nesting, including the empty file. */
    int64_t sfo = zarchive_find(archive, "sce_sys/param.sfo");
    check(sfo != ZARCHIVE_NONE && zarchive_is_file(archive, sfo), "param.sfo");
    unsigned char magic[64] = {0};
    check(zarchive_read_all(archive, "sce_sys/param.sfo", magic, sizeof(magic)) == 64, "param.sfo read");
    check(!memcmp(magic, "\0PSF", 4), "param.sfo magic");
    check(zarchive_read_all(archive, "sce_sys/param.sfo", magic, 4) == -1, "read_all refuses a small buffer");
    int64_t empty = zarchive_find(archive, "empty.bin");
    check(empty != ZARCHIVE_NONE && zarchive_entry_size(archive, empty) == 0, "empty file");
    check(zarchive_read(archive, empty, 0, text, 4) == 0, "empty file read");

    /* A name of 130 characters uses the two-byte length header. */
    char long_name[LONG_NAME_LENGTH + 5];
    memset(long_name, 'x', LONG_NAME_LENGTH);
    memcpy(long_name + LONG_NAME_LENGTH, ".bin", 4);
    long_name[LONG_NAME_LENGTH + 4] = 0;
    int64_t long_node = zarchive_find(archive, long_name);
    check(long_node != ZARCHIVE_NONE && zarchive_entry_size(archive, long_node) == 8, "long name");

    /* Stored (incompressible) blocks crossing 64 KiB boundaries. */
    int64_t wad = zarchive_find(archive, "exec/wad/ps4_le/r_ui.wad");
    check(wad != ZARCHIVE_NONE && zarchive_entry_size(archive, wad) == WAD_SIZE, "R_UI.WAD size");
    check_range(archive, wad, "R_UI.WAD");
    unsigned char crossing[8];
    check(zarchive_read(archive, wad, 65532, crossing, sizeof(crossing)) == 8, "read across a block");
    for (unsigned i = 0; i < sizeof(crossing); ++i) {
        check(crossing[i] == lcg_byte(65532 + i), "bytes across a block");
    }
    check(zarchive_read(archive, wad, WAD_SIZE - 3, crossing, sizeof(crossing)) == 3, "short tail");

    /* Compressed blocks. */
    int64_t compressible = zarchive_find(archive, "compressible.txt");
    check(compressible != ZARCHIVE_NONE && zarchive_entry_size(archive, compressible) == COMPRESSIBLE_SIZE,
          "compressible.txt size");
    unsigned char *whole = malloc(COMPRESSIBLE_SIZE);
    check(whole && zarchive_read(archive, compressible, 0, whole, COMPRESSIBLE_SIZE) == COMPRESSIBLE_SIZE,
          "compressible.txt read");
    int same = whole != NULL;
    for (unsigned i = 0; same && i < COMPRESSIBLE_SIZE; ++i) same = whole[i] == "SPARTANPORT"[i % 11];
    check(same, "compressible.txt content");
    free(whole);

    /* Rejected paths and the listing of a directory. */
    check(zarchive_find(archive, "missing.txt") == ZARCHIVE_NONE, "missing path");
    check(zarchive_find(archive, "../zarchive.zar") == ZARCHIVE_NONE, "parent component");
    check(zarchive_find(archive, "hello.txt/inner") == ZARCHIVE_NONE, "through a file");
    int64_t nested = zarchive_find(archive, "dir/sub");
    check(nested != ZARCHIVE_NONE && !zarchive_is_file(archive, nested), "dir/sub is a directory");
    char child[256];
    int is_file = -1;
    check(zarchive_child(archive, nested, 0, child, sizeof(child), &is_file) &&
          !strcmp(child, "nested.txt") && is_file, "nested listing");
    check(!zarchive_child(archive, nested, 1, child, sizeof(child), &is_file), "listing ends");
    int root_files = 0, root_dirs = 0;
    for (unsigned i = 0; zarchive_child(archive, root, i, child, sizeof(child), &is_file); ++i) {
        if (is_file) ++root_files; else ++root_dirs;
    }
    check(root_files == 4 && root_dirs == 3, "root listing");
    check(!zarchive_child(archive, hello, 0, child, sizeof(child), &is_file),
          "a file has no children");

    zarchive_close(archive);
    check(!zarchive_open("tests/data/not-an-archive.zar"), "missing archive is rejected");
    if (failures) {
        fprintf(stderr, "zarchive-test: %d failure(s)\n", failures);
        return 1;
    }
    printf("zarchive-test: ok\n");
    return 0;
}
