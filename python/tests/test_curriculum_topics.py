"""Oracle tests for the reserved curriculum topics.

The native engine is called through the same C ABI the app uses.  Every
reserved topic id is exercised with its defaults, and the new batch-4 tools are
checked against SymPy/mpmath values computed independently.
"""

from __future__ import annotations

import ctypes
import json
import math
import pathlib

import mpmath as mp
import pytest
import sympy as sp

ROOT = pathlib.Path(__file__).resolve().parents[2]
DEFAULT_LIB = ROOT / "core" / "build" / "lib" / "libodecore.so"

X = sp.Symbol("x", real=True)
LOCALS = {"x": X, "e": sp.E, "pi": sp.pi}


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


def text_of(value: dict | str) -> str:
    return value["plain"] if isinstance(value, dict) else value


def number_of(value: dict | str) -> float:
    return float(sp.N(sp.sympify(text_of(value).replace("^", "**"), locals=LOCALS)))


def assert_checks(native: dict) -> None:
    assert native["ok"], native.get("error_code")
    assert native["checks"], "a solved topic must report checks"
    for check in native["checks"]:
        assert check["ok"], (check["name_key"], check.get("detail_math"))


RESERVED = [
    # batch 1 — series solutions
    "series_power",
    "series_singular_points",
    "series_frobenius",
    "euler_equations",
    "special_legendre",
    "special_bessel",
    # batch 2 — linear systems
    "system_theory",
    "system_complex_eigen",
    "system_fundamental",
    "system_repeated",
    "system_nonhomogeneous",
    "system_rk4",
    # batch 3 — dynamical systems and BVPs
    "competing_species",
    "liapunov",
    "limit_cycle",
    "chaos_attractor",
    "local_linearization",
    "bvp_two_point",
    "bvp_occurrence",
    "sturm_liouville",
    "bvp_nonhomogeneous",
    # batch 4 — special series and PDE series
    "sturm_liouville_singular",
    "bessel_series",
    "orthogonal_series",
    "fourier_convergence",
    "fourier_even_odd",
    "heat_derivation",
    "wave_derivation",
    "heat_other_bc",
    "transcendental",
    "integration_techniques",
]


SINGULAR_CLASSIFICATIONS = {
    "ordinary_point",
    "regular_singular_point",
    "irregular_singular_point",
}


def test_every_reserved_topic_is_wired(engine: Engine) -> None:
    for name in RESERVED:
        native = topic(engine, name)
        if name == "series_singular_points":
            # This handler reports the classification as its type id; the
            # steps still describe the singular-point test.
            assert native["type_id"] in SINGULAR_CLASSIFICATIONS, name
        else:
            assert native["type_id"] == name, name
        if native["ok"]:
            assert_checks(native)
        else:
            # An honest refusal is allowed, but the default must not hit one.
            raise AssertionError(
                f"{name} failed with {native['error_code']}: "
                f"{native.get('error_detail')}"
            )


def test_fourier_convergence_of_cosine(engine: Engine) -> None:
    native = topic(
        engine,
        "fourier_convergence",
        f="cos(x)",
        L=math.pi,
        N=6,
        x0=0.0,
    )
    assert_checks(native)
    assert number_of(native["value"]) == pytest.approx(1.0, abs=1e-9)
    native_half = topic(
        engine, "fourier_convergence", f="cos(x)", L=math.pi, N=6, x0=math.pi / 2
    )
    assert number_of(native_half["value"]) == pytest.approx(0.0, abs=1e-9)


def test_fourier_even_odd_identity(engine: Engine) -> None:
    native = topic(engine, "fourier_even_odd", f="x^2", N=10, x0=0.5)
    assert_checks(native)
    assert number_of(native["value"]) == pytest.approx(0.25, abs=0.02)


def test_heat_derivation_first_mode(engine: Engine) -> None:
    native = topic(engine, "heat_derivation", N=10)
    assert_checks(native)
    expected = math.exp(-0.1 * math.pi**2)
    assert number_of(native["value"]) == pytest.approx(expected, abs=1e-5)
    assert native["numeric_only"] is True


def test_wave_derivation_velocity_mode(engine: Engine) -> None:
    native = topic(engine, "wave_derivation", f="0", g="sin(pi*x/L)", N=20)
    assert_checks(native)
    assert number_of(native["value"]) == pytest.approx(1.0, abs=1e-6)


def test_robin_eigenvalue(engine: Engine) -> None:
    native = topic(
        engine, "heat_other_bc", bc="robin", f="cos(pi*x/L)", h=1, N=10
    )
    assert_checks(native)
    mu = mp.findroot(lambda m: m * mp.tan(m * mp.pi) - 1.0, 0.38)
    lam = float(mu**2)
    assert lam == pytest.approx(0.147033, rel=1e-4)
    assert "0.147" in "\n".join(native["functions"])


def test_bessel_series_reproduces_the_data(engine: Engine) -> None:
    native = topic(
        engine, "bessel_series", f="1 - x^2", nu=0, L=1, N=16, x0=0.5
    )
    assert_checks(native)
    assert number_of(native["value"]) == pytest.approx(0.75, abs=5e-3)


def test_legendre_series_reproduces_x(engine: Engine) -> None:
    native = topic(
        engine, "orthogonal_series", family="legendre", f="x", a=-1, b=1, N=4, x0=0.5
    )
    assert_checks(native)
    assert number_of(native["value"]) == pytest.approx(0.5, abs=1e-9)


def test_singular_sl_bessel_eigenvalue(engine: Engine) -> None:
    native = topic(
        engine, "sturm_liouville_singular", family="bessel", nu=0, L=1, K=4
    )
    assert_checks(native)
    expected = float(mp.besseljzero(0, 1) ** 2)
    assert number_of(native["value"]) == pytest.approx(expected, rel=1e-9)


def test_transcendental_matches_sympy(engine: Engine) -> None:
    native = topic(engine, "transcendental")
    assert_checks(native)
    f = sp.exp(X) * sp.sin(X)
    exact = sp.diff(f, X)
    assert sp.simplify(
        sp.sympify(text_of(native["derivative"]).replace("^", "**"), locals=LOCALS)
        - exact
    ) == 0
    assert native["closed_form"] is True


def test_integration_technique_values(engine: Engine) -> None:
    default = topic(engine, "integration_techniques", a=0, b=1)
    assert_checks(default)
    assert number_of(default["value"]) == pytest.approx(1.0, abs=1e-9)

    substitution = topic(
        engine, "integration_techniques", technique="substitution", a=0, b=1
    )
    assert number_of(substitution["value"]) == pytest.approx(math.sin(1.0), abs=1e-9)

    trig = topic(
        engine, "integration_techniques", technique="trig_substitution", a=0, b=1
    )
    assert number_of(trig["value"]) == pytest.approx(math.pi / 4, abs=1e-6)

    non_elementary = topic(
        engine, "integration_techniques", f="sin(x^2)", a=0, b=1
    )
    assert_checks(non_elementary)
    assert non_elementary["numeric_only"] is True
    assert number_of(non_elementary["value"]) == pytest.approx(
        0.3102683017, abs=1e-8
    )
