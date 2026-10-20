/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SPARTAN_QTE_HOOK_H
#define SPARTAN_QTE_HOOK_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "signature.h"

/* The game's QTE prompts ("minigame buttons"). Offsets as found in a v01.02 build; builds are found
 * by signature. 0x12c670 starts a prompt: it stores the prompt type at +0x10 of a state block
 * (0 triangle, 1 circle, 2 cross, 3 square, 4 Up180CW, 5 Up180CCW, 6 Down180CW, 7 Down180CCW,
 * 8 360CW, 9 360CCW) and makes 0x12c4a0 the object's update, which runs every frame while the
 * prompt is up. For a rotation, that update asks 0x12c280 whether the stick finished the turn
 * (1: passed, then the game plays its own success).
 * Update hook: counts the frames (a prompt is up while the count moves). Rotation hook: answers 1
 * once when the port has set pass. Both sites are the functions' first three instructions
 * (push rbp; mov rbp,rsp; push r15 or r14), where rax and the flags are free. The state block's
 * address is read from the rotation check's lea r8,[rip+disp] (site +0x1c). */
enum { SPARTAN_QTE_HOOK_LENGTH = 6, SPARTAN_QTE_TYPE_OFFSET = 0x10,
       SPARTAN_QTE_BLOCK_DISPLACEMENT = 0x1f, SPARTAN_QTE_BLOCK_NEXT = 0x23 };
static const unsigned char spartanport_qte_update_original[SPARTAN_QTE_HOOK_LENGTH] = {
    0x55, 0x48,0x89,0xe5, 0x41,0x57  /* push rbp; mov rbp,rsp; push r15 */
};
static const unsigned char spartanport_qte_rotation_original[SPARTAN_QTE_HOOK_LENGTH] = {
    0x55, 0x48,0x89,0xe5, 0x41,0x56  /* push rbp; mov rbp,rsp; push r14 */
};
static const SpartanSignature spartanport_qte_update_site = {
    "55 48 89 e5 41 57 41 56 41 55 41 54 53 50 49 89 ff 49 8b 87 b0 00 00 00 a8 07 0f 85 ?? ?? ?? ?? 48 8d 0d", 0};
static const SpartanSignature spartanport_qte_rotation_site = {
    "55 48 89 e5 41 56 53 49 89 fe c5 fa 10 05 ?? ?? ?? ?? 31 ff 4c 89 f6 e8 ?? ?? ?? ?? 4c 8d 05 ?? ?? ?? ?? "
    "49 8b 58 10 0f b7 d3", 0};

/* Shared by the stubs (game thread) and the pad (runtime_pad.c). block: the game's state block,
 * 0 while the hooks are not installed. */
typedef struct {
    uint32_t heartbeat;  /* update calls */
    uint8_t pass;        /* set by the pad; the rotation stub answers 1 once and clears it */
    uint8_t pad[3];
    uint64_t block;
} SpartanQteState;
#ifdef __cplusplus
static_assert(offsetof(SpartanQteState, pass) == 4, "stubs address the fields directly");
#else
_Static_assert(offsetof(SpartanQteState, pass) == 4, "stubs address the fields directly");
#endif

/* The state block's image offset, read from the rotation check at its signature site. */
static inline uint64_t spartanport_qte_block(const unsigned char *code, uint64_t site) {
    int32_t displacement;
    memcpy(&displacement, code + SPARTAN_QTE_BLOCK_DISPLACEMENT, 4);
    return site + SPARTAN_QTE_BLOCK_NEXT + (int64_t)displacement;
}

static inline unsigned spartanport_qte_finish_stub(unsigned char *out, unsigned n, const unsigned char *original,
                                            uintptr_t resume) {
    static const unsigned char jump[] = {0xff,0x25,0,0,0,0}; /* jmp [rip+0] */
    memcpy(out + n, original, SPARTAN_QTE_HOOK_LENGTH);
    n += SPARTAN_QTE_HOOK_LENGTH;
    memcpy(out + n, jump, sizeof(jump));
    n += sizeof(jump);
    memcpy(out + n, &resume, 8);
    return n + 8;
}

static inline unsigned spartanport_qte_build_update_stub(unsigned char *out, uintptr_t heartbeat, uintptr_t resume) {
    static const unsigned char body[] = {
        0x48,0xb8, 0,0,0,0,0,0,0,0,  /* movabs rax, heartbeat */
        0xf0,0xff,0x00,              /* lock inc dword [rax] */
    };
    memcpy(out, body, sizeof(body));
    memcpy(out + 2, &heartbeat, 8);
    return spartanport_qte_finish_stub(out, sizeof(body), spartanport_qte_update_original, resume);
}

