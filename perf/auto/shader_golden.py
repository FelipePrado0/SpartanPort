"""L1 shader regression without opening the game.

Runs the offline verifier (`shader-replay-test`, tests/test_shader_replay.cpp) of one or two builds
over a recorded cache (the corpus: `.meta` inputs + the `.spv` the game wrote) and the dump index,
and reports which shaders changed:

    python perf/auto/shader_golden.py --corpus user/cache/CUSA01623 --index dump.idx \
        --after out/gpu/shader-replay-test.exe [--before <older build's exe>] [--first N --count M]
    python perf/auto/shader_golden.py --self-test --corpus ... --index ... --after ...

With --before, PASS means both builds translate exactly the same set of shaders differently from
the corpus (normally none). Without it, PASS means the build reproduces the corpus byte for byte.
Every SPIR-V that differs is dumped (REPLAY_DUMP), checked with spirv-val and compared with the
corpus after normalizing ids (`%123` -> `%`), so a diff that only renumbers ids is reported apart.
--self-test proves the detector: a copy of a slice of the corpus must pass, then the same slice with
one `.spv` changed by one word must report exactly that shader.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

DIFFER = re.compile(r"^differ: (?P<stage>\S+) (?P<meta>\S+\.meta) \((?P<got>\d+) vs (?P<want>\d+) bytes\)")
TOTAL = re.compile(r"^translated (?P<n>\d+) in")
MASK = (1 << 64) - 1


def hash_combine(seed, value):
    """common/hash.h HashCombine (u64): the cache names a permutation's .meta by this."""
    return (seed ^ ((value + 0x9E3779B9 + (seed << 12) + (seed >> 4)) & MASK)) & MASK


def meta_to_spv(corpus):
    """{meta file name: spv file name} for the permutations in `corpus`."""
    by_perm = {}
    for spv in Path(corpus).glob("0x*_*.spv"):
        pgm, perm = spv.stem.split("_")
        by_perm[hash_combine(int(pgm, 16), int(perm))] = spv.name
    return {m.name: by_perm.get(int(m.stem, 16)) for m in Path(corpus).glob("0x*.meta")}


def tool(name):
    found = shutil.which(name)
    if not found and sys.platform == "win32" and Path(f"C:/msys64/clang64/bin/{name}.exe").exists():
        found = f"C:/msys64/clang64/bin/{name}.exe"
    return found


def replay(exe, corpus, index, first=None, count=None, dump=None):
    """Runs the verifier; returns ({meta: (stage, got, want)}, translated count, output)."""
    args = [str(Path(exe).resolve()), str(corpus), str(index)]
    if first is not None:
        args += [str(first), str(count if count is not None else 1 << 62)]
    env = dict(os.environ)
    if dump:
        Path(dump).mkdir(parents=True, exist_ok=True)
        env["REPLAY_DUMP"] = str(dump)
    if sys.platform == "win32" and Path("C:/msys64/clang64/bin").exists():
        env["PATH"] = "C:\\msys64\\clang64\\bin;" + env.get("PATH", "")  # libc++.dll
    # The whole corpus takes ~10 min on the reference machine; a hung verifier must not hang quick/full.
    p = subprocess.run(args, capture_output=True, text=True, env=env, errors="replace", timeout=3600)
    diffs, total = {}, None
    for line in p.stdout.splitlines():
        if m := DIFFER.match(line):
            diffs[m["meta"]] = (m["stage"], int(m["got"]), int(m["want"]))
        elif m := TOTAL.match(line):
            total = int(m["n"])
    # Exit 1 only means "something differed"; anything else, or no summary, is a broken run.
    if total is None or p.returncode not in (0, 1):
        raise RuntimeError(f"{exe} failed ({p.returncode}): {(p.stderr or p.stdout)[-400:]}")
    return diffs, total, p.stdout


def normalized(spv_path):
    dis = tool("spirv-dis")
    if not dis:
        return None
    text = subprocess.run([dis, str(spv_path)], capture_output=True, text=True).stdout
    return re.sub(r"%[0-9]+", "%", text)


