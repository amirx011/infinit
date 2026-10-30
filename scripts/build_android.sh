#!/usr/bin/env bash
# Build libodecore.so for Android ABIs and package the Flutter APK.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ANDROID_SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$HOME/Android/Sdk}}"
FLUTTER_BIN="${FLUTTER_BIN:-$HOME/toolchains/flutter-sdk/bin}"

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

echo "Using NDK: $NDK"
ABIS=(${ABIS:-arm64-v8a x86_64})
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

# Flutter/Gradle occasionally insists on a specific NDK revision.  Point it at
# the installed one so a clean machine does not try to download another copy.
NDK_VERSION="$(basename "$NDK")"
export ODELAB_NDK_VERSION="$NDK_VERSION"

export PATH="$FLUTTER_BIN:$PATH"
cd "$ROOT/app"
flutter pub get
flutter build apk --release --target-platform android-arm64,android-x64 "$@"

echo
echo "APK: $ROOT/app/build/app/outputs/flutter-apk/app-release.apk"
