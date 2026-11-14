# Reserved curriculum slots — algorithms and implementation plan

Status: **engine, UI and 12-language catalogue implemented and tested.** All 31
reserved ids are dispatched by the native engine, rendered by the shared
`TopicWorkbenchPage`, reachable from the drawer and covered by C++/Python and
Flutter tests. `kComingSoonIndices` is empty; section 1.3 records the delivered
engine, section 1.4 the wiring contract and section 11.1 the delivered UI.

This document defines, for every reserved slot, the mathematics the page
computes, the inputs and outputs, the checks that must pass, and where the code
lives. It is written for the next agent as much as for the author, so it also
records the wiring steps that turn a specification into a working page.

Index numbers are the stable menu indices in `app/lib/main.dart`. Every id
mentioned here is a `topic` id in `app/lib/src/topic_catalog.dart`, unless it
says "reuse".

---

## 1. Ground rules

The rest of the engine follows these rules; the new modules must not break them.

1. **No invented answers.** If a symbolic form cannot be produced, return the
   honest numerical or quadrature form and set `closedForm = false`.
2. **Every answer is checked.** Symbolic answers are differentiated and
   substituted back. Numerical answers get a residual and a step-halving
   comparison. A check that cannot run is reported as "not run", never as
   "passed".
3. **Exact first, numeric second.** Rationals, integers and `pi` multiples stay
   symbolic (`Rational`, `Num`, `Sym("pi")`). Only genuinely numeric quantities
   (roots of transcendental equations, quadrature, eigenvalues of a general
   matrix) become decimals via `numStr`/`smartNum`.
4. **No state.** Each topic request is independent, like `ode_solve`.
5. **English and Persian both come from the engine.** New step titles and notes
   go through `localize.hpp` / the existing `step.*` catalogue, not through
   hard-coded prose in Dart.
6. **The UI is a renderer.** Tables, series terms and grids travel as structured
   JSON; the Flutter page must not recompute mathematics.

### 1.1 Delivered modules

| File | Contents |
|---|---|
| `core/src/series_solutions.cpp/.hpp` | 32, 33, 34, 111, 35, 36 |
| `core/src/linear_systems.cpp/.hpp` | 75, 76, 77, 78, 79, 80 (the vector RK4 folded in here) |
| `core/src/dynamical_systems.cpp/.hpp` | 81, 82, 83, 84, 112 |
| `core/src/bvp.cpp/.hpp` | 85, 90, 91, 92 |
| `core/src/special_functions.cpp/.hpp` | shared Bessel, Legendre, Laguerre and Chebyshev values, quadrature rules and `symmetricEigen` |
| `core/src/special_series.cpp/.hpp` | 93, 94, 95 |
| `core/src/pde_series.cpp/.hpp` | 86, 87, 88, 89, 113 |
| `core/src/calculus_topics.cpp/.hpp` | 96, 97 |
| `core/src/topics.cpp` | dispatcher entries only |

`numeric_systems.cpp` was not needed: the vector RK4 lives in
`linear_systems.cpp` with the rest of slot 80's work. `special_functions` was
split out because 93/94/95, 35/36 and the quadrature rules share the same
special-function values.

The existing `topics.cpp` is already ~5000 lines. New mathematics must not be
appended to it; only the dispatcher switch grows.

### 1.2 Reuse map

Do not rewrite what already exists. These handlers are the starting points:

| Existing handler | Reuse for |
|---|---|
| `matrixEigen` | 76, 77, 78, 112 |
| `criticalPoints` | 81, 112 |
| `phasePortrait` | 76, 81, 112 |
| `fourierSeries` | 86, 87, 88, 89 |
| `heatEquation` | 88, 113 |
| `waveEquation` | 89 |
| `separationPde` | 88, 89, 91 |
| `numericalOde` (scalar RK4) | 80 (vector extension) |
| `Integral` / `taylor` modes | 96, 97 |

### 1.3 Implementation status (engine pass, complete)

The engine half of this document is delivered. Every reserved id returns
`ok = true` with its default inputs, every default check passes, and the
following CTest targets are green:

`algebra`, `ode`, `regression`, `calculus`, `limits`, `calculus_apps`,
`calculus_more`, `pde`, `series`, `linear`, `dynamical`, `bvp`, `special`,
`pde_series`, `calculus_topics`.

`python/tests/test_curriculum_topics.py` sweeps all 31 ids through the C ABI and
checks the numerically reported values against SymPy/mpmath.

**Deliberate limitations**

- **Exact where possible, honest elsewhere.** Series coefficients, Euler
  equations, eigenvalues of small rational systems and indicial roots stay
  rational. Cases the engine cannot represent exactly are refused with a stable
  code rather than approximated silently: `irrational_indicial_roots`,
  `complex_indicial_roots`, `irrational_eigenvalues`,
  `unsupported_eigenvalue_pattern`, `resonance_not_solvable`,
  `degenerate_operator`, `not_singular_endpoint`, `not_regular_singular`,
  `unknown_bc`, `bad_interval`, `bad_expression`, `bad_point`,
  `coefficient_not_admissible`.
- **Numeric evidence is labelled.** Bessel/Legendre values, vector RK4, limit
  cycles, the Lorenz attractor, finite-difference BVPs, the singular
  Sturm–Liouville discretisation and special-series coefficients set
  `closedForm = false` and `numericOnly = true`. Their checks are residual,
  convergence or invariant comparisons, not proofs.
- **Slot 92** uses the self-adjoint form `-(p y')' + q y = f` (default `p = 1`),
  lifts inhomogeneous Dirichlet data with a linear function, detects resonant
  modes and refuses `p ≡ 0` with `degenerate_operator`.
- **Slot 93** discretises on a cell-centred grid whose faces lie on the
  endpoints, so a singular endpoint contributes zero flux (the natural bounded
  condition) while a regular endpoint keeps the Dirichlet truncation.
  Non-Bessel families use the same custom discretisation.
- **Slots 94 and 95** report the partial sum at the requested `x0` (default the
  interval midpoint). The Parseval check includes the truncation-tail energy, so
  every `N` is accepted.
- **Slot 86** reports the finite-`N` Gibbs overshoot; it approaches `0.08949` as
  `N` grows. The accepted band is `0.02–0.115`, not an absolute equality.
- **Slots 88, 89 and 113** verify the initial data by requiring the error to
  decrease from `N/2` to `N` terms. Endpoint jumps are excluded from the grid.
  `113` supports `insulated`, `dirichlet`, `fixed_nonzero`, `mixed` and
  `robin` (Robin is insulated left plus `u_x(L) + h u(L) = 0`, with roots of
  `mu tan(mu L) = h`).
