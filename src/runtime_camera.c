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
#include "gpu/spartanportgpu.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static SpartanCameraState camera_state;
static float camera_up[3] = {0, 1, 0};
static int camera_log;

/* The pivot: SPARTAN_FREE_CAMERA_DISTANCE (or the debug offset's third number) when given, a point
 * that far ahead; else Kratos (the player's object, src/camera_hook.h) while he is near the
 * camera; else the point the game's camera looks at, whose distance is the manager's +0xa4, read
 * whenever the controller writes the camera. */
static float camera_target_distance;
static int camera_fixed_distance;
static SpartanCameraState camera_orbit_state(void) {
    SpartanCameraState state = camera_state;
    if (!camera_fixed_distance) state.distance = camera_target_distance;
    return state;
}

/* Back to the game's framing (a cut, another camera); the debug offset stays. */
static int camera_debug;
static void camera_recenter(void) {
    if (!camera_debug) camera_state.yaw = camera_state.pitch = 0;
}

enum { CAMERA_PLAYER_REACH = 60 };  /* farther than this from the camera, the camera is not following him */
static const uint64_t *camera_player_global;
void spartanport_camera_set_player_global(uintptr_t address) { camera_player_global = (const uint64_t *)address; }

/* Kratos's position when the player's object is there and he is within reach of the camera. */
static int camera_player(const float *eye, float *position) {
    if (!camera_player_global || camera_fixed_distance) return 0;
    const uint64_t player = *camera_player_global;
    if (player < 0x10000 || player & 0xf) return 0;
    memcpy(position, (const unsigned char *)(uintptr_t)player + SPARTAN_CAMERA_PLAYER_POSITION, 3 * sizeof(float));
    const float d[3] = {position[0] - eye[0], position[1] - eye[1], position[2] - eye[2]};
    const float distance = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    return distance > 0.5f && distance < CAMERA_PLAYER_REACH;  /* also false for NaN */
}

/* The title screen is the Shell level (Kratos is there too, on his rock): from the game opening
 * SHELL.WAD until it opens a level of play, named by three letters and three digits (OLY150.WAD,
 * BAS400AI1CENT.WAD; HDS675SHELL.WAD belongs to the title) or CHALLENGE01.WAD and the like. Its
 * menu takes the sticks as they are and its camera stays the game's. runtime_file.c tells. */
static volatile int camera_title;
static int camera_upper(char c) { return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c; }
static int camera_name_has(const char *name, const char *word) {
    for (; *name; ++name) {
        size_t i = 0;
        while (word[i] && camera_upper(name[i]) == word[i]) ++i;
        if (!word[i]) return 1;
    }
    return 0;
}
void spartanport_camera_file_opened(const char *guest) {
    const char *slash = strrchr(guest, '/');
    const char *name = slash ? slash + 1 : guest;
    const size_t n = strlen(name);
    if (n < 5 || !camera_name_has(name + n - 4, ".WAD")) return;
    int title = -1;
    if (camera_name_has(name, "SHELL")) title = camera_name_has(name, "SHELL.WAD") && n == 9 ? 1 : -1;
    else if (camera_name_has(name, "CHALLENGE")) title = 0;
    else if (n >= 10) {
        int level = 1;
        for (int i = 0; i < 6; ++i) {
            const char c = (char)camera_upper(name[i]);
            if (i < 3 ? c < 'A' || c > 'Z' : c < '0' || c > '9') level = 0;
        }
        if (level) title = 0;
    }
    if (title < 0 || title == camera_title) return;
    camera_title = title;
    printf("Free camera: %s (%s)\n", title ? "title screen, the game's camera" : "in play", name);
    fflush(stdout);
}
int spartanport_camera_live(void) { return !camera_title; }

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
    const int numbers = debug ? sscanf(debug, "%f,%f,%f", &yaw, &pitch, &distance) : 0;
    if (numbers < 2) return 0;
    camera_debug = 1;
    if (numbers == 3) camera_fixed_distance = 1;
    camera_state.yaw = yaw * 0.017453292f;
    camera_state.pitch = pitch * 0.017453292f;
    camera_state.distance = distance;
    camera_state.enabled = 1;
    return 1;
}

