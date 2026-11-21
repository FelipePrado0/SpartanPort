// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: the guest memory a draw's vertex V#s are read from (the table behind a user-data
// pointer). Stage A checks the V#s when it chooses the pipeline; if no write still queued for stage
// B (pending_writes) overlaps this span, stage B need not read them again (toggle
// SharpsCheckedOnce, docs/parallel_gpu.md).
#pragma once

#include <algorithm>
#include <cstdint>

namespace SpartanVertexTable {

struct Span {
    std::uint64_t begin = ~0ull;
    std::uint64_t end = 0;

    /// One V# (16 bytes) at `pointer` (raw user-data pair) + `dword_offset` dwords. The masking and
    /// the null rule match Shader::Info::ReadUdReg: a null table reads no memory.
    void Add(std::uint64_t pointer, std::uint32_t dword_offset) {
        const std::uint64_t base = pointer & 0xFFFFFFFFFFFFull;
        if (base < 0x10000) {
            return;
        }
        const std::uint64_t address = base + std::uint64_t{dword_offset} * 4;
        begin = std::min(begin, address);
        end = std::max(end, address + 16);
    }

    [[nodiscard]] bool Empty() const {
        return begin >= end;
    }

    [[nodiscard]] bool Overlaps(std::uint64_t address, std::uint64_t size) const {
        return !Empty() && address < end && begin < address + size;
    }
};

} // namespace SpartanVertexTable