- **Slots 96 and 97 are dedicated tools.** They reuse `diff`, `integrate`,
  `taylorSeries` and `quadratureAdaptive`; they are not article pages with an
  existing engine call. Slot 97 classifies the detected technique and supplies
  the default example for each technique.
- **Slot 33** deliberately sets `typeId` to the classification
  (`ordinary_point`, `regular_singular_point`, `irregular_singular_point`) so
  the result card can show the verdict; the Python sweep allows that one
  exception to the “type id equals topic id” rule.
- **Slot 34** changed its default to `p = -1/(2*x)`, `q = (1+x)/(2x^2)`, which
  really has a regular singular point at `x0 = 0`; the earlier default did not
  exercise the method.
- **Slot 35 (Legendre)** is still absent from the Series drawer. Adding it is a
  UI change (`kComingSoonIndices` and the drawer list), not an engine change.

### 1.4 Wiring a slot (checklist)

1. Implement the engine handler and register the topic id in
   `core/src/topics.cpp`.
2. Add a `TopicSpec` (inputs, defaults, labels, examples) to
   `app/lib/src/topic_catalog.dart`.
3. Add `_topicPages[index] = id` in `app/lib/main.dart` and wire the drawer
   entry with `topic(index, icon, 'topic.<id>.title')`. Do not reuse an index
   for a different topic and do not leave the slot in `kComingSoonIndices`.
4. Add `topic.<id>.title`, `.blurb`, `.field.*.label` and every new `step.*` /
   `check.*` key to all 12 catalogues in `app/assets/i18n/`. The parity test
   must stay green.
5. Add a widget test that renders the page and, for one representative per
   module family, runs the default example and waits for the result key; add
   an engine test with an exact expected value.
6. Rebuild `core/build/lib/libodecore.so` and run `ctest` plus
   `python3 -m pytest` and `flutter test`.

---

## 2. Series solutions and special functions

### 32 — Power series solutions (`series_power`)

**Input**: second-order linear ODE with polynomial coefficients, expansion point
`x0`, number of terms `N` (default 8).

**Output**: two independent series solutions, the recurrence, the guaranteed
radius, and the residual.

**Algorithm**

1. Write the equation as `y'' + p(x)y' + q(x)y = 0`, or keep
   `a2 y'' + a1 y' + a0 y = 0` and divide by `a2` formally.
2. Check `a2(x0) != 0`. If not, refuse and point to section 33/34.
3. Shift `t = x - x0`; expand `p` and `q` in Taylor series up to `N`:
   `P(t) = Σ P_k t^k`, `Q(t) = Σ Q_k t^k`. For polynomial coefficients this is
   exact; for rational coefficients with a removable singularity, cancel first.
4. Set `y = Σ a_n t^n`. Collect the coefficient of `t^k`:
   `(k+2)(k+1) a_{k+2} + Σ_{j=0}^{k} [(j+1) a_{j+1} P_{k-j} + a_j Q_{k-j}] = 0`.
5. Generate `y1` with `(a0, a1) = (1, 0)` and `y2` with `(0, 1)`.
6. Radius: distance from `x0` to the nearest zero of `a2` in the complex plane,
   rounded down; for polynomial `a2` compute the roots or bound them.
7. Return `y = C1 y1 + C2 y2 + O(t^{N+1})`.

**Checks**: substituting the truncated series into the ODE leaves only terms of
order `> N`; `y1(x0) = 1, y1'(x0) = 0`, and the reverse for `y2`.

**Notes**: if `p` or `q` is not analytic at `x0`, this is not the right tool.
Do not silently truncate a Laurent series.

### 33 — Ordinary and singular points (`series_singular_points`)

**Input**: `p(x)`, `q(x)` and a point `x0`.

**Output**: one of `ordinary`, `regular singular`, `irregular singular`, plus
the order of each pole and the indicial equation when regular singular.

**Algorithm**

1. If `p`, `q` are rational, reduce each to `P/Q` and factor the denominator.
2. `x0` is ordinary when neither denominator vanishes at `x0`.
3. Otherwise compute the pole orders `m_p`, `m_q` of `p`, `q` at `x0`.
4. Regular singular iff `m_p <= 1` and `m_q <= 2`, equivalently
   `(x-x0)p(x)` and `(x-x0)^2 q(x)` have finite limits at `x0`.
5. For regular singular, compute
   `p0 = lim (x-x0)p(x)`, `q0 = lim (x-x0)^2 q(x)` and the indicial equation
   `r(r-1) + p0 r + q0 = 0`.
6. Report the nearest other singular point as a lower bound on the radius.

**Checks**: recompute each limit by substituting `x0 + h` with `h -> 0`
symbolically or by a one-sided numeric limit with two step sizes.

### 34 — Frobenius method (`series_frobenius`)

**Input**: ODE, regular singular point `x0`, number of terms `N`, optional
choice of root.

**Output**: indicial roots, recurrence, one or two series solutions, and the
validity note when a logarithm is required.

**Algorithm**

1. Verify regular singularity using section 33. Refuse otherwise.
2. Compute `p0, q0` and the indicial roots `r1 >= r2` (real part order).
3. Assume `y = (x-x0)^r Σ a_n (x-x0)^n`, `a0 = 1`.
4. Substitute and collect the lowest power; the coefficient of `a0` is the
   indicial polynomial. Solve it.
5. For a root `r`, the recurrence for `a_n(r)` is linear:
   `a_n = -[Σ_{j<n} a_j ( ... )] / I(r+n)`, where `I` is the indicial
   polynomial. Derive the numerator from the shifted series of `p` and `q`.
6. If `r1 - r2` is not an integer, generate both series. If it is an integer,
   generate the first series and either the second series when the recurrence
   permits it, or the logarithmic form with the reduction-of-order integral.
7. Report the radius from section 33.

**Checks**: each generated series satisfies the ODE to the requested order; the
indicial polynomial vanishes at both roots.

**Notes**: the logarithmic case is where most implementations cheat. Return
`closedForm = false` and an explicit `log|x-x0|` term rather than dropping it.

### 111 — Euler (Cauchy–Euler) equations (`euler_equations`)

**Input**: coefficients `a_k` of `Σ a_k x^k y^{(k)} = f(x)` for order 2 or 3;
optional forcing.

**Output**: homogeneous solution, particular solution, general solution.

**Algorithm**

