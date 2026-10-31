/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SPARTAN_MOTION_BLUR_HOOK_H
#define SPARTAN_MOTION_BLUR_HOOK_H
#include <stdint.h>
#include <string.h>
#include "signature.h"

/* The game's motion blur ("MotionBlur", the MotionBlurFX pass): its update (0x16ae80 in the 6e72
 * build) queues the pass for the frame only when the blur's strength (xmm1) is above zero:
 *     vxorps xmm0, xmm0, xmm0 ; vucomiss xmm1, xmm0 ; jbe <return>
 * Both camera and object blur (Kratos, enemies) come from that one pass. With the menu's Motion blur
 * off the stub takes the jbe's way, as for a strength of zero; else it replays the compare and
 * returns to the jbe. rax is overwritten before it is read on both ways. */
enum { SPARTAN_MOTION_BLUR_HOOK_LENGTH = 8, SPARTAN_MOTION_BLUR_BRANCH_LENGTH = 6 };
static const unsigned char spartanport_motion_blur_original[SPARTAN_MOTION_BLUR_HOOK_LENGTH] = {
    0xc5,0xf8,0x57,0xc0,  /* vxorps xmm0, xmm0, xmm0 */
    0xc5,0xf8,0x2e,0xc8   /* vucomiss xmm1, xmm0 */
};
static const SpartanSignature spartanport_motion_blur_site = {
    "84 c9 0f 85 ?? ?? ?? ?? c5 f8 57 c0 c5 f8 2e c8 0f 86 ?? ?? ?? ?? c5 fa 11 4d e4", 8};

/* The site's bytes and the jbe after them (0f 86 rel32). */
static inline int spartanport_motion_blur_signature_match(const unsigned char *code, uint64_t size) {
    return size >= SPARTAN_MOTION_BLUR_HOOK_LENGTH + SPARTAN_MOTION_BLUR_BRANCH_LENGTH &&
           !memcmp(code, spartanport_motion_blur_original, SPARTAN_MOTION_BLUR_HOOK_LENGTH) &&
           code[SPARTAN_MOTION_BLUR_HOOK_LENGTH] == 0x0f && code[SPARTAN_MOTION_BLUR_HOOK_LENGTH + 1] == 0x86;
}

/* The jbe's target (the update's return), an image offset. */
static inline uint64_t spartanport_motion_blur_skip(const unsigned char *code, uint64_t site) {
    int32_t displacement;
    memcpy(&displacement, code + SPARTAN_MOTION_BLUR_HOOK_LENGTH + 2, 4);
    return site + SPARTAN_MOTION_BLUR_HOOK_LENGTH + SPARTAN_MOTION_BLUR_BRANCH_LENGTH + (int64_t)displacement;
}

/* enabled: a byte, nonzero while the blur is on (the menu's setting, read every frame). resume: the
 * jbe; skip: its target. */
enum { SPARTAN_MOTION_BLUR_ENABLED_IMMEDIATE = 6 };
static inline unsigned spartanport_motion_blur_build_stub(unsigned char *out, uintptr_t enabled, uintptr_t resume,
                                                          uintptr_t skip) {
    static const unsigned char body[] = {
        0xc5,0xf8,0x57,0xc0,                /* vxorps xmm0, xmm0, xmm0 */
        0x48,0xb8, 0,0,0,0,0,0,0,0,         /* movabs rax, enabled */
        0x80,0x38,0x00,                     /* cmp byte [rax], 0 */
        0x74,0x12,                          /* je +18 (off: the skip below) */
        0xc5,0xf8,0x2e,0xc8,                /* vucomiss xmm1, xmm0 */
        0xff,0x25,0,0,0,0, 0,0,0,0,0,0,0,0, /* jmp [rip+0] resume */
        0xff,0x25,0,0,0,0, 0,0,0,0,0,0,0,0, /* jmp [rip+0] skip */
    };
    memcpy(out, body, sizeof(body));
    memcpy(out + SPARTAN_MOTION_BLUR_ENABLED_IMMEDIATE, &enabled, 8);
    memcpy(out + 29, &resume, 8);
    memcpy(out + 43, &skip, 8);
    return sizeof(body);
}
#endif
