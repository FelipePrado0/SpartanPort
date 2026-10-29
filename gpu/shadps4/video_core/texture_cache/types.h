// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// spartanport: clamp view ranges with standard min/max helpers.
#include <algorithm>
#include "common/slot_vector.h"
#include "common/types.h"

namespace VideoCore {

using ImageId = Common::SlotId;
using ImageViewId = Common::SlotId;

struct Offset2D {
    s32 x;
    s32 y;
};

struct Offset3D {
    s32 x;
    s32 y;
    s32 z;
};

struct Region2D {
    Offset2D start;
    Offset2D end;
};

struct Extent2D {
    u32 width;
    u32 height;

    bool operator==(const Extent2D& other) const {
        return width == other.width && height == other.height;
    }
};

struct Extent3D {
    u32 width;
    u32 height;
    u32 depth;

    bool operator==(const Extent3D& other) const {
        return width == other.width && height == other.height && depth == other.depth;
    }
};

struct SubresourceLayers {
    s32 base_level = 0;
    s32 base_layer = 0;
    s32 num_layers = 1;
};

struct SubresourceBase {
    u32 level = 0;
    u32 layer = 0;

    auto operator<=>(const SubresourceBase&) const = default;
};

struct SubresourceExtent {
    u32 levels = 1;
    u32 layers = 1;

    auto operator<=>(const SubresourceExtent&) const = default;

    // spartanport: check both dimensions so extra mips cannot hide missing layers.
    bool Contains(const SubresourceExtent& other) const noexcept {
        return levels >= other.levels && layers >= other.layers;
    }
};

struct SubresourceRange {
    SubresourceBase base;
    SubresourceExtent extent;

    auto operator<=>(const SubresourceRange&) const = default;

    // spartanport: bound views to the resolved image, keeping counts positive without overflow.
    bool ClampTo(const SubresourceExtent& resources) noexcept {
        const u32 levels = std::max(resources.levels, 1u);
        const u32 layers = std::max(resources.layers, 1u);
        if (base.level < levels && base.layer < layers && extent.levels > 0 && extent.layers > 0 &&
            extent.levels <= levels - base.level && extent.layers <= layers - base.layer) {
            return false;
        }
        base.level = std::min(base.level, levels - 1);
        base.layer = std::min(base.layer, layers - 1);
        extent.levels = std::clamp(extent.levels, 1u, levels - base.level);
        extent.layers = std::clamp(extent.layers, 1u, layers - base.layer);
        return true;
    }
};

struct ImageCopy {
    SubresourceLayers src_subresource;
    SubresourceLayers dst_subresource;
    Offset3D src_offset;
    Offset3D dst_offset;
    Extent3D extent;
};

struct BufferImageCopy {
    std::size_t buffer_offset;
    std::size_t buffer_size;
    u32 buffer_row_length;
    u32 buffer_image_height;
    SubresourceLayers image_subresource;
    Offset3D image_offset;
    Extent3D image_extent;
};

struct BufferCopy {
    u64 src_offset;
    u64 dst_offset;
    std::size_t size;
};

} // namespace VideoCore
