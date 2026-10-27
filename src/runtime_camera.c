/* Free camera (src/camera_hook.h): the gameplay view's camera, orbited by the shared state before
 * the game derives the frame's matrices and culling from it.
 *
 * SPARTAN_FREE_CAMERA_DEBUG=yaw,pitch[,distance]: a fixed offset in degrees around a pivot
 * `distance` game units ahead of the camera (0, the default: in place), without any input.
 * SPARTAN_FREE_CAMERA_UP=x,y,z: the world's vertical axis (default 0,1,0).
 * SPARTAN_FREE_CAMERA_LOG=1: prints a table of the views the hook saw (which one it turns, their
 * cameras, positions and projections) every few seconds. */
#define _GNU_SOURCE
#include "runtime.h"
#include "camera_hook.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static SpartanCameraState camera_state;
static float camera_up[3] = {0, 1, 0};
static int camera_log;

uintptr_t spartanport_camera_state_address(void) { return (uintptr_t)&camera_state; }

static void camera_configure_stick(void);
static void camera_watch(uint64_t node);
static void camera_watch_report(void);

/* Reads the variables; 1 when the debug offset turned the camera on. */
int spartanport_camera_configure(void) {
    const char *up = getenv("SPARTAN_FREE_CAMERA_UP");
    float x, y, z;
    if (up && sscanf(up, "%f,%f,%f", &x, &y, &z) == 3) {
        const float length = sqrtf(x * x + y * y + z * z);
        if (length > 0) { camera_up[0] = x / length; camera_up[1] = y / length; camera_up[2] = z / length; }
    }
    const char *log = getenv("SPARTAN_FREE_CAMERA_LOG");
    camera_log = log && *log && strcmp(log, "0");
    camera_configure_stick();
    const char *debug = getenv("SPARTAN_FREE_CAMERA_DEBUG");
    float yaw, pitch, distance = camera_state.distance;
    if (!debug || sscanf(debug, "%f,%f,%f", &yaw, &pitch, &distance) < 2) return 0;
    camera_state.yaw = yaw * 0.017453292f;
    camera_state.pitch = pitch * 0.017453292f;
    camera_state.distance = distance;
    camera_state.enabled = 1;
    return 1;
}

/* The right stick turns the camera (runtime_pad.c): SPARTAN_FREE_CAMERA=1, and only with the hook in.
 * SPARTAN_FREE_CAMERA_DISTANCE: the pivot's distance ahead of the camera, in game units (0: in place).
 * SPARTAN_FREE_CAMERA_LIMITS=yaw,pitch: the most it turns from the game's camera, in degrees. */
enum { CAMERA_YAW_SPEED = 120, CAMERA_PITCH_SPEED = 80 };  /* degrees per second at full tilt */
static const float CAMERA_RECENTER_SECONDS = 0.3f;
static int camera_stick;
static float camera_limits[2] = {60, 25};

int spartanport_camera_stick_enabled(void) { return camera_stick; }

static void camera_configure_stick(void) {
    const char *on = getenv("SPARTAN_FREE_CAMERA");
    camera_stick = on && *on && strcmp(on, "0");
    if (!camera_stick) return;
    const char *distance = getenv("SPARTAN_FREE_CAMERA_DISTANCE");
    if (distance) camera_state.distance = strtof(distance, NULL);
    const char *limits = getenv("SPARTAN_FREE_CAMERA_LIMITS");
    float yaw, pitch;
    if (limits && sscanf(limits, "%f,%f", &yaw, &pitch) == 2 && yaw >= 0 && pitch >= 0) {
        camera_limits[0] = yaw;
        camera_limits[1] = pitch;
    }
    camera_state.enabled = 1;
}

static float clamp_angle(float value, float limit_degrees) {
    const float limit = limit_degrees * 0.017453292f;
    return value > limit ? limit : value < -limit ? -limit : value;
}

/* x, y: the stick in -1..1 (right, down), past its dead zone; seconds since the last read.
 * recenter: back to the game's camera over CAMERA_RECENTER_SECONDS (a stick prompt). */
