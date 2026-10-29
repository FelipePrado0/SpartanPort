// SPDX-License-Identifier: GPL-2.0-or-later
// Deferred payload ownership, grouped reservations and recycling, without a Vulkan device.
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <span>
#include <vector>

#include "video_core/renderer_vulkan/vk_scheduler.h"

extern "C" {
std::uint64_t runtime_disabled_optimizations = SpartanToggle::RecordPrefetch;
}

template <typename T>
static std::span<const T> Save(Vulkan::RecordChunk& chunk, std::span<const T> data) {
    // Exercise the same reservation contract as Scheduler::RecordData, without a device.
    assert(chunk.ReserveData(data.size_bytes() + alignof(T)));
    auto* saved = static_cast<T*>(chunk.Allocate(data.size_bytes(), alignof(T)));
    assert(saved);
    std::memcpy(saved, data.data(), data.size_bytes());
    return {saved, data.size()};
}

int main() {
    using Vulkan::RecordChunk;
    constexpr size_t Count = RecordChunk::Capacity / sizeof(vk::BufferCopy) + 257;
    struct alignas(64) Marker {
        unsigned value;
    };
    RecordChunk chunk;
    unsigned executed = 0;
    for (unsigned pass = 0; pass < 12; ++pass) {
        std::vector<vk::BufferCopy> regions(Count);
        for (size_t i = 0; i < Count; ++i) {
            regions[i] = vk::BufferCopy{.srcOffset = i * 4, .dstOffset = i * 8, .size = pass + 1};
        }
        // Earlier commands must survive allocating and releasing spill blocks.
        for (unsigned i = 0; i < 4000; ++i) {
            assert(chunk.Push([](vk::CommandBuffer) {}));
        }
        if (pass % 2 == 0) {
            const auto saved = Save(chunk, std::span<const vk::BufferCopy>{regions});
            assert(chunk.Size() > RecordChunk::Capacity);
            assert(chunk.Push([saved, pass, &executed](vk::CommandBuffer) {
                for (size_t i = 0; i < saved.size(); ++i) {
                    assert(saved[i].srcOffset == i * 4);
                    assert(saved[i].dstOffset == i * 8);
                    assert(saved[i].size == pass + 1);
                }
                ++executed;
            }));
        } else {
            const auto half = Count / 2;
            assert(chunk.ReserveData(regions.size() * sizeof(regions[0]) + sizeof(Marker) + 256));
            const auto first = Save(chunk, std::span<const vk::BufferCopy>{regions.data(), half});
            const Marker marker{.value = pass};
            const auto aligned = Save(chunk, std::span<const Marker>{&marker, 1});
            const auto second =
                Save(chunk, std::span<const vk::BufferCopy>{regions.data() + half, Count - half});
            assert(reinterpret_cast<uintptr_t>(aligned.data()) % alignof(Marker) == 0);
            assert(chunk.Push([first, second, aligned, pass, &executed](vk::CommandBuffer) {
                assert(aligned[0].value == pass);
                for (size_t i = 0; i < Count; ++i) {
                    const auto& region = i < first.size() ? first[i] : second[i - first.size()];
                    assert(region.srcOffset == i * 4);
                    assert(region.dstOffset == i * 8);
                    assert(region.size == pass + 1);
                }
                ++executed;
            }));
        }
        // The queued commands must use owned data after the source is destroyed.
        std::fill(regions.begin(), regions.end(), vk::BufferCopy{});
        regions.clear();
        regions.shrink_to_fit();
        chunk.Execute({});
        assert(executed == pass + 1);
        assert(chunk.Empty());
        assert(chunk.Size() == 0);
    }

    // A new spill or a return to inline storage must keep all older spills alive.
    assert(chunk.ReserveData(RecordChunk::Capacity * 2));
    auto* first = static_cast<unsigned char*>(chunk.Allocate(RecordChunk::Capacity * 2, 1));
    assert(first);
    std::memset(first, 0x5a, RecordChunk::Capacity * 2);
    assert(chunk.ReserveData(RecordChunk::Capacity * 3));
    auto* second = static_cast<unsigned char*>(chunk.Allocate(RecordChunk::Capacity * 3, 1));
    assert(second);
    std::memset(second, 0xa5, RecordChunk::Capacity * 3);
    assert(chunk.ReserveData(32));
    assert(chunk.Allocate(32, 1));
    assert(chunk.Push([first, second](vk::CommandBuffer) {
        for (size_t i = 0; i < RecordChunk::Capacity * 2; ++i) {
            assert(first[i] == 0x5a);
        }
        for (size_t i = 0; i < RecordChunk::Capacity * 3; ++i) {
            assert(second[i] == 0xa5);
        }
    }));
    chunk.Execute({});

    // Spilling requires command room; a full chunk must be retired and retried.
    unsigned queued = 0;
    unsigned consumed = 0;
    while (chunk.Push([&consumed](vk::CommandBuffer) { ++consumed; })) {
        ++queued;
    }
    const auto full_size = chunk.Size();
    assert(!chunk.ReserveData(RecordChunk::Capacity * 2));
    assert(chunk.Size() == full_size);
    chunk.Execute({});
    assert(consumed == queued);
    assert(chunk.ReserveData(RecordChunk::Capacity * 2));
    chunk.Execute({});

    // The exact inline limit leaves room for its command, and size checks cannot wrap.
    assert(chunk.ReserveData(RecordChunk::Capacity - RecordChunk::CommandReserve));
    assert(chunk.Allocate(RecordChunk::Capacity - RecordChunk::CommandReserve, 1));
    assert(!chunk.Allocate(std::numeric_limits<size_t>::max(), 1));
    assert(chunk.Push([](vk::CommandBuffer) {}));
    chunk.Execute({});
    std::puts(
        "PASS: record data ownership, grouped alignment, multiple spills and chunk recycling");
}
