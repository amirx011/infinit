"""Oracle tests for orthogonal/oblique trajectories and the Wronskian.

The native engine is called through the C ABI.  Family slopes, trajectory
slopes and implicit derivatives are recomputed with SymPy so the check does not
reuse the engine's own algebra.
"""

from __future__ import annotations

import ctypes
import json
import math
import pathlib

import pytest
import sympy as sp

ROOT = pathlib.Path(__file__).resolve().parents[2]
DEFAULT_LIB = ROOT / "core" / "build" / "lib" / "libodecore.so"

X = sp.Symbol("x", real=True)
Y = sp.Symbol("y", real=True)
C = sp.Symbol("c", real=True)
# The engine writes the integration constant as an upper-case C.
K = sp.Symbol("C", real=True)
LOCALS = {
    "x": X,
    "y": Y,
    "c": C,
    "C": K,
    "e": sp.E,
    "pi": sp.pi,
    "ln": sp.log,
    "abs": sp.Abs,
    "sqrt": sp.sqrt,
    "exp": sp.exp,
    "sin": sp.sin,
    "cos": sp.cos,
    "tan": sp.tan,
    "sec": sp.sec,
    "csc": sp.csc,
    "cot": sp.cot,
    "asin": sp.asin,
    "acos": sp.acos,
    "atan": sp.atan,
    "sinh": sp.sinh,
    "cosh": sp.cosh,
    "tanh": sp.tanh,
}


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


def parse(text: str) -> sp.Expr:
    return sp.sympify(text.replace("^", "**").replace("−", "-"), locals=LOCALS)


def number(expr: sp.Expr, point: dict) -> complex:
    return complex(sp.N(expr.subs(point), 30))


def family_slope(family: str) -> sp.Expr:
    lhs, rhs = family.split("=")
    F = sp.simplify(parse(lhs) - parse(rhs))
    Fx = sp.diff(F, X)
    Fy = sp.diff(F, Y)
    branches = sp.solve(sp.Eq(F, 0), C)
    assert branches, f"the oracle could not eliminate c from {family}"
    out = []
    for branch in branches:
        m = sp.simplify((-Fx / Fy).subs(C, branch))
        if not any(sp.simplify(m - old) == 0 for old in out):
            out.append(m)
    # For a family such as x^2+y^2=c^2 the two signs of c give one slope.
    unique = [m for m in out if not any(sp.simplify(m - old) == 0 for old in out if old is not m)]
    return unique[0] if unique else out[0]


POINTS = [
    {X: sp.Rational(13, 10), Y: sp.Rational(7, 10)},
    {X: sp.Rational(8, 10), Y: sp.Rational(19, 10)},
    {X: sp.Rational(21, 10), Y: sp.Rational(4, 10)},
    {X: sp.Rational(5, 10), Y: sp.Rational(12, 10)},
    {X: sp.Rational(17, 10), Y: sp.Rational(23, 10)},
]


ORTHOGONAL_FAMILIES = [
    "y = c*x",
    "y = c*x^2",
    "y = c*x^3",
    "x^2 + y^2 = c^2",
    "x*y = c",
    "y = c*e^x",
    "y = c/x",
    "y^2 = c*x",
    "y = c*sin(x)",
    "y = c*cos(x)",
    "x^2 - y^2 = c",
    "y = x + c",
]

OBLIQUE_FAMILIES = [
    "y = c*x",
    "y = c*x^2",
    "y = c*x^3",
    "x^2 + y^2 = c^2",
    "x*y = c",
    "y = c*e^x",
    "y = c/x",
    "y^2 = c*x",
    "x^2 - y^2 = c",
]


def canonical_angle(angle: float) -> float:
    """Reduce a crossing angle the same way the engine does.

    A crossing angle is defined modulo 180 degrees, and odd multiples of 90
    collapse to the orthogonal case so tan() never sits on a pole.
    """
    value = math.fmod(angle, 180.0)
    if value <= -90.0:
        value += 180.0
    if value > 90.0:
        value -= 180.0
    if abs(value) < 1e-9:
        value = 0.0
    if abs(abs(value) - 90.0) < 1e-9:
        value = 90.0
    return value


