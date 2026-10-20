// Runs the QTE stubs (src/qte_hook.h) as the game would call the hooked functions: the update
// stub counts frames, the rotation stub answers 1 once after the pad set pass (always with
// Automatic QTE), and the Automatic QTE stubs answer the button, repeated and held prompts.
// With the prepared eboot.elf as argument it also finds every site and the state block.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
#include <windows.h>
#include "../src/qte_hook.h"

// The stubs replay the functions' first instructions, then jump here: undo them and return what
// the game's own function would (0 from the rotation check: not finished yet). The other stubs
// resume mid-function; their landing pads return the register the game reads next.
extern "C" void qte_update_return();
extern "C" void qte_rotation_return();
extern "C" void qte_timed_return();
extern "C" void qte_repeat_original();
extern "C" void qte_charged_return();
extern "C" void qte_circle_pressed();
extern "C" void qte_circle_skip();
asm(".globl qte_circle_pressed\nqte_circle_pressed:\n    mov $1, %eax\n    ret\n"
    ".globl qte_circle_skip\nqte_circle_skip:\n    xor %eax, %eax\n    ret\n"".globl qte_update_return\nqte_update_return:\n    pop %r15\n    pop %rbp\n    ret\n"
    ".globl qte_rotation_return\nqte_rotation_return:\n    pop %r14\n    pop %rbp\n    xor %eax, %eax\n    ret\n"
    ".globl qte_timed_return\nqte_timed_return:\n    mov %ecx, %eax\n    ret\n"
    ".globl qte_repeat_original\nqte_repeat_original:\n    mov $7, %eax\n    ret\n"
    ".globl qte_charged_return\nqte_charged_return:\n    mov %r9d, %eax\n    ret\n");

static void call_update(unsigned char* stub) {
    asm volatile("call *%0" : : "r"(stub) : "rax", "memory", "cc");
}

static uint32_t call_rotation(unsigned char* stub) {
    uint32_t result;
    asm volatile("call *%1" : "=a"(result) : "r"(stub) : "memory", "cc");
    return result;
}

// rbx: the timed prompt's state block, rdi: the pad; the new presses come back in ecx.
static uint32_t call_timed(unsigned char* stub, void* block, void* pad) {
    uint32_t result;
    asm volatile("call *%1" : "=a"(result) : "r"(stub), "b"(block), "D"(pad) : "rcx", "memory", "cc");
    return result;
}

// rbx: the repeat state, rbp: the update's frame (the frame time at rbp-0x3c).
static uint32_t call_repeat(unsigned char* stub, void* state, void* frame) {
    uint32_t result;
    asm volatile("push %%rbp\n    mov %3, %%rbp\n    call *%1\n    pop %%rbp"
                 : "=a"(result) : "r"(stub), "b"(state), "S"(frame) : "xmm0", "memory", "cc");
    return result;
}

// rax: the pad; 1 when the stub went on to the press, 0 when it skipped it.
static uint32_t call_circle(unsigned char* stub, void* pad) {
    uint64_t result = reinterpret_cast<uintptr_t>(pad);
    asm volatile("call *%1" : "+a"(result) : "r"(stub) : "rcx", "memory", "cc");
    return static_cast<uint32_t>(result);
}

// r14: the pad; the held buttons come back in r9d.
static uint32_t call_charged(unsigned char* stub, void* pad) {
    uint32_t result;
    register void* r14 asm("r14") = pad;
    asm volatile("call *%1" : "=a"(result) : "r"(stub), "r"(r14) : "r9", "memory", "cc");
    return result;
}

