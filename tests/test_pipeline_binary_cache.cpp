// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <cstdio>
#include <chrono>
#include <fstream>
#include <xxhash.h>
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_pipeline_serialization.h"

// Match the library's Vulkan-Hpp dispatcher layout, then enable test assertions.
#undef NDEBUG
#include <cassert>

namespace {
unsigned compiled{}, restored{}, destroyed{}, released{};
bool reject_binary{}, reject_pipeline{}, fail_capture{}, fail_compile{};
VKAPI_ATTR VkResult VKAPI_CALL GetKey(VkDevice, const VkPipelineCreateInfoKHR* info,
                                      VkPipelineBinaryKeyKHR* key) {
    assert(info && info->pNext);
    key->keySize = 2;
    key->key[0] = 3;
    key->key[1] = 4;
    return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL CreateBinaries(VkDevice, const VkPipelineBinaryCreateInfoKHR* info,
                                              const VkAllocationCallbacks*,
                                              VkPipelineBinaryHandlesInfoKHR* out) {
    if (info->pKeysAndDataInfo) {
        assert(!info->pipeline);
        assert(out->pipelineBinaryCount == 2);
        out->pPipelineBinaries[0] = reinterpret_cast<VkPipelineBinaryKHR>(uintptr_t{11});
        out->pPipelineBinaries[1] =
            reject_binary ? nullptr : reinterpret_cast<VkPipelineBinaryKHR>(uintptr_t{12});
        return reject_binary ? VK_ERROR_UNKNOWN : VK_SUCCESS;
    }
    assert(info->pipeline);
    if (fail_capture) {
        return VK_ERROR_UNKNOWN;
    }
    out->pipelineBinaryCount = 2;
    if (out->pPipelineBinaries) {
        out->pPipelineBinaries[0] = reinterpret_cast<VkPipelineBinaryKHR>(uintptr_t{11});
        out->pPipelineBinaries[1] = reinterpret_cast<VkPipelineBinaryKHR>(uintptr_t{12});
    }
    return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL DestroyBinary(VkDevice, VkPipelineBinaryKHR binary,
                                         const VkAllocationCallbacks*) {
    assert(binary);
    ++destroyed;
}
VKAPI_ATTR VkResult VKAPI_CALL GetData(VkDevice, const VkPipelineBinaryDataInfoKHR*,
                                       VkPipelineBinaryKeyKHR* key, size_t* size, void* data) {
    key->keySize = 1;
    key->key[0] = 5;
    *size = 2;
    if (data) {
        std::memcpy(data, "ab", 2);
    }
    return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL ReleaseData(VkDevice,
                                           const VkReleaseCapturedPipelineDataInfoKHR* info,
                                           const VkAllocationCallbacks*) {
    assert(info->pipeline);
    ++released;
    return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL CreatePipeline(VkDevice, VkPipelineCache cache, uint32_t count,
                                              const VkGraphicsPipelineCreateInfo* info,
                                              const VkAllocationCallbacks*, VkPipeline* pipeline) {
    assert(count == 1);
    const auto* first = static_cast<const VkBaseInStructure*>(info->pNext);
    if (first && first->sType == VK_STRUCTURE_TYPE_PIPELINE_BINARY_INFO_KHR) {
        assert(!cache);
        const auto* binaries = reinterpret_cast<const VkPipelineBinaryInfoKHR*>(first);
        const auto* flags =
            static_cast<const VkPipelineCreateFlags2CreateInfoKHR*>(binaries->pNext);
        assert(binaries->binaryCount == 2 &&
               !(flags->flags & VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR));
        ++restored;
        if (reject_pipeline) {
            return VK_ERROR_UNKNOWN;
        }
    } else {
        ++compiled;
        if (first && first->sType == VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR) {
            assert(!cache);
            const auto* flags = reinterpret_cast<const VkPipelineCreateFlags2CreateInfoKHR*>(first);
            assert(flags->flags & VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR);
            if (fail_compile) {
                return VK_ERROR_UNKNOWN;
            }
        }
    }
    *pipeline = reinterpret_cast<VkPipeline>(uintptr_t{21});
    return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL DestroyPipeline(VkDevice, VkPipeline, const VkAllocationCallbacks*) {}

void TestDriverFallback(const std::filesystem::path& root) {
    auto& dispatch = VULKAN_HPP_DEFAULT_DISPATCHER;
    const auto original = dispatch;
    dispatch.vkGetPipelineKeyKHR = GetKey;
    dispatch.vkCreatePipelineBinariesKHR = CreateBinaries;
    dispatch.vkDestroyPipelineBinaryKHR = DestroyBinary;
    dispatch.vkGetPipelineBinaryDataKHR = GetData;
    dispatch.vkReleaseCapturedPipelineDataKHR = ReleaseData;
    dispatch.vkCreateGraphicsPipelines = CreatePipeline;
    dispatch.vkDestroyPipeline = DestroyPipeline;
    const vk::Device device{reinterpret_cast<VkDevice>(uintptr_t{1})};
    const vk::PipelineCache cache{reinterpret_cast<VkPipelineCache>(uintptr_t{2})};
    const vk::GraphicsPipelineCreateInfo info{};
    const Vulkan::GraphicsPipelineKey key;
    const vk::PipelineBinaryKeyKHR global{.keySize = 2, .key = std::array<u8, 32>{1, 2}};
    const auto create = [&] {
        auto result = Vulkan::CreateCachedGraphicsPipeline(device, cache, info, key, global, root);
        assert(result.result == vk::Result::eSuccess && result.value);
    };
    const auto legacy = root / "legacy";
    assert(Vulkan::PipelineBinaryCache::Save(
        legacy / "000000000000002a_0304.bin",
        Vulkan::PipelineBinaryCache::Encode({1, 2}, {3, 4}, 42,
                                            {{{5}, {6, 7}}, {{8}, {9, 10}}})));
    {
        auto result = Vulkan::CreateCachedGraphicsPipeline(device, cache, info, key, global,
                                                           legacy);
        assert(result.result == vk::Result::eSuccess);
    }
    assert(compiled == 0 && restored == 1 && destroyed == 2);
    assert(Vulkan::PipelineBinaryCache::Load(legacy / "0304.bin", {1, 2}, {3, 4}, 0));
    restored = destroyed = 0;
    create();
    assert(compiled == 1 && restored == 0 && destroyed == 2 && released == 1);
    create();
    assert(compiled == 1 && restored == 1 && destroyed == 4 && released == 1);
    // The exact driver key must survive changes to the emulator's graphics key.
    auto other_key = key;
    other_key.stage_hashes[0] = 123;
    {
        auto result = Vulkan::CreateCachedGraphicsPipeline(device, cache, info, other_key, global,
                                                           root);
        assert(result.result == vk::Result::eSuccess);
    }
    assert(compiled == 1 && restored == 2);
    restored = 1;
    destroyed -= 2;
    reject_binary = true;
    create();
    assert(compiled == 2 && restored == 1 && destroyed == 7 && released == 2);
    reject_binary = false;
    reject_pipeline = true;
    create();
    assert(compiled == 3 && restored == 2 && destroyed == 11 && released == 3);
    reject_pipeline = false;
    const auto file = std::filesystem::directory_entry(root / "0304.bin");
    {
        std::ofstream corrupt(file.path(), std::ios::binary);
        corrupt << "broken";
    }
    fail_capture = true;
    create();
    assert(compiled == 4 && released == 4 && destroyed == 11);
    fail_capture = false;
    fail_compile = true;
    create();
    assert(compiled == 6 && released == 4);
    fail_compile = false;
    {
        auto result = Vulkan::CreateCachedGraphicsPipeline(device, cache, info, key, {}, root);
        assert(result.result == vk::Result::eSuccess);
    }
    assert(compiled == 7 && released == 4);
    // A path below a file cannot be written, but pipeline creation must still succeed.
    {
        auto result = Vulkan::CreateCachedGraphicsPipeline(device, cache, info, key, global,
                                                           file.path() / "blocked");
        assert(result.result == vk::Result::eSuccess);
    }
    assert(compiled == 8 && released == 5 && destroyed == 13);
    dispatch = original;
}

void TestRealDriver(const std::filesystem::path& directory) {
    Vulkan::Instance instance(0, false);
    static vk::detail::DynamicLoader loader;
    VULKAN_HPP_DEFAULT_DISPATCHER.init(
        loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance.GetInstance());
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance.GetDevice());
    const auto device = instance.GetDevice();
    const auto& global = instance.GetPipelineBinaryGlobalKey();
    assert(global.keySize && "VK_KHR_pipeline_binary must be enabled for this smoke test");
    // GLSL 450: void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }
    const u32 code[]{
        0x7230203, 0x10000,    0x8000b,    0x15,       0x0,        0x20011,    0x1,
        0x6000b,   0x1,        0x4c534c47, 0x6474732e, 0x3035342e, 0x0,        0x3000e,
        0x0,       0x1,        0x6000f,    0x0,        0x4,        0x6e69616d, 0x0,
        0xd,       0x30003,    0x2,        0x1c2,      0x40005,    0x4,        0x6e69616d,
        0x0,       0x60005,    0xb,        0x505f6c67, 0x65567265, 0x78657472, 0x0,
        0x60006,   0xb,        0x0,        0x505f6c67, 0x7469736f, 0x6e6f69,   0x70006,
        0xb,       0x1,        0x505f6c67, 0x746e696f, 0x657a6953, 0x0,        0x70006,
        0xb,       0x2,        0x435f6c67, 0x4470696c, 0x61747369, 0x65636e,   0x70006,
        0xb,       0x3,        0x435f6c67, 0x446c6c75, 0x61747369, 0x65636e,   0x30005,
        0xd,       0x0,        0x30047,    0xb,        0x2,        0x50048,    0xb,
        0x0,       0xb,        0x0,        0x50048,    0xb,        0x1,        0xb,
        0x1,       0x50048,    0xb,        0x2,        0xb,        0x3,        0x50048,
        0xb,       0x3,        0xb,        0x4,        0x20013,    0x2,        0x30021,
        0x3,       0x2,        0x30016,    0x6,        0x20,       0x40017,    0x7,
        0x6,       0x4,        0x40015,    0x8,        0x20,       0x0,        0x4002b,
        0x8,       0x9,        0x1,        0x4001c,    0xa,        0x6,        0x9,
        0x6001e,   0xb,        0x7,        0x6,        0xa,        0xa,        0x40020,
        0xc,       0x3,        0xb,        0x4003b,    0xc,        0xd,        0x3,
        0x40015,   0xe,        0x20,       0x1,        0x4002b,    0xe,        0xf,
        0x0,       0x4002b,    0x6,        0x10,       0x0,        0x4002b,    0x6,
        0x11,      0x3f800000, 0x7002c,    0x7,        0x12,       0x10,       0x10,
        0x10,      0x11,       0x40020,    0x13,       0x3,        0x7,        0x50036,
        0x2,       0x4,        0x0,        0x3,        0x200f8,    0x5,        0x50041,
        0x13,      0x14,       0xd,        0xf,        0x3003e,    0x14,       0x12,
        0x100fd,   0x10038};
    auto shader = device.createShaderModuleUnique({.codeSize = sizeof(code), .pCode = code});
    assert(shader.result == vk::Result::eSuccess);
    auto layout = device.createPipelineLayoutUnique({});
    assert(layout.result == vk::Result::eSuccess);
    const vk::PipelineShaderStageCreateInfo stage{
        .stage = vk::ShaderStageFlagBits::eVertex, .module = *shader.value, .pName = "main"};
    const vk::PipelineVertexInputStateCreateInfo vertex{};
    vk::PipelineInputAssemblyStateCreateInfo assembly{.topology =
                                                          vk::PrimitiveTopology::eTriangleList};
    const vk::Viewport viewport{0, 0, 1, 1, 0, 1};
    const vk::Rect2D scissor{{0, 0}, {1, 1}};
    const vk::PipelineViewportStateCreateInfo viewport_info{
        .viewportCount = 1, .pViewports = &viewport, .scissorCount = 1, .pScissors = &scissor};
    const vk::PipelineRasterizationStateCreateInfo raster{
        .rasterizerDiscardEnable = true, .polygonMode = vk::PolygonMode::eFill, .lineWidth = 1};
    const vk::PipelineMultisampleStateCreateInfo samples{.rasterizationSamples =
                                                             vk::SampleCountFlagBits::e1};
    const vk::PipelineRenderingCreateInfo rendering{};
    const vk::GraphicsPipelineCreateInfo info{.pNext = &rendering,
                                              .stageCount = 1,
                                              .pStages = &stage,
                                              .pVertexInputState = &vertex,
                                              .pInputAssemblyState = &assembly,
                                              .pViewportState = &viewport_info,
                                              .pRasterizationState = &raster,
                                              .pMultisampleState = &samples,
                                              .layout = *layout.value};
    const Vulkan::GraphicsPipelineKey key;
    for (int variant = 0; variant < 2; ++variant) {
        assembly.topology =
            variant ? vk::PrimitiveTopology::eLineList : vk::PrimitiveTopology::eTriangleList;
        for (int run = 0; run < 2; ++run) {
            const auto started = std::chrono::steady_clock::now();
            auto pipeline =
                Vulkan::CreateCachedGraphicsPipeline(device, {}, info, key, global, directory);
            assert(pipeline.result == vk::Result::eSuccess && pipeline.value);
            std::printf("Real driver: variant %d run %d %.3f ms\n", variant, run,
                        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                                  started)
                            .count());
        }
    }
    unsigned files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        files += entry.path().extension() == ".bin";
    }
    assert(files == 2);
    Vulkan::ReportPipelineBinaryCache();
}
} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    if (argc == 3 && std::string_view(argv[1]) == "--gpu") {
        TestRealDriver(argv[2]);
        return 0;
    }
    using namespace Vulkan::PipelineBinaryCache;
    const Key global{1, 2}, pipeline{3, 4};
    const std::vector<Binary> binaries{{{5}, {6, 7}}, {{8}, {9, 10, 11}}};
    const auto encoded = Encode(global, pipeline, 42, binaries);
    const auto decoded = Decode(encoded, global, pipeline, 42);
    assert(decoded && decoded->size() == 2);
    assert((*decoded)[0].key == binaries[0].key && (*decoded)[1].data == binaries[1].data);
    assert(!Decode(encoded, Key{2}, pipeline, 42));
    assert(!Decode(encoded, global, Key{4}, 42));
    assert(!Decode(encoded, global, pipeline, 43));
    const auto malformed = [&](size_t offset, u32 value) {
        auto bytes = encoded;
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
        const auto checksum = XXH3_64bits(bytes.data(), bytes.size() - 8);
        std::memcpy(bytes.data() + bytes.size() - 8, &checksum, 8);
        assert(!Decode(bytes, global, pipeline, 42));
    };
    malformed(4, 2);   // Unsupported version, with a valid checksum.
    malformed(16, 33); // Global key exceeds Vulkan's bound.
    malformed(28, MaxBinaries + 1);
    malformed(37, u32(MaxFileSize + 1)); // Binary data length must be bounded.
    auto trailing = encoded;
    trailing.insert(trailing.end() - 8, 0);
    const auto trailing_checksum = XXH3_64bits(trailing.data(), trailing.size() - 8);
    std::memcpy(trailing.data() + trailing.size() - 8, &trailing_checksum, 8);
    assert(!Decode(trailing, global, pipeline, 42));
    for (size_t i = 0; i < encoded.size(); ++i) {
        assert(!Decode(std::span(encoded).first(i), global, pipeline, 42));
        auto corrupt = encoded;
        corrupt[i] ^= 0x80;
        assert(!Decode(corrupt, global, pipeline, 42));
    }
    assert(Encode({}, pipeline, 42, binaries).empty());
    assert(Encode(global, pipeline, 42, {}).empty());
    assert(Encode(global, pipeline, 42, std::vector<Binary>(MaxBinaries + 1, binaries[0])).empty());
    const auto root = std::filesystem::temp_directory_path() /
                      ("spartanport-binary-test-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto file = root / "pipeline.bin";
    assert(!Load(file, global, pipeline, 42));
    assert(Save(file, encoded));
    assert(Load(file, global, pipeline, 42));
    const auto replacement = Encode(global, pipeline, 42, {binaries[1]});
    assert(Save(file, replacement));
    assert(Load(file, global, pipeline, 42)->size() == 1);
    assert(!Save(file / "blocked.bin", encoded));
    assert(Load(file, global, pipeline, 42)->size() == 1);
    {
        std::ofstream partial(file.string() + ".tmp");
        partial << "partial";
    }
    assert(Load(file, global, pipeline, 42)->size() == 1);
    {
        std::ofstream corrupt(file, std::ios::binary);
        corrupt << "broken";
    }
    assert(!Load(file, global, pipeline, 42));
    TestDriverFallback(root / "driver");
    std::filesystem::remove_all(root);
    std::puts("pipeline-binary-cache-test: passed");
}
