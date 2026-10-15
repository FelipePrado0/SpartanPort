// SPDX-License-Identifier: GPL-2.0-or-later
#include "game_profile.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" std::uint64_t runtime_disabled_optimizations;

namespace Game {

extern const Profile FirstLightProfile;

namespace {
constexpr std::array<const Profile*, 1> Profiles = {&FirstLightProfile};
std::atomic<const Profile*> active{nullptr};
}

void Select(const char* serial) {
    const char* env = std::getenv("FIRSTLIGHT_GAME_PROFILE");
    if (env && std::strcmp(env, "none") == 0) {
        active.store(nullptr, std::memory_order_release);
        __atomic_store_n(&runtime_disabled_optimizations,0,__ATOMIC_RELEASE);
        std::printf("Game profile: none (FIRSTLIGHT_GAME_PROFILE=none)\n");
        return;
    }

    for (const Profile* profile : Profiles) {
        const bool match = serial && std::ranges::any_of(profile->serials, [&](const char* value) {
            return std::strcmp(value, serial) == 0;
        });
        if (match) {
            active.store(profile, std::memory_order_release);
            __atomic_store_n(&runtime_disabled_optimizations,profile->disabled_optimizations,__ATOMIC_RELEASE);
            std::printf("Game profile: %s (%s); disabled GPU optimizations=0x%llx\n",\n                        profile->name, serial,\n                        static_cast<unsigned long long>(profile->disabled_optimizations));
            return;
        }
    }

    active.store(nullptr, std::memory_order_release);
    __atomic_store_n(&runtime_disabled_optimizations,0,__ATOMIC_RELEASE);
    std::printf("Game profile: none for %s; generic GPU paths only\n", serial ? serial : "?");
}

const Profile* Active() {
    return active.load(std::memory_order_acquire);
}

bool IsDynamicWriter(std::uint64_t guest_rip) {
    const Profile* profile = Active();
    return profile && std::ranges::any_of(profile->dynamic_writers, [&](const CodeRange& range) {
        return guest_rip >= range.begin && guest_rip < range.end;
    });
}

} // namespace Game
