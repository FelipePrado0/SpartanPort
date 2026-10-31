/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SPARTAN_HIGH_FPS_TRACE_H
#define SPARTAN_HIGH_FPS_TRACE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "signature.h"

enum { SPARTAN_TRACE_CAPACITY = 64, SPARTAN_TRACE_POINTS = 8, SPARTAN_TRACE_STUB_SIZE = 256,
       SPARTAN_TRACE_FRAME_CONTEXT = 4 };
typedef struct {
    float input[4], rate;
    uint32_t state;
    uintptr_t caller;
} SpartanTraceRecord;
typedef struct {
    uint32_t lock, count, dropped, reserved;
    SpartanTraceRecord records[SPARTAN_TRACE_CAPACITY];
} SpartanTraceSlot;
typedef struct {
    uint32_t calls, dropped, samples;
    SpartanTraceRecord records[SPARTAN_TRACE_CAPACITY];
} SpartanTraceSnapshot;
typedef struct {
    uintptr_t object, caller;
    uint32_t samples, state;
    float input, rate;
} SpartanTraceGroup;

static inline unsigned spartanport_trace_group_context(const SpartanTraceSnapshot *snapshot, SpartanTraceGroup *groups) {
    unsigned count = 0;
    for (unsigned i = 0; i < snapshot->samples && i < SPARTAN_TRACE_CAPACITY; ++i) {
        const SpartanTraceRecord *record = &snapshot->records[i];
        uintptr_t object;
        memcpy(&object, record->input + 2, sizeof(object));
        unsigned group = 0;
        while (group < count && (groups[group].object != object || groups[group].caller != record->caller)) ++group;
        if (group == count) {
            memset(&groups[group], 0, sizeof(groups[group]));
            groups[group].object = object;
            groups[group].caller = record->caller;
            ++count;
        }
        ++groups[group].samples;
        groups[group].state = record->state;
        groups[group].input = record->input[0];
        groups[group].rate = record->rate;
    }
    return count;
}
#ifdef __cplusplus
static_assert(sizeof(SpartanTraceRecord) == 32 && offsetof(SpartanTraceSlot, records) == 16, "stub layout");
static_assert(offsetof(SpartanTraceRecord, state) == 20 && offsetof(SpartanTraceRecord, caller) == 24, "context layout");
#else
_Static_assert(sizeof(SpartanTraceRecord) == 32 && offsetof(SpartanTraceSlot, records) == 16, "stub layout");
_Static_assert(offsetof(SpartanTraceRecord, state) == 20 && offsetof(SpartanTraceRecord, caller) == 24, "context layout");
#endif

enum { SPARTAN_COLLISION_SLOT = SPARTAN_TRACE_POINTS, SPARTAN_COLLISION_HELPER = SPARTAN_TRACE_POINTS + 1,
       SPARTAN_CONTACT_STAGES = 3, SPARTAN_CONTACT_PHASES = 5, SPARTAN_CONTACT_BRANCHES = 7,
       SPARTAN_CONTACT_PATH = 16, SPARTAN_CONTACT_HISTORY = 8 };
/* Frame and object values the wall checks after the primary rays compare. Vectors, in order:
 * [rbp-0x230] probe 0xa4370 output, [rbp-0x300], [rbp-0x90] and [rbp-0x80] ledge ray hit and
 * normal, [rbp-0x70], [rbp-0x2f0], object+0x170, object+0x180. */
typedef struct {
    uint32_t vectors[8][4];
    uint32_t height, blocked, side, ecx;
    uintptr_t surface;
} SpartanContactProbe;
typedef struct {
    uintptr_t object, caller;
    uint32_t request[4], position[4], flags, valid, query_flags, result;
} SpartanCollisionSample;
typedef struct {
    uint32_t calls, mask, threshold, up[4], points[4][4], hits[2][4], normals[2][4];
} SpartanContactStage;

/* path: branch index in bits 0-6, bit 7 set when the jump was taken. history: target x, y, z
 * and the query result of the newest wall calls, oldest first. */
