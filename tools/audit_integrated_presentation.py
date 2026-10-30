"""Integrated hidden-window audit. Defaults + 60 FPS, PGXP on, private saves.
Run --seconds 20 for each single-variable arm; omit it for a full circuit.
--load-grid uses the local guest-clock baseline grid, otherwise boot a fresh grid.
A runtime/build lock is not proof of a quiet host; inspect background-load receipts.
"""
import argparse,csv,json,os,subprocess,sys,time,struct,statistics,hashlib,psutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from dbg import cmd
from smoke import STEPS,START_FRAME
from pad import mask
from r4_online_battle_regression import Driver,find_course,PHASE,car_state,CAR_SEGMENT
from draw_iso_gate import savestate
from measure_interpolation_ab import runtimes,live_runtime_ids
ap=argparse.ArgumentParser();ap.add_argument('--build',default='build');ap.add_argument('--name',default='baseline');ap.add_argument('--port',type=int,default=4858);ap.add_argument('--profile');ap.add_argument('--seconds',type=float,default=0);ap.add_argument('--disable',default='');ap.add_argument('--no-rewind',action='store_true');ap.add_argument('--load-grid',action='store_true');a=ap.parse_args()
out=ROOT/'analysis/present-guest-clock'/a.name;out.mkdir(parents=True,exist_ok=True)
build=ROOT/a.build
if a.profile:
 subprocess.run([r'C:\Users\Shokunin\AppData\Local\retcomm\toolchains\cmake-clang-v1\latest\bin\cmake.exe','--build',str(build),'--parallel','4','--target','psx-runtime'],check=True)
class Peer:
 def ask(self,i,name,**kw):
  return cmd(dict(cmd=name,**kw),port=a.port,timeout=30)
 def read(self,i,addr,n):return bytes.fromhex(self.ask(i,'read_ram',addr=hex(addr),len=n)['hex'])
 def u32(self,i,addr):return int.from_bytes(self.read(i,addr,4),'little')
 def frame(self):return self.ask(0,'frame')['frame']
p=Peer();ask=lambda name,**kw:p.ask(0,name,**kw)
state=build/'mods/state.toml';saved=state.read_bytes() if state.exists() else None
gamefile=build/'game.toml';saved_game=gamefile.read_bytes()
proc=None
lock=ROOT.parent/'perf.lock'
while True:
 if not runtimes():
  try:lock.mkdir()
  except FileExistsError:pass
  else:
   if not runtimes():break
   lock.rmdir()
 print('Waiting for exclusive measurement slot',flush=True);time.sleep(10)
