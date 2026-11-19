"""SymPy oracle for Infinit Lab.

This module is deliberately *not* used by the mobile application.  It is a
reference/verification engine for the native C++ core: it classifies and solves
the same inputs with SymPy and can check a candidate native solution exactly.

The parser here is intentionally tolerant: it accepts the same natural
notation as the C++ core (``2x``, ``y sin y``, ``dy/dx``, ``Integral(...)``,
scientific notation, Euler's ``e``) so that oracle and core see the same
problem.

The ``lang`` option of the native engine is not mirrored here: the oracle
emits SymPy strings (and LaTeX) only, because it is a test/reference tool.
"""

from __future__ import annotations

import contextlib
import re
import signal
from dataclasses import dataclass, field
from typing import Iterable

import sympy as sp

__all__ = [
    "OracleResult",
    "parse_expression",
    "solve_ode",
    "verify_candidate",
    "verify_parametric",
    "wronskian",
    "envelope",
]


@dataclass
class OracleResult:
    ok: bool
    input: str
    classification: list[str] = field(default_factory=list)
    solution: str = ""
    latex: str = ""
    verified: bool = False
    error: str = ""
    hints: list[str] = field(default_factory=list)
    all_solutions: list[str] = field(default_factory=list)

    def to_dict(self) -> dict:
        return {
            "ok": self.ok,
            "input": self.input,
            "classification": self.classification,
            "solution": self.solution,
            "latex": self.latex,
            "verified": self.verified,
            "error": self.error,
            "hints": self.hints,
            "all_solutions": self.all_solutions,
        }


# Known elementary functions.  Only used to *document* the accepted names; the
# parser no longer needs the list because identifiers followed by ``(`` are
# treated as function calls for every multi-letter name (so erf, Integral,
# sign, asec, ... all work).
_FUNCTIONS = {
    "sin", "cos", "tan", "cot", "sec", "csc",
    "asin", "acos", "atan", "acot", "asec", "acsc",
    "sinh", "cosh", "tanh", "coth", "sech", "csch",
    "asinh", "acosh", "atanh", "acoth", "asech", "acsch",
    "exp", "ln", "log", "sqrt", "abs", "sign", "erf", "erfc",
}

_MAX_VERIFY_SAMPLES = [
    (0.37, 1.13), (1.71, 0.77), (2.53, 2.31), (0.91, 3.71),
    (3.17, 0.53), (1.29, 2.03), (0.61, 4.11), (2.87, 1.19),
]


# ------------------------------------------------------------------ utilities
@contextlib.contextmanager
def _time_limit(seconds: float | None):
    """Abort a SymPy call that runs too long (Unix only, best effort)."""
    if seconds is None or not hasattr(signal, "SIGALRM"):
        yield
        return

    def _handler(signum, frame):  # pragma: no cover - signal path
        raise TimeoutError(f"SymPy call exceeded {seconds}s")

    old = signal.signal(signal.SIGALRM, _handler)
    signal.setitimer(signal.ITIMER_REAL, seconds)
    try:
        yield
    finally:
        signal.setitimer(signal.ITIMER_REAL, 0)
        signal.signal(signal.SIGALRM, old)


def _normalize(text: str) -> str:
    return (
        text.strip()
        .replace("−", "-")
        .replace("×", "*")
        .replace("÷", "/")
        .replace("√", "sqrt")
        .replace("’", "'")
        .replace("′", "'")
    )


