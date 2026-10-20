// SPDX-License-Identifier: GPL-2.0-or-later
// SPARTAN_NO_MSAA (video_core/amdgpu/spartanport_msaa.h): guest multisampled images created and sampled
// as single-sampled ones on AMD's and Intel's Windows drivers (spartanport_msaa_default.h), while
// every size in guest memory keeps the real sample count. No game or Vulkan device required.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include "common/logging/log.h"
#include "video_core/amdgpu/spartanport_msaa.h"
#include "video_core/amdgpu/spartanport_msaa_default.h"
#include "video_core/amdgpu/regs_color.h"
#include "video_core/amdgpu/regs_depth.h"
#include "video_core/amdgpu/regs_primitive.h"
#include "video_core/amdgpu/resource.h"

// The register accessors assert and log through the port's logging; no logger is linked here.
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

using namespace AmdGpu;

int main() {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    // A 2-sample T# (Color2DMsaa keeps the sample count in last_level).
    Image msaa{};
    msaa.type = static_cast<u64>(ImageType::Color2DMsaa);
    msaa.last_level = 1;
    Image flat{};
    flat.type = static_cast<u64>(ImageType::Color2D);
    flat.last_level = 3;
    ColorBuffer color{};
    color.attrib.num_fragments_log2 = 1;
    color.info.format = static_cast<u32>(DataFormat::Format8_8_8_8);
    color.slice.tile_max = 32399; // 1920x1080 / 64 - 1
    DepthBuffer depth{};
    depth.z_info.format = DepthBuffer::ZFormat::Z32Float;
    depth.z_info.num_samples = 1;
    depth.depth_slice.tile_max = 32399;
    AaConfig aa{};
    aa.msaa_num_samples = 1;

    no_msaa.store(false);
    assert(!NoMsaa());
    assert(msaa.GuestNumSamples() == 2 && msaa.NumSamples() == 2);
    assert(msaa.GetType() == ImageType::Color2DMsaa && msaa.NumLevels() == 1);
    assert(flat.NumSamples() == 1 && flat.GetType() == ImageType::Color2D && flat.NumLevels() == 4);
    assert(color.GuestNumSamples() == 2 && color.NumSamples() == 2);
    assert(depth.GuestNumSamples() == 2 && depth.NumSamples() == 2);
    assert(aa.NumSamples() == 2);
    const u32 color_slice = color.GetColorSliceSize();
    const u32 depth_slice = depth.GetDepthSliceSize();
    assert(color_slice == 1920u * 1080u * 4u * 2u);
    assert(depth_slice == 1920u * 1080u * 4u * 2u);

    no_msaa.store(true);
    assert(NoMsaa());
    // The host image and the shader see one sample; the guest counts stay.
    assert(msaa.GuestNumSamples() == 2 && msaa.NumSamples() == 1);
    assert(msaa.GetType() == ImageType::Color2D && msaa.NumLevels() == 1);
    msaa.type = static_cast<u64>(ImageType::Color2DMsaaArray);
    assert(msaa.GetType() == ImageType::Color2DArray && msaa.IsGuestMsaa());
    assert(flat.NumSamples() == 1 && flat.GetType() == ImageType::Color2D && flat.NumLevels() == 4);
    assert(!flat.IsGuestMsaa());
    assert(color.GuestNumSamples() == 2 && color.NumSamples() == 1);
    assert(depth.GuestNumSamples() == 2 && depth.NumSamples() == 1);
    assert(aa.NumSamples() == 1);
    // Sizes in guest memory are unchanged.
    assert(color.GetColorSliceSize() == color_slice);
    assert(depth.GetDepthSliceSize() == depth_slice);
    no_msaa.store(false);

    // The default per Vulkan driver, shared by vk_instance.cpp and spartanport-gpu-capabilities --list:
    // off where the 2-sample depth fetch is wrong (#65 RX 6600, #28 Arc), on elsewhere.
    assert(spartanport_no_msaa_default(VK_DRIVER_ID_AMD_PROPRIETARY));
    assert(spartanport_no_msaa_default(VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS));
    for (const VkDriverId keeps_msaa :
         {VK_DRIVER_ID_NVIDIA_PROPRIETARY, VK_DRIVER_ID_MESA_RADV, VK_DRIVER_ID_AMD_OPEN_SOURCE,
          VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA, VK_DRIVER_ID_MOLTENVK, VK_DRIVER_ID_MESA_LLVMPIPE,
          static_cast<VkDriverId>(0)}) {
        assert(!spartanport_no_msaa_default(keeps_msaa));
    }

    std::puts("no msaa OK");
    return EXIT_SUCCESS;
}
