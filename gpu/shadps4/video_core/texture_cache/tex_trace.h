// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace VideoCore {

// spartanport (SPARTAN_TEX_TRACE=1): what the texture cache did with guest memory, for the
// progressive texture corruption reports (#1059, #1016, #1103). Off by default; when off, each
// hook is one predictable branch on a static bool. Lines start with "Tex:".
enum class TexEventKind : std::uint8_t {
    Evict,       ///< GC freed the image
    Download,    ///< GC copied the image to guest memory first (synchronous)
    Writeback,   ///< a GPU copy reached guest memory (deferred readback retired, or per frame)
    Cancel,      ///< a pending copy was dropped (unmap or delete) without reaching memory
    Unmap,       ///< the guest unmapped a range (images there are freed)
    AliasForget, ///< an image that leaves the cache was the alias writer/backing
    Count,
};

constexpr const char* TexEventName(TexEventKind kind) {
    constexpr const char* names[] = {"evict", "download", "writeback", "cancel", "unmap",
                                     "alias-forget"};
    const auto index = static_cast<std::size_t>(kind);
    return index < static_cast<std::size_t>(TexEventKind::Count) ? names[index] : "unknown";
}

struct TexEvent {
    TexEventKind kind{};
    const char* reason = "";     ///< static string
    std::uint64_t address = 0;   ///< guest address
    std::uint64_t size = 0;      ///< bytes
    std::uint32_t image = ~0u;   ///< slot index, ~0u: none
    std::uint64_t uid = 0;
    std::uint32_t format = 0;    ///< VkFormat value
    std::uint32_t pitch = 0, width = 0, height = 0, depth = 0;
    std::uint32_t flags = 0;     ///< ImageFlagBits
    std::uint32_t writer = ~0u, backing = ~0u; ///< alias authority (slot indices), ~0u: none
    std::uint64_t writer_uid = 0, backing_uid = 0;
    std::uint64_t tick = 0;      ///< GPU tick (scheduler) when recorded
    std::uint64_t map_gen = 0;   ///< unmaps seen by the texture cache so far
    std::uint64_t gc_tick = 0;
};

/// One line, without the trailing newline. Returns what snprintf returns.
inline int FormatTexEvent(char* out, std::size_t size, const TexEvent& e) {
    const auto id = [](std::uint32_t index) { return index == ~0u ? -1ll : static_cast<long long>(index); };
    return std::snprintf(
        out, size,
        "Tex: %s addr=%#llx size=%llu img=%lld uid=%llu fmt=%u pitch=%u extent=%ux%ux%u "
        "flags=%#x writer=%lld/%llu backing=%lld/%llu reason=%s tick=%llu gc=%llu mapgen=%llu",
        TexEventName(e.kind), static_cast<unsigned long long>(e.address),
        static_cast<unsigned long long>(e.size), id(e.image),
        static_cast<unsigned long long>(e.uid), e.format, e.pitch, e.width, e.height, e.depth,
        e.flags, id(e.writer), static_cast<unsigned long long>(e.writer_uid), id(e.backing),
        static_cast<unsigned long long>(e.backing_uid), e.reason,
        static_cast<unsigned long long>(e.tick), static_cast<unsigned long long>(e.gc_tick),
        static_cast<unsigned long long>(e.map_gen));
}

/// Fixed-size ring: the newest N events are kept, older ones are counted as overwritten.
template <std::size_t N>
class TexTraceRing {
    static_assert(N > 0);

public:
    void Push(const TexEvent& event) {
        events[(head + count) % N] = event;
        if (count == N) {
            head = (head + 1) % N;
            ++overwritten;
        } else {
            ++count;
        }
    }
    /// Hands over at most `max` events, oldest first, and empties the ring. Returns the events
    /// that were not handed over (overwritten or beyond `max`).
    template <typename Func>
    std::uint64_t Drain(std::size_t max, Func&& func) {
        const std::size_t skip = count > max ? count - max : 0;
        for (std::size_t i = skip; i < count; ++i) {
            func(events[(head + i) % N]);
        }
        const std::uint64_t dropped = overwritten + skip;
        head = count = 0;
        overwritten = 0;
        return dropped;
    }
    std::size_t Size() const {
        return count;
    }

private:
    std::array<TexEvent, N> events{};
    std::size_t head = 0, count = 0;
    std::uint64_t overwritten = 0;
};