def slope_case(engine: Engine, family: str, angle: float | None, mode: str) -> None:
    request = {
        "mode": mode,
        "input": family,
        "c": "c",
        "x": "x",
        "y": "y",
        "lang": "en",
    }
    if angle is not None:
        request["angle"] = angle
    native = engine.call(request)
    assert native["ok"], (family, angle, native.get("error_code"))
    assert native["trajectory_slope"], (family, angle, "missing slope field")
    m_family = family_slope(family)
    m_native = parse(native["trajectory_slope"]["plain"])

    canonical = canonical_angle(angle) if angle is not None else 90.0
    if mode == "orthogonal" or canonical == 90:
        # A 90-degree crossing is the orthogonal case; taking tan(90) directly
        # gives complex infinity, so use the limit -1/m instead.
        expected = sp.simplify(-1 / m_family)
    elif canonical == 0:
        expected = sp.simplify(m_family)
    else:
        t = sp.tan(sp.pi * sp.Rational(canonical) / 180)
        expected = sp.simplify((m_family + t) / (1 - m_family * t))

    for point in POINTS:
        want = number(expected, point)
        got = number(m_native, point)
        if abs(want) < 1e-12:
            assert abs(got) < 1e-8
        elif abs(got) < 1e-8:
            assert math.isinf(abs(want))
        else:
            assert abs(got - want) <= 1e-7 * max(1.0, abs(want))

    # The trajectory curve itself must have the reported slope.  The answer is
    # a family, so pick one member (c = 2) before differentiating.
    expr = native["trajectory"]
    assert expr is not None, (family, angle, "trajectory curve missing")
    u = parse(expr["plain"])
    if u.has(K):
        u = u.subs(K, sp.Integer(2))
    if native["trajectory_explicit"]:
        derivative = sp.diff(u, X)
        for point in POINTS:
            xv = point[X]
            yv = complex(sp.N(u.subs(X, xv), 30))
            slope = number(m_native, {X: xv, Y: yv})
            want = complex(sp.N(derivative.subs(X, xv), 30))
            if abs(want) < 1e-12:
                assert abs(slope) < 1e-8
            else:
                assert abs(slope - want) <= 1e-6 * max(1.0, abs(want))
    else:
        implicit_slope = sp.simplify(-sp.diff(u, X) / sp.diff(u, Y))
        for point in POINTS:
            want = number(implicit_slope, point)
            got = number(m_native, point)
            if abs(want) < 1e-12:
                assert abs(got) < 1e-8
            else:
                assert abs(got - want) <= 1e-6 * max(1.0, abs(want))


@pytest.mark.parametrize("family", ORTHOGONAL_FAMILIES)
def test_orthogonal_slope_field(engine: Engine, family: str) -> None:
    slope_case(engine, family, None, "orthogonal")


@pytest.mark.parametrize("family", OBLIQUE_FAMILIES)
@pytest.mark.parametrize("angle", [0, 30, 45, 60, 90, -30, -45, 150, 180, -90, 270, 360])
def test_oblique_slope_field(engine: Engine, family: str, angle: int) -> None:
    slope_case(engine, family, angle, "oblique")
    native = engine.call(
        {
            "mode": "oblique",
            "input": family,
            "c": "c",
            "x": "x",
            "y": "y",
            "angle": angle,
            "lang": "en",
        }
    )
    assert native["trajectory_angle"] == pytest.approx(canonical_angle(angle))


@pytest.mark.parametrize(
    "angle, equivalent",
    [(270, 90), (-90, 90), (450, 90), (180, 0), (-180, 0), (360, 0),
     (150, -30), (330, -30), (-210, -30)],
)
def test_oblique_angle_is_normalised(engine: Engine, angle: int, equivalent: int) -> None:
    def answer(value: int) -> tuple[str, str]:
        native = engine.call(
            {
                "mode": "oblique",
                "input": "y = c*x",
                "c": "c",
                "x": "x",
                "y": "y",
                "angle": value,
                "lang": "en",
            }
        )
        assert native["ok"], native.get("error_code")
        return (
            native["trajectory_slope"]["plain"],
            native["trajectory"]["plain"],
        )

    assert answer(angle) == answer(equivalent)
    # No answer may carry the huge floating-point tan value near a pole.
    for text in answer(angle):
        assert "16331239" not in text


