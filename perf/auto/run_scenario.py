"""Runs one scenario in the game with nobody playing and writes results/<id>.json.

usage: python perf/auto/run_scenario.py --scenario perf/auto/scenarios/cena-estatica.toml
           [--cache warm|cold] [--runs 3] [--warmup 1] [--out perf/results] [--toggles 0]
           [--settings <settings.json>] [--saves <savedata folder>] [--cache-seed <user/cache>]

Every run: an isolated data folder (SPARTAN_DATA_DIR) with a copy of the test saves, the game started
with `spartanport-launcher --play`, menus driven by SPARTAN_PAD_FILE and PostMessage (never global
input), each screen confirmed by a log line, the measured window cut out of the log, then Exit to
Desktop through the pause menu (the process tree is ended only when that fails or the watchdog
fires). A step the log does not confirm is INFRA_FAIL and the run is repeated up to twice.
"""
import argparse
import datetime
import json
import os
import shutil
import statistics
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import logparse  # noqa: E402
import scenario as scenario_mod  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
# A cold start with the shipped shader list prepares shaders and pipelines before the title (7.5 min
# measured on an RX 6700 XT): added to the scenario's watchdog for --cache cold.
COLD_PREPARATION_S = 1500
PERF_DATA = REPO / "out" / "perf"
PROBE = "spartanport-probe.exe" if sys.platform == "win32" else "spartanport-probe"
# The measured configuration (roadmap W0): 1080p render and output, no upscaler, no frame
# generation, no overlay, engine at its highest rate, no frame limit. PERF_MONITOR: the display index
# from `spartanport-probe --displays` (the same one on every run, so baselines stay comparable).
MONITOR = os.environ.get("PERF_MONITOR", "0")
TEST_INI = {
    "upscaler": "off", "frame_generation": "off", "anti_aliasing": "off", "fsr1": "0", "rcas": "0",
    "render_resolution": "native", "output_res": "1920x1080", "display_mode": "windowed",
    "monitor": MONITOR, "vsync": "0", "fps_limit": "0", "engine_fps": "240", "hdr": "0",
    "show_fps": "0", "overlay_fps": "0", "overlay_frametime": "0", "overlay_upscaler": "0",
    "overlay_gpu": "0", "overlay_cpu": "0", "overlay_ram": "0", "overlay_vram": "0",
    "pause_game": "0", "auto_qte": "1", "cheat_health": "1", "free_camera": "0",
    "restart_unconfirmed": "0", "auto_safe_fps": "1",
}
SETTINGS_OVERRIDES = {"user_dir": "", "discord_rpc": False, "check_updates": False, "frame_stats": True,
                      "gpu_profile": False, "vk_validation": False, "close_on_play": False,
                      "shader_notes": False, "monitor": MONITOR}


class InfraFail(Exception):
    """A step the log did not confirm (menu, window, pad): repeated, never a regression."""


class GameEnded(Exception):
    """The game process ended on its own: a crash for the gate, never repeated."""


def probe_pids():
    if sys.platform != "win32":
        out = subprocess.run(["pgrep", "-x", PROBE], capture_output=True, text=True).stdout
        return {int(p) for p in out.split()}
    out = subprocess.run(["tasklist", "/FO", "CSV", "/NH", "/FI", f"IMAGENAME eq {PROBE}"],
                         capture_output=True).stdout.decode("ascii", "replace")  # localized text
    return {int(line.split('","')[1]) for line in out.splitlines() if line.startswith(f'"{PROBE}"')}


def kill_tree(pid):
    if sys.platform == "win32":
        subprocess.run(["taskkill", "/PID", str(pid), "/T", "/F"], capture_output=True)
    else:
        subprocess.run(["pkill", "-TERM", "-P", str(pid)], capture_output=True)
        subprocess.run(["kill", "-TERM", str(pid)], capture_output=True)


# Another program using the GPU above this (sum of its engines, %) makes the run INFRA_FAIL: a video
# playing in a browser cost 7 to 22 % FPS in the measured scene on 2026-11-18.
BUSY_GPU_PCT = 10.0
GPU_LOAD_PS = (
    "$s=(Get-Counter '\\GPU Engine(*)\\Utilization Percentage' -ErrorAction SilentlyContinue).CounterSamples;"
    "$s|?{$_.CookedValue -gt 0.5}|%{$i=($_.InstanceName -split '_')[1];$p=Get-Process -Id $i -EA 0;"
    "'{0}|{1}|{2:N1}' -f $p.Name,($_.InstanceName -split 'engtype_')[1],$_.CookedValue}")


