// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#include "common/serdes.h"

namespace {
template <typename F>
void ExpectCorrupt(F&& read) {
    bool caught = false;
    try {
        read();
    } catch (const Serialization::CorruptData&) {
        caught = true;
    }
    assert(caught);
}

template <typename T>
void Append(std::vector<u8>& bytes, const T& value) {
    const auto* first = reinterpret_cast<const u8*>(&value);
    bytes.insert(bytes.end(), first, first + sizeof(value));
}

void RoundTripAndTruncation() {
    const u32 version = 5;
    const std::string text = "shader";
    const std::vector<u32> words{0x12345678, 0x9abcdef0};
    Serialization::Archive written;
    Serialization::Writer writer{written};
    writer.Write(version);
    writer.Write(text);
    writer.Write(words);
    auto bytes = written.TakeOff();

    // The on-disk layout stays native scalar/count/payload, with no new fields.
    std::vector<u8> expected;
    Append(expected, version);
    Append(expected, text.size());
    expected.insert(expected.end(), text.begin(), text.end());
    Append(expected, words.size());
    for (const auto word : words) {
        Append(expected, word);
    }
    assert(bytes == expected);

    const auto read = [&](std::vector<u8> record) {
        Serialization::Archive ar{std::move(record)};
        Serialization::Reader reader{ar};
        u32 got_version{};
        std::string got_text;
        std::vector<u32> got_words;
        reader.Read(got_version);
        reader.Read(got_text);
        reader.Read(got_words);
        assert(got_version == version && got_text == text && got_words == words);
        assert(ar.Remaining() == 0 && ar.IsEoS());
    };
    read(bytes);
    for (size_t length = 0; length < bytes.size(); ++length) {
        ExpectCorrupt([&] { read({bytes.begin(), bytes.begin() + length}); });
    }
}

void InvalidLengths() {
    for (const size_t count : {size_t{2}, std::numeric_limits<size_t>::max()}) {
        std::vector<u8> bytes;
        Append(bytes, count);
        bytes.push_back(0);
        {
            Serialization::Archive ar{std::vector<u8>{bytes}};
            Serialization::Reader reader{ar};
            std::string text = "unchanged";
            ExpectCorrupt([&] { reader.Read(text); });
            assert(text == "unchanged" && ar.Remaining() == 1);
        }
        {
            Serialization::Archive ar{std::vector<u8>{bytes}};
            Serialization::Reader reader{ar};
            std::vector<u8> values;
            ExpectCorrupt([&] { reader.Read(values); });
            assert(values.empty() && ar.Remaining() == 1);
        }
    }

    // A count may fit the byte bound while the last multi-byte element is truncated.
    std::vector<u8> bytes;
    Append(bytes, size_t{1});
    bytes.push_back(0);
    Serialization::Archive ar{std::move(bytes)};
    Serialization::Reader reader{ar};
    std::vector<u64> values;
    ExpectCorrupt([&] { reader.Read(values); });
    assert(ar.Remaining() == 1);
}

void EmptyContainers() {
    Serialization::Archive ar;
    Serialization::Writer writer{ar};
    writer.Write(std::string{});
    writer.Write(std::vector<u32>{});
    auto bytes = ar.TakeOff();
    Serialization::Archive input{std::move(bytes)};
    Serialization::Reader reader{input};
    std::string text = "old";
    std::vector<u32> values;
    reader.Read(text);
    reader.Read(values);
    assert(text.empty() && values.empty() && input.Remaining() == 0);
}

void InvalidByteRanges() {
    Serialization::Archive ar{std::vector<u8>{1, 2, 3}};
    ar.Advance(1);
    ExpectCorrupt([&] { ar.Advance(std::numeric_limits<size_t>::max()); });
    assert(ar.Remaining() == 2);

    Serialization::Reader reader{ar};
    u32 value = 0xdeadbeef;
    ExpectCorrupt([&] { reader.Read(value); });
    assert(value == 0xdeadbeef && ar.Remaining() == 2);
    ExpectCorrupt([&] { reader.Read(&value, std::numeric_limits<size_t>::max()); });
    assert(value == 0xdeadbeef && ar.Remaining() == 2);
    ar.Advance(2);
    assert(ar.IsEoS());
    ExpectCorrupt([&] { ar.Advance(1); });
    assert(ar.Remaining() == 0);
}
} // namespace

int main() {
    RoundTripAndTruncation();
    InvalidLengths();
    EmptyContainers();
    InvalidByteRanges();
}
