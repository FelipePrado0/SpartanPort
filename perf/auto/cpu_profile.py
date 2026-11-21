"""Turns the CPU sampler's counts (SPARTAN_CPU_SAMPLER, gpu/shim/spartanport_cpu_sampler.h) into
the heaviest functions of each thread.

usage: python perf/auto/cpu_profile.py cpu_samples.tsv [--exe out/spartanport-probe.exe] [--top 15]

Addresses in the game's own executable are named with llvm-symbolizer (MSYS2 clang64); others show
their module (the Vulkan driver, ntdll waits) or "?" (guest code, not a module)."""
import argparse
import collections
import os
import shutil
import struct
import subprocess
import sys

SYMBOLIZER_GUESSES = ["llvm-symbolizer", "C:/msys64/clang64/bin/llvm-symbolizer.exe"]


def read_samples(path):
    """[(thread, module, offset, count)] from the sampler's file."""
    rows = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            parts = line.rstrip("\n").split("\t")
            if len(parts) != 4:
                continue
            thread, module, offset, count = parts
            # The elapsed line holds decimal TSC ticks where the others hold hex offsets.
            rows.append((thread, module, int(offset, 10 if module == "#elapsed" else 16), int(count)))
    return rows


def image_base(exe):
    """The preferred load address in a PE file's optional header."""
    with open(exe, "rb") as f:
        data = f.read(4096)
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    magic = struct.unpack_from("<H", data, pe + 24)[0]
    return struct.unpack_from("<Q", data, pe + 24 + 24)[0] if magic == 0x20B else \
        struct.unpack_from("<I", data, pe + 24 + 28)[0]


def symbolize(exe, offsets):
    """{offset: function name} for offsets into `exe`, one llvm-symbolizer run."""
    tool = next((t for t in SYMBOLIZER_GUESSES if shutil.which(t) or os.path.exists(t)), None)
    if not tool or not offsets:
        return {}
    base = image_base(exe)
    ordered = sorted(offsets)
    query = "".join(f"0x{base + o:x}\n" for o in ordered)
    out = subprocess.run([tool, "--obj", exe, "--no-inlines", "--functions=linkage", "--demangle"],
                         input=query, capture_output=True, text=True, timeout=600).stdout
    blocks = [b for b in out.split("\n\n")]
    names = {}
    for offset, block in zip(ordered, blocks):
        lines = block.strip().splitlines()
        names[offset] = lines[0] if lines and lines[0] != "??" else f"?+{offset:x}"
    return names


def aggregate(rows, names, exe_module):
    """{thread: Counter(function -> samples)}; functions outside the exe are their module, and
    "via:" rows (the executable's code that led to them) count under "(called from) function"."""
    per_thread = collections.defaultdict(collections.Counter)
    for thread, module, offset, count in rows:
        if module.startswith("#"):
            continue  # CPU time lines (thread_time)
        if module.startswith("via2:"):
            per_thread[thread]["(two calls up) " + names.get(offset, f"+{offset:x}")] += count
            continue
        if module.startswith("via:"):
            per_thread[thread]["(called from) " + names.get(offset, f"+{offset:x}")] += count
            continue
        if module == exe_module:
            function = names.get(offset, f"{module}+{offset:x}")
        elif module == "?":
            function = "(guest code)"
        else:
            function = module
        per_thread[thread][function] += count
    return per_thread


def thread_time(rows, fps=0.0):
    """Lines of each thread's CPU use while sampled: busy share and, with `fps`, ms per frame."""
    used = {t: c for t, m, _, c in rows if m == "#cycles"}
    elapsed = next((o for t, m, o, c in rows if m == "#elapsed"), 0)
    if not used or not elapsed:
        return ""
    lines = ["CPU time per thread while sampled (busy share of one core" +
             (f", ms per frame at {fps:.1f} FPS)" if fps else ")")]
    for thread, cycles in sorted(used.items(), key=lambda kv: -kv[1])[:20]:
        busy = cycles / elapsed
        lines.append(f"  {100.0 * busy:6.1f}%" + (f"  {1000.0 * busy / fps:6.2f} ms" if fps else "") +
                     f"  {thread}")
    return "\n".join(lines)


def report(per_thread, top):
    lines = []
    extra = ("(called from)", "(two calls up)")
    totals = {t: sum(n for f, n in c.items() if not f.startswith(extra))
              for t, c in per_thread.items()}
    for thread in sorted(per_thread, key=lambda t: -totals[t]):
        counter = per_thread[thread]
        total = max(1, totals[thread])
        lines.append(f"== {thread}: {totals[thread]} samples")
        own = collections.Counter({f: n for f, n in counter.items() if not f.startswith(extra)})
        via = collections.Counter({f: n for f, n in counter.items() if f.startswith("(called from)")})
        via2 = collections.Counter({f: n for f, n in counter.items() if f.startswith("(two calls up)")})
        for function, count in own.most_common(top):
            lines.append(f"  {100.0 * count / total:5.1f}%  {function}")
        if via:
            lines.append("  system, driver and guest code above, by the game function that called it:")
            for function, count in via.most_common(top // 2):
                lines.append(f"  {100.0 * count / total:5.1f}%  {function}")
            for function, count in via2.most_common(top // 2):
                lines.append(f"  {100.0 * count / total:5.1f}%  {function}")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("samples")
    ap.add_argument("--exe", default=os.path.join(os.path.dirname(__file__), "..", "..", "out",
                                                  "spartanport-probe.exe"))
    ap.add_argument("--top", type=int, default=15)
    ap.add_argument("--fps", type=float, default=0.0, help="the run's FPS, for ms per frame")
    args = ap.parse_args()
    rows = read_samples(args.samples)
    exe_module = os.path.basename(args.exe)
    names = symbolize(args.exe, {o for _, m, o, _ in rows
                                 if m in (exe_module, "via:" + exe_module, "via2:" + exe_module)})
    print(thread_time(rows, args.fps))
    print(report(aggregate(rows, names, exe_module), args.top))


if __name__ == "__main__":
    sys.exit(main())
