/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SPARTAN_GRAPHICS_HOOK_H
#define SPARTAN_GRAPHICS_HOOK_H
#include <stdint.h>
#include <string.h>
#include "signature.h"

/* Live graphics toggles (Options > Graphics, scripts/pause_menu.py). Each stub reads a byte the
 * settings own (1: effect on, the game's own behaviour; 0: off), so a change applies next frame. */
/* Row ids (scripts/pause_menu.py GRAPHICS_ROWS order). Texture Quality is done in the port's
 * sampler cache (texture_cache.cpp), so it has no hook here. */
enum { SPARTAN_GRAPHICS_TEXTURE_QUALITY = 0, SPARTAN_GRAPHICS_SHADOWS = 1, SPARTAN_GRAPHICS_MOTION_BLUR = 2,
       SPARTAN_GRAPHICS_BLOOM = 3, SPARTAN_GRAPHICS_COUNT = 4 };

/* Texture Quality skips the upgrade (power-up) menu: its red orb medallion smears at Low and
 * Very Low. The script "set global" switch (image 0x41f109 in CUSA01623, EU CUSA01715 file
 * 0x216d9) has one `lea rax, [rip+flag]; mov [rax], r15d; jmp` case per menu flag: inShell,
 * inPauseMenu, inPowerUpMenu (script globals 0x111..0x113, plain dwords). The menu scripts set
 * inPowerUpMenu on open and clear it on close; it is the global behind the third lea. No hook:
 * the sampler cache only reads it. */
enum { SPARTAN_POWER_UP_MENU_LEA = 30 };
static const SpartanSignature spartanport_power_up_menu_site = {
    "48 8d 05 ?? ?? ?? ?? 44 89 38 e9 ?? ?? ?? ?? 48 8d 05 ?? ?? ?? ?? 44 89 38 e9 ?? ?? ?? ?? "
    "48 8d 05 ?? ?? ?? ?? 44 89 38 e9 ?? ?? ?? ?? 41 83 fd 0f 0f 8f", 0};

/* Shadows: the per-frame shadow job builder (image 0x1b01e0, "s_shadowJobList"), called by the
 * frame renderer with the list of shadow casters in rsi. Off: the stub clears rsi first, so the
 * builder takes its own "no casters" path and no shadow map is drawn. Same prologue as motion
 * blur's, replayed by the stub. */
static const SpartanSignature spartanport_shadows_site = {
    "55 48 89 e5 41 57 41 56 41 55 41 54 53 48 81 ec b8 00 00 00 49 89 f6 48 8d 05 ?? ?? ?? ?? "
    "48 63 00 48 8d 0d ?? ?? ?? ?? 4c 8b 3c c1 31 db 31 f6 ba 04 00 00 00", 0};
enum { SPARTAN_SHADOWS_HOOK_LENGTH = 8 };

/* Motion blur: MotionBlurFX's per-frame execute (image 0x16ae80 in CUSA01623 v01.02), called by the
 * FX manager's walker. It clears and then sets the two globals behind its two lea's: the first
 * gates the VelocityPass and MotionVectorResolve passes, the second marks the blur as queued.
 * Off: both are cleared and the function returns, so none of the three passes is queued (the
 * same state as a camera cut). The hook replaces the 8-byte prologue the stub replays. */
enum { SPARTAN_MOTION_BLUR_HOOK_LENGTH = 8, SPARTAN_MOTION_BLUR_GATE = 13, SPARTAN_MOTION_BLUR_QUEUED = 27 };
static const SpartanSignature spartanport_motion_blur_site = {
    "55 48 89 e5 41 57 41 56 53 50 48 89 fb 4c 8d 3d ?? ?? ?? ?? 41 c7 07 00 00 00 00 "
    "4c 8d 35 ?? ?? ?? ?? 41 c7 06 00 00 00 00 48 8d b3 1c 02 00 00", 0};
static const unsigned char spartanport_motion_blur_original[SPARTAN_MOTION_BLUR_HOOK_LENGTH] = {
    0x55, 0x48, 0x89, 0xe5, 0x41, 0x57, 0x41, 0x56  /* push rbp; mov rbp, rsp; push r15; push r14 */
};

/* Bloom: fxBloom's render function (vtable +0x18, image 0x161e40) tests its per-frame "active"
 * byte with `cmp byte [rbx+0x469], 0; mov rdx, rbx; je skip` (image 0x161efb). The skip path is
 * the engine's own: it clears the "bloom ran" flag and returns, so no bright-pass, blur or
 * composite pass is drawn (real GPU time saved, and the engine fades bloom back in when it
 * returns). Off: the stub leaves ZF set so the game's je takes that path. The hook replaces the
 * 7-byte cmp, which the stub replays when bloom is on. rax is free there (overwritten before use). */
enum { SPARTAN_BLOOM_HOOK_LENGTH = 7 };
static const SpartanSignature spartanport_bloom_site = {
    "80 bb 69 04 00 00 00 48 89 da 0f 84 ?? ?? ?? ?? 4c 89 ad c0 fb ff ff", 0};
