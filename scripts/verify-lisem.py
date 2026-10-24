#!/usr/bin/env python3
"""Real RISC-V firmware/Wi-Fi/SSE acceptance, using explicitly synthetic Codex records."""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time
import urllib.request
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
CLI = Path(os.environ.get('LISEM_BIN', ROOT / '.deps/Lisem/lisem'))
from tf_image import create_card
host = os.environ.get('PET_TEST_HOST', '172.17.0.1')
port, token = '18787', 'isolated-tf-test-token'
(ROOT / 'artifacts').mkdir(exist_ok=True)
work = Path(tempfile.mkdtemp(prefix='lisem-e2e-', dir=ROOT / 'artifacts'))
sessions = work / 'sessions'
sessions.mkdir()
rollout = sessions / 'rollout-test.jsonl'
rollout.write_text(json.dumps({'type':'session_meta','payload':{'id':'listenm-e2e'}})+'\n')
env = dict(os.environ, BRIDGE_HOST=host, BRIDGE_PORT=port, BRIDGE_TOKEN=token,
           CODEX_SESSIONS_DIR=str(sessions))
opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
def health():
    request = urllib.request.Request(f'http://{host}:{port}/health',headers={'Authorization':f'Bearer {token}'})
    with opener.open(request,timeout=2) as response:
        return json.load(response)
def cli(*args):
    return subprocess.check_output([str(CLI),'--data-dir',str(work/'library'),'--json',*args],text=True,timeout=20)
try:
    health()
except Exception:
    pass
else:
    raise SystemExit('Port is already used; stop the existing bridge before this isolated test.')
bridge = None
instance = None
log_file = (work/'bridge.log').open('w')
uart = (work/'uart.log').open('w')
cursor = 0
collected = ''
def start_bridge():
    proc = subprocess.Popen(['node',str(ROOT/'bridge/src/main.js')],env=env,stdout=log_file,stderr=log_file)
    for _ in range(100):
        if proc.poll() is not None: raise RuntimeError('bridge failed to start')
        try:
            health()
            return proc
        except Exception: time.sleep(.1)
    proc.terminate()
    raise RuntimeError('bridge health timeout')
def stop_bridge(proc):
    proc.terminate()
    try: proc.wait(5)
    except subprocess.TimeoutExpired: proc.kill(); proc.wait()
def wait_for(predicate, seconds=45):
    global cursor, collected
    deadline = time.monotonic()+seconds
    while time.monotonic()<deadline:
        block = json.loads(cli('logs',instance,'0','--cursor',str(cursor),'--limit','16384'))
        if block['lost']: raise RuntimeError('UART history overflow')
        cursor = block['cursor']
        collected += block['text']
        uart.write(block['text']); uart.flush()
        if predicate(collected): return
        time.sleep(.25)
    raise RuntimeError('firmware timeout; inspect '+str(work/'uart.log'))
def append(kind, subtype, **fields):
    with rollout.open('a') as stream:
        stream.write(json.dumps({'type':kind,'payload':{'type':subtype, **fields}})+'\n')
try:
    bridge = start_bridge()
    instance = json.loads(cli('create','--board','arcs-mini','--name','listenm-e2e'))['id']
    cli('write',instance,str(ROOT/'build/voice/listenm-voice-full.bin'))
    cli('sd-card',instance,'--image',str(create_card(work,host,port,token)))
    cli('start',instance,'--network','--seconds','360','--timeout','480')
    wait_for(lambda text:'[pet] SSE connected' in text)
    wait_for(lambda text:'[pet-ui] display ready' in text)
    wait_for(lambda text:'[voice] wake word ready' in text)
    captures=[]
    transitions=[('thinking','deep_thought',{'type':'event_msg','payload':{'type':'task_started'}}),
                 ('working','coding',{'type':'response_item','payload':{'type':'function_call'}}),
                 ('speaking','cheerful_wave',{'type':'response_item','payload':{'type':'message','role':'assistant'}}),
                 ('attention','attentive_listening',{'hook_event_name':'PermissionRequest'}),
                 ('sleeping','quick_nap',{'hook_event_name':'SessionEnd'}),
                 ('error','jump_scare',{'type':'error'}),
                 ('idle','breathing',{'type':'event_msg','payload':{'type':'task_complete'}})]
    for state, animation, record in transitions:
        start=len(collected)
        if 'hook_event_name' in record:
            request=urllib.request.Request(f'http://{host}:{port}/ingest',
                data=json.dumps(record).encode(),headers={'Authorization':f'Bearer {token}'})
            with opener.open(request,timeout=2) as response: assert response.status==202
        else:
            with rollout.open('a') as stream: stream.write(json.dumps(record)+'\n')
        wait_for(lambda text, a=animation, begin=start: '[pet-ui] animation='+a in text[begin:])
        time.sleep(.6)
        frames=[]
        for n in range(4):
            target=work/f'{state}-{n}.png'
            cli('screenshot',instance,str(target))
            im=Image.open(target).convert('RGB')
            assert im.size==(240,240)
            # Verify character pixels, excluding status text. Reject black/blank screens.
            sprite=im.crop((16,23,224,219))
            assert len(sprite.getcolors(50000) or [])>100, 'blank display'
            frames.append(sprite); captures.append(im.copy())
            time.sleep(.3)
        assert any(ImageChops.difference(frames[0],f).getbbox() for f in frames[1:]), state+' is not animating'
    # Queue a festival, then verify priority preemption and a complete game clip.
    for egg, interrupt in [('decorating_a_christmas_tree',True),('playing_gomoku',False)]:
        request=urllib.request.Request(f'http://{host}:{port}/pet/play/{egg}',data=b'',
            headers={'Authorization':f'Bearer {token}'})
        with opener.open(request,timeout=2) as response: assert response.status==202
        start=len(collected)
        wait_for(lambda text: 'extra TF='+egg in text[start:] and 'extra playing' in text[start:],90)
        frames=[]
        for n in range(4):
            target=work/f'{egg}-{n}.png'; cli('screenshot',instance,str(target))
            im=Image.open(target).convert('RGB'); frames.append(im); captures.append(im.copy()); time.sleep(.3)
        assert any(ImageChops.difference(frames[0],f).getbbox() for f in frames[1:]), egg+' frozen'
        if interrupt:
            append('event_msg','task_started')
            wait_for(lambda text:'extra interrupted' in text[start:])
            append('event_msg','task_complete')
            wait_for(lambda text:'animation=breathing' in text[start:])
        else:
            wait_for(lambda text:'extra finished' in text[start:])
    captures[0].save(work/'preview.gif',save_all=True,append_images=captures[1:],duration=300,loop=0)
    assert '[pet-ui] display/asset failure' not in collected
    stop_bridge(bridge)
    bridge = start_bridge()
    wait_for(lambda text:'[pet] history reset' in text and text.count('[pet] SSE connected')>=2)
    append('event_msg','turn_aborted')
    wait_for(lambda text:'"type":"turn_aborted"' in text)
    result = {'passed':True,'fixture':'synthetic Codex rollout and hook inputs','instance':instance,
              'checks':['TF model wake engine initialization','Wi-Fi DHCP','SSE TCP connection','seven state animations','nonblank display and changing frames','bridge restart/reset/reconnect','festival TF playback','game playback/completion','Codex preempts extras'],
              'uart':str(work/'uart.log')}
    (work/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
finally:
    if instance:
        try: cli('shutdown',instance)
        except Exception: pass
    if bridge and bridge.poll() is None: stop_bridge(bridge)
    log_file.close(); uart.close()
