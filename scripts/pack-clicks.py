#!/usr/bin/env python3
"""Embed all single-click + double-click packs; cheerful_wave is already core."""
from pathlib import Path
import hashlib,json,struct

def main():
    root=Path(__file__).resolve().parents[1]
    assets=root/'firmware/assets'
    manifest=json.loads((root/'bridge/assets/manifest.json').read_text())
    assert manifest['core_manifest_sha256']==hashlib.sha256((assets/'manifest.json').read_bytes()).hexdigest(), 'Regenerate extras for the current core layout first'
    blob=bytearray(); offsets=[]
    for name in ['happy_hop','shy_surprise','tsundere_pout','ticklish_giggle','cheerful_wave','blue_whale_appears']:
     if name=='cheerful_wave':
      offsets.append((0,0,name));continue
     data=(root/f'bridge/assets/{name}.bin').read_bytes()
     assert data[:4]==b'WHP2' and struct.unpack_from('<II',data,8)==(208,196)
     offsets.append((len(blob),len(data),name));blob.extend(data)
    (assets/'click.bin').write_bytes(blob)
    (assets/'click.h').write_text('#ifndef PET_CLICK_ASSETS_H\n#define PET_CLICK_ASSETS_H\n#include <stdint.h>\nextern const uint8_t click_blob[];\n'
     '#define CLICK_SINGLE_COUNT 5\n#define CLICK_DOUBLE 6\n'
     'typedef struct { uint32_t offset,size; const char *name; } click_asset;\n'
     'static const click_asset click_assets[] = {\n'+
     ''.join(f' {{{offset},{size},"{name}"}},\n' for offset,size,name in offsets)+'};\n#endif\n')
    print('Click packs:',len(blob),'bytes')

if __name__ == '__main__':
    main()
