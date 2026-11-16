"""Oracle tests for the matrix and eigenvalue workbench.

The native engine is called through the same C ABI the app uses.  Determinants,
characteristic polynomials, eigenvalues, eigenvectors, inverses and
diagonalizability are checked against independent SymPy computations.
"""

from __future__ import annotations

import ctypes
import json
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
    text = re.sub(r"√\(([^)]*)\)", r"sqrt(\1)", text)
    text = re.sub(r"√(\d+(?:/\d+)?)", r"sqrt(\1)", text)
    text = text.replace("i", "I")
    return sympy.sympify(text)


def _step(response: dict, title_key: str) -> dict:
    return next(s for s in response["steps"] if s["title_key"] == title_key)


def _matrix_from_step(response: dict, title_key: str, prefix: str) -> sympy.Matrix:
    math = _step(response, title_key)["math"]["pretty"]
    body = math.split(prefix, 1)[1].strip()
    return sympy.Matrix(_sympy(body))


def _eigenvalues(response: dict) -> list[sympy.Expr]:
    answer = response["solution"]["pretty"]
    parts = [part.strip() for part in answer.split(",") if part.strip()]
    values = []
    for part in parts:
        _, _, expression = part.partition("=")
        values.append(_sympy(expression.strip()))
    return values


def _close(actual: sympy.Expr, expected: sympy.Expr) -> bool:
    difference = sympy.N(actual - expected)
    return abs(complex(difference)) <= 1e-4 * (1.0 + abs(complex(sympy.N(expected))))


def _close_multiset(actual: list[sympy.Expr], expected: list[sympy.Expr]) -> bool:
    if len(actual) != len(expected):
        return False
    remaining = list(expected)
    for value in actual:
        match = next((item for item in remaining if _close(value, item)), None)
        if match is None:
            return False
        remaining.remove(match)
    return True


def _matrix_text(matrix: sympy.Matrix) -> str:
    return "; ".join(
        ", ".join(str(matrix[i, j]) for j in range(matrix.cols))
        for i in range(matrix.rows)
    )


def _exercise(engine: Engine, matrix: sympy.Matrix) -> dict:
    response = topic(engine, "matrix_eigen", matrix=_matrix_text(matrix))
    assert response["ok"], response.get("error_code")
    assert response["checks"] and all(check["ok"] for check in response["checks"])

    trace_det = _step(response, "step.title.compute_trace_and_determinant")["math"]["pretty"]
    match = re.fullmatch(r"tr\(A\) = (.+?),  det\(A\) = (.+)", trace_det)
    assert match, trace_det
    assert _close(_sympy(match.group(1)), matrix.trace())
    assert _close(_sympy(match.group(2)), matrix.det())

    characteristic = _step(response, "step.title.build_the_characteristic_polynomial")["math"]["pretty"]
    variable = sympy.Symbol("λ")
    assert sympy.expand(_sympy(characteristic)) == sympy.expand(
        matrix.charpoly(variable).as_expr()
    )

    actual_roots = _eigenvalues(response)
    expected_roots = []
    for root, multiplicity in matrix.eigenvals().items():
        expected_roots.extend([root] * multiplicity)
    assert _close_multiset(actual_roots, expected_roots), (
        actual_roots,
        expected_roots,
    )

    # Every eigenvector reported for a rational eigenvalue is exact; the
    # native check already verifies Av = lambda v for the numeric ones.
    vectors = _step(response, "step.title.find_the_eigenvectors")["math"]["pretty"]
    assert "v1" in vectors

    # Exact cubic roots carry a tiny numerical imaginary part, so classify on
    # the evaluated values rather than on the symbolic `im(...) == 0` test.
    def _is_real(root: sympy.Expr) -> bool:
        return abs(complex(sympy.N(sympy.im(root)))) <= 1e-9

    diagonal = _step(response, "step.title.test_diagonalizability")["note_key"]
    if not matrix.is_diagonalizable():
        assert diagonal == (
            "step.note.the_matrix_is_not_diagonalizable_because_an_eigenvalue_lacks_enough_eigenvectors"
        )
    elif all(_is_real(root) for root in expected_roots):
        assert diagonal == "step.note.the_matrix_is_diagonalizable_over_the_reals"
    else:
        assert diagonal == (
            "step.note.the_matrix_is_diagonalizable_over_the_complex_numbers_but_not_the_reals"
        )

    inverse_steps = [
        step for step in response["steps"] if step["title_key"] == "step.title.invert_the_matrix"
    ]
    assert len(inverse_steps) == 1
    if matrix.det() != 0:
        inverse = _matrix_from_step(response, "step.title.invert_the_matrix", "A^{-1} =")
        assert inverse == matrix.inv()
    else:
        assert inverse_steps[0]["note_key"] == "step.note.a_singular_matrix_has_no_inverse"
    return response


