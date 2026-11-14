"""PDE topic oracle tests.

Calls the native engine through the same C ABI the Flutter app uses and checks
every PDE answer with SymPy: the residual of the equation, the initial or
boundary data, and the classification are all verified independently.

The strict ``json.loads`` also guards the response contract: step and check
``params`` must be valid JSON objects.
"""

from __future__ import annotations

import ctypes
import json
import pathlib

import pytest
import sympy as sp

ROOT = pathlib.Path(__file__).resolve().parents[2]
DEFAULT_LIB = ROOT / "core" / "build" / "lib" / "libodecore.so"

X = sp.Symbol("x", real=True)
Y = sp.Symbol("y", real=True)
LOCALS = {"x": X, "y": Y, "e": sp.E, "pi": sp.pi}


class Engine:
    def __init__(self, path: pathlib.Path) -> None:
        if not path.exists():
            pytest.skip(f"native library not built: {path}")
        self.lib = ctypes.CDLL(str(path))
        self.lib.ode_solve.argtypes = [ctypes.c_char_p]
        self.lib.ode_solve.restype = ctypes.c_void_p
        self.lib.ode_free.argtypes = [ctypes.c_void_p]

    def call(self, request: dict) -> dict:
        pointer = self.lib.ode_solve(json.dumps(request).encode("utf-8"))
        if not pointer:
            raise AssertionError("native engine returned a null response")
        try:
            return json.loads(ctypes.string_at(pointer).decode("utf-8"))
        finally:
            self.lib.ode_free(pointer)


@pytest.fixture(scope="module")
def engine() -> Engine:
    return Engine(DEFAULT_LIB)


def sympy_of(native: dict | str) -> sp.Expr:
    text = native["plain"] if isinstance(native, dict) else native
    return sp.sympify(text.replace("^", "**"), locals=LOCALS)


def topic(engine: Engine, name: str, **params) -> dict:
    request = {"mode": "topic", "topic": name, "lang": "en"}
    request.update(params)
    return engine.call(request)


def assert_checks(native: dict) -> None:
    assert native["ok"], native.get("error_code")
    assert native["checks"], "a solved topic must report checks"
    for check in native["checks"]:
        assert check["ok"], (check["name_key"], check.get("detail_math"))


def assert_residual(expr: sp.Expr, residual: sp.Expr) -> None:
    simplified = sp.simplify(residual)
    assert simplified == 0, f"residual {simplified} does not vanish"


FIRST_ORDER = [
    (1, 1, 0, "sin(x)"),
    (2, -3, 1, "x^2"),
    (-1, 3, 2, "cos(x)"),
    (0, 2, 3, "exp(x)"),
    (5, -1, -2, "x^3 - 2*x"),
    (1, 2, -4, "1/(1 + x^2)"),
    (3, 1, 5, "tanh(x)"),
]


@pytest.mark.parametrize("a,b,c,profile", FIRST_ORDER)
def test_first_order(engine: Engine, a, b, c, profile):
    native = topic(engine, "pde_first_order", a=a, b=b, c=c, f=profile)
    assert_checks(native)
    u = sympy_of(native["solution"])
    f = sympy_of(profile)
    assert_residual(u, a * sp.diff(u, X) + b * sp.diff(u, Y) - c)
    assert sp.simplify(u.subs(Y, 0) - f) == 0


@pytest.mark.parametrize(
    "a,b,p,q,profile",
    [
        (1, 1, 1, 0, "sin(x)"),
        (2, 1, 3, 1, "x^2"),
        (1, 1, 0, 2, "x"),
        (-1, 2, -1, 0, "cos(x)"),
        (3, 1, 2, -4, "x^2 + 1"),
        (1, -2, -1, 3, "exp(x)"),
        (2, 3, 1, -2, "sin(2*x) + x"),
    ],
)
def test_characteristics(engine: Engine, a, b, p, q, profile):
    native = topic(engine, "pde_characteristics", a=a, b=b, p=p, q=q, f=profile)
    assert_checks(native)
    u = sympy_of(native["solution"])
    f = sympy_of(profile)
    residual = a * sp.diff(u, X) + b * sp.diff(u, Y) - p * u - q
    assert_residual(u, residual)
    assert sp.simplify(u.subs(Y, 0) - f) == 0


