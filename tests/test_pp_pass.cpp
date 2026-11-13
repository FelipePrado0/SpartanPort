// SPDX-License-Identifier: GPL-2.0-or-later
// Runs the production post-process pass (host_passes/pp_pass.cpp, post_process.frag) on a Vulkan device
// (written for Lavapipe) with a ramp of every 8-bit code and checks the player's picture controls: the neutral
// picture comes out code for code, each control moves the codes as picture_adjust() says, black and white
// stay exact where the shader promises it, and nothing turns NaN. The push constants are filled through
// the shared mappings in spartanport_graphics.h, as vk_presenter.cpp fills them. With
// VK_LAYER_KHRONOS_validation installed it also runs under validation (SPARTAN_PP_TEST_VALIDATE=0 turns
// that off) and fails on any validation error. The exact-code checks assume round-to-nearest UNORM
// conversion, as Lavapipe does; Vulkan also allows a GPU to truncate, so on such a GPU white (encoded as
// 0.99999994) and a few dithered codes can come out one lower and fail those checks.
// A second run draws the HDR output (pp.hdr 2: the SDR picture expanded to HDR10) into an A2B10G10R10
// target and checks it against a CPU model of the whole chain, in double.
#define VMA_IMPLEMENTATION
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <vector>

#include "common/types.h"
#include "spartanport_graphics.h"
#include "video_core/renderer_vulkan/host_passes/pp_pass.h"
#include "video_core/renderer_vulkan/vk_common.h"
#include "video_core/renderer_vulkan/vk_presenter.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/host_shaders/overlay_hdr_frag.h"
#include "video_core/host_shaders/post_process_frag.h"

#include <vk_mem_alloc.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

// The pass asserts and logs through the port's own helpers; this test links neither.
void assert_fail_impl() {
    std::abort();
}
void unreachable_impl() {
    std::abort();
}

static int validation_errors = 0;
static VKAPI_ATTR vk::Bool32 VKAPI_CALL OnMessage(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                                  vk::DebugUtilsMessageTypeFlagsEXT,
                                                  const vk::DebugUtilsMessengerCallbackDataEXT* data, void*) {
    if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
        ++validation_errors;
        std::fprintf(stderr, "VALIDATION ERROR: %s\n", data->pMessage);
    }
    return VK_FALSE;
}

namespace {

using namespace Vulkan;
using Settings = HostPasses::PostProcessingPass::Settings;

// One column per 8-bit code. Several rows, so the dither (which depends on the pixel position) is seen
// at many positions for each code.
constexpr u32 W = 256, H = 16;
constexpr vk::Format OutputFormat = vk::Format::eR8G8B8A8Unorm;
// The HDR10 swapchain's format (vk_swapchain.h, SURFACE_FORMAT_HDR).
constexpr vk::Format HdrFormat = vk::Format::eA2B10G10R10UnormPack32;

struct Gpu {
    vk::detail::DynamicLoader loader;
    vk::UniqueInstance instance;
    vk::UniqueDebugUtilsMessengerEXT messenger;
    vk::PhysicalDevice physical;
    vk::UniqueDevice device;
    vk::Queue queue;
    u32 family = 0;
    VmaAllocator allocator{};
    vk::UniqueCommandPool pool;
};

void Init(Gpu& g) {
    VULKAN_HPP_DEFAULT_DISPATCHER.init(g.loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));
    const char* validate = std::getenv("SPARTAN_PP_TEST_VALIDATE");
    const bool want_validation = !validate || std::string_view(validate) != "0";
    std::vector<const char*> layers;
    if (want_validation) {
        for (const auto& layer : vk::enumerateInstanceLayerProperties().value) {
            if (std::string_view(layer.layerName) == "VK_LAYER_KHRONOS_validation") {
                layers.push_back("VK_LAYER_KHRONOS_validation");
            }
        }
    }
    // Debug utils: the pass names its shader modules (SetObjectName) and the messenger needs it.
    const std::array extensions{VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
    const vk::ApplicationInfo app{.apiVersion = VK_API_VERSION_1_3};
    g.instance = vk::createInstanceUnique({.pApplicationInfo = &app,
                                           .enabledLayerCount = u32(layers.size()),
                                           .ppEnabledLayerNames = layers.data(),
                                           .enabledExtensionCount = extensions.size(),
                                           .ppEnabledExtensionNames = extensions.data()}).value;
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*g.instance);
    std::printf("validation layer: %s\n", layers.empty() ? "not available" : "on");
    if (!layers.empty()) {
        const vk::DebugUtilsMessengerCreateInfoEXT messenger_ci{
            .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                               vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
            .pfnUserCallback = OnMessage};
        g.messenger = g.instance->createDebugUtilsMessengerEXTUnique(messenger_ci).value;
    }
    g.physical = g.instance->enumeratePhysicalDevices().value.front();
    std::printf("device: %s\n", g.physical.getProperties().deviceName.data());
    const auto families = g.physical.getQueueFamilyProperties();
    bool found = false;
    for (u32 i = 0; i < families.size(); ++i) {
        if (families[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            g.family = i;
            found = true;
            break;
        }
    }
    assert(found && "the pass draws: it needs a graphics queue");
    const float priority = 1.0f;
    const vk::DeviceQueueCreateInfo queue_ci{.queueFamilyIndex = g.family, .queueCount = 1, .pQueuePriorities = &priority};
    const std::array device_extensions{VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME};
    // The pass renders with dynamic rendering and records its barrier with synchronization2.
    vk::PhysicalDeviceVulkan13Features features13{.synchronization2 = true, .dynamicRendering = true};
    g.device = g.physical.createDeviceUnique({.pNext = &features13,
                                              .queueCreateInfoCount = 1,
                                              .pQueueCreateInfos = &queue_ci,
                                              .enabledExtensionCount = device_extensions.size(),
                                              .ppEnabledExtensionNames = device_extensions.data()}).value;
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*g.device);
    g.queue = g.device->getQueue(g.family, 0);
    const VmaVulkanFunctions functions{
        .vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr,
    };
    const VmaAllocatorCreateInfo allocator_ci{
        .physicalDevice = g.physical,
        .device = *g.device,
        .pVulkanFunctions = &functions,
        .instance = *g.instance,
        .vulkanApiVersion = VK_API_VERSION_1_3,
    };
    const VkResult allocator_result = vmaCreateAllocator(&allocator_ci, &g.allocator);
    assert(allocator_result == VK_SUCCESS);
    (void)allocator_result;
    g.pool = g.device->createCommandPoolUnique({.queueFamilyIndex = g.family}).value;
}

