# INFINIT — Mathematical Correctness Audit

**Date:** 2026-11-04
**Scope:** `core/` native engine, Flutter app integration, regression suites, documentation.
**Reference checklist:** `/home/RooTx/Downloads/INFINIT_MATHEMATICAL_IMPROVEMENTS.md`

This audit records the root cause of the reported `2XY` crash, the mathematical
and parser fixes, the new ODE features, the tests that lock them down, and the
final verification run. Mathematical correctness was treated as the primary
requirement; step-by-step explanations second; UI/cosmetic work last.

---

## 1. Summary

| Checklist item | Status | Where |
| --- | --- | --- |
| `2XY` Arithmetic Mean crash | Fixed at parser and root-enumeration layers | `core/src/parse.cpp`, `core/src/calculus_apps.cpp` |
| Broad mathematical testing | Executed across every engine mode; results below | Section 11 |
| Step-by-step quality | Improved for first-order, second-order, Wronskian, Clairaut/Lagrange, envelope | `core/src/ode.cpp` |
| First-order ODE workflow | Verified and kept; explanations clarified | `core/src/ode.cpp` |
| Second-order ODEs | Non-homogeneous constant-coefficient support added | `core/src/ode.cpp` |
| Wronskian | Matrix, expansions, numeric cross-check, interpretation | `core/src/ode.cpp` |
| Trajectories | Dedicated orthogonal and oblique backends, API modes, and UI pages | `core/src/ode.cpp`, `core/src/api.cpp`, app |
| Formula layout | `.pretty` text rows replaced by LaTeX-backed inline/block widgets | `app/lib/src/widgets.dart`, `calc_widgets.dart`, `theorems_page.dart` |
| Settings | Language, system/light/dark theme, text size and motion on a dedicated page | `app/lib/src/pages/settings_page.dart`, `app_state.dart` |
| Clairaut vs Lagrange | Two distinct procedures with their own derivations | `core/src/ode.cpp` |
| Envelope | Tangency verification added | `core/src/ode.cpp` |
| Regression tests | Parser, root blow-up, ODE, orthogonal/oblique trajectories, API, UI | `core/test/`, `app/test/` |
| Graph rendering | Engine sampling/domain/field audited; no engine failure reproduced | `core/src/graph.cpp`, `app/lib/src/charts.dart` |
| Documentation | This file and `docs/expressions.md` | repository root |

All changes are in the working tree (uncommitted) at the time of writing.

---

## 2. The `2XY` Arithmetic Mean crash

### 2.1 Exact workflow

`Differential Calculus → Applications of Derivatives → Important Theorems`,
Mean Value Theorem / Arithmetic Mean page, input `2XY`.

### 2.2 Root cause

The crash was a chain of three independent weaknesses:

1. **Implicit multiplication was incomplete.**
   The lexer read `2XY` as the number `2` followed by the identifier `XY`.
   The previous `varVar` regex only handled a narrow set of declared
   single-letter variables. An uppercase multi-letter run such as `XY` was left
   as one symbol instead of `X*Y`, so the expression was interpreted as
   `2·(XY)` with `XY` a free symbol.

2. **The MVT target became identically zero.**
   For `f = 2XY` with `X` and `Y` treated as constants, `f′ = 0`. On `[0, 1]`
   the secant slope is also `0`, so the equation `f′(c) = secant` is
   `0 = 0`. That is true at *every* interior point, not at a finite set.

3. **Root enumeration expanded the whole sample grid.**
   `numericRoots` walked the sampling grid and accepted every sample as a root
   when the target was identically zero. The result was roughly 3999 points.
   Building and rendering thousands of theorem points in the UI caused the
   freeze/crash.

The parser issue alone would also have produced a mathematically wrong answer
(`2XY` should be `2·X·Y`), so it was fixed as well, not just the symptom.

### 2.3 Fixes

**`core/src/parse.cpp`**

- Added `splitImplicitRuns()`, which splits an alphanumeric run into factors
  using the longest protected-name / declared-variable match at each position.
- Added `isProtectedName()` and `containsProtectedMulti()` so the following are
  never split:
  - function names (`sin`, `cos`, `tan`, `atan`, `asin`, `acos`, `exp`, `ln`,
    `log`, `sqrt`, `abs`, `erf`, …),
  - constants (`pi`, `inf`, `e`),
  - differential symbols (`dx`, `dy`, `dt`),
  - the `Deriv` / `Integral` operator names,
  - integration constants such as `C1`, `C2`,
  - declared multi-letter and Greek variable names.
