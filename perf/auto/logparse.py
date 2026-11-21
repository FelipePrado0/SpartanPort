"""Reads the game's session log (stdout / user/last_run.log) into numbers.

Only lines the game already prints are used: `Frame stats:` (every 5 s with SPARTAN_FRAME_STATS=1),
`Frame pacing:`, `Stall:` (frames over SPARTAN_STALL_MS, 40 ms by default), `Disk shaders:` and
`Pipeline binary cache:`.
"""
import re
import statistics

FRAME_STATS = re.compile(
    r"^Frame stats: (?P<fps>[\d.]+) FPS, worst frame (?P<worst>[\d.]+) ms .*?"
    r"(?P<compiles>\d+) shader/pipeline compiles.*?"
    r"GPU thread (?P<us>[\d.]+) us/draw, (?P<draws>[\d.]+) draws/frame")
FRAME_PACING = re.compile(
    r"^Frame pacing: median (?P<median>[\d.]+) ms, stddev (?P<stddev>[\d.]+) ms, "
    r"p99 (?P<p99>[\d.]+) ms, (?P<spikes>\d+) frames over")
STALL = re.compile(r"^Stall: (?P<ms>[\d.]+) ms frame")
DISK = re.compile(
    r"^Disk shaders: (?P<programs>\d+) programs, (?P<perms>\d+) permutations read .*?; "
    r"(?P<used>\d+) used, (?P<compiled>\d+) compiled")
BINARY = re.compile(r"^Pipeline binary cache: (?P<hits>\d+) hits, (?P<compiled>\d+) compiled")
CRASH = re.compile(r"Unhandled exception|Assertion|Assert failed|access violation|fatal signal|"
                   r"guest fault|Segmentation fault", re.IGNORECASE)


def parse(lines, min_draws=0):
    """Returns the samples in `lines` (an iterable of str). Frame stats windows with fewer than
    `min_draws` draws/frame (menus, loading screens) are dropped from `frames`."""
    out = {"frames": [], "pacing": [], "stalls": [], "disk": None, "binary": None, "crash": None}
    for line in lines:
        line = line.rstrip("\r\n")
        if m := FRAME_STATS.match(line):
            if float(m["draws"]) >= min_draws:
                out["frames"].append({k: float(v) for k, v in m.groupdict().items()})
        elif m := FRAME_PACING.match(line):
            out["pacing"].append({k: float(v) for k, v in m.groupdict().items()})
        elif m := STALL.match(line):
            out["stalls"].append(float(m["ms"]))
        elif m := DISK.match(line):
            out["disk"] = {k: int(v) for k, v in m.groupdict().items()}
        elif m := BINARY.match(line):
            out["binary"] = {k: int(v) for k, v in m.groupdict().items()}
        elif out["crash"] is None and CRASH.search(line):
            out["crash"] = line[:200]
    return out


def summarize(parsed):
    """One run's metrics. FPS and us/draw are medians of the 5 s windows; p99 frametime is the
    worst window p99 (guest flip intervals); stalls count frames over 40 ms."""
    frames = parsed["frames"]
    pacing = parsed["pacing"]
    disk = parsed["disk"] or {}
    binary = parsed["binary"] or {}
    return {
        "windows": len(frames),
        "fps_median": statistics.median(f["fps"] for f in frames) if frames else None,
        # 0.00 everywhere means the per-thread CPU clock is missing (Windows): no value, not zero.
        "us_per_draw_median": (statistics.median(f["us"] for f in frames)
                               if frames and any(f["us"] for f in frames) else None),
        "draws_per_frame_median": statistics.median(f["draws"] for f in frames) if frames else None,
        "worst_frame_ms": max((f["worst"] for f in frames), default=None),
        "p99_frametime_ms": max((p["p99"] for p in pacing), default=None),
        # The run sets SPARTAN_STALL_MS=25. shortcut: the game prints at most 10 stalls per 5 s
        # window, so a window with more under-counts; a per-frame log would be exact.
        "frames_over_25ms": sum(ms > 25 for ms in parsed["stalls"]),
        "frames_over_40ms": sum(ms > 40 for ms in parsed["stalls"]),
        "window_compiles": int(sum(f["compiles"] for f in frames)),
        "disk_shaders_compiled": disk.get("compiled"),
        "pipeline_binary_compiled": binary.get("compiled"),
        "crash": parsed["crash"],
    }


def spread(values):
    """(median, min, max) of the non-None values, or (None, None, None)."""
    v = [x for x in values if x is not None]
    if not v:
        return None, None, None
    return statistics.median(v), min(v), max(v)
