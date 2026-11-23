// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <filesystem>
#include <functional>
#include <span>
#include <vector>

#include "common/types.h"
#include "video_core/renderer_vulkan/vk_shader_manifest.h"

namespace Shader {
struct Info;
struct Profile;
} // namespace Shader

namespace Vulkan::Manifest {

// spartanport: writes the shaders and pipeline keys of a manifest into the opened shader cache, in
// the format a played session leaves, so the game finds them as if it had seen them all before.

/// SPIR-V of one permutation from the game's code; info receives the result of the translation.
/// Empty when it cannot be translated. Called from several threads at once.
using Replay = std::function<std::vector<u32>(std::span<const u32> code, const Program& program,
                                              const Permutation& permutation,
                                              const Shader::StageSpecialization& spec,
                                              Shader::Info& info)>;

struct MaterializeStats {
    u32 written{};  ///< Permutations translated and saved.
    u32 kept{};     ///< Permutations already in the cache, as the manifest has them.
    u32 no_code{};  ///< Permutations whose shader is not in the game folder.
    u32 failed{};   ///< Permutations the translation refused.
    u32 keys{};     ///< Pipeline keys saved.
    bool stopped{}; ///< progress asked to stop: the cache holds a part, still valid.
    bool conflict{}; ///< resume: a saved permutation is another shader. Nothing was written.
};

/// ReplayTranslate with this profile, one set of pools per thread.
Replay DefaultReplay(const Shader::Profile& profile);

/// resume: keep the permutations already saved. A saved permutation at an index the manifest uses
/// must be the manifest's own, or nothing is written and conflict is set: clear the cache and run
/// again. progress(done, total) runs on the calling thread, a few times per second (after every
/// program with threads 0, which works on the calling thread); false stops.
MaterializeStats Materialize(const Manifest& manifest, const std::filesystem::path& game_dir,
                             const Replay& replay, u32 threads, bool resume,
                             const std::function<bool(u32, u32)>& progress);

} // namespace Vulkan::Manifest
