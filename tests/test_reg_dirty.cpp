// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <cstdio>
#include <vector>
#include "video_core/amdgpu/regs.h"

int main() {
    using namespace AmdGpu;
    Regs regs{};
    RegDirty dirty;
    std::vector<u32> visited;
    auto scan = [&] {
        visited.clear();
        dirty.ForEach([&](u32 block) { visited.push_back(block); });
    };
    scan();
    assert(visited.empty());
    dirty.Mark(0, 0);
    dirty.Mark(Regs::NumRegs, 1);
    scan();
    assert(visited.empty());
    // A range crossing bits 63/64, duplicate marks, and a clamped final block.
    dirty.Mark(63 * RegDirty::BlockWords + 31, 2);
    dirty.Mark(64 * RegDirty::BlockWords, 1);
    dirty.Mark(Regs::NumRegs - 1, 100);
    dirty.Mark(0, 1);
    scan();
    assert((visited == std::vector<u32>{0, 63, 64, RegDirty::NumBlocks - 1}));
    for (u32 word = 0; word < Regs::NumRegs; ++word) regs.reg_array[word] = word + 1;
    RegDelta delta;
    dirty.reset = true;
    delta.Capture(regs, dirty);
    assert(delta.reset && delta.blocks.size() == visited.size());
    assert(delta.words.size() == visited.size() * RegDirty::BlockWords);
    for (size_t i = 0; i < visited.size(); ++i) {
        assert(delta.blocks[i] == visited[i]);
        for (u32 word = 0; word < RegDirty::BlockWords; ++word)
            assert(delta.words[i * RegDirty::BlockWords + word] ==
                   visited[i] * RegDirty::BlockWords + word + 1);
    }
    dirty.Clear();
    scan();
    assert(visited.empty() && !dirty.reset);
    delta.Capture(regs, dirty);
    assert(!delta.reset && delta.blocks.empty() && delta.words.empty());
    // Full coverage must visit every block once and in increasing order.
    dirty.Mark(0, Regs::NumRegs);
    scan();
    assert(visited.size() == RegDirty::NumBlocks);
    for (u32 block = 0; block < RegDirty::NumBlocks; ++block) assert(visited[block] == block);
    dirty.Clear();
    dirty.MarkField(regs, regs.primitive_type);
    scan();
    const auto word = (reinterpret_cast<const u8*>(&regs.primitive_type) -
                       reinterpret_cast<const u8*>(regs.reg_array.data())) / sizeof(u32);
    assert((visited == std::vector<u32>{u32(word / RegDirty::BlockWords)}));
    std::puts("dirty register iteration and delta capture OK");
}
