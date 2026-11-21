// SPDX-License-Identifier: GPL-2.0-or-later
// The CPU sampler's count table: samples add up per (thread name, address), a full table drops
// samples instead of growing (nothing may allocate while a sampled thread is paused).
#include <cassert>
#include <cstdio>
#include <map>
#include <utility>

#include "spartanport_cpu_sampler.h"

int main() {
    SpartanCpuSampler::Table table{8}; // 256 slots
    for (int i = 0; i < 5; ++i) {
        assert(table.Add(1, 0x1400766e0));
    }
    assert(table.Add(2, 0x1400766e0)); // same address, other thread: its own count
    assert(table.Add(1, 0x7ffd00001234));
    std::map<std::pair<unsigned, unsigned long long>, unsigned> seen;
    table.ForEach([&](std::uint32_t name, std::uint64_t address, std::uint32_t count) {
        seen[{name, address}] = count;
    });
    assert(seen.size() == 3);
    assert((seen[{1, 0x1400766e0}] == 5));
    assert((seen[{2, 0x1400766e0}] == 1));
    assert((seen[{1, 0x7ffd00001234}] == 1));

    SpartanCpuSampler::Table small{4}; // 16 slots
    unsigned added = 0;
    for (unsigned long long address = 0; address < 40; ++address) {
        added += small.Add(0, 0x1000 + address * 64);
    }
    assert(added == 16 && small.Dropped() == 24);
    std::printf("PASS: cpu sampler table\n");
}
