/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SPARTAN_PLAYER_HOOK_H
#define SPARTAN_PLAYER_HOOK_H
#include <stdint.h>
#include <string.h>
#include "signature.h"

/* Kratos' object. The movement setup of a v01.02 build (found by signature) runs for every mover
 * with r14 the object, position at +0xa0; at the title screen Kratos is the only mover, so the first
 * object it sees is his (the same address in every 2026-10-31 log, through loads). The site is
 * lea rcx, [rip+disp] (the engine rate): the stub stores r14 once, then replays the lea. */
enum { SPARTAN_PLAYER_HOOK_LENGTH = 7, SPARTAN_PLAYER_STUB_SIZE = 64 };
static const SpartanSignature spartanport_player_site = {
    "c5 fa 11 83 50 01 00 00 48 8d 0d ?? ?? ?? ?? c5 ea 59 01", 8};
static inline int spartanport_player_match(const unsigned char *code, uint64_t size) {
    return size >= SPARTAN_PLAYER_HOOK_LENGTH && code[0] == 0x48 && code[1] == 0x8d && code[2] == 0x0d;
}
/* rate: the lea's target; resume: the instruction after it. Flags, rax and the red zone are kept. */
static inline unsigned spartanport_player_stub(unsigned char *out, uintptr_t slot, uintptr_t rate, uintptr_t resume) {
    if (!slot || !rate || !resume) return 0;
    static const unsigned char code[] = {
        0x48,0x8d,0xa4,0x24,0x80,0xff,0xff,0xff, /* lea rsp, [rsp-128] */
        0x9c,0x50,                               /* pushfq; push rax */
        0x48,0xb8,0,0,0,0,0,0,0,0,              /* movabs rax, slot */
        0x48,0x83,0x38,0x00,                     /* cmp qword [rax], 0 */
        0x75,0x03,                               /* jne: already stored */
        0x4c,0x89,0x30,                          /* mov [rax], r14 */
        0x58,0x9d,                               /* pop rax; popfq */
        0x48,0x8d,0xa4,0x24,0x80,0,0,0,         /* lea rsp, [rsp+128] */
        0x48,0xb9,0,0,0,0,0,0,0,0,              /* movabs rcx, rate */
        0xff,0x25,0,0,0,0,                       /* jmp [rip] */
        0,0,0,0,0,0,0,0,                         /* resume */
    };
    memcpy(out, code, sizeof(code));
    memcpy(out + 12, &slot, 8);
    memcpy(out + 41, &rate, 8);
    memcpy(out + 55, &resume, 8);
    return sizeof(code);
}
#endif
