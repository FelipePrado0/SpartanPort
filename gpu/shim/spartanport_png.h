// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: PNG pictures for the overlay (save list icons), decoded by FFmpeg to RGBA8.

#pragma once

#include <cstddef>
#include <vector>

namespace SpartanPng {

struct Image {
    int width = 0, height = 0;
    std::vector<unsigned char> rgba; // width * height * 4, rows top to bottom
};

/// False (and `out` empty) when the data is not a PNG FFmpeg can decode.
bool DecodeRgba(const unsigned char* data, size_t size, Image& out);
/// Reads the file and decodes it; false when it is missing or bad.
bool LoadRgba(const char* path, Image& out);

} // namespace SpartanPng
