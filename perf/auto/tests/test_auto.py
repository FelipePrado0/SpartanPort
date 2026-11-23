"""L0 tests of perf/auto with synthetic data (no game, no GPU): python -m unittest discover perf/auto/tests"""
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import capstruct  # noqa: E402
import gate  # noqa: E402
import logparse  # noqa: E402
import scenario  # noqa: E402
import shader_golden  # noqa: E402

FIX = Path(__file__).resolve().parent / "fixtures"


def frame_line(fps, us, draws, worst=12.0, compiles=0):
    return (f"Frame stats: {fps} FPS, worst frame {worst} ms (vblank 240 Hz); {compiles} shader/pipeline "
            "compiles, 0.0 ms; async: capture 0.0 ms, workers 0.0 ms, sync wait 0.0 ms, snapshots 0.0 ms, "
            "GPU capture wait 0.0 ms, 0 pending draws; 0 recorder syncs; 0 write faults/s, 0 hot pages; "
            f"GPU thread {us} us/draw, {draws} draws/frame, idle 1.0%; blocked: recorder 0.0%, host copies "
            "0.0% (0/frame), copy threads 0.0%, GPU ticks 0.0%; reduced-size draws 0/frame of 0 in the scene")


class LogParse(unittest.TestCase):
    def test_real_session_lines(self):
        lines = (FIX / "session.log").read_text(encoding="utf-8").splitlines()
        p = logparse.parse(lines, min_draws=600)
        self.assertEqual(len(p["frames"]), 3)  # the 112-draw menu window is dropped
        self.assertEqual(p["disk"]["compiled"], 4)
        self.assertEqual(p["binary"]["compiled"], 7)
        self.assertIsNone(p["crash"])
        s = logparse.summarize(p)
        self.assertEqual(s["fps_median"], 101.5)
        self.assertEqual(s["us_per_draw_median"], 2.81)
        self.assertEqual(s["p99_frametime_ms"], 14.2)
        self.assertEqual(s["frames_over_25ms"], 2)
        self.assertEqual(s["frames_over_40ms"], 1)
        self.assertEqual(s["window_compiles"], 3)

    def test_crash_and_empty(self):
        p = logparse.parse(["Unhandled exception 0xc0000005 at 0x1234"])
        self.assertIn("Unhandled exception", p["crash"])
        s = logparse.summarize(p)
        self.assertIsNone(s["fps_median"])
        self.assertEqual(s["frames_over_40ms"], 0)

    def test_spread(self):
        self.assertEqual(logparse.spread([3, None, 1, 2]), (2, 1, 3))
        self.assertEqual(logparse.spread([None]), (None, None, None))


def result(fps=100.0, us=2.8, p99=14.0, s40=0, compiles=0, cache="warm", **extra):
    return {"scenario": "t", "cache": cache, "summary": {
        "fps_median": fps, "us_per_draw_median": us, "p99_frametime_ms": p99,
        "frames_over_40ms": s40, "window_compiles": compiles, "crashes": 0}, **extra}


