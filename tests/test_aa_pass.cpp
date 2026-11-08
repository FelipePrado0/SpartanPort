// SPDX-License-Identifier: GPL-2.0-or-later
// Runs the production anti-aliasing pass (host_passes/aa_pass.cpp, fxaa.comp, smaa.comp) on a Vulkan
// device (Lavapipe works) with synthetic images: flat areas stay as they are, jagged and thin edges are
// smoothed, axis aligned edges stay sharp under SMAA, nothing turns NaN, and the ring of output images,
// a size change and the off mode behave. With VK_LAYER_KHRONOS_validation installed it also runs under
// validation (SPARTAN_AA_TEST_VALIDATE=0 turns that off) and fails on any validation error.
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
#include "video_core/renderer_vulkan/host_passes/aa_pass.h"
#include "video_core/renderer_vulkan/vk_common.h"

#include <vk_mem_alloc.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

// The pass asserts and logs through the port's own helpers; this test links neither.
void assert_fail_impl() {
    std::abort();
}
void unreachable_impl() {
    std::abort();
}

namespace VideoCore {
UniqueImage::~UniqueImage() {
    if (image) vmaDestroyImage(allocator, image, allocation);
}
void UniqueImage::Destroy() {
    if (image) {
        vmaDestroyImage(allocator, image, allocation);
        image = vk::Image{};
        allocation = {};
    }
}
void UniqueImage::Create(const vk::ImageCreateInfo& ci) {
    image_ci = ci;
    assert(!image);
    const VmaAllocationCreateInfo alloc_ci{.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
    const VkImageCreateInfo unsafe_ci = static_cast<VkImageCreateInfo>(ci);
    VkImage raw{};
    VmaAllocationInfo info{};
    const VkResult result = vmaCreateImage(allocator, &unsafe_ci, &alloc_ci, &raw, &allocation, &info);
    assert(result == VK_SUCCESS);
    (void)result;
    image = vk::Image{raw};
    size_bytes = info.size;
}
} // namespace VideoCore

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

constexpr u32 W = 160, H = 96;

float SrgbToLinear(float c) {
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}
float Half(u16 v) {
    const int exponent = (v >> 10) & 31;
    const float mantissa = float(v & 1023);
    const float sign = v & 0x8000 ? -1.f : 1.f;
    if (exponent == 31) return mantissa == 0 ? sign * INFINITY : NAN;
    return sign * (exponent == 0 ? std::ldexp(mantissa, -24) : std::ldexp(1.f + mantissa / 1024.f, exponent - 15));
}

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
    const char* validate = std::getenv("SPARTAN_AA_TEST_VALIDATE");
    bool want_validation = !validate || std::string_view(validate) != "0";
    std::vector<const char*> layers;
    if (want_validation) {
        for (const auto& layer : vk::enumerateInstanceLayerProperties().value) {
            if (std::string_view(layer.layerName) == "VK_LAYER_KHRONOS_validation") {
                layers.push_back("VK_LAYER_KHRONOS_validation");
            }
        }
    }
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
    const auto families = g.physical.getQueueFamilyProperties();
    for (u32 i = 0; i < families.size(); ++i) {
        if (families[i].queueFlags & vk::QueueFlagBits::eCompute) {
            g.family = i;
            break;
        }
    }
    const float priority = 1.0f;
    const vk::DeviceQueueCreateInfo queue_ci{.queueFamilyIndex = g.family, .queueCount = 1, .pQueuePriorities = &priority};
    const std::array device_extensions{VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME};
    vk::PhysicalDeviceVulkan13Features features13{.synchronization2 = true};
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
    assert(vmaCreateAllocator(&allocator_ci, &g.allocator) == VK_SUCCESS);
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
    assert(vmaCreateBuffer(g.allocator, &ci, &ac, &b.buffer, &b.allocation, &info) == VK_SUCCESS);
    b.mapped = info.pMappedData;
    return b;
}

// The game's image: an sRGB 8-bit RGBA texture the pass samples as linear light.
struct Input {
    VideoCore::UniqueImage image;
    vk::UniqueImageView view;
    std::vector<float> luma; // sRGB-coded luma per pixel, for the checks
};

using Pattern = float (*)(u32 x, u32 y);

