// Runs the free camera stub (src/camera_hook.h) as the game's render view update would reach it:
// every view calls the port, the view rows the code keeps in registers are reloaded from what the
// port wrote, and the replayed instruction loads xmm8. Then checks the orbit: the
// view stays the world's inverse, the camera keeps its distance to the pivot and faces it.
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <windows.h>
#include "../src/camera_hook.h"

// Where the stub resumes: the registers the game reads next, stored after the view.
extern "C" void camera_return();
asm(".globl camera_return\ncamera_return:\n"
    "    vmovaps %xmm5, 0x200(%rbx)\n    vmovaps %xmm7, 0x210(%rbx)\n"
    "    vmovaps %xmm13, 0x220(%rbx)\n    vmovaps %xmm12, 0x230(%rbx)\n"
    "    vmovaps %xmm8, 0x240(%rbx)\n    ret\n");

static int applied;
static uint64_t applied_caller;
extern "C" __attribute__((sysv_abi)) void camera_apply(unsigned char* view, uint64_t caller) {
    ++applied;
    applied_caller = caller;
    for (int i = 0; i < 16; ++i) reinterpret_cast<float*>(view)[i] = static_cast<float>(100 + i);
}

// rbx: the render view. The game reaches the site with the stack 16-byte aligned and the
// update's return address 0x28 above it (five pushes).
static void call_stub(unsigned char* stub, void* view, uint64_t caller) {
    asm volatile("sub $0x28, %%rsp\n    mov %2, 0x20(%%rsp)\n    call *%0\n    add $0x28, %%rsp"
                 : : "r"(stub), "b"(view), "r"(caller)
                 : "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "xmm0", "xmm1", "xmm2", "xmm3",
                   "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14",
                   "xmm15", "memory", "cc");
}

// The controller's site: rax holds the node, rbx the camera manager; where the stub resumes, rax
// and rdx are read back.
static unsigned char* written_node;
static const unsigned char* written_manager;
extern "C" __attribute__((sysv_abi)) void node_written(unsigned char* node, const unsigned char* manager) {
    written_node = node;
    written_manager = manager;
}
extern "C" void controller_return();
extern "C" uint64_t returned_rax, returned_rdx;
uint64_t returned_rax, returned_rdx;
asm(".globl controller_return\ncontroller_return:\n"
    "    mov %rax, returned_rax(%rip)\n    mov %rdx, returned_rdx(%rip)\n    ret\n");
static void call_controller(unsigned char* stub, void* node, const void* manager) {
    asm volatile("sub $8, %%rsp\n    call *%0\n    add $8, %%rsp"
                 : : "r"(stub), "a"(node), "b"(manager)
                 : "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4",
                   "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15",
                   "memory", "cc");
}

// src/runtime_camera.c, linked in; the menu's Free camera is on.
extern "C" int spartanportgpu_free_camera(void) { return 1; }
// The level's raycast, faked: it records the filter it was given and, when told to, hits halfway.
alignas(16) static unsigned char fake_filter[SPARTAN_CAMERA_RAY_FILTER_SIZE], fake_hit[SPARTAN_CAMERA_RAY_RESULT_SIZE];
static int fake_ray_calls;
static bool fake_ray_hits;
static float fake_wall_top = -1e9f;  // a low wall: ends below this height are hit at 10%
static uint32_t fake_kinds, fake_excluded, fake_flags;
typedef float FakeV4 __attribute__((vector_size(16)));
extern "C" __attribute__((sysv_abi)) int fake_ray(FakeV4 start, FakeV4 end) {
    ++fake_ray_calls;
    std::memcpy(&fake_kinds, fake_filter, 4);
    std::memcpy(&fake_excluded, fake_filter + 4, 4);
    std::memcpy(&fake_flags, fake_filter + 8, 4);
    const float share = end[1] < fake_wall_top ? 0.1f : 0.5f;
    if (!fake_ray_hits && !(end[1] < fake_wall_top)) return 0;
    const float point[3] = {start[0] + (end[0] - start[0]) * share, start[1] + (end[1] - start[1]) * share,
                            start[2] + (end[2] - start[2]) * share};
    std::memcpy(fake_hit, point, sizeof(point));
    return 1;
}
extern "C" void spartanport_camera_set_ray(uintptr_t function, uintptr_t filter, uintptr_t hit);
// No actor recorded (the watch tool's actor search stays idle).
extern "C" uintptr_t spartanport_actor_slots_address(void) { return 0; }
extern "C" {
int spartanport_camera_configure(void);
void spartanport_camera_set_player_global(uintptr_t address);
__attribute__((sysv_abi)) void spartanport_camera_apply(unsigned char* render_view, uint64_t caller);
__attribute__((sysv_abi)) void spartanport_camera_node_written(unsigned char* node, const unsigned char* manager);
}

