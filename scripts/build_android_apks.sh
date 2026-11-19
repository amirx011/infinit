#!/usr/bin/env bash
# Optional split build: one release APK per Android ABI in release/.
#
#   scripts/build_android_apks.sh            # arm64-v8a, armeabi-v7a, x86_64
#   scripts/build_android_apks.sh --clean    # remove build/ and jniLibs first
#
# Releases ship the universal APK from build_android.sh; this script only helps
# when download size matters more than a single file.  Each APK carries just its
# own libodecore.so, so it is smaller, but someone has to pick the right one:
# arm64-v8a for current phones, armeabi-v7a for older 32-bit devices, x86_64 for
# emulators and ChromeOS.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FLUTTER_BIN="${FLUTTER_BIN:-$HOME/toolchains/flutter-sdk/bin}"
RELEASE="$ROOT/release"
ABIS=(arm64-v8a armeabi-v7a x86_64)

CLEAN=0
for arg in "$@"; do
  case "$arg" in
    --clean) CLEAN=1 ;;
    -h|--help)
      sed -n '2,9p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
    *)
      echo "Unknown option: $arg" >&2
      exit 2
      ;;
  esac
done

VERSION="$(sed -n 's/^version: \([^+]*\).*/\1/p' "$ROOT/app/pubspec.yaml")"
[[ -n "$VERSION" ]] || { echo "Could not read the version from pubspec.yaml." >&2; exit 1; }

if (( CLEAN )); then
  echo "== removing previous Android build output =="
  rm -rf "$ROOT/app/build/app" "$ROOT/app/android/app/src/main/jniLibs"
fi

export PATH="$FLUTTER_BIN:$PATH"
cd "$ROOT/app"
if ! flutter pub get --offline; then
  echo "Offline pub get failed; retrying online..." >&2
  flutter pub get
fi

echo
echo "== building $VERSION for ${ABIS[*]} =="
# Gradle's preBuild task cross-compiles libodecore.so for every ABI in
# INFINIT_ANDROID_ABIS, so the APKs stay self-contained.
flutter build apk --release \
  --split-per-abi \
  --target-platform android-arm64,android-arm,android-x64 \
  --no-pub

mkdir -p "$RELEASE"
for abi in "${ABIS[@]}"; do
  src="$ROOT/app/build/app/outputs/flutter-apk/app-$abi-release.apk"
  dst="$RELEASE/infinit-lab-$VERSION-android-$abi.apk"
  if [[ ! -f "$src" ]]; then
    echo "Missing build output: $src" >&2
    exit 1
  fi
  cp "$src" "$dst"
  # awk reads the whole listing, so pipefail cannot turn a match into an error.
  if ! unzip -l "$dst" | awk -v p="lib/$abi/libodecore.so" '$4 == p { found = 1 } END { exit !found }'; then
    echo "Warning: $dst does not contain lib/$abi/libodecore.so" >&2
  fi
  printf '  %-16s %s\n' "$abi" "$(du -h "$dst" | cut -f1)  $(basename "$dst")"
  "$ROOT/scripts/verify_apk_signature.sh" "$dst"
done

echo
echo "APKs in $RELEASE"
