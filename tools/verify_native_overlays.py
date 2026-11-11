#!/usr/bin/env python3
"""Verify native overlay execution in an isolated hidden Mac boot-to-race run."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import tomllib

from dbg import cmd
from check_ingame_menu import check_menu

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path, help='new isolated directory; never reused')
    parser.add_argument('--build', type=Path, default=ROOT / 'build')
    parser.add_argument('--disc', type=Path, default=ROOT / 'disc/R4 - Ridge Racer Type 4 (USA).cue')
    parser.add_argument('--port', type=int, default=4798)
    parser.add_argument('--menu-script', type=Path, help='exercise the custom boot menu before smoke')
    args = parser.parse_args()
    stage = args.output.resolve()
    stage.mkdir(parents=True, exist_ok=False)
    for name in ('r4-runtime', 'game.toml', 'game_options.toml', 'bios', 'assets', 'postfx'):
        src = args.build.resolve() / name
        if src.is_dir():
            shutil.copytree(src, stage / name)
        elif src.is_file():
            shutil.copy2(src, stage / name)
    shutil.copytree(args.build.resolve() / 'mods/bundled', stage / 'mods/bundled')
    env = {k: v for k, v in os.environ.items() if not k.startswith('PSX_OVERLAY_')}
    env['SDL_AUDIODRIVER'] = 'dummy'
    # Cocoa may throttle a hidden window's display link to one swap per second.
    # Script waits count rendered frames; isolated automation must not use VSync.
    env['PSX_VSYNC'] = '0'
    if args.menu_script:
        env['R4L_OVERLAY_SCRIPT'] = str(args.menu_script.resolve())
    (stage / 'settings.toml').write_text('[video]\npresent_thread = false\n')
    launch = [str(stage / 'r4-runtime'), '--game', str(stage / 'game.toml'),
              '--disc', str(args.disc.resolve()), '--hidden-window',
              '--debug-port', str(args.port), '--memcard-dir', str(stage / 'memcards')]
    if not args.menu_script:
        launch.append('--no-launcher')
    with (stage / 'runtime.log').open('w') as log:
        process = subprocess.Popen(launch, cwd=stage, env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            if args.menu_script:
                deadline = time.monotonic() + 180
                captured = set()
                while time.monotonic() < deadline:
                    content = (stage / 'runtime.log').read_text(errors='replace')
                    if process.poll() is not None:
                        raise RuntimeError('Runtime exited during menu checks')
                    for n in range(1, 6):
                        marker = f'[r4l-script] sleep 3.0{n}'
                        if marker in content and n not in captured:
                            captured.add(n)
                            cmd({'cmd': 'present_shot', 'path': str(stage / f'menu-{n}.png')}, port=args.port)
                            (stage / f'menu-{n}.json').write_text(json.dumps(cmd({'cmd': 'menu_host'}, port=args.port), indent=2))
                    if 'overlay script done' in content:
                        if '[r4l-script] FAIL' in content or 'overlay script done, failures=0' not in content:
                            raise RuntimeError('Custom menu script failed; inspect runtime.log')
                        break
                    time.sleep(.15)
                else:
                    raise RuntimeError('Custom menu script timed out')
            if args.menu_script:
                controls = json.loads((stage / 'menu-3.json').read_text())
                volume = json.loads((stage / 'menu-5.json').read_text())
                settings = tomllib.loads((stage / 'settings.toml').read_text())
                assert controls['p2_device'] == 1 and controls['p1_deadzone'] == 8191, controls
                assert 0 < volume['volume'] < 100, volume
                assert settings['audio']['volume'] == volume['volume'], settings
                assert settings['controller']['p2_device'] == 'keyboard', settings
                assert settings['video']['present_thread'] is False, settings
                assert 'cross = K' in (stage / 'keybinds.ini').read_text()
                assert 'cross = x' in (stage / 'input.ini').read_text()
            with (stage / 'smoke.log').open('w') as log:
                subprocess.run([sys.executable, str(ROOT / 'tools/smoke.py'),
                                str(stage / 'shots'), '--port', str(args.port)],
                               cwd=stage, env=env, stdout=log, stderr=subprocess.STDOUT,
                               timeout=240, check=True)
            receipt = {name: cmd({'cmd': name}, port=args.port)
                       for name in ('frame', 'overlay_loader_status', 'dispatch_stats', 'dirty_ram_stats')}
            receipt['overlay_fallback'] = cmd({'cmd': 'dirty_ram_stats', 'lo': '0x801149a8',
                                                'hi': '0x801281a8'}, port=args.port)
            (stage / 'native-overlay-check.json').write_text(json.dumps(receipt, indent=2) + '\n')
            overlay = receipt['overlay_loader_status']
            stats = receipt['dispatch_stats']
            if receipt['overlay_fallback'].get('per_pc_matching', -1) != 0:
                raise RuntimeError('Interpreted R4 overlay PCs observed; inspect receipt')
            if overlay.get('static_hits', 0) <= 0:
                raise RuntimeError('No linked native overlay calls observed')
            if any(stats.get(key, -1) != 0 for key in ('miss_total', 'miss_unique', 'segment_miss_total')):
                raise RuntimeError(f'Dispatch misses: {stats}')
            if any(stage.rglob('*.dylib')) or any(stage.rglob('*.so')) or (stage / 'overlay_toolchain').exists():
                raise RuntimeError('Unexpected external overlay library/toolchain in isolated run')
            check_menu(stage / 'menu', args.port)
            print(json.dumps({'static_hits': overlay['static_hits'], 'dispatch_misses': 0,
                              'receipt': str(stage / 'native-overlay-check.json')}))
        finally:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()

    if args.menu_script:
        env.pop('R4L_OVERLAY_SCRIPT', None)
        with (stage / 'restart.log').open('w') as log:
            process = subprocess.Popen(launch + ['--no-launcher'], cwd=stage, env=env,
                                       stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 45
                while time.monotonic() < deadline:
                    try:
                        restored = cmd({'cmd': 'menu_host'}, port=args.port)
                        if restored.get('attached'):
                            break
                    except (OSError, RuntimeError):
                        pass
                    if process.poll() is not None:
                        raise RuntimeError('Isolated persistence restart exited')
                    time.sleep(.2)
                else:
                    raise RuntimeError('Isolated persistence restart timed out')
                assert restored['volume'] == volume['volume'], restored
                assert restored['p2_device'] == 1 and restored['p1_deadzone'] == 8191, restored
                assert restored['internal_resolution'] == -105, restored
                assert restored['p1_cross_scancode'] == 14 and restored['p1_cross_button'] == 2, restored
                (stage / 'restart-check.json').write_text(json.dumps(restored, indent=2) + '\n')
            finally:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()


if __name__ == '__main__':
    main()
