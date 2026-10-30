"""Command line entry point: python -m odelab_engine '<json>'."""

from __future__ import annotations

import json
import sys

from .engine import envelope, solve_ode, verify_candidate, wronskian


def main(argv: list[str] | None = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    if argv:
        raw = argv[0]
    else:
        raw = sys.stdin.read()
    try:
        request = json.loads(raw)
    except json.JSONDecodeError as exc:
        print(json.dumps({"ok": False, "error": f"invalid JSON: {exc}"}, ensure_ascii=False))
        return 2

    mode = request.get("mode", "ode")
    timeout = request.get("timeout", 20.0)
    try:
        if mode == "wronskian":
            payload = wronskian(request.get("functions", []), request.get("x", "x"))
        elif mode in {"envelope", "orthogonal"}:
            payload = envelope(
                request.get("input", ""),
                request.get("c", "c"),
                request.get("x", "x"),
                request.get("y", "y"),
                orthogonal=mode == "orthogonal" or bool(request.get("orthogonal", False)),
            )
        elif mode == "verify":
            payload = verify_candidate(
                request.get("input", ""),
                request.get("candidate", ""),
                x=request.get("x", "x"),
                y=request.get("y", "y"),
                implicit=bool(request.get("implicit", False)),
            )
        else:
            payload = solve_ode(
                request.get("input", ""),
                request.get("x", "x"),
                request.get("y", "y"),
                timeout=timeout,
            ).to_dict()
    except Exception as exc:  # never leak a traceback to the caller
        payload = {"ok": False, "error": f"{type(exc).__name__}: {exc}"}

    print(json.dumps(payload, ensure_ascii=False))
    if mode == "verify":
        return 0 if payload.get("ok") and payload.get("verified") else 1
    return 0 if payload.get("ok") else 1


if __name__ == "__main__":
    raise SystemExit(main())
