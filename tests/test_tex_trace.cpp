// SPARTAN_TEX_TRACE diagnostics (texture_cache/tex_trace.h): off by default, bounded ring,
// "Tex:" lines. No game, Vulkan device or window required.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "video_core/texture_cache/tex_trace.h"

using namespace VideoCore;

static std::string Read(std::FILE* file) {
    std::rewind(file);
    std::string text;
    char buffer[4096];
    std::size_t n;
    while ((n = std::fread(buffer, 1, sizeof(buffer), file)) > 0) text.append(buffer, n);
    return text;
}

int main() {
    // Off unless exactly "1".
    assert(!ParseTexTrace(nullptr));
    assert(!ParseTexTrace(""));
    assert(!ParseTexTrace("0"));
    assert(!ParseTexTrace("10"));
    assert(!ParseTexTrace("yes"));
    assert(ParseTexTrace("1"));
    // The process under test does not set it: Enabled() is off by default.
    if (!std::getenv("SPARTAN_TEX_TRACE")) assert(!TexTrace::Enabled());

    // One event line carries everything the roadmap asks for.
    TexEvent e{.kind = TexEventKind::Evict, .reason = "gc-pressure", .address = 0x2f0000,
               .size = 8294400, .image = 12, .uid = 345, .format = 37, .pitch = 1920,
               .width = 1920, .height = 1080, .depth = 1, .flags = 0x108, .writer = ~0u,
               .backing = 7, .backing_uid = 300, .tick = 9001, .map_gen = 4, .gc_tick = 77};
    char line[512];
    FormatTexEvent(line, sizeof(line), e);
    assert(std::strcmp(line,
        "Tex: evict addr=0x2f0000 size=8294400 img=12 uid=345 fmt=37 pitch=1920 "
        "extent=1920x1080x1 flags=0x108 writer=-1/0 backing=7/300 reason=gc-pressure "
        "tick=9001 gc=77 mapgen=4") == 0);
    assert(std::strcmp(TexEventName(TexEventKind::AliasForget), "alias-forget") == 0);
    assert(std::strcmp(TexEventName(static_cast<TexEventKind>(200)), "unknown") == 0);
    // A short buffer is truncated, never overrun.
    char tiny[8];
    std::memset(tiny, 'x', sizeof(tiny));
    FormatTexEvent(tiny, sizeof(tiny), e);
    assert(std::strcmp(tiny, "Tex: ev") == 0);

    // Ring: keeps the newest N, counts the rest, drains oldest first.
    {
        TexTraceRing<4> ring;
        for (std::uint64_t i = 1; i <= 6; ++i) ring.Push({.address = i});
        assert(ring.Size() == 4);
        std::vector<std::uint64_t> seen;
        assert(ring.Drain(3, [&](const TexEvent& ev) { seen.push_back(ev.address); }) == 3);
        assert((seen == std::vector<std::uint64_t>{4, 5, 6}));
        assert(ring.Size() == 0);
        seen.clear();
        assert(ring.Drain(3, [&](const TexEvent& ev) { seen.push_back(ev.address); }) == 0);
        assert(seen.empty());
        ring.Push({.address = 7});
        assert(ring.Drain(8, [&](const TexEvent& ev) { seen.push_back(ev.address); }) == 0);
        assert((seen == std::vector<std::uint64_t>{7}));
    }

    // Interval totals and the summary line; Report resets them and bounds the event lines.
    {
        TexTrace trace;
        trace.Created(4 << 20);
        trace.Created(4 << 20);
        trace.Destroyed(2 << 20);
        trace.Gc(1'500'000);
        trace.Gc(500'000);
        for (std::size_t i = 0; i < TexTrace::LinesPerReport + 10; ++i) {
            trace.Record({.kind = TexEventKind::Writeback, .reason = "frame", .size = 1 << 20});
        }
        trace.Record({.kind = TexEventKind::Evict, .reason = "gc-critical", .size = 3 << 20});
        std::FILE* out = std::tmpfile();
        assert(out);
        trace.Report(out, 5.0, 3000ull << 20, 2000ull << 20, 2800ull << 20, 3100ull << 20);
        const std::string text = Read(out);
        assert(text.rfind("Tex: summary 5.0s created=2 (8.0 MiB) destroyed=1 (2.0 MiB) "
                          "evict=1 (3.0 MiB) download=0 (0.0 MiB) writeback=74 (74.0 MiB) "
                          "cancel=0 unmap=0 alias-forget=0 gc=2 runs 2.00 ms max 1.50 ms "
                          "cache=3000 MiB trigger/pressure/critical=2000/2800/3100 MiB\n", 0) == 0);
        std::size_t lines = 0;
        for (std::size_t at = 0; (at = text.find("\nTex: writeback", at)) != std::string::npos; ++at) ++lines;
        assert(lines == TexTrace::LinesPerReport - 1);  // the newest 64: 63 writebacks + the evict
        assert(text.find("reason=gc-critical") != std::string::npos);
        assert(text.find("Tex: 11 older events not printed\n") != std::string::npos);
        std::fclose(out);
        out = std::tmpfile();
        trace.Report(out, 5.0, 0, 0, 0, 0);
        const std::string again = Read(out);
        assert(again.find("created=0 (0.0 MiB)") != std::string::npos);
        assert(again.find("gc=0 runs") != std::string::npos);
        assert(again.find("\nTex: ") == std::string::npos);
        std::fclose(out);
    }
    std::puts("tex-trace-test: OK");
    return 0;
}
