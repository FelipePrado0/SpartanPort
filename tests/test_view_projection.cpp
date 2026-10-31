// SPDX-License-Identifier: GPL-2.0-or-later
// The SpartanPort depth pre-pass camera (one column-major view-projection) split into the view and the
// projection terms the camera motion shader takes.
#include <cassert>
#include <cmath>
#include <cstdio>
#include "video_core/renderer_vulkan/vk_view_projection.h"

static bool near(float a, float b, float eps = 1e-3f) {
    return std::abs(a - b) <= eps;
}

int main() {
    using Vulkan::ViewProjection::Decompose;
    // Pass #7 vertex shader 36a1637dcb4c9453 slot 1, floats 0-15 (frame_20261024-233122_3.txt).
    const float captured[16] = {1.02068f,    0.158421f, 0.853917f,  0.853875f,  -0.0240873f, 3.49567f,
                                -0.0455286f, -0.0455264f, 1.68304f, -0.046045f, -0.51851f,   -0.518484f,
                                -64.9951f,   -1418.23f, -39.8335f,  -39.3315f};
    const auto camera = Decompose(captured);
    assert(camera.valid);
    assert(near(camera.proj[0], 1.9686f) && near(camera.proj[1], 3.4993f));
    assert(near(camera.proj[2], 1.0f, 1e-2f) && camera.proj[3] < 0.0f && camera.proj[3] > -1.0f);

    // A world point through view then projection lands where the game's matrix puts it.
    const float p[3] = {12.0f, -3.5f, 40.0f};
    float view[3];
    for (int r = 0; r < 3; ++r) {
        view[r] = camera.view[r * 4] * p[0] + camera.view[r * 4 + 1] * p[1] +
                  camera.view[r * 4 + 2] * p[2] + camera.view[r * 4 + 3];
    }
    float clip[4];
    for (int r = 0; r < 4; ++r) {
        clip[r] = captured[r] * p[0] + captured[4 + r] * p[1] + captured[8 + r] * p[2] + captured[12 + r];
    }
    assert(near(view[2], clip[3], 1e-2f));
    assert(near(view[0] * camera.proj[0], clip[0], 1e-2f));
    assert(near(view[1] * camera.proj[1], clip[1], 1e-2f));
    assert(near(view[2] * camera.proj[2] + camera.proj[3], clip[2], 1e-2f));

    // inv_view undoes view.
    float back[3];
    for (int r = 0; r < 3; ++r) {
        back[r] = camera.inv_view[r * 4] * view[0] + camera.inv_view[r * 4 + 1] * view[1] +
                  camera.inv_view[r * 4 + 2] * view[2] + camera.inv_view[r * 4 + 3];
    }
    assert(near(back[0], p[0], 1e-2f) && near(back[1], p[1], 1e-2f) && near(back[2], p[2], 1e-2f));

    // Pass #1's full-screen clear binds no camera: identity, and zeros, are rejected.
    const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const float zeros[16] = {};
    assert(!Decompose(identity).valid);
    assert(!Decompose(zeros).valid);
    std::puts("view-projection test: ok");
}
