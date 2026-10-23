/* Read-only ZArchive (.zar) reader: see runtime_zarchive.h for the format and the API.
 *
 * The tables (file tree, names, block offsets) load once, at open; file data is decompressed on
 * demand, 64 KiB at a time, through an LRU cache of 4 MiB. libzstd is resolved at run time so the
 * build needs neither its headers nor its import library, and a missing library is reported as
 * such instead of failing the whole port. */
/* off_t, fseeko, ftello and dlopen: -std=c11 alone is strict ISO C and hides them. */
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include "runtime_zarchive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <pthread.h>
#endif

#define BLOCK_SIZE 65536u            /* uncompressed bytes per block (the format's constant) */
#define RECORDS_PER_OFFSET 16u       /* block sizes in one offset record */
#define OFFSET_RECORD_SIZE 40u       /* on disk: base offset (8) and 16 sizes (2 each) */
#define TREE_RECORD_SIZE 16u
#define FOOTER_SIZE 144u
#define CACHE_BLOCKS 64u             /* 4 MiB of decompressed blocks */
#define MAX_NAMES (64u << 20)        /* tables larger than this are not a game dump */
#define MAX_TREE (128u << 20)
#define MAX_RECORDS (32u << 20)      /* bytes; 32 MiB covers 838,000 blocks (52 GiB) */
#define MAX_DEPTH 64u

typedef struct {
    uint32_t name;   /* offset into the name table */
    int file;
    uint64_t offset; /* files: first byte of the file's data in the compressed section */
    uint64_t size;   /* files: length */
    uint32_t first;  /* directories: first child record */
    uint32_t count;  /* directories: number of children */
} Node;

typedef struct {
    uint64_t base;
    uint16_t size[RECORDS_PER_OFFSET];
} OffsetRecord;

typedef struct {
    uint64_t block;  /* which 64 KiB block this holds; valid when age differs from 0 */
    uint64_t age;
    unsigned char *data;
} CacheBlock;

/* Reads may come from several guest threads: an exclusive lock covers the block cache. Windows
 * uses its own slim lock; the port's pthreads are only needed on the other platforms. */
#ifdef _WIN32
typedef SRWLOCK ZaLock;
static void za_lock_init(ZaLock *lock) { InitializeSRWLock(lock); }
static void za_lock_destroy(ZaLock *lock) { (void)lock; }
static void za_lock_take(ZaLock *lock) { AcquireSRWLockExclusive(lock); }
static void za_lock_release(ZaLock *lock) { ReleaseSRWLockExclusive(lock); }
#else
typedef pthread_mutex_t ZaLock;
static void za_lock_init(ZaLock *lock) { pthread_mutex_init(lock, NULL); }
static void za_lock_destroy(ZaLock *lock) { pthread_mutex_destroy(lock); }
static void za_lock_take(ZaLock *lock) { pthread_mutex_lock(lock); }
static void za_lock_release(ZaLock *lock) { pthread_mutex_unlock(lock); }
#endif

struct ZArchive {
    FILE *file;
    uint64_t size;
    Node *nodes;
    uint32_t node_count;
    unsigned char *names;
    uint32_t names_size;
    OffsetRecord *records;
    uint32_t record_count;
    uint64_t data_offset;
    uint64_t data_size;
    CacheBlock cache[CACHE_BLOCKS];
    unsigned char *scratch; /* one compressed block, read before decompression */
    uint64_t clock;
    ZaLock lock;
};

/* libzstd, loaded once for the process. */
static struct {
    int tried;
    int ok;
#ifdef _WIN32
    HMODULE module;
#else
    void *module;
#endif
    size_t (*decompress)(void *, size_t, const void *, size_t);
    unsigned (*is_error)(size_t);
} zstd;

static char last_error[160];

const char *zarchive_error(void) { return last_error; }

static void set_error(const char *message) {
    snprintf(last_error, sizeof(last_error), "%s", message);
}

