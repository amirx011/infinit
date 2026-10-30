#!/usr/bin/env python3
"""Cross-check the native C++ engine against the SymPy oracle.

Usage:
    python3 python/tests/compare_cpp.py [--lib PATH] [--corpus PATH]
                                        [--timeout SECONDS] [--verbose]
                                        [--category NAME]

Exit status is non-zero when a case fails.  Answers that are intentionally
parametric (Clairaut/Lagrange) or quadrature-only are verified independently
when possible and reported as ``skip`` (with a reason) when SymPy cannot check
them, so nothing passes silently.

Corpus entries may also set ``"expect_error": true`` for inputs that the core
is supposed to reject (unsupported forms); solving one is then a failure.
"""

from __future__ import annotations

import argparse
import ctypes
import json
import pathlib
import sys

import sympy as sp

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "python"))

from infinit_engine import (  # noqa: E402
    envelope,
    solve_ode,
    verify_candidate,
    verify_parametric,
    wronskian,
)


class Engine:
    def __init__(self, path: pathlib.Path):
        if not path.exists():
            raise SystemExit(
                f"native library not found: {path}\n"
                "build it first, e.g. ./scripts/build_core.sh (or pass --lib PATH)"
            )
        self.lib = ctypes.CDLL(str(path))
        self.lib.ode_solve.argtypes = [ctypes.c_char_p]
        self.lib.ode_solve.restype = ctypes.c_void_p
        self.lib.ode_free.argtypes = [ctypes.c_void_p]

    def call(self, payload: dict) -> dict:
        ptr = self.lib.ode_solve(json.dumps(payload).encode())
        try:
            return json.loads(ctypes.string_at(ptr).decode("utf-8"))
        finally:
            self.lib.ode_free(ptr)


def eq_curve(text: str) -> sp.Expr:
    text = text.strip()
    if text.startswith("Eq(") and text.endswith(")"):
        text = text[3:-1]
        left, right = text.rsplit(",", 1)
        return sp.sympify(left.replace("^", "**")) - sp.sympify(right.replace("^", "**"))
    return sp.sympify(text.replace("^", "**"))


def native_kind(native: dict) -> str:
    if native.get("param_x"):
        return "parametric"
    if native.get("closed_form"):
        return "explicit" if native.get("is_explicit") else "implicit"
    return "quadrature"


def check_case(case: dict, native: dict, timeout: float) -> tuple[list[str], str | None]:
    """Return (problems, skip_reason)."""
    problems: list[str] = []
    text = case["input"]
    if case.get("expect_error"):
        if native.get("ok"):
            problems.append(
                f"native unexpectedly solved an unsupported equation "
                f"(type {native.get('type_id')})"
            )
        return problems, None
    if not native.get("ok"):
        problems.append(f"native error: {native.get('error')}")
        return problems, None

    allowed = case.get("type")
    if allowed:
        allowed = [allowed] if isinstance(allowed, str) else list(allowed)
        if native.get("type_id") not in allowed:
            problems.append(f"type {native.get('type_id')} not in {allowed}")

    if case.get("order") and native.get("order") != case["order"]:
        problems.append(f"order {native.get('order')} != {case['order']}")

    verify = case.get("verify") or "auto"
    if verify == "auto":
        if native.get("type_id") == "lagrange" and native.get("param_x"):
            verify = "parametric"
        else:
            verify = native_kind(native)

    if verify == "skip":
        return problems, "no verification requested"

    if verify == "branch":
        return problems, "accepted by sign branch (sqrt(y) has two branches)"

    if verify == "explicit":
        candidate = native.get("explicit_y") or native.get("solution")
        if not candidate:
            problems.append("native produced no explicit answer")
            return problems, None
        check = verify_candidate(text, candidate["plain"])
        if not check.get("ok"):
            problems.append(f"oracle verify error: {check.get('error')}")
        elif not check.get("verified"):
            problems.append(f"native residual: {check.get('residual')}")
        # Clairaut with a transcendental f also exposes the singular branch
        # parametrically; verify it too when present.
        if native.get("param_singular") and native.get("param_x") and native.get("param_y"):
            branch = verify_parametric(
                text, native["param_x"]["plain"], native["param_y"]["plain"]
            )
            if not branch.get("ok"):
                problems.append(f"envelope verify error: {branch.get('error')}")
            elif not branch.get("verified"):
                problems.append(f"envelope residual: {branch.get('residual')}")
        return problems, None

    if verify == "implicit":
        candidate = native.get("solution")
        if not candidate:
            problems.append("native produced no solution")
            return problems, None
        check = verify_candidate(text, candidate["plain"], implicit=True)
        if not check.get("ok"):
            problems.append(f"oracle verify error: {check.get('error')}")
        elif not check.get("verified"):
            reason = check.get("reason") or f"residual: {check.get('residual')}"
            problems.append(f"native implicit: {reason}")
        return problems, None

    if verify == "parametric":
        px = native.get("param_x")
        py = native.get("param_y")
        if not px or not py:
            problems.append("native produced no parametric answer")
            return problems, None
        check = verify_parametric(text, px["plain"], py["plain"])
        if not check.get("ok"):
            problems.append(f"parametric verify error: {check.get('error')}")
        elif not check.get("verified"):
            problems.append(f"parametric residual: {check.get('residual')}")
        return problems, None

    if verify == "quadrature":
        candidate = native.get("solution")
        if not candidate:
            problems.append("native produced no quadrature")
            return problems, None
        check = verify_candidate(
            text, candidate["plain"], implicit=not native.get("is_explicit")
        )
        if check.get("ok") and check.get("verified"):
            return problems, None
        reason = check.get("error") or check.get("reason") or check.get("residual")
        return problems, f"quadrature could not be verified ({reason})"

    problems.append(f"unknown verify mode: {verify}")
    return problems, None


