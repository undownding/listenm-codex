# Whale undersea background

Generated with OpenAI image generation for this project on 2026-10-20.

- `whale-undersea.png`: original 1254×1254 RGB artwork.
- `whale-undersea-240.png`: 240×240 display-sized preview.
- `../../firmware/assets/background.rgb565`: 115,200-byte little-endian RGB565 image, row-major, unrotated. Device rotation must be applied by the display renderer.
- Rebuild the display resources using `scripts/pack-background.sh` (requires FFmpeg).

Direction: gentle pastel underwater alcove, pale blue/lavender/turquoise, soft light from upper left, coral and shells at the outer bottom corners, quiet central space for the blue-haired whale companion, no character or text baked into the background. Top and bottom remain readable for status labels.

These files are project source assets, not temporary build output. The RGB565 background is linked into the firmware. Animation assets retain their source alpha, quantized to 16 levels, and are blended over the background before status labels and display rotation. White costume pixels are preserved; no color-key removal is used.
