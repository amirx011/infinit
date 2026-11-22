"""Randomised oracle suite for every linear-algebra workbench.

The suite is deliberately independent of the engine's own intermediate text:
SymPy recomputes each answer from the definition.  Every call also audits the
worked solution: each step must carry a ``step.note.*`` key that resolves in
both the English and the Persian catalogue, which keeps the prose contract
under test alongside the mathematics.
"""

from __future__ import annotations

import ctypes
import json
import math
import pathlib
import random
import re

import pytest
import sympy

ROOT = pathlib.Path(__file__).resolve().parents[2]
DEFAULT_LIB = ROOT / "core" / "build" / "lib" / "libodecore.so"
CATALOGUE = ROOT / "app" / "assets" / "i18n"

EN = json.loads((CATALOGUE / "en.json").read_text(encoding="utf-8"))
FA = json.loads((CATALOGUE / "fa.json").read_text(encoding="utf-8"))

CASES = 0


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


def _audit_steps(response: dict) -> None:
    """Every linear-algebra step must be explained in both languages."""
    for step in response.get("steps") or []:
        key = step.get("note_key") or ""
        assert key.startswith("step.note."), (response.get("type_id"), key)
        assert key in EN, key
        assert key in FA, key
        assert EN[key].strip(), key
        assert FA[key].strip(), key


def call(engine: Engine, name: str, **params) -> dict:
    global CASES
    CASES += 1
    response = topic(engine, name, **params)
    _audit_steps(response)
    return response


# ---------------------------------------------------------------- parsing
def _expr(text: str) -> sympy.Expr:
    text = text.strip().replace("−", "-").replace("·", "*")
    text = re.sub(r"√\(([^)]*)\)", r"sqrt(\1)", text)
    return sympy.sympify(text)


def _matrix(text: str) -> sympy.Matrix:
    rows = re.findall(r"\[([^\[\]]*)\]", text.strip())
    if not rows:
        raise AssertionError(f"not a matrix: {text!r}")
    return sympy.Matrix(
        [[_expr(cell) for cell in row.split(",")] for row in rows]
    )


def _vec(text: str) -> sympy.Matrix:
    body = text.strip()
    if body.startswith("(") and body.endswith(")"):
        return sympy.Matrix([_expr(part) for part in body[1:-1].split(",")])
    matrix = _matrix(body)
    return matrix.T if matrix.rows == 1 else matrix


def _tuple(text: str) -> list[sympy.Expr]:
    body = text.strip()
    assert body.startswith("(") and body.endswith(")"), text
    return [_expr(part) for part in body[1:-1].split(",")]


def _text(matrix: sympy.Matrix) -> str:
    matrix = sympy.Matrix(matrix)
    return "; ".join(
        ", ".join(sympy.sstr(matrix[i, j]) for j in range(matrix.cols))
        for i in range(matrix.rows)
    )


def _functions(response: dict) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in response.get("functions") or []:
        name, _, body = line.partition("=")
        values[name.strip()] = body.strip()
    return values


def _same(left, right, tol: float | None = None) -> bool:
    left = sympy.Matrix(left)
    right = sympy.Matrix(right)
    if left.shape != right.shape:
        return False
    for i in range(left.rows):
        for j in range(left.cols):
            if tol is None:
                if sympy.simplify(left[i, j] - right[i, j]) != 0:
                    return False
            else:
                value = complex(sympy.N(left[i, j] - right[i, j]))
                if abs(value.real) > tol or abs(value.imag) > tol:
                    return False
    return True


def _zero(expr) -> bool:
    return sympy.simplify(expr) == 0


def _num(expr) -> float:
    return float(sympy.N(expr))


def _close(left, right, tol: float = 1e-6) -> bool:
    return abs(_num(left) - _num(right)) <= tol


def _rand_matrix(rng: random.Random, rows: int, cols: int, lo: int = -4,
                 hi: int = 4) -> sympy.Matrix:
    return sympy.Matrix(
        [[rng.randint(lo, hi) for _ in range(cols)] for _ in range(rows)]
    )


def _invertible(rng: random.Random, n: int, lo: int = -4,
                hi: int = 4) -> sympy.Matrix:
    while True:
        matrix = _rand_matrix(rng, n, n, lo, hi)
        if matrix.det() != 0:
            return matrix


def _unimodular(rng: random.Random, n: int) -> sympy.Matrix:
    matrix = sympy.eye(n)
    for _ in range(3 * n):
        i, j = rng.sample(range(n), 2)
        k = rng.choice([-2, -1, 1, 2])
        row = [matrix[i, t] + k * matrix[j, t] for t in range(n)]
        for t in range(n):
            matrix[i, t] = row[t]
    return matrix


def _eigenvalues(matrix: sympy.Matrix) -> list[float]:
    values = []
    for value, mult in matrix.eigenvals().items():
        values.extend([complex(sympy.N(value)).real] * mult)
    return sorted(values, reverse=True)


def _singular_values(matrix: sympy.Matrix) -> list[float]:
    matrix = sympy.Matrix(matrix)
    gram = matrix.T * matrix if matrix.rows >= matrix.cols else matrix * matrix.T
    values = []
    for value, mult in gram.eigenvals().items():
        values.extend(
            [math.sqrt(max(complex(sympy.N(value)).real, 0.0))] * mult
        )
    return sorted(values, reverse=True)


def _incidence(nodes: int, edges: list[tuple[int, int]]) -> sympy.Matrix:
    matrix = sympy.zeros(len(edges), nodes)
    for row, (a, b) in enumerate(edges):
        matrix[row, a - 1] = 1
        matrix[row, b - 1] = -1
    return matrix


def _combination_coefficients(text: str, count: int) -> list[sympy.Expr]:
    coefficients = [sympy.Integer(0)] * count
    for match in re.finditer(r"([+-]?)\s*(?:([^+\-\s]+)\s+)?v_(\d+)", text):
        sign = -1 if match.group(1) == "-" else 1
        magnitude = match.group(2)
        value = sympy.Integer(1) if magnitude is None else _expr(magnitude)
        coefficients[int(match.group(3)) - 1] += sign * value
    return coefficients


def _inverse_from_solution(solution: str) -> sympy.Matrix | None:
    if "does not exist" in solution:
        return None
    body = solution.split("=", 1)[1].strip()
    if " * " in body:
        factor, rest = body.split(" * ", 1)
        return _expr(factor) * _matrix(rest)
    return _matrix(body)


