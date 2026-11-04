// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: the game's depth of field, found in frame captures (F11) of God of War III
// Remastered (CUSA01623 01.02). A chain of 480x270 passes blurs the scene colour twice; the final
// composite (the first draw of the pass writing the display buffer, pixel shader
// 1b4f84d1cc5f5f9e) reads the linear depth, the sharp scene colour and both blurred images and
// mixes them by how far each pixel is from the focus. Its constant buffer (80 bytes) holds:
//
//   [0] 1/130, -92.5/130 : depth -> distance from the focus (a scale and a bias)
//   [4] -3.03 -3.03 -2.94 2.94, [8] 1 2 2.94 -1.94: four ramps of that distance, each
//       saturate(slope * distance + offset): 1 1 1 0 in focus, 0 0 0 1 at the largest distance
//   [12..19]             : sample offsets
//
// With the four slopes (words 4 to 7) and the distance's scale and bias (words 0 and 1) at zero,
// the ramps stay at their in-focus values for every pixel, so the composite returns the sharp
// scene colour. The blur passes still run; their output is no longer used.

#pragma once

#include <cstdint>

namespace SpartanDepthOfField {

/// Pixel shader hash (as FrameCapture prints it) of the composite.
inline constexpr std::uint64_t Composite = 0x1b4f84d1cc5f5f9eull;
/// Its constant buffer: 20 floats at stage buffer 0.
inline constexpr std::uint32_t ConstantWords = 20;
inline constexpr std::uint32_t ConstantBytes = ConstantWords * 4;

/// Whether a stage's buffer is the composite's constants.
inline bool IsComposite(std::uint64_t pgm_hash, std::uint32_t buffer_index, std::uint64_t size) {
    return pgm_hash == Composite && buffer_index == 0 && size == ConstantBytes;
}

/// Removes the blur from the composite's constants. False (and nothing changed) unless they have
/// the shape described above: ramps that fall with the distance (words 4 to 6 negative, word 7
/// positive) and the first ramp's offset (word 8) at one.
inline bool Disable(float* constants) {
    const float* c = constants;
    if (!(c[4] < 0.0f && c[5] < 0.0f && c[6] < 0.0f && c[7] > 0.0f && c[8] > 0.99f && c[8] < 1.01f)) {
        return false;
    }
    constants[0] = 0.0f;
    constants[1] = 0.0f;
    constants[4] = constants[5] = constants[6] = constants[7] = 0.0f;
    return true;
}

} // namespace SpartanDepthOfField