def _insert_implicit_multiplication(
    text: str, *, differentials: bool = False, variables: str = "xyc"
) -> str:
    """Turn ``2x``, ``3(x+1)``, ``(x+1)(x-1)``, ``xy`` and ``x sin(x)`` into products.

    A *multi-letter* identifier followed by ``(`` is a function call
    (``erf(x)``, ``Integral(g, u)``); a single letter is a factor
    (``x(y+1)``).  Scientific notation such as ``1e-3`` is left intact.
    """
    text = text.replace("^", "**")
    # digit followed by a factor, but not the exponent part of 1e-3
    text = re.sub(r"(?<=[0-9])(?![eE][+-]?[0-9])(?=[A-Za-z_(])", "*", text)
    # )( and )x
    text = re.sub(r"(?<=\))(?=\()", "*", text)
    text = re.sub(r"(?<=\))(?=[A-Za-z0-9])", "*", text)
    # x(...) -> x*(...) ; keep sin(...), erf(...), Integral(...) as calls
    text = re.sub(r"(?<![A-Za-z_])([A-Za-z])\(", r"\1*(", text)
    # function application without parentheses: "sin y'", "ln x"
    names = "|".join(sorted(_FUNCTIONS, key=len, reverse=True))
    text = re.sub(r"\b(" + names + r")\s+(?![({])([A-Za-z_]\w*'*)", r"\1(\2)", text)
    # juxtaposition with whitespace: "x sin(x)", "2 erf(x)"
    text = re.sub(r"(?<=[A-Za-z0-9_)])\s+(?=[A-Za-z_]\w*\s*\()", "*", text)
    # y'sin(y') -> y'*sin(y')
    text = re.sub(r"(?<=')\s*(?=[A-Za-z(])", "*", text)
    cls = "".join(sorted(set(variables)))
    if cls:
        # xy -> x*y, xc -> x*c (never inside a longer word such as "exp")
        text = re.sub(
            r"(?<![A-Za-z_])([" + cls + r"]{2,})(?![A-Za-z_])",
            lambda m: "*".join(m.group(1)),
            text,
        )
        # xsin(x) -> x*sin(x), yet sin(x) stays a call
        text = re.sub(r"(?<=[" + cls + r"])(?=(?:" + names + r")\s*\()", "*", text)
    if differentials:
        # 2x dx, y dy, (x+y) dx
        text = re.sub(r"(?<=[A-Za-z0-9_)])\s*(?=d[xy](?![A-Za-z_]))", "*", text)
    return text


def _primes_to_derivatives(text: str, y: str, x: str) -> str:
    def repl(match: re.Match[str]) -> str:
        name = match.group(1)
        order = len(match.group(2))
        if name != y:
            return match.group(0)
        return f"Derivative({y}, {x}, {order})"

    return re.sub(r"\b([A-Za-z_]\w*)('+)", repl, text)


def _derivative_powers(text: str, y: str, x: str) -> str:
    """Rewrite ``y^(4)`` as the fourth derivative, not a power."""

    def repl(match: re.Match[str]) -> str:
        name = match.group(1)
        if name != y:
            return match.group(0)
        return f"Derivative({y}, {x}, {int(match.group(2))})"

    return re.sub(r"\b([A-Za-z_]\w*)\s*(?:\^|\*\*)\s*\(\s*(\d+)\s*\)", repl, text)


def _strip_derivative_notation(text: str) -> str:
    """Rewrite d²y/dx², dy/dx and d y / d x as y'' and y'."""
    text = re.sub(r"d\s*\^?\s*2\s*y\s*/\s*d\s*x\s*\^?\s*2", "y''", text)
    text = re.sub(r"d\s*y\s*/\s*d\s*x", "y'", text)
    return text


def _symbols(x: str, y: str):
    return sp.Symbol(x, real=True), sp.Symbol(y, real=True)


def _base_locals(x: str, y: str, *, y_function: bool, extra: dict | None = None) -> dict:
    X, Ysym = _symbols(x, y)
    local: dict = {
        x: X,
        "e": sp.E,
        "pi": sp.pi,
        "ln": sp.log,
        "log": sp.log,
    }
    if y_function:
        local[y] = sp.Function(y)(X)
    else:
        local[y] = Ysym
    if extra:
        local.update(extra)
    return local


def parse_expression(text: str, x: str = "x", y: str = "y") -> sp.Expr:
    """Parse an ODE expression where ``y`` means ``y(x)``."""
    text = _normalize(text)
    # ``y^(4)`` must be recognised before implicit multiplication rewrites
    # the caret into a power operator.
    text = _derivative_powers(text, y, x)
    text = _insert_implicit_multiplication(text, variables=x + y + "c")
    text = _primes_to_derivatives(text, y, x)
    return sp.sympify(text, locals=_base_locals(x, y, y_function=True))


def _parse_expr_in_xy(text: str, x: str, y: str) -> sp.Expr:
    """Parse an expression where x and y are plain real symbols."""
    text = _insert_implicit_multiplication(_normalize(text), variables=x + y + "c")
    return sp.sympify(text, locals=_base_locals(x, y, y_function=False))


def _parse_expr_in(text: str, names: Iterable[str]) -> sp.Expr:
    local = {n: sp.Symbol(n, real=True) for n in names}
    local.update({"e": sp.E, "pi": sp.pi, "ln": sp.log, "log": sp.log})
    return sp.sympify(
        _insert_implicit_multiplication(_normalize(text), variables="".join(names)),
        locals=local,
    )


# --------------------------------------------------------- differential forms
def _is_differential_form(text: str) -> bool:
    stripped = _strip_derivative_notation(text)
    return bool(re.search(r"(?<![A-Za-z_])d[xy](?![A-Za-z_])", stripped))


