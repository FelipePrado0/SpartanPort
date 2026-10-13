#include "isa_rewrite.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { HEADER_BYTES = 64, RECORD_BYTES = 64, MAX_SITES = 1u << 20, JMP_BYTES = 5 };

/* ---- SHA-256 (FIPS 180-4), for isa_table_matches ---- */

static uint32_t ror32(uint32_t x, int n) { return x >> n | x << (32 - n); }

static void sha256_block(uint32_t h[8], const unsigned char *block) {
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t w[64], v[8];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)block[4 * i] << 24 | (uint32_t)block[4 * i + 1] << 16 | (uint32_t)block[4 * i + 2] << 8 |
               block[4 * i + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ror32(w[i - 15], 7) ^ ror32(w[i - 15], 18) ^ w[i - 15] >> 3;
        uint32_t s1 = ror32(w[i - 2], 17) ^ ror32(w[i - 2], 19) ^ w[i - 2] >> 10;
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    memcpy(v, h, sizeof v);
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = v[7] + (ror32(v[4], 6) ^ ror32(v[4], 11) ^ ror32(v[4], 25)) + ((v[4] & v[5]) ^ (~v[4] & v[6])) +
                      k[i] + w[i];
        uint32_t t2 = (ror32(v[0], 2) ^ ror32(v[0], 13) ^ ror32(v[0], 22)) + ((v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]));
        memmove(v + 1, v, 7 * sizeof *v);
        v[4] += t1;
        v[0] = t1 + t2;
    }
    for (int i = 0; i < 8; i++) h[i] += v[i];
}

static void sha256(const unsigned char *data, uint64_t size, uint8_t out[32]) {
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    uint64_t done = 0;
    for (; size - done >= 64; done += 64) sha256_block(h, data + done);
    unsigned char tail[128] = {0};
    size_t rest = (size_t)(size - done), tail_size = rest < 56 ? 64 : 128;
    memcpy(tail, data + done, rest);
    tail[rest] = 0x80;
    for (int i = 0; i < 8; i++) tail[tail_size - 1 - i] = (unsigned char)(size * 8 >> 8 * i);
    for (size_t at = 0; at < tail_size; at += 64) sha256_block(h, tail + at);
    for (int i = 0; i < 32; i++) out[i] = (uint8_t)(h[i / 4] >> (24 - 8 * (i % 4)));
}

/* ---- table ---- */

static uint64_t read_u64(const unsigned char *p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = v << 8 | p[i];
    return v;
}

static uint32_t read_u32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int register_valid(uint8_t reg) { return reg <= 15 || reg == ISA_NO_REG; }

/* Field ranges of spec 4.4, plus what the patch writes: a 5-byte jmp needs a 5-byte site (mode 0)
 * or a relocated instruction (mode 1), an int3 site is 4 bytes, and every write stays inside the image.
 * The writer stores scale 0 without a memory operand, else iced's scale (1 when there is no index). */
static int record_valid(const IsaSite *s, uint64_t image_size) {
    if (s->length < 4 || s->length > 15 || s->relocated_length > 15) return 0;
    if (s->kind < ISA_MOVBE_LOAD || s->kind > ISA_VCVTPS2PH || s->mode > ISA_MODE_UNSUPPORTED) return 0;
    if (!register_valid(s->dst) || !register_valid(s->src1) || !register_valid(s->src2) || !register_valid(s->base) ||
        !register_valid(s->index) || s->mem_slot > ISA_MEM_SRC)
        return 0;
    if (s->kind <= ISA_BLSR && s->width != 16 && s->width != 32 && s->width != 64) return 0;
    if (s->scale != 1 && s->scale != 2 && s->scale != 4 && s->scale != 8 && !(s->scale == 0 && s->index == ISA_NO_REG))
        return 0;
    if (s->mode == ISA_MODE_JMP && s->length < JMP_BYTES) return 0;
    if (s->mode == ISA_MODE_INT3 && s->length != 4) return 0;
    if (s->mode == ISA_MODE_JMP_RELOCATE && (s->length != 4 || s->relocated_length == 0)) return 0;
    uint64_t end = s->mode == ISA_MODE_JMP_RELOCATE ? 4u + s->relocated_length : s->length;
    return s->offset <= image_size && end <= image_size - s->offset;
}

static IsaTable *load_fail(FILE *f, IsaTable *t, const char *reason, const char *path) {
    fprintf(stderr, "ISA: %s%s\n", reason, path ? path : "");
    if (f) fclose(f);
    if (t) {
        free(t->sites);
        free(t->hits);
        free(t);
    }
    return NULL;
}

