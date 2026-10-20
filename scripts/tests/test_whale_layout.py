import importlib.util
from pathlib import Path
import unittest
from PIL import Image, ImageDraw, ImageChops

spec=importlib.util.spec_from_file_location('pack_whale',Path(__file__).parents[1]/'pack-whale.py')
pack=importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)

class SharedLayoutTest(unittest.TestCase):
    def test_large_effect_does_not_resize_or_reposition_character(self):
        idle=Image.new('RGBA',(640,360))
        ImageDraw.Draw(idle).rectangle((220,70,420,330),fill=(20,80,180,255))
        thinking=idle.copy()
        ImageDraw.Draw(thinking).rectangle((95,80,150,120),fill=(240,40,20,255))
        layout=pack.shared_layout([pack.bounds([idle]),pack.bounds([thinking])])
        a,b=pack.compose_frame(idle,layout),pack.compose_frame(thinking,layout)
        # The additional thinking effect must not change the character's
        # bounding box or pixels, as the old per-clip normalization did.
        def blue_bounds(image):
            mask=Image.new('L',image.size)
            mask.putdata([255 if b>r*2 and b>g*1.5 else 0 for r,g,b,a in image.getdata()])
            return mask.getbbox()
        box=blue_bounds(a)
        self.assertIsNotNone(box)
        self.assertEqual(box,blue_bounds(b))
        self.assertIsNone(ImageChops.difference(a.crop(box),b.crop(box)).getbbox())
        self.assertIsNotNone(ImageChops.difference(a,b).convert("RGB").getbbox())
        self.assertEqual(a.size,(pack.W,pack.H))

    def test_composition_keeps_white_and_partial_alpha(self):
        source=Image.new('RGBA',(640,360),(0,0,0,0))
        source.putpixel((10,10),(255,255,255,255))
        source.putpixel((11,10),(255,255,255,128))
        layout=dict(crop=[0,0,208,196],sprite_size=[208,196],origin=[0,0])
        frame=pack.compose_frame(source,layout)
        self.assertEqual(frame.getpixel((10,10)),(255,255,255,255))
        self.assertEqual(frame.getpixel((11,10)),(255,255,255,128))
        self.assertEqual(frame.getpixel((0,0))[3],0)

    def test_shared_crop_contains_every_clip(self):
        boxes=[(196,59,436,331),(92,53,552,336),(206,29,430,331)]
        layout=pack.shared_layout(boxes)
        l,t,r,b=layout['crop']
        for x0,y0,x1,y1 in boxes:
            self.assertLessEqual(l,x0); self.assertLessEqual(t,y0)
            self.assertGreaterEqual(r,x1); self.assertGreaterEqual(b,y1)
        x,y=layout['origin']; w,h=layout['sprite_size']
        self.assertGreaterEqual(x,0); self.assertGreaterEqual(y,0)
        self.assertLessEqual(x+w,pack.W); self.assertLessEqual(y+h,pack.H)

if __name__=='__main__': unittest.main()
