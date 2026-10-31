// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: the SpartanPort depth pre-pass binds one column-major view-projection (vertex shader
// slot 1, floats 0-15; docs/upscaler.md). Camera motion takes the view (3x4 rows), its inverse
// and the projection terms separately: with an orthonormal view and a projection of rows
// (sx 0 0 0) (0 sy 0 0) (0 0 zs zo) (0 0 1 0), the rows of view-projection give all of them.
#pragma once

#include <array>
#include <cmath>

namespace Vulkan::ViewProjection {

struct Camera {
    std::array<float, 12> view{};     ///< world to view, 3x4 rows
    std::array<float, 12> inv_view{}; ///< view to world, 3x4 rows
    std::array<float, 4> proj{};      ///< x scale, y scale, z scale, z offset
    bool valid = false;
};

inline Camera Decompose(const float* m) {
    const auto row = [m](int r) { return std::array<float, 4>{m[r], m[4 + r], m[8 + r], m[12 + r]}; };
    const auto length = [](const std::array<float, 4>& v) {
        return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    };
    const auto r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
    const float sx = length(r0), sy = length(r1), sz = length(r3);
    Camera c;
    // w = view z: its axis is a unit vector. Full-screen passes bind identity or nothing.
    if (std::abs(sz - 1.0f) > 1e-2f || sx < 0.1f || sy < 0.1f || sy / sx > 4.0f || sx / sy > 4.0f) {
        return c;
    }
    const float zs = r2[0] * r3[0] + r2[1] * r3[1] + r2[2] * r3[2];
    const float zo = r2[3] - zs * r3[3];
    for (int i = 0; i < 4; ++i) {
        c.view[i] = r0[i] / sx;
        c.view[4 + i] = r1[i] / sy;
        c.view[8 + i] = r3[i];
    }
    // Inverse of [R | t]: [R^T | -R^T t].
    for (int r = 0; r < 3; ++r) {
        for (int k = 0; k < 3; ++k) {
            c.inv_view[r * 4 + k] = c.view[k * 4 + r];
        }
        c.inv_view[r * 4 + 3] = -(c.view[r] * c.view[3] + c.view[4 + r] * c.view[7] +
                                  c.view[8 + r] * c.view[11]);
    }
    c.proj = {sx, sy, zs, zo};
    c.valid = zo < 0.0f;
    return c;
}

} // namespace Vulkan::ViewProjection
