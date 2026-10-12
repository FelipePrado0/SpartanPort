/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef GOW3_CHEAT_HOOK_H
#define GOW3_CHEAT_HOOK_H
#include <stdint.h>
#include <string.h>

enum { GOW3_CHEAT_HEALTH, GOW3_CHEAT_MAGIC, GOW3_CHEAT_ITEM, GOW3_CHEAT_RAGE,
       GOW3_CHEAT_ORBS, GOW3_CHEAT_VERY_HARD, GOW3_CHEAT_DOUBLE_JUMPS, GOW3_CHEAT_COUNT };
typedef struct {
    uint32_t offset;
    unsigned id, length, replacement_length;
    unsigned char original[9], replacement[40];
} Gow3CheatHook;
/* Celogamez's CUSA01623 v01.02 cheats, adapted to the local executable's offsets.
 * https://github.com/GoldHEN/GoldHEN_Cheat_Repository/blob/main/json/CUSA01623_01.02.json */
static const Gow3CheatHook gow3_cheat_hooks[] = {
    {0x5040e,GOW3_CHEAT_HEALTH,9,30,
     {0xc4,0xc1,0x7a,0x11,0x85,0xcc,0x01,0,0},
     {0x9c,0x41,0x80,0xbd,0x98,0x02,0,0,0,0x75,9,
      0xc4,0xc1,0x7a,0x10,0x85,0xd0,0x01,0,0,0x9d,
      0xc4,0xc1,0x7a,0x11,0x85,0xcc,0x01,0,0}},
    {0x72cdc,GOW3_CHEAT_MAGIC,8,16,
     {0xc5,0xfa,0x11,0x83,0xd0,0x01,0,0},
     {0xc5,0xfa,0x10,0x83,0xd0,0x01,0,0,0xc5,0xfa,0x11,0x83,0xd0,0x01,0,0}},
    {0x72d97,GOW3_CHEAT_ITEM,8,16,
     {0xc5,0xfa,0x11,0x87,0xd4,0x01,0,0},
     {0xc5,0xfa,0x10,0x87,0xd4,0x01,0,0,0xc5,0xfa,0x11,0x87,0xd4,0x01,0,0}},
    {0x6a9b8,GOW3_CHEAT_RAGE,8,10,
     {0xc5,0xfa,0x11,0x8b,0xd8,0x01,0,0},
     {0xc7,0x83,0xd8,0x01,0,0,0x28,0x6b,0x6e,0x4e}},
    {0x6a6a0,GOW3_CHEAT_RAGE,8,10,
     {0xc5,0xfa,0x11,0x8b,0xd8,0x01,0,0},
     {0xc7,0x83,0xd8,0x01,0,0,0x28,0x6b,0x6e,0x4e}},
    {0x6a9fc,GOW3_CHEAT_ORBS,8,10,
     {0xc5,0xfa,0x11,0x87,0xe8,0x01,0,0},
     {0xc7,0x87,0xe8,0x01,0,0,0xf0,0x23,0x74,0x49}},
    /* Override only the queried completion level, leaving stored progress untouched. */
    {0xc77ea,GOW3_CHEAT_VERY_HARD,8,8,
     {0xb8,3,0,0,0,0xf6,0xc1,8},
     {0xb8,3,0,0,0,0x85,0xc0,0x90}},
    /* Ignore the used-air-jump flag for Kratos, preserving every other movement check. */
    {0x95d51,GOW3_CHEAT_DOUBLE_JUMPS,9,27,
     {0x41,0xf6,0x84,0x24,0xac,4,0,0,4},
     {0x41,0x80,0xbc,0x24,0x98,2,0,0,0,0x75,7,
      0xa9,0,0,0,0,0xeb,9,
      0x41,0xf6,0x84,0x24,0xac,4,0,0,4}}
};
#define GOW3_CHEAT_HOOK_COUNT (sizeof(gow3_cheat_hooks)/sizeof(gow3_cheat_hooks[0]))
static inline int gow3_cheat_signature_match(const Gow3CheatHook *hook,
                                            const unsigned char *code, uint64_t size) {
    return size >= hook->length && !memcmp(code, hook->original, hook->length);
}

/* Some dumps of CUSA01623 v01.02 are a different byte-exact build whose code sits at a shifted
 * offset: the reference build's addresses do not match, so every hook would fail with a
 * "code signature mismatch". Such a build shifts the whole code region by one constant delta, so
 * we detect that delta once from an anchor hook whose signature occurs EXACTLY ONCE in the image
 * (so the match is unambiguous), then relocate every other hook by the same delta and still
 * verify each one's own signature at the relocated offset before trusting it.
 *
 * Returns 1 and sets *delta when a single consistent shift is found (0 for the reference build),
 * or 0 when no anchor matches uniquely (unknown dump: hooks stay disabled, as before). */
