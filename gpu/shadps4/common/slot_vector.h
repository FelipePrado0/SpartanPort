// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// spartanport: include the standard declarations used by stable storage directly.
#include <cstdlib>
#include <functional>
#include <iterator>
#include <limits>
#include <new>
#include <utility>
#include <vector>
#include "common/assert.h"
#include "common/types.h"

namespace Common {

struct SlotId {
    static constexpr u32 INVALID_INDEX = std::numeric_limits<u32>::max();

    SlotId() noexcept = default;
    constexpr SlotId(u32 index) noexcept : index(index) {}

    constexpr auto operator<=>(const SlotId&) const noexcept = default;

    constexpr explicit operator bool() const noexcept {
        return index != INVALID_INDEX;
    }

    u32 index = INVALID_INDEX;
};

// spartanport: reserve stable storage through the shim so inserts keep references valid.
void* SlotVectorReservePages(std::size_t size) noexcept;
void SlotVectorCommitPages(void* address, std::size_t size) noexcept;
void SlotVectorReleasePages(void* base, std::size_t size) noexcept;

// spartanport: grow by committed pages instead of a relocating initial allocation.
template <class T>
class SlotVector {
public:
    template <typename ValueType, typename Pointer, typename Reference>
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = ValueType;
        using difference_type = std::ptrdiff_t;
        using pointer = Pointer;
        using reference = Reference;

        Iterator(SlotVector& vector_, SlotId index_) : vector(vector_), slot(index_) {
            AdvanceToValid();
        }

        reference operator*() const {
            return vector[slot];
        }

        pointer operator->() const {
            return &vector[slot];
        }

        Iterator& operator++() {
            ++slot.index;
            AdvanceToValid();
            return *this;
        }

        Iterator operator++(int) {
            Iterator temp = *this;
            ++(*this);
            return temp;
        }

        bool operator==(const Iterator& other) const {
            return slot == other.slot;
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }

    private:
        void AdvanceToValid() {
            while (slot < vector.values_capacity && !vector.ReadStorageBit(slot.index)) {
                ++slot.index;
            }
        }

        SlotVector& vector;
        SlotId slot;
    };

    using iterator = Iterator<T, T*, T&>;
    using const_iterator = Iterator<const T, const T*, const T&>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    SlotVector() {
        // spartanport: commit the first chunk inside a fixed address-space reservation.
        AddChunk();
    }

    ~SlotVector() noexcept {
        std::size_t index = 0;
        for (u64 bits : stored_bitset) {
            for (std::size_t bit = 0; bits; ++bit, bits >>= 1) {
                if ((bits & 1) != 0) {
                    values[index + bit].object.~T();
                }
            }
            index += 64;
        }
        // spartanport: release the reservation after destroying its live entries.
        SlotVectorReleasePages(values, ReservedBytes);
    }

    // spartanport: the reservation has one owner and cannot be copied.
    SlotVector(const SlotVector&) = delete;
    SlotVector& operator=(const SlotVector&) = delete;

    [[nodiscard]] T& operator[](SlotId id) noexcept {
        ValidateIndex(id);
        return values[id.index].object;
    }

    [[nodiscard]] const T& operator[](SlotId id) const noexcept {
        ValidateIndex(id);
        return values[id.index].object;
    }

    bool is_allocated(SlotId id) const {
        // spartanport: reject out-of-range IDs before reading the allocation bitset.
        return (id.index / 64) < stored_bitset.size() && ReadStorageBit(id.index);
    }

    template <typename... Args>
    SlotId insert(Args&&... args) noexcept {
        const u32 index = FreeValueIndex();
        new (&values[index].object) T(std::forward<Args>(args)...);
        SetStorageBit(index);

        return SlotId{index};
    }

    void erase(SlotId id) noexcept {
        values[id.index].object.~T();
        free_list.push_back(id.index);
        ResetStorageBit(id.index);
    }

    std::size_t size() const noexcept {
        return values_capacity - free_list.size();
    }

    iterator begin() noexcept {
        return iterator(*this, 0);
    }

    const_iterator begin() const noexcept {
        return const_iterator(*this, 0);
    }