1. Substitute `y = x^r`. For order `n`, obtain
   `P(r) = Σ a_k r(r-1)...(r-k+1) = 0`.
2. Factor `P` over the reals:
   - distinct real roots `r`: `x^r`;
   - repeated root of multiplicity `m`: `x^r, x^r ln|x|, ..., x^r (ln|x|)^{m-1}`;
   - complex pair `α ± iβ`: `x^α cos(β ln|x|)`, `x^α sin(β ln|x|)`.
3. For nonhomogeneous equations, use `x = e^t` (`x > 0`) to reduce to constant
   coefficients, solve, then substitute back; or use variation of parameters.
4. State the interval (`x > 0` or `x < 0`) because the formula uses `ln|x|`.

**Checks**: substitute each basis function into the homogeneous equation;
differentiate the particular solution and compare with `f`.

**Notes**: this is a genuine gap in `ode.cpp` today, which handles constant
coefficients only. It can be implemented inside `series_solutions.cpp` and
exposed both as a topic and as a new `ode` type.

### 35 — Legendre equations and polynomials (`special_legendre`)

**Input**: degree `n` (0–10), evaluation point `x` in `(-1, 1)`, number of
derivative checks.

**Output**: `P_n(x)`, the equation it solves, orthogonality check, and the
second solution `Q_n` when requested.

**Algorithm**

1. Build `P_0 = 1`, `P_1 = x`, then
   `(n+1) P_{n+1} = (2n+1) x P_n - n P_{n-1}` with exact rational arithmetic.
2. Evaluate at `x`; optionally return the polynomial coefficients.
3. Verify `(1-x^2) P_n'' - 2x P_n' + n(n+1) P_n = 0`.
4. `Q_n` for `|x| < 1`:
   `Q_n = P_n(x) * 1/2 ln((1+x)/(1-x)) - W_{n-1}(x)`, where `W_{n-1}` is the
   standard polynomial part; compute it from the recurrence rather than a
   closed table.
5. Orthogonality: numeric `∫_{-1}^{1} P_m P_n dx = 2/(2n+1) δ_{mn}`.

**Checks**: the ODE residual at three interior points; the norm integral.

**Notes**: `n` must be a non-negative integer. Reject fractional `n` here and
send it to section 36 (Bessel) or the general Frobenius tool.

### 36 — Bessel equations and functions (`special_bessel`)

**Input**: order `ν >= 0`, evaluation point `x > 0`, number of series terms.

**Output**: `J_ν(x)`, `Y_ν(x)` when defined, series form, recurrence, zeros
when requested.

**Algorithm**

1. `J_ν(x) = Σ_{k=0}^{N} (-1)^k / (k! Γ(k+ν+1)) (x/2)^{2k+ν}`.
   For integer `ν = n`, use `(k+n)!` instead of `Γ`.
2. `Y_n` for integer `n`: use the standard limit form
   `Y_n = (2/π) J_n ln(x/2) - (1/π) Σ ...`; for non-integer `ν`,
   `Y_ν = (J_ν cos νπ - J_{-ν}) / sin νπ`.
3. Evaluate with `lgamma` for the gamma factors; keep the series symbolic when
   `ν` is a small integer and `N` is small enough.
4. Zeros: bracket sign changes on a grid and refine with bisection/Newton.
5. Verify the Bessel equation `x^2 y'' + x y' + (x^2 - ν^2)y = 0` numerically.

**Checks**: ODE residual at several points; `J_0(0) = 1`; recurrence between
`J_{n-1}, J_n, J_{n+1}`.

**Notes**: never present a numeric value as exact. `Y_ν` has a log singularity
at zero; state the domain.

---

## 3. Linear systems

### 75 — Basic theory of linear systems (`system_theory`)

**Input**: constant matrix `A` (2×2 or 3×3), or a fundamental set of vector
solutions.

**Output**: fundamental matrix, Wronskian, linear independence verdict, and the
general solution.

**Algorithm**

1. Parse `A`. Build the fundamental matrix `Φ(t) = e^{At}` (section 77) or take
   the supplied solution vectors.
2. Compute `W(t) = det Φ(t)`.
3. The set is independent at `t0` iff `W(t0) != 0`; for solutions of a linear
   system this is independent of `t0` (Abel's identity
   `W(t) = W(t0) exp(∫ tr A)`), so check at `t = 0`.
4. Report `x(t) = Φ(t) c`.

**Checks**: `Φ(0) = I`; `Φ' = AΦ`; `W(t)` matches Abel's formula.

### 76 — Complex eigenvalues (`system_complex_eigen`)

**Input**: real matrix `A`.

**Output**: complex eigenpairs, the two real solutions, and the general real
solution.

**Algorithm**

1. Compute the characteristic polynomial and its roots (reuse
   `matrixEigen`/`eigenvalueRoots`).
2. For `λ = α + iβ` with eigenvector `v = a + ib`,
   `e^{λt} v = e^{αt}[(a cos βt - b sin βt) + i(a sin βt + b cos βt)]`.
3. Return the real and imaginary parts as the two real solutions.
4. If a real eigenpair also exists, include it.

**Checks**: `A x = x'` for each returned vector; conjugate symmetry; the
characteristic polynomial vanishes at each eigenvalue.

### 77 — Fundamental matrices (`system_fundamental`)

**Input**: constant matrix `A`, optional `t0`.

**Output**: `Φ(t) = e^{At}` symbolically when possible, otherwise a numeric
matrix for the requested `t`.

**Algorithm**

1. If `A` is diagonalizable: `Φ(t) = V diag(e^{λ_i t}) V^{-1}`.
2. If defective: group each eigenvalue into Jordan blocks and use
   `e^{At} = Σ_λ e^{λt} Σ_{k=0}^{m-1} (A-λI)^k t^k / k!`.
   The sum is finite because the nilpotent part has index `m`.
3. For a general numeric matrix, use scaling-and-squaring with a Padé
   approximation only when the symbolic route fails; label it approximate.
4. Verify `Φ(0) = I` and `Φ' = AΦ` symbolically or numerically.

**Checks**: both matrix identities; determinant equals `exp(tr(A) t)`.

**Notes**: exact `e^{At}` for a defective matrix is a common gap. The Jordan
form route is preferred over a truncated series.

### 78 — Repeated eigenvalues (`system_repeated`)

**Input**: matrix `A` with a repeated eigenvalue.

**Output**: algebraic and geometric multiplicity, generalized eigenvectors,
the solution basis, and whether the matrix is defective.

