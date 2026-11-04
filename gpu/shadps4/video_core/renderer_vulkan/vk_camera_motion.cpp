// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_vulkan/vk_camera_motion.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "spartanport_settings.h"
#include "spartanport_toggles.h"

#include "video_core/host_shaders/camera_motion_comp.h"
#include "video_core/host_shaders/camera_motion_debug_comp.h"
#include "video_core/host_shaders/camera_motion_vp_comp.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_object_motion.h"
#include "video_core/renderer_vulkan/vk_runtime.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/renderer_vulkan/vk_view_projection.h"
#include "video_core/texture_cache/texture_cache.h"

namespace Vulkan {

namespace {

struct PushConstants {
    std::array<float, 12> reproject;
    std::array<float, 4> proj;
    std::array<float, 4> prev_proj;
    std::array<float, 2> size;
    std::array<float, 2> jitter;
    std::array<float, 2> previous_jitter;
    u32 mode;
};

// Mirrors the std430 push constant block of camera_motion_vp.comp: the vec4 viewport sits at offset
// 96, so two floats of padding follow depth_range (without them the shader read the viewport from
// the wrong place and every camera motion vector was off).
struct VpPushConstants {
    CameraVp::Matrix reproject;       // 0
    std::array<float, 2> size;        // 64
    std::array<float, 2> jitter;      // 72
    std::array<float, 2> depth_range; // 80
    std::array<float, 2> pad{};       // 88
    std::array<float, 4> viewport;    // 96
    u32 mode;                         // 112: 1 = object motion vectors are valid
    std::array<u32, 3> pad_end{};     // 116
};
static_assert(sizeof(VpPushConstants) == 128);

/// a * b for 3x4 affine matrices (rows [R | t]).
std::array<float, 12> Multiply(const std::array<float, 12>& a, const std::array<float, 12>& b) {
    std::array<float, 12> out{};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 4; ++c) {
            float v = c == 3 ? a[r * 4 + 3] : 0.0f;
            for (int k = 0; k < 3; ++k) {
                v += a[r * 4 + k] * b[k * 4 + c];
            }
            out[r * 4 + c] = v;
        }
    }
    return out;
}

} // namespace

