// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: where the draw recording thread (stage B, vk_draw_pipe.h) spends its time, by section
// (SPARTAN_STAGE_PROFILE=1). Marks are laps: each charges the cycles since the previous mark to one
// section, so a section never counts twice and the marks cost one rdtsc each. Printed every 5 s
// next to the "Draw pipe:" line, as each section's share of the window and microseconds per draw.
// Off by default; then every mark is a branch on a constant.

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace SpartanStageProfile {

enum Section : unsigned {
    Packet,      ///< reading the packet: register blocks, user data, ring stages
    Pending,     ///< operations queued for this thread, frame capture poll
    Frame,       ///< upscaler, frame boundary, motion bookkeeping before the draw
    RenderState, ///< PrepareRenderState: render targets, depth, viewport state
    Bind,        ///< BindResources: textures and buffers
    Begin,       ///< BeginRendering
    Inputs,      ///< vertex and index buffers
    Barriers,    ///< FlushBarriers
    Record,      ///< the rest of the draw: motion vectors, push constants, the draw command
    Dispatch,    ///< compute dispatches
    Special,     ///< draws FilterDraw handles (clears, resolves, depth copies)
    Task,        ///< other work run on stage B
    // Stage A (the GPU command thread) in its own totals: the rest of its time is PM4 decoding
    // and register writes, and the wait for stage B the "Draw pipe:" line already reports.
    APipeline,   ///< graphics pipeline selection (GetGraphicsPipeline, prepared draw check)
    ARing,       ///< small shader constants copied into the constant ring
    APacket,     ///< the rest of handing a draw to stage B: register blocks, user data, the ring
    Count,
};

inline constexpr const char* Names[Count] = {"packet", "pending",   "frame",    "render state",
                                             "bind",   "begin",     "inputs",   "barriers",
                                             "record", "dispatch",  "special",  "task",
                                             "pipeline", "constants", "hand-off"};

/// Cycles per section and draws, each Totals written by one thread only (relaxed: the reporting
/// thread reads totals that may be one draw old, which a 5 s report does not notice).
struct Totals {
    std::array<uint64_t, Count> cycles{};
    uint64_t draws = 0;
};

class Laps {
public:
    explicit Laps(Totals& totals_) : totals{totals_} {}
    void Start(uint64_t now) {
        last = now;
    }
    /// Charges the time since the previous mark to `section`.
    void Lap(Section section, uint64_t now) {
        totals.cycles[section] += now - last;
        last = now;
    }
    /// A whole packet that is one section.
    void Whole(Section section, uint64_t start, uint64_t end) {
        totals.cycles[section] += end - start;
        last = end;
    }

private:
    Totals& totals;
    uint64_t last = 0;
};

/// One line: sections by share of the window (under 0.05% left out), with microseconds per draw.
inline std::string Format(const Totals& delta, double window_cycles, double seconds,
                          const char* label = "Stage B profile:") {
    if (delta.draws == 0 || window_cycles <= 0 || seconds <= 0) {
        return std::string(label) + " no draws in this window";
    }
    const double hz = window_cycles / seconds;
    std::array<unsigned, Count> order{};
    for (unsigned i = 0; i < Count; ++i) {
        order[i] = i;
    }
    std::stable_sort(order.begin(), order.end(),
                     [&](unsigned a, unsigned b) { return delta.cycles[a] > delta.cycles[b]; });
    std::string line = label;
    double measured = 0;
    char part[96];
    for (const unsigned i : order) {
        const double share = 100.0 * double(delta.cycles[i]) / window_cycles;
        measured += share;
        if (share < 0.05) {
            continue;
        }
        const double us_per_draw = double(delta.cycles[i]) / hz * 1e6 / double(delta.draws);
        std::snprintf(part, sizeof(part), " %s %.1f%% %.2f us/draw,", Names[i], share, us_per_draw);
        line += part;
    }
    std::snprintf(part, sizeof(part), " measured %.1f%% of the window", measured);
    line += part;
    return line;
}

inline bool Enabled() {
    static const bool on = [] {
        const char* value = std::getenv("SPARTAN_STAGE_PROFILE");
        return value && value[0] == '1';
    }();
    return on;
}

} // namespace SpartanStageProfile
