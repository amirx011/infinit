#!/usr/bin/env bash
# Prints the signing certificate of an APK and fails when it is the debug key.
#
#   scripts/verify_apk_signature.sh release/infinit-lab-1.0.0-android.apk
#
# Stores such as Myket reject an APK signed with Android's debug key, which is
# what Flutter falls back to when app/android/key.properties is missing.
set -euo pipefail

APK="${1:-}"
if [[ -z "$APK" || ! -f "$APK" ]]; then
  echo "usage: $(basename "$0") <apk>" >&2
  exit 2
fi

SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$HOME/Android/Sdk}}"
APKSIGNER="$(ls -d "$SDK"/build-tools/*/apksigner 2>/dev/null | sort -V | tail -1 || true)"
if [[ -z "$APKSIGNER" || ! -x "$APKSIGNER" ]]; then
  echo "apksigner not found under $SDK; skipping the signature check." >&2
  exit 0
fi

REPORT="$("$APKSIGNER" verify --print-certs "$APK" 2>/dev/null || true)"
DN="$(awk -F': ' '/certificate DN:/{print $NF; exit}' <<<"$REPORT")"
SHA="$(awk -F': ' '/certificate SHA-256 digest:/{print $NF; exit}' <<<"$REPORT")"

if [[ -z "$DN" ]]; then
  echo "Error: $APK is not signed." >&2
  exit 1
fi

if [[ "$DN" == *"CN=Android Debug"* ]]; then
  echo "Error: $APK is signed with the debug key." >&2
  echo "Add app/android/key.properties and the release keystore, then rebuild." >&2
  exit 1
fi

echo "  signed by $DN"
echo "  sha256 $SHA"