CameraMotion::CameraMotion(const Instance& instance_, Scheduler& scheduler_,
                           VideoCore::TextureCache& texture_cache_, Runtime& runtime_)
    : instance{instance_}, scheduler{scheduler_}, texture_cache{texture_cache_}, runtime{runtime_} {
    const char* env = std::getenv("SPARTAN_DEBUG_MOTION");
    debug_overlay = env && env[0] == '1';
    const char* upscaler = std::getenv("SPARTAN_UPSCALER");
    // The upscaler can be switched on from the menu at any time: the camera is always tracked
    // unless SPARTAN_UPSCALER=none.
    for_upscaler = !(upscaler && std::strcmp(upscaler, "none") == 0);
    const auto device = instance.GetDevice();
    if (for_upscaler) {
        const std::array<vk::DescriptorSetLayoutBinding, 3> motion_bindings = {{
            {.binding = 0,
             .descriptorType = vk::DescriptorType::eSampledImage,
             .descriptorCount = 1,
             .stageFlags = vk::ShaderStageFlagBits::eCompute},
            {.binding = 1,
             .descriptorType = vk::DescriptorType::eStorageImage,
             .descriptorCount = 1,
             .stageFlags = vk::ShaderStageFlagBits::eCompute},
            {.binding = 2,
             .descriptorType = vk::DescriptorType::eSampledImage,
             .descriptorCount = 1,
             .stageFlags = vk::ShaderStageFlagBits::eCompute},
        }};
        motion_desc_layout = Check(device.createDescriptorSetLayoutUnique({
            .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
            .bindingCount = static_cast<u32>(motion_bindings.size()),
            .pBindings = motion_bindings.data(),
        }));
        const vk::PushConstantRange range{.stageFlags = vk::ShaderStageFlagBits::eCompute,
                                          .offset = 0,
                                          .size = sizeof(PushConstants)};
        motion_pipeline_layout = Check(device.createPipelineLayoutUnique({
            .setLayoutCount = 1,
            .pSetLayouts = &*motion_desc_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &range,
        }));
        const auto motion_module = CompileSPV(CAMERA_MOTION_COMP, device);
        motion_pipeline = Check(device.createComputePipelineUnique(
            {}, vk::ComputePipelineCreateInfo{
                    .stage = {.stage = vk::ShaderStageFlagBits::eCompute,
                              .module = motion_module,
                              .pName = "main"},
                    .layout = *motion_pipeline_layout,
                }));
        device.destroyShaderModule(motion_module);

        // The view-projection camera (God of War III): depth in, motion vectors out.
        const std::array<vk::DescriptorSetLayoutBinding, 2> vp_bindings = {{
            {.binding = 0,
             .descriptorType = vk::DescriptorType::eSampledImage,
             .descriptorCount = 1,
             .stageFlags = vk::ShaderStageFlagBits::eCompute},
            {.binding = 1,
             .descriptorType = vk::DescriptorType::eStorageImage,
             .descriptorCount = 1,
             .stageFlags = vk::ShaderStageFlagBits::eCompute},
        }};
        vp_desc_layout = Check(device.createDescriptorSetLayoutUnique({
            .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
            .bindingCount = static_cast<u32>(vp_bindings.size()),
            .pBindings = vp_bindings.data(),
        }));
        const vk::PushConstantRange vp_range{.stageFlags = vk::ShaderStageFlagBits::eCompute,
                                             .offset = 0,
                                             .size = sizeof(VpPushConstants)};
        vp_pipeline_layout = Check(device.createPipelineLayoutUnique({
            .setLayoutCount = 1,
            .pSetLayouts = &*vp_desc_layout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &vp_range,
        }));
        const auto vp_module = CompileSPV(CAMERA_MOTION_VP_COMP, device);
        vp_pipeline = Check(device.createComputePipelineUnique(
            {}, vk::ComputePipelineCreateInfo{
                    .stage = {.stage = vk::ShaderStageFlagBits::eCompute,
                              .module = vp_module,
                              .pName = "main"},
                    .layout = *vp_pipeline_layout,
                }));
        device.destroyShaderModule(vp_module);
        // Calibration of the screen axes against the game's viewport: x, y or xy.
        if (const char* flip = std::getenv("SPARTAN_VP_FLIP")) {
            vp_flip = (std::strchr(flip, 'x') ? 1u : 0u) | (std::strchr(flip, 'y') ? 2u : 0u);
        }
    }
    if (!debug_overlay) {
        return;
    }
    const std::array<vk::DescriptorSetLayoutBinding, 4> bindings = {{
        {.binding = 0,
         .descriptorType = vk::DescriptorType::eSampledImage,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},
        {.binding = 1,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},
        {.binding = 2,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},
        {.binding = 3,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .descriptorCount = 1,
         .stageFlags = vk::ShaderStageFlagBits::eCompute},
    }};
    desc_layout = Check(device.createDescriptorSetLayoutUnique({
        .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
        .bindingCount = static_cast<u32>(bindings.size()),
        .pBindings = bindings.data(),
    }));
    const vk::PushConstantRange push_range{
        .stageFlags = vk::ShaderStageFlagBits::eCompute,
        .offset = 0,
        .size = sizeof(PushConstants),
    };
    pipeline_layout = Check(device.createPipelineLayoutUnique({
        .setLayoutCount = 1,
        .pSetLayouts = &*desc_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range,
    }));
    const auto module = CompileSPV(CAMERA_MOTION_DEBUG_COMP, device);
    overlay_pipeline = Check(device.createComputePipelineUnique(
        {}, vk::ComputePipelineCreateInfo{
                .stage = {.stage = vk::ShaderStageFlagBits::eCompute,
                          .module = module,
                          .pName = "main"},
                .layout = *pipeline_layout,
            }));
    device.destroyShaderModule(module);
    for (auto& frame : frames) {
        frame = std::make_unique<VideoCore::Buffer>(instance, 0, 3840ull * 2160 * 4,
                                                    VideoCore::MemoryType::DeviceLocal);
    }
    std::printf("GPU: camera motion debug overlay on\n");
}

