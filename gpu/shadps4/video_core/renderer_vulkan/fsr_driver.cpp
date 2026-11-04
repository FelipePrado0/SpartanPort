// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_vulkan/fsr_driver_api.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string_view>
#ifdef _WIN32
#include <windows.h>
#endif

namespace Vulkan::FsrDriver {

namespace {
std::string ContextVersion(ffxContext context);
}

bool IsFsr41(const char* name) {
    return name && std::string_view(name).starts_with("4.1.") && name[4] >= '0' && name[4] <= '9';
}

const char* ModelName(std::string_view name) {
    if (name == ".?AVffxProvider_FSR4_Int8_Navi2@@")
        return "INT8 RDNA2";
    if (name == ".?AVffxProvider_FSR4_Int8@@")
        return "INT8";
    if (name == ".?AVffxProvider_FSR4Lite_Int8@@")
        return "INT8 Lite";
    if (name == ".?AVffxProvider_FSR4@@")
        return "FP8";
    return nullptr;
}

std::string ActiveVersion(const Provider& provider, ffxContext& context) {
    if (!context || !provider.query)
        return {};
    ffxQueryGetProviderVersion query{};
    query.header.type = FFX_API_QUERY_DESC_TYPE_GET_PROVIDER_VERSION;
    const auto result = provider.query(&context, &query.header);
    if (result == FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE)
        return ContextVersion(context);
    if (result != FFX_API_RETURN_OK || !query.versionId || !IsFsr41(query.versionName))
        return {};
    return query.versionName;
}

ffxDispatchDescUpscale MakeDispatch(const Fsr4Upscaler::Frame& frame) {
    const auto resource = [](const Fsr4Upscaler::Image& image, uint32_t format, uint32_t usage) {
        FfxApiResource result{};
        result.resource = reinterpret_cast<void*>(VkImage(image.image));
        result.description.type = FFX_API_RESOURCE_TYPE_TEXTURE2D;
        result.description.format = format;
        result.description.width = image.width;
        result.description.height = image.height;
        result.description.depth = 1;
        result.description.mipCount = 1;
        result.description.usage = usage;
        result.state = usage & FFX_API_RESOURCE_USAGE_UAV ? FFX_API_RESOURCE_STATE_UNORDERED_ACCESS
                                                          : FFX_API_RESOURCE_STATE_COMPUTE_READ;
        return result;
    };
    ffxDispatchDescUpscale info{};
    info.header.type = FFX_API_DISPATCH_DESC_TYPE_UPSCALE;
    info.commandList = reinterpret_cast<void*>(VkCommandBuffer(frame.cmdbuf));
    info.color = resource(frame.color,
                          frame.color_format == vk::Format::eR8G8B8A8Unorm
                              ? FFX_API_SURFACE_FORMAT_R8G8B8A8_UNORM
                          : frame.color_format == vk::Format::eR8G8B8A8Srgb
                              ? FFX_API_SURFACE_FORMAT_R8G8B8A8_SRGB
                              : FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT,
                          0);
    info.depth =
        resource(frame.depth, FFX_API_SURFACE_FORMAT_R32_FLOAT, FFX_API_RESOURCE_USAGE_DEPTHTARGET);
    info.motionVectors = resource(frame.motion, FFX_API_SURFACE_FORMAT_R16G16_FLOAT, 0);
    info.output = resource(frame.output, FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT,
                           FFX_API_RESOURCE_USAGE_UAV);
    info.jitterOffset = {frame.jitter[0], frame.jitter[1]};
    info.motionVectorScale = {1.0f, 1.0f};
    info.renderSize = {frame.render_width, frame.render_height};
    info.upscaleSize = {frame.output.width, frame.output.height};
    info.enableSharpening = frame.sharpen;
    info.sharpness = frame.sharpness;
    info.frameTimeDelta = frame.frame_ms;
    info.preExposure = 1.0f;
    info.reset = frame.reset;
    info.cameraNear = frame.near_plane;
    info.cameraFar = frame.far_plane;
    info.cameraFovAngleVertical = frame.vertical_fov;
    info.viewSpaceToMetersFactor = 1.0f;
    // Only UNORM holds sRGB-encoded values; an sRGB view already linearizes them.
    info.flags = frame.color_format == vk::Format::eR8G8B8A8Unorm
                     ? FFX_UPSCALE_FLAG_NON_LINEAR_COLOR_SRGB
                     : 0;
    return info;
}

namespace {
struct Backend {
    ffxCreateContextDescHeader header{0x01000003u, nullptr};
    VkDevice device;
    VkPhysicalDevice physical;
    PFN_vkGetDeviceProcAddr get_proc;
};

#ifdef _WIN32
HMODULE driver_module = nullptr;
thread_local const char* selected_model = nullptr;
thread_local std::string selected_version;
thread_local ffxContext selected_context = nullptr;

const char* ProviderModel(void* object) {
    const auto readable = [](const void* address, size_t bytes) {
        MEMORY_BASIC_INFORMATION region{};
        if (!VirtualQuery(address, &region, sizeof(region)) || region.State != MEM_COMMIT ||
            (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
            return false;
        const auto start = reinterpret_cast<uintptr_t>(address);
        const auto base = reinterpret_cast<uintptr_t>(region.BaseAddress);
        return start >= base && bytes <= region.RegionSize &&
               start - base <= region.RegionSize - bytes;
    };
    if (!driver_module || !readable(object, sizeof(void*)))
        return nullptr;
    const auto table = *static_cast<void***>(object);
    const auto base = reinterpret_cast<uintptr_t>(driver_module);
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const auto size = nt->OptionalHeader.SizeOfImage;
    const auto inside = [&](const void* address, size_t bytes) {
        const auto value = reinterpret_cast<uintptr_t>(address);
        return value >= base && bytes <= size && value - base <= size - bytes;
    };
    if (!inside(table, sizeof(void*)) ||
        reinterpret_cast<uintptr_t>(table) < base + sizeof(void*) ||
        !inside(table - 1, sizeof(void*)))
        return nullptr;
    struct Locator {
        uint32_t signature, offset, constructor, type, hierarchy, self;
    };
    const auto locator = static_cast<const Locator*>(table[-1]);
    if (!inside(locator, sizeof(Locator)) || locator->signature != 1 || locator->type >= size ||
        size - locator->type <= 2 * sizeof(void*))
        return nullptr;
    const auto name = reinterpret_cast<const char*>(base + locator->type + 2 * sizeof(void*));
    const size_t capacity = std::min<size_t>(128, size - locator->type - 2 * sizeof(void*));
    const auto end = static_cast<const char*>(std::memchr(name, 0, capacity));
    return end ? ModelName(std::string_view(name, end - name)) : nullptr;
}

std::string ProviderVersion(void* object) {
    if (!ProviderModel(object))
        return {};
    struct Identity {
        void* vtable;
        size_t references;
        uint64_t id, effect;
        const char* version;
    };
    const auto identity = static_cast<const Identity*>(object);
    MEMORY_BASIC_INFORMATION region{};
    if (!VirtualQuery(identity, &region, sizeof(region)))
        return {};
    const auto address = reinterpret_cast<uintptr_t>(identity);
    const auto base = reinterpret_cast<uintptr_t>(region.BaseAddress);
    if (region.State != MEM_COMMIT || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
        address < base || sizeof(Identity) > region.RegionSize ||
        address - base > region.RegionSize - sizeof(Identity) || !identity->id ||
        identity->effect != FFX_API_EFFECT_ID_UPSCALE || !identity->version)
        return {};
    if (!VirtualQuery(identity->version, &region, sizeof(region)) || region.State != MEM_COMMIT ||
        (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
        return {};
    const auto start = reinterpret_cast<uintptr_t>(identity->version);
    const auto first = reinterpret_cast<uintptr_t>(region.BaseAddress);
    if (start < first || start - first >= region.RegionSize)
        return {};
    const auto length = std::min<size_t>(256, region.RegionSize - (start - first));
    const auto end = static_cast<const char*>(std::memchr(identity->version, 0, length));
    if (!end)
        return {};
    const std::string version(identity->version, end);
    return IsFsr41(version.c_str()) ? version : "";
}

const char* ActiveModel(ffxContext context) {
    return context == selected_context ? selected_model : nullptr;
}
std::string ContextVersion(ffxContext context) {
    return context == selected_context ? selected_version : std::string{};
}

using IsSupported = bool (*)(void*, void*);
struct ModelObserver {
    void** table = nullptr;
    IsSupported original = nullptr;
};
std::array<ModelObserver, 4> model_observers{};

bool ObserveModel(void* provider, void* backend) {
    const auto table = *static_cast<void***>(provider);
    for (const auto& observer : model_observers) {
        if (observer.table != table)
            continue;
        const bool supported = observer.original(provider, backend);
        // The driver's ordered provider selection stops at the first supported model.
        if (supported && backend && !selected_model) {
            selected_model = ProviderModel(provider);
            selected_version = ProviderVersion(provider);
        }
        return supported;
    }
    return false;
}

void ObserveModels(HMODULE module) {
    const auto base = reinterpret_cast<uintptr_t>(module);
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const auto size = nt->OptionalHeader.SizeOfImage;
    const auto sections = IMAGE_FIRST_SECTION(nt);
    size_t found = 0;
    for (unsigned s = 0; s < nt->FileHeader.NumberOfSections; ++s) {
        const auto& section = sections[s];
        if (std::memcmp(section.Name, ".rdata", 6) || section.VirtualAddress >= size ||
            section.Misc.VirtualSize > size - section.VirtualAddress)
            continue;
        const auto start = section.VirtualAddress;
        const auto end = start + section.Misc.VirtualSize;
        // MSVC RTTI locator: signature, offset, constructor, type RVA, hierarchy RVA, self RVA.
        for (size_t rva = start; rva + 24 <= end && found < model_observers.size(); rva += 4) {
            const auto locator = reinterpret_cast<const uint32_t*>(base + rva);
            if (locator[0] != 1 || locator[1] || locator[2] || locator[5] != rva ||
                locator[3] >= size || size - locator[3] < 2 * sizeof(void*) + 128)
                continue;
            const auto name = reinterpret_cast<const char*>(base + locator[3] + 2 * sizeof(void*));
            const auto nul = static_cast<const char*>(std::memchr(name, 0, 128));
            if (!nul || !ModelName(std::string_view(name, nul - name)))
                continue;
            for (size_t entry = start; entry + 4 * sizeof(void*) <= end; entry += sizeof(void*)) {
                const auto reference = reinterpret_cast<void**>(base + entry);
                if (*reference != locator)
                    continue;
                auto table = reference + 1;
                const auto original = reinterpret_cast<uintptr_t>(table[2]);
                if (original < base || original - base >= size)
                    continue;
                DWORD protection;
                if (VirtualProtect(&table[2], sizeof(void*), PAGE_READWRITE, &protection)) {
                    model_observers[found++] = {table, reinterpret_cast<IsSupported>(table[2])};
                    table[2] = reinterpret_cast<void*>(&ObserveModel);
                    DWORD ignored;
                    VirtualProtect(&table[2], sizeof(void*), protection, &ignored);
                }
                break;
            }
        }
    }
}

HMODULE WINAPI SdkModule(LPCWSTR name) {
    // The AMD-signed provider can satisfy its SDK signature/version query itself,
    // as in OptiScaler.
    if (name && (!_wcsicmp(name, L"amd_fidelityfx_loader_vk.dll") ||
                 !_wcsicmp(name, L"amd_fidelityfx_upscaler_vk.dll")))
        return driver_module;
    return GetModuleHandleW(name);
}

class SdkLookup {
    void** slot = nullptr;
    void* original = nullptr;
    static bool Write(void** target, void* value) {
        DWORD protection;
        if (!VirtualProtect(target, sizeof(void*), PAGE_READWRITE, &protection))
            return false;
        *target = value;
        DWORD ignored;
        return VirtualProtect(target, sizeof(void*), protection, &ignored) != 0;
    }

  public:
    explicit SdkLookup(HMODULE module) {
        const auto base = reinterpret_cast<unsigned char*>(module);
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
            return;
        const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
            return;
        const auto size = nt->OptionalHeader.SizeOfImage;
        const auto directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!directory.VirtualAddress || directory.Size < sizeof(IMAGE_IMPORT_DESCRIPTOR) ||
            directory.VirtualAddress >= size || directory.Size > size - directory.VirtualAddress)
            return;
        const auto imports =
            reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress);
        for (size_t i = 0; i < directory.Size / sizeof(*imports) && imports[i].Name; ++i) {
            const auto& entry = imports[i];
            if (!entry.OriginalFirstThunk || entry.OriginalFirstThunk >= size ||
                entry.FirstThunk >= size)
                continue;
            const auto names =
                reinterpret_cast<const IMAGE_THUNK_DATA*>(base + entry.OriginalFirstThunk);
            const auto addresses = reinterpret_cast<IMAGE_THUNK_DATA*>(base + entry.FirstThunk);
            const size_t limit = std::min((size - entry.OriginalFirstThunk) / sizeof(*names),
                                          (size - entry.FirstThunk) / sizeof(*addresses));
            for (size_t j = 0; j < limit && names[j].u1.AddressOfData; ++j) {
                const auto rva = names[j].u1.AddressOfData;
                if (IMAGE_SNAP_BY_ORDINAL(names[j].u1.Ordinal) || rva >= size || size - rva < 3)
                    continue;
                const auto name = reinterpret_cast<const char*>(base + rva + 2);
                constexpr char function[] = "GetModuleHandleW";
                if (sizeof(function) > size - rva - 2 ||
                    std::memcmp(name, function, sizeof(function)))
                    continue;
                slot = reinterpret_cast<void**>(&addresses[j].u1.Function);
                original = *slot;
                if (!Write(slot, reinterpret_cast<void*>(&SdkModule)))
                    slot = nullptr;
                return;
            }
        }
    }
    ~SdkLookup() {
        if (slot)
            Write(slot, original);
    }
};

ProviderData LoadProvider() {
    ProviderData result{};
    // The loaded ICD identifies the active driver; old DriverStore packages must not win.
    const auto icd = GetModuleHandleW(L"amdvlk64.dll");
    wchar_t filename[32768];
    const auto length = icd ? GetModuleFileNameW(icd, filename, 32768) : 0;
    if (!length || length >= 32768)
        return result;
    const auto installed_path = std::filesystem::path(filename).parent_path() / L"amdvkffx64.dll";
    const auto exe_length = GetModuleFileNameW(nullptr, filename, 32768);
    const auto local_path = exe_length && exe_length < 32768
                                ? std::filesystem::path(filename).parent_path() / L"amdvkffx64.dll"
                                : std::filesystem::path{};
    // The RDNA2 driver branch omits the Vulkan runtime; an official local copy can supply it.
    std::error_code error;
    const auto path = !local_path.empty() && std::filesystem::is_regular_file(local_path, error)
                          ? local_path
                          : installed_path;
    HMODULE module = LoadLibraryExW(
        path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) {
        std::printf("Upscaler: AMD Vulkan runtime load failed (%lu): %ls\n", GetLastError(),
                    path.c_str());
        return result;
    }
    using Update = HRESULT(WINAPI*)(void*, uint32_t);
    const auto update = reinterpret_cast<Update>(GetProcAddress(module, "UpdateFfxApiProvider"));
    if (!update) {
        std::printf("Upscaler: AMD Vulkan runtime has no UpdateFfxApiProvider: %ls\n",
                    path.c_str());
        FreeLibrary(module);
        return result;
    }
    driver_module = module;
    {
        SdkLookup sdk(module);
        // Keep the provider DLL alive until process exit; queued GPU work can outlive an upscaler.
        const auto status = update(&result, sizeof(result));
        if (SUCCEEDED(status) && result.provider.create && result.provider.destroy &&
            result.provider.dispatch && result.provider.query && result.supported &&
            IsFsr41(result.provider.version_name)) {
            std::printf("Upscaler: AMD Vulkan provider %s, id %llx, %ls\n",
                        result.provider.version_name,
                        static_cast<unsigned long long>(result.provider.version_id), path.c_str());
            ObserveModels(module);
            return result;
        }
        std::printf("Upscaler: AMD Vulkan provider rejected (status %lx, version %s)\n",
                    static_cast<unsigned long>(status),
                    result.provider.version_name ? result.provider.version_name : "none");
    }
    driver_module = nullptr;
    FreeLibrary(module);
    return {};
}
#else
ProviderData LoadProvider() { return {}; }
#endif

const ProviderData& InstalledProvider() {
    static const ProviderData data = LoadProvider();
    return data;
}
#ifndef _WIN32
const char* ActiveModel(ffxContext) { return nullptr; }
std::string ContextVersion(ffxContext) { return {}; }
#endif
} // namespace

std::string InstalledVersion() {
    const auto& data = InstalledProvider();
    return data.provider.version_name ? data.provider.version_name : "";
}
} // namespace Vulkan::FsrDriver

namespace Vulkan {
struct DriverFsrUpscaler::Impl {
    FsrDriver::Backend backend;
    FsrDriver::Provider provider{};
    ffxContext context = nullptr;
    std::string problem, description;
    u32 width = 0, height = 0, output_width = 0, output_height = 0;
    bool hdr = false;
    vk::Format color_format{};

