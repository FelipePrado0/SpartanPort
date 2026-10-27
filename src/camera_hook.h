/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SPARTAN_CAMERA_HOOK_H
#define SPARTAN_CAMERA_HOOK_H
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "signature.h"

/* Free camera (docs/plans/2026-10-26-free-camera.md). Offsets as found in the v01.02 build (6e72);
 * builds are found by signature.
 *
 * 0x1cc470 updates a render view (rdi, kept in rbx) from the camera node at the head of its camera
 * list (+0x300, sorted by priority; 0x1cbc10 adds one): it copies the node's cached view matrix to
 * +0x00..+0x30 and its world matrix to +0x180..+0x1b0, then derives view x projection (+0x40),
 * the culling matrices (+0x80, +0x100) and the frustum (+0x240) from them. Shadow and reflection
 * views go through it too; the gameplay one is the view stored at the global 0x44ea50 (written
 * when the views are created, read by the camera stack that attaches the gameplay cameras with
 * priority 0x200; its address is read from that code's lea r15,[rip+disp]).
 *
 * The matrices are row-major with row vectors (p' = p * M): world rows are right, up, back/forward
 * and the position; the view is its rigid inverse. The projection is at +0xc0; its [2][3] says
 * which way the camera looks along its third axis.
 *
 * Site: the first instruction after both copies, vmovaps xmm8,[rbx+0x140] (8 bytes), where the
 * caller-saved general registers are dead and the stack is 16-byte aligned (five pushes after the
 * call). The stub calls the port (System V ABI) with the view, then reloads the four view rows the
 * code keeps in xmm5, xmm7, xmm13 and xmm12 before replaying the instruction.
 *
 * In play the view at that global is a 2D one (identity camera, orthographic projection), so the
 * port picks the gameplay view itself: the view's camera list (+0x300: next, prev, camera node at
 * +0x10, priority at +0x18) tells which camera drives it. */
enum { SPARTAN_CAMERA_LIST = 0x300, SPARTAN_CAMERA_LIST_NODE = 0x10, SPARTAN_CAMERA_LIST_PRIORITY = 0x18 };
enum { SPARTAN_CAMERA_HOOK_OFFSET = 0x1cc764, SPARTAN_CAMERA_HOOK_LENGTH = 8,
       SPARTAN_CAMERA_VIEW_GLOBAL = 0x44ea50,
       SPARTAN_CAMERA_GLOBAL_DISPLACEMENT = 3, SPARTAN_CAMERA_GLOBAL_NEXT = 7,
       SPARTAN_CAMERA_VIEW = 0x00, SPARTAN_CAMERA_PROJECTION = 0xc0, SPARTAN_CAMERA_WORLD = 0x180 };
static const unsigned char spartanport_camera_original[SPARTAN_CAMERA_HOOK_LENGTH] = {
    0xc5,0x78,0x28,0x83,0x40,0x01,0x00,0x00  /* vmovaps xmm8, [rbx+0x140] */
};
static const SpartanSignature spartanport_camera_site = {"c5 f8 29 83 b0 01 00 00 c5 78 28 83 40 01 00 00", 8};
/* lea r15, [rip+disp]: the gameplay view's global, in the camera stack's update. */
static const SpartanSignature spartanport_camera_view_site = {"4c 89 f7 e8 ?? ?? ?? ?? 4c 8d 3d ?? ?? ?? ?? 49 8b 0f", 8};

/* lea rax, [rip+disp] in the camera manager's update (0x3f273, one of 47 references): the player's
 * object, whose matrix is at +0x40..+0x70 like a scene node's, so Kratos's position is at +0x70.
 * Found in play by looking for what stays in front of the moving camera (SPARTAN_FREE_CAMERA_FIND). */
enum { SPARTAN_CAMERA_PLAYER_POSITION = 0x70 };
static const SpartanSignature spartanport_camera_player_site = {"81 e2 00 00 00 01 85 f2 74 45 48 8d 05 ?? ?? ?? ?? 48 8b 00", 10};