typedef struct {
    uint32_t valid, stage_mask;
    SpartanCollisionSample pair[2];
    SpartanContactStage stages[SPARTAN_CONTACT_STAGES];
    uint32_t path_count, history_count;
    uint8_t path[SPARTAN_CONTACT_PATH];
    SpartanContactProbe probe;
    uint32_t history[SPARTAN_CONTACT_HISTORY][4];
} SpartanContactDetail;
typedef struct {
    uintptr_t frame, request;
    SpartanCollisionSample sample;
    uint32_t stage_mask;
    SpartanContactStage stages[SPARTAN_CONTACT_STAGES];
    uint32_t path_count;
    uint8_t path[SPARTAN_CONTACT_PATH];
    SpartanContactProbe probe;
} SpartanCollisionPending;
/* fail_* and pass_* are the lowest and highest target z (float bits) of wall calls. */
typedef struct {
    uint32_t calls, requested, moved, adjusted, flags_seen, results_seen, zero_results;
    SpartanCollisionSample first[2], last[2];
    uint32_t wall_failures, passes, fail_low, fail_high, pass_low, pass_high, history_count;
    uint32_t history[SPARTAN_CONTACT_HISTORY][4];
    SpartanContactDetail first_failure, last_success;
} SpartanCollisionGroup;
typedef struct {
    uint32_t lock, entries, completed, busy, unpaired, full, count;
    SpartanCollisionPending pending[SPARTAN_TRACE_CAPACITY];
    SpartanCollisionGroup groups[SPARTAN_TRACE_CAPACITY];
} SpartanCollisionTrace;
typedef struct {
    uint32_t entries, completed, busy, unpaired, full, pending, count;
    SpartanCollisionGroup groups[SPARTAN_TRACE_CAPACITY];
} SpartanCollisionSnapshot;

#define SPARTAN_TRACE_INTEGER __attribute__((target("general-regs-only")))
/* Helpers called from guest code must never touch SIMD/FPU state or call the host CRT. */
static inline SPARTAN_TRACE_INTEGER void spartanport_collision_copy(void *destination, const void *source, size_t size) {
    volatile unsigned char *out = (volatile unsigned char *)destination;
    const volatile unsigned char *in = (const volatile unsigned char *)source;
    for (size_t i = 0; i < size; ++i) out[i] = in[i];
}
static inline SPARTAN_TRACE_INTEGER void spartanport_collision_sample(SpartanCollisionSample *sample,
    uintptr_t object, uintptr_t caller, uintptr_t request, uintptr_t query_flags) {
    sample->object = object;
    sample->caller = caller;
    sample->valid = request ? 1u : 0u;
    for (unsigned i = 0; i < 4; ++i) {
        sample->request[i] = request ? ((const volatile uint32_t *)request)[i] : 0;
        sample->position[i] = ((const volatile uint32_t *)(object + 0xa0))[i];
    }
    sample->flags = *(const volatile uint32_t *)(object + 0x1c0);
    sample->query_flags = (uint32_t)query_flags;
    sample->result = 0;
}
static inline SPARTAN_TRACE_INTEGER void spartanport_contact_stage(SpartanContactStage *stage,
    uintptr_t frame, unsigned index, uint32_t mask) {
    const uintptr_t points = frame - (0x1a0 + index * 0x40);
    ++stage->calls;
    stage->mask = mask;
    stage->threshold = *(const volatile uint32_t *)(frame - 0x24c);
    for (unsigned lane = 0; lane < 4; ++lane) {
        stage->up[lane] = ((const volatile uint32_t *)(frame - 0x2b0))[lane];
        for (unsigned point = 0; point < 4; ++point)
            stage->points[point][lane] = ((const volatile uint32_t *)points)[point * 4 + lane];
        /* The return mask uses two bits per ray: 1/2 for the first, 4/8 for the second.
         * A miss leaves that ray's output uninitialized in the game, so never read it. */
        for (unsigned ray = 0; ray < 2; ++ray) {
            const uintptr_t hit = frame - 0x150 + ray * 0x60;
            const unsigned valid = mask & (3u << (ray * 2));
            stage->hits[ray][lane] = valid ? ((const volatile uint32_t *)hit)[lane] : 0;
            stage->normals[ray][lane] = valid ? ((const volatile uint32_t *)(hit + 0x10))[lane] : 0;
        }
    }
}
static inline SPARTAN_TRACE_INTEGER void spartanport_contact_probe(SpartanContactProbe *probe,
    uintptr_t frame, uintptr_t object, uint32_t ecx) {
    static const int32_t frame_vectors[6] = {-0x230, -0x300, -0x90, -0x80, -0x70, -0x2f0};
    for (unsigned lane = 0; lane < 4; ++lane) {
        for (unsigned i = 0; i < 6; ++i)
            probe->vectors[i][lane] = ((const volatile uint32_t *)(frame + frame_vectors[i]))[lane];
        probe->vectors[6][lane] = ((const volatile uint32_t *)(object + 0x170))[lane];
        probe->vectors[7][lane] = ((const volatile uint32_t *)(object + 0x180))[lane];
    }
    probe->height = *(const volatile uint32_t *)(object + 0x190);
    probe->blocked = *(const volatile uint32_t *)(frame - 0x2b8);
    probe->side = *(const volatile uint32_t *)(frame - 0x2b4);
    probe->ecx = ecx;
    probe->surface = *(const volatile uintptr_t *)(frame - 0x60);
}
/* Float bits mapped to an integer with the same order, without touching SIMD state. */
static inline SPARTAN_TRACE_INTEGER int32_t spartanport_float_order(uint32_t bits) {
    return (int32_t)(bits ^ ((uint32_t)((int32_t)bits >> 31) >> 1));
}
static inline SPARTAN_TRACE_INTEGER void spartanport_contact_detail(SpartanContactDetail *out,
    const SpartanCollisionPending *pending, const SpartanCollisionSample *after, const SpartanCollisionGroup *group) {
    out->valid = 1;
    out->stage_mask = pending->stage_mask;
    spartanport_collision_copy(&out->pair[0], &pending->sample, sizeof(pending->sample));
    spartanport_collision_copy(&out->pair[1], after, sizeof(*after));
    for (unsigned i = 0; i < SPARTAN_CONTACT_STAGES; ++i)
        spartanport_collision_copy(&out->stages[i], &pending->stages[i], sizeof(pending->stages[i]));
    out->path_count = pending->path_count;
    spartanport_collision_copy(out->path, pending->path, sizeof(out->path));
    spartanport_collision_copy(&out->probe, &pending->probe, sizeof(out->probe));
    const unsigned kept = group->history_count < SPARTAN_CONTACT_HISTORY ? group->history_count : SPARTAN_CONTACT_HISTORY;
    out->history_count = kept;
    for (unsigned i = 0; i < kept; ++i)
        spartanport_collision_copy(out->history[i],
            group->history[(group->history_count - kept + i) % SPARTAN_CONTACT_HISTORY], sizeof(out->history[i]));
}
static inline SPARTAN_TRACE_INTEGER unsigned spartanport_branch_taken(unsigned char condition, uint64_t flags) {
    const unsigned carry = flags & 1, zero = (flags >> 6) & 1;
    if (condition == 0x84) return zero;
    if (condition == 0x85) return !zero;
    if (condition == 0x82) return carry;
    return condition == 0x86 ? carry | zero : 0;
}
typedef struct {
    const char *name;
    SpartanSignature site;
    unsigned char condition; /* second byte of the 0f 8x jcc rel32 */
} SpartanContactBranch;
/* The wall checks between the primary rays and the fallback (image+0x5bbfc..0x5c4bb in v01.02).
 * "top" jumps to the success path when taken; the others jump to the fallback. */
