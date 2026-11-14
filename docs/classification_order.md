# ODE classification order

The exact engine reports the **first family that matches**, walking the
catalogue from the simplest and most specific description to the most general
reduction. The order is the same for an explicit `y′ = f(x, y)` input and for a
differential form `M dx + N dy = 0`; notation never changes the label.

`Result::typeId` is the primary family (the first match). `Result::alsoForms`
lists every first-order family the equation belongs to, primary included and in
the same catalogue order, so the interface can say "this is also Bernoulli"
instead of hiding the overlap.

## Stage 0 — preprocessing

1. Highest derivative order.
2. Degree in `y′` (linear in `y′`, quadratic, higher/non-polynomial).
3. Linearity in `y`.
4. Constant or variable coefficients.
5. Homogeneous or non-homogeneous right side.

The order is not a separate pass. The solver scans the highest derivative, then
isolates `y′`; an equation that is not polynomial in `y′` goes to the
Clairaut/Lagrange detector instead.

## First order

| # | Family | `type_id` | Test |
|---|---|---|---|
| 1 | Separable | `separable` | `y′ = f(x)·g(y)` |
| 2 | Exact | `exact` | `M dx + N dy = 0` with `∂M/∂y = ∂N/∂x` |
| 3 | Linear | `linear` | `y′ + P(x)y = Q(x)` |
| 4 | Bernoulli | `bernoulli` | `y′ + P(x)y = Q(x)yⁿ`, `n ≠ 0, 1` |
| 5 | Homogeneous | `homogeneous` | `y′ = f(y/x)` |
| 6 | Integrating factor | `integrating_factor` | exact after `μ(x)`, `μ(y)`, `μ(xy)`, `μ(x+y)`, `μ = x^a y^b` |
| 7 | Riccati | `riccati` | `y′ = q₀(x) + q₁(x)y + q₂(x)y²` |
| 8 | Clairaut / Lagrange | `clairaut`, `lagrange` | `y = xy′ + f(y′)`, `y = x·g(y′) + f(y′)` |
| 9 | Affine shift / linear substitution | `homogeneous`, `linear_sub` | `y′ = (ax+by+c)/(dx+ey+f)`, `y′ = f(ax+by+c)` |
| 10 | Direct quadrature | `quadrature` | `y′ = f(x)` with no earlier family |

Why this order:

- **Separable before exact.** Every separable equation is also exact, but the
  separation steps are shorter and more natural.
- **Exact before linear.** Solving an exact equation through its potential
  function avoids an unnecessary integrating factor.
- **Bernoulli after linear.** For `n = 0` or `n = 1` the equation *is* linear;
  Bernoulli only earns the label for a genuine power.
- **Homogeneous before the integrating-factor search.** The `y = vx`
  substitution explains a homogeneous equation with fewer steps than finding a
  multiplier.
- **Riccati before substitutions.** It is the more specific description: a
  Riccati equation may also be a linear substitution, not the other way round.
- **Affine shift and `f(ax+by+c)` last.** They are the most general reductions.

Two structural notes:

- **Clairaut/Lagrange** are listed at position 8, but a parametric form must be
  recognized before `y′` is isolated: `(y′)³ − y·y′ = 0` is really `y = p²`,
  and splitting it into quadratic sign branches would lose the parametric
  solution. The detector runs only when the equation is **not** linear in `y′`,
  so a degenerate Clairaut with `f(p) = p` keeps the linear label it deserves.
- **A pure quadrature** `y′ = f(x)` is trivially exact and linear. It is left to
  the dedicated last step so it keeps the `quadrature` label and reports no
  alternative families.

### Differential form

`M dx + N dy = 0` tests exactness first (step 2). If it is not exact, the
equation is rewritten as `y′ = −M/N` and the same catalogue runs from the top;
the integrating-factor search (step 6) only happens when no earlier family
matched. This is why `(2y − 3x)dx + x dy = 0` is reported as **linear** rather
than **integrating factor**, exactly like `y′ = 3 − 2y/x`.

## Higher order

| # | Family | `type_id` |
|---|---|---|
| 1 | Reducible: no `y` | `reduction_missing_y` |
| 2 | Reducible: highest derivative only | `direct_integration` |
| 3 | Constant-coefficient homogeneous | `linear_cc` |
| 4 | Constant-coefficient non-homogeneous | `linear_cc` |
| 5 | Cauchy–Euler | `linear_higher` |
| 6 | Reducible: no `x` | `reduction_missing_x` |
| 7 | Variable-coefficient canonical (Bessel, Legendre, Hermite) | `linear_higher` |
| 8 | No symbolic family | `numeric_second_order` (labelled approximation) |

The missing-variable reductions run before the constant-coefficient classifier
when `y` is absent, because they are the most specific description:
`y″ = 6x` is **direct integration**, not a generic constant-coefficient
equation. This follows the same "simplest first" rule as the first-order
catalogue.
