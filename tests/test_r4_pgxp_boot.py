#!/usr/bin/env python3
"""Does R4 boot with PGXP armed, and with it off when the player says so?

tests/test_r4_pgxp.py checks R4's files; this checks what the runtime actually
does with them, end to end through the framework's session start (mod commit,
activation, and the renderer setup's arming in main.cpp), which no unit test
can reach. It boots the built runtime headless three times from an isolated
copy of the build's runtime directory (so the build's own mods/state.toml and
settings.toml are never touched) and asks the debug server what is armed:

  1. the shipped default (no mod state): geometry and texture correction and
     precise culling on;
  2. PGXP switched off on the Mods page: all off;
  3. PGXP on with its culling option off: corrections on, culling off.

Each boot must reach guest frames with 0 dispatch and 0 segment misses.
Needs a build with PSX_DEBUG_TOOLS and the disc; CMake registers it only then.

Usage: test_r4_pgxp_boot.py --build-dir DIR --disc CUE [--frames N]
"""
import argparse
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

PKG = "psx.enhancement.pgxp"
failures = 0


def check(ok, what):
    global failures
    print(("ok   " if ok else "FAIL ") + what, flush=True)
    if not ok:
        failures += 1


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def ask(port, obj, timeout=10.0):
    with socket.create_connection(("127.0.0.1", port), timeout=timeout) as s:
        s.sendall(json.dumps(obj).encode() + b"\n")
        buf = b""
        s.settimeout(timeout)
        while not buf.endswith(b"\n"):
            chunk = s.recv(1 << 20)
            if not chunk:
                break
            buf += chunk
    for line in reversed(buf.decode(errors="replace").splitlines()):
        try:
            return json.loads(line)
        except ValueError:
            continue
    return {}


def stage(build, root):
    """A runtime directory of symlinks to the build's, with its own mods/ and
    no settings: the runtime anchors mods/, settings.toml and its caches next
    to the path it was started from."""
    for name in ("r4-runtime", "bios", "assets"):
        src = build / name
        if src.exists():
            (root / name).symlink_to(src)
    (root / "mods").mkdir()
    (root / "mods" / "bundled").symlink_to(build / "mods" / "bundled")
    shutil.copy(build / "game.toml", root / "game.toml")


def write_state(root, enabled, values=None):
    version = next((build_root / "mods/bundled" / PKG).iterdir()).name
    lines = ["format_version = 2", "", "[[package]]", 'id = "%s"' % PKG,
             'version = "%s"' % version, "", "[[feature]]",
             'package_id = "%s"' % PKG, 'id = "pgxp"',
             "enabled = %s" % ("true" if enabled else "false")]
    if values:
        lines += ["", "[feature.values]"]
        lines += ["%s = %s" % (k, v) for k, v in values.items()]
    (root / "mods" / "state.toml").write_text("\n".join(lines) + "\n")


def boot(root, disc, frames, label):
    port = free_port()
    env = {k: v for k, v in os.environ.items()
           if k not in ("PSX_GEOMETRY_CORRECTION", "PSX_PERSPECTIVE_TEXTURING",
                        "PSX_PGXP_CPU_MODE", "PSX_PGXP_CULLING")}
    env["PSX_HEADLESS"] = "1"
    saves = root / ("saves-" + label)
    saves.mkdir()
    log = open(root / ("run-" + label + ".log"), "w")
    proc = subprocess.Popen(
        [str(root / "r4-runtime"), "--game", str(root / "game.toml"),
         "--disc", str(disc), "--no-launcher", "--headless",
         "--memcard-dir", str(saves), "--debug-port", str(port)],
        cwd=root, stdout=log, stderr=subprocess.STDOUT, env=env)
    try:
        deadline = time.time() + 120
        state = None
        while time.time() < deadline and proc.poll() is None:
            try:
                f = ask(port, {"cmd": "frame"}).get("frame", 0)
                if f >= frames:
                    state = ask(port, {"cmd": "geom_correction"})
                    misses = ask(port, {"cmd": "dispatch_stats"})
                    state["misses"] = [misses.get("miss_total"),
                                       misses.get("segment_miss_total")]
                    break
            except OSError:
                pass
            time.sleep(0.25)
        try:
            ask(port, {"cmd": "quit"}, timeout=3)
        except OSError:
            pass
        return state
    finally:
        try:
            proc.wait(timeout=15)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
        log.close()


def expect(state, label, geometry, texture, culling):
    check(state is not None, "%s: booted and answered" % label)
    if not state:
        return
    pg = state.get("pgxp", {})
    got = (state.get("geometry_correction"), state.get("texture_correction"),
           pg.get("culling"))
    check(got == (geometry, texture, culling),
          "%s: geometry/texture/culling = %s (want %s)"
          % (label, got, (geometry, texture, culling)))
    check(pg.get("tolerance", 0) < 0 and pg.get("position_fallback") == 0 and
          pg.get("preserve_projection") == 1,
          "%s: R4's [video] tuning keys applied" % label)
    check(state.get("misses") == [0, 0],
          "%s: 0 dispatch / 0 segment misses (%s)" % (label, state.get("misses")))


def main():
    global build_root
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", type=Path, required=True)
    ap.add_argument("--disc", type=Path, required=True)
    ap.add_argument("--frames", type=int, default=120)
    a = ap.parse_args()
    build_root = a.build_dir.resolve()
    if not (build_root / "r4-runtime").exists():
        print("FAIL no r4-runtime in %s" % build_root)
        return 1
    with tempfile.TemporaryDirectory(prefix="r4-pgxp-boot-") as tmp:
        root = Path(tmp)
        stage(build_root, root)
        expect(boot(root, a.disc.resolve(), a.frames, "default"),
               "shipped default", 1, 1, 1)
        write_state(root, False)
        expect(boot(root, a.disc.resolve(), a.frames, "off"),
               "PGXP off on the Mods page", 0, 0, 0)
        write_state(root, True, {"culling": "false"})
        expect(boot(root, a.disc.resolve(), a.frames, "noculling"),
               "PGXP on, culling off", 1, 1, 0)
    if failures:
        print("test_r4_pgxp_boot: %d FAILURES" % failures)
        return 1
    print("test_r4_pgxp_boot: all checks passed")
    return 0


build_root = None

if __name__ == "__main__":
    sys.exit(main())