def _split_equation(text: str) -> tuple[str, str]:
    stripped = _strip_derivative_notation(text)
    if stripped.count("=") == 1:
        lhs, rhs = stripped.split("=", 1)
        return lhs.strip(), rhs.strip()
    return stripped.strip(), "0"


def _differential_form(text: str, x: str, y: str):
    """Return ``(M, N)`` for ``M dx + N dy = 0`` or ``(None, error)``."""
    lhs, rhs = _split_equation(text)
    X, Ysym = _symbols(x, y)
    dx, dy = sp.Symbol("_dx"), sp.Symbol("_dy")
    local = _base_locals(x, y, y_function=False, extra={"dx": dx, "dy": dy})
    try:
        expr = sp.sympify(
            _insert_implicit_multiplication(
                _normalize(f"({lhs})-({rhs})"), differentials=True, variables=x + y + "c"
            ),
            locals=local,
        )
    except Exception as exc:
        return None, f"differential form could not be parsed: {exc}"
    expr = sp.expand(expr)
    M = sp.expand(expr).coeff(dx)
    N = sp.expand(expr).coeff(dy)
    remainder = sp.expand(expr - M * dx - N * dy)
    if remainder != 0:
        return None, (
            "the equation is not of the form M(x,y) dx + N(x,y) dy = 0 "
            f"(unexpected remainder: {remainder})"
        )
    if sp.simplify(M) == 0 and sp.simplify(N) == 0:
        return None, "both M and N are zero"
    return (sp.simplify(M), sp.simplify(N)), None


def _rhs_expression(text: str, x: str, y: str) -> tuple[sp.Expr, bool]:
    """Return ``(dy/dx, is_differential_form)``."""
    X, Ysym = _symbols(x, y)
    if _is_differential_form(text):
        mn, error = _differential_form(text, x, y)
        if mn is None:
            raise ValueError(error or "invalid differential form")
        M, N = mn
        if N == 0:
            raise ValueError("N = 0: the differential form cannot be solved for y'")
        return sp.simplify(-M / N), True
    lhs, rhs = _split_equation(text)
    eq = parse_expression(f"({lhs})-({rhs})", x, y)
    dydx = sp.Derivative(sp.Function(y)(X), X)
    sol = sp.solve(sp.Eq(eq, 0), dydx)
    if not sol:
        raise ValueError("the equation cannot be solved for y'")
    return sp.simplify(sol[0]), False


def _ode_equation(text: str, x: str, y: str):
    """Return ``(equation, F, is_differential_form)``."""
    X = sp.Symbol(x, real=True)
    Y = sp.Function(y)
    if _is_differential_form(text):
        mn, error = _differential_form(text, x, y)
        if mn is None:
            raise ValueError(error or "invalid differential form")
        M, N = mn
        if N == 0:
            raise ValueError("N = 0: the differential form cannot be solved for y'")
        Ysym = sp.Symbol(y, real=True)
        Mx = M.subs(Ysym, Y(X))
        Nx = N.subs(Ysym, Y(X))
        F = sp.simplify(Mx + Nx * sp.Derivative(Y(X), X))
        return sp.Eq(sp.Derivative(Y(X), X), sp.simplify(-Mx / Nx)), F, True

    lhs, rhs = _split_equation(text)
    F = parse_expression(f"({lhs})-({rhs})", x, y)
    return sp.Eq(F, 0), F, False


# ----------------------------------------------------------------- verification
def _numerically_zero(expr: sp.Expr, x: str, y: str, *, tol: float = 1e-6) -> bool:
    """Numeric zero test that also fixes constants such as C, C1, C2."""
    X, Y = _symbols(x, y)
    if expr is None:
        return False
    free = {s for s in expr.free_symbols if isinstance(s, sp.Symbol) and s not in (X, Y)}
    constants = {s: sp.Float(1.0 + 0.37 * i) for i, s in enumerate(sorted(free, key=str))}
    checked = 0
    for xv, yv in _MAX_VERIFY_SAMPLES:
        try:
            value = sp.N(
                expr.subs(constants).subs({X: sp.Float(xv, 30), Y: sp.Float(yv, 30)}),
                30,
            )
        except Exception:
            continue
        if not value.is_number or value.has(sp.zoo, sp.nan, sp.oo, -sp.oo):
            continue
        try:
            number = complex(value)
        except Exception:
            continue
        # Samples outside the solution's domain (for example asin(x) for
        # x > 1) are inconclusive, not counter-evidence.  Requiring a few
        # in-domain probes still keeps wrong answers from passing silently.
        if abs(number.imag) > 1e-12:
            continue
        if abs(number.real) > tol:
            return False
        checked += 1
    return checked >= 3


