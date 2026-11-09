#!/usr/bin/env python3
"""Verify native overlay execution in an isolated hidden Mac boot-to-race run."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from dbg import cmd
from check_ingame_menu import check_menu

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path, help='new isolated directory; never reused')
    parser.add_argument('--build', type=Path, default=ROOT / 'build')
    parser.add_argument('--disc', type=Path, default=ROOT / 'disc/R4 - Ridge Racer Type 4 (USA).cue')
    parser.add_argument('--port', type=int, default=4798)
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
    (stage / 'settings.toml').write_text('[video]\npresent_thread = false\n')
    launch = [str(stage / 'r4-runtime'), '--game', str(stage / 'game.toml'),
              '--disc', str(args.disc.resolve()), '--no-launcher', '--hidden-window',
              '--debug-port', str(args.port), '--memcard-dir', str(stage / 'memcards')]
    with (stage / 'runtime.log').open('w') as log:
        process = subprocess.Popen(launch, cwd=stage, env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
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


if __name__ == '__main__':
    main()