CameraMotion::~CameraMotion() = default;

float CameraMotion::VerticalFov() const noexcept {
    if (vp_current_valid) {
        return CameraVp::VerticalFov(vp_current);
    }
    return 2.0f * std::atan(1.0f / current.proj[1]);
}

float CameraMotion::Near() const noexcept {
    if (vp_current_valid) {
        return CameraVp::NearPlane(vp_current);
    }
    // depth = zs + zo / z is 0 at the near plane.
    return -current.proj[3] / current.proj[2];
}

std::array<std::array<float, 4>, 3> CameraMotion::TaaDepthParameters() const noexcept {
    if (vp_current_valid) {
        // Depth 0 at the near plane and 1 at infinity: only the sky threshold (z scale 1) is
        // read from the first two; the third is the viewport, the rest is in Reprojection().
        return {{{vp_depth[0], vp_depth[1], 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 0.0f}, vp_viewport}};
    }
    const auto transform = Multiply(previous.view, current.inv_view);
    return {current.proj, previous.proj,
            {transform[8], transform[9], transform[10], transform[11]}};
}

vk::ImageView CameraMotion::ObjectMotionView() const noexcept {
    return object_motion && object_motion->Enabled() ? object_motion->View() : vk::ImageView{};
}

vk::Image CameraMotion::ObjectMotionImage(u32 width, u32 height) const noexcept {
    return object_motion && object_motion->Enabled() ? object_motion->Image(width, height)
                                                      : vk::Image{};
}

void CameraMotion::RecordMotion(vk::CommandBuffer cmdbuf, vk::ImageView depth_view,
                                vk::ImageView motion_view, u32 width, u32 height) {
    bool object_valid = false;
    const vk::ImageView object_view = object_motion && object_motion->Enabled()
        ? object_motion->PrepareRead(cmdbuf, width, height, object_valid) : depth_view;
    const PushConstants push{
        .reproject = Multiply(previous.view, current.inv_view),
        .proj = current.proj,
        .prev_proj = previous.proj,
        .size = {float(width), float(height)},
        .jitter = jitter,
        .previous_jitter = previous_jitter,
        .mode = object_valid ? 1u : 0u,
    };
    const vk::DescriptorImageInfo depth_info{.imageView = depth_view,
                                             .imageLayout = vk::ImageLayout::eGeneral};
    const vk::DescriptorImageInfo motion_info{.imageView = motion_view,
                                              .imageLayout = vk::ImageLayout::eGeneral};
    const vk::DescriptorImageInfo object_info{.imageView = object_view,
                                              .imageLayout = vk::ImageLayout::eGeneral};
    const std::array<vk::WriteDescriptorSet, 3> writes = {{
        {.dstBinding = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eSampledImage,
         .pImageInfo = &depth_info},
        {.dstBinding = 1,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .pImageInfo = &motion_info},
        {.dstBinding = 2,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eSampledImage,
         .pImageInfo = &object_info},
    }};
    cmdbuf.bindPipeline(vk::PipelineBindPoint::eCompute, *motion_pipeline);
    cmdbuf.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *motion_pipeline_layout, 0,
                                writes);
    cmdbuf.pushConstants(*motion_pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0,
                         sizeof(push), &push);
    cmdbuf.dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

