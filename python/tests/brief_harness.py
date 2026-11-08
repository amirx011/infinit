#!/usr/bin/env python3
"""Reference harness for the Infinit Lab brief (T0).

Calls the native engine through the same C ABI the Flutter app uses and checks
each section against an independent SymPy/mpmath oracle.

Exit status is non-zero on an unexpected failure (including an XPASS: a case
marked xfail that now passes must have its marker removed).
"""

from __future__ import annotations

import argparse
import ctypes
import json
import math
import pathlib
import sys

import mpmath as mp
import sympy as sp

ROOT = pathlib.Path(__file__).resolve().parents[2]
DEFAULT_LIB = ROOT / "core" / "build" / "lib" / "libodecore.so"
DEFAULT_CASES = ROOT / "testdata" / "brief_cases.json"

X = sp.Symbol("x", real=True)
Y = sp.Symbol("y", real=True)
S = sp.Symbol("s", real=True)
T = sp.Symbol("t", real=True)
C = sp.Symbol("c", real=True)
XI = sp.Symbol("xi", real=True)
E = sp.Symbol("e")
LOCALS = {"x": X, "y": Y, "s": S, "t": T, "c": C, "xi": XI, "e": E, "pi": sp.pi}


class Engine:
    def __init__(self, path: pathlib.Path) -> None:
        if not path.exists():
            raise SystemExit(
                f"native library not found: {path}\n"
                "build it first with ./scripts/build_core.sh"
            )
        self.lib = ctypes.CDLL(str(path))
        self.lib.ode_solve.argtypes = [ctypes.c_char_p]
        self.lib.ode_solve.restype = ctypes.c_void_p
        self.lib.ode_free.argtypes = [ctypes.c_void_p]

    def call(self, payload: dict) -> dict:
        ptr = self.lib.ode_solve(json.dumps(payload, ensure_ascii=False).encode("utf-8"))
        try:
            raw = ctypes.string_at(ptr)
        finally:
            self.lib.ode_free(ptr)
        # A UnicodeDecodeError here is the P3 bridge failure: invalid UTF-8.
        return json.loads(raw.decode("utf-8"))


def sympy_of(text: str) -> sp.Expr:
    return sp.sympify(text.replace("^", "**"), locals=LOCALS)


def same_expr(native, expected: str) -> bool:
    if not native:
        return False
    try:
        a = sympy_of(native)
        b = sympy_of(expected)
        return sp.simplify(a - b) == 0
    except Exception:
        return False


def number(expr) -> float | None:
    if not expr:
        return None
    try:
        value = sp.N(sympy_of(expr), 30)
        return float(value)
    except Exception:
        return None


def close(a: float | None, b: float | None, tol: float) -> bool:
    if a is None or b is None:
        return False
    return abs(a - b) <= tol * max(1.0, abs(a), abs(b))


def integrate_ode(rhs: str, x0: float, y0: float, h: float, n: int, method: str) -> float:
    f = sp.lambdify((X, Y), sympy_of(rhs), modules=["math"])
    x, y = float(x0), float(y0)
    for step in range(n):
        k1 = f(x, y)
        if method == "heun":
            k2 = f(x + h, y + h * k1)
            y += h * 0.5 * (k1 + k2)
        else:
            k2 = f(x + 0.5 * h, y + 0.5 * h * k1)
            k3 = f(x + 0.5 * h, y + 0.5 * h * k2)
            k4 = f(x + h, y + h * k3)
            y += h / 6.0 * (k1 + 2 * k2 + 2 * k3 + k4)
        x = float(x0) + (step + 1) * h
    return y


def coeff_table(native: dict) -> list[tuple[int, float, float]]:
    for step in native.get("steps") or []:
        note = step.get("note") or ""
        lines = [line for line in note.splitlines() if line.strip()]
        if not lines or not lines[0].startswith("n\ta_n"):
            continue
        rows = []
        for line in lines[1:]:
            parts = line.split("\t")
            if len(parts) < 3:
                continue
            rows.append((int(parts[0]), float(parts[1]), float(parts[2])))
        return rows
    return []


def oracle_min(expr: str, lo: float, hi: float) -> float:
    """Independent interior minimum from mpmath, ignoring unbounded endpoints."""
    f = sp.lambdify(X, sympy_of(expr), modules=["mpmath"])
    df = sp.lambdify(X, sp.diff(sympy_of(expr), X), modules=["mpmath"])
    samples = 20000
    best = mp.inf
    previous_x = lo + (hi - lo) / samples
    try:
        previous = df(previous_x)
    except Exception:
        previous = None
    for i in range(2, samples):
        xv = lo + (hi - lo) * i / samples
        try:
            value = df(xv)
        except Exception:
            previous = None
            continue
        if previous is not None and previous * value < 0:
            left, right = previous_x, xv
            for _ in range(200):
                mid = 0.5 * (left + right)
                fm = df(mid)
                if previous * fm <= 0:
                    right = mid
                else:
                    left, previous = mid, fm
            root = 0.5 * (left + right)
            try:
                best = min(best, f(root))
            except Exception:
                pass
        previous_x, previous = xv, value
    return float(best)