/* The global's image offset, from a 7-byte lea at its signature site. */
static inline uint64_t spartanport_camera_view_global(const unsigned char *code, uint64_t site) {
    int32_t displacement;
    memcpy(&displacement, code + SPARTAN_CAMERA_GLOBAL_DISPLACEMENT, 4);
    return site + SPARTAN_CAMERA_GLOBAL_NEXT + (int64_t)displacement;
}

static inline int spartanport_camera_signature_match(const unsigned char *code, uint64_t size) {
    return size >= SPARTAN_CAMERA_HOOK_LENGTH && !memcmp(code, spartanport_camera_original, SPARTAN_CAMERA_HOOK_LENGTH);
}

/* apply: void apply(unsigned char *view, uint64_t caller) (System V), called for every view; the
 * port picks the gameplay one. caller: the return address of the update's caller (above its five
 * pushes). */
enum { SPARTAN_CAMERA_APPLY_IMMEDIATE = 10 };
static inline unsigned spartanport_camera_build_stub(unsigned char *out, uintptr_t apply, uintptr_t resume) {
    static const unsigned char body[] = {
        0x48,0x89,0xdf,                           /* mov rdi, rbx */
        0x48,0x8b,0x74,0x24,0x28,                 /* mov rsi, [rsp+0x28] */
        0x48,0xb8, 0,0,0,0,0,0,0,0,               /* movabs rax, apply */
        0xff,0xd0,                                /* call rax */
        0xc5,0xf8,0x28,0x2b,                      /* vmovaps xmm5, [rbx] */
        0xc5,0xf8,0x28,0x7b,0x10,                 /* vmovaps xmm7, [rbx+0x10] */
        0xc5,0x78,0x28,0x6b,0x20,                 /* vmovaps xmm13, [rbx+0x20] */
        0xc5,0x78,0x28,0x63,0x30,                 /* vmovaps xmm12, [rbx+0x30] */
    };
    static const unsigned char jump[] = {0xff,0x25,0,0,0,0}; /* jmp [rip+0] */
    unsigned n = 0;
    memcpy(out, body, sizeof(body));
    memcpy(out + SPARTAN_CAMERA_APPLY_IMMEDIATE, &apply, 8);
    n += sizeof(body);
    memcpy(out + n, spartanport_camera_original, SPARTAN_CAMERA_HOOK_LENGTH);
    n += SPARTAN_CAMERA_HOOK_LENGTH;
    memcpy(out + n, jump, sizeof(jump));
    n += sizeof(jump);
    memcpy(out + n, &resume, 8);
    return n + 8;
}

/* The camera controller (0x3f490, called by 0x3f070 when the camera moves) writes the camera
 * node's local matrix (+0x40..+0x70; the gameplay camera has no parent, +0x80 == -1, so it is also
 * its world), then marks the node changed: a version at +0x88, the cached view at +0xf0 stale
 * (+0x130 = 0), moved (+0x140 bit 0). Many systems read the node directly (the scenery, culling,
 * distances), not the render view, so the free camera turns the node itself.
 * Site: right after the four stores, lea rdx,[rip+disp] (the version counter; 7 bytes) with the node
 * in rax, the stack 16-byte aligned (five pushes and 0x48 after rbp) and rdx, rcx written next. The
 * stub saves rax, calls the port with the node and the camera manager (rbx: 0x3f070 and 0x3f490 take
 * it in rdi; [manager+0x10] is the node, +0xa4 the distance to the point the camera looks at, the
 * one 0x3f214 puts the camera target at), and loads rdx with the counter's address. */
