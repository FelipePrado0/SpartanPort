//  SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
//  SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"
#include "video_core/renderer_vulkan/movie_bars.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {
class Frame;
}

namespace Vulkan::HostPasses {

class PostProcessingPass {
public:
    struct Settings {
        float gamma = 1.0f;
        u32 hdr = 0;
        u32 srgb_input = 0;
        u32 padding = 0; // std430: the vec4 below starts at offset 16
        // spartanport: the part of the input shown; the rest is drawn black (cutscene bars).
        MovieBars::Rect picture = MovieBars::Whole;
        // spartanport: the player's picture controls (SpartanGraphics::Picture*); offsets 32 to 44.
        float picture_power = 1.0f;
        float picture_contrast = 1.0f;
        float picture_brightness = 0.0f;
        u32 picture_adjust = 0; // 0: the shader skips all three
    };
    static_assert(sizeof(Settings) == 48);

    void Create(vk::Device device, vk::Format surface_format);

    void Render(vk::CommandBuffer cmdbuf, vk::ImageView input, vk::Extent2D input_size,
                Frame& output, Settings settings);

private:
    vk::UniquePipeline pipeline{};
    vk::UniquePipelineLayout pipeline_layout{};
    vk::UniqueDescriptorSetLayout desc_set_layout{};
    vk::UniqueSampler sampler{};
};

} // namespace Vulkan::HostPasses
