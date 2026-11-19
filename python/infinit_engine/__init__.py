"""Infinit Lab — SymPy reference engine.

``solve_ode`` returns an :class:`OracleResult`; ``wronskian``, ``envelope`` and
``oblique`` return plain dictionaries because their payloads are structured
data rather than an ODE solution.
"""

from .engine import (
    OracleResult,
    envelope,
    oblique,
    parse_expression,
    solve_ode,
    verify_candidate,
    verify_parametric,
    wronskian,
)

__all__ = [
    "OracleResult",
    "envelope",
    "oblique",
    "parse_expression",
    "solve_ode",
    "verify_candidate",
    "verify_parametric",
    "wronskian",
]