def _find_calls(text: str, name: str) -> list[tuple[int, int, str]]:
    """Locate ``name(...)`` calls and return (start, end, inner) triples."""
    found: list[tuple[int, int, str]] = []
    i = 0
    while True:
        j = text.find(name + "(", i)
        if j < 0:
            break
        if j > 0 and (text[j - 1].isalnum() or text[j - 1] == "_"):
            i = j + 1
            continue
        start = j + len(name) + 1
        depth = 1
        k = start
        while k < len(text) and depth:
            if text[k] == "(":
                depth += 1
            elif text[k] == ")":
                depth -= 1
            k += 1
        if depth:
            break
        found.append((j, k, text[start:k - 1]))
        i = k
    return found


def _split_top_level_args(inner: str) -> list[str]:
    parts: list[str] = []
    depth = 0
    current = ""
    for ch in inner:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append(current)
            current = ""
        else:
            current += ch
    parts.append(current)
    return [p.strip() for p in parts]


def _rewrite_integrals(text: str):
    """Replace ``Integral(g, u)`` (invalid when u is not a symbol) by ``AntiN(u)``.

    Returns ``(rewritten_text, mapping)`` where mapping is a list of
    ``(function_name, u_text, g_text)``.
    """
    out = text
    mapping: list[tuple[str, str, str]] = []
    guard = 0
    while True:
        calls = _find_calls(out, "Integral")
        if not calls:
            break
        guard += 1
        if guard > 64:
            break
        start, end, inner = calls[-1]
        parts = _split_top_level_args(inner)
        g_text = parts[0]
        u_text = parts[1] if len(parts) > 1 else "x"
        # The same Integral(g, u) denotes the same antiderivative everywhere;
        # reusing one symbol keeps algebraically equal integrals in sync.
        name = None
        for existing, eu, eg in mapping:
            if eu.strip() == u_text.strip() and eg.strip() == g_text.strip():
                name = existing
                break
        if name is None:
            name = f"Anti{len(mapping)}"
            mapping.append((name, u_text, g_text))
        out = out[:start] + f"{name}({u_text})" + out[end:]
    return out, mapping


def _parse_candidate(text: str, x: str, y: str, *, y_function: bool = False):
    """Parse a candidate answer, including unevaluated ``Integral`` nodes."""
    body = _insert_implicit_multiplication(_normalize(text), variables=x + y + "c")
    body, mapping = _rewrite_integrals(body)
    local = _base_locals(x, y, y_function=y_function, extra={"C": sp.Symbol("C", real=True)})
    for name, _, _ in mapping:
        local[name] = sp.Function(name)
    expr = sp.sympify(body, locals=local)
    return expr, mapping


def _anti_map(mapping, x: str, y: str):
    """Map each ``AntiN`` function to ``(u, g)`` with u and g in x,y."""
    local = _base_locals(x, y, y_function=False)
    out = {}
    for name, u_text, g_text in mapping:
        u = sp.sympify(
            _insert_implicit_multiplication(_normalize(u_text), variables=x + y + "c"),
            locals=local,
        )
        g = sp.sympify(
            _insert_implicit_multiplication(_normalize(g_text), variables=x + y + "c"),
            locals=local,
        )
        out[sp.Function(name)] = (u, g)
    return out


def _replace_anti_derivatives(expr: sp.Expr, antimap) -> sp.Expr:
    """Turn derivatives of ``AntiN(u)`` back into the integrand."""

    def substitute_g(g, u, point):
        try:
            return g.subs(u, point) if u != point else g
        except Exception:
            return g

    def repl_subs(node):
        inner = node.expr
        fn = getattr(inner.expr, "func", None)
        if fn in antimap:
            u, g = antimap[fn]
            point = node.point[0] if isinstance(node.point, tuple) else node.point
            return substitute_g(g, u, point)
        return node

    def repl_deriv(node):
        fn = getattr(node.expr, "func", None)
        if fn in antimap:
            u, g = antimap[fn]
            arg = node.expr.args[0]
            variables = node.variables
            var = variables[0] if variables else None
            g_at = substitute_g(g, u, arg)
            if var is None:
                return g_at
            try:
                return g_at * sp.diff(arg, var)
            except Exception:
                return g_at
        return node

    out = expr.replace(lambda n: isinstance(n, sp.Subs), repl_subs)
    out = out.replace(lambda n: isinstance(n, sp.Derivative), repl_deriv)
    return out


