// SPDX-License-Identifier: GPL-2.0-or-later
// POC: retranslate cached shaders from the game's WAD code plus the saved .meta, compare to .spv.
// Usage: shader-replay-test <cache dir> <shader_index.txt> [first] [count]
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <unordered_map>
#include "common/serdes.h"
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/recompiler.h"
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

struct Location {
    size_t offset, length;
    std::string path;
};

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <cache dir> <index> [first] [count]\n", argv[0]);
        return 2;
    }
    const fs::path cache = argv[1];
    const size_t first = argc > 3 ? std::stoul(argv[3]) : 0;
    const size_t count = argc > 4 ? std::stoul(argv[4]) : SIZE_MAX;

    std::unordered_map<u64, Location> index;
    {
        std::ifstream in(argv[2]);
        std::string hash, line;
        Location loc;
        while (in >> hash >> loc.offset >> loc.length && std::getline(in >> std::ws, loc.path)) {
            index[std::stoull(hash, nullptr, 16)] = loc;
        }
    }
    const auto profile_bytes = ReadFile(cache / "profile.bin");
    if (profile_bytes.size() != sizeof(Shader::Profile)) {
        std::fprintf(stderr, "profile.bin size %zu != %zu\n", profile_bytes.size(),
                     sizeof(Shader::Profile));
        return 1;
    }
    Shader::Profile profile;
    std::memcpy(&profile, profile_bytes.data(), sizeof(profile));

    std::vector<fs::path> metas;
    for (const auto& entry : fs::directory_iterator(cache)) {
        if (entry.path().extension() == ".meta") {
            metas.push_back(entry.path());
        }
    }
    std::sort(metas.begin(), metas.end());

    // stage name -> {match, differ, skipped}
    std::map<std::string, std::array<u32, 3>> stats;
    double translate_ms = 0;
    u32 translated = 0;
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

        std::string stage = std::string(magic_enum::enum_name(saved.hw_stage)) +
                            (saved.has_fetch_shader ? "+fetch" : "");
        if (saved.hw_stage == Shader::HwStage::Fragment) {
            const auto& fs = spec.runtime_info.hw.fs;
            stage += fs.addr_flags.ancillary_ena && fs.en_flags.ancillary_ena ? "+anc" : "";
        }
        auto& st = stats[stage];
        const auto spv_path = cache / fmt::format("{:#018x}_{}.spv", saved.pgm_hash, perm_idx);
        const auto expected = ReadFile(spv_path);
        const auto loc = index.find(saved.pgm_hash);
        if (expected.empty() || loc == index.end() || saved.flattened_ud_buf.size() < 16 ||
            (saved.hw_stage == Shader::HwStage::Vertex && spec.runtime_info.hw.vs.motion_vectors)) {
            ++st[2];
            std::printf("R %zu skip %s\n", i, stage.c_str());
            continue;
        }
        const auto code_bytes = ReadFile(loc->second.path, loc->second.offset, loc->second.length);
        std::vector<u32> code(code_bytes.size() / 4);
        std::memcpy(code.data(), code_bytes.data(), code.size() * 4);
        std::array<u32, 16> user_data;
        std::memcpy(user_data.data(), saved.flattened_ud_buf.data(), sizeof(user_data));

        Shader::Info info(saved.hw_stage, saved.sw_stage,
                          Shader::ShaderParams{.user_data = user_data, .code = code,
                                               .hash = saved.pgm_hash});
        Shader::Info::ud_snapshots[0] = {&info, user_data.data(), 16,
                                         saved.flattened_ud_buf.data(),
                                         u32(saved.flattened_ud_buf.size()), VAddr(code.data())};
        Shader::Info::num_ud_snapshots = 1;
        Shader::Info::replay_skip_srt_walk = true;

        // Vertex V#s are not saved: rebuild them from the recorded number class and swizzle.
        Shader::Gcn::FetchShaderSnapshot fetch;
        fetch.info = &info;
        fetch.fetch = spec.fetch_shader_data;
        if (fetch.fetch) {
            for (size_t a = 0; a < fetch.fetch->attributes.size(); ++a) {
                AmdGpu::Buffer b{};
                const auto vs = a < spec.vs_attribs.size() ? spec.vs_attribs[a]
                                                           : Shader::VsAttribSpecialization{};
                b.num_format = u32(vs.num_class == AmdGpu::NumberClass::Uint   ? AmdGpu::NumberFormat::Uint
                                   : vs.num_class == AmdGpu::NumberClass::Sint ? AmdGpu::NumberFormat::Sint
                                                                               : AmdGpu::NumberFormat::Float);
                b.data_format = u32(AmdGpu::DataFormat::Format32_32_32_32);
                b.dst_sel_x = u32(vs.dst_select.r);
                b.dst_sel_y = u32(vs.dst_select.g);
                b.dst_sel_z = u32(vs.dst_select.b);
                b.dst_sel_w = u32(vs.dst_select.a);
                fetch.buffers.push_back(b);
            }
        }
        Shader::Gcn::ScopedFetchShaderSnapshot scoped{fetch};

        auto runtime = spec.runtime_info;
        auto binding = spec.start;
        std::printf("T %zu %s\n", i, stage.c_str());
        std::fflush(stdout);
        const auto t0 = std::chrono::steady_clock::now();
        const auto program = Shader::TranslateProgram(code, pools, info, runtime, profile);
        const auto spv = Shader::Backend::SPIRV::EmitSPIRV(profile, runtime, program, binding);
        translate_ms +=
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        ++translated;
        Shader::Info::num_ud_snapshots = 0;

        const bool same = spv.size() * 4 == expected.size() &&
                          std::memcmp(spv.data(), expected.data(), expected.size()) == 0;
        ++st[same ? 0 : 1];
        std::string other = "-";
        for (size_t p = 0; !same && p < 16; ++p) {
            const auto alt = ReadFile(cache / fmt::format("{:#018x}_{}.spv", saved.pgm_hash, p));
            if (p != perm_idx && alt.size() == spv.size() * 4 &&
                std::memcmp(alt.data(), spv.data(), alt.size()) == 0) {
                other = fmt::format("matches_perm{}_not_{}", p, perm_idx);
            }
        }
        const auto age = std::chrono::duration_cast<std::chrono::minutes>(
                             fs::last_write_time(spv_path) - fs::last_write_time(metas[i]))
                             .count();
        std::printf("R %zu %s %s %s %s\n", i, same ? "match" : "differ", stage.c_str(),
                    age > 1 ? "spv-newer" : (age < -1 ? "spv-older" : "same-time"), other.c_str());
        std::fflush(stdout);
        if (const char* dump = std::getenv("REPLAY_DUMP"); dump && !same) {
            std::ofstream(fs::path(dump) / spv_path.filename(), std::ios::binary)
                .write(reinterpret_cast<const char*>(spv.data()), spv.size() * 4);
        }
        if (!same && st[1] <= 3) {
            std::printf("differ: %s %s (%zu vs %zu bytes)\n", stage.c_str(),
                        metas[i].filename().string().c_str(), spv.size() * 4, expected.size());
        }
    }
    for (const auto& [stage, s] : stats) {
        std::printf("%-16s match %5u  differ %5u  skipped %5u\n", stage.c_str(), s[0], s[1], s[2]);
    }
    std::printf("translated %u in %.0f ms (%.2f ms each)\n", translated, translate_ms,
                translated ? translate_ms / translated : 0.0);
    return 0;
}
