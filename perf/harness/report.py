"""W0 report: reads combined scenario results (perf/auto/run_scenario.py JSON, baselines included)
and writes perf/REPORT.md with SpartanPort and the comparison port side by side.

usage: python perf/harness/report.py [result dirs or files...] [--out perf/REPORT.md]
       (default input: perf/baseline)
"""
import argparse
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
# What W0 could not make equal between the two targets, listed in every report (roadmap W0).
NOT_EQUALIZED = [
    "O port de comparação não foi medido: ele só aceita o eboot da 1.02 real (`6e72330e…`) e o único "
    "dump desta máquina tem o eboot `d85c8135…` (código da 1.00).",
    "Saída em janela 1920x1080 no monitor principal (2560x1440); sem upscaler, frame generation, "
    "overlay nem limite de FPS; motor a 240.",
    "`us/draw` não existe no Windows (sai 0.00); só FPS e frametime contam.",
    "A baseline vale para o estado da máquina na sessão em que foi gravada (deriva de ~7 % medida em 3 h).",
]


def load(paths):
    """Combined results (they carry `scenario` and `summary`), from files or folders (not recursive)."""
    out = []
    for p in paths:
        files = sorted(p.glob("*.json")) if p.is_dir() else [p]
        for f in files:
            d = json.loads(f.read_text(encoding="utf-8"))
            if {"scenario", "cache", "summary"} <= d.keys():
                out.append(d)
    return out


def _cell(d):
    if not d or d["summary"].get("fps_median") is None:
        return "sem dados"
    spread = (d.get("spread") or {}).get("fps_median") or []
    rng = f" ({spread[0]:.1f} a {spread[1]:.1f})" if len(spread) == 2 and None not in spread else ""
    return f"{d['summary']['fps_median']:.1f}{rng}"


def render(results):
    rows = {}
    for d in results:
        rows.setdefault((d["scenario"], d["cache"]), {})[d.get("target", "spartanport")] = d
    lines = ["# Relatório W0: SpartanPort e o port de comparação", "",
             "Gerado por `perf/harness/report.py` a partir dos resultados combinados de `perf/auto`.", "",
             "| Cenário | Cache | SpartanPort FPS (mín a máx) | Comparação FPS | Razão SP/comp. | p99 SP | > 40 ms SP | Compilações SP |",
             "|---|---|---|---|---|---|---|---|"]
    for (scen, cache), by in sorted(rows.items()):
        sp, other = by.get("spartanport"), by.get("rehooked")
        ratio = "-"
        if sp and other and other["summary"].get("fps_median") and sp["summary"].get("fps_median"):
            ratio = f"{sp['summary']['fps_median'] / other['summary']['fps_median']:.2f}"
        s = sp["summary"] if sp else {}
        lines.append(f"| {scen} | {cache} | {_cell(sp)} | {_cell(other)} | {ratio} | "
                     f"{s.get('p99_frametime_ms', '-')} | {s.get('frames_over_40ms', '-')} | {s.get('window_compiles', '-')} |")
    machines = sorted({(d.get("machine") or {}).get("gpu", "?") for d in results})
    builds = sorted({d.get("build", "?") for d in results})
    lines += ["", f"GPU: {', '.join(machines)}. Build: {', '.join(builds)}.", "",
              "## Condições não igualadas", ""] + [f"- {n}" for n in NOT_EQUALIZED]
    return "\n".join(lines) + "\n"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("inputs", nargs="*", type=Path)
    ap.add_argument("--out", type=Path, default=REPO / "perf" / "REPORT.md")
    args = ap.parse_args(argv)
    results = load(args.inputs or [REPO / "perf" / "baseline"])
    if not results:
        print("no combined results found", file=sys.stderr)
        return 1
    args.out.write_text(render(results), encoding="utf-8", newline="\n")  # LF, as the repository
    print(f"wrote {args.out} ({len(results)} results)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
