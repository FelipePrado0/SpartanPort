"""Exercise real C trophy services across processes and the production AppRun."""
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
binary = Path(sys.argv[1]).resolve()


def run(args, env, cwd):
    subprocess.run(args, env=env, cwd=cwd, check=True, timeout=30)


with tempfile.TemporaryDirectory(prefix="trophy process ") as folder:
    folder = Path(folder)
    readonly = folder / "readonly mount"
    readonly.mkdir()
    readonly.chmod(0o555)
    base = os.environ.copy()
    base.pop("SPARTAN_USER_DIR", None)
    try:
        for mode in ("fallback", "empty", "override"):
            fallback = folder / f"{mode} runtime user"
            target = folder / "explicit user" if mode == "override" else fallback
            target.mkdir()
            (target / "trophy_texts.txt").write_text(
                "1=Releasing the Floodgates | Kill Poseidon\n", encoding="utf-8"
            )
            env = base.copy()
            if mode != "fallback":
                env["SPARTAN_USER_DIR"] = str(target) if mode == "override" else ""
            for action in ("write", "read"):
                run([str(binary), action, str(fallback), str(target)], env, readonly)
            assert struct.unpack("<4I", (target / "trophies.dat").read_bytes()) == (2, 0, 0, 0)
        assert list(readonly.iterdir()) == []
        print("PASS: native persistence across separate processes, directory precedence and spaces")

        if sys.platform != "win32":
            appdir = folder / "AppDir with spaces"
            share = appdir / "usr/share/spartanport"
            share.mkdir(parents=True)
            apprun = appdir / "AppRun"
            shutil.copyfile(root / "packaging/linux/AppRun", apprun)
            apprun.chmod(0o755)
            launcher = share / "spartanport-launcher"
            launcher.write_text('#!/bin/sh\nexec "$TROPHY_BINARY" "$1" "$SPARTAN_USER_DIR" "$SPARTAN_USER_DIR"\n')
            launcher.chmod(0o755)
            share.chmod(0o555)
            assert not os.access(share, os.W_OK), "Test requires an unprivileged read-only cwd"
            for custom in (False, True):
                env = base.copy()
                env["HOME"] = str(folder / "Home with spaces")
                env["XDG_DATA_HOME"] = str(folder / "XDG with spaces")
                env.pop("SPARTAN_DATA_DIR", None)
                target = folder / "external user" if custom else Path(env["XDG_DATA_HOME"]) / "spartanport/user"
                target.mkdir(parents=True)
                (target / "trophy_texts.txt").write_text("1=Releasing the Floodgates | Kill Poseidon\n")
                if custom:
                    env["SPARTAN_USER_DIR"] = str(target)
                env["TROPHY_BINARY"] = str(binary)
                for action in ("write", "read"):
                    run([str(apprun), action], env, folder)
                assert struct.unpack("<4I", (target / "trophies.dat").read_bytes()) == (2, 0, 0, 0)
            assert not (share / "user").exists()
            share.chmod(0o755)
            print("PASS: production AppRun with read-only cwd, XDG and inherited directory")
    finally:
        readonly.chmod(0o755)