def _lp_optimum(objective, constraints, bounds):
    rows = [list(map(sympy.Rational, row)) for row in constraints]
    rows += [[-1, 0], [0, -1]]
    rhs = [sympy.Rational(value) for value in bounds] + [0, 0]
    best = None
    for i in range(len(rows)):
        for j in range(i + 1, len(rows)):
            block = sympy.Matrix([rows[i], rows[j]])
            if block.det() == 0:
                continue
            point = block.inv() * sympy.Matrix([rhs[i], rhs[j]])
            if any(
                sum(rows[k][t] * point[t] for t in range(2)) > rhs[k]
                for k in range(len(rows))
            ):
                continue
            value = sum(sympy.Rational(objective[t]) * point[t] for t in range(2))
            if best is None or value > best:
                best = value
    return best


def _game_value(matrix: sympy.Matrix):
    a = [[sympy.Rational(matrix[i, j]) for j in range(2)] for i in range(2)]
    maximin = max(min(a[0]), min(a[1]))
    minimax = min(max(a[0][0], a[1][0]), max(a[0][1], a[1][1]))
    if maximin == minimax:
        return maximin
    denom = a[0][0] + a[1][1] - a[0][1] - a[1][0]
    if denom == 0:
        return None
    return (a[0][0] * a[1][1] - a[0][1] * a[1][0]) / denom


# ---------------------------------------------------------------- rank
def test_rank(engine: Engine) -> None:
    rng = random.Random(1101)
    sizes = [(1, 1), (1, 3), (3, 1), (2, 2), (2, 3), (3, 2), (3, 3), (2, 4),
             (4, 2), (3, 4), (4, 4)]
    for rows, cols in sizes:
        for trial in range(6):
            matrix = _rand_matrix(rng, rows, cols)
            if trial == 1 and rows > 1:
                matrix[rows - 1, :] = matrix[0, :]
            if trial == 2:
                matrix = sympy.zeros(rows, cols)
            response = call(engine, "linalg_rank", matrix=_text(matrix))
            assert response["ok"], response
            rank = matrix.rank()
            nullity = cols - rank
            assert response["solution"]["plain"] == (
                f"rank(A) = {rank},  nullity(A) = {nullity}"
            )
            functions = _functions(response)
            assert len(functions) == nullity
            vectors = [_vec(body) for body in functions.values()]
            for vector in vectors:
                assert _same(matrix * vector, sympy.zeros(rows, 1))
            if vectors:
                assert sympy.Matrix.hstack(*vectors).rank() == nullity

    a, b = sympy.symbols("a b")
    symbolic = [
        (sympy.Matrix([[a, 0], [0, b]]), 2),
        (sympy.Matrix([[a, 0], [0, 0]]), 1),
        (sympy.Matrix([[a, b], [2 * a, 2 * b]]), 1),
    ]
    for matrix, expected in symbolic:
        response = call(engine, "linalg_rank", matrix=_text(matrix))
        assert response["ok"], response
        assert f"rank(A) = {expected}" in response["solution"]["plain"]


# ---------------------------------------------------------------- determinant
def test_determinant(engine: Engine) -> None:
    rng = random.Random(1102)
    for size in (1, 2, 3, 4):
        for _ in range(8):
            matrix = _rand_matrix(rng, size, size)
            response = call(engine, "linalg_det", matrix=_text(matrix))
            assert response["ok"], response
            value = _expr(response["solution"]["plain"].split("=", 1)[1])
            assert _zero(value - matrix.det())

    for size in (2, 3, 4):
        for _ in range(3):
            matrix = _rand_matrix(rng, size, size)
            matrix[size - 1, :] = matrix[0, :]
            response = call(engine, "linalg_det", matrix=_text(matrix))
            assert response["ok"], response
            assert _zero(_expr(response["solution"]["plain"].split("=", 1)[1]))
            assert "warning.singular_matrix" in response["warnings"]

    a, b, c, d = sympy.symbols("a b c d")
    symbolic = [
        sympy.Matrix([[a, b], [c, d]]),
        sympy.Matrix([[a, b], [0, d]]),
        sympy.Matrix([[a, 0, 0], [0, b, 0], [0, 0, c]]),
        sympy.Matrix([[a, b, 0], [0, c, d], [0, 0, 1]]),
    ]
    for matrix in symbolic:
        response = call(engine, "linalg_det", matrix=_text(matrix))
        assert response["ok"], response
        value = _expr(response["solution"]["plain"].split("=", 1)[1])
        assert _zero(value - matrix.det())


# ---------------------------------------------------------------- inverse
def test_inverse(engine: Engine) -> None:
    rng = random.Random(1103)
    for size in (2, 3, 4):
        for _ in range(10):
            matrix = _invertible(rng, size)
            response = call(engine, "linalg_inverse", matrix=_text(matrix))
            assert response["ok"], response
            inverse = _inverse_from_solution(response["solution"]["plain"])
            assert inverse is not None
            assert _same(matrix * inverse, sympy.eye(size))

    for matrix in (sympy.Matrix([[1, 2], [2, 4]]),
                   sympy.Matrix([[1, 2, 3], [2, 4, 6], [1, 1, 1]]),
                   sympy.zeros(2, 2)):
        response = call(engine, "linalg_inverse", matrix=_text(matrix))
        assert response["ok"], response
        assert _inverse_from_solution(response["solution"]["plain"]) is None
        assert "warning.singular_matrix" in response["warnings"]

    a, b, c, d = sympy.symbols("a b c d")
    symbolic = [
        sympy.Matrix([[a, 0], [0, b]]),
        sympy.Matrix([[a, b], [c, d]]),
        sympy.Matrix([[a, 1, 0], [0, b, 0], [0, 0, c]]),
    ]
    for matrix in symbolic:
        response = call(engine, "linalg_inverse", matrix=_text(matrix))
        assert response["ok"], response
        inverse = _inverse_from_solution(response["solution"]["plain"])
        assert inverse is not None
        assert _same(matrix * inverse, sympy.eye(matrix.rows))


# ---------------------------------------------------------------- LU
def test_lu(engine: Engine) -> None:
    rng = random.Random(1104)
    for size in (2, 3, 4):
        for _ in range(10):
            matrix = _rand_matrix(rng, size, size)
            response = call(engine, "linalg_lu", matrix=_text(matrix))
            assert response["ok"], response
            factors = {name: _matrix(body) for name, body in _functions(response).items()}
            assert _same(factors["P"] * matrix, factors["L"] * factors["U"])

    a, b, c, d = sympy.symbols("a b c d")
    for matrix in (sympy.Matrix([[a, b], [c, d]]),
                   sympy.Matrix([[a, 0], [0, b]])):
        response = call(engine, "linalg_lu", matrix=_text(matrix))
        assert response["ok"], response
        factors = {name: _matrix(body) for name, body in _functions(response).items()}
        assert _same(factors["P"] * matrix, factors["L"] * factors["U"])