(lock/'owner.json').write_text(json.dumps(dict(pid=os.getpid(),worktree=str(ROOT))))
try:
 subprocess.run([sys.executable,str(ROOT/'tools/mod_state.py'),str(build),'--defaults'],check=True)
 subprocess.run([sys.executable,str(ROOT/'tools/mod_state.py'),str(build),'--enable','r4.enhancement.frame-rate/frame-rate','rate=60'],check=True)
 if a.no_rewind: gamefile.write_bytes(saved_game.replace(b'rewind = true',b'rewind = false'))
 env=dict(os.environ,SDL_AUDIODRIVER='dummy',PSX_PRESENT_TRACE=str(out/'present.csv'),PSX_PRESENT_SLOT_TRACE=str(out/'slots.csv'),PSX_PRESENT_IMAGE_RING='0',PSX_PACKET_INTERPOLATION_AUDIT='0',PSX_GL_ASYNC_GPU_TIMER='1')
 env['PATH']=r'C:\Users\Shokunin\AppData\Local\retcomm\toolchains\cmake-clang-v1\latest\bin;C:\msys64\mingw64\bin;'+env['PATH']
 if a.profile:env['LLVM_PROFILE_FILE']=str(Path(a.profile).resolve())
 if a.disable: subprocess.run([sys.executable,str(ROOT/'tools/mod_state.py'),str(build),'--disable',a.disable],check=True)
 # A private working directory and memory-card path preserve personal saves.
 (out/'provenance.json').write_text(json.dumps(dict(executable=str(build/'r4-runtime.exe'),sha256=hashlib.sha256((build/'r4-runtime.exe').read_bytes()).hexdigest(),framework=subprocess.check_output(['git','-C',str(ROOT/'psxrecomp'),'rev-parse','HEAD'],text=True).strip(),framework_dirty=bool(subprocess.check_output(['git','-C',str(ROOT/'psxrecomp'),'status','--porcelain'],text=True).strip()),environment={k:env[k] for k in ('SDL_AUDIODRIVER','PSX_HLE_SCHEDULER','PSX_BIOS_HLE','PSX_BIOS_HLE_KEEP_INTRO','PSX_HOST_SAMPLER','PSX_PACKET_HD_CACHE','PSX_INTERPOLATION_REUSE','PSX_SURFACE_REUSE','PSX_AUDIO_ADAPTIVE','PSX_PRESENT_DELAY_MS','PSX_PRESENT_GUEST_CLOCK','PSX_GL_LEGACY_SURFACE_TEST','PSX_RENDER_OP_PROFILE','PSX_GL_ASYNC_GPU_TIMER','PSX_PACKET_INTERPOLATION_AUDIT') if k in env},hidden=True,audio_driver='dummy',pgo_profile=a.profile),indent=2))
 proc=subprocess.Popen([str(build/'r4-runtime.exe'),'--game',str(build/'game.toml'),'--disc',str(ROOT/'disc/R4 - Ridge Racer Type 4 (USA).cue'),'--no-launcher','--hidden-window','--debug-port',str(a.port),'--memcard-dir',str(out/'saves')],cwd=out,env=env,stdout=(out/'runtime.log').open('wb'),stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
 deadline=time.monotonic()+180
 while True:
  if proc.poll() is not None:raise RuntimeError(f'Runtime exited {proc.returncode}')
  try:ask('turbo',enabled=1);break
  except OSError:
   if time.monotonic()>deadline:raise
   time.sleep(.2)
 def wait_frames(n,grid=False):
  goal=p.frame()+n;end=time.monotonic()+180
  while p.frame()<goal:
   if grid and p.u32(0,PHASE) in (1,2):return True
   if proc.poll() is not None or time.monotonic()>end:raise RuntimeError('Guest progress timeout')
   time.sleep(.02)
  return False
 if a.load_grid:
  import shutil
  target=out/'saves/openbios';target.mkdir(parents=True,exist_ok=True)
  for f in (ROOT/'analysis/present-guest-clock/baseline-lap/saves/openbios').glob('*slot09*'):shutil.copy2(f,target/f.name)
  wait_frames(1800);ask('turbo',enabled=0);savestate(a.port,'load',9)
 else:
  wait_frames(max(0,START_FRAME-p.frame()))
  found=False
  for label,buttons,hold,settle in STEPS:
   if label.startswith('race'):break
   if buttons:
    ask('set_input',buttons=hex(mask(buttons)));found=wait_frames(int(hold*60),label.startswith(('gp-','pre-')));ask('clear_input')
   if not found:found=wait_frames(int(settle*60),label.startswith(('gp-','pre-')))
   if label=='menu':
    ask('turbo',enabled=0);wait_frames(30);ask('turbo',enabled=1)
   print(label,p.frame(),p.u32(0,PHASE),flush=True)
   if found:break
  if not found:raise RuntimeError('Fresh grid not reached')
  ask('turbo',enabled=0);ask('set_input',buttons='0xFFFF')
  # Course data is present on the grid; save that fresh state before driving.
  savestate(a.port,'save',9)
 base,pts=find_course(p)
 config={k:ask(k) for k in ['video_info','pgxp','rewind_status','packet_interpolation','mod_counters','dispatch_stats']}
 (out/'config.json').write_text(json.dumps(config,indent=2))
 assert config['dispatch_stats'].get('miss_total',0)==0,config['dispatch_stats']
 print('Fresh grid',len(pts),'segments',json.dumps(config['pgxp']),flush=True)
 ask('screenshot',path=str(out/'grid.png'))
 p.driving=True;driver=Driver(p,0,pts)
 # Countdown is outside the measurement. The car is stationary until green.
 deadline=time.monotonic()+60
 while p.u32(0,PHASE)!=2:
  ask('set_input',buttons='0xFFFF')
  if time.monotonic()>deadline:raise RuntimeError('Countdown timeout')
  time.sleep(.05)
 ask('main_cadence',reset=1)
 before={k:ask(k) for k in ['frame','audio_stats','packet_interpolation','render_thread']}
 start=time.perf_counter_ns();last_sample=0;prev=None;progress=0;rows=[];inputs=[]
 print('LAP START',proc.pid,flush=True)
 while progress<len(pts) and (not a.seconds or (time.perf_counter_ns()-start)/1e9<a.seconds):
  if proc.poll() is not None:raise RuntimeError('Runtime exited during lap')
  seg,speed=driver.step();inputs.append(dict(time_ns=time.perf_counter_ns(),frame=p.frame(),**driver.trace[-1]))
  if prev is not None:
   delta=(prev-seg+len(pts)//2)%len(pts)-len(pts)//2
   if abs(delta)<50:progress+=delta
  prev=seg
  now=time.perf_counter_ns()
  if now-last_sample>=1000000000:
   row=dict(host_cpu_percent=psutil.cpu_percent(),background_media=[dict(pid=x.pid,name=x.info['name']) for x in psutil.process_iter(['name']) if (x.info['name'] or '').lower() in {'ffmpeg.exe','mediamediad.exe'}],time_ns=now,progress=progress,segment=seg,speed=speed,interference=[pid for pid in live_runtime_ids() if pid!=proc.pid])
   row.update({k:ask(k) for k in ['frame','audio_stats','packet_interpolation','render_thread']});rows.append(row)
   with (out/'timeline.jsonl').open('a') as f:f.write(json.dumps(row)+'\n')
   last_sample=now
   if len(rows)%10==0:print('lap',len(rows),'s',progress,'/',len(pts),'speed',speed,flush=True)
  if (now-start)>600000000000:raise RuntimeError('Lap exceeded 600 seconds')
  if p.u32(0,PHASE)!=2:raise RuntimeError('Race ended before completed lap')
  time.sleep(.05)
 end=time.perf_counter_ns()
 after={k:ask(k) for k in ['frame','audio_stats','packet_interpolation','main_cadence','render_thread','dispatch_stats','rewind_status','mod_counters']}
 if env.get('PSX_AUDIT_CPU_PROFILE'):
  (out/'host-profile.json').write_text(json.dumps(ask('host_profile',frame_lo=before['frame']['frame'],frame_hi=after['frame']['frame'],top=32768,callers=1)))
 ask('set_input',buttons='0xFFFF');ask('screenshot',path=str(out/'lap.png'))
 rewind_before=ask('rewind_status',hold=1,verify=1);time.sleep(.5);rewind_held=ask('rewind_status');ask('screenshot',path=str(out/'rewind.png'),packet=1);ask('rewind_status',hold=0);time.sleep(.3)
 (out/'raw.json').write_text(json.dumps(dict(before=before,after=after,start=start,end=end)));(out/'rewind.json').write_text(json.dumps(dict(before=rewind_before,held=rewind_held,after=ask('rewind_status'),pgxp=ask('pgxp')),indent=2))
 ask('quit');exit_code=proc.wait(timeout=30)
 (out/'exit.json').write_text(json.dumps(dict(exit_code=exit_code)))
 if exit_code:raise RuntimeError(f'Runtime exit {exit_code}; trace may be incomplete')
 time.sleep(1.2)
 presented=[r for r in csv.DictReader((out/'present.csv').open()) if start<=int(r['time_ns'])<=end]
 pa=before['packet_interpolation']['presenter'];pb=after['packet_interpolation']['presenter'];n=pb['motion_steps']-pa['motion_steps'];sm=pb['motion_sum_ms']-pa['motion_sum_ms'];ss=pb['motion_sum_sq_ms']-pa['motion_sum_sq_ms'];occup=[r['audio_stats']['out']['fill_ms'] for r in rows]
 intervals=[(int(y['time_ns'])-int(x['time_ns']))/1e6 for x,y in zip(presented,presented[1:])]
 summary=dict(seconds=(end-start)/1e9,segments=progress,course_segments=len(pts),interference=[r for r in rows if r['interference']],presented=len(presented),trace_unresolved=sum(int(r['changed'])<0 for r in presented),presented_repeats=sum(int(r['changed'])==0 for r in presented),present_interval_variance_ms2=statistics.pvariance(intervals) if intervals else None,motion_step_mean_ms=sm/n if n else None,motion_step_variance_ms2=max(0,ss/n-(sm/n)**2) if n else None,audio_gap_samples=after['audio_stats']['out']['underruns']-before['audio_stats']['out']['underruns'],audio_occupancy_min_ms=min(occup),audio_occupancy_mean_ms=statistics.mean(occup),audio_occupancy_max_ms=max(occup),guest_max_interval_ms=after['main_cadence']['max_ms'],guest_vblanks_s=(after['frame']['frame']-before['frame']['frame'])/((end-start)/1e9),input_latency_estimate_ms=[pb['delay_ms']+pb['interval_ms'],pb['delay_ms']+pb['interval_ms']+2000/60],max_unmatched=max(r['packet_interpolation']['interpolation']['unmatched'] for r in rows),failed_pairs_delta=after['packet_interpolation']['interpolation']['failed_pairs']-before['packet_interpolation']['interpolation']['failed_pairs'])
 i=0;last_hash=None;held_slots=0;display_slots=0
 for slot in range(int((end-start)*60/1e9)):
  tick=start+slot*1000000000//60
  while i+1<len(presented) and int(presented[i+1]['time_ns'])<=tick:i+=1
  if not presented or int(presented[i]['time_ns'])>tick:continue
  h=presented[i]['image_hash'];held_slots+=h==last_hash;last_hash=h;display_slots+=1
 summary.update(audio_overflow_drops=after['audio_stats']['out']['overflow_drops']-before['audio_stats']['out']['overflow_drops'],display_slots_60hz=display_slots,held_display_slots_60hz=held_slots,presented_hz=len(presented)/summary['seconds'])
 (out/'result.json').write_text(json.dumps(dict(summary=summary,before=before,after=after,config=config),indent=2));(out/'inputs.json').write_text(json.dumps(inputs));print(json.dumps(summary),flush=True)
 (out/'timeline.json').write_text(json.dumps(dict(start_ns=start,end_ns=end,inputs=inputs,samples=rows,presents=presented)))

finally:
 if proc and proc.poll() is None:
  try:ask('quit');proc.wait(timeout=20)
  except Exception:proc.terminate();proc.wait(timeout=20)
 gamefile.write_bytes(saved_game)
 if saved is None:state.unlink(missing_ok=True)
 else:state.write_bytes(saved)
 (lock/'owner.json').unlink();lock.rmdir()
