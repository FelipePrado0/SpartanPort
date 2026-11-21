"""Scenario files (perf/auto/scenarios/*.toml): data, not code.

    name = "cena-estatica"
    start = "load"            # "load" (a row of the Load list) or "new-game"
    load_index = 0            # Load list row, newest first
    expect_save = "SD-..."    # optional: the save directory the log must confirm
    settle_s = 20             # wait after the level is up before measuring
    duration_s = 60           # measured window
    min_draws = 600           # draws/frame that mean "in the level"
    capture = true            # one SPARTAN_CAPTURE_TRIGGER capture mid-window + a window PNG
    timeout_s = 600           # watchdog for the whole run
    loop_steps = true         # repeat [[steps]] until the window ends
    replay = ""               # or a SPARTAN_PAD_REPLAY file played during the measured window
    route = ""                # or one played before it: walks to the spot, then the window is still

    [env]                     # SPARTAN_* diagnostics for this scenario, e.g. SPARTAN_GPU_PROFILE = 1

    [ini]                     # spartanport.ini overrides for this scenario
    cheat_health = "1"

    [[steps]]                 # pad file tokens (src/runtime_pad.c) held `hold_s`, then `wait_s`
    pad = "rx=60"
    hold_s = 2
    wait_s = 8

    [limits]                  # gate overrides (perf/auto/gate.py DEFAULT_LIMITS)
"""
import tomllib
from pathlib import Path

DEFAULTS = {
    "start": "load", "load_index": 0, "expect_save": "", "settle_s": 20, "duration_s": 60,
    "min_draws": 600, "capture": False, "timeout_s": 600, "loop_steps": False, "replay": "", "route": "",
    "ini": {}, "env": {}, "steps": [], "limits": {}, "description": "",
}
PAD_BUTTONS = {"cross", "circle", "square", "triangle", "l1", "r1", "l2", "r2", "l3", "r3", "options",
               "touchpad", "touchpad_left", "touchpad_right", "up", "down", "left", "right",
               "weapon_next", "weapon_prev", "replay"}


# Set by perf/auto/run_scenario.py itself.
ORCHESTRATOR_ENV = {"SPARTAN_PAD_FILE", "SPARTAN_TOGGLE_FILE", "SPARTAN_FRAME_STATS", "SPARTAN_FPS_LIMIT",
                    "SPARTAN_STALL_MS", "SPARTAN_PAD_REPLAY"}
PATH_SUFFIXES = ("_DIR", "_FILE", "_CONFIG", "_TRIGGER", "_RECORD", "_REPLAY", "_PATH")


class ScenarioError(ValueError):
    pass


def env_value(v):
    """TOML true/false as the game's 1/0 switches; numbers and text as written."""
    return ("1" if v else "0") if isinstance(v, bool) else str(v)


def validate_pad(tokens):
    for t in tokens.split():
        if t in PAD_BUTTONS:
            continue
        if t[:3] in ("lx=", "ly=", "rx=", "ry=") and t[3:].isdigit() and 0 <= int(t[3:]) <= 255:
            continue
        raise ScenarioError(f"unknown pad token {t!r}")


def load(path):
    path = Path(path)
    with open(path, "rb") as f:
        data = tomllib.load(f)
    unknown = set(data) - set(DEFAULTS) - {"name"}
    if unknown:
        raise ScenarioError(f"{path.name}: unknown keys {sorted(unknown)}")
    if not data.get("name"):
        raise ScenarioError(f"{path.name}: name is required")
    s = {**DEFAULTS, **data}
    if s["start"] not in ("load", "new-game"):
        raise ScenarioError(f"{path.name}: start must be load or new-game")
    for key in ("settle_s", "duration_s", "timeout_s", "min_draws", "load_index"):
        if not isinstance(s[key], int) or s[key] < 0:
            raise ScenarioError(f"{path.name}: {key} must be a non-negative integer")
    if s["duration_s"] + s["settle_s"] >= s["timeout_s"]:
        raise ScenarioError(f"{path.name}: timeout_s must exceed settle_s + duration_s")
    for i, step in enumerate(s["steps"]):
        if set(step) - {"pad", "hold_s", "wait_s"} or "pad" not in step:
            raise ScenarioError(f"{path.name}: step {i} needs pad and only hold_s / wait_s")
        validate_pad(step["pad"])
        step.setdefault("hold_s", 0.3)
        step.setdefault("wait_s", 0.0)
    if s["replay"] and s["route"]:
        raise ScenarioError(f"{path.name}: replay and route both play SPARTAN_PAD_REPLAY; use one")
    for key in ("replay", "route"):
        if s[key]:
            s[key] = str((path.parent / s[key]).resolve())
    s["ini"] = {k: str(v) for k, v in s["ini"].items()}
    # Only diagnostics the scenario names (inherited SPARTAN_* are dropped by the orchestrator). No
    # folders, files or the orchestrator's own variables: they could point the game at real saves.
    for k in s["env"]:
        if not k.startswith("SPARTAN_") or k in ORCHESTRATOR_ENV or k.endswith(PATH_SUFFIXES):
            raise ScenarioError(f"{path.name}: [env] {k} is not allowed (SPARTAN_* diagnostics only)")
    s["env"] = {k: env_value(v) for k, v in s["env"].items()}
    return s
