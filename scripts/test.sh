#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
node --test bridge/test/*.test.js
python3 -m unittest discover -s scripts/tests
cmake -S firmware -B build/host -DLISTENM_HOST=ON
cmake --build build/host
ctest --test-dir build/host --output-on-failure
