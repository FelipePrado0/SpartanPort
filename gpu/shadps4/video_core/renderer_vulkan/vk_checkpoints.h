// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: VK_NV_device_diagnostic_checkpoints around the game's GPU work
// (vk_checkpoint_ring.h). A marker goes into the command buffer before every draw, dispatch and
// tiling dispatch and at the end of every submission; after a device loss the queue reports the
// last marker each pipeline stage reached, which names the work the GPU was on when it hung
// (#334). On by default on Intel's Windows driver, SPARTAN_GPU_CHECKPOINTS=1/0 forces it
// (vk_instance.cpp); Dump runs before each "Device lost" assertion.
//
// Push runs on the thread that records the command (the GPU thread, in command order), so a
// Record closure captures only the marker; with the markers off a draw costs one atomic load.

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
#include "video_core/renderer_vulkan/vk_scheduler.h"

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
        return enabled.load(std::memory_order_relaxed);
    }

    static void Enable(bool on) {
        enabled.store(on, std::memory_order_relaxed);
    }

    /// Stores `record` and returns its marker. Callers check Enabled() first.
    const void* Push(const CheckpointRecord& record) {
        std::scoped_lock lock{mutex};
        return ring.Push(record);
    }

    /// Writes `marker` into `cmdbuf` ahead of the command it names; a null marker writes nothing.
    static void Set(vk::CommandBuffer cmdbuf, const void* marker) {
        if (marker) {
            cmdbuf.setCheckpointNV(marker);
        }
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
        // vkGetQueueCheckpointDataNV needs the queue externally synchronized: the submit lock,
        // recursive because SubmitExecution and the semaphore refresh under it get here too.
        std::scoped_lock queue_lock{Scheduler::submit_mutex};
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
        // The commands around the last one the front of the pipe reached: the hang is between
        // that marker and the next.
        const void* hung = nullptr;
        for (const auto& entry : data) {
            if (entry.stage == vk::PipelineStageFlagBits::eTopOfPipe &&
                DecodeMarker(entry.pCheckpointMarker) > DecodeMarker(hung)) {
                hung = entry.pCheckpointMarker;
            }
        }
        if (hung) {
            std::printf("GPU: checkpoints around #%llu (> marks it):\n",
                        static_cast<unsigned long long>(DecodeMarker(hung)));
            for (const auto& record : ring.Around(hung, 12, 4)) {
                std::printf("%s %s\n", record.serial == DecodeMarker(hung) ? " >" : "  ",
                            FormatCheckpoint(record).c_str());
            }
        } else {
            std::puts("GPU: no TopOfPipe checkpoint reported");
        }
        const auto last = ring.Last(16);
        std::printf("GPU: last %zu checkpoints recorded by the CPU:\n", last.size());
        for (const auto& record : last) {
            std::printf("  %s\n", FormatCheckpoint(record).c_str());
        }
        std::fflush(stdout);
    }

private:
    static inline std::atomic<bool> enabled{false};
    std::mutex mutex;
    CheckpointRing ring{65536};
};

} // namespace Vulkan
