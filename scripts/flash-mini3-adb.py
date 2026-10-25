#!/usr/bin/env python3
"""Update Mini3 AP/CP through the paired recovery boot, preserving TF and other Flash."""
import argparse
import hashlib
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', required=True, help='Explicit ADB serial or emulator host:port')
    parser.add_argument('--adb', default=shutil.which('adb') or str(Path.home()/'.listenai/listenai-tools/adb/adb'))
    parser.add_argument('--build', type=Path, default=ROOT/'build/voice-mini3')
    parser.add_argument('--recovery', action='store_true', help='Already in paired Mini3 recovery boot')
    args = parser.parse_args()
    cp = args.build/'listenm-pet.bin'
    ap = ROOT/'.deps/arcs_mini/res/arcs-mini3/ap.bin'
    data = cp.read_bytes()
    if len(data) < 0x188 or struct.unpack_from('<II', data, 0x180) != (0x28300000, 0x500000):
        parser.error('CP image does not carry the Mini3 memory compatibility header')
    parts = [(ap, 0x40000, 0xc0000), (cp, 0x600000, 0x400000)]
    for path, _, limit in parts:
        if not 0 < path.stat().st_size <= limit:
            parser.error(f'Invalid partition size: {path}')

    def adb(*cmd, check=True, timeout=30):
        result = subprocess.run([args.adb, '-s', args.serial, *cmd],
                                capture_output=True, text=True, timeout=timeout)
        if check and result.returncode:
            raise RuntimeError(result.stderr or result.stdout)
        return result.stdout + result.stderr

    def reconnect():
        if ':' in args.serial:
            subprocess.run([args.adb, 'disconnect', args.serial], capture_output=True, timeout=10)
            subprocess.run([args.adb, 'connect', args.serial], capture_output=True, timeout=10)

    if not args.recovery:
        # Reset can close the service before ADB receives its acknowledgement.
        adb('reboot', 'recovery', check=False)
        time.sleep(2)
    deadline = time.monotonic() + 30
    while True:
        reconnect()
        try:
            info = adb('shell', 'info flash', timeout=5)
            if '16777216' in info or '0x1000000' in info or '16MB' in info:
                break
        except (RuntimeError, subprocess.TimeoutExpired):
            pass
        if time.monotonic() >= deadline:
            raise RuntimeError('Expected 16 MiB recovery Flash was not detected; nothing written')
        time.sleep(1)
    with tempfile.TemporaryDirectory(prefix='mini3-readback-') as directory:
        devices = subprocess.run([args.adb, 'devices', '-l'], capture_output=True,
                                 text=True, check=True, timeout=10).stdout
        selected = [line for line in devices.splitlines()
                    if line.split() and line.split()[0] == args.serial]
        if len(selected) != 1 or 'model:arcs_mini3' not in selected[0].split():
            raise RuntimeError('Selected recovery device is not Mini3; nothing written')
        for path, offset, _ in parts:
            data = path.read_bytes()
            remote = f'/RAW/FLASH/{offset:x}'
            adb('push', str(path), remote, timeout=120)
            readback = Path(directory)/path.name
            adb('pull', f'{remote}/{len(data):x}', str(readback), timeout=120)
            if readback.read_bytes() != data:
                raise RuntimeError(f'Flash readback mismatch at {offset:#x}; left in recovery')
            print(f'Verified {path.name} at {offset:#x}: SHA256 {hashlib.sha256(data).hexdigest()}', flush=True)
    adb('shell', 'reboot', check=False)
    time.sleep(2)
    reconnect()
    adb('wait-for-device', timeout=30)
    print('AP/CP verified; application ADB reconnected. Boot and TF were not modified.')


if __name__ == '__main__':
    main()
