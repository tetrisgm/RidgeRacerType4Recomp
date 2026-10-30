"""Hidden release measurement; OS thread accounting never suspends the guest."""
import argparse, ctypes, hashlib, json, os, re, subprocess, time
from pathlib import Path
import psutil

ROOT = Path(__file__).resolve().parents[1]
ap = argparse.ArgumentParser()
ap.add_argument('--name', required=True)
ap.add_argument('--seconds', type=float, default=120)
ap.add_argument('--bootstrap', action='store_true')
ap.add_argument('--debug-bootstrap', action='store_true', help='Drive the diagnostic binary before measurement; no TCP during measurement')
ap.add_argument('--save-dir', type=Path)
args = ap.parse_args()
out = ROOT / 'analysis/release-waits' / args.name
out.mkdir(parents=True, exist_ok=True)
build = ROOT / 'build'
game = build / 'game.toml'
game.write_text(game.read_text(encoding='utf-8').replace(
    'match_display_max_lines = 2160', 'match_display_max_lines = 4320'), encoding='utf-8')
saves = args.save_dir or out / 'saves'
saves = saves.resolve()
env = dict(os.environ, SDL_AUDIODRIVER='dummy', PSX_FRAME_REPORT_MS='1000',
           PSX_GL_ASYNC_GPU_TIMER='1', PSX_HLE_SCHEDULER='0', PSX_BIOS_HLE='0',
           PSX_PRESENT_IMAGE_RING='0')
env['PATH'] = ('C:/Users/Shokunin/AppData/Local/retcomm/toolchains/cmake-clang-v1/latest/bin;'
               'C:/msys64/mingw64/bin;' + env.get('PATH', ''))
for key in ('PSX_PRESENT_TRACE', 'PSX_PRESENT_SLOT_TRACE', 'PSX_LOAD_SLOT', 'PSX_PAD_SCRIPT'):
    env.pop(key, None)
if args.bootstrap:
    from smoke import STEPS, START_FRAME
    from pad import mask
    rows = ['0 ffff -1']; frame = START_FRAME
    for label, buttons, hold, settle in STEPS:
        if label.startswith('race'): break
        if buttons:
            rows.append(f'{frame} {mask(buttons):04x} -1')
            frame += int(hold * 60)
            rows.append(f'{frame} ffff -1')
        frame += int(settle * 60)
    rows.append(f'{frame} ffff 9')
    script = out / 'pads.txt'; script.write_text('\n'.join(rows)+'\n')
    env['PSX_PAD_SCRIPT'] = str(script)
else:
    env.update(PSX_LOAD_SLOT='9', PSX_HOLD_PADS='bfff')
if args.debug_bootstrap:
    env.pop('PSX_LOAD_SLOT',None)
    env.pop('PSX_HOLD_PADS',None)  # old binary applies this after its debug pad override
    env.update(PSX_HOST_SAMPLER='1',PSX_RUNTIME_PERF_DIAG='1')

k32 = ctypes.WinDLL('kernel32', use_last_error=True)
k32.SetErrorMode(0x8003)
k32.OpenThread.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32]
k32.OpenThread.restype = ctypes.c_void_p
k32.GetThreadDescription.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
k32.CloseHandle.argtypes = [ctypes.c_void_p]
k32.LocalFree.argtypes = [ctypes.c_void_p]
def thread_name(tid):
    h = k32.OpenThread(0x0800, 0, tid)
    if not h: return ''
    p = ctypes.c_void_p()
    try:
        if k32.GetThreadDescription(h, ctypes.byref(p)) >= 0 and p.value:
            result = ctypes.wstring_at(p.value); k32.LocalFree(p); return result
        return ''
    finally: k32.CloseHandle(h)

exe = build/'r4-runtime.exe'
(out/'provenance.json').write_text(json.dumps(dict(executable=str(exe),
    sha256=hashlib.sha256(exe.read_bytes()).hexdigest(), environment={k:v for k,v in env.items()
    if k.startswith(('PSX_', 'SDL_'))}, seconds=args.seconds), indent=2))
log = (out/'runtime.log').open('wb')
proc = subprocess.Popen([str(exe), '--game', str(build/'game.toml'), '--disc',
    str(ROOT/'disc/R4 - Ridge Racer Type 4 (USA).cue'), '--no-launcher', '--hidden-window',
    '--debug-port', '4908', '--memcard-dir', str(saves)], cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT,
    creationflags=subprocess.CREATE_NO_WINDOW)
print(f'Started owned PID {proc.pid}', flush=True)
p = psutil.Process(proc.pid)
samples = []; names = {}; first = {}; last = {}
start_rows = 0
def reports():
    rows = []
    underruns = 0
    for line in (out/'runtime.log').read_text(errors='replace').splitlines():
        if line.startswith('psxrecomp: frames='):
            rows.append({k:float(v) for k,v in re.findall(r'(\w+)=([0-9.]+)', line)})
            rows[-1].setdefault('audio_underruns', underruns)
        elif rows and line.startswith('psxrecomp: packet_'):
            rows[-1].update({k:float(v) for k,v in re.findall(r'(\w+)=([0-9.]+)', line)})
        elif rows and 'runtime cadence:' in line:
            match=re.search(r'underruns=\+(\d+)',line)
            if match:
                underruns+=int(match[1]); rows[-1]['audio_underruns']=underruns
    return rows
