"""Regression coverage for the agentic ODE QA run (300 cases + Phase 2).

The cases here are the ones that needed engine work during that run: IVP
constants, numeric RK4 fallback, y^(n) parsing, special functions, Lagrange
parametrics, the general mu(x^a y^b) integrating factor, and a few Phase 2
fixes.  Each symbolic answer is checked independently through SymPy.
"""

from __future__ import annotations

import ctypes
import json
import pathlib

import pytest

from infinit_engine import verify_candidate, verify_parametric

ROOT = pathlib.Path(__file__).resolve().parents[2]
DEFAULT_LIB = ROOT / "core" / "build" / "lib" / "libodecore.so"


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


def plain(value) -> str:
    if isinstance(value, dict):
        return value.get("plain") or ""
    return value or ""


def solve(engine: Engine, text: str) -> dict:
    return engine.call({"mode": "ode", "input": text, "lang": "en"})


def solution_of(result: dict) -> str:
    return plain(result.get("explicit_y") or result.get("solution"))


# ------------------------------------------------------------- symbolic cases
SYMBOLIC_CASES = [
    # A1/A2/A3 core families
    "y' = (y / x) * (1 + ln(y) - ln(x))",
    "y' = sqrt((x + y) / x)",
    "y' + y*tanh(x) = sech(x)",
    # A4 Riccati / Bernoulli with variable coefficients
    "x^2 * y' = x^2 * y^2 + x*y + 1",
    "y' + y^2 = 2 / x^2",
    "y' = y^2 - (2/x)*y - 1/x^2",
    # A particular solution that is not a monomial: the search must find the
    # shifted reciprocal 1/(x+1) on its own.
    "y' + y^2 - 2*y/(x+1) + 2/(x+1)^2 = 0",
    # A5 general integrating factor
    "(y*(1 + x*y)) + x*(1 - x*y)*y' = 0",
    # A7/A8 higher order and Cauchy-Euler
    "y^(4) - y'' = 0",
    "y^(4) - 16*y = 0",
    "y^(4) + 8*y'' + 16*y = 0",
    "y^(4) - 2*y''' + 2*y'' - 2*y' + y = 0",
    "y^(5) - y''' = 0",
    "y^(4) - y = cos(x) + exp(x)",
    "x^2 * y'' - 3*x*y' + 4*y = 0",
]


@pytest.mark.parametrize("text", SYMBOLIC_CASES)
def test_symbolic_answer_verifies(engine: Engine, text: str) -> None:
    result = solve(engine, text)
    assert result["ok"], result.get("error_code")
    assert result["verified"], result.get("checks")
    body = solution_of(result)
    if not result.get("is_explicit"):
        detail = verify_candidate(text, plain(result["solution"]), implicit=True)
    else:
        detail = verify_candidate(text, body)
    assert detail["verified"], detail.get("residual")


# ---------------------------------------------------------------- special A9
SPECIAL_CASES = {
    "x^2 * y'' + x*y' + (x^2 - 0)*y = 0": "besselj",
    "x^2 * y'' + x*y' + (x^2 - 1)*y = 0": "bessely",
    "y'' - x*y = 0": "airyai",
    "(1 - x^2)*y'' - x*y' + 9*y = 0": "x*(4*x^2 - 3)",
}


@pytest.mark.parametrize("text,fragment", SPECIAL_CASES.items())
def test_special_canonical_functions(engine: Engine, text: str, fragment: str) -> None:
    result = solve(engine, text)
    assert result["ok"], result.get("error_code")
    assert fragment in solution_of(result)


def test_special_bessel_at_origin() -> None:
    lib = Engine(DEFAULT_LIB)
    result = lib.call(
        {"mode": "topic", "topic": "special_bessel", "nu": 0, "x": 0, "lang": "en"}
    )
    assert result["ok"]
    assert plain(result["value"]) == "1"


def test_scaled_oscillator_reduces_to_cos_sin(engine: Engine) -> None:
    """x y'' + ½ y' + (q/x) y = 0 becomes y_tt + y = 0 under t = 2√(q x)."""
    result = solve(engine, "2*x*y'' + y' + y = 0")
    assert result["ok"], result.get("error_code")
    assert result["verified"], result.get("checks")
    body = solution_of(result)
    assert "cos(sqrt((2*x)))" in body
    assert "sin(sqrt((2*x)))" in body