void spartanport_camera_steer(float x, float y, float seconds, int recenter) {
    if (recenter) {
        const float keep = seconds >= CAMERA_RECENTER_SECONDS ? 0 : 1 - seconds / CAMERA_RECENTER_SECONDS;
        camera_state.yaw *= keep;
        camera_state.pitch *= keep;
        return;
    }
    /* Right turns the camera right (a negative turn around the vertical axis), up looks up. */
    camera_state.yaw = clamp_angle(camera_state.yaw - x * CAMERA_YAW_SPEED * 0.017453292f * seconds, camera_limits[0]);
    camera_state.pitch = clamp_angle(camera_state.pitch - y * CAMERA_PITCH_SPEED * 0.017453292f * seconds,
                                     camera_limits[1]);
}

/* The game's global that holds the view the camera stack attaches to (probe.c). */
static const uint64_t *camera_view_global;
void spartanport_camera_set_view_global(uintptr_t address) { camera_view_global = (const uint64_t *)address; }

/* The priority of the camera driving the view (the head of its list), or -1 without one. */
static int camera_priority(const unsigned char *render_view, uint64_t *node) {
    const uint64_t head = (uint64_t)(uintptr_t)(render_view + SPARTAN_CAMERA_LIST);
    const uint64_t first = *(const uint64_t *)(render_view + SPARTAN_CAMERA_LIST);
    if (!first || first == head) return -1;
    *node = *(const uint64_t *)(uintptr_t)(first + SPARTAN_CAMERA_LIST_NODE);
    return (int)*(const uint32_t *)(uintptr_t)(first + SPARTAN_CAMERA_LIST_PRIORITY);
}

/* SPARTAN_FREE_CAMERA_LOG: every view (and the caller that updated it) the hook saw in the last
 * CAMERA_LOG_SECONDS, as a table. */
enum { CAMERA_LOG_VIEWS = 48, CAMERA_LOG_SECONDS = 3 };
static struct {
    uint64_t view, caller, node;
    uint32_t updates;
    int priority, target;
    float position[3], projection, axis[3];
} camera_views[CAMERA_LOG_VIEWS];
static time_t camera_log_last;
static void camera_note(const unsigned char *render_view, uint64_t caller, int priority, uint64_t node, int target) {
    const uint64_t view = (uint64_t)(uintptr_t)render_view;
    for (int i = 0; i < CAMERA_LOG_VIEWS; ++i) {
        if (camera_views[i].view && (camera_views[i].view != view || camera_views[i].caller != caller)) continue;
        const float *w = (const float *)(render_view + SPARTAN_CAMERA_WORLD);
        camera_views[i].view = view;
        camera_views[i].caller = caller;
        camera_views[i].node = node;
        camera_views[i].priority = priority;
        camera_views[i].target = target;
        ++camera_views[i].updates;
        memcpy(camera_views[i].position, w + 12, sizeof(camera_views[i].position));
        memcpy(camera_views[i].axis, w + 8, sizeof(camera_views[i].axis));
        camera_views[i].projection = ((const float *)(render_view + SPARTAN_CAMERA_PROJECTION))[11];
        break;
    }
    const time_t now = time(NULL);
    if (now - camera_log_last < CAMERA_LOG_SECONDS) return;
    camera_log_last = now;
    const uint64_t global = camera_view_global ? *camera_view_global : 0;
    printf("Free camera: views in the last %d s (yaw %.1f pitch %.1f)\n", CAMERA_LOG_SECONDS,
           camera_state.yaw * 57.29578f, camera_state.pitch * 57.29578f);
    for (int i = 0; i < CAMERA_LOG_VIEWS && camera_views[i].view; ++i) {
        printf("  view %#llx%s%s caller %#llx updates %u camera %#llx priority %#x projection[2][3] %.2f "
               "position %.1f %.1f %.1f axis %.2f %.2f %.2f\n", (unsigned long long)camera_views[i].view,
               camera_views[i].view == global ? " (global)" : "", camera_views[i].target ? " (target)" : "",
               (unsigned long long)camera_views[i].caller, camera_views[i].updates,
               (unsigned long long)camera_views[i].node, camera_views[i].priority, camera_views[i].projection,
               camera_views[i].position[0], camera_views[i].position[1], camera_views[i].position[2],
               camera_views[i].axis[0], camera_views[i].axis[1], camera_views[i].axis[2]);
    }
    camera_watch_report();
    fflush(stdout);
    memset(camera_views, 0, sizeof(camera_views));
}

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
/* SPARTAN_FREE_CAMERA_WATCH=1 (reverse engineering): once the gameplay view is known, hardware
 * data breakpoints on its camera node (the local matrix at +0x40 and its position row +0x70, the
 * world matrix at +0x90 and its position row +0xc0, 8 bytes each, read or write) in every thread
 * of the process; each instruction that touched them is counted and listed with the view table.
 * The address printed is the instruction after the access (a data breakpoint traps after it). */
