// SPDX-License-Identifier: GPL-2.0-or-later
// Usage: dump-index-test [game folder [index output]]. With a folder it indexes the real game files
// (read only) and can write the "hash offset size path" list the replay test takes.
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>

#include <xxhash.h>

#include "shader_recompiler/dump_index.h"
#include "video_core/amdgpu/regs_shader.h"

#undef NDEBUG
#include <cassert>

namespace fs = std::filesystem;

// Made-up bytes, never game code.
static std::vector<u8> MakeShader(u64 hash, u32 words, bool token = true) {
    std::vector<u32> code(words, 0x12345678);
    code[0] = token ? 0xBEEB03FF : 0x11111111;
    code[1] = words / 2 - 1;
    AmdGpu::BinaryInfo footer{};
    std::memcpy(&footer, "OrbShdr", 7);
    footer.length = words * 4;
    footer.shader_hash = hash;
    assert(footer.Valid());
    std::vector<u8> bytes(words * 4 + sizeof(footer));
    std::memcpy(bytes.data(), code.data(), words * 4);
    std::memcpy(bytes.data() + words * 4, &footer, sizeof(footer));
    return bytes;
}

static void Put(std::vector<u8>& file, const std::vector<u8>& bytes) {
    file.insert(file.end(), bytes.begin(), bytes.end());
}

static void Write(const fs::path& path, const std::vector<u8>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
}

static void Synthetic() {
    const auto dir = fs::temp_directory_path() / "spartanport-dump-index-test";
    fs::remove_all(dir);
    std::vector<u8> wad(128, 0xAA);
    const u64 first_offset = wad.size();
    Put(wad, MakeShader(0x1111, 16));
    Put(wad, MakeShader(0x2222, 8, false)); // no token: left out, not guessed
    wad.push_back(0); // real WADs keep shaders at odd file offsets
    Put(wad, MakeShader(0x3333, 32));
    Write(dir / "EXEC/WAD/PS4_LE/a.wad", wad);

    // Crosses the read block.
    std::vector<u8> big(16 * 1024 * 1024 + 60 * 1024, 0);
    big.resize(big.size() + 4096 - 256);
    const u64 cross_offset = big.size();
    Put(big, MakeShader(0x4444, 256));
    Write(dir / "EXEC/DLC/x/WAD/PS4_LE/b.WAD", big);

    Write(dir / "eboot.bin", MakeShader(0x5555, 8));
    Write(dir / "EXEC/note.txt", MakeShader(0x6666, 8));

    const auto index = Shader::IndexDump(dir);
    assert(index.size() == 4);
    assert(index.at(0x3333).offset % 4 != 0);
    assert(!index.contains(0x2222));
    assert(!index.contains(0x6666));
    assert(index.at(0x1111).offset == first_offset && index.at(0x1111).size == 64);
    assert(index.at(0x4444).offset == cross_offset && index.at(0x4444).size == 1024);
    assert(index.at(0x5555).file == "eboot.bin");

    const auto& loc = index.at(0x1111);
    std::vector<u32> code(16, 0x12345678);
    code[0] = 0xBEEB03FF;
    code[1] = 7;
    const u64 hash = XXH3_64bits(code.data(), 64);
    assert(Shader::ReadShaderCode(dir, loc, hash) == code);
    assert(!Shader::ReadShaderCode(dir, loc, hash + 1)); // other hash: refused

    auto wrong = loc;
    wrong.offset += 4;
    assert(!Shader::ReadShaderCode(dir, wrong, hash)); // wrong offset
    wrong = loc;
    wrong.file = "EXEC/WAD/PS4_LE/missing.wad";
    assert(!Shader::ReadShaderCode(dir, wrong, hash)); // missing file
    wrong = loc;
    wrong.file = "../outside.wad";
    assert(!Shader::ReadShaderCode(dir, wrong, hash)); // leaves the folder
    wrong = loc;
    wrong.size = 62;
    assert(!Shader::ReadShaderCode(dir, wrong, hash)); // not whole words
    fs::remove_all(dir);
}

int main(int argc, char** argv) {
    Synthetic();
    std::printf("PASS: dump index\n");
    if (argc > 1) {
        const auto start = std::chrono::steady_clock::now();
        const auto index = Shader::IndexDump(argv[1]);
        std::printf("%zu shaders in %.1f s\n", index.size(),
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
        if (argc > 2) {
            std::ofstream out(argv[2]);
            for (const auto& [hash, loc] : index) {
                out << std::hex << hash << std::dec << ' ' << loc.offset << ' ' << loc.size << ' '
                    << (fs::path(argv[1]) / loc.file).string() << '\n';
            }
        }
    }
    return 0;
}
