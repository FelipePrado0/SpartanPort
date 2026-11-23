// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: a CPU sampling profiler for the whole process. SPARTAN_CPU_SAMPLER=<file> (or 1 for
// cpu_samples.tsv in the working folder) starts a thread that pauses every other thread about
// SPARTAN_CPU_SAMPLER_HZ times a second (1000 by default), notes where it was and resumes it. Every
// 10 s the counts go to the file, one line per thread and address:
//   thread name <TAB> module <TAB> offset in the module (hex) <TAB> samples
// perf/auto/cpu_profile.py turns them into functions per thread. Windows only for now.
#pragma once

#include <cstdint>
#include <vector>

namespace SpartanCpuSampler {

/// Starts the sampler once when SPARTAN_CPU_SAMPLER asks for it; does nothing otherwise.
void StartFromEnv();

/// Sample counts per (thread name, address), in fixed storage: nothing is allocated while a
/// sampled thread is paused (it may hold the heap lock).
class Table {
public:
    explicit Table(unsigned log2_capacity) : slots(std::size_t{1} << log2_capacity) {}

    /// Counts one sample; false when the table is full (the sample is dropped).
    bool Add(std::uint32_t name, std::uint64_t address) {
        const std::uint64_t mask = slots.size() - 1;
        std::uint64_t h = (address ^ (std::uint64_t{name} << 48)) * 0x9E3779B97F4A7C15ull;
        for (std::uint64_t probe = 0; probe < 64; ++probe) {
            auto& slot = slots[((h >> 20) + probe) & mask];
            if (slot.count && slot.address == address && slot.name == name) {
                ++slot.count;
                return true;
            }
            if (!slot.count) {
                slot = {address, name, 1};
                return true;
            }
        }
        ++dropped;
        return false;
    }

    template <typename Func>
    void ForEach(Func&& func) const {
        for (const auto& slot : slots) {
            if (slot.count) {
                func(slot.name, slot.address, slot.count);
            }
        }
    }

    [[nodiscard]] std::uint64_t Dropped() const {
        return dropped;
    }

private:
    struct Slot {
        std::uint64_t address = 0;
        std::uint32_t name = 0;
        std::uint32_t count = 0;
    };
    std::vector<Slot> slots;
    std::uint64_t dropped = 0;
};

} // namespace SpartanCpuSampler
