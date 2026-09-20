#!/usr/bin/env bash
# Build (if needed) and run the designer.
set -euo pipefail
cd "$(dirname "$0")"
cmake -S . -B build -G Ninja >/dev/null
cmake --build build >/dev/null
exec ./build/hui "$@"
