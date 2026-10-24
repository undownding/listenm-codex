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
The blob lives on the TF card. No video decoding is needed on-device; extra packs also live on TF.

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

## TF card resources

The FAT-formatted TF card is required. The firmware mounts it at `/SD:` and never
formats or writes it. Use `python3 scripts/prepare-tf.py /mount/path` to
copy resources. Edit `config.ini`
before boot; an existing configuration is preserved by the preparation script.

| Card file | Format |
| --- | --- |
| `status-font.bin` | PTF1 atlas: 12 antialiased 96×20 status/title labels |
| `config.ini` | UTF-8 key=value runtime configuration; see `config/config.example.ini` |
| `background.rgb565` | 240×240 little-endian RGB565, exactly 115,200 bytes |
| `whale.bin` | 12 FPS state animations, matching compiled `whale.h` offsets |
| `click.bin` | 12 FPS button animations, matching compiled `click.h` offsets |
| `extras/*.bin` | Self-contained WHP2 packs selected by bridge `/pet/next-id` |
| `extras/catalog.json` | Reference catalog copied with the packs |
| `voice/algo.bin`, `voice/wrap.json` | SDK wake model and parameters; paths configurable |

Core animation offsets remain compiled metadata. Repacking core assets requires
matching headers and firmware; ordinary configuration changes do not. The single voice firmware uses full-rate assets.

The background is cached once; core and click frames stream from TF. Extra packs
are loaded one at a time, with a 1.75 MB cap. Wake resources are kept in PSRAM for
the lifetime of the voice algorithm. AP/CP executable code remains in Flash.

Missing configuration stops normal startup. Missing, truncated or malformed mandatory
resources stop animation playback and display the built-in `resource-missing.png`
screen, with a UART diagnostic. This emergency screen is the only embedded image;
its Chinese text is self-contained and does not need the TF font or background. A missing extra pack is skipped. A corrupt frame is decoded
atomically. Insert/replace files before boot and restart; hot swaps are unsupported.

## Status font

Chinese labels use Noto Sans CJK SC Medium at 18 px, with 8-bit antialiasing;
CODEX uses the same face at 14 px. The atlas contains 待机、思考中、工作中、回应中、
等待确认、小憩中、出错了、离线、互动、彩蛋、聆听中 and CODEX. Labels are
centered in a 96×20 cell and blended over the background in navy RGB565.
The title and status cells have an 8 px outer margin at the top and bottom;
the listening meter ends 5 px above the status cell.

Upstream: https://github.com/notofonts/noto-cjk/tree/main/Sans
License: `licenses/noto-sans-cjk-OFL.txt` (SIL Open Font License 1.1).
Generated from Noto Sans CJK Medium OTC v2.004 (© 2014–2021 Adobe), simplified Chinese face (index 2).
The resource is a raster subset named `status-font.bin`, not a renamed font family.

```bash
python3 scripts/pack-font.py /usr/share/fonts/opentype/noto/NotoSansCJK-Medium.ttc --index 2
# Or use upstream NotoSansCJKsc-Medium.otf with the default index 0.
```

The generated resource is committed; normal builds do not require system fonts.

## Missing-resource screen

`resource-missing.png` is a 240×240 status screen with a TF-card pictogram and the
Chinese message “资源缺失 / 请检查 TF 卡 / 补齐资源后重启”. Its generated
`resource_missing.h` contains about 13 KiB of RGB565 run-length data compiled into
the firmware. It is displayed for missing TF/configuration, background, core/click
packs or status font, and on mandatory frame decode/read failures. Optional extra
packs remain skippable. Restart after restoring the files.

```bash
python3 scripts/pack-resource-error.py /usr/share/fonts/opentype/noto/NotoSansCJK-Medium.ttc --index 2
```

Text uses the same Noto Sans CJK SC font and OFL license as the status atlas.
