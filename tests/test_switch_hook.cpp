// Runs the switch-attack stubs (src/switch_hook.h) as the game reaches them: the input check asks
// the pad's C function about branches to switch attacks only, and goes on, fails or runs the
// game's check as it answers; the queue site lets it swap the branch; the d-pad select notes the
// stats. With the prepared eboot.elf as argument it also finds the sites.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
#include <sys/mman.h>
#include "../src/switch_hook.h"

// Volatile: only the stubs call the fake functions, which the compiler cannot see.
static volatile int input_answer;
static const unsigned char* volatile input_seen;
static volatile uintptr_t input_caller;
static const unsigned char* volatile queue_swap;
static const unsigned char* volatile queue_seen;
extern "C" __attribute__((sysv_abi)) int fake_input(const unsigned char* branch, uintptr_t caller) {
    input_seen = branch;
    input_caller = caller;
    return input_answer;
}
extern "C" __attribute__((sysv_abi)) const unsigned char* fake_queue(const unsigned char* branch) {
    queue_seen = branch;
    return queue_swap ? queue_swap : branch;
}

// Landings: the game's own input check (after the replayed push rbp; mov rbp,rsp; push r15),
// the rest of the conditions (rsi branch, rdx the fourth argument), and the queue site's resume.
extern "C" void land_original();
extern "C" void land_pass();
extern "C" void land_queue();
extern "C" void land_dpad();   // after push rbp; mov rbp,rsp; push rbx; push rax
extern "C" uint64_t pass_rsi, pass_rdx, queue_rdx, queue_regs_ok;
// run_queue(stub, branch): rax = branch, rcx/rsi/rdi/r8 set, jump to the stub; returns rax at the
// landing (the branch that goes on), edx and whether the registers came through.
extern "C" uint64_t run_queue(unsigned char* stub, const void* branch);
asm(".data\n.globl pass_rsi\npass_rsi: .quad 0\n.globl pass_rdx\npass_rdx: .quad 0\n"
    ".globl queue_rdx\nqueue_rdx: .quad 0\n.globl queue_regs_ok\nqueue_regs_ok: .quad 0\n.text\n"
    ".globl land_original\nland_original:\n    pop %r15\n    pop %rbp\n    mov $0x77, %eax\n    ret\n"
    ".globl land_dpad\nland_dpad:\n    pop %rax\n    pop %rbx\n    pop %rbp\n    mov $0x77, %eax\n    ret\n"
    ".globl land_pass\nland_pass:\n    mov %rsi, pass_rsi(%rip)\n    mov %rdx, pass_rdx(%rip)\n    mov $0x55, %eax\n    ret\n"
    ".globl run_queue\nrun_queue:\n"
    "    push %rbx\n    push %r12\n    sub $8, %rsp\n    mov %rsi, %rax\n    mov $0x1c1c, %ecx\n    mov $0x5e5e, %esi\n"
    "    mov $0xd1d1, %r12d\n    mov %rdi, %rbx\n    mov $0xd1d1, %edi\n    mov $0x8888, %r8d\n    jmp *%rbx\n"
    ".globl land_queue\nland_queue:\n"
    "    mov %rdx, queue_rdx(%rip)\n    movq $0, queue_regs_ok(%rip)\n"
    "    cmp $0x1c1c, %rcx\n    jne 1f\n    cmp $0x5e5e, %rsi\n    jne 1f\n    cmp $0xd1d1, %rdi\n    jne 1f\n"
    "    cmp $0x8888, %r8\n    jne 1f\n    cmp $0xd1d1, %r12\n    jne 1f\n    movq $1, queue_regs_ok(%rip)\n"
    "1:\n    add $8, %rsp\n    pop %r12\n    pop %rbx\n    ret\n");

__attribute__((noinline)) static uint64_t call_input(unsigned char* stub, const void* branch, uint64_t rcx, uint64_t* rdi_after,
                           uint64_t* rcx_after) {
    uint64_t result;
    uint64_t rdi = 0xd1d1;
    uint64_t rsi = 0x5151;
    asm volatile("call *%[stub]"
                 : "=a"(result), "+D"(rdi), "+S"(rsi), "+d"(branch), "+c"(rcx)
                 : [stub] "r"(stub)
                 : "r8", "r9", "r10", "r11", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
                   "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15", "memory", "cc");
    *rdi_after = rdi;
    *rcx_after = rcx;
    return result;
}