/* The right stick turns the camera (runtime_pad.c) while the menu's or the launcher's Free camera is on
 * (free_camera in spartanport.ini, SPARTAN_FREE_CAMERA) and the hook is in.
 * SPARTAN_FREE_CAMERA_DISTANCE: the pivot's distance ahead of the camera, in game units (0: in
 * place); by default the camera turns around the point it looks at (Kratos in play).
 * SPARTAN_FREE_CAMERA_LIMITS=yaw,pitch: the most it turns from the game's camera, in degrees; a
 * yaw of 180 or more (the default) turns all the way around Kratos. */
enum { CAMERA_YAW_SPEED = 120, CAMERA_PITCH_SPEED = 80 };  /* degrees per second at full tilt */
static const float CAMERA_RECENTER_SECONDS = 0.3f;
static int camera_stick;
static float camera_limits[2] = {180, 25};

int spartanport_camera_stick_enabled(void) { return camera_stick; }

/* The player's switch (G, a tap of R3: runtime_pad.c), for the boss fights: off, the camera goes
 * back to the game's and the right stick is the game's again. */
static int camera_off;
int spartanport_camera_on(void) { return spartanportgpu_free_camera() && !camera_off; }
/* The turn from the game's camera around the vertical axis, radians (positive: to the left). */
float spartanport_camera_yaw(void) { return camera_state.yaw; }
void spartanport_camera_toggle(void) {
    if (!spartanportgpu_free_camera()) return;  /* off in the menu: nothing to switch */
    camera_off = !camera_off;
    printf("Free camera: %s\n", camera_off ? "off (the game's camera)" : "on");
    fflush(stdout);
}

static void camera_configure_stick(void) {
    camera_stick = 1;
    const char *distance = getenv("SPARTAN_FREE_CAMERA_DISTANCE");
    if (distance) {
        camera_state.distance = strtof(distance, NULL);
        camera_fixed_distance = 1;
    }
    const char *limits = getenv("SPARTAN_FREE_CAMERA_LIMITS");
    float yaw, pitch;
    if (limits && sscanf(limits, "%f,%f", &yaw, &pitch) == 2 && yaw >= 0 && pitch >= 0) {
        camera_limits[0] = yaw;
        camera_limits[1] = pitch;
    }
    camera_state.enabled = 1;
}

/* Within the limit; with no limit (180 degrees or more) wrapped into -180..180 instead, so the
 * camera goes on around and a return to the game's camera takes the short way. */