/// Totals of one report interval.
struct TexInterval {
    std::uint64_t created = 0, created_bytes = 0;
    std::uint64_t destroyed = 0, destroyed_bytes = 0;
    std::uint64_t events[static_cast<std::size_t>(TexEventKind::Count)]{};
    std::uint64_t bytes[static_cast<std::size_t>(TexEventKind::Count)]{};
    std::uint64_t gc_runs = 0, gc_ns = 0, gc_max_ns = 0;
    void Count(TexEventKind kind, std::uint64_t size) {
        const auto index = static_cast<std::size_t>(kind);
        if (index >= static_cast<std::size_t>(TexEventKind::Count)) return;
        ++events[index];
        bytes[index] += size;
    }
    void Gc(std::uint64_t ns) {
        ++gc_runs;
        gc_ns += ns;
        gc_max_ns = ns > gc_max_ns ? ns : gc_max_ns;
    }
};

/// The interval line. `cache_bytes`/thresholds are the texture cache's own accounting.
inline int FormatTexSummary(char* out, std::size_t size, const TexInterval& t, double seconds,
                            std::uint64_t cache_bytes, std::uint64_t trigger,
                            std::uint64_t pressure, std::uint64_t critical) {
    constexpr double MiB = 1048576.0;
    const auto n = [&](TexEventKind kind) {
        return static_cast<unsigned long long>(t.events[static_cast<std::size_t>(kind)]);
    };
    const auto b = [&](TexEventKind kind) {
        return t.bytes[static_cast<std::size_t>(kind)] / MiB;
    };
    return std::snprintf(
        out, size,
        "Tex: summary %.1fs created=%llu (%.1f MiB) destroyed=%llu (%.1f MiB) evict=%llu "
        "(%.1f MiB) download=%llu (%.1f MiB) writeback=%llu (%.1f MiB) cancel=%llu unmap=%llu "
        "alias-forget=%llu gc=%llu runs %.2f ms max %.2f ms cache=%.0f MiB "
        "trigger/pressure/critical=%.0f/%.0f/%.0f MiB",
        seconds, static_cast<unsigned long long>(t.created), t.created_bytes / MiB,
        static_cast<unsigned long long>(t.destroyed), t.destroyed_bytes / MiB,
        n(TexEventKind::Evict), b(TexEventKind::Evict), n(TexEventKind::Download),
        b(TexEventKind::Download), n(TexEventKind::Writeback), b(TexEventKind::Writeback),
        n(TexEventKind::Cancel), n(TexEventKind::Unmap), n(TexEventKind::AliasForget),
        static_cast<unsigned long long>(t.gc_runs), t.gc_ns / 1e6, t.gc_max_ns / 1e6,
        cache_bytes / MiB, trigger / MiB, pressure / MiB, critical / MiB);
}

/// SPARTAN_TEX_TRACE: "1" on; anything else (or unset) off.
constexpr bool ParseTexTrace(const char* value) {
    return value && value[0] == '1' && value[1] == '\0';
}

/// The ring, the interval totals and their lock. Only touched when tracing is on.
class TexTrace {
public:
    static constexpr std::size_t Capacity = 4096;  ///< events kept between reports
    static constexpr std::size_t LinesPerReport = 64;
    static bool Enabled() {
        static const bool enabled = ParseTexTrace(std::getenv("SPARTAN_TEX_TRACE"));
        return enabled;
    }
    void Record(const TexEvent& event) {
        std::scoped_lock lock{mutex};
        ring.Push(event);
        interval.Count(event.kind, event.size);
    }
    void Created(std::uint64_t bytes) {
        std::scoped_lock lock{mutex};
        ++interval.created;
        interval.created_bytes += bytes;
    }
    void Destroyed(std::uint64_t bytes) {
        std::scoped_lock lock{mutex};
        ++interval.destroyed;
        interval.destroyed_bytes += bytes;
    }
    void Gc(std::uint64_t ns) {
        std::scoped_lock lock{mutex};
        interval.Gc(ns);
    }
    /// The summary, then the newest LinesPerReport events; resets the interval.
    void Report(std::FILE* out, double seconds, std::uint64_t cache_bytes, std::uint64_t trigger,
                std::uint64_t pressure, std::uint64_t critical) {
        std::scoped_lock lock{mutex};
        char line[512];
        FormatTexSummary(line, sizeof(line), interval, seconds, cache_bytes, trigger, pressure,
                         critical);
        std::fprintf(out, "%s\n", line);
        const std::uint64_t dropped = ring.Drain(LinesPerReport, [&](const TexEvent& event) {
            FormatTexEvent(line, sizeof(line), event);
            std::fprintf(out, "%s\n", line);
        });
        if (dropped) {
            std::fprintf(out, "Tex: %llu older events not printed\n",
                         static_cast<unsigned long long>(dropped));
        }
        interval = {};
    }

private:
    std::mutex mutex;
    TexTraceRing<Capacity> ring;
    TexInterval interval;
};

} // namespace VideoCore
