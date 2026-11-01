// The wall seam fix: the reuse rule, its stub and the patched site's signature.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
#include <windows.h>
#include "../src/ledge_hook.h"

// Enters a stub the way the patched call site does: rbp is the game frame, rbx the object
// and rsp 16-byte aligned, then returns the eax the stub resumes with.
extern "C" __attribute__((sysv_abi)) uint32_t ledge_invoke(void *code, uintptr_t frame, uintptr_t object);
asm(".globl ledge_invoke\nledge_invoke:\n"
    "push %rbp\npush %rbx\nmov %rsi, %rbp\nmov %rdx, %rbx\ncall *%rdi\npop %rbx\npop %rbp\nret\n");
static uint32_t ray_result, ray_calls;
alignas(16) static uint32_t ray_global[24];
extern "C" __attribute__((sysv_abi)) uint32_t fake_ledge_ray() {
    ++ray_calls;
    ray_global[0] = ray_result ? 0x40000000 + ray_calls : 0xdead;
    return ray_result;
}
static void ledge_seam_fix(unsigned char *code) {
    alignas(16) unsigned char stack[0x400]{};
    const uintptr_t frame = (uintptr_t)(stack + 0x380);
    uint32_t object[0x200 / 4]{};
    object[0x1c0 / 4] = 0x10;
    float *position = (float *)(frame - 0x230);
    position[0] = 302.729f; position[1] = -51.604f; position[2] = 180.685f;
    SpartanLedgeCache cache{};
    cache.result = (uintptr_t)ray_global;
    // A hit is kept unchanged and cached.
    ray_global[5] = 0x1234;
    assert(spartanport_ledge_ray(&cache, frame, (uintptr_t)object, 3) == 3);
    assert(cache.hits == 1 && cache.hit == 3 && cache.cache[5] == 0x1234 && cache.object == (uintptr_t)object);
    // A miss inside the seam (0.006 further) returns the cached hit.
    position[2] = 180.691f;
    ray_global[5] = 0;
    assert(spartanport_ledge_ray(&cache, frame, (uintptr_t)object, 0) == 3);
    assert(cache.reused == 1 && cache.misses == 1 && ray_global[5] == 0x1234);
    // Still measured from the real hit, not from the reused one.
    position[2] = 180.685f + SPARTAN_LEDGE_REUSE + 0.001f;
    ray_global[5] = 0;
    assert(spartanport_ledge_ray(&cache, frame, (uintptr_t)object, 0) == 0);
    assert(cache.reused == 1 && cache.misses == 2 && ray_global[5] == 0);
    // Another object never takes this one's hit.
    uint32_t other[0x200 / 4]{};
    other[0x1c0 / 4] = 0x10;
    position[2] = 180.686f;
    assert(spartanport_ledge_ray(&cache, frame, (uintptr_t)other, 0) == 0 && cache.reused == 1);
    // Outside the wall traversal the result passes through and nothing is cached.
    other[0x1c0 / 4] = 1;
    assert(spartanport_ledge_ray(&cache, frame, (uintptr_t)other, 5) == 5 && cache.hits == 1);
    assert(spartanport_ledge_ray(&cache, frame, (uintptr_t)other, 0) == 0 && cache.misses == 3);

    // The stub calls the game's ray, then the helper, and resumes after the original call.
    cache = {};
    cache.result = (uintptr_t)ray_global;
    std::memset(code, 0xcc, 4096);
    code[1024] = 0xc3;
    const unsigned length = spartanport_ledge_stub(code, (uintptr_t)&cache, (uintptr_t)spartanport_ledge_ray,
                                                   (uintptr_t)fake_ledge_ray, (uintptr_t)(code + 1024));
    assert(length && length <= SPARTAN_LEDGE_STUB_SIZE);
    assert(!spartanport_ledge_stub(code, (uintptr_t)&cache, (uintptr_t)spartanport_ledge_ray, 0, (uintptr_t)(code + 1024)));
    FlushInstructionCache(GetCurrentProcess(), code, 4096);
    position[2] = 180.685f;
    ray_result = 1;
    assert(ledge_invoke(code, frame, (uintptr_t)object) == 1 && ray_calls == 1 && cache.hits == 1);
    const uint32_t kept = ray_global[0];
    ray_result = 0;
    position[2] = 180.69f;
    assert(ledge_invoke(code, frame, (uintptr_t)object) == 1 && ray_calls == 2 && cache.reused == 1);
    assert(ray_global[0] == kept);
    position[2] = 181.0f;
    assert(ledge_invoke(code, frame, (uintptr_t)object) == 0 && ray_global[0] == 0xdead);

    // The site exists only when the launcher patch swaps 31 c9 for 33 c9 (same xor ecx, ecx).
    int pattern[SPARTAN_SIGNATURE_MAX];
    const unsigned size = spartanport_signature_parse(spartanport_ledge_site.pattern, pattern);
    assert(size);
    std::vector<unsigned char> image(256);
    for (unsigned i = 0; i < size; ++i) image[32 + i] = pattern[i] < 0 ? 0 : (unsigned char)pattern[i];
    assert(spartanport_signature_find(image.data(), image.size(), &spartanport_ledge_site) == 32);
    image[32 + 5] = 0x31;
    assert(spartanport_signature_find(image.data(), image.size(), &spartanport_ledge_site) == UINT64_MAX);
}
static uint64_t read64(const std::vector<unsigned char>& data, size_t at) {
    uint64_t value;
    assert(at + sizeof(value) <= data.size());
    std::memcpy(&value, data.data() + at, sizeof(value));
    return value;
}
// The maintainer's eboot (the launcher's out/eboot.elf): no site unpatched, one with the patch's 33 c9.
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
    assert(spartanport_signature_find(image.data(), image.size(), &spartanport_ledge_site) == UINT64_MAX);
    const SpartanSignature original = {"e8 ?? ?? ?? ?? 31 c9 85 c0 0f 84 ?? ?? ?? ?? 48 8d 05 ?? ?? ?? ?? c5 f8 28 00", 5};
    const uint64_t swap = spartanport_signature_find(image.data(), image.size(), &original);
    assert(swap != UINT64_MAX);
    image[swap] = 0x33;
    const uint64_t at = spartanport_signature_find(image.data(), image.size(), &spartanport_ledge_site);
    assert(at == swap - 5);
    std::printf("Wall seam fix: signature at image+0x%llx\n", static_cast<unsigned long long>(at));
}

int main(int argc, char **argv) {
    auto *code = static_cast<unsigned char *>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT,
                                                         PAGE_EXECUTE_READWRITE));
    assert(code);
    ledge_seam_fix(code);
    if (argc > 1) check_reference(argv[1]);
    std::puts("Wall seam fix: hit, near miss, far miss, other object, stub and signature OK");
    return 0;
}
