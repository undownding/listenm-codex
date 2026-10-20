# Whale animation pack

These converted animation assets come from
[luweiyabo/dsh-whale-pet](https://github.com/luweiyabo/dsh-whale-pet), commit
`4c9ad7ff8c8ad640a827525b3512ca544572e9ca`, `assets/thumb/`.
The original upstream media is attributed to **PC2005-cloud/dsh-pet contributors**.

The upstream project's MIT code license does **not** relicense these media files.
Its resource terms permit use in open-source projects and prohibit commercial use.
Preserve this attribution and the [upstream media notice](../../licenses/whale-assets-upstream.md).
See the [original resource terms](https://github.com/PC2005-cloud/dsh-pet#license).
This applies equally to `whale.bin` and images/videos derived from it.

`whale.bin`: 7 clips × 120 frames, 12 FPS, 208×196 pixels, 10 seconds per loop.
Each clip has a 256-entry little-endian RGB565 palette followed by indexed
frames with a 4-byte little-endian RGB-plane length, a PackBits palette-index plane, then a PackBits alpha plane (16 alpha levels, stored as 0,17,...,255). For each control byte `c < 128`, copy the next `c+1` indices;
otherwise repeat the following index `c-127` times. `whale.h` supplies frame
boundaries; the decoder checks all lengths and requires exactly 40,768 pixels.
One decoded sprite plus the full display buffer uses 196,736 bytes of PSRAM.
The blob stays in Flash. No video decoding is needed on-device; optional extra packs are downloaded separately.

Rebuild with FFmpeg including `libvpx-vp9` and Pillow (tested Pillow 12.1.1):

```bash
git clone https://github.com/luweiyabo/dsh-whale-pet .deps/dsh-whale-pet
git -C .deps/dsh-whale-pet checkout 4c9ad7ff8c8ad640a827525b3512ca544572e9ca
python3 scripts/pack-whale.py
```

Conversion decodes transparency, uses a shared source-space viewport across all clips, applies one fixed scale
and centered viewport placement, retains source alpha without baking a background, and uses one
palette across each clip. Large effects therefore never shrink only their own
character. The shared crop, scale, dimensions and origin are recorded in the
manifest. All states retain the same registration; natural changes of pose
within the original animation are preserved.
Original aspect ratios and frame timing are preserved; frame rate is sampled at
12 FPS. Movement, turning, dragging, throwing, and roaming clips are not included.
`manifest.json` records all source hashes and the resulting blob hash.

`background.rgb565` is the generated underwater backdrop; its provenance and source PNG are in `assets/backgrounds/`. The decoder blends each frame over this background, preserving opaque white costume pixels and semitransparent edges. `click.bin` uses the same WHP2 format.
