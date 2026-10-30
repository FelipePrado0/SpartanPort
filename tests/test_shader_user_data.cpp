// SPDX-License-Identifier: GPL-2.0-or-later
// Uses the production descriptor reader; no game files, Vulkan device or window needed.
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include "shader_recompiler/frontend/fetch_shader.h"

// Match async-fetch-snapshot-test: link the production fetch reader without a GPU runtime.
extern "C" {
u32 runtime_disabled_optimizations = 0;
}
void assert_fail_impl() {
    std::abort();
}
[[noreturn]] void unreachable_impl() {
    std::abort();
}
namespace Common::Log {
bool ShouldLog(Level) {
    return false;
}
void Write(std::string_view, Level, const char*, int, const char*, std::string&&) {}
} // namespace Common::Log

int main() {
    constexpr u32 pointer_reg = 2;
    constexpr u32 descriptor_offset = 4;
    std::array<u32, Shader::NUM_USER_DATA_REGS> registers{};
    std::array<u32, 8> table{};
    Shader::Info info{};
    info.user_data = registers;

    AmdGpu::Buffer expected{};
    expected.base_address = 0x123400;
    expected.stride = 16;
    expected.num_records = 12;
    std::memcpy(table.data() + descriptor_offset, &expected, sizeof(expected));

    const auto set_pointer = [&](u64 pointer) {
        std::memcpy(registers.data() + pointer_reg, &pointer, sizeof(pointer));
    };
    const auto read = [&] {
        return info.ReadUdReg<AmdGpu::Buffer>(pointer_reg, descriptor_offset);
    };

    // Regression: null table plus a nonzero descriptor offset must not read low memory.
    assert(read() == AmdGpu::Buffer{});
    assert(!read());
    // Pointer metadata is masked before deciding whether the table is null.
    set_pointer(0xabcd000000000000ULL);
    assert(read() == AmdGpu::Buffer{});

    assert(info.UdPointerNull(pointer_reg));
    // Low addresses stay empty after masking, but the first valid address is not null.
    for (const u64 low : {0x8ULL, 0xffffULL, 0xabcd00000000ffffULL}) {
        set_pointer(low);
        assert(info.UdPointerNull(pointer_reg));
        assert(read() == AmdGpu::Buffer{});
    }
    set_pointer(0x10000);
    assert(!info.UdPointerNull(pointer_reg));
    assert(!info.UdPointerNull(Shader::IR::NumScalarRegs));
    set_pointer(0);

    // The vertex consumer must still see an empty descriptor with load overrides applied.
    Shader::Gcn::VertexAttribute attribute{};
    attribute.sgpr_base = pointer_reg;
    attribute.dword_offset = descriptor_offset;
    attribute.inst_offset = 8;
    attribute.data_format = static_cast<u8>(AmdGpu::DataFormat::Format32);
    assert(!attribute.GetSharp(info));

    // Valid tables, masked pointer metadata and instruction offsets retain their behavior.
    set_pointer(reinterpret_cast<u64>(table.data()) | 0xabcd000000000000ULL);
    assert(read() == expected);
    assert(!info.UdPointerNull(pointer_reg));
    const auto vertex = attribute.GetSharp(info);
    assert(vertex.num_records == expected.num_records);
    assert(vertex.base_address == expected.base_address + attribute.inst_offset);
    assert(vertex.data_format == attribute.data_format);

    // The direct-register path bypasses the pointer indirection.
    std::memcpy(registers.data() + descriptor_offset, &expected, sizeof(expected));
    assert(info.ReadUdReg<AmdGpu::Buffer>(Shader::IR::NumScalarRegs, descriptor_offset) ==
           expected);

    // Direct reads must fit entirely, including their last dword.
    constexpr u32 last_descriptor = Shader::NUM_USER_DATA_REGS - sizeof(expected) / sizeof(u32);
    std::memcpy(registers.data() + last_descriptor, &expected, sizeof(expected));
    assert(info.ReadUdReg<AmdGpu::Buffer>(Shader::IR::NumScalarRegs, last_descriptor) == expected);
    for (const u32 offset : {last_descriptor + 1, Shader::NUM_USER_DATA_REGS,
                             Shader::NUM_USER_DATA_REGS + 1, std::numeric_limits<u32>::max()}) {
        assert(info.ReadUdReg<AmdGpu::Buffer>(Shader::IR::NumScalarRegs, offset) ==
               AmdGpu::Buffer{});
    }

    // Missing and truncated pointer pairs must be rejected by both consumers.
    for (const u32 index : {Shader::NUM_USER_DATA_REGS - 1, Shader::NUM_USER_DATA_REGS,
                            Shader::NUM_USER_DATA_REGS + 1, std::numeric_limits<u32>::max()}) {
        assert(info.UdPointerNull(index));
        assert(info.ReadUdReg<AmdGpu::Buffer>(index, 0) == AmdGpu::Buffer{});
    }
    // The final complete pointer pair remains usable.
    constexpr u32 last_pointer = Shader::NUM_USER_DATA_REGS - 2;
    const auto table_pointer = reinterpret_cast<u64>(table.data());
    std::memcpy(registers.data() + last_pointer, &table_pointer, sizeof(table_pointer));
    assert(!info.UdPointerNull(last_pointer));
    assert(info.ReadUdReg<AmdGpu::Buffer>(last_pointer, descriptor_offset) == expected);
    info.user_data = {};
    assert(info.UdPointerNull(pointer_reg));
    assert(read() == AmdGpu::Buffer{});
    assert(info.ReadUdReg<AmdGpu::Buffer>(Shader::IR::NumScalarRegs, 0) == AmdGpu::Buffer{});
    info.user_data = registers;

    // Draw snapshots take precedence over current user data, including a null table.
    std::array<u32, Shader::NUM_USER_DATA_REGS> snapshot_registers{};
    Shader::Info::ud_snapshots[0] = {
        &info, snapshot_registers.data(), static_cast<u32>(snapshot_registers.size()), nullptr, 0,
        0,
    };
    Shader::Info::num_ud_snapshots = 1;
    assert(read() == AmdGpu::Buffer{});
    assert(!attribute.GetSharp(info));
    assert(info.UdPointerNull(pointer_reg));
    const auto snapshot_pointer = reinterpret_cast<u64>(table.data());
    std::memcpy(snapshot_registers.data() + pointer_reg, &snapshot_pointer,
                sizeof(snapshot_pointer));
    assert(read() == expected);
    assert(!info.UdPointerNull(pointer_reg));
    Shader::Info::ud_snapshots[0].user_data_size = pointer_reg + 1;
    assert(info.UdPointerNull(pointer_reg));
    assert(read() == AmdGpu::Buffer{});
    assert(info.ReadUdReg<AmdGpu::Buffer>(Shader::IR::NumScalarRegs, 0) == AmdGpu::Buffer{});
    Shader::Info::num_ud_snapshots = 0;

    std::puts("Shader user data: null/short tables, vertex consumers, direct registers and "
              "snapshots passed");
}