void CameraMotion::OnConstants(const float* data) {
    // Scene constants: far plane 3000, 1/far, and the render size.
    if (data[0] != 3000.0f || data[4] < 64.0f || data[5] < 64.0f ||
        std::abs(data[1] * data[0] - 1.0f) > 1e-3f) {
        return;
    }
    if (frame_has_camera) {
        return; // the first one of a frame is the main camera
    }
    previous = current;
    std::memcpy(current.view.data(), data + 8, 12 * sizeof(float));
    std::memcpy(current.inv_view.data(), data + 180, 12 * sizeof(float));
    current.proj = {data[52], data[57], data[62], data[63]};
    current.valid = current.proj[0] != 0.0f && current.proj[1] != 0.0f;
    const std::array<u32, 2> size{u32(data[4]), u32(data[5])};
    if (size != render_size) {
        std::printf("Camera motion: scene render size %ux%u\n", size[0], size[1]);
        render_size = size;
    }
    frame_has_camera = true;
}

void CameraMotion::OnGBufferPass(VideoCore::ImageId depth) {
    depth_id = depth;
}

bool CameraMotion::OnScenePrepass(VideoCore::ImageId depth, std::array<float, 4> viewport,
                                  std::array<float, 2> depth_range) {
    if (!scene_open && main_depth && depth != main_depth) {
        // A depth-only draw of another screen-sized depth after the composite (UI): not a scene.
        // If it goes on for long (the game reallocated its depth), the scene depth is learnt again.
        if (++foreign_draws >= 256) {
            main_depth = {};
            foreign_draws = 0;
        }
        return false;
    }
    ++scan_draws;
    if (!scene_open) {
        foreign_draws = 0;
        // The first depth-only draw after the last composite: a new scene. (Display passes are not
        // frame boundaries here: the game repeats frames at 30 fps and may issue the next
        // pre-pass before the previous frame's display pass.)
        scene_open = true;
        scene_started = true;
        open_displays = 0;
        vp_resolved = false;
        vp_tally.Clear();
        tally_has_entries = false;
        scene_depth_count = 0;
        depth_id = {};
    }
    // Other screen-sized depth-only draws exist (after the composite, a UI pass): the scene depth
    // is the one the most draws of the scene use, the pre-pass has hundreds.
    SceneDepth* entry = nullptr;
    for (u32 i = 0; i < scene_depth_count; ++i) {
        if (scene_depths[i].depth == depth) {
            entry = &scene_depths[i];
            break;
        }
    }
    if (!entry && scene_depth_count < scene_depths.size()) {
        entry = &scene_depths[scene_depth_count++];
        *entry = {depth, 0, viewport, depth_range};
    }
    if (entry) {
        ++entry->draws;
    }
    const SceneDepth* best = nullptr;
    for (u32 i = 0; i < scene_depth_count; ++i) {
        if (!best || scene_depths[i].draws > best->draws) {
            best = &scene_depths[i];
        }
    }
    if (best && best->depth != depth_id) {
        depth_id = best->depth;
        vp_viewport = best->viewport;
        vp_depth = best->depth_range;
        // SPARTAN_VP_FLIP=x|y|xy mirrors an axis, for calibration against the picture.
        if (vp_flip & 1u) vp_viewport[0] = -vp_viewport[0];
        if (vp_flip & 2u) vp_viewport[1] = -vp_viewport[1];
        static std::array<float, 4> logged{};
        if (best->viewport != logged) {
            logged = best->viewport;
            std::printf("Camera motion: viewport scale %g %g offset %g %g\n", best->viewport[0],
                        best->viewport[1], best->viewport[2], best->viewport[3]);
        }
    }
    return true;
}

