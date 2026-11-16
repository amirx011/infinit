"""Randomised SymPy oracle for the chapter 6 Laplace workbenches.

Each case is checked by substituting the engine's answer into the original
differential equation, by evaluating the initial conditions, and (for impulses)
by verifying the jump in the derivative.  The engine's own checks are not used.
"""

from __future__ import annotations

import ctypes
import json
import pathlib
import random

import pytest
import sympy as sp
from sympy.parsing.sympy_parser import (
    convert_xor,
    parse_expr,
    standard_transformations,
)

ROOT = pathlib.Path(__file__).resolve().parents[2]
DEFAULT_LIB = ROOT / "core" / "build" / "lib" / "libodecore.so"

t, s = sp.symbols("t s")
TRANSFORMS = standard_transformations + (convert_xor,)
LOCALS = {"u": sp.Heaviside, "delta": sp.DiracDelta, "e": sp.E}


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


def topic(engine: Engine, name: str, **params) -> dict:
    request = {"mode": "topic", "topic": name, "lang": "en"}
    request.update(params)
    return engine.call(request)


def parse(plain: str) -> sp.Expr:
    return parse_expr(plain, local_dict=LOCALS, transformations=TRANSFORMS)


def value(expr: sp.Expr, point: float | None = None, sval: float | None = None) -> float:
    sub = {}
    if point is not None:
        sub[t] = sp.Rational(point).limit_denominator(10**9)
    if sval is not None:
        sub[s] = sp.Rational(sval).limit_denominator(10**9)
    try:
        got = complex(sp.N(expr.subs(sub), 30))
    except Exception:
        return float("nan")
    return got.real if abs(got.imag) < 1e-9 else float("nan")


def residual_case(
    result: dict,
    forcing: sp.Expr,
    coeffs: tuple[float, float, float],
    ic: tuple[float, float],
    probes: list[float],
    tolerance: float = 2e-6,
    jump: tuple[float, float] | None = None,
) -> None:
    assert result.get("ok"), f"engine rejected the case: {result.get('error_code')}"
    y = parse(result["solution"]["plain"])
    a, b, c = coeffs
    y0, v0 = ic
    yp = sp.diff(y, t)
    ypp = sp.diff(yp, t)
    residual = sp.expand(a * ypp + b * yp + c * y - forcing)
    for point in probes:
        got = value(residual, point=point)
        assert abs(got) <= tolerance, f"residual {got!r} at t={point} for {result['solution']['plain']}"
    assert abs(value(y, point=0.0) - y0) <= tolerance, "y(0) does not match the initial condition"
    assert abs(value(yp, point=0.0) - v0) <= tolerance, "y'(0) does not match the initial condition"
    if jump is not None:
        at, expected = jump
        eps = 1e-6
        delta = value(yp, point=at + eps) - value(yp, point=at - eps)
        assert abs(delta - expected) <= 2e-4, f"jump {delta!r} at t={at}, expected {expected}"


FORCINGS = ["1", "t", "t^2", "exp(t)", "exp(2*t)", "sin(t)", "cos(t)",
            "exp(-t)*sin(2*t)", "3*t + 1", "exp(t + 1)", "exp(2*t + 1)", "pi", "exp(2)"]


def ivp_cases() -> list[dict]:
    rng = random.Random(20261112)
    cases = []
    for _ in range(70):
        b = rng.choice([-3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0])
        c = rng.choice([0.0, 1.0, 2.0, 4.0, 5.0, 0.25, 6.25])
        if abs(b * b - 4 * c) < 1e-12:
            c += 0.5
        cases.append({
            "a": 1.0, "b": b, "c": c, "g": rng.choice(FORCINGS),
            "y0": rng.choice([0.0, 1.0, -2.0, 0.5, 3.0]),
            "v0": rng.choice([0.0, 1.0, -1.0, 2.0]),
        })
    return cases


def discontinuous_cases() -> list[dict]:
    rng = random.Random(20261113)
    cases = []
    for _ in range(40):
        cases.append({
            "a": 1, "b": rng.choice([-2.0, -1.0, 0.0, 1.0, 3.0]),
            "c": rng.choice([1.0, 2.0, 4.0, 0.5, 6.25]),
            "f": rng.choice(["1", "t", "t^2", "exp(t)", "exp(t + 1)", "exp(2)", "sin(t)", "cos(t)"]),
            "shift": rng.choice([0.25, 0.7, 1.0, 1.8]),
            "y0": rng.choice([0.0, 1.0, -0.5, 2.0]),
            "v0": rng.choice([0.0, 1.0, -1.0, 0.5]),
        })
    return cases


