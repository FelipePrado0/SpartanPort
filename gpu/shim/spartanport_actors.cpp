// SPDX-License-Identifier: GPL-2.0-or-later
#include "spartanport_actors.h"
#include "spartanport_settings.h"
#include <cstdint>
#include "../../src/actor_hook.h"

alignas(64) SpartanActorSlot spartanport_actor_slots[2];

extern "C" uintptr_t spartanport_actor_slots_address(void) {
    return reinterpret_cast<uintptr_t>(spartanport_actor_slots);
}

extern "C" void spartanport_actor_status(int supported) {
    SpartanSettings::Get().actor_watch_supported = supported != 0;
}
