// SPDX-License-Identifier: GPL-2.0-or-later
// Verifier: translates every saved permutation again from the game's code plus its .meta, without
// guest memory, and compares with the saved .spv. Exit code 1 when any permutation differs.
// Usage: shader-replay-test <cache dir> <game dir or index file> [first] [count]
// An index file holds "hash offset size path" lines (dump-index-test writes one).
// REPLAY_DUMP=<dir> saves the SPIR-V of the permutations that differ.
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <xxhash.h>
#include "common/serdes.h"
#include "shader_recompiler/dump_index.h"
#include "shader_recompiler/recompiler.h"
#include "shader_recompiler/replay.h"
#include "shader_recompiler/specialization.h"

namespace fs = std::filesystem;

static std::vector<u8> ReadFile(const fs::path& path, size_t offset = 0, size_t size = 0) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return {};
    }
    if (!size) {
        f.seekg(0, std::ios::end);
        size = size_t(f.tellg()) - offset;
    }
    std::vector<u8> data(size);
    f.seekg(offset);
    f.read(reinterpret_cast<char*>(data.data()), size);
    return f ? data : std::vector<u8>{};
}

static Shader::DumpIndex LoadIndex(const fs::path& source) {
    if (fs::is_directory(source)) {
        return Shader::IndexDump(source);
    }
    Shader::DumpIndex index;
    std::ifstream in(source);
    std::string hash, path;
    Shader::ShaderLocation loc;
    while (in >> hash >> loc.offset >> loc.size && std::getline(in >> std::ws, path)) {
        loc.file = path;
        index[std::stoull(hash, nullptr, 16)] = loc;
    }
    return index;
}

