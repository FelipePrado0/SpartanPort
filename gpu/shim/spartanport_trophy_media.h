// SPDX-License-Identifier: GPL-2.0-or-later
// SpartanPort: the player's files for the trophy notifications, decoded with FFmpeg: sounds to 48 kHz
// stereo float, pictures and animations (GIF, animated PNG, or any single image) to RGBA.

#pragma once

#include <filesystem>
#include <vector>

namespace SpartanToastMedia {

constexpr int kAudioRate = 48000;
/// At most this long: a notification, not a song.
constexpr float kMaxSoundSeconds = 20.0f;
/// Larger pictures are not opened at all.
constexpr int kMaxPictureSide = 8192;

/// 48 kHz stereo float samples (interleaved); empty when the file cannot be decoded.
std::vector<float> DecodeSound(const std::filesystem::path& path);

struct Animation {
    int width = 0, height = 0;         ///< of every frame
    std::vector<unsigned char> pixels; ///< the frames' RGBA, one after another
    std::vector<float> frame_ends;     ///< ms from the start; one per frame
    int Frames() const { return int(frame_ends.size()); }
};

struct AnimationLimits {
    int max_side = 256;      ///< larger frames are scaled down (the art is ~60 px at 1080p)
    int max_frames = 600;
    float max_ms = 20000.0f; ///< frames after this are dropped
    size_t max_pixels = size_t(16) << 20; ///< all frames together (64 MB of RGBA)
};

/// Every frame of an animated GIF or PNG, or the one frame of a still picture (PNG, JPEG, BMP,
/// WebP...). A frame without a duration lasts 100 ms. Empty when the file cannot be decoded.
Animation DecodeAnimation(const std::filesystem::path& path, const AnimationLimits& limits = {});

/// The frames of an Animation laid out as one texture: `columns` frames per row.
struct Sheet {
    int width = 0, height = 0, columns = 1;
    std::vector<unsigned char> pixels; ///< RGBA
};
Sheet PackFrames(const Animation& animation);

} // namespace SpartanToastMedia
