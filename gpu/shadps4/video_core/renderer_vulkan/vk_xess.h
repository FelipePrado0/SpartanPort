// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: Intel XeSS (XeSS-SR, Vulkan) as the accumulation of the God of War III TAA, at the
// game's own resolution (the AA quality setting: no upscaling). It takes the same inputs as the
// TAA: the jittered scene colour, the scene depth and the motion vectors in render pixels (previous
// minus current position, without the jitter), and writes the result into the TAA's output image.
//
// libxess.dll (Intel's binary, redistributable unmodified under the Intel Simplified Software
// License; packaging/fetch_xess.sh) is loaded at run time next to the executable, so the game runs
// without it and the menu says why XeSS is not available. Windows only: Intel ships no other
// binaries. The headers in third_party/xess are Intel's (MIT).

#pragma once

#include <array>
#include <memory>
#include <vector>
#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {

class Instance;

class XessAa {
public:
    explicit XessAa(const Instance& instance);
    ~XessAa();

    /// An image XeSS reads or writes; `view` covers `aspect` of mip 0, layer 0.
    struct Image {
        vk::Image image;
        vk::ImageView view;
        vk::Format format;
        vk::ImageAspectFlags aspect; ///< of the view
        u32 width, height;
    };
    struct Frame {
        vk::CommandBuffer cmdbuf;
        Image color, depth, motion;
        Image output; ///< in layout General, usage storage
        /// Optional responsive pixel mask (R8, 1 = distrust the history there; view null = none),
        /// clipped to `responsive_max`. Whether there is one is fixed when XeSS is (re)initialised.
        Image responsive;
        float responsive_max;
        std::array<float, 2> jitter; ///< pixels, as the game's projection was shifted
        bool reset;
    };

    /// Records XeSS into `frame.cmdbuf`: the inputs go to the shader read-only layout and back to
    /// General, the output stays in General. False when XeSS cannot run (Problem() says why).
    bool Record(const Frame& frame);

    /// Why the last Record failed, or null.
    [[nodiscard]] const char* Problem() const noexcept;
    /// XeSS will not run in this session (library missing, device not supported).
    [[nodiscard]] bool Fatal() const noexcept;

    /// The device extensions XeSS asks for (empty when the library is not there); for the device
    /// creation, which has to enable them.
    static std::vector<const char*> DeviceExtensions(vk::Instance instance,
                                                     vk::PhysicalDevice physical_device);

    /// Asks XeSS for the device features it needs and patches them into the feature chain of the
    /// device being created: `chain` is the head of that chain (a VkPhysicalDeviceFeatures2) and
    /// comes back as the head to create the device with. False when the library is missing or has
    /// no features to give; the chain is then left as it was.
    static bool PatchDeviceFeatures(vk::Instance instance, vk::PhysicalDevice physical_device,
                                    void*& chain);

    /// The device creation says whether the device has the extensions and features XeSS needs;
    /// without them Record fails for good instead of creating a context on a device that lacks them.
    /// `why` (kept, may be null) is shown in the menu then.
    static void SetDeviceReady(bool ready, const char* why = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace Vulkan