# ---------------------------------------------------------------- A x = b
def test_linear_solve(engine: Engine) -> None:
    rng = random.Random(1105)
    for size in (2, 3, 4):
        for _ in range(4):
            matrix = _invertible(rng, size)
            rhs = sympy.Matrix([rng.randint(-5, 5) for _ in range(size)])
            for method in ("gauss", "cramer"):
                response = call(
                    engine, "linear_solve", method=method,
                    matrix=_text(matrix), rhs=_text(rhs.T).replace("; ", "; "),
                )
                assert response["ok"], response
                values = [_expr(body) for _, body in sorted(
                    _functions(response).items())]
                solution = sympy.Matrix(values)
                assert _same(matrix * solution, rhs)

    for rows, cols in ((3, 2), (4, 3), (3, 1)):
        for _ in range(2):
            matrix = _rand_matrix(rng, rows, cols)
            if matrix.rank() < cols:
                continue
            exact = sympy.Matrix([rng.randint(-3, 3) for _ in range(cols)])
            rhs = matrix * exact
            response = call(engine, "linear_solve", matrix=_text(matrix),
                            rhs=_text(rhs.T))
            assert response["ok"], response
            values = [_expr(body) for _, body in sorted(
                _functions(response).items())]
            assert _same(matrix * sympy.Matrix(values), rhs)

    for rows, cols in ((2, 3), (2, 4), (3, 4)):
        for _ in range(2):
            matrix = _rand_matrix(rng, rows, cols)
            if matrix.rank() < rows:
                continue
            exact = sympy.Matrix([rng.randint(-3, 3) for _ in range(cols)])
            rhs = matrix * exact
            response = call(engine, "linear_solve", matrix=_text(matrix),
                            rhs=_text(rhs.T))
            assert response["ok"], response
            assert "warning.linear_system_infinite_solutions" in response["warnings"]
            values = [_expr(body) for _, body in sorted(
                _functions(response).items())]
            assert _same(matrix * sympy.Matrix(values), rhs)

    for _ in range(4):
        matrix = sympy.Matrix([[1, 2], [2, 4]])
        rhs = sympy.Matrix([1, 3])
        response = call(engine, "linear_solve", matrix=_text(matrix),
                        rhs=_text(rhs.T))
        assert not response["ok"]
        assert response["error_code"] == "linear_system_inconsistent"

    a, b = sympy.symbols("a b")
    matrix = sympy.Matrix([[a, 0], [0, b]])
    rhs = sympy.Matrix([1, 2])
    for _ in range(2):
        response = call(engine, "linear_solve", method="cramer",
                        matrix=_text(matrix), rhs=_text(rhs.T))
        assert response["ok"], response
        values = [_expr(body) for _, body in sorted(_functions(response).items())]
        assert _same(matrix * sympy.Matrix(values), rhs)
    for _ in range(2):
        response = call(engine, "linear_solve", method="gauss",
                        matrix=_text(matrix), rhs=_text(rhs.T))
        assert response["ok"], response
        values = [_expr(body) for _, body in sorted(_functions(response).items())]
        assert _same(matrix * sympy.Matrix(values), rhs)


# ---------------------------------------------------------------- span
def test_span(engine: Engine) -> None:
    rng = random.Random(1106)
    for dim, count in ((2, 1), (2, 2), (2, 3), (3, 2), (3, 3), (3, 4), (4, 3)):
        for _ in range(3):
            vectors = _rand_matrix(rng, count, dim)
            columns = vectors.T
            if rng.random() < 0.6:
                coefficients = [rng.randint(-3, 3) for _ in range(count)]
                target = columns * sympy.Matrix(coefficients)
                inside = True
            else:
                target = sympy.Matrix([rng.randint(-4, 4) for _ in range(dim)])
                inside = columns.rank() == columns.row_join(target).rank()
            response = call(engine, "linalg_span", vectors=_text(vectors),
                            target=_text(target.T))
            assert response["ok"], response
            text = response["solution"]["plain"]
            assert ("lies in" in text) == inside
            assert ("not in" in text) == (not inside)
            if inside:
                prefix = text.split("lies in")[0]
                body = prefix.split("b =", 1)[1].strip()
                coefficients = _combination_coefficients(body, count)
                rebuilt = columns * sympy.Matrix(coefficients)
                assert _same(rebuilt, target)
            else:
                assert "warning.not_in_the_span" in response["warnings"]

    for dim, count in ((2, 2), (3, 2), (3, 3), (4, 2)):
        vectors = _rand_matrix(rng, count, dim)
        response = call(engine, "linalg_span", vectors=_text(vectors))
        assert response["ok"], response
        assert f"dimension {vectors.T.rank()}" in response["solution"]["plain"]


# ---------------------------------------------------------------- independence
def test_independence(engine: Engine) -> None:
    rng = random.Random(1107)
    for dim, count in ((2, 1), (2, 2), (2, 3), (3, 2), (3, 3), (3, 4), (4, 3)):
        for trial in range(4):
            vectors = _rand_matrix(rng, count, dim)
            if trial == 1 and count > 1:
                vectors[count - 1, :] = vectors[0, :]
            if trial == 2:
                
                for j in range(dim):
                    vectors[count - 1, j] = 0
            independent = vectors.T.rank() == count
            response = call(engine, "linalg_independence", vectors=_text(vectors))
            assert response["ok"], response
            text = response["solution"]["plain"]
            if independent:
                assert "linearly independent" in text
            else:
                assert "linearly dependent" in text
                assert "warning.linearly_dependent" in response["warnings"]
                relation = text.split(":", 1)[1].strip().replace(" = 0", "")
                coefficients = _combination_coefficients(relation, count)
                assert any(coefficient != 0 for coefficient in coefficients)
                rebuilt = vectors.T * sympy.Matrix(coefficients)
                assert _same(rebuilt, sympy.zeros(dim, 1))


# ---------------------------------------------------------------- basis
def test_basis(engine: Engine) -> None:
    rng = random.Random(1108)
    for dim, count in ((2, 2), (2, 3), (3, 2), (3, 3), (3, 4), (4, 3)):
        for trial in range(4):
            vectors = _rand_matrix(rng, count, dim)
            if trial == 1 and count > 1:
                vectors[count - 1, :] = vectors[0, :]
            if trial == 2:
                
                for j in range(dim):
                    vectors[count - 1, j] = 0
            response = call(engine, "linalg_basis", vectors=_text(vectors))
            assert response["ok"], response
            rank = vectors.T.rank()
            basis = [_vec(body) for body in _functions(response).values()]
            assert len(basis) == rank
            if basis:
                stacked = sympy.Matrix.hstack(*basis)
                assert stacked.rank() == rank
                assert vectors.T.row_join(stacked).rank() == rank
            assert f"dim = {rank}" in response["solution"]["plain"]


