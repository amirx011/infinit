#!/usr/bin/env bash
# Package the release Linux bundle as a relocatable tar.gz.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="${MODE:-release}"
VERSION="$(sed -n 's/^version: \([^+]*\).*/\1/p' "$ROOT/app/pubspec.yaml")"
BUNDLE="$ROOT/app/build/linux/x64/$MODE/bundle"
OUT="$ROOT/dist"

if [[ ! -d "$BUNDLE" ]]; then
  echo "Bundle not found: $BUNDLE" >&2
  echo "Run scripts/build_linux.sh first." >&2
  exit 1
fi

mkdir -p "$OUT"
cp "$ROOT/README.md" "$ROOT/LICENSE" "$BUNDLE/"
ARCHIVE="$OUT/infinit-lab-$VERSION-linux-x64.tar.gz"
tar -czf "$ARCHIVE" -C "$BUNDLE" .

# Keep a ready-to-publish copy in release/ (tracked by Git).
mkdir -p "$ROOT/release"
cp "$ARCHIVE" "$ROOT/release/"
echo "$ARCHIVE"
echo "$ROOT/release/$(basename "$ARCHIVE")"
