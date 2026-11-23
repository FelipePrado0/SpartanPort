// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: host subgroup policy for the game's shaders, written for the PS4's 64-wide waves
// (vk_pipeline_cache.cpp, vk_compute_pipeline.cpp, vk_graphics_pipeline.cpp).
//
// The defaults are shadPS4's on AMD and NVIDIA: every stage pinned to 64 lanes on a host that has
// them, no pin on a host with one width. A host whose subgroup width varies (Intel, 8 to 32) pins
// compute to the device's reported size, the width the recompiler assumed when it lowered wave64
// operations. The environment variables below select other policies for testing; neither changes
// the game for anyone who does not set it.

#pragma once

#include <cstdint>
#include <cstring>
#include <optional>

namespace Vulkan {

/// SPARTAN_SUBGROUP_PIN: which stages get a requiredSubgroupSize.
enum class SubgroupPinMode {
    Auto,    ///< "auto" (default): 64 on a 64-lane host, compute only on a variable-width host.
    All,     ///< "all": every stage on a 64-lane host, as shadPS4 does; nothing elsewhere.
    Allowed, ///< "allowed": only the stages the driver lists in requiredSubgroupSizeStages.
    None,    ///< "none": no pin at all.
};

inline SubgroupPinMode ParseSubgroupPinMode(const char* env) {
    if (env && std::strcmp(env, "all") == 0) {
        return SubgroupPinMode::All;
    }
    if (env && std::strcmp(env, "allowed") == 0) {
        return SubgroupPinMode::Allowed;
    }
    if (env && std::strcmp(env, "none") == 0) {
        return SubgroupPinMode::None;
    }
    return SubgroupPinMode::Auto;
}

/// The subgroup size a shader stage is pinned to with VK_EXT_subgroup_size_control.
///
/// A host with 64-wide subgroups (AMD) pins to 64 in "auto" and "all" whether or not the stage is
/// in requiredSubgroupSizeStages. A host whose width varies between `min_size` and `max_size`
/// (Intel) pins `default_size`, the device's reported subgroup size, so that a 64-thread workgroup
/// is not compiled at SIMD8/16 behind the recompiler's back; "auto" does it for compute only,
/// "allowed" for any stage the driver lists. A host with one width (NVIDIA) has nothing to pin.
inline std::optional<std::uint32_t> ChoosePinnedSubgroupSize(
    bool size_control, std::uint32_t min_size, std::uint32_t max_size, bool stage_pinnable,
    bool stage_is_compute, std::uint32_t default_size,
    SubgroupPinMode mode = SubgroupPinMode::Auto) {
    if (!size_control || mode == SubgroupPinMode::None) {
        return std::nullopt;
    }
    if (mode == SubgroupPinMode::All) {
        if (max_size >= 64) {
            return 64u;
        }
        return std::nullopt;
    }
    if (mode == SubgroupPinMode::Allowed && !stage_pinnable) {
        return std::nullopt;
    }
    if (max_size >= 64) {
        return 64u;
    }
    if (mode == SubgroupPinMode::Auto && !(stage_is_compute && stage_pinnable)) {
        return std::nullopt;
    }
    if (min_size >= max_size || default_size < min_size || default_size > max_size) {
        return std::nullopt;
    }
    return default_size;
}

/// Whether the recompiler must add barriers around LDS accesses of 64-thread compute workgroups.
/// On the PS4 such a workgroup is one wave in lockstep and the game's shaders omit the barriers.
/// `host_needs_barriers` is true for a host that cannot run it as one wave. `override` is
/// SPARTAN_LDS_BARRIERS: "0" or "1" (first character) forces the value, anything else is ignored.
inline bool NeedsLdsBarriers(bool host_needs_barriers, const char* override) {
    if (override && (override[0] == '0' || override[0] == '1')) {
        return override[0] == '1';
    }
    return host_needs_barriers;
}

} // namespace Vulkan
