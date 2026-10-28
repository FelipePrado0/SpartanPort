// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_vulkan/vk_dlss_frame_generation.h"
#include "spartanport_settings.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#endif

namespace Vulkan {
DlssFrameGeneration& DlssFrameGeneration::Get() {
    static auto* fg = new DlssFrameGeneration;
    return *fg;
}
bool DlssFrameGeneration::Requested() const {
    return SpartanSettings::Get().startup_frame_generation == 2;
}
void DlssFrameGeneration::Fail(const char* reason) {
    if (!reason)
        reason = "NVIDIA frame generation operation failed";
    ready = false;
    SpartanSettings::Get().fg_dlss_supported = false;
    if (Requested()) {
        SpartanSettings::Get().fg_problem = reason;
        SpartanSettings::Get().fg_game_fps = 0.0f;
    }
    std::printf("DLSS FG: unavailable (%s)\n", reason);
}
bool DlssFrameGeneration::Prepare(vk::PhysicalDevice physical) {
    if (physical.getProperties().vendorID != 0x10de) {
        if (Requested())
            Fail("DLSS frame generation requires a supported NVIDIA RTX GPU");
        return false;
    }
#ifdef _WIN32
    wchar_t executable[32768]{};
    const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
    if (!length || length >= 32768) {
        Fail("could not locate the Streamline bridge");
        return false;
    }
    const auto directory = std::filesystem::path(executable).parent_path();
    const auto properties =
        physical.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceIDProperties>();
    const auto& identity = properties.get<vk::PhysicalDeviceIDProperties>();
    auto module = LoadLibraryExW((directory / L"spartanport_streamline.dll").c_str(), nullptr,
                                 LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    const auto get_api =
        module
            ? reinterpret_cast<SpartanSlGetApiFn>(GetProcAddress(module, "SpartanStreamlineGetApi"))
            : nullptr;
    api = get_api ? get_api() : nullptr;
    if (!api || api->abi != SPARTAN_STREAMLINE_ABI || api->size != sizeof(*api)) {
        Fail("Streamline 2.14.16 bridge is missing or incompatible");
        return false;
    }
    if (!api->Configure(directory.c_str()) ||
        !api->Supported(physical, identity.deviceLUIDValid ? identity.deviceLUID.data() : nullptr,
                        VK_LUID_SIZE)) {
        Fail(api->Problem());
        api->Shutdown();
        return false;
    }
    SpartanSettings::Get().fg_dlss_supported = true;
    if (!Requested()) {
        api->Shutdown();
        return false;
    }
    std::copy_n(properties.get<vk::PhysicalDeviceIDProperties>().deviceUUID.data(), VK_UUID_SIZE,
                adapter_uuid.begin());
    hooked = true;
    return true;
#else
    Fail("NVIDIA frame generation is available on Windows x64 only");
    return false;
#endif
}
PFN_vkGetInstanceProcAddr DlssFrameGeneration::Resolver() const {
    return hooked ? api->Resolver() : nullptr;
}
void DlssFrameGeneration::Initialize(vk::PhysicalDevice physical, vk::Queue graphics,
                                     vk::Queue present) {
    if (!hooked)
        return;
    if (graphics != present) {
        Fail("NVIDIA frame generation inputs must be written on the presenting queue");
        return;
    }
    const auto properties =
        physical.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceIDProperties>();
    const auto& identity = properties.get<vk::PhysicalDeviceIDProperties>();
    if (!std::equal(adapter_uuid.begin(), adapter_uuid.end(),
                    properties.get<vk::PhysicalDeviceIDProperties>().deviceUUID.begin())) {
        Fail("selected graphics adapter changed during NVIDIA initialization");
        return;
    }
    if (!api->Supported(physical, identity.deviceLUIDValid ? identity.deviceLUID.data() : nullptr,
                        VK_LUID_SIZE) ||
        !api->Initialize() || !api->GetState(&state)) {
        Fail(api->Problem());
        return;
    }
    ready = true;
    SpartanSettings::Get().fg_dlss_supported = true;
    SpartanSettings::Get().fg_problem = nullptr;
    std::puts("DLSS FG: Streamline 2.14.16 and Reflex ready, 2x only");
}
void* DlssFrameGeneration::BeginFrame() {
    if (!Ready()) return nullptr;
    void* token = api->BeginFrame(++next_id);
    if (!token) Fail(api->Problem());
    return token;
}
void DlssFrameGeneration::StartSimulation() {
    if (!Ready())
        return;
    if (submission_started)
        Marker(simulation_token, 3);
    simulation_token = BeginFrame();
    Marker(simulation_token, 0);
    submission_started = false;
}
void* DlssFrameGeneration::SubmissionToken() {
    if (!Ready())
        return nullptr;
    if (!simulation_token)
        StartSimulation();
    if (!submission_started) {
        Marker(simulation_token, 1);
        Marker(simulation_token, 2);
        submission_started = true;
    }
    return simulation_token;
}
void DlssFrameGeneration::Marker(void* token, u32 marker) {
    if (Ready() && token && !api->Marker(token, marker))
        Fail("NVIDIA Reflex marker failed");
}
bool DlssFrameGeneration::Inputs(void* token, vk::CommandBuffer cmd, const SpartanSlCamera& camera,
                                 const SpartanSlImage& depth, const SpartanSlImage& motion) {
    if (!Ready())
        return false;
    if (!api->Inputs(token, cmd, &camera, &depth, &motion)) {
        Fail(api->Problem());
        return false;
    }
    return true;
}
bool DlssFrameGeneration::Color(void* token, vk::CommandBuffer cmd, const SpartanSlImage& hudless,
                                const SpartanSlImage& alpha) {
    if (!Ready())
        return false;
    if (!api->Color(token, cmd, &hudless, &alpha)) {
        Fail(api->Problem());
        return false;
    }
    return true;
}
bool DlssFrameGeneration::Enable(bool value, vk::Extent2D extent) {
    if (!hooked || !Ready()) {
        Pause();
        return false;
    }
    value = value && Ready() && SpartanSettings::Get().frame_generation == 2;
    if (value && SpartanSettings::Get().vsync && !state.vsync_supported) {
        SpartanSettings::Get().fg_problem =
            "NVIDIA Vulkan frame generation does not support VSync on this driver";
        value = false;
    }
    if (value &&
        (extent.width < state.minimum_dimension || extent.height < state.minimum_dimension)) {
        SpartanSettings::Get().fg_problem =
            "the output resolution is below NVIDIA's minimum dimension";
        value = false;
    }
    if (!api->SetEnabled(value)) {
        Fail(api->Problem());
        return false;
    }
    last_enabled = value;
    if (value)
        SpartanSettings::Get().fg_problem = nullptr;
    return value;
}
void DlssFrameGeneration::Presented(bool game_frame) {
    if (!Ready())
        return;
    if (!api->GetState(&state)) {
        Fail(api->Problem());
        return;
    }
    if (state.status && last_enabled) {
        Pause();
        Fail("NVIDIA reported invalid frame generation inputs or runtime state");
        return;
    }
    if (!game_frame) {
        real_count = presented_count = 0;
        report_start = std::chrono::steady_clock::now();
        return;
    }
    real_count += game_frame;
    presented_count += state.presented;
    const auto now = std::chrono::steady_clock::now();
    const float seconds = std::chrono::duration<float>(now - report_start).count();
    if (seconds >= 1.0f) {
        auto& settings = SpartanSettings::Get();
        settings.fg_game_fps = presented_count > real_count ? real_count / seconds : 0.0f;
        settings.fg_presented_fps = presented_count / seconds;
        settings.fg_status = presented_count > real_count ? "NVIDIA DLSS FG 2x, active"
                                                          : "NVIDIA DLSS FG 2x, inactive";
        real_count = presented_count = 0;
        report_start = now;
    }
}
void DlssFrameGeneration::Pause() {
    if (hooked) {
        api->SetEnabled(0);
        SpartanSettings::Get().fg_game_fps = 0.0f;
    }
    last_enabled = false;
}
void DlssFrameGeneration::Shutdown() {
    if (hooked) {
        ready = false;
        api->Shutdown();
        hooked = false;
    }
}
} // namespace Vulkan