static float dot(const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static bool close_to(float a, float b) { return std::fabs(a - b) < 1e-4f; }

// The view times the world is the identity.
static void check_inverse(const unsigned char* render_view) {
    const float* view = reinterpret_cast<const float*>(render_view + SPARTAN_CAMERA_VIEW);
    const float* world = reinterpret_cast<const float*>(render_view + SPARTAN_CAMERA_WORLD);
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
            float sum = 0;
            for (int k = 0; k < 4; ++k) sum += world[r * 4 + k] * view[k * 4 + c];
            assert(close_to(sum, r == c ? 1.0f : 0.0f));
        }
}

// A camera at (0, 2, 10) looking down -z (projection [2][3] = -1), the game's view filled in.
static void make_view(unsigned char* render_view) {
    std::memset(render_view, 0, 0x300);
    float* world = reinterpret_cast<float*>(render_view + SPARTAN_CAMERA_WORLD);
    float* view = reinterpret_cast<float*>(render_view + SPARTAN_CAMERA_VIEW);
    float* projection = reinterpret_cast<float*>(render_view + SPARTAN_CAMERA_PROJECTION);
    world[0] = world[5] = world[10] = world[15] = 1;
    world[13] = 2;
    world[14] = 10;
    view[0] = view[5] = view[10] = view[15] = 1;
    view[13] = -2;
    view[14] = -10;
    projection[11] = -1;
}

