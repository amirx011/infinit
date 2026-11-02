#!/usr/bin/env bash
# Build libodecore.so for the Android ABIs and (optionally) package the APK.
#
#   scripts/build_android.sh              # native engine + APK
#   scripts/build_android.sh --core-only  # native engine only (used by Gradle)
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ANDROID_SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$HOME/Android/Sdk}}"
FLUTTER_BIN="${FLUTTER_BIN:-$HOME/toolchains/flutter-sdk/bin}"

CORE_ONLY=0
ARGS=()
for arg in "$@"; do
  case "$arg" in
    --core-only) CORE_ONLY=1 ;;
    -h|--help)
      sed -n '2,5p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
    *) ARGS+=("$arg") ;;
  esac
done
if (( ${#ARGS[@]} )); then set -- "${ARGS[@]}"; else set --; fi

if [[ -n "${ANDROID_NDK_HOME:-}" ]]; then
  NDK="$ANDROID_NDK_HOME"
elif [[ -n "${ANDROID_NDK_ROOT:-}" ]]; then
  NDK="$ANDROID_NDK_ROOT"
else
  NDK="$(ls -d "$ANDROID_SDK"/ndk/* 2>/dev/null | sort -V | tail -1 || true)"
fi

if [[ -z "${NDK:-}" || ! -f "$NDK/build/cmake/android.toolchain.cmake" ]]; then
  cat >&2 <<EOF
Android NDK not found.
Install one with:
  $ANDROID_SDK/cmdline-tools/bin/sdkmanager --sdk_root=$ANDROID_SDK "ndk;29.0.14206865"
or set ANDROID_NDK_HOME to an existing NDK.
EOF
  exit 1
fi

if ! command -v cmake >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1; then
  echo "cmake and ninja are required to cross-compile the native engine." >&2
  exit 1
fi

echo "Using NDK: $NDK"
ABIS=(${ABIS:-arm64-v8a armeabi-v7a x86_64})
API="${ANDROID_API:-21}"

for abi in "${ABIS[@]}"; do
  echo
  echo "== building libodecore.so for $abi =="
  build="$ROOT/core/build-android/$abi"
  cmake -S "$ROOT/core" -B "$build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$abi" \
    -DANDROID_PLATFORM="android-$API" \
    -DANDROID_STL=c++_static \
    -DCMAKE_BUILD_TYPE=Release \
    -DODE_BUILD_TESTS=OFF \
    -DODE_BUILD_STATIC=OFF
  cmake --build "$build" -j "${JOBS:-$(nproc)}"

  dest="$ROOT/app/android/app/src/main/jniLibs/$abi"
  mkdir -p "$dest"
  cp "$build/lib/libodecore.so" "$dest/libodecore.so"
  echo "  -> $dest/libodecore.so"
done

if [[ "$CORE_ONLY" == 1 ]]; then
  echo
  echo "Native engine staged for: ${ABIS[*]}"
  echo "Location: $ROOT/app/android/app/src/main/jniLibs"
  exit 0
fi

# Flutter/Gradle occasionally insists on a specific NDK revision.  Point it at
# the installed one so a clean machine does not try to download another copy.
NDK_VERSION="$(basename "$NDK")"
export INFINIT_NDK_VERSION="$NDK_VERSION"

export PATH="$FLUTTER_BIN:$PATH"
cd "$ROOT/app"
if ! flutter pub get --offline; then
  echo "Offline pub get failed; retrying online..." >&2
  flutter pub get
fi

flutter build apk --release --target-platform android-arm64,android-arm,android-x64 --no-pub "$@"

VERSION="$(sed -n 's/^version: \([^+]*\).*/\1/p' "$ROOT/app/pubspec.yaml")"
APK="$ROOT/app/build/app/outputs/flutter-apk/app-release.apk"
echo
echo "APK: $APK"
if [[ -f "$APK" ]]; then
  mkdir -p "$ROOT/release"
  cp "$APK" "$ROOT/release/infinit-lab-$VERSION-android.apk"
  echo "Release APK: $ROOT/release/infinit-lab-$VERSION-android.apk"
fi
