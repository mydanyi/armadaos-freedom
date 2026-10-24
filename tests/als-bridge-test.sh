#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/als-bridge-test.py
python3 tests/als-bridge-dbus-test.py