def impulse_cases() -> list[dict]:
    rng = random.Random(20261114)
    cases = []
    for _ in range(40):
        cases.append({
            "a": 1, "b": rng.choice([-2.0, -1.0, 0.0, 1.0, 2.0]),
            "c": rng.choice([1.0, 2.0, 4.0, 0.25, 6.25]),
            "k": rng.choice([1.0, -1.0, 2.5]),
            "shift": rng.choice([0.5, 1.0, 1.7]),
            "y0": rng.choice([0.0, 1.0, -0.5, 2.0]),
            "v0": rng.choice([0.0, 1.0, -1.0, 0.5]),
        })
    return cases


@pytest.mark.parametrize("case", ivp_cases(), ids=lambda c: f"b{c['b']}-c{c['c']}-{c['g']}")
def test_laplace_ivp_randomised(engine: Engine, case: dict) -> None:
    result = topic(engine, "laplace_ivp", **case)
    residual_case(
        result,
        sp.sympify(case["g"].replace("^", "**")),
        (case["a"], case["b"], case["c"]),
        (case["y0"], case["v0"]),
        [0.137, 0.413, 0.829, 1.271],
    )


@pytest.mark.parametrize("f_text", ["sin(t)", "cos(t)", "exp(-2*t)", "t^2", "t*exp(t)"])
@pytest.mark.parametrize("y0,v0", [(0.0, 1.0), (2.0, -1.0), (0.5, -0.25)])
def test_transform_derivatives_randomised(engine: Engine, f_text: str, y0: float, v0: float) -> None:
    result = topic(engine, "transform_derivatives", f=f_text, y0=y0, v0=v0)
    assert result.get("ok"), result.get("error_code")
    got = parse(result["solution"]["plain"])
    F = sp.laplace_transform(sp.sympify(f_text.replace("^", "**")), t, s, noconds=True)
    want = sp.simplify(s**2 * F - s * y0 - v0)
    for sval in (3.0, 5.0, 11.0):
        assert abs(value(got, sval=sval) - value(want, sval=sval)) <= 2e-6 * (
            1 + abs(value(want, sval=sval))
        ), f"{f_text} at s={sval}"


@pytest.mark.parametrize("f_text", ["1", "t", "t^2", "exp(-t)", "sin(2*t)", "pi", "exp(2)"])
@pytest.mark.parametrize("shift", [0.0, 0.5, 1.5, 2.0])
def test_step_functions_randomised(engine: Engine, f_text: str, shift: float) -> None:
    result = topic(engine, "step_functions", f=f_text, shift=shift)
    assert result.get("ok"), result.get("error_code")
    got = parse(result["solution"]["plain"])
    F = sp.laplace_transform(sp.sympify(f_text.replace("^", "**")), t, s, noconds=True)
    want = sp.exp(-shift * s) * F
    for sval in (3.0, 7.0):
        assert abs(value(got, sval=sval) - value(want, sval=sval)) <= 2e-6 * (
            1 + abs(value(want, sval=sval))
        ), f"{f_text} at s={sval}, a={shift}"


@pytest.mark.parametrize("case", discontinuous_cases(), ids=lambda c: f"b{c['b']}-c{c['c']}-{c['f']}-a{c['shift']}")
def test_discontinuous_forcing_randomised(engine: Engine, case: dict) -> None:
    result = topic(engine, "discontinuous_forcing", **case)
    base = sp.sympify(case["f"].replace("^", "**"))
    forcing = base * (1 - sp.Heaviside(t - case["shift"]))
    probes = [0.11, 0.39, case["shift"] - 0.21, case["shift"] + 0.19, case["shift"] + 0.53]
    probes = [p for p in probes if p > 0 and abs(p - case["shift"]) > 0.05]
    residual_case(
        result, forcing, (case["a"], case["b"], case["c"]),
        (case["y0"], case["v0"]), probes,
    )


@pytest.mark.parametrize("case", impulse_cases(), ids=lambda c: f"b{c['b']}-c{c['c']}-k{c['k']}-a{c['shift']}")
def test_impulse_randomised(engine: Engine, case: dict) -> None:
    result = topic(engine, "impulse", **case)
    probes = [0.09, 0.33, case["shift"] - 0.27, case["shift"] + 0.31]
    probes = [p for p in probes if p > 0 and abs(p - case["shift"]) > 0.05]
    residual_case(
        result, sp.Integer(0), (case["a"], case["b"], case["c"]),
        (case["y0"], case["v0"]), probes,
        jump=(case["shift"], case["k"] / case["a"]),
    )
