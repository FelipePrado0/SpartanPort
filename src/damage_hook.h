/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SPARTAN_DAMAGE_HOOK_H
#define SPARTAN_DAMAGE_HOOK_H
#include <stdint.h>
#include <string.h>
#include "signature.h"

/* CUSA01623 v01.02, image offset 0x50181, the damage routine's entry (0x50170): the damage
 * arrives in xmm0 and rdi is the actor (+0x298 is 0 for the player). The routine then checks the
 * actor's health thresholds against health - damage (0x50295) and fires the events of the ones the
 * hit crosses (stagger, finisher, phase changes), so the multiplier must apply before that, as the
 * game's own damage scaling does. Hooking later, at the health store, skipped those events and left
 * enemies stuck. rax and xmm1 are overwritten before they are read. */
enum { SPARTAN_DAMAGE_HOOK_OFFSET = 0x50181, SPARTAN_DAMAGE_HOOK_LENGTH = 8 };
static const unsigned char spartanport_damage_original[SPARTAN_DAMAGE_HOOK_LENGTH] = {
    0xc5,0xfa,0x11,0x45,0xa0,  /* vmovss [rbp-0x60], xmm0 */
    0x49,0x89,0xfd             /* mov r13, rdi */
};
static const SpartanSignature spartanport_damage_site = {"48 83 ec 48 c5 fa 11 45 a0 49 89 fd 48 8b 1d ?? ?? ?? ??", 4};

static inline int spartanport_damage_signature_match(const unsigned char *code, uint64_t size) {
    return size >= SPARTAN_DAMAGE_HOOK_LENGTH && !memcmp(code, spartanport_damage_original, SPARTAN_DAMAGE_HOOK_LENGTH);
}

/* Damage above zero is multiplied by *taken (player) or *dealt (any other actor). */
static inline unsigned spartanport_damage_build_stub(unsigned char *out, uintptr_t taken, uintptr_t dealt,
                                              uintptr_t resume) {
    static const unsigned char body[] = {
        0x48,0xb8, 0,0,0,0,0,0,0,0,          /* movabs rax, taken */
        0x80,0xbf,0x98,0x02,0,0, 0x00,       /* cmp byte [rdi+0x298], 0 */
        0x74,0x0a,                           /* je +10 (player) */
        0x48,0xb8, 0,0,0,0,0,0,0,0,          /* movabs rax, dealt */
        0xc5,0xf0,0x57,0xc9,                 /* vxorps xmm1, xmm1, xmm1 */
        0xc5,0xf8,0x2e,0xc1,                 /* vucomiss xmm0, xmm1 */
        0x76,0x04,                           /* jbe +4 (no damage, or NaN) */
        0xc5,0xfa,0x59,0x00,                 /* vmulss xmm0, xmm0, [rax] */
    };
    static const unsigned char jump[] = {0xff,0x25,0,0,0,0}; /* jmp [rip+0] */
    unsigned n = 0;
    memcpy(out, body, sizeof(body));
    memcpy(out + 2, &taken, 8);
    memcpy(out + 21, &dealt, 8);
    n += sizeof(body);
    memcpy(out + n, spartanport_damage_original, SPARTAN_DAMAGE_HOOK_LENGTH);
    n += SPARTAN_DAMAGE_HOOK_LENGTH;
    memcpy(out + n, jump, sizeof(jump));
    n += sizeof(jump);
    memcpy(out + n, &resume, 8);
    return n + 8;
}
#endif
