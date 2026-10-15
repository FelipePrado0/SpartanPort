// SPDX-License-Identifier: GPL-2.0-or-later
// gow3: vertex attribute formats read from a V# that the guest may still be writing (#35):
// VertexAttributeFormat tolerates unknown pairs and VertexFormatSeen records each format once.
// No game or Vulkan device required.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include "common/logging/log.h"
#include "video_core/renderer_vulkan/vk_vertex_format.h"

// The format tables assert and log through the port's logging; no logger is linked here.
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

using namespace Vulkan::LiverpoolToVK;
using AmdGpu::DataFormat;
using AmdGpu::NumberFormat;

int main() {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    assert(VertexAttributeFormat(DataFormat::FormatInvalid, NumberFormat::Unorm) ==
           vk::Format::eUndefined);
    assert(VertexAttributeFormat(DataFormat::Format8_8_8_8, NumberFormat::Unorm) ==
           vk::Format::eR8G8B8A8Unorm);
    assert(VertexAttributeFormat(DataFormat::Format32_32_32, NumberFormat::Float) ==
           vk::Format::eR32G32B32Sfloat);
    // #35, 480p: "SurfaceFormat: Unknown data_format=15 and num_format=5".
    assert(!VertexAttributeFormat(static_cast<DataFormat>(15), NumberFormat::Sint).has_value());
    VertexFormatSeen seen;
    assert(seen.Add(vk::Format::eB10G11R11UfloatPack32));
    assert(!seen.Add(vk::Format::eB10G11R11UfloatPack32));
    assert(seen.Add(vk::Format::eR8G8B8A8Unorm));
    assert(!seen.Add(vk::Format::eUndefined));
    assert(seen.seen.size() == 2);
    std::puts("vertex-format-test: ok");
}
