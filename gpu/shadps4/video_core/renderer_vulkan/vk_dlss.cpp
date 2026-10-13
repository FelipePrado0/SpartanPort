// SPDX-FileCopyrightText: Copyright 2026 IFreemz, gow3 contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string_view>

#include "video_core/renderer_vulkan/vk_dlss.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include "../../../dlss_bridge/gow3_dlss_bridge.h"

namespace Vulkan {

int Dlss::QualityForScale(float scale) {
    // DLAA only for equal sizes: any real upscale needs at least the Quality mode.
    return scale >= 2.9f ? 4 : scale >= 1.95f ? 3 : scale >= 1.65f ? 2 : scale > 1.01f ? 1 : 0;
}

namespace {
// The bridge (gpu/dlss_bridge) and NVIDIA's DLSS library, next to gow3-probe.
#ifdef _WIN32
using Module = HMODULE;
constexpr char BridgeName[] = "gow3_dlss.dll";

std::filesystem::path ExecutableDirectory() {
    std::wstring path(MAX_PATH, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), DWORD(path.size()));
    path.resize(length);
    return std::filesystem::path{path}.parent_path();
}

bool HasNgxLibrary(const std::filesystem::path& directory) {
    return std::filesystem::is_regular_file(directory / "nvngx_dlss.dll");
}

std::string NgxVersion(const std::filesystem::path&) {
    return {}; // the version is in the DLL's resources, not its name
}

Module OpenModule(const std::filesystem::path& path) {
    return LoadLibraryW(path.c_str());
}

void* Symbol(Module module, const char* name) {
    return reinterpret_cast<void*>(GetProcAddress(module, name));
}
#else
using Module = void*;
constexpr char BridgeName[] = "libgow3_dlss.so";

std::filesystem::path ExecutableDirectory() {
    std::error_code error;
    return std::filesystem::read_symlink("/proc/self/exe", error).parent_path();
}

// libnvidia-ngx-dlss.so.<version> (the SDK's lib/Linux_x86_64/rel; not dlssd/dlssg): the
// version, or empty without one.
std::string NgxVersion(const std::filesystem::path& directory) {
    constexpr std::string_view prefix = "libnvidia-ngx-dlss.so.";
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        const auto name = entry.path().filename().string();
        if (name.starts_with(prefix)) {
            return name.substr(prefix.size());
        }
    }
    return {};
}

bool HasNgxLibrary(const std::filesystem::path& directory) {
    return !NgxVersion(directory).empty();
}

Module OpenModule(const std::filesystem::path& path) {
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
}

void* Symbol(Module module, const char* name) {
    return dlsym(module, name);
}
#endif

// GOW3_DLSS_PRESET: the DLSS model. NVIDIA's "default" may change with driver updates; by
// default the newest model of the SDK is forced (M; K is the DLSS 4 transformer that NVIDIA
// recommends for DLAA/Quality, J a variant of it).
u32 PresetFromEnvironment() {
    const char* value = std::getenv("GOW3_DLSS_PRESET");
    const char letter = value && value[0] ? char(std::toupper(u8(value[0]))) : 'M';
    switch (letter) {
    case 'J': return 10;
    case 'K': return 11;
    case 'L': return 12;
    case 'M': return 13;
    default: return 0; // "default"/"0": the driver's choice
    }
}

char PresetLetter(u32 preset) {
    return preset >= 10 && preset <= 13 ? char('J' + (preset - 10)) : '\0';
}

void BridgeLog(int warning, const char* message) {
    std::printf("DLSS: %s%s\n", warning ? "warning: " : "", message);
}

bool CollectExtensions(const VkExtensionProperties* required, u32 count,
                       const std::vector<vk::ExtensionProperties>& available,
                       std::vector<std::string>& storage, std::vector<const char*>& enabled) {
    storage.clear();
    for (u32 i = 0; i < count; ++i) {
        const auto& request = required[i];
        const auto match = std::ranges::find_if(available, [&](const auto& extension) {
            return std::strcmp(extension.extensionName.data(), request.extensionName) == 0 &&
                   extension.specVersion >= request.specVersion;
        });
        if (match == available.end()) {
            std::printf("DLSS: missing Vulkan extension %s\n", request.extensionName);
            return false;
        }
        storage.emplace_back(request.extensionName);
    }
    for (const auto& name : storage) {
        if (std::ranges::none_of(enabled, [&](const char* current) { return name == current; })) {
            enabled.push_back(name.c_str());
        }
    }
    return true;
}
} // namespace

