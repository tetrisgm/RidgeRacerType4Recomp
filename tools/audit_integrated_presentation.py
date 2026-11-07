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
ap=argparse.ArgumentParser();ap.add_argument('--build',default='build');ap.add_argument('--name',default='baseline');ap.add_argument('--port',type=int,default=4858);ap.add_argument('--profile');ap.add_argument('--seconds',type=float,default=0);ap.add_argument('--disable',default='');ap.add_argument('--no-rewind',action='store_true');ap.add_argument('--load-grid',action='store_true');ap.add_argument('--grid-source',default='baseline-lap');ap.add_argument('--owner-8k',action='store_true');a=ap.parse_args()
out=ROOT/'analysis/present-guest-clock'/a.name;out.mkdir(parents=True,exist_ok=True)
build=ROOT/a.build
framework=next(Path(line.split('=',1)[1]) for line in (build/'CMakeCache.txt').read_text().splitlines() if line.startswith('PSXRECOMP_ROOT:PATH='))
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
proc=None;gpu_monitor=None
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
 gpu_monitor=subprocess.Popen(['nvidia-smi','--query-gpu=timestamp,utilization.gpu,utilization.memory,memory.used','--format=csv','-l','1'],stdout=(out/'gpu-load.csv').open('wb'),stderr=subprocess.DEVNULL,creationflags=subprocess.CREATE_NO_WINDOW)
 subprocess.run([sys.executable,str(ROOT/'tools/mod_state.py'),str(build),'--defaults'],check=True)
 subprocess.run([sys.executable,str(ROOT/'tools/mod_state.py'),str(build),'--enable','r4.enhancement.frame-rate/frame-rate','rate=60','--enable','r4.enhancement.car-effects/car-effects'],check=True)
 normalized=saved_game if b'\nrewind = ' in saved_game else saved_game.replace(b'\n[video]\n',b'\n[video]\nrewind = true\n',1)
 gamefile.write_bytes(normalized.replace(b'match_display_max_lines = 4320',b'match_display_max_lines = 2160').replace(b'dynamic_resolution = true',b'dynamic_resolution = false').replace(b'dynamic_resolution_start_min = true',b'dynamic_resolution_start_min = false'))
 if a.owner_8k: gamefile.write_bytes(saved_game.replace(b'match_display_max_lines = 2160',b'match_display_max_lines = 4320'))
 if a.no_rewind: gamefile.write_bytes(saved_game.replace(b'rewind = true',b'rewind = false'))
 env=dict(os.environ,PSX_PGXP_CPU_MODE='1',PSX_HOST_SAMPLER='0',SDL_AUDIODRIVER='dummy',PSX_HLE_SCHEDULER='0',PSX_BIOS_HLE='0',PSX_PRESENT_TRACE=str(out/'present.csv'),PSX_PRESENT_SLOT_TRACE=str(out/'slots.csv'),PSX_PRESENT_IMAGE_RING='0',PSX_PACKET_INTERPOLATION_AUDIT='0',PSX_GL_ASYNC_GPU_TIMER='1')
 env['PATH']=r'C:\Users\Shokunin\AppData\Local\retcomm\toolchains\cmake-clang-v1\latest\bin;C:\msys64\mingw64\bin;'+env['PATH']
 if env.get('PSX_AUDIT_NO_TRACE')=='1':env.pop('PSX_PRESENT_TRACE',None);env.pop('PSX_PRESENT_SLOT_TRACE',None);env['PSX_GL_ASYNC_GPU_TIMER']='0'
 if a.profile:env['LLVM_PROFILE_FILE']=str(Path(a.profile).resolve())
 if a.disable: subprocess.run([sys.executable,str(ROOT/'tools/mod_state.py'),str(build),'--disable',a.disable],check=True)
 # A private working directory and memory-card path preserve personal saves.
 (out/'provenance.json').write_text(json.dumps(dict(executable=str(build/'r4-runtime.exe'),sha256=hashlib.sha256((build/'r4-runtime.exe').read_bytes()).hexdigest(),framework=subprocess.check_output(['git','-C',str(framework),'rev-parse','HEAD'],text=True).strip(),framework_dirty=bool(subprocess.check_output(['git','-C',str(framework),'status','--porcelain'],text=True).strip()),environment={k:env[k] for k in ('PSX_AUDIT_NO_TRACE','PSX_PGXP_CPU_MODE','PSX_HOST_SCHEDULING','SDL_AUDIODRIVER','PSX_HLE_SCHEDULER','PSX_BIOS_HLE','PSX_BIOS_HLE_KEEP_INTRO','PSX_HOST_SAMPLER','PSX_PACKET_HD_CACHE','PSX_INTERPOLATION_REUSE','PSX_SURFACE_REUSE','PSX_AUDIO_ADAPTIVE','PSX_PRESENT_DELAY_MS','PSX_PRESENT_GUEST_CLOCK','PSX_GL_LEGACY_SURFACE_TEST','PSX_RENDER_OP_PROFILE','PSX_GL_ASYNC_GPU_TIMER','PSX_PACKET_INTERPOLATION_AUDIT') if k in env},hidden=True,audio_driver='dummy',pgo_profile=a.profile),indent=2))
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
  for f in (ROOT/'analysis/present-guest-clock'/a.grid_source/'saves/openbios').glob('*slot09*'):shutil.copy2(f,target/f.name)
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
 if a.owner_8k: ask('window_size',w=7680,h=4320)
 base,pts=find_course(p)
 config={k:ask(k) for k in ['host_profile','video_info','pgxp','rewind_status','packet_interpolation','mod_counters','dispatch_stats']}
 if a.owner_8k:
  assert config['video_info']['effective_scale']==18,config['video_info']
  assert config['video_info']['drawable_w']==7680 and config['video_info']['drawable_h']==4320,config['video_info']
 (out/'config.json').write_text(json.dumps(config,indent=2))
 assert config['dispatch_stats'].get('miss_total',0)==0,config['dispatch_stats']
 assert config['host_profile'].get('seq',0)==0,config['host_profile']
 assert config['rewind_status']['enabled'],config['rewind_status']
 assert config['video_info']['effective_scale']==9,config['video_info']
 assert config['host_profile'].get('seq',0)==0,config['host_profile']
 assert config['rewind_status']['enabled'],config['rewind_status']
 assert config['video_info']['effective_scale']==9,config['video_info']
 assert config['pgxp']['enabled'] and config['pgxp']['cpu_mode'],config['pgxp']
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
 after={k:ask(k) for k in ['host_profile','frame_wall_ring','frame','audio_stats','packet_interpolation','main_cadence','render_thread','dispatch_stats','rewind_status','mod_counters']}
 if env.get('PSX_AUDIT_CPU_PROFILE'):
  (out/'host-profile.json').write_text(json.dumps(ask('host_profile',frame_lo=before['frame']['frame'],frame_hi=after['frame']['frame'],top=32768,callers=1)))
 ask('set_input',buttons='0xFFFF');ask('screenshot',path=str(out/'lap.png'))
 rewind_before=ask('rewind_status',hold=1,verify=1);time.sleep(.5);rewind_held=ask('rewind_status');ask('screenshot',path=str(out/'rewind.png'),packet=1);ask('rewind_status',hold=0);time.sleep(.3)
 (out/'raw.json').write_text(json.dumps(dict(before=before,after=after,start=start,end=end)));(out/'rewind.json').write_text(json.dumps(dict(before=rewind_before,held=rewind_held,after=ask('rewind_status'),pgxp=ask('pgxp')),indent=2))
 ask('quit');exit_code=proc.wait(timeout=30)
 (out/'exit.json').write_text(json.dumps(dict(exit_code=exit_code)))
 if exit_code:raise RuntimeError(f'Runtime exit {exit_code}; trace may be incomplete')
 time.sleep(1.2)
 presented=[r for r in csv.DictReader((out/'present.csv').open()) if start<=int(r['time_ns'])<=end] if (out/'present.csv').exists() else []
 pa=before['packet_interpolation'].get('presenter',{});pb=after['packet_interpolation'].get('presenter',{});n=pb.get('motion_steps',0)-pa.get('motion_steps',0);sm=pb.get('motion_sum_ms',0)-pa.get('motion_sum_ms',0);ss=pb.get('motion_sum_sq_ms',0)-pa.get('motion_sum_sq_ms',0);occup=[r['audio_stats']['out']['fill_ms'] for r in rows]
 intervals=[(int(y['time_ns'])-int(x['time_ns']))/1e6 for x,y in zip(presented,presented[1:])]
 summary=dict(seconds=(end-start)/1e9,segments=progress,course_segments=len(pts),interference=[r for r in rows if r['interference']],presented=len(presented),trace_unresolved=sum(int(r['changed'])<0 for r in presented),presented_repeats=sum(int(r['changed'])==0 for r in presented),present_interval_variance_ms2=statistics.pvariance(intervals) if intervals else None,motion_step_mean_ms=sm/n if n else None,motion_step_variance_ms2=max(0,ss/n-(sm/n)**2) if n else None,audio_gap_samples=after['audio_stats']['out']['underruns']-before['audio_stats']['out']['underruns'],audio_occupancy_min_ms=min(occup),audio_occupancy_mean_ms=statistics.mean(occup),audio_occupancy_max_ms=max(occup),guest_max_interval_ms=after['main_cadence'].get('max_ms'),guest_vblanks_s=(after['frame']['frame']-before['frame']['frame'])/((end-start)/1e9),input_latency_estimate_ms=None,max_unmatched=None,failed_pairs_delta=None)
 wall=after['frame_wall_ring'];stride=wall.get('stride',6);fr=wall.get('f',[]);wt=[fr[j+1] for j in range(0,len(fr),stride) if before['frame']['frame']<=fr[j]<=after['frame']['frame']]
 if summary['guest_max_interval_ms'] is None and len(wt)>1:summary['guest_max_interval_ms']=max(y-x for x,y in zip(wt,wt[1:]))/1000
 i=0;last_hash=None;held_slots=0;display_slots=0
 for slot in range(int((end-start)*60/1e9)):
  tick=start+slot*1000000000//60
  while i+1<len(presented) and int(presented[i+1]['time_ns'])<=tick:i+=1
  if not presented or int(presented[i]['time_ns'])>tick:continue
  h=presented[i].get('image_hash',presented[i].get('sample_hash'));held_slots+=h==last_hash;last_hash=h;display_slots+=1
 summary['counter_present_hz']=(after['render_thread']['present_thread']['presents']-before['render_thread']['present_thread']['presents'])/summary['seconds']
 summary['distinct_hz']=(len(presented)-summary['presented_repeats'])/summary['seconds'] if presented else None
 summary.update(audio_overflow_drops=after['audio_stats']['out'].get('overflow_drops',0)-before['audio_stats']['out'].get('overflow_drops',0),display_slots_60hz=display_slots,held_display_slots_60hz=held_slots,presented_hz=len(presented)/summary['seconds'])
 if env.get('PSX_AUDIT_NO_TRACE')=='1':
  summary['presented_hz']=None;summary['held_display_slots_60hz']=None;summary['display_slots_60hz']=None
 (out/'result.json').write_text(json.dumps(dict(summary=summary,before=before,after=after,config=config),indent=2));(out/'inputs.json').write_text(json.dumps(inputs));print(json.dumps(summary),flush=True)
 (out/'timeline.json').write_text(json.dumps(dict(start_ns=start,end_ns=end,inputs=inputs,samples=rows,presents=presented)))

finally:
 if gpu_monitor and gpu_monitor.poll() is None:
  gpu_monitor.terminate();gpu_monitor.wait(timeout=10)
 if proc and proc.poll() is None:
  try:ask('quit');proc.wait(timeout=20)
  except Exception:proc.terminate();proc.wait(timeout=20)
 gamefile.write_bytes(saved_game)
 if saved is None:state.unlink(missing_ok=True)
 else:state.write_bytes(saved)
 (lock/'owner.json').unlink();lock.rmdir()