def other_gpu_load():
    """{process: GPU %} of the other programs right now (Windows performance counters; dwm and the
    game itself left out). Empty when the counters cannot be read."""
    if sys.platform != "win32":
        return {}  # shortcut: no portable per-process GPU counter on Linux; add one when L2 runs there
    try:
        out = subprocess.run(["powershell", "-NoProfile", "-Command", GPU_LOAD_PS], capture_output=True,
                             timeout=30).stdout.decode("utf-8", "replace")
    except (OSError, subprocess.TimeoutExpired):
        return {}
    return parse_gpu_load(out)


def parse_gpu_load(text):
    """`name|engine|percent` lines (GPU_LOAD_PS) summed per process, without dwm and the game. Only the
    engines the game uses count (3D, compute, copy): video decode and encode are a separate block."""
    load = {}
    for line in text.splitlines():
        parts = line.strip().split("|")
        if len(parts) != 3 or not parts[0] or parts[0].lower().startswith(("spartanport", "dwm")):
            continue
        if "video" in parts[1].lower():
            continue
        try:
            load[parts[0]] = round(load.get(parts[0], 0.0) + float(parts[2].replace(",", ".")), 1)
        except ValueError:
            continue
    return load


def write_ini(path, values):
    path.write_text("".join(f"{k}={v}\n" for k, v in values.items()), encoding="utf-8")


class Log:
    """The session log, read as it grows."""

    def __init__(self, path):
        self.path, self.pos, self.lines = path, 0, []
        self.deadline = float("inf")  # the scenario's watchdog (timeout_s), set when the game starts

    def poll(self):
        if self.path.exists():
            with open(self.path, "rb") as f:
                f.seek(self.pos)
                data = f.read()
            cut = data.rfind(b"\n") + 1
            self.pos += cut
            self.lines += data[:cut].decode("utf-8", "replace").splitlines()
        return self.lines

    def wait(self, test, timeout, what, proc, since=0):
        """The first line from `since` on that passes `test`; each line is tested once."""
        end = min(time.monotonic() + timeout, self.deadline)
        while True:
            lines = self.poll()
            for i in range(since, len(lines)):
                if test(lines[i]):
                    return lines[i]
            since = len(lines)
            if proc.poll() is not None:
                raise GameEnded(f"game ended (exit {proc.returncode}) while waiting for {what}")
            if time.monotonic() >= end:
                break
            time.sleep(0.25)
        if end == self.deadline:
            raise InfraFail(f"watchdog (timeout_s) while waiting for {what}")
        raise InfraFail(f"no log line for {what} within {timeout} s")