    Impl(vk::Device device, vk::PhysicalDevice physical, PFN_vkGetDeviceProcAddr get_proc)
        : backend{{0x01000003u, nullptr}, VkDevice(device), VkPhysicalDevice(physical), get_proc} {
        const auto& installed = FsrDriver::InstalledProvider();
        const struct {
            VkDevice device;
            VkPhysicalDevice physical;
            PFN_vkGetDeviceProcAddr get_proc;
        } support{backend.device, backend.physical, backend.get_proc};
        if (!installed.provider.create) {
            problem = "AMD Vulkan FSR 4.1 runtime unavailable (amdvkffx64.dll)";
        } else if (!installed.supported(&support)) {
            problem = "The AMD Vulkan FSR 4.1 provider rejected this device or its "
                      "enabled features";
        } else {
            provider = installed.provider;
        }
    }
    void Destroy() {
        if (!context)
            return;
        const auto wait = reinterpret_cast<PFN_vkDeviceWaitIdle>(
            backend.get_proc(backend.device, "vkDeviceWaitIdle"));
        if (wait)
            wait(backend.device);
        provider.destroy(&context, nullptr);
        context = nullptr;
    }
    ~Impl() { Destroy(); }
    bool Record(const Fsr4Upscaler::Frame& f) {
        if (!provider.create)
            return false;
        if (context &&
            (width != f.render_width || height != f.render_height ||
             output_width != f.output.width || output_height != f.output.height || hdr != f.hdr ||
             color_format != f.color_format))
            Destroy();
        if (!context) {
            ffxCreateContextDescUpscaleVersion version{
                {FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE_VERSION, nullptr}, FFX_UPSCALER_VERSION};
            backend.header.pNext = &version.header;
            ffxCreateContextDescUpscale create{};
            create.header = {FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE, &backend.header};
            create.flags = f.hdr ? FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE
                           : f.color_format == vk::Format::eR8G8B8A8Unorm
                               ? FFX_UPSCALE_ENABLE_NON_LINEAR_COLORSPACE
                               : 0;
            if (f.auto_exposure)
                create.flags |= FFX_UPSCALE_ENABLE_AUTO_EXPOSURE;
            create.maxRenderSize = {f.render_width, f.render_height};
            create.maxUpscaleSize = {f.output.width, f.output.height};
#ifdef _WIN32
            FsrDriver::selected_model = nullptr;
            FsrDriver::selected_version.clear();
            FsrDriver::selected_context = nullptr;
#endif
            const auto result = provider.create(&context, &create.header, nullptr);
#ifdef _WIN32
            if (result == FFX_API_RETURN_OK)
                FsrDriver::selected_context = context;
#endif
            backend.header.pNext = nullptr;
            if (result != FFX_API_RETURN_OK || !context) {
                problem = "AMD FSR 4.1 context creation failed (" + std::to_string(result) + ")";
                Destroy();
                return false;
            }
            const auto actual = FsrDriver::ActiveVersion(provider, context);
            if (actual.empty()) {
                problem = "AMD returned a different or unverified upscaler; FSR 4.1 is "
                          "not active";
                Destroy();
                return false;
            }
            width = f.render_width;
            height = f.render_height;
            output_width = f.output.width;
            output_height = f.output.height;
            hdr = f.hdr;
            color_format = f.color_format;
            const char* model = FsrDriver::ActiveModel(context);
            if (!model) {
                problem = "The AMD FSR 4.1 hardware model could not be verified";
                Destroy();
                return false;
            }
            description = "AMD Vulkan " + actual + ", model " + model + ", " +
                          std::to_string(width) + "x" + std::to_string(height) + " -> " +
                          std::to_string(output_width) + "x" + std::to_string(output_height);
            std::printf("Upscaler: %s\n", description.c_str());
        }
        const auto barrier = reinterpret_cast<PFN_vkCmdPipelineBarrier2>(
            backend.get_proc(backend.device, "vkCmdPipelineBarrier2"));
        if (!barrier) {
            problem = "FSR 4.1 requires Vulkan synchronization2";
            return false;
        }
        const auto transition = [&](bool restore) {
            const Fsr4Upscaler::Image* images[] = {&f.color, &f.depth, &f.motion};
            VkImageMemoryBarrier2 barriers[3]{};
            for (size_t i = 0; i < 3; ++i) {
                auto& b = barriers[i];
                b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                b.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
                b.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
                b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
                b.dstAccessMask = restore
                                      ? VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT
                                      : VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
                b.oldLayout =
                    restore ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
                b.newLayout =
                    restore ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b.image = VkImage(images[i]->image);
                b.subresourceRange = {
                    i == 1 ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            }
            const VkDependencyInfo dependency{
                VK_STRUCTURE_TYPE_DEPENDENCY_INFO, nullptr, 0, 0, nullptr, 0, nullptr, 3, barriers};
            barrier(VkCommandBuffer(f.cmdbuf), &dependency);
        };
        transition(false);
        const auto dispatch = FsrDriver::MakeDispatch(f);
        const auto result = provider.dispatch(&context, &dispatch.header);
        transition(true);
        if (result != FFX_API_RETURN_OK) {
            problem = "AMD FSR 4.1 dispatch failed (" + std::to_string(result) + ")";
            return false;
        }
        problem.clear();
        return true;
    }
};

DriverFsrUpscaler::DriverFsrUpscaler(vk::Device device, vk::PhysicalDevice physical,
                                     PFN_vkGetDeviceProcAddr get_proc)
    : impl{std::make_unique<Impl>(device, physical, get_proc)} {}
DriverFsrUpscaler::~DriverFsrUpscaler() = default;
bool DriverFsrUpscaler::Available() const { return impl->provider.create != nullptr; }
bool DriverFsrUpscaler::Record(const Fsr4Upscaler::Frame& frame) { return impl->Record(frame); }
const std::string& DriverFsrUpscaler::Problem() const { return impl->problem; }
const std::string& DriverFsrUpscaler::Description() const { return impl->description; }
} // namespace Vulkan
