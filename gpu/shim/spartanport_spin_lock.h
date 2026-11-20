// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: a recursive lock that spins (then yields) instead of sleeping, for the short holds
// of the parallel draw resolvers (docs/plans/2026-11-parallel-recording-spec.md). Lockable, so
// std::unique_lock works with it.
#pragma once

#include <atomic>
#include <cstdint>
#include <thread>

namespace SpartanSpin {

class RecursiveLock {
public:
    void lock() {
        const std::uintptr_t me = Self();
        if (owner.load(std::memory_order_relaxed) == me) {
            ++depth;
            return;
        }
        for (unsigned spins = 0;; ++spins) {
            std::uintptr_t expected = 0;
            if (owner.load(std::memory_order_relaxed) == 0 &&
                owner.compare_exchange_weak(expected, me, std::memory_order_acquire,
                                            std::memory_order_relaxed)) {
                break;
            }
            if (spins < 256) {
                __builtin_ia32_pause();
            } else {
                std::this_thread::yield();
            }
        }
        depth = 1;
    }

    bool try_lock() {
        const std::uintptr_t me = Self();
        if (owner.load(std::memory_order_relaxed) == me) {
            ++depth;
            return true;
        }
        std::uintptr_t expected = 0;
        if (owner.compare_exchange_strong(expected, me, std::memory_order_acquire,
                                          std::memory_order_relaxed)) {
            depth = 1;
            return true;
        }
        return false;
    }

    void unlock() {
        if (--depth == 0) {
            owner.store(0, std::memory_order_release);
        }
    }

private:
    /// A nonzero value unique to the calling thread while it lives.
    static std::uintptr_t Self() {
        thread_local const char tag = 0;
        return reinterpret_cast<std::uintptr_t>(&tag);
    }
    std::atomic<std::uintptr_t> owner{0};
    unsigned depth = 0; ///< only the owner touches it
};

} // namespace SpartanSpin