IsaTable *isa_table_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return load_fail(NULL, NULL, "cannot open rewrite table ", path);
    unsigned char header[HEADER_BYTES];
    if (fread(header, 1, sizeof header, f) != sizeof header || memcmp(header, "G3ISA1\0\0", 8) != 0)
        return load_fail(f, NULL, "not a G3ISA1 table: ", path);
    uint32_t count = read_u32(header + 48);
    if (count > MAX_SITES) return load_fail(f, NULL, "table has too many sites", NULL);
    if (fseek(f, 0, SEEK_END) != 0 || ftell(f) != (long)(HEADER_BYTES + (uint64_t)RECORD_BYTES * count) ||
        fseek(f, HEADER_BYTES, SEEK_SET) != 0)
        return load_fail(f, NULL, "table size does not match its site count", NULL);
    IsaTable *t = calloc(1, sizeof *t);
    if (!t) return load_fail(f, NULL, "out of memory", NULL);
    t->sites = calloc(count ? count : 1, sizeof *t->sites);
    t->hits = calloc(count ? count : 1, sizeof *t->hits);
    if (!t->sites || !t->hits) return load_fail(f, t, "out of memory", NULL);
    if (fread(t->sites, sizeof *t->sites, count, f) != count) return load_fail(f, t, "cannot read table ", path);
    fclose(f);
    t->count = count;
    t->unsupported = read_u32(header + 52);
    t->image_size = read_u64(header + 8);
    memcpy(t->sha256, header + 16, sizeof t->sha256);
    for (uint32_t i = 0; i < count; i++) {
        if (i > 0 && t->sites[i].offset <= t->sites[i - 1].offset)
            return load_fail(NULL, t, "table records are not sorted", NULL);
        if (!record_valid(&t->sites[i], t->image_size)) {
            char reason[48];
            snprintf(reason, sizeof reason, "invalid table record %u", i);
            return load_fail(NULL, t, reason, NULL);
        }
    }
    return t;
}

int isa_table_matches(const IsaTable *t, const unsigned char *image, uint64_t size) {
    if (size != t->image_size) return 1;
    uint8_t digest[32];
    sha256(image, size, digest);
    return memcmp(digest, t->sha256, sizeof digest) != 0;
}

size_t isa_stub_area_size(const IsaTable *t) { return (size_t)t->count * ISA_STUB_BYTES; }

unsigned isa_site_set(const IsaSite *s) {
    switch (s->kind) {
    case ISA_MOVBE_LOAD: case ISA_MOVBE_STORE: return CPU_SET_MOVBE;
    case ISA_LZCNT: return CPU_SET_LZCNT;
    case ISA_TZCNT: case ISA_ANDN: case ISA_BEXTR: case ISA_BLSI: case ISA_BLSMSK: case ISA_BLSR: return CPU_SET_BMI1;
    case ISA_VCVTPH2PS: case ISA_VCVTPS2PH: return CPU_SET_F16C;
    default: return 0;
    }
}

/* ---- stub emission (spec 5.5) ---- */

static void emit_u8(unsigned char **p, uint8_t v) { *(*p)++ = v; }
static void emit_u32(unsigned char **p, uint32_t v) { for (int i = 0; i < 4; i++) emit_u8(p, (uint8_t)(v >> 8 * i)); }
static void emit_u64(unsigned char **p, uint64_t v) { for (int i = 0; i < 8; i++) emit_u8(p, (uint8_t)(v >> 8 * i)); }
static void emit_bytes(unsigned char **p, const uint8_t *bytes, size_t n) {
    memcpy(*p, bytes, n);
    *p += n;
}

/* mov [rsp+disp8], reg (opcode 89) or mov reg, [rsp+disp8] (opcode 8B): REX.W(.R) modrm(01 reg 100) SIB 24. */
static void emit_rsp_mov(unsigned char **p, uint8_t opcode, int reg, uint8_t disp) {
    emit_u8(p, reg >= 8 ? 0x4c : 0x48);
    emit_u8(p, opcode);
    emit_u8(p, (uint8_t)(0x44 | (reg & 7) << 3));
    emit_u8(p, 0x24);
    emit_u8(p, disp);
}
static void emit_mov_store(unsigned char **p, int reg, uint8_t disp) { emit_rsp_mov(p, 0x89, reg, disp); }
static void emit_mov_load(unsigned char **p, int reg, uint8_t disp) { emit_rsp_mov(p, 0x8b, reg, disp); }

/* rel32 from the byte after a jmp ending at `next` to `target`; 0 when it does not fit. */
static int rel32_to(const unsigned char *next, const unsigned char *target, int32_t *rel) {
    int64_t delta = (int64_t)((intptr_t)target - (intptr_t)next);
    if (delta < INT32_MIN || delta > INT32_MAX) return 0;
    *rel = (int32_t)delta;
    return 1;
}

