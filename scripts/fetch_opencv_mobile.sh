#!/usr/bin/env bash
# Fetch the stripped OpenCV build used by the Android scanner.
#
# opencv-mobile ships static core/imgproc libraries for every Android ABI and
# keeps the APK small compared to the full OpenCV SDK. The archives are build
# inputs, not source, so third_party/ stays out of git.
#
#   scripts/fetch_opencv_mobile.sh            # default version
#   OPENCV_MOBILE_VERSION=4.14.0 scripts/fetch_opencv_mobile.sh
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${OPENCV_MOBILE_VERSION:-4.14.0}"
TAG="${OPENCV_MOBILE_TAG:-v38}"
DEST="$ROOT/third_party/opencv-mobile"

if [[ -f "$DEST/sdk/native/jni/OpenCVConfig.cmake" ]]; then
  echo "OpenCV mobile already present: $DEST"
  exit 0
fi

ZIP="opencv-mobile-$VERSION-android.zip"
URL="https://github.com/nihui/opencv-mobile/releases/download/$TAG/$ZIP"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "Downloading $URL"
curl -fL --retry 3 --max-time 600 -o "$TMP/$ZIP" "$URL"
mkdir -p "$ROOT/third_party"
unzip -q "$TMP/$ZIP" -d "$TMP/unpacked"
rm -rf "$DEST"
mv "$TMP/unpacked/opencv-mobile-$VERSION-android" "$DEST"
echo "OpenCV mobile $VERSION ready: $DEST"
