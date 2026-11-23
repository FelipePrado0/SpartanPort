// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "../../../third_party/fsr-vulkan/upstream/ffx-2.3.0/Kits/FidelityFX/upscalers/include/ffx_upscale.h"
#include "video_core/renderer_vulkan/fsr_driver.h"
#include <string_view>

namespace Vulkan::FsrDriver {

// Driver ABI used by OptiScaler's FSR4DriverProviderVk; no SDK loader or shader
// replay.
struct Provider {
    uint64_t version_id;
    const char* version_name;
    bool (*can_provide)(uint64_t);
    PfnFfxCreateContext create;
    PfnFfxDestroyContext destroy;
    PfnFfxConfigure configure;
    PfnFfxQuery query;
    PfnFfxDispatch dispatch;
};
struct ProviderData {
    uint32_t version = 3;
    uint64_t effect = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
    Provider provider{};
    bool (*supported)(const void*) = nullptr;
    void* reserved = nullptr;
};
static_assert(offsetof(ProviderData, supported) == 0x50);
static_assert(sizeof(ProviderData) == 0x60);

bool IsFsr41(const char* name);
std::string ActiveVersion(const Provider& provider, ffxContext& context);
ffxDispatchDescUpscale MakeDispatch(const Fsr4Upscaler::Frame& frame);
std::string InstalledVersion();
const char* ModelName(std::string_view type_name);

} // namespace Vulkan::FsrDriver