enum { WATCH_SITES = 64 };
static const unsigned watch_offsets[4] = {0x40, 0x70, 0x90, 0xc0};
static struct { uint64_t rip; unsigned slot; volatile LONG hits; } watch_sites[WATCH_SITES];
static volatile LONG watch_count;
static uint64_t watch_node;

static LONG CALLBACK watch_handler(EXCEPTION_POINTERS *info) {
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;
    CONTEXT *c = info->ContextRecord;
    const DWORD64 status = c->Dr6 & 0xf;
    if (!status) return EXCEPTION_CONTINUE_SEARCH;
    const unsigned slot = status & 1 ? 0 : status & 2 ? 1 : status & 4 ? 2 : 3;
    const uint64_t rip = c->Rip;
    int i = 0;
    for (; i < watch_count; ++i)
        if (watch_sites[i].rip == rip && watch_sites[i].slot == slot) break;
    if (i == watch_count && i < WATCH_SITES) {
        watch_sites[i].slot = slot;
        watch_sites[i].rip = rip;  /* a race between threads may list a site twice: harmless */
        InterlockedIncrement(&watch_count);
    }
    if (i < WATCH_SITES) InterlockedIncrement(&watch_sites[i].hits);
    c->Dr6 = 0;
    return EXCEPTION_CONTINUE_EXECUTION;
}

/* Sets the four breakpoints in every other thread (suspended while their context changes). */
static DWORD WINAPI watch_arm(void *unused) {
    (void)unused;
    const DWORD self = GetCurrentThreadId(), process = GetCurrentProcessId();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    THREADENTRY32 entry = {.dwSize = sizeof(entry)};
    unsigned armed = 0;
    for (BOOL more = Thread32First(snapshot, &entry); more; more = Thread32Next(snapshot, &entry)) {
        if (entry.th32OwnerProcessID != process || entry.th32ThreadID == self) continue;
        HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE,
                                   entry.th32ThreadID);
        if (!thread) continue;
        if (SuspendThread(thread) != (DWORD)-1) {
            CONTEXT c = {.ContextFlags = CONTEXT_DEBUG_REGISTERS};
            if (GetThreadContext(thread, &c)) {
                c.Dr0 = watch_node + watch_offsets[0];
                c.Dr1 = watch_node + watch_offsets[1];
                c.Dr2 = watch_node + watch_offsets[2];
                c.Dr3 = watch_node + watch_offsets[3];
                c.Dr6 = 0;
                /* L0-L3 on; each RW = 11 (read or write), LEN = 10 (8 bytes). */
                c.Dr7 = 0x55 | 0xbbbbull << 16;
                armed += SetThreadContext(thread, &c) != 0;
            }
            ResumeThread(thread);
        }
        CloseHandle(thread);
    }
    CloseHandle(snapshot);
    printf("Free camera: watching camera node %#llx in %u threads\n", (unsigned long long)watch_node, armed);
    fflush(stdout);
    return 0;
}

static void camera_watch(uint64_t node) {
    static int checked, wanted;
    if (!checked) {
        const char *watch = getenv("SPARTAN_FREE_CAMERA_WATCH");
        wanted = watch && *watch && strcmp(watch, "0");
        checked = 1;
    }
    if (!wanted || watch_node || !node || node & 7) return;
    watch_node = node;
    AddVectoredExceptionHandler(1, watch_handler);
    HANDLE thread = CreateThread(NULL, 0, watch_arm, NULL, 0, NULL);
    if (thread) CloseHandle(thread);
}