Input MakeInput(Gpu& g, vk::CommandBuffer cmd, Pattern pattern, u32 w, u32 h, Buffer& staging) {
    Input in;
    in.image = VideoCore::UniqueImage(*g.device, g.allocator);
    in.image.Create({.imageType = vk::ImageType::e2D,
                     .format = vk::Format::eR8G8B8A8Srgb,
                     .extent = {w, h, 1},
                     .mipLevels = 1,
                     .arrayLayers = 1,
                     .samples = vk::SampleCountFlagBits::e1,
                     .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst |
                              vk::ImageUsageFlagBits::eTransferSrc,
                     .initialLayout = vk::ImageLayout::eUndefined});
    in.view = g.device->createImageViewUnique({.image = in.image,
                                               .viewType = vk::ImageViewType::e2D,
                                               .format = vk::Format::eR8G8B8A8Srgb,
                                               .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}}).value;
    auto* bytes = static_cast<u8*>(staging.mapped);
    in.luma.resize(size_t(w) * h);
    for (u32 y = 0; y < h; ++y) {
        for (u32 x = 0; x < w; ++x) {
            const float v = std::clamp(pattern(x, y), 0.0f, 1.0f);
            const u8 q = u8(std::lround(v * 255.0f));
            u8* px = bytes + (size_t(y) * w + x) * 4;
            px[0] = px[1] = px[2] = q;
            px[3] = 255;
            in.luma[size_t(y) * w + x] = float(q) / 255.0f;
        }
    }
    const vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    const vk::ImageMemoryBarrier2 to_dst{.srcStageMask = vk::PipelineStageFlagBits2::eNone,
                                         .dstStageMask = vk::PipelineStageFlagBits2::eCopy,
                                         .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
                                         .oldLayout = vk::ImageLayout::eUndefined,
                                         .newLayout = vk::ImageLayout::eTransferDstOptimal,
                                         .image = in.image,
                                         .subresourceRange = range};
    cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &to_dst});
    const vk::BufferImageCopy copy{.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1}, .imageExtent = {w, h, 1}};
    cmd.copyBufferToImage(staging.buffer, in.image, vk::ImageLayout::eTransferDstOptimal, copy);
    const vk::ImageMemoryBarrier2 to_read{.srcStageMask = vk::PipelineStageFlagBits2::eCopy,
                                          .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                                          .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
                                          .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
                                          .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                                          .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                                          .image = in.image,
                                          .subresourceRange = range};
    cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &to_read});
    return in;
}

struct Result {
    std::vector<float> luma; // sRGB-coded luma of the (grey) output
    bool finite = true;
    bool alpha_one = true;
    bool same_view = false;
    Input input;
};

