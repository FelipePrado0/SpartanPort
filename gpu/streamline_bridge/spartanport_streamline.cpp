// SPDX-License-Identifier: MIT
#include "spartanport_streamline.h"
#include "sl.h"
#include "sl_dlss_g.h"
#include "sl_matrix_helpers.h"
#include "sl_reflex.h"
#include "sl_security.h"
#include <algorithm>
#include <atomic>
#include <bcrypt.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>
#include <vector>
#include <windows.h>

namespace {
HMODULE interposer{};
bool initialized{}, ready{}, enabled{};
std::atomic<int32_t> async_error{};
std::atomic<const char*> problem{"NVIDIA frame generation is not initialized"};
std::mutex api_mutex;
sl::ViewportHandle viewport{0};
PFun_slInit* init{};
PFun_slShutdown* shutdown_sdk{};
PFun_slIsFeatureSupported* is_supported{};
PFun_slGetFeatureFunction* feature_function{};
PFun_slGetNewFrameToken* new_token{};
PFun_slSetConstants* set_constants{};
PFun_slSetTagForFrame* set_tags{};
PFun_slDLSSGSetOptions* set_options{};
PFun_slDLSSGGetState* get_state{};
PFun_slReflexSetOptions* reflex_options{};
PFun_slReflexSleep* reflex_sleep{};
PFun_slPCLSetMarker* set_marker{};
PFN_vkGetInstanceProcAddr resolver{};

int Fail(const char* reason) {
    problem = reason;
    return 0;
}
bool Pinned(const std::filesystem::path& path, std::string_view expected) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto size = file.tellg();
    if (size <= 0 || size > 16 * 1024 * 1024 || expected.size() != 64)
        return false;
    std::vector<UCHAR> bytes(static_cast<size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), size))
        return false;
    BCRYPT_ALG_HANDLE algorithm{};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        return false;
    UCHAR digest[32]{};
    const auto status = BCryptHash(algorithm, nullptr, 0, bytes.data(), ULONG(bytes.size()), digest,
                                   sizeof(digest));
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status < 0)
        return false;
    constexpr char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < 32; ++i) {
        if (hex[digest[i] >> 4] != expected[i * 2] || hex[digest[i] & 15] != expected[i * 2 + 1])
            return false;
    }
    return true;
}
template <class T> bool Import(T*& out, const char* name) {
    out = reinterpret_cast<T*>(GetProcAddress(interposer, name));
    return out != nullptr;
}
template <class T> bool Feature(T*& out, sl::Feature feature, const char* name) {
    void* address{};
    if (feature_function(feature, name, address) != sl::Result::eOk || !address)
        return false;
    out = reinterpret_cast<T*>(address);
    return true;
}

