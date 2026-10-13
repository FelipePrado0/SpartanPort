// SPDX-License-Identifier: GPL-2.0-or-later
// The frame analyzer's buffer dump (vk_frame_capture.cpp). A binding whose bytes the host cannot
// read (data null: a constant-ring copy of the flattened user data carries guest address 0,
// issue #174) is skipped instead of read through the null pointer. No game or Vulkan device.
#include <array>
#include <cstdio>

#include "video_core/renderer_vulkan/vk_frame_capture.h"

int main() {
    // A buffer with host bytes is dumped as before.
    std::array<float, 16> words{};
    Vulkan::FrameCapture::Buffer(0x1234, 0, 0x1000, words.data(), sizeof(words));
    // Flattened user data in the constant ring: host copy only, guest address 0, and here
    // nothing readable at all. Must not dereference `data`.
    Vulkan::FrameCapture::Buffer(0x1234, 1, 0, nullptr, 64);
    std::puts("frame-capture-test ok");
    return 0;
}
