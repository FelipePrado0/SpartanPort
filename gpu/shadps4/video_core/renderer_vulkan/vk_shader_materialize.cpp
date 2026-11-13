// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>

#include <fmt/format.h>

#include "common/hash.h"
#include "common/logging/log.h"
#include "shader_recompiler/dump_index.h"
#include "shader_recompiler/info.h"
#include "shader_recompiler/recompiler.h"
#include "video_core/cache_storage.h"
#include "video_core/renderer_vulkan/vk_compute_pipeline.h"
#include "video_core/renderer_vulkan/vk_graphics_pipeline.h"
#include "video_core/renderer_vulkan/vk_pipeline_serialization.h"
#include "video_core/renderer_vulkan/vk_shader_materialize.h"

namespace fs = std::filesystem;

namespace Vulkan::Manifest {

namespace {

// What RegisterShaderMeta writes before the specialization: two versions, perm_hash, perm_idx.
constexpr size_t MetaHeader = sizeof(u32) * 2 + sizeof(u64) + sizeof(size_t);

u64 PermHash(u64 pgm_hash, u32 perm_idx) {
    return HashCombine(pgm_hash, u64(perm_idx));
}

enum class Saved { None, Same, Other };

Saved SavedPermutation(const std::string& name, std::span<const u8> spec) {
    std::vector<u8> meta;
    Storage::DataBase::Instance().Load(Storage::BlobType::ShaderMeta, name, meta);
    if (meta.size() < MetaHeader) {
        return Saved::None;
    }
    const auto versions = CurrentVersions();
    u32 meta_version{}, binary_version{};
    std::memcpy(&meta_version, meta.data(), sizeof(u32));
    std::memcpy(&binary_version, meta.data() + sizeof(u32), sizeof(u32));
    if (meta_version != versions.meta || binary_version != versions.binary) {
        return Saved::None; // another build's file: the game refuses it, so it is replaced
    }
    return meta.size() >= MetaHeader + spec.size() &&
                   std::equal(spec.begin(), spec.end(), meta.begin() + MetaHeader)
               ? Saved::Same
               : Saved::Other;
}

} // namespace

Replay DefaultReplay(const Shader::Profile& profile) {
    return [profile](std::span<const u32> code, const Program& program,
                     const Permutation& permutation, const Shader::StageSpecialization& spec,
                     Shader::Info& info) {
        thread_local Shader::Pools pools;
        auto spv = Shader::ReplayTranslate(code, Shader::HwStage(program.hw_stage),
                                           Shader::SwStage(program.sw_stage), program.pgm_hash,
                                           permutation.inputs, spec, profile, pools, info);
        pools.ReleaseContents();
        return spv;
    };
}

MaterializeStats Materialize(const Manifest& manifest, const fs::path& game_dir,
                             const Replay& replay, u32 threads, bool resume,
                             const std::function<bool(u32, u32)>& progress) {
    MaterializeStats stats;
    const u32 total = u32(manifest.permutations.size());
    // 1: in the cache when this run ends. Each permutation is written by one thread only.
    std::vector<u8> present(total);

    if (resume) {
        // Listed once: a Load of a missing file logs an error.
        auto& db = Storage::DataBase::Instance();
        const auto metas = db.ListBlobs(Storage::BlobType::ShaderMeta);
        const auto binaries = db.ListBlobs(Storage::BlobType::ShaderBinary);
        const std::set<std::string> saved_metas(metas.begin(), metas.end());
        const std::set<std::string> saved_binaries(binaries.begin(), binaries.end());
        for (u32 i = 0; i < total; ++i) {
            const auto& permutation = manifest.permutations[i];
            const auto& program = manifest.programs[permutation.program];
            const auto meta =
                fmt::format("{:#018x}", PermHash(program.pgm_hash, permutation.perm_idx));
            const auto saved = saved_metas.contains(meta)
                                   ? SavedPermutation(meta, permutation.spec)
                                   : Saved::None;
            if (saved == Saved::Other) {
                stats.conflict = true;
                return stats;
            }
            present[i] = saved == Saved::Same &&
                         saved_binaries.contains(
                             fmt::format("{:#018x}_{}", program.pgm_hash, permutation.perm_idx));
            stats.kept += present[i];
        }
    }

    // The title screen's shaders first: a skip there still leaves the start smooth.
    std::vector<std::vector<u32>> by_program(manifest.programs.size());
    std::vector<u8> first_priority(manifest.programs.size(), PriorityGame);
    for (u32 i = 0; i < total; ++i) {
        const auto& permutation = manifest.permutations[i];
        by_program[permutation.program].push_back(i);
        first_priority[permutation.program] =
            std::min(first_priority[permutation.program], permutation.priority);
    }
    std::vector<u32> order(manifest.programs.size());
    for (u32 i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::ranges::stable_sort(order, {}, [&](u32 program) { return first_priority[program]; });

    std::atomic<u32> done{stats.kept}, written{}, no_code{}, failed{}, next{};
    std::atomic<bool> stop{};
    // One index per game file a mod or update changed, built once: never every game file, which
    // takes minutes and held every worker.
    struct FileIndex {
        std::once_flag once;
        Shader::DumpIndex index;
    };
    std::mutex index_mutex;
    std::map<std::string, std::unique_ptr<FileIndex>> file_indexes;

    const auto run_program = [&](u32 program_idx) {
        const auto& program = manifest.programs[program_idx];
        std::vector<u32> todo;
        for (const u32 i : by_program[program_idx]) {
            if (!present[i]) {
                todo.push_back(i);
            }
        }
        if (todo.empty()) {
            return;
        }
        auto code = Shader::ReadShaderCode(
            game_dir, {.file = program.file, .offset = program.offset, .size = program.code_size},
            program.code_xxh3);
        if (!code) {
            // A replaced game file may keep the shader at another offset: index that file once.
            FileIndex* file_index;
            {
                std::scoped_lock lock{index_mutex};
                auto& slot = file_indexes[program.file];
                if (!slot) {
                    slot = std::make_unique<FileIndex>();
                }
                file_index = slot.get();
            }
            std::call_once(file_index->once, [&] {
                LOG_INFO(Render_Vulkan, "Shader {:#x} is not where the shader list says: "
                                        "indexing {}", program.pgm_hash, program.file);
                file_index->index = Shader::IndexGameFile(game_dir, program.file);
            });
            if (const auto found = file_index->index.find(program.pgm_hash);
                found != file_index->index.end()) {
                code = Shader::ReadShaderCode(game_dir, found->second, program.code_xxh3);
            }
        }
        if (!code) {
            no_code += u32(todo.size());
            done += u32(todo.size());
            return;
        }
        for (const u32 i : todo) {
            const auto& permutation = manifest.permutations[i];
            Serialization::Archive spec_ar{std::vector<u8>{permutation.spec}};
            Shader::StageSpecialization spec;
            spec.Deserialize(spec_ar);
            Shader::Info info;
            std::vector<u32> spv;
            try {
                spv = replay(*code, program, permutation, spec, info);
            } catch (const std::exception& error) {
                // Left to the game: one bad shader must not end the preparation.
                LOG_WARNING(Render_Vulkan, "Shader {:#x} not prepared: {}", program.pgm_hash,
                            error.what());
            }
            if (spv.empty()) {
                ++failed;
            } else {
                RegisterShaderBinary(std::move(spv), program.pgm_hash, permutation.perm_idx);
                RegisterShaderMeta(info, spec.fetch_shader_data, spec,
                                   PermHash(program.pgm_hash, permutation.perm_idx),
                                   permutation.perm_idx, permutation.inputs);
                present[i] = 1;
                ++written;
            }
            ++done;
        }
    };

    if (threads == 0) {
        for (const u32 program : order) {
            run_program(program);
            if (!progress(done, total)) {
                stop = true;
                break;
            }
        }
    } else {
        std::atomic<u32> running{threads};
        std::vector<std::jthread> workers;
        for (u32 t = 0; t < threads; ++t) {
            workers.emplace_back([&] {
                for (u32 n; !stop && (n = next++) < order.size();) {
                    run_program(order[n]);
                }
                --running;
            });
        }
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if (!stop && !progress(done, total)) {
                stop = true;
            }
        }
    }
    stats.stopped = stop;
    stats.written = written;
    stats.no_code = no_code;
    stats.failed = failed;

    // Keys whose every shader is in the cache, with the hashes the game gives those permutations.
    for (const auto& pipeline : manifest.pipelines) {
        std::array<u64, MaxPipelineStages> hashes{};
        bool complete = true;
        for (size_t stage = 0; stage < pipeline.stages.size(); ++stage) {
            const u32 ref = pipeline.stages[stage].permutation;
            if (ref == NoStage) {
                continue;
            }
            if (ref >= total || !present[ref]) {
                complete = false;
                break;
            }
            const auto& permutation = manifest.permutations[ref];
            hashes[stage] =
                PermHash(manifest.programs[permutation.program].pgm_hash, permutation.perm_idx);
        }
        if (!complete) {
            continue;
        }
        Serialization::Archive ar{std::vector<u8>{pipeline.key}};
        if (pipeline.is_compute) {
            ComputePipelineKey key{};
            ComputePipeline::SerializationSupport support;
            const bool key_ok = key.Deserialize(ar);
            if (!support.Deserialize(ar) || !key_ok) {
                continue;
            }
            key.value = hashes[0];
            RegisterPipelineData(key, support);
        } else {
            GraphicsPipelineKey key;
            GraphicsPipeline::SerializationSupport support;
            const bool key_ok = key.Deserialize(ar);
            if (!support.Deserialize(ar) || !key_ok) {
                continue; // a key that does not fit the hardware is not registered
            }
            std::copy_n(hashes.begin(), key.stage_hashes.size(), key.stage_hashes.begin());
            RegisterPipelineData(key, std::hash<GraphicsPipelineKey>{}(key), support);
        }
        ++stats.keys;
    }
    return stats;
}

} // namespace Vulkan::Manifest
