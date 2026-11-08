// SPDX-License-Identifier: GPL-2.0-or-later
// Writes a small manifest into an empty shader cache with a stand-in translation: file names,
// permutation hashes, pipeline keys, stopping and resuming, missing code, a cache that disagrees.
#include <atomic>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <fmt/format.h>
#include <xxhash.h>

#include "../gpu/spartanportgpu.h"
#include "common/elf_info.h"
#include "common/hash.h"
#include "common/path_util.h"
#include "common/serdes.h"
#include "shader_recompiler/info.h"
#include "video_core/cache_storage.h"
#include "video_core/renderer_vulkan/vk_compute_pipeline.h"
#include "video_core/renderer_vulkan/vk_graphics_pipeline.h"
#include "video_core/renderer_vulkan/vk_pipeline_serialization.h"
#include "video_core/renderer_vulkan/vk_shader_materialize.h"

#undef NDEBUG
#include <cassert>

// The production metadata shim (gpu/shim/spartanportgpu.cpp), without its window and device.
namespace Core {
class Emulator {
public:
    static void FillElfInfo(const SpartanGpuConfig& config) {
        auto& info = Common::ElfInfo::Instance();
        info.initialized = true;
        info.game_serial = config.serial ? config.serial : "UNKNOWN";
        info.title = config.title ? config.title : "";
        info.sdk_ver = config.sdk_version;
        info.psf_attributes.raw = config.psf_attributes;
    }
};
} // namespace Core

namespace fs = std::filesystem;
using namespace Vulkan;

static std::vector<u8> Spec(u32 divisor) {
    Shader::StageSpecialization spec;
    spec.vs_attribs.push_back({.divisor = divisor});
    Serialization::Archive ar;
    spec.Serialize(ar);
    return std::move(ar.TakeOff());
}

template <typename Key, typename Support>
static std::vector<u8> KeyBytes(const Key& key) {
    Support support{};
    Serialization::Archive ar;
    key.Serialize(ar);
    support.Serialize(ar);
    return std::move(ar.TakeOff());
}

static std::atomic<u32> replays{0};

static std::vector<u32> StandIn(std::span<const u32> code, const Manifest::Program& program,
                                const Manifest::Permutation& permutation,
                                const Shader::StageSpecialization&, Shader::Info& info) {
    ++replays;
    info.pgm_hash = program.pgm_hash;
    info.hw_stage = Shader::HwStage(program.hw_stage);
    info.sw_stage = Shader::SwStage(program.sw_stage);
    return {code[0], permutation.perm_idx};
}

static bool Has(const fs::path& cache, const std::string& name) {
    return fs::exists(cache / name);
}

static size_t Count(const fs::path& cache, std::string_view extension) {
    size_t count = 0;
    for (const auto& entry : fs::directory_iterator(cache)) {
        count += entry.path().extension() == extension;
    }
    return count;
}