struct Buffer {
    VkBuffer buffer{};
    VmaAllocation allocation{};
    void* mapped{};
};
Buffer MakeBuffer(Gpu& g, size_t size, VkBufferUsageFlags usage) {
    Buffer b;
    const VkBufferCreateInfo ci{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage};
    const VmaAllocationCreateInfo ac{.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                                     .usage = VMA_MEMORY_USAGE_AUTO};
    VmaAllocationInfo info{};
    const VkResult result = vmaCreateBuffer(g.allocator, &ci, &ac, &b.buffer, &b.allocation, &info);
    assert(result == VK_SUCCESS);
    (void)result;
    b.mapped = info.pMappedData;
    return b;
}

struct Image {
    VkImage image{};
    VmaAllocation allocation{};
    vk::UniqueImageView view;
};
Image MakeImage(Gpu& g, vk::Format format, vk::ImageUsageFlags usage) {
    Image img;
    const VkImageCreateInfo ci = static_cast<VkImageCreateInfo>(vk::ImageCreateInfo{
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {W, H, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .usage = usage,
        .initialLayout = vk::ImageLayout::eUndefined});
    const VmaAllocationCreateInfo ac{.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
    const VkResult result = vmaCreateImage(g.allocator, &ci, &ac, &img.image, &img.allocation, nullptr);
    assert(result == VK_SUCCESS);
    (void)result;
    img.view = g.device->createImageViewUnique({.image = img.image,
                                                .viewType = vk::ImageViewType::e2D,
                                                .format = format,
                                                .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}}).value;
    return img;
}

constexpr vk::ImageSubresourceRange ColorRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

template <typename Record>
void Submit(Gpu& g, Record record) {
    vk::UniqueFence fence = g.device->createFenceUnique({}).value;
    auto cmds = g.device->allocateCommandBuffersUnique({.commandPool = *g.pool, .commandBufferCount = 1}).value;
    const vk::CommandBuffer cmd = *cmds[0];
    (void)cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    record(cmd);
    (void)cmd.end();
    const vk::CommandBufferSubmitInfo cb{.commandBuffer = cmd};
    (void)g.queue.submit2(vk::SubmitInfo2{.commandBufferInfoCount = 1, .pCommandBufferInfos = &cb}, *fence);
    const vk::Result waited = g.device->waitForFences(*fence, true, 10'000'000'000ull);
    assert(waited == vk::Result::eSuccess);
    (void)waited;
}

// The game's finished picture: an sRGB 8-bit image whose column x holds code x in every channel,
// sampled through an sRGB view as the presenter samples the game's sRGB buffers (srgb_input off). The
// sampler decodes code x to linear light and the shader's gamma() encodes it again, so with the
// controls neutral the output should be code x. The output is as wide as the input, so every fragment
// samples one texel at its centre and the linear filter mixes in nothing from the neighbours.
// The HDR run's ramp (orange_half) holds orange, code x in red and x / 2 in green, in its lower half and
// alpha 128 everywhere, so the colour matrix, the alpha the pass writes and whether one factor scales all
// three channels of a pixel (the hue) are seen too.
Image MakeRamp(Gpu& g, bool orange_half = false) {
    Image in = MakeImage(g, vk::Format::eR8G8B8A8Srgb,
                         vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
    Buffer staging = MakeBuffer(g, size_t(W) * H * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    auto* bytes = static_cast<u8*>(staging.mapped);
    for (u32 y = 0; y < H; ++y) {
        for (u32 x = 0; x < W; ++x) {
            u8* px = bytes + (size_t(y) * W + x) * 4;
            const bool orange = orange_half && y >= H / 2;
            px[0] = u8(x);
            px[1] = orange ? u8(x / 2) : u8(x);
            px[2] = orange ? 0 : u8(x);
            px[3] = orange_half ? 128 : 255;
        }
    }
    vmaFlushAllocation(g.allocator, staging.allocation, 0, VK_WHOLE_SIZE);
    Submit(g, [&](vk::CommandBuffer cmd) {
        const vk::ImageMemoryBarrier2 to_dst{.srcStageMask = vk::PipelineStageFlagBits2::eNone,
                                             .dstStageMask = vk::PipelineStageFlagBits2::eCopy,
                                             .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
                                             .oldLayout = vk::ImageLayout::eUndefined,
                                             .newLayout = vk::ImageLayout::eTransferDstOptimal,
                                             .image = in.image,
                                             .subresourceRange = ColorRange};
        cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &to_dst});
        const vk::BufferImageCopy copy{.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                                       .imageExtent = {W, H, 1}};
        cmd.copyBufferToImage(staging.buffer, in.image, vk::ImageLayout::eTransferDstOptimal, copy);
        const vk::ImageMemoryBarrier2 to_read{.srcStageMask = vk::PipelineStageFlagBits2::eCopy,
                                              .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                                              .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
                                              .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                                              .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                                              .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                                              .image = in.image,
                                              .subresourceRange = ColorRange};
        cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &to_read});
    });
    vmaDestroyBuffer(g.allocator, staging.buffer, staging.allocation);
    return in;
}