def explain(diffs, corpus, dump):
    """Per differing shader: spirv-val of the new SPIR-V and whether only ids changed."""
    val = tool("spirv-val")
    names = meta_to_spv(corpus)
    rows = []
    for meta, (stage, got, want) in sorted(diffs.items()):
        spv = names.get(meta)
        new = Path(dump) / spv if spv else None
        row = {"meta": meta, "stage": stage, "bytes": [got, want], "spv": spv}
        if new and new.exists():
            if val:
                v = subprocess.run([val, str(new)], capture_output=True, text=True)
                row["spirv_val"] = "ok" if v.returncode == 0 else (v.stderr or v.stdout).strip()[:200]
            a, b = normalized(Path(corpus) / spv), normalized(new)
            if a is not None and b is not None:
                row["ids_only"] = a == b
        rows.append(row)
    return rows


def golden(args):
    work = Path(tempfile.mkdtemp(prefix="shader-golden-"))
    try:
        after, n_after, _ = replay(args.after, args.corpus, args.index, args.first, args.count, work / "after")
        report = {"translated": n_after, "after_differ": len(after)}
        if args.before:
            before, n_before, _ = replay(args.before, args.corpus, args.index, args.first, args.count,
                                         work / "before")
            # A shader both builds translate differently from the corpus counts too when the two
            # builds' SPIR-V differ from each other.
            names = meta_to_spv(args.corpus)
            both = {m for m in set(after) & set(before)
                    if after[m][1] != before[m][1] or not names.get(m) or
                    (work / "after" / names[m]).read_bytes() != (work / "before" / names[m]).read_bytes()}
            changed = sorted((set(after) ^ set(before)) | both)
            report.update(before_differ=len(before), changed=changed)
            ok = not changed and n_before == n_after
        else:
            changed = sorted(after)
            ok = not after
        report["details"] = explain({m: after[m] for m in changed if m in after}, args.corpus, work / "after")
        return ok, report
    finally:
        shutil.rmtree(work, ignore_errors=True)


def self_test(args, slice_size=200):
    """The detector itself: an unchanged slice passes, a slice with one changed .spv fails on
    exactly that shader."""
    names = meta_to_spv(args.corpus)
    metas = sorted(m for m, s in names.items() if s)[:slice_size]
    work = Path(tempfile.mkdtemp(prefix="shader-golden-self-"))
    try:
        corpus = work / "corpus"
        corpus.mkdir()
        shutil.copy2(Path(args.corpus) / "profile.bin", corpus)
        for m in metas:
            shutil.copy2(Path(args.corpus) / m, corpus)
            shutil.copy2(Path(args.corpus) / names[m], corpus)
        clean, n, _ = replay(args.after, corpus, args.index)
        if clean or n == 0:
            return False, {"step": "unchanged slice", "translated": n, "differ": sorted(clean)}
        victim = metas[len(metas) // 2]
        spv = corpus / names[victim]
        data = bytearray(spv.read_bytes())
        at = len(data) // 2 & ~3  # one 32-bit word in the middle of the module
        data[at] ^= 0x01
        spv.write_bytes(bytes(data))
        dirty, _, _ = replay(args.after, corpus, args.index)
        ok = sorted(dirty) == [victim]
        return ok, {"slice": len(metas), "changed": victim, "reported": sorted(dirty)}
    finally:
        shutil.rmtree(work, ignore_errors=True)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--corpus", required=True)
    ap.add_argument("--index", required=True, help="dump index (dump-index-test) or the game folder")
    ap.add_argument("--after", required=True, help="shader-replay-test of the build under test")
    ap.add_argument("--before", help="shader-replay-test of the reference build")
    ap.add_argument("--first", type=int)
    ap.add_argument("--count", type=int)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)
    ok, report = (self_test if args.self_test else golden)(args)
    print(("PASS" if ok else "FAIL") + (" (self-test)" if args.self_test else ""))
    for k, v in report.items():
        if k != "details":
            print(f"  {k}: {v}")
    for row in report.get("details", []):
        print(f"  changed {row}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