static const SpartanContactBranch spartanport_contact_branches[SPARTAN_CONTACT_BRANCHES] = {
    {"normals", {"c5 fa 58 c1 48 8d 05 ?? ?? ?? ?? c5 f8 2e 00 0f 86 ?? ?? ?? ??", 15}, 0x86},
    {"ledge-ray", {"45 31 ff f6 c1 10 0f 84 ?? ?? ?? ??", 6}, 0x84},
    {"blocked", {"83 bd 48 fd ff ff 00 0f 85 ?? ?? ?? ??", 7}, 0x85},
    {"ledge-steep", {"c5 f8 2e 8d b4 fd ff ff 0f 82 ?? ?? ?? ??", 8}, 0x82},
    {"ledge-surface", {"48 8b 45 a0 48 8d 0d ?? ?? ?? ?? 8b 09 f6 44 08 03 01 0f 84 ?? ?? ?? ??", 18}, 0x84},
    {"side-ray", {"c5 f0 5c ca e8 ?? ?? ?? ?? 45 31 ff 85 c0 0f 84 ?? ?? ?? ??", 14}, 0x84},
    {"top", {"c5 fa 10 48 04 c5 f8 2e c8 0f 86 ?? ?? ?? ??", 9}, 0x86},
};
static inline int spartanport_branch_match(unsigned branch, const unsigned char *code, uint64_t size) {
    return branch < SPARTAN_CONTACT_BRANCHES && size >= 6 && code[0] == 0x0f &&
           code[1] == spartanport_contact_branches[branch].condition;
}
/* The fifth argument is the target pointer on entry, the return mask on exit and ecx at a branch.
 * At a branch the sixth is the guest flags. */