def _diff(expr: sp.Expr, var, antimap) -> sp.Expr:
    """Differentiate, resolving derivatives of unevaluated antiderivatives."""
    d = sp.diff(expr, var)
    return _replace_anti_derivatives(d, antimap)


def _substitute_candidate(F: sp.Expr, candidate: sp.Expr, antimap, x: str, y: str) -> sp.Expr:
    X = sp.Symbol(x, real=True)
    Y = sp.Function(y)
    orders = [
        d.derivative_count
        for d in F.atoms(sp.Derivative)
        if d.expr == Y(X)
    ]
    max_order = max(orders, default=0)
    deriv = candidate
    repl = {}
    for k in range(1, max_order + 1):
        deriv = _diff(deriv, X, antimap)
        repl[sp.Derivative(Y(X), X, k)] = deriv
    residual = F.subs(repl).subs(Y(X), candidate)
    residual = _replace_anti_derivatives(residual, antimap)
    return sp.simplify(residual)


def _has_opaque(expr: sp.Expr) -> bool:
    if expr.has(sp.Subs, sp.Integral):
        return True
    return any(
        isinstance(a, sp.core.function.AppliedUndef)
        for a in expr.atoms(sp.core.function.AppliedUndef)
    )


def _verified(residual: sp.Expr, x: str, y: str) -> bool:
    if residual is None:
        return False
    try:
        if residual == 0:
            return True
    except Exception:
        pass
    if _has_opaque(residual):
        return False
    return _numerically_zero(residual, x, y)


def verify_candidate(
    text: str,
    candidate_latex: str,
    *,
    x: str = "x",
    y: str = "y",
    implicit: bool = False,
) -> dict:
    """Verify a native solution with SymPy.

    ``candidate_latex`` must be a SymPy-parseable expression.  For an implicit
    solution, pass the left-hand side ``u(x,y)`` of ``u(x,y)=C``.
    """
    try:
        X, Ysym = _symbols(x, y)
        _, F, _ = _ode_equation(text, x, y)
        if implicit:
            u, mapping = _parse_candidate(candidate_latex, x, y, y_function=False)
            # |z| only records a branch choice.  Differentiating it produces
            # sign()/DiracDelta artifacts that SymPy cannot cancel, so work on
            # the branch where z >= 0, which is what the numeric probes use.
            u = u.replace(sp.Abs, lambda a: a)
            antimap = _anti_map(mapping, x, y)
            Ysym_local = sp.Symbol(y, real=True)
            ux = _diff(u, X, antimap)
            uy = _diff(u, Ysym_local, antimap)
            if sp.simplify(uy) == 0:
                return {
                    "ok": True,
                    "residual": "candidate does not depend on y",
                    "verified": False,
                    "reason": "implicit candidate is independent of y",
                }
            orders = [
                d.derivative_count
                for d in F.atoms(sp.Derivative)
                if d.expr == sp.Function(y)(X)
            ]
            max_order = max(orders, default=0)
            if max_order <= 1:
                try:
                    yp, _ = _rhs_expression(text, x, y)
                except Exception:
                    yp = None
                if yp is None:
                    yp = sp.simplify(-ux / uy)
                else:
                    yp = sp.simplify(yp.subs(sp.Function(y)(X), Ysym_local))
                residual = sp.simplify(ux + uy * yp)
            elif max_order == 2:
                # Implicit differentiation to second order:
                #   y'  = -u_x/u_y
                #   y'' = -(u_xx + 2 u_xy y' + u_yy y'^2)/u_y
                yp = sp.simplify(-ux / uy)
                uxx = _diff(ux, X, antimap)
                uxy = _diff(ux, Ysym_local, antimap)
                uyy = _diff(uy, Ysym_local, antimap)
                ypp = sp.simplify(-(uxx + 2 * uxy * yp + uyy * yp**2) / uy)
                Y = sp.Function(y)
                repl = {
                    Y(X): Ysym_local,
                    sp.Derivative(Y(X), X): yp,
                    sp.Derivative(Y(X), X, 2): ypp,
                }
                residual = sp.simplify(F.subs(repl))
            else:
                return {
                    "ok": True,
                    "residual": "implicit verification supports order <= 2",
                    "verified": False,
                    "reason": f"order {max_order} is too high for this checker",
                }
        else:
            expr, mapping = _parse_candidate(candidate_latex, x, y, y_function=False)
            expr = expr.replace(sp.Abs, lambda a: a)
            Y = sp.Function(y)
            if expr.has(Ysym) or expr.has(Y(X)):
                return {
                    "ok": True,
                    "residual": "candidate still contains y",
                    "verified": False,
                    "reason": "explicit candidate is not solved for y",
                }
            antimap = _anti_map(mapping, x, y)
            residual = _substitute_candidate(F, expr, antimap, x, y)
        return {
            "ok": True,
            "residual": str(residual),
            "verified": _verified(residual, x, y),
        }
    except Exception as exc:  # verification failures are data, not crashes
        return {"ok": False, "error": f"{type(exc).__name__}: {exc}", "verified": False}


