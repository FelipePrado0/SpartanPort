// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: see vk_xess.h.

#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_xess.h"

#if defined(_WIN32) && __has_include(<xess_vk.h>)
#define SPARTAN_HAS_XESS 1
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <xess_vk.h>
#else
#define SPARTAN_HAS_XESS 0
#endif

namespace Vulkan {

#if SPARTAN_HAS_XESS

namespace {

/// libxess.dll and the entry points this file calls; loaded once, on first use.
struct Library {
    HMODULE module{};
    std::string error;
    xess_result_t (*get_version)(xess_version_t*){};
    xess_result_t (*create)(VkInstance, VkPhysicalDevice, VkDevice, xess_context_handle_t*){};
    xess_result_t (*init)(xess_context_handle_t, const xess_vk_init_params_t*){};
    xess_result_t (*execute)(xess_context_handle_t, VkCommandBuffer,
                             const xess_vk_execute_params_t*){};
    xess_result_t (*destroy)(xess_context_handle_t){};
    xess_result_t (*required_extensions)(VkInstance, VkPhysicalDevice, uint32_t*,
                                         const char* const**){};
    xess_result_t (*set_logging)(xess_context_handle_t, xess_logging_level_t,
                                 xess_app_log_callback_t){};
    xess_result_t (*is_optimal_driver)(xess_context_handle_t){};

    bool Ok() const {
        return module && create && init && execute && destroy;
    }
};

std::string ExecutableDirectory() {
    wchar_t path[MAX_PATH * 2];
    const DWORD length = GetModuleFileNameW(nullptr, path, DWORD(std::size(path)));
    if (length == 0 || length >= std::size(path)) {
        return {};
    }
    std::wstring wide(path, length);
    const auto slash = wide.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return {};
    }
    wide.resize(slash);
    std::string narrow(wide.size(), '\0');
    for (size_t i = 0; i < wide.size(); ++i) {
        if (wide[i] > 0x7f) {
            return {}; // a non-ASCII folder: the default search below still finds it
        }
        narrow[i] = char(wide[i]);
    }
    return narrow;
}

template <typename T>
void Resolve(HMODULE module, const char* name, T& out) {
    out = reinterpret_cast<T>(reinterpret_cast<void*>(GetProcAddress(module, name)));
}

Library& GetLibrary() {
    static Library library = [] {
        Library lib;
        const std::string directory = ExecutableDirectory();
        // Next to the executable, in its bin folder, then wherever Windows looks.
        if (!directory.empty()) {
            // Altered search path: the library's own dependencies are looked for beside it.
            lib.module = LoadLibraryExA((directory + "\\libxess.dll").c_str(), nullptr,
                                        LOAD_WITH_ALTERED_SEARCH_PATH);
            if (!lib.module) {
                lib.module = LoadLibraryExA((directory + "\\bin\\libxess.dll").c_str(), nullptr,
                                            LOAD_WITH_ALTERED_SEARCH_PATH);
            }
        }
        if (!lib.module) {
            lib.module = LoadLibraryA("libxess.dll");
        }
        if (!lib.module) {
            lib.error = "libxess.dll was not found next to the game (Intel's XeSS library)";
            return lib;
        }
        Resolve(lib.module, "xessGetVersion", lib.get_version);
        Resolve(lib.module, "xessVKCreateContext", lib.create);
        Resolve(lib.module, "xessVKInit", lib.init);
        Resolve(lib.module, "xessVKExecute", lib.execute);
        Resolve(lib.module, "xessDestroyContext", lib.destroy);
        Resolve(lib.module, "xessVKGetRequiredDeviceExtensions", lib.required_extensions);
        Resolve(lib.module, "xessSetLoggingCallback", lib.set_logging);
        Resolve(lib.module, "xessIsOptimalDriver", lib.is_optimal_driver);
        if (!lib.Ok()) {
            lib.error = "libxess.dll has no Vulkan entry points (not the XeSS SDK library?)";
        }
        return lib;
    }();
    return library;
}

void LogCallback(const char* message, xess_logging_level_t level) {
    std::printf("XeSS [%d]: %s\n", int(level), message ? message : "");
}

const char* ResultName(xess_result_t result) {
    switch (result) {
    case XESS_RESULT_ERROR_UNSUPPORTED_DEVICE: return "unsupported device";
    case XESS_RESULT_ERROR_UNSUPPORTED_DRIVER: return "unsupported driver";
    case XESS_RESULT_ERROR_UNINITIALIZED: return "not initialised";
    case XESS_RESULT_ERROR_INVALID_ARGUMENT: return "invalid argument";
    case XESS_RESULT_ERROR_DEVICE_OUT_OF_MEMORY: return "out of GPU memory";
    case XESS_RESULT_ERROR_DEVICE: return "device error";
    case XESS_RESULT_ERROR_NOT_IMPLEMENTED: return "not implemented";
    case XESS_RESULT_ERROR_INVALID_CONTEXT: return "invalid context";
    case XESS_RESULT_ERROR_UNSUPPORTED: return "unsupported configuration";
    case XESS_RESULT_ERROR_WRONG_CALL_ORDER: return "wrong call order";
    default: return "error";
    }
}

bool HasStencil(vk::Format format) {
    return format == vk::Format::eD16UnormS8Uint || format == vk::Format::eD24UnormS8Uint ||
           format == vk::Format::eD32SfloatS8Uint;
}

xess_vk_image_view_info ViewInfo(const XessAa::Image& image) {
    xess_vk_image_view_info info{};
    info.imageView = static_cast<VkImageView>(image.view);
    info.image = static_cast<VkImage>(image.image);
    info.subresourceRange = {static_cast<VkImageAspectFlags>(image.aspect), 0, 1, 0, 1};
    info.format = static_cast<VkFormat>(image.format);
    info.width = image.width;
    info.height = image.height;
    return info;
}

} // namespace

