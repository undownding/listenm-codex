#!/usr/bin/env bash
set -euo pipefail
PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$PROJECT_ROOT"
mkdir -p .deps
SDK_COMMIT=23fca91e13dbd0f2c81eb2f69a6b337953021637
if [ ! -d .deps/arcs_mini ]; then
  git clone --no-checkout --depth 1 https://cloud.listenai.com/CSKG836746/arcs-sdk/public/arcs_mini.git .deps/arcs_mini
  git -C .deps/arcs_mini fetch --depth 1 origin "$SDK_COMMIT"
  git -C .deps/arcs_mini checkout --detach "$SDK_COMMIT"
fi
if [ "$(git -C .deps/arcs_mini rev-parse HEAD)" != "$SDK_COMMIT" ]; then
  echo 'SDK checkout differs from the tested commit; use a separate checkout.' >&2
  exit 1
fi
if [ "$(uname -s)-$(uname -m)" != Linux-x86_64 ]; then
  echo 'SDK ready. Install the matching Lisem release for this host manually.'
  exit 0
fi
if [ ! -x .deps/Lisem/lisem ]; then
  curl -fL https://github.com/LISTENAI/Lisem/releases/download/v0.8.1/Lisem-linux-x86_64.tar.gz -o .deps/lisem.tar.gz
  echo '218172c43cefd815fb6d5109f98d376f6d5447f365fb46c489deb47e51400909  .deps/lisem.tar.gz' | sha256sum -c -
  tar -xzf .deps/lisem.tar.gz -C .deps
fi
printf '%s\n' 'Dependencies ready. build-firmware.sh uses the SDK environment installer for the toolchain.'
