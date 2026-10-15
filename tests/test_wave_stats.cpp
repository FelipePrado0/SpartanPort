// SPDX-License-Identifier: GPL-2.0-or-later
// Histogram of the subgroup widths the driver compiled pipeline stages at (vk_wave_stats.h).
// No game, Vulkan device or window required.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <optional>

#include "video_core/renderer_vulkan/vk_wave_stats.h"

using namespace Vulkan;

int main() {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    WaveSizeHistogram histogram;
    assert(histogram.Summary() == "");

    // Add reports whether the (stages, width) pair is new: the caller prints on news.
    assert(histogram.Add("{ Fragment }", 64));
    assert(!histogram.Add("{ Fragment }", 64));
    assert(histogram.Add("{ Fragment }", 32));
    assert(histogram.Add("{ Compute }", 64));
    assert(histogram.Add("{ Vertex }", 64));
    assert(!histogram.Add("{ Vertex }", 64));

    // Sorted by stage then width, with the count of executables seen.
    assert(histogram.Summary() ==
           "{ Compute }@64 x1, { Fragment }@32 x1, { Fragment }@64 x2, { Vertex }@64 x2");

    // One pipeline per call; a report every 256 pipelines even without news.
    for (int i = 0; i < 255; ++i) {
        assert(!histogram.EndPipeline(false));
    }
    assert(histogram.EndPipeline(false));
    assert(histogram.Pipelines() == 256u);
    assert(histogram.EndPipeline(true));
    assert(histogram.Pipelines() == 257u);

    // Disassembly dumps: the first `limit` executables of each stage set, counted separately.
    WaveSizeHistogram dumps;
    assert(dumps.ShouldDump("{ Fragment }", 2));
    assert(dumps.ShouldDump("{ Fragment }", 2));
    assert(!dumps.ShouldDump("{ Fragment }", 2));
    assert(dumps.ShouldDump("{ Compute }", 1));
    assert(!dumps.ShouldDump("{ Compute }", 1));
    assert(dumps.DumpsOf("{ Fragment }") == 2u);
    assert(dumps.DumpsOf("{ Vertex }") == 0u);

    // VK_AMD_shader_info takes one stage bit: the last stage of the executable (a merged
    // executable such as { Vertex | TessellationControl } answers the same for either bit),
    // none when no known graphics or compute bit is set.
    assert(AmdShaderInfoStageBit(0x10u) == 0x10u);      // { Fragment }
    assert(AmdShaderInfoStageBit(0x1u) == 0x1u);        // { Vertex }
    assert(AmdShaderInfoStageBit(0x1u | 0x2u) == 0x2u); // { Vertex | TessellationControl }
    assert(AmdShaderInfoStageBit(0x4u | 0x8u) == 0x8u); // { TessellationEvaluation | Geometry }
    assert(AmdShaderInfoStageBit(0x8u) == 0x8u);        // { Geometry }
    assert(AmdShaderInfoStageBit(0x20u) == 0x20u);      // { Compute }
    assert(AmdShaderInfoStageBit(0x40u) == 0u);         // task, mesh, ray tracing: skipped
    assert(AmdShaderInfoStageBit(0u) == 0u);

    // Dump file names carry the stage set, the physical VGPR count (what separates wave32 from
    // wave64 on AMD), the index within that key and the reported width, so two keys never
    // overwrite each other; the braces, spaces, bars and slashes of the key are dropped.
    assert(WaveDumpFileName("{ Fragment }", 32u, 1, 0) == "Fragment_vgpr32_1_0.txt");
    assert(WaveDumpFileName("{ Vertex | TessellationControl }", 24u, 3, 0) ==
           "Vertex_TessellationControl_vgpr24_3_0.txt");
    assert(WaveDumpFileName("{ Compute }", std::nullopt, 2, 64) == "Compute_vgprNA_2_64.txt");
    // The caller may pass the histogram key, which already carries the VGPR suffix.
    assert(WaveDumpFileName("{ Fragment }/vgpr32", 32u, 1, 0) == "Fragment_vgpr32_1_0.txt");

    std::puts("wave-stats-test: ok");
    return 0;
}
