#!/usr/bin/env python3
"""test_r4_defaults.py - R4's shipped defaults and their off-switches.

R4 turns its enhancements on by default (README "On by default"):
  - r4.enhancement.widescreen: on, View = Fit to Window;
  - game.toml [video] internal_resolution = Match display, at the display's
    full resolution (no match_display_max_lines cap; owner, 2026-10-01),
    supersample 1.5 and no post AA filter, with dynamic resolution on and
    floored at native x1 (owner, 2026-10-08: Ultra by default, the dynamic
    systems are the safety net; "display" or another preset
    is accepted too) so the default holds full speed; the Display row turns
    it off;
  - game.toml [video] render_thread, present_thread and Smooth motion
    (frame_generation, method reprojection) on (PSX_RENDER_THREAD=0,
    PSX_PRESENT_THREAD=0, PSX_FRAME_GEN=0 turn them off for a run);
  - PGXP extras: depth buffer, colour correction, fine seams (inert while
    the PGXP package is off);
  - [timing] guest_cycle_scale = 2, gated to races with Max Detail.
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

    check(not os.path.exists(os.path.join(PRELOADED, 'r4.enhancement.frame-rate')),
          'frame rate: the old R4 Frame Rate package is gone (Smooth motion replaces it)')

    # A release zip carries no loose boot EXE, so psxrecomp hashes nothing
    # and an exe_sha256 target never matches: a default-on package with one
    # would refuse to launch the game. game_id targets only.
    for package in sorted(os.listdir(PRELOADED)):
        if not os.path.isdir(os.path.join(PRELOADED, package)):
            continue
        m = manifest(package)
        if not any(f.get('default_enabled') for f in m.get('feature', [])):
            continue
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
    floor = video.get('dynamic_resolution_min')
    check(floor in ('display', 'native') or (isinstance(floor, str) and floor.endswith('p'))
          or (isinstance(floor, int) and floor >= 240),
          'game.toml: dynamic resolution has a floor ("native", "display" or a preset)')
    check(floor == 'native',
          'game.toml: Ultra dynamic resolution floor is native x1 (safety net)')
    check(video.get('supersample') == 1.5, 'game.toml: supersample = 1.5')
    check(video.get('antialiasing_mode') == 'off',
          'game.toml: no post-process AA filter (antialiasing_mode = "off")')
    check('dynamic_resolution_priority' not in video,
          'game.toml: no dynamic_resolution_priority (not in the render-thread stack)')
    for k in ('render_thread', 'present_thread'):
        check(video.get(k) is True, f'game.toml: {k} on by default')
    check(video.get('frame_generation') is True,
          'game.toml: Smooth motion (frame_generation) on by default')
    check(video.get('frame_generation_method') == 'reprojection',
          'game.toml: Smooth motion uses reprojection (R4 opts in)')
    check(video.get('pgxp_depth_buffer') is True and
          video.get('pgxp_color_correction') is True and
          video.get('pgxp_seam') == 'fine',
          'game.toml: PGXP depth buffer, colour correction, fine seams')
    timing = g.get('timing', {})
    check(timing.get('guest_cycle_scale') == 2 and
          timing.get('guest_cycle_scale_gated') is True and
          'guest_cycle_scale_gate' in timing,
          'game.toml: guest_cycle_scale 2, gated to races with Max Detail')
    check(video.get('aspect_ratio') == '4:3',
          'game.toml: the base aspect stays 4:3 (widescreen is the mod)')
    check(video.get('renderer') == 'opengl',
          'game.toml: OpenGL renderer (render thread and native-wide need it)')


def test_quality_presets():
    with open(os.path.join(ROOT, 'game.toml'), 'rb') as fh:
        cfg = tomllib.load(fh)
    video, q = cfg.get('video', {}), cfg.get('quality', {})
    check(sorted(q) == ['high', 'low', 'medium', 'ultra'],
          'game.toml: Low, Medium, High and Ultra graphics presets')
    check(q.get('ultra') == {}, 'game.toml: Ultra is the shipped [video] block')
    low = q.get('low', {})
    check(low.get('frame_generation') is False and low.get('supersample') == 1.0 and
          low.get('dynamic_resolution_min') == 'native',
          'game.toml: Low drops Smooth motion and supersampling, floor Native')
    check(low.get('pgxp_depth_buffer') is False and low.get('pgxp_color_correction') is False
          and low.get('pgxp_seam') == 'off',
          'game.toml: Low turns the PGXP extras off (PGXP itself stays on)')
    check(all('pgxp' not in k for n in ('ultra', 'high', 'medium') for k in q[n]),
          'game.toml: Medium and up keep the PGXP extras')
    check(all(p.get('dynamic_resolution', True) is True for p in q.values()),
          'game.toml: dynamic resolution stays on in every preset')
    check(video.get('texture_lod') == 'mipmap' and video.get('anisotropic_filtering') == 16
          and video.get('fmv_chroma_smoothing') is True and video.get('bloom') == 1.0
          and video.get('dithering') == 'off' and video.get('texture_filtering') != 'xbr'
          and 'accurate_blending' not in video,
          'game.toml: Ultra turns on mipmaps + 16x aniso, FMV chroma, bloom 1; dithering off; no xBR/accurate blending')
    check(low.get('texture_lod') == 'off' and low.get('bloom') == 0.0,
          'game.toml: Low turns the new visual features off')
    check(all(isinstance(v, (bool, int, float, str)) for p in q.values() for v in p.values()),
          'game.toml: presets hold plain [video] values')
    ss = [video.get('supersample'), q['high'].get('supersample', video.get('supersample')),
          q['medium'].get('supersample'), low.get('supersample')]
    check(ss == sorted(ss, reverse=True),
          'game.toml: supersample never rises from Ultra down to Low')


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
        rest = [v for k, v in feats.items()
                if k != ('r4.enhancement.widescreen', 'widescreen')]
        check(r.returncode == 0 and ws.get('enabled') is True and
              ws.get('values', {}).get('aspect') == '21:9' and
              all(v.get('enabled') is False for v in rest),
              '--stock --enable: one feature on with its option, the rest off')

        r = run('--disable', 'r4.enhancement.widescreen/widescreen')
        feats = load()
        check(r.returncode == 0 and list(feats) ==
              [('r4.enhancement.widescreen', 'widescreen')] and
              feats[('r4.enhancement.widescreen', 'widescreen')]['enabled'] is False,
              '--disable: names only that feature (others keep their default)')

        r = run('--enable', 'r4.enhancement.widescreen/nope')
        check(r.returncode != 0, 'unknown feature is refused')

        r = run('--defaults')
        check(r.returncode == 0 and not os.path.exists(state),
              '--defaults removes state.toml (manifest defaults)')


def main():
    test_manifests()
    test_game_toml()
    test_quality_presets()
    test_mod_state()
    if FAILS:
        print(f'{len(FAILS)} failure(s)')
        return 1
    print('all passed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
