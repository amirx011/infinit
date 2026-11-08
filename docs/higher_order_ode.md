# Higher-order ODE reductions — algorithm and audit

This report documents the two order-reduction methods implemented in `core/src/ode.cpp` and records a 116-equation audit. The audit is produced by running each equation through the native engine in a separate process, so a crash or a hang cannot abort the whole run.

## Algorithm

### 1. Missing dependent variable (`y` does not occur)

The equation has the form `F(x, y′, y″, …, y⁽ⁿ⁾) = 0` with no bare `y`.

1. Substitute `p = y′`. Then `y″ = p′`, `y‴ = p″`, and in general `y⁽ᵏ⁾ = p⁽ᵏ⁻¹⁾`.
2. The equation becomes a first-order (or lower-order) equation in `p(x)`.
3. Solve it with the ordinary first-order pipeline (separable, linear, exact, Bernoulli, …).
4. Integrate the resulting `p(x)` once: `y = ∫ p(x) dx + Cₙ`.
5. Differentiate the answer and substitute it into the original equation as a check.

If only the highest derivative occurs, the same path is reported as **direct integration** `y⁽ⁿ⁾ = f(x)`, because repeated quadrature is the clearest description.

### 2. Missing independent variable (`x` does not occur)

The equation has the form `F(y, y′, y″) = 0` with no explicit `x`.

1. Substitute `y′ = p(y)`. By the chain rule, `y″ = dp/dx = (dp/dy)(dy/dx) = p·dp/dy`.
2. The equation becomes a first-order equation in `p(y)`.
3. Solve it for `p(y)` (including quadratic energy relations such as `p² = 2C − y²`).
4. Separate `dy/p(y) = dx` and integrate both sides: `∫ dy/p(y) = x + C₂`.
5. Solve the relation for `y` when an elementary inverse exists; otherwise report the implicit relation.
6. Check by implicit differentiation: `u_x + p(y) u_y = 0`.

The current implementation covers second-order missing-`x` equations. Higher orders fail honestly rather than returning an invented answer.

### 3. Dispatch order

1. Detect the highest derivative order.
2. If `y` is absent, use the missing-dependent reduction.
3. Otherwise try the linear constant-coefficient classifier.
4. If that classifier does not apply and `x` is absent, use the missing-independent reduction.
5. If a second-order equation still has no symbolic family, return a labelled RK4 approximation instead of an invented closed form.
6. Otherwise report that the equation is outside the supported forms.

## Audit summary

```
total=116 explicit=71 implicit=45 failed=0 crash=0
```

Every solved equation is verified symbolically or numerically. The one equation with no closed form is now handled honestly rather than failing:

- `y″ = y′ + y²` reduces to `p·dp/dy = p + y²`, which is neither separable, linear, exact, Bernoulli, nor one of the special first-order forms handled by the engine. The app returns `numeric_second_order`: an RK4 trajectory of `y″ = y² + y′`, labelled **Approximate** and plotted, with no symbolic answer invented.

The `y″ + y·y′ = 0` family used to return a complex-looking `tan` branch; the antiderivative now carries the sign of the quadratic coefficient, so the real branch verifies.

Some answers are exact quadratures such as `Integral(1/sqrt(...), y)` when the antiderivative is non-elementary; these are reported honestly and are not claimed as elementary closed forms.

## Missing Y (59 equations)

