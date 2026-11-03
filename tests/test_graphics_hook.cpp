// Runs the Options > Graphics stubs (src/graphics_hook.h) with each toggle on and off.
#include <cassert>
#include <cstdint>
#include <cstring>
#include "../src/graphics_hook.h"
#include <windows.h>

// Motion blur: the stub is entered as the function itself. On, it replays the prologue (push rbp;
// mov rbp, rsp; push r15; push r14) and jumps here: undo it, note the visit and return.
extern "C" int blur_resumed;
int blur_resumed;
extern "C" void blur_resume();
asm(".globl blur_resume\nblur_resume:\n"
    "    pop %r14\n    pop %r15\n    pop %rbp\n"
    "    movl $1, blur_resumed(%rip)\n    ret\n");
// The effect's parameter controller update (image 0x2c7680 in CUSA01623 v01.00): notes its argument
// and whether the stack was 16-byte aligned at the call, as the game's code expects.
extern "C" uint64_t blur_update_arg;
uint64_t blur_update_arg;
extern "C" uint32_t blur_update_misaligned;
uint32_t blur_update_misaligned;
extern "C" void blur_update();
asm(".globl blur_update\nblur_update:\n"
    "    movq %rdi, blur_update_arg(%rip)\n"
    "    leaq 8(%rsp), %rax\n    andl $15, %eax\n    movl %eax, blur_update_misaligned(%rip)\n    ret\n");
// Enters the stub as the game does: `this` in rdi (SysV), rsp 16-byte aligned before the call.
static void run_blur(unsigned char* stub, unsigned char* fx) {
    asm volatile("push %%rbp\n    mov %%rsp, %%rbp\n    and $-16, %%rsp\n    call *%%rsi\n    mov %%rbp, %%rsp\n    pop %%rbp"
                 : "+D"(fx), "+S"(stub) : : "rax", "rcx", "rdx", "r8", "r9", "r10", "r11", "r14", "r15", "memory", "cc");
}
// Shadows: like motion blur's prologue; the resume notes the casters list (rsi) it was given.
extern "C" uint64_t shadow_rsi;
uint64_t shadow_rsi;
extern "C" void shadow_resume();
asm(".globl shadow_resume\nshadow_resume:\n"
    "    pop %r14\n    pop %r15\n    pop %rbp\n"
    "    movq %rsi, shadow_rsi(%rip)\n    ret\n");
static void run_shadows(unsigned char* stub, uint64_t casters) {
    asm volatile("call *%1" : "+S"(casters) : "r"(stub) : "rax", "memory", "cc");
}
// Bloom: the stub replaces `cmp byte [rbx+0x469], 0` before the game's `je skip`; the resume
// notes ZF, i.e. whether that je would skip the bloom passes.
extern "C" uint8_t bloom_skipped;
uint8_t bloom_skipped;
extern "C" void bloom_resume();
asm(".globl bloom_resume\nbloom_resume:\n    sete bloom_skipped(%rip)\n    ret\n");

static bool run_bloom(unsigned char* stub, unsigned char* fx) {
    asm volatile("push %%rbx\n    mov %%rdi, %%rbx\n    call *%%rsi\n    pop %%rbx"
                 : : "S"(stub), "D"(fx) : "rax", "memory", "cc");
    return bloom_skipped != 0;
}