- `naturalizeImplicit()` now supplies a default variable set when `parseExpr`
  has no declared list, so plain app inputs such as `2xy` become `2*x*y`.
- The `atan(x)` / `asin(x)` regression is guarded with a whole-function
  negative lookahead, so the leading `a` is not turned into a factor.

**`core/src/calculus_apps.cpp`**

- Added `hasFreeSymbols()`: a target with symbols other than `x` is not treated
  as a numeric root equation.
- Added `identicallyZero()`: detects `f ≡ 0` on the interval, both symbolically
  and by sampling.
- Added `kMaxRoots = 64` and capped `numericRoots` / `allRoots`.
- In `meanValueTheorems()` and `cauchyMeanValue()`, an identically-zero target
  now reports **one representative interior point** (the midpoint) with an
  explicit step saying the conclusion holds everywhere.
- Theorem point lists are capped at 16 with a warning instead of rendering an
  unbounded list.

### 2.4 Verification

| Input | Interval | Result | Meaning |
| --- | --- | --- | --- |
| `2XY` | `[0,1]` | one point `1/2` | `2·X·Y` is linear in the active variable; every point works, midpoint reported |
| `2xy` | `[0,1]` | no point + warning | `y` remains a free symbol, so the theorem is honestly not applied |
| `x^2` | `[0,2]` | `1` | correct MVT point |
| `x^3-2x` | `[-1,2]` | `1` | correct |
| `sqrt(x)` | `[0,4]` | `1` | correct |
| `1/x` | `[1,3]` | `sqrt(3)` numerically | correct |

The `2XY` case no longer produces a large point list, and the native call
returns immediately.

---

## 3. Implicit multiplication and parser hardening

Representative parses (from the regression suite and manual probes):

| Input | Parsed as | Note |
| --- | --- | --- |
| `2XY` | `2*X*Y` | uppercase variables |
| `2xy` | `2*x*y` | declared lowercase variables |
| `3x^2y` | `3*x^2*y` | mixed exponents |
| `exp(x)` | `exp(x)` | function name intact |
| `atan(x)` | `atan(x)` | leading `a` not split |
| `dx`, `dy`, `dt` | preserved | differentials are bookkeeping |
| `C1`, `C2` | preserved | integration constants |
| `alpha`, `theta` | preserved | multi-letter / Greek names |
| `sinx` | `sin(x)` | natural function-argument form |
| `cos^2(x)` | `(cos(x))^2` | function power form |

`docs/expressions.md` documents the multi-letter implicit-multiplication rules
and the protected names.

---

## 4. Step-by-step explanation quality

### 4.1 Style rules

The whole solution-path layer was rewritten around one rule: **one step is one
real mathematical action**. The revised rules are:

- Coefficient extraction is merged into the type-identification step; the old
  separate `P(x)`, `Q(x)`, `n`, `f(p)` steps are gone.
- Duplicate setup is removed: no repeated “rewrite as `y′ = f(x,y)`”, no
  separate “approach” and “limit laws” steps, no expression-tree step.
- Exactly one final-answer step per method.
- Every step carries the actual expression (or matrix/equation) that the prose
  refers to, in both English and Persian.
- Typical length is 3–6 steps per method; orthogonal and oblique trajectories
  are 2–8.

### 4.2 First-order ODEs

| Method | Steps | Flow |
|---|---:|---|
| Linear | 3 | type + `P,Q` → integrating factor → general solution |
| Separable | 3 | type + `f,g` → separation → both integrals |
| Bernoulli | 4 | type + `n,P,Q` → `v=y^(1−n)` → solve for `v` → return to `y` |
| Exact | 3 | exactness test (both partials in the note) → potential → `u=C` |
| Integrating factor | 5 | differential form → exactness test → `μ` → potential → `u=C` |
| Homogeneous | 5 | type → `y=vx` → separation → integrate → return to `y` |
| Linear substitution | 4 | type → `u=ax+by` → integrate → implicit answer |
| Riccati | 5 | type + `P,Q,R` → particular solution → `y=y₁+1/z` → solve → return |
| Clairaut | 5 | type → `p=y′` → differentiate → general family → envelope |
| Lagrange | 5 | type → `p=y′` → linear equation for `x(p)` → solve → parametric answer |

### 4.3 Trajectories: orthogonal and oblique

Both paths share the same short, explicit skeleton:

1. **Eliminate `c` and form the family ODE** — form the system
   `F = 0`, `F_x + F_y·y′ = 0`; when the slope still contains `c`, show
   `c = …` from `F = 0`, substitute it, and print the resulting family ODE.
