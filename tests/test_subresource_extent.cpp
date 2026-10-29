// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <cstdio>
#include <limits>

#include "video_core/texture_cache/types.h"

using VideoCore::SubresourceExtent;

int main() {
    struct Case {
        SubresourceExtent cached;
        SubresourceExtent requested;
        bool contains;
    };
    constexpr u32 max = std::numeric_limits<u32>::max();
    const Case cases[] = {
        {{1, 1}, {1, 1}, true},
        {{4, 2}, {3, 1}, true},
        {{4, 1}, {3, 2}, false}, // More mips cannot cover missing layers.
        {{3, 2}, {4, 1}, false}, // More layers cannot cover missing mips.
        {{3, 1}, {3, 2}, false},
        {{3, 2}, {3, 1}, true},
        {{3, 2}, {4, 2}, false},
        {{4, 2}, {3, 2}, true},
        {{2, 1}, {3, 2}, false},
        {{0, 0}, {0, 0}, true},
        {{max, max}, {max, max}, true},
        {{max, max - 1}, {max - 1, max}, false},
        {{max - 1, max}, {max, max - 1}, false},
    };
    static_assert(noexcept(SubresourceExtent{}.Contains(SubresourceExtent{})));
    for (const auto& test : cases) {
        assert(test.cached.Contains(test.requested) == test.contains);
        assert(test.cached.Contains(test.cached));
    }
    const SubresourceExtent extent{3, 2};
    assert((extent == SubresourceExtent{3, 2}));
    assert((extent != SubresourceExtent{3, 1}));
    std::puts("SubresourceExtent: per-dimension containment and equality passed");
}
