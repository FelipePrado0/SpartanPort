// SPDX-License-Identifier: GPL-2.0-or-later
// Save list pictures: icon0.png decoded to RGBA, bad or missing files rejected.
#include <cassert>
#include <cstdio>
#include <vector>

#include "../gpu/shim/spartanport_png.h"

// 2x2 RGB: red, green / blue, white.
static const unsigned char kPng[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44,
    0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x02, 0x00, 0x00, 0x00, 0xfd,
    0xd4, 0x9a, 0x73, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8,
    0xcf, 0xc0, 0xc0, 0x00, 0xc2, 0x0c, 0xff, 0x81, 0x00, 0x00, 0x1f, 0xee, 0x05, 0xfb, 0x0b,
    0xd9, 0x68, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};

int main() {
    SpartanPng::Image image;
    assert(SpartanPng::DecodeRgba(kPng, sizeof(kPng), image));
    assert(image.width == 2 && image.height == 2 && image.rgba.size() == 16);
    const unsigned char expected[16] = {255, 0, 0, 255, 0, 255, 0, 255,
                                        0, 0, 255, 255, 255, 255, 255, 255};
    for (int i = 0; i < 16; ++i) {
        assert(image.rgba[i] == expected[i]);
    }

    // Truncated data and data that is not a PNG fail and leave the image empty.
    SpartanPng::Image bad;
    assert(!SpartanPng::DecodeRgba(kPng, 40, bad) && bad.rgba.empty());
    const unsigned char junk[64] = {1, 2, 3};
    assert(!SpartanPng::DecodeRgba(junk, sizeof(junk), bad) && bad.rgba.empty());
    assert(!SpartanPng::DecodeRgba(nullptr, 0, bad));

    assert(!SpartanPng::LoadRgba("does/not/exist/icon0.png", bad) && bad.rgba.empty());
    std::puts("png-decode-test: ok");
    return 0;
}