# ---------------------------------------------------------------- coordinates
def test_coordinates(engine: Engine) -> None:
    rng = random.Random(1109)
    for size in (2, 3):
        for _ in range(10):
            basis = _unimodular(rng, size)
            vector = sympy.Matrix([rng.randint(-5, 5) for _ in range(size)])
            response = call(engine, "linalg_coordinates", basis=_text(basis),
                            vector=_text(vector.T))
            assert response["ok"], response
            values = [_expr(body) for _, body in sorted(
                _functions(response).items())]
            assert _same(basis.T * sympy.Matrix(values), vector)

    a, b = sympy.symbols("a b")
    for _ in range(3):
        response = call(engine, "linalg_coordinates",
                        basis=_text(sympy.Matrix([[a, 0], [0, b]])),
                        vector="1, 2")
        assert response["ok"], response
        values = [_expr(body) for _, body in sorted(_functions(response).items())]
        assert _same(sympy.Matrix([[a, 0], [0, b]]).T * sympy.Matrix(values),
                     sympy.Matrix([1, 2]))


# ---------------------------------------------------------------- change of basis
def test_change_basis(engine: Engine) -> None:
    rng = random.Random(1110)
    for size in (2, 3):
        for _ in range(15):
            first = _unimodular(rng, size)
            second = _unimodular(rng, size)
            response = call(engine, "linalg_change_basis",
                            basis_b=_text(first), basis_c=_text(second))
            assert response["ok"], response
            transition = _matrix(_functions(response)["P"])
            assert _same(second.T * transition, first.T)


# ---------------------------------------------------------------- subspace
def test_subspace(engine: Engine) -> None:
    rng = random.Random(1111)
    for dim, count in ((2, 1), (2, 2), (3, 1), (3, 2), (3, 3), (4, 2)):
        for trial in range(4):
            vectors = _rand_matrix(rng, count, dim)
            if trial == 1 and count > 1:
                vectors[count - 1, :] = vectors[0, :]
            response = call(engine, "linalg_subspace", vectors=_text(vectors))
            assert response["ok"], response
            rank = vectors.T.rank()
            basis = [_vec(body) for body in _functions(response).values()]
            assert len(basis) == rank
            if basis:
                assert sympy.Matrix.hstack(*basis).rank() == rank
                assert vectors.T.row_join(sympy.Matrix.hstack(*basis)).rank() == rank
            assert f"dim W = {rank}" in response["solution"]["plain"]


# ---------------------------------------------------------------- projection
def test_projection(engine: Engine) -> None:
    rng = random.Random(1112)
    for dim, count in ((2, 1), (2, 2), (3, 1), (3, 2), (3, 3), (4, 2)):
        for _ in range(6):
            vectors = _rand_matrix(rng, count, dim)
            if vectors.T.rank() < count:
                continue
            vector = sympy.Matrix([rng.randint(-4, 4) for _ in range(dim)])
            response = call(engine, "linalg_projection", basis=_text(vectors),
                            vector=_text(vector.T))
            assert response["ok"], response
            match = re.match(
                r"proj_W\(v\) = (\([^)]*\)),   v - proj_W\(v\) = "
                r"(\([^)]*\)),   distance = (.+)$",
                response["solution"]["plain"],
            )
            assert match, response["solution"]["plain"]
            projection = sympy.Matrix(_tuple(match.group(1)))
            residual = sympy.Matrix(_tuple(match.group(2)))
            basis = vectors.T
            expected = basis * (basis.T * basis).inv() * basis.T * vector
            assert _same(projection, expected)
            assert _same(residual, vector - expected)
            assert _close(_expr(match.group(3)),
                          sympy.sqrt((vector - expected).dot(vector - expected)))


# ---------------------------------------------------------------- Gram-Schmidt
def test_gram_schmidt(engine: Engine) -> None:
    rng = random.Random(1113)
    for dim, count in ((2, 2), (2, 3), (3, 2), (3, 3), (3, 4), (4, 3)):
        for trial in range(6):
            vectors = _rand_matrix(rng, count, dim)
            if trial == 1 and count > 1:
                vectors[count - 1, :] = vectors[0, :]
            if trial == 2:
                
                for j in range(dim):
                    vectors[count - 1, j] = 0
            response = call(engine, "linalg_gram_schmidt", vectors=_text(vectors))
            assert response["ok"], response
            functions = _functions(response)
            orthogonal = [_vec(body) for name, body in functions.items()
                          if name.startswith("u_")]
            orthonormal = [_vec(body) for name, body in functions.items()
                           if name.startswith("q_")]
            assert len(orthogonal) == len(orthonormal) == vectors.T.rank()
            for i in range(len(orthonormal)):
                assert _close(orthonormal[i].dot(orthonormal[i]), 1, 1e-7)
                for j in range(i + 1, len(orthonormal)):
                    assert _close(orthonormal[i].dot(orthonormal[j]), 0, 1e-7)
            for i in range(len(orthogonal)):
                for j in range(i + 1, len(orthogonal)):
                    assert _zero(orthogonal[i].dot(orthogonal[j]))
            if orthogonal:
                assert sympy.Matrix.hstack(*orthogonal).rank() == vectors.T.rank()
            if vectors.T.rank() < count:
                assert "warning.linearly_dependent" in response["warnings"]


# ---------------------------------------------------------------- QR
def test_qr(engine: Engine) -> None:
    rng = random.Random(1114)
    for rows, cols in ((2, 1), (2, 2), (3, 1), (3, 2), (3, 3), (4, 2), (4, 3)):
        for _ in range(5):
            matrix = _rand_matrix(rng, rows, cols)
            if matrix.rank() < cols:
                continue
            response = call(engine, "linalg_qr", matrix=_text(matrix))
            assert response["ok"], response
            functions = _functions(response)
            q = _matrix(functions["Q"])
            r = _matrix(functions["R"])
            assert _same(q * r, matrix, tol=1e-7)
            assert _same(q.T * q, sympy.eye(cols), tol=1e-7)
            for i in range(cols):
                for j in range(i):
                    assert r[i, j] == 0


