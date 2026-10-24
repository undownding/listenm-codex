#!/usr/bin/env python3
"""Copy all device resources to a directory/TF mount; preserve existing config."""
import argparse
import os
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]

def prepare(target, sdk):
    target.mkdir(parents=True, exist_ok=True)
    for name in ('whale.bin', 'click.bin', 'background.rgb565', 'status-font.bin'):
        shutil.copyfile(ROOT / 'firmware/assets' / name, target / name)
    (target / 'extras').mkdir(exist_ok=True)
    for source in (ROOT / 'bridge/assets').glob('*.bin'):
        shutil.copyfile(source, target / 'extras' / source.name)
    shutil.copyfile(ROOT / 'bridge/assets/catalog.json', target / 'extras/catalog.json')
    if not (target / 'config.ini').exists():
        shutil.copyfile(ROOT / 'config/config.example.ini', target / 'config.ini')
    if sdk:
        source = sdk / 'arcs-sdk/samples/algorithms/wake_up/wakeup_single/res/algo'
        (target / 'voice').mkdir(exist_ok=True)
        for name in ('algo.bin', 'wrap.json'):
            shutil.copyfile(source / name, target / 'voice' / name)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('target', type=Path)
    parser.add_argument('--sdk', type=Path, default=Path(os.environ.get('SDK_ROOT', ROOT / '.deps/arcs_mini')))
    args = parser.parse_args()
    prepare(args.target, args.sdk)
    print(f'TF resources ready: {args.target}; edit config.ini before boot.')
