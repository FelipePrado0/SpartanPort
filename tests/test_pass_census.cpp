// SPDX-License-Identifier: GPL-2.0-or-later
// One state line per render pass in the frame analyzer's capture (vk_pass_census.h): guest and
// host sample counts, the sample path the pipeline cache chose, stencil use, the depth clear
// source, the depth layout, the feedback loop and the render area. No game or Vulkan device.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "video_core/renderer_vulkan/vk_pass_census.h"

using namespace Vulkan;

int main() {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    // The sample path mirrors vk_pipeline_cache.cpp: colour targets with more than one sample
    // count need VK_AMD_mixed_attachment_samples, colour != depth needs it or the NV extension,
    // and without either the colour samples are forced to the depth samples (Intel).
    using PassCensus::SamplePath;
    assert(PassCensus::ResolveSamplePath({1}, 1, true, true, true) == SamplePath::Equal);
    assert(PassCensus::ResolveSamplePath({1}, 4, true, true, true) == SamplePath::MixedAmd);
    assert(PassCensus::ResolveSamplePath({1}, 4, true, false, true) == SamplePath::MixedNv);
    assert(PassCensus::ResolveSamplePath({1}, 4, true, false, false) == SamplePath::ForcedEqual);
    assert(PassCensus::ResolveSamplePath({4}, 4, true, false, false) == SamplePath::Equal);
    // Colour counts that differ among the targets.
    assert(PassCensus::ResolveSamplePath({1, 4}, 4, true, true, true) == SamplePath::MixedAmd);
    assert(PassCensus::ResolveSamplePath({1, 4}, 4, true, false, true) == SamplePath::ForcedEqual);
    // No depth target: the key uses one depth sample, nothing is forced.
    assert(PassCensus::ResolveSamplePath({4}, 4, false, false, false) == SamplePath::Equal);
    // No colour target at all (depth-only pass).
    assert(PassCensus::ResolveSamplePath({}, 2, true, false, false) == SamplePath::Equal);

    assert(std::string(PassCensus::Name(SamplePath::Equal)) == "equal");
    assert(std::string(PassCensus::Name(SamplePath::MixedAmd)) == "mixed-amd");
    assert(std::string(PassCensus::Name(SamplePath::MixedNv)) == "mixed-nv");
    assert(std::string(PassCensus::Name(SamplePath::ForcedEqual)) == "forced-equal");

    // A lighting pass as AMD Windows would run it: 1x colour on 4x depth, sample shading on,
    // stencil tested and written but never cleared, depth cleared through HTILE.
    PassCensus::State amd{};
    amd.color_samples = {1};
    amd.depth_samples = 4;
    amd.raster_samples = 4;
    amd.path = SamplePath::MixedAmd;
    amd.sample_shading = true;
    amd.stencil_test = true;
    amd.stencil_write = true;
    amd.stencil_clear = false;
    amd.depth_clear = PassCensus::DepthClear::Htile;
    amd.depth_layout = "DepthReadOnlyStencilAttachmentOptimal";
    amd.feedback_loop = false;
    amd.area = {1920, 1080};
    amd.depth_size = {1920, 1088};
    assert(PassCensus::Format(amd) ==
           "state samples c1 d4 r4 mixed-amd shading 1 | stencil t1 w1 c0 | zclear htile"
           " | ds DepthReadOnlyStencilAttachmentOptimal | fb 0 | area 1920x1080 (depth 1920x1088)");

    // A plain 1x pass without a depth target.
    PassCensus::State flat{};
    flat.color_samples = {1, 1};
    flat.raster_samples = 1;
    flat.path = SamplePath::Equal;
    flat.area = {1280, 720};
    assert(PassCensus::Format(flat) ==
           "state samples c1,1 d0 r1 equal shading 0 | stencil t0 w0 c0 | zclear none"
           " | ds - | fb 0 | area 1280x720");

    // Intel-like forced path with a register depth clear and a feedback loop; the depth size
    // is only printed when it differs from the render area.
    PassCensus::State intel{};
    intel.color_samples = {1};
    intel.depth_samples = 2;
    intel.raster_samples = 2;
    intel.path = SamplePath::ForcedEqual;
    intel.stencil_clear = true;
    intel.depth_clear = PassCensus::DepthClear::Register;
    intel.depth_layout = "DepthStencilAttachmentOptimal";
    intel.feedback_loop = true;
    intel.area = {3840, 2160};
    intel.depth_size = {3840, 2160};
    assert(PassCensus::Format(intel) ==
           "state samples c1 d2 r2 forced-equal shading 0 | stencil t0 w0 c1 | zclear register"
           " | ds DepthStencilAttachmentOptimal | fb 1 | area 3840x2160");

    std::puts("pass census OK");
    return EXIT_SUCCESS;
}