# ---------------------------------------------------------------- least squares
def test_least_squares(engine: Engine) -> None:
    rng = random.Random(1115)
    for rows, cols in ((2, 1), (3, 1), (3, 2), (4, 2), (4, 3), (5, 3)):
        for _ in range(6):
            matrix = _rand_matrix(rng, rows, cols)
            if matrix.rank() < cols:
                continue
            rhs = sympy.Matrix([rng.randint(-5, 5) for _ in range(rows)])
            response = call(engine, "linalg_least_squares",
                            matrix=_text(matrix), rhs=_text(rhs.T))
            assert response["ok"], response
            match = re.match(
                r"x = (\([^)]*\)),   residual = (\([^)]*\)),   \|\|r\|\| = (.+)$",
                response["solution"]["plain"],
            )
            assert match, response["solution"]["plain"]
            solution = sympy.Matrix(_tuple(match.group(1)))
            residual = sympy.Matrix(_tuple(match.group(2)))
            assert _same(matrix.T * matrix * solution, matrix.T * rhs)
            assert _same(residual, rhs - matrix * solution)
            assert _close(_expr(match.group(3)),
                          sympy.sqrt(residual.dot(residual)))


# ---------------------------------------------------------------- diagonalisation
def test_diagonalize(engine: Engine) -> None:
    rng = random.Random(1116)
    for size in (2, 3):
        for _ in range(12):
            values = rng.sample(range(-4, 6), size)
            change = _unimodular(rng, size)
            diagonal = sympy.diag(*values)
            matrix = change * diagonal * change.inv()
            response = call(engine, "linalg_diagonalize", matrix=_text(matrix))
            assert response["ok"], response
            functions = _functions(response)
            p = _matrix(functions["P"])
            d = _matrix(functions["D"])
            assert _same(matrix * p, p * d)
            assert p.det() != 0
            assert all(d[i, j] == 0 for i in range(size) for j in range(size)
                       if i != j)
            assert sorted(_num(d[i, i]) for i in range(size)) == sorted(values)

    for matrix in (sympy.Matrix([[1, 1], [0, 1]]),
                   sympy.Matrix([[0, 1], [0, 0]])):
        response = call(engine, "linalg_diagonalize", matrix=_text(matrix))
        assert not response["ok"]
        assert response["error_code"] in (
            "linalg_not_diagonalizable", "linalg_eigenvalues_not_rational")


# ---------------------------------------------------------------- powers
def test_power(engine: Engine) -> None:
    rng = random.Random(1117)
    for size in (2, 3):
        for _ in range(6):
            matrix = _invertible(rng, size)
            for power in (-3, -1, 0, 1, 2, 5):
                response = call(engine, "linalg_power", matrix=_text(matrix),
                                power=str(power))
                assert response["ok"], response
                value = _matrix(list(_functions(response).values())[0])
                assert _same(value, matrix ** power)

    singular = sympy.Matrix([[1, 2], [2, 4]])
    response = call(engine, "linalg_power", matrix=_text(singular), power="-1")
    assert not response["ok"]
    assert response["error_code"] == "linalg_singular_matrix"


# ---------------------------------------------------------------- orthogonal complement
def test_orthogonal_complement(engine: Engine) -> None:
    rng = random.Random(1118)
    for rows, cols in ((2, 2), (3, 2), (3, 3), (4, 2), (4, 3), (2, 3)):
        for _ in range(5):
            matrix = _rand_matrix(rng, rows, cols)
            response = call(engine, "linalg_orthogonal_complement",
                            matrix=_text(matrix))
            assert response["ok"], response
            vectors = [_vec(body) for body in _functions(response).values()]
            assert len(vectors) == rows - matrix.rank()
            for vector in vectors:
                assert _same(matrix.T * vector, sympy.zeros(cols, 1))
            if vectors:
                assert sympy.Matrix.hstack(*vectors).rank() == len(vectors)


# ---------------------------------------------------------------- graphs
def test_graph(engine: Engine) -> None:
    rng = random.Random(1119)
    for nodes in (2, 3, 4, 5, 6):
        for _ in range(6):
            candidates = [(a, b) for a in range(1, nodes + 1)
                          for b in range(a + 1, nodes + 1)]
            count = rng.randint(1, len(candidates))
            edges = rng.sample(candidates, count)
            text = ", ".join(f"{a}-{b}" for a, b in edges)
            response = call(engine, "linalg_graph", nodes=str(nodes), edges=text)
            assert response["ok"], response
            matrix = _incidence(nodes, edges)
            rank = matrix.rank()
            components = nodes - rank
            assert f"components = {components}" in response["solution"]["plain"]
            assert f"rank(B) = {rank}" in response["solution"]["plain"]
            assert f"cycle dimension = {len(edges) - rank}" in response["solution"]["plain"]
            for name, body in _functions(response).items():
                vector = _vec(body)
                if name.startswith("potential"):
                    assert _same(matrix * vector, sympy.zeros(len(edges), 1))
                else:
                    assert _same(matrix.T * vector, sympy.zeros(nodes, 1))


# ---------------------------------------------------------------- DFT
def test_fft(engine: Engine) -> None:
    rng = random.Random(1120)
    for size in (4, 8):
        for _ in range(10):
            signal = [rng.randint(-5, 5) for _ in range(size)]
            response = call(engine, "linalg_fft",
                            signal=", ".join(map(str, signal)))
            assert response["ok"], response
            functions = _functions(response)
            assert len(functions) == size
            for k in range(size):
                body = functions[f"X_{k}"]
                body = re.sub(r"(?<=[0-9])i", "*I", body)
                value = sympy.sympify(body)
                expected = sum(
                    signal[j] * sympy.exp(-2 * sympy.pi * sympy.I * k * j / size)
                    for j in range(size)
                )
                # The spectrum is printed with six significant digits.
                assert abs(complex(sympy.N(value - expected))) < 5e-4


# ---------------------------------------------------------------- similarity
def test_similarity(engine: Engine) -> None:
    rng = random.Random(1121)
    for size in (2, 3):
        for _ in range(15):
            matrix = _rand_matrix(rng, size, size)
            change = _unimodular(rng, size)
            response = call(engine, "linalg_similarity", matrix=_text(matrix),
                            change=_text(change))
            assert response["ok"], response
            similar = _matrix(_functions(response)["B"])
            assert _same(similar, change.inv() * matrix * change)


