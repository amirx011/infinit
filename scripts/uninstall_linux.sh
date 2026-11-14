#!/usr/bin/env bash
# Remove an Infinit Lab installation from a user prefix.
#
#   ./scripts/uninstall_linux.sh
#   ./scripts/uninstall_linux.sh --purge
#   PREFIX=/opt/infinit ./scripts/uninstall_linux.sh --dry-run
#
# The paths mirror install_linux.sh. Settings live outside the prefix, so
# --purge is required to delete them.
set -euo pipefail

PREFIX="${PREFIX-$HOME/.local}"
raw_prefix="$PREFIX"
PREFIX="${PREFIX%/}"
if [[ -z "$PREFIX" || "$PREFIX" != /* ]]; then
  echo "PREFIX must be an absolute path other than /: '$raw_prefix'" >&2
  exit 2
fi

APP_DIR="$PREFIX/share/infinit"
BIN_DIR="$PREFIX/bin"
DESKTOP_DIR="$PREFIX/share/applications"
ICON_ROOT="$PREFIX/share/icons/hicolor"
ICON_SCALABLE_DIR="$ICON_ROOT/scalable/apps"
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/infinit"
APP_ID="ir.infinit"
DESKTOP_FILE="$DESKTOP_DIR/$APP_ID.desktop"
LAUNCHER="$BIN_DIR/infinit"

# Icon sizes written by install_linux.sh.
ICON_SIZES=(16 24 32 48 64 96 128 192 256 512)

usage() {
  cat <<EOF
Usage: $(basename "$0") [--purge] [--dry-run]

Removes the Infinit Lab app, launcher, desktop entry and icons from
$PREFIX.

  --purge, -p    also delete $CONFIG_DIR
  --dry-run, -n  list what would be removed, change nothing
  --help, -h     show this message

Set PREFIX to uninstall from a different prefix.
EOF
}

purge=0
dry_run=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --purge|-p) purge=1 ;;
    --dry-run|-n) dry_run=1 ;;
    --help|-h) usage; exit 0 ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

removed=0
remove() {
  local path="$1"
  if [[ ! -e "$path" && ! -L "$path" ]]; then
    return 0
  fi
  if (( dry_run )); then
    echo "would remove  $path"
  else
    rm -rf -- "$path"
    echo "removed       $path"
  fi
  removed=$((removed + 1))
}

refresh_caches() {
  if (( dry_run )); then
    return 0
  fi
  if [[ -d "$DESKTOP_DIR" ]] && command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$DESKTOP_DIR" >/dev/null 2>&1 || true
  fi
  if [[ -d "$ICON_ROOT" ]] && command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "$ICON_ROOT" >/dev/null 2>&1 || true
  fi
}

remove "$APP_DIR"
remove "$LAUNCHER"
remove "$DESKTOP_FILE"
# Names used before the ir.infinit app id.
remove "$DESKTOP_DIR/infinit.desktop"
remove "$ICON_SCALABLE_DIR/$APP_ID.svg"
remove "$ICON_SCALABLE_DIR/infinit.svg"
for size in "${ICON_SIZES[@]}"; do
  remove "$ICON_ROOT/${size}x${size}/apps/$APP_ID.png"
  remove "$ICON_ROOT/${size}x${size}/apps/infinit.png"
done
if (( purge )); then
  remove "$CONFIG_DIR"
fi

refresh_caches

if (( removed == 0 )); then
  echo "Nothing to remove under $PREFIX."
elif (( dry_run )); then
  echo "Dry run: $removed item(s) would be removed."
else
  echo "Removed $removed item(s) from $PREFIX."
fi

if (( purge == 0 )) && [[ -e "$CONFIG_DIR" ]]; then
  echo "Settings kept in $CONFIG_DIR (use --purge to delete them)."
fi