def test_orthogonal_and_oblique_vertical_families(engine: Engine) -> None:
    # Horizontal family y = c: orthogonal trajectories are the vertical lines
    # x = C, and a zero-degree oblique crossing leaves the family unchanged.
    vertical = engine.call(
        {"mode": "orthogonal", "input": "y = c", "c": "c", "x": "x", "y": "y",
         "lang": "en"}
    )
    assert vertical["ok"]
    assert vertical["orthogonal_slope"] is None
    assert vertical["orthogonal"]["plain"] == "x"

    same = engine.call(
        {"mode": "oblique", "input": "y = c", "c": "c", "x": "x", "y": "y",
         "angle": 180, "lang": "en"}
    )
    assert same["ok"]
    assert same["trajectory"]["plain"] == "y"
    assert same["trajectory_slope"]["plain"] == "0"

    # Vertical family x = c: orthogonal trajectories are horizontal, and the
    # same family comes back at 0 degrees.
    horizontal = engine.call(
        {"mode": "orthogonal", "input": "x = c", "c": "c", "x": "x", "y": "y",
         "lang": "en"}
    )
    assert horizontal["ok"]
    assert horizontal["orthogonal"]["plain"] == "y"
    assert horizontal["orthogonal_slope"]["plain"] == "0"

    same = engine.call(
        {"mode": "oblique", "input": "x = c", "c": "c", "x": "x", "y": "y",
         "angle": 0, "lang": "en"}
    )
    assert same["ok"]
    assert same["trajectory"]["plain"] == "x"

    right = engine.call(
        {"mode": "oblique", "input": "x = c", "c": "c", "x": "x", "y": "y",
         "angle": 90, "lang": "en"}
    )
    assert right["ok"]
    assert right["trajectory"]["plain"] == "y"


def test_oblique_unsolvable_family_is_reported(engine: Engine) -> None:
    # y = c sin(x) at 45 degrees has no closed form in the current engine.  It
    # must fail with a code instead of reporting success with an empty answer.
    native = engine.call(
        {
            "mode": "oblique",
            "input": "y = c*sin(x)",
            "c": "c",
            "x": "x",
            "y": "y",
            "angle": 45,
            "lang": "en",
        }
    )
    assert not native["ok"]
    assert native["error_code"]
    assert native["solution"] is None


WRONSKIAN_SETS = [
    (["x", "x^2"], True),
    (["sin(x)", "cos(x)"], True),
    (["e^x", "e^(2*x)"], True),
    (["x", "x*ln(x)"], True),
    (["1", "x", "x^2"], True),
    (["e^x", "e^(2*x)", "e^(3*x)"], True),
    (["x", "2*x"], False),
    (["sin(x)^2", "cos(x)^2", "1"], False),
    # W(x, x+1) = 1, so these are independent even though their difference is
    # constant; only a zero Wronskian proves dependence on an interval.
    (["x", "x + 1"], True),
    (["x", "x"], False),
    (["1", "1"], False),
    (["e^x", "2*e^x"], False),
    (["sin(x)", "sin(x+1)"], True),
    (["1", "x", "x^2", "x^3"], True),
    (["e^x", "x*e^x", "x^2*e^x"], True),
    # W = 0 everywhere, but x^2 and x|x| are still independent on the whole
    # real line: a zero Wronskian only proves dependence for smooth enough
    # functions (these are not analytic at 0).
    (["x^2", "x*abs(x)"], True),
]


@pytest.mark.parametrize("functions, independent", WRONSKIAN_SETS)
def test_wronskian(engine: Engine, functions: list[str], independent: bool) -> None:
    native = engine.call(
        {"mode": "wronskian", "functions": functions, "x": "x", "lang": "en"}
    )
    assert native["ok"], native.get("error_code")
    assert native["independent"] is independent
    fs = [parse(f) for f in functions]
    expected = sp.simplify(sp.Matrix([[sp.diff(f, X, i) for f in fs] for i in range(len(fs))]).det())
    got = parse(native["wronskian"]["plain"])
    assert sp.simplify(got - expected) == 0, (functions, got, expected)


def test_wronskian_single_function(engine: Engine) -> None:
    native = engine.call(
        {"mode": "wronskian", "functions": ["x^2"], "x": "x", "lang": "en"}
    )
    assert native["ok"]
    assert native["independent"] is True
    assert parse(native["wronskian"]["plain"]) == X ** 2


def test_wronskian_errors_are_coded(engine: Engine) -> None:
    empty = engine.call({"mode": "wronskian", "functions": [], "x": "x", "lang": "en"})
    assert not empty["ok"]
    assert empty["error_code"] == "ode.f1001"

    too_many = engine.call(
        {
            "mode": "wronskian",
            "functions": ["1", "x", "x^2", "x^3", "x^4", "x^5", "x^6"],
            "x": "x",
            "lang": "en",
        }
    )
    assert not too_many["ok"]
    assert too_many["error_code"] == "ode.f1002"