struct Dlss::Impl {
    Module module{};
    std::string version; ///< NVIDIA's library, e.g. 310.9.1
    u32 preset = PresetFromEnvironment();
    const Gow3DlssApi* api{};
    std::vector<std::string> instance_extensions, device_extensions;
    std::optional<FeatureDesc> feature;
    std::string problem;
    bool eligible{true}, instance_ready{}, device_ready{}, initialized{}, available{};

    void Disable(std::string_view reason) {
        eligible = available = false;
        problem = reason;
        std::printf("DLSS: unavailable (%.*s)\n", int(reason.size()), reason.data());
    }
};

Dlss* Dlss::Get() {
    static Dlss* const dlss = []() -> Dlss* {
        const char* setting = std::getenv("GOW3_DLSS");
        if (setting && setting[0] == '0') {
            return nullptr;
        }
        const auto directory = ExecutableDirectory();
        if (!std::filesystem::is_regular_file(directory / BridgeName) || !HasNgxLibrary(directory)) {
            return nullptr;
        }
        auto* created = new Dlss;
        // The process may end through quick_exit/_exit paths: release NGX first when it can.
        std::atexit([] {
            if (Dlss* live = Get()) {
                live->Shutdown();
            }
        });
        return created;
    }();
    return dlss;
}

Dlss::Dlss() : impl{std::make_unique<Impl>()} {
    const auto directory = ExecutableDirectory();
    impl->version = NgxVersion(directory);
    impl->module = OpenModule(directory / BridgeName);
    const auto get_api = impl->module
        ? reinterpret_cast<Gow3DlssGetApiFn>(Symbol(impl->module, "Gow3DlssGetApi"))
        : nullptr;
    impl->api = get_api ? get_api() : nullptr;
    if (!impl->api || impl->api->abi != GOW3_DLSS_BRIDGE_ABI) {
        impl->Disable(std::string{BridgeName} + " is missing or from another version");
        return;
    }
    // NGX writes its logs and model updates here: beside the saves and shader caches.
    std::filesystem::path data = directory / "dlss";
    if (const char* user = std::getenv("GOW3_GPU_USER_DIR"); user && user[0]) {
        data = std::filesystem::path{user} / "dlss";
    }
    std::error_code error;
    std::filesystem::create_directories(data, error);
    // NGX takes wide-character paths on every platform.
    if (!impl->api->Configure(directory.wstring().c_str(), data.wstring().c_str(), BridgeLog)) {
        impl->Disable("bridge configuration failed");
    }
}

Dlss::~Dlss() {
    Shutdown();
}

void Dlss::AppendInstanceExtensions(std::vector<const char*>& enabled) {
    if (!impl->eligible) {
        return;
    }
    u32 count{};
    const VkExtensionProperties* required{};
    if (!impl->api->InstanceExtensions(&count, &required)) {
        return impl->Disable("instance extension query failed");
    }
    const auto [result, extensions] = vk::enumerateInstanceExtensionProperties();
    if (result != vk::Result::eSuccess ||
        !CollectExtensions(required, count, extensions, impl->instance_extensions, enabled)) {
        return impl->Disable("required Vulkan instance extensions are missing");
    }
    impl->instance_ready = true;
}

void Dlss::AppendDeviceExtensions(vk::Instance instance, vk::PhysicalDevice physical,
                                  std::vector<const char*>& enabled) {
    if (!impl->eligible || !impl->instance_ready) {
        return;
    }
    const auto properties = physical.getProperties();
    if (properties.vendorID != 0x10de) {
        return impl->Disable("not an NVIDIA GPU");
    }
    u32 count{};
    const VkExtensionProperties* required{};
    if (!impl->api->DeviceExtensions(instance, physical, &count, &required)) {
        return impl->Disable("this GPU or driver does not support DLSS (GeForce RTX needed)");
    }
    const auto [result, extensions] = physical.enumerateDeviceExtensionProperties();
    if (result != vk::Result::eSuccess ||
        !CollectExtensions(required, count, extensions, impl->device_extensions, enabled)) {
        return impl->Disable("required Vulkan device extensions are missing");
    }
    impl->device_ready = true;
}

