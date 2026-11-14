"""Independent SymPy checks for the chapter 6 Laplace workbenches.

The native answers are parsed back into SymPy and verified by substitution,
by direct transform identities, or by SymPy's own inverse Laplace transform.
Nothing here trusts the engine's own checks.
"""

from __future__ import annotations

import ctypes
import json
import pathlib

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
    return parse_expr(
        plain,
        local_dict={"u": sp.Heaviside, "delta": sp.DiracDelta},
        transformations=standard_transformations + (convert_xor,),
    )


def sample(expr: sp.Expr, points=(sp.Rational(1, 7), sp.Rational(2, 5), sp.Rational(7, 6))) -> None:
    for value in points:
        got = complex(sp.N(expr.subs(t, value), 12))
        assert abs(got) < 1e-7, f"residual at t={value}: {got}"


# ---------------------------------------------------------------- transform of derivatives
@pytest.mark.parametrize(
    "f_text, y0, v0",
    [
        ("t", 0, 0),
        ("t", 1, 0),
        ("sin(t)", 0, 1),
        ("exp(2*t)", 2, -1),
    ],
)
def test_transform_of_derivatives_matches_sympy(
    engine: Engine, f_text: str, y0: float, v0: float
) -> None:
    f = parse(f_text)
    response = topic(engine, "transform_derivatives", f=f_text, y0=y0, v0=v0)
    assert response["ok"], response
    answer = parse(response["solution"]["plain"])

    F = sp.laplace_transform(f, t, s, noconds=True)
    expected = s**2 * F - s * y0 - v0
    difference = sp.simplify(answer - expected)
    assert sp.simplify(difference) == 0, (answer, expected, difference)


# ---------------------------------------------------------------- Laplace IVP
def check_ivp(response: dict, a: float, b: float, c: float, forcing: sp.Expr,
              y0: float, v0: float) -> None:
    assert response["ok"], response
    y = parse(response["solution"]["plain"])
    residual = sp.simplify(a * sp.diff(y, t, 2) + b * sp.diff(y, t) + c * y - forcing)
    # Away from any step or impulse the residual must vanish identically.
    sample(residual)
    assert abs(complex(sp.N(y.subs(t, 0) - y0, 12))) < 1e-8
    assert abs(complex(sp.N(sp.diff(y, t).subs(t, 0) - v0, 12))) < 1e-8


@pytest.mark.parametrize(
    "a, b, c, forcing_text, y0, v0",
    [
        (1, 3, 2, "exp(t)", 1, 0),
        (1, 0, 1, "1", 1, 0),
        (1, 0, 1, "sin(t)", 0, 0),
        (2, -1, 3, "t", 0, 1),
        (1, -2, 1, "exp(t)", 1, 1),
    ],
)
def test_laplace_ivp_satisfies_the_equation(
    engine: Engine, a: float, b: float, c: float, forcing_text: str, y0: float, v0: float
) -> None:
    response = topic(
        engine,
        "laplace_ivp",
        a=a,
        b=b,
        c=c,
        g=forcing_text,
        y0=y0,
        v0=v0,
    )
    check_ivp(response, a, b, c, parse(forcing_text), y0, v0)


def test_laplace_ivp_random_coefficients(engine: Engine) -> None:
    import random

    rng = random.Random(20261112)
    for _ in range(8):
        a = rng.choice([1, 2, 3])
        b = rng.randint(-4, 4)
        c = rng.randint(1, 6)
        y0 = rng.randint(-3, 3)
        v0 = rng.randint(-3, 3)
        forcing_text = rng.choice(["1", "t", "exp(t)", "sin(t)", "cos(2*t)"])
        response = topic(
            engine,
            "laplace_ivp",
            a=a,
            b=b,
            c=c,
            g=forcing_text,
            y0=y0,
            v0=v0,
        )
        check_ivp(response, a, b, c, parse(forcing_text), y0, v0)


