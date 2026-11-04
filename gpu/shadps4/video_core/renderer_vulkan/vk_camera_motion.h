// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: camera motion vectors for temporal upscaling (docs/upscaler.md). The scene constants
// give the camera of each frame; the previous one is kept here. SPARTAN_DEBUG_MOTION=1 blends the
// motion vectors as colors into the frame before it is copied to the display.

#pragma once

#include <array>
#include <utility>
#include <vector>

#include "common/types.h"
#include "video_core/renderer_vulkan/camera_view_projection.h"
#include "video_core/renderer_vulkan/vk_common.h"
#include "video_core/buffer_cache/buffer.h"
#include "video_core/texture_cache/image.h"

namespace VideoCore {
class TextureCache;
}

namespace Vulkan {

class Instance;
class Scheduler;
class Runtime;
class ObjectMotion;

class CameraMotion {
public:
    CameraMotion(const Instance& instance, Scheduler& scheduler,
                 VideoCore::TextureCache& texture_cache, Runtime& runtime);
    ~CameraMotion();
    void InvalidateHistory() {
        current.valid = previous.valid = false;
        vp_current_valid = vp_previous_valid = false;
    }
    void SetObjectMotion(ObjectMotion* motion) noexcept {
        object_motion = motion;
    }
    /// This frame's object motion image (after RecordMotion), for the debug view; or null.
    vk::ImageView ObjectMotionView() const noexcept;
    vk::Image ObjectMotionImage(u32 width, u32 height) const noexcept;

    void SetJitter(std::array<float, 2> value) noexcept {
        previous_jitter = jitter;
        jitter = value;
    }

    [[nodiscard]] bool Enabled() const noexcept {
        return debug_overlay || for_upscaler;
    }

    /// Both cameras and the scene depth of the current frame are known.
    [[nodiscard]] bool Ready() const noexcept {
        return CameraHistory() && depth_id;
    }
    /// This frame's camera and the previous one are known (the log's status line).
    [[nodiscard]] bool CameraHistory() const noexcept {
        return (current.valid && previous.valid) || (vp_current_valid && vp_previous_valid);
    }
    /// Frames with a view-projection camera since the last call (the log's status line).
    [[nodiscard]] u32 TakeCameraFrames() noexcept {
        return std::exchange(vp_frames, 0u);
    }
    /// Pre-pass draws scanned and constant buffers that were a camera, since the last call.
    [[nodiscard]] std::array<u32, 2> TakeScanCounts() noexcept {
        return {std::exchange(scan_draws, 0u), std::exchange(scan_hits, 0u)};
    }
    /// Vertex buffers scanned for the camera since the last call: ring, guest address, user data.
    [[nodiscard]] std::array<u32, 3> TakeScanSources() noexcept {
        const auto value = scan_sources;
        scan_sources = {};
        return value;
    }
    /// The camera is one view-projection matrix (God of War III), not the 864 byte constants.
    [[nodiscard]] bool ViewProjectionCamera() const noexcept {
        return vp_current_valid;
    }
    [[nodiscard]] VideoCore::ImageId Depth() const noexcept {
        return depth_id;
    }
    /// The camera came from a depth pre-pass (SpartanPort): characters and effects draw with another
    /// depth target of the same size, and need the jitter too.
    [[nodiscard]] bool PrepassCamera() const noexcept {
        return prepass_camera;
    }
    /// The render size in the scene constants (the game's viewport; its targets may be
    /// allocated larger, e.g. 1916x1080 for a 1916x1078 scene), or zero before the first camera.
    [[nodiscard]] std::array<u32, 2> RenderSize() const noexcept {
        return render_size;
    }
    /// Vertical field of view and near/far planes of the current camera.
    [[nodiscard]] float VerticalFov() const noexcept;
    [[nodiscard]] float Near() const noexcept;
    // Current projection, previous projection, and row Z of previous-view * inverse-view.
    [[nodiscard]] std::array<std::array<float, 4>, 3> TaaDepthParameters() const noexcept;

    /// Records the motion vector pass: `depth_view` (depth aspect, General layout) into
    /// `motion_view` (RG16F storage, General), pixels, previous minus current.
    void RecordMotion(vk::CommandBuffer cmdbuf, vk::ImageView depth_view, vk::ImageView motion_view,
                      u32 width, u32 height);

    /// The depth pre-pass (no colour target, a screen-sized depth): its depth is the scene depth.
    /// `viewport`: the game's x scale, y scale, x offset, y offset (screen = offset + scale * ndc).
    bool OnScenePrepass(VideoCore::ImageId depth, std::array<float, 4> viewport,
                        std::array<float, 2> depth_range);
    /// True once after a new scene's first depth pre-pass draw.
    [[nodiscard]] bool TakeSceneStart() noexcept {
        return std::exchange(scene_started, false);
    }
    /// Between the first pre-pass draw of a frame and its composite: the only time draws are
    /// jittered (the HUD, drawn when the game repeats a frame, must not be).
    [[nodiscard]] bool SceneOpen() const noexcept {
        return scene_open;
    }

