#!/usr/bin/env bash
# Build the native exact-ODE engine.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

BUILD_DIR="${BUILD_DIR:-$ROOT/core/build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

cmake -S "$ROOT/core" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DODE_BUILD_TESTS="${ODE_BUILD_TESTS:-ON}"
cmake --build "$BUILD_DIR" -j "${JOBS:-$(nproc)}"

echo
echo "Core built:"
echo "  static:  $BUILD_DIR/libodecore.a"
echo "  shared:  $BUILD_DIR/lib/libodecore.so"