void CameraMotion::OnCameraCandidate(const float* data, u32 size_bytes, u32 width, u32 height,
                                     u32 source) {
    ++scan_sources[std::min(source, 2u)];
    if (size_bytes < 16 * sizeof(float)) {
        return;
    }
    const u32 floats = size_bytes / sizeof(float);
    // The camera is normally the first 16 floats; when the scene has none yet, also try the later
    // 16 byte aligned windows of the buffer (another vertex shader variant, a cut scene).
    const u32 last_window = tally_has_entries ? 0u : std::min(floats - 16u, 252u);
    bool found = false;
    for (u32 offset = 0; offset <= last_window; offset += 4) {
        const float* m = data + offset;
        if (!CameraVp::LooksLikeViewProjection(m, float(width), float(height))) {
            continue;
        }
        found = true;
        vp_tally.Add(m);
        tally_has_entries = true;
        ++scan_hits;
        const std::array<u32, 2> size{width, height};
        if (size != render_size) {
            std::printf("Camera motion: scene render size %ux%u (view-projection camera, float offset %u)\n",
                        width, height, offset);
            render_size = size;
        }
        if (offset != 0) {
            static u32 logged_offsets = 0;
            if (logged_offsets++ < 8) {
                std::printf("Camera motion: camera found at float offset %u of a %u byte buffer\n",
                            offset, size_bytes);
            }
        }
        break;
    }
    if (!found && miss_streak >= 2 && !tally_has_entries) {
        // The camera was not found in the last scenes: log what the pre-pass binds.
        static u32 dumped = 0;
        if (dumped < 60) {
            ++dumped;
            std::printf("Camera motion: unrecognised buffer %u bytes, floats %g %g %g %g | %g %g %g %g | "
                        "%g %g %g %g | %g %g %g %g\n",
                        size_bytes, data[0], data[1], data[2], data[3], data[4], data[5], data[6],
                        data[7], data[8], data[9], data[10], data[11], data[12], data[13], data[14],
                        data[15]);
        }
    }
}

void CameraMotion::RecordMotionVp(vk::CommandBuffer cmdbuf, vk::ImageView depth_view,
                                  vk::ImageView motion_view, u32 width, u32 height) {
    const VpPushConstants push{
        .reproject = vp_reprojection,
        .size = {float(width), float(height)},
        .jitter = jitter,
        .depth_range = vp_depth,
        .viewport = vp_viewport,
    };
    const vk::DescriptorImageInfo depth_info{.imageView = depth_view,
                                             .imageLayout = vk::ImageLayout::eGeneral};
    const vk::DescriptorImageInfo motion_info{.imageView = motion_view,
                                              .imageLayout = vk::ImageLayout::eGeneral};
    const std::array<vk::WriteDescriptorSet, 2> writes = {{
        {.dstBinding = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eSampledImage,
         .pImageInfo = &depth_info},
        {.dstBinding = 1,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .pImageInfo = &motion_info},
    }};
    cmdbuf.bindPipeline(vk::PipelineBindPoint::eCompute, *vp_pipeline);
    cmdbuf.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *vp_pipeline_layout, 0, writes);
    cmdbuf.pushConstants(*vp_pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(push),
                         &push);
    cmdbuf.dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

bool CameraMotion::ResolveViewProjection() {
    if (vp_resolved) {
        return vp_current_valid;
    }
    CameraVp::Matrix best;
    if (!scene_open || !vp_tally.Best(best)) {
        if (scene_open) {
            ++miss_streak; // a scene without a camera (a menu, a movie, or an unknown layout)
            main_depth = {};
            scene_open = false;
            vp_resolved = true;
        }
        return false;
    }
    miss_streak = 0;
    open_displays = 0;
    vp_resolved = true;
    scene_open = false;
    main_depth = depth_id;
    tally_has_entries = false;
    vp_previous = vp_current;
    vp_previous_valid = vp_current_valid;
    vp_current = best;
    vp_current_valid = true;
    frame_has_camera = true;
    ++vp_frames;
    if (vp_previous_valid && !CameraVp::Reprojection(vp_previous, vp_current, vp_reprojection)) {
        vp_previous_valid = false;
    }
    return true;
}

