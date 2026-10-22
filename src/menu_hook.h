/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SPARTAN_MENU_HOOK_H
#define SPARTAN_MENU_HOOK_H
#include <stdint.h>
#include <string.h>
#include "signature.h"

/* CUSA01623 v01.02, image offset 0x1d5f0f: the menu script interpreter's builtin call (opcode
 * 0x13), right before the provider's vtable call. esi holds the function id (12 bits), eax the
 * provider index, rbp the interpreter frame. The pause menu's "Exit to Desktop" row
 * (scripts/pause_menu.py) calls function 0xfff, which no provider has: the stub sends it to
 * `quit`, which does not return. GBL functions 0xf00..0xf60 and 0xffd/0xffe use our settings
 * provider; every other call replays the two instructions and goes on. */
enum { SPARTAN_MENU_HOOK_OFFSET = 0x1d5f0f, SPARTAN_MENU_HOOK_LENGTH = 11, SPARTAN_MENU_EXIT_FUNCTION = 0xfff };
static const unsigned char spartanport_menu_original[SPARTAN_MENU_HOOK_LENGTH] = {
    0x48,0x8b,0x8d,0x60,0xfd,0xff,0xff,  /* mov rcx, [rbp-0x2a0] */
    0x48,0x8b,0x3c,0xc1                  /* mov rdi, [rcx+rax*8] */
};
static const SpartanSignature spartanport_menu_site = {
    "c6 44 05 88 00 e9 ?? ?? ?? ?? 41 0f b6 0f 89 c8 c1 e8 04 39 d8 0f 8d ?? ?? ?? ?? 83 e1 0f "
    "c1 e1 08 41 0f b6 75 02 49 83 c5 03 09 ce 48 8b 8d 60 fd ff ff 48 8b 3c c1 48 8b 07", 44};

static inline int spartanport_menu_signature_match(const unsigned char *code, uint64_t size) {
    return size >= SPARTAN_MENU_HOOK_LENGTH && !memcmp(code, spartanport_menu_original, SPARTAN_MENU_HOOK_LENGTH);
}

static inline unsigned spartanport_menu_build_stub(unsigned char *out, uintptr_t quit, uintptr_t resume,
                                                  uintptr_t provider) {
    static const unsigned char body[] = {
        0x81,0xfe,0xff,0x0f,0,0,             /* cmp esi, 0xfff */
        0x75,0x10,                           /* jne +16 (any other function) */
        0x48,0x83,0xe4,0xf0,                 /* and rsp, -16 (quit does not return) */
        0x48,0xb8, 0,0,0,0,0,0,0,0,          /* movabs rax, quit */
        0xff,0xd0,                           /* call rax */
        0x85,0xc0,                           /* test eax, eax (GBL only) */
        0x75,0x2c,                           /* jne original */
        0x81,0xfe,0x00,0x0f,0,0,             /* cmp esi, 0xf00 */
        0x72,0x24,                           /* jb original */
        0x81,0xfe,0x60,0x0f,0,0,             /* cmp esi, 0xf60 */
        0x76,0x10,                           /* jbe native */
        0x81,0xfe,0xfd,0x0f,0,0,             /* cmp esi, 0xffd (toggle) */
        0x74,0x08,                           /* je native */
        0x81,0xfe,0xfe,0x0f,0,0,             /* cmp esi, 0xffe (query) */
        0x75,0x0c,                           /* jne original */
        0x48,0xbf,0,0,0,0,0,0,0,0,          /* movabs rdi, provider */
        0xeb,SPARTAN_MENU_HOOK_LENGTH,        /* jmp resume (skip original lookup) */
    };
    static const unsigned char jump[] = {0xff,0x25,0,0,0,0}; /* jmp [rip+0] */
    unsigned n = 0;
    memcpy(out, body, sizeof(body));
    memcpy(out + 14, &quit, 8);
    memcpy(out + 62, &provider, 8);
    n += sizeof(body);
    memcpy(out + n, spartanport_menu_original, SPARTAN_MENU_HOOK_LENGTH);
    n += SPARTAN_MENU_HOOK_LENGTH;
    memcpy(out + n, jump, sizeof(jump));
    n += sizeof(jump);
    memcpy(out + n, &resume, 8);
    return n + 8;
}
#endif
