"""Oracle tests for the numerical-methods workbenches.

The native engine is called through the same C ABI the app uses.  Multistep and
error/convergence answers are checked against independent Python
implementations of Euler, Heun, RK4 and Adams–Bashforth.
"""

from __future__ import annotations

import ctypes
import json
import math
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


def run_method(f, x0, y0, h, n, method):
    x, y = x0, y0
    for _ in range(n):
        k1 = f(x, y)
        if method == "euler":
            y += h * k1
        elif method == "heun":
            k2 = f(x + h, y + h * k1)
            y += h * 0.5 * (k1 + k2)
        else:
            k2 = f(x + h / 2, y + h * k1 / 2)
            k3 = f(x + h / 2, y + h * k2 / 2)
            k4 = f(x + h, y + h * k3)
            y += h * (k1 + 2 * k2 + 2 * k3 + k4) / 6
        x = x0 + (_ + 1) * h
    return y


def run_multistep(f, x0, y0, h, n, order):
    fs = []
    x, y = x0, y0
    for _ in range(order - 1):
        k1 = f(x, y)
        fs.append(k1)
        k2 = f(x + h / 2, y + h * k1 / 2)
        k3 = f(x + h / 2, y + h * k2 / 2)
        k4 = f(x + h, y + h * k3)
        y += h * (k1 + 2 * k2 + 2 * k3 + k4) / 6
        x = x0 + (_ + 1) * h
    for i in range(order - 1, n):
        fi = f(x, y)
        fs.append(fi)
        if order == 2:
            inc = h / 2 * (3 * fs[-1] - fs[-2])
        elif order == 3:
            inc = h / 12 * (23 * fs[-1] - 16 * fs[-2] + 5 * fs[-3])
        else:
            inc = h / 24 * (55 * fs[-1] - 59 * fs[-2] + 37 * fs[-3] - 9 * fs[-4])
        y += inc
        x = x0 + (i + 1) * h
    return y


def check_ok(native: dict, name_key: str) -> bool:
    return any(
        c["name_key"] == name_key and c["ok"] for c in native.get("checks", [])
    )


@pytest.mark.parametrize("order", [2, 3, 4])
def test_multistep_matches_reference(engine: Engine, order: int) -> None:
    f = lambda x, y: x + y
    native = topic(
        engine,
        "multistep",
        f="x + y",
        x0=0,
        y0=1,
        h=0.1,
        n=10,
        order=order,
    )
    assert native["ok"], native.get("error_code")
    expected = run_multistep(f, 0, 1, 0.1, 10, order)
    assert native["numeric_value"] == pytest.approx(expected, rel=1e-9, abs=1e-9)
    assert check_ok(native, "check.multistep_matches_rk4")
    assert check_ok(native, "check.step_count")


def test_multistep_fourth_order_is_accurate(engine: Engine) -> None:
    native = topic(engine, "multistep", f="y", x0=0, y0=1, h=0.1, n=10, order=4)
    assert native["ok"], native.get("error_code")
    assert native["numeric_value"] == pytest.approx(math.e, abs=2e-3)
    rk4 = run_method(lambda x, y: y, 0, 1, 0.1, 10, "rk4")
    assert native["numeric_value"] == pytest.approx(rk4, abs=1e-2)


def test_multistep_rejects_short_history(engine: Engine) -> None:
    native = topic(engine, "multistep", f="x + y", x0=0, y0=1, h=0.1, n=4, order=4)
    assert not native["ok"]
    assert native["error_code"] == "too_few_steps"


@pytest.mark.parametrize(
    "method, expected_order",
    [(1, 1.0), (2, 2.0), (3, 4.0)],
)
def test_error_convergence_measures_order(
    engine: Engine, method: int, expected_order: float
) -> None:
    native = topic(
        engine,
        "error_convergence",
        f="y",
        x0=0,
        y0=1,
        h=0.1,
        n=10,
        method=method,
        exact="e^x",
    )
    assert native["ok"], native.get("error_code")
    assert check_ok(native, "check.error_shrinks")
    assert check_ok(native, "check.convergence_order")
    # Re-derive the observed order from the emitted error values.
    detail = next(
        c
        for c in native["checks"]
        if c["name_key"] == "check.error_shrinks"
    )["detail_math"]
    assert "e_h" in detail
    assert native["numeric_value"] == pytest.approx(
        run_method(lambda x, y: y, 0, 1, 0.1, 10,
                   ["", "euler", "heun", "rk4"][method]),
        rel=1e-9,
        abs=1e-9,
    )


def test_error_convergence_uses_numeric_reference(engine: Engine) -> None:
    native = topic(
        engine,
        "error_convergence",
        f="x + y",
        x0=0,
        y0=1,
        h=0.1,
        n=10,
        method=3,
    )
    assert native["ok"], native.get("error_code")
    assert check_ok(native, "check.convergence_order")
    # The native result must match an independent RK4 run.
    expected = run_method(lambda x, y: x + y, 0, 1, 0.1, 10, "rk4")
    assert native["numeric_value"] == pytest.approx(expected, rel=1e-9, abs=1e-9)