float LinearToSrgb(float c) {
    return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

// Renders the pattern through the pass `frames` times (the ring is shorter than that) and reads the
// last output back.
Result Run(Gpu& g, HostPasses::AaPass& pass, Pattern pattern, u32 w, u32 h, int mode, bool linear_input,
           int frames = 1) {
    Buffer staging = MakeBuffer(g, size_t(w) * h * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    Buffer readback = MakeBuffer(g, size_t(w) * h * 8, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    Result result;
    vk::UniqueFence fence = g.device->createFenceUnique({}).value;
    auto cmds = g.device->allocateCommandBuffersUnique({.commandPool = *g.pool, .commandBufferCount = 1}).value;
    vk::CommandBuffer cmd = *cmds[0];
    (void)cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    result.input = MakeInput(g, cmd, pattern, w, h, staging);
    vk::ImageView out_view{};
    for (int f = 0; f < frames; ++f) {
        out_view = pass.Render(cmd, *result.input.view, {w, h}, mode, linear_input);
    }
    result.same_view = out_view == *result.input.view;
    const vk::Image shown = result.same_view ? vk::Image(result.input.image) : pass.ImageOf(out_view);
    assert(shown);
    const vk::ImageSubresourceRange range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    const vk::ImageMemoryBarrier2 to_src{.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                                         .srcAccessMask = vk::AccessFlagBits2::eMemoryWrite,
                                         .dstStageMask = vk::PipelineStageFlagBits2::eCopy,
                                         .dstAccessMask = vk::AccessFlagBits2::eTransferRead,
                                         .oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                                         .newLayout = vk::ImageLayout::eTransferSrcOptimal,
                                         .image = shown,
                                         .subresourceRange = range};
    cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &to_src});
    const vk::BufferImageCopy copy{.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1}, .imageExtent = {w, h, 1}};
    cmd.copyImageToBuffer(shown, vk::ImageLayout::eTransferSrcOptimal, readback.buffer, copy);
    const vk::MemoryBarrier2 host{.srcStageMask = vk::PipelineStageFlagBits2::eCopy,
                                  .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                                  .dstStageMask = vk::PipelineStageFlagBits2::eHost,
                                  .dstAccessMask = vk::AccessFlagBits2::eHostRead};
    cmd.pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &host});
    (void)cmd.end();
    const vk::CommandBufferSubmitInfo cb{.commandBuffer = cmd};
    (void)g.queue.submit2(vk::SubmitInfo2{.commandBufferInfoCount = 1, .pCommandBufferInfos = &cb}, *fence);
    assert(g.device->waitForFences(*fence, true, 10'000'000'000ull) == vk::Result::eSuccess);
    vmaInvalidateAllocation(g.allocator, readback.allocation, 0, VK_WHOLE_SIZE);

    result.luma.resize(size_t(w) * h);
    for (size_t i = 0; i < result.luma.size(); ++i) {
        float rgb[4];
        if (result.same_view) {
            const u8* px = static_cast<const u8*>(readback.mapped) + i * 4; // 8-bit sRGB copied raw
            result.luma[i] = px[0] / 255.0f;
            continue;
        }
        const u16* half = reinterpret_cast<const u16*>(static_cast<const u8*>(readback.mapped) + i * 8);
        for (int c = 0; c < 4; ++c) rgb[c] = Half(half[c]);
        for (int c = 0; c < 4; ++c) result.finite = result.finite && std::isfinite(rgb[c]);
        result.alpha_one = result.alpha_one && std::fabs(rgb[3] - 1.0f) < 1e-3f;
        result.luma[i] = LinearToSrgb(rgb[0]);
    }
    vmaDestroyBuffer(g.allocator, staging.buffer, staging.allocation);
    vmaDestroyBuffer(g.allocator, readback.buffer, readback.allocation);
    return result;
}

// Test patterns, values 0..1 in sRGB coding.
float Flat(u32, u32) { return 0.5f; }
float Diagonal(u32 x, u32 y) { return float(x) * 0.5f + float(y) > 70.0f ? 1.0f : 0.0f; }
float Rectangle(u32 x, u32 y) { return x >= 40 && x < 120 && y >= 20 && y < 76 ? 1.0f : 0.0f; }
float ThinLine(u32 x, u32 y) { return y == 20 + x / 5 ? 1.0f : 0.0f; }
float Circle(u32 x, u32 y) {
    const float dx = float(x) - 80.0f, dy = float(y) - 48.0f;
    return dx * dx + dy * dy < 34.0f * 34.0f ? 0.9f : 0.15f;
}
float LowContrast(u32 x, u32 y) { return float(x) * 0.5f + float(y) > 70.0f ? 0.52f : 0.50f; }

int Intermediate(const std::vector<float>& v) {
    int n = 0;
    for (const float c : v) n += c > 0.08f && c < 0.92f;
    return n;
}
float MaxDiff(const std::vector<float>& a, const std::vector<float>& b) {
    float d = 0;
    for (size_t i = 0; i < a.size(); ++i) d = std::max(d, std::fabs(a[i] - b[i]));
    return d;
}
float MeanLinear(const std::vector<float>& v) {
    double sum = 0;
    for (const float c : v) sum += SrgbToLinear(c);
    return float(sum / double(v.size()));
}

int failures = 0;
void Expect(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    failures += !ok;
}

void CheckMode(Gpu& g, HostPasses::AaPass& pass, int mode, const char* name) {
    std::printf("-- %s\n", name);
    char text[160];
    {
        const Result r = Run(g, pass, Flat, W, H, mode, true);
        Expect(r.finite && r.alpha_one, "flat: finite, alpha 1");
        Expect(MaxDiff(r.luma, r.input.luma) < 0.004f, "flat: unchanged");
    }
    {
        const Result r = Run(g, pass, LowContrast, W, H, mode, true);
        Expect(MaxDiff(r.luma, r.input.luma) < 0.004f, "low contrast edge: left alone");
    }
    {
        const Result r = Run(g, pass, Diagonal, W, H, mode, true);
        const int smooth = Intermediate(r.luma);
        std::snprintf(text, sizeof text, "diagonal edge: smoothed (%d intermediate pixels, input %d)", smooth,
                      Intermediate(r.input.luma));
        Expect(r.finite && r.alpha_one && smooth >= 60, text);
        // Far from the edge nothing changes.
        float far_diff = 0;
        for (u32 y = 0; y < H; ++y) {
            for (u32 x = 0; x < W; ++x) {
                const float d = (float(x) * 0.5f + float(y) - 70.0f) / std::sqrt(1.25f);
                if (std::fabs(d) > 6.0f) far_diff = std::max(far_diff, std::fabs(r.luma[y * W + x] - r.input.luma[y * W + x]));
            }
        }
        std::snprintf(text, sizeof text, "diagonal edge: 6+ pixels away unchanged (max diff %.4f)", far_diff);
        Expect(far_diff < 0.01f, text);
        const float before = MeanLinear(r.input.luma), after = MeanLinear(r.luma);
        std::snprintf(text, sizeof text, "diagonal edge: brightness kept (%.4f -> %.4f)", before, after);
        Expect(std::fabs(after - before) < 0.02f * before, text);
    }
    {
        const Result r = Run(g, pass, ThinLine, W, H, mode, true);
        std::snprintf(text, sizeof text, "thin line: smoothed (%d intermediate pixels)", Intermediate(r.luma));
        Expect(r.finite && Intermediate(r.luma) >= 20, text);
    }
    {
        const Result r = Run(g, pass, Diagonal, W, H, mode, false);
        Expect(r.finite && r.alpha_one && Intermediate(r.luma) >= 40, "perceptual input values also work");
    }
    {
        const Result r = Run(g, pass, Diagonal, W, H, mode, true, 7);
        const Result once = Run(g, pass, Diagonal, W, H, mode, true, 1);
        Expect(MaxDiff(r.luma, once.luma) < 0.004f, "seven frames in a row (output ring wraps) match one");
    }
    {
        const Result r = Run(g, pass, Diagonal, 96, 64, mode, true);
        Expect(r.finite && r.alpha_one && Intermediate(r.luma) >= 30, "size change (96x64) works");
        const Result back = Run(g, pass, Diagonal, W, H, mode, true);
        Expect(back.finite && Intermediate(back.luma) >= 60, "and back to the first size");
    }
}

} // namespace

