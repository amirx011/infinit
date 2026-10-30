# Infinit Lab — Exact Calculus & Differential Equations

Exact symbolic solving of ordinary differential equations and single-variable calculus (derivatives and indefinite integrals), with step-by-step solution paths and a bilingual (English/Persian) UI for **Linux** and **Android**.

> The app is branded **Infinit Lab**. The Flutter package is `ode_lab` and the native library is `libodecore`.

- Default appearance: **Dark**
- Default language: **English**
- Theme (dark/light) and language (English/Persian) are changed from the Settings sheet and persisted automatically
- Home dashboard for quick access to Derivative, Integral, and Differential Equations; Wronskian and Envelope live in the navigation drawer

Settings are stored in `~/.config/ode_lab/settings.json` on Linux (or `$XDG_CONFIG_HOME/ode_lab/settings.json`) and in `SharedPreferences` on Android.

Infinit Lab does not use numerical approximation to *produce* solutions. The computer-algebra core is written natively in C++17 and stores numbers as exact rationals (`int64` with an `__int128` overflow guard). A small C99 numeric evaluator is used only to **certify** results, never to generate them. SymPy is used as an external **reference oracle** for tests and desktop tooling only.

---

## Modules

| Module | What it does | Example |
|---|---|---|
| Derivative | First and second derivatives with exact simplification | `x^3*sin(x)` |
| Integral | Indefinite integration with automatic method detection | `2*x*cos(x^2)` |
| Differential equations | Exact first-order and constant-coefficient higher-order ODEs | `y' = y` |
| Wronskian | Wronskian determinant and linear independence | `W(sin x, cos x)` |
| Envelope & orthogonal | Family envelopes and orthogonal trajectories | `y = c*x + c^2` |

### Differential equation support

| Family | Example | Status |
|---|---|---|
| Separable | `y' = x/y` | ✅ exact + steps |
| Homogeneous | `y' = (x^2+y^2)/(x*y)` | ✅ exact + implicit/explicit |
| Exact | `(2x+y) + (x+2y)y' = 0` | ✅ + `du = M dx + N dy` certificate |
| Integrating factor | `μ(x)`, `μ(y)`, `μ(xy)`, `μ(x+y)` | ✅ |
| First-order linear | `y' + y/x = x^2` | ✅ + integrating factor |
| Bernoulli | `y' = x y + y^2` | ✅ |
| Clairaut | `y = x y' + (y')^2` | ✅ general + singular solution (envelope) |
| Lagrange | `y = 2x y' + (y')^2` | ✅ parametric |
| Riccati | `y' = P y^2 + Q y + R` | ✅ with a particular solution or second-order reduction |
| Linear substitution | `y' = (x+y)^2` | ✅ |
| Second/higher order, constant coefficients | `y'' - 3y' + 2y = 0` | ✅ real/complex/repeated roots |
| Direct integration | `y'' = 6x` | ✅ |
| Wronskian | `W(sin x, cos x)` | ✅ + linear independence |
| Family envelope | `y = c x + c^2` | ✅ + "no envelope" detection |
| Orthogonal trajectories | Family of circles | ✅ (within the implemented methods) |

### Single-variable calculus support

| Operation | Example | Notes |
|---|---|---|
| First derivative | `x^3*sin(x)` | Simplified output with plain/pretty/LaTeX views |
| Second derivative | `x^3` → `6*x` | Separate action in the UI |
| Standard table integral | `sin(x)` | Method: `table` |
| Substitution | `2*x*cos(x^2)` | Method: `substitution` |
| Integration by parts | `x*exp(x)` | Method: `parts` |
| Rational functions | `1/(x^2-x+1)`, `x/(x^2+1)` | Method: `rational` |
| Trigonometric powers | `sin(x)^2` | Method: `trig` |
| Polynomials and term-by-term | `x^2 + 3*x` | Methods: `expand` / `sum` |

### Scientific honesty

- If an antiderivative is not elementary, the result is returned as an **exact quadrature** using an `Integral(f, x)` node and `closed_form = false`; no hidden approximation is produced.
- Computed integrals are verified by differentiating the result (`checks`).
- Implicit solutions are returned as `u(x,y) = C`; Lagrange parametric solutions as `x(p), y(p)`.
- Singular solutions (envelopes) are listed separately.
- Domain assumptions (for example `x ≠ 0` or `|x| > 0`) are reported in the validation section.

---

## Architecture