**Algorithm**

1. Compute `λ`, `alg`, `geom` (already in `matrixEigen`).
2. If `geom = alg`, return the ordinary eigenvectors.
3. Otherwise build a chain: pick `v_1` in `ker(A-λI)^m` but not in
   `ker(A-λI)^{m-1}`, then solve `(A-λI) v_{k+1} = v_k` down to `v_1`.
4. Solutions: `e^{λt} v_1`, `e^{λt}(t v_1 + v_2)`, ...,
   `e^{λt}(t^{m-1}/(m-1)! v_1 + ... + v_m)`.
5. Report the Jordan block sizes.

**Checks**: `(A-λI) v_{k+1} = v_k`; each solution satisfies `x' = Ax`; the
number of independent solutions equals the matrix size.

### 79 — Nonhomogeneous systems (`system_nonhomogeneous`)

**Input**: `A`, forcing vector `f(t)` (components as expressions), initial
condition optional.

**Output**: homogeneous solution, particular solution, general solution, and
the IVP solution when initial data is supplied.

**Algorithm**

1. Build `Φ(t)` (section 77).
2. Variation of parameters:
   `x_p(t) = Φ(t) ∫ Φ(s)^{-1} f(s) ds`.
3. When `f` is polynomial, exponential, sine or cosine, also try undetermined
   coefficients; detect resonance when a forcing frequency matches an
   eigenvalue and multiply by `t` (or `t^k`).
4. Apply `x(t0) = x0` by solving `Φ(t0) c = x0 - x_p(t0)`.

**Checks**: differentiate `x_p` and substitute into `x' = Ax + f`; check the
initial condition.

**Notes**: keep `Φ^{-1}` symbolic only for 2×2; for 3×3 and larger use the
adjugate/determinant form and simplify.

---

## 4. Nonlinear systems and stability

### 81 — Competing species (`competing_species`)

**Input**: coefficients `a, b, c, d, e, f` of
`x' = x(a - bx - cy)`, `y' = y(d - ex - fy)`; plotting window.

**Output**: equilibria, Jacobian at each, classification, nullclines, and a
phase portrait with sample trajectories.

**Algorithm**

1. Solve `x(a - bx - cy) = 0` and `y(d - ex - fy) = 0` exactly (four candidates,
   keeping only non-negative points).
2. For each equilibrium compute the Jacobian and its eigenvalues.
3. Classify: node/saddle/spiral/center and stability.
4. Nullclines: `x = 0`, `a - bx - cy = 0`, `y = 0`, `d - ex - fy = 0`.
5. Integrate several initial conditions with RK4 for the portrait.
6. Coexistence condition for the interior equilibrium: compare `a/b` with
   `d/e` and `a/c` with `d/f`.

**Checks**: each equilibrium satisfies both right-hand sides to `1e-10`;
eigenvalues satisfy the characteristic polynomial; trajectories conserve the
reported direction.

### 82 — Lyapunov's second method (`liapunov`)

**Input**: system `x' = f(x,y)`, equilibrium, optional candidate `V(x,y)`.

**Output**: `V`, `V' = ∇V·f`, definiteness verdict, and stability conclusion.

**Algorithm**

1. If `V` is omitted, solve the Lyapunov equation `A^T P + P A = -I` for the
   linearization at the equilibrium and use `V = (x-x0)^T P (x-x0)`.
2. Check `V(x0) = 0` and positive definiteness near the point:
   - quadratic form: Sylvester's criterion on the symmetric part;
   - general `V`: sample a small circle and require `V > 0` (report as numeric
     evidence, not proof).
3. Compute `V'` symbolically and simplify.
4. If `V' <= 0` everywhere near the point, conclude stable; if `V' < 0` except
   at the point, asymptotically stable; if `V' > 0`, unstable; otherwise
   inconclusive.
5. State explicitly that a failed candidate proves nothing.

**Checks**: `V(x0) = 0`; `V'` computed by automatic differentiation matches a
finite-difference gradient.

### 83 — Limit cycles (`limit_cycle`)

**Input**: planar system, initial point, time span, maximum periods.

**Output**: numeric evidence of a closed orbit, estimated period, plot, and the
Poincaré map values.

**Algorithm**

1. Integrate with RK4 and adaptive step control.
2. Detect a return: find the first `T > 0` with `x(T) ≈ x(0)` and a transversal
   crossing of the initial ray.
3. Refine `T` by solving the scalar equation `(x(T) - x(0)) · n = 0` with a
   secant/bisection method, where `n` is normal to the flow at `x(0)`.
4. Check that nearby initial conditions spiral toward or away from the orbit
   (stable/unstable limit cycle).
5. Report "no closed orbit detected in the window" when the map has no root.

**Checks**: the residual `|x(T) - x(0)|`; the orbit does not collapse to an
equilibrium; the period is positive.

**Notes**: this is numerical evidence, not a proof. A negative result cannot
exclude a cycle outside the window. Say so in the output.

### 84 — Chaos and strange attractors (`chaos_attractor`)

**Input**: system id (`lorenz`, `rossler`, `duffing`), parameters, initial
condition, time span, step.

**Output**: trajectory table, 2D/3D plot, finite-time maximal Lyapunov
exponent, and a nearby-orbit separation curve.

**Algorithm**

1. Integrate the system with RK4 at a fixed step; keep the full trajectory.
2. Integrate a second orbit with initial separation `δ0 = 1e-8`.
3. Every fixed renormalisation time, compute
   `λ_i = ln(|δ_i|/δ0) / t_i`, then renormalise the separation back to `δ0`.
4. Estimate `λ_max` as the average of `λ_i` over the second half of the run.
5. Report the separation curve; if it grows exponentially, the estimate is
   meaningful; if it saturates, explain that the attractor is bounded.

**Checks**: the known equilibria of Lorenz/Rossler are fixed points of the
right-hand side; step halving changes the trajectory by less than the reported
tolerance for short times.

**Notes**: chaos is a numerical statement here. Never label a short simulation
as a proof. The Lyapunov exponent is finite-time and must be reported with its
window.

### 112 — Locally linear systems (`local_linearization`)

**Input**: nonlinear system `x' = f(x,y)`, `y' = g(x,y)`, search window.

**Output**: equilibria, Jacobian at each, linear classification, nonlinear
classification note, and a comparison plot.

**Algorithm**

1. Find equilibria with the existing `criticalPoints` Newton grid.
2. Compute the Jacobian symbolically and evaluate at each equilibrium.
3. Classify the linearization with `phasePortrait`.
4. If the linearization is hyperbolic (no zero real part), the nonlinear
   classification agrees; otherwise say the linear test is inconclusive.