# ---------------------------------------------------------------- exp(At)
def test_matrix_exp(engine: Engine) -> None:
    t = sympy.Symbol("t")
    a = sympy.Symbol("a")

    response = call(engine, "linalg_matrix_exp", matrix=_text(sympy.Matrix([[a]])))
    assert response["ok"], response
    exponential = _matrix(list(_functions(response).values())[0])
    assert _zero(exponential[0, 0] - sympy.exp(a * t))

    cases = [
        sympy.Matrix([[1, 0], [0, 2]]),
        sympy.Matrix([[1, 1], [0, 2]]),
        sympy.Matrix([[0, 1], [-1, 0]]),
        sympy.Matrix([[0, 1], [1, 0]]),
        sympy.Matrix([[1, 1], [0, 1]]),
        sympy.Matrix([[0, 1], [0, 0]]),
        sympy.Matrix([[1, 2, 3], [0, 4, 5], [0, 0, 6]]),
        sympy.Matrix([[1, 0, 0], [0, 2, 0], [0, 0, 3]]),
    ]
    for matrix in cases:
        response = call(engine, "linalg_matrix_exp", matrix=_text(matrix))
        assert response["ok"], response
        exponential = _matrix(list(_functions(response).values())[0])
        at_zero = exponential.subs(t, 0)
        assert _same(at_zero, sympy.eye(matrix.rows), tol=1e-9)
        derivative = exponential.diff(t).subs(t, 0)
        assert _same(derivative, matrix, tol=1e-7)


# ---------------------------------------------------------------- positive definite
def test_positive_definite(engine: Engine) -> None:
    rng = random.Random(1122)
    for size in (2, 3):
        for _ in range(12):
            seed = _rand_matrix(rng, size, size)
            matrix = seed.T * seed + sympy.eye(size)
            response = call(engine, "linalg_positive_definite", matrix=_text(matrix))
            assert response["ok"], response
            l = _matrix(_functions(response)["L"])
            assert _same(l * l.T, matrix, tol=1e-7)
            assert all(l[i, j] == 0 for i in range(size) for j in range(i + 1, size))

    response = call(engine, "linalg_positive_definite",
                    matrix=_text(sympy.Matrix([[2, 0], [0, 5]])))
    assert response["ok"], response
    assert _same(_matrix(_functions(response)["L"]) *
                 _matrix(_functions(response)["L"]).T,
                 sympy.Matrix([[2, 0], [0, 5]]), tol=1e-7)

    for matrix in (sympy.Matrix([[1, 2], [2, 1]]),
                   sympy.Matrix([[1, 2], [3, 4]]),
                   sympy.Matrix([[0, 0], [0, 1]])):
        response = call(engine, "linalg_positive_definite", matrix=_text(matrix))
        assert not response["ok"]
        assert response["error_code"] in (
            "linalg_not_positive_definite", "linalg_not_symmetric")


# ---------------------------------------------------------------- SVD
def test_svd(engine: Engine) -> None:
    rng = random.Random(1123)
    for rows, cols in ((1, 1), (2, 1), (1, 3), (2, 2), (2, 3), (3, 2), (3, 3),
                       (4, 2), (2, 4), (3, 4)):
        for trial in range(4):
            matrix = _rand_matrix(rng, rows, cols)
            if trial == 1 and rows > 1:
                matrix[rows - 1, :] = matrix[0, :]
            if trial == 2:
                matrix = sympy.zeros(rows, cols)
            response = call(engine, "linalg_svd", matrix=_text(matrix))
            assert response["ok"], response
            functions = _functions(response)
            u = _matrix(functions["U"])
            s = _matrix(functions["S"])
            v = _matrix(functions["V"])
            assert _same(u * s * v.T, matrix, tol=1e-6)
            assert _same(u.T * u, sympy.eye(u.cols), tol=1e-6)
            values = _singular_values(matrix)
            reported = [complex(sympy.N(s[i, i])).real for i in range(s.rows)]
            assert len(reported) == len(values)
            for got, want in zip(reported, values):
                assert abs(got - want) < 1e-5


# ---------------------------------------------------------------- norms
def test_norm(engine: Engine) -> None:
    rng = random.Random(1124)
    for rows, cols in ((1, 1), (2, 2), (2, 3), (3, 2), (3, 3), (4, 2)):
        for trial in range(4):
            matrix = _rand_matrix(rng, rows, cols)
            if trial == 1:
                matrix = sympy.zeros(rows, cols)
            response = call(engine, "linalg_norm", matrix=_text(matrix))
            assert response["ok"], response
            match = re.match(
                r"\|\|A\|\|_1 = ([^,]+),   \|\|A\|\|_inf = ([^,]+),   "
                r"\|\|A\|\|_2 = ([^,]+),   cond_2\(A\) = (.+)$",
                response["solution"]["plain"],
            )
            assert match, response["solution"]["plain"]
            one = max(sum(abs(matrix[i, j]) for i in range(rows))
                      for j in range(cols))
            infinity = max(sum(abs(matrix[i, j]) for j in range(cols))
                           for i in range(rows))
            frobenius = sympy.sqrt(sum(matrix[i, j] ** 2
                                       for i in range(rows) for j in range(cols)))
            spectral = _singular_values(matrix)[0]
            smallest = _singular_values(matrix)[-1]
            assert _close(_expr(match.group(1)), one)
            assert _close(_expr(match.group(2)), infinity)
            assert _close(_expr(match.group(3)), spectral, 1e-4)
            if smallest > 1e-9:
                assert _close(_expr(match.group(4)), spectral / smallest, 1e-3)
            else:
                assert match.group(4) == "inf"
            assert spectral <= frobenius + 1e-9


# ---------------------------------------------------------------- power iteration
def test_power_iteration(engine: Engine) -> None:
    rng = random.Random(1125)
    cases = [
        sympy.Matrix([[2, 0], [0, 1]]),
        sympy.Matrix([[-3, 0], [0, 1]]),
        sympy.Matrix([[2, 1], [1, 2]]),
        sympy.Matrix([[4, 1], [1, 3]]),
        sympy.Matrix([[2, 0, 0], [0, 1, 0], [0, 0, -1]]),
    ]
    for _ in range(15):
        seed = _rand_matrix(rng, 2, 2)
        symmetric = seed + seed.T
        values = _eigenvalues(symmetric)
        if len(values) == 2 and abs(values[0]) > abs(values[1]) + 0.5:
            cases.append(symmetric)
    for matrix in cases:
        response = call(engine, "linalg_power_iteration", matrix=_text(matrix))
        assert response["ok"], response
        match = re.match(r"lambda_1 = ([^,]+),   v_1 = (\([^)]*\))$",
                         response["solution"]["plain"])
        assert match, response["solution"]["plain"]
        value = _expr(match.group(1))
        vector = sympy.Matrix(_tuple(match.group(2)))
        assert _same(matrix * vector, value * vector, tol=1e-6)
        dominant = max(_eigenvalues(matrix), key=abs)
        assert abs(_num(value) - dominant) < 1e-5


