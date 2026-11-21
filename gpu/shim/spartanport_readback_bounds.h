// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: the address span that holds every pending GPU-to-RAM copy (texture_cache.cpp). A query
// outside it has nothing to wait for, without taking the readback mutex (ReadbackBounds, toggle
// 1 << 30). The span only grows while copies are pending and resets when none is left.
#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>

namespace SpartanReadback {

class Bounds {
public:
    /// Under the readback mutex, before the pending flag is published.
    void Add(std::uint64_t address, std::uint64_t size) {
        lo.store(std::min(lo.load(std::memory_order_relaxed), address), std::memory_order_relaxed);
        hi.store(std::max(hi.load(std::memory_order_relaxed), address + size),
                 std::memory_order_relaxed);
    }
    /// Under the readback mutex, when no copy is pending any more.
    void Reset() {
        lo.store(~std::uint64_t{0}, std::memory_order_relaxed);
        hi.store(0, std::memory_order_relaxed);
    }
    /// Lock-free: `[address, address + size)` misses every pending copy.
    [[nodiscard]] bool Outside(std::uint64_t address, std::uint64_t size) const {
        return address + size <= lo.load(std::memory_order_relaxed) ||
               address >= hi.load(std::memory_order_relaxed);
    }

private:
    std::atomic<std::uint64_t> lo{~std::uint64_t{0}};
    std::atomic<std::uint64_t> hi{0};
};

} // namespace SpartanReadback
