#!/usr/bin/env python3
"""R4's PGXP defaults (psxrecomp docs/ENHANCEMENTS.md G1.11), no disc needed.

  1. mods/preloaded overrides the framework's builtin psx.enhancement.pgxp at
     the same id and version, with the feature and its precise-culling option
     on by default and the same plugin, feature id and options as the
     builtin, so only those two defaults and the wording differ.
  2. game.toml [video] sets the PGXP tuning keys R4 needs and pgxp_mod_only,
     and does NOT set geometry_correction / perspective_texturing (those are
     not cleared for netplay, so they must never be how R4 turns PGXP on).
  3. With --build-dir: the runtime target was built as the flavor R4_PGXP
     asks for (2 = PGXP hooks, 0 = base), read from the file the framework
     publishes for packagers.

Usage: test_r4_pgxp.py [--build-dir DIR]
"""
import argparse
import re
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PKG = "psx.enhancement.pgxp"
OVERRIDE = ROOT / "mods/preloaded/packages" / PKG / "1.0.0/manifest.toml"
BUILTIN = ROOT / "psxrecomp/mods/builtin/packages" / PKG / "1.0.0/manifest.toml"

failures = 0


def check(ok, what):
    global failures
    print(("ok   " if ok else "FAIL ") + what)
    if not ok:
        failures += 1


def load(path):
    with open(path, "rb") as f:
        return tomllib.load(f)


def strip_text(entries):
    """Entries without their player-facing wording."""
    out = []
    for e in entries:
        out.append({k: v for k, v in e.items()
                    if k not in ("description", "label", "name")})
    return out


def check_manifest():
    check(OVERRIDE.is_file(), "R4 ships %s" % OVERRIDE.relative_to(ROOT))
    check(BUILTIN.is_file(), "the framework builtin exists at the same id/version")
    if failures:
        return
    r4, fw = load(OVERRIDE), load(BUILTIN)
    check(r4.get("id") == fw.get("id") == PKG, "same package id")
    check(r4.get("version") == fw.get("version"),
          "same version (an override, not a second package)")
    check(r4.get("format_version") == fw.get("format_version"), "same format_version")
    check(r4.get("target") == fw.get("target"), "same [[target]]")
    feats = r4.get("feature", [])
    check(len(feats) == 1 and feats[0].get("id") == "pgxp", "one feature, id pgxp")
    if feats:
        check(feats[0].get("default_enabled") is True, "pgxp is on by default for R4")
        check("hidden" not in feats[0] and "channel" not in feats[0],
              "listed on the Mods page like the builtin, so it can be turned off")
    fw_feats = [{k: v for k, v in f.items() if k != "default_enabled"}
                for f in strip_text(fw.get("feature", []))]
    r4_feats = [{k: v for k, v in f.items() if k != "default_enabled"}
                for f in strip_text(feats)]
    check(r4_feats == fw_feats, "feature fields other than wording/default match the builtin")
    def opts(m, skip_culling_default):
        out = []
        for o in strip_text(m.get("option", [])):
            if skip_culling_default and o.get("id") == "culling":
                o = {k: v for k, v in o.items() if k != "default"}
            out.append(o)
        return out
    check(opts(r4, True) == opts(fw, True),
          "options match the builtin (ids, types; defaults except culling)")
    r4_cull = [o for o in r4.get("option", []) if o.get("id") == "culling"]
    fw_cull = [o for o in fw.get("option", []) if o.get("id") == "culling"]
    check(len(r4_cull) == 1 and r4_cull[0].get("default") == "true",
          "precise culling is on by default for R4")
    check(len(fw_cull) == 1 and fw_cull[0].get("default") == "false",
          "the framework builtin keeps precise culling off")
    check(r4.get("plugin") == fw.get("plugin"), "same [[plugin]] (psx.pgxp)")
    check(fw.get("feature", [{}])[0].get("default_enabled") is False,
          "the framework builtin itself stays default off")


def check_game_toml():
    video = load(ROOT / "game.toml").get("video", {})
    check(video.get("pgxp_tolerance") == -1.0, "[video] pgxp_tolerance = -1.0 (no clamp)")
    check(video.get("pgxp_position_fallback") is False,
          "[video] pgxp_position_fallback = false (dataflow only)")
    check(video.get("pgxp_preserve_projection") is True,
          "[video] pgxp_preserve_projection = true")
    check(video.get("pgxp_mod_only") is True,
          "[video] pgxp_mod_only = true (the Mods page is the one switch)")
    check("geometry_correction" not in video and "perspective_texturing" not in video,
          "[video] does not switch PGXP on (netplay would keep it)")


def check_build(build_dir):
    cache = build_dir / "CMakeCache.txt"
    flavor_file = build_dir / "psxrecomp_overlay_flavor-psx-runtime.txt"
    check(cache.is_file(), "build dir has a CMakeCache.txt")
    check(flavor_file.is_file(), "the framework published the runtime flavor")
    if not cache.is_file() or not flavor_file.is_file():
        return
    m = re.search(r"^R4_PGXP:BOOL=(\w+)", cache.read_text(), re.M)
    want = 2 if (m is None or m.group(1).upper() in ("ON", "1", "TRUE", "YES")) else 0
    got = int(flavor_file.read_text().strip())
    check(got == want, "runtime flavor %d matches R4_PGXP (want %d)" % (got, want))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", type=Path)
    a = ap.parse_args()
    check_manifest()
    check_game_toml()
    if a.build_dir:
        check_build(a.build_dir)
    if failures:
        print("test_r4_pgxp: %d FAILURES" % failures)
        return 1
    print("test_r4_pgxp: all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
