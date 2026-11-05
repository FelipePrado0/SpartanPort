// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <fmt/format.h>
#include <xxhash.h>

#include "common/serdes.h"
#include "video_core/amdgpu/regs_shader.h"
#include "video_core/renderer_vulkan/vk_compute_pipeline.h"
#include "video_core/renderer_vulkan/vk_shader_manifest.h"

#undef NDEBUG
#include <cassert>

namespace fs = std::filesystem;
using namespace Vulkan;

constexpr u32 CodeMarker = 0xC0DE51A5; // made up, must never reach the manifest

static void Write(const fs::path& path, const std::vector<u8>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
}

static void WriteGameFile(const fs::path& game, std::span<const u64> hashes) {
    std::vector<u8> wad;
    for (const u64 hash : hashes) {
        std::vector<u32> code(8, CodeMarker);
        code[0] = 0xBEEB03FF;
        code[1] = 3;
        AmdGpu::BinaryInfo footer{};
        std::memcpy(&footer, "OrbShdr", 7);
        footer.length = 32;
        footer.shader_hash = hash;
        const auto* bytes = reinterpret_cast<const u8*>(code.data());
        wad.insert(wad.end(), bytes, bytes + 32);
        const auto* tail = reinterpret_cast<const u8*>(&footer);
        wad.insert(wad.end(), tail, tail + sizeof(footer));
    }
    Write(game / "EXEC/WAD/PS4_LE/a.WAD", wad);
}

static u64 ComputePermutation(fs::path cache, u64 pgm_hash, u32 session, u32 stride,
                              u32 user_value = 1) {
    const u64 perm_hash = (pgm_hash << 8) | session;
    Serialization::Archive ar;
    Serialization::Writer meta{ar};
    const auto versions = Manifest::CurrentVersions();
    meta.Write(versions.meta);
    meta.Write(versions.binary);
    meta.Write(perm_hash);
    meta.Write(size_t{session});

    Shader::Info info;
    info.hw_stage = Shader::HwStage::Compute;
    info.sw_stage = Shader::SwStage::Compute;
    info.pgm_hash = pgm_hash;
    Shader::StageSpecialization spec;
    spec.vs_attribs.push_back({.divisor = stride});
    spec.Serialize(ar);
    info.Serialize(ar);
    Shader::ReplayInputs inputs;
    inputs.user_data.assign(16, user_value);
    inputs.flat_ud.assign(16, user_value);
    inputs.code_size = 32;
    inputs.code_xxh3 = pgm_hash * 3;
    inputs.Serialize(ar);
    Write(cache / fmt::format("{:#018x}.meta", perm_hash), std::vector<u8>{ar.TakeOff()});
    return perm_hash;
}

static void ComputeKey(fs::path cache, u64 perm_hash) {
    Serialization::Archive ar;
    Serialization::Writer writer{ar};
    writer.Write(Manifest::CurrentVersions().pipeline);
    writer.Write(u32{1});
    ComputePipelineKey key{.value = perm_hash};
    ComputePipeline::SerializationSupport support{};
    key.Serialize(ar);
    support.Serialize(ar);
    Write(cache / fmt::format("c_{:#018x}.key", perm_hash), std::vector<u8>{ar.TakeOff()});
}

static std::vector<u32> SameSpirv(const Manifest::Recorded&) {
    return {1};
}

// Different SPIR-V for different recorded inputs: what a missing input looks like.
static std::vector<u32> InputsSpirv(const Manifest::Recorded& recorded) {
    return {recorded.inputs.user_data[0]};
}

static std::optional<Manifest::Manifest> BuildFrom(const fs::path& game,
                                                   std::vector<fs::path> caches,
                                                   Manifest::BuildStats& stats, std::string& error,
                                                   const Manifest::Translator& translator) {
    return Manifest::Build("TEST00000", game, caches, translator, stats, error);
}

