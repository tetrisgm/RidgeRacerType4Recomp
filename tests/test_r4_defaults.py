#!/usr/bin/env python3
"""test_r4_defaults.py - R4's shipped defaults and their off-switches.

R4 turns its enhancements on by default (README "On by default"):
  - r4.enhancement.widescreen: on, View = Fit to Window;
  - r4.enhancement.frame-rate: on, Display refresh, Interpolated;
  - game.toml [video] internal_resolution = Match display, at the display's
    full resolution (no match_display_max_lines cap; owner, 2026-10-01),
    with dynamic resolution on (720p floor, frame-rate priority) so the
    default holds full speed; the Display row turns it off.
The framework defaults stay off for every other title; these are R4's own
manifests and game.toml. This checks them, checks that every R4 package can
resolve in a release install (no loose boot EXE there, so no exe_sha256
target), and checks tools/mod_state.py, which scripted runs use to switch
the defaults off (--stock) and back (--defaults).

Pure Python, no build or disc needed: ctest -R r4_defaults
"""
import os
import shutil
import subprocess
import sys
import tempfile

try:
    import tomllib
except ImportError:  # Python < 3.11
    print('SKIP: needs Python 3.11+ (tomllib)')
    sys.exit(77)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRELOADED = os.path.join(ROOT, 'mods', 'preloaded', 'packages')
FAILS = []


def check(cond, what):
    print(('ok   ' if cond else 'FAIL ') + what)
    if not cond:
        FAILS.append(what)


def manifest(package):
    root = os.path.join(PRELOADED, package)
    version = sorted(os.listdir(root))[-1]
    with open(os.path.join(root, version, 'manifest.toml'), 'rb') as fh:
        return tomllib.load(fh)


def feature(m, fid):
    return next((f for f in m.get('feature', []) if f['id'] == fid), None)


def option_default(m, fid, oid):
    o = next((o for o in m.get('option', [])
              if o['feature'] == fid and o['id'] == oid), None)
    return o and o.get('default')


def option_values(m, fid, oid):
    o = next((o for o in m.get('option', [])
              if o['feature'] == fid and o['id'] == oid), None)
    return [c['value'] for c in (o or {}).get('choice', [])]


def test_manifests():
    ws = manifest('r4.enhancement.widescreen')
    f = feature(ws, 'widescreen')
    check(f is not None and f.get('default_enabled') is True,
          'widescreen: feature on by default')
    check(option_default(ws, 'widescreen', 'aspect') == 'Fit',
          'widescreen: View defaults to Fit to Window')
    check('Fit' in option_values(ws, 'widescreen', 'aspect'),
          'widescreen: Fit is a declared choice')

    fr = manifest('r4.enhancement.frame-rate')
    f = feature(fr, 'frame-rate')
    check(f is not None and f.get('default_enabled') is True,
          'frame rate: feature on by default')
    check(option_default(fr, 'frame-rate', 'rate') == 'display',
          'frame rate: Rate defaults to Display refresh')
    check(option_default(fr, 'frame-rate', 'method') == 'interpolate',
          'frame rate: Method defaults to Interpolated')
    for v in ('display', 'interpolate'):
        check(v in option_values(fr, 'frame-rate', 'rate') +
              option_values(fr, 'frame-rate', 'method'),
              f'frame rate: {v} is a declared choice')

    # A release zip carries no loose boot EXE, so psxrecomp hashes nothing
    # and an exe_sha256 target never matches: a default-on package with one
    # would refuse to launch the game. game_id targets only.
    for package in sorted(os.listdir(PRELOADED)):
        if not os.path.isdir(os.path.join(PRELOADED, package)):
            continue
        m = manifest(package)
        for t in m.get('target', []):
            check(t.get('game_id') == 'SLUS-00797' and 'exe_sha256' not in t,
                  f'{package}: target is game_id SLUS-00797, no exe_sha256')
        for f in m.get('feature', []):
            check(f.get('channel', m.get('channel', 'stable')) != 'developer'
                  or not f.get('default_enabled'),
                  f'{package}/{f["id"]}: a default-on feature ships (not developer)')


def test_game_toml():
    with open(os.path.join(ROOT, 'game.toml'), 'rb') as fh:
        g = tomllib.load(fh)
    video = g.get('video', {})
    check(video.get('internal_resolution') == 'display',
          'game.toml: internal_resolution = "display" (Match display)')
    check('match_display_max_lines' not in video,
          'game.toml: Match display is not capped (full display resolution)')
    check(video.get('dynamic_resolution') is True,
          'game.toml: dynamic resolution on by default')
    check(video.get('dynamic_resolution_min') == '720p',
          'game.toml: dynamic resolution never below 720p')
    check(video.get('dynamic_resolution_priority') == 'frame_rate',
          'game.toml: dynamic resolution priority = frame_rate')
    check(video.get('aspect_ratio') == '4:3',
          'game.toml: the base aspect stays 4:3 (widescreen is the mod)')
    check(video.get('renderer') == 'opengl',
          'game.toml: OpenGL renderer (frame rate and native-wide need it)')


def test_mod_state():
    tool = os.path.join(ROOT, 'tools', 'mod_state.py')
    with tempfile.TemporaryDirectory() as build:
        shutil.copytree(PRELOADED, os.path.join(build, 'mods', 'bundled'),
                        ignore=shutil.ignore_patterns('.gitkeep'))
        state = os.path.join(build, 'mods', 'state.toml')

        def run(*args):
            return subprocess.run([sys.executable, tool, build, *args],
                                  capture_output=True, text=True)

        def load():
            with open(state, 'rb') as fh:
                s = tomllib.load(fh)
            return {(f['package_id'], f['id']): f for f in s.get('feature', [])}

        r = run('--stock')
        check(r.returncode == 0, '--stock runs')
        feats = load()
        expected = set()
        for package in os.listdir(os.path.join(build, 'mods', 'bundled')):
            m = manifest(package)
            expected |= {(package, f['id']) for f in m.get('feature', [])}
        check(set(feats) == expected and
              all(f['enabled'] is False for f in feats.values()),
              '--stock: every bundled feature explicitly off')

        r = run('--stock', '--enable', 'r4.enhancement.widescreen/widescreen',
                'aspect=21:9')
        feats = load()
        ws = feats.get(('r4.enhancement.widescreen', 'widescreen'), {})
        fr = feats.get(('r4.enhancement.frame-rate', 'frame-rate'), {})
        check(r.returncode == 0 and ws.get('enabled') is True and
              ws.get('values', {}).get('aspect') == '21:9' and
              fr.get('enabled') is False,
              '--stock --enable: one feature on with its option, the rest off')

        r = run('--disable', 'r4.enhancement.frame-rate/frame-rate')
        feats = load()
        check(r.returncode == 0 and list(feats) ==
              [('r4.enhancement.frame-rate', 'frame-rate')] and
              feats[('r4.enhancement.frame-rate', 'frame-rate')]['enabled'] is False,
              '--disable: names only that feature (others keep their default)')

        r = run('--enable', 'r4.enhancement.widescreen/nope')
        check(r.returncode != 0, 'unknown feature is refused')

        r = run('--defaults')
        check(r.returncode == 0 and not os.path.exists(state),
              '--defaults removes state.toml (manifest defaults)')


def main():
    test_manifests()
    test_game_toml()
    test_mod_state()
    if FAILS:
        print(f'{len(FAILS)} failure(s)')
        return 1
    print('all passed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