int main(int argc, char** argv) {
    alignas(64) static SpartanQteState state;
    static bool automatic;
    auto* code = static_cast<unsigned char*>(
        VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    assert(code);
    unsigned char* update = code;
    unsigned char* rotation = code + 128;
    unsigned char* timed = code + 256;
    unsigned char* repeat = code + 384;
    unsigned char* charged = code + 512;
    unsigned char* circle = code + 640;
    const auto flag = reinterpret_cast<uintptr_t>(&automatic);
    assert(spartanport_qte_build_update_stub(update, reinterpret_cast<uintptr_t>(&state.heartbeat),
                                      reinterpret_cast<uintptr_t>(&qte_update_return)) < 128);
    assert(spartanport_qte_build_rotation_stub(rotation, reinterpret_cast<uintptr_t>(&state.pass), flag,
                                        reinterpret_cast<uintptr_t>(&qte_rotation_return)) < 128);
    assert(spartanport_qte_build_timed_stub(timed, flag, reinterpret_cast<uintptr_t>(&qte_timed_return)) < 128);
    assert(spartanport_qte_build_repeat_stub(repeat, flag, reinterpret_cast<uintptr_t>(&qte_repeat_original)) < 128);
    assert(spartanport_qte_build_charged_stub(charged, flag, reinterpret_cast<uintptr_t>(&qte_charged_return)) < 128);
    assert(spartanport_qte_build_circle_stub(circle, flag, reinterpret_cast<uintptr_t>(&qte_circle_pressed),
                                      reinterpret_cast<uintptr_t>(&qte_circle_skip)) < 128);
    FlushInstructionCache(GetCurrentProcess(), code, 4096);

    // A frame of a prompt: the count moves, the registers the function saves come back intact.
    call_update(update);
    call_update(update);
    assert(state.heartbeat == 2);

    // Rotation: the game's own answer until the pad sets pass, then 1 exactly once.
    assert(call_rotation(rotation) == 0);
    state.pass = 1;
    assert(call_rotation(rotation) == 1);
    assert(state.pass == 0);
    assert(call_rotation(rotation) == 0);
    automatic = true;  // Automatic QTE: every turn finished, pass left alone
    assert(call_rotation(rotation) == 1 && call_rotation(rotation) == 1 && state.pass == 0);
    automatic = false;

    // Timed press: the pad's new presses (held and not before), or the prompt's own button alone.
    alignas(8) uint8_t pad[0xa0] = {};
    const uint32_t held = 0x60, before = 0x20;  // circle held from before, cross new
    std::memcpy(pad + 0x94, &held, 4);
    std::memcpy(pad + 0x98, &before, 4);
    alignas(8) uint8_t block[0x20] = {};
    block[SPARTAN_QTE_TYPE_OFFSET] = 0;  // triangle
    assert(call_timed(timed, block, pad) == 0x40);
    automatic = true;
    assert(call_timed(timed, block, pad) == 0x10);
    block[SPARTAN_QTE_TYPE_OFFSET] = 3;  // square
    assert(call_timed(timed, block, pad) == 0x80);
    automatic = false;

    // Repeat press: the game's own query, or a press every frame credited with the frame's time.
    alignas(8) float repeat_state[16] = {}, frame[32] = {};
    char* rbp = reinterpret_cast<char*>(frame) + 0x40;
    const float dt = 1.0f / 120;
    std::memcpy(rbp - 0x3c, &dt, 4);
    assert(call_repeat(repeat, repeat_state, rbp) == 7);
    automatic = true;
    assert(call_repeat(repeat, repeat_state, rbp) == 1 && repeat_state[2] == dt);  // [state+8]
    automatic = false;

    // Circle struggle: a new Circle press goes on to the press, else it is skipped; always pressed
    // with Automatic QTE.
    assert(call_circle(circle, pad) == 0);  // circle held from before: not new
    const uint32_t none = 0;
    std::memcpy(pad + 0x98, &none, 4);
    assert(call_circle(circle, pad) == 1);
    std::memcpy(pad + 0x94, &none, 4);
    assert(call_circle(circle, pad) == 0);
    automatic = true;
    assert(call_circle(circle, pad) == 1);
    automatic = false;
    std::memcpy(pad + 0x94, &held, 4);

    // Charged press: the held buttons, or all of them.
    assert(call_charged(charged, pad) == held);
    automatic = true;
    assert(call_charged(charged, pad) == 0xffffffffu);
    automatic = false;

    // The state block from the rotation check's lea r8,[rip+disp], a call's target from its rel32.
    unsigned char lea[SPARTAN_QTE_BLOCK_NEXT] = {};
    const int32_t displacement = 0x33940d;
    std::memcpy(lea + SPARTAN_QTE_BLOCK_DISPLACEMENT, &displacement, 4);
    assert(spartanport_qte_block(lea, 0x12c280) == 0x4656b0);
    const unsigned char call[5] = {0xe8, 0x5b, 0x77, 0xf4, 0xff};
    assert(spartanport_qte_call_target(call, 0x12b750) == 0x72eb0);

    if (argc > 1) {
        // The code segment starts at file offset 0x4000 (image offset 0).
        std::ifstream file(argv[1], std::ios::binary);
        std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)), {});
        assert(data.size() > 0x4000);
        const unsigned char* image = data.data() + 0x4000;
        const uint64_t size = data.size() - 0x4000;
        const uint64_t update_at = spartanport_signature_find(image, size, &spartanport_qte_update_site);
        const uint64_t rotation_at = spartanport_signature_find(image, size, &spartanport_qte_rotation_site);
        assert(update_at != UINT64_MAX && rotation_at != UINT64_MAX);
        assert(!std::memcmp(image + update_at, spartanport_qte_update_original, SPARTAN_QTE_HOOK_LENGTH));
        assert(!std::memcmp(image + rotation_at, spartanport_qte_rotation_original, SPARTAN_QTE_HOOK_LENGTH));
        const uint64_t block_at = spartanport_qte_block(image + rotation_at, rotation_at);
        assert(block_at > update_at && block_at % 8 == 0);
        const uint64_t timed_at = spartanport_signature_find(image, size, &spartanport_qte_timed_site);
        const uint64_t repeat_at = spartanport_signature_find(image, size, &spartanport_qte_repeat_site);
        const uint64_t charged_at = spartanport_signature_find(image, size, &spartanport_qte_charged_site);
        const uint64_t circle_at = spartanport_signature_find(image, size, &spartanport_qte_circle_site);
        assert(timed_at != UINT64_MAX && repeat_at != UINT64_MAX && charged_at != UINT64_MAX);
        assert(circle_at != UINT64_MAX && image[circle_at + SPARTAN_QTE_CIRCLE_LENGTH] == 0xc4 &&  // vmovss
               image[circle_at + SPARTAN_QTE_CIRCLE_SKIP] == 0x4b);                              // lea
        assert(timed_at > update_at && timed_at < update_at + 0x200);  // inside the timed update
        assert(image[repeat_at] == 0xe8);
        std::printf("QTE sites: update 0x%llx, rotation 0x%llx, state block 0x%llx, timed 0x%llx, "
                    "repeat 0x%llx (query 0x%llx), charged 0x%llx, circle 0x%llx\n",
                    static_cast<unsigned long long>(update_at), static_cast<unsigned long long>(rotation_at),
                    static_cast<unsigned long long>(block_at), static_cast<unsigned long long>(timed_at),
                    static_cast<unsigned long long>(repeat_at),
                    static_cast<unsigned long long>(spartanport_qte_call_target(image + repeat_at, repeat_at)),
                    static_cast<unsigned long long>(charged_at), static_cast<unsigned long long>(circle_at));
    }
    VirtualFree(code, 0, MEM_RELEASE);
    std::puts("PASS: QTE hooks");
}
