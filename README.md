# Infinit Lab

Exact symbolic calculus and differential equations for **Linux** and **Android**, with a multilingual, RTL-aware UI and step-by-step solutions.

- Modules: **Derivative**, **Integral**, **Integral applications**, **Taylor & Maclaurin**, **Partial derivatives**, **Series & convergence**, **Differential Equations**, **Wronskian**, **Envelope & Orthogonal Trajectories**, **Applications of the Derivative** (function behaviour, tangent & normal, optimisation, related rates, and the mean value theorems), plus the curriculum workbenches for **systems, transforms, numerical methods, applications and partial differential equations**
- Dark/light/system theme, six accent palettes, high-contrast mode, reduced motion, adjustable text size, and **12 interface languages** (English, Persian, Arabic, Spanish, French, German, Russian, Chinese, Japanese, Hindi, Portuguese, Turkish), all saved between runs
- A Ctrl+K command palette, settings from the header gear or Ctrl+comma, and a configurable start page
- Settings for step-by-step solutions, validation checks, graph visibility and numeric decimal places, so the same engine can serve a quick answer-only view or a full worked derivation
- Exact rational arithmetic in a native C++17 core; numeric RK4 is used only as a clearly labelled fallback when no symbolic form fits
- SymPy is used only as an external test oracle

---

## What it solves

- **ODEs:** separable, exact, first-order linear, Bernoulli, homogeneous, integrating factor, Riccati, Clairaut, Lagrange, linear substitution, constant-coefficient second/higher order, direct integration, and order reduction for equations missing a variable. The engine reports the first matching family and lists every other family the equation belongs to (`also_forms`), see `docs/classification_order.md`. If the dependent variable `y` is absent, the solver substitutes `p = y′` and solves the lower-order equation; if the independent variable `x` is absent, it substitutes `y′ = p(y)` and `y″ = p·dp/dy`, solves for `p(y)`, then separates `dy/p(y) = dx`. Every path reports the derivation stage by stage, including the intermediate partial derivatives, the potential function, and the substitution that was applied. When no symbolic family fits a second-order equation, the solver returns a clearly labelled RK4 approximation instead of an unsolved result.
- **Calculus:** derivatives of order 1–10; indefinite integrals using standard tables, substitution, integration by parts, rational functions, and trigonometric powers; Taylor and Maclaurin polynomials with the Lagrange remainder; area between curves, volume of revolution and arc length
- **Multivariable and series:** first and second partial derivatives, the gradient at a point, the Laplacian and directional derivatives; geometric, p-series, ratio, root and alternating-series tests with exact sums where a closed form exists
- **Applications of the derivative:** monotonicity, concavity and inflection points; vertical, horizontal and oblique asymptotes plus a curve-sketch summary; tangent and normal lines; extrema on a closed interval; related rates; and the mean value theorems (Rolle, Lagrange, Cauchy) plus L'Hôpital's rule
- **Plots:** direction fields for first-order equations in either `y' = f(x, y)` or `M dx + N dy = 0` form, solution curves for higher-order equations, and the tangent, secant and normal lines that illustrate each theorem. Every other section draws the graph that carries its answer: `f` and `f′` for derivatives, integrand and antiderivative (or the bounded region) for integrals, the generating curve for volume and arc length, `f` with its Taylor polynomial, partial sums for series, the functions under test for the Wronskian, and a contour map for surfaces. Graphs can be switched off globally in Settings.
- **Linear algebra:** Wronskian determinant and linear independence; matrix workbench for 1×1–3×3 matrices with exact determinant, trace, characteristic polynomial, eigenvalues, eigenvectors, inverse and diagonalizability
- **Geometry:** family envelopes, orthogonal trajectories, and oblique trajectories that cross a family at a constant angle
- **Curriculum workbenches:** linear systems, phase portraits and the matrix/eigenvalue workbench (determinant, characteristic polynomial, eigenvalues, eigenvectors, inverse and diagonalizability), critical points and their linear stability, and the Lotka–Volterra predator–prey model with its conserved quantity, Laplace and inverse Laplace transforms, transform of derivatives, initial-value problems, step functions, discontinuous forcing and impulse responses, convolution, Fourier series and the Fourier transform, Euler/Heun/RK4, Adams–Bashforth multistep methods and numerical error/convergence, growth and decay, Newton cooling, logistic growth, mixing, spring–mass, damped oscillation, circuits and free fall. The partial-differential-equation group covers first-order transport, the method of characteristics, classification by the discriminant, separation of variables, the heat and wave equations, and Laplace's equation on a rectangle. Each PDE answer is checked against the equation itself, the initial or boundary data, or the sign of the quadratic form, and the checks are shown with the derivation.

If an antiderivative is not elementary, the result stays as an exact `Integral(f, x)` node with `closed_form = false`. Implicit, parametric, and singular solutions are reported separately.

---

## Languages

The interface ships in 12 languages and flips to right-to-left automatically for Persian and Arabic. Every interface string lives in `app/assets/i18n/*.json`, one catalogue per language with identical keys and placeholders, so translations never enter the native core. The engine still receives `en` or `fa`, which keeps worked solutions predictable.

To add a language, copy `app/assets/i18n/en.json`, translate the values without changing the keys or `{placeholders}`, and register the locale in `app/lib/src/l10n.dart`. No C or C++ change is required.

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

This installs `~/.local/bin/infinit` and a menu entry named **Infinit Lab**. Uninstall with `./scripts/uninstall_linux.sh`; add `--purge` to delete saved settings as well.

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
{"mode":"behaviour","input":"x^3 - 3*x","x":"x","xmin":-3,"xmax":3}
{"mode":"tangent","input":"x^2","x":"x","at":"1"}
{"mode":"optimize","input":"x^3 - 3*x","x":"x","xmin":-2,"xmax":2}
{"mode":"related_rates","input":"x^2 + y^2 = 25","var":"x","rate_var":"y","q_value":3,"w_value":4,"given_rate":2}
{"mode":"mvt","input":"x^2","x":"x","a":0,"b":2}
{"mode":"cauchy","input":"x^2","g":"x^3","x":"x","a":0,"b":1}
{"mode":"lhopital","input":"sin(x)","g":"x","x":"x","point":"0"}
{"mode":"field","input":"(x+y)dx + (x+2y)dy = 0","x":"x","y":"y"}
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
- The missing-`x` reduction is implemented for second-order equations. When the reduced first-order form is outside the separable/linear/exact/Bernoulli families, the app falls back to a labelled numeric RK4 approximation; for example `y″ = y′ + y²` reduces to `p·dp/dy = p + y²`, which has no symbolic solution in the current engine but is still plotted and marked approximate.
- Higher-order solution graphs require the equation to be linear in its highest derivative so the trajectory can be integrated. Otherwise the graph reports that it cannot be drawn rather than showing a misleading curve.
- Power-series/Frobenius methods are not implemented yet; their curriculum slots show a "coming soon" page.
- Riccati equations need a particular solution or a constant-coefficient reduction.
- Non-elementary integrals remain exact quadratures.
- The UI exposes derivatives up to order 10; the API accepts higher `order` values.
- Series tests are selected from the term's structure; a term that no elementary test recognises falls back to numeric evidence, labelled as such.
- Factorial notation `n!` is supported and the ratio test handles factorial quotients symbolically. Non-integer factorial arguments are evaluated through the Gamma function; no exact closed-form sum is produced for factorial series.
- Only 64-bit Android ABIs are supported.

---

## License

Apache License 2.0. See [`LICENSE`](LICENSE).

Copyright 2026 Infinit Lab contributors.