static int zstd_load(void) {
    if (zstd.tried) return zstd.ok;
    zstd.tried = 1;
#ifdef _WIN32
    zstd.module = LoadLibraryA("libzstd.dll");
    if (zstd.module) {
        zstd.decompress = (size_t (*)(void *, size_t, const void *, size_t))(void *)
            GetProcAddress(zstd.module, "ZSTD_decompress");
        zstd.is_error = (unsigned (*)(size_t))(void *)GetProcAddress(zstd.module, "ZSTD_isError");
    }
#else
    zstd.module = dlopen("libzstd.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!zstd.module) zstd.module = dlopen("libzstd.so", RTLD_LAZY | RTLD_LOCAL);
    if (zstd.module) {
        zstd.decompress = (size_t (*)(void *, size_t, const void *, size_t))dlsym(zstd.module, "ZSTD_decompress");
        zstd.is_error = (unsigned (*)(size_t))dlsym(zstd.module, "ZSTD_isError");
    }
#endif
    zstd.ok = zstd.decompress != NULL && zstd.is_error != NULL;
    return zstd.ok;
}

int zarchive_library_available(void) { return zstd_load(); }

static uint16_t be16(const unsigned char *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t be32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint64_t be64(const unsigned char *p) {
    return ((uint64_t)be32(p) << 32) | be32(p + 4);
}

static int seek_file(ZArchive *archive, uint64_t offset) {
#ifdef _WIN32
    return _fseeki64(archive->file, (long long)offset, SEEK_SET) == 0;
#else
    return fseeko(archive->file, (off_t)offset, SEEK_SET) == 0;
#endif
}

static int read_at(ZArchive *archive, uint64_t offset, void *buffer, size_t size) {
    if (offset > archive->size || size > archive->size - offset) return 0;
    if (!seek_file(archive, offset)) return 0;
    return fread(buffer, 1, size, archive->file) == size;
}

/* Name table entries: one length byte (bit 7 set: a second byte follows and the length spans
 * 15 bits), then the name bytes (Windows-1252, used as opaque bytes). */
static const char *name_of(ZArchive *archive, uint32_t offset, size_t *length) {
    if (offset >= archive->names_size) return NULL;
    const unsigned char *table = archive->names;
    uint32_t size = table[offset] & 0x7Fu;
    uint32_t start = offset + 1;
    if (table[offset] & 0x80u) {
        if (start >= archive->names_size) return NULL;
        size |= (uint32_t)table[start] << 7;
        start++;
    }
    if (start > archive->names_size || size > archive->names_size - start) return NULL;
    if (length) *length = size;
    return (const char *)table + start;
}

static int node_ok(const ZArchive *archive, int64_t node) {
    return node >= 0 && (uint64_t)node < archive->node_count;
}

static int same_name(const char *a, size_t alen, const char *b, size_t blen) {
    if (alen != blen) return 0;
    for (size_t i = 0; i < alen; ++i) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return 0;
    }
    return 1;
}

/* Next path component of a guest path, skipping repeated and leading separators. */
static int next_component(const char **path, const char **start, size_t *length) {
    const char *p = *path;
    while (*p == '/' || *p == '\\') p++;
    if (!*p) {
        *path = p;
        return 0;
    }
    const char *from = p;
    while (*p && *p != '/' && *p != '\\') p++;
    *start = from;
    *length = (size_t)(p - from);
    *path = p;
    return 1;
}

ZArchive *zarchive_open(const char *path) {
    set_error("");
    FILE *file = fopen(path, "rb");
    if (!file) {
        set_error("cannot open the archive");
        return NULL;
    }
#ifdef _WIN32
    if (_fseeki64(file, 0, SEEK_END)) {
#else
    if (fseeko(file, 0, SEEK_END)) {
#endif
        fclose(file);
        set_error("cannot read the archive");
        return NULL;
    }
#ifdef _WIN32
    long long end = _ftelli64(file);
#else
    off_t end = ftello(file);
#endif
    if (end <= (long long)FOOTER_SIZE) {
        fclose(file);
        set_error("not a ZArchive (too small)");
        return NULL;
    }
    uint64_t size = (uint64_t)end;
    unsigned char footer_bytes[FOOTER_SIZE];
    ZArchive *archive = calloc(1, sizeof(*archive));
    if (!archive) {
        fclose(file);
        set_error("out of memory");
        return NULL;
    }
    archive->file = file;
    archive->size = size;
    za_lock_init(&archive->lock);
    if (!read_at(archive, size - FOOTER_SIZE, footer_bytes, sizeof(footer_bytes))) {
        zarchive_close(archive);
        set_error("cannot read the footer");
        return NULL;
    }
    uint32_t magic = be32(footer_bytes + FOOTER_SIZE - 4);
    uint32_t version = be32(footer_bytes + FOOTER_SIZE - 8);
    uint64_t total = be64(footer_bytes + FOOTER_SIZE - 16);
    if (magic != 0x169f52d6u || version != 0x61bf3a01u || total != size) {
        zarchive_close(archive);
        set_error("not a ZArchive (bad footer)");
        return NULL;
    }
    uint64_t sections[6][2];
    for (unsigned i = 0; i < 6; ++i) {
        sections[i][0] = be64(footer_bytes + i * 16);
        sections[i][1] = be64(footer_bytes + i * 16 + 8);
        if (sections[i][0] > size || sections[i][1] > size - sections[i][0]) {
            zarchive_close(archive);
            set_error("not a ZArchive (section outside the file)");
            return NULL;
        }
    }
    archive->data_offset = sections[0][0];
    archive->data_size = sections[0][1];
    uint64_t offsets_size = sections[1][1], names_size = sections[2][1], tree_size = sections[3][1];
    if (offsets_size % OFFSET_RECORD_SIZE || !offsets_size || names_size > MAX_NAMES ||
        tree_size % TREE_RECORD_SIZE || !tree_size || tree_size > MAX_TREE ||
        tree_size / TREE_RECORD_SIZE > 0xFFFFFFFFu || offsets_size > MAX_RECORDS) {
        zarchive_close(archive);
        set_error("not a ZArchive (bad table sizes)");
        return NULL;
    }
    archive->record_count = (uint32_t)(offsets_size / OFFSET_RECORD_SIZE);
    archive->node_count = (uint32_t)(tree_size / TREE_RECORD_SIZE);
    archive->names_size = (uint32_t)names_size;
    archive->records = malloc((size_t)archive->record_count * sizeof(OffsetRecord));
    archive->nodes = malloc((size_t)archive->node_count * sizeof(Node));
    archive->names = malloc(names_size ? (size_t)names_size : 1);
    archive->scratch = malloc(BLOCK_SIZE);
    unsigned char *raw_offsets = malloc((size_t)offsets_size);
    unsigned char *raw_tree = malloc((size_t)tree_size);
    int all_memory = archive->records && archive->nodes && archive->names && archive->scratch &&
                     raw_offsets && raw_tree;
    for (unsigned i = 0; all_memory && i < CACHE_BLOCKS; ++i) {
        archive->cache[i].data = malloc(BLOCK_SIZE);
        all_memory = archive->cache[i].data != NULL;
    }
    int tables = all_memory && read_at(archive, sections[1][0], raw_offsets, (size_t)offsets_size) &&
                 read_at(archive, sections[2][0], archive->names, (size_t)names_size) &&
                 read_at(archive, sections[3][0], raw_tree, (size_t)tree_size);
    int compressed = 0;
    for (uint32_t i = 0; tables && i < archive->record_count; ++i) {
        const unsigned char *entry = raw_offsets + (size_t)i * OFFSET_RECORD_SIZE;
        OffsetRecord *record = &archive->records[i];
        record->base = be64(entry);
        for (unsigned j = 0; j < RECORDS_PER_OFFSET; ++j) {
            record->size[j] = be16(entry + 8 + j * 2);
            if (record->size[j] != BLOCK_SIZE - 1) compressed = 1;
        }
    }
    for (uint32_t i = 0; tables && i < archive->node_count; ++i) {
        const unsigned char *entry = raw_tree + (size_t)i * TREE_RECORD_SIZE;
        Node *node = &archive->nodes[i];
        uint32_t name_and_flag = be32(entry);
        node->name = name_and_flag & 0x7FFFFFFFu;
        node->file = (name_and_flag & 0x80000000u) != 0;
        uint32_t low = be32(entry + 4);
        uint32_t high = be32(entry + 12);
        if (node->file) {
            /* Files: offset low, size low, then 16 bits each of the extensions. */
            node->offset = (uint64_t)low | ((uint64_t)(high & 0xFFFFu) << 32);
            node->size = (uint64_t)be32(entry + 8) | ((uint64_t)(high >> 16) << 32);
            node->first = node->count = 0;
        } else {
            /* Directories: first child record and its count. */
            node->first = low;
            node->count = be32(entry + 8);
        }
    }
    free(raw_offsets);
    free(raw_tree);
    if (!all_memory) {
        zarchive_close(archive);
        set_error("out of memory");
        return NULL;
    }
    if (!tables) {
        zarchive_close(archive);
        set_error("cannot read the archive tables");
        return NULL;
    }
    size_t root_length = 0;
    const char *root_name = name_of(archive, archive->nodes[0].name, &root_length);
    if (archive->nodes[0].file || (root_name && root_length != 0)) {
        zarchive_close(archive);
        set_error("not a ZArchive (bad root)");
        return NULL;
    }
    if (compressed && !zstd_load()) {
        zarchive_close(archive);
        set_error("ZArchive support needs libzstd, which was not found");
        return NULL;
    }
    return archive;
}

void zarchive_close(ZArchive *archive) {
    if (!archive) return;
    if (archive->file) fclose(archive->file);
    free(archive->nodes);
    free(archive->names);
    free(archive->records);
    free(archive->scratch);
    for (unsigned i = 0; i < CACHE_BLOCKS; ++i) free(archive->cache[i].data);
    za_lock_destroy(&archive->lock);
    free(archive);
}

uint64_t zarchive_size(const ZArchive *archive) { return archive ? archive->size : 0; }
uint32_t zarchive_entries(const ZArchive *archive) { return archive ? archive->node_count : 0; }

int64_t zarchive_find(ZArchive *archive, const char *path) {
    if (!archive || !path) return ZARCHIVE_NONE;
    int64_t node = 0;
    int64_t current = 0;
    const char *rest = path;
    const char *start;
    size_t length;
    unsigned depth = 0;
    while (next_component(&rest, &start, &length)) {
        if ((length == 1 && start[0] == '.') || (length == 2 && start[0] == '.' && start[1] == '.'))
            return ZARCHIVE_NONE;
        if (++depth > MAX_DEPTH) return ZARCHIVE_NONE;
        Node *dir = &archive->nodes[current];
        if (dir->file || dir->first > archive->node_count ||
            dir->count > archive->node_count - dir->first)
            return ZARCHIVE_NONE;
        int found = 0;
        for (uint32_t i = 0; i < dir->count; ++i) {
            Node *child = &archive->nodes[dir->first + i];
            size_t name_length;
            const char *name = name_of(archive, child->name, &name_length);
            if (name && same_name(name, name_length, start, length)) {
                current = (int64_t)(dir->first + i);
                node = current;
                found = 1;
                break;
            }
        }
        if (!found) return ZARCHIVE_NONE;
    }
    return node;
}

int zarchive_is_file(const ZArchive *archive, int64_t node) {
    if (!archive || !node_ok(archive, node)) return 0;
    return archive->nodes[node].file;
}

uint64_t zarchive_entry_size(const ZArchive *archive, int64_t node) {
    if (!archive || !node_ok(archive, node) || !archive->nodes[node].file) return 0;
    return archive->nodes[node].size;
}

int zarchive_child(ZArchive *archive, int64_t node, uint32_t index, char *name, size_t size,
                   int *is_file) {
    if (!archive || !name || !size || !node_ok(archive, node)) return 0;
    Node *dir = &archive->nodes[node];
    if (dir->file || index >= dir->count || dir->first > archive->node_count ||
        dir->count > archive->node_count - dir->first)
        return 0;
    Node *child = &archive->nodes[dir->first + index];
    size_t length = 0;
    const char *text = name_of(archive, child->name, &length);
    if (!text) return 0;
    if (length >= size) length = size - 1;
    memcpy(name, text, length);
    name[length] = 0;
    if (is_file) *is_file = child->file;
    return 1;
}

/* The 64 KiB block `block`, decompressed into the cache; NULL when it cannot be read. */
static const unsigned char *block_data(ZArchive *archive, uint64_t block) {
    if (block >= (uint64_t)archive->record_count * RECORDS_PER_OFFSET) return NULL;
    uint32_t victim = 0;
    for (uint32_t i = 0; i < CACHE_BLOCKS; ++i) {
        if (archive->cache[i].age && archive->cache[i].block == block) {
            archive->cache[i].age = ++archive->clock;
            return archive->cache[i].data;
        }
        if (!archive->cache[victim].age || archive->cache[i].age < archive->cache[victim].age)
            victim = i;
    }
    OffsetRecord *record = &archive->records[block / RECORDS_PER_OFFSET];
    unsigned sub = (unsigned)(block % RECORDS_PER_OFFSET);
    uint64_t offset = record->base;
    for (unsigned i = 0; i < sub; ++i) offset += (uint64_t)record->size[i] + 1;
    uint32_t compressed = (uint32_t)record->size[sub] + 1;
    if (offset > archive->data_size || compressed > archive->data_size - offset) return NULL;
    CacheBlock *slot = &archive->cache[victim];
    slot->age = 0;
    if (!read_at(archive, archive->data_offset + offset,
                 compressed == BLOCK_SIZE ? slot->data : archive->scratch, compressed))
        return NULL;
    if (compressed == BLOCK_SIZE) {
        slot->block = block;
        slot->age = ++archive->clock;
        return slot->data;
    }
    size_t produced = zstd.decompress(slot->data, BLOCK_SIZE, archive->scratch, compressed);
    if (zstd.is_error(produced) || produced != BLOCK_SIZE) {
        slot->block = 0;
        return NULL;
    }
    slot->block = block;
    slot->age = ++archive->clock;
    return slot->data;
}

uint64_t zarchive_read(ZArchive *archive, int64_t node, uint64_t offset, void *buffer,
                       uint64_t length) {
    if (!archive || !buffer || !node_ok(archive, node)) return 0;
    Node *file = &archive->nodes[node];
    if (!file->file || offset >= file->size) return 0;
    if (length > file->size - offset) length = file->size - offset;
    uint64_t raw = file->offset + offset;
    uint64_t remaining = length;
    unsigned char *out = buffer;
    za_lock_take(&archive->lock);
    while (remaining) {
        uint64_t block = raw / BLOCK_SIZE;
        uint32_t within = (uint32_t)(raw % BLOCK_SIZE);
        uint64_t step = remaining < BLOCK_SIZE - within ? remaining : BLOCK_SIZE - within;
        const unsigned char *data = block_data(archive, block);
        if (!data) {
            za_lock_release(&archive->lock);
            return 0;
        }
        memcpy(out, data + within, (size_t)step);
        raw += step;
        out += step;
        remaining -= step;
    }
    za_lock_release(&archive->lock);
    return length;
}

int64_t zarchive_read_all(ZArchive *archive, const char *path, void *buffer, uint64_t size) {
    int64_t node = zarchive_find(archive, path);
    if (!archive || node == ZARCHIVE_NONE || !zarchive_is_file(archive, node)) return -1;
    uint64_t file_size = zarchive_entry_size(archive, node);
    if (file_size > size) return -1;
    if (file_size && zarchive_read(archive, node, 0, buffer, file_size) != file_size) return -1;
    return (int64_t)file_size;
}
