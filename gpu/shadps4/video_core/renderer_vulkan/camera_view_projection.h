// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: the main camera of a God of War III frame as one 4x4 view-projection matrix.
// The depth pre-pass binds it as the first 16 floats (column-major, clip = M * position) of one
// of its vertex shader constant buffers; which buffer differs per vertex shader, so a buffer is
// recognised by content (the right and up rows are orthogonal to the view direction and scaled
// by the screen's aspect ratio) and not by shader hash or slot. Header only, for unit tests.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace Vulkan::CameraVp {

using Matrix = std::array<float, 16>; ///< column-major: element(row, column) = m[4 * column + row]

inline float Element(const Matrix& m, int row, int column) {
    return m[4 * column + row];
}

/// How much 16 floats look like a perspective view-projection for a screen of the given aspect:
/// result = {|row0| / |row1| - 1 / aspect, cos(row0, row1), cos(row0, row3), cos(row1, row3)}
/// (rows of the 3x3 part, all absolute). False when a value is not finite or a row is empty.
inline bool Metrics(const float* m, float aspect, float* result) {
    for (int i = 0; i < 16; ++i) {
        if (!std::isfinite(m[i]) || std::abs(m[i]) > 1e7f) {
            return false;
        }
    }
    float rows[4][3];
    float norms[4];
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 3; ++c) {
            rows[r][c] = m[4 * c + r];
        }
        norms[r] = std::sqrt(rows[r][0] * rows[r][0] + rows[r][1] * rows[r][1] +
                             rows[r][2] * rows[r][2]);
    }
    if (norms[0] < 1e-3f || norms[1] < 1e-3f || norms[3] < 1e-3f) {
        return false;
    }
    const auto cosine = [&](int a, int b) {
        const float dot = rows[a][0] * rows[b][0] + rows[a][1] * rows[b][1] +
                          rows[a][2] * rows[b][2];
        return std::abs(dot) / (norms[a] * norms[b]);
    };
    result[0] = std::abs(norms[0] / norms[1] - 1.0f / aspect);
    result[1] = cosine(0, 1);
    result[2] = cosine(0, 3);
    result[3] = cosine(1, 3);
    return true;
}

/// Whether the 16 floats are a perspective view-projection for a `width` x `height` screen.
inline bool LooksLikeViewProjection(const float* m, float width, float height) {
    float v[4];
    return Metrics(m, width / height, v) && v[0] <= 0.02f && v[1] < 0.02f && v[2] < 0.02f &&
           v[3] < 0.02f;
}

/// Inverse of a general 4x4 matrix (double precision); false when singular.
inline bool Invert(const Matrix& m, Matrix& out) {
    double a[4][8];
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            a[r][c] = Element(m, r, c);
            a[r][4 + c] = r == c ? 1.0 : 0.0;
        }
    }
    for (int col = 0; col < 4; ++col) {
        int pivot = col;
        for (int r = col + 1; r < 4; ++r) {
            if (std::abs(a[r][col]) > std::abs(a[pivot][col])) {
                pivot = r;
            }
        }
        if (std::abs(a[pivot][col]) < 1e-12) {
            return false;
        }
        for (int c = 0; c < 8; ++c) {
            std::swap(a[col][c], a[pivot][c]);
        }
        const double scale = 1.0 / a[col][col];
        for (int c = 0; c < 8; ++c) {
            a[col][c] *= scale;
        }
        for (int r = 0; r < 4; ++r) {
            if (r == col) {
                continue;
            }
            const double factor = a[r][col];
            for (int c = 0; c < 8; ++c) {
                a[r][c] -= factor * a[col][c];
            }
        }
    }
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            out[4 * c + r] = float(a[r][4 + c]);
        }
    }
    return true;
}

/// previous * inverse(current): current clip space (x, y, depth, 1) to previous clip space.
inline bool Reprojection(const Matrix& previous, const Matrix& current, Matrix& out) {
    Matrix inverse;
    if (!Invert(current, inverse)) {
        return false;
    }
    // Multiplied in double: the inverse of a far-translated camera loses float precision.
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            double v = 0.0;
            for (int k = 0; k < 4; ++k) {
                v += double(Element(previous, r, k)) * double(Element(inverse, k, c));
            }
            out[4 * c + r] = float(v);
        }
    }
    return true;
}

/// Clip space z at the near plane is 0 and, for the infinite far plane this game uses,
/// z = w - near, so near = (w row, translation) - (z row, translation).
inline float NearPlane(const Matrix& m) {
    return Element(m, 3, 3) - Element(m, 2, 3);
}

inline float VerticalFov(const Matrix& m) {
    float y = 0.0f;
    for (int c = 0; c < 3; ++c) {
        y += Element(m, 1, c) * Element(m, 1, c);
    }
    return 2.0f * std::atan(1.0f / std::sqrt(y));
}

/// The matrices seen in a frame's depth pre-pass, counted. The main camera is the most frequent
/// one (a handful of draws may use another, e.g. a weapon or a map).
class Tally {
public:
    void Clear() {
        used = 0;
    }
    void Add(const float* m) {
        for (int i = 0; i < used; ++i) {
            if (Same(entries[i].matrix.data(), m)) {
                ++entries[i].count;
                return;
            }
        }
        if (used < int(entries.size())) {
            Entry& entry = entries[used++];
            std::copy(m, m + 16, entry.matrix.begin());
            entry.count = 1;
        }
    }
    bool Best(Matrix& out) const {
        int best = -1;
        for (int i = 0; i < used; ++i) {
            if (best < 0 || entries[i].count > entries[best].count) {
                best = i;
            }
        }
        if (best < 0) {
            return false;
        }
        out = entries[best].matrix;
        return true;
    }

private:
    struct Entry {
        Matrix matrix{};
        int count = 0;
    };
    static bool Same(const float* a, const float* b) {
        for (int i = 0; i < 16; ++i) {
            if (a[i] != b[i]) {
                return false;
            }
        }
        return true;
    }
    std::array<Entry, 4> entries{};
    int used = 0;
};

} // namespace Vulkan::CameraVp
