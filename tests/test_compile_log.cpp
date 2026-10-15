// SPDX-License-Identifier: GPL-2.0-or-later
// The "Compiling graphics pipeline" line (vk_compile_log.h): a crash inside the driver's shader
// compiler (igc64.dll on Intel, #35) must leave the pipeline and its shader hashes as the last
// lines of last_run.log, also when a compile worker builds it or the preload / background
// warm-up rebuilds it from the disk cache. No game, Vulkan device or window required.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <span>
#include <vector>

#include "video_core/renderer_vulkan/vk_compile_log.h"

using namespace Vulkan;

int main() {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    const std::vector<CompileLogStage> stages{{"vs", 0x1234}, {"fs", 0xabcdef0123456789}};
    // Synchronous compile: the pipeline hash, then every stage in the order given.
    assert(FormatPipelineCompile(0x9f, stages, "") == "0x9f (vs 0x1234, fs 0xabcdef0123456789)");
    // A compile worker marks the line, so the log tells the worker from the GPU thread.
    assert(FormatPipelineCompile(0x9f, stages, "async") ==
           "0x9f (async; vs 0x1234, fs 0xabcdef0123456789)");
    assert(FormatPipelineCompile(0x9f, std::span{stages}.first(1), "async") ==
           "0x9f (async; vs 0x1234)");
    // A pipeline rebuilt from the disk cache (preload, background warm-up) says so.
    assert(FormatPipelineCompile(0x9f, stages, "cache") ==
           "0x9f (cache; vs 0x1234, fs 0xabcdef0123456789)");
    // No stages: only the pipeline hash and the marker.
    assert(FormatPipelineCompile(0x9f, {}, "") == "0x9f");
    assert(FormatPipelineCompile(0x9f, {}, "async") == "0x9f (async)");
    assert(FormatPipelineCompile(0x9f, {}, "cache") == "0x9f (cache)");
    // A pipeline with a fragment shader passes no inherited state: the text above is unchanged.
    assert(FormatPipelineCompile(0x9f, stages, "async", std::nullopt) ==
           "0x9f (async; vs 0x1234, fs 0xabcdef0123456789)");

    // A pipeline without a fragment shader is built with the fragment state of the pixel shader
    // bound at an earlier draw (or with what the disk cache stored): sample shading and, for a
    // RectList / QuadList, the auxiliary tessellation shaders declaring its inputs. The line
    // says what reached the driver (#35: igc64.dll crashed on such a pipeline).
    const std::vector<CompileLogStage> vs_only{{"vs", 0xccdebd802401e5ac}};
    assert(FormatPipelineCompile(0x4a3437eeeac9ff2, vs_only, "",
                                 NoFragmentShaderState{.sample_shading = false,
                                                       .aux_tess = "RectList",
                                                       .aux_tess_inputs = 3}) ==
           "0x4a3437eeeac9ff2 (vs 0xccdebd802401e5ac; no fs: RectList, sample shading off, aux "
           "tess 3 inputs)");
    assert(FormatPipelineCompile(0x9f, vs_only, "async",
                                 NoFragmentShaderState{.sample_shading = true,
                                                       .aux_tess = "QuadList",
                                                       .aux_tess_inputs = 1}) ==
           "0x9f (async; vs 0xccdebd802401e5ac; no fs: QuadList, sample shading on, aux tess 1 "
           "input)");
    assert(FormatPipelineCompile(0x9f, vs_only, "",
                                 NoFragmentShaderState{.aux_tess = "RectList",
                                                       .aux_tess_inputs = 0}) ==
           "0x9f (vs 0xccdebd802401e5ac; no fs: RectList, sample shading off, aux tess 0 inputs)");
    // Other primitives get no auxiliary shaders: only the sample shading is inherited.
    assert(FormatPipelineCompile(0x9f, vs_only, "",
                                 NoFragmentShaderState{.sample_shading = true}) ==
           "0x9f (vs 0xccdebd802401e5ac; no fs: sample shading on)");
    // The disk cache stores the auxiliary shaders' SPIR-V, not their input count: none printed.
    assert(FormatPipelineCompile(0x9f, vs_only, "cache",
                                 NoFragmentShaderState{.aux_tess = "RectList"}) ==
           "0x9f (cache; vs 0xccdebd802401e5ac; no fs: RectList, sample shading off, aux tess)");
    // No stages at all: the marker (if any), then the inherited state.
    assert(FormatPipelineCompile(0x9f, {}, "", NoFragmentShaderState{}) ==
           "0x9f (no fs: sample shading off)");
    assert(FormatPipelineCompile(0x9f, {}, "cache", NoFragmentShaderState{}) ==
           "0x9f (cache; no fs: sample shading off)");

    // The "Compiling <stage> shader" line before vkCreateShaderModule: the driver compiles at
    // pipeline creation, so the one line per shader the preload and warm-up rebuild from the disk
    // cache is Debug. The GPU thread's and the compile workers' lines stay at Info.
    assert(!LogsShaderModuleCompileAtInfo("cache"));
    assert(LogsShaderModuleCompileAtInfo("async"));
    assert(LogsShaderModuleCompileAtInfo(""));
    std::puts("compile-log-test: ok");
    return EXIT_SUCCESS;
}