# --------------------------------------------------------------------- solving
def solve_ode(text: str, x: str = "x", y: str = "y", timeout: float | None = 20.0) -> OracleResult:
    result = OracleResult(ok=False, input=text)
    try:
        X = sp.Symbol(x, real=True)
        Y = sp.Function(y)
        ode, F, is_diff_form = _ode_equation(text, x, y)
        hints = list(sp.classify_ode(ode, Y(X)))
        result.classification = hints
        if is_diff_form:
            result.hints.append("differential form M dx + N dy = 0")

        dsol = None
        errors: list[str] = []
        for hint in hints[:10] or [None]:
            try:
                with _time_limit(timeout):
                    candidate = sp.dsolve(ode, Y(X), hint=hint)
                if candidate is not None:
                    dsol = candidate
                    break
            except Exception as exc:  # try the next hint
                errors.append(f"{hint}: {type(exc).__name__}: {exc}")
        if dsol is None:
            try:
                with _time_limit(timeout):
                    dsol = sp.dsolve(ode, Y(X))
            except Exception as exc:
                errors.append(f"default: {type(exc).__name__}: {exc}")
        if dsol is None:
            raise ValueError("; ".join(errors[-3:]) or "dsolve produced no solution")

        solutions = dsol if isinstance(dsol, list) else [dsol]
        solutions = [s for s in solutions if not s.has(sp.O)]
        if not solutions:
            raise ValueError("dsolve returned only asymptotic series")
        result.all_solutions = [str(s) for s in solutions]
        dsol = solutions[0]
        result.solution = str(dsol)
        result.latex = sp.latex(dsol)
        result.ok = True

        # Verify the first solution; for Clairaut/Lagrange also try the rest.
        for candidate in solutions:
            if not isinstance(candidate, sp.Eq):
                continue
            with _time_limit(timeout):
                if candidate.lhs == Y(X):
                    check = verify_candidate(text, str(candidate.rhs), x=x, y=y)
                else:
                    check = verify_candidate(text, str(candidate.lhs), x=x, y=y, implicit=True)
            if check.get("verified"):
                result.verified = True
                break
            detail = check.get("residual") or check.get("error") or check.get("reason")
            if detail:
                result.hints.append(f"residual: {detail}")
    except Exception as exc:  # oracle failures must not abort a comparison run
        result.error = f"{type(exc).__name__}: {exc}"
    return result


def verify_parametric(
    text: str,
    px_text: str,
    py_text: str,
    *,
    p: str = "p",
    x: str = "x",
    y: str = "y",
) -> dict:
    """Check a parametric solution ``x(p), y(p)`` of a first-order ODE."""
    try:
        P = sp.Symbol(p, real=True)
        C = sp.Symbol("C", real=True)
        X, Ysym = _symbols(x, y)
        local = dict(_base_locals(x, y, y_function=False, extra={"C": C}))
        local[p] = P
        px = sp.sympify(
            _insert_implicit_multiplication(_normalize(px_text), variables=x + y + p + "C"),
            locals=local,
        )
        py = sp.sympify(
            _insert_implicit_multiplication(_normalize(py_text), variables=x + y + p + "C"),
            locals=local,
        )
        dx = sp.simplify(sp.diff(px, P))
        dy = sp.simplify(sp.diff(py, P))
        if dx == 0:
            return {"ok": False, "verified": False, "error": "dx/dp vanishes"}
        slope = sp.simplify(dy / dx)
        _, F, _ = _ode_equation(text, x, y)
        Y = sp.Function(y)
        orders = [
            d.derivative_count
            for d in F.atoms(sp.Derivative)
            if d.expr == Y(X)
        ]
        max_order = max(orders, default=1)
        # Parametric higher derivatives: d/dx = (d/dp)/(dx/dp).
        derivs = {1: slope}
        current = slope
        for k in range(2, max_order + 1):
            current = sp.simplify(sp.diff(current, P) / dx)
            derivs[k] = current
        Fp = F
        for k in sorted(derivs, reverse=True):
            Fp = Fp.subs(sp.Derivative(Y(X), X, k), derivs[k])
        Fp = Fp.subs(Y(X), py).subs(X, px)
        Fp = sp.simplify(Fp)
        return {
            "ok": True,
            "verified": _zero_in_p(Fp, P),
            "residual": str(Fp),
            "slope": str(slope),
            "derivatives": {k: str(v) for k, v in derivs.items()},
        }
    except Exception as exc:
        return {"ok": False, "verified": False, "error": f"{type(exc).__name__}: {exc}"}


