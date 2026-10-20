#!/usr/bin/env python3
"""Offline media conversion only: transparent VP9 -> indexed RGB + alpha PackBits frames.
Requires FFmpeg (libvpx-vp9) and Pillow. No runtime video decoder needed.
"""
import argparse, hashlib, json, struct, subprocess
from pathlib import Path
from PIL import Image
ROOT = Path(__file__).resolve().parents[1]
CLIPS = [('idle','idle/breathing'),('thinking','work/deep_thought'),
         ('working','work/coding'),('speaking','clicks/cheerful_wave'),
         ('attention','daily/attentive_listening'),('sleeping','daily/quick_nap'),
         ('error','fun/jump_scare')]
FPS, W, H = 12, 208, 196
BG = (237,243,252)
def pack(data):
    out=bytearray(); i=0
    while i<len(data):
        run=1
        while i+run<len(data) and data[i+run]==data[i] and run<128: run+=1
        if run>=3:
            out.extend((127+run,data[i])); i+=run
        else:
            start=i; i+=run
            while i<len(data) and i-start<128:
                if i+2<len(data) and data[i]==data[i+1]==data[i+2]: break
                i+=1
            out.append(i-start-1); out.extend(data[start:i])
    return bytes(out)
def decode_frames(source):
    raw=subprocess.check_output(['ffmpeg','-v','error','-c:v','libvpx-vp9','-i',str(source),
          '-vf',f'fps={FPS}','-f','rawvideo','-pix_fmt','rgba','-'])
    stride=640*360*4
    if not raw or len(raw)%stride: raise ValueError(f'Invalid 640x360 animation: {source}')
    return [Image.frombytes('RGBA',(640,360),raw[i:i+stride]) for i in range(0,len(raw),stride)]

def bounds(frames):
    boxes=[im.getchannel('A').point(lambda a:255 if a>12 else 0).getbbox() for im in frames]
    boxes=[b for b in boxes if b]
    if not boxes: raise ValueError('Animation has no visible content')
    return (min(b[0] for b in boxes),min(b[1] for b in boxes),
            max(b[2] for b in boxes),max(b[3] for b in boxes))

def shared_layout(boxes):
    # A single source-space viewport and affine transform for every frame of
    # every state. Props/bubbles affect the shared safety margin, never scale
    # an individual character. Source-space feet and centers stay registered.
    box=(max(0,min(b[0] for b in boxes)-8),max(0,min(b[1] for b in boxes)-8),
         min(640,max(b[2] for b in boxes)+8),min(360,max(b[3] for b in boxes)+8))
    width,height=box[2]-box[0],box[3]-box[1]
    scale=min((W-12)/width,(H-8)/height)
    size=(round(width*scale),round(height*scale))
    return dict(mode='shared-source-viewport',source_size=[640,360],crop=list(box),
                scale=scale,sprite_size=list(size),origin=[(W-size[0])//2,(H-size[1])//2])

def compose_frame(im,layout):
    sprite=im.crop(layout['crop']).resize(layout['sprite_size'],Image.Resampling.LANCZOS)
    canvas=Image.new('RGBA',(W,H),(0,0,0,0))
    canvas.paste(sprite,layout['origin'])
    return canvas

def encode_frame(im,palette):
    # 16 alpha levels retain soft edges while suppressing near-transparent codec
    # noise. Fully transparent RGB is irrelevant: use a single index for RLE.
    alpha=im.getchannel('A').point(lambda a: ((a+8)//17)*17).tobytes()
    indexed=bytearray(im.convert('RGB').quantize(palette=palette,dither=Image.Dither.NONE).tobytes())
    for i,a in enumerate(alpha):
        if not a: indexed[i]=0
    colors=pack(indexed)
    return struct.pack('<I',len(colors))+colors+pack(alpha)

def main():
    parser=argparse.ArgumentParser(); parser.add_argument('--source',type=Path,default=ROOT/'.deps/dsh-whale-pet')
    args=parser.parse_args(); dest=ROOT/'firmware/assets'; dest.mkdir(exist_ok=True)
    source_boxes=[bounds(decode_frames(args.source/'assets/thumb'/f'{clip}.webm')) for _,clip in CLIPS]
    layout=shared_layout(source_boxes)
    print('Shared layout:',layout,flush=True)
    blob=bytearray(); descriptions=[]; declarations=[]
    for state,clip in CLIPS:
        source=args.source/'assets/thumb'/f'{clip}.webm'
        frames=decode_frames(source)
        canvases=[compose_frame(im,layout) for im in frames]
        samples=canvases[::max(1,len(canvases)//12)]
        montage=Image.new('RGB',(W,H*len(samples)))
        for i,im in enumerate(samples): montage.paste(im,(0,H*i))
        palette=montage.quantize(colors=256,method=Image.Quantize.MEDIANCUT)
        colors=palette.getpalette()
        palette_offset=len(blob)
        for i in range(256):
            r,g,b=colors[i*3:i*3+3]; blob.extend(struct.pack('<H',((r>>3)<<11)|((g>>2)<<5)|(b>>3)))
        offsets=[]
        for im in canvases:
            offsets.append(len(blob)); blob.extend(encode_frame(im,palette))
        offsets.append(len(blob))
        declarations.append(f'static const uint32_t frames_{state}[] = {{'+','.join(map(str,offsets))+'};')
        descriptions.append(dict(state=state,clip=clip,frames=len(frames),fps=FPS,palette=palette_offset,
                                 source_sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
        print(state,len(frames),'frames; total',len(blob),'bytes',flush=True)
    (dest/'whale.bin').write_bytes(blob)
    header='#ifndef WHALE_ASSETS_H\n#define WHALE_ASSETS_H\n#include <stdint.h>\n'
    header+=f'#define WHALE_WIDTH {W}\n#define WHALE_HEIGHT {H}\n#define WHALE_FPS {FPS}\n'
    header+='extern const uint8_t whale_blob[], whale_blob_end[];\n'
    header+='typedef struct { uint32_t palette; const uint32_t *frames; unsigned count; const char *name; } whale_clip;\n'
    header+='\n'.join(declarations)+'\nstatic const whale_clip whale_clips[] = {\n'
    for d in descriptions: header+=f'  {{{d["palette"]}, frames_{d["state"]}, {d["frames"]}, "{d["clip"].split("/")[-1]}"}},\n'
    header+='};\n#endif\n'; (dest/'whale.h').write_text(header)
    (dest/'manifest.json').write_text(json.dumps(dict(source_commit=subprocess.check_output(['git','-C',str(args.source),'rev-parse','HEAD'],text=True).strip(),
        format="WHP2-RGB565-A8",alpha_levels=16,width=W,height=H,fps=FPS,layout=layout,bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),clips=descriptions),indent=2)+'\n')
if __name__=='__main__': main()