static inline int gow3_cheat_find_signature_unique(const unsigned char *image, uint64_t size,
                                                   const unsigned char *sig, unsigned length,
                                                   uint64_t *at) {
    uint64_t found = UINT64_MAX;
    for (uint64_t i = 0; i + length <= size; ++i) {
        if (!memcmp(image + i, sig, length)) {
            if (found != UINT64_MAX) return 0;  /* ambiguous */
            found = i;
        }
    }
    if (found == UINT64_MAX) return 0;
    *at = found;
    return 1;
}

/* Anchors with a single occurrence in the image: Very Hard (0xc77ea) and the double-jump check
 * (0x95d51). Named statics (not compound literals) so the header is valid in C and C++ alike. */
static const unsigned char gow3_cheat_anchor_very_hard[8] = {0xb8,3,0,0,0,0xf6,0xc1,8};
static const unsigned char gow3_cheat_anchor_double_jump[9] = {0x41,0xf6,0x84,0x24,0xac,4,0,0,4};

static inline int gow3_cheat_build_delta(const unsigned char *image, uint64_t size, int64_t *delta) {
    const struct { uint32_t offset; unsigned length; const unsigned char *sig; }
    anchors[] = {
        {0xc77ea, 8, gow3_cheat_anchor_very_hard},
        {0x95d51, 9, gow3_cheat_anchor_double_jump},
    };
    int64_t agreed = 0;
    int have = 0;
    for (unsigned a = 0; a < sizeof(anchors) / sizeof(anchors[0]); ++a) {
        uint64_t at;
        if (!gow3_cheat_find_signature_unique(image, size, anchors[a].sig, anchors[a].length, &at))
            continue;
        int64_t d = (int64_t)at - (int64_t)anchors[a].offset;
        if (have && d != agreed) return 0;  /* anchors disagree: not a simple uniform shift */
        agreed = d;
        have = 1;
    }
    if (!have) return 0;
    *delta = agreed;
    return 1;
}

/* The hook's offset relocated by the build delta, verified by its own signature there.
 * Returns the resolved image offset, or UINT64_MAX when it cannot be placed safely. */
static inline uint64_t gow3_cheat_resolve_offset(const Gow3CheatHook *hook, int64_t delta,
                                                 const unsigned char *image, uint64_t size) {
    int64_t at = (int64_t)hook->offset + delta;
    if (at < 0 || (uint64_t)at > size || hook->length > size - (uint64_t)at) return UINT64_MAX;
    if (!gow3_cheat_signature_match(hook, image + at, size - (uint64_t)at)) return UINT64_MAX;
    return (uint64_t)at;
}
static inline unsigned gow3_cheat_build_stub(unsigned char *out, const Gow3CheatHook *hook,
                                            uintptr_t flag, uintptr_t resume) {
    /* Keep the SysV red zone, GPRs and condition flags intact; no host call on the hot path. */
    const unsigned char prefix[] = {
        0x48,0x8d,0x64,0x24,0x80,0x9c,0x50,0x48,0xb8,0,0,0,0,0,0,0,0,
        0x80,0x38,0,0x74,0
    };
    const unsigned char restore[] = {0x58,0x9d};
    const unsigned char stack[] = {0x48,0x8d,0xa4,0x24,0x80,0,0,0};
    const unsigned char jump[] = {0xff,0x25,0,0,0,0};
    unsigned n = sizeof(prefix);
    memcpy(out, prefix, n);
    memcpy(out + 9, &flag, 8);
    out[21] = (unsigned char)(sizeof(restore) + hook->replacement_length + sizeof(stack) + 14);
    memcpy(out+n,restore,sizeof(restore)); n += sizeof(restore);
    memcpy(out+n,hook->replacement,hook->replacement_length); n += hook->replacement_length;
    memcpy(out+n,stack,sizeof(stack)); n += sizeof(stack);
    memcpy(out+n,jump,sizeof(jump)); n += sizeof(jump);
    memcpy(out+n,&resume,8); n += 8;
    memcpy(out+n,restore,sizeof(restore)); n += sizeof(restore);
    memcpy(out+n,hook->original,hook->length); n += hook->length;
    memcpy(out+n,stack,sizeof(stack)); n += sizeof(stack);
    memcpy(out+n,jump,sizeof(jump)); n += sizeof(jump);
    memcpy(out+n,&resume,8); n += 8;
    return n;
}
#endif