// The swapchain stand-in the pass draws into, as a presenter Frame.
struct Target {
    Image image;
    Frame frame{};
};
void MakeTarget(Gpu& g, Target& t, vk::Format format = OutputFormat) {
    t.image = MakeImage(g, format,
                        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);
    t.frame.width = W;
    t.frame.height = H;
    t.frame.allocation = t.image.allocation;
    t.frame.image = t.image.image;
    t.frame.image_view = *t.image.view;
    t.frame.format = format;
}

struct Output {
    std::vector<u8> code; // red channel, W * H
    bool grey = true;     // green and blue equal red everywhere
    bool alpha_one = true;
};

// Draws the pass into the target and returns its pixels, 32 bits each.
std::vector<u32> DrawPass(Gpu& g, HostPasses::PostProcessingPass& pass, const Image& in, Target& target,
                          const Settings& settings) {
    Buffer readback = MakeBuffer(g, size_t(W) * H * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    Submit(g, [&](vk::CommandBuffer cmd) {
        // The presenter hands the pass a frame in the color attachment layout; the pass leaves it General.
        const vk::ImageMemoryBarrier2 to_attachment{.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                                                    .srcAccessMask = vk::AccessFlagBits2::eMemoryRead,
                                                    .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                    .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
                                                    .oldLayout = vk::ImageLayout::eUndefined,
                                                    .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                    .image = target.frame.image,
                                                    .subresourceRange = ColorRange};
        cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &to_attachment});
        pass.Render(cmd, *in.view, {W, H}, target.frame, settings);
        const vk::ImageMemoryBarrier2 to_src{.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                                             .srcAccessMask = vk::AccessFlagBits2::eMemoryWrite,
                                             .dstStageMask = vk::PipelineStageFlagBits2::eCopy,
                                             .dstAccessMask = vk::AccessFlagBits2::eTransferRead,
                                             .oldLayout = vk::ImageLayout::eGeneral,
                                             .newLayout = vk::ImageLayout::eTransferSrcOptimal,
                                             .image = target.frame.image,
                                             .subresourceRange = ColorRange};
        cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &to_src});
        const vk::BufferImageCopy copy{.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                                       .imageExtent = {W, H, 1}};
        cmd.copyImageToBuffer(target.frame.image, vk::ImageLayout::eTransferSrcOptimal, readback.buffer, copy);
        const vk::MemoryBarrier2 host{.srcStageMask = vk::PipelineStageFlagBits2::eCopy,
                                      .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                                      .dstStageMask = vk::PipelineStageFlagBits2::eHost,
                                      .dstAccessMask = vk::AccessFlagBits2::eHostRead};
        cmd.pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &host});
    });
    vmaInvalidateAllocation(g.allocator, readback.allocation, 0, VK_WHOLE_SIZE);
    std::vector<u32> pixels(size_t(W) * H);
    std::memcpy(pixels.data(), readback.mapped, pixels.size() * 4);
    vmaDestroyBuffer(g.allocator, readback.buffer, readback.allocation);
    return pixels;
}

Output Run(Gpu& g, HostPasses::PostProcessingPass& pass, const Image& in, Target& target, const Settings& settings) {
    const std::vector<u32> pixels = DrawPass(g, pass, in, target, settings);
    Output out;
    out.code.resize(size_t(W) * H);
    for (size_t i = 0; i < out.code.size(); ++i) {
        u8 px[4];
        std::memcpy(px, &pixels[i], 4);
        out.code[i] = px[0];
        out.grey = out.grey && px[1] == px[0] && px[2] == px[0];
        out.alpha_one = out.alpha_one && px[3] == 255;
    }
    return out;
}

// The player's controls in the units of the ini and the menu, and whether the shader runs them even
// when they are neutral (the presenter never does that; the test does it to see the dither alone).
struct Picture {
    int brightness = SpartanGraphics::PictureBrightnessNeutral;
    int contrast = SpartanGraphics::PictureContrastNeutral;
    int gamma = SpartanGraphics::PictureGammaNeutral;
    bool force_adjust = false;
};