    /// A vertex constant buffer of the pre-pass: counted when its first 16 floats are a
    /// view-projection for a screen of `width` x `height`.
    void OnCameraCandidate(const float* data, u32 size_bytes, u32 width, u32 height,
                           u32 source = 0);

    /// Picks this frame's camera (once per frame; the previous one is kept). False without one.
    bool ResolveViewProjection();

    /// The motion vector pass for the view-projection camera (see camera_motion_vp.comp).
    void RecordMotionVp(vk::CommandBuffer cmdbuf, vk::ImageView depth_view,
                        vk::ImageView motion_view, u32 width, u32 height);

    /// The game's depth scale and offset (depth = offset + scale * clip z / w).
    [[nodiscard]] std::array<float, 2> DepthRange() const noexcept {
        return vp_depth;
    }

    /// TAA's reprojection matrix (view-projection camera).
    [[nodiscard]] const CameraVp::Matrix& Reprojection() const noexcept {
        return vp_reprojection;
    }

    /// A bound constant buffer of 864 bytes: checks the scene constant signature.
    void OnConstants(const float* data);

    /// The G-buffer pass (5+ color targets): its depth is the scene depth.
    void OnGBufferPass(VideoCore::ImageId depth);

    /// A depth-only pass at the render size binding a view-projection (SpartanPort's depth pre-pass,
    /// vk_view_projection.h): the first valid one of a frame is the main camera, its depth
    /// the scene depth.
    void OnViewProjection(const float* matrix, VideoCore::ImageId depth);

    /// The pass copying the finished frame (`frame`, the last target drawn) to the display: the
    /// frame's depth and camera are complete. Records the debug overlay, starts a new frame.
    void OnDisplayPass(VideoCore::ImageId frame);

private:
    struct Camera {
        std::array<float, 12> view{};     ///< world to view, 3x4 rows
        std::array<float, 12> inv_view{}; ///< view to world, 3x4 rows
        std::array<float, 4> proj{};      ///< x scale, y scale, z scale, z offset
        bool valid = false;
    };

    void Overlay(VideoCore::ImageId frame);

    const Instance& instance;
    Scheduler& scheduler;
    VideoCore::TextureCache& texture_cache;
    Runtime& runtime;
    bool debug_overlay = false;
    bool for_upscaler = false;
    CameraVp::Tally vp_tally;
    CameraVp::Matrix vp_current{}, vp_previous{}, vp_reprojection{};
    bool vp_current_valid = false, vp_previous_valid = false, vp_resolved = false;
    u32 vp_flip = 0;
    std::array<float, 4> vp_viewport{1.0f, -1.0f, 0.0f, 0.0f};
    std::array<float, 2> vp_depth{1.0f, 0.0f}; ///< depth scale, offset
    u32 vp_frames = 0, scan_draws = 0, scan_hits = 0;
    std::array<u32, 3> scan_sources{}; ///< buffers scanned: ring, guest address, user data
    bool scene_open = false, tally_has_entries = false, scene_started = false;
    struct SceneDepth {
        VideoCore::ImageId depth{};
        u32 draws = 0;
        std::array<float, 4> viewport{};
        std::array<float, 2> depth_range{};
    };
    std::array<SceneDepth, 4> scene_depths{};
    u32 scene_depth_count = 0;
    VideoCore::ImageId main_depth{}; ///< the scene depth of the last frame with a camera
    u32 foreign_draws = 0;
    u32 miss_streak = 0, scenes_missed = 0, open_displays = 0;
    vk::UniqueDescriptorSetLayout vp_desc_layout;
    vk::UniquePipelineLayout vp_pipeline_layout;
    vk::UniquePipeline vp_pipeline;
    vk::UniqueDescriptorSetLayout motion_desc_layout;
    vk::UniquePipelineLayout motion_pipeline_layout;
    vk::UniquePipeline motion_pipeline;

    ObjectMotion* object_motion = nullptr;
    Camera current, previous;
    bool frame_has_camera = false;
    std::array<float, 2> jitter{}, previous_jitter{};
    std::array<u32, 2> render_size{};
    VideoCore::ImageId depth_id{};
    bool prepass_camera = false;

    vk::UniqueDescriptorSetLayout desc_layout;
    vk::UniquePipelineLayout pipeline_layout;
    vk::UniquePipeline overlay_pipeline;
    /// Debug: the last two finished frames (RGBA8 packed), for the reprojection check.
    std::unique_ptr<VideoCore::Buffer> frames[2];
    u32 frame_index = 0;
};

} // namespace Vulkan