try:
    if args.debug_bootstrap:
        from dbg import cmd
        from smoke import STEPS, START_FRAME
        from pad import mask
        from draw_iso_gate import savestate
        def ask(command, **kwargs): return cmd(dict(cmd=command,**kwargs),port=4908,timeout=20)
        def frame(): return ask('frame')['frame']
        def phase(): return int.from_bytes(bytes.fromhex(ask('read_ram',addr='0x800ff860',len=4)['hex']),'little')
        until=time.monotonic()+90
        while True:
            try: ask('set_input',buttons='0xffff');ask('turbo',enabled=1);break
            except OSError:
                if time.monotonic()>until: raise
                time.sleep(.2)
        def advance(count, watch=False):
            target=frame()+count; deadline=time.monotonic()+120
            while frame()<target:
                if watch and phase() in (1,2):return True
                if time.monotonic()>deadline:raise RuntimeError('Bootstrap stalled')
                time.sleep(.03)
            return False
        advance(max(0,START_FRAME-frame()))
        found=False
        for label,buttons,hold,settle in STEPS:
            if label.startswith('race'):break
            watch=label.startswith(('gp-','pre-'))
            if buttons:
                ask('set_input',buttons=hex(mask(buttons)));found=advance(int(hold*60),watch)
                ask('set_input',buttons='0xffff')
            if not found:found=advance(int(settle*60),watch)
            if label=='menu':
                ask('turbo',enabled=0);advance(30);ask('turbo',enabled=1)
            if found:break
        if not found:raise RuntimeError('Diagnostic grid not reached')
        ask('turbo',enabled=0);ask('set_input',buttons='0xffff')
        savestate(4908,'save',9);ask('set_input',buttons='0xbfff')
        print('Diagnostic grid saved; ending TCP control before measurement',flush=True)
    time.sleep(10)
    if not args.bootstrap and not args.debug_bootstrap:
        startup = (out/'runtime.log').read_text(errors='replace')
        if 'PSX_LOAD_SLOT=9 restored' not in startup:
            raise RuntimeError('Startup restore was not verified; rejecting cold-boot measurement')
    begin = time.monotonic(); psutil.cpu_percent()
    start_rows = len(reports())
    while time.monotonic()-begin < args.seconds and proc.poll() is None:
        now = time.monotonic()
        threads = p.threads()
        for t in threads:
            first.setdefault(t.id, (now, t.user_time+t.system_time))
            last[t.id] = (now, t.user_time+t.system_time)
            if t.id not in names: names[t.id] = thread_name(t.id)
        gpu = subprocess.run(['nvidia-smi', '--query-gpu=utilization.gpu,utilization.memory,memory.used',
            '--format=csv,noheader,nounits'], capture_output=True, text=True,
            creationflags=subprocess.CREATE_NO_WINDOW).stdout.strip()
        samples.append(dict(time=now, host_cpu=psutil.cpu_percent(), gpu=gpu,
                            threads=[dict(id=t.id, user=t.user_time, kernel=t.system_time) for t in threads]))
        time.sleep(1)
finally:
    if proc.poll() is None: proc.terminate()
    proc.wait(timeout=15); log.close()
    (out/'samples.json').write_text(json.dumps(samples))
    result = []
    for tid, (end, cpu) in last.items():
        start, initial = first[tid]
        result.append(dict(tid=tid, name=names[tid], cpu_percent=100*(cpu-initial)/max(.001,end-start)))
    result.sort(key=lambda r:r['cpu_percent'], reverse=True)
    (out/'threads.json').write_text(json.dumps(result, indent=2))
    rows = reports()
    if start_rows and len(rows)>start_rows:
        a,b=rows[start_rows-1],rows[-1]; seconds=(b['elapsed_ms']-a['elapsed_ms'])/1000
        swaps=sum(r['swaps'] for r in rows[start_rows:])
        summary=dict(report_seconds=seconds, vblanks_per_s=(b['frames']-a['frames'])/seconds,
                     presented_per_s=swaps/seconds, game_cpu_core_percent=sum(r['cpu_percent'] for r in result),
                     host_cpu_percent=sum(s['host_cpu'] for s in samples)/max(1,len(samples)))
        if 'audio_underruns' in b: summary['audio_underruns']=b['audio_underruns']-a['audio_underruns']
        if 'packet_gpu_ns' in b:
            summary['gpu_ms_per_present']=(b['packet_gpu_ns']-a['packet_gpu_ns'])/max(1,swaps)/1e6
            summary['gpu_present_ms_per_present']=(b['packet_present_ns']-a['packet_present_ns'])/max(1,swaps)/1e6
        (out/'summary.json').write_text(json.dumps(summary, indent=2))
        print(json.dumps(summary), flush=True)
    print(json.dumps(result[:15], indent=2), flush=True)