/* Writes the stub for `s` into `code` (laid out to run at `at`). Returns its size, or 0 when the
 * jmp back is out of rel32 range. */
static size_t emit_stub(unsigned char *code, const unsigned char *at, const IsaSite *s, uint64_t *hit,
                        unsigned char *image) {
    static const uint8_t lea_rsp_down[] = {0x48, 0x8d, 0xa4, 0x24, 0x00, 0xff, 0xff, 0xff};   /* lea rsp,[rsp-0x100] */
    static const uint8_t lea_rax_rsp_up[] = {0x48, 0x8d, 0x84, 0x24, 0x00, 0x01, 0x00, 0x00}; /* lea rax,[rsp+0x100] */
    static const uint8_t lea_rsp_up[] = {0x48, 0x8d, 0xa4, 0x24, 0x00, 0x01, 0x00, 0x00};     /* lea rsp,[rsp+0x100] */
    unsigned char *p = code;
    emit_bytes(&p, lea_rsp_down, sizeof lea_rsp_down);
    for (int reg = 0; reg < 16; reg++)
        if (reg != 4) emit_mov_store(&p, reg, (uint8_t)(8 * reg));
    emit_bytes(&p, lea_rax_rsp_up, sizeof lea_rax_rsp_up);
    emit_mov_store(&p, 0, 8 * 4);                                   /* gpr[4] = guest rsp */
    emit_u8(&p, 0x9c);                                              /* pushfq: rsp = IsaRegs* */
    emit_bytes(&p, (const uint8_t[]){0x48, 0xb8}, 2);               /* mov rax, &hits[i] */
    emit_u64(&p, (uint64_t)(uintptr_t)hit);
    emit_bytes(&p, (const uint8_t[]){0x48, 0xff, 0x00}, 3);         /* inc qword [rax] */
    emit_bytes(&p, (const uint8_t[]){0x48, 0x89, 0xe5}, 3);         /* mov rbp, rsp */
    emit_bytes(&p, (const uint8_t[]){0x48, 0x83, 0xe4, 0xf0}, 4);   /* and rsp, -16 */
    emit_bytes(&p, (const uint8_t[]){0x48, 0xbf}, 2);               /* mov rdi, site */
    emit_u64(&p, (uint64_t)(uintptr_t)s);
    emit_bytes(&p, (const uint8_t[]){0x48, 0x89, 0xee}, 3);         /* mov rsi, rbp */
    emit_bytes(&p, (const uint8_t[]){0x48, 0xb8}, 2);               /* mov rax, isa_emulate */
    emit_u64(&p, (uint64_t)(uintptr_t)isa_emulate);
    emit_bytes(&p, (const uint8_t[]){0xff, 0xd0}, 2);               /* call rax */
    emit_bytes(&p, (const uint8_t[]){0x48, 0x89, 0xec}, 3);         /* mov rsp, rbp */
    emit_u8(&p, 0x9d);                                              /* popfq: rsp = &gpr[0] */
    for (int reg = 0; reg < 16; reg++)
        if (reg != 4) emit_mov_load(&p, reg, (uint8_t)(8 * reg));
    emit_bytes(&p, lea_rsp_up, sizeof lea_rsp_up);
    const unsigned char *back = image + s->offset + s->length;
    if (s->mode == ISA_MODE_JMP_RELOCATE) {
        emit_bytes(&p, s->relocated, s->relocated_length);
        back = image + s->offset + 4 + s->relocated_length;
    }
    int32_t rel;
    if (!rel32_to(at + (p - code) + JMP_BYTES, back, &rel)) return 0;
    emit_u8(&p, 0xe9);
    emit_u32(&p, (uint32_t)rel);
    return (size_t)(p - code);
}

/* Site bytes of spec 4.3. */
static void patch_site(unsigned char *site, const IsaSite *s, int32_t rel) {
    if (s->mode == ISA_MODE_INT3) {
        site[0] = 0xcc;
        return;
    }
    unsigned char *p = site;
    emit_u8(&p, 0xe9);
    emit_u32(&p, (uint32_t)rel);
    size_t covered = s->mode == ISA_MODE_JMP_RELOCATE ? 4u + s->relocated_length : s->length;
    memset(p, 0x90, covered - JMP_BYTES);
}

