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
APP_DIR="$PREFIX/share/ode-lab"
BIN_DIR="$PREFIX/bin"
DESKTOP_DIR="$PREFIX/share/applications"
ICON_DIR="$PREFIX/share/icons/hicolor/scalable/apps"
LAUNCHER="$BIN_DIR/ode-lab"

uninstall() {
  rm -rf "$APP_DIR"
  rm -f "$LAUNCHER" "$DESKTOP_DIR/ode-lab.desktop" "$ICON_DIR/ode-lab.svg"
  command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$DESKTOP_DIR" >/dev/null 2>&1 || true
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

if [[ ! -x "$BUNDLE/ode_lab" ]]; then
  echo "Invalid bundle (ode_lab not found): $BUNDLE" >&2
  exit 1
fi

echo "Installing into $APP_DIR"
rm -rf "$APP_DIR"
mkdir -p "$APP_DIR" "$BIN_DIR" "$DESKTOP_DIR" "$ICON_DIR"
cp -a "$BUNDLE/." "$APP_DIR/"

cat > "$LAUNCHER" <<EOF
#!/usr/bin/env bash
exec "$APP_DIR/ode_lab" "\$@"
EOF
chmod +x "$LAUNCHER"

cp "$ROOT/app/assets/ode-lab.svg" "$ICON_DIR/ode-lab.svg"

cat > "$DESKTOP_DIR/ode-lab.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Infinit Lab
Name[fa]=Infinit Lab
Comment=Exact symbolic calculus and differential equations
Comment[fa]=حساب دیفرانسیل و انتگرال و معادلات دیفرانسیل دقیق
Exec=$LAUNCHER
Icon=ode-lab
Terminal=false
Categories=Education;Math;Science;
Keywords=ode;differential;equation;wronskian;envelope;
StartupWMClass=ode_lab
EOF

command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$DESKTOP_DIR" >/dev/null 2>&1 || true

# Report missing runtime libraries (the GTK development headers are only
# needed at build time; the runtime GTK libraries are normally installed).
missing="$(ldd "$APP_DIR/ode_lab" "$APP_DIR/lib/libodecore.so" 2>/dev/null | grep 'not found' || true)"
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
echo "  menu:     $DESKTOP_DIR/ode-lab.desktop"
echo
if [[ ":$PATH:" != *":$BIN_DIR:"* ]]; then
  echo "Add $BIN_DIR to PATH to run 'ode-lab' from a terminal:"
  echo "  export PATH=\"$BIN_DIR:\$PATH\""
else
  echo "Run it with: ode-lab"
fi
