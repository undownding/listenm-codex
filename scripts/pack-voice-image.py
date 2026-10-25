#!/usr/bin/env python3
"""Package the SDK's AP + algorithm + our CP image, rejecting overlapping partitions."""
import pathlib
import sys
sdk, build = map(pathlib.Path, sys.argv[1:3])
board = sys.argv[3] if len(sys.argv)>3 else 'arcs_mini'
resources = sdk / 'arcs-sdk/samples/algorithms/wake_up/wakeup_single/res'
parts = [(0, 0x200000, resources/'ap.bin'),
         (0x800000, 0x1000000, build/'listenm-pet.bin')]
if board == 'arcs_mini3':
    resources = sdk / 'res/arcs-mini3'
    parts = [(0, 0x3d000, build.parent/'mini3-recovery/boot.bin'),
             (0x40000, 0x100000, resources/'ap.bin'),
             (0x600000, 0xa00000, build/'listenm-pet.bin')]
elif board != 'arcs_mini':
    raise SystemExit(f'Unsupported board: {board}')
image = bytearray(b'\xff' * 0x1000000)
for start, end, path in parts:
    data = path.read_bytes()
    if len(data) > end-start:
        raise SystemExit(f'{path.name} exceeds its Flash partition')
    image[start:start+len(data)] = data
(build/'listenm-voice-full.bin').write_bytes(image)
print(f'Voice image: {build}/listenm-voice-full.bin ({board}, Flash offset 0)')