def verify_orthogonal_native(solution_text: str, explicit: bool, slope_text: str):
    """Check a native orthogonal family against the oracle's orthogonal slope."""
    try:
        X, Y = sp.Symbol("x", real=True), sp.Symbol("y", real=True)
        slope = sp.sympify(slope_text.replace("^", "**"), locals={"x": X, "y": Y, "e": sp.E})
        expr = sp.sympify(solution_text.replace("^", "**"), locals={"x": X, "y": Y, "C": sp.Symbol("C", real=True)})
        if explicit:
            residual = sp.diff(expr, X) - slope.subs(Y, expr)
        else:
            residual = sp.diff(expr, X) + sp.diff(expr, Y) * slope
        free = {s for s in residual.free_symbols if isinstance(s, sp.Symbol) and s not in (X, Y)}
        constants = {s: sp.Float(1.0 + 0.3 * i) for i, s in enumerate(sorted(free, key=str))}
        for xv, yv in ((0.37, 1.13), (1.71, 0.77), (2.53, 2.31), (0.91, 3.71)):
            value = sp.N(residual.subs(constants).subs({X: xv, Y: yv}), 20)
            if not value.is_number or abs(complex(value)) > 1e-5:
                return False, f"residual={sp.simplify(residual)}"
        return True, ""
    except Exception as exc:
        return False, f"{type(exc).__name__}: {exc}"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--lib",
        default=str(ROOT / "core" / "build" / "lib" / "libodecore.so"),
    )
    parser.add_argument("--corpus", default=str(ROOT / "testdata" / "corpus.json"))
    parser.add_argument("--timeout", type=float, default=20.0)
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--category", help="only run one corpus category")
    args = parser.parse_args()

    engine = Engine(pathlib.Path(args.lib))
    corpus = json.loads(pathlib.Path(args.corpus).read_text(encoding="utf-8"))
    if args.category:
        corpus = [c for c in corpus if c.get("category") == args.category]
    if not corpus:
        raise SystemExit("no corpus cases selected")

    failures = 0
    passed = 0
    skipped = 0
    by_category: dict[str, list[int]] = {}
    label_width = max(len(c["input"]) for c in corpus) + 2
    for case in corpus:
        text = case["input"]
        category = case.get("category", "misc")
        native = engine.call({"mode": "ode", "input": text})
        problems, skip_reason = check_case(case, native, args.timeout)
        if skip_reason and not problems:
            status = "skip"
            skipped += 1
        elif problems:
            status = "FAIL"
            failures += 1
        else:
            status = "ok"
            passed += 1
        stats = by_category.setdefault(category, [0, 0, 0])
        stats[{"ok": 0, "skip": 1, "FAIL": 2}[status]] += 1
        pretty = native.get("solution", {}).get("pretty", "") if native.get("solution") else ""
        print(
            f"[{status:4}] {category:<17} {text:<{label_width}} "
            f"{native.get('type_id', ''):<20} {pretty}"
        )
        for problem in problems:
            print(f"       - {problem}")
        if skip_reason:
            print(f"       ~ {skip_reason}")
        if case.get("note"):
            print(f"       · {case['note']}")
        if args.verbose:
            oracle = solve_ode(text, timeout=args.timeout)
            print(f"       oracle: {oracle.solution or oracle.error}")

    # ---------------------------------------------------------- Wronskian
    wronskian_cases = [
        (["sin(x)", "cos(x)"], True),
        (["x", "2*x"], False),
        (["1", "x", "x^2"], True),
        (["2x", "x^2"], True),
        (["e^x", "e^(2x)"], True),
        (["x", "x*ln(abs(x))"], True),
    ]
    for fns, expect_independent in wronskian_cases:
        native = engine.call({"mode": "wronskian", "functions": fns})
        oracle = wronskian(fns)
        try:
            nw = sp.sympify((native.get("wronskian") or {}).get("plain", "nan").replace("^", "**"))
            ow = sp.sympify(oracle["wronskian"])
            same = sp.simplify(nw - ow) == 0
        except Exception:
            same = False
        ok = (
            same
            and native.get("independent") == oracle["independent"]
            and oracle["independent"] == expect_independent
        )
        print(
            f"[{'ok' if ok else 'FAIL':4}] W{tuple(fns)} native={nw} oracle={ow} "
            f"independent={native.get('independent')}"
        )
        if not ok:
            failures += 1

    # ------------------------------------------------------------ Envelope
    envelope_cases = [
        ("y = c*x + c^2", False),
        ("x^2 + y^2 = c^2", True),
    ]
    for family, expect_empty in envelope_cases:
        native = engine.call({"mode": "envelope", "input": family})
        oracle = envelope(family)
        native_curves = native.get("envelope") or []
        if expect_empty:
            ok = native.get("ok") and not native_curves and not oracle["envelope"]
        else:
            ok = (
                native.get("ok")
                and native_curves
                and oracle["envelope"]
                and sp.simplify(
                    eq_curve(native_curves[0]["plain"]) - eq_curve(oracle["envelope"][0])
                )
                == 0
            )
        print(
            f"[{'ok' if ok else 'FAIL':4}] envelope {family} "
            f"native={len(native_curves)} oracle={len(oracle['envelope'])}"
        )
        if not ok:
            failures += 1

    # ------------------------------------------------ Orthogonal trajectories
    for family in ("y = c*x", "x^2 + y^2 = c^2"):
        native = engine.call({"mode": "orthogonal", "input": family})
        oracle = envelope(family, orthogonal=True)
        native_solution = native.get("orthogonal")
        ok = bool(native.get("ok")) and bool(oracle.get("ok")) and native_solution is not None
        detail = ""
        if ok:
            ok, detail = verify_orthogonal_native(
                native_solution["plain"],
                bool(native.get("orthogonal_explicit")),
                oracle.get("orthogonal_slope", ""),
            )
        print(
            f"[{'ok' if ok else 'FAIL':4}] orthogonal {family} "
            f"native={native_solution['plain'] if native_solution else None} "
            f"oracle={oracle.get('orthogonal_slope')} {detail}"
        )
        if not ok:
            failures += 1

    print()
    for category, (ok, sk, bad) in sorted(by_category.items()):
        print(f"  {category:<20} {ok:>2} ok  {sk:>2} skip  {bad:>2} fail")
    print()
    print(f"{passed} passed, {skipped} skipped, {failures} failure(s)")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
