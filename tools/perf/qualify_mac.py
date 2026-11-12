#!/usr/bin/env python3
"""Measure an isolated, moving 2160-line GP race using the actual Mac binary.

Requires a local grid savestate in --state (slot 0). Reports game speed and
intermediate-frame coverage separately; a swap counter is not a 60 FPS pass.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from dbg import cmd
from crossover_runtime import runtime_command


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--app', type=Path, help='measure the signed app with isolated player data')
    parser.add_argument('--crossover-bottle', help='Windows PE compatibility run on Mac; not native Windows performance qualification')
    parser.add_argument('--state', required=True, type=Path)
    parser.add_argument('--disc', type=Path,
                        default=ROOT / 'disc/R4 - Ridge Racer Type 4 (USA).cue')
    parser.add_argument('--seconds', type=int, default=120)
    parser.add_argument('--port', type=int, default=4799)
    parser.add_argument('--queue-frames', type=int, choices=range(1, 9))
    parser.add_argument('--visible', action='store_true')
    parser.add_argument('--native-display', action=argparse.BooleanOptionalAction, default=True,
                        help='inherit actual display timing; --no-native-display simulates 60 Hz')
    parser.add_argument('--borderless', action='store_true')
    parser.add_argument('--present-thread', action=argparse.BooleanOptionalAction, default=None)
    parser.add_argument('--rewind', action=argparse.BooleanOptionalAction, default=None,
                        help='diagnostic override; normal runs inherit the game preference')
    parser.add_argument('--rewind-interval', type=int, choices=(1, 4, 8, 12, 15),
                        help='diagnostic checkpoint frequency override')
    parser.add_argument('--rewind-video', action='store_true', help='measure full-resolution lossless video history')
    parser.add_argument('--rewind-repetitions', type=int, choices=range(1, 6), default=1)
    parser.add_argument('--rewind-hold-seconds', type=float, default=1.0)
    parser.add_argument('--verify-draw', action='store_true', help='expensive exact internal-pixel rewind diagnostic, not performance evidence')
    parser.add_argument('--rewind-commands', action='store_true', help='qualify exact resolution-independent draw-command history')
    parser.add_argument('--verify-rewind', action='store_true',
                        help='after timing, navigate and load an automatic rewind point')
    parser.add_argument('--capture-rewind', action='store_true', help='capture accepted composed rewind frames separately from performance qualification')
    parser.add_argument('--verify-state', action='store_true', help='diagnostic RAM equality at rewind resume; separate from timing qualification')
    parser.add_argument('--full-cost', action='store_true',
                        help='disable redraw shedding to measure full workload capacity')
    args = parser.parse_args()
    if args.app and args.crossover_bottle:
        parser.error('--app and --crossover-bottle are mutually exclusive')
    executable_name = 'r4-runtime.exe' if args.crossover_bottle else 'r4-runtime'
    if args.app:
        subprocess.run(['codesign', '--verify', '--deep', '--strict', str(args.app.resolve())], check=True)
    if args.seconds < 30 or args.seconds % 5:
        parser.error('--seconds must be a multiple of five, at least 30')
    stage = args.output.resolve()
    stage.mkdir(parents=True, exist_ok=False)
    build = args.build.resolve()
    if not args.app:
        for name in (executable_name, 'r4-runtime.execution.json', 'game.toml',
                     'game_options.toml', 'assets', 'bios', 'postfx'):
            source = build / name
            if source.is_dir():
                shutil.copytree(source, stage / name)
            elif source.is_file():
                shutil.copy2(source, stage / name)
        shutil.copytree(build / 'mods/bundled', stage / 'mods/bundled')
    shutil.copytree(args.state, stage / 'memcards')
    (stage / 'mods').mkdir(exist_ok=True)
    (stage / 'mods/state.toml').write_text('''format_version = 2
[[package]]
id = "psx.enhancement.internal-resolution"
[[feature]]
package_id = "psx.enhancement.internal-resolution"
id = "internal-resolution"
enabled = true
[feature.values]
resolution = "9x"
[[package]]
id = "r4.enhancement.frame-rate"
[[feature]]
package_id = "r4.enhancement.frame-rate"
id = "frame-rate"
enabled = true
[feature.values]
rate = "60"
''')
    (stage / 'settings.toml').write_text('''[video]
renderer = "opengl"
fullscreen = 0
render_thread = true
frame_generation = false
''')
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(('PSX_', 'R4L_'))}
    env.update(SDL_AUDIODRIVER='dummy', R4L_BOOT_MENU_DONE='1',
               PSX_DEBUG_SERVER='1', PSX_VSYNC='0', PSX_DYNRES='0',
               PSX_HOST_REFRESH_HZ='60', PSX_HOST_REFRESH_MAX_HZ='60')
    if args.borderless:
        settings = stage / 'settings.toml'
        settings.write_text(settings.read_text().replace('fullscreen = 0', 'fullscreen = 1'))
    if args.native_display:
        for key in ('PSX_HOST_REFRESH_HZ', 'PSX_HOST_REFRESH_MAX_HZ', 'PSX_VSYNC'):
            env.pop(key, None)
    if args.present_thread is not None:
        env['PSX_PRESENT_THREAD'] = str(int(args.present_thread))
    if args.rewind is not None:
        env['PSX_REWIND'] = str(int(args.rewind))
    if args.rewind_interval is not None:
        env['PSX_REWIND_INTERVAL'] = str(args.rewind_interval)
    if args.rewind_video:
        env['PSX_REWIND_VIDEO'] = '1'
    if args.rewind_commands:
        env['PSX_REWIND_COMMANDS'] = '1'
    if args.verify_draw:
        env['PSX_REWIND_VERIFY_DRAW'] = '1'
    if args.verify_state:
        env['PSX_REWIND_VERIFY_STATE'] = '1'
    if args.full_cost:
        env['PSX_RENDER_PASS_SHED'] = '0'
    if args.queue_frames:
        env['PSX_RENDER_THREAD_FRAMES'] = str(args.queue_frames)

    executable = args.app.resolve() / 'Contents/MacOS/r4-runtime' if args.app else stage / executable_name
    if args.app:
        env['PSX_BUNDLE_DATA_DIR'] = str(stage)

    def call(command, **kwargs):
        result = cmd(dict(cmd=command, **kwargs), port=args.port, timeout=15)
        if not result.get('ok', True):
            raise RuntimeError(result)
        return result

    def wait(predicate, seconds=60, interval=.1):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            if runtime.poll() is not None:
                raise RuntimeError('Runtime exited; inspect runtime.log')
            try:
                if predicate():
                    return
            except OSError:
                pass
            time.sleep(interval)
        raise RuntimeError('Timed out waiting for a live, moving race')

    def camera():
        return [int(v, 16) for v in call('mem_words', addr='0x1F800008', count=3)['words']]

    launch = [str(executable), '--game', str(stage / 'game.toml'),
              '--disc', str(args.disc.resolve()), '--no-launcher',
              '--debug-port', str(args.port), '--memcard-dir', str(stage / 'memcards')]
    if not args.visible:
        launch.append('--hidden-window')
    launch = runtime_command(executable, launch[1:], stage, args.crossover_bottle)
    if args.crossover_bottle:
        env['WINEDEBUG'] = '-all'
    driver = None
    with (stage / 'runtime.log').open('w') as log, (stage / 'driver.log').open('w') as driver_log:
        runtime = subprocess.Popen(launch, cwd=stage, env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            wait(lambda: call('frame')['frame'] > 15)
            call('turbo', enabled=0)
            generation = call('savestate_status')['generation']
            call('savestate', op='load', slot=0)
            wait(lambda: call('savestate_status')['generation'] != generation)
            loaded = call('savestate_status')
            assert loaded['last_op'] == 'load' and loaded['last_ok'], loaded
            time.sleep(1)
            call('display_aspect', num=16, den=9)
            if args.capture_rewind:
                call('window_size', w=3840, h=2160)
            origin = camera()
            call('set_input', buttons='0xBFFF')
            wait(lambda: sum(abs(a-b) for a, b in zip(camera(), origin)) > 1024)
            driver = subprocess.Popen([sys.executable, '-u', str(ROOT / 'tools/perf/autopilot.py'),
                                       str(args.seconds + 60)], env=dict(env, R4_DEBUG_PORT=str(args.port)),
                                      stdout=driver_log, stderr=subprocess.STDOUT)
            time.sleep(3)
            video = call('video_info')
            assert video['dynamic'] == 0 and video['internal_lines'] == 2160, video
            pgxp = call('pgxp')
            assert pgxp['enabled'] and pgxp['active'] and pgxp['culling'], pgxp
            before = call('render_pass_stats')
            frame = call('frame')['frame']
            start = time.monotonic()
            samples = []
            for _ in range(args.seconds // 5):
                time.sleep(5)
                if driver.poll() is not None:
                    raise RuntimeError(f'Driver exited before measurement completed (exit {driver.returncode}, runtime {runtime.poll()})')
                samples.append(dict(camera=camera(), frame=call('frame'),
                                    perf=call('perf', window_ms=5000)))
                (stage / 'progress.json').write_text(json.dumps(samples, indent=2))
            elapsed = time.monotonic() - start
            end_frame = call('frame')['frame']
            speed = (end_frame - frame) / elapsed
            after = call('render_pass_stats')
            dispatch = call('dispatch_stats')
            assert dispatch['miss_total'] == dispatch['segment_miss_total'] == 0, dispatch
            assert len({tuple(s['camera']) for s in samples}) >= len(samples) * .8, 'Stationary workload'
            result = dict(execution_host='CrossOver on macOS; not native Windows' if args.crossover_bottle else 'native macOS', diagnostics=dict(ram=args.verify_state, pixels=args.verify_draw, captures=args.capture_rewind), native_display=args.native_display, borderless=args.borderless, rewind=args.rewind, rewind_interval=args.rewind_interval, present_thread=args.present_thread, full_cost=args.full_cost, queue_frames=args.queue_frames, frame_start=frame, frame_end=end_frame, elapsed=elapsed, vblanks_per_second=speed, video=video, pgxp=pgxp,
                          pass_start=before, passes=after, dispatch=dispatch, samples=samples,
                          render=call('render_thread'), present=call('gl_present_ring', n=4096),
                          frame_wall=call('frame_wall_ring', n=12000),
                          host_spans=call('host_span_ring', n=16000),
                          rewind_status=call('rewind_status'), pixel_history=call('rewind_video'), display_timing=call('display_timing'),
                          binary_sha256=hashlib.sha256(executable.read_bytes()).hexdigest())
            if args.rewind_video:
                result['rewind_video'] = call('rewind_video')
            (stage / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
            if args.verify_rewind:
                result['rewind_attempts']=[]
                for attempt in range(args.rewind_repetitions):
                    assert result['rewind_status']['enabled'] and result['rewind_status']['snaps'] > 5
                    if driver is not None:
                        driver.terminate(); driver.wait(timeout=10); driver = None
                    call('clear_input')
                    def host(buttons):
                        call('set_input', layer='host', buttons=buttons)
                    host('0xFFFF'); time.sleep(.15)
                    checks_before=call('rewind_status')['resume_checks']
                    call('host_key', name='F8', down=1)
                    wait(lambda: call('rewind_status')['direct'], seconds=10)
                    paused=call('frame')['frame']
                    before_camera=camera()
                    if args.capture_rewind:
                        for shot in range(6):
                            sequence = call('present_shot_seq')['seq']
                            call('present_shot', path=str(stage / f'rewind-{shot}.png'))
                            wait(lambda: call('present_shot_seq')['seq'] != sequence, seconds=10)
                            assert call('present_shot_seq')['wrote']
                            time.sleep(.08)
                    else:
                        rewind_samples = []
                        hold_start = time.monotonic()
                        while time.monotonic() - hold_start < args.rewind_hold_seconds:
                            rewind_samples.append(dict(elapsed=time.monotonic()-hold_start,
                                                       presentation=call('rewind_video')))
                            time.sleep(.02)
                        result['rewind_samples'] = rewind_samples
                        if len(rewind_samples)>1:
                            first,last=rewind_samples[0],rewind_samples[-1]
                            result['rewind_updates_per_second'] = (last['presentation'].get('draw_rendered',0)-first['presentation'].get('draw_rendered',0))/(last['elapsed']-first['elapsed'])
                        (stage / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
                    if call('rewind_status')['state_playback'] != 1:
                        assert call('frame')['frame']==paused, 'Guest advanced behind video playback'
                    assert call('rewind_status')['direct'], 'Direct rewind ended before key release'
                    release_start=time.monotonic()
                    call('host_key', name='F8', down=0)
                    wait(lambda: (lambda status: not status['direct'] and status.get('resume_image_us', 0) != 18446744073709551615)(call('rewind_status')), seconds=10, interval=.005)
                    result['release_observed_seconds']=time.monotonic()-release_start
                    result['release_observation']='key release through first complete forward image; 5 ms polling'
                    if args.verify_draw:
                        verified = call('rewind_video')
                        assert verified['draw_pixel_checks'] > 0 and verified['draw_pixel_mismatches'] == 0, verified
                    result['rewind_frame_wall'] = call('frame_wall_ring', n=4096)
                    history=call('rewind_status')
                    restored=camera()
                    if args.verify_state:
                        assert history['resume_checks']==checks_before+1, history
                        assert history['expected_ram_crc']==history['actual_ram_crc'], history
                        if history['state_playback']==2:
                            assert history['resume_tick']==call('rewind_video')['draw_last_tick'], history
                    assert restored!=before_camera, 'Rewind did not change the race position'
                    resumed_frame=call('frame')['frame']
                    wait(lambda: call('frame')['frame']>resumed_frame+60, seconds=10)
                    assert call('dispatch_stats')['miss_total']==0
                    assert call('rewind_video').get('resume_complete_pending',0)==0, 'Forward complete-image gate remained closed'
                    result['rewind_restore']=dict(passed=True,paused_frame=paused,
                        camera_before=before_camera,camera_restored=restored,
                        history_after=history,presentation_after=call('rewind_video'),
                        resumed_frame=call('frame')['frame'])
                    call('clear_input')
                    result['rewind_attempts'].append(dict(
                        release_observed_seconds=result['release_observed_seconds'],
                        backward_updates_per_second=result.get('rewind_updates_per_second'),
                        restore=result['rewind_restore']))
                    (stage / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
            (stage / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
            print(json.dumps(dict(vblanks_per_second=speed,
                                  intermediate_requested=after['wanted']-before['wanted'],
                                  intermediate_rendered=after['passes']-before['passes']), indent=2))
        finally:
            for process in (driver, runtime):
                if process is not None:
                    process.terminate()
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()

    if args.app:
        subprocess.run(['codesign', '--verify', '--deep', '--strict', str(args.app.resolve())], check=True)


if __name__ == '__main__':
    main()
