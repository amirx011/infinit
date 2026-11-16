#!/usr/bin/env bash
# Builds the universal Android APK with Persian as the first-launch language.
#
#   scripts/build_android_fa.sh            # Persian-default universal APK
#   scripts/build_android_fa.sh --clean    # remove build/ and jniLibs first
#
# The repository keeps English as its default; this build only passes
# INFINIT_DEFAULT_LOCALE=fa to the Dart compiler, so no source file is edited
# and the English build stays untouched.  Readers can still switch language in
# settings.  Output: release/infinit-lab-<version>-android-fa.apk
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
      sed -n '2,10p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
    *)
      echo "Unknown option: $arg" >&2
      exit 2
      ;;
  esac
done

if ! grep -q "INFINIT_DEFAULT_LOCALE" "$ROOT/app/lib/src/l10n.dart"; then
  echo "app/lib/src/l10n.dart does not read INFINIT_DEFAULT_LOCALE." >&2
  echo "Update the source before building a Persian-default APK." >&2
  exit 1
fi

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
echo "== building $VERSION for Android with Persian as the default language =="
flutter build apk --release --no-pub \
  --dart-define=INFINIT_DEFAULT_LOCALE=fa

OUT="$ROOT/app/build/app/outputs/flutter-apk/app-release.apk"
if [[ ! -f "$OUT" ]]; then
  echo "Missing build output: $OUT" >&2
  exit 1
fi

# A universal APK must carry the native engine for every Android ABI.
for abi in "${ABIS[@]}"; do
  # awk reads the whole listing, so pipefail cannot turn a match into an error.
  if ! unzip -l "$OUT" | awk -v p="lib/$abi/libodecore.so" '$4 == p { found = 1 } END { exit !found }'; then
    echo "Warning: the APK does not contain lib/$abi/libodecore.so" >&2
  fi
done

mkdir -p "$RELEASE"
DST="$RELEASE/infinit-lab-$VERSION-android-fa.apk"
cp "$OUT" "$DST"
printf '  %-16s %s\n' "universal" "$(du -h "$DST" | cut -f1)  $(basename "$DST")"
"$ROOT/scripts/verify_apk_signature.sh" "$DST"

echo
echo "Persian-default APK in $RELEASE"
