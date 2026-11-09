// SPDX-License-Identifier: GPL-2.0-or-later
// A stale texture descriptor (one the game is still rewriting) carries a data/number format pair
// no real texture has. SurfaceFormat used to assert on it and the game stopped with exit 23; the
// pairs below are the ones Polaris players reported (#976, #1060, #1181, #1223). Textures with
// such a pair are rejected before SurfaceFormat (IsKnownFormat, vk_rasterizer.cpp); this covers
// the fallback other callers get, and the colour target check. No game or Vulkan device required.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "common/logging/log.h"
#include "video_core/amdgpu/pixel_format.h"
#include "video_core/amdgpu/regs_color.h"
#include "video_core/renderer_vulkan/liverpool_to_vk.h"

void assert_fail_impl() {
    std::abort();
}
void unreachable_impl() {
    std::abort();
}
namespace Common::Log {
bool ShouldLog(Level) {
    return false;
}
void Write(std::string_view, Level, const char*, int, const char*, std::string&&) {}
} // namespace Common::Log

using AmdGpu::DataFormat;
using AmdGpu::NumberFormat;
using Vulkan::LiverpoolToVK::SurfaceFormat;

// What every caller passes: the descriptor's raw data format after RemapDataFormat.
static vk::Format FromDescriptor(u32 data_format, u32 num_format) {
    return SurfaceFormat(AmdGpu::RemapDataFormat(static_cast<DataFormat>(data_format)),
                         static_cast<NumberFormat>(num_format));
}

int main() {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    // Real data formats with a number format they have no texture format for: the same data
    // format with an existing number format, so the element size stays.
    assert(FromDescriptor(9, 7) == vk::Format::eA2B10G10R10UnormPack32);  // #1223, #1106
    assert(FromDescriptor(10, 7) == vk::Format::eR8G8B8A8Unorm);          // #1060
    assert(FromDescriptor(6, 0) == vk::Format::eB10G11R11UfloatPack32);   // #976, #287
    assert(FromDescriptor(6, 4) == vk::Format::eB10G11R11UfloatPack32);   // #1060
    assert(FromDescriptor(6, 5) == vk::Format::eB10G11R11UfloatPack32);   // #1181, #1060
    assert(FromDescriptor(14, 1) == vk::Format::eR32G32B32A32Sfloat);     // 32_32_32_32 Snorm

    // Values the hardware leaves unused are recognised; real formats are not.
    for (const u32 unused : {15u, 23u, 24u, 31u, 42u, 43u}) {
        assert(AmdGpu::IsUnusedDataFormat(unused));
    }
    for (const u32 used : {0u, 9u, 10u, 14u, 16u, 22u, 32u, 41u, 44u, 63u}) {
        assert(!AmdGpu::IsUnusedDataFormat(used));
    }

    // Real pairs are unchanged.
    assert(FromDescriptor(10, 0) == vk::Format::eR8G8B8A8Unorm);
    assert(FromDescriptor(9, 0) == vk::Format::eA2B10G10R10UnormPack32);

    // A colour buffer with an unused format is no target; a real one still is.
    AmdGpu::ColorBuffer color{};
    color.base_address = 0x1000;
    color.info.format = 15;
    assert(!color);
    color.info.format = static_cast<u32>(DataFormat::Format8_8_8_8);
    assert(color);
    color.info.format = 0;
    assert(!color);

    std::puts("surface-format-test: OK");
    return 0;
}
