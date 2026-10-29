// SPDX-License-Identifier: GPL-2.0-or-later
// Ring of GPU checkpoint records behind VK_NV_device_diagnostic_checkpoints markers
// (vk_checkpoint_ring.h). No game, Vulkan device or window required.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "video_core/renderer_vulkan/vk_checkpoint_ring.h"

using namespace Vulkan;

int main() {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    // A marker is the record's serial, never a pointer the driver could follow; serial 0 is
    // "no marker".
    assert(EncodeMarker(0) == nullptr);
    assert(DecodeMarker(nullptr) == 0);
    assert(DecodeMarker(EncodeMarker(42)) == 42);

    // Push hands out one marker per record; Find gives the record back.
    CheckpointRing ring{4};
    CheckpointRecord draw{};
    draw.kind = CheckpointKind::Draw;
    draw.tick = 7;
    draw.stage_hashes[u32(CheckpointStage::Vertex)] = 0x1111;
    draw.stage_hashes[u32(CheckpointStage::Fragment)] = 0x2222;
    draw.x = 300;
    draw.y = 1;
    const void* first = ring.Push(draw);
    assert(first != nullptr);
    assert(DecodeMarker(first) == 1);
    const auto found = ring.Find(first);
    assert(found.has_value());
    assert(found->serial == 1);
    assert(found->kind == CheckpointKind::Draw);
    assert(found->tick == 7);
    assert(found->x == 300);

    // Unknown or empty markers find nothing.
    assert(!ring.Find(nullptr).has_value());
    assert(!ring.Find(EncodeMarker(99)).has_value());

    // The ring keeps `capacity` records: the fifth push overwrites the first.
    CheckpointRecord dispatch{};
    dispatch.kind = CheckpointKind::Dispatch;
    dispatch.stage_hashes[u32(CheckpointStage::Compute)] = 0x3333;
    dispatch.x = 12;
    dispatch.y = 1;
    dispatch.z = 1;
    const void* second = ring.Push(dispatch);
    ring.Push(dispatch);
    ring.Push(dispatch);
    const void* fifth = ring.Push(dispatch);
    assert(DecodeMarker(fifth) == 5);
    assert(!ring.Find(first).has_value()); // overwritten
    assert(ring.Find(second).has_value());
    assert(ring.Find(fifth)->serial == 5);

    // Last(n): the newest n records, oldest first, never more than the ring holds.
    const auto last = ring.Last(2);
    assert(last.size() == 2);
    assert(last[0].serial == 4);
    assert(last[1].serial == 5);
    assert(ring.Last(10).size() == 4);

    // Around(marker, before, after): the records whose serials surround the marker's, in order,
    // skipping the ones the ring no longer holds.
    const auto around = ring.Around(EncodeMarker(4), 1, 1);
    assert(around.size() == 3);
    assert(around[0].serial == 3 && around[1].serial == 4 && around[2].serial == 5);
    assert(ring.Around(EncodeMarker(2), 5, 5).size() == 4); // serial 1 is gone, 6 never existed
    assert(ring.Around(nullptr, 2, 2).empty());

    // One line per record: kind, serial, submission tick, the hashes that matter, the sizes; a
    // draw also names its primitive type and whether the port's tessellation stages expand it.
    assert(FormatCheckpoint(*found) == "draw #1 tick 7 vs 0x1111 fs 0x2222 300 indices x1 prim 0");
    CheckpointRecord rect{};
    rect.kind = CheckpointKind::Draw;
    rect.serial = 11;
    rect.stage_hashes[u32(CheckpointStage::Vertex)] = 0x1111;
    rect.x = 3;
    rect.y = 1;
    rect.z = DrawPrimitive(17, true);
    assert(FormatCheckpoint(rect) == "draw #11 tick 0 vs 0x1111 fs 0x0 3 indices x1 prim 17 tess");
    CheckpointRecord pass{};
    pass.kind = CheckpointKind::RenderPass;
    pass.serial = 12;
    pass.tick = 7;
    pass.x = 1280;
    pass.y = 720;
    pass.z = RenderPassLayout(2, true, false);
    assert(FormatCheckpoint(pass) == "render-pass #12 tick 7 1280x720 2 color depth");
    pass.z = RenderPassLayout(0, true, true);
    assert(FormatCheckpoint(pass) == "render-pass #12 tick 7 1280x720 0 color depth stencil");
    pass.z = RenderPassLayout(1, false, false);
    assert(FormatCheckpoint(pass) == "render-pass #12 tick 7 1280x720 1 color");
    pass.kind = CheckpointKind::RenderPassEnd;
    assert(FormatCheckpoint(pass) == "render-pass-end #12 tick 7");
    assert(FormatCheckpoint(*ring.Find(fifth)) == "dispatch #5 tick 0 cs 0x3333 12x1x1");
    CheckpointRecord detile{};
    detile.kind = CheckpointKind::Detile;
    detile.serial = 9;
    detile.tick = 3;
    detile.stage_hashes[u32(CheckpointStage::Compute)] = 0xabc;
    detile.x = 4096;
    detile.y = 1024;
    detile.z = 87383;
    assert(FormatCheckpoint(detile) == "detile #9 tick 3 image 0xabc 4096x1024 87383 groups");
    CheckpointRecord submit{};
    submit.kind = CheckpointKind::Submit;
    submit.serial = 10;
    submit.tick = 3;
    assert(FormatCheckpoint(submit) == "submit #10 tick 3");

    // The driver answers one (stage, marker) pair per pipeline stage it tracks; describe each.
    const std::vector<CheckpointStageData> data = {
        {"TopOfPipe", fifth},
        {"BottomOfPipe", second},
        {"Transfer", first},
        {"ComputeShader", nullptr},
        {"Host", EncodeMarker(99)},
    };
    const std::string text = DescribeCheckpointData(ring, data);
    assert(text == "  TopOfPipe: dispatch #5 tick 0 cs 0x3333 12x1x1\n"
                   "  BottomOfPipe: dispatch #2 tick 0 cs 0x3333 12x1x1\n"
                   "  Transfer: #1 (overwritten)\n"
                   "  ComputeShader: none\n"
                   "  Host: #99 (unknown)\n");
    assert(DescribeCheckpointData(ring, {}) == "  (no checkpoint data)\n");

    // SPARTAN_GPU_CHECKPOINTS: unset or empty keeps the default (on for Intel's Windows driver,
    // off elsewhere); "1" turns the markers on anywhere, anything else turns them off.
    assert(CheckpointsWanted(nullptr, true) == true);
    assert(CheckpointsWanted("", true) == true);
    assert(CheckpointsWanted(nullptr, false) == false);
    assert(CheckpointsWanted("1", false) == true);
    assert(CheckpointsWanted("0", true) == false);
    assert(CheckpointsWanted("yes", true) == false);

    std::puts("checkpoint-ring-test: ok");
    return 0;
}