| # | Equation | Result | Type | Solution / note |
|---:|---|---|---|---|
| 1 | `y'' = 0` | explicit | direct_integration | `C2 + C*x` |
| 2 | `y'' = 1` | explicit | direct_integration | `C2 + 1/2*x^2 + C*x` |
| 3 | `y'' = x` | explicit | direct_integration | `C2 + 1/6*x^3 + C*x` |
| 4 | `y'' = x^2` | explicit | direct_integration | `C2 + 1/12*x^4 + C*x` |
| 5 | `y'' = x^3` | explicit | direct_integration | `C2 + 1/20*x^5 + C*x` |
| 6 | `y'' = 2*x + 1` | explicit | direct_integration | `C2 + 1/3*x^3 + 1/2*x^2 + C*x` |
| 7 | `y'' = 6*x` | explicit | direct_integration | `C2 + x^3 + C*x` |
| 8 | `y'' = sin(x)` | explicit | direct_integration | `C2 - sin(x) + C*x` |
| 9 | `y'' = cos(x)` | explicit | direct_integration | `C2 - cos(x) + C*x` |
| 10 | `y'' = exp(x)` | explicit | direct_integration | `C2 + exp(x) + C*x` |
| 11 | `y'' = exp(-x)` | explicit | direct_integration | `C2 + exp(-x) + C*x` |
| 12 | `y'' = 1/x` | explicit | direct_integration | `C2 + Integral(ln(abs(x)), x) + C*x` |
| 13 | `y'' = 1/x^2` | explicit | direct_integration | `C2 - ln(abs(x)) + C*x` |
| 14 | `y'' = sqrt(x)` | explicit | direct_integration | `C2 + 4/15*x^(5/2) + C*x` |
| 15 | `y'' = 1/(1+x^2)` | explicit | direct_integration | `C2 - 1/2*ln(x^2 + 1) + C*x + x*atan(x)` |
| 16 | `y'' = tan(x)` | explicit | direct_integration | `C2 - Integral(ln(abs(cos(x))), x) + C*x` |
| 17 | `y'' = sec(x)^2` | explicit | direct_integration | `C2 + Integral(Integral(sec(x)^2, x), x) + C*x` |
| 18 | `y'' = sinh(x)` | explicit | direct_integration | `C2 + sinh(x) + C*x` |
| 19 | `y'' = cosh(x)` | explicit | direct_integration | `C2 + cosh(x) + C*x` |
| 20 | `y'' = ln(x)` | explicit | direct_integration | `C2 - 3/4*x^2 + 1/2*x^2*ln(x) + C*x` |
| 21 | `y'' = x*exp(x)` | explicit | direct_integration | `C2 - 2*exp(x) + C*x + x*exp(x)` |
| 22 | `y'' = x*sin(x)` | explicit | direct_integration | `C2 - 2*cos(x) - x*sin(x) + C*x` |
| 23 | `y'' = exp(x)*cos(x)` | explicit | direct_integration | `C2 + 1/2*exp(x)*sin(x) + C*x` |
| 24 | `y''' = 0` | explicit | direct_integration | `C3 + 1/2*C*x^2 + C2*x` |
| 25 | `y''' = 1` | explicit | direct_integration | `C3 + 1/6*x^3 + 1/2*C*x^2 + C2*x` |
| 26 | `y''' = x` | explicit | direct_integration | `C3 + 1/24*x^4 + 1/2*C*x^2 + C2*x` |
| 27 | `y''' = exp(x)` | explicit | direct_integration | `C3 + exp(x) + 1/2*C*x^2 + C2*x` |
| 28 | `y''' = sin(x)` | explicit | direct_integration | `C3 + cos(x) + 1/2*C*x^2 + C2*x` |
| 29 | `y'''' = x` | explicit | direct_integration | `C4 + 1/120*x^5 + 1/6*C*x^3 + 1/2*C2*x^2 + C3*x` |
| 30 | `y'''' = exp(x)` | explicit | direct_integration | `C4 + exp(x) + 1/6*C*x^3 + 1/2*C2*x^2 + C3*x` |
| 31 | `y'' = y'` | explicit | reduction_missing_y | `C2 + C*exp(x)` |
| 32 | `y'' = 2*y'` | explicit | reduction_missing_y | `C2 + 1/2*C*exp(2*x)` |
| 33 | `y'' = -y'` | explicit | reduction_missing_y | `C2 - C*exp(-x)` |
| 34 | `y'' = y'^2` | explicit | reduction_missing_y | `C2 - ln(abs(-C - x))` |
| 35 | `y'' = y'^3` | explicit | reduction_missing_y | `C2 - sqrt((-2*C - 2*x))` |
| 36 | `y'' = sqrt(1+y'^2)` | explicit | reduction_missing_y | `C2 + cosh(C + x)` |
| 37 | `y'' = 1+y'^2` | explicit | reduction_missing_y | `C2 - ln(abs(cos(C + x)))` |
| 38 | `y'' = 1-y'^2` | implicit | reduction_missing_y | `-x - 1/2*ln(abs(y' - 1)) + 1/2*ln(abs(y' + 1))` |
| 39 | `y'' = exp(-y')` | explicit | reduction_missing_y | `C2 - C - x + C*ln(C + x) + x*ln(C + x)` |
| 40 | `y'' = exp(y')` | explicit | reduction_missing_y | `C + C2 + x - C*ln(-C - x) - x*ln(-C - x)` |
| 41 | `y'' = 1/y'` | explicit | reduction_missing_y | `C2 + 1/3*(2*C + 2*x)^(3/2)` |
| 42 | `y'' = y'/x` | explicit | reduction_missing_y | `C2 + 1/2*C*x^2` |
| 43 | `y'' = x*y'` | explicit | reduction_missing_y | `C2 + C*Integral(exp(1/2*x^2), x)` |
| 44 | `y'' = y' + x` | explicit | reduction_missing_y | `C2 - x - 1/2*x^2 + C*exp(x)` |
| 45 | `y'' = y' + exp(x)` | explicit | reduction_missing_y | `C2 - exp(x) + C*exp(x) + x*exp(x)` |
| 46 | `y'' + y' = 0` | explicit | reduction_missing_y | `C2 - C*exp(-x)` |
| 47 | `y'' + y' = 1` | explicit | reduction_missing_y | `C2 + x - C*exp(-x)` |
| 48 | `y'' + y' = x` | explicit | reduction_missing_y | `C2 - C*exp(-x) - x + 1/2*x^2` |
| 49 | `y'' + y' = sin(x)` | explicit | reduction_missing_y | `C2 - C*exp(-x) - 1/2*cos(x) - 1/2*sin(x)` |
| 50 | `y'' + 2*y' = exp(x)` | explicit | reduction_missing_y | `C2 - 1/2*C*exp(-2*x) + 1/3*exp(x)` |
| 51 | `y'' - y' = x^2` | explicit | reduction_missing_y | `C2 - 2*x - x^2 - 1/3*x^3 + C*exp(x)` |
| 52 | `y'' + y'^2 = 0` | explicit | reduction_missing_y | `C2 + ln(abs(x - C))` |
| 53 | `y'' = y'*(1-y')` | implicit | reduction_missing_y | `ln(abs(y')) - x - ln(abs(y' - 1))` |
| 54 | `y'' = y'*(y'-1)` | implicit | reduction_missing_y | `ln(abs(y' - 1)) - x - ln(abs(y'))` |
| 55 | `y''' = y''` | explicit | reduction_missing_y | `C3 + C*exp(x) + C2*x` |
| 56 | `y''' + y'' = 0` | explicit | reduction_missing_y | `C3 + C*exp(-x) + C2*x` |
| 57 | `y''' = x*y''` | explicit | reduction_missing_y | `C3 + C*Integral(Integral(exp(1/2*x^2), x), x) + C2*x` |
| 58 | `y''' = y'' + x` | explicit | reduction_missing_y | `C3 - 1/2*x^2 - 1/6*x^3 + C*exp(x) + C2*x` |
| 59 | `y''' = exp(x)` | explicit | direct_integration | `C3 + exp(x) + 1/2*C*x^2 + C2*x` |

