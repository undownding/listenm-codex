#!/usr/bin/env bash
set -e
PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
SDK_ROOT=${SDK_ROOT:-"$PROJECT_ROOT/.deps/arcs_mini"}
BOARD=${PET_BOARD:-arcs_mini}
case "$BOARD" in
  arcs_mini) BUILD_DIR="$PROJECT_ROOT/build/voice"; CONFIG_FILES='prj.conf;voice.conf' ;;
  arcs_mini3) BUILD_DIR="$PROJECT_ROOT/build/voice-mini3"; CONFIG_FILES='prj.conf;voice.conf;mini3.conf' ;;
  *) echo "Unsupported board: $BOARD" >&2; exit 1 ;;
esac
set +e
source "$SDK_ROOT/arcs-sdk/env.sh" ""
setup_status=$?
set -e
if [ "$setup_status" -ne 0 ]; then exit "$setup_status"; fi
if [ "$BOARD" = arcs_mini3 ]; then
  bash "$PROJECT_ROOT/scripts/build-mini3-boot.sh"
fi
bash "$SDK_ROOT/build.sh" -S "$PROJECT_ROOT/firmware" -B "$BUILD_DIR" \
  "-DBOARD=$BOARD" "-DCONFIG_FILES=$CONFIG_FILES" \
  "$@"
python3 "$PROJECT_ROOT/scripts/pack-voice-image.py" "$SDK_ROOT" "$BUILD_DIR" "$BOARD"
