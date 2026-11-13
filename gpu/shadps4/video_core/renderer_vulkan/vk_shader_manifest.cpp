// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <unordered_map>

#include <fmt/format.h>
#include <miniz.h>
#include <xxhash.h>

#include "common/serdes.h"
#include "shader_recompiler/dump_index.h"
#include "video_core/renderer_vulkan/vk_compute_pipeline.h"
#include "video_core/renderer_vulkan/vk_graphics_pipeline.h"
#include "video_core/renderer_vulkan/vk_shader_manifest.h"

namespace fs = std::filesystem;

namespace Vulkan::Manifest {

namespace {

constexpr char Magic[4] = {'S', 'P', 'S', 'M'};
constexpr u32 FormatVersion = 1;
constexpr u64 MaxPayload = u64{1} << 30;
constexpr u32 MaxPermutations = 64; // PipelineCache::IndexDiskShaders reads no index past this
constexpr auto TitleWindow = std::chrono::minutes(2);

static_assert(MaxPipelineStages >= MaxShaderStages);

struct Header {
    char magic[4];
    u32 format;
    u64 id;
    u64 payload_size;
};

std::vector<u8> ReadFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }
    std::vector<u8> data(size_t(file.tellg()));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(data.data()), std::streamsize(data.size()));
    return file ? data : std::vector<u8>{};
}

std::vector<fs::path> FilesWith(const fs::path& dir, std::string_view extension) {
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() == extension) {
            files.push_back(entry.path());
        }
    }
    std::ranges::sort(files);
    return files;
}

void WriteBytes(Serialization::Writer& writer, std::span<const u8> bytes) {
    writer.Write(bytes.size());
    if (!bytes.empty()) {
        writer.Write(bytes.data(), bytes.size());
    }
}

void ReadBytes(Serialization::Reader& reader, std::vector<u8>& bytes) {
    size_t size{};
    reader.Read(size);
    bytes.resize(size);
    if (size) {
        reader.Read(bytes.data(), size);
    }
}

std::vector<u8> SerializeSpec(const Shader::StageSpecialization& spec) {
    Serialization::Archive ar;
    spec.Serialize(ar);
    return std::move(ar.TakeOff());
}

struct Candidate {
    Recorded recorded;
    std::vector<u8> spec_bytes;
    u8 priority{};
    std::vector<std::pair<size_t, u64>> sources; ///< Cache index and perm_hash of each recording.
};

using SessionKey = std::pair<size_t, u64>; // cache index, perm_hash of that session

std::optional<Recorded> ReadMeta(const fs::path& path, const Versions& versions, u64& perm_hash) {
    auto bytes = ReadFile(path);
    if (bytes.size() < sizeof(u32) * 2) {
        return std::nullopt;
    }
    Serialization::Archive ar{std::move(bytes)};
    Serialization::Reader meta{ar};
    u32 meta_version{}, binary_version{};
    size_t perm_idx{};
    meta.Read(meta_version);
    meta.Read(binary_version);
    if (meta_version != versions.meta) {
        return std::nullopt;
    }
    meta.Read(perm_hash);
    meta.Read(perm_idx);
    Recorded recorded;
    Shader::Info info;
    recorded.spec.Deserialize(ar);
    info.Deserialize(ar);
    recorded.inputs.Deserialize(ar);
    recorded.pgm_hash = info.pgm_hash;
    recorded.hw_stage = info.hw_stage;
    recorded.sw_stage = info.sw_stage;
    return recorded;
}

void MarkTitlePermutations(const std::vector<std::pair<fs::path, u64>>& metas, size_t cache,
                           std::map<SessionKey, u8>& priority) {
    if (metas.empty()) {
        return;
    }
    auto first = fs::file_time_type::max();
    for (const auto& [path, hash] : metas) {
        first = std::min(first, fs::last_write_time(path));
    }
    for (const auto& [path, hash] : metas) {
        priority[{cache, hash}] =
            fs::last_write_time(path) - first <= TitleWindow ? PriorityTitle : PriorityGame;
    }
}

/// Key and support of a saved pipeline, stage hashes zeroed, and the hashes it named.
std::optional<Pipeline> ReadKey(const fs::path& path, const Versions& versions,
                                std::array<u64, MaxPipelineStages>& hashes) {
    auto bytes = ReadFile(path);
    if (bytes.size() < sizeof(u32) * 2) {
        return std::nullopt;
    }
    Serialization::Archive ar{std::move(bytes)};
    Serialization::Reader reader{ar};
    u32 version{};
    Pipeline pipeline;
    reader.Read(version);
    if (version != versions.pipeline) {
        return std::nullopt;
    }
    reader.Read(pipeline.is_compute);
    Serialization::Archive out;
    if (pipeline.is_compute) {
        ComputePipelineKey key{};
        ComputePipeline::SerializationSupport support;
        key.Deserialize(ar);
        support.Deserialize(ar);
        hashes[0] = key.value;
        key.value = 0;
        key.Serialize(out);
        support.Serialize(out);
    } else {
        GraphicsPipelineKey key;
        GraphicsPipeline::SerializationSupport support;
        // A recorded key that does not fit the hardware is refused, never shipped.
        if (!key.Deserialize(ar) || !support.Deserialize(ar)) {
            return std::nullopt;
        }
        std::ranges::copy(key.stage_hashes, hashes.begin());
        key.stage_hashes.fill(0);
        key.Serialize(out);
        support.Serialize(out);
    }
    pipeline.key = std::move(out.TakeOff());
    return pipeline;
}

} // namespace