def _zero_in_p(expr: sp.Expr, P) -> bool:
    """Numeric zero test for an expression depending on the parameter p."""
    free = {s for s in expr.free_symbols if isinstance(s, sp.Symbol) and s != P}
    constants = {s: sp.Float(1.0 + 0.3 * i) for i, s in enumerate(sorted(free, key=str))}
    for pv in (0.37, 0.83, 1.41, 2.07, 2.63):
        try:
            value = sp.N(expr.subs(constants).subs(P, sp.Float(pv, 30)), 30)
        except Exception:
            return False
        if not value.is_number or value.has(sp.zoo, sp.nan, sp.oo, -sp.oo):
            return False
        if abs(complex(value)) > 1e-5:
            return False
    return True


# --------------------------------------------------------------------- algebra
def wronskian(functions: Iterable[str], x: str = "x") -> dict:
    X = sp.Symbol(x, real=True)

    def parse_fn(text: str) -> sp.Expr:
        return sp.sympify(
            _insert_implicit_multiplication(_normalize(text), variables=x),
            locals={x: X, "e": sp.E, "pi": sp.pi, "ln": sp.log, "log": sp.log},
        )

    fs = [parse_fn(f) for f in functions]
    n = len(fs)
    if n == 0:
        return {"ok": False, "error": "no functions given", "wronskian": "0", "independent": False}
    matrix = sp.Matrix([[sp.diff(fs[j], X, i) for j in range(n)] for i in range(n)])
    W = sp.simplify(matrix.det())
    independent = bool(W != 0)
    if not independent:
        # guard against simplify failures: sample numerically
        for xv in (0.37, 1.13, 2.41, 0.79, 3.17):
            try:
                value = sp.N(W.subs(X, sp.Float(xv, 30)), 30)
                if value.is_number and abs(complex(value)) > 1e-9:
                    independent = True
                    break
            except Exception:
                continue
    return {
        "ok": True,
        "wronskian": str(W),
        "latex": sp.latex(W),
        "independent": independent,
        "matrix": [[str(entry) for entry in row] for row in matrix.tolist()],
    }


def envelope(
    family: str,
    c: str = "c",
    x: str = "x",
    y: str = "y",
    orthogonal: bool = False,
) -> dict:
    X, Y, C = sp.Symbol(x, real=True), sp.Symbol(y, real=True), sp.Symbol(c, real=True)
    lhs, rhs = _split_equation(family)
    F = sp.sympify(
        _insert_implicit_multiplication(
            _normalize(f"({lhs})-({rhs})"), variables=x + y + c
        ),
        locals=_base_locals(x, y, y_function=False, extra={c: C}),
    )
    Fc = sp.simplify(sp.diff(F, C))
    candidates = sp.solve(Fc, C, dict=False)
    curves: list[str] = []
    warnings: list[str] = []
    for cv in candidates:
        if not cv.free_symbols & {X, Y}:
            warnings.append(f"{c} = {cv} is independent of x and y; degenerate member")
            continue
        curve = sp.simplify(F.subs(C, cv))
        if curve == 0:
            warnings.append("degenerate curve 0 = 0")
            continue
        curves.append(str(sp.Eq(curve, 0)))

    payload = {
        "ok": True,
        "family": family,
        "F": str(F),
        "Fc": str(Fc),
        "candidates": [str(cv) for cv in candidates],
        "envelope": curves,
        "warnings": warnings,
    }

    if orthogonal:
        Fx = sp.simplify(sp.diff(F, X))
        Fy = sp.simplify(sp.diff(F, Y))
        if Fy == 0:
            payload["orthogonal"] = ""
            payload["warnings"].append(
                "the family slope is undefined; orthogonal trajectories were not computed"
            )
            return payload
        slope = sp.simplify(-Fx / Fy)
        # eliminate c from the slope using F = 0
        cvals = sp.solve(F, C, dict=False)
        if cvals:
            slope = sp.simplify(slope.subs(C, cvals[0]))
        if slope.has(C):
            payload["orthogonal"] = ""
            payload["warnings"].append(
                "c could not be eliminated from the family slope; no orthogonal family"
            )
            payload["orthogonal_slope"] = str(slope)
            return payload
        ortho = sp.simplify(-1 / slope)
        payload["family_slope"] = str(slope)
        payload["orthogonal_slope"] = str(ortho)
        try:
            Yfun = sp.Function(y)
            ortho_y = ortho.subs(Y, Yfun(X))
            ode = sp.Eq(Yfun(X).diff(X), ortho_y)
            with _time_limit(15):
                sol = sp.dsolve(ode, Yfun(X))
            solutions = sol if isinstance(sol, list) else [sol]
            verified = None
            for candidate in solutions:
                if _verify_orthogonal(ortho_y, candidate, X):
                    sol = candidate
                    verified = True
                    break
            if verified is None:
                sol = solutions[0]
                verified = False
            payload["orthogonal"] = str(sol)
            payload["orthogonal_latex"] = sp.latex(sol)
            payload["orthogonal_verified"] = verified
            payload["all_orthogonal"] = [str(c) for c in solutions]
        except Exception as exc:
            payload["orthogonal"] = ""
            payload["warnings"].append(f"orthogonal ODE could not be solved: {exc}")
    return payload


