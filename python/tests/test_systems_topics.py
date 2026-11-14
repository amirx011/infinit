"""Oracle tests for the systems and stability workbench.

The native engine is called through the same C ABI the app uses.  Phase
portraits are re-classified from SymPy eigenvalues, critical points are solved
independently, and the predator-prey trajectory is reproduced with a separate
RK4 implementation.
"""

from __future__ import annotations

import ctypes
import json
import math
import pathlib
import random
import re

import pytest
import sympy

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


def topic(engine: Engine, name: str, **params) -> dict:
    request = {"mode": "topic", "topic": name, "lang": "en"}
    request.update(params)
    return engine.call(request)


def _sympy(text: str) -> sympy.Expr:
    text = text.replace("−", "-").replace("·", "*").replace("^", "**")
    text = text.replace("i", "I")
    return sympy.sympify(text)


def _close(actual, expected, tol: float = 1e-4) -> bool:
    difference = abs(complex(sympy.N(actual - expected)))
    return difference <= tol * (1.0 + abs(complex(sympy.N(expected))))


def _step(response: dict, title_key: str) -> dict:
    return next(s for s in response["steps"] if s["title_key"] == title_key)


def _steps(response: dict, title_key: str) -> list[dict]:
    return [s for s in response["steps"] if s["title_key"] == title_key]


def _classification(a, b, c, d) -> str:
    """Recompute the portrait type with SymPy eigenvalues."""
    trace = a + d
    determinant = a * d - b * c
    discriminant = trace * trace - 4 * determinant
    scale = 1 + abs(trace) + abs(determinant)
    tolerance = sympy.Float("1e-9") * scale
    if determinant < -tolerance:
        return "saddle"
    if abs(determinant) <= tolerance:
        return "degenerate"
    if discriminant > tolerance:
        return "stable_node" if trace < 0 else "unstable_node"
    if discriminant < -tolerance:
        if abs(trace) <= tolerance:
            return "center"
        return "stable_spiral" if trace < 0 else "unstable_spiral"
    return "stable_degenerate_node" if trace < 0 else "unstable_degenerate_node"


def _square_root(x, y) -> bool:
    return _close(x, y, tol=1e-6)


# ------------------------------------------------------------------ phase portraits


def _exercise_portrait(engine: Engine, a, b, c, d) -> dict:
    response = topic(engine, "phase_portrait", a=a, b=b, c=c, d=d)
    assert response["ok"], response.get("error_code")
    assert response["checks"] and all(check["ok"] for check in response["checks"])

    matrix = sympy.Matrix([[a, b], [c, d]])
    trace = matrix.trace()
    determinant = matrix.det()
    text = _step(
        response, "step.title.compute_trace_determinant_and_discriminant"
    )["math"]["pretty"]
    match = re.fullmatch(r"tr = (.+?),  det = (.+?),  Δ = (.+)", text)
    assert match, text
    assert _square_root(_sympy(match.group(1)), trace)
    assert _square_root(_sympy(match.group(2)), determinant)
    assert _square_root(_sympy(match.group(3)), trace * trace - 4 * determinant)

    codes = {
        "saddle": "saddle",
        "stable_node": "stable_node",
        "unstable_node": "unstable_node",
        "stable_degenerate_node": "stable_degenerate_node",
        "unstable_degenerate_node": "unstable_degenerate_node",
        "center": "center",
        "stable_spiral": "stable_spiral",
        "unstable_spiral": "unstable_spiral",
    }
    expected = _classification(a, b, c, d)
    assert response["type_id"] == expected, (response["type_id"], expected)
    classify = _step(response, "step.title.classify_the_equilibrium")
    if expected == "degenerate":
        assert classify["note_key"] == "step.note.the_linearisation_is_not_hyperbolic"
        assert response["solution"]["pretty"] == "degenerate (non-hyperbolic)"
        return response
    assert classify["note_key"] == f"step.note.origin_{expected}"
    assert response["solution"]["pretty"] == _portrait_name(codes[expected])

    nullclines = _step(response, "step.title.draw_the_nullclines")["math"]["pretty"]
    assert nullclines.count("= 0") == 2
    return response