std::optional<Manifest> Build(const std::string& serial, const fs::path& game_dir,
                              std::span<const fs::path> caches, const Translator& translator,
                              BuildStats& stats, std::string& error) {
    const auto versions = CurrentVersions();
    const auto index = Shader::IndexDump(game_dir);

    std::map<u64, std::vector<Candidate>> programs;
    for (size_t cache = 0; cache < caches.size(); ++cache) {
        std::vector<std::pair<fs::path, u64>> metas;
        std::map<SessionKey, u8> priority;
        std::vector<std::pair<u64, Recorded>> recorded;
        for (const auto& path : FilesWith(caches[cache], ".meta")) {
            u64 perm_hash{};
            auto entry = ReadMeta(path, versions, perm_hash);
            if (!entry) {
                error = fmt::format("{}: no saved inputs, record it with this build",
                                    path.string());
                return std::nullopt;
            }
            metas.emplace_back(path, perm_hash);
            recorded.emplace_back(perm_hash, std::move(*entry));
        }
        MarkTitlePermutations(metas, cache, priority);
        for (auto& [perm_hash, entry] : recorded) {
            const SessionKey source{cache, perm_hash};
            auto& candidates = programs[entry.pgm_hash];
            // The operator compares against the bound resources of the other side.
            static const Shader::Info no_info{};
            entry.spec.info = &no_info;
            const auto same = std::ranges::find_if(candidates, [&](const Candidate& candidate) {
                return candidate.recorded.spec == entry.spec;
            });
            if (same == candidates.end()) {
                auto bytes = SerializeSpec(entry.spec);
                candidates.push_back({std::move(entry), std::move(bytes), priority[source], {source}});
                continue;
            }
            if (translator(same->recorded) != translator(entry)) {
                error = fmt::format("shader {:#x}: equal specialization, different SPIR-V: an input "
                                    "is not recorded",
                                    entry.pgm_hash);
                return std::nullopt;
            }
            same->priority = std::min(same->priority, priority[source]);
            same->sources.push_back(source);
            ++stats.merged;
        }
    }

    Manifest manifest{.serial = serial, .versions = versions};
    std::map<SessionKey, u32> sessions; // to the index in manifest.permutations
    for (auto& [pgm_hash, candidates] : programs) {
        const auto location = index.find(pgm_hash);
        if (location == index.end()) {
            stats.no_code += u32(candidates.size());
            continue;
        }
        const auto order = [](const Candidate& candidate) {
            return std::make_pair(XXH3_64bits(candidate.spec_bytes.data(), candidate.spec_bytes.size()),
                                  candidate.spec_bytes);
        };
        std::ranges::sort(candidates, {}, order);
        const u32 program = u32(manifest.programs.size());
        const auto& first = candidates.front().recorded;
        if (candidates.size() > MaxPermutations) {
            stats.too_many += u32(candidates.size()) - MaxPermutations;
            candidates.resize(MaxPermutations);
        }
        manifest.programs.push_back({.pgm_hash = pgm_hash,
                                     .hw_stage = u32(first.hw_stage),
                                     .sw_stage = u32(first.sw_stage),
                                     .file = location->second.file.generic_string(),
                                     .offset = location->second.offset,
                                     .code_size = first.inputs.code_size,
                                     .code_xxh3 = first.inputs.code_xxh3});
        for (u32 idx = 0; idx < candidates.size(); ++idx) {
            auto& candidate = candidates[idx];
            if (candidate.recorded.inputs.code_xxh3 != first.inputs.code_xxh3) {
                error = fmt::format("shader {:#x}: recordings of different code", pgm_hash);
                return std::nullopt;
            }
            for (const auto& source : candidate.sources) {
                sessions[source] = u32(manifest.permutations.size());
            }
            manifest.permutations.push_back({.program = program,
                                             .perm_idx = idx,
                                             .priority = candidate.priority,
                                             .spec = std::move(candidate.spec_bytes),
                                             .inputs = std::move(candidate.recorded.inputs)});
        }
    }

    using PipelineId = std::pair<std::vector<u8>, std::array<StageRef, MaxPipelineStages>>;
    std::map<PipelineId, u32> pipelines; // compute or not
    for (size_t cache = 0; cache < caches.size(); ++cache) {
        for (const auto& path : FilesWith(caches[cache], ".key")) {
            std::array<u64, MaxPipelineStages> hashes{};
            auto pipeline = ReadKey(path, versions, hashes);
            if (!pipeline) {
                error = fmt::format("{}: pipeline key of another version", path.string());
                return std::nullopt;
            }
            bool stale = false;
            for (size_t stage = 0; stage < hashes.size(); ++stage) {
                if (!hashes[stage]) {
                    continue;
                }
                const auto session = sessions.find({cache, hashes[stage]});
                if (session == sessions.end()) {
                    stale = true;
                    break;
                }
                pipeline->stages[stage].permutation = session->second;
            }
            if (stale) {
                ++stats.stale_keys;
                continue;
            }
            pipelines[{std::move(pipeline->key), pipeline->stages}] = pipeline->is_compute;
        }
    }
    for (const auto& [id, is_compute] : pipelines) {
        manifest.pipelines.push_back({is_compute, id.first, id.second});
    }
    return manifest;
}