static float clamp_angle(float value, float limit_degrees) {
    const float pi = 3.14159265f;
    if (limit_degrees >= 180) return value > pi ? value - 2 * pi : value < -pi ? value + 2 * pi : value;
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

/* The mouse turns the camera by an angle, not a speed: right turns it right, down looks down. */
void spartanport_camera_look(float right_degrees, float down_degrees) {
    camera_state.yaw = clamp_angle(camera_state.yaw - right_degrees * 0.017453292f, camera_limits[0]);
    camera_state.pitch = clamp_angle(camera_state.pitch - down_degrees * 0.017453292f, camera_limits[1]);
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
    float kratos[3] = {0, 0, 0};
    const float *eye = (const float *)(render_view + SPARTAN_CAMERA_WORLD) + 12;
    const int pivot = camera_player(eye, kratos);
    const float to[3] = {kratos[0] - eye[0], kratos[1] - eye[1], kratos[2] - eye[2]};
    printf("Free camera: views in the last %d s (yaw %.1f pitch %.1f focus %.1f, Kratos %.1f %.1f %.1f %.1f away%s)\n",
           CAMERA_LOG_SECONDS, camera_state.yaw * 57.29578f, camera_state.pitch * 57.29578f,
           camera_orbit_state().distance, kratos[0], kratos[1], kratos[2],
           sqrtf(to[0] * to[0] + to[1] * to[1] + to[2] * to[2]), pivot ? ", the pivot" : "");
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

/* SPARTAN_FREE_CAMERA_SCAN=1 (reverse engineering): looks for the camera's target (Kratos) in the
 * objects the camera manager points at, two levels deep. Every SCAN_SECONDS a host thread reads
 * them (ReadProcessMemory: a stale pointer fails instead of faulting) and counts every place
 * (manager +a -> +b -> +c) holding three floats within SCAN_RADIUS of the point the game's camera
 * looks at; the places that keep matching while Kratos moves are listed. */
enum { SCAN_SECONDS = 4, SCAN_OBJECT = 0x600, SCAN_POINTERS = 0x200, SCAN_PATHS = 512 };
static const float SCAN_RADIUS = 8.0f;
static struct { uint16_t offsets[3]; unsigned char depth; uint32_t hits; float distance; } scan_paths[SCAN_PATHS];
static unsigned scan_count, scan_rounds;

static int scan_read(uint64_t address, void *out, size_t size) {
    SIZE_T done = 0;
    return address > 0x10000 && ReadProcessMemory(GetCurrentProcess(), (const void *)(uintptr_t)address, out, size, &done) &&
           done == size;
}

static void scan_note(const uint16_t *offsets, unsigned depth, float distance) {
    for (unsigned i = 0; i < scan_count; ++i) {
        if (scan_paths[i].depth != depth || memcmp(scan_paths[i].offsets, offsets, depth * sizeof(*offsets))) continue;
        ++scan_paths[i].hits;
        scan_paths[i].distance = distance;
        return;
    }
    if (scan_count == SCAN_PATHS) return;
    memcpy(scan_paths[scan_count].offsets, offsets, depth * sizeof(*offsets));
    scan_paths[scan_count].depth = (unsigned char)depth;
    scan_paths[scan_count].hits = 1;
    scan_paths[scan_count].distance = distance;
    ++scan_count;
}

/* Float triples near the focus in one object; pointers go one level deeper while depth allows (all
 * of the manager's, the first SCAN_POINTERS bytes of the objects it points at). */
static void scan_object(uint64_t object, uint16_t *offsets, unsigned depth, const float *focus) {
    static unsigned char buffers[3][SCAN_OBJECT];
    unsigned char *bytes = buffers[depth - 1];
    if (!scan_read(object, bytes, SCAN_OBJECT)) return;
    for (unsigned at = 0; at + 12 <= SCAN_OBJECT; at += 4) {
        float v[3];
        memcpy(v, bytes + at, sizeof(v));
        const float dx = v[0] - focus[0], dy = v[1] - focus[1], dz = v[2] - focus[2];
        const float d = sqrtf(dx * dx + dy * dy + dz * dz);
        if (d < SCAN_RADIUS) {
            offsets[depth - 1] = (uint16_t)at;
            scan_note(offsets, depth, d);
        }
    }
    if (depth == 3) return;
    unsigned char copy[SCAN_OBJECT];
    const unsigned span = depth == 1 ? SCAN_OBJECT : SCAN_POINTERS;
    memcpy(copy, bytes, span);
    for (unsigned at = 0; at < span; at += 8) {
        uint64_t pointer;
        memcpy(&pointer, copy + at, 8);
        if (pointer < 0x100000000ull || pointer > 0x7fffffffffffull || pointer & 7) continue;
        offsets[depth - 1] = (uint16_t)at;
        scan_object(pointer, offsets, depth + 1, focus);
    }
}

static volatile uint64_t scan_manager;
static float scan_focus[3];
static DWORD WINAPI scan_thread(void *unused) {
    (void)unused;
    for (;;) {
        Sleep(SCAN_SECONDS * 1000);
        const uint64_t manager = scan_manager;
        float focus[3];
        memcpy(focus, scan_focus, sizeof(focus));
        if (!manager) continue;
        uint16_t offsets[3] = {0};
        scan_object(manager, offsets, 1, focus);
        ++scan_rounds;
        printf("Free camera: target scan %u, focus %.2f %.2f %.2f, places matched (manager path: hits, last distance)\n",
               scan_rounds, focus[0], focus[1], focus[2]);
        unsigned listed = 0;
        for (uint32_t floor = scan_rounds; floor && listed < 40; --floor) {
            for (unsigned i = 0; i < scan_count && listed < 40; ++i) {
                if (scan_paths[i].hits != floor) continue;
                ++listed;
                printf("  +%#x", scan_paths[i].offsets[0]);
                for (unsigned d = 1; d < scan_paths[i].depth; ++d) printf(" -> +%#x", scan_paths[i].offsets[d]);
                printf(": %u, %.2f\n", (unsigned)scan_paths[i].hits, scan_paths[i].distance);
            }
        }
        fflush(stdout);
    }
    return 0;
}

/* The manager and the point its camera looks at, from the controller's stub. */
static void camera_scan(uint64_t manager, const float *focus) {
    static int checked, wanted;
    if (!checked) {
        const char *scan = getenv("SPARTAN_FREE_CAMERA_SCAN");
        wanted = scan && *scan && strcmp(scan, "0");
        checked = 1;
    }
    if (!wanted) return;
    memcpy(scan_focus, focus, sizeof(scan_focus));
    if (!scan_manager) {
        scan_manager = manager;
        HANDLE thread = CreateThread(NULL, 0, scan_thread, NULL, 0, NULL);
        if (thread) CloseHandle(thread);
    }
    scan_manager = manager;
}

/* SPARTAN_FREE_CAMERA_FIND=1 (reverse engineering): looks for Kratos's position in the whole guest
 * memory. A first pass keeps every 16-byte aligned float triple in front of the camera (along its
 * axis FIND_NEAR..FIND_FAR, within FIND_WIDTH of it); later passes, every FIND_SECONDS, keep only
 * those still in front of it, and the ones that also moved are listed, each with the globals of the
 * image that point into the FIND_BACK bytes before it (a stable way back to the object). */
enum { FIND_SECONDS = 3, FIND_CANDIDATES = 1 << 20, FIND_LISTED = 24, FIND_BACK = 0x1000, FIND_CHUNK = 1 << 20 };
static const float FIND_NEAR = 1, FIND_FAR = 40, FIND_WIDTH = 3;
static struct { uint64_t address; float first[3]; } *find_candidates;
static unsigned find_count, find_round;
static float find_eye[3], find_forward[3];
static uint64_t find_image, find_image_size;
static unsigned char find_chunk[FIND_CHUNK];

void spartanport_camera_set_image(uintptr_t image, uint64_t size) { find_image = image; find_image_size = size; }

/* Distance along the camera's axis and away from it. */
static int find_in_front(const float *v, const float *eye, const float *forward, float *along, float *away) {
    const float r[3] = {v[0] - eye[0], v[1] - eye[1], v[2] - eye[2]};
    *along = r[0] * forward[0] + r[1] * forward[1] + r[2] * forward[2];
    if (!(*along >= FIND_NEAR && *along <= FIND_FAR)) return 0;
    const float p[3] = {r[0] - forward[0] * *along, r[1] - forward[1] * *along, r[2] - forward[2] * *along};
    *away = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
    return *away < FIND_WIDTH;
}

/* The guest's CPU heap, the first direct memory mapping (the GPU's comes after it). Pages the GPU
 * tracks are write-protected, so any readable page counts. */
static const uint64_t FIND_HEAP = 0x1000000000ull, FIND_HEAP_END = 0x10b0000000ull;
static uint64_t find_bytes;
static void find_first_pass(const float *eye, const float *forward) {
    find_count = 0;
    find_bytes = 0;
    MEMORY_BASIC_INFORMATION region;
    for (uint64_t at = FIND_HEAP; at < FIND_HEAP_END && find_count < FIND_CANDIDATES;
         at = (uint64_t)(uintptr_t)region.BaseAddress + region.RegionSize) {
        if (!VirtualQuery((const void *)(uintptr_t)at, &region, sizeof(region))) break;
        const DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                               PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        if (region.State != MEM_COMMIT || !(region.Protect & readable) || region.Protect & PAGE_GUARD) continue;
        const uint64_t start = (uint64_t)(uintptr_t)region.BaseAddress, end = start + region.RegionSize;
        for (uint64_t chunk = start; chunk < end && find_count < FIND_CANDIDATES; chunk += FIND_CHUNK) {
            const size_t size = end - chunk < FIND_CHUNK ? (size_t)(end - chunk) : FIND_CHUNK;
            if (!scan_read(chunk, find_chunk, size)) continue;
            find_bytes += size;
            for (size_t i = 0; i + 12 <= size && find_count < FIND_CANDIDATES; i += 16) {
                float v[3], along, away;
                memcpy(v, find_chunk + i, sizeof(v));
                if (!find_in_front(v, eye, forward, &along, &away)) continue;
                find_candidates[find_count].address = chunk + i;
                memcpy(find_candidates[find_count].first, v, sizeof(v));
                ++find_count;
            }
        }
    }
}

static void find_report(const float *eye, const float *forward) {
    unsigned listed = 0;
    for (unsigned i = 0; i < find_count && listed < FIND_LISTED; ++i) {
        float v[3], along, away;
        if (!scan_read(find_candidates[i].address, v, sizeof(v))) continue;
        const float dx = v[0] - find_candidates[i].first[0], dy = v[1] - find_candidates[i].first[1],
                    dz = v[2] - find_candidates[i].first[2];
        const float moved = sqrtf(dx * dx + dy * dy + dz * dz);
        if (moved < 2 || !find_in_front(v, eye, forward, &along, &away)) continue;
        ++listed;
        printf("  %#llx: %.2f %.2f %.2f (ahead %.2f, off axis %.2f, moved %.2f)", (unsigned long long)find_candidates[i].address,
               v[0], v[1], v[2], along, away, moved);
        /* Globals of the image pointing into the object before it. */
        unsigned globals = 0;
        for (uint64_t at = 0; at < find_image_size && globals < 3; at += FIND_CHUNK) {
            const size_t size = find_image_size - at < FIND_CHUNK ? (size_t)(find_image_size - at) : FIND_CHUNK;
            if (!scan_read(find_image + at, find_chunk, size)) continue;
            for (size_t j = 0; j + 8 <= size && globals < 3; j += 8) {
                uint64_t pointer;
                memcpy(&pointer, find_chunk + j, 8);
                if (pointer > find_candidates[i].address || find_candidates[i].address - pointer >= FIND_BACK) continue;
                printf(" [image %#llx -> +%#llx]", (unsigned long long)(at + j),
                       (unsigned long long)(find_candidates[i].address - pointer));
                ++globals;
            }
        }
        printf("\n");
    }
}

static DWORD WINAPI find_thread(void *unused) {
    (void)unused;
    find_candidates = malloc(sizeof(*find_candidates) * FIND_CANDIDATES);
    if (!find_candidates) return 0;
    for (;;) {
        Sleep(FIND_SECONDS * 1000);
        float eye[3], forward[3];
        memcpy(eye, find_eye, sizeof(eye));
        memcpy(forward, find_forward, sizeof(forward));
        /* A new scene (the camera jumped): what was in front of the old camera means nothing. */
        static float first_eye[3];
        const float jump[3] = {eye[0] - first_eye[0], eye[1] - first_eye[1], eye[2] - first_eye[2]};
        const int jumped = sqrtf(jump[0] * jump[0] + jump[1] * jump[1] + jump[2] * jump[2]) > 200;
        if (!find_count || (jumped && find_round)) {
            memcpy(first_eye, eye, sizeof(first_eye));
            find_first_pass(eye, forward);
            find_round = 1;
            printf("Free camera: find, first pass: %u places in front of the camera in %llu MB (eye %.1f %.1f %.1f)\n",
                   find_count, (unsigned long long)(find_bytes >> 20), eye[0], eye[1], eye[2]);
            fflush(stdout);
            continue;
        }
        unsigned kept = 0;
        for (unsigned i = 0; i < find_count; ++i) {
            float v[3], along, away;
            if (!scan_read(find_candidates[i].address, v, sizeof(v)) || !find_in_front(v, eye, forward, &along, &away))
                continue;
            find_candidates[kept++] = find_candidates[i];
        }
        find_count = kept;
        ++find_round;
        printf("Free camera: find, pass %u: %u places still in front of the camera (eye %.1f %.1f %.1f)\n", find_round,
               find_count, eye[0], eye[1], eye[2]);
        if (find_count <= 4096) find_report(eye, forward);
        fflush(stdout);
    }
    return 0;
}

/* The game's camera, from the controller's stub: its position and the way it looks. */
static void camera_find(const float *game) {
    static int checked, wanted, started;
    if (!checked) {
        const char *find = getenv("SPARTAN_FREE_CAMERA_FIND");
        wanted = find && *find && strcmp(find, "0");
        checked = 1;
    }
    if (!wanted) return;
    const float length = sqrtf(game[8] * game[8] + game[9] * game[9] + game[10] * game[10]);
    if (!(length > 0)) return;
    for (int i = 0; i < 3; ++i) {
        find_eye[i] = game[12 + i];
        find_forward[i] = -game[8 + i] / length;  /* the gameplay camera looks down its third axis */
    }
    if (!started) {
        started = 1;
        HANDLE thread = CreateThread(NULL, 0, find_thread, NULL, 0, NULL);
        if (thread) CloseHandle(thread);
    }
}
#else
static void camera_watch(uint64_t node) { (void)node; }
static void camera_watch_report(void) {}
static void camera_scan(uint64_t manager, const float *focus) { (void)manager; (void)focus; }
static void camera_find(const float *game) { (void)game; }
void spartanport_camera_set_image(uintptr_t image, uint64_t size) { (void)image; (void)size; }
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
    if ((camera_state.yaw != 0 || camera_state.pitch != 0) && spartanport_camera_live()) {
        const SpartanCameraState state = camera_orbit_state();
        float kratos[3];
        if (camera_player(world + 12, kratos)) spartanport_camera_orbit_about(world, kratos, &state, camera_up);
        else spartanport_camera_orbit_world(world, camera_sign, &state, camera_up);
    }
    unsigned char *matrix = node + SPARTAN_CAMERA_NODE_MATRIX;
    if (memcmp(matrix, world, sizeof(camera_clean))) {
        memcpy(matrix, world, sizeof(camera_clean));
        memset(node + SPARTAN_CAMERA_NODE_STALE, 0, 8);
        node[SPARTAN_CAMERA_NODE_FLAGS] |= 1;
    }
    memcpy(camera_written, world, sizeof(camera_written));
    camera_has_written = 1;
}

/* The game's camera has a new matrix. A cut (it jumped CAMERA_CUT_DISTANCE or turned more than
 * CAMERA_CUT_DEGREES at once: a new scene, a scripted camera) starts again from the game's framing. */
enum { CAMERA_CUT_DISTANCE = 4, CAMERA_CUT_DEGREES = 30 };
static int camera_has_clean;
static void camera_take_clean(const unsigned char *matrix) {
    float next[16];
    memcpy(next, matrix, sizeof(next));
    if (camera_has_clean) {
        const float d[3] = {next[12] - camera_clean[12], next[13] - camera_clean[13], next[14] - camera_clean[14]};
        const float a = sqrtf(next[8] * next[8] + next[9] * next[9] + next[10] * next[10]);
        const float b = sqrtf(camera_clean[8] * camera_clean[8] + camera_clean[9] * camera_clean[9] +
                              camera_clean[10] * camera_clean[10]);
        const float turn = (next[8] * camera_clean[8] + next[9] * camera_clean[9] + next[10] * camera_clean[10]) / (a * b);
        if (sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) > CAMERA_CUT_DISTANCE ||
            !(turn > cosf(CAMERA_CUT_DEGREES * 0.017453292f)))
            camera_recenter();
    }
    memcpy(camera_clean, next, sizeof(camera_clean));
    camera_has_clean = 1;
}

