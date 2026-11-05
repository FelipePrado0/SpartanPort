// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <string>

#include <xxhash.h>

#include "shader_recompiler/dump_index.h"
#include "video_core/amdgpu/regs_shader.h"

namespace fs = std::filesystem;

namespace Shader {

namespace {

constexpr u32 CodeToken = 0xBEEB03FF; // s_mov_b32 vcc_hi, imm: first instruction of every shader
constexpr size_t MaxCodeDistance = 64 * 1024;
constexpr size_t BlockSize = 16 * 1024 * 1024;

u32 ReadU32(std::span<const u8> data, size_t offset) {
    u32 value;
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return value;
}

/// The footer sits (imm + 1) * 8 bytes after the token. The footer length is not that distance.
std::optional<size_t> FindCodeStart(std::span<const u8> data, size_t footer) {
    for (size_t distance = 8; distance <= std::min(footer, MaxCodeDistance); distance += 8) {
        const size_t start = footer - distance;
        if (ReadU32(data, start) == CodeToken &&
            (ReadU32(data, start + 4) + u64{1}) * 8 == distance) {
            return start;
        }
    }
    return std::nullopt;
}

bool IsGameFile(const fs::path& relative) {
    if (relative == "eboot.bin") {
        return true;
    }
    auto extension = relative.extension().string();
    std::ranges::transform(extension, extension.begin(),
                           [](unsigned char c) { return char(std::toupper(c)); });
    return extension == ".WAD";
}

void IndexFile(const fs::path& game_dir, const fs::path& relative, DumpIndex& index) {
    constexpr size_t Footer = sizeof(AmdGpu::BinaryInfo);
    std::ifstream file(game_dir / relative, std::ios::binary);
    std::vector<u8> block(BlockSize + MaxCodeDistance + Footer);
    size_t kept = 0;
    u64 block_offset = 0;
    u64 next_footer = 0;
    const auto found = [&](u64 hash, u64 offset, u32 size) {
        index.try_emplace(hash, ShaderLocation{relative, offset, size});
    };
    while (file) {
        file.read(reinterpret_cast<char*>(block.data() + kept),
                  std::streamsize(block.size() - kept));
        const size_t size = kept + size_t(file.gcount());
        if (size < Footer) {
            return;
        }
        ScanShaderBlock({block.data(), size}, block_offset, next_footer, found);
        // Footers that fit in this block are done. The rest, and the code before them, stay.
        next_footer = block_offset + size - Footer + 1;
        kept = size_t(std::min<u64>(size, MaxCodeDistance + Footer));
        std::memmove(block.data(), block.data() + size - kept, kept);
        block_offset += size - kept;
    }
}

} // namespace

void ScanShaderBlock(std::span<const u8> data, u64 file_offset, u64 first_footer,
                     const std::function<void(u64, u64, u32)>& found) {
    if (data.size() < sizeof(AmdGpu::BinaryInfo)) {
        return;
    }
    // Footers are not word aligned in the files: real WADs keep shaders at odd offsets.
    const size_t last = data.size() - sizeof(AmdGpu::BinaryInfo);
    for (size_t pos = 0; pos <= last; ++pos) {
        const void* next = std::memchr(data.data() + pos, 'O', last - pos + 1);
        if (!next) {
            return;
        }
        pos = size_t(static_cast<const u8*>(next) - data.data());
        if (file_offset + pos < first_footer) {
            continue;
        }
        AmdGpu::BinaryInfo footer;
        std::memcpy(&footer, data.data() + pos, sizeof(footer));
        if (!footer.Valid()) {
            continue;
        }
        if (const auto start = FindCodeStart(data, pos)) {
            found(footer.shader_hash, file_offset + *start, u32(footer.length));
        }
    }
}

DumpIndex IndexDump(const fs::path& game_dir) {
    std::vector<fs::path> files;
    std::error_code ec;
    constexpr auto options = fs::directory_options::follow_directory_symlink |
                             fs::directory_options::skip_permission_denied;
    for (fs::recursive_directory_iterator it(game_dir, options, ec), end; !ec && it != end;
         it.increment(ec)) {
        std::error_code file_ec;
        if (!it->is_regular_file(file_ec)) {
            continue;
        }
        const auto relative = fs::relative(it->path(), game_dir, file_ec);
        if (!file_ec && IsGameFile(relative)) {
            files.push_back(relative);
        }
    }
    std::ranges::sort(files);
    DumpIndex index;
    for (const auto& file : files) {
        IndexFile(game_dir, file, index);
    }
    return index;
}

std::optional<std::vector<u32>> ReadShaderCode(const fs::path& game_dir,
                                               const ShaderLocation& location, u64 code_xxh3) {
    // A location can come from a shipped manifest: it never leaves the game folder.
    if (location.size == 0 || location.size % 4 != 0 || location.file.is_absolute() ||
        std::ranges::find(location.file, fs::path("..")) != location.file.end()) {
        return std::nullopt;
    }
    std::ifstream file(game_dir / location.file, std::ios::binary);
    if (!file.seekg(std::streamoff(location.offset))) {
        return std::nullopt;
    }
    std::vector<u32> code(location.size / 4);
    if (!file.read(reinterpret_cast<char*>(code.data()), location.size) ||
        XXH3_64bits(code.data(), location.size) != code_xxh3) {
        return std::nullopt;
    }
    return code;
}

} // namespace Shader
