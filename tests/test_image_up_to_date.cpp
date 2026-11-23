// SPDX-License-Identifier: GPL-2.0-or-later
// When UpdateImage has nothing to do for an image (TextureCache::IsUpToDate). A guest MSAA image is
// never refreshed from guest memory (RefreshImage returns before clearing Dirty), so Dirty alone
// must not keep sending it, and every texture set holding it, down the slow path.
#include <cassert>
#include <cstdio>
#include <initializer_list>

#include "spartanport_image_state.h"

int main() {
    using SpartanImageState::UpToDate;
    SpartanImageState::Inputs clean{.registered = true, .dirty = false, .aliased = false,
                                    .guest_samples = 1, .backing_writes = false, .tracked = true,
                                    .touched = true, .msaa_dirty_ignored = true};
    assert(UpToDate(clean));

    auto dirty = clean;
    dirty.dirty = true;
    assert(!UpToDate(dirty)); // single-sampled: Dirty means an upload

    auto msaa = dirty;
    msaa.guest_samples = 2;
    assert(UpToDate(msaa)); // nothing RefreshImage would do
    msaa.msaa_dirty_ignored = false;
    assert(!UpToDate(msaa)); // the toggle restores the old rule

    for (auto other : {&SpartanImageState::Inputs::registered, &SpartanImageState::Inputs::tracked,
                       &SpartanImageState::Inputs::touched}) {
        auto off = clean;
        off.guest_samples = 2;
        off.dirty = true;
        off.*other = false;
        assert(!UpToDate(off)); // the other conditions still apply to MSAA images
    }
    auto aliased = clean;
    aliased.aliased = true;
    assert(!UpToDate(aliased));
    auto writes = clean;
    writes.backing_writes = true;
    assert(!UpToDate(writes));
    std::printf("PASS: image up to date\n");
}