5. Plot both the nonlinear field and the linear field near the equilibrium.

**Checks**: `f, g` vanish at each reported point; the Jacobian matches finite
differences; eigenvalues satisfy the characteristic polynomial.

---

## 5. Numerical methods for systems

### 80 — Numerical methods for systems (`system_rk4`)

**Input**: right-hand sides `f(t,x,y)`, `g(t,x,y)`, initial condition, `t0`,
`T`, step `h`, method (`euler`, `heun`, `rk4`).

**Output**: step table, final state, plot, and a step-halving error estimate.

**Algorithm**

1. Vectorise the existing scalar RK4: each stage updates `(x,y)` together.
2. Run with `h` and `h/2`; compare the final states.
3. Estimate the observed order `p = log2(|y_h - y_{h/2}| / |y_{h/2} - y_{h/4}|)`
   when three step sizes are available.
4. Report the error estimate and the method order (1, 2, 4).

**Checks**: the ODE residual at each step; the observed order is close to the
theoretical order for a smooth test problem.

**Notes**: reuse the scalar implementation's step titles and table format.

---

## 6. Boundary value problems and Sturm–Liouville

### 85 — Two-point boundary value problems (`bvp_two_point`)

**Input**: `-y'' + p(x)y' + q(x)y = f(x)` (or `y'' = f(x,y,y')`), interval
`[a,b]`, BCs `y(a)=α`, `y(b)=β`, grid size `N`.

**Output**: grid solution, plot, residuals, and a convergence estimate.

**Algorithm**

1. Build the finite-difference system on `x_i = a + i h`:
   `-(y_{i+1} - 2y_i + y_{i-1})/h^2 + p_i (y_{i+1} - y_{i-1})/(2h) + q_i y_i = f_i`.
2. Solve the tridiagonal system with the Thomas algorithm.
3. For nonlinear `f(x,y,y')`, use Newton's method on the discrete system
   (or shooting with a secant solve for the missing initial slope).
4. Run with `N` and `2N`; estimate the observed order from the max norm.
5. Verify the residual and the boundary values.

**Checks**: `max |L_h y - f|`; boundary residuals; convergence order ≈ 2.

**Notes**: if the matrix is singular, the continuous problem is either
resonant or ill-posed. Report that instead of returning a huge solution.

### 90 — Occurrence of two-point BVPs (`bvp_occurrence`)

**Input**: a textual/physical description or a small set of examples.

**Output**: classification (IVP / BVP / eigenvalue problem), canonical form,
and a link to the appropriate solver.

**Algorithm**

1. Present the three canonical examples: heat rod, hanging string, beam.
2. For each, write the ODE, the BCs, and whether the data are initial or
   boundary conditions.
3. Classify: two conditions at one endpoint → IVP; one at each → BVP; a
   homogeneous equation with a parameter → eigenvalue problem.
4. Offer the matching computation: `bvp_two_point` for 85, or `sturm_liouville`
   for 91.

**Checks**: none beyond consistency of the examples.

**Notes**: this is primarily an article. It should not invent a numeric answer
for a description that has no numbers.

### 91 — Sturm–Liouville problems (`sturm_liouville`)

**Input**: `-(p y')' + q y = λ w y` on `[a,b]`, separated BCs, number of
eigenvalues `K`.

**Output**: first `K` eigenvalues, eigenfunctions (numeric grid or symbolic when
constant coefficients), orthogonality weights, and the Rayleigh quotient.

**Algorithm**

1. Check `p > 0`, `w > 0` on `[a,b]`; otherwise refuse (singular problem → 93).
2. Discretise with finite differences or a Galerkin basis of hat functions:
   `A y = λ B y`.
3. Solve the symmetric generalized eigenproblem; for small `K` use inverse
   iteration or a dense Jacobi solver on the reduced matrix.
4. Sort eigenvalues, normalise eigenfunctions with `∫ w φ_i φ_j = δ_ij`.
5. Compare the first eigenvalue with the Rayleigh quotient
   `R[u] = (∫ p u'^2 + q u^2) / (∫ w u^2)` for the computed eigenfunction.

**Checks**: ODE residual on the grid; orthogonality; `λ_1 <= R[u]` for any
admissible `u`; convergence under grid refinement.

**Notes**: exact closed forms only for constant coefficients and standard BCs.
Everything else is numeric and labelled as such.

### 92 — Nonhomogeneous BVPs (`bvp_nonhomogeneous`)

**Input**: `L y = f` with homogeneous or inhomogeneous separated BCs.

**Output**: solution, eigenfunction expansion or Green's function, solvability
verdict.

**Algorithm**

1. Solve the associated eigenvalue problem (section 91).
2. Expand `f = Σ f_n φ_n` and the boundary data in the same basis.
3. If `λ != λ_n` for every `n`, `y = Σ f_n/(λ_n - λ) φ_n`.
4. If `λ = λ_m` (resonance), the problem is solvable only when `f_m = 0` and
   the boundary terms are orthogonal to `φ_m`; report the solvability
   condition, and add the homogeneous solution `c φ_m`.
5. For non-self-adjoint problems, fall back to finite differences (section 85)
   and label the result numeric.

**Checks**: substitute the expansion into the ODE; check BCs; compare with the
finite-difference solution when both are available.

### 93 — Singular Sturm–Liouville problems (`sturm_liouville_singular`)

**Input**: singular endpoint type (`bessel`, `legendre`, `laguerre`, custom
`p(a)=0`), parameters, `K`.

**Output**: eigenvalues (zeros of the corresponding special function),
eigenfunctions, weight, and boundary behaviour.

**Algorithm**

1. Identify the singular endpoint and the boundedness condition.
2. Bessel on `(0,1)`: `λ_n = α_n^2`, where `α_n` are zeros of `J_ν` (or of the
   Robin combination). Compute zeros by bracketing plus Newton.
3. Legendre on `(-1,1)`: `λ_n = n(n+1)`, `φ_n = P_n`, weight `w = 1`.
4. Laguerre on `(0,∞)`: `λ_n = n`, `φ_n = L_n`, weight `e^{-x}`.
5. Verify the ODE and the orthogonality integral numerically.

**Checks**: each eigenvalue's eigenfunction satisfies the ODE on the interior
grid; zeros are correct (residual `J_ν(α_n) < 1e-10`); orthogonality.

### 94 — Bessel series expansion (`bessel_series`)

