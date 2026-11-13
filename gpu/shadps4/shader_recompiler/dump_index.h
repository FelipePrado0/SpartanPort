// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include "common/types.h"

namespace Shader {

/// spartanport: where the code of one guest shader sits in the user's game files.
struct ShaderLocation {
    std::filesystem::path file; ///< Relative to the game folder.
    u64 offset{};
    u32 size{}; ///< Bytes.
};

using DumpIndex = std::unordered_map<u64, ShaderLocation>;

/// Calls found(hash, offset, size) for each shader whose footer starts at or after first_footer.
/// data starts at file_offset. A footer without its code token is left out.
void ScanShaderBlock(std::span<const u8> data, u64 file_offset, u64 first_footer,
                     const std::function<void(u64, u64, u32)>& found);

/// Scans the WADs and eboot.bin of a game folder. The first file holding a hash wins. Read only.
DumpIndex IndexDump(const std::filesystem::path& game_dir);

/// A path a shipped manifest gives stays in the game folder: relative, with no drive or "..".
bool InsideGameFolder(const std::filesystem::path& relative);

/// Scans one WAD or eboot.bin of the game folder; nothing for a path outside it. Read only.
DumpIndex IndexGameFile(const std::filesystem::path& game_dir,
                        const std::filesystem::path& relative);

/// The code at location, or nothing when it cannot be read or its hash is not code_xxh3.
std::optional<std::vector<u32>> ReadShaderCode(const std::filesystem::path& game_dir,
                                               const ShaderLocation& location, u64 code_xxh3);

} // namespace Shader