def oblique(
    family: str,
    c: str = "c",
    angle: float = 45.0,
    x: str = "x",
    y: str = "y",
) -> dict:
    """Reference slope of the family that crosses ``family`` at ``angle``.

    The crossing angle is measured in degrees.  The returned
    ``trajectory_slope`` is the right-hand side of the first-order ODE that the
    native solver integrates; ``verify_candidate`` (or the caller) can then
    check a native solution against it.
    """
    X, Y, C = sp.Symbol(x, real=True), sp.Symbol(y, real=True), sp.Symbol(c, real=True)
    lhs, rhs = _split_equation(family)
    F = sp.sympify(
        _insert_implicit_multiplication(
            _normalize(f"({lhs})-({rhs})"), variables=x + y + c
        ),
        locals=_base_locals(x, y, y_function=False, extra={c: C}),
    )
    payload = {
        "ok": True,
        "family": family,
        "angle": angle,
        "F": str(F),
        "family_slope": "",
        "trajectory_slope": "",
        "warnings": [],
    }
    Fx = sp.simplify(sp.diff(F, X))
    Fy = sp.simplify(sp.diff(F, Y))
    if Fy == 0:
        # Vertical family: every member has an infinite slope.
        payload["warnings"].append(
            "the family slope is infinite; the trajectory slope is the cotangent"
        )
        if abs(angle - 90.0) < 1e-9:
            payload["trajectory_slope"] = "0"
        else:
            payload["trajectory_slope"] = str(
                sp.simplify(1 / sp.tan(sp.pi * angle / 180))
            )
        return payload

    slope = sp.simplify(-Fx / Fy)
    cvals = sp.solve(F, C, dict=False)
    if cvals:
        slope = sp.simplify(slope.subs(C, cvals[0]))
    if slope.has(C):
        payload["family_slope"] = str(slope)
        payload["warnings"].append(
            "c could not be eliminated from the family slope; no trajectory family"
        )
        return payload
    payload["family_slope"] = str(slope)

    if abs(angle - 90.0) < 1e-9:
        if slope == 0:
            payload["warnings"].append("the trajectory is vertical (x = C)")
            return payload
        payload["trajectory_slope"] = str(sp.simplify(-1 / slope))
        return payload

    tangent = sp.tan(sp.pi * angle / 180)
    denominator = sp.simplify(1 - slope * tangent)
    if denominator == 0:
        payload["warnings"].append("the trajectory is vertical (x = C)")
        return payload
    payload["trajectory_slope"] = str(
        sp.simplify((slope + tangent) / denominator)
    )
    return payload


def _verify_orthogonal(ortho: sp.Expr, solution: sp.Eq, X) -> bool:
    try:
        if not isinstance(solution, sp.Eq) or solution.lhs != sp.Function("y")(X):
            return False
        rhs = solution.rhs
        if rhs.has(sp.Symbol("y", real=True)):
            return False  # not solved explicitly for y
        yp = sp.diff(rhs, X)
        residual = sp.simplify(ortho.subs(sp.Function("y")(X), rhs) - yp)
        return _numerically_zero(residual, str(X), "y")
    except Exception:
        return False