int main() {
    const auto dir = fs::temp_directory_path() / "spartanport-shader-manifest-test";
    fs::remove_all(dir);
    const auto game = dir / "game";
    const std::array<u64, 3> hashes{0xAAAA, 0xBBBB, 0xCCCC};
    WriteGameFile(game, hashes);

    // Two sessions numbered the same permutations differently and share one of them.
    const auto a = dir / "a", b = dir / "b";
    const u64 a1 = ComputePermutation(a, 0xAAAA, 0, 10);
    const u64 a2 = ComputePermutation(a, 0xAAAA, 1, 20);
    const u64 b1 = ComputePermutation(b, 0xAAAA, 5, 20);
    ComputePermutation(b, 0xBBBB, 6, 30);
    ComputePermutation(b, 0xDDDD, 7, 40); // not in the game files
    ComputeKey(a, a1);
    ComputeKey(a, a2);
    ComputeKey(b, b1); // the same pipeline as a2 once remapped
    ComputeKey(b, 0x1234); // names nothing

    Manifest::BuildStats stats;
    std::string error;
    auto manifest = BuildFrom(game, {a, b}, stats, error, SameSpirv);
    assert(manifest);
    assert(manifest->programs.size() == 2);
    assert(manifest->permutations.size() == 3);
    assert(manifest->pipelines.size() == 2);
    assert(stats.merged == 1 && stats.no_code == 1 && stats.stale_keys == 1);

    // perm_idx counts within the program, whatever the sessions used.
    assert(manifest->programs[0].pgm_hash == 0xAAAA);
    assert(manifest->programs[0].file == "EXEC/WAD/PS4_LE/a.WAD");
    assert(manifest->permutations[0].perm_idx == 0 && manifest->permutations[1].perm_idx == 1);
    assert(manifest->permutations[2].program == 1 && manifest->permutations[2].perm_idx == 0);

    // The key of b's permutation 5 and a's permutation 1 point at one manifest permutation.
    std::set<u32> named;
    for (const auto& pipeline : manifest->pipelines) {
        assert(pipeline.stages[0].permutation < manifest->permutations.size());
        assert(pipeline.is_compute == 1);
        named.insert(pipeline.stages[0].permutation);
    }
    assert(named.size() == 2);

    // Round trip, same bytes twice, no game code.
    const auto bytes = Manifest::Encode(*manifest);
    u64 id{};
    const auto back = Manifest::Decode(bytes, &id);
    assert(back && id != 0);
    assert(Manifest::Encode(*back) == bytes);
    assert(back->serial == "TEST00000" && back->versions == Manifest::CurrentVersions());
    assert(back->permutations[1].inputs == manifest->permutations[1].inputs);

    Manifest::BuildStats second_stats;
    assert(Manifest::Encode(*BuildFrom(game, {a, b}, second_stats, error, SameSpirv)) == bytes);

    const auto payload_has_marker = [&] {
        Serialization::Archive ar;
        Serialization::Writer writer{ar};
        for (const auto& permutation : manifest->permutations) {
            permutation.inputs.Serialize(ar);
            writer.Write(permutation.spec.data(), permutation.spec.size());
        }
        const std::vector<u8> raw = ar.TakeOff();
        for (size_t i = 0; i + 4 <= raw.size(); ++i) {
            u32 word;
            std::memcpy(&word, raw.data() + i, 4);
            if (word == CodeMarker) {
                return true;
            }
        }
        return false;
    };
    assert(!payload_has_marker());

    // Damage is refused, not read.
    auto damaged = bytes;
    damaged[damaged.size() / 2] ^= 0xFF;
    assert(!Manifest::Decode(damaged));
    damaged = bytes;
    damaged[0] = 'X';
    assert(!Manifest::Decode(damaged));
    assert(!Manifest::Decode(std::span<const u8>{bytes.data(), 8}));
    assert(!Manifest::Decode({}));

    // Equal specialization with different SPIR-V stops the build.
    const auto c = dir / "c", d = dir / "d";
    ComputePermutation(c, 0xCCCC, 0, 50, 1);
    ComputePermutation(d, 0xCCCC, 0, 50, 2);
    Manifest::BuildStats conflict_stats;
    assert(!BuildFrom(game, {c, d}, conflict_stats, error, InputsSpirv));
    assert(error.find("different SPIR-V") != std::string::npos);

    // A cache of another version is refused.
    const auto old = dir / "old";
    Serialization::Archive ar;
    Serialization::Writer writer{ar};
    writer.Write(u32{7});
    writer.Write(u32{0});
    Write(old / "0x0000000000000001.meta", std::vector<u8>{ar.TakeOff()});
    Manifest::BuildStats old_stats;
    assert(!BuildFrom(game, {old}, old_stats, error, SameSpirv));

    fs::remove_all(dir);
    std::printf("PASS: shader manifest\n");
    return 0;
}
