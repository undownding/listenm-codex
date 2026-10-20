#!/usr/bin/env bash
# Deterministic device-size and RGB565 encoding of the generated source artwork.
set -euo pipefail
cd "$(dirname "$0")/.."
ffmpeg -v error -y -i assets/backgrounds/whale-undersea.png \
  -vf scale=240:240:flags=lanczos -frames:v 1 assets/backgrounds/whale-undersea-240.png
ffmpeg -v error -y -i assets/backgrounds/whale-undersea-240.png \
  -frames:v 1 -f rawvideo -pix_fmt rgb565le firmware/assets/background.rgb565