CLASSIFY = [
    (1, 0, 1),
    (1, 2, 1),
    (1, 4, 1),
    (1, 0, -1),
    (2, 0, 8),
    (0, 2, 1),
    (0, 0, 3),
    (-1, 0, -1),
    (1, -4, 4),
]


@pytest.mark.parametrize("A,B,C", CLASSIFY)
def test_classification(engine: Engine, A, B, C):
    native = topic(engine, "pde_classify", A=A, B=B, C=C)
    assert_checks(native)
    discriminant = B * B - 4 * A * C
    expected = (
        "hyperbolic"
        if discriminant > 0
        else "elliptic"
        if discriminant < 0
        else "parabolic"
    )
    assert native["solution"]["plain"] == expected

    # The discriminant the engine reports is the same one SymPy sees.
    reported = next(
        check for check in native["checks"] if check["name_key"] == "check.discriminant_sign"
    )
    assert reported["params"]["discriminant"] == pytest.approx(discriminant)


@pytest.mark.parametrize(
    "A,C,L,n",
    [
        (1, 1, sp.pi, 1),
        (-1, 1, sp.pi, 1),
        (4, 1, 2, 2),
        (1, 4, sp.Rational(3, 2), 3),
        (9, 4, 1, 2),
        (-4, 9, sp.pi, 4),
    ],
)
def test_separation(engine: Engine, A, C, L, n):
    native = topic(engine, "pde_separation", A=A, C=C, L=float(L), n=n)
    assert_checks(native)
    u = sympy_of(native["solution"])
    # The truncated series satisfies the PDE for arbitrary coefficients.  The
    # separated frequency is a rational approximation of sqrt(A/C), so check
    # the residual numerically rather than expecting an exact cancellation.
    residual = A * sp.diff(u, X, 2) + C * sp.diff(u, Y, 2)
    free = [s for s in residual.free_symbols if s not in (X, Y) and s != sp.pi]
    sampled = residual.subs({symbol: 1 for symbol in free})
    values = sp.lambdify((X, Y), sampled, "math")
    for xv, yv in [(0.37, 0.21), (1.13, -0.33), (2.05, 0.47)]:
        assert abs(values(xv, yv)) < 1e-6
    # Every mode obeys the zero boundary values at x = 0 and x = L.
    length = sp.pi if abs(float(L) - float(sp.pi)) < 1e-9 else sp.Rational(str(float(L)))
    assert sp.simplify(u.subs(X, 0)) == 0
    assert sp.simplify(u.subs(X, length)) == 0


def test_separation_eigenvalues(engine: Engine):
    L, n = 1.7, 3
    native = topic(engine, "pde_separation", A=2, C=5, L=L, n=n)
    assert_checks(native)
    table = next(
        step["note"]
        for step in native["steps"]
        if step.get("note", "").startswith("n\t")
    )
    rows = [line.split("\t") for line in table.strip().splitlines()[1:]]
    assert len(rows) == n
    for row in rows:
        index = int(row[0])
        lam = sp.sympify(row[1].replace("·", "*").replace("π", "pi").replace("²", "**2"))
        length = sp.Rational(str(L))
        assert sp.simplify(lam - 2 * (index * sp.pi / length) ** 2) == 0


@pytest.mark.parametrize(
    "payload,code",
    [
        ({"topic": "pde_first_order", "a": 1, "b": 0, "c": 0, "f": "x"},
         "degenerate_characteristics"),
        ({"topic": "pde_characteristics", "a": 1, "b": 0, "p": 1, "q": 0, "f": "x"},
         "degenerate_characteristics"),
        ({"topic": "pde_first_order", "a": 1, "b": 1, "c": 0, "f": "wibble(x)"},
         "unknown_function"),
        ({"topic": "pde_classify", "A": 0, "B": 0, "C": 0}, "bad_principal_part"),
        ({"topic": "pde_separation", "A": 1, "C": 0, "L": 1, "n": 1},
         "bad_principal_part"),
        ({"topic": "pde_separation", "A": 1, "C": 1, "L": 0, "n": 1}, "bad_length"),
    ],
)
def test_pde_errors(engine: Engine, payload, code):
    native = engine.call({"mode": "topic", "lang": "en", **payload})
    assert not native["ok"]
    assert native["error_code"] == code
