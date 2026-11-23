// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <cstdio>
#include <limits>

#include "video_core/texture_cache/types.h"

using VideoCore::SubresourceExtent;
using VideoCore::SubresourceRange;

static void TestViewRangeClamp() {
    struct Case {
        SubresourceExtent resources;
        SubresourceRange requested;
        SubresourceRange expected;
    };
    constexpr u32 max = std::numeric_limits<u32>::max();
    const Case cases[] = {
        {{4, 6}, {{1, 2}, {2, 3}}, {{1, 2}, {2, 3}}}, // Valid subview is unchanged.
        {{4, 6}, {{1, 2}, {3, 4}}, {{1, 2}, {3, 4}}}, // Exact end is valid.
        {{4, 6}, {{2, 1}, {4, 3}}, {{2, 1}, {2, 3}}}, // Mip-only overflow preserves layers.
        {{4, 6}, {{1, 4}, {2, 6}}, {{1, 4}, {2, 2}}}, // Layer-only overflow preserves mips.
        {{4, 6}, {{2, 4}, {4, 6}}, {{2, 4}, {2, 2}}}, // Overlap offsets shorten both counts.
        {{4, 6}, {{4, 6}, {2, 3}}, {{3, 5}, {1, 1}}}, // Base at the end moves inside.
        {{4, 6}, {{8, 9}, {2, 3}}, {{3, 5}, {1, 1}}},
        {{4, 6}, {{1, 2}, {0, 0}}, {{1, 2}, {1, 1}}}, // Empty views need positive counts.
        {{4, 6}, {{1, 2}, {0, 3}}, {{1, 2}, {1, 3}}},
        {{4, 6}, {{1, 2}, {2, 0}}, {{1, 2}, {2, 1}}},
        {{4, 6}, {{1, 2}, {max, max}}, {{1, 2}, {3, 4}}}, // Addition would wrap.
        {{4, 6}, {{max, max}, {1, 1}}, {{3, 5}, {1, 1}}},
        {{0, 0}, {{9, 9}, {3, 3}}, {{0, 0}, {1, 1}}}, // Retain the source's minimum size.
        {{max, max}, {{0, 0}, {max, max}}, {{0, 0}, {max, max}}},
        {{max, max}, {{max - 1, max - 1}, {2, 2}}, {{max - 1, max - 1}, {1, 1}}},
    };
    static_assert(noexcept(SubresourceRange{}.ClampTo(SubresourceExtent{})));
    for (const auto& test : cases) {
        auto range = test.requested;
        assert(range.ClampTo(test.resources) == (test.requested != test.expected));
        assert(range == test.expected);
        assert(!range.ClampTo(test.resources)); // Repeated cache hits are stable.
    }
}

int main() {
    TestViewRangeClamp();
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
    std::puts("SubresourceExtent: containment, equality and view range clamping passed");
}