**Input**: `f(x)` on `(0,L)`, order `ν`, number of terms `N`, BC type.

**Output**: coefficients `c_n`, partial sums, pointwise error and `L^2` error.

**Algorithm**

1. Compute the zeros `α_n` of the relevant Bessel combination.
2. `c_n = 2/(L^2 J_{ν+1}(α_n)^2) ∫_0^L x f(x) J_ν(α_n x/L) dx`
   for the standard `J_ν(α_n)=0` case; adjust the normalisation for Robin BCs.
3. Evaluate the integral with Gauss–Legendre quadrature (32–64 nodes) and the
   Bessel series from section 36.
4. Form `S_N(x)` and report `max |f - S_N|` on a grid and the `L^2` norm.
5. Verify Parseval: `Σ c_n^2 * norm_n = ∫ x f^2 dx` within quadrature error.

**Checks**: coefficients converge; the partial sum satisfies the BC; Parseval.

### 95 — Series of orthogonal functions (`orthogonal_series`)

**Input**: family (`legendre`, `chebyshev`, `sine`, `cosine`, `bessel`),
`f(x)`, interval, `N`.

**Output**: coefficients, partial sums, error norms, and the weight function.

**Algorithm**

1. Select the family's weight `w`, interval and normalisation.
2. `c_n = ∫ w f φ_n / ∫ w φ_n^2` by Gauss–Legendre/Gauss–Chebyshev quadrature.
3. Form the partial sum and compute pointwise and `L^2` errors.
4. Check Parseval and orthogonality `∫ w φ_m φ_n = 0` for `m != n`.

**Checks**: orthogonality matrix is diagonal within tolerance; Parseval;
convergence as `N` grows.

---

## 7. PDEs and Fourier series

### 86 — Fourier convergence theorem (`fourier_convergence`)

**Input**: `f(x)` on `[-L,L]`, number of terms `N`, evaluation points.

**Output**: Fourier coefficients, partial sums `S_N`, pointwise limits at jumps,
Gibbs overshoot estimate, and error norms.

**Algorithm**

1. Compute `a_0, a_n, b_n` by quadrature (reuse `fourierSeries`).
2. At a point of continuity, `S_N(x) -> f(x)`.
3. At a jump, the limit is `(f(x^-) + f(x^+))/2`; report both one-sided limits.
4. Gibbs: find the overshoot of `S_N` near the jump and report its ratio to the
   jump height (about 1.0895).
5. Compute `||f - S_N||_2` and the decay rate of the coefficients.

**Checks**: `S_N` satisfies the same period and parity as `f`; coefficients
match the direct integrals; the error decreases as `N` grows.

### 87 — Even and odd functions (`fourier_even_odd`)

**Input**: `f(x)` on `[-L,L]`, `N`.

**Output**: `f_e`, `f_o`, the identity `f = f_e + f_o`, and the cosine/sine
coefficients.

**Algorithm**

1. `f_e(x) = (f(x) + f(-x))/2`, `f_o(x) = (f(x) - f(-x))/2`.
2. If `f` is even, `b_n = 0`; if odd, `a_n = 0` (including `a_0`).
3. Compute the surviving coefficients by quadrature.
4. Plot `f`, `f_e`, `f_o` and the partial sum.

**Checks**: `f_e(-x) = f_e(x)`, `f_o(-x) = -f_o(x)`, and
`f_e + f_o = f` at sample points.

### 88 — Heat equation derivation (`heat_derivation`)

**Input**: the physical setup (rod length `L`, diffusivity `α`, initial
temperature `f(x)`, BC type).

**Output**: the derivation steps, the IBVP, the series solution, and snapshots
at selected times.

**Algorithm**

1. Derivation: conservation of energy on a slice, Fourier's law
   `q = -K u_x`, divide by `ρ c Δx`, let `Δx -> 0` to obtain
   `u_t = α^2 u_xx`.
2. Solve by separation `u = X(x)T(t)`:
   `X'' + λX = 0`, `T' + α^2 λ T = 0`.
3. Apply the BCs to get the eigenvalues and eigenfunctions; expand `f` in that
   basis.
4. `u(x,t) = Σ b_n φ_n(x) e^{-α^2 λ_n t}`; compute `b_n` by quadrature.
5. Plot snapshots and the steady state.

**Checks**: PDE residual, BC residual, initial-condition error; decay of the
coefficients.

**Notes**: this is the article version of `heatEquation`; reuse its solver and
add the derivation steps and the general BC switch.

### 89 — Wave equation derivation (`wave_derivation`)

**Input**: string length `L`, wave speed `c`, initial displacement `f(x)`,
initial velocity `g(x)`, BC type.

**Output**: derivation, series solution, d'Alembert form when the domain is
infinite, and snapshots.

**Algorithm**

1. Derivation: Newton's second law on a small string element,
   `u_tt = c^2 u_xx`.
2. Separation: `X'' + λX = 0`, `T'' + c^2 λ T = 0`.
3. Series solution with `f` (sine coefficients) and `g` (sine coefficients
   divided by the frequency).
4. d'Alembert: `u(x,t) = 1/2[f(x-ct)+f(x+ct)] + 1/(2c) ∫_{x-ct}^{x+ct} g(s) ds`
   on the whole line.
5. Plot snapshots and, for the finite string, the standing-wave modes.

**Checks**: PDE and BC residuals; initial displacement/velocity errors; energy
`∫ (u_t^2 + c^2 u_x^2)` is constant for the homogeneous problem.

### 113 — Other heat conduction problems (`heat_other_bc`)

**Input**: BC combination (`insulated-insulated`, `fixed-fixed`,
`fixed-insulated`, `Robin`), parameters, initial data.

**Output**: steady state, eigenvalues, coefficients, solution, and snapshots.

**Algorithm**

1. Fixed nonzero ends: split `u = v(x) + w(x,t)` where `v` is the linear steady
   state satisfying the BCs, and `w` solves the homogeneous problem with zero
   BCs.
2. Insulated ends: cosine series,
   `λ_n = (nπ/L)^2`, `φ_n = cos(nπx/L)`, `n = 0,1,2,...`; the `n=0` mode is the
   average temperature and does not decay.
3. Mixed BC: solve `tan(μL) = h/μ` (or the corresponding transcendental
   equation) for `μ_n`, then `λ_n = μ_n^2`.
4. Robin BC: same transcendental solve with the Biot number.
5. Expand the initial data in the resulting basis.

