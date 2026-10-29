"""Exclusive hidden 8K A/B run. Restores local mod state; never kills other processes.
The existing 8x8 presenter trace measures a lower bound on distinct images.
"""
import argparse,csv,json,os,subprocess,sys,time,ctypes
from ctypes import wintypes
from pathlib import Path
from dbg import cmd

ROOT=Path(__file__).resolve().parents[1]
LOCK=ROOT.parent/'perf.lock'
FLAGS=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0

def runtimes():
    p=subprocess.run(['powershell','-NoProfile','-Command',
        '(Get-Process r4-runtime -ErrorAction SilentlyContinue).Id'],capture_output=True,text=True,creationflags=FLAGS)
    return p.stdout.strip()

def live_runtime_ids():
    # Cheap in-window interference check; admission above deliberately uses Get-Process.
    class Entry(ctypes.Structure):
        _fields_=[('size',wintypes.DWORD),('usage',wintypes.DWORD),('pid',wintypes.DWORD),
                  ('heap',ctypes.c_size_t),('module',wintypes.DWORD),('threads',wintypes.DWORD),
                  ('parent',wintypes.DWORD),('priority',wintypes.LONG),('flags',wintypes.DWORD),
                  ('exe',wintypes.WCHAR*260)]
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.CreateToolhelp32Snapshot.restype=wintypes.HANDLE
    kernel.Process32FirstW.argtypes=[wintypes.HANDLE,ctypes.POINTER(Entry)]
    kernel.Process32NextW.argtypes=[wintypes.HANDLE,ctypes.POINTER(Entry)]
    kernel.CloseHandle.argtypes=[wintypes.HANDLE]
    handle=kernel.CreateToolhelp32Snapshot(2,0)
    if handle==wintypes.HANDLE(-1).value:raise ctypes.WinError(ctypes.get_last_error())
    found=[];entry=Entry();entry.size=ctypes.sizeof(entry)
    try:
        ok=kernel.Process32FirstW(handle,ctypes.byref(entry))
        while ok:
            if entry.exe.lower()=='r4-runtime.exe':found.append(entry.pid)
            ok=kernel.Process32NextW(handle,ctypes.byref(entry))
    finally:kernel.CloseHandle(handle)
    return found

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--port',type=int,default=4822)
    ap.add_argument('--rates',type=int,nargs='+',default=[60,120])
    args=ap.parse_args();out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    while True:
        if not runtimes():
            try: LOCK.mkdir()
            except FileExistsError: pass
            else:
                if not runtimes(): break
                LOCK.rmdir()
        print('Waiting for exclusive GPU measurement lock',flush=True);time.sleep(10)
    (LOCK/'owner.json').write_text(json.dumps({'pid':os.getpid(),'worktree':str(ROOT)}))
    state=ROOT/'build/mods/state.toml';saved=state.read_bytes() if state.exists() else None
    process=None;results=[]
    def ask(name,**kw):return cmd(dict(cmd=name,**kw),port=args.port,timeout=30)
    try:
        for rate in args.rates:
            for enabled in (False,True):
                while runtimes():
                    print('Waiting for other runtime before next A/B arm',flush=True);time.sleep(5)
                name=f'{rate}-'+('B' if enabled else 'A');trace=out/(name+'.csv')
                if saved is None: state.unlink(missing_ok=True)
                else: state.write_bytes(saved)
                subprocess.run([sys.executable,'tools/mod_state.py','build',
                    '--enable' if enabled else '--disable','r4.enhancement.frame-rate/frame-rate',
                    *([f'rate={rate}'] if enabled else [])],cwd=ROOT,check=True,capture_output=True)
                env=dict(os.environ,PSX_HOST_SAMPLER_CALLERS='1',PSX_PACKET_INTERPOLATION_AUDIT='0',
                         PSX_PRESENT_TRACE=str(trace),PSX_PRESENT_IMAGE_RING='0')
                env['PATH']=r'C:\msys64\mingw64\bin;'+env['PATH']
                process=subprocess.Popen([str(ROOT/'build/r4-runtime.exe'),'--game',str(ROOT/'build/game.toml'),
                    '--disc',str(ROOT/'disc/R4 - Ridge Racer Type 4 (USA).cue'),'--no-launcher',
                    '--hidden-window','--debug-port',str(args.port)],cwd=ROOT,env=env,
                    stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=FLAGS)
                deadline=time.monotonic()+90
                while True:
                    if process.poll() is not None: raise RuntimeError('Runtime exited')
                    try:
                        if ask('frame').get('frame',0)>180:break
                    except OSError:pass
                    if time.monotonic()>deadline:raise RuntimeError('Runtime startup timeout')
                    time.sleep(.2)
                ask('savestate',op='load',slot=9)
                deadline=time.monotonic()+30
                while True:
                    status=ask('savestate_status')
                    if status.get('last_ok') and not status.get('pending'):break
                    if time.monotonic()>deadline:raise RuntimeError(status)
                    time.sleep(.2)
                ask('set_input',buttons='0xBFFF');time.sleep(3)
                config={k:ask(k) for k in ['video_info','pgxp','rewind_status','packet_interpolation']}
                ask('main_cadence',reset=1)
                before={k:ask(k) for k in ['frame','audio_stats','host_profile','mod_counters']}
                start=time.perf_counter_ns();print(f'{name}: timing 60 seconds, PID {process.pid}',flush=True)
                interference=[];census=[]
                for second in range(60):
                    if process.poll() is not None:raise RuntimeError('Owned runtime exited during measurement')
                    due=start+(second+1)*1_000_000_000
                    time.sleep(max(0,(due-time.perf_counter_ns())/1e9))
                    others=[pid for pid in live_runtime_ids() if pid!=process.pid]
                    if others:interference.append({'second':second+1,'pids':others})
                    census.append(ask('packet_interpolation')['interpolation'])
                end=time.perf_counter_ns()
                after={k:ask(k) for k in ['frame','audio_stats','packet_interpolation','mod_counters','main_cadence','dispatch_stats']}
                profile=subprocess.run([sys.executable,'psxrecomp/tools/host_profile.py','--port',str(args.port),
                    '--exe','build/r4-runtime.exe','--since',str(before['host_profile']['seq']),
                    '--callers','--top','60','--nm',r'C:\msys64\mingw64\bin\nm.exe'],cwd=ROOT,capture_output=True,text=True,check=True)
                (out/(name+'-profile.txt')).write_text(profile.stdout)
                time.sleep(2) # trace flushes once a second
                rows=list(csv.DictReader(trace.open()))
                rows=[r for r in rows if start<=int(r['time_ns'])<=end]
                duration=(end-start)/1e9
                summary={'name':name,'seconds':duration,'guest_vblanks_s':(after['frame']['frame']-before['frame']['frame'])/duration,
                    'starved_samples':after['audio_stats']['out']['underruns']-before['audio_stats']['out']['underruns'],
                    'distinct_lower_bound_s':sum(int(r['changed']) for r in rows)/duration,'presented_s':len(rows)/duration,
                    'max_interval_ms':after['main_cadence']['max_ms'],'interference':interference,
                    'max_unmatched':max(r['unmatched'] for r in census),'max_storage_misses':max(r.get('storage_misses',0) for r in census),
                    'failed_pairs_delta':after['packet_interpolation']['interpolation'].get('failed_pairs',0)-config['packet_interpolation']['interpolation'].get('failed_pairs',0),
                    'interpolation':after['packet_interpolation']}
                (out/(name+'.json')).write_text(json.dumps(dict(summary=summary,config=config,before=before,after=after,census=census),indent=2))
                results.append(summary);print(json.dumps(summary),flush=True)
                process.terminate();process.wait(timeout=20);process=None
                (out/'summary.json').write_text(json.dumps(results,indent=2))
                if interference:raise RuntimeError('A/B pair rejected: another runtime overlapped; see receipt')
    finally:
        if process is not None and process.poll() is None:process.terminate();process.wait(timeout=20)
        if saved is None:state.unlink(missing_ok=True)
        else:state.write_bytes(saved)
        (LOCK/'owner.json').unlink();LOCK.rmdir()
if __name__=='__main__':main()