class Gate(unittest.TestCase):
    def test_pass_inside_noise(self):
        self.assertEqual(gate.check(result(fps=98.0, us=2.86), result())[0], "PASS")

    def test_each_threshold(self):
        base = result()
        self.assertEqual(gate.check(result(fps=96.5), base)[0], "FAIL")
        self.assertEqual(gate.check(result(us=2.9), base)[0], "FAIL")
        self.assertEqual(gate.check(result(p99=15.5), base)[0], "FAIL")
        self.assertEqual(gate.check(result(s40=3), base)[0], "FAIL")
        self.assertEqual(gate.check(result(s40=2), base)[0], "PASS")  # under +3 absolute
        self.assertEqual(gate.check(result(compiles=1), base)[0], "FAIL")
        self.assertEqual(gate.check(result(compiles=5, cache="cold"), result(cache="cold"))[0], "PASS")
        # Only compiles beyond the baseline's count: a build's fixed per-start compiles are not new.
        self.assertEqual(gate.check(result(compiles=2), result(compiles=2))[0], "PASS")
        self.assertEqual(gate.check(result(compiles=3), result(compiles=2))[0], "FAIL")

    def test_stalls_need_both_percent_and_absolute(self):
        self.assertEqual(gate.check(result(s40=12), result(s40=10))[0], "PASS")  # +2, +20 %
        self.assertEqual(gate.check(result(s40=16), result(s40=10))[0], "FAIL")  # +6, +60 %

    def test_infra_fail_is_not_a_regression(self):
        v, reasons = gate.check({"infra_fail": "no log line for the level within 240 s"}, result())
        self.assertEqual(v, "INFRA_FAIL")
        self.assertIn("level", reasons[0])

    def test_crash_and_structure(self):
        r = result()
        r["summary"]["crashes"] = 1
        self.assertEqual(gate.check(r, result())[0], "FAIL")
        self.assertEqual(gate.check(result(structure_mismatch="entry #3 targets"), result())[0], "FAIL")

    def test_limit_override_and_cli(self):
        self.assertEqual(gate.check(result(fps=96.5), result(), {"fps_drop_pct": 5})[0], "PASS")
        with tempfile.TemporaryDirectory() as d:
            a, b = Path(d) / "a.json", Path(d) / "b.json"
            a.write_text(json.dumps(result(fps=90.0)))
            b.write_text(json.dumps(result()))
            self.assertEqual(gate.main(["gate", str(a), str(b)]), 1)
            a.write_text(json.dumps({"infra_fail": "x", "scenario": "t"}))
            self.assertEqual(gate.main(["gate", str(a), str(b)]), 2)

    def test_cli_compares_frame_structure(self):
        with tempfile.TemporaryDirectory() as d:
            ref = Path(d) / "ref.txt"
            ref.write_text((FIX / "frame_a.txt").read_text(encoding="utf-8"), encoding="utf-8")
            changed = Path(d) / "run.txt"
            changed.write_text(ref.read_text(encoding="utf-8").replace("D32SfloatS8Uint", "D16Unorm"),
                               encoding="utf-8")
            a, b = Path(d) / "a.json", Path(d) / "b.json"
            b.write_text(json.dumps(result(capture_ref="ref.txt")))
            a.write_text(json.dumps(result(captures=[str(FIX / "frame_b.txt")])))
            self.assertEqual(gate.main(["gate", str(a), str(b)]), 0)
            a.write_text(json.dumps(result(captures=[str(changed)])))
            self.assertEqual(gate.main(["gate", str(a), str(b)]), 1)


class GateNoise(unittest.TestCase):
    def test_limits_widen_to_twice_the_baseline_noise(self):
        base = result(fps=190.0, p99=9.0)
        base["spread"] = {"fps_median": [182.0, 198.0], "p99_frametime_ms": [8.0, 12.0]}  # +-4.2 %, +-22 %
        self.assertEqual(gate.check(result(fps=180.0, p99=12.0), base)[0], "PASS")  # -5.3 %, +33 %
        self.assertEqual(gate.check(result(fps=170.0), base)[0], "FAIL")  # -10.5 % > 8.4 %
        self.assertEqual(gate.check(result(p99=14.0), base)[0], "FAIL")  # +56 % > 44 %

    def test_other_gpu_is_infra(self):
        base = result(machine={"gpu": "GPU A"})
        self.assertEqual(gate.check(result(machine={"gpu": "GPU B"}), base)[0], "INFRA_FAIL")
        self.assertEqual(gate.check(result(machine={"gpu": "GPU A"}), base)[0], "PASS")

    def test_missing_capture_fails_and_notes(self):
        self.assertEqual(gate.check(result(), result(capture_ref="ref.txt"))[0], "FAIL")
        n = gate.notes(result(us=None, runs=1, runs_requested=3), result())
        self.assertTrue(any("us/draw" in x for x in n) and any("1 of 3" in x for x in n))

    def test_baseline_limits_win_over_the_result(self):
        with tempfile.TemporaryDirectory() as d:
            a, b = Path(d) / "a.json", Path(d) / "b.json"
            b.write_text(json.dumps(result()))
            a.write_text(json.dumps(result(fps=90.0, limits={"fps_drop_pct": 50})))
            self.assertEqual(gate.main(["gate", str(a), str(b)]), 1)


class FakeProc:
    def __init__(self, code=None):
        self.returncode = code

    def poll(self):
        return self.returncode