**Checks**: BC residuals, PDE residual, energy decay for insulated ends,
convergence of the transcendental root solver.

---

## 8. Dedicated calculus tools

### 96 — Transcendental functions (`transcendental`)

**Content**: definitions and derivatives of `exp`, `ln`, the trigonometric and
inverse trigonometric functions, and the hyperbolic functions; the identities
that connect them.

**Computation**: a dedicated handler in `calculus_topics.cpp`. It calls
`diff`, `integrate` and `taylorSeries`, reports the first two derivatives, the
antiderivative, the Taylor polynomial, a coefficient table, a sample table and
the transcendental family table, and cross-checks the derivative against a
central difference and the antiderivative by differentiating it back. The
default is `f(x) = exp(x) sin(x)`.

### 97 — Techniques of integration (`integration_techniques`)

**Content**: substitution, integration by parts, partial fractions, trigonometric
substitution, and when each applies.

**Computation**: a dedicated handler in `calculus_topics.cpp`. It classifies
the engine's detected method, supplies the default example for each technique,
evaluates the definite integral by `F(b) - F(a)` and by adaptive quadrature,
and reports both. When no elementary antiderivative exists it keeps the
quadrature value, sets `closedForm = false` and `numericOnly = true`.

**Note**: these two slots are no longer article-only. They reuse the shared
calculus engine but have their own topic ids, steps and checks, so the UI can
render them like every other workbench.

---

## 9. Cross-cutting checks

Every handler added from this document must provide:

1. `baseId(r, "topic", id, variable)` and a stable `typeId`.
2. At least one symbolic or numerical check with a numeric residual in the
   detail string.
3. `closedForm = false` and an explicit note when the answer is numeric.
4. Steps with catalogue keys, no hard-coded English/Persian strings.
5. A default example that runs in under 100 ms on the reference machine.
6. A core test in `core/test/` and a Python test in `python/tests/`.
7. A Flutter widget test that runs the default and waits for the result key.

## 10. Decisions taken (these were the open questions)

1. **Scope**: engine-first. Algorithms, native tests, Python oracle tests and
   this document came first; the Flutter workbench and translations are the
   next pass.
2. **Slots 90, 96 and 97**: 90 is a dedicated BVP-occurrence tool, not an
   article-only page; 96 and 97 are dedicated tools that reuse the calculus
   engine (`diff`, `integrate`, `taylorSeries`, `quadratureAdaptive`).
3. **Batching**: four batches of 7–8 slots, each built and tested before the
   next. The batches are complete.
4. **Slot 35 (Legendre)**: it must be added to the Series menu in the UI pass.
5. **Numerical honesty**: numeric evidence with an explicit “not a proof” note
   is accepted for 83 and 84. The same rule is applied to every numerical
   handler listed in 1.3.

## 11. Status checklist

| Index | Topic id | Module | Status |
|---:|---|---|---|
| 32 | `series_power` | series_solutions | implemented + UI + tested |
| 33 | `series_singular_points` | series_solutions | implemented + UI + tested |
| 34 | `series_frobenius` | series_solutions | implemented + UI + tested |
| 111 | `euler_equations` | series_solutions | implemented + UI + tested |
| 35 | `special_legendre` | series_solutions | implemented + UI + tested |
| 36 | `special_bessel` | series_solutions | implemented + UI + tested |
| 75 | `system_theory` | linear_systems | implemented + UI + tested |
| 76 | `system_complex_eigen` | linear_systems | implemented + UI + tested |
| 77 | `system_fundamental` | linear_systems | implemented + UI + tested |
| 78 | `system_repeated` | linear_systems | implemented + UI + tested |
| 79 | `system_nonhomogeneous` | linear_systems | implemented + UI + tested |
| 80 | `system_rk4` | linear_systems | implemented + UI + tested |
| 81 | `competing_species` | dynamical_systems | implemented + UI + tested |
| 82 | `liapunov` | dynamical_systems | implemented + UI + tested |
| 83 | `limit_cycle` | dynamical_systems | implemented + UI + tested (numerical evidence) |
| 84 | `chaos_attractor` | dynamical_systems | implemented + UI + tested (numerical evidence) |
| 112 | `local_linearization` | dynamical_systems | implemented + UI + tested |
| 85 | `bvp_two_point` | bvp | implemented + UI + tested |
| 90 | `bvp_occurrence` | bvp | implemented + UI + tested |
| 91 | `sturm_liouville` | bvp | implemented + UI + tested |
| 92 | `bvp_nonhomogeneous` | bvp | implemented + UI + tested |
| 93 | `sturm_liouville_singular` | special_series | implemented + UI + tested |
| 94 | `bessel_series` | special_series | implemented + UI + tested |
| 95 | `orthogonal_series` | special_series | implemented + UI + tested |
| 86 | `fourier_convergence` | pde_series | implemented + UI + tested |
| 87 | `fourier_even_odd` | pde_series | implemented + UI + tested |
| 88 | `heat_derivation` | pde_series | implemented + UI + tested |
| 89 | `wave_derivation` | pde_series | implemented + UI + tested |
| 113 | `heat_other_bc` | pde_series | implemented + UI + tested |
| 96 | `transcendental` | calculus_topics | implemented + UI + tested |
| 97 | `integration_techniques` | calculus_topics | implemented + UI + tested |

### 11.1 Delivered UI work

Every slot left `ComingSoonPage`:

1. A `TopicSpec` exists for all 31 ids in `app/lib/src/topic_catalog.dart`
   with the engine's input keys, defaults and worked examples.
2. Slot 35 was added to the Series drawer and `_topicPages` maps all 31
   indices. `kComingSoonIndices` is now empty.
3. All `topic.*`, `step.*` and `check.*` keys below exist in all 12 catalogues
   (`app/assets/i18n/*.json`); `i18n_deferred_keys.txt` and the temporary
   guard exemption were removed.
4. `ResultView` renders the topic answer (`solution`, falling back to `value`),
   the numeric-evidence label and the raw `functions` tables.
5. `app/test/reserved_topics_test.dart` renders every reserved page and runs a
   representative from each module family; `topic_keys_i18n_test.dart` still
   sweeps all 31 ids through the engine.

The list below is the exact set of `step.*` and `check.*` keys added for the
modules in section 1.3. Tables and data dumps travel as raw `note`/`functions`
text, not as translation keys.

**New step keys (99)**

