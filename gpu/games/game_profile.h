// SPDX-License-Identifier: GPL-2.0-or-later
// Game profiles keep game-specific knowledge out of the generic GPU translator.
// Adapted from deadinside28/bloodborne_pc.
#pragma once
#include <cstdint>
#include <span>

namespace Game {

struct CodeRange {
    std::uint64_t begin = 0, end = 0;
};

struct CoreServices {
    void (*fresh_range)(std::uint64_t address, std::uint64_t size) = nullptr;
};

struct Profile {
    const char* name = nullptr;
    std::span<const char* const> serials;
    void (*install_hooks)(const CoreServices& services) = nullptr;
    std::span<const CodeRange> dynamic_writers;
    std::uint64_t buffer_copy_shader = 0;
};

void Select(const char* serial);
const Profile* Active();
bool IsDynamicWriter(std::uint64_t guest_rip);

inline std::uint64_t BufferCopyShader() {
    const Profile* profile = Active();
    return profile ? profile->buffer_copy_shader : 0;
}

} // namespace Game