void Dlss::Initialize(vk::Instance instance, vk::PhysicalDevice physical, vk::Device device) {
    if (!impl->eligible || !impl->device_ready || impl->initialized) {
        return;
    }
    impl->initialized = true;
    if (!impl->api->Initialize(instance, physical, device,
                               VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
                               VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr)) {
        return impl->Disable("NVIDIA NGX initialization failed (update the GPU driver)");
    }
    impl->available = true;
    const char letter = PresetLetter(impl->preset);
    std::printf("DLSS: ready (NVIDIA DLSS library %s, model preset %s%s)\n",
                impl->version.empty() ? "?" : impl->version.c_str(),
                letter ? std::string(1, letter).c_str() : "driver default",
                letter >= 'L' ? ", DLSS 4.5" : letter ? ", DLSS 4" : "");
}

bool Dlss::Available() const {
    return impl->available;
}

std::string Dlss::Problem() const {
    return impl->problem;
}

std::string Dlss::Description() const {
    if (!impl->available || !impl->feature) {
        return {};
    }
    static constexpr const char* modes[] = {"DLAA", "Quality", "Balanced", "Performance",
                                            "Ultra Performance"};
    // Presets J and K are DLSS 4's transformer, L and M DLSS 4.5's second generation (SDK
    // 310.5.0 and later); the driver's default has no fixed generation.
    const char letter = PresetLetter(impl->preset);
    std::string text = letter >= 'L' ? "DLSS 4.5" : letter ? "DLSS 4" : "DLSS";
    if (!impl->version.empty()) {
        text += " (" + impl->version + ")";
    }
    text += std::string(" ") + modes[std::clamp(impl->feature->quality, 0, 4)];
    if (letter) {
        text += std::string(" \xc2\xb7 ") + letter;
    }
    return text;
}

bool Dlss::HasFeature(const FeatureDesc& desc) const {
    return impl->feature && *impl->feature == desc;
}

bool Dlss::CreateFeature(vk::CommandBuffer command, const FeatureDesc& desc) {
    if (!impl->available) {
        return false;
    }
    impl->feature.reset();
    const Gow3DlssFeature feature{desc.input_width,  desc.input_height, desc.output_width,
                                desc.output_height, desc.quality,      0,
                                impl->preset,       desc.hdr ? 1 : 0};
    if (!impl->api->CreateFeature(command, &feature)) {
        return false;
    }
    impl->feature = desc;
    std::printf("DLSS: running %s\n", Description().c_str());
    return true;
}

bool Dlss::Evaluate(vk::CommandBuffer command, const Frame& frame) {
    if (!impl->available || !impl->feature) {
        return false;
    }
    const auto image = [](const Resource& r) {
        const vk::ImageSubresourceRange range{r.aspect, 0, 1, 0, 1};
        return Gow3DlssImage{r.image,
                           r.view,
                           static_cast<VkImageSubresourceRange>(range),
                           static_cast<VkFormat>(r.format),
                           r.width,
                           r.height};
    };
    const Gow3DlssEvaluate parameters{image(frame.color),  image(frame.depth), image(frame.motion),
                                    image(frame.output), frame.jitter_x,     frame.jitter_y,
                                    frame.reset ? 1 : 0, frame.frame_ms,     frame.sharpness};
    return impl->api->Evaluate(command, &parameters) != 0;
}

void Dlss::ReleaseFeature() {
    impl->feature.reset();
    if (impl->api) {
        impl->api->ReleaseFeature();
    }
}

void Dlss::Shutdown() {
    impl->available = false;
    impl->feature.reset();
    if (impl->api && impl->initialized) {
        impl->api->Shutdown();
        impl->initialized = false;
    }
}



} // namespace Vulkan