// The same fill as vk_presenter.cpp.
Settings Fill(const Picture& p) {
    Settings s{};
    s.picture_power = SpartanGraphics::PicturePower(p.gamma);
    s.picture_contrast = SpartanGraphics::PictureContrast(p.contrast);
    s.picture_brightness = SpartanGraphics::PictureBrightness(p.brightness);
    s.picture_adjust = p.force_adjust || !SpartanGraphics::PictureNeutral(p.brightness, p.contrast, p.gamma);
    return s;
}

// picture_adjust() on the CPU, in double, for an input code: the output value in [0, 1] before the dither.
double Model(const Picture& p, double code) {
    const double power = SpartanGraphics::PicturePower(p.gamma);
    const double contrast = SpartanGraphics::PictureContrast(p.contrast);
    const double lift = SpartanGraphics::PictureBrightness(p.brightness);
    double v = std::pow(std::clamp(code / 255.0, 0.0, 1.0), power);
    v = std::clamp((v - 0.5) * contrast + 0.5, 0.0, 1.0);
    v = lift >= 0.0 ? v * (1.0 - lift) + lift : std::max(v + lift, 0.0) / (1.0 + lift);
    return std::clamp(v, 0.0, 1.0);
}

// The shader's dither noise for the pixel at (x, y), in codes: pcg2d of the pixel position, two
// uniforms summed into a triangular noise from -1 to +1. Same uint arithmetic as post_process.frag.
double DitherNoise(u32 x, u32 y) {
    u32 vx = x * 1664525u + 1013904223u, vy = y * 1664525u + 1013904223u;
    vx += vy * 1664525u; vy += vx * 1664525u;
    vx ^= vx >> 16u; vy ^= vy >> 16u;
    vx += vy * 1664525u; vy += vx * 1664525u;
    vx ^= vx >> 16u; vy ^= vy >> 16u;
    const float nx = float(vx) * (1.0f / 4294967296.0f), ny = float(vy) * (1.0f / 4294967296.0f);
    return double(nx + ny - 1.0f);
}

int failures = 0;
void Expect(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    failures += !ok;
}

u8 At(const Output& o, u32 code, u32 row) {
    return o.code[size_t(row) * W + code];
}

// Every pixel of every column x satisfies `ok(x, out)`; returns how many do not.
template <typename Pred>
int CountBad(const Output& o, Pred ok) {
    int bad = 0;
    for (u32 y = 0; y < H; ++y) {
        for (u32 x = 0; x < W; ++x) bad += !ok(int(x), int(At(o, x, y)));
    }
    return bad;
}

double ColumnMean(const Output& o, u32 code) {
    double sum = 0;
    for (u32 y = 0; y < H; ++y) sum += At(o, code, y);
    return sum / H;
}

// Checks shared by every setting: a grey opaque picture, black and white where the model puts them,
// every pixel within one dither step of the model, and no NaN-derived collapse of the mid ramp.
void CheckCommon(const char* name, const Picture& p, const Output& o) {
    char text[200];
    std::snprintf(text, sizeof text, "%s: grey and opaque", name);
    Expect(o.grey && o.alpha_one, text);
    // The encoded value reaching the controls is not exactly code / 255: the sampler's sRGB decode
    // and gamma()'s pow() leave it up to a fraction of a code off (Lavapipe: about 0.35; the neutral
    // run shows it stays under half a code, since the undithered output comes back exact). So for
    // each column the test finds the values before the dither that explain all its rows at once,
    // given each pixel's own noise and the 8-bit rounding, and requires one of them to lie where
    // picture_adjust() puts the codes x - 0.5 to x + 0.5. Without the exact dither, rows with
    // different noise but the same output leave no common value, so this also proves the dither.
    const bool dithered = Fill(p).picture_adjust != 0;
    constexpr double slack = 0.02; // the GPU's float pow against the CPU's double
    int off = 0, worst_code = -1;
    double worst = 0;
    for (u32 x = 0; x < W; ++x) {
        const double lo = Model(p, x - 0.5) * 255.0 - slack, hi = Model(p, x + 0.5) * 255.0 + slack;
        // The values u that explain every row at once, within [from, to): each output code owns the
        // half-open bin [out - 0.5, out + 0.5) of u + noise (0 and 255 also own what the UNORM write
        // clamps), so two rows with the same value cannot round apart. Returns 0 when a value of
        // positive width remains, else by how much the rows miss each other.
        auto fit = [&](double from, double to, bool with_noise) {
            double a = std::max(lo, from), b = std::min(hi, to);
            for (u32 y = 0; y < H; ++y) {
                const double out = At(o, x, y), n = with_noise ? DitherNoise(x, y) : 0.0;
                a = std::max(a, (out == 0 ? -1e9 : out - 0.5) - n);
                b = std::min(b, (out == 255 ? 1e9 : out + 0.5) - n);
            }
            return a < b ? 0.0 : a - b + 1e-9;
        };
        // The shader adds the noise only to values strictly between half a step from black and from
        // white (dither_8bit()), so each branch keeps to its own values.
        double gap = 0.0;
        if (!dithered) {
            gap = fit(0.0, 255.0, false);
        } else {
            gap = std::min({fit(0.5, 254.5, true), fit(0.0, 0.5, false), fit(254.5, 255.0, false)});
        }
        off += gap > 0.0;
        if (gap > worst) worst = gap, worst_code = int(x);
    }
    std::snprintf(text, sizeof text, "%s: every column is picture_adjust() plus each pixel's dither, rounded (%d columns off, worst by %.3f at code %d)",
                  name, off, worst, worst_code);
    Expect(off == 0, text);
    int mid_zero = 0;
    for (u32 y = 0; y < H; ++y) {
        for (u32 x = 1; x <= 254; ++x) mid_zero += At(o, x, y) == 0;
    }
    std::snprintf(text, sizeof text, "%s: codes 1..254 are not all black (no NaN)", name);
    Expect(mid_zero < int(254 * H), text);
}