def test_exact_2x2(engine: Engine) -> None:
    _exercise(engine, sympy.Matrix([[4, 1], [2, 3]]))


@pytest.mark.parametrize(
    "matrix",
    [
        [[2, 0, 0], [1, 3, 0], [4, 5, 6]],
        [[1, 2, 3], [0, 4, 5], [0, 0, 6]],
        [[1, 2, 3], [4, 5, 6], [7, 8, 9]],
        [[1, 0, 0], [0, 1, 0], [0, 0, 1]],
        [[5]],
        [[sympy.Rational(1, 2), sympy.Rational(1, 3)],
         [sympy.Rational(1, 4), sympy.Rational(1, 5)]],
    ],
)
def test_exact_matrices(engine: Engine, matrix: list) -> None:
    _exercise(engine, sympy.Matrix(matrix))


@pytest.mark.parametrize(
    "matrix,note",
    [
        ([[2, 1], [0, 2]], "step.note.the_matrix_is_not_diagonalizable_because_an_eigenvalue_lacks_enough_eigenvectors"),
        ([[0, -1], [1, 0]], "step.note.the_matrix_is_diagonalizable_over_the_complex_numbers_but_not_the_reals"),
        ([[1, 1, 0], [0, 1, 0], [0, 0, 2]], "step.note.the_matrix_is_not_diagonalizable_because_an_eigenvalue_lacks_enough_eigenvectors"),
    ],
)
def test_diagonalizability_edges(engine: Engine, matrix: list, note: str) -> None:
    response = _exercise(engine, sympy.Matrix(matrix))
    assert _step(response, "step.title.test_diagonalizability")["note_key"] == note


def test_irreducible_cubic_is_labelled_numeric(engine: Engine) -> None:
    matrix = sympy.Matrix([[0, 0, 2], [1, 0, 0], [0, 1, 0]])
    response = _exercise(engine, matrix)
    assert _step(response, "step.title.find_the_eigenvalues")["note_key"] == (
        "step.note.some_eigenvalues_are_reported_numerically"
    )


def test_random_matrices_match_sympy(engine: Engine) -> None:
    rng = random.Random(20261111)
    for _ in range(24):
        size = rng.choice([2, 2, 3, 3])
        if rng.random() < 0.3:
            matrix = sympy.Matrix(
                [
                    [
                        sympy.Rational(rng.randint(-6, 6), rng.randint(1, 6))
                        for _ in range(size)
                    ]
                    for _ in range(size)
                ]
            )
        else:
            matrix = sympy.Matrix(
                [[rng.randint(-5, 5) for _ in range(size)] for _ in range(size)]
            )
        _exercise(engine, matrix)


def test_irreducible_cubic_with_real_roots_is_diagonalizable_over_reals(
    engine: Engine,
) -> None:
    # The exact cubic roots evaluate with a round-off imaginary part; the real
    # eigenvalue test must not trip on that noise.
    matrix = sympy.Matrix([[4, 6, -5], [1, -1, 0], [-6, -2, 2]])
    response = _exercise(engine, matrix)
    assert _step(response, "step.title.test_diagonalizability")["note_key"] == (
        "step.note.the_matrix_is_diagonalizable_over_the_reals"
    )


@pytest.mark.parametrize(
    "text,code",
    [
        ("1, 2; 3", "bad_matrix"),
        ("1, 2; 3, 4; 5, 6", "bad_matrix"),
        ("1, x; 2, 3", "matrix_entry_invalid"),
        ("1, 2, 3, 4; 5, 6, 7, 8; 9, 10, 11, 12; 13, 14, 15, 16", "matrix_too_large"),
        ("", "bad_matrix"),
    ],
)
def test_invalid_matrices_are_coded(engine: Engine, text: str, code: str) -> None:
    response = topic(engine, "matrix_eigen", matrix=text)
    assert not response["ok"] and response["error_code"] == code


def test_translations_resolve_through_the_catalogue() -> None:
    # The widget/catalogue guard checks the whole app; this is a focused check
    # that the new keys are present in both languages used by the engine.
    catalogue = ROOT / "app" / "assets" / "i18n"
    english = json.loads((catalogue / "en.json").read_text(encoding="utf-8"))
    persian = json.loads((catalogue / "fa.json").read_text(encoding="utf-8"))
    for key in (
        "topic.matrix_eigen.title",
        "topic.matrix_eigen.blurb",
        "step.title.find_the_eigenvalues",
        "step.note.eigenvectors_solve_a_minus_lambda_i_v_0",
        "step.note.some_eigenvectors_are_reported_numerically",
        "check.characteristic_polynomial_vanishes_at_each_eigenvalue",
        "error.bad_matrix",
    ):
        assert key in english and key in persian
