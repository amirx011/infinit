#!/usr/bin/env python3
"""Run a broad set of higher-order ODEs and report how each one is handled.

Each equation is solved in a child process so that a crash or an infinite loop
cannot take down the whole audit.
"""
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / "core" / "build" / "lib" / "libodecore.so"

RUNNER = r'''
import ctypes, json, sys
lib = ctypes.CDLL(sys.argv[2])
lib.ode_solve.argtypes = [ctypes.c_char_p]
lib.ode_solve.restype = ctypes.c_void_p
lib.ode_free.argtypes = [ctypes.c_void_p]
ptr = lib.ode_solve(json.dumps({"mode": "ode", "input": sys.argv[1],
                                "x": "x", "y": "y", "lang": "en"}).encode())
if not ptr:
    print(json.dumps({"crash": "null result"}))
    sys.exit(0)
try:
    r = json.loads(ctypes.string_at(ptr).decode())
finally:
    lib.ode_free(ptr)
def plain(x):
    return (x or {}).get("plain") if x else None
print(json.dumps({
    "ok": r.get("ok"), "type": r.get("type_id"), "explicit": r.get("is_explicit"),
    "solution": plain(r.get("solution")), "explicit_y": plain(r.get("explicit_y")),
    "error": r.get("error", ""), "steps": len(r.get("steps", [])),
    "checks": [(c.get("name"), c.get("ok"), c.get("detail")) for c in r.get("checks", [])],
}))
'''

MISSING_Y = [
    "y'' = 0", "y'' = 1", "y'' = x", "y'' = x^2", "y'' = x^3",
    "y'' = 2*x + 1", "y'' = 6*x", "y'' = sin(x)", "y'' = cos(x)",
    "y'' = exp(x)", "y'' = exp(-x)", "y'' = 1/x", "y'' = 1/x^2",
    "y'' = sqrt(x)", "y'' = 1/(1+x^2)", "y'' = tan(x)", "y'' = sec(x)^2",
    "y'' = sinh(x)", "y'' = cosh(x)", "y'' = ln(x)", "y'' = x*exp(x)",
    "y'' = x*sin(x)", "y'' = exp(x)*cos(x)",
    "y''' = 0", "y''' = 1", "y''' = x", "y''' = exp(x)", "y''' = sin(x)",
    "y'''' = x", "y'''' = exp(x)",
    "y'' = y'", "y'' = 2*y'", "y'' = -y'", "y'' = y'^2", "y'' = y'^3",
    "y'' = sqrt(1+y'^2)", "y'' = 1+y'^2", "y'' = 1-y'^2", "y'' = exp(-y')",
    "y'' = exp(y')", "y'' = 1/y'", "y'' = y'/x", "y'' = x*y'", "y'' = y' + x",
    "y'' = y' + exp(x)", "y'' + y' = 0", "y'' + y' = 1", "y'' + y' = x",
    "y'' + y' = sin(x)", "y'' + 2*y' = exp(x)", "y'' - y' = x^2",
    "y'' + y'^2 = 0", "y'' = y'*(1-y')", "y'' = y'*(y'-1)",
    "y''' = y''", "y''' + y'' = 0", "y''' = x*y''", "y''' = y'' + x",
    "y''' = exp(x)",
]

MISSING_X = [
    "y'' = y^2", "y'' = y^3", "y'' = -y^2", "y'' = -y^3", "y'' = y^4",
    "y'' = sqrt(y)", "y'' = 1/sqrt(y)", "y'' = 1/y", "y'' = 1/y^2",
    "y'' = exp(y)", "y'' = exp(-y)", "y'' = sin(y)", "y'' = cos(y)",
    "y'' = tan(y)", "y'' = sinh(y)", "y'' = cosh(y)",
    "y'' = y*y'", "y'' = y^2*y'", "y'' = y'*y^3", "y'' = y' + y",
    "y'' = y' + y^2", "y'' = y'^2 + y", "y'' = y'^2 + y^2",
    "y'' = y'*sqrt(y)", "y'' = y'*exp(y)", "y'' = y'/(1+y^2)",
    "y'' = (1+y'^2)/y", "y'' = (1+y'^2)^(3/2)", "y'' = y'^2/y",
    "y'' = y'^3", "y'' = y'^2 + 1", "y'' = y'^2 - 1",
    "y'' + y*y' = 0", "y'' + y^2 = 0", "y'' + y^3 = 0",
    "y'' + sin(y) = 0", "y'' + exp(y) = 0", "y'' - y^2 = 0",
    "y*y'' = y'^2", "y*y'' + y'^2 = 0", "y'' = y'*(1-y')",
    "y'' = y'*(y'-1)", "y'' = y'*(y'+1)", "y'' = y'/y",
    "y'' = y'/(1+y)", "y'' = y'*cos(y)", "y'' = y'*sin(y)",
]

LINEAR = [
    "y'' - 3*y' + 2*y = 0", "y'' + 4*y = 0", "y'' - y = x",
    "y'' + y = sin(x)", "y''' - y = 0", "y''' + y = 0",
    "y'''' + y'' = 0", "y''' - 3*y'' + 3*y' - y = 0",
    "y'''' - y = 0", "y''' + y'' + y' + y = 0",
]


def run_one(eq):
    try:
        p = subprocess.run([sys.executable, "-c", RUNNER, eq, str(LIB)],
                           capture_output=True, text=True, timeout=20)
    except subprocess.TimeoutExpired:
        return {"crash": "timeout"}
    if p.returncode != 0:
        return {"crash": f"exit {p.returncode}: {p.stderr.strip()[-300:]}"}
    try:
        return json.loads(p.stdout.strip().splitlines()[-1])
    except Exception as exc:
        return {"crash": f"bad output: {exc}: {p.stdout[-300:]}"}


def main():
    groups = [("missing y", MISSING_Y), ("missing x", MISSING_X), ("linear", LINEAR)]
    rows = []
    for label, eqs in groups:
        for eq in eqs:
            r = run_one(eq)
            rows.append((label, eq, r))

    counts = {"explicit": 0, "implicit": 0, "failed": 0, "crash": 0}
    problems = []
    for label, eq, r in rows:
        if "crash" in r:
            counts["crash"] += 1
            problems.append((label, eq, "CRASH " + r["crash"]))
            continue
        if not r.get("ok"):
            counts["failed"] += 1
            problems.append((label, eq, "FAIL " + (r.get("error") or "")[:160]))
            continue
        bad = [c for c in r.get("checks", []) if not c[1]]
        if bad:
            counts["failed"] += 1
            problems.append((label, eq, "CHECK " + str(bad)[:160]))
            continue
        if r.get("explicit"):
            counts["explicit"] += 1
        else:
            counts["implicit"] += 1

    print(f"total={len(rows)} explicit={counts['explicit']} implicit={counts['implicit']} "
          f"failed={counts['failed']} crash={counts['crash']}")
    for label, eq, msg in problems:
        print(f"  [{label}] {eq} -> {msg}")
    print()
    print("per-equation results:")
    for label, eq, r in rows:
        if "crash" in r:
            print(f"  [{label:9}] {eq:34} CRASH {r['crash']}")
        elif not r.get("ok"):
            print(f"  [{label:9}] {eq:34} FAIL  {r.get('error') or ''}")
        else:
            kind = "explicit" if r.get("explicit") else "implicit"
            sol = r.get("explicit_y") or r.get("solution")
            print(f"  [{label:9}] {eq:34} {kind:8} {r.get('type')} :: {sol}")


if __name__ == "__main__":
    main()
