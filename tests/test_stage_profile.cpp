// SPDX-License-Identifier: GPL-2.0-or-later
// spartanport: the stage B section profiler (gpu/shim/spartanport_stage_profile.h): laps add the
// time since the previous mark to a section, and the report gives each section's share of the
// window and its microseconds per draw.
#include "spartanport_stage_profile.h"
#include <cassert>
#include <cstdio>
#include <string>

using namespace SpartanStageProfile;

static bool Contains(const std::string& text, const char* part) {
    return text.find(part) != std::string::npos;
}

int main() {
    // Laps: each one charges the time since the previous mark, so nested calls never count twice.
    Totals totals;
    Laps laps{totals};
    laps.Start(1000);
    laps.Lap(RenderState, 1100);
    laps.Lap(Bind, 1600);
    laps.Lap(Begin, 1700);
    laps.Lap(Record, 2000);
    assert(totals.cycles[RenderState] == 100);
    assert(totals.cycles[Bind] == 500);
    assert(totals.cycles[Begin] == 100);
    assert(totals.cycles[Record] == 300);
    assert(totals.cycles[Packet] == 0);
    laps.Whole(Task, 2000, 2400); // a packet that is one section from start to end
    assert(totals.cycles[Task] == 400);

    // Report: a 1 s window at 1 GHz with 1000 draws; Bind took half of it.
    Totals delta;
    delta.cycles[Bind] = 500'000'000;
    delta.cycles[Record] = 100'000'000;
    delta.draws = 1000;
    const std::string line = Format(delta, 1'000'000'000.0, 1.0);
    std::printf("%s\n", line.c_str());
    assert(line.rfind("Stage B profile: ", 0) == 0);
    assert(Contains(line, "bind 50.0% 500.00 us/draw"));
    assert(Contains(line, "record 10.0% 100.00 us/draw"));
    assert(Contains(line, "measured 60.0%"));
    assert(!Contains(line, "begin")); // sections under 0.05% are left out
    // The busiest section comes first.
    assert(line.find("bind") < line.find("record"));

    // No draws or no time: no division by zero, no section lines.
    assert(Contains(Format(Totals{}, 0.0, 0.0), "no draws"));

    // Stage A: its own sections and label.
    Totals a;
    a.cycles[APipeline] = 250'000'000;
    a.cycles[ARing] = 50'000'000;
    a.draws = 500;
    const std::string a_line = Format(a, 1'000'000'000.0, 1.0, "Stage A profile:");
    std::printf("%s\n", a_line.c_str());
    assert(a_line.rfind("Stage A profile: ", 0) == 0);
    assert(Contains(a_line, "pipeline 25.0% 500.00 us/draw"));
    assert(Contains(a_line, "constants 5.0% 100.00 us/draw"));

    // Names cover every section.
    for (unsigned i = 0; i < Count; ++i) {
        assert(Names[i] && Names[i][0]);
    }
    std::puts("PASS: stage profile");
    return 0;
}
