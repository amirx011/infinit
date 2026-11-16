#!/usr/bin/env bash
# Build the Linux desktop bundle.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLCHAINS="${TOOLCHAINS:-$HOME/toolchains}"
FLUTTER_BIN="${FLUTTER_BIN:-$TOOLCHAINS/flutter-sdk/bin}"
MODE="${MODE:-release}"

# 1. Native engine
"$ROOT/scripts/build_core.sh"

# 2. GTK development files.
#    Fedora ships the GTK runtime without the -devel packages, so the local flow
#    builds a private sysroot.  Debian, Ubuntu and the CI runner install
#    libgtk-3-dev instead, and the system pkg-config already knows about it.
PKG_CONFIG="${PKG_CONFIG:-}"
if [[ -z "$PKG_CONFIG" ]]; then
  if [[ -x "$TOOLCHAINS/bin/pkg-config" ]]; then
    PKG_CONFIG="$TOOLCHAINS/bin/pkg-config"
  else
    PKG_CONFIG="pkg-config"
  fi
fi
if ! "$PKG_CONFIG" --exists gtk+-3.0 2>/dev/null; then
  "$ROOT/scripts/setup_gtk_sysroot.sh"
  PKG_CONFIG="$TOOLCHAINS/bin/pkg-config"
fi

export PATH="$TOOLCHAINS/bin:$FLUTTER_BIN:$PATH"
export PKG_CONFIG
export GTK_SYSROOT="${GTK_SYSROOT:-$TOOLCHAINS/gtk-sysroot}"

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