int main() {
    Gpu g;
    Init(g);
    {
    HostPasses::AaPass pass;
    pass.Create(*g.device, g.allocator, 3);

    {
        const Result off = Run(g, pass, Diagonal, W, H, HostPasses::AaPass::ModeOff, true);
        Expect(off.same_view && MaxDiff(off.luma, off.input.luma) == 0.0f, "off: the input view comes back untouched");
        const Result tiny = Run(g, pass, Flat, 8, 8, HostPasses::AaPass::ModeFxaa, true);
        Expect(tiny.same_view, "an 8x8 image is left alone");
    }
    CheckMode(g, pass, HostPasses::AaPass::ModeFxaa, "FXAA");
    CheckMode(g, pass, HostPasses::AaPass::ModeSmaa, "SMAA");
    {
        // SMAA leaves the middle of a long straight edge alone: its patterns blend only a jagged edge or
        // the end of a line (a corner is rounded over a few pixels, fading with the distance).
        const Result r = Run(g, pass, Rectangle, W, H, HostPasses::AaPass::ModeSmaa, true);
        float worst = 0;
        for (u32 y : {19u, 20u, 21u, 74u, 75u, 76u}) {
            for (u32 x = 74; x <= 86; ++x) {
                worst = std::max(worst, std::fabs(r.luma[y * W + x] - r.input.luma[y * W + x]));
            }
        }
        char text[120];
        std::snprintf(text, sizeof text, "SMAA: the middle of a straight edge stays sharp (max diff %.4f)", worst);
        Expect(worst < 0.02f, text);
        const float corner = std::fabs(r.luma[20 * W + 40] - r.input.luma[20 * W + 40]);
        std::snprintf(text, sizeof text, "SMAA: the corner pixel is rounded (diff %.3f)", corner);
        Expect(corner > 0.05f, text);
    }
    if (const char* dump = std::getenv("SPARTAN_AA_TEST_DUMP")) {
        // SPARTAN_AA_TEST_DUMP=<dir>: the circle as binary PGM, input and each mode, for a look.
        for (int mode = 0; mode <= 2; ++mode) {
            const Result r = Run(g, pass, Circle, W, H, mode, true);
            char name[256];
            std::snprintf(name, sizeof name, "%s/circle_%d.pgm", dump, mode);
            if (FILE* f = std::fopen(name, "wb")) {
                std::fprintf(f, "P5\n%u %u\n255\n", W, H);
                for (const float c : r.luma) std::fputc(int(std::lround(std::clamp(c, 0.0f, 1.0f) * 255.0f)), f);
                std::fclose(f);
            }
        }
    }
    (void)g.device->waitIdle();
    }
    vmaDestroyAllocator(g.allocator);
    Expect(validation_errors == 0, "no validation errors");
    std::printf(failures ? "aa-pass-test FAILED (%d)\n" : "aa-pass-test passed\n", failures);
    return failures ? 1 : 0;
}
