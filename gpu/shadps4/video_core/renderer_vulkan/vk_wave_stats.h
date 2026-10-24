// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: histogram of the subgroup widths the driver compiled pipeline stages at, from
// VK_KHR_pipeline_executable_properties (SPARTAN_WAVE_STATS=1; Pipeline::RecordWaveSizes in
// vk_pipeline_common.cpp). The PS4 shaders assume 64 lanes; a fragment stage compiled at 32
// covers 8x4 pixels per wave.

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>

namespace Vulkan {

/// The one stage bit VK_AMD_shader_info is queried with for an executable of `stages`
/// (VkShaderStageFlags): the last stage of the graphics pipeline that is set, or compute. A
/// merged executable such as { Vertex | TessellationControl } answers the same for either of
/// its bits. 0 when no known bit is set (task, mesh, ray tracing).
inline std::uint32_t AmdShaderInfoStageBit(std::uint32_t stages) {
    constexpr std::uint32_t kCompute = 0x20, kFragment = 0x10, kGeometry = 0x08,
                            kTessEval = 0x04, kTessControl = 0x02, kVertex = 0x01;
    for (const std::uint32_t bit :
         {kCompute, kFragment, kGeometry, kTessEval, kTessControl, kVertex}) {
        if (stages & bit) {
            return bit;
        }
    }
    return 0;
}

/// "<stages>_vgpr<V>_<index>_<width>.txt": the ISA dump of the `index`-th executable of the
/// key (stages plus physical VGPR count, "NA" without statistics), so that two keys never share
/// a file. `stages` may be the histogram key itself, whose "/vgprN" suffix is dropped before the
/// count is appended once. The braces and spaces of vk::to_string are dropped; bars become '_'.
inline std::string WaveDumpFileName(std::string stages, std::optional<std::uint32_t> vgprs,
                                    std::uint32_t index, std::uint32_t width) {
    if (const auto slash = stages.find("/vgpr"); slash != std::string::npos) {
        stages.erase(slash);
    }
    std::string name;
    for (const char c : stages) {
        if (c == '{' || c == '}' || c == ' ') {
            continue;
        }
        name += (c == '|' || c == '/') ? '_' : c;
    }
    name += "_vgpr" + (vgprs ? std::to_string(*vgprs) : std::string{"NA"});
    name += "_" + std::to_string(index) + "_" + std::to_string(width) + ".txt";
    return name;
}

class WaveSizeHistogram {
public:
    /// Counts one executable of `stages` (as vk::to_string prints the flags) compiled at
    /// `subgroup_size` lanes. Returns true the first time this pair is seen.
    bool Add(std::string stages, std::uint32_t subgroup_size) {
        auto& count = counts[{std::move(stages), subgroup_size}];
        ++count;
        return count == 1;
    }

    /// Closes one pipeline. Returns true when a report is due: on news, and every 256
    /// pipelines regardless.
    bool EndPipeline(bool news) {
        ++pipelines;
        return news || pipelines % 256 == 0;
    }

    std::uint32_t Pipelines() const {
        return pipelines;
    }

    /// Whether the ISA of one more executable of `stages` should be written out: the first
    /// `limit` of each stage set are.
    bool ShouldDump(const std::string& stages, std::uint32_t limit) {
        auto& dumped = dumps[stages];
        if (dumped >= limit) {
            return false;
        }
        ++dumped;
        return true;
    }

    std::uint32_t DumpsOf(const std::string& stages) const {
        const auto it = dumps.find(stages);
        return it == dumps.end() ? 0u : it->second;
    }

    /// "{ Compute }@64 x1, { Fragment }@32 x3", sorted by stages then width.
    std::string Summary() const {
        std::string out;
        for (const auto& [key, count] : counts) {
            if (!out.empty()) {
                out += ", ";
            }
            out += key.first + "@" + std::to_string(key.second) + " x" + std::to_string(count);
        }
        return out;
    }

private:
    std::map<std::pair<std::string, std::uint32_t>, std::uint32_t> counts;
    std::map<std::string, std::uint32_t> dumps;
    std::uint32_t pipelines = 0;
};

} // namespace Vulkan