    const_iterator cbegin() const noexcept {
        return begin();
    }

    iterator end() noexcept {
        return iterator(*this, values_capacity);
    }

    const_iterator end() const noexcept {
        return const_iterator(*this, values_capacity);
    }

    const_iterator cend() const noexcept {
        return end();
    }

    reverse_iterator rbegin() noexcept {
        return reverse_iterator(end());
    }

    const_reverse_iterator rbegin() const noexcept {
        return const_reverse_iterator(end());
    }

    const_reverse_iterator crbegin() const noexcept {
        return rbegin();
    }

    reverse_iterator rend() noexcept {
        return reverse_iterator(begin());
    }

    const_reverse_iterator rend() const noexcept {
        return const_reverse_iterator(begin());
    }

    const_reverse_iterator crend() const noexcept {
        return rend();
    }

private:
    struct NonTrivialDummy {
        NonTrivialDummy() noexcept {}
    };

    union Entry {
        Entry() noexcept : dummy{} {}
        ~Entry() noexcept {}

        NonTrivialDummy dummy;
        T object;
    };

    void SetStorageBit(u32 index) noexcept {
        stored_bitset[index / 64] |= u64(1) << (index % 64);
    }

    void ResetStorageBit(u32 index) noexcept {
        stored_bitset[index / 64] &= ~(u64(1) << (index % 64));
    }

    bool ReadStorageBit(u32 index) const noexcept {
        return ((stored_bitset[index / 64] >> (index % 64)) & 1) != 0;
    }

    void ValidateIndex([[maybe_unused]] SlotId id) const noexcept {
        DEBUG_ASSERT(id);
        DEBUG_ASSERT(id.index / 64 < stored_bitset.size());
        DEBUG_ASSERT(((stored_bitset[id.index / 64] >> (id.index % 64)) & 1) != 0);
    }

    [[nodiscard]] u32 FreeValueIndex() noexcept {
        if (free_list.empty()) {
            // spartanport: commit more storage without moving existing entries.
            AddChunk();
        }

        const u32 free_index = free_list.back();
        free_list.pop_back();
        return free_index;
    }

    // spartanport: commit chunks in the reserved range so image references never dangle on insert.
    void AddChunk() noexcept {
        if (!values) {
            values = static_cast<Entry*>(SlotVectorReservePages(ReservedBytes));
        }
        const std::size_t old_capacity = values_capacity;
        const std::size_t new_capacity = (committed_bytes + ChunkBytes) / sizeof(Entry);
        if (committed_bytes > ReservedBytes - ChunkBytes) {
            ASSERT_MSG(false, "SlotVector out of space");
            std::abort();
        }
        SlotVectorCommitPages(reinterpret_cast<u8*>(values) + committed_bytes, ChunkBytes);
        committed_bytes += ChunkBytes;
        for (std::size_t i = old_capacity; i < new_capacity; ++i) {
            new (&values[i]) Entry();
        }

        stored_bitset.resize((new_capacity + 63) / 64);
        // Lowest new index on top, as before.
        for (std::size_t i = new_capacity; i-- > old_capacity;) {
            free_list.push_back(static_cast<u32>(i));
        }
        values_capacity = new_capacity;
    }

    // spartanport: bound the reservation and require page-compatible entry size and alignment.
    static constexpr std::size_t ChunkBytes = std::size_t{2} << 20;    // 2 MiB committed at a time
    static constexpr std::size_t ReservedBytes = std::size_t{1} << 30; // 1 GiB of address space
    static_assert(sizeof(Entry) <= ChunkBytes);
    // All supported hosts provide at least 4 KiB alignment for page reservations.
    static_assert(alignof(Entry) <= 4096);

    Entry* values = nullptr;
    std::size_t values_capacity = 0;
    // spartanport: track committed bytes independently of partially filled entries.
    std::size_t committed_bytes = 0;

    std::vector<u64> stored_bitset;
    std::vector<u32> free_list;
};

} // namespace Common

template <>
struct std::hash<Common::SlotId> {
    std::size_t operator()(const Common::SlotId& id) const noexcept {
        return std::hash<u32>{}(id.index);
    }
};