def test_wrong_exact_potential_falls_through(engine: Engine) -> None:
    """An integrating factor whose antiderivative drops a term must not win.

    `y' = sqrt(x+y)` has an exact-looking potential, but differentiating it
    does not return the equation.  The linear substitution is the real answer.
    """
    result = solve(engine, "y' = sqrt(x+y)")
    assert result["ok"], result.get("error_code")
    assert result["verified"], result.get("checks")
    assert result["type_id"] == "linear_sub"


def test_series_with_two_power_factors_is_not_one_p_series() -> None:
    """1/(n(n+1)) behaves like 1/n²; it is not the divergent p-series 1/n."""
    lib = Engine(DEFAULT_LIB)
    result = lib.call(
        {"mode": "series", "input": "1/(n*(n+1))", "n": "n", "start": 1, "lang": "en"}
    )
    assert result["ok"]
    assert result["converges"] is True
    assert result.get("series_test") != "p-series"


# --------------------------------------------------------------- parametric
PARAMETRIC_CASES = [
    "(y')^3 - y*y' = 0",
    "y = (y')^2 + 2*(y')^3",
    "x = y' + sin(y')",
    "(y'')^2 + (y')^2 = 1",
]


@pytest.mark.parametrize("text", PARAMETRIC_CASES)
def test_parametric_solutions(engine: Engine, text: str) -> None:
    result = solve(engine, text)
    assert result["ok"], result.get("error_code")
    assert result["param_x"] and result["param_y"]
    detail = verify_parametric(
        text,
        plain(result["param_x"]),
        plain(result["param_y"]),
        p=result.get("param_name") or "p",
    )
    assert detail["verified"], detail.get("residual")


# ------------------------------------------------------------------- IVPs
IVP_CASES = [
    "y' + 2*y = 4, y(0) = 1",
    "y' = -x / y, y(0) = 3",
    "y'' + 4*y = 0, y(0) = 2, y'(0) = -4",
    "y'' - 3*y' + 2*y = exp(3*x), y(0) = 1, y'(0) = 0",
    "x^2 * y'' - 3*x*y' + 4*y = 0, y(1) = 2, y'(1) = 3",
    "y''' - y' = 0, y(0) = 0, y'(0) = 1, y''(0) = 2",
    # The separable family 1/(-x - C) is singular at the initial point for
    # y(0) = ±1, so the constant must be fitted from the implicit family
    # rather than from a linear coefficient probe.
    "y' = y^2, y(0) = 1",
    "y' = y^2, y(0) = -1",
    "y' = y^2, y(2) = 0",
    "y' = 1 + y^2, y(0) = 1",
]


@pytest.mark.parametrize("text", IVP_CASES)
def test_ivp_constants_are_evaluated(engine: Engine, text: str) -> None:
    result = solve(engine, text)
    assert result["ok"], result.get("error_code")
    assert result["verified"], result.get("checks")
    body = solution_of(result)
    assert "C" not in body, body


def test_cauchy_euler_ivp_matches_closed_form(engine: Engine) -> None:
    result = solve(engine, "x^2 * y'' - 3*x*y' + 4*y = 0, y(1) = 2, y'(1) = 3")
    assert result["ok"]
    assert solution_of(result) == "x^2*(-ln(x) + 2)"


def test_separable_ivp_branches_at_the_pole(engine: Engine) -> None:
    """y' = y² with y(0) = ±1 must not lose the branch at the pole."""
    cases = {
        "y' = y^2, y(0) = 1": "-1/(x - 1)",
        "y' = y^2, y(0) = -1": "-1/(x + 1)",
        "y' = y^2, y(2) = 0": "0",
    }
    for text, want in cases.items():
        result = solve(engine, text)
        assert result["ok"], (text, result.get("error_code"))
        assert result["verified"], (text, result.get("checks"))
        assert solution_of(result) == want


