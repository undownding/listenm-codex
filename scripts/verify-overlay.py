#!/usr/bin/env python3
"""Real Lisem required TF resource acceptance. Requires Pillow, pyfatfs and mkfs.fat."""
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import time
from PIL import Image, ImageChops, ImageStat
from pyfatfs.PyFatFS import PyFatFS

ROOT = Path(__file__).resolve().parents[1]
CLI = Path(os.environ.get('LISEM_BIN', ROOT / '.deps/Lisem/lisem'))
(ROOT / 'artifacts').mkdir(exist_ok=True)
work = Path(tempfile.mkdtemp(prefix='overlay-e2e-', dir=ROOT / 'artifacts'))


def cli(*args):
    return json.loads(subprocess.check_output(
        [str(CLI), '--data-dir', str(work / 'library'), '--json', *args],
        text=True, timeout=30))


def card(name, background, animations=False, omit=None):
    path = work / (name + '.img')
    with path.open('wb') as stream:
        stream.truncate(64 * 1024 * 1024)
    subprocess.run(['mkfs.fat', '-F', '16', str(path)], check=True, stdout=subprocess.DEVNULL)
    fs = PyFatFS(str(path))
    try:
        if omit != 'background.rgb565': fs.writebytes('background.rgb565', background)
        if omit != 'status-font.bin': fs.writebytes('status-font.bin',(ROOT/'firmware/assets/status-font.bin').read_bytes())
        fs.writebytes('config.ini', (ROOT/'config/config.example.ini').read_bytes())
        fs.makedir('voice')
        source=ROOT/'.deps/arcs_mini/arcs-sdk/samples/algorithms/wake_up/wakeup_single/res/algo'
        for name in ('algo.bin','wrap.json'):
            fs.writebytes('voice/'+name,(source/name).read_bytes())
        if animations:
            for asset in ('whale.bin', 'click.bin'):
                if omit != asset: fs.writebytes(asset, (ROOT / 'firmware/assets' / asset).read_bytes())
    finally:
        fs.close()
    return path


# Deliberately distinct from the built-in underwater art. A uniform gold border
# permits exact source checks independent of panel framebuffer rotation.
gold = 0xFDE0
pixels = [gold if x < 16 or x >= 224 or y < 23 or y >= 219
          else (0xFDA0 if (x // 24 + y // 24) % 2 else 0xFFE8)
          for y in range(240) for x in range(240)]
background = struct.pack('<57600H', *pixels)
(work / 'background.rgb565').write_bytes(background)
valid = card('valid', background, True)
broken = card('truncated', b'broken', True)
scenarios=[('valid', valid), ('no-card', None), ('truncated', broken)]
for name in ('background.rgb565','whale.bin','click.bin','status-font.bin'):
    scenario='missing-'+name.split('.')[0]
    scenarios.append((scenario,card(scenario,background,True,omit=name)))
fallback=Image.open(ROOT/'firmware/assets/resource-missing.png').convert('RGB')
# Hardware conversion to RGB565 and 95% backlight produces small rounding differences.
fallback=fallback.point(lambda value: int(value*.95))
results = []
for scenario, medium in scenarios:
    instance = None
    text = ''
    cursor = 0
    try:
        instance = cli('create', '--board', 'arcs-mini', '--name', 'Overlay ' + scenario)['id']
        cli('write', instance, str(ROOT / 'build/voice/listenm-voice-full.bin'))
        if medium:
            cli('sd-card', instance, '--image', str(medium))
        cli('start', instance, '--seconds', '100', '--timeout', '150')

        def wait(fragment, start=0):
            global cursor, text
            deadline = time.monotonic() + 60
            while time.monotonic() < deadline:
                block = cli('logs', instance, '0', '--cursor', str(cursor), '--limit', '16384')
                assert not block['lost'], 'UART history lost'
                cursor = block['cursor']
                text += block['text']
                (work / (scenario + '.log')).write_text(text)
                if fragment in text[start:]:
                    return
                time.sleep(.2)
            raise AssertionError(f'{scenario}: missing {fragment}; see {work}')

        def capture(name):
            target = work / (scenario + '-' + name + '.png')
            cli('screenshot', instance, str(target))
            return Image.open(target).convert('RGB')

        if scenario != 'valid':
            wait('TF/config.ini missing or invalid' if scenario == 'no-card' else 'display/asset failure')
            wait('resource missing screen shown')
            shown=capture('error')
            difference=ImageStat.Stat(ImageChops.difference(shown,fallback))
            assert max(difference.mean)<4, f'{scenario}: wrong fallback image {difference.mean}'
            results.append({'scenario': scenario, 'passed': True})
            continue
        wait('display ready')
        wait('[voice] wake word ready')
        time.sleep(1)
        first = capture('idle-0')
        time.sleep(.7)
        second = capture('idle-1')
        assert ImageChops.difference(first, second).getbbox(), 'idle animation frozen'
        corners = [first.getpixel(p) for p in [(0, 0), (239, 0), (0, 239), (239, 239)]]
        # Firmware sets backlight to 95%; Lisem applies it to RGB565 output.
        is_gold = all(240 <= r <= 243 and 178 <= g <= 182 and b <= 7 for r, g, b in corners)
        if scenario == 'valid':
            assert is_gold, corners
            for name in ('background.rgb565', 'whale.bin', 'click.bin'):
                assert '[overlay] TF ' + name in text, name
            status = cli('status', instance)
            run = status['runtime']['session']['output']
            if isinstance(run, dict):
                run = run['id']
            start = len(text)
            cli('button-sequence', instance, 'function', '--run', str(run),
                '--count', '2', '--hold-ms', '80', '--gap-ms', '100')
            wait('click animation=blue_whale_appears', start)
            first_click = capture('click-0')
            time.sleep(.7)
            second_click = capture('click-1')
            assert ImageChops.difference(first_click, second_click).getbbox(), 'click animation frozen'
            wait('click finished; idle buffer', start)
            assert 'invalid/read failed' not in text, text
        assert 'display/asset failure' not in text
        results.append({'scenario': scenario, 'passed': True, 'corners': corners})
    finally:
        if instance:
            cli('shutdown', instance)
result = {'passed': True, 'checks': results, 'artifacts': str(work)}
(work / 'result.json').write_text(json.dumps(result, indent=2))
print(json.dumps(result, indent=2))