static SPARTAN_TRACE_INTEGER __attribute__((noinline, sysv_abi, unused)) void spartanport_collision_record(
    uintptr_t address, unsigned exit, uintptr_t frame, uintptr_t object, uintptr_t request, uintptr_t query_flags) {
    if (exit >= SPARTAN_CONTACT_PHASES + SPARTAN_CONTACT_BRANCHES) return;
    SpartanCollisionTrace *trace = (SpartanCollisionTrace *)address;
    if (__atomic_exchange_n(&trace->lock, 1, __ATOMIC_ACQUIRE)) {
        __atomic_fetch_add(&trace->busy, 1, __ATOMIC_RELAXED);
        return;
    }
    unsigned pending = SPARTAN_TRACE_CAPACITY, empty = SPARTAN_TRACE_CAPACITY;
    for (unsigned i = 0; i < SPARTAN_TRACE_CAPACITY; ++i) {
        if (trace->pending[i].frame == frame) { pending = i; break; }
        if (!trace->pending[i].frame && empty == SPARTAN_TRACE_CAPACITY) empty = i;
    }
    if (!exit) {
        ++trace->entries;
        if (pending != SPARTAN_TRACE_CAPACITY) ++trace->unpaired;
        else pending = empty;
        if (pending == SPARTAN_TRACE_CAPACITY) ++trace->full;
        else {
            SpartanCollisionPending *item = &trace->pending[pending];
            item->frame = frame;
            item->request = request;
            item->stage_mask = 0;
            item->path_count = 0;
            for (unsigned i = 0; i < SPARTAN_CONTACT_STAGES; ++i) item->stages[i].calls = 0;
            spartanport_collision_sample(&item->sample, object, *(const uintptr_t *)(frame + 8), request, query_flags);
        }
    } else if (pending == SPARTAN_TRACE_CAPACITY || trace->pending[pending].sample.object != object) {
        ++trace->unpaired;
    } else if (exit >= SPARTAN_CONTACT_PHASES) {
        SpartanCollisionPending *item = &trace->pending[pending];
        if (item->sample.flags & 0x10) {
            const unsigned branch = exit - SPARTAN_CONTACT_PHASES;
            const unsigned taken = spartanport_branch_taken(spartanport_contact_branches[branch].condition, query_flags);
            if (item->path_count < SPARTAN_CONTACT_PATH) item->path[item->path_count] = (uint8_t)(branch | (taken << 7));
            ++item->path_count;
            spartanport_contact_probe(&item->probe, frame, object, (uint32_t)request);
        }
    } else if (exit >= 2) {
        SpartanCollisionPending *item = &trace->pending[pending];
        if (item->sample.flags & 0x10) {
            spartanport_contact_stage(&item->stages[exit - 2], frame, exit - 2, (uint32_t)request);
            item->stage_mask |= 1u << (exit - 2);
        }
    } else {
        SpartanCollisionPending *item = &trace->pending[pending];
        unsigned group = 0;
        while (group < trace->count && (trace->groups[group].first[0].object != object ||
               trace->groups[group].first[0].caller != item->sample.caller)) ++group;
        ++trace->completed;
        if (group == SPARTAN_TRACE_CAPACITY) ++trace->full;
        else {
            SpartanCollisionGroup *result = &trace->groups[group];
            SpartanCollisionSample after;
            spartanport_collision_sample(&after, object, item->sample.caller, item->request, 0);
            after.query_flags = item->sample.query_flags;
            after.result = (uint32_t)request;
            after.valid |= 2u;
            if (group == trace->count) {
                ++trace->count;
                result->calls = result->requested = result->moved = result->adjusted = result->flags_seen = 0;
                result->results_seen = result->zero_results = 0;
                result->wall_failures = result->passes = result->history_count = 0;
                result->first_failure.valid = result->last_success.valid = 0;
                spartanport_collision_copy(&result->first[0], &item->sample, sizeof(item->sample));
                spartanport_collision_copy(&result->first[1], &after, sizeof(after));
            }
            unsigned requested = 0, moved = 0, adjusted = 0;
            for (unsigned i = 0; i < 3; ++i) {
                requested |= item->sample.request[i] & 0x7fffffff;
                moved |= (item->sample.position[i] != after.position[i]);
                adjusted |= (item->sample.request[i] != after.request[i]);
            }
            ++result->calls;
            result->requested += requested != 0;
            result->moved += moved != 0;
            result->adjusted += adjusted != 0;
            result->flags_seen |= item->sample.flags | after.flags;
            result->results_seen |= after.result;
            result->zero_results += after.result == 0;
            if (item->sample.flags & 0x10) {
                /* ponytail: z only, the Hades wall runs along z; record x too for other walls. */
                const uint32_t z = item->sample.request[2];
                uint32_t *slot = result->history[result->history_count++ % SPARTAN_CONTACT_HISTORY];
                for (unsigned i = 0; i < 3; ++i) slot[i] = item->sample.request[i];
                slot[3] = after.result;
                uint32_t *low = after.result ? &result->pass_low : &result->fail_low;
                uint32_t *high = after.result ? &result->pass_high : &result->fail_high;
                if (!(after.result ? result->passes : result->wall_failures)) *low = *high = z;
                if (spartanport_float_order(z) < spartanport_float_order(*low)) *low = z;
                if (spartanport_float_order(z) > spartanport_float_order(*high)) *high = z;
                if (!after.result) {
                    ++result->wall_failures;
                    if (!result->first_failure.valid) spartanport_contact_detail(&result->first_failure, item, &after, result);
                } else {
                    ++result->passes;
                    spartanport_contact_detail(&result->last_success, item, &after, result);
                }
            }
            spartanport_collision_copy(&result->last[0], &item->sample, sizeof(item->sample));
            spartanport_collision_copy(&result->last[1], &after, sizeof(after));
        }
        item->frame = 0;
    }
    __atomic_store_n(&trace->lock, 0, __ATOMIC_RELEASE);
}
static inline int spartanport_collision_take(SpartanCollisionTrace *trace, SpartanCollisionSnapshot *out) {
    if (__atomic_exchange_n(&trace->lock, 1, __ATOMIC_ACQUIRE)) return 0;
    out->entries = trace->entries; out->completed = trace->completed;
    out->busy = __atomic_exchange_n(&trace->busy, 0, __ATOMIC_RELAXED);
    out->unpaired = trace->unpaired; out->full = trace->full; out->count = trace->count;
    out->pending = 0;
    for (unsigned i = 0; i < SPARTAN_TRACE_CAPACITY; ++i) out->pending += trace->pending[i].frame != 0;
    memcpy(out->groups, trace->groups, trace->count * sizeof(trace->groups[0]));
    trace->entries = trace->completed = trace->unpaired = trace->full = trace->count = 0;
    __atomic_store_n(&trace->lock, 0, __ATOMIC_RELEASE);
    return 1;
}
static const SpartanSignature spartanport_collision_sites[SPARTAN_CONTACT_PHASES] = {
    {"48 89 8d a8 fd ff ff 41 89 d5 48 89 b5 b8 fd ff ff c5 f8 29 85 30 fd ff ff 48 89 fb", 10},
    {"41 89 c7 48 8b 05 ?? ?? ?? ?? 48 8b 00 48 3b 45 d0 75 15 44 89 f8 48 81 c4 08 03 00 00 5b 41 5c 41 5d 41 5e 41 5f 5d c3", 3},
    {"4c 8b 85 a8 fd ff ff e8 ?? ?? ?? ?? 41 89 c4 45 85 e4 0f 85 ?? ?? ?? ?? f6 83 c1 01 00 00 08", 12},
    {"4c 8b 85 a8 fd ff ff e8 ?? ?? ?? ?? 41 89 c4 41 f6 c4 02 74 ?? c5 fa 10 85 c4 fe ff ff", 12},
    {"4c 8b 85 a8 fd ff ff e8 ?? ?? ?? ?? 83 e0 0a 83 f8 0a 0f 85 ?? ?? ?? ??", 12}
};
static const unsigned char spartanport_collision_original[SPARTAN_CONTACT_PHASES][7] = {
    {0x48,0x89,0xb5,0xb8,0xfd,0xff,0xff}, /* mov [rbp-0x248], rsi */
    {0x48,0x8b,0x05,0,0,0,0},          /* mov rax, [rip+disp32] */
    {0x41,0x89,0xc4,0x45,0x85,0xe4,0}, /* mov r12d, eax; test r12d, r12d */
    {0x41,0x89,0xc4,0x41,0xf6,0xc4,2}, /* mov r12d, eax; test r12b, 2 */
    {0x83,0xe0,0x0a,0x83,0xf8,0x0a,0} /* and eax, 10; cmp eax, 10 */
};
static inline unsigned spartanport_collision_length(unsigned exit) {
    if (exit >= SPARTAN_CONTACT_PHASES) return 0;
    return exit == 2 || exit == 4 ? 6 : 7;
}
static inline int spartanport_collision_match(unsigned exit, const unsigned char *code, uint64_t size) {
    const unsigned length = spartanport_collision_length(exit);
    return length && size >= length && !memcmp(code, spartanport_collision_original[exit], exit == 1 ? 3 : length);
}
static inline unsigned spartanport_collision_prefix(unsigned char *out, uintptr_t trace, uintptr_t helper, unsigned exit) {
    if (!trace || !helper || exit >= SPARTAN_CONTACT_PHASES + SPARTAN_CONTACT_BRANCHES) return 0;
    static const unsigned char save[] = {
        0x48,0x8d,0xa4,0x24,0x80,0xff,0xff,0xff,
        0x9c,0x50,0x51,0x52,0x53,0x55,0x56,0x57,
        0x41,0x50,0x41,0x51,0x41,0x52,0x41,0x53,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,
        0x48,0x89,0xe3,0x48,0x83,0xe4,0xf0,0xfc, /* aligned SysV call, original rsp in saved rbx */
        0x48,0xbf
    };
    memcpy(out, save, sizeof(save));
    unsigned n = sizeof(save);
    memcpy(out + n, &trace, 8); n += 8;
    out[n++] = 0xbe;
    memcpy(out + n, &exit, 4); n += 4;
    /* rbx points to the saved registers, not the guest object. Entry uses saved rdi;
     * exit uses saved rbx. r15d holds the query result at the common exit. */
    const unsigned char args[] = {0x48,0x89,0xea,0x48,0x8b,0x4b,(unsigned char)(exit ? 0x58 : 0x40)};
    memcpy(out + n, args, sizeof(args)); n += sizeof(args);
    if (!exit) {
        const unsigned char pointers[] = {0x4c,0x8b,0x43,0x48,0x44,0x8b,0x4b,0x60}; /* r8=request, r9d=query flags */
        memcpy(out + n, pointers, sizeof(pointers)); n += sizeof(pointers);
    } else if (exit >= SPARTAN_CONTACT_PHASES) {
        const unsigned char branch[] = {0x44,0x8b,0x43,0x68,0x4c,0x8b,0x4b,0x78}; /* r8d=saved ecx, r9=saved flags */
        memcpy(out + n, branch, sizeof(branch)); n += sizeof(branch);
    } else if (exit == 1) {
        const unsigned char zero[] = {0x45,0x89,0xf8,0x45,0x31,0xc9}; /* r8d=result, r9d=0 */
        memcpy(out + n, zero, sizeof(zero)); n += sizeof(zero);
    } else {
        const unsigned char mask[] = {0x44,0x8b,0x43,0x70,0x45,0x31,0xc9}; /* r8d=saved eax, r9d=0 */
        memcpy(out + n, mask, sizeof(mask)); n += sizeof(mask);
    }
    out[n++] = 0x48; out[n++] = 0xb8;
    memcpy(out + n, &helper, 8); n += 8;
    static const unsigned char restore[] = {
        0xff,0xd0,0x48,0x89,0xdc,
        0x41,0x5f,0x41,0x5e,0x41,0x5d,0x41,0x5c,0x41,0x5b,0x41,0x5a,0x41,0x59,0x41,0x58,
        0x5f,0x5e,0x5d,0x5b,0x5a,0x59,0x58,0x9d,
        0x48,0x8d,0xa4,0x24,0x80,0,0,0
    };
    memcpy(out + n, restore, sizeof(restore));
    return n + sizeof(restore);
}
static inline unsigned spartanport_collision_stub(unsigned char *out, uintptr_t trace, uintptr_t helper,
                                                   unsigned exit, uintptr_t resume, uintptr_t operand) {
    unsigned n = spartanport_collision_prefix(out, trace, helper, exit);
    if (!n || !resume || (exit == 1 && !operand)) return 0;
    const unsigned length = spartanport_collision_length(exit);
    if (exit == 1) {
        out[n++] = 0x48; out[n++] = 0xb8;
        memcpy(out + n, &operand, 8); n += 8;
        const unsigned char load[] = {0x48,0x8b,0x00};
        memcpy(out + n, load, sizeof(load)); n += sizeof(load);
    } else {
        memcpy(out + n, spartanport_collision_original[exit], length); n += length;
    }
    const unsigned char jump[] = {0xff,0x25,0,0,0,0};
    memcpy(out + n, jump, sizeof(jump)); n += sizeof(jump);
    memcpy(out + n, &resume, 8);
    return n + 8;
}

