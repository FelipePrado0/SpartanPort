/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SPARTAN_LEDGE_HOOK_H
#define SPARTAN_LEDGE_HOOK_H
#include <stdint.h>
#include <string.h>
#include "signature.h"

/* Wall seam fix. The ledge ray at image+0x5bfb2 misses through a ~0.015-unit seam
 * in the Realm of Hades wall: 60 FPS steps (~0.021) often skip it, 240 FPS steps (~0.005) always
 * land in it. A miss within SPARTAN_LEDGE_REUSE of the last real hit reuses that hit. The launcher
 * patch swaps 31 c9 for 33 c9 (the same xor ecx, ecx) after the call to opt in. */
#define SPARTAN_LEDGE_REUSE 0.05f
enum { SPARTAN_LEDGE_RESULT_WORDS = 24, SPARTAN_LEDGE_STUB_SIZE = 64 };
typedef struct {
    uintptr_t result, object; /* the game's 0x60-byte ray result; object of the cached hit */
    float position[3];
    uint32_t hit, hits, misses, reused;
    uint32_t cache[SPARTAN_LEDGE_RESULT_WORDS];
} SpartanLedgeCache;
static const SpartanSignature spartanport_ledge_site = {
    "e8 ?? ?? ?? ?? 33 c9 85 c0 0f 84 ?? ?? ?? ?? 48 8d 05 ?? ?? ?? ?? c5 f8 28 00", 0};
/* Runs right after the game's call returns: caller-saved registers, flags and SIMD are dead there.
 * ponytail: one cache, the game thread is the only caller; per-object caches if others appear. */
static __attribute__((noinline, sysv_abi, unused)) uint32_t spartanport_ledge_ray(
    SpartanLedgeCache *cache, uintptr_t frame, uintptr_t object, uint32_t hit) {
    if (!(*(const volatile uint32_t *)(object + 0x1c0) & 0x10)) return hit;
    const volatile float *position = (const volatile float *)(frame - 0x230);
    volatile uint32_t *result = (volatile uint32_t *)cache->result;
    if (hit) {
        for (unsigned i = 0; i < SPARTAN_LEDGE_RESULT_WORDS; ++i) cache->cache[i] = result[i];
        for (unsigned i = 0; i < 3; ++i) cache->position[i] = position[i];
        cache->object = object;
        cache->hit = hit;
        ++cache->hits;
        return hit;
    }
    ++cache->misses;
    if (!cache->hit || cache->object != object) return 0;
    float distance = 0;
    for (unsigned i = 0; i < 3; ++i) {
        const float delta = position[i] - cache->position[i];
        distance += delta * delta;
    }
    if (!(distance < SPARTAN_LEDGE_REUSE * SPARTAN_LEDGE_REUSE)) return 0;
    for (unsigned i = 0; i < SPARTAN_LEDGE_RESULT_WORDS; ++i) result[i] = cache->cache[i];
    ++cache->reused;
    return cache->hit;
}
/* Replaces the 5-byte call: calls the game's ray, then the helper with rbp, rbx and eax. */
static inline unsigned spartanport_ledge_stub(unsigned char *out, uintptr_t cache, uintptr_t helper,
                                              uintptr_t callee, uintptr_t resume) {
    if (!cache || !helper || !callee || !resume) return 0;
    unsigned n = 0;
    const unsigned char call[] = {0xff,0x15,2,0,0,0,0xeb,8};
    memcpy(out + n, call, sizeof(call)); n += sizeof(call);
    memcpy(out + n, &callee, 8); n += 8;
    out[n++] = 0x48; out[n++] = 0xbf;
    memcpy(out + n, &cache, 8); n += 8;
    const unsigned char args[] = {0x48,0x89,0xee,0x48,0x89,0xda,0x89,0xc1,0x48,0xb8}; /* rsi=rbp, rdx=rbx, ecx=eax */
    memcpy(out + n, args, sizeof(args)); n += sizeof(args);
    memcpy(out + n, &helper, 8); n += 8;
    const unsigned char tail[] = {0xff,0xd0,0xff,0x25,0,0,0,0};
    memcpy(out + n, tail, sizeof(tail)); n += sizeof(tail);
    memcpy(out + n, &resume, 8);
    return n + 8;
}
#endif
