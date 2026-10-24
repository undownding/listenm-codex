#!/usr/bin/env bash
set -e
PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
SDK_ROOT=${SDK_ROOT:-"$PROJECT_ROOT/.deps/arcs_mini"}
if [ "${PET_BOARD:-arcs_mini}" != arcs_mini ]; then
  echo 'Only arcs_mini currently has a validated voice AP/CP layout; arcs_mini3 voice is not supported.' >&2
  exit 1
fi
set +e
source "$SDK_ROOT/arcs-sdk/env.sh" ""
setup_status=$?
set -e
if [ "$setup_status" -ne 0 ]; then exit "$setup_status"; fi
bash "$SDK_ROOT/build.sh" -S "$PROJECT_ROOT/firmware" -B "$PROJECT_ROOT/build/voice" \
  -DBOARD=arcs_mini "-DCONFIG_FILES=prj.conf;voice.conf" \
  "$@"
python3 "$PROJECT_ROOT/scripts/pack-voice-image.py" "$SDK_ROOT" "$PROJECT_ROOT/build/voice"
