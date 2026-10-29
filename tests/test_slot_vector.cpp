// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <vector>

#include "common/slot_vector.h"

// This standalone test uses the production header and page operations, with a fatal
// assertion sink instead of the GPU logger. It needs no renderer or game resources.
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

struct alignas(64) Item {
    explicit Item(unsigned value_) : value(value_) {
        ++constructed;
        ++live;
        payload.fill(value);
    }
    Item(const Item&) = delete;
    Item(Item&&) = delete;
    ~Item() {
        --live;
        ++destroyed;
    }
    unsigned value;
    std::array<unsigned, 38> payload; // 192-byte entries straddle 2 MiB chunk boundaries.
    static inline unsigned live = 0, constructed = 0, destroyed = 0;
};

int main() {
    constexpr unsigned count = 50000;
    static_assert(sizeof(Item) == 192);
    static_assert(!std::is_copy_constructible_v<Common::SlotVector<Item>>);
    static_assert(!std::is_move_constructible_v<Common::SlotVector<Item>>);
    std::vector<Common::SlotId> ids;
    std::vector<Item*> addresses;
    ids.reserve(count);
    addresses.reserve(count);
    {
        Common::SlotVector<Item> pool;
        assert(pool.size() == 0);
        assert(pool.begin() == pool.end());
        for (unsigned i = 0; i < count; ++i) {
            ids.push_back(pool.insert(i));
            addresses.push_back(&pool[ids.back()]);
            assert(reinterpret_cast<std::uintptr_t>(addresses.back()) % alignof(Item) == 0);
            assert(&pool[ids.front()] == addresses.front());
        }
        assert(pool.size() == count);
        assert(Item::live == count);
        unsigned visited = 0;
        for (auto& item : pool) {
            assert(item.value == visited);
            assert(&item == addresses[visited]);
            for (auto value : item.payload) {
                assert(value == visited);
            }
            ++visited;
        }
        assert(visited == count);
        for (unsigned i = 0; i < count; i += 2) {
            pool.erase(ids[i]);
            assert(!pool.is_allocated(ids[i]));
        }
        assert(pool.size() == count / 2);
        visited = 0;
        for (auto& item : pool) {
            assert(item.value == visited * 2 + 1);
            ++visited;
        }
        assert(visited == count / 2);
        for (unsigned i = 0; i < count / 2; ++i) {
            const auto id = pool.insert(count + i);
            assert(id.index % 2 == 0);
            assert(&pool[id] == addresses[id.index]);
        }
        for (unsigned i = 1; i < count; i += 2) {
            assert(&pool[ids[i]] == addresses[i]);
            assert(pool[ids[i]].value == i);
        }
        assert(pool.size() == count);
    }
    assert(Item::live == 0);
    assert(Item::constructed == count + count / 2);
    assert(Item::destroyed == Item::constructed);
    std::puts("SlotVector: 50000 stable entries, alignment, iteration, reuse and destruction passed");
}