def _portrait_name(code: str) -> str:
    names = {
        "saddle": "saddle",
        "stable_node": "stable node",
        "unstable_node": "unstable node",
        "stable_degenerate_node": "stable degenerate node",
        "unstable_degenerate_node": "unstable degenerate node",
        "center": "centre",
        "stable_spiral": "stable spiral",
        "unstable_spiral": "unstable spiral",
    }
    return names[code]


@pytest.mark.parametrize(
    "a,b,c,d,expected",
    [
        (-1, 0, 0, -2, "stable_node"),
        (1, 0, 0, -1, "saddle"),
        (0, 1, -1, 0, "center"),
        (-1, 1, -1, -1, "stable_spiral"),
        (1, -1, 1, 1, "unstable_spiral"),
        (2, 0, 0, 2, "unstable_degenerate_node"),
        (-2, 0, 0, -2, "stable_degenerate_node"),
        (1, 0, 0, 2, "unstable_node"),
    ],
)
def test_portrait_classification(engine: Engine, a, b, c, d, expected) -> None:
    response = _exercise_portrait(engine, a, b, c, d)
    assert response["type_id"] == expected


def test_portrait_rejects_degenerate_system(engine: Engine) -> None:
    response = topic(engine, "phase_portrait", a=0, b=0, c=0, d=0)
    assert not response["ok"] and response["error_code"] == "degenerate_system"


def test_portrait_rejects_out_of_range_coefficients(engine: Engine) -> None:
    response = topic(engine, "phase_portrait", a=1e300, b=0, c=0, d=1)
    assert not response["ok"] and response["error_code"] == "coefficient_not_evaluable"


def test_portrait_random_sweep_matches_sympy(engine: Engine) -> None:
    rng = random.Random(20261111)
    for _ in range(40):
        a, b, c, d = (rng.choice([-3, -2, -1, 0, 1, 2, 3]) for _ in range(4))
        if a == 0 and b == 0 and c == 0 and d == 0:
            continue
        _exercise_portrait(engine, a, b, c, d)


# ------------------------------------------------------------------ critical points


def _parse_points(response: dict) -> list[tuple[float, float]]:
    text = _step(response, "step.title.find_the_critical_points")["math"]["pretty"]
    points = []
    for match in re.finditer(r"P\d+ = \(([^,]+), ([^)]+)\)", text):
        points.append(
            (
                float(sympy.N(_sympy(match.group(1)))),
                float(sympy.N(_sympy(match.group(2)))),
            )
        )
    return points


def _note_code(note_key: str) -> str:
    mapping = {
        "step.note.a_critical_point_is_a_saddle": "saddle",
        "step.note.a_critical_point_is_a_stable_node": "stable_node",
        "step.note.a_critical_point_is_an_unstable_node": "unstable_node",
        "step.note.a_critical_point_is_a_stable_degenerate_node": "stable_degenerate_node",
        "step.note.a_critical_point_is_an_unstable_degenerate_node": "unstable_degenerate_node",
        "step.note.a_critical_point_is_a_centre": "center",
        "step.note.a_critical_point_is_a_stable_spiral": "stable_spiral",
        "step.note.a_critical_point_is_an_unstable_spiral": "unstable_spiral",
    }
    return mapping.get(note_key, "degenerate")


def test_critical_points_default_system(engine: Engine) -> None:
    x, y = sympy.symbols("x y")
    f = x - x**2 - x * y
    g = y * x - sympy.Rational(1, 2) * y
    expected = sympy.solve([f, g], [x, y], dict=True)
    assert len(expected) == 3

    response = topic(engine, "critical_points")
    assert response["ok"], response.get("error_code")
    assert all(check["ok"] for check in response["checks"])
    found = _parse_points(response)
    assert len(found) == 3

    for point in found:
        assert any(
            _close(point[0], solution[x], 1e-6)
            and _close(point[1], solution[y], 1e-6)
            for solution in expected
        ), (point, expected)

    # Classify each found point from the symbolic Jacobian.
    jacobian = sympy.Matrix([[sympy.diff(f, x), sympy.diff(f, y)],
                             [sympy.diff(g, x), sympy.diff(g, y)]])
    for step, point in zip(_steps(response, "step.title.critical_point_at"), found):
        jacobian_at = jacobian.subs({x: sympy.Float(point[0]), y: sympy.Float(point[1])})
        a, b = jacobian_at[0, 0], jacobian_at[0, 1]
        c, d = jacobian_at[1, 0], jacobian_at[1, 1]
        assert _note_code(step["note_key"]) == _classification(a, b, c, d), step
        assert step["params"]["x"] is not None and step["params"]["y"] is not None


