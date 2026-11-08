#!/usr/bin/env bash
# Build the Linux desktop bundle.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLCHAINS="${TOOLCHAINS:-$HOME/toolchains}"
FLUTTER_BIN="${FLUTTER_BIN:-$TOOLCHAINS/flutter-sdk/bin}"
MODE="${MODE:-release}"

# 1. Native engine
"$ROOT/scripts/build_core.sh"

# 2. GTK development sysroot (no root required)
if [[ ! -f "$TOOLCHAINS/gtk-sysroot/usr/lib64/pkgconfig/gtk+-3.0.pc" ]]; then
  "$ROOT/scripts/setup_gtk_sysroot.sh"
fi

export PATH="$TOOLCHAINS/bin:$FLUTTER_BIN:$PATH"
export PKG_CONFIG="${PKG_CONFIG:-$TOOLCHAINS/bin/pkg-config}"
export GTK_SYSROOT="$TOOLCHAINS/gtk-sysroot"

cd "$ROOT/app"
if ! flutter pub get --offline; then
  echo "Offline pub get failed; retrying online..." >&2
  flutter pub get
fi
flutter build linux "--$MODE" --no-pub

BUNDLE="$ROOT/app/build/linux/x64/$MODE/bundle"

# Ship the tiny Julia bridge source next to the executable.  The Julia runtime
# itself is never bundled: if `julia` is not installed the app silently uses the
# C++ core.
mkdir -p "$BUNDLE/julia/src"
cp "$ROOT/julia/Project.toml" "$BUNDLE/julia/"
cp "$ROOT/julia/src/"*.jl "$BUNDLE/julia/src/"

echo
echo "Bundle: $BUNDLE"
echo "Run:    $BUNDLE/infinit"