static void camera_watch_report(void) {
    if (!watch_node) return;
    printf("Free camera: camera node %#llx accesses (instruction after; +offset)\n", (unsigned long long)watch_node);
    for (int i = 0; i < watch_count && i < WATCH_SITES; ++i)
        printf("  %#llx +%#x hits %ld\n", (unsigned long long)watch_sites[i].rip, watch_offsets[watch_sites[i].slot],
               (long)watch_sites[i].hits);
}
#else
static void camera_watch(uint64_t node) { (void)node; }
static void camera_watch_report(void) {}
#endif

/* The gameplay camera's node and matrices. clean: the game's own matrix (what the controller last
 * wrote, or what the node held when the port first saw it); written: what the port left in the
 * node (clean, turned). A node still holding `written` was not moved by the game since. */
static uint64_t camera_node;
static float camera_sign = -1;
static float camera_clean[16], camera_written[16];
static int camera_has_written;

static int camera_node_free(const unsigned char *node) {
    int16_t parent;
    memcpy(&parent, node + SPARTAN_CAMERA_NODE_PARENT, sizeof(parent));
    return parent == -1;  /* no parent: the local matrix is the world one */
}

/* The clean matrix turned by the state into `world` and into the node, marked changed (its cached
 * view recomputed) when that differs from what it holds. */
static void camera_turn_node(unsigned char *node, float *world) {
    memcpy(world, camera_clean, sizeof(camera_clean));
    if (camera_state.yaw != 0 || camera_state.pitch != 0)
        spartanport_camera_orbit_world(world, camera_sign, &camera_state, camera_up);
    unsigned char *matrix = node + SPARTAN_CAMERA_NODE_MATRIX;
    if (memcmp(matrix, world, sizeof(camera_clean))) {
        memcpy(matrix, world, sizeof(camera_clean));
        memset(node + SPARTAN_CAMERA_NODE_STALE, 0, 8);
        node[SPARTAN_CAMERA_NODE_FLAGS] |= 1;
    }
    memcpy(camera_written, world, sizeof(camera_written));
    camera_has_written = 1;
}

/* Called by the controller's stub after it wrote a camera node: the gameplay camera's new matrix
 * is the clean one, and the node is turned at once, before anything reads it. */
ABI void spartanport_camera_node_written(unsigned char *node) {
    if (!camera_state.enabled || (uint64_t)(uintptr_t)node != camera_node || !camera_node_free(node)) return;
    memcpy(camera_clean, node + SPARTAN_CAMERA_NODE_MATRIX, sizeof(camera_clean));
    float world[16];
    camera_turn_node(node, world);
}

/* Called by the hook's stub on every view update (shadow maps, reflections, 2D, the game's camera).
 * The gameplay view: a perspective projection driven by a camera from the gameplay stack
 * (priority 0x200, docs/plans/2026-10-26-free-camera.md). Its node is turned here too, for the
 * frames the controller leaves it alone (the stick moved, the camera did not), and the view takes
 * the same matrix. */
ABI void spartanport_camera_apply(unsigned char *render_view, uint64_t caller) {
    uint64_t node = 0;
    const int priority = camera_priority(render_view, &node);
    const float projection = ((const float *)(render_view + SPARTAN_CAMERA_PROJECTION))[11];
    const int target = priority == 0x200 && projection != 0;
    if (camera_log) camera_note(render_view, caller, priority, node, target);
    if (!target) return;
    camera_watch(node);
    ++camera_state.heartbeat;
    if (!camera_state.enabled) return;
    if (node != camera_node) { camera_node = node; camera_has_written = 0; }  /* another camera: start clean */
    camera_sign = spartanport_camera_sign(render_view);
    unsigned char *camera = (unsigned char *)(uintptr_t)node;
    if (!camera_node_free(camera)) {  /* a parented camera: the view alone, as before */
        if (camera_state.yaw != 0 || camera_state.pitch != 0) spartanport_camera_orbit(render_view, &camera_state, camera_up);
        return;
    }
    const unsigned char *matrix = camera + SPARTAN_CAMERA_NODE_MATRIX;
    if (!camera_has_written || memcmp(matrix, camera_written, sizeof(camera_written)))
        memcpy(camera_clean, matrix, sizeof(camera_clean));
    float world[16];
    camera_turn_node(camera, world);
    spartanport_camera_set_view(render_view, world);
}
