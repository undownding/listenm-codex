#!/usr/bin/env bash
set -e
PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
SDK_ROOT=${SDK_ROOT:-"$PROJECT_ROOT/.deps/arcs_mini"}
: "${PET_BRIDGE_IP:?Set PET_BRIDGE_IP to a host IPv4 address reachable by Lisem (not loopback)}"
: "${BRIDGE_TOKEN:?Set BRIDGE_TOKEN for the bridge and firmware}"
# SDK environment probing uses expected nonzero return codes internally.
set +e
source "$SDK_ROOT/arcs-sdk/env.sh" ""
setup_status=$?
set -e
if [ "$setup_status" -ne 0 ]; then exit "$setup_status"; fi
bash "$SDK_ROOT/build.sh" -S "$PROJECT_ROOT/firmware" -B "$PROJECT_ROOT/build/firmware" \
  -DBOARD=arcs_mini "-DPET_BRIDGE_IP=$PET_BRIDGE_IP" \
  "-DPET_BRIDGE_PORT=${BRIDGE_PORT:-8787}" "-DPET_BRIDGE_TOKEN=$BRIDGE_TOKEN" \
  "-DPET_WIFI_SSID=${PET_WIFI_SSID:-Lisem}" "-DPET_WIFI_PASSWORD=${PET_WIFI_PASSWORD:-}" "$@"
