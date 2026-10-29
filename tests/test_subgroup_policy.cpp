// SPDX-License-Identifier: GPL-2.0-or-later
// Host subgroup policy for the game's wave64 shaders (vk_subgroup_policy.h). No game, Vulkan
// device or window required.
#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "video_core/renderer_vulkan/vk_subgroup_policy.h"

using namespace Vulkan;

int main() {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    // Auto (default): AMD keeps shadPS4's pin on every stage, Intel pins compute only,
    // NVIDIA (one width) gets nothing.
    assert(ChoosePinnedSubgroupSize(true, 32, 64, true, true, 64) == 64u);    // AMD compute
    assert(ChoosePinnedSubgroupSize(true, 32, 64, false, false, 64) == 64u);  // AMD fragment
    assert(!ChoosePinnedSubgroupSize(true, 32, 32, true, true, 32));          // NVIDIA
    assert(ChoosePinnedSubgroupSize(true, 8, 32, true, true, 32) == 32u);     // Intel compute
    assert(ChoosePinnedSubgroupSize(true, 16, 32, true, true, 32) == 32u);
    assert(!ChoosePinnedSubgroupSize(true, 8, 32, true, false, 32));          // Intel fragment
    assert(!ChoosePinnedSubgroupSize(true, 8, 32, false, true, 32));          // not pinnable
    assert(!ChoosePinnedSubgroupSize(true, 8, 16, true, true, 32));           // size out of range
    assert(!ChoosePinnedSubgroupSize(false, 8, 32, true, true, 32));          // no size control

    // All: shadPS4's behaviour.
    assert(ChoosePinnedSubgroupSize(true, 32, 64, false, false, 64, SubgroupPinMode::All) == 64u);
    assert(!ChoosePinnedSubgroupSize(true, 8, 32, true, true, 32, SubgroupPinMode::All));

    // Allowed: only the stages the driver lists.
    assert(ChoosePinnedSubgroupSize(true, 32, 64, true, true, 64, SubgroupPinMode::Allowed) ==
           64u);
    assert(!ChoosePinnedSubgroupSize(true, 32, 64, false, false, 64, SubgroupPinMode::Allowed));
    assert(ChoosePinnedSubgroupSize(true, 8, 32, true, false, 32, SubgroupPinMode::Allowed) ==
           32u); // any listed stage
    assert(!ChoosePinnedSubgroupSize(true, 32, 32, true, true, 32, SubgroupPinMode::Allowed));

    // None: no pin anywhere.
    assert(!ChoosePinnedSubgroupSize(true, 32, 64, true, true, 64, SubgroupPinMode::None));
    assert(!ChoosePinnedSubgroupSize(true, 8, 32, true, true, 32, SubgroupPinMode::None));

    // SPARTAN_SUBGROUP_PIN parsing: anything unknown is Auto.
    assert(ParseSubgroupPinMode(nullptr) == SubgroupPinMode::Auto);
    assert(ParseSubgroupPinMode("") == SubgroupPinMode::Auto);
    assert(ParseSubgroupPinMode("auto") == SubgroupPinMode::Auto);
    assert(ParseSubgroupPinMode("garbage") == SubgroupPinMode::Auto);
    assert(ParseSubgroupPinMode("all") == SubgroupPinMode::All);
    assert(ParseSubgroupPinMode("allowed") == SubgroupPinMode::Allowed);
    assert(ParseSubgroupPinMode("none") == SubgroupPinMode::None);

    // SPARTAN_LDS_BARRIERS: "0"/"1" force the value, anything else keeps the host's.
    assert(NeedsLdsBarriers(false, nullptr) == false);
    assert(NeedsLdsBarriers(true, nullptr) == true);
    assert(NeedsLdsBarriers(false, "1") == true);
    assert(NeedsLdsBarriers(true, "0") == false);
    assert(NeedsLdsBarriers(true, "x") == true);

    std::puts("subgroup-policy-test: ok");
    return 0;
}
