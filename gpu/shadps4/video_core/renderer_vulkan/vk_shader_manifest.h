// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "common/types.h"
#include "shader_recompiler/replay.h"
#include "shader_recompiler/specialization.h"

namespace Vulkan::Manifest {

// spartanport: the list of shaders and pipelines the game uses, shipped with the port. It holds the
// context of each shader and where to find its code in the user's game files, never the code.

/// A cache written by another build cannot be read when these differ.
struct Versions {
    u32 meta{};
    u32 binary{};
    u32 pipeline{};
    bool operator==(const Versions&) const = default;
};
Versions CurrentVersions();

struct Program {
    u64 pgm_hash{};
    u32 hw_stage{};
    u32 sw_stage{};
    std::string file; ///< Relative to the game folder.
    u64 offset{};
    u32 code_size{};
    u64 code_xxh3{};
};

constexpr u8 PriorityTitle = 0;
constexpr u8 PriorityGame = 1;

struct Permutation {
    u32 program{};
    u32 perm_idx{};
    u8 priority{PriorityGame};
    std::vector<u8> spec; ///< StageSpecialization::Serialize.
    Shader::ReplayInputs inputs;
};

constexpr u32 NoStage = 0xFFFFFFFF;
constexpr size_t MaxPipelineStages = 8;

struct StageRef {
    u32 permutation{NoStage}; ///< Index in Manifest::permutations.
    bool operator==(const StageRef&) const = default;
    auto operator<=>(const StageRef&) const = default;
};

struct Pipeline {
    u32 is_compute{};
    std::vector<u8> key; ///< Key and SerializationSupport, stage hashes zeroed.
    std::array<StageRef, MaxPipelineStages> stages{};
};

struct Manifest {
    std::string serial;
    Versions versions;
    std::vector<Program> programs;
    std::vector<Permutation> permutations;
    std::vector<Pipeline> pipelines;
};

/// One saved permutation, as Build hands it to the translator.
struct Recorded {
    u64 pgm_hash{};
    Shader::HwStage hw_stage{};
    Shader::SwStage sw_stage{};
    Shader::StageSpecialization spec;
    Shader::ReplayInputs inputs;
};

/// SPIR-V of a recorded permutation. Build uses it to tell apart two recordings it would merge.
using Translator = std::function<std::vector<u32>(const Recorded&)>;

struct BuildStats {
    u32 merged{};         ///< Permutations seen in more than one recording.
    u32 no_code{};        ///< Permutations whose shader is not in the game folder.
    u32 too_many{};       ///< Permutations past the limit of one program.
    u32 stale_keys{};     ///< Pipeline keys naming a permutation nobody saved.
};

/// Joins recorded caches. Nothing on error, with the reason in error. Same input, same result.
std::optional<Manifest> Build(const std::string& serial, const std::filesystem::path& game_dir,
                              std::span<const std::filesystem::path> caches,
                              const Translator& translator, BuildStats& stats, std::string& error);

std::vector<u8> Encode(const Manifest& manifest);

/// Nothing when the data is damaged or of another format. id: hash of the content.
std::optional<Manifest> Decode(std::span<const u8> data, u64* id = nullptr);

} // namespace Vulkan::Manifest
