// SPDX-License-Identifier: GPL-2.0-or-later
// Block streams (parallel recording): commands recorded apart keep their order inside a block, and
// spliced streams run in block order after the commands recorded before them. No Vulkan device.
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>

#include "video_core/renderer_vulkan/vk_scheduler.h"

extern "C" {
std::uint64_t runtime_disabled_optimizations = SpartanToggle::RecordPrefetch;
}

// A fatal assertion sink instead of the GPU logger (RecordStream asserts on a full fresh chunk).
void assert_fail_impl() {
    std::abort();
}
namespace Common::Log {
bool ShouldLog(Level) {
    return true;
}
void Write(std::string_view, Level, const char*, int, const char*, std::string&& message) {
    std::fprintf(stderr, "%s\n", message.c_str());
}
} // namespace Common::Log

int main() {
    using Vulkan::RecordChunk;
    using Vulkan::RecordStream;
    std::vector<std::unique_ptr<RecordChunk>> pool;
    unsigned acquired = 0;
    const auto acquire = [&] {
        ++acquired;
        if (pool.empty()) {
            return std::make_unique<RecordChunk>();
        }
        auto chunk = std::move(pool.back());
        pool.pop_back();
        return chunk;
    };
    std::vector<unsigned> order;
    // Large enough that a block overflows into several chunks.
    struct Payload {
        unsigned value;
        unsigned char pad[4000];
    };
    const auto command = [&order](unsigned value) {
        return [&order, payload = Payload{value, {}}](vk::CommandBuffer) {
            order.push_back(payload.value);
        };
    };

    std::vector<std::unique_ptr<RecordChunk>> stream_out;
    auto main_chunk = std::make_unique<RecordChunk>();
    assert(main_chunk->Push(command(0)));
    stream_out.push_back(std::move(main_chunk));

    RecordStream first, second, empty;
    constexpr unsigned PerBlock = 100; // ~400 KiB: four chunks of 128 KiB
    for (unsigned i = 0; i < PerBlock; ++i) {
        second.Push(command(1000 + i), acquire); // recorded first, spliced second
    }
    for (unsigned i = 0; i < PerBlock; ++i) {
        first.Push(command(1 + i), acquire);
    }
    assert(first.chunks.size() > 1 && second.chunks.size() > 1);

    // Reserved data shares its chunk with the command that uses it.
    const auto& chunk = first.Reserve(RecordChunk::Capacity / 2, acquire);
    auto* data = static_cast<unsigned*>(
        const_cast<RecordChunk&>(chunk).Allocate(sizeof(unsigned), alignof(unsigned)));
    *data = 500;
    first.Push([&order, data](vk::CommandBuffer) { order.push_back(*data); }, acquire);
    assert(&chunk == first.chunks.back().get());

    std::vector<std::unique_ptr<RecordChunk>> recycled;
    const auto recycle = [&](std::unique_ptr<RecordChunk> chunk) {
        recycled.push_back(std::move(chunk));
    };
    first.Splice(stream_out, recycle);
    empty.Splice(stream_out, recycle);
    second.Splice(stream_out, recycle);
    assert(first.chunks.empty() && second.chunks.empty());

    for (auto& out : stream_out) {
        out->Execute({});
    }
    std::vector<unsigned> expected{0};
    for (unsigned i = 0; i < PerBlock; ++i) {
        expected.push_back(1 + i);
    }
    expected.push_back(500);
    for (unsigned i = 0; i < PerBlock; ++i) {
        expected.push_back(1000 + i);
    }
    assert(order == expected);
    assert(recycled.empty()); // no stream left an empty chunk behind
    std::printf("PASS: record streams keep block order (%u chunks acquired)\n", acquired);
}