```
┌──────────────────── Flutter UI (Dart, RTL/LTR, dark/light) ────────────────────┐
│  Derivative │ Integral │ Differential equations │ Wronskian │ Envelope        │
└───────────────────────────────────┬───────────────────────────────────────────┘
                                    │ dart:ffi  (JSON request/response)
┌───────────────────────────────────▼───────────────────────────────────────────┐
│        libodecore.so  —  C ABI: ode_solve / ode_free / ode_core_version        │
├───────────────────────────────────────────────────────────────────────────────┤
│  C++17 exact CAS                                                              │
│  Rational (int64 + __int128) · AST · parser · simplify · differentiate         │
│  integrate (table/substitution/parts/rational/trig) · polynomial factorisation │
│  partial fractions · ODE solvers · Wronskian · envelope                       │
├───────────────────────────────────────────────────────────────────────────────┤
│  C99 RPN numeric evaluator  →  verification certificates (not generation)     │
└───────────────────────────────────────────────────────────────────────────────┘

Python / SymPy  ──(outside the app)──►  oracle, test generation, cross-checking
```

See [`docs/architecture.md`](docs/architecture.md) for details.

---

## Project layout

```
infinit/
├── core/                      C++17 and C99 native core
│   ├── include/odecore.h      C ABI
│   ├── src/                   AST, algebra, calculus, solvers, JSON API
│   └── test/                  Native tests (algebra / ode / regression)
├── app/                       Flutter app (Linux + Android)
│   ├── lib/main.dart          App shell, dashboard, drawer, settings
│   ├── lib/src/pages/         Derivative, Integral, ODE, Wronskian, Envelope
│   ├── lib/src/               Models, FFI, theme, widgets, state, settings store
│   ├── linux/                 Linux desktop runner
│   └── android/               Android runner + generated jniLibs
├── python/                    SymPy oracle and cross-check tests
├── testdata/corpus.json       Shared ODE/Wronskian/envelope test corpus
├── release/                   Ready-to-publish artifacts
├── scripts/                   Build, test, packaging, sysroot helpers
└── docs/                      Architecture, API, expression syntax
```

---

## Requirements

- Linux (Fedora/Ubuntu) with `clang`/`gcc`, `cmake`, `ninja`, `pkg-config`
- Flutter **3.47.8** or newer (Dart 3.13.7+)
- Python **3.11+** and `sympy` (tests/oracle only)
- Android builds: Android SDK + NDK (version 29 tested)

Install the Python dependencies:

```bash
python3 -m pip install --user sympy pytest
```

---

## Building and running on Linux

If GTK 3 development files are not installed and you do not have root access, the following script builds a local sysroot (it downloads RPMs with `dnf download` and wraps `pkg-config`):

```bash
./scripts/setup_gtk_sysroot.sh
```

Then:

```bash
./scripts/build_linux.sh
./app/build/linux/x64/release/bundle/ode_lab
```

Or build manually:

```bash
./scripts/build_core.sh
export PATH="$HOME/toolchains/bin:$HOME/toolchains/flutter-sdk/bin:$PATH"
export PKG_CONFIG="$HOME/toolchains/bin/pkg-config"
cd app && flutter pub get && flutter build linux --release
```

> You can override the native library path with the `ODELAB_LIB` environment variable.

### Install for the current user (no root)

```bash
# Build and install into ~/.local
./scripts/package_linux.sh
./scripts/install_linux.sh dist/ode-lab-0.1.0-linux-x64-release.tar.gz
```

After installing:

- Terminal: `~/.local/bin/ode-lab` (add `~/.local/bin` to `PATH` if needed)
- Application menu: **Infinit Lab**
- Uninstall: `./scripts/install_linux.sh --uninstall`

The installed app needs the GTK 3 **runtime** libraries, which are normally already present on a desktop. GTK development headers are only needed at build time.

---

## Building for Android

```bash
# Once: install the NDK
$HOME/Android/Sdk/cmdline-tools/bin/sdkmanager --sdk_root=$HOME/Android/Sdk "ndk;29.0.14206865"

./scripts/build_android.sh
```

The script builds `libodecore.so` for `arm64-v8a` and `x86_64`, copies it into `app/android/app/src/main/jniLibs/`, and produces the APK at:

```
app/build/app/outputs/flutter-apk/app-release.apk
```

> Only 64-bit ABIs are supported. `armeabi-v7a` (32-bit) cannot be supported because the core uses `__int128` to guard rational overflow; modern Android devices are 64-bit.
> To change ABIs: `ABIS="arm64-v8a" ./scripts/build_android.sh`
> To build an App Bundle: `./scripts/build_android.sh --split-per-abi`
>
> The default APK is signed with the **debug key** so it can be installed on any device. For store distribution, change `signingConfig` in `app/android/app/build.gradle.kts` to your release key.
> The native library is linked with 16 KiB page alignment (required by Android 15+).

