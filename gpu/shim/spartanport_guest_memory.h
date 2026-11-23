// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: guarded copies from guest memory for readers that follow guest pointers (descriptor
// tables in shader user data). A table can be unmapped after the shader that reads it was built.
#pragma once

#include <atomic>
#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "spartanport_toggles.h"

namespace SpartanGuest {

/// Copies size bytes from the guest address into destination. Returns false, with destination
/// zeroed, when the address is null or the copy faults. When the calling thread already has a
/// recovery point (a draw-preparation worker), a fault goes to that owner, which abandons its
/// work. Otherwise the recovery point lives in this helper's own frame, so the jump never crosses
/// the caller's C++ objects; noinline keeps that frame separate.
[[gnu::noinline]] inline bool Read(void* destination, std::uintptr_t address,
                                   std::size_t size) noexcept {
    if (address == 0) {
        std::memset(destination, 0, size);
        return false;
    }
    if (runtime_fault_recover) {
        std::memcpy(destination, reinterpret_cast<const void*>(address), size);
        return true;
    }
    sigjmp_buf recover;
    if (sigsetjmp(recover, 0)) {
        // The fault handler has cleared runtime_fault_recover. Discard the partial copy.
        runtime_fault_recover = nullptr;
        std::memset(destination, 0, size);
        return false;
    }
    // The fences keep the compiler from dropping the store of the recovery point as dead or
    // moving the copy across it: only the fault handler reads runtime_fault_recover here.
    runtime_fault_recover = &recover;
    std::atomic_signal_fence(std::memory_order_seq_cst);
    std::memcpy(destination, reinterpret_cast<const void*>(address), size);
    std::atomic_signal_fence(std::memory_order_seq_cst);
    runtime_fault_recover = nullptr;
    return true;
}

} // namespace SpartanGuest