```text
step.note.a_defective_matrix_needs_generalized_eigenvectors
step.note.an_irregular_point_has_no_frobenius_series_in_general
step.note.both_conditions_hold_so_the_point_is_regular_singular
step.note.energy_conservation_on_a_slice_gives_the_heat_equation
step.note.no_boundaries_only_initial_data
step.note.no_pole_at_all_so_the_point_is_ordinary
step.note.one_condition_fails_so_the_point_is_irregular_singular
step.note.polynomial_coefficients_are_entire_so_the_series_converges_everywhere
step.note.taylor_expansion_about_a
step.note.the_first_basis_has_a0_1_and_a1_0
step.note.the_formula_is_valid_on_x_greater_than_zero_or_x_less_than_zero
step.note.the_matrix_is_diagonalizable
step.note.the_second_basis_has_a0_0_and_a1_1
step.note.the_test_is_exact_for_rational_coefficients
step.title.build_the_fundamental_matrix
step.title.build_the_jordan_chain
step.title.build_the_partial_sum
step.title.build_the_polynomial
step.title.check_by_substitution
step.title.check_coexistence
step.title.check_orthogonality_and_parseval
step.title.check_parseval_and_the_boundary
step.title.check_solvability_and_residual
step.title.check_the_coefficients
step.title.check_the_ode_and_orthogonality
step.title.check_the_rayleigh_quotient
step.title.check_transversality_and_stability
step.title.choose_the_expansion_point
step.title.choose_the_orthogonal_family
step.title.choose_the_solver
step.title.classify_each_equilibrium
step.title.classify_the_conditions
step.title.classify_the_linearization
step.title.collect_equal_powers
step.title.compare_the_multiplicities
step.title.compare_with_the_function
step.title.compare_with_the_nonlinear_field
step.title.compare_with_the_regular_singular_test
step.title.compute_the_indicial_equation
step.title.compute_the_jacobian_at_each_point
step.title.compute_the_pole_orders
step.title.conclude_from_the_sign
step.title.diagonalize_or_use_the_spectral_projectors
step.title.differentiate_along_the_flow
step.title.discretize_the_interval
step.title.discretize_with_finite_differences
step.title.divide_by_the_eigenvalue
step.title.estimate_the_convergence_order
step.title.estimate_the_lyapunov_exponent
step.title.estimate_the_period
step.title.evaluate_at_the_point
step.title.evaluate_the_series
step.title.expand_the_forcing
step.title.factor_the_characteristic_polynomial
step.title.find_the_bessel_zeros
step.title.find_the_characteristic_polynomial
step.title.find_the_complex_eigenvalue
step.title.find_the_poincare_returns
step.title.first_frobenius_series
step.title.first_solution
step.title.identify_the_singular_endpoint
step.title.integrate_the_planar_system
step.title.integrate_the_trajectory
step.title.linearize_at_the_equilibrium
step.title.list_the_eigenvalues
step.title.record_the_reasoning
step.title.recurrence
step.title.report_the_weight_and_boundary_behaviour
step.title.second_frobenius_series
step.title.second_solution
step.title.second_solution_by_reduction_of_order
step.title.solve_the_associated_eigenvalue_problem
step.title.solve_the_generalized_eigenproblem
step.title.solve_the_indicial_equation
step.title.solve_the_lyapunov_equation
step.title.solve_the_tridiagonal_system
step.title.step_through_the_grid
step.title.substitute_y_equals_x_to_the_r
step.title.use_the_bonnet_recurrence
step.title.variation_of_parameters
step.title.verify_the_regular_singular_point
step.title.write_p_and_q
step.title.write_the_bessel_equation
step.title.write_the_candidate
step.title.write_the_canonical_problem
step.title.write_the_competing_species_system
step.title.write_the_equation_in_standard_form
step.title.write_the_euler_equation
step.title.write_the_finite_difference_equations
step.title.write_the_frobenius_series
step.title.write_the_indicial_roots
step.title.write_the_legendre_equation
step.title.write_the_matrix
step.title.write_the_particular_solution
step.title.write_the_power_series
step.title.write_the_real_general_solution
step.title.write_the_special_function_eigenvalues
step.title.write_the_system
step.title.wronskian
```

**New check keys (53)**

```text
check.a_matching_solver_is_reported
check.abel_identity
check.at_least_one_renormalisation
check.at_least_two_transversal_returns
check.candidate_is_positive_definite_near_the_point
check.candidate_vanishes_at_the_equilibrium
check.each_equilibrium_vanishes
check.eigenvalues_have_zero_real_part_when_expected
check.energy_shrinks
check.every_equilibrium_is_hyperbolic
check.every_reported_point_vanishes
check.every_solution_satisfies_the_system
check.gibbs_overshoot
check.initial_values_at_the_expansion_point
check.no_resonant_mode_was_skipped
check.number_of_solutions_equals_the_dimension
check.parseval_matches_the_energy
check.phi_at_zero_is_the_identity
check.phi_satisfies_the_system
check.radius_is_infinite_for_polynomial_coefficients
check.step_halving_changes_the_short_trajectory_slightly
check.the_bessel_function_satisfies_the_ode
check.the_boundary_values_are_exact
check.the_boundary_values_match_the_data
check.the_characteristic_polynomial_was_factored
check.the_classification_matches_the_condition_count
check.the_classification_matches_the_limits
check.the_discrete_residual_is_small
check.the_eigenfunctions_are_orthogonal
check.the_eigenfunctions_satisfy_the_ode
check.the_eigenvalues_are_ordered
check.the_eigenvalues_are_positive
check.the_eigenvalues_match_the_trace_and_determinant
check.the_error_does_not_grow_with_more_terms
check.the_final_state_is_finite
check.the_first_series_satisfies_the_recurrence
check.the_indicial_roots_annihilate_the_indicial_polynomial
check.the_observed_order_is_close_to_two
check.the_partial_sum_vanishes_at_the_boundary
check.the_particular_solution_is_reported
check.the_particular_solution_satisfies_the_system
check.the_period_is_positive
check.the_polynomial_satisfies_the_legendre_equation
check.the_rayleigh_quotient_matches
check.the_second_series_satisfies_the_recurrence
check.the_section_points_do_not_collapse
check.the_series_satisfies_the_ode
check.the_sign_of_v_dot_was_sampled
check.the_step_halving_error_is_small
check.the_three_term_recurrence_holds
check.the_trajectory_stays_bounded
check.the_value_matches_the_recurrence
check.two_endpoints_are_involved
```

Unless a key already appears above, the English value is the key
with underscores replaced by spaces and the first letter capitalised;
the other 11 catalogues need a translated sentence, not a copy of the
English text.
