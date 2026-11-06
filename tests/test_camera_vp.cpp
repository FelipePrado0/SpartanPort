// SPDX-License-Identifier: GPL-2.0-or-later
// Camera view-projection recognition and reprojection (renderer_vulkan/camera_view_projection.h),
// with the matrices of a real God of War III Remastered frame capture (depth pre-pass).
#include <cassert>
#include <cmath>
#include <cstdio>

#include "video_core/renderer_vulkan/camera_view_projection.h"

using namespace Vulkan::CameraVp;

namespace {
// Pre-pass vertex constants slot 1 of the capture, floats 0-15 (view-projection) ...
const float kCamera[16] = {1.92307f, 0.00184878f, -0.21362f, -0.213609f,
                           0.0222797f, 3.49418f, 0.0542342f, 0.0542315f,
                           -0.4199f, 0.193866f, -0.975462f, -0.975413f,
                           -113.489f, -1353.24f, 55.6436f, 56.1409f};
// ... and floats 16-31 (the world matrix of the draw, not a camera).
const float kWorld[16] = {4.74349f, -0.0405886f, -3.67391f, 0.0f, 0.00895813f, 5.99974f,
                          -0.0547124f, 0.0f, 3.67413f, 0.037771f, 4.74335f, 0.0f,
                          17.8158f, -35.1237f, 53.5653f, 1.0f};
// Slot 0 of the same pass starts with identity and scale matrices.
const float kIdentity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

Matrix Make(const float* m) {
    Matrix out;
    std::copy(m, m + 16, out.begin());
    return out;
}

// Pixel of a world position: clip = M * (x, y, z, 1), then depth = z / w.
void Project(const Matrix& m, const float p[3], float& x, float& y, float& depth) {
    float c[4];
    for (int r = 0; r < 4; ++r) {
        c[r] = Element(m, r, 0) * p[0] + Element(m, r, 1) * p[1] + Element(m, r, 2) * p[2] +
               Element(m, r, 3);
    }
    x = c[0] / c[3];
    y = c[1] / c[3];
    depth = c[2] / c[3];
}
} // namespace

int main() {
    assert(LooksLikeViewProjection(kCamera, 1920, 1080));
    assert(!LooksLikeViewProjection(kCamera, 1920, 1200)); // wrong aspect
    assert(!LooksLikeViewProjection(kWorld, 1920, 1080));
    assert(!LooksLikeViewProjection(kIdentity, 1920, 1080));
    float nan[16] = {};
    nan[0] = nanf("");
    assert(!LooksLikeViewProjection(nan, 1920, 1080));

    const Matrix camera = Make(kCamera);
    assert(std::abs(NearPlane(camera) - 0.4973f) < 1e-3f);
    const float fov = VerticalFov(camera) * 180.0f / 3.14159265f;
    assert(fov > 30.0f && fov < 34.0f);

    // Depth is z / w = 1 - near / w: 0 at the near plane, towards 1 far away.
    // A world point seen at ndc (0.3, -0.2) with depth 0.95: unprojected with the inverse.
    Matrix inverse;
    assert(Invert(camera, inverse));
    float unprojected[4];
    const float screen[4] = {0.3f, -0.2f, 0.95f, 1.0f};
    for (int r = 0; r < 4; ++r) {
        unprojected[r] = 0;
        for (int c = 0; c < 4; ++c) {
            unprojected[r] += Element(inverse, r, c) * screen[c];
        }
    }
    const float point[3] = {unprojected[0] / unprojected[3], unprojected[1] / unprojected[3],
                            unprojected[2] / unprojected[3]};
    float x, y, depth;
    Project(camera, point, x, y, depth);

    // A camera moved by a small translation and rotation: reprojecting the current pixel with
    // the matching depth must land where the previous camera sees the same world point.
    Matrix previous = camera;
    previous[12] += 3.0f;
    previous[13] -= 1.5f;
    previous[0] += 0.02f;
    Matrix reproject;
    assert(Reprojection(previous, camera, reproject));
    float expected_x, expected_y, expected_depth;
    Project(previous, point, expected_x, expected_y, expected_depth);
    float h[4] = {x, y, depth, 1.0f};
    float out[4];
    for (int r = 0; r < 4; ++r) {
        out[r] = 0;
        for (int c = 0; c < 4; ++c) {
            out[r] += Element(reproject, r, c) * h[c];
        }
    }
    assert(std::abs(out[0] / out[3] - expected_x) < 2e-3f);
    assert(std::abs(out[1] / out[3] - expected_y) < 2e-3f);
    assert(std::abs(out[2] / out[3] - expected_depth) < 2e-3f);

    // The sky (depth 1, a direction) still reprojects: the camera's rotation only.
    h[2] = 1.0f;
    for (int r = 0; r < 4; ++r) {
        out[r] = 0;
        for (int c = 0; c < 4; ++c) {
            out[r] += Element(reproject, r, c) * h[c];
        }
    }
    assert(std::isfinite(out[0]) && std::isfinite(out[3]));

    // A singular matrix has no reprojection.
    Matrix zero{};
    assert(!Reprojection(camera, zero, reproject));

    Tally tally;
    Matrix best;
    assert(!tally.Best(best));
    tally.Add(kWorld);
    tally.Add(kCamera);
    tally.Add(kCamera);
    assert(tally.Best(best) && best == camera);
    tally.Clear();
    assert(!tally.Best(best));
    std::printf("camera view-projection test passed\n");
    return 0;
}
