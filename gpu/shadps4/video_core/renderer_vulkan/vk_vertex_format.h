// SPDX-License-Identifier: GPL-2.0-or-later
// gow3: vertex attribute formats read from a V# that the guest may still be writing. The
// draw-preparation workers read user data before the draw is submitted, so a half-written V#
// can carry a pair the format table does not know (#35: "Unknown data_format=15 and
// num_format=5" on the static vertex-input path). VertexAttributeFormat reports it instead of
// asserting, as TrySurfaceFormat already does on the dynamic path (efb428d), and the GPU thread
// reads the inputs again once the draw is submitted.

#pragma once

#include <algorithm>
#include <optional>
#include <vector>

#include "video_core/amdgpu/pixel_format.h"
#include "video_core/renderer_vulkan/liverpool_to_vk.h"

namespace Vulkan::LiverpoolToVK {

/// eUndefined for FormatInvalid (a null buffer), the Vulkan format when the table knows the
/// pair, std::nullopt when it does not.
inline std::optional<vk::Format> VertexAttributeFormat(AmdGpu::DataFormat data_format,
                                                       AmdGpu::NumberFormat num_format) {
    const vk::Format format = TrySurfaceFormat(data_format, num_format);
    if (format == vk::Format::eUndefined && data_format != AmdGpu::DataFormat::FormatInvalid) {
        return std::nullopt;
    }
    return format;
}

/// The vertex formats seen so far; Add is true the first time a format shows up (never for
/// eUndefined).
struct VertexFormatSeen {
    std::vector<vk::Format> seen;

    bool Add(vk::Format format) {
        if (format == vk::Format::eUndefined ||
            std::find(seen.begin(), seen.end(), format) != seen.end()) {
            return false;
        }
        seen.push_back(format);
        return true;
    }
};

} // namespace Vulkan::LiverpoolToVK