def check_case(case: dict, native: dict) -> list[str]:
    problems: list[str] = []
    check = case["check"]
    section = case["section"]

    if "error_code" in check:
        if native.get("ok"):
            problems.append("expected an error, engine returned ok")
            return problems
        if native.get("error_code") != check["error_code"]:
            problems.append(
                f"error_code {native.get('error_code')!r} != {check['error_code']!r}"
            )
        if "error_point" in check:
            point = native.get("error_point")
            if point is None or not close(float(point), check["error_point"], 1e-6):
                problems.append(f"error_point {point!r} != {check['error_point']!r}")
        return problems

    if not native.get("ok"):
        problems.append(f"engine error: {native.get('error')}")
        return problems

    if section == "parse":
        if not same_expr((native.get("normalized") or {}).get("plain"), check["canonical"]):
            problems.append(
                f"canonical {(native.get('normalized') or {}).get('plain')!r} "
                f"!= {check['canonical']!r}"
            )
        if check.get("interpreted") and not native.get("interpreted_as"):
            problems.append("no interpreted_as notice")
        return problems

    if "polynomial" in check:
        if not same_expr((native.get("polynomial") or {}).get("plain"), check["polynomial"]):
            problems.append(
                f"polynomial {(native.get('polynomial') or {}).get('plain')!r} "
                f"!= {check['polynomial']!r}"
            )
        if "remainder_has" in check:
            remainder = (native.get("remainder") or {}).get("plain") or ""
            if check["remainder_has"] not in remainder:
                problems.append(f"remainder {remainder!r} lacks {check['remainder_has']!r}")
        if "warning_names" in check:
            warnings = " ".join(native.get("warnings") or [])
            if check["warning_names"] not in warnings:
                problems.append(f"warning does not name {check['warning_names']!r}")
        checks = native.get("checks") or []
        if len(checks) < check.get("min_checks", 0):
            problems.append(f"only {len(checks)} checks, expected {check['min_checks']}")
        return problems

    if "solution" in check:
        got = (native.get("solution") or {}).get("plain")
        if not same_expr(got, check["solution"]):
            problems.append(f"solution {got!r} != {check['solution']!r}")
        if check.get("interpreted") and not native.get("interpreted_as"):
            problems.append("no interpreted_as notice")
        if "numeric" in check:
            value = native.get("numeric_value")
            if value is None or not close(float(value), float(check["numeric"]),
                                          check.get("tol", 1e-6)):
                problems.append(f"numeric {value!r} != {check['numeric']!r}")
        return problems

    if check.get("no_solution"):
        if native.get("solution") or native.get("envelope"):
            problems.append("expected no envelope, engine returned one")
        return problems

    if "numeric" in check:
        value = native.get("numeric_value")
        if value is None or not close(float(value), float(check["numeric"]),
                                      check.get("tol", 1e-6)):
            problems.append(f"numeric {value!r} != {check['numeric']!r}")
        return problems

    if check.get("ode_oracle"):
        request = case["request"]
        rhs = check.get("oracle_rhs", request["f"])
        method = "heun" if request["topic"] == "heun" else "rk4"
        expected = integrate_ode(
            rhs, request["x0"], request["y0"], request["h"], int(request["n"]), method
        )
        value = native.get("numeric_value")
        if value is None or not close(float(value), expected, check.get("tol", 1e-8)):
            problems.append(f"numeric {value!r} != oracle {expected!r}")
        return problems

    if "fourier" in check:
        L = float(case["request"]["L"])
        terms = int(check["terms"])
        expr = sympy_of(check["fourier"])
        rows = coeff_table(native)
        if len(rows) != terms:
            problems.append(f"coefficient table has {len(rows)} rows, expected {terms}")
            return problems
        for n, an, bn in rows:
            oracle_an = sp.N(sp.integrate(expr * sp.cos(n * sp.pi * X / L),
                                          (X, -L, L)) / L, 20)
            oracle_bn = sp.N(sp.integrate(expr * sp.sin(n * sp.pi * X / L),
                                          (X, -L, L)) / L, 20)
            if not close(an, float(oracle_an), 1e-6):
                problems.append(f"a_{n} {an!r} != {float(oracle_an)!r}")
            if not close(bn, float(oracle_bn), 1e-6):
                problems.append(f"b_{n} {bn!r} != {float(oracle_bn)!r}")
        constant_detail = ""
        for item in native.get("checks") or []:
            if item.get("name") == "Constant term":
                constant_detail = item.get("detail") or ""
        oracle_constant = float(sp.N(sp.integrate(expr, (X, -L, L)) / (2 * L), 20))
        try:
            native_constant = float(constant_detail.split("=")[-1].strip().rstrip("."))
        except Exception:
            native_constant = None
        if native_constant is None or not close(native_constant, oracle_constant, 1e-6):
            problems.append(f"a0/2 {native_constant!r} != {oracle_constant!r}")
        if check.get("gibbs"):
            warnings = " ".join(native.get("warnings") or [])
            if "Gibbs" not in warnings:
                problems.append("no Gibbs note")
        for point in check.get("interior_points") or []:
            name = f"Partial sum at x = {point:g}"
            found = [c for c in native.get("checks") or [] if c.get("name") == name]
            if not found:
                problems.append(f"no interior validation at x = {point}")
            elif not found[0].get("ok"):
                problems.append(f"interior validation failed at x = {point}")
        return problems

    if "no_max" in check or "oracle_expr" in check:
        if check.get("no_max"):
            if native.get("maximum") is not None:
                problems.append(f"maximum {native['maximum']} should be absent")
        expected_min = oracle_min(check["oracle_expr"], float(case["request"]["xmin"]),
                                  float(case["request"]["xmax"]))
        value = number((native.get("minimum") or {}).get("plain"))
        if not close(value, expected_min, 2e-4):
            problems.append(f"minimum {value!r} != mpmath {expected_min!r}")
        if "warning_names" in check:
            warnings = " ".join(native.get("warnings") or [])
            if check["warning_names"] not in warnings:
                problems.append(f"warning does not name {check['warning_names']!r}")
        return problems

    if any(key in check for key in ("max", "min")):
        for key in ("max", "min"):
            if key not in check:
                continue
            got = number((native.get(f"{key}imum") or {}).get("plain"))
            if not close(got, float(check[key]), 1e-6):
                problems.append(f"{key}imum {got!r} != {check[key]!r}")
            at_key = f"{key}_at"
            if at_key in check:
                at = native.get(f"{key}imum_at")
                at_value = number((at or {}).get("plain")) if at else None
                if not close(at_value, float(check[at_key]), 1e-6):
                    problems.append(f"{key}imum_at {at_value!r} != {check[at_key]!r}")
        return problems

    problems.append(f"no check implemented for section {section!r}")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--lib", default=str(DEFAULT_LIB))
    parser.add_argument("--cases", default=str(DEFAULT_CASES))
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    engine = Engine(pathlib.Path(args.lib))
    cases = json.loads(pathlib.Path(args.cases).read_text(encoding="utf-8"))

    passed = failed = xfailed = xpassed = 0
    by_section: dict[str, list[int]] = {}
    for case in cases:
        section = case["section"]
        try:
            native = engine.call(case["request"])
            problems = check_case(case, native)
        except UnicodeDecodeError as exc:
            native = {}
            problems = [f"invalid UTF-8 response: {exc}"]
        except Exception as exc:  # engine crash or malformed JSON
            native = {}
            problems = [f"{type(exc).__name__}: {exc}"]

        xfail = case.get("xfail")
        if problems and xfail:
            status = "xfail"
            xfailed += 1
        elif problems:
            status = "FAIL"
            failed += 1
        elif xfail:
            status = "XPASS"
            xpassed += 1
        else:
            status = "ok"
            passed += 1
        stats = by_section.setdefault(section, [0, 0, 0, 0])
        stats[{"ok": 0, "xfail": 1, "FAIL": 2, "XPASS": 3}[status]] += 1
        print(f"[{status:5}] {section:<9} {case['id']}")
        for problem in problems:
            print(f"        - {problem}")
        if xfail:
            print(f"        ~ xfail {xfail}")
        if args.verbose and native:
            print(f"        solution: {(native.get('solution') or {}).get('plain')}")

    print()
    for section, (ok, xf, bad, xp) in sorted(by_section.items()):
        print(f"  {section:<10} {ok:>2} ok  {xf:>2} xfail  {bad:>2} fail  {xp:>2} xpass")
    print()
    print(f"{passed} passed, {xfailed} xfail, {failed} failed, {xpassed} xpass")
    unexpected = failed + xpassed
    return 0 if unexpected == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
