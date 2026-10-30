import pytest
import sympy as sp

from infinit_engine import (
    envelope,
    parse_expression,
    solve_ode,
    verify_candidate,
    verify_parametric,
    wronskian,
)


CASES = [
    "y' = y",
    "y' + y/x = x^2",
    "y' = y^2",
    "y' = 1 + y^2",
    "y' = x*y + y^2",
    "y' = (x + y)^2",
    "y'' - 3y' + 2y = 0",
    "y'' + y = 0",
    "y'' = 6x",
    "(2x+y) + (x+2y)*y' = 0",
    "(x+y)dx + (x+2y)dy = 0",
    "dy/dx = x + y",
]


@pytest.mark.parametrize("text", CASES)
def test_oracle_solves_and_verifies(text):
    result = solve_ode(text)
    assert result.ok, result.error
    assert result.solution
    assert result.verified, result.hints


def test_wronskian_sin_cos():
    result = wronskian(["sin(x)", "cos(x)"])
    assert result["wronskian"] == "-1"
    assert result["independent"] is True


def test_wronskian_dependent():
    result = wronskian(["x", "2*x"])
    assert result["wronskian"] == "0"
    assert result["independent"] is False


def test_wronskian_accepts_implicit_multiplication_and_e():
    assert wronskian(["2x", "x^2"])["independent"] is True
    assert sp.simplify(sp.sympify(wronskian(["e^x", "e^(2x)"])["wronskian"]) - sp.exp(3 * sp.Symbol("x"))) == 0


def test_envelope_line_family():
    result = envelope("y = c*x + c^2")
    assert result["envelope"]
    assert "x**2/4 + y" in result["envelope"][0]


def test_envelope_concentric_circles_have_none():
    result = envelope("x^2 + y^2 = c^2")
    assert result["envelope"] == []
    assert result["warnings"]


def test_verify_explicit_candidate():
    result = verify_candidate("y' = y", "C*exp(x)")
    assert result["verified"], result["residual"]


def test_verify_implicit_candidate():
    result = verify_candidate("y' = x/y", "y**2 - x**2", implicit=True)
    assert result["verified"], result["residual"]


# ---------------------------------------------------------------- regressions
def test_verify_abs_and_constant_c():
    """Abs(x) and the constant C must not make a correct answer fail."""
    result = verify_candidate("y' = (x+y)/x", "x*(C + ln(abs(x)))")
    assert result["verified"], result["residual"]


def test_verify_rejects_wrong_candidate():
    result = verify_candidate("y' = y", "C*exp(2*x)")
    assert result["ok"] is True
    assert result["verified"] is False


def test_verify_quadrature_with_integral():
    result = verify_candidate("y' = exp(-x^2)", "C + Integral(exp(-x^2), x)")
    assert result["verified"], result["residual"]


def test_verify_implicit_integral_with_compound_variable():
    result = verify_candidate(
        "y' = y/x + tan(y/x)", "ln(abs(sin(y/x))) - ln(abs(x))", implicit=True
    )
    assert result["ok"], result.get("error")
    assert result["verified"], result["residual"]


def test_derivative_notation_dydx():
    result = solve_ode("dy/dx = x + y")
    assert result.ok, result.error
    assert result.verified


def test_differential_form_nonzero_rhs_is_rejected():
    result = solve_ode("(x+y)dx + (x+2y)dy = 5")
    assert result.ok is False
    assert "M(x,y) dx + N(x,y) dy" in result.error


def test_differential_form_rhs_sign():
    result = solve_ode("(x+y)dx = -(x+2y)dy")
    assert result.ok, result.error
    assert result.verified


def test_erf_and_unknown_functions_parse():
    expr = parse_expression("erf(x)*y'")
    assert expr.has(sp.erf) or expr.has(sp.erf(sp.Symbol("x")))
    assert "Integral" in str(parse_expression("Integral(x^2, x)"))


def test_scientific_notation():
    expr = parse_expression("1e-3*y")
    x = sp.Symbol("x", real=True)
    y = sp.Function("y")(x)
    assert expr.has(y)
    assert abs(float(expr.coeff(y)) - 1e-3) < 1e-12


def test_sin_without_parentheses():
    expr = parse_expression("y'sin y'")
    assert expr.has(sp.sin)


def test_parametric_verification():
    # y = 2 x y' + y'^2  ->  x(p) = C/p^2 - 2p/3, y(p) = 2C/p - p^2/3
    result = verify_parametric(
        "y = 2*x*(y') + (y')^2", "C/p^2 - 2*p/3", "2*C/p - p^2/3"
    )
    assert result["ok"], result.get("error")
    assert result["verified"], result["residual"]


def test_orthogonal_trajectories():
    result = envelope("y = c*x", orthogonal=True)
    assert result["ok"]
    assert result["orthogonal"]
    assert result["orthogonal_verified"]


def test_orthogonal_circles_are_lines():
    result = envelope("x^2 + y^2 = c^2", orthogonal=True)
    assert result["ok"]
    assert result["orthogonal"]
    assert result["orthogonal_verified"]
