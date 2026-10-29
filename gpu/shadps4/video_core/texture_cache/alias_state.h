// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"
#include "video_core/texture_cache/types.h"

namespace VideoCore {

/// spartanport: images with the same guest address (TextureCache::alias_states, guarded by its
/// `mutex`). `backing` holds the complete contents; `writer` is a newer write not yet copied into
/// it. Kept apart from TextureCache so aliasing-test can drive it without a Vulkan device.
struct AliasState {
    u64 backing_uid{};
    u64 writer_uid{};
    ImageId backing{};
    ImageId writer{};
    u32 members{};
    void ResetAuthority() {
        const u32 count = members;
        *this = {};
        members = count;
    }
};

/// Before reading the authority: a backing that is gone gives way to a live writer.
/// False when neither is live (the authority is reset). `is_live(id, uid)`.
template <typename IsLive>
[[nodiscard]] bool ResolveAliasBacking(AliasState& state, IsLive&& is_live) {
    if (is_live(state.backing, state.backing_uid)) {
        return true;
    }
    if (!is_live(state.writer, state.writer_uid)) {
        state.ResetAuthority();
        return false;
    }
    state.backing = state.writer, state.backing_uid = state.writer_uid;
    state.writer = {}, state.writer_uid = 0;
    return true;
}

/// An image leaves the cache (TextureCache::DeleteImage). Its slot is erased later, so it still
/// looks live: the state must stop naming it here. A deleted backing gives way to a live writer,
/// as in ResolveAliasBacking: resetting dropped the writer's newer contents for the other aliases
/// until it was written again.
template <typename IsLive>
void ForgetAliasMember(AliasState& state, ImageId image_id, u64 image_uid, IsLive&& is_live) {
    if (state.writer == image_id && state.writer_uid == image_uid) {
        state.ResetAuthority();
    } else if (state.backing == image_id && state.backing_uid == image_uid) {
        state.backing = {}, state.backing_uid = 0;
        (void)ResolveAliasBacking(state, is_live);
    }
}

} // namespace VideoCore
