#!/usr/bin/env python3
"""Build a small antialiased status-label atlas from Noto Sans CJK SC."""
import argparse
from pathlib import Path
import struct
from PIL import Image, ImageDraw, ImageFont
ROOT=Path(__file__).resolve().parents[1]
LABELS=['待机','思考中','工作中','回应中','等待确认','小憩中','出错了','离线','互动','彩蛋','聆听中','CODEX']

def pack(source, index):
    body=bytearray(b'PTF1'+struct.pack('<III',len(LABELS),96,20))
    for label in LABELS:
        font=ImageFont.truetype(str(source),14 if label=='CODEX' else 18,index=index)
        image=Image.new('L',(96,20)); draw=ImageDraw.Draw(image)
        box=draw.textbbox((0,0),label,font=font)
        draw.text(((96-(box[2]-box[0]))//2-box[0],(20-(box[3]-box[1]))//2-box[1]),label,font=font,fill=255)
        body.extend(image.tobytes())
    (ROOT/'firmware/assets/status-font.bin').write_bytes(body)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('font',type=Path)
    parser.add_argument('--index',type=int,default=0,help='TTC face index; SC is 2 in Noto CJK OTC')
    args=parser.parse_args(); pack(args.font,args.index)
