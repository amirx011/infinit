#!/usr/bin/env bash
# Install Infinit Lab for the current user (no root required).
#
#   ./scripts/install_linux.sh
#   ./scripts/install_linux.sh dist/infinit-lab-1.0.0-linux-x64.tar.gz
#   ./scripts/install_linux.sh --uninstall
#
# Installs into ~/.local (override with PREFIX=/some/prefix).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX="${PREFIX:-$HOME/.local}"
APP_DIR="$PREFIX/share/infinit"
BIN_DIR="$PREFIX/bin"
DESKTOP_DIR="$PREFIX/share/applications"
ICON_ROOT="$PREFIX/share/icons/hicolor"
ICON_SCALABLE_DIR="$ICON_ROOT/scalable/apps"
APP_ID="ir.infinit"
DESKTOP_FILE="$DESKTOP_DIR/$APP_ID.desktop"
LAUNCHER="$BIN_DIR/infinit"

# Icon sizes shipped in app/assets/branding.
ICON_SIZES=(16 24 32 48 64 96 128 192 256 512)

refresh_caches() {
  command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$DESKTOP_DIR" >/dev/null 2>&1 || true
  command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -f -t "$ICON_ROOT" >/dev/null 2>&1 || true
}

uninstall() {
  rm -rf "$APP_DIR"
  rm -f "$LAUNCHER" "$DESKTOP_FILE" "$DESKTOP_DIR/infinit.desktop" \
        "$ICON_SCALABLE_DIR/$APP_ID.svg" "$ICON_ROOT/scalable/apps/infinit.svg"
  for size in "${ICON_SIZES[@]}"; do
    rm -f "$ICON_ROOT/${size}x${size}/apps/$APP_ID.png"
  done
  refresh_caches
  echo "Infinit Lab removed from $PREFIX."
}

if [[ "${1:-}" == "--uninstall" ]]; then
  uninstall
  exit 0
fi

SOURCE="${1:-}"
TMP=""
cleanup() { [[ -n "$TMP" && -d "$TMP" ]] && rm -rf "$TMP" || true; }
trap cleanup EXIT

if [[ -n "$SOURCE" && -f "$SOURCE" && "$SOURCE" == *.tar.gz ]]; then
  TMP="$(mktemp -d)"
  tar -xzf "$SOURCE" -C "$TMP"
  BUNDLE="$TMP"
elif [[ -n "$SOURCE" && -d "$SOURCE" ]]; then
  BUNDLE="$SOURCE"
elif [[ -d "$ROOT/app/build/linux/x64/release/bundle" ]]; then
  BUNDLE="$ROOT/app/build/linux/x64/release/bundle"
else
  cat >&2 <<EOF
No Linux bundle found.

Build it first:
  ./scripts/build_linux.sh

or pass the packaged archive:
  ./scripts/install_linux.sh dist/infinit-lab-1.0.0-linux-x64.tar.gz
EOF
  exit 1
fi

if [[ ! -x "$BUNDLE/infinit" ]]; then
  echo "Invalid bundle (infinit not found): $BUNDLE" >&2
  exit 1
fi

echo "Installing into $APP_DIR"
rm -rf "$APP_DIR"
mkdir -p "$APP_DIR" "$BIN_DIR" "$DESKTOP_DIR" "$ICON_SCALABLE_DIR"
cp -a "$BUNDLE/." "$APP_DIR/"

cat > "$LAUNCHER" <<EOF
#!/usr/bin/env bash
exec "$APP_DIR/infinit" "\$@"
EOF
chmod +x "$LAUNCHER"

# Install icons from the bundle (not from the source tree) so a packaged
# archive is self-contained.  The scalable SVG is the fallback; the raster
# sizes give the taskbar and app switcher a crisp icon without downscaling.
ICON_SOURCE="$APP_DIR/data/flutter_assets/assets/branding"
if [[ -f "$ICON_SOURCE/infinit-icon.svg" ]]; then
  cp "$ICON_SOURCE/infinit-icon.svg" "$ICON_SCALABLE_DIR/$APP_ID.svg"
elif [[ -f "$ROOT/app/assets/branding/infinit-icon.svg" && "$BUNDLE" == "$ROOT/app/build/linux/x64/release/bundle" ]]; then
  cp "$ROOT/app/assets/branding/infinit-icon.svg" "$ICON_SCALABLE_DIR/$APP_ID.svg"
fi
for size in "${ICON_SIZES[@]}"; do
  src="$ICON_SOURCE/infinit-icon-$size.png"
  [[ -f "$src" ]] || continue
  dir="$ICON_ROOT/${size}x${size}/apps"
  mkdir -p "$dir"
  cp "$src" "$dir/$APP_ID.png"
done

cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Type=Application
Name=Infinit Lab
Name[fa]=Infinit Lab
Comment=Exact symbolic calculus and differential equations
Comment[fa]=حساب دیفرانسیل و انتگرال و معادلات دیفرانسیل دقیق
Exec=$LAUNCHER
Icon=$APP_ID
Terminal=false
Categories=Education;Math;Science;
Keywords=ode;differential;equation;wronskian;envelope;
StartupWMClass=$APP_ID
EOF

refresh_caches

# Report missing runtime libraries (the GTK development headers are only
# needed at build time; the runtime GTK libraries are normally installed).
missing="$(ldd "$APP_DIR/infinit" "$APP_DIR/lib/libodecore.so" 2>/dev/null | grep 'not found' || true)"
if [[ -n "$missing" ]]; then
  echo
  echo "Warning: some runtime libraries are missing:" >&2
  echo "$missing" >&2
  echo "Install the GTK3 runtime packages with your distribution." >&2
fi

echo
echo "Installed:"
echo "  app:      $APP_DIR"
echo "  command:  $LAUNCHER"
echo "  menu:     $DESKTOP_FILE"
echo
if [[ ":$PATH:" != *":$BIN_DIR:"* ]]; then
  echo "Add $BIN_DIR to PATH to run 'infinit' from a terminal:"
  echo "  export PATH=\"$BIN_DIR:\$PATH\""
else
  echo "Run it with: infinit"
fi