enum { SPARTAN_CAMERA_NODE_MATRIX = 0x40, SPARTAN_CAMERA_NODE_PARENT = 0x80, SPARTAN_CAMERA_NODE_STALE = 0x130,
       SPARTAN_CAMERA_NODE_FLAGS = 0x140, SPARTAN_CAMERA_CONTROLLER_LENGTH = 7,
       SPARTAN_CAMERA_CONTROLLER_DISPLACEMENT = 3, SPARTAN_CAMERA_NODE_IMMEDIATE = 10,
       SPARTAN_CAMERA_COUNTER_IMMEDIATE = 24, SPARTAN_CAMERA_MANAGER_DISTANCE = 0xa4 };
static const SpartanSignature spartanport_camera_controller_site = {
    "c5 f8 29 60 50 c5 f8 29 40 60 c5 f8 29 48 70 48 8d 15 ?? ?? ?? ?? 48 8b 0a 48 ff c1 48 89 0a", 15};

static inline int spartanport_camera_controller_match(const unsigned char *code, uint64_t size) {
    static const unsigned char lea[3] = {0x48,0x8d,0x15};
    return size >= SPARTAN_CAMERA_CONTROLLER_LENGTH && !memcmp(code, lea, sizeof(lea));
}

/* The counter the replaced lea pointed at (an image offset). */
static inline uint64_t spartanport_camera_controller_counter(const unsigned char *code, uint64_t site) {
    int32_t displacement;
    memcpy(&displacement, code + SPARTAN_CAMERA_CONTROLLER_DISPLACEMENT, 4);
    return site + SPARTAN_CAMERA_CONTROLLER_LENGTH + (int64_t)displacement;
}

/* node_written: void node_written(unsigned char *node, const unsigned char *manager) (System V);
 * counter: the replaced lea's target. */
static inline unsigned spartanport_camera_build_controller_stub(unsigned char *out, uintptr_t node_written,
                                                                uintptr_t counter, uintptr_t resume) {
    static const unsigned char body[] = {
        0x50, 0x50,                               /* push rax twice: rax kept, the stack still aligned */
        0x48,0x89,0xc7,                           /* mov rdi, rax */
        0x48,0x89,0xde,                           /* mov rsi, rbx: the manager */
        0x48,0xb8, 0,0,0,0,0,0,0,0,               /* movabs rax, node_written */
        0xff,0xd0,                                /* call rax */
        0x58, 0x58,                               /* pop rax twice */
        0x48,0xba, 0,0,0,0,0,0,0,0,               /* movabs rdx, counter: the lea */
        0xff,0x25,0,0,0,0,                        /* jmp [rip+0] */
    };
    memcpy(out, body, sizeof(body));
    memcpy(out + SPARTAN_CAMERA_NODE_IMMEDIATE, &node_written, 8);
    memcpy(out + SPARTAN_CAMERA_COUNTER_IMMEDIATE, &counter, 8);
    memcpy(out + sizeof(body), &resume, 8);
    return sizeof(body) + 8;
}

/* Shared by the port's apply function (game thread) and the input (runtime_pad.c). Angles in
 * radians: yaw turns the camera around the world's vertical axis, pitch around its own right
 * axis, both about a pivot `distance` units ahead of it (0: in place). */
typedef struct {
    float yaw, pitch, distance;
    uint32_t enabled;
    uint32_t heartbeat;  /* gameplay view updates */
    uint32_t pad;
} SpartanCameraState;

/* src/runtime_camera.c, for the pad (runtime_pad.c). */
#ifdef __cplusplus
extern "C" {
#endif
int spartanport_camera_stick_enabled(void);
int spartanport_camera_on(void);
void spartanport_camera_toggle(void);
void spartanport_camera_steer(float x, float y, float seconds, int recenter);
#ifdef __cplusplus
}
#endif

/* v * cos + (k x v) * sin + k (k . v) (1 - cos), around the unit axis k. */
static inline void spartanport_camera_rotate(float *v, const float *k, float c, float s) {
    const float dot = k[0] * v[0] + k[1] * v[1] + k[2] * v[2];
    const float cross[3] = {k[1] * v[2] - k[2] * v[1], k[2] * v[0] - k[0] * v[2], k[0] * v[1] - k[1] * v[0]};
    for (int i = 0; i < 3; ++i) v[i] = v[i] * c + cross[i] * s + k[i] * dot * (1 - c);
}

