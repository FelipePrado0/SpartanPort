// Live cheat stubs execute on synthetic state without touching game saves.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <windows.h>
#include "../src/cheat_hook.h"

static int read_difficulty(void* entry, unsigned completed) {
    int result;
    asm volatile("mov %2, %%ecx; call *%1"
                 : "=a"(result) : "r"(entry), "r"(completed) : "rcx", "memory", "cc");
    return result;
}

static int jump_blocked(void* entry, void* actor) {
    int result;
    asm volatile("mov %2, %%r12; call *%1"
                 : "=a"(result) : "r"(entry), "r"(actor) : "r12", "memory", "cc");
    return result;
}

int main() {
    _set_error_mode(_OUT_TO_STDERR);
    const SpartanCheatHook* unlock = nullptr;
    for (const auto& hook : spartanport_cheat_hooks) {
        if (hook.offset == 0xc77ea) unlock = &hook;
    }
    assert(unlock);
    const unsigned char tail[] = {
        0xb8,3,0,0,0,0xf6,0xc1,8,0x75,0x1b,
        0xb8,2,0,0,0,0xf6,0xc1,4,0x75,0x11,
        0xb8,1,0,0,0,0xf6,0xc1,2,0x75,7,
        0x83,0xe1,1,0xff,0xc9,0x89,0xc8,0xc3
    };
    assert(spartanport_cheat_signature_match(unlock, tail, sizeof(tail)));
    auto* memory = static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    assert(memory);
    std::memcpy(memory+512,tail,sizeof(tail));
    unsigned char enabled=0;
    spartanport_cheat_build_stub(memory,unlock,reinterpret_cast<uintptr_t>(&enabled),reinterpret_cast<uintptr_t>(memory+512+unlock->length));
    for(unsigned mask=0;mask<16;++mask) {
        enabled=0;
        const int expected=mask&8?3:mask&4?2:mask&2?1:mask&1?0:-1;
        assert(read_difficulty(memory,mask)==expected);
        enabled=1;
        assert(read_difficulty(memory,mask)==3);
    }
    const SpartanCheatHook* jumps = nullptr;
    for (const auto& hook : spartanport_cheat_hooks) {
        if (hook.offset == 0x95d51) jumps = &hook;
    }
    assert(jumps);
    const unsigned char original[] = {0x41,0xf6,0x84,0x24,0xac,4,0,0,4};
    assert(spartanport_cheat_signature_match(jumps,original,sizeof(original)));
    assert(!spartanport_cheat_signature_match(jumps,original,sizeof(original)-1));
    unsigned char mismatch[9];std::memcpy(mismatch,original,9);mismatch[8]=8;
    assert(!spartanport_cheat_signature_match(jumps,mismatch,9));
    const unsigned char resume[] = {0x0f,0x95,0xc0,0x0f,0xb6,0xc0,0xc3};
    std::memcpy(memory+512,resume,sizeof(resume));
    spartanport_cheat_build_stub(memory,jumps,reinterpret_cast<uintptr_t>(&enabled),reinterpret_cast<uintptr_t>(memory+512));
    unsigned char actor[0x500]{};
    for (unsigned flags : {0u,4u,0x80004u}) {
        std::memcpy(actor+0x4ac,&flags,4);
        actor[0x298]=0;enabled=0;
        assert(jump_blocked(memory,actor)==int((flags&4)!=0));
        enabled=1;
        assert(jump_blocked(memory,actor)==0);
        actor[0x298]=1;
        assert(jump_blocked(memory,actor)==int((flags&4)!=0));
        unsigned after;std::memcpy(&after,actor+0x4ac,4);assert(after==flags);
    }
    VirtualFree(memory,0,MEM_RELEASE);
    std::puts("PASS: live cheat hooks");
}
