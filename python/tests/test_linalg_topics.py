"""Oracle tests for the elementary linear-algebra workbenches.

Rank, determinant, inverse, LU and A x = b are checked against independent
SymPy computations through the same C ABI the app uses.  The symbolic answers
are verified structurally (for example A A^{-1} = I), never by matching the
engine's own intermediate strings.
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


def _expr(text: str) -> sympy.Expr:
    text = text.strip().replace("−", "-").replace("·", "*")
    text = re.sub(r"√\(([^)]*)\)", r"sqrt(\1)", text)
    return sympy.sympify(text)


def _matrix(text: str) -> sympy.Matrix:
    body = text.strip()
    rows = re.findall(r"\[([^\[\]]*)\]", body)
    if not rows:
        raise AssertionError(f"not a matrix: {text!r}")
    return sympy.Matrix([[_expr(cell) for cell in row.split(",")] for row in rows])


def _after_equals(text: str) -> str:
    return text.split("=", 1)[1].strip()


def _inverse(text: str) -> sympy.Matrix:
    body = _after_equals(text)
    if " * " in body:
        factor, rest = body.split(" * ", 1)
        return _expr(factor) * _matrix(rest)
    return _matrix(body)


def _request_text(matrix: sympy.Matrix) -> str:
    return "; ".join(", ".join(sympy.sstr(cell) for cell in row) for row in matrix.tolist())


def _functions(response: dict) -> dict[str, sympy.Expr]:
    values = {}
    for line in response["functions"]:
        name, _, expression = line.partition("=")
        values[name.strip()] = _expr(expression)
    return values


def _checks_ok(response: dict) -> bool:
    return bool(response["checks"]) and all(check["ok"] for check in response["checks"])


# ---------------------------------------------------------------- rank
def test_rank_and_null_space_match_sympy(engine: Engine) -> None:
    cases = [
        sympy.Matrix([[1, 2, 3], [2, 4, 6], [1, 1, 1]]),
        sympy.Matrix([[1, 2], [2, 4]]),
        sympy.Matrix([[1, 0], [0, 1]]),
        sympy.Matrix([[1, 2, 3], [0, 1, 4]]),
        sympy.Matrix([[2, 4, 6], [1, 2, 3]]),
        sympy.Matrix([[1, 2], [3, 4], [5, 6]]),
    ]
    for matrix in cases:
        response = topic(engine, "linalg_rank", matrix=_request_text(matrix))
        assert response["ok"], response
        rank = matrix.rank()
        nullity = matrix.cols - rank
        assert response["solution"]["plain"] == (
            f"rank(A) = {rank},  nullity(A) = {nullity}"
        )
        assert _checks_ok(response)
        if nullity:
            basis = matrix.nullspace()
            assert len(response["functions"]) == len(basis)
            for line in response["functions"]:
                vector = _matrix(_after_equals(line))
                assert matrix * vector == sympy.zeros(matrix.rows, 1)
        else:
            assert response["functions"] == []


def test_symbolic_rank_is_generic(engine: Engine) -> None:
    a, b = sympy.symbols("a b")
    matrix = sympy.Matrix([[a, b], [2 * a, 2 * b]])
    response = topic(engine, "linalg_rank", matrix=_request_text(matrix))
    assert response["ok"]
    assert response["solution"]["plain"] == "rank(A) = 1,  nullity(A) = 1"
    assert response["assumptions"] == ["assumption.generic_symbolic_pivots"]
    assert _checks_ok(response)


# ---------------------------------------------------------------- determinant
def test_determinant_matches_sympy(engine: Engine) -> None:
    cases = [
        sympy.Matrix([[1, 2], [3, 4]]),
        sympy.Matrix([[2, 0, 0], [1, 3, 0], [4, 5, 6]]),
        sympy.Matrix([[1, 2, 3], [4, 5, 6], [7, 8, 10]]),
        sympy.Matrix([[sympy.Rational(1, 3), 0], [0, sympy.Rational(1, 2)]]),
        sympy.Matrix([[1, 2], [2, 4]]),
    ]
    for matrix in cases:
        response = topic(engine, "linalg_det", matrix=_request_text(matrix))
        assert response["ok"], response
        assert sympy.simplify(_expr(_after_equals(response["solution"]["plain"])) - matrix.det()) == 0
        assert _checks_ok(response)


def test_symbolic_determinants(engine: Engine) -> None:
    a, b, c, d = sympy.symbols("a b c d")
    matrix = sympy.Matrix([[a, b], [c, d]])
    response = topic(engine, "linalg_det", matrix=_request_text(matrix))
    assert response["ok"]
    value = _expr(_after_equals(response["solution"]["plain"]))
    assert sympy.simplify(value - matrix.det()) == 0

    a, b, c, d, e, f, g, h, k = sympy.symbols("a b c d e f g h k")
    cube = sympy.Matrix([[a, b, c], [d, e, f], [g, h, k]])
    response = topic(engine, "linalg_det", matrix=_request_text(cube))
    assert response["ok"]
    value = _expr(_after_equals(response["solution"]["plain"]))
    assert sympy.simplify(value - cube.det()) == 0


def test_symbolic_4x4_determinant(engine: Engine) -> None:
    # The elimination pivots nest three fractions deep; the engine has to
    # expand their product back to the 24-term cofactor form.
    matrix = sympy.Matrix(4, 4, sympy.symbols("a b c d f g h j k l m n p q r s"))
    response = topic(engine, "linalg_det", matrix=_request_text(matrix))
    assert response["ok"]
    value = _expr(_after_equals(response["solution"]["plain"]))
    assert sympy.expand(value - matrix.det()) == 0
    assert _checks_ok(response)


# ---------------------------------------------------------------- inverse
def test_inverse_matches_sympy(engine: Engine) -> None:
    cases = [
        sympy.Matrix([[2, 1], [1, 1]]),
        sympy.Matrix([[4, 3], [6, 3]]),
        sympy.Matrix([[2, 0, 0], [1, 3, 0], [4, 5, 6]]),
        sympy.Matrix([[sympy.Rational(1, 2), sympy.Rational(1, 3)],
                      [sympy.Rational(1, 4), sympy.Rational(1, 5)]]),
    ]
    for matrix in cases:
        response = topic(engine, "linalg_inverse", matrix=_request_text(matrix))
        assert response["ok"], response
        assert _inverse(response["solution"]["plain"]) == matrix.inv()
        assert _checks_ok(response)


def test_symbolic_inverse_is_verified(engine: Engine) -> None:
    a, b, c, d = sympy.symbols("a b c d")
    matrix = sympy.Matrix([[a, b], [c, d]])
    response = topic(engine, "linalg_inverse", matrix=_request_text(matrix))
    assert response["ok"]
    inverse = _inverse(response["solution"]["plain"])
    assert sympy.simplify(matrix * inverse - sympy.eye(2)) == sympy.zeros(2)
    assert response["assumptions"] == ["assumption.generic_symbolic_pivots"]

    a, b, c, d, e, f, g, h, k = sympy.symbols("a b c d e f g h k")
    cube = sympy.Matrix([[a, b, c], [d, e, f], [g, h, k]])
    response = topic(engine, "linalg_inverse", matrix=_request_text(cube))
    assert response["ok"]
    inverse = _inverse(response["solution"]["plain"])
    assert sympy.simplify(cube * inverse - sympy.eye(3)) == sympy.zeros(3)


def test_symbolic_4x4_inverse_is_verified(engine: Engine) -> None:
    # A 4x4 symbolic inverse is only readable as adj(A)/det(A); the nested
    # Gauss-Jordan form runs to thousands of characters.
    matrix = sympy.Matrix(4, 4, sympy.symbols("a b c d f g h j k l m n p q r s"))
    response = topic(engine, "linalg_inverse", matrix=_request_text(matrix))
    assert response["ok"]
    plain = response["solution"]["plain"]
    assert plain.startswith("A^{-1} = 1/(")
    assert len(plain) < 2000
    inverse = _inverse(plain)
    product = matrix * inverse
    for i in range(4):
        for j in range(4):
            assert sympy.simplify(product[i, j]) == (1 if i == j else 0)
    assert _checks_ok(response)


def test_singular_inverse_is_an_answer(engine: Engine) -> None:
    response = topic(engine, "linalg_inverse", matrix="1, 1; 1, 1")
    assert response["ok"]
    assert response["solution"]["plain"] == "A^{-1} does not exist"
    assert "warning.singular_matrix" in response["warnings"]
    assert response["error_code"] == ""


# ---------------------------------------------------------------- LU
def test_lu_reconstructs_the_matrix(engine: Engine) -> None:
    cases = [
        sympy.Matrix([[4, 3], [6, 3]]),
        sympy.Matrix([[0, 1], [1, 0]]),
        sympy.Matrix([[2, 1, 1], [4, 3, 3], [8, 7, 9]]),
        sympy.Matrix([[1, 2, 3], [4, 5, 6], [7, 8, 10]]),
    ]
    for matrix in cases:
        response = topic(engine, "linalg_lu", matrix=_request_text(matrix))
        assert response["ok"], response
        factors = {
            name: _matrix(_after_equals(line))
            for name, line in zip(("L", "U", "P"), response["functions"])
        }
        L, U, P = factors["L"], factors["U"], factors["P"]
        assert sympy.simplify(P * matrix - L * U) == sympy.zeros(matrix.rows)
        assert all(L[i, i] == 1 for i in range(L.rows))
        assert all(L[i, j] == 0 for i in range(L.rows) for j in range(L.cols) if j > i)
        assert all(U[i, j] == 0 for i in range(U.rows) for j in range(U.cols) if j < i)
        # P is a permutation matrix.
        assert sorted(P.tolist()) == sorted(sympy.eye(matrix.rows).tolist())
        assert _checks_ok(response)


def test_symbolic_lu_is_generic(engine: Engine) -> None:
    a, b, c, d = sympy.symbols("a b c d")
    matrix = sympy.Matrix([[a, b], [c, d]])
    response = topic(engine, "linalg_lu", matrix=_request_text(matrix))
    assert response["ok"]
    factors = {
        name: _matrix(_after_equals(line))
        for name, line in zip(("L", "U", "P"), response["functions"])
    }
    assert sympy.simplify(
        factors["P"] * matrix - factors["L"] * factors["U"]
    ) == sympy.zeros(2)
    assert response["assumptions"] == ["assumption.generic_symbolic_pivots"]


# ---------------------------------------------------------------- A x = b
def test_solve_matches_sympy(engine: Engine) -> None:
    cases = [
        (sympy.Matrix([[2, 1], [1, -1]]), sympy.Matrix([5, 1])),
        (sympy.Matrix([[1, 2], [3, 4]]), sympy.Matrix([5, 6])),
        (sympy.Matrix([[1, 2, 3], [0, 1, 4], [5, 6, 0]]), sympy.Matrix([1, 2, 3])),
        (
            sympy.Matrix([[sympy.Rational(1, 2), 1], [1, 3]]),
            sympy.Matrix([1, 0]),
        ),
    ]
    for matrix, rhs in cases:
        for method in ("gauss", "cramer"):
            response = topic(
                engine,
                "linear_solve",
                matrix=_request_text(matrix),
                rhs="; ".join(sympy.sstr(v) for v in rhs),
                method=method,
            )
            assert response["ok"], response
            values = _functions(response)
            assert len(values) == matrix.cols
            solution = sympy.Matrix([values[f"x_{i + 1}"] for i in range(matrix.cols)])
            assert sympy.simplify(matrix * solution - rhs) == sympy.zeros(matrix.rows, 1)
            assert _checks_ok(response)


def test_symbolic_solve_is_verified(engine: Engine) -> None:
    a, b, c, d = sympy.symbols("a b c d")
    matrix = sympy.Matrix([[a, b], [c, d]])
    rhs = sympy.Matrix([1, 0])
    response = topic(
        engine,
        "linear_solve",
        matrix=_request_text(matrix),
        rhs="1; 0",
        method="gauss",
    )
    assert response["ok"]
    values = _functions(response)
    solution = sympy.Matrix([values["x_1"], values["x_2"]])
    assert sympy.simplify(matrix * solution - rhs) == sympy.zeros(2, 1)
    assert response["assumptions"] == ["assumption.generic_symbolic_pivots"]


def test_underdetermined_system_reports_free_variables(engine: Engine) -> None:
    response = topic(
        engine,
        "linear_solve",
        matrix="1, 2, 3; 2, 4, 6",
        rhs="1; 2",
        method="gauss",
    )
    assert response["ok"]
    assert "warning.linear_system_infinite_solutions" in response["warnings"]
    assert "t_1" in response["solution"]["plain"]


def test_inconsistent_system_is_coded(engine: Engine) -> None:
    response = topic(
        engine, "linear_solve", matrix="1, 1; 1, 1", rhs="1; 2", method="gauss"
    )
    assert not response["ok"]
    assert response["error_code"] == "linear_system_inconsistent"


# ---------------------------------------------------------------- invalid input
@pytest.mark.parametrize(
    "topic_id,params,code",
    [
        ("linalg_rank", {"matrix": "1, 2; 3"}, "linalg_ragged_matrix"),
        ("linalg_rank", {"matrix": ""}, "linalg_bad_matrix"),
        ("linalg_rank", {"matrix": "1, ; 2, 3"}, "linalg_bad_matrix"),
        ("linalg_det", {"matrix": "1, 2, 3; 4, 5, 6"}, "linalg_not_square"),
        ("linalg_inverse", {"matrix": "1, 2, 3; 4, 5, 6"}, "linalg_not_square"),
        ("linalg_lu", {"matrix": "1, 2, 3; 4, 5, 6"}, "linalg_not_square"),
        (
            "linalg_det",
            {"matrix": "1, 2, 3, 4, 5; 6, 7, 8, 9, 10; 1, 1, 1, 1, 1; "
                       "2, 2, 2, 2, 2; 3, 3, 3, 3, 3"},
            "linalg_too_large",
        ),
        (
            "linear_solve",
            {"matrix": "1, 2; 3, 4", "rhs": "1; 2; 3", "method": "gauss"},
            "linear_system_size_mismatch",
        ),
        (
            "linear_solve",
            {"matrix": "1, 2; 3, 4", "rhs": "1, 2; 3, 4", "method": "gauss"},
            "linear_system_bad_rhs",
        ),
    ],
)
def test_invalid_inputs_are_coded(
    engine: Engine, topic_id: str, params: dict, code: str
) -> None:
    response = topic(engine, topic_id, **params)
    assert not response["ok"]
    assert response["error_code"] == code


# ---------------------------------------------------------------- random sweep
def test_random_matrices_match_sympy(engine: Engine) -> None:
    rng = random.Random(20261121)
    for _ in range(16):
        size = rng.choice([2, 2, 3])
        matrix = sympy.Matrix(
            [[rng.randint(-4, 4) for _ in range(size)] for _ in range(size)]
        )
        response = topic(engine, "linalg_det", matrix=_request_text(matrix))
        assert response["ok"]
        value = _expr(_after_equals(response["solution"]["plain"]))
        assert sympy.simplify(value - matrix.det()) == 0

        if matrix.det() != 0:
            response = topic(engine, "linalg_inverse", matrix=_request_text(matrix))
            assert response["ok"]
            assert sympy.simplify(
                matrix * _inverse(response["solution"]["plain"]) - sympy.eye(size)
            ) == sympy.zeros(size)

        response = topic(engine, "linalg_lu", matrix=_request_text(matrix))
        assert response["ok"]
        factors = {
            name: _matrix(_after_equals(line))
            for name, line in zip(("L", "U", "P"), response["functions"])
        }
        assert sympy.simplify(
            factors["P"] * matrix - factors["L"] * factors["U"]
        ) == sympy.zeros(size)


# ---------------------------------------------------------------- vector spaces
def _vectors(text: str) -> list[sympy.Matrix]:
    return [_matrix(row) for row in re.findall(r"\[([^\[\]]*)\]", text)]


def test_span_membership_matches_sympy(engine: Engine) -> None:
    cases = [
        (sympy.Matrix([[1, 0], [0, 1]]), sympy.Matrix([3, 5])),
        (sympy.Matrix([[1, 2], [2, 4]]), sympy.Matrix([1, 1])),
        (sympy.Matrix([[1, 2], [2, 4]]), sympy.Matrix([3, 6])),
        (sympy.Matrix([[1, 2, 3], [0, 1, 4]]), sympy.Matrix([1, 1, 1])),
        (sympy.Matrix([[1, 2], [3, 4], [5, 6]]), sympy.Matrix([0, 0])),
        (sympy.Matrix([[1, 2], [3, 4], [5, 6]]), sympy.Matrix([1, 0])),
    ]
    for rows, target in cases:
        response = topic(
            engine,
            "linalg_span",
            vectors=_request_text(rows),
            target=", ".join(sympy.sstr(v) for v in target),
        )
        assert response["ok"], response
        columns = rows.T
        augmented = columns.row_join(target)
        member = augmented.rank() == columns.rank()
        assert (not response["warnings"]) is member, response
        assert _checks_ok(response)
        assert f"dim span = {columns.rank()}" in response["solution"]["plain"]


def test_span_basis_matches_sympy(engine: Engine) -> None:
    cases = [
        sympy.Matrix([[1, 2, 3], [2, 4, 6], [1, 1, 1]]),
        sympy.Matrix([[1, 0, 0], [0, 1, 0], [0, 0, 1]]),
        sympy.Matrix([[0, 0], [0, 0]]),
        sympy.Matrix([[1, 2], [2, 4]]),
    ]
    for rows in cases:
        response = topic(engine, "linalg_span", vectors=_request_text(rows))
        assert response["ok"], response
        assert f"dimension {rows.rank()}" in response["solution"]["plain"]
        assert _checks_ok(response)
        if rows.rank() == 0:
            assert response["functions"] == []


def test_independence_matches_sympy(engine: Engine) -> None:
    cases = [
        sympy.Matrix([[1, 2], [2, 4]]),
        sympy.Matrix([[1, 0], [0, 1]]),
        sympy.Matrix([[1, 2, 3], [2, 4, 6], [1, 1, 1]]),
        sympy.Matrix([[1, 0, 0], [0, 1, 0], [0, 0, 1]]),
        sympy.Matrix([[1, 1], [1, 1], [1, 1]]),
    ]
    for rows in cases:
        response = topic(engine, "linalg_independence", vectors=_request_text(rows))
        assert response["ok"], response
        independent = rows.rank() == rows.rows
        assert (not response["warnings"]) is independent, response
        assert _checks_ok(response)
        assert ("linearly independent" in response["solution"]["plain"]) is independent


def test_basis_is_independent_and_spans(engine: Engine) -> None:
    cases = [
        sympy.Matrix([[1, 2, 3], [2, 4, 6], [1, 1, 1]]),
        sympy.Matrix([[1, 0], [0, 1], [1, 1]]),
        sympy.Matrix([[1, 2], [2, 4]]),
        sympy.Matrix([[0, 0], [0, 0]]),
    ]
    for rows in cases:
        response = topic(engine, "linalg_basis", vectors=_request_text(rows))
        assert response["ok"], response
        assert f"dim = {rows.rank()}" in response["solution"]["plain"]
        assert _checks_ok(response)
        basis = [_matrix(line.split("=", 1)[1]) for line in response["functions"]]
        assert len(basis) == rows.rank()
        if basis:
            stacked = sympy.Matrix.hstack(*basis)
            assert stacked.rank() == rows.rank()
            assert rows.T.columnspace() == stacked.columnspace()


def test_coordinates_match_sympy(engine: Engine) -> None:
    cases = [
        (sympy.Matrix([[1, 0], [0, 1]]), sympy.Matrix([3, 5])),
        (sympy.Matrix([[1, 1], [0, 1]]), sympy.Matrix([3, 5])),
        (sympy.Matrix([[2, 1], [1, 1]]), sympy.Matrix([1, 0])),
        (sympy.Matrix([[1, 1, 0], [0, 1, 1], [0, 0, 1]]), sympy.Matrix([1, 2, 3])),
    ]
    for rows, vector in cases:
        response = topic(
            engine,
            "linalg_coordinates",
            basis=_request_text(rows),
            vector=", ".join(sympy.sstr(v) for v in vector),
        )
        assert response["ok"], response
        basis = rows.T
        expected = basis.LUsolve(vector)
        coordinates = [_expr(line.split("=", 1)[1]) for line in response["functions"]]
        assert len(coordinates) == len(expected)
        rebuilt = basis * sympy.Matrix(coordinates)
        assert sympy.simplify(rebuilt - vector) == sympy.zeros(vector.rows, 1)
        assert _checks_ok(response)


def test_change_of_basis_matches_sympy(engine: Engine) -> None:
    cases = [
        (sympy.Matrix([[1, 0], [0, 1]]), sympy.Matrix([[1, 1], [0, 1]])),
        (sympy.Matrix([[1, 1], [0, 1]]), sympy.Matrix([[1, 0], [0, 1]])),
        (sympy.Matrix([[1, 2], [3, 4]]), sympy.Matrix([[1, 0], [1, 1]])),
        (sympy.Matrix([[2, 0], [0, 3]]), sympy.Matrix([[1, 1], [1, -1]])),
    ]
    for basis_b, basis_c in cases:
        response = topic(
            engine,
            "linalg_change_basis",
            basis_b=_request_text(basis_b),
            basis_c=_request_text(basis_c),
        )
        assert response["ok"], response
        B = basis_b.T
        C = basis_c.T
        P = _matrix(response["functions"][0].split("=", 1)[1])
        assert sympy.simplify(C * P - B) == sympy.zeros(B.rows, B.cols)
        assert _checks_ok(response)


def test_subspace_axioms_are_verified(engine: Engine) -> None:
    for rows in (
        sympy.Matrix([[1, 0], [0, 1], [1, 1]]),
        sympy.Matrix([[1, 2], [2, 4]]),
        sympy.Matrix([[0, 0], [0, 0]]),
    ):
        response = topic(engine, "linalg_subspace", vectors=_request_text(rows))
        assert response["ok"], response
        assert f"dim W = {rows.rank()}" in response["solution"]["plain"]
        names = [check["name_key"] for check in response["checks"]]
        assert names == [
            "check.zero_is_in_the_subspace",
            "check.closed_under_addition",
            "check.closed_under_scalar_multiplication",
        ]
        assert _checks_ok(response)


def test_vector_space_symbolic_inputs(engine: Engine) -> None:
    a, b, c, d = sympy.symbols("a b c d")
    symbolic = sympy.Matrix([[a, b], [c, d]])
    for name in ("linalg_span", "linalg_independence", "linalg_basis"):
        response = topic(engine, name, vectors=_request_text(symbolic))
        assert response["ok"], response
        assert response["assumptions"] == ["assumption.generic_symbolic_pivots"]
        assert _checks_ok(response)


def test_vector_space_input_errors(engine: Engine) -> None:
    cases = [
        ("linalg_span", {"vectors": "1, 0; 0, 1", "target": "1, 2, 3"},
         "linalg_dimension_mismatch"),
        ("linalg_coordinates",
         {"basis": "1, 1; 2, 2", "vector": "3, 5"}, "linalg_not_a_basis"),
        ("linalg_coordinates",
         {"basis": "1, 0; 0, 1", "vector": "1, 2, 3"},
         "linalg_dimension_mismatch"),
        ("linalg_change_basis",
         {"basis_b": "1, 0; 0, 1", "basis_c": "1, 1; 2, 2"},
         "linalg_not_a_basis"),
    ]
    for name, params, code in cases:
        response = topic(engine, name, **params)
        assert not response["ok"]
        assert response["error_code"] == code


# ---------------------------------------------------------------- applications
def _tuple(text: str) -> sympy.Matrix:
    body = text.strip()
    assert body.startswith("(") and body.endswith(")"), body
    return sympy.Matrix([_expr(cell) for cell in body[1:-1].split(",")])


def _labelled_tuple(solution: str, label: str) -> sympy.Matrix:
    match = re.search(re.escape(label) + r"\s*=\s*\(([^)]*)\)", solution)
    assert match, (label, solution)
    return sympy.Matrix([_expr(cell) for cell in match.group(1).split(",")])


def test_projection_matches_sympy(engine: Engine) -> None:
    cases = [
        (sympy.Matrix([[1, 0], [0, 1]]), sympy.Matrix([3, 4])),
        (sympy.Matrix([[1, 1], [1, -1]]), sympy.Matrix([3, 4])),
        (sympy.Matrix([[1, 1, 0], [0, 1, 1]]), sympy.Matrix([1, 2, 3])),
        (sympy.Matrix([[1, 2, 0], [2, 1, 1], [0, 1, 2]]),
         sympy.Matrix([5, 1, -2])),
    ]
    for rows, vector in cases:
        response = topic(
            engine,
            "linalg_projection",
            basis=_request_text(rows),
            vector=", ".join(sympy.sstr(v) for v in vector),
        )
        assert response["ok"], response
        basis = rows.T
        expected = basis * (basis.T * basis).inv() * basis.T * vector
        projection = _labelled_tuple(response["solution"]["plain"], "proj_W(v)")
        assert sympy.simplify(projection - expected) == sympy.zeros(vector.rows, 1)
        residual = _labelled_tuple(
            response["solution"]["plain"], "v - proj_W(v)"
        )
        assert sympy.simplify(residual - (vector - expected)) == sympy.zeros(
            vector.rows, 1
        )
        assert _checks_ok(response)


def test_projection_rejects_a_dependent_basis(engine: Engine) -> None:
    response = topic(
        engine, "linalg_projection", basis="1, 2; 2, 4", vector="3, 4"
    )
    assert not response["ok"]
    assert response["error_code"] == "linalg_dependent_basis"


def test_projection_symbolic_input_is_generic(engine: Engine) -> None:
    a, b, c, d = sympy.symbols("a b c d")
    response = topic(
        engine,
        "linalg_projection",
        basis=_request_text(sympy.Matrix([[a, b], [c, d]])),
        vector="1, 2",
    )
    assert response["ok"], response
    assert response["assumptions"] == ["assumption.generic_symbolic_pivots"]
    assert response["solution"]["plain"].startswith("proj_W(v) = (1, 2)")
    assert "v - proj_W(v) = (0, 0)" in response["solution"]["plain"]
    assert response["solution"]["plain"].endswith("distance = 0")
    assert _checks_ok(response)


def test_gram_schmidt_matches_sympy(engine: Engine) -> None:
    cases = [
        sympy.Matrix([[3, 4], [1, 0]]),
        sympy.Matrix([[1, 1, 0], [1, 0, 1], [0, 1, 1]]),
        sympy.Matrix([[1, 0], [0, 1]]),
        sympy.Matrix([[2, 1, 0], [1, 2, 1]]),
    ]
    for rows in cases:
        response = topic(engine, "linalg_gram_schmidt", vectors=_request_text(rows))
        assert response["ok"], response
        orthogonal: list[sympy.Matrix] = []
        for row in rows.tolist():
            vector = sympy.Matrix(row)
            for previous in orthogonal:
                vector -= (
                    previous * (previous.dot(vector) / previous.dot(previous))
                )
            assert sympy.simplify(vector) != sympy.zeros(len(row), 1)
            orthogonal.append(vector)
        for index, vector in enumerate(orthogonal, start=1):
            parsed = _matrix(response["functions"][2 * (index - 1)].split("=", 1)[1])
            assert sympy.simplify(parsed - vector) == sympy.zeros(len(vector), 1)
            unit = _matrix(response["functions"][2 * (index - 1) + 1].split("=", 1)[1])
            assert sympy.simplify(unit.T * unit) == sympy.eye(1)
            assert sympy.simplify(unit - vector / vector.norm()) == sympy.zeros(
                len(vector), 1
            )
        assert len(response["functions"]) == 2 * len(orthogonal)
        assert _checks_ok(response)


def test_gram_schmidt_drops_dependent_vectors(engine: Engine) -> None:
    response = topic(engine, "linalg_gram_schmidt", vectors="1, 2; 2, 4")
    assert response["ok"]
    assert response["warnings"] == ["warning.linearly_dependent"]
    assert len(response["functions"]) == 2
    assert _checks_ok(response)


def test_qr_matches_sympy(engine: Engine) -> None:
    cases = [
        sympy.Matrix([[1, 1], [1, 0], [0, 1]]),
        sympy.Matrix([[1, 2], [3, 4]]),
        sympy.Matrix([[2, 1, 0], [1, 2, 1], [0, 1, 2], [1, 0, 1]]),
    ]
    for matrix in cases:
        response = topic(engine, "linalg_qr", matrix=_request_text(matrix))
        assert response["ok"], response
        assert [line.split("=", 1)[0].strip() for line in response["functions"]] == [
            "Q",
            "R",
        ]
        Q = _matrix(response["functions"][0].split("=", 1)[1])
        R = _matrix(response["functions"][1].split("=", 1)[1])
        assert sympy.simplify(Q.T * Q - sympy.eye(Q.cols)) == sympy.zeros(
            Q.cols, Q.cols
        )
        assert sympy.simplify(Q * R - matrix) == sympy.zeros(matrix.rows, matrix.cols)
        assert all(
            sympy.simplify(R[i, j]) == 0 for i in range(R.rows) for j in range(i)
        )
        assert _checks_ok(response)


def test_qr_rejects_dependent_columns(engine: Engine) -> None:
    response = topic(engine, "linalg_qr", matrix="1, 2; 2, 4")
    assert not response["ok"]
    assert response["error_code"] == "linalg_not_full_column_rank"


def test_least_squares_matches_sympy(engine: Engine) -> None:
    cases = [
        (sympy.Matrix([[1, 0], [0, 1], [1, 1]]), sympy.Matrix([1, 2, 4])),
        (sympy.Matrix([[1, 1], [1, 2], [1, 3]]), sympy.Matrix([1, 2, 2])),
        (sympy.Matrix([[1, 1], [1, 0], [0, 1]]), sympy.Matrix([2, 1, 3])),
        (sympy.Matrix([[1, 0], [0, 1]]), sympy.Matrix([5, -2])),
    ]
    for matrix, rhs in cases:
        response = topic(
            engine,
            "linalg_least_squares",
            matrix=_request_text(matrix),
            rhs="; ".join(sympy.sstr(v) for v in rhs),
        )
        assert response["ok"], response
        expected = (matrix.T * matrix).inv() * matrix.T * rhs
        solution = _labelled_tuple(response["solution"]["plain"], "x")
        assert sympy.simplify(solution - expected) == sympy.zeros(expected.rows, 1)
        residual = _labelled_tuple(response["solution"]["plain"], "residual")
        assert sympy.simplify(residual - (rhs - matrix * expected)) == sympy.zeros(
            rhs.rows, 1
        )
        assert _checks_ok(response)


def test_least_squares_rejects_a_rank_deficient_matrix(engine: Engine) -> None:
    response = topic(
        engine, "linalg_least_squares", matrix="1, 1; 1, 1", rhs="1; 3"
    )
    assert not response["ok"]
    assert response["error_code"] == "linalg_not_full_column_rank"


def test_diagonalization_matches_sympy(engine: Engine) -> None:
    cases = [
        sympy.Matrix([[2, 1], [1, 2]]),
        sympy.Matrix([[4, 1], [2, 3]]),
        sympy.Matrix([[1, 2, 3], [0, 4, 5], [0, 0, 6]]),
        sympy.Matrix([[2, 0, 0], [0, 2, 0], [0, 0, 3]]),
    ]
    for matrix in cases:
        response = topic(engine, "linalg_diagonalize", matrix=_request_text(matrix))
        assert response["ok"], response
        assert [line.split("=", 1)[0].strip() for line in response["functions"]] == [
            "P",
            "D",
        ]
        P = _matrix(response["functions"][0].split("=", 1)[1])
        D = _matrix(response["functions"][1].split("=", 1)[1])
        assert sympy.simplify(P.inv() * matrix * P - D) == sympy.zeros(
            matrix.rows, matrix.cols
        )
        assert all(
            sympy.simplify(D[i, j]) == 0
            for i in range(D.rows)
            for j in range(D.cols)
            if i != j
        )
        eigenvalues = [
            value for value, count in matrix.eigenvals().items() for _ in range(count)
        ]
        assert sorted(D.diagonal().tolist()[0]) == sorted(eigenvalues)
        assert _checks_ok(response)


def test_diagonalization_reports_failures(engine: Engine) -> None:
    cases = [
        ("1, 1; 0, 1", "linalg_not_diagonalizable"),
        ("0, -1; 1, 0", "linalg_eigenvalues_not_rational"),
        ("a, b; c, d", "linalg_numeric_required"),
    ]
    for matrix, code in cases:
        response = topic(engine, "linalg_diagonalize", matrix=matrix)
        assert not response["ok"]
        assert response["error_code"] == code


def test_matrix_power_matches_sympy(engine: Engine) -> None:
    cases = [
        (sympy.Matrix([[2, 1], [1, 2]]), 3),
        (sympy.Matrix([[1, 1], [0, 1]]), 4),
        (sympy.Matrix([[2, 0], [0, 3]]), -2),
        (sympy.Matrix([[1, 1], [1, 0]]), 5),
    ]
    for matrix, power in cases:
        response = topic(
            engine,
            "linalg_power",
            matrix=_request_text(matrix),
            power=str(power),
        )
        assert response["ok"], response
        result = _matrix(_after_equals(response["solution"]["plain"]))
        assert sympy.simplify(result - matrix**power) == sympy.zeros(
            matrix.rows, matrix.cols
        )
        if response["checks"]:
            assert _checks_ok(response)


def test_matrix_power_input_errors(engine: Engine) -> None:
    cases = [
        ("1, 1; 1, 1", "-1", "linalg_singular_matrix"),
        ("1, 1; 1, 1", "99", "linalg_bad_power"),
        ("1, 1; 1, 1", "abc", "linalg_bad_power"),
    ]
    for matrix, power, code in cases:
        response = topic(engine, "linalg_power", matrix=matrix, power=power)
        assert not response["ok"]
        assert response["error_code"] == code



# ---------------------------------------------------------------- extended chapters
def _matrix_after(text: str, label: str) -> sympy.Matrix:
    part = text.split(label, 1)[1]
    start = part.index("[[")
    end = part.index("]]", start) + 2
    return _engine_matrix(part[start:end])


def _engine_matrix(text: str) -> sympy.Matrix:
    start = text.index("[[")
    end = text.rindex("]]")
    body = text[start + 2 : end]
    rows = re.split(r"\];\s*\[", body)
    return sympy.Matrix([[_expr(cell) for cell in row.split(",")] for row in rows])


def _vectors(text: str) -> list[list[sympy.Expr]]:
    return [
        [_expr(part) for part in group.split(",")]
        for group in re.findall(r"\(([^()]*)\)", text)
    ]


def _complex(text: str) -> complex:
    cleaned = text.replace(" ", "")
    cleaned = re.sub(r"(?<=[\d.])i", "*I", cleaned)
    cleaned = cleaned.replace("i", "I")
    return complex(sympy.sympify(cleaned))


def test_orthogonal_complement_matches_sympy(engine: Engine) -> None:
    matrix = sympy.Matrix([[1, 2], [2, 4], [3, 6]])
    response = topic(engine, "linalg_orthogonal_complement",
                     matrix="1, 2; 2, 4; 3, 6")
    assert response["ok"], response
    plain = response["solution"]["plain"]
    assert f"dim = {matrix.rows - matrix.rank()}" in plain
    basis = [_engine_matrix(fn.split("=", 1)[1]) for fn in response["functions"]]
    assert len(basis) == matrix.rows - matrix.rank()
    for vector in basis:
        assert vector.shape == (matrix.rows, 1)
        assert matrix.T * vector == sympy.zeros(matrix.cols, 1)


def test_graph_incidence_matches_sympy(engine: Engine) -> None:
    response = topic(engine, "linalg_graph", nodes="3", edges="1-2, 2-3, 3-1")
    assert response["ok"], response
    incidence = sympy.Matrix([[1, -1, 0], [-1, 0, 1], [0, 1, -1]])
    plain = response["solution"]["plain"]
    assert "components = 1" in plain
    assert f"rank(B) = {incidence.rank()}" in plain
    assert f"cycle dimension = {incidence.cols - incidence.rank()}" in plain


def test_fft_matches_sympy(engine: Engine) -> None:
    signal = [1, 2, 3, 4]
    response = topic(engine, "linalg_fft", signal="1, 2, 3, 4")
    assert response["ok"], response
    plain = response["solution"]["plain"]
    expected = [
        sum(signal[j] * sympy.exp(-2 * sympy.pi * sympy.I * k * j / 4)
            for j in range(4))
        for k in range(4)
    ]
    for k, value in enumerate(expected):
        match = re.search(rf"X_{k} = ([^,]+)", plain)
        assert match, plain
        assert abs(_complex(match.group(1)) - complex(value)) < 1e-6


def test_similarity_matches_sympy(engine: Engine) -> None:
    a = sympy.Matrix([[1, 2], [3, 4]])
    p = sympy.Matrix([[1, 1], [0, 1]])
    response = topic(engine, "linalg_similarity",
                     matrix="1, 2; 3, 4", change="1, 1; 0, 1")
    assert response["ok"], response
    b = _engine_matrix(response["solution"]["plain"])
    assert b == p.inv() * a * p


def test_matrix_exponential_matches_sympy(engine: Engine) -> None:
    t = sympy.symbols("t")
    cases = [
        ("0, 1; -1, 0", sympy.Matrix([[0, 1], [-1, 0]])),
        ("1, 1; 0, 1", sympy.Matrix([[1, 1], [0, 1]])),
    ]
    for text, matrix in cases:
        response = topic(engine, "linalg_matrix_exp", matrix=text)
        assert response["ok"], response
        engine_matrix = _engine_matrix(response["solution"]["plain"])
        expected = (matrix * t).exp()
        difference = engine_matrix - expected
        for entry in difference:
            assert abs(complex(sympy.N(entry.subs(t, sympy.Rational(1, 3))))) < 1e-8


def test_cholesky_matches_sympy(engine: Engine) -> None:
    matrix = sympy.Matrix([[4, 2], [2, 3]])
    response = topic(engine, "linalg_positive_definite", matrix="4, 2; 2, 3")
    assert response["ok"], response
    plain = response["solution"]["plain"]
    factor = _engine_matrix(plain.split("L = ", 1)[1])
    assert sympy.simplify(factor * factor.T - matrix) == sympy.zeros(2, 2)
    bad = topic(engine, "linalg_positive_definite", matrix="1, 2; 2, 1")
    assert not bad["ok"] and bad["error_code"] == "linalg_not_positive_definite"


def test_svd_matches_sympy(engine: Engine) -> None:
    matrix = sympy.Matrix([[1, 2, 3], [4, 5, 6]])
    response = topic(engine, "linalg_svd", matrix="1, 2, 3; 4, 5, 6")
    assert response["ok"], response
    plain = response["solution"]["plain"]
    reported = [float(x) for x in re.findall(r"sigma_\d = ([0-9.]+)", plain)]
    gram = matrix.H * matrix
    expected = sorted(
        (sympy.sqrt(sympy.N(value)) for value in gram.eigenvals()), reverse=True
    )[: min(matrix.rows, matrix.cols)]
    assert len(reported) == len(expected)
    for got, want in zip(reported, expected):
        assert abs(got - float(want)) < 1e-5
    functions = " ".join(response["functions"])
    u = _matrix_after(functions, "U = ")
    sigma = _matrix_after(functions, "S = ")
    v = _matrix_after(functions, "V = ")
    rebuilt = u * sigma * v.T
    assert all(abs(float(sympy.N(rebuilt[i, j] - matrix[i, j]))) < 1e-6
               for i in range(2) for j in range(3))


def test_norms_match_sympy(engine: Engine) -> None:
    matrix = sympy.Matrix([[1, 2, 3], [4, 5, 6]])
    response = topic(engine, "linalg_norm", matrix="1, 2, 3; 4, 5, 6")
    assert response["ok"], response
    plain = response["solution"]["plain"]
    one = max(sum(abs(matrix[i, j]) for i in range(matrix.rows))
              for j in range(matrix.cols))
    infinity = max(sum(abs(matrix[i, j]) for j in range(matrix.cols))
                   for i in range(matrix.rows))
    spectral = max(sympy.N(v) for v in matrix.singular_values())
    assert f"||A||_1 = {one}" in plain
    assert f"||A||_inf = {infinity}" in plain
    match = re.search(r"\|\|A\|\|_2 = ([0-9.]+)", plain)
    assert match and abs(float(match.group(1)) - float(spectral)) < 1e-4


def test_power_iteration_matches_sympy(engine: Engine) -> None:
    matrix = sympy.Matrix([[2, 1], [1, 2]])
    response = topic(engine, "linalg_power_iteration",
                     matrix="2, 1; 1, 2", start="1, 0")
    assert response["ok"], response
    match = re.search(r"lambda_1 = ([0-9.]+)", response["solution"]["plain"])
    assert match
    expected = max(matrix.eigenvals())
    assert abs(float(match.group(1)) - float(sympy.N(expected))) < 1e-6
    assert all(check["ok"] for check in response["checks"])


def test_qr_algorithm_matches_sympy(engine: Engine) -> None:
    matrix = sympy.Matrix([[4, 1], [1, 4]])
    response = topic(engine, "linalg_qr_algorithm", matrix="4, 1; 1, 4")
    assert response["ok"], response
    reported = sorted(float(x) for x in re.findall(r"lambda_\d = ([0-9.]+)",
                                                   response["solution"]["plain"]))
    expected = sorted(float(sympy.N(v)) for v in matrix.eigenvals())
    assert len(reported) == len(expected)
    for got, want in zip(reported, expected):
        assert abs(got - want) < 1e-5
    assert not topic(engine, "linalg_qr_algorithm",
                     matrix="1, 2; 3, 4")["ok"]


def test_iterative_solvers_match_sympy(engine: Engine) -> None:
    a = sympy.Matrix([[4, 1], [1, 3]])
    b = sympy.Matrix([1, 2])
    exact = a.solve(b)
    for method in ("jacobi", "gauss_seidel"):
        response = topic(engine, "linalg_iterative",
                         matrix="4, 1; 1, 3", rhs="1, 2", method=method)
        assert response["ok"], response
        values = _vectors(response["solution"]["plain"])[0]
        for got, want in zip(values, exact):
            assert abs(float(sympy.N(got - want))) < 1e-9
    diverged = topic(engine, "linalg_iterative", matrix="1, 2; 2, 1",
                     rhs="3, 3")
    assert diverged["ok"]
    assert "warning.iterative_diverged" in diverged["warnings"]


def test_linear_program_matches_the_vertices(engine: Engine) -> None:
    constraints = [(1, 1, 4), (1, 0, 2), (0, 1, 3)]
    response = topic(engine, "linalg_lp", objective="3, 2",
                     constraints="1, 1; 1, 0; 0, 1", bounds="4, 2, 3")
    assert response["ok"], response
    best = None
    for x in range(0, 8):
        for y in range(0, 8):
            if all(a * x + b * y <= rhs for a, b, rhs in constraints):
                value = 3 * x + 2 * y
                best = value if best is None else max(best, value)
    assert best == 10
    assert "z = 10" in response["solution"]["plain"]
    unbounded = topic(engine, "linalg_lp", objective="1, 1",
                      constraints="-1, 0; 0, -1", bounds="1, 1")
    assert unbounded["error_code"] == "linalg_lp_unbounded"


def test_matrix_game_value(engine: Engine) -> None:
    payoff = sympy.Matrix([[3, 0], [0, 2]])
    response = topic(engine, "linalg_game", matrix="3, 0; 0, 2")
    assert response["ok"], response
    plain = response["solution"]["plain"]
    value = float(re.search(r"value = ([0-9.]+)", plain).group(1))
    a, b, c, d = payoff[0, 0], payoff[0, 1], payoff[1, 0], payoff[1, 1]
    expected = (a * d - b * c) / (a + d - b - c)
    assert abs(value - float(expected)) < 1e-6
    saddle = topic(engine, "linalg_game", matrix="3, 1; 2, 1")
    assert saddle["ok"], saddle
    assert "row strategy = (1, 0)" in saddle["solution"]["plain"]
    assert "column strategy = (0, 1)" in saddle["solution"]["plain"]


def test_spectral_theorem_matches_sympy(engine: Engine) -> None:
    matrix = sympy.Matrix([[2, 1], [1, 2]])
    response = topic(engine, "linalg_symmetric", matrix="2, 1; 1, 2")
    assert response["ok"], response
    functions = " ".join(response["functions"])
    q = _matrix_after(functions, "Q = ")
    d = _matrix_after(functions, "D = ")
    assert all(abs(float(sympy.N((q.T * q)[i, j] - (1 if i == j else 0))))
               < 1e-7 for i in range(2) for j in range(2))
    rebuilt = q * d * q.T
    assert all(abs(float(sympy.N(rebuilt[i, j] - matrix[i, j]))) < 1e-7
               for i in range(2) for j in range(2))

# ---------------------------------------------------------------- catalogue
def test_new_keys_resolve() -> None:
    catalogue = ROOT / "app" / "assets" / "i18n"
    english = json.loads((catalogue / "en.json").read_text(encoding="utf-8"))
    persian = json.loads((catalogue / "fa.json").read_text(encoding="utf-8"))
    for key in (
        "nav.group.linalg",
        "topic.linalg_rank.title",
        "topic.linalg_det.title",
        "topic.linalg_inverse.title",
        "topic.linalg_lu.title",
        "topic.linalg_span.title",
        "topic.linalg_independence.title",
        "topic.linalg_basis.title",
        "topic.linalg_coordinates.title",
        "topic.linalg_change_basis.title",
        "topic.linalg_subspace.title",
        "step.title.write_the_vectors",
        "step.title.find_a_dependence_relation",
        "step.title.build_the_transition_matrix",
        "check.dependence_relation_vanishes",
        "check.transition_matrix_maps_b_to_c",
        "warning.not_in_the_span",
        "error.linalg_not_a_basis",
        "step.title.identify_the_pivots",
        "step.title.find_the_null_space",
        "step.title.find_the_cofactors",
        "step.title.build_l_and_u",
        "check.null_space_vector_satisfies_a_v_0",
        "check.lu_reconstructs_the_matrix",
        "warning.rank_deficient",
        "error.linalg_not_square",
        "topic.linalg_orthogonal_complement.title",
        "topic.linalg_graph.title",
        "topic.linalg_fft.title",
        "topic.linalg_similarity.title",
        "topic.linalg_matrix_exp.title",
        "topic.linalg_positive_definite.title",
        "topic.linalg_svd.title",
        "topic.linalg_norm.title",
        "topic.linalg_power_iteration.title",
        "topic.linalg_qr_algorithm.title",
        "topic.linalg_iterative.title",
        "topic.linalg_lp.title",
        "topic.linalg_game.title",
        "topic.linalg_symmetric.title",
        "study.la_fields.title",
        "study.la_vector_spaces.title",
        "study.la_transformations.title",
        "study.la_determinants.title",
        "study.la_canonical.title",
        "study.la_inner_products.title",
        "study.section.linalg",
        "study.group.la_foundations",
        "study.group.la_structure",
        "study.group.la_geometry",
        "linalg.home.overview",
        "linalg.home.topics",
        "linalg.home.articles",
        "error.linalg_not_symmetric",
        "error.linalg_not_positive_definite",
        "error.linalg_fft_length",
        "error.linalg_lp_unbounded",
        "error.linalg_lp_infeasible",
        "error.linalg_lp_negative_rhs",
        "warning.iterative_diverged",
        "warning.power_iteration_not_converged",
        "warning.qr_not_converged",
        "step.title.build_cholesky",
        "step.title.compute_the_singular_values",
        "step.title.compute_the_pseudoinverse",
        "step.title.qr_iteration",
        "step.title.build_the_tableau",
        "step.title.find_the_saddle_point",
        "check.svd_reconstructs_the_matrix",
        "check.pseudoinverse_satisfies_the_penrose_identity",
        "check.cholesky_reconstructs_the_matrix",
        "check.simplex_vertex_is_feasible",
        "topic.linalg_projection.title",
        "topic.linalg_gram_schmidt.title",
        "topic.linalg_qr.title",
        "topic.linalg_least_squares.title",
        "topic.linalg_diagonalize.title",
        "topic.linalg_power.title",
        "topic.linalg_projection.field.basis.label",
        "topic.linalg_least_squares.field.rhs.label",
        "topic.linalg_power.field.power.label",
        "step.title.build_the_gram_matrix",
        "step.title.orthogonalize_each_vector",
        "step.title.normalize_the_basis",
        "step.title.build_q_and_r",
        "step.title.solve_the_normal_equations",
        "step.title.build_the_diagonal_factorization",
        "step.title.use_the_diagonalization",
        "check.residual_is_orthogonal_to_the_basis",
        "check.qr_reconstructs_the_matrix",
        "check.normal_equations_hold",
        "check.matrix_equals_p_d_p_inverse",
        "check.power_matches_direct_multiplication",
        "error.linalg_dependent_basis",
        "error.linalg_not_full_column_rank",
        "error.linalg_numeric_required",
        "error.linalg_not_diagonalizable",
        "error.linalg_eigenvalues_not_rational",
        "error.linalg_bad_power",
        "error.linalg_singular_matrix",
    ):
        assert key in english and key in persian
