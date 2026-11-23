// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: anti-aliasing of the finished frame, between the game's image and the sharpening pass.
// FXAA is one compute dispatch; SMAA 1x is three (edges, blending weights, neighbourhood blending)
// with the reference lookup textures. Neither needs the game's depth, motion or history: they work on
// the image that is about to be presented, so they apply to the HUD too (its text is already smooth
// and its edges are mostly left alone by the edge tests).

#pragma once

#include <array>
#include <span>
#include <vector>

#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"
#include "video_core/texture_cache/image.h"

namespace Vulkan::HostPasses {

class AaPass {
public:
    /// Same values as SpartanGraphics::AntiAliasing.
    static constexpr int ModeOff = 0, ModeFxaa = 1, ModeSmaa = 2;

    AaPass() = default;
    ~AaPass();
    AaPass(const AaPass&) = delete;
    AaPass& operator=(const AaPass&) = delete;

    /// Creates the descriptor layout and the samplers; pipelines and textures are made when a mode is
    /// first used, so a driver that rejects one only loses that mode. `num_images` outputs are kept in
    /// turn: the presenter reads one while the GPU may still be writing the next.
    void Create(vk::Device device, VmaAllocator allocator, u32 num_images);

    /// Filters `input` (shader read layout, sampled linearly) and returns the filtered view in the same
    /// layout; `input` itself when `mode` is off, the image is too small or the mode could not be set
    /// up. `linear_input`: the values are linear light (see SpartanGraphics::AaInputIsLinear).
    vk::ImageView Render(vk::CommandBuffer cmdbuf, vk::ImageView input, vk::Extent2D size, int mode,
                         bool linear_input);

    /// The image behind a view this pass returned (tests read it back); null for any other view.
    [[nodiscard]] vk::Image ImageOf(vk::ImageView view) const {
        for (const auto& output : outputs) {
            if (output.view && *output.view == view) {
                return output.image;
            }
        }
        return {};
    }

    /// A mode whose pipelines or textures failed to be created; it then behaves as off.
    [[nodiscard]] bool Failed(int mode) const {
        return mode == ModeFxaa ? fxaa_failed : mode == ModeSmaa ? smaa_failed : false;
    }

private:
    struct Constants {
        float metrics[4]; // 1 / width, 1 / height, width, height
        float edge_threshold;
        float edge_threshold_min;
        float subpixel;
        u32 linear_input;
    };
    static_assert(sizeof(Constants) == 32);

    struct Target {
        VideoCore::UniqueImage image;
        vk::UniqueImageView view;
    };

    bool EnsureFxaa();
    bool EnsureSmaa();
    vk::UniquePipeline MakePipeline(std::span<const u32> code, const char* name);
    void RecreateTargets(vk::Extent2D size, bool smaa);
    void MakeTarget(Target& target, vk::Extent2D size, vk::Format format, vk::ImageUsageFlags usage,
                    const char* name);
    bool UploadTables(vk::CommandBuffer cmdbuf);
    void Dispatch(vk::CommandBuffer cmdbuf, vk::Pipeline pipeline, vk::Extent2D size,
                  const Constants& constants, std::span<const vk::DescriptorImageInfo> images,
                  std::span<const u32> bindings);

    vk::Device device{};
    VmaAllocator allocator{};

    vk::UniqueSampler linear_sampler;
    vk::UniqueSampler point_sampler;
    vk::UniqueDescriptorSetLayout set_layout;
    vk::UniquePipelineLayout pipeline_layout;
    vk::UniquePipeline fxaa_pipeline;
    vk::UniquePipeline smaa_edges_pipeline;
    vk::UniquePipeline smaa_weights_pipeline;
    vk::UniquePipeline smaa_blend_pipeline;
    bool fxaa_failed = false;
    bool smaa_failed = false;

    // Output images in turn, and SMAA's two intermediate images (one set: the barriers order the use).
    std::vector<Target> outputs;
    u32 current_output = 0;
    Target edges;
    Target weights;
    vk::Extent2D target_size{};
    bool targets_have_smaa = false;

    // SMAA lookup textures, uploaded once through a staging buffer kept until the pass is destroyed.
    Target area_texture;
    Target search_texture;
    VkBuffer staging_buffer{};
    VmaAllocation staging_allocation{};
    bool tables_uploaded = false;
};

} // namespace Vulkan::HostPasses
