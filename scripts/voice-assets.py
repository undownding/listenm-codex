#!/usr/bin/env python3
"""Downsample bundled animations to 6 FPS for the 8 MiB voice CP partition."""
import pathlib
import re
import shutil
import struct
import sys

source = pathlib.Path(__file__).resolve().parents[1] / 'firmware/assets'
target = pathlib.Path(sys.argv[1])
target.mkdir(parents=True, exist_ok=True)
blob = (source / 'whale.bin').read_bytes()
header = (source / 'whale.h').read_text()
output = bytearray()
arrays = {}
for name, values in re.findall(r'static const uint32_t frames_(\w+)\[\] = \{([^}]+)\};', header):
    arrays[name] = list(map(int, values.split(',')))
for palette, name, count, label in re.findall(r'\{(\d+), frames_(\w+), (\d+), "([^"]+)"\}', header):
    palette = int(palette)
    new_palette = len(output)
    output.extend(blob[palette:palette+512])
    offsets = []
    for i in range(0, int(count), 2):
        offsets.append(len(output))
        output.extend(blob[arrays[name][i]:arrays[name][i+1]])
    offsets.append(len(output))
    header = re.sub(r'(frames_' + name + r'\[\] = )\{[^}]+\}', lambda m: m[1] + '{' + ','.join(map(str, offsets)) + '}', header)
    header = header.replace(f'{{{palette}, frames_{name}, {count},', f'{{{new_palette}, frames_{name}, {len(offsets)-1},')
(target / 'whale.bin').write_bytes(output)
(target / 'whale.h').write_text(header.replace('WHALE_FPS 12', 'WHALE_FPS 6'))
blob = (source / 'click.bin').read_bytes()
header = (source / 'click.h').read_text()
output = bytearray()
for offset, size, name in re.findall(r'\{(\d+),(\d+),"([^"]+)"\}', header):
    if int(size) == 0:
        continue
    pack = blob[int(offset):int(offset)+int(size)]
    count = struct.unpack_from('<I', pack, 4)[0]
    offsets = struct.unpack_from(f'<{count+1}I', pack, 16)
    base = 16 + 4*(count+1)
    payload = bytearray(pack[base:base+512])
    new_offsets = []
    for i in range(0, count, 2):
        new_offsets.append(len(payload))
        payload.extend(pack[base+offsets[i]:base+offsets[i+1]])
    new_offsets.append(len(payload))
    result = b'WHP2' + struct.pack('<III', len(new_offsets)-1, 208, 196)
    result += struct.pack(f'<{len(new_offsets)}I', *new_offsets) + payload
    header = header.replace(f'{{{offset},{size},"{name}"}}', f'{{{len(output)},{len(result)},"{name}"}}')
    output.extend(result)
(target / 'click.bin').write_bytes(output)
(target / 'click.h').write_text(header)
shutil.copyfile(source / 'background.rgb565', target / 'background.rgb565')
