# Infinit Lab

Exact symbolic calculus and differential equations for **Linux** and **Android**, with a bilingual English/Persian UI and step-by-step solutions.

- Five modules: **Derivative**, **Integral**, **Differential Equations**, **Wronskian**, and **Envelope & Orthogonal Trajectories**
- Dark/light theme and English/Persian language, saved between runs
- Exact rational arithmetic in a native C++17 core; no numerical approximation is used to generate answers
- SymPy is used only as an external test oracle

---

## What it solves

- **ODEs:** separable, homogeneous, exact, integrating factor, first-order linear, Bernoulli, Clairaut, Lagrange, Riccati, linear substitution, constant-coefficient second/higher order, and direct integration
- **Calculus:** first and second derivatives; indefinite integrals using standard tables, substitution, integration by parts, rational functions, and trigonometric powers
- **Linear algebra:** Wronskian determinant and linear independence
- **Geometry:** family envelopes and orthogonal trajectories

If an antiderivative is not elementary, the result stays as an exact `Integral(f, x)` node with `closed_form = false`. Implicit, parametric, and singular solutions are reported separately.

---

## Quick start

Requirements: Linux with `clang`/`gcc`, `cmake`, `ninja`, `pkg-config`; Flutter 3.47.8+ (Dart 3.13.7+); Python 3.11+ with `sympy` for tests. Android builds also need the SDK and NDK 29.

```bash
python3 -m pip install --user sympy pytest

# Linux
./scripts/build_linux.sh
./app/build/linux/x64/release/bundle/infinit

# Android
./scripts/build_android.sh
```

The build scripts use the local pub cache first and fall back to the network. If GTK 3 development files are missing, run `./scripts/setup_gtk_sysroot.sh` once before building Linux.

### Install (Linux, no root)

```bash
./scripts/package_linux.sh
./scripts/install_linux.sh dist/infinit-lab-1.0.0-linux-x64.tar.gz
```

This installs `~/.local/bin/infinit` and a menu entry named **Infinit Lab**. Uninstall with `./scripts/install_linux.sh --uninstall`.

---

## Artifacts

| Artifact | Path |
|---|---|
| Linux executable | `app/build/linux/x64/release/bundle/infinit` |
| Linux package | `dist/infinit-lab-1.0.0-linux-x64.tar.gz` |
| Android APK | `app/build/app/outputs/flutter-apk/app-release.apk` |
| Ready to publish | `release/infinit-lab-1.0.0-linux-x64.tar.gz`, `release/infinit-lab-1.0.0-android.apk` |

`release/` is tracked for GitHub Releases or direct distribution. Build directories and `dist/` are generated and ignored by Git.

---

## Tests

```bash
./scripts/run_tests.sh
```

This runs native C++ tests (`ctest`), SymPy oracle tests (`pytest`), native ↔ SymPy cross-checks, and Flutter analysis/widget tests. Flutter dependencies can be resolved offline with `cd app && flutter pub get --offline`.

---

## Native API

```c
char*       ode_solve(const char* request_json);
void        ode_free(char* text);
const char* ode_core_version(void);
```

All operations use the same JSON contract:

```json
{"mode":"ode","input":"y' = y"}
{"mode":"integral","input":"2*x*cos(x^2)","x":"x"}
{"mode":"derivative","input":"x^3*sin(x)","x":"x","order":1}
{"mode":"wronskian","functions":["sin(x)","cos(x)"]}
{"mode":"envelope","input":"y = c*x + c^2","c":"c"}
```

Use `"lang":"en"` or `"lang":"fa"` for step text. Full reference: [`docs/api.md`](docs/api.md).

---

## Project layout

```
core/      C++17 exact engine and C99 verification evaluator
app/       Flutter UI, Linux runner, Android runner
python/    SymPy oracle and cross-check tests
scripts/   Build, test, package, install, and sysroot helpers
docs/      Architecture, API, and expression syntax
release/   Ready-to-publish artifacts
testdata/  Shared test corpus
```

The UI links to the native library `libodecore.so` through `dart:ffi`; the Flutter package is named `infinit`.

---

## Limitations

- Higher-order non-homogeneous ODEs with variable coefficients are not supported.
- Power-series/Frobenius methods and systems/PDEs are out of scope.
- Riccati equations need a particular solution or a constant-coefficient reduction.
- Non-elementary integrals remain exact quadratures.
- The UI exposes first and second derivatives; the API accepts higher `order` values.
- Only 64-bit Android ABIs are supported.

---

## License

Apache License 2.0 — see [`LICENSE`](LICENSE).

Copyright 2026 Infinit Lab contributors.
