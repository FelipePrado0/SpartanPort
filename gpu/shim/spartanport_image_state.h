// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: when UpdateImage has nothing to do for an image (TextureCache::IsUpToDate and the
// fast path of UpdateImage). A guest MSAA image is never refreshed from guest memory
// (RefreshImage returns before clearing Dirty): Dirty does not stop it from being up to date,
// or the texture sets that hold it miss on every draw (MsaaDirtyIgnored, toggle 1 << 63).
#pragma once

namespace SpartanImageState {

struct Inputs {
    bool registered;
    bool dirty;
    bool aliased;
    unsigned guest_samples;
    bool backing_writes; ///< TextureCache has backing writes to apply
    bool tracked;        ///< the image's whole guest range is tracked
    bool touched;        ///< touched in this garbage collection period
    bool msaa_dirty_ignored;
};

[[nodiscard]] constexpr bool UpToDate(const Inputs& in) {
    const bool stale = in.dirty && !(in.msaa_dirty_ignored && in.guest_samples > 1);
    return in.registered && !stale && !in.aliased && !in.backing_writes && in.tracked &&
           in.touched;
}

} // namespace SpartanImageState
