// Hook sites found by signature (src/signature.h): in the reference eboot.bin and in a build whose
// code moved, as the full 01.02 update's does (+0x120 in one region, -0x10 in another).
#include <cassert>
#include <cstdio>
#include <vector>
#include "../src/cheat_hook.h"
#include "../src/actor_hook.h"
#include "../src/damage_hook.h"
#include "../src/orb_hook.h"

struct Site { const SpartanSignature* sig; uint64_t reference; };

static uint64_t Find(const std::vector<unsigned char>& image, const SpartanSignature& sig) {
    return spartanport_signature_find(image.data(), image.size(), &sig);
}

int main(int argc, char** argv) {
    int parsed[SPARTAN_SIGNATURE_MAX];
    assert(spartanport_signature_parse("c5 ?? 0a", parsed) == 3 && parsed[0] == 0xc5 && parsed[1] == -1 && parsed[2] == 10);
    assert(!spartanport_signature_parse("c5 g0", parsed) && !spartanport_signature_parse("c5 0", parsed));

    std::vector<unsigned char> buffer(256);
    const unsigned char bytes[] = {0x48, 0x8d, 0x05, 1, 2, 3, 4, 0xc3};
    const SpartanSignature sig = {"48 8d 05 ?? ?? ?? ?? c3", 3};
    assert(Find(buffer, sig) == UINT64_MAX);                       // absent
    memcpy(buffer.data() + 100, bytes, sizeof(bytes));
    assert(Find(buffer, sig) == 103);                              // the site, wildcards ignored
    buffer[103] = 9;
    assert(Find(buffer, sig) == 103);
    memcpy(buffer.data() + 200, bytes, sizeof(bytes));
    assert(Find(buffer, sig) == UINT64_MAX);                       // twice: ambiguous
    memcpy(buffer.data() + 248, bytes, sizeof(bytes));             // a match at the very end counts
    buffer[200] = 0;
    assert(Find(buffer, sig) == UINT64_MAX);
    buffer[100] = 0;
    assert(Find(buffer, sig) == 251);

    if (argc < 2) {
        std::puts("signatures-test: OK (search; pass out/eboot.elf to check the game's hooks)");
        return 0;
    }
    FILE* file = std::fopen(argv[1], "rb");
    assert(file && !std::fseek(file, 0, SEEK_END));
    std::vector<unsigned char> image(std::ftell(file) - 0x4000);  // the text segment, at vaddr 0
    assert(!std::fseek(file, 0x4000, SEEK_SET) && std::fread(image.data(), 1, image.size(), file) == image.size());
    std::fclose(file);

    std::vector<Site> sites;
    for (const auto& hook : spartanport_cheat_hooks) sites.push_back({&hook.site, hook.offset});
    sites.push_back({&spartanport_actor_site, SPARTAN_ACTOR_HOOK_OFFSET});
    sites.push_back({&spartanport_damage_site, SPARTAN_DAMAGE_HOOK_OFFSET});
    sites.push_back({&spartanport_orb_pickup_site, SPARTAN_ORB_PICKUP_OFFSET});
    sites.push_back({&spartanport_orb_setter_site, 0x6a9f0});
    const uint64_t calls[] = {0x6a775, 0x6fa33, 0x6fbd7};
    for (unsigned i = 0; i < 3; ++i) sites.push_back({&spartanport_orb_call_sites[i], calls[i]});
    for (const auto& site : sites) assert(Find(image, *site.sig) == site.reference);
    uint64_t found[3];
    assert(spartanport_orb_find(image.data(), image.size(), found) == 0x6a9f0);
    for (unsigned i = 0; i < 3; ++i) assert(found[i] == calls[i]);

    // Another build: 0x120 bytes more before 0x60000, 0x10 fewer at 0x90000.
    std::vector<unsigned char> moved(image.begin(), image.begin() + 0x60000);
    moved.insert(moved.end(), 0x120, 0xcc);
    moved.insert(moved.end(), image.begin() + 0x60000, image.begin() + 0x90000);
    moved.insert(moved.end(), image.begin() + 0x90010, image.end());
    const auto shifted = [](uint64_t at) { return at < 0x60000 ? at : at < 0x90000 ? at + 0x120 : at + 0x110; };
    for (const auto& site : sites) assert(Find(moved, *site.sig) == shifted(site.reference));
    assert(spartanport_orb_find(moved.data(), moved.size(), found) == shifted(0x6a9f0));
    for (const auto& hook : spartanport_cheat_hooks) {
        const uint64_t at = Find(moved, hook.site);
        assert(spartanport_cheat_signature_match(&hook, moved.data() + at, moved.size() - at));
    }
    // A call that no longer reaches the setter leaves the orb multiplier off.
    moved[shifted(0x6fa33) + 1] ^= 1;
    assert(spartanport_orb_find(moved.data(), moved.size(), found) == UINT64_MAX);
    std::printf("signatures-test: OK (%zu hook sites in the reference build and in a moved build)\n", sites.size());
}
