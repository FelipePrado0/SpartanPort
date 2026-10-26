// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: VK_NV_device_diagnostic_checkpoints around the game's GPU work
// (vk_checkpoint_ring.h). A marker goes into the command buffer before every draw, dispatch and
// tiling dispatch and at the end of every submission; after a device loss the queue reports the
// last marker each pipeline stage reached, which names the work the GPU was on when it hung
// (#334). On by default on Intel's Windows driver, SPARTAN_GPU_CHECKPOINTS=1/0 forces it
// (vk_instance.cpp); Dump runs before each "Device lost" assertion.

#pragma once

#include <atomic>
#include <cstdio>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include "shader_recompiler/info.h"
#include "video_core/renderer_vulkan/vk_checkpoint_ring.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {

/// The record of a draw or dispatch of `stages`, a Pipeline::GetStages() span: the five graphics
/// stages in Shader::SwStage order, or the one compute stage.
inline CheckpointRecord StageCheckpoint(std::span<const Shader::Info* const> stages,
                                        bool is_compute, CheckpointKind kind, u64 tick, u32 x,
                                        u32 y, u32 z) {
    CheckpointRecord record{};
    record.kind = kind;
    record.tick = tick;
    record.x = x;
    record.y = y;
    record.z = z;
    const std::size_t first = is_compute ? std::size_t(CheckpointStage::Compute) : 0;
    for (std::size_t i = 0; i < stages.size() && first + i < record.stage_hashes.size(); ++i) {
        if (stages[i]) {
            record.stage_hashes[first + i] = stages[i]->pgm_hash;
        }
    }
    return record;
}

class Checkpoints {
public:
    static Checkpoints& Get() {
        static Checkpoints checkpoints;
        return checkpoints;
    }

    /// Whether markers are recorded: the extension is on (Instance::CreateDevice).
    static bool Enabled() {
        return Get().enabled.load(std::memory_order_relaxed);
    }

    void Enable(bool on) {
        enabled.store(on, std::memory_order_relaxed);
    }

    /// Stores `record` and writes its marker into `cmdbuf`. Callers check Enabled() first.
    void Mark(vk::CommandBuffer cmdbuf, const CheckpointRecord& record) {
        const void* marker;
        {
            std::scoped_lock lock{mutex};
            marker = ring.Push(record);
        }
        cmdbuf.setCheckpointNV(marker);
    }

    /// Prints what the GPU was on when `queue` lost the device, for last_run.log, before the
    /// assertion at `where`.
    void Dump(vk::Queue queue, const char* where) {
        if (!Enabled()) {
            std::printf("GPU: device lost at %s; checkpoints off (SPARTAN_GPU_CHECKPOINTS=1 turns "
                        "them on)\n",
                        where);
            std::fflush(stdout);
            return;
        }
        const std::vector<vk::CheckpointDataNV> data = queue.getCheckpointDataNV();
        std::vector<std::string> names;
        names.reserve(data.size());
        for (const auto& entry : data) {
            names.push_back(vk::to_string(entry.stage));
        }
        std::vector<CheckpointStageData> stages;
        stages.reserve(data.size());
        for (std::size_t i = 0; i < data.size(); ++i) {
            stages.push_back({names[i].c_str(), data[i].pCheckpointMarker});
        }
        std::scoped_lock lock{mutex};
        std::printf("GPU: device lost at %s; last checkpoint each stage reached:\n", where);
        std::fputs(DescribeCheckpointData(ring, stages).c_str(), stdout);
        const auto last = ring.Last(16);
        std::printf("GPU: last %zu checkpoints recorded by the CPU:\n", last.size());
        for (const auto& record : last) {
            std::printf("  %s\n", FormatCheckpoint(record).c_str());
        }
        std::fflush(stdout);
    }

private:
    std::mutex mutex;
    CheckpointRing ring{65536};
    std::atomic<bool> enabled{false};
};

} // namespace Vulkan
