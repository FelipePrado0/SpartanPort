// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: records behind VK_NV_device_diagnostic_checkpoints markers (vk_checkpoints.h).
//
// A marker is the record's serial number cast to a pointer, never an address: the driver only
// stores it and hands it back from vkGetQueueCheckpointDataNV after a device loss, and a
// serial survives the ring reusing its slot (Find then reports the record as overwritten).
// No Vulkan dependency, so the ring is unit-tested on its own (tests/test_checkpoint_ring.cpp).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "common/types.h"

namespace Vulkan {

enum class CheckpointKind : std::uint8_t {
    Draw,
    DrawIndirect,
    Dispatch,
    DispatchIndirect,
    Detile,
    Tile,
    Submit,
};

/// Index into CheckpointRecord::stage_hashes; the order of Shader::SwStage.
enum class CheckpointStage : std::uint8_t {
    Fragment,
    TessellationControl,
    TessellationEval,
    Vertex,
    Geometry,
    Compute,
    Count,
};

struct CheckpointRecord {
    std::uint64_t serial{};
    std::uint64_t tick{}; ///< Submission tick the command belongs to.
    std::array<std::uint64_t, std::size_t(CheckpointStage::Count)> stage_hashes{};
    std::uint32_t x{}, y{}, z{}; ///< Draw: indices, instances. Dispatch: groups. Tiling: w, h, groups.
    CheckpointKind kind{};
};

inline const void* EncodeMarker(std::uint64_t serial) {
    return reinterpret_cast<const void*>(static_cast<std::uintptr_t>(serial));
}

inline std::uint64_t DecodeMarker(const void* marker) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(marker));
}

class CheckpointRing {
public:
    explicit CheckpointRing(std::size_t capacity) : slots(capacity) {}

    /// Stores `record` under the next serial and returns its marker.
    const void* Push(CheckpointRecord record) {
        record.serial = next_serial++;
        slots[record.serial % slots.size()] = record;
        return EncodeMarker(record.serial);
    }

    /// The record behind `marker`, unless the marker is empty, unknown or its slot was reused.
    std::optional<CheckpointRecord> Find(const void* marker) const {
        const std::uint64_t serial = DecodeMarker(marker);
        if (serial == 0 || serial >= next_serial) {
            return std::nullopt;
        }
        const CheckpointRecord& slot = slots[serial % slots.size()];
        if (slot.serial != serial) {
            return std::nullopt;
        }
        return slot;
    }

    /// The newest `count` records still in the ring, oldest first.
    std::vector<CheckpointRecord> Last(std::size_t count) const {
        const std::uint64_t pushed = next_serial - 1;
        count = std::min({count, slots.size(), static_cast<std::size_t>(pushed)});
        std::vector<CheckpointRecord> result;
        result.reserve(count);
        for (std::uint64_t serial = pushed - count + 1; serial <= pushed; ++serial) {
            result.push_back(slots[serial % slots.size()]);
        }
        return result;
    }

private:
    std::vector<CheckpointRecord> slots;
    std::uint64_t next_serial{1};
};

inline std::string FormatCheckpoint(const CheckpointRecord& record) {
    const auto hash = [&](CheckpointStage stage) {
        return static_cast<unsigned long long>(record.stage_hashes[std::size_t(stage)]);
    };
    char buf[256];
    const auto head = [&](const char* kind) {
        return std::snprintf(buf, sizeof(buf), "%s #%llu tick %llu", kind,
                             static_cast<unsigned long long>(record.serial),
                             static_cast<unsigned long long>(record.tick));
    };
    const auto graphics_stages = [&](int n) {
        n += std::snprintf(buf + n, sizeof(buf) - n, " vs 0x%llx fs 0x%llx",
                           hash(CheckpointStage::Vertex), hash(CheckpointStage::Fragment));
        if (hash(CheckpointStage::TessellationControl)) {
            n += std::snprintf(buf + n, sizeof(buf) - n, " tcs 0x%llx tes 0x%llx",
                               hash(CheckpointStage::TessellationControl),
                               hash(CheckpointStage::TessellationEval));
        }
        if (hash(CheckpointStage::Geometry)) {
            n += std::snprintf(buf + n, sizeof(buf) - n, " gs 0x%llx",
                               hash(CheckpointStage::Geometry));
        }
        return n;
    };
    int n = 0;
    switch (record.kind) {
    case CheckpointKind::Draw:
        n = graphics_stages(head("draw"));
        std::snprintf(buf + n, sizeof(buf) - n, " %u indices x%u", record.x, record.y);
        break;
    case CheckpointKind::DrawIndirect:
        n = graphics_stages(head("draw-indirect"));
        std::snprintf(buf + n, sizeof(buf) - n, " %u max", record.x);
        break;
    case CheckpointKind::Dispatch:
        n = head("dispatch");
        std::snprintf(buf + n, sizeof(buf) - n, " cs 0x%llx %ux%ux%u", hash(CheckpointStage::Compute),
                      record.x, record.y, record.z);
        break;
    case CheckpointKind::DispatchIndirect:
        n = head("dispatch-indirect");
        std::snprintf(buf + n, sizeof(buf) - n, " cs 0x%llx", hash(CheckpointStage::Compute));
        break;
    case CheckpointKind::Detile:
    case CheckpointKind::Tile:
        n = head(record.kind == CheckpointKind::Detile ? "detile" : "tile");
        std::snprintf(buf + n, sizeof(buf) - n, " image 0x%llx %ux%u %u groups",
                      hash(CheckpointStage::Compute), record.x, record.y, record.z);
        break;
    case CheckpointKind::Submit:
        head("submit");
        break;
    }
    return buf;
}

/// Whether the markers are recorded. SPARTAN_GPU_CHECKPOINTS=1 turns them on anywhere, any other
/// value turns them off; unset or empty keeps the default: on for Intel's Windows driver, whose
/// device losses (#334) are the reason they exist, off elsewhere.
inline bool CheckpointsWanted(const char* env, bool intel_windows) {
    if (env && *env) {
        return env[0] == '1' && env[1] == '\0';
    }
    return intel_windows;
}

/// One entry of vkGetQueueCheckpointDataNV: the stage name as vk::to_string prints it and the
/// last marker that stage executed.
struct CheckpointStageData {
    const char* stage;
    const void* marker;
};

/// One line per stage, for the log after a device loss.
inline std::string DescribeCheckpointData(const CheckpointRing& ring,
                                          std::span<const CheckpointStageData> data) {
    if (data.empty()) {
        return "  (no checkpoint data)\n";
    }
    std::string text;
    for (const auto& [stage, marker] : data) {
        text += "  ";
        text += stage;
        text += ": ";
        if (const auto record = ring.Find(marker)) {
            text += FormatCheckpoint(*record);
        } else if (marker == nullptr) {
            text += "none";
        } else {
            text += "#" + std::to_string(DecodeMarker(marker)) + " (overwritten)";
        }
        text += '\n';
    }
    return text;
}

} // namespace Vulkan
