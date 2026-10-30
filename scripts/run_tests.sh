#!/usr/bin/env bash
# Run every automated check: native regression tests, SymPy oracle tests,
# native/oracle cross-checks and Flutter widget tests.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FLUTTER_BIN="${FLUTTER_BIN:-$HOME/toolchains/flutter-sdk/bin}"

echo "== native build =="
"$ROOT/scripts/build_core.sh"

echo
echo "== native tests =="
ctest --test-dir "$ROOT/core/build" --output-on-failure

echo
echo "== SymPy oracle tests =="
(cd "$ROOT/python" && python3 -m pytest -q)

echo
echo "== native ↔ SymPy cross-check =="
python3 "$ROOT/python/tests/compare_cpp.py"

if [[ -x "$FLUTTER_BIN/flutter" ]]; then
  echo
  echo "== Flutter analysis and widget tests =="
  export PATH="$FLUTTER_BIN:$PATH"
  (cd "$ROOT/app" && flutter analyze && flutter test)
else
  echo "Flutter SDK not found at $FLUTTER_BIN; skipping UI tests."
fi

echo
echo "All checks completed."
