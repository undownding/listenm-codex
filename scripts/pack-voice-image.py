#!/usr/bin/env python3
"""Package the SDK's AP + algorithm + our CP image, rejecting overlapping partitions."""
import pathlib
import sys
sdk, build = map(pathlib.Path, sys.argv[1:])
resources = sdk / 'arcs-sdk/samples/algorithms/wake_up/wakeup_single/res'
parts = [(0, 0x200000, resources/'ap.bin'),
         (0x200000, 0x4d0000, resources/'algo/algo.bin'),
         (0x4d0000, 0x800000, resources/'algo/wrap.json'),
         (0x800000, 0x1000000, build/'listenm-pet.bin')]
image = bytearray(b'\xff' * 0x1000000)
for start, end, path in parts:
    data = path.read_bytes()
    if len(data) > end-start:
        raise SystemExit(f'{path.name} exceeds its Flash partition')
    image[start:start+len(data)] = data
(build/'listenm-voice-full.bin').write_bytes(image)
print('Voice image: build/voice/listenm-voice-full.bin (Flash offset 0)')
