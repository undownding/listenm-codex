#!/usr/bin/env python3
"""Build optional bridge animation packs using the firmware's shared transform."""
import importlib.util, json, struct, hashlib
from pathlib import Path
spec=importlib.util.spec_from_file_location('pack',Path(__file__).with_name('pack-whale.py'))
p=importlib.util.module_from_spec(spec); spec.loader.exec_module(p)

def main():
    root=p.ROOT
    layout=json.loads((root/'firmware/assets/manifest.json').read_text())['layout']
    dest=root/'bridge/assets'; dest.mkdir(exist_ok=True)
    core={clip for _,clip in p.CLIPS}; catalog=[]
    for source in sorted((root/'.deps/dsh-whale-pet/assets/thumb').glob('*/*.webm')):
        category=source.parent.name; clip=f'{category}/{source.stem}'
        if category in ('drag','moves','turn') or clip in core: continue
        frames=p.decode_frames(source)
        canvases=[p.compose_frame(im,layout) for im in frames]
        samples=canvases[::max(1,len(canvases)//12)]
        montage=p.Image.new('RGB',(p.W,p.H*len(samples)))
        for i,im in enumerate(samples): montage.paste(im,(0,p.H*i))
        palette=montage.quantize(colors=256,method=p.Image.Quantize.MEDIANCUT)
        colors=palette.getpalette(); blob=bytearray()
        for i in range(256):
            r,g,b=colors[i*3:i*3+3]; blob.extend(struct.pack('<H',((r>>3)<<11)|((g>>2)<<5)|(b>>3)))
        offsets=[]
        for im in canvases:
            offsets.append(len(blob)); blob.extend(p.encode_frame(im,palette))
        offsets.append(len(blob))
        # Header + offsets precede palette/frames; all offsets relative to blob.
        header=struct.pack('<4sIII',b'WHP2',len(frames),p.W,p.H)+struct.pack(f'<{len(offsets)}I',*offsets)
        data=header+blob
        if len(data)>1750000: raise ValueError(f'Pack exceeds device budget: {clip} ({len(data)} bytes)')
        (dest/f'{source.stem}.bin').write_bytes(data)
        catalog.append(dict(id=source.stem,category=category,frames=len(frames),bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
        print(clip,len(data),flush=True)
    (dest/'catalog.json').write_text(json.dumps(catalog,indent=2)+'\n')
    (dest/'manifest.json').write_text(json.dumps(dict(
        source_commit=p.subprocess.check_output(['git','-C',str(root/'.deps/dsh-whale-pet'),'rev-parse','HEAD'],text=True).strip(),
        format="WHP2-RGB565-A8",alpha_levels=16,layout=layout,width=p.W,height=p.H,fps=p.FPS,
        core_manifest_sha256=hashlib.sha256((root/'firmware/assets/manifest.json').read_bytes()).hexdigest(),
        clips=len(catalog),bytes=sum(c['bytes'] for c in catalog)),indent=2)+'\n')

if __name__ == '__main__':
    main()