/* auto: the menu's Automatic QTE byte; while it is set every turn is finished at once. */
static inline unsigned spartanport_qte_build_rotation_stub(unsigned char *out, uintptr_t pass, uintptr_t automatic,
                                                    uintptr_t resume) {
    static const unsigned char body[] = {
        0x48,0xb8, 0,0,0,0,0,0,0,0,  /* movabs rax, auto */
        0x80,0x38,0x00,              /* cmp byte [rax], 0 */
        0x75,0x12,                   /* jne finished */
        0x48,0xb8, 0,0,0,0,0,0,0,0,  /* movabs rax, pass */
        0x80,0x38,0x00,              /* cmp byte [rax], 0 */
        0x74,0x09,                   /* je original */
        0xc6,0x00,0x00,              /* mov byte [rax], 0 */
        0xb8,0x01,0x00,0x00,0x00,    /* finished: mov eax, 1 */
        0xc3,                        /* ret: the turn is finished */
    };
    memcpy(out, body, sizeof(body));
    memcpy(out + 2, &automatic, 8);
    memcpy(out + 17, &pass, 8);
    return spartanport_qte_finish_stub(out, sizeof(body), spartanport_qte_rotation_original, resume);
}

static inline unsigned spartanport_qte_jump(unsigned char *out, unsigned n, uintptr_t resume) {
    static const unsigned char jump[] = {0xff,0x25,0,0,0,0}; /* jmp [rip+0] */
    memcpy(out + n, jump, sizeof(jump));
    memcpy(out + n + sizeof(jump), &resume, 8);
    return n + sizeof(jump) + 8;
}

/* Automatic QTE in the other two prompt scripts, and in the button check of the timed one (the
 * script names are the game's: SCR_TimedPress, SCR_RepeatPress, SCR_ChargedPress). Each stub reads
 * the menu's byte and leaves the game's own code alone while it is 0.
 *
 * Timed press, button prompts (types 0-3): the update takes the buttons pressed this frame from
 * the player's pad (andn ecx, [pad+0x98], [pad+0x94]) and passes when bit type+4 is set; another
 * face button fails the prompt. With the byte set, ecx is that one bit. Site: 15 bytes after the
 * type check, the two instructions moved into the stub (rbx: the state block, rdi: the pad). */
enum { SPARTAN_QTE_TIMED_LENGTH = 15, SPARTAN_QTE_CHARGED_LENGTH = 7 };
static const SpartanSignature spartanport_qte_timed_site = {
    "66 83 7b 10 03 77 ?? 8b 87 98 00 00 00 c4 e2 78 f2 8f 94 00 00 00 8b 53 10 8d 72 04", 7};
static inline unsigned spartanport_qte_build_timed_stub(unsigned char *out, uintptr_t automatic, uintptr_t resume) {
    static const unsigned char body[] = {
        0x48,0xb8, 0,0,0,0,0,0,0,0,               /* movabs rax, auto */
        0x80,0x38,0x00,                           /* cmp byte [rax], 0 */
        0x75,0x11,                                /* jne automatic */
        0x8b,0x87,0x98,0x00,0x00,0x00,            /* mov eax, [rdi+0x98] */
        0xc4,0xe2,0x78,0xf2,0x8f,0x94,0,0,0,      /* andn ecx, eax, [rdi+0x94] */
        0xeb,0x10,                                /* jmp back */
        0x0f,0xb7,0x4b,0x10,                      /* automatic: movzx ecx, word [rbx+0x10] */
        0x83,0xc1,0x04,                           /* add ecx, 4 */
        0xb8,0x01,0x00,0x00,0x00,                 /* mov eax, 1 */
        0xd3,0xe0,                                /* shl eax, cl */
        0x89,0xc1,                                /* mov ecx, eax: the prompt's button, alone */
    };
    memcpy(out, body, sizeof(body));
    memcpy(out + 2, &automatic, 8);
    return spartanport_qte_jump(out, sizeof(body), resume);
}

/* Repeat press (mash): the update asks 0x72eb0(pad, button, &pressure) whether the prompt's button
 * was pressed this frame. A press adds [config+0x1c] times [state+8] / [config+0x14] (the time since
 * the last press over the full-credit interval, at most 1) and resets [state+8]; the bar drains only
 * on frames without one. With the byte set the stub presses every frame with that frame's credit:
 * [state+8] = the frame time ([rbp-0x3c] in the update), the most any mashing gets, with no drain.
 * (rbx: the state.) Site: the call, which becomes a call to the stub. */