2. **Trajectory equation** — for orthogonal trajectories use
   `y′_traj = −1/y′_family`; for oblique trajectories use
   `tan α = (m_t − m)/(1 + m_t m)`, hence
   `m_t = (m + tan α)/(1 − m tan α)`. Print the first-order equation.
3. Solver steps for that equation (typically 2–4).
4. **Trajectory family** — one final-answer step
   (`Orthogonal family` or `Oblique family`).

Vertical families (`F_y = 0`) and horizontal families (slope `0`) short-circuit
to two or three steps: the family type and the trajectory answer. At
`α = 90°` the oblique path reduces to the orthogonal slope `−1/m`.

### 4.4 Second-order equations

- Homogeneous: 3 steps — type, characteristic equation + roots (one step),
  general solution.
- Non-homogeneous: 6 steps — type, characteristic equation + roots,
  homogeneous solution, forcing term + ansatz, coefficients, general solution.

### 4.5 Wronskian, envelope, calculus

| Section | Steps | Flow |
|---|---:|---|
| Wronskian | 4 | matrix (all rows in one note) → determinant expansion → `W` → conclusion |
| Envelope | 3 | family → eliminate `c` → tangency check |
| Derivative | 2 + order − 1 | rules applied → first/second/… derivative |
| Implicit derivative | 4–5 | implicit equation → `F_x` → `F_y` → `y′` → `y″` |
| Integral | 2–4 | method/integrand → antiderivative → bounds/value |
| Limit | 4 | expression + point → left → right → result (laws in the note) |
| Continuity | 5 | function + point → left → right → value → conclusion |
| Behaviour | 4 | function + derivatives → critical points → inflections → monotonicity |
| Tangent/normal | 4 | curve + point → slope → tangent → normal |
| Optimisation | 4 | objective + derivative → candidates → maximum → minimum |
| Related rates | 3 | relation + time derivative → isolate rate → numeric value |
| Mean value theorem | 4 | function + endpoints → secant → solve → point |
| Cauchy | 3 | functions → derivatives + ratio → point |
| L’Hôpital | 3 | quotient → indeterminate check → limit |

All step text is bilingual and is exercised by the native, Python and Flutter
regression suites.

## 5. Second-order non-homogeneous ODEs

### 5.1 What was added

Previously the engine rejected any non-homogeneous higher-order equation with
“Non-homogeneous terms are currently supported only for first-order equations.”
`constCoeffSolution()` was extended and a new undetermined-coefficients pipeline
was added in `core/src/ode.cpp`:

- `solveRationalSystem()` — exact Gaussian elimination over rational
  expressions (no floating-point coefficient drift).
- `binomialSmall()`, `intPow()`, `diffN()` — exact repeated differentiation.
- `shiftedOperator()` — computes `L_k[g]` for `y = e^(kx)·g`, avoiding carrying
  the exponential through every derivative.
- `characteristicMultiplicity()` — detects resonance and repeated roots.
- `decomposeForcingTerm()` — splits a forcing term into
  `e^(kx) · polynomial · cos/sin(bx)`.
- `extractTrigCoeffs()` — separates the cosine and sine coefficients.
- `particularForTerm()` / `particularSolution()` — builds the ansatz, matches
  coefficients, and returns the particular solution.

Supported forcing terms:

- polynomials,
- `e^(kx)`,
- `sin(bx)` and `cos(bx)`,
- finite sums of the above,
- polynomial products such as `x^2·e^(kx)` and `x·cos(bx)`.

Resonance is handled by multiplying the ansatz by `x^s`, where `s` is the
multiplicity of `k + ib` as a characteristic root. Repeated homogeneous roots
are also handled.

### 5.2 Verified cases

Every case below was checked by the engine’s substitution check
(`Substitution into the ODE`), which must be `true`.

| Equation | Solution (plain) | Check |
| --- | --- | --- |
| `y'' - 3*y' + 2*y = exp(x)` | `-x*exp(x) + C*exp(x) + C2*exp(2*x)` | pass |
| `y'' - 3*y' + 2*y = x^2` | `1/2*x^2 + 3/2*x + C*exp(x) + C2*exp(2*x) + 7/4` | pass |
| `y'' + y = cos(x)` | `1/2*x*sin(x) + C*cos(x) + C2*sin(x)` | pass |
| `y'' + y = sin(2*x)` | `-1/3*sin(2*x) + C*cos(x) + C2*sin(x)` | pass |
| `y'' - y = exp(2*x) + x` | `-x + 1/3*exp(2*x) + C*exp(-x) + C2*exp(x)` | pass |
| `y'' - 4*y = 3` | constant particular + exponentials | pass |
| `y'' + 2*y' + y = exp(-x)` | `1/2*exp(-x)*(x^2 + 2*C + 2*C2*x)` | pass |
| `y'' + y = x*cos(x)` | polynomial-times-cosine particular | pass |
| `y'' - y = x^2*exp(x)` | polynomial-times-exponential particular | pass |
| `y'' + y = x^2 + 2*x + 1` | polynomial particular | pass |