static const unsigned char spartanport_bloom_original[SPARTAN_BLOOM_HOOK_LENGTH] = {
    0x80, 0xbb, 0x69, 0x04, 0x00, 0x00, 0x00  /* cmp byte ptr [rbx + 0x469], 0 */
};

/* The image offset a `lea reg, [rip+disp32]` at `at` (7 bytes) points to. */
static inline uint64_t spartanport_rip_target(const unsigned char *image, uint64_t at) {
    int32_t disp;
    memcpy(&disp, image + at + 3, 4);
    return (uint64_t)((int64_t)at + 7 + disp);
}

static inline int spartanport_motion_blur_signature_match(const unsigned char *code, uint64_t size) {
    return size >= SPARTAN_MOTION_BLUR_HOOK_LENGTH &&
           !memcmp(code, spartanport_motion_blur_original, SPARTAN_MOTION_BLUR_HOOK_LENGTH);
}
static inline int spartanport_bloom_signature_match(const unsigned char *code, uint64_t size) {
    return size >= SPARTAN_BLOOM_HOOK_LENGTH &&
           !memcmp(code, spartanport_bloom_original, SPARTAN_BLOOM_HOOK_LENGTH);
}

static inline unsigned spartanport_put64(unsigned char *out, unsigned n, uintptr_t value) {
    memcpy(out + n, &value, 8);
    return n + 8;
}

/* Returns the stub's size. */
static inline unsigned spartanport_motion_blur_build_stub(unsigned char *out, uintptr_t enabled, uintptr_t gate,
                                                         uintptr_t queued, uintptr_t resume) {
    unsigned n = 0;
    out[n++] = 0x48; out[n++] = 0xb8; n = spartanport_put64(out, n, enabled);     /* movabs rax, enabled */
    out[n++] = 0x80; out[n++] = 0x38; out[n++] = 0x00;                            /* cmp byte [rax], 0 */
    out[n++] = 0x75; out[n++] = 2 * 10 + 2 * 6 + 1;                               /* jne on (skip 33 bytes) */
    out[n++] = 0x48; out[n++] = 0xb8; n = spartanport_put64(out, n, gate);        /* movabs rax, gate */
    out[n++] = 0xc7; out[n++] = 0x00; memset(out + n, 0, 4); n += 4;              /* mov dword [rax], 0 */
    out[n++] = 0x48; out[n++] = 0xb8; n = spartanport_put64(out, n, queued);      /* movabs rax, queued */
    out[n++] = 0xc7; out[n++] = 0x00; memset(out + n, 0, 4); n += 4;              /* mov dword [rax], 0 */
    out[n++] = 0xc3;                                                              /* ret */
    memcpy(out + n, spartanport_motion_blur_original, SPARTAN_MOTION_BLUR_HOOK_LENGTH);  /* on: */
    n += SPARTAN_MOTION_BLUR_HOOK_LENGTH;
    out[n++] = 0xff; out[n++] = 0x25; memset(out + n, 0, 4); n += 4;              /* jmp [rip+0] */
    return spartanport_put64(out, n, resume);
}

/* Shadows: off clears rsi (the casters list), then the prologue runs as usual. */
static inline unsigned spartanport_shadows_build_stub(unsigned char *out, uintptr_t enabled, uintptr_t resume) {
    unsigned n = 0;
    out[n++] = 0x48; out[n++] = 0xb8; n = spartanport_put64(out, n, enabled);     /* movabs rax, enabled */
    out[n++] = 0x80; out[n++] = 0x38; out[n++] = 0x00;                            /* cmp byte [rax], 0 */
    out[n++] = 0x75; out[n++] = 2;                                                /* jne on */
    out[n++] = 0x31; out[n++] = 0xf6;                                             /* xor esi, esi */
    memcpy(out + n, spartanport_motion_blur_original, SPARTAN_SHADOWS_HOOK_LENGTH);     /* on: */
    n += SPARTAN_SHADOWS_HOOK_LENGTH;
    out[n++] = 0xff; out[n++] = 0x25; memset(out + n, 0, 4); n += 4;              /* jmp [rip+0] */
    return spartanport_put64(out, n, resume);
}

/* Bloom: off jumps back with ZF set (from the cmp against the 0 toggle byte), so the game's
 * following je skips the bloom passes; on replays the original cmp. */
static inline unsigned spartanport_bloom_build_stub(unsigned char *out, uintptr_t enabled, uintptr_t resume) {
    unsigned n = 0;
    out[n++] = 0x48; out[n++] = 0xb8; n = spartanport_put64(out, n, enabled);     /* movabs rax, enabled */
    out[n++] = 0x80; out[n++] = 0x38; out[n++] = 0x00;                            /* cmp byte [rax], 0 */
    out[n++] = 0x74; out[n++] = SPARTAN_BLOOM_HOOK_LENGTH;                        /* je off (ZF stays set) */
    memcpy(out + n, spartanport_bloom_original, SPARTAN_BLOOM_HOOK_LENGTH);       /* on: the game's cmp */
    n += SPARTAN_BLOOM_HOOK_LENGTH;
    out[n++] = 0xff; out[n++] = 0x25; memset(out + n, 0, 4); n += 4;              /* off: jmp [rip+0] */
    return spartanport_put64(out, n, resume);
}
#endif
