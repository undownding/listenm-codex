#!/usr/bin/env bash
set -e
PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
SDK_ROOT=${SDK_ROOT:-"$PROJECT_ROOT/.deps/arcs_mini"}
: "${PET_BRIDGE_IP:?Set PET_BRIDGE_IP}"
: "${BRIDGE_TOKEN:?Set BRIDGE_TOKEN}"
set +e
source "$SDK_ROOT/arcs-sdk/env.sh" ""
setup_status=$?
set -e
if [ "$setup_status" -ne 0 ]; then exit "$setup_status"; fi
bash "$SDK_ROOT/build.sh" -S "$PROJECT_ROOT/firmware" -B "$PROJECT_ROOT/build/voice" \
  -DBOARD=arcs_mini -DPET_VOICE=ON "-DCONFIG_FILES=prj.conf;voice.conf" \
  "-DPET_BRIDGE_IP=$PET_BRIDGE_IP" "-DPET_BRIDGE_TOKEN=$BRIDGE_TOKEN" \
  "-DPET_BRIDGE_PORT=${BRIDGE_PORT:-8787}" "-DPET_WIFI_SSID=${PET_WIFI_SSID:-Lisem}" \
  "-DPET_WIFI_PASSWORD=${PET_WIFI_PASSWORD:-}" "$@"
python3 "$PROJECT_ROOT/scripts/pack-voice-image.py" "$SDK_ROOT" "$PROJECT_ROOT/build/voice"