/* Orbits a camera's world matrix (16 floats, row vectors) in place by the state's angles around a
 * point of the world: yaw around the vertical axis through it, pitch around the camera's right
 * axis through it. up: the world's vertical axis (unit). */
static inline void spartanport_camera_orbit_about(float *world, const float *pivot, const SpartanCameraState *state,
                                                  const float *up) {
    float *position = world + 12;
    float offset[3];
    for (int i = 0; i < 3; ++i) offset[i] = position[i] - pivot[i];
    float right[3] = {world[0], world[1], world[2]};
    const float right_length = sqrtf(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
    if (!(right_length > 0)) return;
    for (int i = 0; i < 3; ++i) right[i] /= right_length;
    const float pc = cosf(state->pitch), ps = sinf(state->pitch), yc = cosf(state->yaw), ys = sinf(state->yaw);
    for (int row = 0; row < 3; ++row) {
        spartanport_camera_rotate(world + row * 4, right, pc, ps);
        spartanport_camera_rotate(world + row * 4, up, yc, ys);
    }
    spartanport_camera_rotate(offset, right, pc, ps);
    spartanport_camera_rotate(offset, up, yc, ys);
    for (int i = 0; i < 3; ++i) position[i] = pivot[i] + offset[i];
}

/* The same around the point `state->distance` ahead of the camera. sign: -1 when the camera looks
 * down its third axis (the projection's [2][3]), +1 when it looks up it. */
static inline void spartanport_camera_orbit_world(float *world, float sign, const SpartanCameraState *state,
                                                  const float *up) {
    const float forward[3] = {world[8] * sign, world[9] * sign, world[10] * sign};
    const float length = sqrtf(forward[0] * forward[0] + forward[1] * forward[1] + forward[2] * forward[2]);
    if (!(length > 0)) return;
    float pivot[3];
    for (int i = 0; i < 3; ++i) pivot[i] = world[12 + i] + forward[i] / length * state->distance;
    spartanport_camera_orbit_about(world, pivot, state, up);
}

/* view = [R^T 0; -p R^T 1], the rigid inverse of a world matrix. */
static inline void spartanport_camera_view_of(const float *world, float *view) {
    const float *position = world + 12;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) view[row * 4 + column] = world[column * 4 + row];
        view[row * 4 + 3] = 0;
    }
    for (int column = 0; column < 3; ++column)
        view[12 + column] = -(position[0] * world[column * 4] + position[1] * world[column * 4 + 1] +
                              position[2] * world[column * 4 + 2]);
    view[15] = 1;
}

/* The camera looks down its third axis when the projection's w takes +z, else up it. */
static inline float spartanport_camera_sign(const unsigned char *render_view) {
    float projection[16];
    memcpy(projection, render_view + SPARTAN_CAMERA_PROJECTION, sizeof(projection));
    return projection[11] < 0 ? -1.0f : 1.0f;
}

/* Writes a world matrix into the render view (view, world at the offsets above). */
static inline void spartanport_camera_set_view(unsigned char *render_view, const float *world) {
    float view[16];
    spartanport_camera_view_of(world, view);
    memcpy(render_view + SPARTAN_CAMERA_WORLD, world, 16 * sizeof(float));
    memcpy(render_view + SPARTAN_CAMERA_VIEW, view, sizeof(view));
}

/* Orbits the render view's camera by the state's angles and rewrites the view as the world's
 * rigid inverse. */
static inline void spartanport_camera_orbit(unsigned char *render_view, const SpartanCameraState *state, const float *up) {
    float world[16];
    memcpy(world, render_view + SPARTAN_CAMERA_WORLD, sizeof(world));
    spartanport_camera_orbit_world(world, spartanport_camera_sign(render_view), state, up);
    spartanport_camera_set_view(render_view, world);
}
#endif
