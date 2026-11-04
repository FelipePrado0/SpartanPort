// Runs the Options > Graphics stubs (src/graphics_hook.h) with each toggle on and off.
#include <cassert>
#include <cstdint>
#include <cstring>
#include "../src/graphics_hook.h"
#include <windows.h>

// Motion blur: the stub replaces `test dl, dl; je epilogue`. The two targets return, so a call of
// the stub records which side ran and the eax the branch has to keep.
extern "C" int blur_went_resume;
int blur_went_resume;
extern "C" int blur_went_epilogue;
int blur_went_epilogue;
extern "C" uint32_t blur_eax;
uint32_t blur_eax;
extern "C" void blur_resume();
asm(".text\n.globl blur_resume\nblur_resume:\n"
    "    movl %eax, blur_eax(%rip)\n"
    "    movl $1, blur_went_resume(%rip)\n"
    "    ret\n");
extern "C" void blur_epilogue();
asm(".text\n.globl blur_epilogue\nblur_epilogue:\n"
    "    movl %eax, blur_eax(%rip)\n"
    "    movl $1, blur_went_epilogue(%rip)\n"
    "    ret\n");
static void run_blur(unsigned char* stub, uint32_t eax, uint32_t edx) {
    asm volatile("call *%2" : "+a"(eax), "+d"(edx) : "c"(stub) : "rsi", "memory", "cc");
}
// Shadows: replays an 8-byte prologue; the resume notes the casters list (rsi) it was given.
extern "C" uint64_t shadow_rsi;
uint64_t shadow_rsi;
extern "C" void shadow_resume();
asm(".text\n.globl shadow_resume\nshadow_resume:\n"
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
asm(".text\n.globl bloom_resume\nbloom_resume:\n    sete bloom_skipped(%rip)\n    ret\n");

static bool run_bloom(unsigned char* stub, unsigned char* fx) {
    asm volatile("push %%rbx\n    mov %%rdi, %%rbx\n    call *%%rsi\n    pop %%rbx"
                 : : "S"(stub), "D"(fx) : "rax", "memory", "cc");
    return bloom_skipped != 0;
}

int main() {
    // The signatures name the sites they were made from.
    int bytes[SPARTAN_SIGNATURE_MAX];
    assert(spartanport_signature_parse(spartanport_motion_blur_site.pattern, bytes) == 20);
    assert(bytes[0] == 0x84 && bytes[1] == 0xd2 && bytes[2] == 0x0f && bytes[3] == 0x84);  // test dl, dl; je
    assert(bytes[8] == 0x48 && bytes[9] == 0x8d && bytes[10] == 0x15);                    // lea rdx, [rip]
    assert(bytes[15] == 0x85 && bytes[16] == 0xc0 && bytes[17] == 0x0f && bytes[18] == 0x94 && bytes[19] == 0xc0);
    assert(spartanport_motion_blur_signature_match(spartanport_motion_blur_original, SPARTAN_MOTION_BLUR_HOOK_LENGTH));
    assert(!spartanport_motion_blur_branch_match(spartanport_motion_blur_original, SPARTAN_MOTION_BLUR_HOOK_LENGTH));
    {
        // CUSA01623 v01.02 at image 0x16af5c: je +0x168 lands on the epilogue at 0x16b0cc.
        const unsigned char site[] = {
            0x84, 0xd2, 0x0f, 0x84, 0x68, 0x01, 0x00, 0x00, 0x48, 0x8d, 0x15, 0x65, 0xe1, 0x90, 0x01,
            0x85, 0xc0, 0x0f, 0x94, 0xc0};
        assert(spartanport_motion_blur_branch_match(site, sizeof(site)));
        assert(spartanport_motion_blur_epilogue(site, 0) == 0x170);
        assert(spartanport_signature_find(site, sizeof(site), &spartanport_motion_blur_site) == 0);
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

    // Motion blur. eax is the previous-frame flag the bytes after the branch still read.
    volatile bool blur_on = true;
    assert(spartanport_motion_blur_build_stub(page, (uintptr_t)&blur_on, (uintptr_t)&blur_resume,
                                              (uintptr_t)&blur_epilogue) < 256);
    const uint32_t sentinel = 0xa5a5a5a5u;
    auto check_blur = [&](bool on, uint32_t dl, bool resume) {
        blur_on = on;
        blur_went_resume = 0;
        blur_went_epilogue = 0;
        blur_eax = 0;
        run_blur(page, sentinel, dl);
        assert(blur_eax == sentinel);
        assert(blur_went_resume == (resume ? 1 : 0));
        assert(blur_went_epilogue == (resume ? 0 : 1));
    };
    check_blur(true, 1, true);    // on, camera moving: the queue path
    check_blur(true, 0, false);   // on, camera cut: the game's own skip
    check_blur(false, 1, false);  // off: skip even when the camera moved
    check_blur(false, 0, false);

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
