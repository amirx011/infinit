"""Edge-case tests for the curriculum topic workbenches.

These cases guard degenerate but legal inputs: zero rates, zero forcing, a
tank that empties mid-run and parabolic forms whose characteristic direction
does not line up with a sampling grid.  ``json.loads`` is deliberately strict
so a non-finite number leaking into the response fails the test.
"""

from __future__ import annotations

import ctypes
import json
import pathlib

import pytest

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


def checks_ok(response: dict) -> bool:
    return bool(response["checks"]) and all(c["ok"] for c in response["checks"])


def test_cooling_with_zero_rate_keeps_temperature(engine: Engine) -> None:
    r = topic(engine, "cooling", k=0, room=20, t0=90, t=10)
    assert r["ok"] and r["solution"]["plain"] == "90"
    assert len(r["checks"]) == 1 and checks_ok(r)


def test_cooling_with_positive_rate_approaches_room(engine: Engine) -> None:
    r = topic(engine, "cooling", k=0.2, room=20, t0=90, t=10)
    assert r["ok"] and checks_ok(r)
    assert any(c["name_key"] == "check.long_term_temperature" for c in r["checks"])


def test_logistic_with_zero_rate_stays_at_p0(engine: Engine) -> None:
    r = topic(engine, "logistic", r=0, K=1000, p0=50, t=10)
    assert r["ok"] and r["solution"]["plain"] == "50"
    assert len(r["checks"]) == 1 and checks_ok(r)


def test_mixing_after_the_tank_empties_is_an_error(engine: Engine) -> None:
    r = topic(engine, "mixing", V0=100, q0=10, rin=2, rout=5, cin=0.2, t=100)
    assert not r["ok"] and r["error_code"] == "tank_empties"


def test_mixing_before_the_tank_empties_has_finite_values(engine: Engine) -> None:
    r = topic(engine, "mixing", V0=100, q0=10, rin=2, rout=5, cin=0.2, t=20)
    assert r["ok"] and checks_ok(r)
    assert r["numeric_value"] == r["numeric_value"]  # not NaN


def test_overflowing_numeric_value_is_null(engine: Engine) -> None:
    # exp(1000) overflows; the response must stay valid JSON and mark the
    # numeric value as absent instead of emitting a bare `inf` token.
    r = topic(engine, "cooling", k=1000, room=0, t0=1, t=-1)
    assert r["ok"]
    assert r["has_numeric"] is False
    assert r["numeric_value"] is None


@pytest.mark.parametrize(
    "m,c,k,f0,w",
    [
        (2, 0, 0, 1, 0),   # constant forcing, quadratic particular
        (1, 1, 0, 1, 0),   # damped constant forcing, linear particular
        (1, 0, 4, 0, 2),   # unforced at the natural frequency: no resonance
        (1, 0, 4, 1, 2),   # genuine resonance
    ],
)
def test_oscillator_degenerate_forcing(
    engine: Engine, m: float, c: float, k: float, f0: float, w: float
) -> None:
    r = topic(
        engine, "spring_mass", m=m, c=c, k=k, f0=f0, w=w, y0=1, v0=0
    )
    assert r["ok"] and checks_ok(r)
    titles = [s.get("title_key") for s in r["steps"]]
    assert ("step.title.resonance" in titles) == (f0 != 0 and w != 0)


@pytest.mark.parametrize(
    "A,B,C",
    [
        (1, 1, 0.25),
        (1, 0.5, 0.0625),
        (1, 10, 25),
        (431.41473727805385, -2.375915246235751, 0.0032711986688908658),
        (1, 0, 1),
        (1, 0, -1),
        (0, 1, 1),
        (0, 0, 3),
    ],
)
def test_classification_checks_survive_rotated_forms(
    engine: Engine, A: float, B: float, C: float
) -> None:
    r = topic(engine, "pde_classify", A=A, B=B, C=C)
    assert r["ok"] and checks_ok(r)


def test_linear_system_solution_satisfies_the_ode(engine: Engine) -> None:
    for a, b, c, d in [(0, 1, -1, 0), (1, 2, 3, 4), (2, 0, 0, 2), (1, 1, 0, 1)]:
        r = topic(engine, "linear_system", a=a, b=b, c=c, d=d)
        assert r["ok"] and checks_ok(r)


@pytest.mark.parametrize(
    "name, params, key",
    [
        ("laplace", {"f": "sin(2*t)"}, "check.domain"),
        ("laplace", {"f": "t*exp(2*t)"}, "check.domain"),
        ("inverse_laplace", {"F": "3/(s-1)"}, "check.table_check"),
        ("inverse_laplace", {"F": "5/(s^2+4)"}, "check.table_check"),
    ],
)
def test_transform_checks_compare_numerically(
    engine: Engine, name: str, params: dict, key: str
) -> None:
    r = topic(engine, name, **params)
    assert r["ok"] and checks_ok(r)
    check = next(c for c in r["checks"] if c["name_key"] == key)
    assert "integral" in check["detail_math"]