struct XessAa::Impl {
    explicit Impl(const Instance& instance_) : instance(instance_) {}
    ~Impl() {
        if (context && GetLibrary().destroy) {
            instance.GetDevice().waitIdle();
            GetLibrary().destroy(context);
        }
    }

    const Instance& instance;
    xess_context_handle_t context{};
    u32 initialised_width = 0, initialised_height = 0;
    bool fresh = true; ///< the history of the context is empty: the next frame resets it
    bool fatal = false;
    u32 failures = 0;
    std::string problem;
    bool logged_running = false;

    void Fail(const char* what, xess_result_t result, bool permanent) {
        char text[200];
        std::snprintf(text, sizeof(text), "XeSS %s: %s (%d)", what, ResultName(result),
                      int(result));
        problem = text;
        std::printf("%s\n", text);
        if (permanent) {
            fatal = true;
        }
    }

    void Barrier(vk::CommandBuffer cmdbuf, const Image& image, bool to_read) {
        vk::ImageAspectFlags aspect = image.aspect;
        if (HasStencil(image.format)) {
            aspect |= vk::ImageAspectFlagBits::eStencil; // the whole image changes layout together
        }
        const vk::ImageMemoryBarrier2 barrier{
            .srcStageMask = to_read ? vk::PipelineStageFlagBits2::eAllCommands
                                    : vk::PipelineStageFlagBits2::eComputeShader,
            .srcAccessMask = to_read ? vk::AccessFlagBits2::eMemoryRead |
                                           vk::AccessFlagBits2::eMemoryWrite
                                     : vk::AccessFlagBits2::eNone,
            .dstStageMask = to_read ? vk::PipelineStageFlagBits2::eComputeShader
                                    : vk::PipelineStageFlagBits2::eAllCommands,
            .dstAccessMask = to_read ? vk::AccessFlagBits2::eShaderSampledRead
                                     : vk::AccessFlagBits2::eMemoryRead |
                                           vk::AccessFlagBits2::eMemoryWrite,
            .oldLayout = to_read ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal,
            .newLayout = to_read ? vk::ImageLayout::eShaderReadOnlyOptimal : vk::ImageLayout::eGeneral,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image.image,
            .subresourceRange = {aspect, 0, 1, 0, 1},
        };
        cmdbuf.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier});
    }

    bool Record(const Frame& frame) {
        if (fatal) {
            return false;
        }
        const Library& lib = GetLibrary();
        if (!lib.Ok()) {
            problem = lib.error;
            fatal = true;
            std::printf("XeSS: %s\n", problem.c_str());
            return false;
        }
        if (!context) {
            const xess_result_t result =
                lib.create(static_cast<VkInstance>(instance.GetInstance()),
                           static_cast<VkPhysicalDevice>(instance.GetPhysicalDevice()),
                           static_cast<VkDevice>(instance.GetDevice()), &context);
            if (result < 0) {
                context = nullptr;
                Fail("context", result, true);
                return false;
            }
            if (lib.set_logging) {
                lib.set_logging(context, XESS_LOGGING_LEVEL_WARNING, &LogCallback);
            }
            if (lib.get_version) {
                xess_version_t version{};
                if (lib.get_version(&version) >= 0) {
                    std::printf("XeSS: library %u.%u.%u\n", version.major, version.minor,
                                version.patch);
                }
            }
            if (lib.is_optimal_driver && lib.is_optimal_driver(context) ==
                                             XESS_RESULT_WARNING_OLD_DRIVER) {
                std::printf("XeSS: the graphics driver is older than XeSS likes (it may run "
                            "slower or look worse)\n");
            }
        }
        if (initialised_width != frame.output.width || initialised_height != frame.output.height) {
            // A (re)initialisation needs the GPU idle; it happens at the start and when the
            // game's picture changes size.
            instance.GetDevice().waitIdle();
            xess_vk_init_params_t init{};
            init.outputResolution = {frame.output.width, frame.output.height};
            init.qualitySetting = XESS_QUALITY_SETTING_AA;
            // The scene colour is tonemapped (display range), not HDR.
            init.initFlags = XESS_INIT_FLAG_LDR_INPUT_COLOR;
            init.creationNodeMask = 1;
            init.visibleNodeMask = 1;
            const xess_result_t result = lib.init(context, &init);
            if (result < 0) {
                Fail("initialisation", result, true);
                return false;
            }
            initialised_width = frame.output.width;
            initialised_height = frame.output.height;
            fresh = true;
            logged_running = false;
        }

        xess_vk_execute_params_t params{};
        params.colorTexture = ViewInfo(frame.color);
        params.velocityTexture = ViewInfo(frame.motion);
        params.depthTexture = ViewInfo(frame.depth);
        params.outputTexture = ViewInfo(frame.output);
        // XeSS takes the jitter in pixels as the projection was shifted; SPARTAN_XESS_INVERT_JITTER=1
        // flips it (one switch for a test, in case a sign differs from FSR's).
        static const bool invert = [] {
            const char* value = std::getenv("SPARTAN_XESS_INVERT_JITTER");
            return value && *value == '1';
        }();
        const float sign = invert ? -1.0f : 1.0f;
        params.jitterOffsetX = sign * frame.jitter[0];
        params.jitterOffsetY = sign * frame.jitter[1];
        params.exposureScale = 1.0f;
        params.resetHistory = frame.reset || fresh ? 1u : 0u;
        params.inputWidth = frame.color.width;
        params.inputHeight = frame.color.height;

        // Resources in the layouts XeSS documents (shader read only; the output General), back
        // to General afterwards for everything else the frame does with them.
        const vk::MemoryBarrier2 output_ready{
            .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
            .srcAccessMask = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
            .dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead |
                             vk::AccessFlagBits2::eShaderStorageWrite |
                             vk::AccessFlagBits2::eShaderSampledRead,
        };
        frame.cmdbuf.pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &output_ready});
        Barrier(frame.cmdbuf, frame.color, true);
        Barrier(frame.cmdbuf, frame.depth, true);
        Barrier(frame.cmdbuf, frame.motion, true);
        const xess_result_t result =
            lib.execute(context, static_cast<VkCommandBuffer>(frame.cmdbuf), &params);
        Barrier(frame.cmdbuf, frame.color, false);
        Barrier(frame.cmdbuf, frame.depth, false);
        Barrier(frame.cmdbuf, frame.motion, false);
        if (result < 0) {
            Fail("execute", result, ++failures >= 3);
            return false;
        }
        failures = 0;
        problem.clear();
        fresh = false;
        if (!logged_running) {
            logged_running = true;
            std::printf("XeSS: running, anti-aliasing at %ux%u\n", initialised_width,
                        initialised_height);
        }
        return true;
    }
};