# ---------------------------------------------------------------- QR algorithm
def test_qr_algorithm(engine: Engine) -> None:
    rng = random.Random(1126)
    for size in (2, 3):
        for _ in range(12):
            seed = _rand_matrix(rng, size, size)
            matrix = seed + seed.T
            response = call(engine, "linalg_qr_algorithm", matrix=_text(matrix))
            assert response["ok"], response
            reported = sorted(
                (_num(_expr(body)) for body in _functions(response).values()),
                reverse=True,
            )
            expected = _eigenvalues(matrix)
            assert len(reported) == len(expected)
            for got, want in zip(reported, expected):
                assert abs(got - want) < 1e-4


# ---------------------------------------------------------------- iterative solvers
def test_iterative(engine: Engine) -> None:
    rng = random.Random(1127)
    for size in (2, 3):
        for _ in range(6):
            matrix = _rand_matrix(rng, size, size, -2, 2)
            for i in range(size):
                matrix[i, i] = sum(abs(matrix[i, j]) for j in range(size)
                                   if j != i) + rng.randint(1, 3)
            rhs = sympy.Matrix([rng.randint(-4, 4) for _ in range(size)])
            exact = matrix.inv() * rhs
            for method in ("jacobi", "gauss_seidel"):
                response = call(engine, "linalg_iterative", matrix=_text(matrix),
                                rhs=_text(rhs.T), method=method)
                assert response["ok"], response
                body = list(_functions(response).values())[0]
                values = sympy.Matrix(_tuple(body))
                for got, want in zip(values, exact):
                    assert abs(_num(got - want)) < 1e-6

    diverged = call(engine, "linalg_iterative", matrix="1, 2; 2, 1",
                    rhs="3, 3", method="jacobi")
    assert diverged["ok"]
    assert "warning.iterative_diverged" in diverged["warnings"]


# ---------------------------------------------------------------- linear programming
def test_linear_program(engine: Engine) -> None:
    rng = random.Random(1128)
    for _ in range(24):
        objective = [rng.randint(-2, 4), rng.randint(-2, 4)]
        bound = rng.randint(2, 6)
        constraints = [[1, 1], [1, 0], [0, 1]]
        bounds = [rng.randint(1, 2 * bound), bound, bound]
        for _ in range(rng.randint(0, 2)):
            a, b = rng.randint(0, 2), rng.randint(0, 2)
            if a == 0 and b == 0:
                continue
            constraints.append([a, b])
            bounds.append(rng.randint(1, 2 * bound))
        response = call(engine, "linalg_lp",
                        objective=", ".join(map(str, objective)),
                        constraints=_text(sympy.Matrix(constraints)),
                        bounds=", ".join(map(str, bounds)))
        assert response["ok"], response
        match = re.match(r"x = \(([^)]*)\),   z = (.+)$",
                         response["solution"]["plain"])
        assert match, response["solution"]["plain"]
        point = [sympy.Rational(part) for part in match.group(1).split(",")]
        value = _expr(match.group(2))
        for row, limit in zip(constraints, bounds):
            assert row[0] * point[0] + row[1] * point[1] <= limit
        assert point[0] >= 0 and point[1] >= 0
        assert _close(value, objective[0] * point[0] + objective[1] * point[1])
        optimum = _lp_optimum(objective, constraints, bounds)
        assert _close(value, optimum, 1e-5)

    unbounded = call(engine, "linalg_lp", objective="1, 0",
                     constraints="-1, 0; 0, -1", bounds="1, 1")
    assert not unbounded["ok"]
    assert unbounded["error_code"] == "linalg_lp_unbounded"

    negative = call(engine, "linalg_lp", objective="1, 1",
                    constraints="1, 1; 1, 0; 0, 1", bounds="-1, 2, 2")
    assert not negative["ok"]
    assert negative["error_code"] == "linalg_lp_negative_rhs"


# ---------------------------------------------------------------- matrix games
def test_matrix_game(engine: Engine) -> None:
    rng = random.Random(1129)
    cases = [
        sympy.Matrix([[3, 0], [0, 2]]),
        sympy.Matrix([[1, 2], [2, 1]]),
        sympy.Matrix([[0, 0], [0, 0]]),
        sympy.Matrix([[4, 1], [1, 3]]),
        sympy.Matrix([[2, -1], [-1, 1]]),
    ]
    for _ in range(20):
        cases.append(_rand_matrix(rng, 2, 2, -3, 4))
    for matrix in cases:
        response = call(engine, "linalg_game", matrix=_text(matrix))
        expected = _game_value(matrix)
        if expected is None:
            assert not response["ok"]
            continue
        assert response["ok"], response
        match = re.match(
            r"value = ([^,]+),   row strategy = (\([^)]*\)),   "
            r"column strategy = (\([^)]*\))$",
            response["solution"]["plain"],
        )
        assert match, response["solution"]["plain"]
        assert _close(_expr(match.group(1)), expected, 1e-6)
        row = [sympy.Rational(part) for part in match.group(2)[1:-1].split(",")]
        column = [sympy.Rational(part) for part in match.group(3)[1:-1].split(",")]
        assert _close(sum(row), 1)
        assert _close(sum(column), 1)
        assert all(part >= -1e-12 for part in row + column)
        payoff = sum(row[i] * column[j] * matrix[i, j]
                     for i in range(2) for j in range(2))
        assert _close(payoff, expected, 1e-6)


# ---------------------------------------------------------------- spectral theorem
def test_symmetric(engine: Engine) -> None:
    rng = random.Random(1130)
    for size in (2, 3):
        for _ in range(12):
            seed = _rand_matrix(rng, size, size)
            matrix = seed + seed.T
            response = call(engine, "linalg_symmetric", matrix=_text(matrix))
            assert response["ok"], response
            functions = _functions(response)
            q = _matrix(functions["Q"])
            d = _matrix(functions["D"])
            assert _same(q * d * q.T, matrix, tol=1e-6)
            assert _same(q.T * q, sympy.eye(size), tol=1e-6)
            reported = sorted((_num(d[i, i]) for i in range(size)), reverse=True)
            expected = _eigenvalues(matrix)
            for got, want in zip(reported, expected):
                assert abs(got - want) < 1e-6