def test_critical_points_pendulum(engine: Engine) -> None:
    x, y = sympy.symbols("x y")
    response = topic(
        engine, "critical_points", f="y", g="-sin(x)", xmin=-4, xmax=4, ymin=-4, ymax=4
    )
    assert response["ok"], response.get("error_code")
    assert all(check["ok"] for check in response["checks"])
    found = sorted(_parse_points(response))
    # sin(x) vanishes at 0 and ±π inside the box.
    expected = sorted((root, 0.0) for root in (-math.pi, 0.0, math.pi))
    assert len(found) == len(expected) == 3
    for actual, wanted in zip(found, expected):
        assert _close(actual[0], wanted[0], 1e-6)
        assert _close(actual[1], wanted[1], 1e-6)

    notes = [
        (point[0], _note_code(step["note_key"]))
        for point, step in zip(found, _steps(response, "step.title.critical_point_at"))
    ]

    def code_near(target: float) -> str:
        return min(notes, key=lambda item: abs(item[0] - target))[1]

    assert code_near(0.0) == "center"
    assert code_near(-math.pi) == "saddle"
    assert code_near(math.pi) == "saddle"


def test_critical_points_errors(engine: Engine) -> None:
    constant = topic(engine, "critical_points", f="1", g="1")
    assert not constant["ok"] and constant["error_code"] == "no_critical_points"
    bounds = topic(engine, "critical_points", f="x", g="y", xmin=2, xmax=-2)
    assert not bounds["ok"] and bounds["error_code"] == "bad_bounds"
    unknown = topic(engine, "critical_points", f="foo(x)", g="y")
    assert not unknown["ok"] and unknown["error_code"] == "unknown_function"


# ------------------------------------------------------------------ predator-prey


def _rk4(a, b, c, d, x0, y0, t_end, steps=4000) -> tuple[float, float]:
    def rates(x, y):
        return a * x - b * x * y, -c * y + d * x * y

    x, y, h = x0, y0, t_end / steps
    for _ in range(steps):
        k1x, k1y = rates(x, y)
        k2x, k2y = rates(x + h * k1x / 2, y + h * k1y / 2)
        k3x, k3y = rates(x + h * k2x / 2, y + h * k2y / 2)
        k4x, k4y = rates(x + h * k3x, y + h * k3y)
        x += h * (k1x + 2 * k2x + 2 * k3x + k4x) / 6
        y += h * (k1y + 2 * k2y + 2 * k3y + k4y) / 6
    return x, y


def _invariant(a, b, c, d, x, y) -> float:
    return d * x - c * math.log(x) + b * y - a * math.log(y)


def test_predator_prey_default_matches_independent_rk4(engine: Engine) -> None:
    response = topic(engine, "predator_prey")
    assert response["ok"], response.get("error_code")
    assert all(check["ok"] for check in response["checks"])
    text = response["solution"]["pretty"]
    match = re.fullmatch(r"\(x, y\)\((.+?)\) = \((.+?), (.+?)\)", text)
    assert match, text

    a, b, c, d = 1.1, 0.4, 0.4, 0.1
    x0, y0, t_end = 10.0, 5.0, 10.0
    assert float(sympy.N(_sympy(match.group(1)))) == pytest.approx(t_end)
    expected_x, expected_y = _rk4(a, b, c, d, x0, y0, t_end)
    assert float(match.group(2)) == pytest.approx(expected_x, rel=1e-4)
    assert float(match.group(3)) == pytest.approx(expected_y, rel=1e-4)

    assert _step(response, "step.title.find_the_equilibria")["math"]["pretty"] == (
        "E0 = (0, 0),  E* = (4, 2.75)"
    )
    assert _step(response, "step.title.write_the_lotka_volterra_system")["math"][
        "pretty"
    ].count("0.4") >= 2

    start = _invariant(a, b, c, d, x0, y0)
    end = _invariant(a, b, c, d, expected_x, expected_y)
    assert abs(end - start) / (1 + abs(start)) < 1e-3