std::vector<const char*> XessAa::DeviceExtensions(vk::Instance instance,
                                                  vk::PhysicalDevice physical_device) {
    std::vector<const char*> names;
    const Library& lib = GetLibrary();
    if (!lib.Ok() || !lib.required_extensions) {
        return names;
    }
    uint32_t count = 0;
    const char* const* list = nullptr;
    if (lib.required_extensions(static_cast<VkInstance>(instance),
                                static_cast<VkPhysicalDevice>(physical_device), &count,
                                &list) < 0 ||
        !list) {
        return names;
    }
    names.assign(list, list + count);
    return names;
}

#else // no XeSS headers or not Windows

struct XessAa::Impl {
    explicit Impl(const Instance&) {}
    bool Record(const Frame&) {
        return false;
    }
};

std::vector<const char*> XessAa::DeviceExtensions(vk::Instance, vk::PhysicalDevice) {
    return {};
}

#endif

XessAa::XessAa(const Instance& instance) : impl(std::make_unique<Impl>(instance)) {}

XessAa::~XessAa() = default;

bool XessAa::Record(const Frame& frame) {
    return impl->Record(frame);
}

const char* XessAa::Problem() const noexcept {
#if SPARTAN_HAS_XESS
    return impl->problem.empty() ? nullptr : impl->problem.c_str();
#else
    return "XeSS is available only in the Windows build";
#endif
}

bool XessAa::Fatal() const noexcept {
#if SPARTAN_HAS_XESS
    return impl->fatal;
#else
    return true;
#endif
}

} // namespace Vulkan
