"""Why the long frames happened, from the game's own counters: every `Stall:` line (frames over
SPARTAN_STALL_MS) and the detail lines under it, classified by what took most of the frame.

usage: python perf/auto/stalls.py <session.log or folders...> [--min-ms 40] [--all]
       (default: only frames once the level draws 600+ draws/frame; --all counts menus and loading)
"""
import argparse
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

STALL = re.compile(
    r"^Stall: (?P<ms>[\d.]+) ms frame; waits: readback (?P<readback>[\d.]+) ms, GPU ticks (?P<ticks>[\d.]+) ms, "
    r"recorder (?P<recorder>[\d.]+) ms, host copies (?P<copies>[\d.]+) ms; (?P<n>\d+) compiles (?P<compile>[\d.]+) ms; "
    r"GC (?P<gc>[\d.]+) ms")
MS_IN = re.compile(r"^\s+ms in: resident (?P<resident>[\d.]+), protect (?P<protect>[\d.]+), image create "
                   r"(?P<create>[\d.]+), image refresh (?P<refresh>[\d.]+), staging (?P<staging>[\d.]+), "
                   r"waiting for host copies (?P<hostwait>[\d.]+)")
FAULTS = re.compile(r"^\s+fault handlers: \d+ read faults (?P<rfault>[\d.]+) ms, write faults (?P<wfault>[\d.]+) ms")
UPLOADS = re.compile(r"^\s+GPU thread on CPU (?P<cpu>[\d.]+) ms; (?P<images>\d+) images registered, "
                     r"(?P<img_mb>[\d.]+) MB image uploads, (?P<buf_mb>[\d.]+) MB buffer uploads")
HEAVY = re.compile(r"^Frame stats: .* (?P<draws>[\d.]+) draws/frame")
LABELS = {
    "readback": "leitura de volta da GPU (readback)", "ticks": "espera da GPU (GPU ticks)",
    "recorder": "gravador de comandos", "copies": "cópias do host", "compile": "compilação de shader",
    "gc": "limpeza de memória da GPU (GC)",
    "streaming": "streaming de texturas/buffers na thread da GPU (resident, create, refresh, protect)",
    "faults": "faltas na memória do jogo (páginas protegidas)",
    "protect": "proteção de páginas", "create": "criação de imagens", "refresh": "atualização de imagens",
    "staging": "staging", "rfault": "faltas de leitura na memória do jogo",
    "wfault": "faltas de escrita na memória do jogo (páginas protegidas)", "hostwait": "espera de cópias do host",
    "none": "nada medido explica (fora da thread da GPU: jogo, sistema, apresentação)",
}


def classify(stall, detail, share=0.4):
    """The single cause that took at least `share` of the frame, else "none"."""
    ms = float(stall["ms"])
    parts = {k: float(stall[k]) for k in ("readback", "ticks", "recorder", "copies", "compile", "gc")}
    detail = {k: float(v) for k, v in (detail or {}).items()}
    # One job on the GPU thread: making the textures and buffers a frame uses ready on the GPU.
    parts["streaming"] = sum(detail.get(k, 0.0) for k in ("resident", "protect", "create", "refresh", "staging"))
    parts["faults"] = detail.get("rfault", 0.0) + detail.get("wfault", 0.0)
    parts["hostwait"] = detail.get("hostwait", 0.0)
    top = max(parts, key=parts.get)
    return top if parts[top] >= share * ms else "none"


def scan_lines(lines, min_ms, all_frames=False):
    """[(cause, ms, image MB)] for the long frames in `lines`."""
    out, live, armed, pending = [], all_frames, False, None
    for line in lines + [""]:
        if line.startswith("Free camera: in play"):
            armed = True  # the level's WAD opened; play starts with the first heavy frame
        elif line.startswith("Free camera: title screen"):
            live, armed = all_frames, False
        elif armed and (f := HEAVY.match(line)) and float(f["draws"]) >= 600:
            live, armed = True, False
        if pending and (m := MS_IN.match(line)):
            pending[1].update(m.groupdict())
            continue
        if pending and (m := FAULTS.match(line)):
            pending[1].update(m.groupdict())
            continue
        if pending and (m := UPLOADS.match(line)):
            pending[2] = float(m["img_mb"])
            continue
        if pending and not line.startswith(" "):
            stall, detail, mb = pending
            out.append((classify(stall, detail), float(stall["ms"]), mb))
            pending = None
        if (m := STALL.match(line)) and live and float(m["ms"]) >= min_ms:
            pending = [m, {}, 0.0]
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("paths", nargs="+", type=Path)
    ap.add_argument("--min-ms", type=float, default=40.0)
    ap.add_argument("--all", action="store_true")
    a = ap.parse_args(argv)
    rows = []
    for p in a.paths:
        for f in (sorted(p.rglob("session.log")) if p.is_dir() else [p]):
            rows += scan_lines(f.read_text(encoding="utf-8", errors="replace").splitlines(), a.min_ms, a.all)
    if not rows:
        print("no long frames")
        return 0
    by, worst, mb = Counter(), defaultdict(float), defaultdict(float)
    for cause, ms, img in rows:
        by[cause] += 1
        worst[cause] = max(worst[cause], ms)
        mb[cause] += img
    print(f"{len(rows)} frames over {a.min_ms:.0f} ms" + ("" if a.all else " during play"))
    for c, n in by.most_common():
        print(f"  {n:5d} ({n / len(rows):5.1%})  pior {worst[c]:7.1f} ms  upload médio {mb[c] / n:6.1f} MB  {LABELS[c]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