class Game:
    def __init__(self, run_dir, data_dir, s, toggles):
        self.run_dir, self.data_dir, self.s = run_dir, data_dir, s
        self.pad = run_dir / "pad"
        self.toggles = run_dir / "toggles"
        self.trigger = run_dir / "capture.trigger"
        self.log = Log(run_dir / "session.log")
        self.toggle_mask = toggles
        self.proc = self.hwnd = None
        self.notes = []

    def press(self, tokens, hold=0.3, wait=0.0):
        self.pad.write_text(tokens, encoding="ascii")
        time.sleep(hold)
        self.pad.write_text("", encoding="ascii")
        time.sleep(max(wait, 0.1))

    def key(self, name):
        if sys.platform != "win32":
            raise InfraFail("the Load list needs PostMessage, which only the Windows path has")
        import window
        window.key(self.window(), name)

    def window(self):
        if not self.hwnd:
            import window
            self.hwnd = window.find(probe_pids() - self.pids_before)
            if not self.hwnd:
                raise InfraFail("game window not found")
        return self.hwnd

    def screenshot(self, name):
        if sys.platform != "win32":
            return None
        import window
        path = self.run_dir / f"{name}.png"
        return str(path) if window.capture(self.window(), str(path)) else None

    def start(self):
        self.pids_before = probe_pids()
        if self.pids_before:
            raise RuntimeError("the game is already running; not touching it (someone may be playing)")
        self.pad.write_text("", encoding="ascii")
        self.toggles.write_text(str(self.toggle_mask), encoding="ascii")
        # Inherited SPARTAN_* (SPARTAN_USER_DIR, SPARTAN_CONFIG, ...) would win over the isolated data
        # folder and could point the game at the real saves.
        env = {k: v for k, v in os.environ.items() if not k.upper().startswith("SPARTAN_")}
        env.update({
            "SPARTAN_DATA_DIR": str(self.data_dir), "SPARTAN_PAD_FILE": str(self.pad),
            "SPARTAN_TOGGLE_FILE": str(self.toggles), "SPARTAN_FRAME_STATS": "1",
            "SPARTAN_FPS_LIMIT": "0", "SPARTAN_STALL_MS": "25",
            "SPARTAN_CAPTURE_TRIGGER": str(self.trigger),
            "SPARTAN_CAPTURE_DIR": str(self.run_dir / "captures"),
        })
        if self.s["replay"] or self.s["route"]:
            env["SPARTAN_PAD_REPLAY"] = self.s["replay"] or self.s["route"]
        env.update(self.s["env"])
        # SPARTAN_CPU_SAMPLER = 1 in a scenario: the samples go to this run's folder.
        if str(env.get("SPARTAN_CPU_SAMPLER", "")) == "1":
            env["SPARTAN_CPU_SAMPLER"] = str(self.run_dir / "cpu_samples.tsv")
        exe = REPO / "launcher-rust" / "target" / "release" / (
            "spartanport-launcher.exe" if sys.platform == "win32" else "spartanport-launcher")
        if not exe.exists():
            raise RuntimeError(f"{exe} missing: cargo build --release in launcher-rust/")
        self.log_file = open(self.log.path, "wb")
        self.proc = subprocess.Popen([str(exe), "--play"], cwd=REPO, env=env, stdout=self.log_file,
                                     stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
        self.t0 = time.monotonic()
        self.log.deadline = self.t0 + self.s["timeout_s"]

    def to_level(self, cold):
        s, log, p = self.s, self.log, self.proc
        log.wait(lambda l: l.startswith("Free camera: title screen"), COLD_PREPARATION_S if cold else 300,
                 "the title screen", p)
        log.wait(lambda l: l.startswith("Runtime: pad opened"), 60, "the pad", p)
        # The title menu fades in after SHELL.WAD opens; after a cold start's preparation it took
        # longer to take input (a press 8 s in was lost once).
        time.sleep(20 if cold else 8)
        self.screenshot("title")
        mark = len(log.poll())
        if s["start"] == "load":
            self.press("down", 0.15, 1.0)  # title: New Game, Load, Options, Treasures
            self.screenshot("title-load")
            self.press("cross", 0.3, 3.0)
            self.screenshot("load-list")
            for _ in range(s["load_index"]):
                self.key("s")
                time.sleep(0.4)
            self.key("enter")
            chose = log.wait(lambda l: "SaveDataDialog list: " in l, 20, "the Load list choice", p, mark)
            if "chose" not in chose:
                raise InfraFail(f"Load list: {chose}")
            got = chose.split("chose ", 1)[1].strip()
            if s["expect_save"] and got != s["expect_save"]:
                raise InfraFail(f"Load list chose {got}, expected {s['expect_save']}")
            self.notes.append(f"save {got}")
        else:
            self.press("cross", 0.3, 3.0)  # New Game
            self.screenshot("difficulty")
            self.press("cross", 0.3, 1.0)  # default difficulty
        line = log.wait(lambda l: l.startswith("Free camera: in play"), 240, "the level", p, mark)
        self.notes.append(line)
        mark = self.level_mark = len(log.poll())
        need = 2

        def heavy(l):
            nonlocal need
            m = logparse.FRAME_STATS.match(l)
            if m and float(m["draws"]) >= s["min_draws"]:
                need -= 1
            return need <= 0
        log.wait(heavy, 300, f"{s['min_draws']} draws/frame", p, mark)

    def measure(self):
        s = self.s
        if s["route"]:  # walk to the measured spot first (a recording made with record_route.py)
            mark = len(self.log.poll())
            self.press("replay", 0.3)
            self.log.wait(lambda l: l.startswith("Runtime: pad replay finished"), 180, "the end of the route",
                          self.proc, mark)
            self.screenshot("route-end")
        time.sleep(s["settle_s"])
        start = len(self.log.poll())
        end = time.monotonic() + s["duration_s"]
        if s["replay"]:
            self.press("replay", 0.3)
        every = float(os.environ.get("PERF_SHOT_EVERY", "0"))  # debugging a new route: periodic PNGs
        next_shot = time.monotonic() + every
        steps_left = not s["replay"] and bool(s["steps"])
        while time.monotonic() < end:
            if every and time.monotonic() >= next_shot:
                next_shot += every
                self.screenshot(f"t{int(time.monotonic() - end + s['duration_s']):04d}")
            if self.proc.poll() is not None:
                raise GameEnded(f"game ended (exit {self.proc.returncode}) during the measured window")
            if time.monotonic() >= self.log.deadline:
                raise InfraFail("watchdog (timeout_s) during the measured window")
            if steps_left:
                for step in s["steps"]:
                    if time.monotonic() >= end:
                        break
                    self.press(step["pad"], step["hold_s"], step["wait_s"])
                steps_left = s["loop_steps"]
                continue
            time.sleep(min(1.0, max(0.0, end - time.monotonic())))
        self.pad.write_text("", encoding="ascii")
        stop = len(self.log.poll())
        if s["capture"]:  # after the window: recording a frame and the PNG cost frame time
            self.trigger.write_text("", encoding="ascii")
            try:
                self.log.wait(lambda l: l.startswith("Frame capture: ") and "written" in l, 15,
                              "the frame capture", self.proc, stop)
            except InfraFail as e:
                self.notes.append(f"no capture: {e}")
            self.screenshot("scene")
        return start, stop

    def exit(self):
        """Exit to Desktop through the pause menu (Continue, Options, Photo Mode, Restart, Quit Game,
        Exit to Desktop); ends the process tree only if the game does not close."""
        if self.proc.poll() is None:
            how = "killed"
            # The game ignored the pause press once (2026-11-18): one more try before ending it.
            for attempt in range(2):
                try:
                    suffix = "-retry" if attempt else ""
                    self.press("touchpad_right", 0.3, 3.0)  # the menu ignores input while it slides in
                    self.screenshot("pause" + suffix)
                    for i in range(int(os.environ.get("PERF_EXIT_DOWNS", "5"))):
                        self.press("down", 0.15, 0.6)
                        if os.environ.get("PERF_EXIT_SHOTS"):
                            self.screenshot(f"pause-down{i + 1}{suffix}")
                    self.screenshot("pause-exit" + suffix)
                    self.press("cross", 0.3, 2.0)
                    # The row asks Quit Game's question with No selected (scripts/pause_menu.py).
                    self.screenshot("exit-question" + suffix)
                    self.press(os.environ.get("PERF_EXIT_YES", "up"), 0.15, 0.8)
                    self.screenshot("exit-yes" + suffix)
                    self.press("cross", 0.3, 0.5)
                    self.proc.wait(timeout=20)
                    how = "menu" if not attempt else "menu (second try)"
                    break
                except Exception:
                    continue
        else:
            how = f"ended by itself (exit {self.proc.returncode})"
        if self.proc.poll() is None:
            kill_tree(self.proc.pid)
            how = "killed"
        for pid in probe_pids() - self.pids_before:
            kill_tree(pid)
        self.log_file.close()
        return how


def ship_shader_lists():
    """The game reads the shipped shader list from shaders/ beside its executable (vk_presenter.cpp),
    which packaging/*/ fills and build.sh does not: without this a checkout's cold start never uses
    the list. Copies shaders/*.spm into out/shaders/ when missing or different; returns the names."""
    names = []
    for src in sorted((REPO / "shaders").glob("*.spm")):
        dst = REPO / "out" / "shaders" / src.name
        if os.environ.get("PERF_SHADER_LISTS") == "0":  # A/B of the list itself: a checkout without it
            dst.unlink(missing_ok=True)
            continue
        if not dst.exists() or dst.read_bytes() != src.read_bytes():
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(src, dst)
        names.append(src.name)
    return names


def clear_dir(path):
    """Empties `path` for a cold run. Deleting ~55k cache files in place failed on Windows (WinError
    145: entries still pending when the folder is removed), so the folder is renamed aside at once
    and the old ones are deleted as far as they go; leftovers retry on the next run."""
    trash_root = path.parent / "trash"
    trash_root.mkdir(exist_ok=True)
    for attempt in range(6):  # antivirus held files of a fresh cache for ~50 s once
        try:
            if path.exists():
                path.rename(trash_root / f"{path.name}-{time.time_ns()}")
            break
        except PermissionError as e:
            if attempt == 5:
                raise RuntimeError(f"cold folder {path} still locked after 50 s (antivirus?): {e}") from e
            time.sleep(10)
    for old in trash_root.iterdir():
        shutil.rmtree(old, ignore_errors=True)


def prepare_data(data_dir, settings_src, saves_src, cache_seed, s, cold):
    if cold:
        clear_dir(data_dir)
    (data_dir / "user").mkdir(parents=True, exist_ok=True)
    settings = json.loads(Path(settings_src).read_text(encoding="utf-8"))
    settings.update(SETTINGS_OVERRIDES)
    (data_dir / "settings.json").write_text(json.dumps(settings, indent=2), encoding="utf-8")
    write_ini(data_dir / "spartanport.ini", {**TEST_INI, **s["ini"]})
    saves = data_dir / "user" / "savedata"
    if saves.exists():
        shutil.rmtree(saves)
    shutil.copytree(saves_src, saves)
    cache = data_dir / "user" / "cache"
    if not cold and not cache.exists() and cache_seed:
        shutil.copytree(cache_seed, cache)
    return settings


def run_once(s, args, run_id, cold):
    run_dir = Path(args.out) / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    data_dir = PERF_DATA / ("cold" if cold else "warm")
    saves = PERF_DATA / "saves"
    if not saves.exists():
        shutil.copytree(args.saves, saves)  # frozen once: runs never touch the real saves
    prepare_data(data_dir, args.settings, saves, args.cache_seed, s, cold)
    game = Game(run_dir, data_dir, s, args.toggles)
    result = {"id": run_id, "infra_fail": None, "notes": game.notes, "shader_lists": ship_shader_lists()}
    game.start()
    if cold:
        game.log.deadline += COLD_PREPARATION_S
    ended = None
    try:
        game.to_level(cold)
        result["load_s"] = round(time.monotonic() - game.t0, 1)
        a, b = game.measure()
        # Sampled with the game still up, after the window so the sampling itself is not measured.
        result["other_gpu"] = other_gpu_load()
        busy = {k: v for k, v in result["other_gpu"].items() if v >= BUSY_GPU_PCT}
        if busy:
            result["infra_fail"] = "machine busy: " + ", ".join(f"{k} GPU {v} %" for k, v in busy.items())
        lines = game.log.poll()
        result["summary"] = logparse.summarize(logparse.parse(lines[a:b], s["min_draws"]))
        # Shader stalls count from the level's first frame, light windows included (loading, settle,
        # streaming), not only the measured heavy windows.
        since_level = logparse.parse(lines[game.level_mark:b])
        result["summary"]["window_compiles"] = int(sum(f["compiles"] for f in since_level["frames"]))
    except GameEnded as e:
        ended = str(e)
    except InfraFail as e:
        result["infra_fail"] = str(e)
        game.screenshot("infra-fail")
    except OSError as e:  # window / capture calls: the run is unusable, not the build
        result["infra_fail"] = f"{type(e).__name__}: {e}"
    finally:
        result["exit"] = game.exit()
    whole = logparse.parse(game.log.poll())
    result["disk"], result["binary"], result["crash"] = whole["disk"], whole["binary"], whole["crash"]
    for line in game.log.poll():  # the first start's preparation (vk_pipeline_serialization.cpp)
        if line.startswith(("Shader preparation: ", "Pipeline preparation: ")):
            result.setdefault("preparation", []).append(line.strip())
    if ended and not result["crash"]:
        result["crash"] = ended  # no crash line in the log: the exit itself is the evidence
    caps = sorted((run_dir / "captures").glob("frame_*.txt")) if (run_dir / "captures").exists() else []
    result["capture"] = str(caps[-1]) if caps else None
    (run_dir / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8", newline="\n")
    return result


def combine(s, args, runs):
    good = [r for r in runs if not r["infra_fail"] and r.get("summary")]
    keys = ["fps_median", "us_per_draw_median", "draws_per_frame_median", "p99_frametime_ms",
            "frames_over_25ms", "frames_over_40ms", "window_compiles", "worst_frame_ms"]
    summary, spread = {}, {}
    for k in keys:
        med, lo, hi = logparse.spread(r["summary"][k] for r in good)
        summary[k], spread[k] = med, [lo, hi]
    summary["crashes"] = sum(1 for r in runs if r.get("crash"))
    summary["crash_line"] = next((r["crash"] for r in runs if r.get("crash")), "")
    crashed = [r for r in runs if r.get("crash")]
    infra = None if good or crashed else "; ".join(r["infra_fail"] or "no samples" for r in runs)
    return {"scenario": s["name"], "cache": args.cache, "runs": len(good), "runs_requested": args.runs,
            "summary": summary,
            "spread": spread, "infra_fail": infra, "limits": s["limits"],
            "machine": machine(), "build": build_id(), "toggles": args.toggles,
            "date": datetime.datetime.now().isoformat(timespec="seconds"),
            "run_ids": [r["id"] for r in runs], "captures": [r["capture"] for r in good if r.get("capture")],
            "load_s": logparse.spread(r.get("load_s") for r in good)[0]}


def machine():
    info = {}
    log = PERF_DATA / "warm" / "user" / "last_run.log"
    if log.exists():
        for line in log.read_text(encoding="utf-8", errors="replace").splitlines()[:200]:
            if line.startswith("GPU: using "):
                info["gpu"] = line[len("GPU: using "):]
    return info


def build_id():
    rev = subprocess.run(["git", "-C", str(REPO), "rev-parse", "--short", "HEAD"], capture_output=True,
                         text=True).stdout.strip()
    dirty = subprocess.run(["git", "-C", str(REPO), "status", "--porcelain", "--untracked-files=no",
                            "--ignore-submodules"],
                           capture_output=True, text=True).stdout.strip()
    return rev + ("+dirty" if dirty else "")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--scenario", required=True)
    ap.add_argument("--target", default="spartanport", choices=["spartanport", "rehooked"])
    ap.add_argument("--cache", default="warm", choices=["warm", "cold"])
    ap.add_argument("--runs", type=int, default=3)
    ap.add_argument("--warmup", type=int, default=1)
    ap.add_argument("--out", default=str(REPO / "perf" / "results"))
    ap.add_argument("--toggles", type=int, default=0)
    ap.add_argument("--settings", default=os.environ.get("PERF_SETTINGS", str(REPO / "settings.json")))
    ap.add_argument("--saves", default=os.environ.get("PERF_SAVES", str(REPO / "user" / "savedata")))
    ap.add_argument("--cache-seed", default=os.environ.get("PERF_CACHE_SEED", str(REPO / "user" / "cache")))
    args = ap.parse_args(argv)
    args.out = str(Path(args.out).resolve())  # the game resolves paths from its own folder
    if args.target != "spartanport":
        print("blocked: no scripted input verified for the other target (roadmap WV-B); measure it "
              "by hand once and mark the result manual", file=sys.stderr)
        return 2
    s = scenario_mod.load(args.scenario)
    if not Path(args.cache_seed).is_dir():
        args.cache_seed = ""
    if args.cache == "warm" and args.warmup == 0 and not (PERF_DATA / "warm" / "user" / "cache").is_dir():
        args.warmup = 1  # a warm run on a cache made just now would count its compiles as stalls
        print("warm cache folder is new: 1 warm-up run added", flush=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    runs, n = [], 0
    for i in range(args.warmup + args.runs):
        for attempt in range(3):  # INFRA_FAIL: repeat up to twice
            n += 1
            rid = f"{s['name']}-{args.cache}-{stamp}-{n}"
            r = run_once(s, args, rid, args.cache == "cold")
            tag = "warmup" if i < args.warmup else "run"
            if r.get("crash") and not r.get("summary"):
                text = f"CRASH {r['crash']}"
            elif r["infra_fail"]:
                text = f"INFRA_FAIL {r['infra_fail']}"
            else:
                text = (f"{r['summary']['fps_median']} FPS, {r['summary']['us_per_draw_median']} us/draw, "
                        f"exit {r['exit']}")
            print(f"{tag} {rid}: {text}", flush=True)
            if not r["infra_fail"]:  # a crash is a result: never repeated away
                break
        if i >= args.warmup or r.get("crash"):
            runs.append(r)
    combined = combine(s, args, runs)
    Path(args.out).mkdir(parents=True, exist_ok=True)
    out = Path(args.out) / f"{s['name']}-{args.cache}-{stamp}.json"
    out.write_text(json.dumps(combined, indent=2), encoding="utf-8", newline="\n")  # LF: baselines are copied from it
    print(f"result: {out}")
    return 2 if combined["infra_fail"] else 0


if __name__ == "__main__":
    sys.exit(main())