## Missing X (47 equations)

| # | Equation | Result | Type | Solution / note |
|---:|---|---|---|---|
| 1 | `y'' = y^2` | implicit | reduction_missing_x | `Integral(1/sqrt((2/3*(y^3 + 3*C))), y) - C2 - x` |
| 2 | `y'' = y^3` | implicit | reduction_missing_x | `Integral(1/sqrt((1/2*(y^4 + 4*C))), y) - C2 - x` |
| 3 | `y'' = -y^2` | implicit | reduction_missing_x | `Integral(1/sqrt((-2/3*y^3 + 2*C)), y) - C2 - x` |
| 4 | `y'' = -y^3` | implicit | reduction_missing_x | `Integral(1/sqrt((-1/2*y^4 + 2*C)), y) - C2 - x` |
| 5 | `y'' = y^4` | implicit | reduction_missing_x | `Integral(1/sqrt((2/5*(y^5 + 5*C))), y) - C2 - x` |
| 6 | `y'' = sqrt(y)` | implicit | reduction_missing_x | `Integral(1/sqrt((2/3*(2*y^(3/2) + 3*C))), y) - C2 - x` |
| 7 | `y'' = 1/sqrt(y)` | implicit | reduction_missing_x | `-C2 - x - 1/6*(2*(C + 2*sqrt(y)))^(3/2) + sqrt(y)*sqrt((2*(C + 2*sqrt(y))))` |
| 8 | `y'' = 1/y` | implicit | reduction_missing_x | `Integral(1/sqrt((2*(C + ln(abs(y))))), y) - C2 - x` |
| 9 | `y'' = 1/y^2` | implicit | reduction_missing_x | `Integral(1/sqrt((2*(C - 1/y))), y) - C2 - x` |
| 10 | `y'' = exp(y)` | implicit | reduction_missing_x | `Integral(1/sqrt((2*(C + exp(y)))), y) - C2 - x` |
| 11 | `y'' = exp(-y)` | implicit | reduction_missing_x | `Integral(1/sqrt((2*(C - exp(-y)))), y) - C2 - x` |
| 12 | `y'' = sin(y)` | implicit | reduction_missing_x | `Integral(1/sqrt((2*(C - cos(y)))), y) - C2 - x` |
| 13 | `y'' = cos(y)` | implicit | reduction_missing_x | `Integral(1/sqrt((2*(C + sin(y)))), y) - C2 - x` |
| 14 | `y'' = tan(y)` | implicit | reduction_missing_x | `Integral(1/sqrt((2*(C - ln(abs(cos(y)))))), y) - C2 - x` |
| 15 | `y'' = sinh(y)` | implicit | reduction_missing_x | `Integral(1/sqrt((2*(C + cosh(y)))), y) - C2 - x` |
| 16 | `y'' = cosh(y)` | implicit | reduction_missing_x | `Integral(1/sqrt((2*(C + sinh(y)))), y) - C2 - x` |
| 17 | `y'' = y*y'` | explicit | reduction_missing_x | `tan(C2*sqrt((1/2*C)) + x*sqrt((1/2*C)))/sqrt((1/(2*C)))` |
| 18 | `y'' = y^2*y'` | implicit | reduction_missing_x | `Integral(1/(C + 1/3*y^3), y) - C2 - x` |
| 19 | `y'' = y'*y^3` | implicit | reduction_missing_x | `Integral(1/(C + 1/4*y^4), y) - C2 - x` |
| 20 | `y'' = y' + y` | explicit | linear_cc | `C*exp(1/2*x*(sqrt(5) + 1)) + C2*exp(1/2*x*(-sqrt(5) + 1))` |
| 21 | `y'' = y' + y^2` | implicit | numeric_second_order | No symbolic family fits; RK4 trajectories of `y'' = y^2 + y'` are labelled **Approximate**. |
| 22 | `y'' = y'^2 + y` | implicit | reduction_missing_x | `Integral(1/sqrt((-y + C*exp(2*y) - 1/2)), y) - C2 - x` |
| 23 | `y'' = y'^2 + y^2` | implicit | reduction_missing_x | `Integral(1/sqrt((-y - y^2 + C*exp(2*y) - 1/2)), y) - C2 - x` |
| 24 | `y'' = y'*sqrt(y)` | implicit | reduction_missing_x | `Integral(1/(C + 2/3*y^(3/2)), y) - C2 - x` |
| 25 | `y'' = y'*exp(y)` | implicit | reduction_missing_x | `Integral(1/(C + exp(y)), y) - C2 - x` |
| 26 | `y'' = y'/(1+y^2)` | implicit | reduction_missing_x | `Integral(1/(C + atan(y)), y) - C2 - x` |
| 27 | `y'' = (1+y'^2)/y` | implicit | reduction_missing_x | `Integral(1/sqrt((C*y^2 - 1)), y) - C2 - x` |
| 28 | `y'' = (1+y'^2)^(3/2)` | implicit | reduction_missing_y | `Integral((y'^2 + 1)^(-3/2), y') - x` |
| 29 | `y'' = y'^2/y` | implicit | reduction_missing_x | `-C2 - x + ln(abs(y))/C` |
| 30 | `y'' = y'^3` | explicit | reduction_missing_y | `C2 - sqrt((-2*C - 2*x))` |
| 31 | `y'' = y'^2 + 1` | explicit | reduction_missing_y | `C2 - ln(abs(cos(C + x)))` |
| 32 | `y'' = y'^2 - 1` | implicit | reduction_missing_y | `-x - 1/2*ln(abs(y' + 1)) + 1/2*ln(abs(y' - 1))` |
| 33 | `y'' + y*y' = 0` | explicit | reduction_missing_x | `tan(-C2*sqrt((-1/2*C)) - x*sqrt((-1/2*C)))/sqrt((-1/(2*C)))` |
| 34 | `y'' + y^2 = 0` | implicit | reduction_missing_x | `Integral(1/sqrt((-2/3*y^3 + 2*C)), y) - C2 - x` |
| 35 | `y'' + y^3 = 0` | implicit | reduction_missing_x | `Integral(1/sqrt((-1/2*y^4 + 2*C)), y) - C2 - x` |
| 36 | `y'' + sin(y) = 0` | implicit | reduction_missing_x | `Integral(1/sqrt((2*(C + cos(y)))), y) - C2 - x` |
| 37 | `y'' + exp(y) = 0` | implicit | reduction_missing_x | `Integral(1/sqrt((2*(C - exp(y)))), y) - C2 - x` |
| 38 | `y'' - y^2 = 0` | implicit | reduction_missing_x | `Integral(1/sqrt((2/3*(y^3 + 3*C))), y) - C2 - x` |
| 39 | `y*y'' = y'^2` | implicit | reduction_missing_x | `-C2 - x + ln(abs(y))/C` |
| 40 | `y*y'' + y'^2 = 0` | implicit | reduction_missing_x | `-C2 - x + 1/2*y^2/C` |
| 41 | `y'' = y'*(1-y')` | implicit | reduction_missing_y | `ln(abs(y')) - x - ln(abs(y' - 1))` |
| 42 | `y'' = y'*(y'-1)` | implicit | reduction_missing_y | `ln(abs(y' - 1)) - x - ln(abs(y'))` |
| 43 | `y'' = y'*(y'+1)` | implicit | reduction_missing_y | `ln(abs(y')) - x - ln(abs(y' + 1))` |
| 44 | `y'' = y'/y` | implicit | reduction_missing_x | `Integral(1/(C + ln(abs(y))), y) - C2 - x` |
| 45 | `y'' = y'/(1+y)` | implicit | reduction_missing_x | `Integral(1/(C + ln(abs(y + 1))), y) - C2 - x` |
| 46 | `y'' = y'*cos(y)` | implicit | reduction_missing_x | `Integral(1/(C + sin(y)), y) - C2 - x` |
| 47 | `y'' = y'*sin(y)` | implicit | reduction_missing_x | `Integral(1/(C - cos(y)), y) - C2 - x` |

