#!/usr/bin/env bash
# Build a local GTK3 development sysroot without root privileges.
#
# Fedora keeps the runtime libraries installed, but the -devel packages are
# missing.  This script extracts downloaded -devel RPMs into
# $TOOLCHAINS/gtk-sysroot and creates a small pkg-config wrapper that points
# the compiler at the extracted headers while linking against the real
# runtime libraries in /usr/lib64.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLCHAINS="${TOOLCHAINS:-$HOME/toolchains}"
RPMDIR="${RPMDIR:-$TOOLCHAINS/rpms}"
SYSROOT="${SYSROOT:-$TOOLCHAINS/gtk-sysroot}"
BINDIR="$TOOLCHAINS/bin"
PKGDIR="$SYSROOT/usr/lib64/pkgconfig"

mkdir -p "$SYSROOT" "$BINDIR" "$RPMDIR"

# RPMs required for compiling a GTK3 desktop app.  Runtime libraries are
# expected to be installed by the distribution.
RPM_NAMES=(
  gtk3-devel pango-devel cairo-gobject-devel gdk-pixbuf2-devel
  atk-devel libsecret-devel fribidi-devel libthai-devel
)

missing=()
for name in "${RPM_NAMES[@]}"; do
  found=$(ls "$RPMDIR"/${name}-*.x86_64.rpm 2>/dev/null | grep -v '\.i686\.' | head -1 || true)
  if [[ -z "$found" || ! -s "$found" ]]; then
    missing+=("$name")
  fi
done

if ((${#missing[@]})); then
  echo "Downloading missing RPMs: ${missing[*]}"
  (cd "$RPMDIR" && dnf download --destdir . "${missing[@]}")
fi

for f in "$RPMDIR"/*-*.x86_64.rpm; do
  name="$(basename "$f")"
  case "$name" in
    gtk3-devel-*|pango-devel-*|cairo-gobject-devel-*|gdk-pixbuf2-devel-*|atk-devel-*|libsecret-devel-*|fribidi-devel-*|libthai-devel-*)
      if [[ -s "$f" ]]; then
        echo "  extracting $name"
        rpm2cpio "$f" | cpio -idm --quiet -D "$SYSROOT"
      fi
      ;;
  esac
done

# Private requirements only matter for static linking; their .pc files are
# often absent in a minimal installation.
find "$PKGDIR" -name '*.pc' -print0 | xargs -0 sed -i '/^Requires\.private:/d'

# cairo-gobject.pc is shipped by a separate sub-package that is not always
# downloadable; provide the standard contents.
if [[ ! -f "$PKGDIR/cairo-gobject.pc" ]]; then
  cat > "$PKGDIR/cairo-gobject.pc" <<'EOF'
prefix=/usr
exec_prefix=${prefix}
libdir=${exec_prefix}/lib64
includedir=${prefix}/include

Name: cairo-gobject
Description: cairo-gobject
Version: 1.18.6
Requires: cairo gobject-2.0
Libs: -L${libdir} -lcairo-gobject
Cflags: -I${includedir}/cairo
EOF
fi

# Re-point dangling .so symlinks at the installed runtime libraries.
for so in "$SYSROOT"/usr/lib64/*.so; do
  [[ -L "$so" ]] || continue
  target="$(readlink "$so")"
  if [[ ! -e "$so" ]]; then
    if [[ -e "/usr/lib64/$target" ]]; then
      ln -sf "/usr/lib64/$target" "$so"
    elif [[ -e "/usr/lib/$target" ]]; then
      ln -sf "/usr/lib/$target" "$so"
    fi
  fi
done

# Runtime-only libraries need an unversioned link name for -l.
for lib in cairo-gobject gtk-3 gdk-3 pango-1.0 pangocairo-1.0 atk-1.0 \
           gdk_pixbuf-2.0 harfbuzz gobject-2.0 glib-2.0 gio-2.0 secret-1; do
  if [[ ! -e "$SYSROOT/usr/lib64/lib$lib.so" ]]; then
    real=$(ls /usr/lib64/lib$lib.so.* 2>/dev/null | head -1 || true)
    [[ -n "$real" ]] && ln -sf "$real" "$SYSROOT/usr/lib64/lib$lib.so"
  fi
done

cat > "$BINDIR/pkg-config" <<'EOF'
#!/usr/bin/env bash
# pkg-config wrapper: use the local GTK sysroot for headers and the system
# runtime libraries for linking.
REAL_PKG_CONFIG="${REAL_PKG_CONFIG:-/usr/bin/pkg-config}"
SYSROOT="${GTK_SYSROOT:-$HOME/toolchains/gtk-sysroot}"
export PKG_CONFIG_PATH="$SYSROOT/usr/lib64/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

out="$("$REAL_PKG_CONFIG" "$@" 2>/dev/null)" || exit $?

# -I/usr/include/... and -L/usr/lib64 are rewritten only when the path is
# absent from the system but present in the sysroot.
rewritten=""
has_L=0
has_lib=0
for token in $out; do
  case "$token" in
    -I/*|-L/*)
      flag="${token:0:2}"
      path="${token:2}"
      if [[ ! -e "$path" && -e "$SYSROOT$path" ]]; then
        path="$SYSROOT$path"
      fi
      [[ "$flag" == "-L" ]] && has_L=1
      rewritten+="$flag$path "
      ;;
    -l*)
      has_lib=1
      rewritten+="$token "
      ;;
    /*)
      if [[ ! -e "$token" && -e "$SYSROOT$token" ]]; then
        rewritten+="$SYSROOT$token "
      else
        rewritten+="$token "
      fi
      ;;
    *)
      rewritten+="$token "
      ;;
  esac
done

if [[ $has_L -eq 1 || $has_lib -eq 1 ]]; then
  printf '%s' "-L$SYSROOT/usr/lib64 "
fi
printf '%s\n' "$rewritten"
EOF
chmod +x "$BINDIR/pkg-config"

echo
echo "GTK sysroot ready at $SYSROOT"
echo "Add $BINDIR to PATH before building Flutter Linux:"
echo "  export PATH=\"$BINDIR:\$PATH\""
echo "  export PKG_CONFIG=\"$BINDIR/pkg-config\""
echo "  flutter build linux"
