#!/usr/bin/env bash
set -euo pipefail
PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
SDK_ROOT=${SDK_ROOT:-"$PROJECT_ROOT/.deps/arcs_mini"}
# Build a private source copy: this SDK snapshot omits its ILM linker snippet.
BOOT_SOURCE="$PROJECT_ROOT/build/mini3-boot-source"
mkdir -p "$BOOT_SOURCE"
cp -R "$SDK_ROOT/arcs-sdk/system/uboot/." "$BOOT_SOURCE/"
cp "$PROJECT_ROOT/firmware/boot-ilm-noinit.ld" "$BOOT_SOURCE/cmake/boot_ilm_noinit.ld"
python3 "$PROJECT_ROOT/scripts/prepare-mini3-boot.py" "$BOOT_SOURCE" "$SDK_ROOT/arcs-sdk"
bash "$SDK_ROOT/build.sh" -S "$BOOT_SOURCE" -B "$PROJECT_ROOT/build/mini3-recovery" \
  -DBOARD=arcs_mini3 "-DCONFIG_FILES=$PROJECT_ROOT/firmware/mini3-boot.conf"