int main(int argc, char** argv) {
    auto* code = static_cast<unsigned char*>(
        VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    assert(code);
    assert(spartanport_camera_build_stub(code, reinterpret_cast<uintptr_t>(&camera_apply),
                                         reinterpret_cast<uintptr_t>(&camera_return)) < 128);
    FlushInstructionCache(GetCurrentProcess(), code, 4096);

    alignas(64) static unsigned char view[0x300];
    float* rows = reinterpret_cast<float*>(view);
    for (int i = 0; i < 16; ++i) rows[i] = static_cast<float>(i);
    for (int i = 0; i < 4; ++i) reinterpret_cast<float*>(view + 0x140)[i] = static_cast<float>(50 + i);

    // The port rewrites the view and the registers hold the new rows; the instruction is replayed.
    assert(rows[0] == 0);
    call_stub(code, view, 0x5cc4a0);
    assert(applied == 1 && applied_caller == 0x5cc4a0);
    const float* registers = reinterpret_cast<float*>(view + 0x200);
    for (int i = 0; i < 16; ++i) assert(registers[i] == static_cast<float>(100 + i));
    assert(reinterpret_cast<float*>(view + 0x240)[0] == 50);

    // Orbit in place: the position stays, the view is the new world's inverse.
    const float up[3] = {0, 1, 0};
    alignas(64) static unsigned char render_view[0x300];
    float* world = reinterpret_cast<float*>(render_view + SPARTAN_CAMERA_WORLD);
    make_view(render_view);
    SpartanCameraState state = {};
    state.yaw = 1.5707963f;  // a quarter turn
    spartanport_camera_orbit(render_view, &state, up);
    assert(close_to(world[12], 0) && close_to(world[13], 2) && close_to(world[14], 10));
    assert(close_to(world[8], 1) && close_to(world[10], 0));  // the third axis turned a quarter around y
    check_inverse(render_view);

    // Orbit around a pivot 10 units ahead, (0, 2, 0): same distance, still facing it.
    for (const float pitch : {0.0f, 0.5f, -0.4f}) {
        make_view(render_view);
        state.yaw = 0.7f;
        state.pitch = pitch;
        state.distance = 10;
        spartanport_camera_orbit(render_view, &state, up);
        const float to_pivot[3] = {0 - world[12], 2 - world[13], 0 - world[14]};
        assert(close_to(std::sqrt(dot(to_pivot, to_pivot)), 10));
        const float forward[3] = {-world[8], -world[9], -world[10]};
        assert(close_to(dot(forward, to_pivot), 10));
        assert(close_to(world[1], 0));  // the right axis stays level: no roll
        check_inverse(render_view);
    }
    // The floor: the camera at (0, 2, 10) looks at a pivot at (0, 0, 0); a pitch that would take it
    // below height 1 stops where it meets it, one that keeps it above stays, and when the game's own
    // camera is lower than the floor the pitch that leaves the camera higher of the two wins.
    {
        make_view(render_view);
        float pose[16];
        std::memcpy(pose, world, sizeof(pose));
        const float pivot[3] = {0, 0, 0};
        SpartanCameraState floor_state = {};
        for (const float pitch : {0.6f, -0.6f}) {
            floor_state.pitch = pitch;
            const float lower = spartanport_camera_orbit_height(pose, pivot, &floor_state, pitch, up);
            if (lower >= 1) {
                assert(spartanport_camera_floor_pitch(pose, pivot, &floor_state, up, 1) == pitch);
                continue;
            }
            const float held = spartanport_camera_floor_pitch(pose, pivot, &floor_state, up, 1);
            assert(std::fabs(held) < std::fabs(pitch) && held * pitch >= 0);
            assert(close_to(spartanport_camera_orbit_height(pose, pivot, &floor_state, held, up), 1));
            // The game's camera (height 2) under a floor at 3: stay with it rather than go lower.
            assert(spartanport_camera_floor_pitch(pose, pivot, &floor_state, up, 3) == 0);
        }
    }
    // Held motion blur: the previous view×projection is view * projection (row vectors).
    {
        alignas(64) static unsigned char blur_view[0x300];
        float* v = reinterpret_cast<float*>(blur_view + SPARTAN_CAMERA_VIEW);
        float* p = reinterpret_cast<float*>(blur_view + SPARTAN_CAMERA_PROJECTION);
        for (int i = 0; i < 16; ++i) v[i] = static_cast<float>(i + 1), p[i] = static_cast<float>((i * 7) % 5 - 2);
        spartanport_camera_hold_blur(blur_view);
        const float* previous = reinterpret_cast<float*>(blur_view + SPARTAN_CAMERA_PREVIOUS_VIEW_PROJECTION);
        for (int row = 0; row < 4; ++row)
            for (int column = 0; column < 4; ++column) {
                float expected = 0;
                for (int k = 0; k < 4; ++k) expected += v[row * 4 + k] * p[k * 4 + column];
                assert(close_to(previous[row * 4 + column], expected));
            }
        assert(reinterpret_cast<float*>(blur_view + SPARTAN_CAMERA_VIEW)[0] == 1);  // the view is left alone
    }
    // The controller's stub: the port gets the node and the manager, rax comes back, rdx holds the
    // counter.
    static uint64_t counter;
    alignas(64) static unsigned char node[0x160], manager[0x300];
    assert(spartanport_camera_build_controller_stub(code + 128, reinterpret_cast<uintptr_t>(&node_written),
                                                    reinterpret_cast<uintptr_t>(&counter),
                                                    reinterpret_cast<uintptr_t>(&controller_return)) < 128);
    FlushInstructionCache(GetCurrentProcess(), code, 4096);
    call_controller(code + 128, node, manager);
    assert(written_node == node && written_manager == manager && returned_rax == reinterpret_cast<uintptr_t>(node) &&
           returned_rdx == reinterpret_cast<uintptr_t>(&counter));

    // The port (src/runtime_camera.c) with the stick on: the node and the view take the turned
    // matrix, again and again without adding up, and the game's own one when the stick is back.
    _putenv_s("SPARTAN_FREE_CAMERA", "1");
    spartanport_camera_configure();
    alignas(64) static unsigned char gameplay[0x400];
    make_view(gameplay);
    const uint64_t head = reinterpret_cast<uintptr_t>(gameplay + 0x380);  // the list's one entry
    const uint64_t camera = reinterpret_cast<uintptr_t>(node);
    const uint32_t priority = 0x200;
    std::memcpy(gameplay + SPARTAN_CAMERA_LIST, &head, 8);
    std::memcpy(gameplay + 0x380, &head, 8);
    std::memcpy(gameplay + 0x380 + SPARTAN_CAMERA_LIST_NODE, &camera, 8);
    std::memcpy(gameplay + 0x380 + SPARTAN_CAMERA_LIST_PRIORITY, &priority, 4);
    float clean[16];
    std::memcpy(clean, gameplay + SPARTAN_CAMERA_WORLD, sizeof(clean));
    std::memcpy(node + SPARTAN_CAMERA_NODE_MATRIX, clean, sizeof(clean));
    const int16_t no_parent = -1;
    std::memcpy(node + SPARTAN_CAMERA_NODE_PARENT, &no_parent, 2);
    spartanport_camera_apply(gameplay, 0);  // a new camera: the game's framing
    assert(!std::memcmp(clean, node + SPARTAN_CAMERA_NODE_MATRIX, sizeof(clean)));
    spartanport_camera_steer(1, 0, 0.25f, 0);  // 30 degrees to the right
    spartanport_camera_apply(gameplay, 0);
    float turned[16];
    std::memcpy(turned, node + SPARTAN_CAMERA_NODE_MATRIX, sizeof(turned));
    assert(std::memcmp(turned, clean, sizeof(clean)) && node[SPARTAN_CAMERA_NODE_FLAGS] & 1);
    assert(!std::memcmp(turned, gameplay + SPARTAN_CAMERA_WORLD, sizeof(turned)));
    check_inverse(gameplay);
    assert(close_to(turned[12], 0) && close_to(turned[13], 2) && close_to(turned[14], 10));
    spartanport_camera_apply(gameplay, 0);  // the second update of the frame
    assert(!std::memcmp(turned, node + SPARTAN_CAMERA_NODE_MATRIX, sizeof(turned)));
    // Until the controller runs the port knows no target and turns the camera in place; then it
    // orbits the point the game's camera looks at, 10 ahead, (0, 2, 0): same distance, facing it.
    std::memcpy(node + SPARTAN_CAMERA_NODE_MATRIX, clean, sizeof(clean));  // the controller moves it
    const float target = 10;
    std::memcpy(manager + SPARTAN_CAMERA_MANAGER_DISTANCE, &target, 4);
    spartanport_camera_node_written(node, manager);
    std::memcpy(turned, node + SPARTAN_CAMERA_NODE_MATRIX, sizeof(turned));
    const float to_target[3] = {0 - turned[12], 2 - turned[13], 0 - turned[14]};
    assert(close_to(std::sqrt(dot(to_target, to_target)), 10) && !close_to(turned[12], 0));
    const float ahead[3] = {-turned[8], -turned[9], -turned[10]};
    assert(close_to(dot(ahead, to_target), 10));
    spartanport_camera_apply(gameplay, 0);  // the view takes the same camera
    assert(!std::memcmp(turned, node + SPARTAN_CAMERA_NODE_MATRIX, sizeof(turned)));
    assert(!std::memcmp(turned, gameplay + SPARTAN_CAMERA_WORLD, sizeof(turned)));
    spartanport_camera_steer(-1, 0, 0.25f, 0);  // back to the game's camera
    spartanport_camera_apply(gameplay, 0);
    assert(!std::memcmp(clean, node + SPARTAN_CAMERA_NODE_MATRIX, sizeof(clean)));
    // With the player's object there the camera orbits Kratos, at (3, 0, 0): the same distance to
    // him, the same height above him.
    static uint64_t player_global;
    alignas(16) static unsigned char player[0x100];
    const float kratos[4] = {3, 0, 0, 1};
    std::memcpy(player + SPARTAN_CAMERA_PLAYER_POSITION, kratos, sizeof(kratos));
    player_global = reinterpret_cast<uintptr_t>(player);
    spartanport_camera_set_player_global(reinterpret_cast<uintptr_t>(&player_global));
    spartanport_camera_steer(1, 0, 0.25f, 0);
    std::memcpy(node + SPARTAN_CAMERA_NODE_MATRIX, clean, sizeof(clean));
    spartanport_camera_node_written(node, manager);
    std::memcpy(turned, node + SPARTAN_CAMERA_NODE_MATRIX, sizeof(turned));
    const float before[3] = {clean[12] - kratos[0], clean[13] - kratos[1], clean[14] - kratos[2]};
    const float after[3] = {turned[12] - kratos[0], turned[13] - kratos[1], turned[14] - kratos[2]};
    assert(close_to(dot(before, before), dot(after, after)) && close_to(after[1], before[1]));
    assert(!close_to(turned[12], clean[12]));

    // Walls: the level's raycast from his chest (1.5 above him) to the camera. Clear, the camera
    // stays; a hit halfway brings it in to 0.4 short of the hit at once, on the controller's call
    // only (the view's update reuses it); clear again, it eases back out. The game's filter and hit
    // are as they were after each cast.
    {
        float open[16];
        std::memcpy(open, turned, sizeof(open));
        std::memset(fake_filter, 0xab, sizeof(fake_filter));
        std::memset(fake_hit, 0xcd, sizeof(fake_hit));
        unsigned char kept_filter[sizeof(fake_filter)], kept_hit[sizeof(fake_hit)];
        std::memcpy(kept_filter, fake_filter, sizeof(kept_filter));
        std::memcpy(kept_hit, fake_hit, sizeof(kept_hit));
        spartanport_camera_set_ray(reinterpret_cast<uintptr_t>(&fake_ray), reinterpret_cast<uintptr_t>(fake_filter),
                                   reinterpret_cast<uintptr_t>(fake_hit));
        std::memcpy(node + SPARTAN_CAMERA_NODE_MATRIX, clean, sizeof(clean));
        spartanport_camera_node_written(node, manager);
        // Two casts: the rise's search (nothing to rise for) and the pull.
        assert(fake_ray_calls == 2 && fake_kinds == SPARTAN_CAMERA_RAY_KINDS &&
               fake_excluded == SPARTAN_CAMERA_RAY_EXCLUDED && fake_flags == SPARTAN_CAMERA_RAY_CLOSEST);
        assert(!std::memcmp(open, node + SPARTAN_CAMERA_NODE_MATRIX, sizeof(open)));
        assert(!std::memcmp(kept_filter, fake_filter, sizeof(kept_filter)) && !std::memcmp(kept_hit, fake_hit, sizeof(kept_hit)));

        const float chest[3] = {kratos[0], kratos[1] + 1.5f, kratos[2]};
        const float way[3] = {open[12] - chest[0], open[13] - chest[1], open[14] - chest[2]};
        const float length = std::sqrt(dot(way, way));
        fake_ray_hits = true;
        std::memcpy(node + SPARTAN_CAMERA_NODE_MATRIX, clean, sizeof(clean));
        spartanport_camera_node_written(node, manager);
        std::memcpy(turned, node + SPARTAN_CAMERA_NODE_MATRIX, sizeof(turned));
        const float closer[3] = {turned[12] - chest[0], turned[13] - chest[1], turned[14] - chest[2]};
        assert(close_to(std::sqrt(dot(closer, closer)), length * 0.5f - 0.4f));
        assert(close_to(dot(closer, way), std::sqrt(dot(closer, closer)) * length));  // on the same line
        assert(!std::memcmp(turned + 8, open + 8, 3 * sizeof(float)));         // still looking the same way
        assert(!std::memcmp(kept_filter, fake_filter, sizeof(kept_filter)) && !std::memcmp(kept_hit, fake_hit, sizeof(kept_hit)));
        spartanport_camera_apply(gameplay, 0);
        assert(fake_ray_calls == 4 && !std::memcmp(turned, node + SPARTAN_CAMERA_NODE_MATRIX, sizeof(turned)));

        fake_ray_hits = false;
        std::memcpy(node + SPARTAN_CAMERA_NODE_MATRIX, clean, sizeof(clean));
        spartanport_camera_node_written(node, manager);
        std::memcpy(turned, node + SPARTAN_CAMERA_NODE_MATRIX, sizeof(turned));
        const float eased[3] = {turned[12] - chest[0], turned[13] - chest[1], turned[14] - chest[2]};
        const float pull = (length * 0.5f - 0.4f) / length;
        assert(close_to(std::sqrt(dot(eased, eased)), length * (pull + (1 - pull) * 0.15f)));

        // Up against a low wall (the way is clear only from higher up): the camera rises over him,
        // a quarter of the rise per cast (pulled in while its way still passes under the wall), and
        // ends above the wall at its full distance, looking down at him.
        fake_wall_top = open[13] + 0.5f;
        for (int cast = 0; cast < 40; ++cast) {
            std::memcpy(node + SPARTAN_CAMERA_NODE_MATRIX, clean, sizeof(clean));
            spartanport_camera_node_written(node, manager);
        }
        std::memcpy(turned, node + SPARTAN_CAMERA_NODE_MATRIX, sizeof(turned));
        const float risen[3] = {turned[12] - chest[0], turned[13] - chest[1], turned[14] - chest[2]};
        assert(turned[13] > fake_wall_top && std::sqrt(dot(risen, risen)) > length * 0.95f);
        assert(turned[9] * open[9] >= 0 && std::fabs(turned[9]) > std::fabs(open[9]));  // tilted further down
        fake_wall_top = -1e9f;
        spartanport_camera_set_ray(0, 0, 0);
    }

    // A cut (the game's camera jumped): back to the game's framing, the stick's turn dropped.
    float cut[16];
    std::memcpy(cut, clean, sizeof(cut));
    cut[14] += 20;
    std::memcpy(node + SPARTAN_CAMERA_NODE_MATRIX, cut, sizeof(cut));
    spartanport_camera_node_written(node, manager);
    assert(!std::memcmp(cut, node + SPARTAN_CAMERA_NODE_MATRIX, sizeof(cut)));

    // No yaw limit by default: the camera goes all the way around Kratos. Half a turn puts it on
    // his other side, at the same distance; a whole turn brings it back where the game had it.
    spartanport_camera_steer(1, 0, 1.5f, 0);  // 180 degrees
    spartanport_camera_node_written(node, manager);
    std::memcpy(turned, node + SPARTAN_CAMERA_NODE_MATRIX, sizeof(turned));
    const float back[3] = {turned[12] - kratos[0], turned[13] - kratos[1], turned[14] - kratos[2]};
    const float front[3] = {cut[12] - kratos[0], cut[13] - kratos[1], cut[14] - kratos[2]};
    assert(close_to(back[0], -front[0]) && close_to(back[2], -front[2]) && close_to(back[1], front[1]));
    spartanport_camera_steer(1, 0, 1.5f, 0);  // 360
    std::memcpy(node + SPARTAN_CAMERA_NODE_MATRIX, cut, sizeof(cut));  // the controller writes the game's camera
    spartanport_camera_node_written(node, manager);
    std::memcpy(turned, node + SPARTAN_CAMERA_NODE_MATRIX, sizeof(turned));
    for (int i = 0; i < 16; ++i) assert(std::fabs(turned[i] - cut[i]) < 1e-3f);

    // The title screen (the Shell level, its own WADs too) keeps the game's camera until a level of
    // play opens; the game's other files change nothing.
    assert(spartanport_camera_live());
    spartanport_camera_file_opened("/app0/EXEC/WAD/PS4_LE/SHELL.WAD");
    assert(!spartanport_camera_live());
    for (const char* other : {"/app0/EXEC/WAD/PS4_LE/HDS675SHELL.WAD", "/app0/EXEC/WAD/PS4_LE/R_UI.WAD",
                              "/app0/EXEC/WAD/PS4_LE/POSSKYBOX_050.WAD", "/app0/sound/OLY150.BNK"})
        spartanport_camera_file_opened(other);
    assert(!spartanport_camera_live());
    spartanport_camera_steer(1, 0, 0.25f, 0);
    std::memcpy(node + SPARTAN_CAMERA_NODE_MATRIX, cut, sizeof(cut));
    spartanport_camera_node_written(node, manager);
    assert(!std::memcmp(cut, node + SPARTAN_CAMERA_NODE_MATRIX, sizeof(cut)));
    spartanport_camera_file_opened("/app0/EXEC/WAD/PS4_LE/Oly150.WAD");
    assert(spartanport_camera_live());
    spartanport_camera_file_opened("/app0/EXEC/WAD/PS4_LE/SHELL.WAD");
    spartanport_camera_file_opened("/app0/EXEC/WAD/PS4_LE/CHALLENGE01.WAD");
    assert(spartanport_camera_live());

    // The mouse turns it by an angle: 30 degrees to the right, as the stick's quarter second; down
    // looks down, up to the pitch limit.
    spartanport_camera_steer(0, 0, 1, 1);
    spartanport_camera_look(30, 0);
    assert(close_to(spartanport_camera_yaw(), -30 * 0.017453292f));
    spartanport_camera_look(-30, 0);
    spartanport_camera_steer(1, 0, 0.25f, 0);
    assert(close_to(spartanport_camera_yaw(), -30 * 0.017453292f));
    spartanport_camera_steer(0, 0, 1, 1);
    (void)argc;
    (void)argv;
    std::puts("camera-hook-test: OK");
}