int main() {
    const auto dir = fs::temp_directory_path() / "spartanport-materialize-test";
    fs::remove_all(dir);
    const auto game = dir / "game";
    fs::create_directories(game / "EXEC/WAD");

    // Two programs in one game file, a third the game folder does not hold.
    std::vector<u32> code(16);
    for (u32 i = 0; i < code.size(); ++i) {
        code[i] = 0x1000 + i;
    }
    std::ofstream(game / "EXEC/WAD/a.WAD", std::ios::binary)
        .write(reinterpret_cast<const char*>(code.data()), std::streamsize(code.size() * 4));
    const auto xxh = [&](size_t word) { return XXH3_64bits(code.data() + word, 32); };

    Manifest::Manifest manifest{.serial = "TEST00000", .versions = Manifest::CurrentVersions()};
    using Shader::HwStage;
    using Shader::SwStage;
    manifest.programs = {
        {0xAAAA, u32(HwStage::Compute), u32(SwStage::Compute), "EXEC/WAD/a.WAD", 0, 32, xxh(0)},
        {0xBBBB, u32(HwStage::Vertex), u32(SwStage::Vertex), "EXEC/WAD/a.WAD", 32, 32, xxh(8)},
        {0xCCCC, u32(HwStage::Vertex), u32(SwStage::Vertex), "EXEC/WAD/a.WAD", 0, 32, 0x51},
    };
    manifest.permutations = {
        {.program = 0, .perm_idx = 0, .spec = Spec(10)},
        {.program = 0, .perm_idx = 1, .spec = Spec(20)},
        {.program = 1, .perm_idx = 0, .priority = Manifest::PriorityTitle, .spec = Spec(30)},
        {.program = 2, .perm_idx = 0, .spec = Spec(40)},
    };
    Manifest::Pipeline compute{.is_compute = 1,
                               .key = KeyBytes<ComputePipelineKey,
                                               ComputePipeline::SerializationSupport>({})};
    compute.stages[0].permutation = 1;
    Manifest::Pipeline graphics{.key = KeyBytes<GraphicsPipelineKey,
                                                GraphicsPipeline::SerializationSupport>({})};
    graphics.stages[0].permutation = 2;
    Manifest::Pipeline orphan = graphics; // its shader has no code: never saved
    orphan.stages[0].permutation = 3;
    manifest.pipelines = {compute, graphics, orphan};

    Core::Emulator::FillElfInfo(SpartanGpuConfig{.serial = "TEST00000"});
    fs::create_directories(dir / "cache");
    Common::FS::SetUserPath(Common::FS::PathType::CacheDir, dir / "cache");
    auto& db = Storage::DataBase::Instance();
    db.Open();
    assert(db.IsOpened());
    const auto cache = dir / "cache" / "TEST00000";
    const auto flush = [&] { assert(db.Flush(std::chrono::seconds(10))); };

    // Stopped after the first program (the title one goes first), then resumed.
    auto stats = Manifest::Materialize(manifest, game, StandIn, 0, false,
                                       [](u32, u32) { return false; });
    flush();
    assert(stats.stopped && stats.written == 1 && replays == 1);
    assert(Has(cache, "0x000000000000bbbb_0.spv"));
    assert(!Has(cache, "0x000000000000aaaa_0.spv"));

    replays = 0;
    stats = Manifest::Materialize(manifest, game, StandIn, 4, true, [](u32, u32) { return true; });
    flush();
    assert(!stats.stopped && !stats.conflict);
    assert(stats.kept == 1 && stats.written == 2 && stats.no_code == 1 && replays == 2);
    assert(stats.keys == 2);

    // Names and hashes the game looks for: <pgm>_<perm>.spv, the permutation hash for .meta.
    for (const auto& [pgm, idx] : {std::pair{0xAAAAull, 0}, {0xAAAAull, 1}, {0xBBBBull, 0}}) {
        const u64 perm_hash = HashCombine(pgm, u64(idx));
        assert(Has(cache, fmt::format("{:#018x}_{}.spv", pgm, idx)));
        std::vector<u8> meta;
        db.Load(Storage::BlobType::ShaderMeta, fmt::format("{:#018x}", perm_hash), meta);
        Serialization::Archive ar{std::move(meta)};
        Shader::Info saved;
        std::optional<Shader::Gcn::FetchShaderData> fetch;
        Shader::StageSpecialization spec;
        size_t perm_idx{};
        assert(LoadShaderMeta(ar, saved, fetch, spec, perm_idx));
        assert(perm_idx == size_t(idx) && saved.pgm_hash == pgm);
    }
    assert(!Has(cache, "0x000000000000cccc_0.spv"));
    assert(Count(cache, ".spv") == 3 && Count(cache, ".meta") == 3);

    // Pipeline keys name the permutations they use, as a played session saves them.
    assert(Has(cache, fmt::format("c_{:#018x}.key", HashCombine(0xAAAAull, 1ull))));
    GraphicsPipelineKey key{};
    key.stage_hashes[0] = HashCombine(0xBBBBull, 0ull);
    assert(Has(cache, fmt::format("g_{:#018x}.key", std::hash<GraphicsPipelineKey>{}(key))));
    assert(Count(cache, ".key") == 2);

    // Done: nothing translated again.
    replays = 0;
    stats = Manifest::Materialize(manifest, game, StandIn, 4, true, [](u32, u32) { return true; });
    flush();
    assert(stats.kept == 3 && stats.written == 0 && replays == 0);

    // A session saved another shader at an index the manifest uses: refused, nothing written.
    Shader::Info other;
    other.pgm_hash = 0xAAAA;
    Serialization::Archive spec_ar{Spec(99)};
    Shader::StageSpecialization other_spec;
    other_spec.Deserialize(spec_ar);
    RegisterShaderMeta(other, std::nullopt, other_spec, HashCombine(0xAAAAull, 0ull), 0, {});
    flush();
    replays = 0;
    stats = Manifest::Materialize(manifest, game, StandIn, 4, true, [](u32, u32) { return true; });
    assert(stats.conflict && stats.written == 0 && replays == 0);

    db.Close();
    fs::remove_all(dir);
    std::printf("PASS: shader materialize\n");
    return 0;
}
