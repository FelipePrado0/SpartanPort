// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: the guest memory span of a draw's vertex V# table (gpu/shim/spartanport_vertex_table.h),
// which stage A compares with the writes still queued for stage B.
#include "spartanport_vertex_table.h"
#include <cassert>
#include <cstdio>

using SpartanVertexTable::Span;

int main() {
    // Nothing added: no memory read, nothing to overlap.
    Span empty;
    assert(empty.Empty());
    assert(!empty.Overlaps(0, ~0ull));

    // Each V# is 16 bytes at pointer + dword_offset * 4; the span covers them all.
    Span span;
    span.Add(0x2000'0000, 4);  // 0x2000'0010 .. 0x2000'0020
    span.Add(0x2000'0000, 0);  // 0x2000'0000 .. 0x2000'0010
    span.Add(0x2000'0000, 12); // 0x2000'0030 .. 0x2000'0040
    assert(!span.Empty());
    assert(span.begin == 0x2000'0000 && span.end == 0x2000'0040);
    assert(span.Overlaps(0x2000'003c, 4));
    assert(span.Overlaps(0x1fff'fff0, 0x11));
    assert(!span.Overlaps(0x2000'0040, 0x100)); // ends are exclusive
    assert(!span.Overlaps(0x1fff'fff0, 0x10));

    // The pointer's top 16 bits are not address bits, as in Info::ReadUdReg.
    Span masked;
    masked.Add(0xabcd'0000'3000'0000ull, 0);
    assert(masked.begin == 0x3000'0000 && masked.end == 0x3000'0010);

    // A null table (below 0x10000) reads as empty descriptors: no memory.
    Span null_table;
    null_table.Add(0xffff, 2);
    assert(null_table.Empty());

    std::puts("PASS: vertex table");
    return 0;
}
