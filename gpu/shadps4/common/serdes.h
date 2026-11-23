// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/assert.h"
#include "common/types.h"

#include <cstddef>
// spartanport: report damaged cache records through recoverable exceptions.
#include <stdexcept>
#include <vector>

namespace Serialization {

// spartanport: let cache loaders skip damaged records instead of stopping startup.
struct CorruptData : std::runtime_error {
    CorruptData()
        : std::runtime_error{"Invalid or corrupted deserialization container/shader cache"} {}
};

template <typename T>
concept Container = requires(T t) {
    typename T::iterator;
    { t.begin() } -> std::same_as<typename T::iterator>;
    { t.end() } -> std::same_as<typename T::iterator>;
    { t.size() } -> std::convertible_to<std::size_t>;
};

struct Archive {
    void Alloc(size_t size) {
        container.resize(size);
    }

    void Grow(size_t size) {
        container.resize(container.size() + size);
    }

    void Merge(const Archive& ar) {
        container.insert(container.end(), ar.container.cbegin(), ar.container.cend());
        offset = container.size();
    }

    [[nodiscard]] size_t SizeBytes() const {
        return container.size();
    }

    u8* CurrPtr() {
        return container.data() + offset;
    }

    // spartanport: check against remaining bytes without overflowing offset + size.
    [[nodiscard]] size_t Remaining() const {
        return container.size() - offset;
    }

    void Advance(size_t size) {
        if (size > Remaining()) {
            throw CorruptData{};
        }
        offset += size;
    }

    std::vector<u8>&& TakeOff() {
        offset = 0;
        return std::move(container);
    }

    [[nodiscard]] bool IsEoS() const {
        return offset >= container.size();
    }

    Archive() = default;
    explicit Archive(std::vector<u8>&& v) : container{v} {}

private:
    u32 offset{};
    std::vector<u8> container{};

    friend struct Writer;
    friend struct Reader;
};

struct Writer {
    template <typename T>
    void Write(const T* ptr, size_t size) {
        if (ar.offset + size >= ar.container.size()) {
            ar.Grow(size);
        }
        std::memcpy(ar.CurrPtr(), reinterpret_cast<const void*>(ptr), size);
        ar.Advance(size);
    }

    template <typename T>
        requires(!Container<T>)
    void Write(const T& value) {
        const auto size = sizeof(value);
        Write(&value, size);
    }

    void Write(const auto& v) {
        Write(v.size());
        for (const auto& elem : v) {
            Write(elem);
        }
    }

    void Write(const std::string& s) {
        Write(s.size());
        Write(s.c_str(), s.size());
    }

    Writer() = delete;
    explicit Writer(Archive& ar_) : ar{ar_} {}

    Archive& ar;
};

struct Reader {
    template <typename T>
    void Read(T* ptr, size_t size) {
        // spartanport: reject truncated records before copying from them.
        if (size > ar.Remaining()) {
            throw CorruptData{};
        }
        std::memcpy(reinterpret_cast<void*>(ptr), ar.CurrPtr(), size);
        ar.Advance(size);
    }

    template <typename T>
        requires(!Container<T>)
    void Read(T& value) {
        const auto size = sizeof(value);
        Read(&value, size);
    }

    void Read(auto& v) {
        size_t num_elements{};
        Read(num_elements);
        // spartanport: every element needs a byte; reject corrupt counts before allocating.
        if (num_elements > ar.Remaining()) {
            throw CorruptData{};
        }
        for (size_t i = 0; i < num_elements; ++i) {
            v.emplace_back();
            Read(v.back());
        }
    }

    void Read(std::string& s) {
        size_t length{};
        Read(length);
        // spartanport: reject corrupt string lengths before resizing the destination.
        if (length > ar.Remaining()) {
            throw CorruptData{};
        }
        s.resize(length);
        Read(s.data(), length);
    }

    Reader() = delete;
    explicit Reader(Archive& ar_) : ar{ar_} {}

    Archive& ar;
};

} // namespace Serialization
