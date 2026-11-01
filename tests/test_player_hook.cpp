// The player hook's stub: stores the first r14 only, replays the lea, keeps rax, flags and the red zone.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
#include <windows.h>
#include "../src/player_hook.h"

struct Result { uint64_t rax, rcx, flags_in, flags_out, red_zone; };
// Enters the stub with r14 = object and a marked red zone, the way the patched site does.
extern "C" __attribute__((sysv_abi)) void player_invoke(void *code, uintptr_t object, Result *result);
asm(".globl player_invoke\nplayer_invoke:\n"
    "push %rbp\nmov %rsp, %rbp\npush %r14\npush %rbx\nmov %rdx, %rbx\nmov %rsi, %r14\n"
    "movabs $0x5a5a5a5a5a5a5a5a, %rax\nmov %rax, -16(%rsp)\nmov %rax, -128(%rsp)\n"
    "movabs $0x1111222233334444, %rax\nxor %ecx, %ecx\n"
    "pushfq\norq $0x8d5, (%rsp)\npopfq\npushfq\npopq 16(%rbx)\n"
    "call *%rdi\n"
    "pushfq\npopq 24(%rbx)\nmov %rax, 0(%rbx)\nmov %rcx, 8(%rbx)\n"
    "movabs $0x5a5a5a5a5a5a5a5a, %rax\nxor %ecx, %ecx\n"
    "cmp %rax, -16(%rsp)\njne 1f\ncmp %rax, -128(%rsp)\njne 1f\ninc %ecx\n"
    "1: mov %rcx, 32(%rbx)\npop %rbx\npop %r14\npop %rbp\nret\n");

static uint64_t read64(const std::vector<unsigned char>& data, size_t at) {
    uint64_t value;
    assert(at + sizeof(value) <= data.size());
    std::memcpy(&value, data.data() + at, sizeof(value));
    return value;
}
// The site in the maintainer's eboot (an ELF from the launcher's out/eboot.elf).
static void check_reference(const char *path) {
    std::ifstream file(path, std::ios::binary);
    assert(file);
    std::vector<unsigned char> elf{std::istreambuf_iterator<char>(file), {}};
    assert(elf.size() >= 64 && !std::memcmp(elf.data(), "\x7f" "ELF", 4));
    uint16_t stride, count;
    std::memcpy(&stride, elf.data() + 0x36, 2);
    std::memcpy(&count, elf.data() + 0x38, 2);
    std::vector<unsigned char> image;
    for (unsigned i = 0; i < count; ++i) {
        const size_t at = static_cast<size_t>(read64(elf, 0x20)) + i * stride;
        uint32_t type;
        std::memcpy(&type, elf.data() + at, 4);
        if (type != 1) continue;
        const size_t offset = read64(elf, at + 8), address = read64(elf, at + 16), size = read64(elf, at + 32);
        assert(offset <= elf.size() && size <= elf.size() - offset && address + size <= 512 * 1024 * 1024);
        if (image.size() < address + size) image.resize(address + size);
        std::memcpy(image.data() + address, elf.data() + offset, size);
    }
    const uint64_t at = spartanport_signature_find(image.data(), image.size(), &spartanport_player_site);
    assert(at != UINT64_MAX && spartanport_player_match(image.data() + at, image.size() - at));
    std::printf("Player hook: signature at image+0x%llx\n", static_cast<unsigned long long>(at));
}

int main(int argc, char **argv) {
    auto *code = static_cast<unsigned char *>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT,
                                                         PAGE_EXECUTE_READWRITE));
    assert(code);
    std::memset(code, 0xcc, 4096);
    code[1024] = 0xc3;  // the resumed site
    uintptr_t slot = 0;
    const uintptr_t rate = 0x123456789a;
    assert(!spartanport_player_stub(code, 0, rate, (uintptr_t)(code + 1024)));
    const unsigned length = spartanport_player_stub(code, (uintptr_t)&slot, rate, (uintptr_t)(code + 1024));
    assert(length && length <= SPARTAN_PLAYER_STUB_SIZE);
    FlushInstructionCache(GetCurrentProcess(), code, 4096);

    Result result{};
    player_invoke(code, 0x10298d4b00, &result);
    assert(slot == 0x10298d4b00 && result.rcx == rate && result.rax == 0x1111222233334444ull);
    assert(result.flags_in == result.flags_out && result.red_zone == 1);
    player_invoke(code, 0x102d35aca0, &result);  // a later mover never replaces Kratos
    assert(slot == 0x10298d4b00 && result.rcx == rate && result.flags_in == result.flags_out);

    // Only lea rcx, [rip+disp] is the site.
    const unsigned char lea[] = {0x48, 0x8d, 0x0d, 0, 0, 0, 0}, other[] = {0x48, 0x8d, 0x05, 0, 0, 0, 0};
    assert(spartanport_player_match(lea, sizeof(lea)) && !spartanport_player_match(other, sizeof(other)));
    assert(!spartanport_player_match(lea, 6));
    if (argc > 1) check_reference(argv[1]);
    std::puts("Player hook: first object kept, lea replayed, rax, flags and red zone preserved");
    return 0;
}
