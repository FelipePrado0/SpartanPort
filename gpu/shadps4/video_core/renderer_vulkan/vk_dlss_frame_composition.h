// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/texture_cache/image.h"

#include "../../../streamline_bridge/spartanport_streamline.h"

namespace Vulkan {
class Instance;
struct Frame;
struct NvidiaInputs;

// Owned by a presentation Frame. Its present_done fence protects all these resources.
class NvidiaFrame {
public:
    NvidiaFrame(const Instance& instance, u32 width, u32 height, vk::Extent2D extent,
                vk::Format format, vk::Format backbuffer_format);
    ~NvidiaFrame();
    bool Matches(u32 width, u32 height, vk::Extent2D extent, vk::Format format,
                 vk::Format backbuffer_format) const;
    Frame Target() const;
    void Begin(vk::CommandBuffer cmd);
    bool Composite(vk::CommandBuffer cmd, vk::Image final_image, vk::ImageView final_view,
                   vk::ImageLayout final_layout);
    std::array<SpartanSlImage, 2> Resources() const;
    void ReadCoverage(); // Only after present_done is signaled.
    void* token{};
    bool reset = true;
    std::shared_ptr<NvidiaInputs> inputs;

private:
    const Instance& instance;
    u32 width, height;
    vk::Extent2D extent;
    vk::Format format;
    vk::Format backbuffer_format;
    VideoCore::UniqueImage processed, hudless, alpha;
    vk::UniqueImageView processed_view, hudless_view, alpha_view;
    vk::UniqueDescriptorSetLayout descriptors;
    vk::UniquePipelineLayout layout;
    vk::UniquePipeline pipeline;
    vk::Buffer counter{};
    VmaAllocation counter_allocation{};
    u32* mapped{};
    float coverage{};
    bool counted{};
};
} // namespace Vulkan