def test_predator_prey_other_parameters(engine: Engine) -> None:
    params = {"a": 2.0, "b": 1.0, "c": 0.5, "d": 0.25, "x0": 4.0, "y0": 2.0, "t": 6.0}
    response = topic(engine, "predator_prey", **params)
    assert response["ok"], response.get("error_code")
    assert all(check["ok"] for check in response["checks"])
    match = re.fullmatch(
        r"\(x, y\)\((.+?)\) = \((.+?), (.+?)\)", response["solution"]["pretty"]
    )
    assert match
    expected_x, expected_y = _rk4(
        params["a"], params["b"], params["c"], params["d"],
        params["x0"], params["y0"], params["t"],
    )
    assert float(match.group(2)) == pytest.approx(expected_x, rel=1e-4)
    assert float(match.group(3)) == pytest.approx(expected_y, rel=1e-4)
    equilibria = _step(response, "step.title.find_the_equilibria")["math"]["pretty"]
    assert equilibria == "E0 = (0, 0),  E* = (2, 2)"


@pytest.mark.parametrize(
    "params",
    [
        {"t": 10000},
        {"a": 10.0, "x0": 1.0, "y0": 1.0, "t": 100.0},
        {"a": 0.01, "b": 0.001, "c": 0.001, "d": 0.0001, "x0": 1000.0, "y0": 500.0, "t": 1000.0},
    ],
)
def test_predator_prey_long_and_fast_runs(engine: Engine, params: dict) -> None:
    response = topic(engine, "predator_prey", **params)
    assert response["ok"], response.get("error_code")
    assert all(check["ok"] for check in response["checks"])
    match = re.fullmatch(
        r"\(x, y\)\((.+?)\) = \((.+?), (.+?)\)", response["solution"]["pretty"]
    )
    assert match
    assert float(match.group(2)) > 0 and float(match.group(3)) > 0

    defaults = {"a": 1.1, "b": 0.4, "c": 0.4, "d": 0.1, "x0": 10.0, "y0": 5.0, "t": 10.0}
    values = {**defaults, **params}
    start = _invariant(
        values["a"], values["b"], values["c"], values["d"], values["x0"], values["y0"]
    )
    end = _invariant(
        values["a"], values["b"], values["c"], values["d"],
        float(match.group(2)), float(match.group(3)),
    )
    assert abs(end - start) / (1 + abs(start)) < 1e-3


@pytest.mark.parametrize(
    "params,code",
    [
        ({"a": 0}, "bad_predator_prey_rates"),
        ({"b": -1}, "bad_predator_prey_rates"),
        ({"x0": 0}, "bad_initial_population"),
        ({"y0": -2}, "bad_initial_population"),
        ({"t": 0}, "bad_time_interval"),
        ({"t": -5}, "bad_time_interval"),
    ],
)
def test_predator_prey_errors(engine: Engine, params: dict, code: str) -> None:
    response = topic(engine, "predator_prey", **params)
    assert not response["ok"] and response["error_code"] == code


def test_systems_translation_keys_resolve() -> None:
    catalogue = ROOT / "app" / "assets" / "i18n"
    english = json.loads((catalogue / "en.json").read_text(encoding="utf-8"))
    persian = json.loads((catalogue / "fa.json").read_text(encoding="utf-8"))
    for key in (
        "topic.phase_portrait.title",
        "topic.phase_portrait.blurb",
        "topic.critical_points.title",
        "topic.predator_prey.title",
        "step.title.critical_point_at",
        "step.note.critical_points_found_in_the_box",
        "step.note.a_critical_point_is_a_stable_spiral",
        "check.the_portrait_matches_the_eigenvalues",
        "check.the_conserved_quantity_is_constant",
        "error.degenerate_system",
        "error.no_critical_points",
        "error.bad_predator_prey_rates",
        "error.bad_initial_population",
        "error.bad_time_interval",
    ):
        assert key in english and key in persian