void CameraMotion::OnViewProjection(const float* matrix, VideoCore::ImageId depth) {
    if (frame_has_camera) {
        return;
    }
    const auto camera = ViewProjection::Decompose(matrix);
    if (!camera.valid) {
        static int rejected = 0;
        if (rejected < 4) {
            ++rejected;
            std::printf("Camera motion: depth pre-pass constants rejected: %g %g %g %g | %g %g %g %g\n",
                        matrix[0], matrix[1], matrix[2], matrix[3], matrix[12], matrix[13],
                        matrix[14], matrix[15]);
        }
        return;
    }
    previous = current;
    current.view = camera.view;
    current.inv_view = camera.inv_view;
    current.proj = camera.proj;
    current.valid = true;
    depth_id = depth;
    prepass_camera = true;
    const auto& image = texture_cache.GetImage(depth);
    const std::array<u32, 2> size{image.info.size.width, image.info.size.height};
    if (size != render_size) {
        std::printf("Camera motion: depth pre-pass camera, scene render size %ux%u\n", size[0], size[1]);
        render_size = size;
    }
    frame_has_camera = true;
}

void CameraMotion::OnDisplayPass(VideoCore::ImageId frame) {
    if (debug_overlay && frame && depth_id && current.valid && previous.valid) {
        Overlay(frame);
    }
    if (SpartanSettings::Get().upscaler != SpartanSettings::UpscalerTaa) {
        // Upscaler path (FSR): the unchanged upstream behaviour, nothing of the scene TAA applies.
        if (!frame_has_camera) InvalidateHistory();
        frame_has_camera = false;
        depth_id = {};
        return;
    }
    // A scene (pre-pass) that no composite closed for a few frames is dropped.
    // The game repeats display passes at 60 Hz while a heavy scene (an arena) takes several of them
    // from its pre-pass to its composite: dropping it after 3 lost the camera for the whole level.
    if (scene_open && ++open_displays >= 24) {
        scene_open = false;
        tally_has_entries = false;
        vp_tally.Clear();
    }
    // A display pass without a scene is a repeated frame (30 fps): not a break of the history.
    // Only many in a row (a menu, a loading screen, a slow scene) drop it.
    if (frame_has_camera) {
        scenes_missed = 0;
    } else if (++scenes_missed >= 16) {
        InvalidateHistory();
    }
    frame_has_camera = false;
}

