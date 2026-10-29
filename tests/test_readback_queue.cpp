#include <cstdint>
#include <cassert>
#include <vector>
#include <cstring>
#include "video_core/texture_cache/readback_queue.h"

int main() {
    VideoCore::ReadbackQueue<int> queue;
    std::vector<int> published;
    auto complete = [&](auto& entry) {
        if (entry.valid) published.push_back(entry.payload);
    };
    queue.Push(0x1000, 4096, 3, 1);
    queue.Push(0x1000, 4096, 4, 2);
    assert(queue.RequiredTick(0x1fff, 8) == 4);
    assert(queue.RequiredTick(0x2000, 8) == 0);
    queue.Retire([](auto tick) { return tick <= 2; }, complete);
    assert(published.empty());
    queue.Retire([](auto tick) { return tick <= 3; }, complete);
    assert((published == std::vector<int>{1}));
    unsigned canceled = 0;
    queue.Cancel(0x1001, 1, [&](auto&) { ++canceled; });
    queue.Cancel(0x1001, 1, [&](auto&) { ++canceled; });
    assert(canceled == 1);
    queue.Push(0x1000, 4096, 5, 3);
    queue.Retire([](auto) { return true; }, complete);
    assert((published == std::vector<int>{1, 3}));
    assert(queue.Bytes() == 0);
    assert(!queue.CanFit(65ull << 20));
    for (int i = 0; i < 64; ++i) queue.Push(0x1000, 1, 6, i);
    assert(!queue.CanFit(1));
    assert(queue.RequiredTick(0x1ff0, 1) == 6);
    assert(queue.RequiredTick(0x3000, 1) == 0);
    queue.Retire([](auto) { return true; }, [](auto&) {});
    queue.Push(0x1000, 64ull << 20, 7, 4);
    assert(!queue.CanFit(1));
    assert(queue.RequiredTick(0x1000, 0) == 0);
    queue.Retire([](auto) { return true; }, [](auto&) {});
    assert(queue.CanFit(64ull << 20));

    // Byte rule: a CPU write next to a small pending copy, on the same page but outside its
    // bytes, releases that copy's page protection instead of waiting for the GPU.
    std::vector<std::uint64_t> released;
    auto release = [&](auto& entry) { released.push_back(entry.address); };
    queue.Push(0x10000c00, 4, 8, 5);            // 1x1 image
    queue.Push(0x10001000, 4096, 9, 6);         // a page fully covered by a copy
    queue.Push(0x20000000, 1u << 20, 10, 7);    // large copy
    assert(queue.ReleaseOutside(0x10000100, 8, 64 << 10, release) == 1);
    assert((released == std::vector<std::uint64_t>{0x10000c00}));
    assert(queue.RequiredTick(0x10000100, 8) == 0);   // nothing left to wait for there
    assert(queue.RequiredTick(0x10000c00, 4) == 0);   // released copies never block again
    assert(queue.ReleaseOutside(0x10001010, 8, 64 << 10, release) == 0);  // inside the bytes
    assert(queue.RequiredTick(0x10001010, 8) == 9);
    assert(queue.ReleaseOutside(0x20000008, 8, 64 << 10, release) == 0);  // too large
    assert(queue.RequiredTick(0x20000008, 8) == 10);
    unsigned watched = 0, published_released = 0;
    queue.Retire([](auto) { return true; }, [&](auto& entry) {
        watched += entry.watched;
        published_released += entry.valid && !entry.watched;  // still published, no watcher left
    });
    assert(watched == 2 && published_released == 1);

    // Exact overlap: an image upload or free next to a pending copy, on a shared page but not
    // its bytes, neither waits for nor cancels it.
    queue.Push(0x30000c00, 256, 11, 8);         // 8x8 image at the end of a page
    assert(queue.RequiredTick(0x30000000, 0xc00) == 11);
    assert(queue.RequiredTick(0x30000000, 0xc00, true) == 0);
    assert(queue.RequiredTick(0x30000000, 0xc01, true) == 11);
    canceled = 0;
    queue.Cancel(0x30000000, 0xc00, [&](auto&) { ++canceled; }, true);
    assert(canceled == 0);
    published.clear();
    queue.Retire([](auto) { return true; }, complete);
    assert((published == std::vector<int>{8}));
    // Integrity: copies publish bytes into a model of guest memory (as RetireReadbacks does with
    // TryWriteBacking). Payload = fill byte.
    {
        constexpr std::uint64_t base = 0x40000000, page = 0x4000; // runtime_memory unmaps 16 KiB pages
        std::vector<std::uint8_t> ram(4 * page, 0);
        VideoCore::ReadbackQueue<int> copies;
        auto publish = [&](auto& entry) {
            if (entry.valid) std::memset(&ram[entry.address - base], entry.payload, entry.size);
        };
        auto all = [](auto) { return true; };
        auto at = [&](std::uint64_t address) { return ram[address - base]; };

        // Remap: generation A's pending copy, then UnmapMemory (CancelReadbacks over the unmapped
        // pages) and generation B written by the new mapping. Draining the old copy afterwards
        // must not overwrite B; a neighbor's copy outside the unmapped pages still lands.
        copies.Push(base + page, 0x1000, 20, 0xA1);
        copies.Push(base + 2 * page + 0x100, 0x200, 20, 0xC3);
        copies.Cancel(base + page, page, [](auto&) {});
        std::memset(&ram[page], 0xB2, page);
        copies.Retire(all, publish);
        assert(at(base + page) == 0xB2 && at(base + page + 0xfff) == 0xB2);
        assert(at(base + 2 * page + 0x100) == 0xC3 && at(base + 2 * page + 0xff) == 0);

        // Two copies of the same bytes (an older and a newer generation): the newer one wins, and
        // a newer copy never retires before an older one still on the GPU.
        copies.Push(base, 0x100, 21, 0x11);
        copies.Push(base, 0x100, 22, 0x22);
        copies.Retire([](auto tick) { return tick == 22; }, publish);
        assert(at(base) == 0);
        copies.Retire(all, publish);
        assert(at(base) == 0x22);

        // Neighbors on one page: freeing image A (FreeImage resolves A, DeleteImage cancels A's
        // exact bytes) neither waits for nor cancels B's copy, and B's bytes stay B's.
        const std::uint64_t a = base + 3 * page, b = a + 0x800;
        copies.Push(a, 0x800, 23, 0xAA);
        copies.Push(b, 0x800, 24, 0xBB);
        assert(copies.RequiredTick(a, 0x800, true) == 23);
        copies.Retire([](auto tick) { return tick <= 23; }, publish);   // A resolved
        unsigned dropped = 0;
        copies.Cancel(a, 0x800, [&](auto&) { ++dropped; }, true);       // A deleted
        assert(dropped == 0 && copies.RequiredTick(b, 0x800, true) == 24);
        std::memset(&ram[a - base], 0x5A, 0x800);                       // guest reuses A's bytes
        copies.Retire(all, publish);
        assert(at(a) == 0x5A && at(a + 0x7ff) == 0x5A);
        assert(at(b) == 0xBB && at(b + 0x7ff) == 0xBB);
        assert(copies.Bytes() == 0);
    }
}