// Meta files of version 7 hold no inputs: the buffer is the one a later draw left, and the vertex
// sharps come from the recorded number class and swizzle. Such files can differ.
static Shader::ReplayInputs LegacyInputs(const Shader::Info& saved,
                                         const Shader::StageSpecialization& spec) {
    Shader::ReplayInputs inputs;
    inputs.user_data.assign(saved.flattened_ud_buf.begin(), saved.flattened_ud_buf.begin() + 16);
    inputs.flat_ud = saved.flattened_ud_buf;
    if (spec.fetch_shader_data) {
        for (size_t a = 0; a < spec.fetch_shader_data->attributes.size(); ++a) {
            AmdGpu::Buffer b{};
            const auto vs = a < spec.vs_attribs.size() ? spec.vs_attribs[a]
                                                       : Shader::VsAttribSpecialization{};
            using Num = AmdGpu::NumberFormat;
            b.num_format = u32(vs.num_class == AmdGpu::NumberClass::Uint   ? Num::Uint
                               : vs.num_class == AmdGpu::NumberClass::Sint ? Num::Sint
                                                                           : Num::Float);
            b.data_format = u32(AmdGpu::DataFormat::Format32_32_32_32);
            b.dst_sel_x = u32(vs.dst_select.r);
            b.dst_sel_y = u32(vs.dst_select.g);
            b.dst_sel_z = u32(vs.dst_select.b);
            b.dst_sel_w = u32(vs.dst_select.a);
            inputs.fetch_buffers.push_back(b);
        }
    }
    return inputs;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <cache dir> <game dir or index> [first] [count]\n", argv[0]);
        return 2;
    }
    const fs::path cache = argv[1];
    const fs::path source = argv[2];
    const fs::path game_dir = fs::is_directory(source) ? source : fs::path{};
    const size_t first = argc > 3 ? std::stoul(argv[3]) : 0;
    const size_t count = argc > 4 ? std::stoul(argv[4]) : SIZE_MAX;

    const auto index = LoadIndex(source);
    const auto profile_bytes = ReadFile(cache / "profile.bin");
    Shader::Profile profile;
    if (!Shader::LoadReplayProfile(profile_bytes, profile)) {
        std::fprintf(stderr, "profile.bin size %zu is not %zu or %zu\n", profile_bytes.size(),
                     sizeof(Shader::Profile), sizeof(Shader::Profile) + 1);
        return 1;
    }

    std::vector<fs::path> metas;
    for (const auto& entry : fs::directory_iterator(cache)) {
        if (entry.path().extension() == ".meta") {
            metas.push_back(entry.path());
        }
    }
    std::sort(metas.begin(), metas.end());

    // stage name -> {same, different, skipped}
    std::map<std::string, std::array<u32, 3>> stats;
    double translate_ms = 0;
    u32 translated = 0, legacy = 0, motion = 0, wrong_code = 0;
    Shader::Pools pools;
    for (size_t i = first; i < metas.size() && i - first < count; ++i) {
        auto bytes = ReadFile(metas[i]);
        Serialization::Archive ar{std::move(bytes)};
        Serialization::Reader meta{ar};
        u32 meta_version{}, binary_version{};
        u64 perm_hash{};
        size_t perm_idx{};
        meta.Read(meta_version);
        meta.Read(binary_version);
        meta.Read(perm_hash);
        meta.Read(perm_idx);
        Shader::StageSpecialization spec;
        Shader::Info saved;
        spec.Deserialize(ar);
        saved.Deserialize(ar);

        auto stage = std::string(magic_enum::enum_name(saved.hw_stage));
        stage += saved.has_fetch_shader ? "+fetch" : "";
        auto& st = stats[stage];
        const auto spv_path = cache / fmt::format("{:#018x}_{}.spv", saved.pgm_hash, perm_idx);
        const auto expected = ReadFile(spv_path);
        const auto loc = index.find(saved.pgm_hash);
        const bool has_motion = saved.hw_stage == Shader::HwStage::Vertex &&
                                spec.runtime_info.hw.vs.motion_vectors;
        motion += has_motion;
        if (expected.empty() || loc == index.end() || has_motion) {
            ++st[2];
            continue;
        }
        Shader::ReplayInputs inputs;
        if (meta_version > 7) {
            inputs.Deserialize(ar);
        } else if (saved.flattened_ud_buf.size() >= 16) {
            inputs = LegacyInputs(saved, spec);
            ++legacy;
        } else {
            ++st[2];
            continue;
        }
        const auto code_bytes = ReadFile(game_dir / loc->second.file, loc->second.offset,
                                         loc->second.size);
        std::vector<u32> code(code_bytes.size() / 4);
        std::memcpy(code.data(), code_bytes.data(), code.size() * 4);
        if (inputs.code_size &&
            (code_bytes.size() != inputs.code_size ||
             XXH3_64bits(code_bytes.data(), code_bytes.size()) != inputs.code_xxh3)) {
            ++wrong_code;
            ++st[2];
            continue;
        }

        Shader::Info info;
        const auto t0 = std::chrono::steady_clock::now();
        const auto spv = Shader::ReplayTranslate(code, saved.hw_stage, saved.sw_stage, saved.pgm_hash,
                                                 inputs, spec, profile, pools, info);
        translate_ms +=
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        ++translated;
        pools.ReleaseContents();

        const bool same = spv.size() * 4 == expected.size() &&
                          std::memcmp(spv.data(), expected.data(), expected.size()) == 0;
        ++st[same ? 0 : 1];
        if (!same) {
            std::printf("differ: %s %s (%zu vs %zu bytes)\n", stage.c_str(),
                        metas[i].filename().string().c_str(), spv.size() * 4, expected.size());
            if (const char* dump = std::getenv("REPLAY_DUMP")) {
                std::ofstream(fs::path(dump) / spv_path.filename(), std::ios::binary)
                    .write(reinterpret_cast<const char*>(spv.data()), spv.size() * 4);
            }
        }
    }
    u32 different = 0;
    for (const auto& [stage, s] : stats) {
        std::printf("%-16s same %5u  different %5u  skipped %5u\n", stage.c_str(), s[0], s[1], s[2]);
        different += s[1];
    }
    std::printf("translated %u in %.0f ms (%.2f ms each); %u without saved inputs, %u with motion "
                "vectors, %u with other code\n",
                translated, translate_ms, translated ? translate_ms / translated : 0.0, legacy,
                motion, wrong_code);
    return different ? 1 : 0;
}