static const SpartanSignature spartanport_qte_repeat_site = {
    "41 0f b6 77 2c 48 8d 55 cc 48 8b 7d b8 e8 ?? ?? ?? ?? 41 89 c4 45 85 e4", 13};
static inline unsigned spartanport_qte_build_repeat_stub(unsigned char *out, uintptr_t automatic, uintptr_t original) {
    static const unsigned char body[] = {
        0x48,0xb8, 0,0,0,0,0,0,0,0,               /* movabs rax, auto */
        0x80,0x38,0x00,                           /* cmp byte [rax], 0 */
        0x74,0x10,                                /* je original */
        0xc5,0xfa,0x10,0x45,0xc4,                 /* vmovss xmm0, [rbp-0x3c]: the frame time */
        0xc5,0xfa,0x11,0x43,0x08,                 /* vmovss [rbx+8], xmm0 */
        0xb8,0x01,0x00,0x00,0x00,                 /* mov eax, 1 */
        0xc3,                                     /* ret: pressed */
    };
    memcpy(out, body, sizeof(body));
    memcpy(out + 2, &automatic, 8);
    return spartanport_qte_jump(out, sizeof(body), original);  /* original: the game's own answer */
}
/* The call's target, from its rel32. */
static inline uint64_t spartanport_qte_call_target(const unsigned char *code, uint64_t site) {
    int32_t relative;
    memcpy(&relative, code + 1, 4);
    return site + 5 + (int64_t)relative;
}

/* Charged press (hold): the update loads the held buttons (mov r9d, [r14+0x94]) and fills the bar
 * while bit button+2 is set. With the byte set every button is held. */
static const SpartanSignature spartanport_qte_charged_site = {
    "83 c0 02 45 8b 8e 94 00 00 00 44 20 c2 31 f6 41 0f a3 c1", 3};
static inline unsigned spartanport_qte_build_charged_stub(unsigned char *out, uintptr_t automatic, uintptr_t resume) {
    static const unsigned char body[] = {
        0x49,0xb9, 0,0,0,0,0,0,0,0,               /* movabs r9, auto */
        0x41,0x80,0x39,0x00,                      /* cmp byte [r9], 0 */
        0x45,0x8b,0x8e,0x94,0x00,0x00,0x00,       /* mov r9d, [r14+0x94] */
        0x74,0x06,                                /* je back */
        0x41,0xb9,0xff,0xff,0xff,0xff,            /* mov r9d, -1: held */
    };
    memcpy(out, body, sizeof(body));
    memcpy(out + 2, &automatic, 8);
    return spartanport_qte_jump(out, sizeof(body), resume);
}

/* Circle struggle (SCR_ExplosiveRain): its own bar, drained every frame and raised by each new
 * Circle press (test [pad+0x94] and not [pad+0x98], bit 0x20). With the byte set every frame is a
 * press. Site: the two tests and their branches (rax: the pad; rcx is free here). */
enum { SPARTAN_QTE_CIRCLE_LENGTH = 18, SPARTAN_QTE_CIRCLE_SKIP = 0x26 };
static const SpartanSignature spartanport_qte_circle_site = {
    "48 8b 40 10 48 8b 40 08 f6 80 94 00 00 00 20 74 1d f6 80 98 00 00 00 20 75 14 c4 c1 7a 10 04 24", 8};
static inline unsigned spartanport_qte_build_circle_stub(unsigned char *out, uintptr_t automatic, uintptr_t pressed,
                                                  uintptr_t skip) {
    static const unsigned char body[] = {
        0x48,0xb9, 0,0,0,0,0,0,0,0,               /* movabs rcx, auto */
        0x80,0x39,0x00,                           /* cmp byte [rcx], 0 */
        0x75,0x12,                                /* jne pressed */
        0xf6,0x80,0x94,0x00,0x00,0x00,0x20,       /* test byte [rax+0x94], 0x20 */
        0x74,0x17,                                /* je skip */
        0xf6,0x80,0x98,0x00,0x00,0x00,0x20,       /* test byte [rax+0x98], 0x20 */
        0x75,0x0e,                                /* jne skip; else on to pressed */
    };
    memcpy(out, body, sizeof(body));
    memcpy(out + 2, &automatic, 8);
    unsigned n = spartanport_qte_jump(out, sizeof(body), pressed);
    return spartanport_qte_jump(out, n, skip);
}
#endif
