"""Regression gate: compares a scenario result with its baseline and says PASS / FAIL / INFRA_FAIL.

usage: python perf/auto/gate.py <result.json> <baseline.json>   (exit 0 PASS, 1 FAIL, 2 INFRA_FAIL)

Thresholds are the roadmap's pre-decided defaults (section 5); a scenario file can override them
under [limits].
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

DEFAULT_LIMITS = {
    "fps_drop_pct": 3.0,          # median FPS more than 3 % under the baseline
    "us_per_draw_rise_pct": 3.0,  # GPU thread us/draw more than 3 % over
    "p99_rise_pct": 10.0,         # p99 frametime more than 10 % over
    "stalls40_rise_pct": 50.0,    # frames over 40 ms: +50 % ...
    "stalls40_rise_abs": 3,       # ... and at least +3 frames
    "warm_compiles_max": 0,       # shaders compiled in game with a warm cache
}


def _pct(new, old):
    return (new - old) / old * 100.0 if old else 0.0


def noise_pct(baseline, key):
    """Half the spread of the baseline's runs, in % of its median (0 without a spread)."""
    lo, hi = (baseline.get("spread") or {}).get(key) or (None, None)
    med = baseline["summary"].get(key)
    return (hi - lo) / 2 / med * 100.0 if lo is not None and hi is not None and med else 0.0


def check(result, baseline, limits=None):
    """Returns (verdict, reasons). `result` and `baseline` hold a `summary` (medians over runs) as
    perf/auto/run_scenario.py writes it; a result with `infra_fail` set is INFRA_FAIL. Each
    percentage limit is at least twice the noise measured across the baseline's runs (roadmap
    section 5: a change counts only above 2x the measured noise)."""
    lim = dict(DEFAULT_LIMITS, **(limits or {}))
    if result.get("infra_fail"):
        return "INFRA_FAIL", [result["infra_fail"]]
    gpu_new, gpu_old = (result.get("machine") or {}).get("gpu"), (baseline.get("machine") or {}).get("gpu")
    if gpu_new and gpu_old and gpu_new != gpu_old:
        return "INFRA_FAIL", [f"baseline is from another GPU ({gpu_old}, this run {gpu_new})"]
    new, old = result["summary"], baseline["summary"]
    reasons = []
    if new.get("crashes"):
        reasons.append(f"crash in {new['crashes']} run(s): {new.get('crash_line', '')}")
    if new.get("fps_median") is None:
        reasons.append("no Frame stats in the measured window")
        return "FAIL", reasons
    fps_lim = max(lim["fps_drop_pct"], 2 * noise_pct(baseline, "fps_median"))
    if old.get("fps_median") and _pct(new["fps_median"], old["fps_median"]) < -fps_lim:
        reasons.append(f"FPS {new['fps_median']:.1f} vs {old['fps_median']:.1f} "
                       f"({_pct(new['fps_median'], old['fps_median']):+.1f} %, limit -{fps_lim:.1f} %)")
    us_lim = max(lim["us_per_draw_rise_pct"], 2 * noise_pct(baseline, "us_per_draw_median"))
    if old.get("us_per_draw_median") and new.get("us_per_draw_median") is not None and \
            _pct(new["us_per_draw_median"], old["us_per_draw_median"]) > us_lim:
        reasons.append(f"us/draw {new['us_per_draw_median']:.3f} vs {old['us_per_draw_median']:.3f}")
    p99_lim = max(lim["p99_rise_pct"], 2 * noise_pct(baseline, "p99_frametime_ms"))
    if old.get("p99_frametime_ms") and new.get("p99_frametime_ms") is not None and \
            _pct(new["p99_frametime_ms"], old["p99_frametime_ms"]) > p99_lim:
        reasons.append(f"p99 {new['p99_frametime_ms']:.2f} ms vs {old['p99_frametime_ms']:.2f} ms "
                       f"(limit +{p99_lim:.0f} %)")
    s_new, s_old = new.get("frames_over_40ms") or 0, old.get("frames_over_40ms") or 0
    if s_new - s_old >= lim["stalls40_rise_abs"] and \
            (s_old == 0 or _pct(s_new, s_old) > lim["stalls40_rise_pct"]):
        reasons.append(f"frames over 40 ms {s_new} vs {s_old}")
    # Roadmap: "> 0 new" compiles with a warm cache, so the ones the baseline already has (a build's
    # constant per-start compiles) do not count.
    c_new, c_old = new.get("window_compiles") or 0, old.get("window_compiles") or 0
    if result.get("cache") == "warm" and c_new - c_old > lim["warm_compiles_max"]:
        reasons.append(f"{c_new} shader/pipeline compiles in game with a warm cache ({c_old} in the baseline)")
    if result.get("structure_mismatch"):
        reasons.append("frame structure differs: " + result["structure_mismatch"])
    if baseline.get("capture_ref") and not result.get("captures") and not result.get("structure_mismatch"):
        reasons.append("no frame capture to compare with the baseline's")
    return ("FAIL" if reasons else "PASS"), reasons


def notes(result, baseline):
    """What the verdict did not check, so a PASS never hides a missing metric."""
    out = []
    if result.get("summary", {}).get("us_per_draw_median") is None:
        out.append("us/draw: not measured (no per-thread CPU clock on this system)")
    if result.get("runs_requested") and result.get("runs", 0) < result["runs_requested"]:
        out.append(f"only {result['runs']} of {result['runs_requested']} runs gave samples")
    if not (baseline.get("machine") or {}).get("gpu"):
        out.append("baseline has no GPU name: machine not compared")
    return out


def main(argv):
    if len(argv) != 3:
        print(__doc__.strip().splitlines()[2], file=sys.stderr)
        return 2
    with open(argv[1], encoding="utf-8") as f:
        result = json.load(f)
    with open(argv[2], encoding="utf-8") as f:
        baseline = json.load(f)
    ref = baseline.get("capture_ref")
    if ref and not result.get("structure_mismatch"):
        import capstruct
        with open(Path(argv[2]).parent / ref, encoding="utf-8") as f:
            reference = capstruct.parse(f.read())
        for path in result.get("captures", []):
            with open(path, encoding="utf-8") as f:
                if d := capstruct.diff(reference, capstruct.parse(f.read())):
                    result["structure_mismatch"] = f"{Path(path).name}: {d}"
                    break
    # The baseline's limits: a branch under test must not loosen its own gate.
    verdict, reasons = check(result, baseline, baseline.get("limits"))
    print(f"{verdict}: {result.get('scenario')} ({result.get('cache')})")
    for r in reasons:
        print(f"  - {r}")
    for n in notes(result, baseline):
        print(f"  note: {n}")
    return {"PASS": 0, "FAIL": 1}.get(verdict, 2)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