# ---------------------------------------------------------------- error paths
def test_error_paths(engine: Engine) -> None:
    failures = [
        ("linalg_det", {"matrix": "1, 2, 3; 4, 5, 6"}, "linalg_not_square"),
        ("linalg_inverse", {"matrix": "1, 2, 3"}, "linalg_not_square"),
        ("linalg_lu", {"matrix": "1, 2; 3, 4; 5, 6"}, "linalg_not_square"),
        ("linalg_rank", {"matrix": "1, 2; x"}, "linalg_ragged_matrix"),
        ("linalg_det", {"matrix": ""}, "linalg_bad_matrix"),
        ("linalg_coordinates", {"basis": "1, 2; 2, 4", "vector": "1, 1"},
         "linalg_not_a_basis"),
        ("linalg_coordinates", {"basis": "1, 0; 0, 1", "vector": "1, 2, 3"},
         "linalg_dimension_mismatch"),
        ("linalg_change_basis", {"basis_b": "1, 2; 2, 4", "basis_c": "1, 0; 0, 1"},
         "linalg_not_a_basis"),
        ("linalg_projection", {"basis": "1, 2; 2, 4", "vector": "1, 1"},
         "linalg_dependent_basis"),
        ("linalg_projection", {"basis": "1, 0; 0, 1", "vector": "1"},
         "linalg_dimension_mismatch"),
        ("linalg_qr", {"matrix": "1, 2; 2, 4; 3, 6"}, "linalg_not_full_column_rank"),
        ("linalg_qr", {"matrix": "1, 2; 2, 4"}, "linalg_not_full_column_rank"),
        ("linalg_least_squares", {"matrix": "1, 0, 0; 0, 1, 0", "rhs": "1; 2"},
         "linalg_not_full_column_rank"),
        ("linalg_least_squares", {"matrix": "1, 2; 2, 4; 3, 6", "rhs": "1; 2; 3"},
         "linalg_not_full_column_rank"),
        ("linalg_least_squares", {"matrix": "1, 0; 0, 1", "rhs": "1; 2; 3"},
         "linalg_dimension_mismatch"),
        ("linalg_diagonalize", {"matrix": "1, 1; 0, 1"}, "linalg_not_diagonalizable"),
        ("linalg_diagonalize", {"matrix": "a, 0; 0, b"}, "linalg_numeric_required"),
        ("linalg_power", {"matrix": "1, 0; 0, 1", "power": "99"}, "linalg_bad_power"),
        ("linalg_power", {"matrix": "1, 0; 0, 1", "power": "x"}, "linalg_bad_power"),
        ("linalg_power", {"matrix": "1, 2; 2, 4", "power": "-2"}, "linalg_singular_matrix"),
        ("linalg_fft", {"signal": "1, 2, 3"}, "linalg_fft_length"),
        ("linalg_fft", {"signal": "a, b, c, d"}, "linalg_numeric_required"),
        ("linalg_positive_definite", {"matrix": "1, 2; 3, 4"}, "linalg_not_symmetric"),
        ("linalg_positive_definite", {"matrix": "1, 2; 2, 1"},
         "linalg_not_positive_definite"),
        ("linalg_svd", {"matrix": "a, 0; 0, b"}, "linalg_numeric_required"),
        ("linalg_norm", {"matrix": "a, 0; 0, b"}, "linalg_numeric_required"),
        ("linalg_power_iteration", {"matrix": "a, 0; 0, b"}, "linalg_numeric_required"),
        ("linalg_qr_algorithm", {"matrix": "1, 2; 3, 4"}, "linalg_not_symmetric"),
        ("linalg_qr_algorithm", {"matrix": "a, 0; 0, b"}, "linalg_numeric_required"),
        ("linalg_iterative", {"matrix": "a, 0; 0, b", "rhs": "1, 2"},
         "linalg_numeric_required"),
        ("linalg_iterative", {"matrix": "0, 1; 1, 0", "rhs": "1, 2"},
         "linalg_singular_matrix"),
        ("linalg_iterative", {"matrix": "1, 0; 0, 1", "rhs": "1, 2, 3"},
         "linalg_dimension_mismatch"),
        ("linalg_lp", {"objective": "a, 2", "constraints": "1, 1; 1, 0; 0, 1",
                       "bounds": "4, 2, 2"}, "linalg_numeric_required"),
        ("linalg_lp", {"objective": "1, 2", "constraints": "1, 1",
                       "bounds": "4, 2"}, "linalg_bad_rhs"),
        ("linalg_game", {"matrix": "1, 2, 3; 4, 5, 6"}, "linalg_dimension_mismatch"),
        ("linalg_game", {"matrix": "a, 0; 0, b"}, "linalg_numeric_required"),
        ("linalg_symmetric", {"matrix": "1, 2; 3, 4"}, "linalg_not_symmetric"),
        ("linalg_symmetric", {"matrix": "a, 0; 0, b"}, "linalg_numeric_required"),
        ("linalg_similarity", {"matrix": "1, 2; 3, 4", "change": "1, 1; 2, 2"},
         "linalg_singular_matrix"),
        ("linalg_matrix_exp", {"matrix": "1, 2, 3"}, "linalg_not_square"),
        ("linalg_graph", {"nodes": "9", "edges": "1-2"}, "linalg_too_large"),
        ("linalg_graph", {"nodes": "3", "edges": "1-4"}, "linalg_bad_matrix"),
        ("linalg_graph", {"nodes": "3", "edges": "1-1"}, "linalg_bad_matrix"),
    ]
    for name, params, code in failures:
        response = call(engine, name, **params)
        assert not response["ok"], (name, params, response.get("solution"))
        assert response["error_code"] == code, (name, params, response["error_code"])


# ---------------------------------------------------------------- coverage
def test_sections_are_present() -> None:
    topics = [
        "linear_solve", "linalg_rank", "linalg_det", "linalg_inverse",
        "linalg_lu", "linalg_span", "linalg_independence", "linalg_basis",
        "linalg_coordinates", "linalg_change_basis", "linalg_subspace",
        "linalg_projection", "linalg_gram_schmidt", "linalg_qr",
        "linalg_least_squares", "linalg_diagonalize", "linalg_power",
        "linalg_orthogonal_complement", "linalg_graph", "linalg_fft",
        "linalg_similarity", "linalg_matrix_exp", "linalg_positive_definite",
        "linalg_svd", "linalg_norm", "linalg_power_iteration",
        "linalg_qr_algorithm", "linalg_iterative", "linalg_lp", "linalg_game",
        "linalg_symmetric",
    ]
    articles = ["study.la_fields", "study.la_vector_spaces",
                "study.la_transformations", "study.la_determinants",
                "study.la_canonical", "study.la_inner_products"]
    assert len(topics) + len(articles) == 37
    for name in topics:
        assert f"topic.{name}.title" in EN and f"topic.{name}.title" in FA
    for name in articles:
        for suffix in (".title", ".subtitle", ".short_answer", ".pitfalls",
                       ".theory.0", ".example.0.problem", ".example.0.answer"):
            assert name + suffix in EN and name + suffix in FA


def test_suite_is_large_enough() -> None:
    assert CASES >= 1000, f"only {CASES} engine cases ran"