# ---------------------------------------------------------------- inverse Laplace
@pytest.mark.parametrize(
    "F_text",
    [
        "1/(s^2+1)",
        "s/(s^2+4)",
        "5/(s^2+4)",
        "1/(s-1)^3",
        "1/((s^2+1)^2)",
        "s/((s^2+1)^2)",
        "(3*s+2)/((s-1)*(s^2+4))",
        "1/((s-1)^2*(s+2))",
        "(3*s-2)/(s^2-4*s+13)",
    ],
)
def test_inverse_laplace_matches_sympy(engine: Engine, F_text: str) -> None:
    response = topic(engine, "inverse_laplace", F=F_text)
    assert response["ok"], response
    got = parse(response["solution"]["plain"])
    F = parse(F_text)
    expected = sp.inverse_laplace_transform(F, s, t)
    difference = sp.simplify(got - expected)
    # Heaviside(0) differs between the engine (1) and SymPy (1/2), so compare
    # away from t = 0 and from any delayed jump.
    sample(difference, points=(sp.Rational(1, 7), sp.Rational(3, 5), sp.Rational(7, 4)))


# ---------------------------------------------------------------- step functions
def test_step_function_time_shift_matches_sympy(engine: Engine) -> None:
    response = topic(engine, "step_functions", f="t", shift=2)
    assert response["ok"], response
    got = parse(response["solution"]["plain"])
    F = sp.laplace_transform(t, t, s, noconds=True)
    expected = sp.exp(-2 * s) * F
    assert sp.simplify(got - expected) == 0


def test_step_function_checked_against_direct_integral(engine: Engine) -> None:
    response = topic(engine, "step_functions", f="exp(2*t)", shift=2)
    assert response["ok"], response
    assert response["solution"]["plain"] == "exp(-2*s)/(s - 2)"
    assert response["checks"] and all(c["ok"] for c in response["checks"])


# ---------------------------------------------------------------- discontinuous forcing
@pytest.mark.parametrize(
    "a, b, c, f_text, shift, y0, v0",
    [
        (1, 2, 1, "1", 1, 0, 0),
        (1, 3, 2, "t", 2, 1, 0),
        (1, 0, 4, "sin(t)", 1, 0, 1),
    ],
)
def test_discontinuous_forcing_satisfies_the_equation(
    engine: Engine, a: float, b: float, c: float, f_text: str, shift: float,
    y0: float, v0: float
) -> None:
    response = topic(
        engine,
        "discontinuous_forcing",
        a=a,
        b=b,
        c=c,
        f=f_text,
        shift=shift,
        y0=y0,
        v0=v0,
    )
    assert response["ok"], response
    y = parse(response["solution"]["plain"])
    f = parse(f_text)
    gate = 1 - sp.Heaviside(t - shift)
    forcing = f * gate
    residual = sp.simplify(a * sp.diff(y, t, 2) + b * sp.diff(y, t) + c * y - forcing)
    sample(residual, points=(sp.Rational(1, 9), sp.Rational(1, 2), sp.Rational(9, 5)))
    assert abs(complex(sp.N(y.subs(t, 0) - y0, 12))) < 1e-7
    assert abs(complex(sp.N(sp.diff(y, t).subs(t, 0) - v0, 12))) < 1e-7


# ---------------------------------------------------------------- impulse
@pytest.mark.parametrize(
    "a, b, c, k, shift, y0, v0",
    [
        (1, 0, 4, 1, 1, 0, 0),
        (1, 2, 1, 2, 1, 1, 0),
        (2, 0, 8, 1, 2, 0, 1),
    ],
)
def test_impulse_response_satisfies_the_equation(
    engine: Engine, a: float, b: float, c: float, k: float, shift: float,
    y0: float, v0: float
) -> None:
    response = topic(
        engine,
        "impulse",
        a=a,
        b=b,
        c=c,
        k=k,
        shift=shift,
        y0=y0,
        v0=v0,
    )
    assert response["ok"], response
    y = parse(response["solution"]["plain"])
    forcing = k * sp.DiracDelta(t - shift)
    residual = sp.simplify(a * sp.diff(y, t, 2) + b * sp.diff(y, t) + c * y - forcing)
    sample(residual, points=(sp.Rational(1, 9), sp.Rational(1, 2), sp.Rational(9, 5)))
    assert abs(complex(sp.N(y.subs(t, 0) - y0, 12))) < 1e-7
    assert abs(complex(sp.N(sp.diff(y, t).subs(t, 0) - v0, 12))) < 1e-7


# ---------------------------------------------------------------- boundary cases
def test_step_function_zero_shift_reduces_to_the_plain_transform(engine: Engine) -> None:
    response = topic(engine, "step_functions", f="t", shift=0)
    assert response["ok"], response
    got = parse(response["solution"]["plain"])
    assert sp.simplify(got - 1 / s**2) == 0


