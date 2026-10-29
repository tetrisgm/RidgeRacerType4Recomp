#!/usr/bin/env python3
"""test_r4_defaults.py - R4's shipped defaults and their off-switches.

R4 turns its enhancements on by default (README "On by default"):
  - r4.enhancement.widescreen: on, View = Fit to Window;
  - game.toml [video] internal_resolution = Match display, at the display's
    resolution, capped at 4K/9x (match_display_max_lines = 2160; owner 2026-10-28),
    supersample 1.5 and no post AA filter, with dynamic resolution on and
    floored at 1080p (owner, 2026-10-22; Ultra since 2026-10-08: Ultra by default, the dynamic
    systems are the safety net; "display" or another preset
    is accepted too) so the default holds full speed; the Display row turns
    it off;
  - game.toml [video] fullscreen = borderless (desktop) by default;
  - game.toml [video] render_thread and present_thread on
    (PSX_RENDER_THREAD=0, PSX_PRESENT_THREAD=0 turn them off for a run);
    Smooth motion (frame_generation) off in [video] and every preset, and
    its launcher row hidden (removed from R4, owner 2026-10-20);
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

    fr = manifest('r4.enhancement.frame-rate')
    check(feature(fr, 'frame-rate').get('default_enabled') is True,
          'frame rate: on by default (phase 5 owner defaults)')
    check(option_default(fr, 'frame-rate', 'rate') == '60',
          'frame rate: 60 FPS by default')
    check(set(option_values(fr, 'frame-rate', 'rate')) == {'auto', '30', '60', '90', '100', '120', '144', 'display'},
          'frame rate: auto, fixed rates, display refresh and original 30 FPS')
    plugin = open(os.path.join(ROOT, 'src', 'mods', 'r4_frame_rate_plugin.c'), encoding='utf-8').read()
    check('R4_FR_AUTO_ORIGINAL_SCALE 18u' in plugin and 'psx_mod_internal_scale()' in plugin,
          'frame rate: Auto resolves from the internal scale, original frames from 18x')
    check(not any(o['id'] in ('method', 'blend') for o in fr.get('option', [])),
          'frame rate: no crossfade or image-warp option')

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
    # Owner, 2026-10-21: cap Match display at 8K/18x and drop SSAA on 4K/8K
    # displays (27x on an 8K panel forced the high-resolution-window path).
    # Owner playtest, 2026-10-28: 18x on an 8K panel ran ~21 FPS; cap at 4K/9x.
    check(video.get('match_display_max_lines') == 2160,
          'game.toml: Match display capped at 4K (match_display_max_lines = 2160)')
    check(video.get('supersample_disable_at_lines') == 2160,
          'game.toml: no supersampling from 4K displays up (supersample_disable_at_lines = 2160)')
    check(video.get('dynamic_resolution') is False,
          'game.toml: dynamic resolution off (owner, 2026-10-27)')
    floor = video.get('dynamic_resolution_min')
    check(floor in ('display', 'native') or (isinstance(floor, str) and floor.endswith('p'))
          or (isinstance(floor, int) and floor >= 240),
          'game.toml: dynamic resolution has a floor ("native", "display" or a preset)')
    check(floor == '1080p',
          'game.toml: dynamic resolution never drops below 1080p (owner, 2026-10-22)')
    check(video.get('dynamic_resolution_start_min') is False,
          'game.toml: no floor start while dynamic resolution is off')
    check(video.get('supersample') == 1.5, 'game.toml: supersample = 1.5')
    check(video.get('antialiasing_mode') == 'off',
          'game.toml: no post-process AA filter (antialiasing_mode = "off")')
    check(video.get('rewind') is True, 'game.toml: per-frame native rewind on by default')
    pg = manifest('psx.enhancement.pgxp')
    check(option_default(pg, 'pgxp', 'cpu_mode') == 'true',
          'PGXP: CPU-mode propagation on by default (owner 2026-10-27)')
    ce = manifest('r4.enhancement.car-effects')
    check(feature(ce, 'car-effects').get('default_enabled') is True,
          'car effects: on by default (owner 2026-10-27)')
    check(video.get('fmv_filter') == 'bicubic' and video.get('antialiasing', True) is True,
          'game.toml: FMV upscaled bicubic (owner 2026-10-27: nearest looked blocky)')
    check('dynamic_resolution_priority' not in video,
          'game.toml: no dynamic_resolution_priority (not in the render-thread stack)')
    check(video.get('fullscreen') == 'borderless',
          'game.toml: borderless (desktop) fullscreen by default')
    for k in ('render_thread', 'present_thread'):
        check(video.get(k) is True, f'game.toml: {k} on by default')
    check(video.get('present_thread_slots') == 8,
          'game.toml: eight bounded presenter slots for 60 Hz output')
    check(video.get('frame_generation') is False,
          'game.toml: Smooth motion (frame_generation) off: removed from R4')
    check(video.get('pgxp_depth_buffer') is True and
          video.get('pgxp_color_correction') is True and
          video.get('pgxp_seam') == 'fine',
          'game.toml: PGXP depth buffer, colour correction, fine seams')
    timing = g.get('timing', {})
    check(timing.get('guest_cycle_scale') == 2 and
          timing.get('guest_cycle_scale_gated') is True and
          'guest_cycle_scale_gate' in timing,
          'game.toml: guest_cycle_scale 2, gated to races with Max Detail')
    check(timing.get('guest_cycle_scale_batch') is True,
          'game.toml: the scaled CPU charge is batched (guest_cycle_scale_batch)')
    check(video.get('aspect_ratio') == '4:3',
          'game.toml: the base aspect stays 4:3 (widescreen is the mod)')
    check(video.get('renderer') == 'opengl',
          'game.toml: OpenGL renderer (render thread and native-wide need it)')


def test_idle_skip():
    with open(os.path.join(ROOT, 'game.toml'), 'rb') as fh:
        runtime = tomllib.load(fh).get('runtime', {})
    check(runtime.get('idle_skip') is True and
          runtime.get('idle_skip_store_counters') is True,
          'game.toml: idle_skip with store-counter loops (PsyQ v_wait)')
    check(runtime.get('idle_skip_store_counter_sites') == ['0x8008B4D0', '0x8008B518'],
          'game.toml: store-counter skips only at the audited v_wait site')


def test_quality_presets():
    with open(os.path.join(ROOT, 'game.toml'), 'rb') as fh:
        cfg = tomllib.load(fh)
    video, q = cfg.get('video', {}), cfg.get('quality', {})
    check(sorted(q) == ['high', 'low', 'medium', 'ultra'],
          'game.toml: Low, Medium, High and Ultra graphics presets')
    check(q.get('ultra') == {'frame_generation': False}, 'game.toml: Ultra is the shipped [video] block')
    low = q.get('low', {})
    check(low.get('supersample') == 1.0 and low.get('dynamic_resolution_min') == '1080p',
          'game.toml: Low drops supersampling, floor Native')
    check(all(q[n].get('frame_generation') is False for n in q),
          'game.toml: every preset keeps Smooth motion off')
    surface = open(os.path.join(ROOT, 'launcher', 'title_r4.cpp'), encoding='utf-8').read()
    check('{"graphics.frame_generation", Vis::Auto, "0"}' in surface,
          'launcher: the Smooth motion row is hidden and pinned off')
    check(low.get('pgxp_color_correction') is False and low.get('pgxp_seam') == 'off',
          'game.toml: Low turns colour correction and seam expansion off (PGXP itself stays on)')
    check(all('pgxp_depth_buffer' not in q[n] for n in q) and video.get('pgxp_depth_buffer') is True,
          'game.toml: the PGXP depth buffer stays on in every preset (Max Detail + reprojection need depth)')
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
    check(all('dithering' not in p for p in q.values()),
          'game.toml: dithering stays off in every preset')
    check(all('bloom' not in q[n] for n in ('ultra', 'high', 'medium')),
          'game.toml: bloom stays on (1.0) in every preset but Low')
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
    test_idle_skip()
    test_quality_presets()
    test_mod_state()
    if FAILS:
        print(f'{len(FAILS)} failure(s)')
        return 1
    print('all passed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