int main() {
    // The signatures name the sites they were made from.
    int bytes[SPARTAN_SIGNATURE_MAX];
    assert(spartanport_signature_parse(spartanport_motion_blur_site.pattern, bytes) == 48);
    for (unsigned i = 0; i < SPARTAN_MOTION_BLUR_HOOK_LENGTH; ++i)
        assert(bytes[i] == spartanport_motion_blur_original[i]);
    assert(bytes[SPARTAN_MOTION_BLUR_GATE] == 0x4c && bytes[SPARTAN_MOTION_BLUR_GATE + 2] == 0x3d);    // lea r15
    assert(bytes[SPARTAN_MOTION_BLUR_QUEUED] == 0x4c && bytes[SPARTAN_MOTION_BLUR_QUEUED + 2] == 0x35);  // lea r14
    {
        // The controller update is a `call rel32` at SPARTAN_MOTION_BLUR_UPDATE_CALL.
        static unsigned char image[0x100] = {};
        const uint64_t at = 0x10 + SPARTAN_MOTION_BLUR_UPDATE_CALL;
        const int32_t rel = 0x40;
        image[at] = 0xe8;
        std::memcpy(image + at + 1, &rel, 4);
        assert(spartanport_call_target(image, sizeof(image), at) == at + 5 + 0x40);
        image[at] = 0x90;
        assert(spartanport_call_target(image, sizeof(image), at) == UINT64_MAX);
        assert(spartanport_call_target(image, sizeof(image), sizeof(image) - 3) == UINT64_MAX);
    }
    assert(spartanport_signature_parse(spartanport_bloom_site.pattern, bytes) == 23);
    for (unsigned i = 0; i < SPARTAN_BLOOM_HOOK_LENGTH; ++i)
        assert(bytes[i] == spartanport_bloom_original[i]);
    assert(bytes[10] == 0x0f && bytes[11] == 0x84);  // the je the hook leaves in place

    // rip targets: lea at offset 16 with displacement 0x10 -> 16 + 7 + 0x10.
    unsigned char code[32] = {};
    const int32_t disp = 0x10;
    std::memcpy(code + 19, &disp, 4);
    assert(spartanport_rip_target(code, 16) == 16 + 7 + 0x10);

    // Upgrade menu flag: three `lea rax, [rip+flag]; mov [rax], r15d; jmp` cases (inShell,
    // inPauseMenu, inPowerUpMenu), then `cmp r13d, 15; jg`. The third lea names inPowerUpMenu.
    assert(spartanport_signature_parse(spartanport_power_up_menu_site.pattern, bytes) == 51);
    for (unsigned c = 0; c < 3; ++c) {
        assert(bytes[c * 15] == 0x48 && bytes[c * 15 + 1] == 0x8d && bytes[c * 15 + 2] == 0x05);
        assert(bytes[c * 15 + 7] == 0x44 && bytes[c * 15 + 8] == 0x89 && bytes[c * 15 + 9] == 0x38);
    }
    assert(SPARTAN_POWER_UP_MENU_LEA == 2 * 15);
    {
        // Synthetic switch at offset 8, flags at 0x100/0x104/0x108 (like 0x8203d0/d4/d8).
        static unsigned char image[0x200] = {};
        const uint64_t at = 8;
        const unsigned char tail[] = {0x41, 0x83, 0xfd, 0x0f, 0x0f, 0x8f};
        for (unsigned c = 0; c < 3; ++c) {
            unsigned char* p = image + at + c * 15;
            const int32_t d = int32_t(0x100 + 4 * c) - int32_t(at + c * 15 + 7);
            p[0] = 0x48; p[1] = 0x8d; p[2] = 0x05;
            std::memcpy(p + 3, &d, 4);
            p[7] = 0x44; p[8] = 0x89; p[9] = 0x38;
            p[10] = 0xe9;
        }
        std::memcpy(image + at + 45, tail, sizeof(tail));
        assert(spartanport_signature_find(image, sizeof(image), &spartanport_power_up_menu_site) == at);
        assert(spartanport_rip_target(image, at + SPARTAN_POWER_UP_MENU_LEA) == 0x108);
    }

    auto* page = static_cast<unsigned char*>(
        VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    assert(page);

    // Motion blur.
    volatile bool blur_on = true;
    volatile int32_t gate = 1, queued = 1;
    assert(spartanport_motion_blur_build_stub(page, (uintptr_t)&blur_on, (uintptr_t)&gate, (uintptr_t)&queued,
                                              (uintptr_t)&blur_update, (uintptr_t)&blur_resume) < 256);
    static unsigned char blur_fx[0x230] = {};  // the MotionBlurFX object; +0x28 is its parameter controller
    run_blur(page, blur_fx);
    assert(blur_resumed == 1 && gate == 1 && queued == 1 && blur_update_arg == 0);  // on: the game's own function runs
    blur_resumed = 0;
    blur_on = false;
    blur_update_misaligned = 99;
    run_blur(page, blur_fx);
    assert(blur_resumed == 0 && gate == 0 && queued == 0);  // off: gates cleared, nothing queued
    // off: the controller is still updated, or the parameter requests it frees fill the game's pool (#1186)
    assert(blur_update_arg == (uintptr_t)(blur_fx + 0x28) && blur_update_misaligned == 0);

    // Bloom.
    unsigned char* bloom = page + 256;
    static unsigned char fx[0x470] = {};  // the fxBloom object; +0x469 is its "active this frame" byte
    volatile bool bloom_on = true;
    volatile int32_t menus[3] = {};  // inShell, inPauseMenu, inPowerUpMenu
    const uintptr_t menu_flags[3] = {(uintptr_t)&menus[0], (uintptr_t)&menus[1], (uintptr_t)&menus[2]};
    assert(spartanport_bloom_build_stub(bloom, (uintptr_t)&bloom_on, menu_flags, (uintptr_t)&bloom_resume) < 256);
    fx[0x469] = 1;
    assert(!run_bloom(bloom, fx));  // on + active: bloom passes run
    fx[0x469] = 0;
    assert(run_bloom(bloom, fx));   // on + inactive: the game's own skip
    bloom_on = false;
    fx[0x469] = 1;
    assert(run_bloom(bloom, fx));   // off: skipped even when active
    for (unsigned m = 0; m < 3; ++m) {
        menus[m] = 1;
        assert(!run_bloom(bloom, fx));  // off + a menu open: bloom runs, the menu text needs it (#1176)
        fx[0x469] = 0;
        assert(run_bloom(bloom, fx));   // off + menu + inactive: still the game's own skip
        fx[0x469] = 1;
        menus[m] = 0;
    }
    bloom_on = true;
    assert(!run_bloom(bloom, fx));

    // Menu flags not found: off skips as before.
    const uintptr_t no_menus[3] = {};
    assert(spartanport_bloom_build_stub(bloom, (uintptr_t)&bloom_on, no_menus, (uintptr_t)&bloom_resume) < 256);
    bloom_on = false;
    menus[1] = 1;
    assert(run_bloom(bloom, fx));
    menus[1] = 0;
    bloom_on = true;
    assert(!run_bloom(bloom, fx));

    // Shadows.
    assert(spartanport_signature_parse(spartanport_shadows_site.pattern, bytes) == 53);
    for (unsigned i = 0; i < SPARTAN_SHADOWS_HOOK_LENGTH; ++i)
        assert(bytes[i] == spartanport_motion_blur_original[i]);
    unsigned char* shadows = page + 512;
    volatile bool shadows_on = true;
    assert(spartanport_shadows_build_stub(shadows, (uintptr_t)&shadows_on, (uintptr_t)&shadow_resume) < 256);
    run_shadows(shadows, 0x1234);
    assert(shadow_rsi == 0x1234);  // on: the casters list goes through
    shadows_on = false;
    run_shadows(shadows, 0x1234);
    assert(shadow_rsi == 0);  // off: no casters, no shadow maps
    return 0;
}
