// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_vulkan/vk_fsr4.h"
#include <memory>
#include <string>

namespace Vulkan {

class DriverFsrUpscaler {
  public:
    DriverFsrUpscaler(vk::Device device, vk::PhysicalDevice physical,
                      PFN_vkGetDeviceProcAddr get_proc);
    ~DriverFsrUpscaler();
    bool Available() const;
    bool Record(const Fsr4Upscaler::Frame& frame);
    const std::string& Problem() const;
    const std::string& Description() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace Vulkan
