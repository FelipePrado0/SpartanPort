// Damage and green/blue/gold orb multipliers (src/damage_hook.h, src/orb_hook.h).
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include "../src/damage_hook.h"
#include "../src/orb_hook.h"
#include "spartanport_settings.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static unsigned char* Executable() {
#ifdef _WIN32
    auto* code = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
#else
    auto* code = static_cast<unsigned char*>(mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    assert(code != MAP_FAILED);
#endif
    assert(code);
    return code;
}

static void Seal(unsigned char* code) {
#ifdef _WIN32
    DWORD previous;
    assert(VirtualProtect(code, 4096, PAGE_EXECUTE_READ, &previous));
    FlushInstructionCache(GetCurrentProcess(), code, 4096);
#else
    assert(!mprotect(code, 4096, PROT_READ | PROT_EXEC));
#endif
}

static uintptr_t Address(const void* p) {
    return reinterpret_cast<uintptr_t>(p);
}

// Runs the damage stub as the game's routine entry would: rdi the actor, xmm0 the damage, rbp the
// routine's frame. Returns the damage the routine goes on with (xmm0) and checks the replayed
// instructions stored it in the frame and set r13.
static float Damage(bool player, float damage, float taken, float dealt) {
    unsigned char* code = Executable();
    const unsigned char entry[] = {0x55, 0x41,0x55, 0x48,0x89,0xf5, 0xff,0x25,0,0,0,0};
    std::memcpy(code, entry, sizeof(entry));
    const uintptr_t stub = Address(code + 64), exit = Address(code + 512);
    std::memcpy(code + sizeof(entry), &stub, 8);
    assert(spartanport_damage_build_stub(code + 64, Address(&taken), Address(&dealt), exit) < 448);
    const unsigned char leave[] = {0x4c,0x89,0x2a, 0x41,0x5d, 0x5d, 0xc3};  // mov [rdx], r13
    std::memcpy(code + 512, leave, sizeof(leave));
    Seal(code);
    std::vector<unsigned char> actor(0x400);
    actor[0x298] = player ? 0 : 1;
    float frame[32]{};
    float* top = frame + 32;
    void* r13 = nullptr;
    const float result = reinterpret_cast<float(__attribute__((sysv_abi))*)(void*, void*, void**, float)>(code)(
        actor.data(), top, &r13, damage);
    float stored;
    std::memcpy(&stored, reinterpret_cast<unsigned char*>(top) - 0x60, 4);
    assert(r13 == actor.data());
    assert(std::memcmp(&stored, &result, 4) == 0);
    return result;
}

struct Amounts {
    float health, magic, rage;
};

// Runs the pickup stub with the routine's arguments; the resume point stores them and undoes
// the prologue the stub replayed.
static Amounts Pickup(Amounts in, float green, float blue, float gold) {
    unsigned char* code = Executable();
    assert(spartanport_orb_pickup_build_stub(code, Address(&green), Address(&blue), Address(&gold),
                                      Address(code + 512)) < 512);
    const unsigned char leave[] = {0xc5,0xfa,0x11,0x07, 0xc5,0xfa,0x11,0x4f,0x04, 0xc5,0xfa,0x11,0x57,0x08,
                                   0x48,0x83,0xc4,0x18, 0x5b, 0x5d, 0xc3};
    std::memcpy(code + 512, leave, sizeof(leave));
    Seal(code);
    Amounts out{};
    reinterpret_cast<void(__attribute__((sysv_abi))*)(Amounts*, float, float, float)>(code)(
        &out, in.health, in.magic, in.rage);
    return out;
}

static std::string Read(const char* path) {
    std::ifstream in(path);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

int main(int argc, char** argv) {
    assert(spartanport_damage_signature_match(spartanport_damage_original, SPARTAN_DAMAGE_HOOK_LENGTH));
    assert(!spartanport_damage_signature_match(spartanport_damage_original, SPARTAN_DAMAGE_HOOK_LENGTH - 1));
    assert(spartanport_orb_pickup_signature_match(spartanport_orb_pickup_original, SPARTAN_ORB_PICKUP_LENGTH));
    if (argc > 1) {
        std::ifstream file(argv[1], std::ios::binary);
        std::vector<unsigned char> image((std::istreambuf_iterator<char>(file)), {});
        assert(image.size() > 0x4000 + SPARTAN_ORB_PICKUP_OFFSET + SPARTAN_ORB_PICKUP_LENGTH);
        assert(spartanport_damage_signature_match(image.data() + 0x4000 + SPARTAN_DAMAGE_HOOK_OFFSET, SPARTAN_DAMAGE_HOOK_LENGTH));
        assert(spartanport_orb_pickup_signature_match(image.data() + 0x4000 + SPARTAN_ORB_PICKUP_OFFSET, SPARTAN_ORB_PICKUP_LENGTH));
    }

    assert(Damage(true, 10, 2, 5) == 20);       // player: taken
    assert(Damage(false, 10, 2, 5) == 50);      // enemy: dealt, before the health thresholds
    assert(Damage(false, 10, 0.5f, 0.1f) == 1);
    assert(Damage(true, -10, 2, 5) == -10);     // healing stays
    assert(Damage(false, 0, 2, 5) == 0);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float left = Damage(false, nan, 2, 5);
    assert(left != left);

    Amounts a = Pickup({10, 20, 30}, 2, 3, 0.5f);
    assert(a.health == 20 && a.magic == 60 && a.rage == 15);
    a = Pickup({0, -5, 4}, 2, 3, 100);
    assert(a.health == 0 && a.magic == -5 && a.rage == 400);

    const char* path = "stat-multipliers-test.ini";
    {
        std::ofstream out(path);
        out << "damage_dealt=2.5\ndamage_taken=0.5\ngreen_orb_multiplier=3\n"
               "blue_orb_multiplier=1000\ngold_orb_multiplier=0\n";
    }
#ifdef _WIN32
    _putenv_s("SPARTAN_CONFIG", path);
#else
    setenv("SPARTAN_CONFIG", path, 1);
#endif
    auto& v = SpartanSettings::Get();
    SpartanSettings::Load();
    assert(v.damage_dealt == 2.5f && v.damage_taken == 0.5f && v.green_orb_multiplier == 3.0f);
    assert(v.blue_orb_multiplier == 100.0f && v.gold_orb_multiplier == 0.1f);
    v.gold_orb_multiplier = 4.0f;
    SpartanSettings::Save();
    const std::string saved = Read(path);
    for (const char* line : {"damage_dealt=2.500\n", "damage_taken=0.500\n", "green_orb_multiplier=3.000\n",
                             "blue_orb_multiplier=100.000\n", "gold_orb_multiplier=4.000\n"}) {
        assert(saved.find(line) != std::string::npos);
    }
    std::remove(path);
    std::puts("PASS: stat multipliers");
}