### 5.3 Honest limits

Forcing terms outside the supported family are rejected with a message that
lists the supported forms. Variable-coefficient second-order equations (for
example Euler equations) remain out of scope and are still reported as
unsupported rather than mis-solved.

---

## 6. Wronskian

`core/src/ode.cpp` now:

- prints the full Wronskian matrix row by row;
- shows the explicit `2×2` expansion `f·g′ − f′·g`;
- shows the `3×3` cofactor expansion;
- cross-checks the symbolic determinant against a numeric evaluation;
- explains the independence conclusion (“nonzero on an interval ⇒ linearly
  independent there”).

Verified values:

| Functions | `W` | Independent |
| --- | --- | --- |
| `sin(x)`, `cos(x)` | `-1` | yes |
| `x`, `2x` | `0` | no |
| `1`, `x`, `x^2` | `2` | yes |
| `e^x`, `e^(2x)` | `exp(3*x)` | yes |
| `x`, `x*ln(abs(x))` | `x` | yes |
| `x^2`, `x^3` | `x^4` | yes |

The native results match the SymPy oracle in `python/tests/compare_cpp.py`.

---

## 7. Trajectories: orthogonal and oblique

### 7.1 Dedicated features

Orthogonal and oblique trajectories are no longer only a toggle on the envelope
page.

- **Backend:** `solveOrthogonal()` and `solveOblique()` in `core/src/ode.cpp`,
  both built on the shared `solveTrajectory()` pipeline.
- **API:** `{"mode":"orthogonal", ...}` and `{"mode":"oblique","angle":45,...}`.
- **Type names:** `orthogonal` → “Orthogonal trajectories” / «مسیرهای قائم»;
  `oblique` → “Oblique trajectories” / «مسیرهای مایل».
- **UI:** dedicated `EnvelopePage(orthogonalOnly: true)` and
  `EnvelopePage(obliqueOnly: true)` entries at drawer indices 20 and 21, each
  with its own title, subtitle, examples, button, and learning card. The
  envelope switch is hidden on those pages. The oblique page exposes the
  crossing angle as a first-class input.

### 7.2 Procedure and steps

1. Parse the family `F(x,y,c) = 0`.
2. Differentiate implicitly: `F_x + F_y·y′ = 0`.
3. Solve for the family slope `y′ = −F_x/F_y`.
4. Eliminate `c` using the family equation.
5. Replace the family slope: `−1/y′` for orthogonal trajectories, or
   `(m + tan α)/(1 − m tan α)` for oblique trajectories.
6. Solve the resulting first-order ODE with the full first-order pipeline.
7. Return the trajectory family, its explicit/implicit flag, checks, and steps.

Special cases:

- `F_y = 0` with `F_x ≠ 0`: the family has a vertical tangent, so orthogonal
  trajectories are horizontal (`y = C`); oblique trajectories have slope
  `cot α`.
- family slope `0`: orthogonal trajectories are vertical lines (`x = C`);
  oblique trajectories have slope `tan α`.
- family slope still contains `c`: the engine returns the trajectory ODE and a
  warning instead of pretending to have a single family.
- `α = 90°`: the oblique path normalises to the orthogonal case and also fills
  the legacy `orthogonal` fields for compatibility.

### 7.3 Parameter elimination

`solveLinear()` handles families linear in `c`. A new
`quadraticBranches()` helper handles families at most quadratic in `c` with
symbolic coefficients, using the quadratic formula. It also extracts the
explicit `y(x,c)` branches used by the plot. This closes the previous gap for
inputs such as `(y-c)^2 = x`. Only branches that actually remove `c` from the
slope are accepted; if several remain, the first is used with a warning.

### 7.4 Verified orthogonal families

| Family | Orthogonal trajectories |
| --- | --- |
| `x^2 + y^2 = c^2` | `y = C*x` |
| `y = c*x` | `1/2*x^2 + 1/2*y^2 = C` |
| `y = c/x` | `-1/2*x^2 + 1/2*y^2 = C` |
| `y^2 = c*x` | `x^2 + 1/2*y^2 = C` |
| `x^2 = c*y` | `1/4*x^2 + 1/2*y^2 = C` |
| `y = c*x^2` | `1/4*x^2 + 1/2*y^2 = C` |
| `x = c` | `y = C` |
| `y = c` | `x = C` |
| `(y-c)^2 = x` | `y = C + 1/6*(4*x)^(3/2)` (one branch, warned) |

