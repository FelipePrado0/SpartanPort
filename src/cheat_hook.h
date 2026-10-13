/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef GOW3_CHEAT_HOOK_H
#define GOW3_CHEAT_HOOK_H
#include <stdint.h>
#include <string.h>
#include "signature.h"

enum { GOW3_CHEAT_HEALTH, GOW3_CHEAT_MAGIC, GOW3_CHEAT_ITEM, GOW3_CHEAT_RAGE,
       GOW3_CHEAT_ORBS, GOW3_CHEAT_VERY_HARD, GOW3_CHEAT_DOUBLE_JUMPS, GOW3_CHEAT_COUNT };
typedef struct {
    uint32_t offset;
    unsigned id, length, replacement_length;
    unsigned char original[9], replacement[40];
    Gow3Signature site;  /* finds the hook in any build; offset is the reference build's */
} Gow3CheatHook;
/* Celogamez's CUSA01623 v01.02 cheats, adapted to the local executable's offsets.
 * https://github.com/GoldHEN/GoldHEN_Cheat_Repository/blob/main/json/CUSA01623_01.02.json */
static const Gow3CheatHook gow3_cheat_hooks[] = {
    {0x5040e,GOW3_CHEAT_HEALTH,9,30,
     {0xc4,0xc1,0x7a,0x11,0x85,0xcc,0x01,0,0},
     {0x9c,0x41,0x80,0xbd,0x98,0x02,0,0,0,0x75,9,
      0xc4,0xc1,0x7a,0x10,0x85,0xd0,0x01,0,0,0x9d,
      0xc4,0xc1,0x7a,0x11,0x85,0xcc,0x01,0,0},
     {"c4 c1 7a 11 85 cc 01 00 00 49 8b 45 00 4c 89 ef",0}},
    {0x72cdc,GOW3_CHEAT_MAGIC,8,16,
     {0xc5,0xfa,0x11,0x83,0xd0,0x01,0,0},
     {0xc5,0xfa,0x10,0x83,0xd0,0x01,0,0,0xc5,0xfa,0x11,0x83,0xd0,0x01,0,0},
     {"c5 fa 5f c2 c5 fa 5d c1 c5 fa 11 83 d0 01 00 00",8}},
    {0x72d97,GOW3_CHEAT_ITEM,8,16,
     {0xc5,0xfa,0x11,0x87,0xd4,0x01,0,0},
     {0xc5,0xfa,0x10,0x87,0xd4,0x01,0,0,0xc5,0xfa,0x11,0x87,0xd4,0x01,0,0},
     {"c5 fa 5f c2 c5 fa 5d c1 c5 fa 11 87 d4 01 00 00 c3",8}},
    {0x6a9b8,GOW3_CHEAT_RAGE,8,10,
     {0xc5,0xfa,0x11,0x8b,0xd8,0x01,0,0},
     {0xc7,0x83,0xd8,0x01,0,0,0x28,0x6b,0x6e,0x4e},
     {"80 8b 68 01 00 00 80 c5 fa 11 8b d8 01 00 00 c5 f8 2e e0 73 20",7}},
    {0x6a6a0,GOW3_CHEAT_RAGE,8,10,
     {0xc5,0xfa,0x11,0x8b,0xd8,0x01,0,0},
     {0xc7,0x83,0xd8,0x01,0,0,0x28,0x6b,0x6e,0x4e},
     {"c5 fa 11 8b d8 01 00 00 c5 fa 10 4d f4 c5 f8 2e c8",0}},
    {0x6a9fc,GOW3_CHEAT_ORBS,8,10,
     {0xc5,0xfa,0x11,0x87,0xe8,0x01,0,0},
     {0xc7,0x87,0xe8,0x01,0,0,0xf0,0x23,0x74,0x49},
     {"c5 fa 11 87 e8 01 00 00 48 8b 87 b0 01 00 00 a9 00 00 04 00",0}},
    /* Override only the queried completion level, leaving stored progress untouched. */
    {0xc77ea,GOW3_CHEAT_VERY_HARD,8,8,
     {0xb8,3,0,0,0,0xf6,0xc1,8},
     {0xb8,3,0,0,0,0x85,0xc0,0x90},
     {"48 8b 08 b8 03 00 00 00 f6 c1 08 75 1b b8 02 00 00 00",3}},
    /* Ignore the used-air-jump flag for Kratos, preserving every other movement check. */
    {0x95d51,GOW3_CHEAT_DOUBLE_JUMPS,9,27,
     {0x41,0xf6,0x84,0x24,0xac,4,0,0,4},
     {0x41,0x80,0xbc,0x24,0x98,2,0,0,0,0x75,7,
      0xa9,0,0,0,0,0xeb,9,
      0x41,0xf6,0x84,0x24,0xac,4,0,0,4},
     {"0f 84 ?? ?? ?? ?? 41 f6 84 24 ac 04 00 00 04 0f 85 ?? ?? ?? ??",6}}
};
#define GOW3_CHEAT_HOOK_COUNT (sizeof(gow3_cheat_hooks)/sizeof(gow3_cheat_hooks[0]))
static inline int gow3_cheat_signature_match(const Gow3CheatHook *hook,
                                            const unsigned char *code, uint64_t size) {
    return size >= hook->length && !memcmp(code, hook->original, hook->length);
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
