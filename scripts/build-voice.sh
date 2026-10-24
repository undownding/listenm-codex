#!/usr/bin/env bash
# Compatibility alias: the only firmware is the voice firmware.
set -e
exec "$(dirname "$0")/build-firmware.sh" "$@"