class Orchestrator(unittest.TestCase):
    def setUp(self):
        import run_scenario
        self.rs = run_scenario
        self.dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.dir.cleanup)
        self.path = Path(self.dir.name) / "session.log"

    def test_wait_tests_each_line_once(self):
        heavy = frame_line(100.0, 0.0, 2000)
        self.path.write_text(heavy + "\n", encoding="utf-8")
        log, seen = self.rs.Log(self.path), []

        def two(line):
            seen.append(line)
            return len(seen) >= 2
        with self.assertRaises(self.rs.InfraFail):
            log.wait(two, 0.6, "two heavy windows", FakeProc())
        self.assertEqual(len(seen), 1)  # one line, tested once over several polls

    def test_game_exit_is_not_infra(self):
        self.path.write_text("Free camera: title screen\n", encoding="utf-8")
        with self.assertRaises(self.rs.GameEnded):
            self.rs.Log(self.path).wait(lambda l: "in play" in l, 5, "the level", FakeProc(3))

    def test_clear_dir_moves_aside_and_empties(self):
        cold = Path(self.dir.name) / "cold"
        (cold / "user" / "cache").mkdir(parents=True)
        (cold / "user" / "cache" / "a.spv").write_bytes(b"x")
        self.rs.clear_dir(cold)
        self.assertFalse(cold.exists())
        self.assertEqual(list((Path(self.dir.name) / "trash").iterdir()), [])
        self.rs.clear_dir(cold)  # nothing to move: still fine

    def test_gpu_load_parsing(self):
        # Video decode/encode run on their own block: a browser video does not compete with the game.
        text = ("msedge|video codec 0|57,1\nmsedge|high priority 3d|5,5\ndwm|high priority 3d|3,6\n"
                "spartanport-probe|3D|91.0\n|3D|4\nbroken line\nAMDRSServ|high priority compute|83,7\n"
                "osu!|3d|14,4\nobs64|video encode 0|40\nx|copy|2,0\n")
        self.assertEqual(self.rs.parse_gpu_load(text), {"msedge": 5.5, "AMDRSServ": 83.7, "osu!": 14.4, "x": 2.0})
        self.assertLess(5.5, self.rs.BUSY_GPU_PCT)

    def test_combine_keeps_crashes(self):
        class Args:
            cache, runs, toggles = "warm", 2, 0
        s = {"name": "x", "limits": {}}
        crash = {"id": "1", "infra_fail": None, "crash": "game ended (exit 3)", "summary": None}
        good = {"id": "2", "infra_fail": None, "crash": None, "capture": None, "load_s": 30.0,
                "summary": logparse.summarize(logparse.parse([frame_line(100.0, 0.0, 2000)]))}
        c = self.rs.combine(s, Args, [crash, good])
        self.assertIsNone(c["infra_fail"])
        self.assertEqual(c["summary"]["crashes"], 1)
        self.assertEqual(gate.check(c, c)[0], "FAIL")
        only = self.rs.combine(s, Args, [crash])
        self.assertIsNone(only["infra_fail"])  # a crash is a result, not an infrastructure failure
        self.assertEqual(gate.check(only, c)[0], "FAIL")


class CapStruct(unittest.TestCase):
    def setUp(self):
        self.ref = capstruct.parse((FIX / "frame_a.txt").read_text(encoding="utf-8"))

    def test_parse(self):
        self.assertEqual([e["kind"] for e in self.ref], ["PASS", "COMPUTE", "PASS"])
        self.assertEqual(self.ref[0]["targets"][0], ("color0", "R8G8B8A8Srgb", "1920x1080"))
        self.assertEqual(self.ref[0]["targets"][1], ("depth", "D32SfloatS8Uint", "1920x1080"))
        self.assertEqual(self.ref[1]["shaders"], ["000000009a9cf8a9"])

    def test_addresses_ignored_draws_optional(self):
        other = capstruct.parse((FIX / "frame_b.txt").read_text(encoding="utf-8"))  # moved, more draws
        self.assertIsNone(capstruct.diff(self.ref, other))
        self.assertIn("count", capstruct.diff(self.ref, other, draws=True))

    def test_format_change_is_reported(self):
        text = (FIX / "frame_a.txt").read_text(encoding="utf-8").replace("R8G8B8A8Srgb", "R16G16B16A16Sfloat", 1)
        self.assertIn("targets", capstruct.diff(self.ref, capstruct.parse(text)))
        self.assertIn("passes", capstruct.diff(self.ref, self.ref[:2]))