std::vector<u8> Encode(const Manifest& manifest) {
    Serialization::Archive ar;
    Serialization::Writer writer{ar};
    writer.Write(manifest.serial);
    writer.Write(manifest.versions);
    writer.Write(manifest.programs.size());
    for (const auto& program : manifest.programs) {
        writer.Write(program.pgm_hash);
        writer.Write(program.hw_stage);
        writer.Write(program.sw_stage);
        writer.Write(program.file);
        writer.Write(program.offset);
        writer.Write(program.code_size);
        writer.Write(program.code_xxh3);
    }
    writer.Write(manifest.permutations.size());
    for (const auto& permutation : manifest.permutations) {
        writer.Write(permutation.program);
        writer.Write(permutation.perm_idx);
        writer.Write(permutation.priority);
        WriteBytes(writer, permutation.spec);
        permutation.inputs.Serialize(ar);
    }
    writer.Write(manifest.pipelines.size());
    for (const auto& pipeline : manifest.pipelines) {
        writer.Write(pipeline.is_compute);
        WriteBytes(writer, pipeline.key);
        writer.Write(pipeline.stages.data(), sizeof(pipeline.stages));
    }
    const std::vector<u8> payload = std::move(ar.TakeOff());

    mz_ulong packed_size = mz_compressBound(mz_ulong(payload.size()));
    std::vector<u8> out(sizeof(Header) + packed_size);
    const int result = mz_compress2(out.data() + sizeof(Header), &packed_size, payload.data(),
                                    mz_ulong(payload.size()), MZ_BEST_COMPRESSION);
    ASSERT(result == MZ_OK);
    out.resize(sizeof(Header) + packed_size);
    Header header{};
    std::memcpy(header.magic, Magic, sizeof(Magic));
    header.format = FormatVersion;
    header.id = XXH3_64bits(payload.data(), payload.size());
    header.payload_size = payload.size();
    std::memcpy(out.data(), &header, sizeof(header));
    return out;
}

std::optional<Manifest> Decode(std::span<const u8> data, u64* id) {
    Header header;
    if (data.size() < sizeof(header)) {
        return std::nullopt;
    }
    std::memcpy(&header, data.data(), sizeof(header));
    if (std::memcmp(header.magic, Magic, sizeof(Magic)) != 0 || header.format != FormatVersion ||
        header.payload_size == 0 || header.payload_size > MaxPayload) {
        return std::nullopt;
    }
    std::vector<u8> payload(header.payload_size);
    mz_ulong size = mz_ulong(payload.size());
    if (mz_uncompress(payload.data(), &size, data.data() + sizeof(header),
                      mz_ulong(data.size() - sizeof(header))) != MZ_OK ||
        size != payload.size() || XXH3_64bits(payload.data(), size) != header.id) {
        return std::nullopt;
    }
    // Past the hash the content is what Encode wrote, so the reader's own checks cannot fire.
    Serialization::Archive ar{std::move(payload)};
    Serialization::Reader reader{ar};
    Manifest manifest;
    reader.Read(manifest.serial);
    reader.Read(manifest.versions);
    size_t count{};
    reader.Read(count);
    manifest.programs.resize(count);
    for (auto& program : manifest.programs) {
        reader.Read(program.pgm_hash);
        reader.Read(program.hw_stage);
        reader.Read(program.sw_stage);
        reader.Read(program.file);
        reader.Read(program.offset);
        reader.Read(program.code_size);
        reader.Read(program.code_xxh3);
    }
    reader.Read(count);
    manifest.permutations.resize(count);
    for (auto& permutation : manifest.permutations) {
        reader.Read(permutation.program);
        reader.Read(permutation.perm_idx);
        reader.Read(permutation.priority);
        ReadBytes(reader, permutation.spec);
        permutation.inputs.Deserialize(ar);
    }
    reader.Read(count);
    manifest.pipelines.resize(count);
    for (auto& pipeline : manifest.pipelines) {
        reader.Read(pipeline.is_compute);
        ReadBytes(reader, pipeline.key);
        reader.Read(pipeline.stages.data(), sizeof(pipeline.stages));
    }
    if (id) {
        *id = header.id;
    }
    return manifest;
}

} // namespace Vulkan::Manifest