# --------------------------------------------------------- numeric fallback
NUMERIC_CASES = {
    "y' = x^2 + y^2, y(0) = 0": "numeric_first_order",
    "y' = sin(x*y), y(0) = 1": "numeric_first_order",
    "y'' - (1 - y^2)*y' + y = 0, y(0) = 2, y'(0) = 0": "numeric_second_order",
    "y''' + y*y'' = 0, y(0) = 0, y'(0) = 0, y''(0) = 0.332": "numeric_higher_order",
    "y'' + (2/x)*y' + y^5 = 0, y(0.001) = 1, y'(0.001) = 0": "numeric_second_order",
}


@pytest.mark.parametrize("text,type_id", NUMERIC_CASES.items())
def test_numeric_rk4_fallback(engine: Engine, text: str, type_id: str) -> None:
    result = solve(engine, text)
    assert result["ok"], result.get("error_code")
    assert result["numeric_only"] is True
    assert result["type_id"] == type_id
    assert result["numeric_rhs"]
    assert "ode.w2" in result["warnings"]


def test_unsupported_without_ic_stays_symbolic(engine: Engine) -> None:
    """The RK4 fallback is only for actual IVPs, not plain unsupported ODEs."""
    result = solve(engine, "y' = x^2 + y^2")
    assert result["ok"] is False
    assert result.get("numeric_only") is not True


# ------------------------------------------------ honest unsupported shapes
@pytest.mark.parametrize(
    "text",
    [
        "y' = cos(x)*y^2 - sin(x)*y - cos(x)",
        "y' - 2*y*tan(x) = y^2 * tan(x)^2 - 1",
        "x^2 * y'' + x*y' + (x^2 - 4)*y = x^3",
        # Riccati with no elementary particular solution (Bessel-type):
        # the engine must name the obstacle instead of guessing.
        "y' + (1/x)*y^2 - (2 + 1/x)*y + x + 2 = 0",
    ],
)
def test_non_elementary_cases_fail_honestly(engine: Engine, text: str) -> None:
    """SymPy cannot solve these either; the engine must not invent an answer."""
    result = solve(engine, text)
    assert result["ok"] is False
    assert result["error_code"] in {"ode.f7", "ode.f16", "ode.f19"}


def test_riccati_reports_the_missing_particular_solution(engine: Engine) -> None:
    result = solve(engine, "y' + (1/x)*y^2 - (2 + 1/x)*y + x + 2 = 0")
    assert result["ok"] is False
    assert result["error_code"] == "ode.f19"


# ------------------------------------------------------ y^(n) parser detail
def test_derivative_power_notation_verifies() -> None:
    cases = [
        ("y^(4) - y'' = 0", "C + C2*x + C3*exp(x) + C4*exp(-x)"),
        ("y^(4) - 16*y = 0",
         "C*exp(2*x) + C2*exp(-2*x) + C3*cos(2*x) + C4*sin(2*x)"),
        ("y^(5) - y''' = 0", "C + C2*x + C3*x^2 + C4*exp(x) + C5*exp(-x)"),
    ]
    for text, candidate in cases:
        detail = verify_candidate(text, candidate)
        assert detail["verified"], detail.get("residual")


# ------------------------------------------------ Phase 2 regressions
def test_wronskian_dependent_is_zero() -> None:
    engine = Engine(DEFAULT_LIB)
    result = engine.call(
        {"mode": "wronskian", "functions": ["sin(x)^2", "cos(x)^2", "1"]}
    )
    assert plain(result["wronskian"]) == "0"
    assert result["independent"] is False


def test_surface_removable_limit_is_finite() -> None:
    engine = Engine(DEFAULT_LIB)
    result = engine.call(
        {
            "mode": "surface",
            "input": "sin(sqrt(x^2 + y^2))/sqrt(x^2 + y^2)",
            "xmin": -5,
            "xmax": 5,
            "ymin": -5,
            "ymax": 5,
            "nx": 21,
            "ny": 21,
        }
    )
    assert result["ok"]
    assert result["nx"] == 21 and result["ny"] == 21


def test_direction_field_accepts_bare_rhs() -> None:
    engine = Engine(DEFAULT_LIB)
    result = engine.call(
        {
            "mode": "field",
            "input": "y - x^2 + 1",
            "xmin": -4,
            "xmax": 4,
            "ymin": -4,
            "ymax": 4,
            "cols": 20,
            "rows": 20,
        }
    )
    assert result["ok"]
    assert result["rhs"]
