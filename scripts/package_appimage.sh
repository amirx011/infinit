#!/usr/bin/env bash
# Package the release Linux bundle as a single-file AppImage.
#
#   scripts/package_appimage.sh
#
# The bundle keeps its Flutter layout inside usr/lib/infinit, because the
# runner looks for lib/ and data/ next to the executable.  AppRun hands over to
# that binary, so nothing has to be moved around at start time.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="${MODE:-release}"
VERSION="$(sed -n 's/^version: \([^+]*\).*/\1/p' "$ROOT/app/pubspec.yaml")"
BUNDLE="$ROOT/app/build/linux/x64/$MODE/bundle"
WORK="$ROOT/dist/appimage"
APPDIR="$WORK/InfinitLab.AppDir"
ICON="$ROOT/app/assets/branding/infinit-icon-256.png"
DESKTOP="$ROOT/scripts/linux/infinit.desktop"

[[ -n "$VERSION" ]] || { echo "Could not read the version from pubspec.yaml." >&2; exit 1; }
if [[ ! -x "$BUNDLE/infinit" ]]; then
  echo "Bundle not found: $BUNDLE" >&2
  echo "Run scripts/build_linux.sh first." >&2
  exit 1
fi

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/lib/infinit" "$APPDIR/usr/share/applications" \
  "$APPDIR/usr/share/icons/hicolor/256x256/apps"
cp -a "$BUNDLE/." "$APPDIR/usr/lib/infinit/"
install -m644 "$DESKTOP" "$APPDIR/usr/share/applications/infinit.desktop"
install -m644 "$ICON" "$APPDIR/usr/share/icons/hicolor/256x256/apps/infinit.png"

# appimagetool reads the entry and the icon from the AppDir root.
cp "$DESKTOP" "$APPDIR/infinit.desktop"
cp "$ICON" "$APPDIR/infinit.png"
cat > "$APPDIR/AppRun" <<'LAUNCHER'
#!/bin/sh
HERE="${APPDIR:-$(dirname "$(readlink -f "$0")")}"
exec "$HERE/usr/lib/infinit/infinit" "$@"
LAUNCHER
chmod 755 "$APPDIR/AppRun"

TOOL="${APPIMAGETOOL:-}"
if [[ -z "$TOOL" ]]; then
  CACHE="$WORK/tool"
  mkdir -p "$CACHE"
  curl -fsSL -o "$CACHE/appimagetool" \
    "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
  chmod +x "$CACHE/appimagetool"
  # FUSE is not available everywhere, so unpack the tool and call it directly.
  ( cd "$CACHE" && ./appimagetool --appimage-extract > /dev/null )
  TOOL="$CACHE/squashfs-root/AppRun"
fi

OUT="$ROOT/dist/infinit-lab-$VERSION-x86_64.AppImage"
rm -f "$OUT"
ARCH=x86_64 "$TOOL" "$APPDIR" "$OUT"
chmod 755 "$OUT"

mkdir -p "$ROOT/release"
cp "$OUT" "$ROOT/release/"
printf '  %-16s %s\n' "AppImage" "$(du -h "$OUT" | cut -f1)  $(basename "$OUT")"