/* Replaces a 6-byte jcc: the helper reads the saved flags, then a short jcc replays the jump. */
static inline unsigned spartanport_branch_stub(unsigned char *out, uintptr_t trace, uintptr_t helper,
                                               unsigned branch, uintptr_t taken, uintptr_t fallthrough) {
    if (branch >= SPARTAN_CONTACT_BRANCHES || !taken || !fallthrough) return 0;
    unsigned n = spartanport_collision_prefix(out, trace, helper, SPARTAN_CONTACT_PHASES + branch);
    if (!n) return 0;
    out[n++] = (unsigned char)(0x70 | (spartanport_contact_branches[branch].condition & 0x0f));
    out[n++] = 14;
    const unsigned char jump[] = {0xff,0x25,0,0,0,0};
    memcpy(out + n, jump, sizeof(jump)); n += sizeof(jump);
    memcpy(out + n, &fallthrough, 8); n += 8;
    memcpy(out + n, jump, sizeof(jump)); n += sizeof(jump);
    memcpy(out + n, &taken, 8);
    return n + 8;
}

/* Wall seam fix (experimental). The ledge ray at image+0x5bfb2 misses through a ~0.015-unit seam
 * in the Realm of Hades wall: 60 FPS steps (~0.021) often skip it, 240 FPS steps (~0.005) always
 * land in it. A miss within SPARTAN_LEDGE_REUSE of the last real hit reuses that hit. The launcher
 * patch swaps 31 c9 for 33 c9 (the same xor ecx, ecx) after the call to opt in. */
