#!/usr/bin/env bash
# Package the release Linux bundle as an RPM for Fedora, RHEL and openSUSE.
#
#   scripts/package_rpm.sh
#
# The bundle is installed under /usr/lib/infinit with /usr/bin/infinit as a
# symlink, because the Flutter runner loads lib/ and data/ from the directory
# of the executable, and /proc/self/exe resolves the symlink back there.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="${MODE:-release}"
VERSION="$(sed -n 's/^version: \([^+]*\).*/\1/p' "$ROOT/app/pubspec.yaml")"
# A rebuilt bundle with the same version and release is byte-different but has
# the same NEVRA, so rpm/dnf report "already installed" and keep the old files.
# Local rebuilds can pass RPM_RELEASE=2, 3, ... to make the package an upgrade.
RELEASE="${RPM_RELEASE:-1}"
BUNDLE="$ROOT/app/build/linux/x64/$MODE/bundle"
WORK="$ROOT/dist/rpm"
TOPDIR="$WORK/rpmbuild"
ARCH="${RPM_ARCH:-x86_64}"

[[ -n "$VERSION" ]] || { echo "Could not read the version from pubspec.yaml." >&2; exit 1; }
if [[ ! -x "$BUNDLE/infinit" ]]; then
  echo "Bundle not found: $BUNDLE" >&2
  echo "Run scripts/build_linux.sh first." >&2
  exit 1
fi
if ! command -v rpmbuild > /dev/null 2>&1; then
  echo "rpmbuild is missing. Install it with:" >&2
  echo "  Fedora:  sudo dnf install rpm-build" >&2
  echo "  Ubuntu:  sudo apt install rpm" >&2
  exit 1
fi

rm -rf "$TOPDIR"
mkdir -p "$TOPDIR/BUILD" "$TOPDIR/BUILDROOT" "$TOPDIR/RPMS" "$TOPDIR/SOURCES" "$TOPDIR/SPECS" "$TOPDIR/SRPMS"

SPEC="$TOPDIR/SPECS/infinit-lab.spec"
cat > "$SPEC" <<SPECFILE
Name:           infinit-lab
Version:        $VERSION
Release:        $RELEASE
Summary:        Differential equations and calculus workbench
License:        Apache-2.0
URL:            https://github.com/amirx011/infinit
Source0:        infinit-lab-$VERSION.tar.gz
BuildArch:      $ARCH
AutoReqProv:    no
Requires:       gtk3
Requires:       libstdc++

%description
Infinit Lab solves differential equations, calculus and linear algebra problems
symbolically and shows every step that produced the answer.  It runs offline
and needs no account.

%prep

%build

%install
mkdir -p %{buildroot}/usr/lib/infinit
cp -a "$BUNDLE"/. %{buildroot}/usr/lib/infinit/
mkdir -p %{buildroot}/usr/bin
ln -sf /usr/lib/infinit/infinit %{buildroot}/usr/bin/infinit
install -Dm644 "$ROOT/scripts/linux/infinit.desktop" \
  %{buildroot}/usr/share/applications/infinit.desktop
install -Dm644 "$ROOT/app/assets/branding/infinit-icon-256.png" \
  %{buildroot}/usr/share/icons/hicolor/256x256/apps/infinit.png

%files
/usr/lib/infinit
/usr/bin/infinit
/usr/share/applications/infinit.desktop
/usr/share/icons/hicolor/256x256/apps/infinit.png

%changelog
* $(LC_ALL=C date '+%a %b %d %Y') Infinit Lab <noreply@infinit.local> - $VERSION-$RELEASE
- Release $VERSION-$RELEASE
SPECFILE

rpmbuild -bb --define "_topdir $TOPDIR" "$SPEC" > "$WORK/rpmbuild.log" 2>&1 || {
  echo "rpmbuild failed; the log follows." >&2
  tail -30 "$WORK/rpmbuild.log" >&2
  exit 1
}

RPM="$(find "$TOPDIR/RPMS" -name '*.rpm' | head -1)"
[[ -n "$RPM" ]] || { echo "rpmbuild produced no file; see $WORK/rpmbuild.log" >&2; exit 1; }

mkdir -p "$ROOT/release"
cp "$RPM" "$ROOT/release/"
printf '  %-16s %s\n' "RPM" "$(du -h "$RPM" | cut -f1)  $(basename "$RPM")"
