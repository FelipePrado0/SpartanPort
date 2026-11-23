"""Records a route once, with a person playing: the game opens in the warm test folder (a copy of the
test saves, the measuring ini) with SPARTAN_PAD_RECORD on. Load the save, press F9, walk to the spot,
press F9 again and leave through Pause > Exit to Desktop. The recording then serves as a scenario's
`route`, replayed with nobody playing.

usage: python perf/auto/record_route.py perf/auto/scenarios/<name>.pad
"""
import os
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import run_scenario as rs  # noqa: E402
import scenario as scenario_mod  # noqa: E402


def main(argv):
    if len(argv) != 2:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    out = Path(argv[1]).resolve()
    s = {**scenario_mod.DEFAULTS, "name": "record-route", "timeout_s": 10 ** 9}
    s["env"] = {"SPARTAN_PAD_RECORD": str(out)}  # set here, not from a scenario file: only this tool records
    run_dir = Path(rs.REPO) / "perf" / "results" / f"record-{time.strftime('%Y%m%d-%H%M%S')}"
    run_dir.mkdir(parents=True)
    data_dir = rs.PERF_DATA / "warm"
    saves = rs.PERF_DATA / "saves"
    settings = os.environ.get("PERF_SETTINGS", str(rs.REPO / "settings.json"))
    seed = os.environ.get("PERF_CACHE_SEED", str(rs.REPO / "user" / "cache"))
    rs.prepare_data(data_dir, settings, saves, seed if Path(seed).is_dir() else "", s, cold=False)
    game = rs.Game(run_dir, data_dir, s, 0)
    game.start()
    print(f"game open (log: {run_dir / 'session.log'}); waiting for it to close", flush=True)
    game.proc.wait()
    game.log_file.close()
    lines = [l for l in game.log.poll() if "pad recording" in l]
    print("\n".join(lines) or "no 'pad recording' line: F9 was not pressed in the game window")
    n = len(out.read_text().splitlines()) if out.exists() else 0
    print(f"{out}: {n} samples")
    return 0 if n else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