// Output code of input code x is 0 (or 255) at every pixel.
bool ColumnIs(const Output& o, u32 x, u8 value) {
    for (u32 y = 0; y < H; ++y) {
        if (At(o, x, y) != value) return false;
    }
    return true;
}

// White (input 255) must come out 255 at every pixel: the dither skips exact white. Says how many
// pixels of the column did not, and the lowest code among them.
void ExpectWhite(const char* name, const Output& o) {
    int off = 0, lowest = 255;
    for (u32 y = 0; y < H; ++y) {
        off += At(o, 255, y) != 255;
        lowest = std::min(lowest, int(At(o, 255, y)));
    }
    char text[200];
    std::snprintf(text, sizeof text, "%s: 255 stays 255 (%d of %u pixels off, lowest %d)", name, off, H, lowest);
    Expect(off == 0, text);
}

// HDR output (pp.hdr 2): the SDR picture expanded to HDR10, in the units the presenter fills.
struct Hdr {
    float paper_white = 200.0f; // nits of the SDR picture's white
};

Settings FillHdr(const Picture& p, const Hdr& h) {
    Settings s = Fill(p);
    s.hdr = 2;
    s.hdr_paper_white = h.paper_white;
    return s;
}

// SMPTE ST 2084: nits to the PQ value in [0, 1].
double Pq(double nits) {
    const double m1 = 0.1593017578125, m2 = 78.84375, c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
    const double y = std::pow(std::clamp(nits / 10000.0, 0.0, 1.0), m1);
    return std::pow((c1 + c2 * y) / (1.0 + c3 * y), m2);
}

constexpr double HighlightGain = 1.5; // post_process.frag: SDR white is shown at 1.5 x paper white

// The shader's mode-2 chain for an input pixel of 8-bit codes, in double: the encoded value (code / 255,
// as the neutral SDR run shows), picture_adjust() when the controls are on (Model), the 2.2 power
// decode, the highlight boost (one factor from the largest channel), paper white, the BT.2087
// Rec.709-to-Rec.2020 matrix and PQ. Returns the three PQ values.
std::array<double, 3> HdrModel(const Picture& p, const Hdr& h, const std::array<double, 3>& code) {
    std::array<double, 3> lin{};
    for (int c = 0; c < 3; ++c) lin[c] = std::pow(Model(p, code[c]), 2.2);
    const double peak = std::max({lin[0], lin[1], lin[2]});
    const double t = std::clamp((peak - 0.6) / 0.4, 0.0, 1.0), smooth = t * t * (3.0 - 2.0 * t);
    const double scale = (1.0 + (HighlightGain - 1.0) * smooth * smooth) * h.paper_white;
    const double r = lin[0] * scale, g = lin[1] * scale, b = lin[2] * scale;
    return {Pq(0.6274 * r + 0.3293 * g + 0.0433 * b), Pq(0.0691 * r + 0.9195 * g + 0.0114 * b),
            Pq(0.0164 * r + 0.0880 * g + 0.8956 * b)};
}

// One channel (0 red, 1 green, 2 blue) or alpha (3) of an A2B10G10R10 pixel.
int Channel(u32 pixel, int c) {
    return c == 3 ? int(pixel >> 30) : int((pixel >> (10 * c)) & 0x3FF);
}
u32 PixelAt(const std::vector<u32>& o, u32 x, u32 y) {
    return o[size_t(y) * W + x];
}
// The ramp's input codes at (x, y): grey in the upper half, orange in the lower (MakeRamp(g, true)).
std::array<double, 3> InputAt(u32 x, u32 y) {
    return y >= H / 2 ? std::array<double, 3>{double(x), double(x / 2), 0.0}
                      : std::array<double, 3>{double(x), double(x), double(x)};
}
// The 10-bit code the model gives for channel c of pixel (x, y), with the input moved by `shift` codes.
int HdrModelCode(const Picture& p, const Hdr& h, u32 x, u32 y, int c, double shift) {
    std::array<double, 3> in = InputAt(x, y);
    for (double& v : in) v += shift;
    return int(std::lround(HdrModel(p, h, in)[c] * 1023.0));
}