#define SPARTAN_LEDGE_REUSE 0.05f
enum { SPARTAN_LEDGE_SLOT = SPARTAN_TRACE_POINTS + 2, SPARTAN_LEDGE_HELPER = SPARTAN_TRACE_POINTS + 3,
       SPARTAN_LEDGE_RESULT_WORDS = 24 };
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

static inline int spartanport_trace_due(uint64_t *last, uint64_t now) {
    uint64_t previous = __atomic_load_n(last, __ATOMIC_RELAXED);
    if (!previous) {
        __atomic_compare_exchange_n(last, &previous, now, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
        return 0;
    }
    if (now < previous || now - previous < 5000000) return 0;
    return __atomic_compare_exchange_n(last, &previous, now, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

typedef struct {
    const char *name;
    SpartanSignature site;
    unsigned input, destination;
} SpartanTracePoint;
/* Temporary F4.1 instrumentation. Swing observes the existing correction as a reference. */
static const SpartanTracePoint spartanport_trace_points[SPARTAN_TRACE_POINTS] = {
    {"angle", {"c5 e2 58 1d ?? ?? ?? ?? 48 8d 05 ?? ?? ?? ?? c5 fa 59 10", 8}, 0, 0},
    {"swing", {"c5 fa 11 85 98 fb ff ff 48 8d 05 ?? ?? ?? ?? c5 fa 10 00", 8}, 3, 0},
    {"move-init", {"c5 fa 11 83 50 01 00 00 48 8d 0d ?? ?? ?? ?? c5 ea 59 01", 8}, SPARTAN_TRACE_FRAME_CONTEXT, 1},
    {"traversal-1", {"48 8d 05 ?? ?? ?? ?? c5 f2 59 08 c5 fa 11 8d 0c fe ff ff", 0}, 1, 0},
    {"blend", {"48 8d 15 ?? ?? ?? ?? c5 fa 59 02 c5 f2 59 d0 c4 c1 78 2e d1", 0}, 0, 2},
    {"motion", {"c5 fa 11 85 7c ff ff ff 48 8d 05 ?? ?? ?? ?? c5 fa 59 08", 8}, 0, 0},
    {"steps", {"c4 c1 7a 10 07 48 8d 05 ?? ?? ?? ?? c5 fa 59 08", 5}, 0, 0},
    {"vector", {"c5 e0 59 c9 48 8d 05 ?? ?? ?? ?? c4 e2 79 18 18", 4}, 1, 0},
};

/* No calls or XMM writes. Saves stay below the guest's 128-byte red zone.
 * A busy slot drops the sample instead of stalling the game thread. */
static inline unsigned spartanport_trace_prefix(unsigned char *out, uintptr_t slot, uintptr_t rate, unsigned input) {
    if (!slot || !rate || input > SPARTAN_TRACE_FRAME_CONTEXT) return 0;
    static const unsigned char begin[] = {
        0x48,0x8d,0xa4,0x24,0x80,0xff,0xff,0xff, /* lea rsp, [rsp-128] */
        0x9c,0x50,0x51,0x52,                     /* flags, rax, rcx, rdx */
        0x48,0xba,0,0,0,0,0,0,0,0,              /* movabs rdx, slot */
        0xb8,1,0,0,0,0x87,0x02,                 /* xchg eax, [rdx]: try lock */
        0x85,0xc0,0x75,0,                       /* busy: branch to dropped */
        0x8b,0x4a,0x04,0x83,0xe1,0x3f,         /* count & 63 */
        0x48,0xc1,0xe1,0x05,                    /* 32-byte records */
        0x48,0x8d,0x4c,0x0a,0x10,              /* rcx = &records[index] */
    };
    memcpy(out, begin, sizeof(begin));
    memcpy(out + 14, &slot, 8);
    unsigned n = sizeof(begin);
    if (input == 3) {
        const unsigned char field[] = {0x41,0x8b,0x86,0x48,0x01,0,0,0x89,0x01}; /* [r14+0x148] */
        memcpy(out + n, field, sizeof(field)); n += sizeof(field);
    } else {
        const unsigned source = input == SPARTAN_TRACE_FRAME_CONTEXT ? 2 : input;
        out[n++] = 0x0f; out[n++] = 0x11; out[n++] = (unsigned char)(1 + source * 8); /* movups [rcx], xmmN */
    }
    if (input == SPARTAN_TRACE_FRAME_CONTEXT) {
        /* This frame-context builder has r14=object and a live rbp frame. Its scalar uses only
         * the first lane, so the object pointer replaces two unused lanes without growing records. */
        const unsigned char context[] = {
            0x41,0x0f,0xb7,0x86,0xc0,0x01,0,0, /* movzx eax, word [r14+0x1c0] */
            0x89,0x41,0x14,                     /* record.state */
            0x4c,0x89,0x71,0x08,               /* record.input[2..3] = r14 */
            0x48,0x8b,0x45,0x08,               /* caller = [rbp+8] */
            0x48,0x89,0x41,0x18,               /* record.caller */
        };
        memcpy(out + n, context, sizeof(context)); n += sizeof(context);
    }
    out[n++] = 0x48; out[n++] = 0xb8;
    memcpy(out + n, &rate, 8); n += 8;
    const unsigned char write[] = {
        0x8b,0x00,0x89,0x41,0x10,              /* record.rate = *rate */
        0xff,0x42,0x04,                         /* ++count */
        0xc7,0x02,0,0,0,0,                     /* release slot */
        0xeb,0x04,                              /* skip dropped */
    };
    memcpy(out + n, write, sizeof(write)); n += sizeof(write);
    out[32] = (unsigned char)(n - 33);
    const unsigned char end[] = {
        0xf0,0xff,0x42,0x08,                    /* lock inc dropped */
        0x5a,0x59,0x58,0x9d,
        0x48,0x8d,0xa4,0x24,0x80,0,0,0,       /* restore guest rsp */
    };
    memcpy(out + n, end, sizeof(end));
    return n + sizeof(end);
}
static inline int spartanport_trace_take(SpartanTraceSlot *slot, SpartanTraceSnapshot *out) {
    if (__atomic_exchange_n(&slot->lock, 1, __ATOMIC_ACQUIRE)) return 0;
    out->calls = slot->count;
    out->dropped = __atomic_exchange_n(&slot->dropped, 0, __ATOMIC_RELAXED);
    out->samples = slot->count < SPARTAN_TRACE_CAPACITY ? slot->count : SPARTAN_TRACE_CAPACITY;
    const uint32_t first = slot->count - out->samples;
    for (uint32_t i = 0; i < out->samples; ++i)
        out->records[i] = slot->records[(first + i) & (SPARTAN_TRACE_CAPACITY - 1)];
    slot->count = 0;
    __atomic_store_n(&slot->lock, 0, __ATOMIC_RELEASE);
    return 1;
}
static inline int spartanport_trace_match(const SpartanTracePoint *point, const unsigned char *code, uint64_t size) {
    return size >= 7 && code[0] == 0x48 && code[1] == 0x8d && code[2] == 5 + point->destination * 8;
}
static inline unsigned spartanport_trace_stub(unsigned char *out, const SpartanTracePoint *point,
                                            uintptr_t slot, uintptr_t rate, uintptr_t resume) {
    unsigned n = spartanport_trace_prefix(out, slot, rate, point->input);
    if (!n) return 0;
    out[n++] = 0x48; out[n++] = (unsigned char)(0xb8 + point->destination);
    memcpy(out + n, &rate, 8); n += 8; /* replay the LEA at its relocated target */
    const unsigned char jump[] = {0xff,0x25,0,0,0,0};
    memcpy(out + n, jump, sizeof(jump)); n += sizeof(jump);
    memcpy(out + n, &resume, 8);
    return n + 8;
}
#endif
