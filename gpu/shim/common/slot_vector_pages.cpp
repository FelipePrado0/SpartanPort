// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: reserve storage for Common::SlotVector so entries stay fixed as the pool grows.

#include <cstddef>
#include <cstdio>
#include <cstdlib>

#include "common/slot_vector.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace Common {

namespace {
[[noreturn]] void PageFailure(const char* operation) noexcept {
    std::fprintf(stderr, "SlotVector: %s failed\n", operation);
    std::abort();
}
} // namespace

void* SlotVectorReservePages(std::size_t size) noexcept {
#ifdef _WIN32
    void* const base = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_NOACCESS);
    if (!base) {
        PageFailure("address-space reservation");
    }
#else
    const long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0 || size % static_cast<std::size_t>(page_size) != 0) {
        PageFailure("reservation alignment");
    }
    void* const base = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) {
        PageFailure("address-space reservation");
    }
#endif
    return base;
}

void SlotVectorCommitPages(void* address, std::size_t size) noexcept {
#ifdef _WIN32
    if (VirtualAlloc(address, size, MEM_COMMIT, PAGE_READWRITE) != address) {
        PageFailure("page commit");
    }
#else
    // Keep the owned reservation; no fixed-address replacement mapping is needed.
    if (mprotect(address, size, PROT_READ | PROT_WRITE) != 0) {
        PageFailure("page commit");
    }
#endif
}

void SlotVectorReleasePages(void* base, [[maybe_unused]] std::size_t size) noexcept {
    if (!base) {
        return;
    }
#ifdef _WIN32
    if (!VirtualFree(base, 0, MEM_RELEASE)) {
        PageFailure("page release");
    }
#else
    if (munmap(base, size) != 0) {
        PageFailure("page release");
    }
#endif
}

} // namespace Common
