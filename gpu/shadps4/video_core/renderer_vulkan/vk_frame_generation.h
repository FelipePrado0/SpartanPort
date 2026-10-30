// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: AMD FSR 3.1 frame generation (FireBurn/FSR-Vulkan, 3.1.6 FI/OF). The upscaler
// captures the scene without the HUD after the upscale and generates the intermediate scene
// (Capture); once the display pass is done, the presenter copies the real HUD over it
// (Generate) and shows it before the real frame.
#pragma once

#include <chrono>
#include <deque>
#include <string>
#include <utility>

#include "common/types.h"
#include "video_core/renderer_vulkan/frame_generation_policy.h"
#include "video_core/renderer_vulkan/vk_common.h"
#include "video_core/texture_cache/image.h"

struct FfxVkFsr3_3_1_6FrameGenerationContext;
struct VmaAllocation_T;

namespace Vulkan {

class Instance;
class Scheduler;

class FrameGeneration {
public:
    FrameGeneration(const Instance& instance, Scheduler& scheduler);
    ~FrameGeneration();

    struct CaptureInfo {
        vk::Image scene;        ///< the upscaled frame before the HUD (output size, 8-bit), General
        vk::Format scene_format;
        u32 output_width, output_height;
        vk::ImageView depth;    ///< scene depth, sampled in General layout
        vk::Image motion;       ///< R16G16 render-size motion vectors in pixels, General
        vk::ImageView motion_view;    ///< sampled view of `motion`
        vk::ImageView character_mask; ///< R8, 1 on characters (stencil), General; or null
        vk::ImageView object_motion;  ///< RGBA: motion, validity, written depth; or null
        u32 width, height;      ///< render size
        float jitter_x, jitter_y;
        float frame_ms, near_plane, vertical_fov;
        float projection_z;
        bool reset;
    };
    /// After the upscale, before the HUD (TemporalUpscaler::RunScaled).
    void Capture(vk::CommandBuffer cmdbuf, const CaptureInfo& info);

    /// The display pass is done (Presenter::PrepareFrame). `final_image` (output size, General)
    /// is the real frame with its HUD. Returns the generated frame to show first, or null.
    vk::Image Generate(vk::CommandBuffer cmdbuf, bool upscaled, vk::Image final_image,
                       vk::Format final_format);

    /// A sampled view of the generated image in `format` (an 8-bit RGBA format).
    vk::ImageView GeneratedView(vk::Format format);

private:
    bool EnsureContext(u32 render_width, u32 render_height, u32 output_width, u32 output_height);
    void Destroy();
    void CreatePipelines();
    void Fail(const char* reason);
    void Report();

    const Instance& instance;
    Scheduler& scheduler;

    FfxVkFsr3_3_1_6FrameGenerationContext* context = nullptr;
    u32 width = 0, height = 0, out_width = 0, out_height = 0;
    VideoCore::UniqueImage scene_image, depth_image, motion_image, generated_image;
    vk::UniqueImageView scene_view, depth_view, motion_view, generated_storage_view;
    u64 slot_ticks[4] = {};
    bool slot_pending[4] = {};
    std::vector<std::pair<vk::Format, vk::UniqueImageView>> generated_views;
    vk::Buffer counts{};
    VmaAllocation_T* counts_allocation = nullptr;
    u32* counts_mapped = nullptr;

    vk::UniqueDescriptorSetLayout depth_set_layout, hud_set_layout;
    vk::UniquePipelineLayout depth_layout, hud_layout;
    vk::UniquePipeline depth_pipeline, hud_pipeline;

    // This frame's capture.
    bool captured = false;
    bool capture_reset = false;
    float capture_frame_ms = 16.6f;
    float near_plane = 0.1f, vertical_fov = 1.0f;
    u64 frame_id = 0;
    u64 dispatched_frame_id = 0;
    bool scene_srgb = false;
    bool failed = false;
    bool prepared_last_frame = false;
    /// Frame ids with the GPU tick that retires their temporary views.
    std::deque<std::pair<u64, u64>> in_flight;
    u32 slot = 0;
    float ui_coverage = 0.0f;

    // Once a second: real and generated frames, skips by reason.
    std::chrono::steady_clock::time_point report_start = std::chrono::steady_clock::now();
    u32 real = 0, generated = 0, skips[u32(FrameGenerationPolicy::Skip::Failed) + 1] = {};
    /// Two buffers: the Insert menu reads one while the other is written.
    std::string status[2];
    u32 status_index = 0;
};

} // namespace Vulkan