int Configure(const wchar_t* directory) {
    std::scoped_lock lock{api_mutex};
    if (initialized)
        return 1;
    if (!directory || !std::filesystem::path(directory).is_absolute())
        return Fail("invalid SDK directory");
    // Hashes from the verified 2.14.16 release archive. NGX DLLs lack SL's secondary signature.
    constexpr std::pair<const wchar_t*, const char*> pins[]{
        {L"sl.interposer.dll", "b1d7395aebd7b9722841d64dcf7433680020b6a8e421f851c16d5ba0c2182edf"},
        {L"sl.common.dll", "38eea3b4209afc455636ea372b5933d65fcc13becec6c84d101dcfc4d8648f75"},
        {L"sl.dlss_g.dll", "3ed9538df724993bea8a050195a6a78b9c8c8a566d5b05ade477aab3c390e90f"},
        {L"sl.reflex.dll", "443577e869352436ff7ec8f4d9c90418d3e95368436ce219261e0f4f07a38122"},
        {L"sl.pcl.dll", "5698c38e722318681f215c9fe68e45b9bedd51378acc699d8eee26e961e57770"},
        {L"nvngx_dlssg.dll", "8d59d49b36410dd6b62fef2503a5871d347415bd85d943fa0848563a95dd50c7"},
        {L"NvLowLatencyVk.dll",
         "2a77dc3e1c724b7eea5755be0ae7423752e79a2459fae72181a9f00e3507e5d6"}};
    for (const auto& [name, hash] : pins) {
        if (!Pinned(std::filesystem::path(directory) / name, hash))
            return Fail(
                "NVIDIA SDK DLL is missing or differs from the pinned Streamline 2.14.16 release");
    }
    if (!sl::security::verifyEmbeddedSignature(
            (std::filesystem::path(directory) / L"sl.interposer.dll").c_str()))
        return Fail("NVIDIA Streamline signature verification failed");
    interposer =
        LoadLibraryExW((std::filesystem::path(directory) / L"sl.interposer.dll").c_str(), nullptr,
                       LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!interposer)
        return Fail("could not load sl.interposer.dll");
    if (!Import(init, "slInit") || !Import(shutdown_sdk, "slShutdown") ||
        !Import(is_supported, "slIsFeatureSupported") ||
        !Import(feature_function, "slGetFeatureFunction") ||
        !Import(new_token, "slGetNewFrameToken") || !Import(set_constants, "slSetConstants") ||
        !Import(set_tags, "slSetTagForFrame") || !Import(resolver, "vkGetInstanceProcAddr")) {
        return Fail("incompatible Streamline exports");
    }
    constexpr sl::Feature features[]{sl::kFeatureDLSS_G, sl::kFeatureReflex, sl::kFeaturePCL};
    const wchar_t* paths[]{directory};
    sl::Preferences preferences{};
    preferences.pathsToPlugins = paths;
    preferences.numPathsToPlugins = 1;
    preferences.featuresToLoad = features;
    preferences.numFeaturesToLoad = 3;
    preferences.flags = sl::PreferenceFlags::eDisableCLStateTracking |
                        sl::PreferenceFlags::eUseManualHooking |
                        sl::PreferenceFlags::eUseFrameBasedResourceTagging;
    preferences.engineVersion = "SpartanPort 1.0";
    preferences.projectId = "cb968bc2-39a2-4d93-a2b7-3d9021b9415b";
    preferences.renderAPI = sl::RenderAPI::eVulkan;
    preferences.pathToLogsAndData = nullptr;
    if (init(preferences, sl::kSDKVersion) != sl::Result::eOk)
        return Fail("Streamline initialization failed");
    initialized = true;
    return 1;
}
int Supported(VkPhysicalDevice physical, const uint8_t* luid, uint32_t luid_size) {
    std::scoped_lock lock{api_mutex};
    if (!initialized || !physical || !luid || luid_size != VK_LUID_SIZE)
        return Fail("selected NVIDIA adapter has no valid Windows LUID");
    sl::AdapterInfo adapter{};
    adapter.vkPhysicalDevice = physical;
    uint8_t identity[VK_LUID_SIZE];
    std::copy_n(luid, VK_LUID_SIZE, identity);
    adapter.deviceLUID = identity;
    adapter.deviceLUIDSizeInBytes = VK_LUID_SIZE;
    const auto supported = is_supported(sl::kFeatureDLSS_G, adapter);
    if (supported == sl::Result::eErrorOSDisabledHWS)
        return Fail(
            "enable Hardware-accelerated GPU scheduling in Windows and restart the computer");
    if (supported == sl::Result::eErrorDriverOutOfDate)
        return Fail("update the NVIDIA graphics driver for DLSS frame generation");
    if (supported == sl::Result::eErrorOSOutOfDate)
        return Fail("update Windows for DLSS frame generation");
    if (supported != sl::Result::eOk)
        return Fail("DLSS frame generation requires a supported RTX 40/50 GPU, driver and OS");
    if (is_supported(sl::kFeatureReflex, adapter) != sl::Result::eOk)
        return Fail("NVIDIA Reflex is unavailable");
    return 1;
}
int Initialize() {
    std::scoped_lock lock{api_mutex};
    if (!initialized)
        return 0;
    if (!Feature(set_options, sl::kFeatureDLSS_G, "slDLSSGSetOptions") ||
        !Feature(get_state, sl::kFeatureDLSS_G, "slDLSSGGetState") ||
        !Feature(reflex_options, sl::kFeatureReflex, "slReflexSetOptions") ||
        !Feature(reflex_sleep, sl::kFeatureReflex, "slReflexSleep") ||
        !Feature(set_marker, sl::kFeaturePCL, "slPCLSetMarker"))
        return Fail("NVIDIA feature entry points are unavailable");
    sl::ReflexOptions options{};
    options.mode = sl::ReflexMode::eLowLatency;
    if (reflex_options(options) != sl::Result::eOk)
        return Fail("NVIDIA Reflex initialization failed");
    ready = true;
    problem = nullptr;
    return 1;
}
void* BeginFrame(uint32_t id) {
    sl::FrameToken* token{};
    {
        std::scoped_lock lock{api_mutex};
        if (!ready)
            return nullptr;
        if (new_token(token, &id) != sl::Result::eOk || !token) {
            Fail("could not obtain NVIDIA frame token");
            return nullptr;
        }
    }
    // Sleep must not hold the lock needed by Present to retire the previous frame.
    if (reflex_sleep(*token) != sl::Result::eOk) {
        Fail("NVIDIA Reflex sleep failed");
        return nullptr;
    }
    return reinterpret_cast<void*>(uintptr_t(id) + 1);
}
sl::FrameToken* TokenFor(void* ticket) {
    // SDK token objects rotate after six factory requests; keep only the immutable frame index.
    if (!ticket || uintptr_t(ticket) - 1 > UINT32_MAX)
        return nullptr;
    const auto id = uint32_t(uintptr_t(ticket) - 1);
    sl::FrameToken* token{};
    return new_token(token, &id) == sl::Result::eOk ? token : nullptr;
}
int Marker(void* opaque, uint32_t marker) {
    std::scoped_lock lock{api_mutex};
    if (!ready || !opaque || marker > uint32_t(sl::PCLMarker::ePresentEnd))
        return 0;
    auto* token = TokenFor(opaque);
    return token && set_marker(sl::PCLMarker(marker), *token) == sl::Result::eOk;
}
bool Valid(const SpartanSlImage* image) {
    return image && image->image && image->view && image->width && image->height &&
           image->format != VK_FORMAT_UNDEFINED && (image->usage & VK_IMAGE_USAGE_SAMPLED_BIT);
}
sl::Resource Resource(const SpartanSlImage& image) {
    sl::Resource result{sl::ResourceType::eTex2d, (void*)image.image, nullptr, (void*)image.view,
                        uint32_t(image.layout)};
    result.width = image.width;
    result.height = image.height;
    result.nativeFormat = image.format;
    result.usage = image.usage;
    result.mipLevels = result.arrayLayers = 1;
    result.flags = 0;
    return result;
}
sl::float4x4 Projection(const float* p) {
    // Streamline uses row vectors; the game's affine matrices use column vectors.
    sl::float4x4 result{};
    result[0] = {p[0], 0, 0, 0};
    result[1] = {0, p[1], 0, 0};
    result[2] = {0, 0, p[2], 1};
    result[3] = {0, 0, p[3], 0};
    return result;
}
sl::float4x4 Affine(const float* m) {
    sl::float4x4 result{};
    result[0] = {m[0], m[4], m[8], 0};
    result[1] = {m[1], m[5], m[9], 0};
    result[2] = {m[2], m[6], m[10], 0};
    result[3] = {m[3], m[7], m[11], 1};
    return result;
}
int Inputs(void* opaque, VkCommandBuffer cmd, const SpartanSlCamera* camera,
           const SpartanSlImage* depth, const SpartanSlImage* motion) {
    std::scoped_lock lock{api_mutex};
    if (!ready || !opaque || !cmd || !camera || !Valid(depth) || !Valid(motion) ||
        depth->width != motion->width || depth->height != motion->height)
        return 0;
    const auto& c = *camera;
    for (const float* projection : {c.projection, c.previous_projection}) {
        if (!std::all_of(projection, projection + 4, [](float v) { return std::isfinite(v); }) ||
            projection[0] == 0 || projection[1] == 0 || projection[3] == 0)
            return Fail("singular NVIDIA projection matrix");
    }
    if (!std::isfinite(c.jitter[0]) || !std::isfinite(c.jitter[1]))
        return Fail("invalid NVIDIA jitter");
    if (!(std::isfinite(c.near_plane) && c.near_plane > 0 && c.far_plane > c.near_plane &&
          std::isfinite(c.fov) && c.fov > 0 && c.fov < 3.141593f))
        return Fail("invalid NVIDIA camera range");
    sl::Constants constants{};
    constants.cameraViewToClip = Projection(c.projection);
    sl::matrixFullInvert(constants.clipToCameraView, constants.cameraViewToClip);
    sl::float4x4 view_to_previous{}, clip_to_previous_view{};
    sl::matrixMul(view_to_previous, Affine(c.inverse_view), Affine(c.previous_view));
    sl::matrixMul(clip_to_previous_view, constants.clipToCameraView, view_to_previous);
    sl::matrixMul(constants.clipToPrevClip, clip_to_previous_view,
                  Projection(c.previous_projection));
    sl::matrixFullInvert(constants.prevClipToClip, constants.clipToPrevClip);
    for (const auto* matrix : {&constants.cameraViewToClip, &constants.clipToCameraView,
                               &constants.clipToPrevClip, &constants.prevClipToClip}) {
        const auto* data = &(*matrix)[0].x;
        for (int i = 0; i < 16; ++i)
            if (!std::isfinite(data[i]))
                return Fail("invalid NVIDIA camera matrix");
    }
    constants.jitterOffset = {c.jitter[0], c.jitter[1]};
    constants.mvecScale = {1.0f / motion->width, 1.0f / motion->height};
    constants.cameraPos = {c.inverse_view[3], c.inverse_view[7], c.inverse_view[11]};
    constants.cameraRight = {c.inverse_view[0], c.inverse_view[4], c.inverse_view[8]};
    constants.cameraUp = {c.inverse_view[1], c.inverse_view[5], c.inverse_view[9]};
    constants.cameraFwd = {c.inverse_view[2], c.inverse_view[6], c.inverse_view[10]};
    constants.cameraNear = c.near_plane;
    constants.cameraFar = c.far_plane;
    constants.cameraFOV = c.fov;
    constants.cameraAspectRatio = float(depth->width) / depth->height;
    constants.depthInverted = sl::Boolean::eFalse;
    constants.cameraMotionIncluded = sl::Boolean::eTrue;
    constants.motionVectors3D = sl::Boolean::eFalse;
    constants.reset = c.reset ? sl::Boolean::eTrue : sl::Boolean::eFalse;
    auto* frame = TokenFor(opaque);
    if (!frame)
        return Fail("invalid NVIDIA frame ticket");
    auto& token = *frame;
    if (set_constants(constants, token, viewport) != sl::Result::eOk)
        return Fail("NVIDIA camera constants failed");
    auto d = Resource(*depth), m = Resource(*motion);
    const sl::ResourceTag tags[]{{&d, sl::kBufferTypeDepth, sl::eValidUntilPresent},
                                 {&m, sl::kBufferTypeMotionVectors, sl::eValidUntilPresent}};
    if (set_tags(token, viewport, tags, 2, cmd) != sl::Result::eOk)
        return Fail("NVIDIA depth/motion tagging failed");
    return 1;
}
int Color(void* opaque, VkCommandBuffer cmd, const SpartanSlImage* hudless,
          const SpartanSlImage* alpha) {
    std::scoped_lock lock{api_mutex};
    if (!ready || !opaque || !cmd || !Valid(hudless) || !Valid(alpha) ||
        hudless->width != alpha->width || hudless->height != alpha->height)
        return 0;
    auto h = Resource(*hudless), a = Resource(*alpha);
    const sl::ResourceTag tags[]{{&h, sl::kBufferTypeHUDLessColor, sl::eValidUntilPresent},
                                 {&a, sl::kBufferTypeUIAlpha, sl::eValidUntilPresent}};
    auto* token = TokenFor(opaque);
    if (!token || set_tags(*token, viewport, tags, 2, cmd) != sl::Result::eOk)
        return Fail("NVIDIA HUD tagging failed");
    return 1;
}
int SetEnabled(int value) {
    std::scoped_lock lock{api_mutex};
    if (!ready)
        return 0;
    if (enabled == bool(value))
        return 1;
    sl::DLSSGOptions options{};
    options.mode = value ? sl::DLSSGMode::eOn : sl::DLSSGMode::eOff;
    options.numFramesToGenerate = 1;
    // Tagged resources are reused on this queue after Present, without cross-queue fence waits.
    options.queueParallelismMode = sl::DLSSGQueueParallelismMode::eBlockPresentingClientQueue;
    options.onErrorCallback = [](const sl::APIError& error) {
        async_error.store(int32_t(error.vkRes));
    };
    if (set_options(viewport, options) != sl::Result::eOk)
        return Fail("NVIDIA frame generation options failed");
    enabled = value != 0;
    return 1;
}
int GetState(SpartanSlState* out) {
    std::scoped_lock lock{api_mutex};
    if (!ready || !out)
        return 0;
    const auto error = async_error.exchange(0);
    if (error < 0 && error != VK_ERROR_OUT_OF_DATE_KHR && error != VK_ERROR_SURFACE_LOST_KHR)
        return Fail("NVIDIA asynchronous presentation failed");
    sl::DLSSGState state{};
    if (get_state(viewport, state, nullptr) != sl::Result::eOk)
        return Fail("NVIDIA frame generation state failed");
    *out = {state.numFramesActuallyPresented, uint32_t(state.status), state.minWidthOrHeight,
            state.bIsVsyncSupportAvailable == sl::Boolean::eTrue};
    return 1;
}
void Shutdown() {
    std::scoped_lock lock{api_mutex};
    ready = enabled = false;
    if (initialized) {
        shutdown_sdk();
        initialized = false;
    }
    // Vulkan dispatch pointers can outlive shutdown until native device/instance destruction.
}
} // namespace
extern "C" __declspec(dllexport) const SpartanSlApi* SpartanStreamlineGetApi() {
    static const SpartanSlApi api{SPARTAN_STREAMLINE_ABI,
                                  sizeof(SpartanSlApi),
                                  Configure,
                                  Supported,
                                  [] { return resolver; },
                                  Initialize,
                                  BeginFrame,
                                  Marker,
                                  Inputs,
                                  Color,
                                  SetEnabled,
                                  GetState,
                                  [] { return problem.load(); },
                                  Shutdown};
    return &api;
}
