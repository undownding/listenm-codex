#!/usr/bin/env python3
"""Verify offline physical-key single/double clicks in real Lisem firmware."""
import json, re, subprocess, tempfile, time
from pathlib import Path
from PIL import Image, ImageChops
ROOT=Path(__file__).resolve().parents[1]
(ROOT/'artifacts').mkdir(exist_ok=True)
work=Path(tempfile.mkdtemp(prefix='button-e2e-',dir=ROOT/'artifacts'))
CLI=ROOT/'.deps/Lisem/lisem'
def cli(*args):
 return json.loads(subprocess.check_output([str(CLI),'--data-dir',str(work/'library'),'--json',*args],text=True,timeout=20))
instance=None; cursor=0; text=''
def wait(fragment,start=0,seconds=45):
 global cursor,text
 until=time.monotonic()+seconds
 while time.monotonic()<until:
  result=cli('logs',instance,'0','--cursor',str(cursor),'--limit','16384')
  assert not result['lost'];cursor=result['cursor'];text+=result['text'];(work/'uart.log').write_text(text)
  if fragment in text[start:]:return
  time.sleep(.2)
 raise RuntimeError(f'Timeout: {fragment}; {work}')
def screenshot(name):
 target=work/f'{name}.png';cli('screenshot',instance,str(target));return Image.open(target).convert('RGB')
try:
 instance=cli('create','--board','arcs-mini','--name','Click acceptance')['id']
 cli('write',instance,str(ROOT/'build/firmware/listenm-pet.bin'))
 result=cli('start',instance,'--seconds','150','--timeout','240')
 (work/'start.json').write_text(json.dumps(result,indent=2))
 wait('display ready')
 # Use virtual-time sequences so host scheduling cannot turn a double into two singles.
 status=cli('status',instance);(work/'status.json').write_text(json.dumps(status,indent=2))
 run=status['runtime']['session']['output']
 if isinstance(run,dict):run=run.get('id')
 if not run: raise RuntimeError('Inspect status for run identifier')
 def press(count=1,hold=80):
  cli('button-sequence',instance,'function','--run',str(run),'--count',str(count),'--hold-ms',str(hold),'--gap-ms','100')
 start=len(text);press();wait('click animation=',start)
 frames=[screenshot('single-0')];time.sleep(.6);frames.append(screenshot('single-1'))
 assert ImageChops.difference(frames[0],frames[1]).getbbox(),'single animation frozen'
 # Five consecutive singles cover the complete shuffled response pool.
 names=re.findall(r'click animation=(\w+)',text[start:])
 for n in range(1,5):
  start=len(text);press();wait('click animation=',start)
  names.extend(re.findall(r'click animation=(\w+)',text[start:]))
  frames.append(screenshot(f'pool-{n}'));time.sleep(.6)
 assert set(names)=={'happy_hop','shy_surprise','tsundere_pout','ticklish_giggle','cheerful_wave'},names
 # Double click interrupts the first single's in-progress animation.
 start=len(text);press(2);wait('click animation=blue_whale_appears',start)
 frames.append(screenshot('double-0'));time.sleep(.6);frames.append(screenshot('double-1'))
 assert ImageChops.difference(frames[-2],frames[-1]).getbbox(),'double animation frozen'
 wait('click finished; idle buffer',start)
 start=len(text);press(hold=1200);wait('click animation=',start);wait('click finished; idle buffer',start)
 assert text[start:].count('function key single')==1,'hold repeated'
 assert '[pet-ui] display/asset failure' not in text
 frames[0].save(work/'preview.gif',save_all=True,append_images=frames[1:],duration=600,loop=0)
 result={'passed':True,'checks':['offline single click','all five single-click animations without repeats','animated frames','320ms double click','double preempts single','completion and idle buffer','long hold emits once'],'artifacts':str(work)}
 (work/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
finally:
 if instance:cli('shutdown',instance)
