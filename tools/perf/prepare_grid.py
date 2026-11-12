#!/usr/bin/env python3
import sys,os,shutil,subprocess,time,json
from pathlib import Path
import argparse
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root/'tools'))
from dbg import cmd
from smoke import STEPS,START_FRAME,wait_frames,engage_widescreen
from pad import mask
parser=argparse.ArgumentParser(description='Create an isolated, current-build GP grid checkpoint without showing the intro.')
parser.add_argument('output',type=Path)
parser.add_argument('--build',required=True,type=Path)
args=parser.parse_args()
stage=args.output.resolve();stage.mkdir(parents=True,exist_ok=False)
source=args.build.resolve()
for name in ('r4-runtime','r4-runtime.execution.json','game.toml','game_options.toml','assets','bios','postfx'):
 p=source/name
 if p.is_dir():shutil.copytree(p,stage/name)
 else:shutil.copy2(p,stage/name)
shutil.copytree(source/'mods/bundled',stage/'mods/bundled')
(stage/'mods/state.toml').write_text('format_version = 2\n[[package]]\nid = "psx.enhancement.internal-resolution"\n[[feature]]\npackage_id = "psx.enhancement.internal-resolution"\nid = "internal-resolution"\nenabled = true\n[feature.values]\nresolution = "9x"\n[[package]]\nid = "r4.enhancement.frame-rate"\n[[feature]]\npackage_id = "r4.enhancement.frame-rate"\nid = "frame-rate"\nenabled = true\n[feature.values]\nrate = "60"\n')
env={k:v for k,v in os.environ.items() if not k.startswith(('PSX_','R4L_'))}
env.update(SDL_AUDIODRIVER='dummy',PSX_DEBUG_SERVER='1',PSX_VSYNC='0',PSX_INTERNAL_RESOLUTION='native',PSX_REWIND='0',R4L_BOOT_MENU_DONE='1')
port=4799
def c(**kw):return cmd(kw,port=port)
with (stage/'runtime.log').open('w') as log:
 p=subprocess.Popen([str(stage/'r4-runtime'),'--game',str(stage/'game.toml'),'--disc',str(root/'disc/R4 - Ridge Racer Type 4 (USA).cue'),'--no-launcher','--hidden-window','--debug-port',str(port),'--memcard-dir',str(stage/'memcards')],cwd=stage,env=env,stdout=log,stderr=log)
 try:
  for _ in range(300):
   try:c(cmd='turbo',enabled=1);break
   except OSError:time.sleep(.1)
  wait_frames(max(0,START_FRAME-c(cmd='frame')['frame']),port)
  for label,buttons,hold,settle in STEPS:
   if label.startswith('race'):break
   if buttons:c(cmd='set_input',buttons=hex(mask(buttons)));wait_frames(int(hold*60),port);c(cmd='clear_input')
   wait_frames(int(settle*60),port)
   if label=='menu':engage_widescreen(port)
  c(cmd='turbo',enabled=0);c(cmd='savestate',op='save',slot=0)
  for _ in range(100):
   s=c(cmd='savestate_status')
   if s.get('last_op')=='save' and s.get('last_ok'):break
   time.sleep(.1)
  assert s.get('last_ok'),s
  assert c(cmd='dispatch_stats')['miss_total']==0
  assert int(c(cmd='mem_words',addr='0x800FF860',count=1)['words'][0],16)==2, 'Not a GP race'
  (stage/'grid.json').write_text(json.dumps({'save':s,'dispatch':c(cmd='dispatch_stats')},indent=2))
  print(stage)
 finally:p.terminate();p.wait(timeout=10)