int main(int argc, char** argv) {
    alignas(16) static SpartanSwitchState state;
    auto* code = static_cast<unsigned char*>(mmap(nullptr, SPARTAN_SWITCH_SITES * SPARTAN_SWITCH_SLOT,
                                                  PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    assert(code != MAP_FAILED);
    unsigned char* input = code;
    unsigned char* queue = code + SPARTAN_SWITCH_SLOT;
    unsigned char* dpad = code + 2 * SPARTAN_SWITCH_SLOT;
    const uintptr_t s = reinterpret_cast<uintptr_t>(&state);
    assert(spartanport_switch_build_stub(input, &spartanport_switch_sites[SPARTAN_SWITCH_INPUT], s, reinterpret_cast<uintptr_t>(&fake_input),
                                  reinterpret_cast<uintptr_t>(&land_pass), reinterpret_cast<uintptr_t>(&land_original)) <=
           SPARTAN_SWITCH_SLOT);
    assert(spartanport_switch_build_stub(queue, &spartanport_switch_sites[SPARTAN_SWITCH_QUEUE], s, reinterpret_cast<uintptr_t>(&fake_queue), 0,
                                  reinterpret_cast<uintptr_t>(&land_queue)) <= SPARTAN_SWITCH_SLOT);
    assert(spartanport_switch_build_stub(dpad, &spartanport_switch_sites[SPARTAN_SWITCH_DPAD], s, 0, 0,
                                  reinterpret_cast<uintptr_t>(&land_dpad)) <= SPARTAN_SWITCH_SLOT);

    // A switch branch as the game's data has them (to the Blades), another one, and an ordinary one.
    alignas(16) static unsigned char to_blades[0x38], to_claws[0x38], other[0x38];
    for (unsigned char* b : {to_blades, to_claws}) {
        b[0x10] = b[0x11] = 0xaa;
        b[0x2c] = b[0x2d] = 0xff;
        b[0x2e] = 0x0f;
        b[0x30] = b[0x31] = 1;
    }
    to_blades[0x2f] = 0x12;
    to_claws[0x2f] = 0x13;
    to_blades[8] = 0x11;  // the flags the queue site tests (bit 29 clear)
    to_claws[11] = 0x20;  // bit 29 set
    assert(spartanport_switch_branch(to_blades) && spartanport_switch_branch(to_claws) && !spartanport_switch_branch(other));
    other[0x30] = other[0x31] = 1;
    other[0x2f] = 0x06;  // Cross just pressed, for another item
    uint64_t rdi, rcx;

    // Input check: ordinary branches never reach the C function.
    input_seen = nullptr;
    assert(call_input(input, other, 0xc0c0, &rdi, &rcx) == 0x77 && !input_seen && rdi == 0xd1d1 && rcx == 0xc0c0);
    // A switch branch: the answer decides.
    input_answer = 1;
    assert(call_input(input, to_blades, 0xc0c0, &rdi, &rcx) == 0x55 && input_seen == to_blades);
    assert(input_caller > reinterpret_cast<uintptr_t>(&call_input) &&
           input_caller < reinterpret_cast<uintptr_t>(&call_input) + 0x200);  // its return address
    assert(pass_rsi == reinterpret_cast<uintptr_t>(to_blades) && pass_rdx == 0xc0c0 && rdi == 0xd1d1);
    input_answer = 0;
    assert(call_input(input, to_blades, 0xc0c0, &rdi, &rcx) == 0);
    to_blades[0x31] = 4;  // Cross held: a switch branch too
    assert(call_input(input, to_blades, 0xc0c0, &rdi, &rcx) == 0 && input_seen == to_blades);
    to_blades[0x31] = 2;  // Cross released: not one
    input_seen = nullptr;
    assert(call_input(input, to_blades, 0xc0c0, &rdi, &rcx) == 0x77 && !input_seen);
    to_blades[0x31] = 1;
    input_answer = 2;
    assert(call_input(input, to_blades, 0xc0c0, &rdi, &rcx) == 0x77 && rdi == 0xd1d1 && rcx == 0xc0c0);

    // Queue site: switch branches go through the C function, which may swap them.
    queue_seen = nullptr;
    assert(run_queue(queue, other) == reinterpret_cast<uintptr_t>(other) && !queue_seen && queue_regs_ok);
    queue_swap = nullptr;
    assert(run_queue(queue, to_blades) == reinterpret_cast<uintptr_t>(to_blades) && queue_seen == to_blades &&
           queue_regs_ok && queue_rdx == 0x11);
    queue_swap = to_claws;
    assert(run_queue(queue, to_blades) == reinterpret_cast<uintptr_t>(to_claws) && queue_regs_ok &&
           queue_rdx == 0x20000000);

    // d-pad select: the stats noted, the function goes on.
    {
        alignas(16) static unsigned char stats[0x300];
        uint64_t r;
        register void* rdi_r asm("rdi") = stats;
        asm volatile("call *%[s]" : "=a"(r), "+r"(rdi_r) : [s] "r"(dpad) : "rcx", "rdx", "rsi", "r8", "r9", "r10", "r11", "memory", "cc");
        assert(r == 0x77 && state.stats == reinterpret_cast<uintptr_t>(stats) && rdi_r == stats);
    }

    if (argc > 1) {
        // The code segment starts at file offset 0x4000 (image offset 0).
        std::ifstream file(argv[1], std::ios::binary);
        std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)), {});
        assert(data.size() > 0x4000);
        const unsigned char* image = data.data() + 0x4000;
        const uint64_t size = data.size() - 0x4000;
        for (const SpartanSwitchSite& site : spartanport_switch_sites) {
            const uint64_t at = spartanport_signature_find(image, size, &site.site);
            assert(at != UINT64_MAX && !memcmp(image + at, site.original, site.length));
            std::printf("Switch site %s 0x%llx\n", site.name, static_cast<unsigned long long>(at));
        }
        const uint64_t caller = spartanport_signature_find(image, size, &spartanport_switch_caller_site);
        assert(caller != UINT64_MAX && image[caller - 5] == 0xe8);
        std::printf("Input check called back to 0x%llx\n", static_cast<unsigned long long>(caller));
        const uint64_t pass_at = spartanport_signature_find(image, size, &spartanport_switch_pass_site);
        assert(pass_at != UINT64_MAX);
        int32_t rel;
        memcpy(&rel, image + pass_at, 4);
        std::printf("Input check goes on at 0x%llx\n", static_cast<unsigned long long>(pass_at + 4 + rel));
    }
    munmap(code, SPARTAN_SWITCH_SITES * SPARTAN_SWITCH_SLOT);
    std::puts("PASS: switch attack hooks");
}