class Scenario(unittest.TestCase):
    def write(self, text):
        f = tempfile.NamedTemporaryFile("w", suffix=".toml", delete=False, encoding="utf-8")
        f.write(text)
        f.close()
        self.addCleanup(Path(f.name).unlink)
        return f.name

    def test_shipped_scenarios_load(self):
        files = sorted((Path(__file__).resolve().parents[1] / "scenarios").glob("*.toml"))
        self.assertTrue(files)
        for f in files:
            s = scenario.load(f)
            self.assertTrue(s["name"])

    def test_route_is_resolved_and_exclusive_with_replay(self):
        s = scenario.load(self.write('name = "x"\nroute = "walk.pad"\n'))
        self.assertTrue(Path(s["route"]).is_absolute() and s["route"].endswith("walk.pad"))
        with self.assertRaises(scenario.ScenarioError):
            scenario.load(self.write('name = "x"\nroute = "a.pad"\nreplay = "b.pad"\n'))

    def test_env_switches_become_game_values(self):
        s = scenario.load(self.write('name = "x"\n[env]\nSPARTAN_GPU_PROFILE = true\nSPARTAN_X = 3\n'))
        self.assertEqual(s["env"], {"SPARTAN_GPU_PROFILE": "1", "SPARTAN_X": "3"})

    def test_defaults_and_ini_strings(self):
        s = scenario.load(self.write('name = "x"\n[ini]\ncheat_health = 1\n[[steps]]\npad = "rx=60 square"\n'))
        self.assertEqual(s["start"], "load")
        self.assertEqual(s["ini"], {"cheat_health": "1"})
        self.assertEqual(s["steps"][0]["hold_s"], 0.3)

    def test_rejects(self):
        for bad in ['start = "load"', 'name = "x"\nfoo = 1', 'name = "x"\nstart = "menu"',
                    'name = "x"\n[[steps]]\npad = "jump"', 'name = "x"\n[[steps]]\npad = "lx=300"',
                    'name = "x"\nduration_s = 900', 'name = "x"\n[[steps]]\nhold_s = 1',
                    'name = "x"\n[env]\nPATH = "x"', 'name = "x"\n[env]\nSPARTAN_USER_DIR = "C:/u"',
                    'name = "x"\n[env]\nSPARTAN_CONFIG = "a.ini"', 'name = "x"\n[env]\nSPARTAN_PAD_FILE = "p"']:
            with self.assertRaises(scenario.ScenarioError, msg=bad):
                scenario.load(self.write(bad + "\n"))


class ShaderGolden(unittest.TestCase):
    def test_hash_combine_matches_cache_names(self):
        # perm 0 of program 0x2c65c47: HashCombine(seed, 0) by common/hash.h, in u64.
        seed = 0x2C65C47
        self.assertEqual(shader_golden.hash_combine(seed, 0), seed ^ (0x9E3779B9 + (seed << 12) + (seed >> 4)))
        big = 0xFFFFFFFFFFFFFFFF
        self.assertLess(shader_golden.hash_combine(big, 7), 1 << 64)

    def test_meta_to_spv(self):
        with tempfile.TemporaryDirectory() as d:
            pgm = 0xABCDEF0123456789
            (Path(d) / f"{pgm:#018x}_2.spv").write_bytes(b"")
            (Path(d) / f"{shader_golden.hash_combine(pgm, 2):#018x}.meta").write_bytes(b"")
            (Path(d) / "0x0000000000000001.meta").write_bytes(b"")
            m = shader_golden.meta_to_spv(d)
            self.assertEqual(m[f"{shader_golden.hash_combine(pgm, 2):#018x}.meta"], f"{pgm:#018x}_2.spv")
            self.assertIsNone(m["0x0000000000000001.meta"])

    def test_differ_line(self):
        m = shader_golden.DIFFER.match("differ: Vertex+fetch 0x00dc0eafb14fc268.meta (39704 vs 39708 bytes)")
        self.assertEqual((m["stage"], m["meta"], m["got"]), ("Vertex+fetch", "0x00dc0eafb14fc268.meta", "39704"))


if __name__ == "__main__":
    unittest.main()
