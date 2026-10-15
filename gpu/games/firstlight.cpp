// SPDX-License-Identifier: GPL-2.0-or-later
// inFAMOUS First Light CUSA00575.
//
// Stage 1 deliberately has no guessed GPU hooks, guest address ranges or shader hashes.
// Add game-specific data only after a reproducible CUSA00575 trace proves it is necessary.
#include "game_profile.h"

namespace Game {
namespace {
constexpr const char* Serials[] = {"CUSA00575"};
}

extern const Profile FirstLightProfile = {
    .name = "inFAMOUS First Light",
    .serials = Serials,
    .install_hooks = nullptr,
    .dynamic_writers = {},
    .buffer_copy_shader = 0,
};

} // namespace Game