// Checks shared by every HDR run: alpha 1, no dither (each half's rows equal), and every channel of
// every pixel where the model puts it. Tolerance: the model is taken with the input half a code either
// side (the encoded value reaching the expansion is a fraction of a code off code / 255, see
// CheckCommon) and one 10-bit code is allowed beyond that for the GPU's float maths.
void CheckHdr(const char* name, const Picture& p, const Hdr& h, const std::vector<u32>& o) {
    char text[240];
    int not_opaque = 0, row_diff = 0, off = 0, worst = 0, worst_x = -1, worst_c = -1;
    for (u32 y = 0; y < H; ++y) {
        for (u32 x = 0; x < W; ++x) {
            const u32 px = PixelAt(o, x, y);
            not_opaque += Channel(px, 3) != 3;
            row_diff += px != PixelAt(o, x, y < H / 2 ? 0 : H / 2);
            for (int c = 0; c < 3; ++c) {
                const int lo = HdrModelCode(p, h, x, y, c, -0.5) - 1, hi = HdrModelCode(p, h, x, y, c, 0.5) + 1;
                const int out = Channel(px, c);
                const int miss = out < lo ? lo - out : out > hi ? out - hi : 0;
                off += miss > 0;
                if (miss > worst) worst = miss, worst_x = int(x), worst_c = c;
            }
        }
    }
    std::snprintf(text, sizeof text, "%s: alpha is 1 (%d pixels not)", name, not_opaque);
    Expect(not_opaque == 0, text);
    std::snprintf(text, sizeof text, "%s: no dither, each column the same in every row of its half (%d pixels differ)",
                  name, row_diff);
    Expect(row_diff == 0, text);
    std::snprintf(text, sizeof text,
                  "%s: every channel within the model +-(half an input code + one 10-bit code) (%d off, worst by %d at code %d channel %d)",
                  name, off, worst, worst_x, worst_c);
    Expect(off == 0, text);
}

// The red channel of the grey half at input code x.
int GreyCode(const std::vector<u32>& o, u32 x) {
    return Channel(PixelAt(o, x, 0), 0);
}

} // namespace