void CameraMotion::Overlay(VideoCore::ImageId frame) {
    auto& depth = texture_cache.GetImage(depth_id);
    auto& color = texture_cache.GetImage(frame);
    const auto depth_format = depth.info.pixel_format;
    if ((depth_format != vk::Format::eD32Sfloat && depth_format != vk::Format::eD32SfloatS8Uint) ||
        color.info.pixel_format != vk::Format::eR8G8B8A8Unorm ||
        !(color.usage_flags & vk::ImageUsageFlagBits::eStorage) ||
        color.info.size.width != depth.info.size.width ||
        color.info.size.height != depth.info.size.height) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            std::printf("Camera motion: overlay skipped (depth %s, frame %s %ux%u)\n",
                        vk::to_string(depth_format).c_str(),
                        vk::to_string(color.info.pixel_format).c_str(), color.info.size.width,
                        color.info.size.height);
        }
        return;
    }
    const auto device = instance.GetDevice();
    const auto depth_view = Check(device.createImageView({
        .image = vk::Image(depth.backing->image),
        .viewType = vk::ImageViewType::e2D,
        .format = depth_format,
        .subresourceRange = {vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1},
    }));
    const auto color_view = Check(device.createImageView({
        .image = vk::Image(color.backing->image),
        .viewType = vk::ImageViewType::e2D,
        .format = vk::Format::eR8G8B8A8Unorm,
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
    }));

    scheduler.EndRendering();
    runtime.Transit(&depth, vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderRead);
    runtime.Transit(&color, vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite);
    runtime.FlushBarriers();

    const PushConstants push{
        .reproject = Multiply(previous.view, current.inv_view),
        .proj = current.proj,
        .prev_proj = previous.proj,
        .size = {float(color.info.size.width), float(color.info.size.height)},
        .jitter = jitter,
        .previous_jitter = previous_jitter,
        .mode = SpartanToggle::Disabled(1u << 20)   ? 1u
                : SpartanToggle::Disabled(1u << 21) ? 2u
                : SpartanToggle::Disabled(1u << 22) ? 3u
                : SpartanToggle::Disabled(1u << 23) ? 4u
                                               : 0u,
    };
    static u32 log_counter = 0;
    if (++log_counter % 200 == 0) {
        const auto& m = push.reproject;
        std::printf("Camera motion: proj %g %g %g %g prev %g %g %g %g\n"
                    "  view  %8.4f %8.4f %8.4f %9.3f | %8.4f %8.4f %8.4f %9.3f | %8.4f %8.4f %8.4f %9.3f\n"
                    "  reproj %8.4f %8.4f %8.4f %9.4f | %8.4f %8.4f %8.4f %9.4f | %8.4f %8.4f %8.4f %9.4f\n"
                    "  depth %s %ux%u, frame %ux%u\n",
                    push.proj[0], push.proj[1], push.proj[2], push.proj[3], push.prev_proj[0],
                    push.prev_proj[1], push.prev_proj[2], push.prev_proj[3], current.view[0],
                    current.view[1], current.view[2], current.view[3], current.view[4],
                    current.view[5], current.view[6], current.view[7], current.view[8],
                    current.view[9], current.view[10], current.view[11], m[0], m[1], m[2], m[3],
                    m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11],
                    vk::to_string(depth_format).c_str(), depth.info.size.width,
                    depth.info.size.height, color.info.size.width, color.info.size.height);
    }
    const vk::DescriptorImageInfo depth_info{.imageView = depth_view,
                                             .imageLayout = vk::ImageLayout::eGeneral};
    const vk::DescriptorImageInfo color_info{.imageView = color_view,
                                             .imageLayout = vk::ImageLayout::eGeneral};
    const auto& prev_frame = *frames[frame_index ^ 1];
    const auto& next_frame = *frames[frame_index];
    frame_index ^= 1;
    const vk::DescriptorBufferInfo prev_info{prev_frame.Handle(), 0, prev_frame.SizeBytes()};
    const vk::DescriptorBufferInfo next_info{next_frame.Handle(), 0, next_frame.SizeBytes()};
    const std::array<vk::WriteDescriptorSet, 4> writes = {{
        {.dstBinding = 0,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eSampledImage,
         .pImageInfo = &depth_info},
        {.dstBinding = 1,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageImage,
         .pImageInfo = &color_info},
        {.dstBinding = 2,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .pBufferInfo = &prev_info},
        {.dstBinding = 3,
         .descriptorCount = 1,
         .descriptorType = vk::DescriptorType::eStorageBuffer,
         .pBufferInfo = &next_info},
    }};
    const auto cmdbuf = scheduler.CommandBuffer();
    cmdbuf.bindPipeline(vk::PipelineBindPoint::eCompute, *overlay_pipeline);
    cmdbuf.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *pipeline_layout, 0, writes);
    cmdbuf.pushConstants(*pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(push),
                         &push);
    // The previous frame buffer was written by the last dispatch.
    const vk::MemoryBarrier2 frame_barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
        .srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
        .dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite,
    };
    cmdbuf.pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &frame_barrier});
    cmdbuf.dispatch((color.info.size.width + 7) / 8, (color.info.size.height + 7) / 8, 1);

    scheduler.DeferOperation([device, depth_view, color_view] {
        device.destroyImageView(depth_view);
        device.destroyImageView(color_view);
    });
}

} // namespace Vulkan
