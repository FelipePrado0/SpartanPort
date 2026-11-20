// SPDX-License-Identifier: GPL-2.0-or-later
// The span of pending readbacks: queries outside it skip the readback mutex, inside it they still
// go to the exact check.
#include <cassert>
#include <cstdio>

#include "spartanport_readback_bounds.h"

int main() {
    SpartanReadback::Bounds bounds;
    assert(bounds.Outside(0x1000, 0x100)); // nothing pending
    bounds.Add(0x10ce8d0000, 256);
    bounds.Add(0x10b9a02c00, 4);
    assert(!bounds.Outside(0x10b9a02c00, 4));  // a pending copy
    assert(!bounds.Outside(0x10c0000000, 64)); // between them: still checked exactly
    assert(!bounds.Outside(0x10ce8d00ff, 1));  // the last byte
    assert(bounds.Outside(0x10ce8d0100, 64));  // just past the span
    assert(bounds.Outside(0x10b9a02b00, 0x100)); // ends where the span starts
    assert(!bounds.Outside(0x10b9a02b00, 0x101));
    bounds.Reset();
    assert(bounds.Outside(0x10ce8d0000, 256));
    std::printf("PASS: readback bounds\n");
}