int main() {
    Gpu g;
    Init(g);
    {
        HostPasses::PostProcessingPass pass;
        pass.Create(*g.device, OutputFormat);
        Image ramp = MakeRamp(g);
        Target target;
        MakeTarget(g, target);
        char text[200];

        // a. Neutral: the shader skips the controls; the sRGB decode in the sampler and the encode in
        // gamma() give every code back. Both are float maths, so the value before the 8-bit write is
        // a fraction of a code off code / 255 (Lavapipe: up to about 0.35), and the write rounds it.
        const Picture neutral{};
        const Output base = Run(g, pass, ramp, target, Fill(neutral));
        {
            Expect(Fill(neutral).picture_adjust == 0, "neutral: the presenter's fill turns the controls off");
            int differ = 0, worst = 0;
            CountBad(base, [&](int x, int out) {
                differ += out != x;
                worst = std::max(worst, std::abs(out - x));
                return true;
            });
            // On Lavapipe every code comes back exactly. CheckCommon relies on that: it bounds how far
            // the encoded value can sit from code / 255 by half a code.
            std::snprintf(text, sizeof text, "neutral: every code comes back exactly (%d pixels off, worst %d)", differ, worst);
            Expect(worst == 0, text);
            Expect(ColumnIs(base, 0, 0) && ColumnIs(base, 255, 255), "neutral: 0 and 255 exact");
            CheckCommon("neutral", neutral, base);
        }

        // b. Neutral values with the controls forced on: only the dither changes anything.
        {
            const Picture p{.force_adjust = true};
            const Output o = Run(g, pass, ramp, target, Fill(p));
            int worst = 0, changed = 0, interior_changed = 0, interior = 0, signed_sum = 0;
            for (u32 y = 0; y < H; ++y) {
                for (u32 x = 0; x < W; ++x) {
                    const int d = int(At(o, x, y)) - int(At(base, x, y));
                    worst = std::max(worst, std::abs(d));
                    changed += d != 0;
                    if (x > 0 && x < W - 1) {
                        ++interior;
                        interior_changed += d != 0;
                        signed_sum += d;
                    }
                }
            }
            std::snprintf(text, sizeof text, "neutral, controls on: at most one code of dither (%d pixels moved, worst %d)",
                          changed, worst);
            Expect(worst <= 1, text);
            // The dither exists and has its size: a triangular noise from -1 to +1 step moves a value on a
            // code by one step when |noise| > 0.5, 25% of pixels, and a value off the code a little more
            // often (Lavapipe: 29%), about as often up as down. CheckCommon below checks every pixel.
            const double moved = double(interior_changed) / interior, mean = double(signed_sum) / interior;
            std::snprintf(text, sizeof text,
                          "neutral, controls on: the dither moves %.1f%% of interior pixels (22 to 35%% expected), mean %+.3f code",
                          100.0 * moved, mean);
            Expect(moved > 0.22 && moved < 0.35 && std::abs(mean) < 0.05, text);
            Expect(ColumnIs(o, 0, 0), "neutral, controls on: 0 stays 0");
            ExpectWhite("neutral, controls on", o);
            CheckCommon("neutral, controls on", p, o);
        }

        // c. Brightness +50: black lifts to 12.5% of the range, white stays.
        {
            const Picture p{.brightness = 50};
            const Output o = Run(g, pass, ramp, target, Fill(p));
            const int lifted = int(std::lround(0.125 * 255.0));
            const int low = CountBad(o, [&](int x, int out) { return x != 0 || std::abs(out - lifted) <= 1; });
            std::snprintf(text, sizeof text, "brightness +50: code 0 -> %d +-1 (column mean %.2f, %d off)", lifted,
                          ColumnMean(o, 0), low);
            Expect(low == 0, text);
            ExpectWhite("brightness +50", o);
            // Each pixel carries its own dither, so monotony is checked on the column means.
            double drop = 0;
            for (u32 x = 1; x < W; ++x) drop = std::max(drop, ColumnMean(o, x - 1) - ColumnMean(o, x));
            std::snprintf(text, sizeof text, "brightness +50: non-decreasing within a code (largest drop %.2f)", drop);
            Expect(drop <= 1.0, text);
            CheckCommon("brightness +50", p, o);
        }

        // d. Brightness -50: the darkest 12.5% go to black, exactly (zero gets no dither), white stays.
        {
            const Picture p{.brightness = -50};
            const Output o = Run(g, pass, ramp, target, Fill(p));
            bool crushed = true;
            for (u32 x = 0; x <= 31; ++x) crushed = crushed && ColumnIs(o, x, 0);
            Expect(crushed, "brightness -50: codes 0..31 are exactly 0");
            ExpectWhite("brightness -50", o);
            CheckCommon("brightness -50", p, o);
        }

        // e. Gamma 2.80 and 1.80: the ends stay, mid grey moves as pow() says.
        const double base_mid = ColumnMean(base, 128);
        for (const int gamma : {280, 180}) {
            const Picture p{.gamma = gamma};
            const Output o = Run(g, pass, ramp, target, Fill(p));
            char name[40];
            std::snprintf(name, sizeof name, "gamma %.2f", gamma / 100.0);
            std::snprintf(text, sizeof text, "%s: 0 stays 0", name);
            Expect(ColumnIs(o, 0, 0), text);
            ExpectWhite(name, o);
            const double expected = 255.0 * std::pow(128.0 / 255.0, SpartanGraphics::PicturePower(gamma));
            const int far = CountBad(o, [&](int x, int out) { return x != 128 || std::fabs(out - expected) <= 2.0; });
            const double mid = ColumnMean(o, 128);
            const bool direction = gamma > SpartanGraphics::PictureGammaNeutral ? mid < base_mid : mid > base_mid;
            std::snprintf(text, sizeof text, "%s: code 128 -> %.2f (expected %.2f +-2, neutral %.2f, %d off)", name, mid,
                          expected, base_mid, far);
            Expect(far == 0 && direction, text);
            CheckCommon(name, p, o);
        }

        // f. Contrast 150%: mid grey stays, the ends clip where (v - 0.5) * 1.5 + 0.5 leaves [0, 1].
        {
            const Picture p{.contrast = 150};
            const Output o = Run(g, pass, ramp, target, Fill(p));
            const double contrast = SpartanGraphics::PictureContrast(p.contrast);
            // Codes clearly past the clip points (by more than float error) must be exact.
            int black_to = -1, white_from = 256;
            for (int x = 0; x < 256; ++x) {
                const double v = (x / 255.0 - 0.5) * contrast + 0.5;
                if (v < -1e-4) black_to = x;
                if (v > 1.0 + 1e-4 && white_from == 256) white_from = x;
            }
            bool clipped = true;
            for (int x = 0; x <= black_to; ++x) clipped = clipped && ColumnIs(o, x, 0);
            for (int x = white_from; x < 256; ++x) clipped = clipped && ColumnIs(o, x, 255);
            std::snprintf(text, sizeof text, "contrast 150%%: codes 0..%d are exactly 0 and %d..255 exactly 255", black_to,
                          white_from);
            Expect(clipped, text);
            const int mid_off = CountBad(o, [&](int x, int out) { return x != 128 || std::abs(out - 128) <= 1; });
            std::snprintf(text, sizeof text, "contrast 150%%: code 128 stays within 1 of 128 (mean %.2f, %d off)",
                          ColumnMean(o, 128), mid_off);
            Expect(mid_off == 0, text);
            CheckCommon("contrast 150%", p, o);
        }

        (void)g.device->waitIdle();
        target.image.view.reset();
        ramp.view.reset();
        vmaDestroyImage(g.allocator, target.image.image, target.image.allocation);
        vmaDestroyImage(g.allocator, ramp.image, ramp.allocation);
    }
    {
        // HDR output: the same pass built for the HDR10 swapchain format, drawing the game's SDR picture
        // expanded to HDR10 (pp.hdr 2).
        HostPasses::PostProcessingPass pass;
        pass.Create(*g.device, HdrFormat);
        Image ramp = MakeRamp(g, true);
        Target target;
        MakeTarget(g, target, HdrFormat);
        char text[240];

        // g. Defaults: black is PQ 0, white is PQ(paper white x gain), mid grey where the model says.
        const Picture neutral{};
        const Hdr defaults{};
        const std::vector<u32> base = DrawPass(g, pass, ramp, target, FillHdr(neutral, defaults));
        {
            bool black = true;
            for (u32 y = 0; y < H; ++y) {
                for (int c = 0; c < 3; ++c) black = black && Channel(PixelAt(base, 0, y), c) == 0;
            }
            Expect(black, "hdr: code 0 is PQ 0 in every channel");
            const int white = int(std::lround(Pq(defaults.paper_white * HighlightGain) * 1023.0));
            std::snprintf(text, sizeof text, "hdr: white is PQ(%.0f x %.1f nits) = %d +-1 (got %d)", defaults.paper_white,
                          HighlightGain, white, GreyCode(base, 255));
            Expect(std::abs(GreyCode(base, 255) - white) <= 1, text);
            const int mid = HdrModelCode(neutral, defaults, 128, 0, 0, 0.0);
            std::snprintf(text, sizeof text, "hdr: code 128 is %d +-2, the model's (got %d)", mid, GreyCode(base, 128));
            Expect(std::abs(GreyCode(base, 128) - mid) <= 2, text);
            CheckHdr("hdr", neutral, defaults, base);
        }

        // h. Paper white 80 and 500 nits: white follows, and the whole ramp with it.
        for (const float paper : {80.0f, 500.0f}) {
            const Hdr h{.paper_white = paper};
            const std::vector<u32> o = DrawPass(g, pass, ramp, target, FillHdr(neutral, h));
            char name[40];
            std::snprintf(name, sizeof name, "hdr paper white %.0f", paper);
            const int white = int(std::lround(Pq(paper * HighlightGain) * 1023.0));
            std::snprintf(text, sizeof text, "%s: white is %d +-1 (got %d)", name, white, GreyCode(o, 255));
            Expect(std::abs(GreyCode(o, 255) - white) <= 1, text);
            CheckHdr(name, neutral, h, o);
        }

        // i. The picture controls still act, before the expansion: gamma 2.40 darkens mid grey, brightness
        // +50 lifts black, white stays where it was.
        {
            const Picture p{.gamma = 240};
            const std::vector<u32> o = DrawPass(g, pass, ramp, target, FillHdr(p, defaults));
            std::snprintf(text, sizeof text, "hdr gamma 2.40: code 128 darker than at 2.20 (%d < %d), white the same",
                          GreyCode(o, 128), GreyCode(base, 128));
            Expect(GreyCode(o, 128) < GreyCode(base, 128) && GreyCode(o, 255) == GreyCode(base, 255), text);
            CheckHdr("hdr gamma 2.40", p, defaults, o);
        }
        {
            const Picture p{.brightness = 50};
            const std::vector<u32> o = DrawPass(g, pass, ramp, target, FillHdr(p, defaults));
            const int black = HdrModelCode(p, defaults, 0, 0, 0, 0.0);
            std::snprintf(text, sizeof text, "hdr brightness +50: code 0 lifts to %d +-1 (got %d), white the same", black,
                          GreyCode(o, 0));
            Expect(black > 0 && std::abs(GreyCode(o, 0) - black) <= 1 && GreyCode(o, 255) == GreyCode(base, 255), text);
            CheckHdr("hdr brightness +50", p, defaults, o);
        }

        (void)g.device->waitIdle();
        target.image.view.reset();
        ramp.view.reset();
        vmaDestroyImage(g.allocator, target.image.image, target.image.allocation);
        vmaDestroyImage(g.allocator, ramp.image, ramp.allocation);
    }
    {
        // j. The menu's white on the HDR10 swapchain (spartanport_overlay.cpp): the paper white is written
        // into overlay_hdr.frag's menu_white_nits (constant_id 0), and the patched code still makes a
        // module. Code without that constant is left alone.
        const auto bits = [](float f) {
            u32 w;
            std::memcpy(&w, &f, sizeof w);
            return w;
        };
        const auto count = [](const std::vector<u32>& code, u32 word) {
            return int(std::count(code.begin(), code.end(), word));
        };
        std::vector<u32> overlay(std::begin(OVERLAY_HDR_FRAG), std::end(OVERLAY_HDR_FRAG));
        const int at_200 = count(overlay, bits(200.0f)), at_340 = count(overlay, bits(340.0f));
        const bool patched = Vulkan::SetSpecConstantDefault(overlay, 0, 340.0f);
        Expect(patched && count(overlay, bits(200.0f)) == at_200 - 1 && count(overlay, bits(340.0f)) == at_340 + 1,
               "overlay: the menu's white (constant_id 0) becomes 340 nits, one word changed");
        const vk::ShaderModule module = Vulkan::CompileSPV(overlay, *g.device);
        Expect(bool(module), "overlay: the patched shader makes a module");
        g.device->destroyShaderModule(module);
        std::vector<u32> post(std::begin(POST_PROCESS_FRAG), std::end(POST_PROCESS_FRAG));
        const std::vector<u32> post_before = post;
        Expect(!Vulkan::SetSpecConstantDefault(post, 0, 340.0f) && post == post_before,
               "overlay: code without constant_id 0 is left alone");
    }
    vmaDestroyAllocator(g.allocator);
    Expect(validation_errors == 0, "no validation errors");
    std::printf(failures ? "pp-pass-test FAILED (%d)\n" : "pp-pass-test passed\n", failures);
    return failures ? 1 : 0;
}