/* Called by the controller's stub after it wrote a camera node: the gameplay camera's new matrix
 * is the clean one, and the node is turned at once, before anything reads it. */
static volatile unsigned camera_writes;
/* How many times the game's controller wrote the gameplay camera: it stops while the game is paused. */
unsigned spartanport_camera_writes(void) { return camera_writes; }

ABI void spartanport_camera_node_written(unsigned char *node, const unsigned char *manager) {
    if ((uint64_t)(uintptr_t)node == camera_node) ++camera_writes;
    float distance;
    memcpy(&distance, manager + SPARTAN_CAMERA_MANAGER_DISTANCE, sizeof(distance));
    if ((uint64_t)(uintptr_t)node == camera_node) {
        float game[16], focus[3];  /* the point the game's camera looks at, as 0x3f214 puts its target */
        memcpy(game, node + SPARTAN_CAMERA_NODE_MATRIX, sizeof(game));
        for (int i = 0; i < 3; ++i) focus[i] = game[12 + i] - game[8 + i] * distance;
        camera_scan((uint64_t)(uintptr_t)manager, focus);
        camera_find(game);
    }
    if (!camera_state.enabled || (uint64_t)(uintptr_t)node != camera_node || !camera_node_free(node)) return;
    if (distance > 0 && distance < 100000) camera_target_distance = distance;  /* also false for NaN */
    camera_take_clean(node + SPARTAN_CAMERA_NODE_MATRIX);
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
    if (node != camera_node) {  /* another camera: start clean, from the game's framing */
        camera_node = node;
        camera_has_written = camera_has_clean = 0;
        camera_recenter();
    }
    camera_sign = spartanport_camera_sign(render_view);
    unsigned char *camera = (unsigned char *)(uintptr_t)node;
    if (!camera_node_free(camera)) {  /* a parented camera: the view alone, as before */
        const SpartanCameraState state = camera_orbit_state();
        if ((state.yaw != 0 || state.pitch != 0) && spartanport_camera_live())
            spartanport_camera_orbit(render_view, &state, camera_up);
        return;
    }
    const unsigned char *matrix = camera + SPARTAN_CAMERA_NODE_MATRIX;
    if (!camera_has_written || memcmp(matrix, camera_written, sizeof(camera_written))) camera_take_clean(matrix);
    float world[16];
    camera_turn_node(camera, world);
    spartanport_camera_set_view(render_view, world);
}
