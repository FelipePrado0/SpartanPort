// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_vulkan/fsr_driver_api.h"
#include <cassert>
#include <cstdio>

namespace {
const char* reported_version = "3.1.5";
ffxReturnCode_t QueryVersion(ffxContext*, ffxQueryDescHeader* header) {
    auto& query = *reinterpret_cast<ffxQueryGetProviderVersion*>(header);
    query.versionId = 42;
    query.versionName = reported_version;
    return FFX_API_RETURN_OK;
}
} // namespace

int main(int argc, char** argv) {
    using namespace Vulkan::FsrDriver;
    if (argc == 2 && std::string_view(argv[1]) == "--probe") {
        vk::detail::DynamicLoader loader;
        const auto get_proc =
            loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        if (!get_proc)
            return 3;
        const auto create =
            reinterpret_cast<PFN_vkCreateInstance>(get_proc(nullptr, "vkCreateInstance"));
        if (!create)
            return 3;
        const VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                            nullptr,
                                            "SpartanPort FSR probe",
                                            1,
                                            nullptr,
                                            0,
                                            VK_API_VERSION_1_3};
        const VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                        nullptr,
                                        0,
                                        &application,
                                        0,
                                        nullptr,
                                        0,
                                        nullptr};
        VkInstance instance = VK_NULL_HANDLE;
        if (create(&info, nullptr, &instance) != VK_SUCCESS)
            return 3;
        const auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
            get_proc(instance, "vkEnumeratePhysicalDevices"));
        uint32_t count = 0;
        if (enumerate)
            enumerate(instance, &count, nullptr);
        const auto version = InstalledVersion();
        std::printf("AMD Vulkan FSR provider: %s\n",
                    version.empty() ? "missing amdvkffx64.dll in the executable or active driver folder"
                                    : version.c_str());
        const auto destroy =
            reinterpret_cast<PFN_vkDestroyInstance>(get_proc(instance, "vkDestroyInstance"));
        if (destroy)
            destroy(instance, nullptr);
        return version.empty() ? 2 : 0;
    }
    assert(IsFsr41("4.1.2"));
    assert(IsFsr41("4.1.1 *"));
    assert(std::string_view(ModelName(".?AVffxProvider_FSR4_Int8_Navi2@@")) == "INT8 RDNA2");
    assert(std::string_view(ModelName(".?AVffxProvider_FSR4_Int8@@")) == "INT8");
    assert(std::string_view(ModelName(".?AVffxProvider_FSR4@@")) == "FP8");
    assert(!ModelName(".?AVffxProvider_FSR3@@"));
    for (const char* version :
         std::initializer_list<const char*>{nullptr, "", "3.1.5", "4.0.3", "14.1.2", "4.10.0"}) {
        assert(!IsFsr41(version));
    }
    Provider provider{};
    provider.query = QueryVersion;
    ffxContext context = reinterpret_cast<void*>(1);
    assert(ActiveVersion(provider, context).empty());
    reported_version = "4.1.2";
    assert(ActiveVersion(provider, context) == "4.1.2");
    provider.query = nullptr;
    assert(ActiveVersion(provider, context).empty());

    Vulkan::Fsr4Upscaler::Frame frame{};
    frame.render_width = 1280;
    frame.render_height = 720;
    frame.color = {{}, {}, 1280, 720};
    frame.depth = {{}, {}, 1280, 720};
    frame.motion = {{}, {}, 1280, 720};
    frame.output = {{}, {}, 2560, 1440};
    frame.color_format = vk::Format::eR8G8B8A8Unorm;
    frame.jitter = {0.25f, -0.125f};
    frame.reset = true;
    const auto dispatch = MakeDispatch(frame);
    assert(dispatch.renderSize.width == 1280 && dispatch.upscaleSize.width == 2560);
    assert(dispatch.color.description.format == FFX_API_SURFACE_FORMAT_R8G8B8A8_UNORM);
    assert(dispatch.depth.description.usage == FFX_API_RESOURCE_USAGE_DEPTHTARGET);
    assert(dispatch.motionVectors.description.format == FFX_API_SURFACE_FORMAT_R16G16_FLOAT);
    assert(dispatch.output.state == FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
    assert(dispatch.jitterOffset.x == 0.25f && dispatch.jitterOffset.y == -0.125f);
    assert(dispatch.flags == FFX_UPSCALE_FLAG_NON_LINEAR_COLOR_SRGB && dispatch.reset);
    // An sRGB image is read through an sRGB view: linear in, linear out, so the blit into the
    // sRGB UI image encodes it once (UNORM + the non-linear flag would encode it twice).
    frame.color_format = vk::Format::eR8G8B8A8Srgb;
    const auto srgb = MakeDispatch(frame);
    assert(srgb.color.description.format == FFX_API_SURFACE_FORMAT_R8G8B8A8_SRGB);
    assert(srgb.flags == 0);
    frame.hdr = true;
    frame.color_format = vk::Format::eR16G16B16A16Sfloat;
    assert(MakeDispatch(frame).flags == 0);
    std::puts("FSR driver: actual provider version and Vulkan dispatch PASS");
}