The first two cases are also compared against the SymPy oracle in
`python/tests/compare_cpp.py`.

### 7.5 Trajectory graph

The dedicated pages draw the direction field **before** the step-by-step
solution:

- `ResultView` accepts an optional `beforeSteps` widget and places it after the
  answer card, before the solution path.
- **Native fields:** `trajectory_slope` (and the legacy `orthogonal_slope`) is
  the right side of `y′ = f(x,y)` after `c` is eliminated;
  `family_branches` are the explicit `y(x,c)` branches of the original family.
  Both are serialized by `core/src/api.cpp`.
- **Widget:** `FieldPlot` draws the direction field and RK4 trajectories.
  `extraCurves` overlays dashed members of the original family, sampled through
  `sampleAsync(constants: {c: value})`.
- **Coverage:** circles, lines, hyperbolas, and nonlinear-in-`c` families. A
  vertical family (`x = c`) has horizontal orthogonal trajectories, so it
  exposes slope `0` and is also plotted. A horizontal family (`y = c`) has
  vertical trajectories, which no `y′ = f(x,y)` field can represent; the
  symbolic result is still returned, but the field plot is skipped.
- **Tests:** the widget tests wait for `Orthogonal family` / `Oblique family`
  and then for `field-ready`, so the graph must finish loading before the test
  ends.

### 7.6 Oblique trajectories

The oblique workflow uses the constant crossing angle `α`:

1. `F(x,y,c) = 0` gives the family slope `m = −F_x/F_y`.
2. The angle condition is `tan α = (m_t − m)/(1 + m_t m)`.
3. Solving for the trajectory slope gives
   `m_t = (m + tan α)/(1 − m tan α)`.
4. The engine substitutes `m`, solves `y′ = m_t`, and returns the oblique
   family.

Implementation notes:

- `solveOblique()` accepts an angle in degrees and normalises `90°` to the
  orthogonal case.
- Common angles use exact expressions (`tan 45° = 1`, `tan 30° = 1/√3`,
  `tan 60° = √3`); other angles use a bounded rational approximation.
- The result serializes `trajectory`, `trajectory_explicit`,
  `trajectory_slope`, `trajectory_angle`, and `trajectory_mode`.
- Verified examples: `y = c` at `45°` → `y = x + C`; `x = c` at `45°` →
  `y = x + C`; `x^2 + y^2 = c^2` at `45°` → implicit logarithmic spirals;
  `x^2 + y^2 = c^2` at `90°` → `y = Cx`.

### 7.7 Formula layout and settings

- `MathBlock` already rendered `math.latex`; the remaining linear rows used
  `.pretty` text. `MathInline` now renders those rows with real LaTeX layout in
  the tangent/normal values, related-rates instant, theorem verification rows,
  and point tables.
- The trajectory answer card renders the family with `MathBlock` and an
  explicit/implicit-aware `y = …` or `… = C` form.
- Language and theme controls were removed from the drawer footer and the old
  settings sheet. A dedicated `SettingsPage` (drawer index 22 and the header
  shortcut) now holds:
  - system / light / dark theme;
  - English / فارسی;
  - normal / large text;
  - reduce motion;
  - engine status and restore-defaults.
- `AppSettings` persists `theme=system`, text scale, and reduce-motion flags;
  `MaterialApp.builder` applies the text scale and disables animations when
  requested.

---

## 8. Clairaut and Lagrange

The two forms now use separate procedures instead of one shared algorithm.

### 8.1 Clairaut: `y = x·p + f(p)`, `p = y′`

1. Substitute `p = y′` and differentiate with respect to `x`:
   `p = p + (x + f′(p))·dp/dx`.
2. Therefore `(x + f′(p))·dp/dx = 0`.
3. `dp/dx = 0` gives `p = C` and the general family
   `y = C·x + f(C)`.
4. `x + f′(p) = 0` gives the singular solution
   `x = −f′(p)`, `y = f(p) − p·f′(p)` (the envelope), when it exists.

### 8.2 Lagrange: `y = x·g(p) + f(p)`

1. Substitute `p = y′` and differentiate:
   `p = g(p) + x·g′(p)·dp/dx + f′(p)·dp/dx`.
2. Solve for `dx/dp`:
   `dx/dp − g′(p)/(p − g(p))·x = f′(p)/(p − g(p))`.