If `flutter pub get` fails with an authentication/403 error, use the local pub cache:

```bash
cd app && flutter pub get --offline
```

---

## Build artifacts

After running the build scripts:

| Artifact | Path |
|---|---|
| Linux executable | `app/build/linux/x64/release/bundle/ode_lab` |
| Linux package | `dist/ode-lab-0.1.0-linux-x64-release.tar.gz` |
| Android APK (build output) | `app/build/app/outputs/flutter-apk/app-release.apk` |
| Ready-to-publish Android APK | `release/ode-lab-0.1.0-android.apk` |

> `core/build/`, `core/build-android/`, `app/build/`, `app/.dart_tool/`, and `dist/` are generated build outputs and are ignored by Git. The APK in `release/` is kept for GitHub Releases or direct distribution.

---

## Tests

```bash
./scripts/run_tests.sh
```

The script runs four layers:

1. **Native C++ tests** — algebra, calculus, ODEs, Wronskian, envelopes, C ABI (`ctest`)
2. **SymPy oracle tests** — exact solving and verification for ODE/Wronskian/envelope cases (`pytest`)
3. **Native ↔ SymPy cross-check** — every native answer is checked against SymPy (`python/tests/compare_cpp.py`)
4. **Flutter checks** — `flutter analyze` and widget tests for the default theme, language switching, settings, navigation, integral/derivative modules, and a real ODE solve through FFI (`flutter test`)

Sample cross-check output:

```
[ok  ] y' = y                   linear               C·exp(x)
[ok  ] y'' + y = 0              linear_cc            C·cos(x) + C2·sin(x)
[ok  ] W('sin(x)', 'cos(x)') native=-1 oracle=-1
[ok  ] envelope y = c*x + c^2 native=1 oracle=1
0 failure(s)
```

> In this version the SymPy oracle covers ODE, Wronskian, and envelope/orthogonal cases only. Derivatives and integrals are covered by the native engine and Flutter tests.

---

## Native API

```c
char*       ode_solve(const char* request_json);
void        ode_free(char* text);
const char* ode_core_version(void);
```

Requests (all accept an optional `"lang": "en"` or `"fa"`):

```json
{"mode":"ode","input":"y' = y","x":"x","y":"y","particular":""}
{"mode":"wronskian","functions":["sin(x)","cos(x)"],"x":"x"}
{"mode":"envelope","input":"y = c*x + c^2","c":"c","orthogonal":false}
{"mode":"orthogonal","input":"x^2 + y^2 = c^2","c":"c","x":"x","y":"y"}
{"mode":"integral","input":"2*x*cos(x^2)","x":"x"}
{"mode":"derivative","input":"x^3*sin(x)","x":"x","order":1}
```

Responses include `ok`, `mode`, `type_id`, `type_fa`, `type_en`, `method`, `variable`, `order`, `solution` (plain/pretty/LaTeX views), `explicit_y`, `param_x`, `param_y`, `singular`, `wronskian`, `independent`, `envelope`, `orthogonal`, `steps`, `checks`, `warnings`, `assumptions`, `closed_form`, and `time_ms`.

Full reference: [`docs/api.md`](docs/api.md)

---

## Expression syntax

- Variables: `x`, `y`, `c`, and any other name
- Derivatives: `y'`, `y''`, `y'''`, or `dy/dx`
- Powers: `x^2`, `x**(1/2)` (`**` is also supported)
- Implicit multiplication: `2x`, `(x+1)(x-1)`, `2sin(x)`
- Functions: `sin`, `cos`, `tan`, `cot`, `sec`, `csc`, `asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`, `exp`, `ln`, `log`, `sqrt`, `abs`
- Absolute value: `|x|`
- Differential form: `(x+y)dx + (x+2y)dy = 0`
- Numbers: exact integers and decimals (`0.25` becomes the fraction `1/4`)
- Constants: `e` and `pi`

Full guide: [`docs/expressions.md`](docs/expressions.md)

---

## Known limitations

- Higher-order **non-homogeneous** equations with variable coefficients are not covered in this version.
- Power-series and Frobenius methods are not implemented.
- Riccati equations are solved only with a found/given particular solution or a constant-coefficient second-order reduction.
- Non-elementary integrals remain as `Integral` nodes (exact, but not closed-form).
- Integration is limited to table, substitution, integration by parts, rational functions, and trigonometric powers; definite integrals are not exposed in the UI.
- The UI offers first and second derivatives only (the API accepts higher `order` values).
- Systems of equations and partial differential equations are outside the scope of this version.

---

## License

MIT — see [`LICENSE`](LICENSE).