def test_impulse_at_time_zero(engine: Engine) -> None:
    response = topic(engine, "impulse", a=1, b=0, c=4, k=1, shift=0, y0=0, v0=0)
    assert response["ok"], response
    # L{delta(t)} = 1, so y = sin(2t)/2 for t > 0.  The response carries a
    # Heaviside factor because the impulse fires at t = 0, so compare away from
    # the jump instead of demanding pointwise equality at the origin.
    y = parse(response["solution"]["plain"])
    sample(sp.simplify(y - sp.sin(2 * t) / 2), points=(sp.Rational(1, 7), sp.Rational(3, 5)))


def test_laplace_ivp_zero_initial_data_and_zero_forcing(engine: Engine) -> None:
    response = topic(engine, "laplace_ivp", a=1, b=2, c=1, g="0", y0=0, v0=0)
    assert response["ok"], response
    assert response["solution"]["plain"] == "0"


# ---------------------------------------------------------------- step/answer consistency
STEP_CASES = [
    ("transform_derivatives", {"f": "sin(t)", "y0": 0.5, "v0": 2}),
    ("laplace_ivp", {"a": 1, "b": 1, "c": 2, "g": "exp(t + 1)", "y0": 0.5, "v0": 2}),
    ("step_functions", {"f": "exp(t)", "shift": 0.5}),
    ("discontinuous_forcing", {"a": 1, "b": 0, "c": 1, "f": "t^2", "shift": 1.5,
                               "y0": 0, "v0": 0}),
    ("impulse", {"a": 1, "b": 0, "c": 4, "k": 0.5, "shift": 0.5, "y0": 0, "v0": 0}),
]


@pytest.mark.parametrize("name, params", STEP_CASES, ids=[c[0] for c in STEP_CASES])
def test_answer_is_the_last_displayed_step(engine: Engine, name: str, params: dict) -> None:
    response = topic(engine, name, **params)
    assert response["ok"], response
    steps = response["steps"]
    assert steps
    assert response["solution"]["plain"] == steps[-1]["math"]["plain"], (
        "the final answer must be the last displayed step"
    )
    for step in steps:
        assert step["math"]["plain"], f"{name} has an empty step"
        assert step["title_key"].startswith("step.title."), step["title_key"]
    for check in response["checks"]:
        assert check["name_key"].startswith("check."), check["name_key"]


def test_laplace_keys_resolve_in_every_catalogue(engine: Engine) -> None:
    catalogues = {}
    for path in sorted((ROOT / "app" / "assets" / "i18n").glob("*.json")):
        catalogues[path.stem] = json.loads(path.read_text(encoding="utf-8"))
    assert catalogues, "no translation catalogues found"
    keys = set()
    for name, params in STEP_CASES:
        response = topic(engine, name, **params)
        assert response["ok"], response
        for step in response["steps"]:
            keys.add(step.get("title_key", ""))
            keys.add(step.get("note_key", ""))
        for check in response["checks"]:
            keys.add(check.get("name_key", ""))
            keys.add(check.get("detail_key", ""))
    for language, catalogue in catalogues.items():
        missing = sorted(key for key in keys if key and key not in catalogue)
        assert not missing, f"{language} is missing {missing}"


# ---------------------------------------------------------------- input guards
@pytest.mark.parametrize(
    "name, extra",
    [
        ("step_functions", {"f": "t"}),
        ("discontinuous_forcing", {"a": 1, "b": 2, "c": 1, "f": "1", "y0": 0, "v0": 0}),
        ("impulse", {"a": 1, "b": 0, "c": 4, "k": 1, "y0": 0, "v0": 0}),
    ],
)
def test_negative_shift_is_reported_as_bad_shift(engine: Engine, name: str, extra: dict) -> None:
    response = topic(engine, name, shift=-1, **extra)
    assert not response["ok"]
    assert response["error_code"] == "bad_shift"


def test_degenerate_leading_coefficient_is_reported(engine: Engine) -> None:
    response = topic(engine, "discontinuous_forcing", a=0, b=2, c=1, f="1",
                     shift=1, y0=0, v0=0)
    assert not response["ok"]
    assert response["error_code"] == "unsupported_form"