## Linear (10 equations)

| # | Equation | Result | Type | Solution / note |
|---:|---|---|---|---|
| 1 | `y'' - 3*y' + 2*y = 0` | explicit | linear_cc | `C*exp(x) + C2*exp(2*x)` |
| 2 | `y'' + 4*y = 0` | explicit | linear_cc | `C*cos(2*x) + C2*sin(2*x)` |
| 3 | `y'' - y = x` | explicit | linear_cc | `-x + C*exp(-x) + C2*exp(x)` |
| 4 | `y'' + y = sin(x)` | explicit | linear_cc | `-1/2*x*cos(x) + C*cos(x) + C2*sin(x)` |
| 5 | `y''' - y = 0` | explicit | linear_cc | `C*exp(x) + C2*cos(1/2*x*sqrt(3))*exp(-1/2*x) + C3*exp(-1/2*x)*sin(1/2*x*sqrt(3))` |
| 6 | `y''' + y = 0` | explicit | linear_cc | `C*exp(-x) + C2*cos(1/2*x*sqrt(3))*exp(1/2*x) + C3*exp(1/2*x)*sin(1/2*x*sqrt(3))` |
| 7 | `y'''' + y'' = 0` | explicit | reduction_missing_y | `C4 - C*cos(x) - C2*sin(x) + C3*x` |
| 8 | `y''' - 3*y'' + 3*y' - y = 0` | explicit | linear_cc | `C*exp(x) + C2*x*exp(x) + C3*x^2*exp(x)` |
| 9 | `y'''' - y = 0` | explicit | linear_cc | `C*exp(-x) + C2*exp(x) + C3*cos(x) + C4*sin(x)` |
| 10 | `y''' + y'' + y' + y = 0` | explicit | linear_cc | `C*exp(-x) + C2*cos(x) + C3*sin(x)` |

## Reproducing the audit

The audit driver is `scripts/audit_higher_order.py`. It calls the shared library through `ctypes` and runs each equation in a child process:

```bash
./scripts/build_core.sh
python3 scripts/audit_higher_order.py
```
