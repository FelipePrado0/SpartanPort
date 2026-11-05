// SPDX-License-Identifier: GPL-2.0-or-later
// Builds the shader list shipped with the port from recorded caches, and shows what one holds.
//   shader-manifest build --out <file> --serial <id> --game <folder> <cache> [<cache> ...]
//   shader-manifest info <file>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

#include "shader_recompiler/dump_index.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/recompiler.h"
#include "video_core/renderer_vulkan/vk_shader_manifest.h"

namespace fs = std::filesystem;
using namespace Vulkan;

static std::vector<u8> ReadAll(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

static int Build(int argc, char** argv) {
    fs::path out, game;
    std::string serial;
    std::vector<fs::path> caches;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--out" && i + 1 < argc) {
            out = argv[++i];
        } else if (arg == "--serial" && i + 1 < argc) {
            serial = argv[++i];
        } else if (arg == "--game" && i + 1 < argc) {
            game = argv[++i];
        } else {
            caches.emplace_back(arg);
        }
    }
    if (out.empty() || game.empty() || serial.empty() || caches.empty()) {
        std::fprintf(stderr, "build needs --out, --serial, --game and at least one cache\n");
        return 2;
    }
    const auto profile_bytes = ReadAll(caches[0] / "profile.bin");
    if (profile_bytes.size() != sizeof(Shader::Profile)) {
        std::fprintf(stderr, "%s: no usable profile.bin\n", caches[0].string().c_str());
        return 1;
    }
    Shader::Profile profile;
    std::memcpy(&profile, profile_bytes.data(), sizeof(profile));

    const auto index = Shader::IndexDump(game);
    Shader::Pools pools;
    const Manifest::Translator translator = [&](const Manifest::Recorded& recorded) {
        const auto location = index.find(recorded.pgm_hash);
        const auto code =
            location == index.end()
                ? std::nullopt
                : Shader::ReadShaderCode(game, location->second, recorded.inputs.code_xxh3);
        if (!code) {
            return std::vector<u32>{};
        }
        Shader::Info info;
        auto spv = Shader::ReplayTranslate(*code, recorded.hw_stage, recorded.sw_stage,
                                           recorded.pgm_hash, recorded.inputs, recorded.spec,
                                           profile, pools, info);
        pools.ReleaseContents();
        return spv;
    };

    Manifest::BuildStats stats;
    std::string error;
    const auto manifest = Manifest::Build(serial, game, caches, translator, stats, error);
    if (!manifest) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    const auto bytes = Manifest::Encode(*manifest);
    std::ofstream(out, std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    std::printf("%zu programs, %zu permutations, %zu pipelines, %.1f MB; merged %u, no code %u, "
                "over the limit %u, stale keys %u\n",
                manifest->programs.size(), manifest->permutations.size(),
                manifest->pipelines.size(), double(bytes.size()) / 1e6, stats.merged, stats.no_code,
                stats.too_many, stats.stale_keys);
    return 0;
}

static int Info(const fs::path& path) {
    const auto bytes = ReadAll(path);
    u64 id{};
    const auto manifest = Manifest::Decode(bytes, &id);
    if (!manifest) {
        std::fprintf(stderr, "%s: damaged or of another format\n", path.string().c_str());
        return 1;
    }
    std::array<u32, 8> stages{};
    u32 title{};
    for (const auto& permutation : manifest->permutations) {
        ++stages[manifest->programs[permutation.program].hw_stage & 7];
        title += permutation.priority == Manifest::PriorityTitle;
    }
    std::printf("%s id %016llx, %.1f MB: %zu programs, %zu permutations (%u title), %zu pipelines\n",
                manifest->serial.c_str(), static_cast<unsigned long long>(id),
                double(bytes.size()) / 1e6, manifest->programs.size(),
                manifest->permutations.size(), title, manifest->pipelines.size());
    for (u32 stage = 0; stage < stages.size(); ++stage) {
        if (stages[stage]) {
            std::printf("  stage %u: %u permutations\n", stage, stages[stage]);
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc >= 3 && !std::strcmp(argv[1], "build")) {
        return Build(argc, argv);
    }
    if (argc == 3 && !std::strcmp(argv[1], "info")) {
        return Info(argv[2]);
    }
    std::fprintf(stderr, "usage: %s build --out <file> --serial <id> --game <folder> <cache>...\n"
                         "       %s info <file>\n",
                 argv[0], argv[0]);
    return 2;
}