IsaReport isa_rewrite_apply(IsaTable *t, unsigned char *image, unsigned char *stubs, unsigned sets) {
    IsaReport report = {0};
    t->image = image;
    t->stubs = stubs;
    isa_emulate_set_image(image);
    for (uint32_t i = 0; i < t->count; i++)
        if (t->sites[i].mode == ISA_MODE_UNSUPPORTED && (isa_site_set(&t->sites[i]) & sets)) report.unsupported_needed++;
    if (report.unsupported_needed) return report;
    for (uint32_t i = 0; i < t->count; i++) {
        const IsaSite *s = &t->sites[i];
        if (s->mode == ISA_MODE_UNSUPPORTED || !(isa_site_set(s) & sets)) continue;
        unsigned char *site = image + s->offset;
        if (memcmp(site, s->original, s->length) != 0) {
            fprintf(stderr, "ISA: site 0x%llx %s skipped: bytes changed\n", (unsigned long long)s->offset,
                    isa_kind_name(s->kind));
            report.skipped++;
            continue;
        }
        if (s->mode == ISA_MODE_JMP_RELOCATE && memcmp(site + 4, s->relocated, s->relocated_length) != 0) {
            /* A patch or hook changed the instruction the jmp would cover: the stub would run the old copy. */
            fprintf(stderr, "ISA: site 0x%llx %s: next instruction changed, int3 fallback\n",
                    (unsigned long long)s->offset, isa_kind_name(s->kind));
            t->sites[i].mode = ISA_MODE_INT3;
        }
        if (s->mode == ISA_MODE_INT3) {
            patch_site(site, s, 0);
            report.int3++;
            report.rewritten++;
            continue;
        }
        unsigned char *stub = stubs + (size_t)i * ISA_STUB_BYTES;
        unsigned char code[2 * ISA_STUB_BYTES];
        int32_t rel;
        size_t size;
        if (!rel32_to(site + JMP_BYTES, stub, &rel) || (size = emit_stub(code, stub, s, &t->hits[i], image)) == 0) {
            fprintf(stderr, "ISA: site 0x%llx out of rel32 range\n", (unsigned long long)s->offset);
            report.skipped++;
            continue;
        }
        if (size > ISA_STUB_BYTES) {
            fprintf(stderr, "ISA: stub for site 0x%llx exceeds 256 bytes, site skipped\n",
                    (unsigned long long)s->offset);
            report.skipped++;
            continue;
        }
        memcpy(stub, code, size);
        patch_site(site, s, rel);
        if (s->mode == ISA_MODE_JMP) report.jmp++;
        else report.relocated++;
        report.rewritten++;
    }
    return report;
}

/* ---- int3 trap, stub lookup, report ---- */

int isa_rewrite_trap(IsaTable *t, uintptr_t address, IsaRegs *regs, uintptr_t *next_rip) {
    if (!t || !t->image || address < (uintptr_t)t->image) return 0;
    uint64_t offset = address - (uintptr_t)t->image;
    uint32_t lo = 0, hi = t->count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        if (t->sites[mid].offset < offset) lo = mid + 1;
        else hi = mid;
    }
    if (lo == t->count || t->sites[lo].offset != offset) return 0;
    if (t->sites[lo].mode != ISA_MODE_INT3 || *(const unsigned char *)address != 0xcc) return 0;
    isa_emulate(&t->sites[lo], regs);
    t->hits[lo]++;
    *next_rip = address + t->sites[lo].length;
    return 1;
}

int isa_rewrite_site_of_stub(const IsaTable *t, uintptr_t rip) {
    if (!t || !t->stubs || rip < (uintptr_t)t->stubs) return -1;
    uintptr_t at = rip - (uintptr_t)t->stubs;
    return at < isa_stub_area_size(t) ? (int)(at / ISA_STUB_BYTES) : -1;
}

const char *isa_kind_name(uint8_t kind) {
    static const char *const names[] = {"?",     "movbe", "movbe",  "lzcnt", "tzcnt",     "andn",
                                        "bextr", "blsi",  "blsmsk", "blsr",  "vcvtph2ps", "vcvtps2ph"};
    return kind < sizeof names / sizeof names[0] ? names[kind] : "?";
}

/* Report order: more hits first, then lower offset. */
static int ranks_before(const IsaTable *t, uint32_t a, uint32_t b) {
    return t->hits[a] > t->hits[b] || (t->hits[a] == t->hits[b] && t->sites[a].offset < t->sites[b].offset);
}

void isa_rewrite_report(const IsaTable *t, FILE *out) {
    fprintf(out, "ISA: most executed sites:\n");
    int64_t prev = -1;   /* last printed site; each pass takes the best one ranked after it */
    for (int line = 0; line < 10; line++) {
        int64_t best = -1;
        for (uint32_t i = 0; i < t->count; i++) {
            if (t->hits[i] == 0 || (prev >= 0 && !ranks_before(t, (uint32_t)prev, i))) continue;
            if (best < 0 || ranks_before(t, i, (uint32_t)best)) best = i;
        }
        if (best < 0) return;
        fprintf(out, "  0x%llx %s %llu\n", (unsigned long long)t->sites[best].offset,
                isa_kind_name(t->sites[best].kind), (unsigned long long)t->hits[best]);
        prev = best;
    }
}