3. This is linear in `x(p)`; the integrating factor is
   `exp(∫ −g′/(p−g) dp)`.
4. Integrate to get `x(p)`, then recover `y(p)` from the original equation.
5. The singular case `p = g(p)` is handled separately.

The engine prints the substituted equation, the differentiated equation, the
`x(p)` linear equation, the integrating factor, the integral, and the singular
root analysis.

---

## 9. Envelope verification

`solveEnvelope()` now verifies that a candidate is actually tangent to the
family, not merely a formal elimination artifact:

- computes the implicit slope of the candidate and of the family at
  `c = c(x,y)`;
- declares tangency when the slopes agree symbolically or numerically;
- treats the case where both tangents are vertical as tangent;
- rejects candidates whose slopes do not agree;
- keeps the “no envelope” conclusion when `∂F/∂c = 0` yields only a family
  member or a degenerate curve.

Verified:

| Family | Envelope | Check |
| --- | --- | --- |
| `y = c*x + c^2` | `y = -x^2/4` | tangent |
| `x^2 + y^2 = c^2` | none | correctly rejected |
| `y = c*x - c^2` | `y = x^2/4` | tangent |

---

## 10. Regression tests

### 10.1 Native

`core/test/test_calculus_apps.cpp`

- `2XY → 2*X*Y`, `2xy → 2*x*y`;
- protected function names, differentials, `C1`, Greek/multi-letter names;
- MVT `2XY` returns exactly one point (`1/2`), not thousands;
- Cauchy `2XY` behaves the same way;
- orthogonal API request returns `type_id = orthogonal`, an orthogonal family,
  and the explicit flag;
- oblique API request returns `type_id = oblique`, `trajectory`,
  `trajectory_explicit`, `trajectory_slope`, `trajectory_angle`, and
  `trajectory_mode`;
- step-count contract tests: linear 3, exact 3, integrating factor 5,
  homogeneous second-order 3, non-homogeneous second-order 6, Wronskian 4,
  envelope 3, behaviour 4, tangent 4, optimisation 4, MVT 4, Cauchy 3,
  L’Hôpital 3, related rates 2/3, derivative `order + 1`, limit 4,
  continuity 5.

`core/test/test_regression.cpp`

- second-order non-homogeneous: exponential, polynomial, resonant cosine,
  sum of forcing terms, repeated-root exponential, polynomial-times-cosine;
  every case must pass the substitution check;
- dedicated orthogonal solver: circles, lines, hyperbolas, `x = c`, `y = c`,
  and the nonlinear-in-`c` family `(y-c)^2 = x`;
- dedicated oblique solver: horizontal family (`y = c` → `y = x + C` at 45°),
  vertical family (`x = c` → `y = x + C`), circles at 45° (implicit spirals),
  and the 90° normalisation back to `y = Cx` with the legacy orthogonal fields
  populated;
- oblique step contract: exactly one final `Oblique family` step and no
  duplicated `General solution` step;
- existing Wronskian, envelope, and ODE regression coverage retained.

### 10.2 Python

- `python/tests/test_engine.py` includes envelope, orthogonal, and oblique
  cases. The oblique test checks the exact 45°/60°/90° slopes.
- `python/tests/compare_cpp.py` compares native and SymPy oracle results for
  ODEs, Wronskians, envelopes, orthogonal trajectories, and oblique
  trajectories. The oblique checks verify the native explicit/implicit family
  against the oracle slope at 45° and the 90° orthogonal reduction.

### 10.3 Flutter

- `app/test/native_engine_test.dart` covers first-order, second-order,
  Wronskian, envelope, orthogonal, derivatives, integrals, and async solving.
- `app/test/widget_test.dart` adds a dedicated orthogonal-page test: opens the
  drawer entry, checks the page has no envelope switch, runs the solver, waits
  for the `Orthogonal family` step, and then waits for the direction field
  (`field-ready`) so the graph is covered as well.
- A dedicated oblique-page test opens the new drawer entry, checks the crossing
  angle field and the absence of the envelope switch, runs the solver, waits for
  `Oblique family`, and then waits for `field-ready`.
- A settings test opens the header shortcut, checks the theme/language/system
  choices, switches to light, then system, then Persian, and verifies the
  persisted `AppSettings` values.

---

## 11. Graph rendering investigation

The engine-side graph modes were exercised with diverse inputs:

| Mode | Inputs | Result |
| --- | --- | --- |
| `sample` | `x^2+1`, `2XY`, `2xy`, `sin(x)`, `exp(x)`, `1/x`, `sqrt(x)`, `ln(x)`, `tan(x)`, `x*atan(x)`, `1/(x^2-1)`, `x^0.5`, `abs(x)`, `x^3-3*x+1` | all `ok`; non-finite values become `NaN` |
| `domain` | same set | polynomial/trig/rational/log/sqrt domains and ranges reported |
| `field` | `y' = x+y`, `y' = y^2`, `y' = -x/y`, `y' = sin(x)*cos(y)`, orthogonal slopes `y/x`, `-x/y`, `x/y`, `sqrt(4*x)`, `0`, oblique slopes `(y/x + 1)/(1 - y/x)` | all `ok`, slopes and curves returned |

Findings:

- `sampleFunction()` already emits `NaN` for undefined samples; the UI’s
  `charts.dart` breaks the path at non-finite points.
- The chart already has a `jumpLimit` that starts a new path segment when the
  screen-space jump is too large, preventing a false vertical line across a
  pole.
- The y-range uses 2%–98% percentile clipping when the user has not fixed the
  range, so a pole does not flatten the rest of the plot.
- No engine-side graph failure was reproduced. The most likely cause of the
  reported “graph rendering failure” was the same unbounded root list from the
  `2XY` MVT path or another non-finite value; the root list is now bounded and
  non-finite samples are handled explicitly.

No graph-engine code change was required; the audit documents the behavior and
the existing safeguards.

---

## 12. Broad mathematical testing matrix

Representative probes (all `ok: true` unless marked):

| Mode | Inputs | Outcome |
| --- | --- | --- |
| derivative | `2XY`, `2xy`, `3x^2y`, `x/(1+x^2)`, `-x^2+0.5x`, `sin(x^2)` (2nd), `exp(-x)*cos(2x)` (3rd), `x^x`, `1/x` | correct symbolic derivatives |
| integral | `2XY`, `2xy`, `x^2+3x-1`, `1/(1+x^2)`, `exp(-x^2)`, `sqrt(x)`, `x*ln(x)`, `1/x`, `sin(x)^2`, `x/(x^2+1)` | correct antiderivatives, including `erf` and `atan` |
| definite integral | `x^2` on `[0,1]`, `1/x` on `[1,2]`, `1/x` on `[-1,1]`, `exp(-x)` on `[0,5]`, `sin(x)` on `[0,π]` | finite values where they exist; `1/x` on `[-1,1]` flagged improper/divergent |
| limit | `sin(x)/x` at `0`, `(1+1/x)^x` at `∞`, `1/x` at `0` from each side, `(x^2-1)/(x-1)` at `1`, `exp(x)` at `−∞`, `tan(x)` at `π/2` | correct finite/infinite/one-sided results; `tan` at `π/2` reported as non-existent with `±∞` sides |
| continuity | `1/x` at `0`, `(x^2-1)/(x-1)` at `1`, `abs(x)` at `0`, `x^2` at `2`, `tan(x)` at `π/2` | removable vs infinite discontinuity identified correctly |
| behaviour | `2XY`, `x^3-3x+1`, `1/x`, `x/(x^2+1)`, `sqrt(x)`, `ln(x)`, `tan(x)`, `x^4-2x^2+0.5` | critical points, inflections, pieces, domain behavior returned |
| optimize | `x^2-4x+1`, `-x^2+2x`, `x^3-3x`, `1/x`, `sin(x)` | extrema on the requested window |
| tangent | `x^2` at `1`, `1/x` at `2`, `sin(x)` at `0`, `x^3-2x` at `-1`, `sqrt(x)` at `4` | correct slopes and tangent/normal lines |
| MVT | `2XY`, `2xy`, `x^2`, `x^3-2x`, `sqrt(x)`, `1/x` | correct points or honest warning |
| Cauchy MVT | `x^2/x^3`, `sin(x)/cos(x)`, `2XY/x` | correct points |
| L'Hôpital | `sin(x)/x`, `(1-cos(x))/x^2`, `exp(x)-1` over `x`, `ln(x)` over `x-1` | `1`, `1/2`, `1`, `1` |
| Wronskian | six function sets | values match the oracle |
| envelope | line family, concentric circles | envelope or correct “none” |
| orthogonal | nine families | see Section 7.4 |
| oblique | `y = c`, `x = c`, `y = c*x`, `x^2 + y^2 = c^2` at 45°/90° | explicit or implicit family, correct slope, single final answer |

Undefined and boundary cases are reported honestly rather than coerced into a
finite answer: divergent integrals set `divergent`, one-sided infinite limits
set `left_infinite`/`right_infinite`, and free-symbol MVT targets warn instead
of inventing a point.

---

## 13. Final verification

Run from the repository root unless noted.

| Command | Result |
| --- | --- |
| `./scripts/build_core.sh` | success, static and shared libraries built |
| `ctest --test-dir core/build --output-on-failure` | **6/6 suites passed** |
| `cd python && python3 -m pytest -q` | **34 passed** |
| `python3 python/tests/compare_cpp.py` | **68 passed, 1 skipped, 0 failures** |
| `cd app && flutter analyze` | **No issues found** |
| `cd app && flutter test` | **32/32 passed** |

The skipped oracle case is the pre-existing Riccati case that the oracle cannot
solve; it is marked as a skip, not a failure.

---

## 14. Known limitations and follow-ups

1. **Symbolic parameter elimination** covers families at most quadratic in `c`.
   Higher-degree families still fall back to returning the trajectory ODE with
   a warning. A resultant-based elimination would remove this limit.
2. **Multiple trajectory branches** (for example a family with several `c`
   branches) return one branch with a warning; presenting all branches would
   require a richer result shape and UI.
3. **Oblique angles** use exact expressions for 30°/45°/60° and a bounded
   rational approximation otherwise. Angles whose trajectory ODE is
   non-elementary (for example `y = c*x` at 60°) return an unevaluated
   `Integral(...)` with `closed_form=false` instead of inventing a closed form.
4. **Oblique oracle coverage** verifies the slope relation and the native
   family numerically; the 60° line family has a non-elementary quadrature, so
   it is checked by the native regression/API tests rather than by dsolve.
5. **Non-homogeneous forcing** is limited to polynomials, exponentials,
   sine/cosine, finite sums, and polynomial products. Other forcing terms are
   rejected explicitly.
6. **Variable-coefficient second-order equations** (Euler equations, series
   solutions) remain out of scope.
7. **Graph rendering** was verified at the engine and widget layers; if a
   platform-specific rendering issue remains, it is outside the native math
   engine and should be reproduced with a specific expression and window.

---

## Appendix — files changed

| File | Change |
| --- | --- |
| `core/src/parse.cpp` | implicit multiplication, protected names, default variables |
| `core/src/calculus_apps.cpp` | identically-zero detection, root caps, MVT/Cauchy representative point |
| `core/src/ode.cpp` | second-order non-homogeneous, Wronskian, Clairaut/Lagrange, envelope tangency, shared `solveTrajectory()` with orthogonal and oblique solvers |
| `core/src/solver.hpp` | `solveOrthogonal()`, `solveOblique()`, `trajectory*` fields, `orthogonalSlope`, `familyBranches` |
| `core/src/api.cpp` | `orthogonal`/`oblique` dispatch, `trajectory*` and `orthogonal_slope`/`family_branches` serialization |
| `core/test/test_calculus_apps.cpp` | parser, root blow-up, orthogonal and oblique API regressions |
| `core/test/test_regression.cpp` | non-homogeneous, orthogonal, oblique, nonlinear-parameter regressions |
| `app/lib/src/pages/envelope_page.dart` | envelope/orthogonal/oblique modes, angle input, examples, graph before steps |
| `app/lib/src/pages/settings_page.dart` | dedicated language/theme/display settings page |
| `app/lib/src/app_state.dart` | system/light/dark theme, text scale, reduce motion, defaults |
| `app/lib/src/widgets.dart` | `MathInline`, trajectory answer card, `ResultView.beforeSteps` |
| `app/lib/src/calc_widgets.dart` | point table rendered with `MathInline` |
| `app/lib/src/pages/theorems_page.dart` | verification rows rendered with `MathInline` |
| `app/lib/src/charts.dart` | `FieldPlot.extraCurves`, `PlotCurveSpec.constants` |
| `app/lib/src/native_engine.dart` | `oblique`/`obliqueAsync`, `sampleAsync(constants:)` |
| `app/lib/src/models.dart` | `trajectory*` fields, `orthogonalSlope`, `familyBranches` |
| `app/lib/main.dart` | trajectory and settings drawer entries, cases 20–22, settings shortcut |
| `app/test/widget_test.dart` | orthogonal, oblique, and settings widget tests |
| `python/infinit_engine/engine.py` | SymPy `oblique()` reference slope |
| `python/tests/compare_cpp.py` | native ↔ oracle checks for oblique trajectories |
| `python/tests/test_engine.py` | exact 45°/60°/90° oblique slope tests |
| `docs/api.md` | oblique mode and `trajectory*` fields |
| `docs/architecture.md` | settings page, formula layout, oblique oracle coverage |
| `docs/expressions.md` | multi-letter implicit multiplication documentation |
