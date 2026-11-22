// Infinit Lab — elementary linear algebra (rank, determinant, inverse, LU,
// A x = b).
//
// The matrix type is symbolic on purpose.  A pivot is chosen only when it is
// provably nonzero after canonical simplification; when the pivot contains
// symbols the answer is the generic one and the result says so in
// `assumptions`.  Nothing is ever produced with floating point: doubles are
// used only for the optional numeric read-out and for the certificates.
// SPDX-License-Identifier: Apache-2.0
#include "linalg.hpp"

#include "expr.hpp"
#include "numeric.hpp"
#include "poly.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstdio>
#include <string>
#include <vector>

namespace ode {
namespace {

using SMat = std::vector<std::vector<Ex>>;

// ---------------------------------------------------------------- response
void baseId(Result& r, const std::string& typeId, const std::string& variable) {
    r.ok = true;
    r.mode = "topic";
    r.typeId = typeId;
    r.variable = variable;
    r.engine = "native";
    r.closedForm = true;
    r.verified = true;
}

void failCode(Result& r, const std::string& code) {
    r.ok = false;
    r.errorCode = code;
    r.verified = false;
    r.error.clear();
}

// Every linear-algebra step carries a prose explanation, so the worked solution
// reads as a lesson rather than a list of operations.  The note key is derived
// from the title key, which keeps the two in sync: `step.title.row_reduce` is
// explained by `step.note.row_reduce`.
std::string noteKeyFor(const std::string& titleKey) {
    const std::string prefix = "step.title.";
    if (titleKey.rfind(prefix, 0) != 0) return "";
    return "step.note." + titleKey.substr(prefix.size());
}

void addStep(Result& r, const std::string& titleKey, const Ex& math = nullptr,
             const std::string& noteKey = "", const std::string& params = "") {
    Step s;
    s.titleKey = titleKey;
    s.math = math;
    s.noteKey = noteKey.empty() ? noteKeyFor(titleKey) : noteKey;
    s.params = params;
    r.steps.push_back(std::move(s));
}

std::string numStr(double v, int digits = 8) {
    if (std::isnan(v)) return "nan";
    if (!std::isfinite(v)) return v < 0 ? "-inf" : "inf";
    if (std::fabs(v) < 1e-13) return "0";
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*g", digits, v);
    std::string s(buf);
    if (s.find('.') != std::string::npos && s.find('e') == std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s;
}

// ---------------------------------------------------------------- exact helpers
size_t sizeOf(const Ex& e);

Ex simp(const Ex& e) {
    if (!e) return Num(0);
    try {
        return simplify(e);
    } catch (...) {
        return e;
    }
}

Ex tidy(const Ex& e) {
    if (!e) return Num(0);
    // Deep simplification is quadratic in the worst case; a very large
    // symbolic entry keeps its canonical form instead of stalling the page.
    if (sizeOf(e) > 2000) return simp(e);
    try {
        return simplifyStrong(e);
    } catch (...) {
        return simp(e);
    }
}

bool zeroFast(const Ex& e) {
    if (!e) return true;
    try {
        return simplify(e)->isZero();
    } catch (...) {
        return false;
    }
}

// A zero test that also survives a symbolic cancellation: simplify, then
// simplifyStrong, then a numeric probe on the free symbols.
bool zeroEx(const Ex& e) {
    if (!e) return true;
    if (zeroFast(e)) return true;
    try {
        if (simplifyStrong(e)->isZero()) return true;
    } catch (...) {
    }
    std::vector<std::string> syms;
    collectSymbols(e, syms);
    if (syms.empty()) {
        bool ok = false;
        const double v = evalReal(e, {}, &ok);
        return ok && std::fabs(v) < 1e-9;
    }
    try {
        return numericallyZero(e, syms, 8, 1e-7);
    } catch (...) {
        return false;
    }
}

size_t sizeOf(const Ex& e) {
    if (!e) return 0;
    size_t s = 1;
    for (auto& x : e->args) s += sizeOf(x);
    return s;
}

bool hasSymbols(const Ex& e) {
    std::vector<std::string> syms;
    collectSymbols(e, syms);
    return !syms.empty();
}

bool hasDivision(const Ex& e) {
    if (!e) return false;
    if (e->isPow() && e->args.size() == 2 && e->args[1]->isNum() &&
        e->args[1]->num.n < 0)
        return true;
    for (auto& x : e->args)
        if (hasDivision(x)) return true;
    return false;
}

std::string normalizeText(int row, const Ex& pivot) {
    const std::string r = "R" + std::to_string(row + 1);
    Ex p = simp(pivot);
    if (p->isNum() && p->num.n == -1) return r + " <- -" + r;
    if (p->isNum() && p->num.isInt()) return r + " <- " + r + " / " +
                                                std::to_string(p->num.n);
    return r + " <- " + r + " / (" + toPlain(p) + ")";
}

std::string plain(const Ex& e) { return toPlain(tidy(e)); }

// A factor of 1/det in front of the adjugate.  A bare symbol or number needs
// no parentheses; anything that could absorb the following `*` does.
std::string reciprocalFactor(const Ex& det) {
    const std::string text = toPlain(det);
    if (det->isSym() || det->isNum()) return "1/" + text;
    return "1/(" + text + ")";
}

void replaceAll(std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t p = 0;
    while ((p = s.find(from, p)) != std::string::npos) {
        s.replace(p, from.size(), to);
        p += to.size();
    }
}

// ---------------------------------------------------------------- matrix I/O
std::string matText(const SMat& A) {
    std::string s = "[";
    for (size_t i = 0; i < A.size(); ++i) {
        if (i) s += "; ";
        s += "[";
        for (size_t j = 0; j < A[i].size(); ++j) {
            if (j) s += ", ";
            s += toPlain(simp(A[i][j]));
        }
        s += "]";
    }
    return s + "]";
}

std::string vectorText(const std::vector<Ex>& v) {
    std::string s = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) s += "; ";
        s += "[" + toPlain(simp(v[i])) + "]";
    }
    return s + "]";
}

// A row vector stays on one line: the row space is a set of rows, not of
// columns.
std::string rowVectorText(const std::vector<Ex>& v) {
    std::string s = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) s += ", ";
        s += toPlain(simp(v[i]));
    }
    return s + "]";
}

std::string tupleText(const std::vector<Ex>& v) {
    std::string s = "(";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) s += ", ";
        s += toPlain(tidy(v[i]));
    }
    return s + ")";
}

std::vector<std::string> splitCells(const std::string& row) {
    std::vector<std::string> out;
    std::string cur;
    int depth = 0;
    for (const char c : row) {
        if (c == '(') ++depth;
        if (c == ')') --depth;
        if (c == ',' && depth == 0) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

// Accepts all the shapes the app already prints and the reader is likely to
// type:
//   2, 1; 1, -1        [[2, 1], [1, -1]]        [[2,1];[1,-1]]
// with entries that are numbers or expressions such as a, a-1, sqrt(2).
bool parseMatrixText(const std::string& raw, SMat& A, std::string& code,
                     int maxSize, int maxRows = 0) {
    std::string s = raw;
    replaceAll(s, "\xE2\x88\x92", "-");  // U+2212 minus
    replaceAll(s, "\xC2\xB7", "*");      // U+00B7 middle dot
    replaceAll(s, "\xC3\x97", "*");      // U+00D7 multiplication sign
    replaceAll(s, "\xE2\x8B\x85", "*");  // U+22C5 dot operator
    for (char& c : s) {
        if (c == '\n' || c == '\r') c = ';';
    }
    s.erase(std::remove_if(s.begin(), s.end(),
                           [](unsigned char c) { return std::isspace(c) != 0; }),
            s.end());
    if (s.empty()) {
        code = "linalg_bad_matrix";
        return false;
    }
    if (s.find('[') != std::string::npos) {
        replaceAll(s, "],[", ";");
        replaceAll(s, "];[", ";");
        s.erase(std::remove_if(s.begin(), s.end(),
                               [](char c) { return c == '[' || c == ']'; }),
                s.end());
    }

    std::vector<std::string> rows;
    std::string current;
    for (const char c : s) {
        if (c == ';') {
            if (!current.empty()) rows.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) rows.push_back(current);
    if (rows.empty()) {
        code = "linalg_bad_matrix";
        return false;
    }

    A.clear();
    for (const std::string& row : rows) {
        std::vector<Ex> entries;
        for (const std::string& cell : splitCells(row)) {
            if (cell.empty()) {
                code = "linalg_bad_matrix";
                return false;
            }
            try {
                Ex e = parseExpr(cell);
                if (!e) {
                    code = "linalg_entry_invalid";
                    return false;
                }
                entries.push_back(e);
            } catch (...) {
                code = "linalg_entry_invalid";
                return false;
            }
        }
        if (entries.empty()) {
            code = "linalg_bad_matrix";
            return false;
        }
        if (!A.empty() && entries.size() != A.front().size()) {
            code = "linalg_ragged_matrix";
            return false;
        }
        A.push_back(std::move(entries));
    }
    if (A.empty() || A.front().empty()) {
        code = "linalg_bad_matrix";
        return false;
    }
    const int rowLimit = maxRows > 0 ? maxRows : maxSize;
    if ((int)A.size() > rowLimit || (int)A.front().size() > maxSize) {
        code = "linalg_too_large";
        return false;
    }
    code.clear();
    return true;
}

bool parseVectorText(const std::string& raw, std::vector<Ex>& v,
                     std::string& code) {
    SMat M;
    if (!parseMatrixText(raw, M, code, 8)) return false;
    if (M.size() == 1) {
        v = M.front();
        return true;
    }
    if (M.front().size() == 1) {
        v.clear();
        for (auto& row : M) v.push_back(row.front());
        return true;
    }
    code = "linalg_bad_rhs";
    return false;
}

// ---------------------------------------------------------------- operations
SMat identityMatrix(int n) {
    SMat I(n, std::vector<Ex>(n, Num(0)));
    for (int i = 0; i < n; ++i) I[i][i] = Num(1);
    return I;
}

SMat multiplyMatrices(const SMat& A, const SMat& B) {
    if (A.empty() || B.empty() || A.front().size() != B.size()) return {};
    const int n = (int)A.size(), m = (int)B.front().size();
    SMat C(n, std::vector<Ex>(m, Num(0)));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < m; ++j) {
            Ex sum = Num(0);
            for (int k = 0; k < (int)B.size(); ++k)
                if (!zeroFast(A[i][k]) && !zeroFast(B[k][j]))
                    sum = Add(sum, Mul(A[i][k], B[k][j]));
            C[i][j] = simp(sum);
        }
    }
    return C;
}

std::vector<Ex> multiplyMatrixVector(const SMat& A, const std::vector<Ex>& v) {
    std::vector<Ex> out(A.size(), Num(0));
    for (size_t i = 0; i < A.size(); ++i) {
        Ex sum = Num(0);
        for (size_t j = 0; j < A[i].size(); ++j)
            if (!zeroFast(A[i][j]) && !zeroFast(v[j]))
                sum = Add(sum, Mul(A[i][j], v[j]));
        out[i] = simp(sum);
    }
    return out;
}

std::string rowOpText(int target, int pivot, const Ex& factor) {
    const std::string t = "R" + std::to_string(target + 1);
    const std::string p = "R" + std::to_string(pivot + 1);
    Ex f = simp(factor);
    if (f->isOne()) return t + " <- " + t + " - " + p;
    if (f->isNum() && f->num.n == -1) return t + " <- " + t + " + " + p;
    return t + " <- " + t + " - (" + toPlain(f) + ") " + p;
}

struct Rref {
    SMat m;
    std::vector<int> pivots;
    std::vector<std::string> ops;
    bool symbolic = false;
};

Rref rowReduce(SMat A, bool recordOps) {
    Rref out;
    out.m = std::move(A);
    if (out.m.empty()) return out;
    const int rows = (int)out.m.size();
    const int cols = (int)out.m.front().size();
    int row = 0;
    for (int col = 0; col < cols && row < rows; ++col) {
        // The smallest pivot keeps symbolic elimination readable.
        int best = -1;
        size_t bestSize = (size_t)-1;
        for (int r = row; r < rows; ++r) {
            Ex v = simp(out.m[r][col]);
            if (v->isZero()) continue;
            const size_t s = sizeOf(v);
            if (s < bestSize) {
                bestSize = s;
                best = r;
            }
        }
        if (best < 0) continue;
        if (best != row) {
            std::swap(out.m[best], out.m[row]);
            if (recordOps)
                out.ops.push_back("R" + std::to_string(row + 1) + " <-> R" +
                                  std::to_string(best + 1));
        }
        Ex pivot = simp(out.m[row][col]);
        if (hasSymbols(pivot)) out.symbolic = true;
        if (!pivot->isOne()) {
            Ex inv = Pow(pivot, Num(-1));
            for (int j = col; j < cols; ++j)
                out.m[row][j] = simp(Mul(out.m[row][j], inv));
            if (recordOps) out.ops.push_back(normalizeText(row, pivot));
        }
        for (int r = 0; r < rows; ++r) {
            if (r == row) continue;
            Ex factor = simp(out.m[r][col]);
            if (factor->isZero()) continue;
            for (int j = col; j < cols; ++j)
                out.m[r][j] = simp(Add(out.m[r][j],
                                       Mul(Num(-1), factor, out.m[row][j])));
            if (recordOps) out.ops.push_back(rowOpText(r, row, factor));
        }
        out.pivots.push_back(col);
        ++row;
    }
    return out;
}

struct Determinant {
    Ex value;
    bool singular = false;
    bool symbolic = false;
    std::vector<std::string> ops;
};

Determinant determinantByElimination(SMat A) {
    Determinant out;
    const int n = (int)A.size();
    Ex sign = Num(1);
    std::vector<Ex> diagonal;
    for (int k = 0; k < n; ++k) {
        int best = -1;
        size_t bestSize = (size_t)-1;
        for (int r = k; r < n; ++r) {
            Ex v = simp(A[r][k]);
            if (v->isZero()) continue;
            const size_t s = sizeOf(v);
            if (s < bestSize) {
                bestSize = s;
                best = r;
            }
        }
        if (best < 0) {
            out.value = Num(0);
            out.singular = true;
            out.ops.push_back("column " + std::to_string(k + 1) +
                              " has no pivot, so the determinant is zero");
            return out;
        }
        if (best != k) {
            std::swap(A[best], A[k]);
            sign = Mul(sign, Num(-1));
            out.ops.push_back("R" + std::to_string(k + 1) + " <-> R" +
                              std::to_string(best + 1) +
                              "   (the sign flips)");
        }
        Ex pivot = simp(A[k][k]);
        if (hasSymbols(pivot)) out.symbolic = true;
        diagonal.push_back(pivot);
        for (int r = k + 1; r < n; ++r) {
            Ex factor = simp(Mul(A[r][k], Pow(pivot, Num(-1))));
            if (factor->isZero()) continue;
            for (int j = k; j < n; ++j)
                A[r][j] = simp(Add(A[r][j],
                                   Mul(Num(-1), factor, A[k][j])));
            out.ops.push_back(rowOpText(r, k, factor));
        }
    }
    Ex value = sign;
    for (auto& d : diagonal) value = Mul(value, d);
    out.value = tidy(value);
    out.singular = out.value->isZero();
    return out;
}

// Independent certificate: Laplace expansion along the first row.
Ex determinantByCofactors(const SMat& A) {
    const int n = (int)A.size();
    if (n == 0) return Num(1);
    if (n == 1) return A[0][0];
    if (n == 2)
        return simp(Add(Mul(A[0][0], A[1][1]),
                        Mul(Num(-1), A[0][1], A[1][0])));
    Ex total = Num(0);
    for (int j = 0; j < n; ++j) {
        if (zeroFast(A[0][j])) continue;
        SMat minor(n - 1, std::vector<Ex>(n - 1, Num(0)));
        for (int i = 1; i < n; ++i) {
            int target = 0;
            for (int c = 0; c < n; ++c) {
                if (c == j) continue;
                minor[i - 1][target++] = A[i][c];
            }
        }
        Ex term = Mul(A[0][j], determinantByCofactors(minor));
        total = (j % 2 == 0) ? Add(total, term)
                             : Add(total, Mul(Num(-1), term));
    }
    return tidy(total);
}

struct Inverse {
    bool ok = false;
    SMat value;
    std::vector<std::string> ops;
    bool symbolic = false;
};

Inverse inverseByGaussJordan(const SMat& A, bool recordOps) {
    Inverse out;
    const int n = (int)A.size();
    SMat aug(n, std::vector<Ex>(2 * n, Num(0)));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) aug[i][j] = A[i][j];
        aug[i][n + i] = Num(1);
    }
    for (int k = 0; k < n; ++k) {
        int best = -1;
        size_t bestSize = (size_t)-1;
        for (int r = k; r < n; ++r) {
            Ex v = simp(aug[r][k]);
            if (v->isZero()) continue;
            const size_t s = sizeOf(v);
            if (s < bestSize) {
                bestSize = s;
                best = r;
            }
        }
        if (best < 0) return out;  // singular
        if (best != k) {
            std::swap(aug[best], aug[k]);
            if (recordOps)
                out.ops.push_back("R" + std::to_string(k + 1) + " <-> R" +
                                  std::to_string(best + 1));
        }
        Ex pivot = simp(aug[k][k]);
        if (hasSymbols(pivot)) out.symbolic = true;
        if (!pivot->isOne()) {
            Ex inv = Pow(pivot, Num(-1));
            for (int j = 0; j < 2 * n; ++j)
                aug[k][j] = simp(Mul(aug[k][j], inv));
            if (recordOps) out.ops.push_back(normalizeText(k, pivot));
        }
        for (int r = 0; r < n; ++r) {
            if (r == k) continue;
            Ex factor = simp(aug[r][k]);
            if (factor->isZero()) continue;
            for (int j = 0; j < 2 * n; ++j)
                aug[r][j] = simp(Add(aug[r][j],
                                     Mul(Num(-1), factor, aug[k][j])));
            if (recordOps) out.ops.push_back(rowOpText(r, k, factor));
        }
    }
    out.ok = true;
    out.value.assign(n, std::vector<Ex>(n, Num(0)));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) out.value[i][j] = tidy(aug[i][n + j]);
    return out;
}

struct Lu {
    SMat L, U, P;
    std::vector<std::string> ops;
    bool zeroPivot = false;
    bool symbolic = false;
};

Lu luDecompose(SMat A) {
    Lu out;
    const int n = (int)A.size();
    out.L = identityMatrix(n);
    out.U = std::move(A);
    out.P = identityMatrix(n);
    for (int k = 0; k < n; ++k) {
        int best = -1;
        size_t bestSize = (size_t)-1;
        for (int r = k; r < n; ++r) {
            Ex v = simp(out.U[r][k]);
            if (v->isZero()) continue;
            const size_t s = sizeOf(v);
            if (s < bestSize) {
                bestSize = s;
                best = r;
            }
        }
        if (best < 0) {
            out.zeroPivot = true;
            out.ops.push_back("column " + std::to_string(k + 1) +
                              " has no pivot; U keeps a zero on the diagonal");
            continue;
        }
        if (best != k) {
            std::swap(out.U[best], out.U[k]);
            std::swap(out.P[best], out.P[k]);
            for (int j = 0; j < k; ++j) std::swap(out.L[best][j], out.L[k][j]);
            out.ops.push_back("swap R" + std::to_string(k + 1) + " and R" +
                              std::to_string(best + 1) +
                              "   (recorded in P)");
        }
        Ex pivot = simp(out.U[k][k]);
        if (hasSymbols(pivot)) out.symbolic = true;
        for (int i = k + 1; i < n; ++i) {
            Ex factor = simp(Mul(out.U[i][k], Pow(pivot, Num(-1))));
            if (factor->isZero()) continue;
            out.L[i][k] = tidy(factor);
            for (int j = k; j < n; ++j)
                out.U[i][j] = simp(Add(out.U[i][j],
                                       Mul(Num(-1), factor, out.U[k][j])));
            out.ops.push_back(rowOpText(i, k, factor));
        }
    }
    for (auto& row : out.U)
        for (auto& e : row) e = tidy(e);
    for (auto& row : out.L)
        for (auto& e : row) e = tidy(e);
    return out;
}

bool matricesEqual(const SMat& A, const SMat& B) {
    if (A.size() != B.size()) return false;
    for (size_t i = 0; i < A.size(); ++i) {
        if (A[i].size() != B[i].size()) return false;
        for (size_t j = 0; j < A[i].size(); ++j)
            if (!zeroEx(Add(A[i][j], Mul(Num(-1), B[i][j])))) return false;
    }
    return true;
}

bool isIdentity(const SMat& A) {
    for (size_t i = 0; i < A.size(); ++i)
        for (size_t j = 0; j < A[i].size(); ++j) {
            Ex want = (i == j) ? Num(1) : Num(0);
            if (!zeroEx(Add(A[i][j], Mul(Num(-1), want)))) return false;
        }
    return true;
}

bool numericMatrix(const SMat& A, std::vector<std::vector<double>>& out) {
    out.assign(A.size(), std::vector<double>(A.front().size(), 0.0));
    for (size_t i = 0; i < A.size(); ++i)
        for (size_t j = 0; j < A[i].size(); ++j) {
            if (hasSymbols(A[i][j])) return false;
            bool ok = false;
            const double v = evalReal(A[i][j], {}, &ok);
            if (!ok || !std::isfinite(v)) return false;
            out[i][j] = v;
        }
    return true;
}

bool numericValue(const Ex& e, double& out) {
    if (hasSymbols(e)) return false;
    bool ok = false;
    out = evalReal(e, {}, &ok);
    return ok && std::isfinite(out);
}

// ---------------------------------------------------------------- topics
Result linalgRank(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_rank", "r");
    SMat A;
    std::string code;
    if (!parseMatrixText(req.text("matrix", "1, 2, 3; 2, 4, 6; 1, 1, 1"),
                         A, code, 4)) {
        failCode(r, code);
        return r;
    }
    const int rows = (int)A.size();
    const int cols = (int)A.front().size();
    r.normalized = Sym("A = " + matText(A));

    addStep(r, "step.title.write_the_matrix", Sym("A = " + matText(A)));
    const Rref g = rowReduce(A, true);
    for (const auto& op : g.ops) addStep(r, "step.title.eliminate", Sym(op));
    addStep(r, "step.title.row_reduce", Sym(matText(g.m)),
            "step.note.row_operations_do_not_change_the_row_space");

    const int rank = (int)g.pivots.size();
    std::string pivotList;
    for (int p : g.pivots) {
        if (!pivotList.empty()) pivotList += ", ";
        pivotList += std::to_string(p + 1);
    }
    if (pivotList.empty()) pivotList = "none";
    addStep(r, "step.title.identify_the_pivots", Sym("pivot columns: " + pivotList),
            "step.note.pivot_columns_name_the_independent_columns");

    addStep(r, "step.title.read_the_rank",
            Sym("rank(A) = " + std::to_string(rank)));

    // Null space: one basis vector per free column.
    std::vector<int> freeCols;
    for (int c = 0; c < cols; ++c)
        if (std::find(g.pivots.begin(), g.pivots.end(), c) == g.pivots.end())
            freeCols.push_back(c);
    std::vector<std::vector<Ex>> nullBasis;
    for (int f : freeCols) {
        std::vector<Ex> v(cols, Num(0));
        v[f] = Num(1);
        for (int i = 0; i < rank; ++i)
            v[g.pivots[i]] = simp(Mul(Num(-1), g.m[i][f]));
        nullBasis.push_back(std::move(v));
    }
    std::string nullText;
    if (nullBasis.empty()) {
        nullText = "{0}";
    } else {
        for (size_t i = 0; i < nullBasis.size(); ++i) {
            if (i) nullText += ",  ";
            nullText += "v_" + std::to_string(i + 1) + " = " +
                        vectorText(nullBasis[i]);
        }
    }
    addStep(r, "step.title.find_the_null_space", Sym(nullText),
            "step.note.the_null_space_vectors_solve_a_v_0");

    std::string colText;
    for (int p : g.pivots) {
        std::vector<Ex> col;
        for (int i = 0; i < rows; ++i) col.push_back(A[i][p]);
        if (!colText.empty()) colText += ",  ";
        colText += vectorText(col);
    }
    if (colText.empty()) colText = "{0}";
    addStep(r, "step.title.find_the_column_space", Sym("{" + colText + "}"),
            "step.note.the_column_space_is_spanned_by_the_pivot_columns");

    std::string rowText;
    for (int i = 0; i < rank; ++i) {
        if (!rowText.empty()) rowText += ",  ";
        rowText += rowVectorText(g.m[i]);
    }
    if (rowText.empty()) rowText = "{0}";
    addStep(r, "step.title.find_the_row_space", Sym("{" + rowText + "}"),
            "step.note.the_row_space_is_spanned_by_the_nonzero_rows_of_the_rref");

    addStep(r, "step.title.check_the_dimension_theorem",
            Sym("rank + nullity = " + std::to_string(rank) + " + " +
                std::to_string(cols - rank) + " = " + std::to_string(cols)),
            "step.note.rank_plus_nullity_equals_the_number_of_columns");

    r.solution = Sym("rank(A) = " + std::to_string(rank) +
                     ",  nullity(A) = " + std::to_string(cols - rank));
    for (size_t i = 0; i < nullBasis.size(); ++i)
        r.functions.push_back("v_" + std::to_string(i + 1) + " = " +
                              vectorText(nullBasis[i]));

    bool nullOk = true;
    for (auto& v : nullBasis) {
        const std::vector<Ex> prod = multiplyMatrixVector(A, v);
        for (auto& e : prod)
            if (!zeroEx(e)) nullOk = false;
    }
    addCheck(r, "check.null_space_vector_satisfies_a_v_0", nullOk, "", "",
             "A v_j = 0");

    // Independent certificate for the rank: the pivot columns themselves must
    // be linearly independent, so their submatrix has full column rank.
    SMat pivotSub(rows, std::vector<Ex>(rank, Num(0)));
    for (int i = 0; i < rows; ++i)
        for (int j = 0; j < rank; ++j)
            pivotSub[i][j] = A[i][g.pivots[j]];
    const Rref pivotRank = rowReduce(pivotSub, false);
    addCheck(r, "check.pivot_columns_are_independent",
             (int)pivotRank.pivots.size() == rank, "", "",
             "rank = " + std::to_string(rank));

    if (rank < std::min(rows, cols)) r.warnings.push_back("warning.rank_deficient");
    if (g.symbolic) r.assumptions.push_back("assumption.generic_symbolic_pivots");
    return r;
}

Result linalgDet(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_det", "det");
    SMat A;
    std::string code;
    if (!parseMatrixText(req.text("matrix", "1, 2; 3, 4"), A, code, 4)) {
        failCode(r, code);
        return r;
    }
    if (A.size() != A.front().size()) {
        failCode(r, "linalg_not_square");
        return r;
    }
    r.normalized = Sym("A = " + matText(A));
    addStep(r, "step.title.write_the_matrix", Sym("A = " + matText(A)));

    const Determinant d = determinantByElimination(A);
    for (const auto& op : d.ops) addStep(r, "step.title.eliminate", Sym(op));
    addStep(r, "step.title.compute_the_determinant",
            Sym("det(A) = " + plain(d.value)),
            d.singular
                ? "step.note.a_zero_determinant_means_the_rows_are_dependent"
                : "step.note.a_nonzero_determinant_means_the_matrix_is_invertible");

    const Ex cofactor = determinantByCofactors(A);
    const bool match = zeroEx(Add(d.value, Mul(Num(-1), cofactor)));
    addCheck(r, "check.determinant_matches_cofactor_expansion", match, "", "",
             "det(A) = " + plain(cofactor));

    r.solution = Sym("det(A) = " + plain(d.value));
    if (d.singular) r.warnings.push_back("warning.singular_matrix");

    double numeric = 0.0;
    const bool alreadyExactInteger =
        d.value->isNum() && d.value->num.isInt();
    if (!alreadyExactInteger && numericValue(d.value, numeric)) {
        r.hasNumeric = true;
        r.numericValue = numeric;
        addStep(r, "step.title.numeric_value",
                Sym("det(A) = " + numStr(numeric)),
                "step.note.the_numeric_value_is_an_approximation");
    }
    if (d.symbolic) r.assumptions.push_back("assumption.generic_symbolic_pivots");
    return r;
}

Result linalgInverse(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_inverse", "A");
    SMat A;
    std::string code;
    if (!parseMatrixText(req.text("matrix", "2, 1; 1, 1"), A, code, 4)) {
        failCode(r, code);
        return r;
    }
    if (A.size() != A.front().size()) {
        failCode(r, "linalg_not_square");
        return r;
    }
    const int n = (int)A.size();
    r.normalized = Sym("A = " + matText(A));
    addStep(r, "step.title.write_the_matrix", Sym("A = " + matText(A)));

    const Determinant d = determinantByElimination(A);
    addStep(r, "step.title.compute_the_determinant",
            Sym("det(A) = " + plain(d.value)));
    if (d.value->isZero()) {
        addStep(r, "step.title.invert_the_matrix",
                Sym("A is singular, so A^{-1} does not exist"),
                "step.note.a_singular_matrix_has_no_inverse");
        r.solution = Sym("A^{-1} does not exist");
        r.warnings.push_back("warning.singular_matrix");
        return r;
    }

    const Ex det = tidy(d.value);
    bool symbolicInput = false;
    for (auto& row : A)
        for (auto& e : row)
            if (hasSymbols(e)) symbolicInput = true;

    SMat inverseMatrix;
    std::string invText;

    if (symbolicInput && n <= 4) {
        // Cofactor route: one common denominator reads better than the nested
        // fractions that symbolic row operations produce.  It stays affordable
        // up to 4x4 because every entry is a 3x3 determinant.
        SMat cof(n, std::vector<Ex>(n, Num(0)));
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) {
                SMat minor(n - 1, std::vector<Ex>(n - 1, Num(0)));
                int r2 = 0;
                for (int r = 0; r < n; ++r) {
                    if (r == i) continue;
                    int c2 = 0;
                    for (int c = 0; c < n; ++c) {
                        if (c == j) continue;
                        minor[r2][c2++] = A[r][c];
                    }
                    ++r2;
                }
                Ex value = tidy(determinantByCofactors(minor));
                if ((i + j) % 2) value = tidy(Mul(Num(-1), value));
                cof[i][j] = value;
            }
        SMat adj(n, std::vector<Ex>(n, Num(0)));
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) adj[i][j] = cof[j][i];
        addStep(r, "step.title.find_the_cofactors",
                Sym("adj(A) = " + matText(adj)),
                "step.note.the_adjugate_transposes_the_cofactor_matrix");
        inverseMatrix.assign(n, std::vector<Ex>(n, Num(0)));
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                inverseMatrix[i][j] =
                    tidy(Mul(adj[i][j], Pow(det, Num(-1))));
        if (det->isOne())
            invText = matText(adj);
        else if (det->isNum() && det->num.n == -1)
            invText = "-" + matText(adj);
        else
            invText = reciprocalFactor(det) + " * " + matText(adj);
        r.assumptions.push_back("assumption.generic_symbolic_pivots");
    } else {
        addStep(r, "step.title.augment_with_the_identity",
                Sym("A = " + matText(A) + ",  I = " +
                    matText(identityMatrix(n))));
        const Inverse inv = inverseByGaussJordan(A, true);
        for (const auto& op : inv.ops)
            addStep(r, "step.title.eliminate", Sym(op));
        if (!inv.ok) {
            addStep(r, "step.title.invert_the_matrix",
                    Sym("A is singular, so A^{-1} does not exist"),
                    "step.note.a_singular_matrix_has_no_inverse");
            r.solution = Sym("A^{-1} does not exist");
            r.warnings.push_back("warning.singular_matrix");
            return r;
        }
        inverseMatrix = inv.value;
        invText = matText(inv.value);
        // 4x4 symbolic: factor the determinant out when the adjugate stays
        // polynomial; otherwise the Gauss-Jordan form is the honest one.
        SMat adj(n, std::vector<Ex>(n, Num(0)));
        bool factored = true;
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) {
                adj[i][j] = tidy(Mul(det, inv.value[i][j]));
                if (hasDivision(adj[i][j])) factored = false;
            }
        if (factored && hasSymbols(det) && !det->isOne())
            invText = reciprocalFactor(det) + " * " + matText(adj);
        if (d.symbolic || inv.symbolic)
            r.assumptions.push_back("assumption.generic_symbolic_pivots");
    }

    addStep(r, "step.title.invert_the_matrix",
            Sym("A^{-1} = " + invText),
            "step.note.the_inverse_is_read_off_the_right_half");
    const SMat product = multiplyMatrices(A, inverseMatrix);
    const bool identity = isIdentity(product);
    addCheck(r, "check.inverse_times_the_matrix_is_the_identity", identity, "", "",
             "A A^{-1} = I");

    r.solution = Sym("A^{-1} = " + invText);

    bool allInteger = true;
    for (auto& row : inverseMatrix)
        for (auto& e : row)
            if (!e->isNum() || !e->num.isInt()) allInteger = false;
    std::vector<std::vector<double>> numeric;
    if (!allInteger && numericMatrix(inverseMatrix, numeric)) {
        std::string text = "[";
        for (size_t i = 0; i < numeric.size(); ++i) {
            if (i) text += "; ";
            text += "[";
            for (size_t j = 0; j < numeric[i].size(); ++j) {
                if (j) text += ", ";
                text += numStr(numeric[i][j], 6);
            }
            text += "]";
        }
        text += "]";
        addStep(r, "step.title.numeric_value", Sym("A^{-1} = " + text),
                "step.note.the_numeric_value_is_an_approximation");
    }
    return r;
}

Result linalgLu(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_lu", "L");
    SMat A;
    std::string code;
    if (!parseMatrixText(req.text("matrix", "4, 3; 6, 3"), A, code, 4)) {
        failCode(r, code);
        return r;
    }
    if (A.size() != A.front().size()) {
        failCode(r, "linalg_not_square");
        return r;
    }
    r.normalized = Sym("A = " + matText(A));
    addStep(r, "step.title.write_the_matrix", Sym("A = " + matText(A)));

    const Lu lu = luDecompose(A);
    for (const auto& op : lu.ops) addStep(r, "step.title.eliminate", Sym(op));
    addStep(r, "step.title.build_l_and_u",
            Sym("L = " + matText(lu.L) + ",  U = " + matText(lu.U)),
            "step.note.l_is_unit_lower_triangular_and_u_is_upper_triangular");
    addStep(r, "step.title.choose_the_pivot", Sym("P = " + matText(lu.P)),
            "step.note.the_permutation_records_the_row_swaps");

    const SMat PA = multiplyMatrices(lu.P, A);
    const SMat LU = multiplyMatrices(lu.L, lu.U);
    addCheck(r, "check.lu_reconstructs_the_matrix", matricesEqual(PA, LU), "", "",
             "P A = L U");

    r.solution = Sym("L = " + matText(lu.L) + ",  U = " + matText(lu.U) +
                     ",  P = " + matText(lu.P));
    r.functions.push_back("L = " + matText(lu.L));
    r.functions.push_back("U = " + matText(lu.U));
    r.functions.push_back("P = " + matText(lu.P));

    if (lu.zeroPivot) {
        r.warnings.push_back("warning.lu_with_zero_pivot");
        r.warnings.push_back("warning.singular_matrix");
    }
    if (lu.symbolic) r.assumptions.push_back("assumption.generic_symbolic_pivots");
    return r;
}

// ---------------------------------------------------------------- vector sets
// The rows of V are the vectors; the columns of the returned matrix are the
// same vectors, the shape row reduction wants.
SMat columnMatrix(const SMat& V) {
    if (V.empty()) return {};
    const int m = (int)V.size();
    const int n = (int)V.front().size();
    SMat A(n, std::vector<Ex>(m, Num(0)));
    for (int i = 0; i < m; ++i)
        for (int j = 0; j < n; ++j) A[j][i] = V[i][j];
    return A;
}

int vectorSetRank(const SMat& V) {
    if (V.empty()) return 0;
    return (int)rowReduce(columnMatrix(V), false).pivots.size();
}

std::string vectorListText(const SMat& V, const std::string& prefix) {
    std::string s;
    for (size_t i = 0; i < V.size(); ++i) {
        if (i) s += ",   ";
        s += prefix + std::to_string(i + 1) + " = " + vectorText(V[i]);
    }
    return s;
}

std::string basisText(const std::vector<std::vector<Ex>>& basis) {
    if (basis.empty()) return "{ }";
    std::string s = "{";
    for (size_t i = 0; i < basis.size(); ++i) {
        if (i) s += ", ";
        s += vectorText(basis[i]);
    }
    return s + "}";
}

// c_1 v_1 + ... + c_m v_m with the signs folded into the text.
std::string combinationText(const std::vector<Ex>& c, const std::string& prefix) {
    std::string out;
    for (size_t i = 0; i < c.size(); ++i) {
        Ex v = simp(c[i]);
        if (v->isZero()) continue;
        bool neg = false;
        Ex mag = v;
        if (v->isNum() && v->num.isNeg()) {
            neg = true;
            mag = Num(v->num.abs());
        } else if (v->isMul() && v->args.size() == 2 && v->args[0]->isNum() &&
                   v->args[0]->num.n == -1) {
            neg = true;
            mag = v->args[1];
        }
        std::string term;
        if (!mag->isOne()) {
            std::string m = toPlain(mag);
            if (!mag->isNum() && !mag->isSym()) m = "(" + m + ")";
            term = m + " ";
        }
        term += prefix + std::to_string(i + 1);
        if (out.empty())
            out = (neg ? "-" : "") + term;
        else
            out += (neg ? " - " : " + ") + term;
    }
    return out.empty() ? "0" : out;
}

std::vector<Ex> linearCombination(const SMat& V, const std::vector<Ex>& c) {
    std::vector<Ex> out(V.front().size(), Num(0));
    for (size_t i = 0; i < V.size() && i < c.size(); ++i) {
        if (zeroFast(c[i])) continue;
        for (size_t j = 0; j < out.size(); ++j)
            out[j] = simp(Add(out[j], Mul(c[i], V[i][j])));
    }
    return out;
}

// One combination of the vectors that equals the target, or a report that no
// combination exists.  Free coordinates are set to zero.
struct Combination {
    bool found = false;
    std::vector<Ex> coeffs;
    SMat augmented;
    SMat rref;
    std::vector<int> pivots;
    std::vector<std::string> ops;
};

Combination combineToTarget(const SMat& V, const std::vector<Ex>& b) {
    Combination out;
    const SMat A = columnMatrix(V);
    const int n = (int)A.size();
    const int m = (int)A.front().size();
    SMat aug(n, std::vector<Ex>(m + 1, Num(0)));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < m; ++j) aug[i][j] = A[i][j];
        aug[i][m] = b[i];
    }
    out.augmented = aug;
    const Rref g = rowReduce(aug, true);
    out.rref = g.m;
    out.pivots = g.pivots;
    out.ops = g.ops;
    for (int i = 0; i < n; ++i) {
        bool zeroCoeffs = true;
        for (int j = 0; j < m; ++j)
            if (!zeroFast(out.rref[i][j])) zeroCoeffs = false;
        if (zeroCoeffs && !zeroFast(out.rref[i][m])) return out;
    }
    for (int p : out.pivots)
        if (p == m) return out;
    out.found = true;
    out.coeffs.assign(m, Num(0));
    for (size_t i = 0; i < out.pivots.size() && (int)i < n; ++i) {
        const int p = out.pivots[i];
        if (p < m) out.coeffs[p] = out.rref[i][m];
    }
    return out;
}

// Numeric read-out for a coordinate vector when it is not made of integers.
void addNumericVectorStep(Result& r, const std::vector<Ex>& v,
                          const std::string& label) {
    std::vector<double> values;
    bool allInteger = true;
    for (const auto& e : v) {
        if (!e->isNum() || !e->num.isInt()) allInteger = false;
        if (hasSymbols(e)) return;
        bool ok = false;
        const double x = evalReal(e, {}, &ok);
        if (!ok || !std::isfinite(x)) return;
        values.push_back(x);
    }
    if (allInteger) return;
    std::string text = "(";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i) text += ", ";
        text += numStr(values[i], 6);
    }
    text += ")";
    addStep(r, "step.title.numeric_value", Sym(label + " = " + text),
            "step.note.the_numeric_value_is_an_approximation");
}

std::string vectorNamesText(int count) {
    if (count <= 0) return "no vectors";
    if (count == 1) return "v_1";
    if (count == 2) return "v_1, v_2";
    return "v_1, ..., v_" + std::to_string(count);
}

std::string spanNameText(int count) {
    if (count <= 0) return "span{ }";
    if (count == 1) return "span{v_1}";
    if (count == 2) return "span{v_1, v_2}";
    return "span{v_1, ..., v_" + std::to_string(count) + "}";
}

bool isIndependentSet(const std::vector<std::vector<Ex>>& basis) {
    SMat V(basis.begin(), basis.end());
    return vectorSetRank(V) == (int)basis.size();
}

// ---------------------------------------------------------------- span
Result linalgSpan(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_span", "c");
    SMat V;
    std::string code;
    if (!parseMatrixText(req.text("vectors", "1, 0; 0, 1"), V, code, 6)) {
        failCode(r, code);
        return r;
    }
    const int count = (int)V.size();
    const int dim = (int)V.front().size();
    addStep(r, "step.title.write_the_vectors", Sym(vectorListText(V, "v_")),
            "step.note.the_columns_of_the_matrix_are_the_vectors");

    const SMat A = columnMatrix(V);
    const Rref g = rowReduce(A, false);
    const int rank = (int)g.pivots.size();
    std::vector<std::vector<Ex>> basis;
    for (int p : g.pivots) basis.push_back(V[p]);
    if (g.symbolic) r.assumptions.push_back("assumption.generic_symbolic_pivots");

    addStep(r, "step.title.build_the_column_matrix", Sym("B = " + matText(A)),
            "step.note.the_columns_of_the_matrix_are_the_vectors");
    addStep(r, "step.title.extract_a_basis", Sym("basis = " + basisText(basis)),
            "step.note.the_pivot_vectors_are_a_maximal_independent_subset");
    addStep(r, "step.title.read_the_rank",
            Sym("dim span = " + std::to_string(rank)));

    const std::string spanName = spanNameText(count);
    const std::string targetText = req.text("target", "");
    if (targetText.empty()) {
        r.solution = Sym(spanName + " has dimension " +
                         std::to_string(rank) +
                         ",  basis = " + basisText(basis));
        addCheck(r, "check.span_basis_is_independent",
                 vectorSetRank(basis) == rank, "", "",
                 "rank = " + std::to_string(rank));
        return r;
    }

    std::vector<Ex> b;
    if (!parseVectorText(targetText, b, code)) {
        failCode(r, code);
        return r;
    }
    if ((int)b.size() != dim) {
        failCode(r, "linalg_dimension_mismatch");
        return r;
    }

    const Combination c = combineToTarget(V, b);
    addStep(r, "step.title.augment_with_the_target",
            Sym(matText(c.augmented)),
            "step.note.the_augmented_system_tests_membership");
    for (const auto& op : c.ops) addStep(r, "step.title.eliminate", Sym(op));
    addStep(r, "step.title.row_reduce", Sym(matText(c.rref)));

    if (!c.found) {
        addStep(r, "step.title.decide_membership",
                Sym("b is not in " + spanName),
                "step.note.a_pivot_in_the_last_column_means_no_solution");
        r.solution = Sym("b is not in " + spanName +
                         ",  dim span = " + std::to_string(rank));
        r.warnings.push_back("warning.not_in_the_span");
    } else {
        addStep(r, "step.title.solve_for_the_coefficients",
                Sym("b = " + combinationText(c.coeffs, "v_")),
                "step.note.free_coordinates_are_set_to_zero");
        const std::vector<Ex> rebuilt = linearCombination(V, c.coeffs);
        bool ok = true;
        for (int j = 0; j < dim; ++j)
            if (!zeroEx(Add(rebuilt[j], Mul(Num(-1), b[j])))) ok = false;
        addCheck(r, "check.the_combination_reproduces_the_target", ok, "", "",
                 "b = " + combinationText(c.coeffs, "v_"));
        addNumericVectorStep(r, c.coeffs, "c");
        r.solution = Sym("b = " + combinationText(c.coeffs, "v_") +
                         "  lies in " + spanName +
                         ",  dim span = " + std::to_string(rank));
    }
    addCheck(r, "check.span_basis_is_independent",
             vectorSetRank(basis) == rank, "", "",
             "rank = " + std::to_string(rank));
    return r;
}

// ---------------------------------------------------------------- independence
Result linalgIndependence(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_independence", "c");
    SMat V;
    std::string code;
    if (!parseMatrixText(req.text("vectors", "1, 2; 2, 4"), V, code, 6)) {
        failCode(r, code);
        return r;
    }
    const int count = (int)V.size();
    const int dim = (int)V.front().size();
    addStep(r, "step.title.write_the_vectors", Sym(vectorListText(V, "v_")),
            "step.note.the_columns_of_the_matrix_are_the_vectors");

    const SMat A = columnMatrix(V);
    const Rref g = rowReduce(A, true);
    for (const auto& op : g.ops) addStep(r, "step.title.eliminate", Sym(op));
    addStep(r, "step.title.row_reduce", Sym(matText(g.m)));
    const int rank = (int)g.pivots.size();
    if (g.symbolic) r.assumptions.push_back("assumption.generic_symbolic_pivots");
    addStep(r, "step.title.read_the_rank",
            Sym("rank = " + std::to_string(rank) + ",  vectors = " +
                std::to_string(count)));

    std::vector<std::vector<Ex>> basis;
    for (int p : g.pivots) basis.push_back(V[p]);

    if (rank == count) {
        addStep(r, "step.title.decide_independence",
                Sym("the vectors are linearly independent"),
                "step.note.the_pivot_vectors_are_a_maximal_independent_subset");
        r.solution = Sym(vectorNamesText(count) + " are linearly independent");
    } else {
        int freeCol = -1;
        for (int c = 0; c < count; ++c)
            if (std::find(g.pivots.begin(), g.pivots.end(), c) ==
                g.pivots.end()) {
                freeCol = c;
                break;
            }
        std::vector<Ex> relation(count, Num(0));
        relation[freeCol] = Num(1);
        for (int i = 0; i < rank; ++i)
            relation[g.pivots[i]] = simp(Mul(Num(-1), g.m[i][freeCol]));
        const std::string rel = combinationText(relation, "v_") + " = 0";
        addStep(r, "step.title.find_a_dependence_relation", Sym(rel),
                "step.note.a_free_column_gives_a_dependence_relation");

        const std::vector<Ex> rebuilt = linearCombination(V, relation);
        bool ok = true;
        for (int j = 0; j < dim; ++j)
            if (!zeroEx(rebuilt[j])) ok = false;
        addCheck(r, "check.dependence_relation_vanishes", ok, "", "", rel);
        r.solution = Sym(vectorNamesText(count) + " are linearly dependent: " +
                         rel);
        r.warnings.push_back("warning.linearly_dependent");
    }
    addCheck(r, "check.span_basis_is_independent",
             vectorSetRank(basis) == rank, "", "",
             "rank = " + std::to_string(rank));
    return r;
}

// ---------------------------------------------------------------- basis
Result linalgBasis(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_basis", "b");
    SMat V;
    std::string code;
    if (!parseMatrixText(req.text("vectors", "1, 2, 3; 2, 4, 6; 1, 1, 1"), V,
                         code, 6)) {
        failCode(r, code);
        return r;
    }
    const int count = (int)V.size();
    addStep(r, "step.title.write_the_vectors", Sym(vectorListText(V, "v_")),
            "step.note.the_columns_of_the_matrix_are_the_vectors");

    const SMat A = columnMatrix(V);
    const Rref g = rowReduce(A, true);
    for (const auto& op : g.ops) addStep(r, "step.title.eliminate", Sym(op));
    addStep(r, "step.title.row_reduce", Sym(matText(g.m)));
    const int rank = (int)g.pivots.size();
    if (g.symbolic) r.assumptions.push_back("assumption.generic_symbolic_pivots");
    std::string pivotNames;
    for (int p : g.pivots) {
        if (!pivotNames.empty()) pivotNames += ", ";
        pivotNames += "v_" + std::to_string(p + 1);
    }
    if (pivotNames.empty()) pivotNames = "none";
    addStep(r, "step.title.identify_the_pivots",
            Sym("pivot vectors: " + pivotNames),
            "step.note.pivot_columns_name_the_independent_columns");

    std::vector<std::vector<Ex>> basis;
    for (int p : g.pivots) basis.push_back(V[p]);
    addStep(r, "step.title.extract_a_basis", Sym("basis = " + basisText(basis)),
            "step.note.the_pivot_vectors_are_a_maximal_independent_subset");

    // Spanning: adding every original vector as a column cannot raise the rank.
    SMat all = columnMatrix(basis);
    if (all.empty()) all.assign((size_t)V.front().size(), {});
    for (int i = 0; i < count; ++i)
        for (int j = 0; j < (int)all.size(); ++j) all[j].push_back(V[i][j]);
    const int withAll = (int)rowReduce(all, false).pivots.size();

    addCheck(r, "check.basis_vectors_are_independent", isIndependentSet(basis),
             "", "", "rank = " + std::to_string(rank));
    addCheck(r, "check.basis_spans_the_vectors", withAll == rank, "", "",
             "rank = " + std::to_string(rank));

    if (rank == 0) {
        addStep(r, "step.title.describe_the_subspace",
                Sym("every vector is zero, so the span is {0}"));
        r.solution = Sym("basis = { },  dim = 0");
    } else {
        addStep(r, "step.title.describe_the_subspace",
                Sym("dim = " + std::to_string(rank) + ",  basis = " +
                    basisText(basis)));
        r.solution = Sym("basis = " + basisText(basis) + ",  dim = " +
                         std::to_string(rank));
    }
    for (size_t i = 0; i < basis.size(); ++i)
        r.functions.push_back("b_" + std::to_string(i + 1) + " = " +
                              vectorText(basis[i]));
    return r;
}

// ---------------------------------------------------------------- coordinates
Result linalgCoordinates(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_coordinates", "c");
    SMat B;
    std::string code;
    if (!parseMatrixText(req.text("basis", "1, 0; 0, 1"), B, code, 6)) {
        failCode(r, code);
        return r;
    }
    const int n = (int)B.size();
    if ((int)B.front().size() != n) {
        failCode(r, "linalg_not_a_basis");
        return r;
    }
    std::vector<Ex> v;
    if (!parseVectorText(req.text("vector", "3, 5"), v, code)) {
        failCode(r, code);
        return r;
    }
    if ((int)v.size() != n) {
        failCode(r, "linalg_dimension_mismatch");
        return r;
    }

    addStep(r, "step.title.write_the_basis", Sym(vectorListText(B, "b_")),
            "step.note.coordinates_solve_b_c_equals_v");
    const SMat Bc = columnMatrix(B);
    const Determinant d = determinantByElimination(Bc);
    if (d.value->isZero()) {
        failCode(r, "linalg_not_a_basis");
        return r;
    }
    if (d.symbolic) r.assumptions.push_back("assumption.generic_symbolic_pivots");

    const Combination c = combineToTarget(B, v);
    addStep(r, "step.title.augment_with_the_target",
            Sym(matText(c.augmented)),
            "step.note.coordinates_solve_b_c_equals_v");
    for (const auto& op : c.ops) addStep(r, "step.title.eliminate", Sym(op));
    addStep(r, "step.title.row_reduce", Sym(matText(c.rref)));
    if (!c.found) {
        failCode(r, "linalg_not_a_basis");
        return r;
    }
    addStep(r, "step.title.read_the_coordinates",
            Sym("[v]_B = " + tupleText(c.coeffs)),
            "step.note.coordinates_solve_b_c_equals_v");

    const std::vector<Ex> rebuilt = linearCombination(B, c.coeffs);
    bool ok = true;
    for (int j = 0; j < n; ++j)
        if (!zeroEx(Add(rebuilt[j], Mul(Num(-1), v[j])))) ok = false;
    addCheck(r, "check.coordinates_rebuild_the_vector", ok, "", "",
             "v = " + combinationText(c.coeffs, "b_"));
    addNumericVectorStep(r, c.coeffs, "[v]_B");
    r.solution = Sym("[v]_B = " + tupleText(c.coeffs));
    for (int i = 0; i < n; ++i)
        r.functions.push_back("c_" + std::to_string(i + 1) + " = " +
                              toPlain(tidy(c.coeffs[i])));
    return r;
}

// ---------------------------------------------------------------- change of basis
Result linalgChangeBasis(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_change_basis", "P");
    SMat B, C;
    std::string code;
    if (!parseMatrixText(req.text("basis_b", "1, 0; 0, 1"), B, code, 6)) {
        failCode(r, code);
        return r;
    }
    if (!parseMatrixText(req.text("basis_c", "1, 1; 0, 1"), C, code, 6)) {
        failCode(r, code);
        return r;
    }
    const int n = (int)B.size();
    if ((int)B.front().size() != n || (int)C.size() != n ||
        (int)C.front().size() != n) {
        failCode(r, "linalg_not_a_basis");
        return r;
    }
    addStep(r, "step.title.write_the_basis",
            Sym("B: " + vectorListText(B, "b_") + ",   C: " +
                vectorListText(C, "c_")),
            "step.note.coordinates_solve_b_c_equals_v");

    const SMat Bc = columnMatrix(B), Cc = columnMatrix(C);
    const Determinant db = determinantByElimination(Bc);
    const Determinant dc = determinantByElimination(Cc);
    if (db.value->isZero() || dc.value->isZero()) {
        failCode(r, "linalg_not_a_basis");
        return r;
    }
    if (db.symbolic || dc.symbolic)
        r.assumptions.push_back("assumption.generic_symbolic_pivots");

    // [C | B] reduces to [I | C^{-1} B], so the right half is the transition
    // matrix without a separate inversion.
    SMat aug(n, std::vector<Ex>(2 * n, Num(0)));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            aug[i][j] = Cc[i][j];
            aug[i][n + j] = Bc[i][j];
        }
    }
    const Rref g = rowReduce(aug, true);
    for (const auto& op : g.ops) addStep(r, "step.title.eliminate", Sym(op));
    addStep(r, "step.title.build_the_transition_matrix", Sym(matText(g.m)),
            "step.note.the_transition_matrix_columns_are_c_coordinates");

    SMat P(n, std::vector<Ex>(n, Num(0)));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) P[i][j] = g.m[i][n + j];

    addCheck(r, "check.transition_matrix_maps_b_to_c",
             matricesEqual(multiplyMatrices(Cc, P), Bc), "", "", "C P = B");
    r.solution = Sym("P_{C<-B} = " + matText(P));
    r.functions.push_back("P = " + matText(P));
    return r;
}

// ---------------------------------------------------------------- subspace
Result linalgSubspace(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_subspace", "W");
    SMat V;
    std::string code;
    if (!parseMatrixText(req.text("vectors", "1, 0; 0, 1"), V, code, 6)) {
        failCode(r, code);
        return r;
    }
    const int count = (int)V.size();
    const int dim = (int)V.front().size();
    addStep(r, "step.title.write_the_vectors", Sym(vectorListText(V, "v_")),
            "step.note.the_columns_of_the_matrix_are_the_vectors");

    const SMat A = columnMatrix(V);
    const Rref g = rowReduce(A, false);
    const int rank = (int)g.pivots.size();
    std::vector<std::vector<Ex>> basis;
    for (int p : g.pivots) basis.push_back(V[p]);
    if (g.symbolic) r.assumptions.push_back("assumption.generic_symbolic_pivots");

    // 0 = 0 v_1 + ... + 0 v_m.
    addStep(r, "step.title.show_the_zero_vector",
            Sym("0 = 0 v_1 + ... + 0 v_" + std::to_string(count)),
            "step.note.a_subspace_contains_zero_and_is_closed_under_both_operations");
    const std::vector<Ex> zero(dim, Num(0));
    addCheck(r, "check.zero_is_in_the_subspace",
             combineToTarget(V, zero).found, "", "", "0 = 0 v_1 + ... + 0 v_m");

    // Closure under addition with u = v_1 and w = v_2 (or v_1 when there is
    // only one vector).
    std::vector<Ex> ucoef(count, Num(0)), wcoef(count, Num(0));
    ucoef[0] = Num(1);
    wcoef[count > 1 ? 1 : 0] = Num(1);
    std::vector<Ex> sumcoef(count, Num(0));
    for (int i = 0; i < count; ++i) sumcoef[i] = Add(ucoef[i], wcoef[i]);
    const std::vector<Ex> u = linearCombination(V, ucoef);
    const std::vector<Ex> w = linearCombination(V, wcoef);
    std::vector<Ex> sum(dim, Num(0));
    for (int j = 0; j < dim; ++j) sum[j] = simp(Add(u[j], w[j]));
    addStep(r, "step.title.show_closure_under_addition",
            Sym("u + w = " + combinationText(sumcoef, "v_")),
            "step.note.closure_follows_by_adding_the_coefficients");
    addCheck(r, "check.closed_under_addition", combineToTarget(V, sum).found,
             "", "", "u + w = " + combinationText(sumcoef, "v_"));

    // Closure under scalar multiplication with c = 2.
    std::vector<Ex> scaledcoef(count, Num(0));
    for (int i = 0; i < count; ++i) scaledcoef[i] = Mul(Num(2), ucoef[i]);
    const std::vector<Ex> scaled = linearCombination(V, scaledcoef);
    addStep(r, "step.title.show_closure_under_scalar_multiplication",
            Sym("2 u = " + combinationText(scaledcoef, "v_")),
            "step.note.scaling_multiplies_the_coefficients");
    addCheck(r, "check.closed_under_scalar_multiplication",
             combineToTarget(V, scaled).found, "", "",
             "2 u = " + combinationText(scaledcoef, "v_"));

    addStep(r, "step.title.extract_a_basis", Sym("basis = " + basisText(basis)),
            "step.note.the_pivot_vectors_are_a_maximal_independent_subset");
    addStep(r, "step.title.describe_the_subspace",
            Sym("dim W = " + std::to_string(rank) + ",  basis = " +
                basisText(basis)));
    r.solution = Sym("W is a subspace,  dim W = " + std::to_string(rank) +
                     ",  basis = " + basisText(basis));
    for (size_t i = 0; i < basis.size(); ++i)
        r.functions.push_back("b_" + std::to_string(i + 1) + " = " +
                              vectorText(basis[i]));
    return r;
}

// ---------------------------------------------------------------- applications
Ex dotProduct(const std::vector<Ex>& a, const std::vector<Ex>& b) {
    Ex s = Num(0);
    for (size_t i = 0; i < a.size() && i < b.size(); ++i)
        s = Add(s, Mul(a[i], b[i]));
    return simp(s);
}

std::vector<Ex> scaleVector(const std::vector<Ex>& v, const Ex& c) {
    std::vector<Ex> out(v.size(), Num(0));
    for (size_t i = 0; i < v.size(); ++i) out[i] = simp(Mul(c, v[i]));
    return out;
}

std::vector<Ex> subtractVectors(const std::vector<Ex>& a,
                                const std::vector<Ex>& b) {
    std::vector<Ex> out(a.size(), Num(0));
    for (size_t i = 0; i < a.size(); ++i)
        out[i] = simp(Add(a[i], Mul(Num(-1), b[i])));
    return out;
}

// Deep-clean a vector before it feeds a dot product.  `simp` is enough for
// numbers, but a symbolic residual like v - B c only cancels after the strong
// simplifier, and the norm must see the cancelled entries.
std::vector<Ex> tidyVector(const std::vector<Ex>& v) {
    std::vector<Ex> out(v.size(), Num(0));
    for (size_t i = 0; i < v.size(); ++i) out[i] = tidy(v[i]);
    return out;
}

std::vector<Ex> columnVector(const SMat& A, int j) {
    std::vector<Ex> out(A.size(), Num(0));
    for (size_t i = 0; i < A.size(); ++i) out[i] = A[i][j];
    return out;
}

SMat transposeMatrix(const SMat& A) {
    if (A.empty()) return {};
    SMat T(A.front().size(), std::vector<Ex>(A.size(), Num(0)));
    for (size_t i = 0; i < A.size(); ++i)
        for (size_t j = 0; j < A[i].size(); ++j) T[j][i] = A[i][j];
    return T;
}

bool zeroVector(const std::vector<Ex>& v) {
    for (const auto& e : v)
        if (!zeroFast(e)) return false;
    return true;
}

std::string vectorListTuple(const std::vector<std::vector<Ex>>& vs) {
    if (vs.empty()) return "{ }";
    std::string s = "{";
    for (size_t i = 0; i < vs.size(); ++i) {
        if (i) s += ", ";
        s += tupleText(vs[i]);
    }
    return s + "}";
}

// Solve the square system G x = b and return the reduced augmented matrix.
bool solveSquareSystem(const SMat& G, const std::vector<Ex>& b,
                       std::vector<Ex>& x, Rref& g) {
    const int n = (int)G.size();
    SMat aug(n, std::vector<Ex>(n + 1, Num(0)));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) aug[i][j] = G[i][j];
        aug[i][n] = b[i];
    }
    g = rowReduce(aug, true);
    for (int i = 0; i < n; ++i) {
        bool zeroRow = true;
        for (int j = 0; j < n; ++j)
            if (!zeroFast(g.m[i][j])) zeroRow = false;
        if (zeroRow && !zeroFast(g.m[i][n])) return false;
    }
    if ((int)g.pivots.size() != n) return false;
    x.assign(n, Num(0));
    for (int i = 0; i < n; ++i) {
        const int p = g.pivots[i];
        if (p >= n) return false;
        x[p] = g.m[i][n];
    }
    return true;
}

SMat gramMatrix(const SMat& B) {
    const int n = (int)B.size();
    const int k = (int)B.front().size();
    SMat G(k, std::vector<Ex>(k, Num(0)));
    for (int i = 0; i < k; ++i)
        for (int j = 0; j < k; ++j)
            G[i][j] = dotProduct(columnVector(B, i), columnVector(B, j));
    return G;
}

std::vector<Ex> matrixTransposeVector(const SMat& A,
                                      const std::vector<Ex>& v) {
    std::vector<Ex> out(A.front().size(), Num(0));
    for (int j = 0; j < (int)A.front().size(); ++j) {
        Ex s = Num(0);
        for (int i = 0; i < (int)A.size(); ++i) s = Add(s, Mul(A[i][j], v[i]));
        out[j] = simp(s);
    }
    return out;
}

// ---------------------------------------------------------------- projection
Result linalgProjection(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_projection", "p");
    SMat V;
    std::string code;
    if (!parseMatrixText(req.text("basis", "1, 0; 0, 1"), V, code, 6)) {
        failCode(r, code);
        return r;
    }
    std::vector<Ex> v;
    if (!parseVectorText(req.text("vector", "3, 4"), v, code)) {
        failCode(r, code);
        return r;
    }
    const int n = (int)V.front().size();
    if ((int)v.size() != n) {
        failCode(r, "linalg_dimension_mismatch");
        return r;
    }

    addStep(r, "step.title.write_the_vectors", Sym(vectorListText(V, "b_")),
            "step.note.the_columns_of_the_matrix_are_the_vectors");
    addStep(r, "step.title.write_the_vectors",
            Sym("v = " + tupleText(v)),
            "step.note.the_normal_equations_make_the_residual_orthogonal");

    const SMat B = columnMatrix(V);
    const SMat G = gramMatrix(B);
    const std::vector<Ex> rhs = matrixTransposeVector(B, v);
    addStep(r, "step.title.build_the_gram_matrix",
            Sym("B^T B = " + matText(G) + ",   B^T v = " +
                tupleText(rhs)),
            "step.note.the_normal_equations_make_the_residual_orthogonal");

    std::vector<Ex> c;
    Rref g;
    if (!solveSquareSystem(G, rhs, c, g)) {
        failCode(r, "linalg_dependent_basis");
        return r;
    }
    if (g.symbolic) r.assumptions.push_back("assumption.generic_symbolic_pivots");
    for (const auto& op : g.ops) addStep(r, "step.title.eliminate", Sym(op));
    addStep(r, "step.title.solve_the_normal_equations",
            Sym("c = " + tupleText(c)),
            "step.note.the_normal_equations_make_the_residual_orthogonal");

    const std::vector<Ex> p = tidyVector(linearCombination(V, c));
    const std::vector<Ex> residual = tidyVector(subtractVectors(v, p));
    addStep(r, "step.title.compute_the_projection",
            Sym("proj_W(v) = " + tupleText(p)),
            "step.note.the_projection_is_the_closest_point_in_the_subspace");
    addStep(r, "step.title.compute_the_residual",
            Sym("v - proj_W(v) = " + tupleText(residual)),
            "step.note.the_residual_is_orthogonal_to_every_basis_vector");

    bool orthogonal = true;
    for (int i = 0; i < (int)V.size(); ++i) {
        const Ex d = dotProduct(columnVector(B, i), residual);
        if (!zeroEx(d)) orthogonal = false;
    }
    addCheck(r, "check.residual_is_orthogonal_to_the_basis", orthogonal, "", "",
             "B^T (v - p) = 0");
    addCheck(r, "check.projection_lies_in_the_span",
             combineToTarget(V, p).found, "", "", "p = B c");

    const Ex norm2 = dotProduct(residual, residual);
    const Ex norm = simp(Sqrt(norm2));
    addStep(r, "step.title.compute_the_residual",
            Sym("||v - proj_W(v)|| = " + plain(norm)),
            "step.note.the_norm_measures_the_distance_to_the_subspace");

    r.solution = Sym("proj_W(v) = " + tupleText(p) +
                     ",   v - proj_W(v) = " + tupleText(residual) +
                     ",   distance = " + plain(norm));
    addNumericVectorStep(r, p, "proj_W(v)");
    return r;
}

// ---------------------------------------------------------------- Gram–Schmidt
Result linalgGramSchmidt(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_gram_schmidt", "q");
    SMat V;
    std::string code;
    if (!parseMatrixText(req.text("vectors", "1, 0; 1, 1; 0, 1"), V, code, 6)) {
        failCode(r, code);
        return r;
    }
    addStep(r, "step.title.write_the_vectors", Sym(vectorListText(V, "v_")),
            "step.note.the_columns_of_the_matrix_are_the_vectors");

    std::vector<std::vector<Ex>> orthogonal;
    std::vector<std::vector<Ex>> orthonormal;
    bool dependent = false;
    for (size_t k = 0; k < V.size(); ++k) {
        std::vector<Ex> u = V[k];
        for (const auto& q : orthogonal) {
            const Ex denom = dotProduct(q, q);
            if (zeroEx(denom)) continue;
            const Ex coeff = simp(Mul(dotProduct(V[k], q), Pow(denom, Num(-1))));
            u = subtractVectors(u, scaleVector(q, coeff));
        }
        u = tidyVector(u);
        if (zeroVector(u)) {
            dependent = true;
            addStep(r, "step.title.orthogonalize_each_vector",
                    Sym("v_" + std::to_string(k + 1) +
                        " is a linear combination of the previous vectors"),
                    "step.note.subtract_the_projection_onto_the_previous_vectors");
            continue;
        }
        addStep(r, "step.title.orthogonalize_each_vector",
                Sym("u_" + std::to_string(orthogonal.size() + 1) + " = " +
                    tupleText(u)),
                "step.note.subtract_the_projection_onto_the_previous_vectors");
        orthogonal.push_back(u);
    }
    if (dependent) r.warnings.push_back("warning.linearly_dependent");

    for (const auto& u : orthogonal) {
        const Ex denom = dotProduct(u, u);
        const Ex norm = simp(Sqrt(denom));
        orthonormal.push_back(scaleVector(u, Pow(norm, Num(-1))));
    }

    addStep(r, "step.title.normalize_the_basis",
            Sym("orthogonal: " + vectorListTuple(orthogonal) +
                ",   orthonormal: " + vectorListTuple(orthonormal)),
            "step.note.the_normalized_vectors_have_unit_length");

    bool orthogonalOk = true;
    for (size_t i = 0; i < orthogonal.size(); ++i)
        for (size_t j = i + 1; j < orthogonal.size(); ++j)
            if (!zeroEx(dotProduct(orthogonal[i], orthogonal[j])))
                orthogonalOk = false;
    addCheck(r, "check.orthogonal_basis_vectors_are_orthogonal", orthogonalOk,
             "", "", "u_i . u_j = 0 for i != j");

    bool unitOk = true;
    for (const auto& q : orthonormal)
        if (!zeroEx(Add(dotProduct(q, q), Num(-1)))) unitOk = false;
    addCheck(r, "check.orthonormal_vectors_have_unit_length", unitOk, "", "",
             "q_i . q_i = 1");

    bool spans = vectorSetRank(SMat(orthogonal.begin(), orthogonal.end())) ==
                 vectorSetRank(V);
    addCheck(r, "check.basis_spans_the_vectors", spans, "", "",
             "rank = " + std::to_string(vectorSetRank(V)));

    r.solution = Sym("orthogonal basis = " + vectorListTuple(orthogonal) +
                     ",   orthonormal basis = " + vectorListTuple(orthonormal));
    for (size_t i = 0; i < orthogonal.size(); ++i) {
        r.functions.push_back("u_" + std::to_string(i + 1) + " = " +
                              vectorText(orthogonal[i]));
        r.functions.push_back("q_" + std::to_string(i + 1) + " = " +
                              vectorText(orthonormal[i]));
    }
    return r;
}

// ---------------------------------------------------------------- QR
// Defined with the extension routines below.  The QR certificate evaluates
// these numerically: multiplying two radical matrices symbolically expands
// their common denominators until an int64 overflows.
std::vector<std::vector<double>> multiplyNumeric(
    const std::vector<std::vector<double>>& A,
    const std::vector<std::vector<double>>& B);
std::vector<std::vector<double>> transposeNumeric(
    const std::vector<std::vector<double>>& A);
bool closeMatrix(const std::vector<std::vector<double>>& A,
                 const std::vector<std::vector<double>>& B, double tol);

Result linalgQr(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_qr", "Q");
    SMat A;
    std::string code;
    if (!parseMatrixText(req.text("matrix", "1, 1; 1, 0; 0, 1"), A, code, 4,
                         6)) {
        failCode(r, code);
        return r;
    }
    const int m = (int)A.size();
    const int n = (int)A.front().size();
    if (m < n) {
        failCode(r, "linalg_not_full_column_rank");
        return r;
    }
    addStep(r, "step.title.write_the_matrix", Sym("A = " + matText(A)));

    std::vector<std::vector<Ex>> q;
    std::vector<std::vector<Ex>> orthogonal;
    std::vector<Ex> norms;
    for (int j = 0; j < n; ++j) {
        std::vector<Ex> u = columnVector(A, j);
        // Project on the unnormalised vectors: their coefficients stay
        // rational, so the strong simplifier never has to rationalise a
        // nested radical.  Normalising first overflows int64 on ordinary
        // 4x3 integer matrices.
        for (size_t i = 0; i < orthogonal.size(); ++i) {
            const Ex denom = dotProduct(orthogonal[i], orthogonal[i]);
            if (zeroEx(denom)) continue;
            const Ex coeff = simp(Mul(dotProduct(columnVector(A, j),
                                                 orthogonal[i]),
                                      Pow(denom, Num(-1))));
            u = subtractVectors(u, scaleVector(orthogonal[i], coeff));
        }
        u = tidyVector(u);
        if (zeroVector(u)) {
            failCode(r, "linalg_not_full_column_rank");
            return r;
        }
        const Ex norm = simp(Sqrt(dotProduct(u, u)));
        orthogonal.push_back(u);
        norms.push_back(norm);
        q.push_back(scaleVector(u, Pow(norm, Num(-1))));
    }

    SMat Q(m, std::vector<Ex>(n, Num(0)));
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < m; ++i) Q[i][j] = q[j][i];
    SMat R(n, std::vector<Ex>(n, Num(0)));
    for (int i = 0; i < n; ++i)
        for (int j = i; j < n; ++j)
            R[i][j] = tidy(Mul(dotProduct(orthogonal[i], columnVector(A, j)),
                               Pow(norms[i], Num(-1))));

    addStep(r, "step.title.build_q_and_r",
            Sym("Q = " + matText(Q) + ",   R = " + matText(R)),
            "step.note.q_has_orthonormal_columns_and_r_is_upper_triangular");

    bool numericInput = true;
    for (const auto& row : A)
        for (const auto& entry : row)
            if (hasSymbols(entry)) numericInput = false;

    bool reconstructs = false;
    bool orthonormal = false;
    if (numericInput) {
        std::vector<std::vector<double>> Qn, Rn, An;
        if (numericMatrix(Q, Qn) && numericMatrix(R, Rn) &&
            numericMatrix(A, An)) {
            reconstructs = closeMatrix(multiplyNumeric(Qn, Rn), An, 1e-7);
            std::vector<std::vector<double>> identity(
                n, std::vector<double>(n, 0.0));
            for (int i = 0; i < n; ++i) identity[i][i] = 1.0;
            orthonormal = closeMatrix(
                multiplyNumeric(transposeNumeric(Qn), Qn), identity, 1e-7);
        }
    } else {
        reconstructs = matricesEqual(multiplyMatrices(Q, R), A);
        orthonormal =
            isIdentity(multiplyMatrices(transposeMatrix(Q), Q));
    }
    addCheck(r, "check.qr_reconstructs_the_matrix", reconstructs, "", "",
             "Q R = A");
    addCheck(r, "check.q_has_orthonormal_columns", orthonormal, "", "",
             "Q^T Q = I");

    r.solution = Sym("Q = " + matText(Q) + ",   R = " + matText(R));
    r.functions.push_back("Q = " + matText(Q));
    r.functions.push_back("R = " + matText(R));
    return r;
}

// ---------------------------------------------------------------- least squares
Result linalgLeastSquares(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_least_squares", "x");
    SMat A;
    std::string code;
    if (!parseMatrixText(req.text("matrix", "1, 0; 0, 1; 1, 1"), A, code, 4,
                         6)) {
        failCode(r, code);
        return r;
    }
    std::vector<Ex> b;
    if (!parseVectorText(req.text("rhs", "1; 2; 3"), b, code)) {
        failCode(r, "linalg_bad_rhs");
        return r;
    }
    const int m = (int)A.size();
    const int n = (int)A.front().size();
    if ((int)b.size() != m) {
        failCode(r, "linalg_dimension_mismatch");
        return r;
    }
    if (m < n) {
        failCode(r, "linalg_not_full_column_rank");
        return r;
    }

    addStep(r, "step.title.write_the_matrix",
            Sym("A = " + matText(A) + ",   b = " + tupleText(b)),
            "step.note.the_normal_equations_make_the_residual_orthogonal");

    const SMat G = multiplyMatrices(transposeMatrix(A), A);
    const std::vector<Ex> rhs = matrixTransposeVector(A, b);
    addStep(r, "step.title.build_the_normal_equations",
            Sym("A^T A = " + matText(G) + ",   A^T b = " + tupleText(rhs)),
            "step.note.the_normal_equations_make_the_residual_orthogonal");

    std::vector<Ex> x;
    Rref g;
    if (!solveSquareSystem(G, rhs, x, g)) {
        failCode(r, "linalg_not_full_column_rank");
        return r;
    }
    if (g.symbolic) r.assumptions.push_back("assumption.generic_symbolic_pivots");
    for (const auto& op : g.ops) addStep(r, "step.title.eliminate", Sym(op));
    addStep(r, "step.title.solve_the_normal_equations",
            Sym("x = " + tupleText(x)),
            "step.note.the_normal_equations_make_the_residual_orthogonal");

    const std::vector<Ex> p = tidyVector(multiplyMatrixVector(A, x));
    const std::vector<Ex> residual = tidyVector(subtractVectors(b, p));
    addStep(r, "step.title.compute_the_residual",
            Sym("A x = " + tupleText(p) + ",   b - A x = " +
                tupleText(residual)),
            "step.note.the_residual_is_orthogonal_to_every_basis_vector");

    bool normalOk = true;
    const std::vector<Ex> normalResidual = matrixTransposeVector(A, residual);
    for (const auto& e : normalResidual)
        if (!zeroEx(e)) normalOk = false;
    addCheck(r, "check.normal_equations_hold", normalOk, "", "",
             "A^T (b - A x) = 0");
    addCheck(r, "check.residual_is_orthogonal_to_the_columns", normalOk, "", "",
             "A^T r = 0");

    const Ex norm = simp(Sqrt(dotProduct(residual, residual)));
    r.solution = Sym("x = " + tupleText(x) + ",   residual = " +
                     tupleText(residual) + ",   ||r|| = " + plain(norm));
    addNumericVectorStep(r, x, "x");
    return r;
}

// ---------------------------------------------------------------- diagonalization
struct Diagonalization {
    bool ok = false;
    std::string code;
    SMat P;
    SMat D;
    SMat Pinv;
    std::vector<Ex> eigenvalues;
};

Diagonalization diagonalizeMatrix(const SMat& A) {
    Diagonalization out;
    const int n = (int)A.size();
    for (const auto& row : A)
        for (const auto& e : row)
            if (hasSymbols(e)) {
                out.code = "linalg_numeric_required";
                return out;
            }

    const Ex lambda = Sym("lambda");
    SMat shifted(n, std::vector<Ex>(n, Num(0)));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            shifted[i][j] = (i == j)
                                ? simp(Add(lambda, Mul(Num(-1), A[i][j])))
                                : simp(Mul(Num(-1), A[i][j]));
    const Ex chEx = simp(determinantByCofactors(shifted));
    Poly ch;
    if (!toPoly(chEx, "lambda", ch) || ch.deg() < 1) {
        out.code = "linalg_numeric_required";
        return out;
    }
    std::vector<Factor> factors;
    if (!factorPolynomial(ch, factors) || factors.empty()) {
        out.code = "linalg_eigenvalues_not_rational";
        return out;
    }

    std::vector<std::vector<Ex>> columns;
    std::vector<Ex> values;
    for (const Factor& f : factors) {
        if (f.p.deg() != 1) {
            out.code = "linalg_eigenvalues_not_rational";
            return out;
        }
        const Rational root =
            Rational(-f.p.c[0].n, f.p.c[0].d) / f.p.c[1];
        SMat B(n, std::vector<Ex>(n, Num(0)));
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                B[i][j] = (i == j)
                              ? simp(Add(A[i][j], Mul(Num(-1), Num(root))))
                              : A[i][j];
        const Rref g = rowReduce(B, false);
        const int rank = (int)g.pivots.size();
        const int nullity = n - rank;
        if (nullity < f.mult) {
            out.code = "linalg_not_diagonalizable";
            return out;
        }
        int added = 0;
        for (int freeCol = 0; freeCol < n && added < f.mult; ++freeCol) {
            if (std::find(g.pivots.begin(), g.pivots.end(), freeCol) !=
                g.pivots.end())
                continue;
            std::vector<Ex> v(n, Num(0));
            v[freeCol] = Num(1);
            for (int i = 0; i < rank; ++i)
                v[g.pivots[i]] = simp(Mul(Num(-1), g.m[i][freeCol]));
            columns.push_back(v);
            values.push_back(Num(root));
            ++added;
        }
        if (added < f.mult) {
            out.code = "linalg_not_diagonalizable";
            return out;
        }
    }
    if ((int)columns.size() != n) {
        out.code = "linalg_not_diagonalizable";
        return out;
    }

    out.P.assign(n, std::vector<Ex>(n, Num(0)));
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i) out.P[i][j] = columns[j][i];
    out.D.assign(n, std::vector<Ex>(n, Num(0)));
    for (int i = 0; i < n; ++i) out.D[i][i] = values[i];
    out.eigenvalues = values;

    const Inverse inv = inverseByGaussJordan(out.P, false);
    if (!inv.ok) {
        out.code = "linalg_not_diagonalizable";
        return out;
    }
    out.Pinv = inv.value;
    if (!matricesEqual(multiplyMatrices(A, out.P),
                       multiplyMatrices(out.P, out.D))) {
        out.code = "linalg_not_diagonalizable";
        return out;
    }
    out.ok = true;
    return out;
}

std::string eigenvalueListText(const std::vector<Ex>& values) {
    std::string s;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i) s += ",  ";
        s += "lambda_" + std::to_string(i + 1) + " = " + plain(values[i]);
    }
    return s;
}

Result linalgDiagonalize(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_diagonalize", "P");
    SMat A;
    std::string code;
    if (!parseMatrixText(req.text("matrix", "2, 1; 1, 2"), A, code, 4)) {
        failCode(r, code);
        return r;
    }
    if ((int)A.front().size() != (int)A.size()) {
        failCode(r, "linalg_not_square");
        return r;
    }
    addStep(r, "step.title.write_the_matrix", Sym("A = " + matText(A)));

    const Diagonalization d = diagonalizeMatrix(A);
    if (!d.ok) {
        failCode(r, d.code);
        return r;
    }
    const int n = (int)A.size();

    const Ex lambda = Sym("lambda");
    SMat shifted(n, std::vector<Ex>(n, Num(0)));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            shifted[i][j] = (i == j)
                                ? simp(Add(lambda, Mul(Num(-1), A[i][j])))
                                : simp(Mul(Num(-1), A[i][j]));
    addStep(r, "step.title.build_the_characteristic_polynomial",
            Sym("det(lambda I - A) = " +
                plain(simp(determinantByCofactors(shifted)))),
            "step.note.the_characteristic_polynomial_is_det_lambda_i_minus_a");
    addStep(r, "step.title.find_the_eigenvalues",
            Sym(eigenvalueListText(d.eigenvalues)),
            "step.note.eigenvalues_are_the_roots_of_the_characteristic_polynomial");
    addStep(r, "step.title.find_the_eigenvectors",
            Sym("P = " + matText(d.P)),
            "step.note.eigenvectors_solve_a_minus_lambda_i_v_0");
    addStep(r, "step.title.build_the_diagonal_factorization",
            Sym("A = P D P^{-1},   D = " + matText(d.D)),
            "step.note.d_is_diagonal_with_the_eigenvalues_on_its_diagonal");

    addCheck(r, "check.matrix_equals_p_d_p_inverse",
             matricesEqual(multiplyMatrices(A, d.P),
                           multiplyMatrices(d.P, d.D)),
             "", "", "A P = P D");
    addCheck(r, "check.eigenvectors_are_independent",
             !d.Pinv.empty(), "", "", "det(P) != 0");

    r.solution = Sym("A = P D P^{-1},   P = " + matText(d.P) +
                     ",   D = " + matText(d.D));
    r.functions.push_back("P = " + matText(d.P));
    r.functions.push_back("D = " + matText(d.D));
    return r;
}

// ---------------------------------------------------------------- matrix power
Result linalgPower(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_power", "A");
    SMat A;
    std::string code;
    if (!parseMatrixText(req.text("matrix", "2, 1; 1, 2"), A, code, 4)) {
        failCode(r, code);
        return r;
    }
    if ((int)A.front().size() != (int)A.size()) {
        failCode(r, "linalg_not_square");
        return r;
    }
    int k = 0;
    try {
        k = std::stoi(req.text("power", "3"));
    } catch (...) {
        failCode(r, "linalg_bad_power");
        return r;
    }
    if (k < -12 || k > 12) {
        failCode(r, "linalg_bad_power");
        return r;
    }
    const int n = (int)A.size();
    addStep(r, "step.title.write_the_matrix",
            Sym("A = " + matText(A) + ",   k = " + std::to_string(k)));

    SMat base = A;
    int exponent = k;
    if (k < 0) {
        const Inverse inv = inverseByGaussJordan(A, false);
        if (!inv.ok) {
            failCode(r, "linalg_singular_matrix");
            return r;
        }
        base = inv.value;
        exponent = -k;
        if (inv.symbolic)
            r.assumptions.push_back("assumption.generic_symbolic_pivots");
        addStep(r, "step.title.invert_the_matrix",
                Sym("A^{-1} = " + matText(base)),
                "step.note.a_negative_power_uses_the_inverse");
    }

    SMat direct = identityMatrix(n);
    for (int step = 0; step < exponent; ++step) {
        direct = multiplyMatrices(direct, base);
        addStep(r, "step.title.multiply_the_matrix",
                Sym("A^" + std::to_string(step + 1) + " = " + matText(direct)),
                "step.note.repeated_multiplication_builds_the_power");
    }
    if (k < 0)
        addStep(r, "step.title.multiply_the_matrix",
                Sym("A^{-" + std::to_string(-k) + "} = " + matText(direct)),
                "step.note.a_negative_power_uses_the_inverse");

    const Diagonalization d = diagonalizeMatrix(A);
    if (d.ok) {
        std::vector<Ex> powered;
        for (const auto& value : d.eigenvalues) {
            Ex p = Num(1);
            for (int i = 0; i < (k < 0 ? -k : k); ++i)
                p = simp(Mul(p, value));
            if (k < 0) p = simp(Pow(p, Num(-1)));
            powered.push_back(p);
        }
        SMat Dk(n, std::vector<Ex>(n, Num(0)));
        for (int i = 0; i < n; ++i) Dk[i][i] = powered[i];
        const SMat viaDiagonal = multiplyMatrices(
            multiplyMatrices(d.P, Dk), d.Pinv);
        addStep(r, "step.title.use_the_diagonalization",
                Sym("A^k = P D^k P^{-1},   D^k = " + matText(Dk)),
                "step.note.diagonalization_turns_the_power_into_a_diagonal_power");
        addCheck(r, "check.power_matches_direct_multiplication",
                 matricesEqual(viaDiagonal, direct), "", "",
                 "P D^k P^{-1} = A^k");
    } else {
        addStep(r, "step.title.multiply_the_matrix",
                Sym("direct multiplication"),
                "step.note.direct_multiplication_is_used_when_the_matrix_is_not_diagonalizable");
    }

    r.solution = Sym("A^" + std::to_string(k) + " = " + matText(direct));
    r.functions.push_back("A^" + std::to_string(k) + " = " + matText(direct));
    return r;
}

Result linalgSolve(const TopicRequest& req) {
    Result r;
    baseId(r, "linear_solve", "x");
    r.normalized = Sym("A x = b");

    const std::string method = req.text("method", "gauss");
    SMat A;
    std::vector<Ex> b;
    std::string code;
    if (!parseMatrixText(req.text("matrix", "2, 1; 1, -1"), A, code, 4)) {
        failCode(r, "linear_system_bad_matrix");
        return r;
    }
    if (!parseVectorText(req.text("rhs", "5; 1"), b, code)) {
        failCode(r, "linear_system_bad_rhs");
        return r;
    }
    if (b.size() != A.size()) {
        failCode(r, "linear_system_size_mismatch");
        return r;
    }
    const int m = (int)A.size();
    const int n = (int)A.front().size();

    addStep(r, "step.title.write_the_augmented_matrix",
            Sym("A = " + matText(A) + ",  b = " + vectorText(b)),
            "step.note.gaussian_elimination_keeps_the_solution_set");

    std::vector<Ex> x(n, Num(0));
    bool needGauss = true;
    std::vector<int> freeCols;

    if (method == "cramer" && m == n) {
        const Determinant d = determinantByElimination(A);
        addStep(r, "step.title.compute_the_determinant",
                Sym("det(A) = " + plain(d.value)));
        if (!d.value->isZero()) {
            needGauss = false;
            if (hasSymbols(d.value))
                r.assumptions.push_back("assumption.generic_symbolic_pivots");
            std::string rule;
            for (int i = 0; i < n; ++i) {
                SMat Ai = A;
                for (int row = 0; row < n; ++row) Ai[row][i] = b[row];
                const Ex di = determinantByElimination(Ai).value;
                x[i] = tidy(Mul(di, Pow(d.value, Num(-1))));
                addStep(r, "step.title.replace_column",
                        Sym("det(A_" + std::to_string(i + 1) + ") = " +
                            plain(di)));
                if (!rule.empty()) rule += ",   ";
                const std::string den =
                    hasSymbols(d.value) ? "(" + plain(d.value) + ")"
                                        : plain(d.value);
                rule += "x_" + std::to_string(i + 1) + " = " + plain(di) +
                        "/" + den + " = " + plain(x[i]);
            }
            addStep(r, "step.title.apply_cramers_rule", Sym(rule),
                    "step.note.cramers_rule_replaces_one_column_at_a_time");
        } else {
            // A zero determinant is not yet an answer: the system may be
            // inconsistent or underdetermined.  Row reduction decides.
            r.warnings.push_back("warning.singular_matrix");
        }
    }

    if (needGauss) {
        SMat aug(m, std::vector<Ex>(n + 1, Num(0)));
        for (int i = 0; i < m; ++i) {
            for (int j = 0; j < n; ++j) aug[i][j] = A[i][j];
            aug[i][n] = b[i];
        }
        const Rref g = rowReduce(aug, true);
        for (const auto& op : g.ops) addStep(r, "step.title.eliminate", Sym(op));
        if (g.symbolic &&
            std::find(r.assumptions.begin(), r.assumptions.end(),
                      "assumption.generic_symbolic_pivots") ==
                r.assumptions.end())
            r.assumptions.push_back("assumption.generic_symbolic_pivots");

        int badRow = -1;
        for (int i = 0; i < m; ++i) {
            bool zeroCoeffs = true;
            for (int j = 0; j < n; ++j)
                if (!zeroFast(g.m[i][j])) zeroCoeffs = false;
            if (zeroCoeffs && !zeroFast(g.m[i][n])) badRow = i;
        }
        if (badRow >= 0) {
            addStep(r, "step.title.back_substitution",
                    Sym("0 = " + toPlain(simp(g.m[badRow][n])) +
                        "  (contradiction)"));
            failCode(r, "linear_system_inconsistent");
            return r;
        }

        const int rank = (int)g.pivots.size();
        freeCols.clear();
        for (int c = 0; c < n; ++c)
            if (std::find(g.pivots.begin(), g.pivots.end(), c) == g.pivots.end())
                freeCols.push_back(c);

        x.assign(n, Num(0));
        for (int i = 0; i < rank; ++i) x[g.pivots[i]] = g.m[i][n];

        std::string back;
        for (int i = 0; i < rank; ++i) {
            if (!back.empty()) back += ",   ";
            back += "x_" + std::to_string(g.pivots[i] + 1) + " = " +
                    toPlain(tidy(x[g.pivots[i]]));
        }
        if (back.empty()) back = "0 = 0  (every variable is free)";
        addStep(r, "step.title.back_substitution", Sym(back));

        if (!freeCols.empty()) {
            r.warnings.push_back("warning.linear_system_infinite_solutions");
            std::string freeText;
            for (size_t j = 0; j < freeCols.size(); ++j) {
                if (!freeText.empty()) freeText += ",   ";
                freeText += "x_" + std::to_string(freeCols[j] + 1) + " = t_" +
                            std::to_string(j + 1);
            }
            addStep(r, "step.title.free_variables", Sym(freeText),
                    "step.note.free_variables_are_set_to_zero_for_the_particular_solution");
        }

        // General solution: x_p + t_1 v_1 + ...
        std::string general = "x = " + tupleText(x);
        for (size_t j = 0; j < freeCols.size(); ++j) {
            std::vector<Ex> v(n, Num(0));
            v[freeCols[j]] = Num(1);
            for (int i = 0; i < rank; ++i)
                v[g.pivots[i]] = simp(Mul(Num(-1), g.m[i][freeCols[j]]));
            general += " + t_" + std::to_string(j + 1) + " " + vectorText(v);
        }
        r.solution = Sym(general);
    } else {
        r.solution = Sym("x = " + tupleText(x));
    }

    for (int i = 0; i < n; ++i)
        r.functions.push_back("x_" + std::to_string(i + 1) + " = " +
                              toPlain(tidy(x[i])));

    const std::vector<Ex> residual = multiplyMatrixVector(A, x);
    bool satisfies = true;
    double worst = 0.0;
    for (int i = 0; i < m; ++i) {
        const Ex diff = Add(residual[i], Mul(Num(-1), b[i]));
        if (!zeroEx(diff)) satisfies = false;
        bool ok = false;
        const double v = evalReal(diff, {}, &ok);
        if (ok && std::isfinite(v)) worst = std::max(worst, std::fabs(v));
    }
    addCheck(r, "check.linear_system_solution_satisfies_every_equation",
             satisfies, "", "", "max residual = " + numStr(worst, 3));

    addStep(r, "step.title.write_the_solution", r.solution);
    return r;
}

// ---------------------------------------------------------------- extensions
// The routines below continue the workbench through the rest of Strang's
// syllabus: orthogonal complements, graphs and incidence matrices, the DFT,
// similarity, matrix exponentials, positive definite matrices, the SVD, norms
// and conditioning, iterative eigenvalues and solvers, linear programming and
// matrix games.  Symbolic inputs stay exact where the algorithm allows it;
// every numerical routine reports its approximation and carries a residual
// certificate.

Ex approxEx(double v) {
    if (std::isnan(v)) return Sym("nan");
    if (!std::isfinite(v)) return Sym(v < 0 ? "-inf" : "inf");
    if (std::fabs(v) < 1e-12) return Num(0);
    const Rational q = Rational::fromDouble(v, 10000);
    if (q.d != 0 &&
        std::fabs(q.toDouble() - v) <= 1e-9 * std::max(1.0, std::fabs(v))) {
        return Num(q);
    }
    return Sym(numStr(v, 8));
}

SMat approxMatrix(const std::vector<std::vector<double>>& A) {
    const int cols = A.empty() ? 0 : (int)A.front().size();
    SMat out(A.size(), std::vector<Ex>(cols, Num(0)));
    for (size_t i = 0; i < A.size(); ++i)
        for (int j = 0; j < cols; ++j) out[i][j] = approxEx(A[i][j]);
    return out;
}

std::string complexText(double re, double im, int digits = 6) {
    if (std::fabs(im) < 1e-10) return numStr(re, digits);
    if (std::fabs(re) < 1e-10) return numStr(im, digits) + "i";
    return numStr(re, digits) + (im < 0 ? " - " : " + ") +
           numStr(std::fabs(im), digits) + "i";
}

bool symmetricMatrix(const SMat& A) {
    if (A.empty() || A.size() != A.front().size()) return false;
    for (size_t i = 0; i < A.size(); ++i)
        for (size_t j = i + 1; j < A.size(); ++j)
            if (!zeroEx(Add(A[i][j], Mul(Num(-1), A[j][i])))) return false;
    return true;
}

std::string trimText(const std::string& raw) {
    size_t a = 0, b = raw.size();
    while (a < b && std::isspace((unsigned char)raw[a])) ++a;
    while (b > a && std::isspace((unsigned char)raw[b - 1])) --b;
    return raw.substr(a, b - a);
}

std::vector<std::vector<Ex>> nullBasisFrom(const Rref& g, int cols) {
    const int rank = (int)g.pivots.size();
    std::vector<std::vector<Ex>> basis;
    for (int f = 0; f < cols; ++f) {
        if (std::find(g.pivots.begin(), g.pivots.end(), f) != g.pivots.end())
            continue;
        std::vector<Ex> v(cols, Num(0));
        v[f] = Num(1);
        for (int i = 0; i < rank; ++i)
            v[g.pivots[i]] = simp(Mul(Num(-1), g.m[i][f]));
        basis.push_back(std::move(v));
    }
    return basis;
}

std::string basisTupleText(const std::vector<std::vector<Ex>>& basis) {
    if (basis.empty()) return "{0}";
    std::string s = "{";
    for (size_t i = 0; i < basis.size(); ++i) {
        if (i) s += ",  ";
        s += "v_" + std::to_string(i + 1) + " = " + tupleText(basis[i]);
    }
    return s + "}";
}

bool closeMatrix(const std::vector<std::vector<double>>& A,
                 const std::vector<std::vector<double>>& B,
                 double tol = 1e-7) {
    if (A.size() != B.size()) return false;
    for (size_t i = 0; i < A.size(); ++i) {
        if (A[i].size() != B[i].size()) return false;
        for (size_t j = 0; j < A[i].size(); ++j)
            if (std::fabs(A[i][j] - B[i][j]) > tol) return false;
    }
    return true;
}

// Symmetric Jacobi eigenvalue iteration.  It is short, backward stable and
// needs no external library, which matters for the Android build.
bool jacobiEigen(std::vector<std::vector<double>> A,
                 std::vector<double>& values,
                 std::vector<std::vector<double>>& vectors,
                 int maxSweeps = 80) {
    const int n = (int)A.size();
    if (n == 0 || (int)A.front().size() != n) return false;
    vectors.assign(n, std::vector<double>(n, 0.0));
    for (int i = 0; i < n; ++i) vectors[i][i] = 1.0;
    for (int sweep = 0; sweep < maxSweeps; ++sweep) {
        double off = 0.0;
        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j) off += A[i][j] * A[i][j];
        if (off < 1e-26) break;
        for (int p = 0; p < n; ++p) {
            for (int q = p + 1; q < n; ++q) {
                if (std::fabs(A[p][q]) < 1e-15) continue;
                const double theta =
                    (A[q][q] - A[p][p]) / (2.0 * A[p][q]);
                const double sign = theta >= 0.0 ? 1.0 : -1.0;
                const double t = sign /
                                 (std::fabs(theta) +
                                  std::sqrt(theta * theta + 1.0));
                const double c = 1.0 / std::sqrt(t * t + 1.0);
                const double s = t * c;
                for (int k = 0; k < n; ++k) {
                    const double akp = A[k][p], akq = A[k][q];
                    A[k][p] = c * akp - s * akq;
                    A[k][q] = s * akp + c * akq;
                }
                for (int k = 0; k < n; ++k) {
                    const double apk = A[p][k], aqk = A[q][k];
                    A[p][k] = c * apk - s * aqk;
                    A[q][k] = s * apk + c * aqk;
                }
                for (int k = 0; k < n; ++k) {
                    const double vkp = vectors[k][p], vkq = vectors[k][q];
                    vectors[k][p] = c * vkp - s * vkq;
                    vectors[k][q] = s * vkp + c * vkq;
                }
            }
        }
    }
    values.resize(n);
    for (int i = 0; i < n; ++i) values[i] = A[i][i];
    // Sort the pairs by descending eigenvalue so the output is deterministic.
    for (int i = 0; i < n; ++i) {
        int best = i;
        for (int j = i + 1; j < n; ++j)
            if (values[j] > values[best]) best = j;
        if (best != i) {
            std::swap(values[i], values[best]);
            for (int k = 0; k < n; ++k)
                std::swap(vectors[k][i], vectors[k][best]);
        }
    }
    return true;
}

std::vector<std::vector<double>> multiplyNumeric(
    const std::vector<std::vector<double>>& A,
    const std::vector<std::vector<double>>& B) {
    if (A.empty() || B.empty()) return {};
    const int m = (int)A.size(), n = (int)B.front().size(), k = (int)B.size();
    std::vector<std::vector<double>> C(m, std::vector<double>(n, 0.0));
    for (int i = 0; i < m; ++i)
        for (int j = 0; j < n; ++j) {
            double s = 0.0;
            for (int p = 0; p < k; ++p) s += A[i][p] * B[p][j];
            C[i][j] = s;
        }
    return C;
}

std::vector<std::vector<double>> transposeNumeric(
    const std::vector<std::vector<double>>& A) {
    if (A.empty()) return {};
    const int m = (int)A.size(), n = (int)A.front().size();
    std::vector<std::vector<double>> T(n, std::vector<double>(m, 0.0));
    for (int i = 0; i < m; ++i)
        for (int j = 0; j < n; ++j) T[j][i] = A[i][j];
    return T;
}

// ---------------------------------------------------------------- A^perp
Result linalgOrthogonalComplement(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_orthogonal_complement", "y");
    SMat A;
    std::string code;
    if (!parseMatrixText(req.text("matrix", "1, 2; 2, 4; 3, 6"), A, code, 6)) {
        failCode(r, code);
        return r;
    }
    const int m = (int)A.size();
    addStep(r, "step.title.write_the_matrix", Sym("A = " + matText(A)));

    const SMat At = transposeMatrix(A);
    addStep(r, "step.title.write_the_matrix", Sym("A^T = " + matText(At)),
            "step.note.the_orthogonal_complement_is_the_null_space_of_the_transpose");
    const Rref g = rowReduce(At, true);
    for (const auto& op : g.ops) addStep(r, "step.title.eliminate", Sym(op));
    addStep(r, "step.title.row_reduce", Sym(matText(g.m)),
            "step.note.row_operations_do_not_change_the_row_space");

    const int rank = (int)g.pivots.size();
    const std::vector<std::vector<Ex>> basis = nullBasisFrom(g, m);
    addStep(r, "step.title.read_the_rank",
            Sym("rank(A) = " + std::to_string(rank) +
                ",   dim(N(A^T)) = " + std::to_string(m - rank)));
    addStep(r, "step.title.find_the_null_space", Sym(basisTupleText(basis)),
            "step.note.the_null_space_vectors_solve_a_v_0");

    bool orthogonal = true;
    for (const auto& y : basis) {
        for (int j = 0; j < (int)A.front().size(); ++j) {
            const Ex d = dotProduct(y, columnVector(A, j));
            if (!zeroEx(d)) orthogonal = false;
        }
    }
    addCheck(r, "check.residual_is_orthogonal_to_the_columns", orthogonal, "",
             "", "A^T y = 0");
    addCheck(r, "check.basis_vectors_are_independent",
             (int)basis.size() == m - rank, "", "",
             "dim = " + std::to_string(m - rank));

    r.solution = Sym("N(A^T) = " + basisTupleText(basis) +
                     ",   dim = " + std::to_string(m - rank));
    for (size_t i = 0; i < basis.size(); ++i)
        r.functions.push_back("y_" + std::to_string(i + 1) + " = " +
                              vectorText(basis[i]));
    if (g.symbolic) r.assumptions.push_back("assumption.generic_symbolic_pivots");
    return r;
}

// ---------------------------------------------------------------- graphs
Result linalgGraph(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_graph", "x");
    int n = 0;
    try {
        n = std::stoi(req.text("nodes", "3"));
    } catch (...) {
        failCode(r, "linalg_bad_matrix");
        return r;
    }
    if (n < 2 || n > 6) {
        failCode(r, "linalg_too_large");
        return r;
    }
    std::string raw = req.text("edges", "1-2, 2-3, 3-1");
    replaceAll(raw, "\xE2\x88\x92", "-");
    std::vector<std::pair<int, int>> edges;
    std::string cur;
    for (size_t i = 0; i <= raw.size(); ++i) {
        if (i == raw.size() || raw[i] == ',' || raw[i] == ';' ||
            raw[i] == '\n') {
            const std::string token = trimText(cur);
            cur.clear();
            if (token.empty()) continue;
            const size_t dash = token.find('-');
            if (dash == std::string::npos) {
                failCode(r, "linalg_bad_matrix");
                return r;
            }
            try {
                const int a = std::stoi(trimText(token.substr(0, dash)));
                const int b = std::stoi(trimText(token.substr(dash + 1)));
                if (a < 1 || b < 1 || a > n || b > n || a == b) {
                    failCode(r, "linalg_bad_matrix");
                    return r;
                }
                edges.emplace_back(a - 1, b - 1);
            } catch (...) {
                failCode(r, "linalg_bad_matrix");
                return r;
            }
        } else {
            cur.push_back(raw[i]);
        }
    }
    if (edges.empty()) {
        failCode(r, "linalg_bad_matrix");
        return r;
    }

    SMat B(edges.size(), std::vector<Ex>(n, Num(0)));
    for (size_t e = 0; e < edges.size(); ++e) {
        B[e][edges[e].first] = Num(1);
        B[e][edges[e].second] = Num(-1);
    }
    addStep(r, "step.title.write_the_matrix", Sym("B = " + matText(B)),
            "step.note.the_incidence_matrix_records_the_edges");

    const Rref g = rowReduce(B, true);
    for (const auto& op : g.ops) addStep(r, "step.title.eliminate", Sym(op));
    addStep(r, "step.title.row_reduce", Sym(matText(g.m)),
            "step.note.row_operations_do_not_change_the_row_space");
    const int rank = (int)g.pivots.size();
    const int components = n - rank;
    addStep(r, "step.title.read_the_rank",
            Sym("rank(B) = " + std::to_string(rank) +
                ",   components = " + std::to_string(components)));

    const std::vector<std::vector<Ex>> potentials = nullBasisFrom(g, n);
    const Rref left = rowReduce(transposeMatrix(B), false);
    const std::vector<std::vector<Ex>> cycles =
        nullBasisFrom(left, (int)edges.size());
    addStep(r, "step.title.find_the_null_space",
            Sym("potentials: " + basisTupleText(potentials)),
            "step.note.node_potentials_are_constant_on_each_component");
    addStep(r, "step.title.find_the_left_null_space",
            Sym("cycles: " + basisTupleText(cycles)),
            "step.note.the_left_null_space_is_the_cycle_space");

    bool potentialsOk = true;
    for (const auto& p : potentials) {
        const std::vector<Ex> prod = multiplyMatrixVector(B, p);
        for (const auto& e : prod)
            if (!zeroEx(e)) potentialsOk = false;
    }
    addCheck(r, "check.null_space_vector_satisfies_a_v_0", potentialsOk, "",
             "", "B x = 0");
    bool cyclesOk = true;
    for (const auto& c : cycles) {
        const std::vector<Ex> prod = multiplyMatrixVector(
            transposeMatrix(B), c);
        for (const auto& e : prod)
            if (!zeroEx(e)) cyclesOk = false;
    }
    addCheck(r, "check.null_space_vector_satisfies_a_v_0", cyclesOk, "",
             "", "B^T y = 0");

    r.solution = Sym("components = " + std::to_string(components) +
                     ",   rank(B) = " + std::to_string(rank) +
                     ",   cycle dimension = " +
                     std::to_string((int)edges.size() - rank));
    for (size_t i = 0; i < potentials.size(); ++i)
        r.functions.push_back("potential_" + std::to_string(i + 1) + " = " +
                              vectorText(potentials[i]));
    for (size_t i = 0; i < cycles.size(); ++i)
        r.functions.push_back("cycle_" + std::to_string(i + 1) + " = " +
                              vectorText(cycles[i]));
    return r;
}

// ---------------------------------------------------------------- DFT
void fftRec(std::vector<std::complex<double>>& a, bool invert) {
    const int n = (int)a.size();
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    const double pi = std::acos(-1.0);
    for (int len = 2; len <= n; len <<= 1) {
        // Forward DFT uses the negative exponent, like the DFT matrix F
        // built below; the inverse uses the positive one.
        const double ang = 2.0 * pi / len * (invert ? 1.0 : -1.0);
        const std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (int j = 0; j < len / 2; ++j) {
                const std::complex<double> u = a[i + j];
                const std::complex<double> v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
    if (invert) {
        for (auto& x : a) x /= (double)n;
    }
}

Result linalgFft(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_fft", "X");
    std::vector<Ex> signal;
    std::string code;
    if (!parseVectorText(req.text("signal", "1, 2, 3, 4"), signal, code)) {
        failCode(r, code);
        return r;
    }
    const int n = (int)signal.size();
    if (n != 4 && n != 8) {
        failCode(r, "linalg_fft_length");
        return r;
    }
    std::vector<double> x(n, 0.0);
    for (int i = 0; i < n; ++i) {
        if (hasSymbols(signal[i])) {
            failCode(r, "linalg_numeric_required");
            return r;
        }
        bool ok = false;
        x[i] = evalReal(signal[i], {}, &ok);
        if (!ok || !std::isfinite(x[i])) {
            failCode(r, "linalg_numeric_required");
            return r;
        }
    }
    addStep(r, "step.title.write_the_vectors", Sym("x = " + tupleText(signal)));

    if (n == 4) {
        SMat F(4, std::vector<Ex>(4, Num(1)));
        const char* entries[4][4] = {
            {"1", "1", "1", "1"},
            {"1", "-i", "-1", "i"},
            {"1", "-1", "1", "-1"},
            {"1", "i", "-1", "-i"},
        };
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) F[i][j] = Sym(entries[i][j]);
        addStep(r, "step.title.build_the_dft_matrix", Sym("F_4 = " + matText(F)),
                "step.note.the_dft_matrix_has_roots_of_unity_as_entries");
    }

    std::vector<std::complex<double>> a(n);
    for (int i = 0; i < n; ++i) a[i] = std::complex<double>(x[i], 0.0);
    fftRec(a, false);
    std::string spectrum;
    for (int k = 0; k < n; ++k) {
        if (k) spectrum += ",  ";
        spectrum += "X_" + std::to_string(k) + " = " +
                    complexText(a[k].real(), a[k].imag());
    }
    addStep(r, "step.title.evaluate", Sym(spectrum),
            "step.note.the_fft_evaluates_the_same_sum_as_the_dft");

    std::vector<std::complex<double>> back = a;
    fftRec(back, true);
    bool inverseOk = true;
    for (int i = 0; i < n; ++i)
        if (std::fabs(back[i].real() - x[i]) > 1e-8 ||
            std::fabs(back[i].imag()) > 1e-8)
            inverseOk = false;
    addCheck(r, "check.inverse_dft_reconstructs_the_signal", inverseOk, "", "",
             "F^{-1} F x = x");
    addStep(r, "step.title.numeric_value", Sym("x = " + tupleText(signal)),
            "step.note.the_numeric_value_is_an_approximation");

    r.solution = Sym(spectrum);
    for (int k = 0; k < n; ++k)
        r.functions.push_back("X_" + std::to_string(k) + " = " +
                              complexText(a[k].real(), a[k].imag()));
    return r;
}

// ---------------------------------------------------------------- similarity
Result linalgSimilarity(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_similarity", "B");
    SMat A, P;
    std::string code;
    if (!parseMatrixText(req.text("matrix", "1, 2; 3, 4"), A, code, 4)) {
        failCode(r, code);
        return r;
    }
    if (!parseMatrixText(req.text("change", "1, 1; 0, 1"), P, code, 4)) {
        failCode(r, code);
        return r;
    }
    if (A.size() != A.front().size() || P.size() != P.front().size() ||
        A.size() != P.size()) {
        failCode(r, "linalg_not_square");
        return r;
    }
    const int n = (int)A.size();
    addStep(r, "step.title.write_the_matrix",
            Sym("A = " + matText(A) + ",   P = " + matText(P)));

    const Inverse inv = inverseByGaussJordan(P, true);
    if (!inv.ok) {
        failCode(r, "linalg_singular_matrix");
        return r;
    }
    addStep(r, "step.title.invert_the_matrix",
            Sym("P^{-1} = " + matText(inv.value)),
            "step.note.the_inverse_is_read_off_the_right_half");
    const SMat B = multiplyMatrices(multiplyMatrices(inv.value, A), P);
    addStep(r, "step.title.multiply_the_matrix",
            Sym("B = P^{-1} A P = " + matText(B)),
            "step.note.similar_matrices_share_trace_determinant_and_eigenvalues");

    Ex traceA = Num(0), traceB = Num(0);
    for (int i = 0; i < n; ++i) {
        traceA = Add(traceA, A[i][i]);
        traceB = Add(traceB, B[i][i]);
    }
    addCheck(r, "check.similar_matrices_share_the_trace",
             zeroEx(Add(traceA, Mul(Num(-1), traceB))), "", "",
             "tr(A) = tr(B)");
    addCheck(r, "check.similar_matrices_share_the_determinant",
             zeroEx(Add(determinantByCofactors(A),
                        Mul(Num(-1), determinantByCofactors(B)))),
             "", "", "det(A) = det(B)");

    r.solution = Sym("B = P^{-1} A P = " + matText(B));
    r.functions.push_back("B = " + matText(B));
    r.functions.push_back("P^{-1} = " + matText(inv.value));
    return r;
}

// ---------------------------------------------------------------- exp(At)
Result linalgMatrixExp(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_matrix_exp", "t");
    SMat A;
    std::string code;
    if (!parseMatrixText(req.text("matrix", "0, 1; -1, 0"), A, code, 4)) {
        failCode(r, code);
        return r;
    }
    if (A.size() != A.front().size()) {
        failCode(r, "linalg_not_square");
        return r;
    }
    const int n = (int)A.size();
    addStep(r, "step.title.write_the_matrix", Sym("A = " + matText(A)));

    if (n == 1) {
        const Ex e = Fn("exp", {Mul(A[0][0], Sym("t"))});
        r.solution = Sym("e^{At} = [" + plain(e) + "]");
        r.functions.push_back("e^{At} = [" + plain(e) + "]");
        addStep(r, "step.title.exponentiate_the_diagonal",
                Sym("e^{At} = [" + plain(e) + "]"),
                "step.note.the_exponential_of_a_diagonal_entry_is_a_scalar_exponential");
        return r;
    }

    if (n == 2) {
        const Ex trace = Add(A[0][0], A[1][1]);
        const Ex det = Add(Mul(A[0][0], A[1][1]),
                           Mul(Num(-1), Mul(A[0][1], A[1][0])));
        const Ex disc = Add(Mul(trace, trace), Mul(Num(-4), det));
        if (hasSymbols(disc)) {
            // The sign of the discriminant is unknown, so the closed 2x2
            // formula is not available; fall through to diagonalization.
        } else {
        bool ok = false;
        const double d = evalReal(disc, {}, &ok);
        if (!ok || !std::isfinite(d)) {
            failCode(r, "linalg_numeric_required");
            return r;
        }
        addStep(r, "step.title.compute_trace_and_determinant",
                Sym("tr(A) = " + plain(trace) + ",   det(A) = " + plain(det) +
                    ",   discriminant = " + plain(disc)),
                "step.note.the_trace_and_determinant_give_the_eigenvalues");

        const Ex half = Mul(trace, Num(Rational(1, 2)));
        SMat M(n, std::vector<Ex>(n, Num(0)));
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                M[i][j] = (i == j) ? simp(Add(A[i][j], Mul(Num(-1), half)))
                                   : A[i][j];
        const Ex alpha = Fn("exp", {Mul(half, Sym("t"))});
        SMat E(n, std::vector<Ex>(n, Num(0)));
        if (std::fabs(d) < 1e-12) {
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j) {
                    const Ex id = (i == j) ? Num(1) : Num(0);
                    E[i][j] = simp(Mul(alpha, Add(id, Mul(Sym("t"), M[i][j]))));
                }
            addStep(r, "step.title.exponentiate_the_diagonal",
                    Sym("e^{At} = e^{tr(A)t/2} (I + t (A - tr(A)/2 I))"),
                    "step.note.a_repeated_eigenvalue_adds_a_linear_term");
        } else if (d > 0) {
            const Ex root = simp(Sqrt(disc));
            const Ex halfRoot = Mul(root, Num(Rational(1, 2)));
            const Ex ch = Fn("cosh", {Mul(halfRoot, Sym("t"))});
            const Ex sh = Fn("sinh", {Mul(halfRoot, Sym("t"))});
            const Ex coef = Mul(Num(2), sh, Pow(root, Num(-1)));
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j) {
                    const Ex id = (i == j) ? Num(1) : Num(0);
                    E[i][j] = simp(Mul(alpha, Add(Mul(ch, id),
                                                  Mul(coef, M[i][j]))));
                }
            addStep(r, "step.title.exponentiate_the_diagonal",
                    Sym("e^{At} = e^{tr(A)t/2} (cosh(dt/2) I + "
                        "(2/d) sinh(dt/2) (A - tr(A)/2 I)),   d = sqrt(disc)"),
                    "step.note.real_eigenvalues_give_hyperbolic_functions");
        } else {
            const Ex omega = simp(Sqrt(Mul(Num(-1), disc)));
            const Ex halfOmega = Mul(omega, Num(Rational(1, 2)));
            const Ex co = Fn("cos", {Mul(halfOmega, Sym("t"))});
            const Ex si = Fn("sin", {Mul(halfOmega, Sym("t"))});
            const Ex coef = Mul(Num(2), si, Pow(omega, Num(-1)));
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j) {
                    const Ex id = (i == j) ? Num(1) : Num(0);
                    E[i][j] = simp(Mul(alpha, Add(Mul(co, id),
                                                  Mul(coef, M[i][j]))));
                }
            addStep(r, "step.title.exponentiate_the_diagonal",
                    Sym("e^{At} = e^{tr(A)t/2} (cos(wt/2) I + "
                        "(2/w) sin(wt/2) (A - tr(A)/2 I)),   w = sqrt(-disc)"),
                    "step.note.complex_eigenvalues_give_rotation_and_decay");
        }

        SMat atZero(n, std::vector<Ex>(n, Num(0)));
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                atZero[i][j] = tidy(substitute(E[i][j], "t", Num(0)));
        addCheck(r, "check.exponential_at_zero_is_the_identity",
                 isIdentity(atZero), "", "", "e^{0} = I");
        r.solution = Sym("e^{At} = " + matText(E));
        r.functions.push_back("e^{At} = " + matText(E));
        return r;
        }
    }

    const Diagonalization d = diagonalizeMatrix(A);
    if (!d.ok) {
        failCode(r, d.code);
        return r;
    }
    SMat E(n, std::vector<Ex>(n, Num(0)));
    for (int i = 0; i < n; ++i)
        E[i][i] = Fn("exp", {Mul(d.eigenvalues[i], Sym("t"))});
    const SMat expAt = multiplyMatrices(multiplyMatrices(d.P, E), d.Pinv);
    addStep(r, "step.title.build_the_diagonal_factorization",
            Sym("A = P D P^{-1},   D = " + matText(d.D)),
            "step.note.d_is_diagonal_with_the_eigenvalues_on_its_diagonal");
    addStep(r, "step.title.exponentiate_the_diagonal",
            Sym("e^{Dt} = " + matText(E)),
            "step.note.the_exponential_of_a_diagonal_entry_is_a_scalar_exponential");
    addStep(r, "step.title.multiply_the_matrix",
            Sym("e^{At} = P e^{Dt} P^{-1} = " + matText(expAt)),
            "step.note.similarity_moves_the_exponential_back_to_a");

    SMat atZero(n, std::vector<Ex>(n, Num(0)));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            atZero[i][j] = tidy(substitute(expAt[i][j], "t", Num(0)));
    addCheck(r, "check.exponential_at_zero_is_the_identity",
             isIdentity(atZero), "", "", "e^{0} = I");
    r.solution = Sym("e^{At} = " + matText(expAt));
    r.functions.push_back("e^{At} = " + matText(expAt));
    return r;
}

// ---------------------------------------------------------------- positive definite
Result linalgPositiveDefinite(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_positive_definite", "L");
    SMat A;
    std::string code;
    if (!parseMatrixText(req.text("matrix", "4, 2; 2, 3"), A, code, 4)) {
        failCode(r, code);
        return r;
    }
    if (A.size() != A.front().size()) {
        failCode(r, "linalg_not_square");
        return r;
    }
    const int n = (int)A.size();
    addStep(r, "step.title.write_the_matrix", Sym("A = " + matText(A)));
    if (!symmetricMatrix(A)) {
        failCode(r, "linalg_not_symmetric");
        return r;
    }

    std::vector<Ex> minors;
    bool numericOk = true;
    bool positive = true;
    for (int k = 1; k <= n; ++k) {
        SMat sub(k, std::vector<Ex>(k, Num(0)));
        for (int i = 0; i < k; ++i)
            for (int j = 0; j < k; ++j) sub[i][j] = A[i][j];
        const Ex m = determinantByCofactors(sub);
        minors.push_back(m);
        if (hasSymbols(m)) {
            numericOk = false;
            continue;
        }
        bool ok = false;
        const double v = evalReal(m, {}, &ok);
        if (!ok || !std::isfinite(v)) {
            numericOk = false;
        } else if (v <= 1e-10) {
            positive = false;
        }
    }
    std::string minorText;
    for (int k = 0; k < n; ++k) {
        if (k) minorText += ",   ";
        minorText += "D_" + std::to_string(k + 1) + " = " + plain(minors[k]);
    }
    addStep(r, "step.title.compute_the_leading_minors", Sym(minorText),
            "step.note.sylvester_criterion_checks_the_leading_minors");
    if (numericOk && !positive) {
        failCode(r, "linalg_not_positive_definite");
        return r;
    }

    SMat L(n, std::vector<Ex>(n, Num(0)));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j <= i; ++j) {
            Ex sum = A[i][j];
            for (int k = 0; k < j; ++k)
                sum = Add(sum, Mul(Num(-1), Mul(L[i][k], L[j][k])));
            sum = tidy(sum);
            if (i == j) {
                if (!hasSymbols(sum)) {
                    bool ok = false;
                    const double v = evalReal(sum, {}, &ok);
                    if (ok && std::isfinite(v) && v <= 1e-12) {
                        failCode(r, "linalg_not_positive_definite");
                        return r;
                    }
                }
                L[i][j] = simp(Sqrt(sum));
            } else {
                if (zeroEx(L[j][j])) {
                    failCode(r, "linalg_not_positive_definite");
                    return r;
                }
                L[i][j] = tidy(Mul(sum, Pow(L[j][j], Num(-1))));
            }
        }
    }
    addStep(r, "step.title.build_cholesky",
            Sym("A = L L^T,   L = " + matText(L)),
            "step.note.cholesky_is_the_square_root_of_a_positive_definite_matrix");
    addCheck(r, "check.cholesky_reconstructs_the_matrix",
             matricesEqual(multiplyMatrices(L, transposeMatrix(L)), A), "", "",
             "L L^T = A");
    bool lower = true;
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            if (!zeroEx(L[i][j])) lower = false;
    addCheck(r, "check.cholesky_is_lower_triangular", lower, "", "",
             "L is lower triangular");

    r.solution = Sym("A = L L^T,   L = " + matText(L));
    r.functions.push_back("L = " + matText(L));
    r.functions.push_back("L^T = " + matText(transposeMatrix(L)));
    if (!numericOk) r.assumptions.push_back("assumption.generic_symbolic_pivots");
    return r;
}

// ---------------------------------------------------------------- SVD
double dotNumeric(const std::vector<double>& a, const std::vector<double>& b) {
    double s = 0.0;
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) s += a[i] * b[i];
    return s;
}

void fillOrthonormalColumn(std::vector<std::vector<double>>& U, int rows,
                           int col) {
    const int cols = (int)U.front().size();
    for (int e = 0; e < rows; ++e) {
        std::vector<double> v(rows, 0.0);
        v[e] = 1.0;
        for (int j = 0; j < col; ++j) {
            const double d = dotNumeric(v, [&] {
                std::vector<double> c(rows, 0.0);
                for (int i = 0; i < rows; ++i) c[i] = U[i][j];
                return c;
            }());
            for (int i = 0; i < rows; ++i) v[i] -= d * U[i][j];
        }
        double norm = 0.0;
        for (int i = 0; i < rows; ++i) norm += v[i] * v[i];
        norm = std::sqrt(norm);
        if (norm > 1e-8) {
            for (int i = 0; i < rows; ++i) U[i][col] = v[i] / norm;
            (void)cols;
            return;
        }
    }
    for (int i = 0; i < rows; ++i) U[i][col] = 0.0;
    if (rows > 0) U[0][col] = 1.0;
}

struct SvdData {
    std::vector<std::vector<double>> U, S, V;
    std::vector<double> sv;
    int rank = 0;
};

SvdData computeSvd(const std::vector<std::vector<double>>& A) {
    SvdData out;
    const int m = (int)A.size();
    const int n = (int)A.front().size();
    if (m >= n) {
        std::vector<std::vector<double>> B(n, std::vector<double>(n, 0.0));
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) {
                double s = 0.0;
                for (int k = 0; k < m; ++k) s += A[k][i] * A[k][j];
                B[i][j] = s;
            }
        std::vector<double> ev;
        std::vector<std::vector<double>> V;
        jacobiEigen(B, ev, V);
        out.sv.resize(n);
        out.V.assign(n, std::vector<double>(n, 0.0));
        for (int j = 0; j < n; ++j) {
            out.sv[j] = std::sqrt(std::max(ev[j], 0.0));
            for (int i = 0; i < n; ++i) out.V[i][j] = V[i][j];
        }
        // A zero eigenvalue of A^T A comes back as round-off, and its square
        // root is far above the old absolute threshold.  Cut relative to the
        // largest singular value so the rank and the null directions stay
        // clean.
        double smax = 0.0;
        for (double v : out.sv) smax = std::max(smax, v);
        const double cutoff = 1e-7 * smax;
        for (int j = 0; j < n; ++j)
            if (out.sv[j] <= cutoff) out.sv[j] = 0.0;
        out.U.assign(m, std::vector<double>(n, 0.0));
        for (int j = 0; j < n; ++j) {
            if (out.sv[j] > 0.0) {
                for (int i = 0; i < m; ++i) {
                    double s = 0.0;
                    for (int k = 0; k < n; ++k)
                        s += A[i][k] * out.V[k][j];
                    out.U[i][j] = s / out.sv[j];
                }
            } else {
                fillOrthonormalColumn(out.U, m, j);
            }
        }
        out.S.assign(n, std::vector<double>(n, 0.0));
        for (int i = 0; i < n; ++i) out.S[i][i] = out.sv[i];
    } else {
        std::vector<std::vector<double>> B(m, std::vector<double>(m, 0.0));
        for (int i = 0; i < m; ++i)
            for (int j = 0; j < m; ++j) {
                double s = 0.0;
                for (int k = 0; k < n; ++k) s += A[i][k] * A[j][k];
                B[i][j] = s;
            }
        std::vector<double> ev;
        std::vector<std::vector<double>> U;
        jacobiEigen(B, ev, U);
        out.sv.resize(m);
        out.U.assign(m, std::vector<double>(m, 0.0));
        for (int j = 0; j < m; ++j) {
            out.sv[j] = std::sqrt(std::max(ev[j], 0.0));
            for (int i = 0; i < m; ++i) out.U[i][j] = U[i][j];
        }
        double smax = 0.0;
        for (double v : out.sv) smax = std::max(smax, v);
        const double cutoff = 1e-7 * smax;
        for (int j = 0; j < m; ++j)
            if (out.sv[j] <= cutoff) out.sv[j] = 0.0;
        out.V.assign(n, std::vector<double>(m, 0.0));
        for (int j = 0; j < m; ++j) {
            if (out.sv[j] > 0.0) {
                for (int i = 0; i < n; ++i) {
                    double s = 0.0;
                    for (int k = 0; k < m; ++k)
                        s += A[k][i] * out.U[k][j];
                    out.V[i][j] = s / out.sv[j];
                }
            } else {
                fillOrthonormalColumn(out.V, n, j);
            }
        }
        out.S.assign(m, std::vector<double>(m, 0.0));
        for (int i = 0; i < m; ++i) out.S[i][i] = out.sv[i];
    }
    for (double v : out.sv)
        if (v > 0.0) ++out.rank;
    return out;
}

Result linalgSvd(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_svd", "sigma");
    SMat A;
    std::string code;
    if (!parseMatrixText(req.text("matrix", "3, 0; 0, 2"), A, code, 4)) {
        failCode(r, code);
        return r;
    }
    addStep(r, "step.title.write_the_matrix", Sym("A = " + matText(A)));
    std::vector<std::vector<double>> N;
    if (!numericMatrix(A, N)) {
        failCode(r, "linalg_numeric_required");
        return r;
    }
    const SvdData s = computeSvd(N);
    const int k = (int)s.sv.size();
    std::string sigmaText;
    for (int i = 0; i < k; ++i) {
        if (i) sigmaText += ",   ";
        sigmaText += "sigma_" + std::to_string(i + 1) + " = " +
                     numStr(s.sv[i], 8);
    }
    addStep(r, "step.title.compute_the_singular_values", Sym(sigmaText),
            "step.note.the_singular_values_are_the_square_roots_of_the_eigenvalues_of_a_t_a");
    addStep(r, "step.title.build_u_and_v",
            Sym("U = " + matText(approxMatrix(s.U)) + ",   S = " +
                matText(approxMatrix(s.S)) + ",   V = " +
                matText(approxMatrix(s.V))),
            "step.note.svd_diagonalizes_the_map_into_rotation_scale_rotation");

    const std::vector<std::vector<double>> Vt = transposeNumeric(s.V);
    const std::vector<std::vector<double>> rebuilt =
        multiplyNumeric(multiplyNumeric(s.U, s.S), Vt);
    addCheck(r, "check.svd_reconstructs_the_matrix", closeMatrix(rebuilt, N),
             "", "", "U S V^T = A");
    const std::vector<std::vector<double>> UtU =
        multiplyNumeric(transposeNumeric(s.U), s.U);
    bool unit = true;
    for (int i = 0; i < (int)UtU.size(); ++i)
        for (int j = 0; j < (int)UtU[i].size(); ++j) {
            const double want = (i == j) ? 1.0 : 0.0;
            if (std::fabs(UtU[i][j] - want) > 1e-7) unit = false;
        }
    addCheck(r, "check.u_has_orthonormal_columns", unit, "", "",
             "U^T U = I");

    // Moore-Penrose pseudoinverse from the same factorization.
    const int rows = (int)N.size();
    const int cols = (int)N.front().size();
    std::vector<std::vector<double>> pinv(cols,
                                          std::vector<double>(rows, 0.0));
    for (int i = 0; i < cols; ++i)
        for (int j = 0; j < rows; ++j) {
            double sum = 0.0;
            for (int p = 0; p < k; ++p)
                if (s.sv[p] > 0.0)
                    sum += s.V[i][p] * (1.0 / s.sv[p]) * s.U[j][p];
            pinv[i][j] = sum;
        }
    addStep(r, "step.title.compute_the_pseudoinverse",
            Sym("A^+ = V S^+ U^T = " + matText(approxMatrix(pinv))),
            "step.note.the_pseudoinverse_inverts_only_the_nonzero_singular_directions");
    const std::vector<std::vector<double>> AAplusA =
        multiplyNumeric(multiplyNumeric(N, pinv), N);
    addCheck(r, "check.pseudoinverse_satisfies_the_penrose_identity",
             closeMatrix(AAplusA, N), "", "", "A A^+ A = A");

    const double smax = s.sv.empty() ? 0.0 : s.sv.front();
    const double smin = s.sv.empty() ? 0.0 : s.sv.back();
    std::string condition = smin > 0.0 ? numStr(smax / smin, 6) : "inf";
    r.solution = Sym("sigma = (" + sigmaText + "),   rank = " +
                     std::to_string(s.rank) + ",   cond(A) = " + condition);
    r.functions.push_back("U = " + matText(approxMatrix(s.U)));
    r.functions.push_back("S = " + matText(approxMatrix(s.S)));
    r.functions.push_back("V = " + matText(approxMatrix(s.V)));
    r.functions.push_back("A^+ = " + matText(approxMatrix(pinv)));
    r.assumptions.push_back("assumption.generic_symbolic_pivots");
    return r;
}

// ---------------------------------------------------------------- norms
Result linalgNorm(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_norm", "norm");
    SMat A;
    std::string code;
    if (!parseMatrixText(req.text("matrix", "1, -2; 3, 4"), A, code, 4)) {
        failCode(r, code);
        return r;
    }
    addStep(r, "step.title.write_the_matrix", Sym("A = " + matText(A)));
    std::vector<std::vector<double>> N;
    if (!numericMatrix(A, N)) {
        failCode(r, "linalg_numeric_required");
        return r;
    }
    const int m = (int)N.size();
    const int n = (int)N.front().size();
    double one = 0.0, inf = 0.0, fro = 0.0;
    for (int j = 0; j < n; ++j) {
        double s = 0.0;
        for (int i = 0; i < m; ++i) s += std::fabs(N[i][j]);
        one = std::max(one, s);
    }
    for (int i = 0; i < m; ++i) {
        double s = 0.0;
        for (int j = 0; j < n; ++j) s += std::fabs(N[i][j]);
        inf = std::max(inf, s);
    }
    for (int i = 0; i < m; ++i)
        for (int j = 0; j < n; ++j) fro += N[i][j] * N[i][j];
    fro = std::sqrt(fro);
    const SvdData s = computeSvd(N);
    const double spectral = s.sv.empty() ? 0.0 : s.sv.front();
    const double smin = s.sv.empty() ? 0.0 : s.sv.back();
    const double cond = smin > 0.0 ? spectral / smin : INFINITY;

    addStep(r, "step.title.compute_the_norms",
            Sym("||A||_1 = " + numStr(one, 8) + ",   ||A||_inf = " +
                numStr(inf, 8) + ",   ||A||_F = " + numStr(fro, 8)),
            "step.note.the_one_and_infinity_norms_are_maximum_column_and_row_sums");
    addStep(r, "step.title.compute_the_condition_number",
            Sym("||A||_2 = " + numStr(spectral, 8) + ",   cond_2(A) = " +
                (std::isfinite(cond) ? numStr(cond, 6) : "inf")),
            "step.note.the_condition_number_measures_error_amplification");
    addCheck(r, "check.spectral_norm_is_at_most_the_frobenius_norm",
             spectral <= fro + 1e-8, "", "", "||A||_2 <= ||A||_F");
    addCheck(r, "check.norm_two_is_at_most_the_geometric_mean",
             spectral <= std::sqrt(one * inf) + 1e-8, "", "",
             "||A||_2 <= sqrt(||A||_1 ||A||_inf)");

    r.solution = Sym("||A||_1 = " + numStr(one, 6) + ",   ||A||_inf = " +
                     numStr(inf, 6) + ",   ||A||_2 = " +
                     numStr(spectral, 6) + ",   cond_2(A) = " +
                     (std::isfinite(cond) ? numStr(cond, 6) : "inf"));
    r.assumptions.push_back("assumption.generic_symbolic_pivots");
    return r;
}

// ---------------------------------------------------------------- power iteration
Result linalgPowerIteration(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_power_iteration", "lambda");
    SMat A;
    std::string code;
    if (!parseMatrixText(req.text("matrix", "2, 1; 1, 2"), A, code, 4)) {
        failCode(r, code);
        return r;
    }
    if (A.size() != A.front().size()) {
        failCode(r, "linalg_not_square");
        return r;
    }
    const int n = (int)A.size();
    addStep(r, "step.title.write_the_matrix", Sym("A = " + matText(A)));
    std::vector<std::vector<double>> N;
    if (!numericMatrix(A, N)) {
        failCode(r, "linalg_numeric_required");
        return r;
    }
    std::vector<Ex> startEx;
    std::vector<double> x(n, 1.0);
    if (parseVectorText(req.text("start", ""), startEx, code) &&
        (int)startEx.size() == n) {
        for (int i = 0; i < n; ++i) {
            bool ok = false;
            x[i] = evalReal(startEx[i], {}, &ok);
            if (!ok) x[i] = 1.0;
        }
    }
    double norm = 0.0;
    for (double v : x) norm += v * v;
    norm = std::sqrt(norm);
    for (double& v : x) v /= norm;
    addStep(r, "step.title.write_the_vectors", Sym("x_0 = " + tupleText(
                  [&] {
                      std::vector<Ex> v(n, Num(0));
                      for (int i = 0; i < n; ++i) v[i] = approxEx(x[i]);
                      return v;
                  }())),
              "step.note.power_iteration_follows_the_dominant_eigenvector");

    double mu = 0.0;
    bool converged = false;
    for (int iter = 1; iter <= 80; ++iter) {
        std::vector<double> y(n, 0.0);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) y[i] += N[i][j] * x[j];
        double ynorm = 0.0;
        for (double v : y) ynorm += v * v;
        ynorm = std::sqrt(ynorm);
        if (ynorm < 1e-14) break;
        for (int i = 0; i < n; ++i) x[i] = y[i] / ynorm;
        std::vector<double> ax(n, 0.0);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) ax[i] += N[i][j] * x[j];
        const double next = dotNumeric(x, ax);
        mu = next;
        double stepResidual = 0.0;
        for (int i = 0; i < n; ++i) {
            const double d = ax[i] - mu * x[i];
            stepResidual += d * d;
        }
        stepResidual = std::sqrt(stepResidual);
        if (iter > 1 && stepResidual < 1e-11) converged = true;
        if (iter == 1 || iter % 8 == 0 || converged) {
            addStep(r, "step.title.iterate",
                    Sym("k = " + std::to_string(iter) + ",   lambda = " +
                        numStr(mu, 8) + ",   x = " + tupleText([&] {
                            std::vector<Ex> v(n, Num(0));
                            for (int i = 0; i < n; ++i) v[i] = approxEx(x[i]);
                            return v;
                        }())),
                    "step.note.power_iteration_follows_the_dominant_eigenvector");
        }
        if (converged) break;
    }
    std::vector<double> ax(n, 0.0);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) ax[i] += N[i][j] * x[j];
    double residual = 0.0;
    for (int i = 0; i < n; ++i) {
        const double d = ax[i] - mu * x[i];
        residual += d * d;
    }
    residual = std::sqrt(residual);
    addCheck(r, "check.eigenvectors_satisfy_av_lambda_v", residual < 1e-5,
             "", "", "||A x - lambda x|| = " + numStr(residual, 4));
    if (!converged) r.warnings.push_back("warning.power_iteration_not_converged");
    std::vector<Ex> xEx(n, Num(0));
    for (int i = 0; i < n; ++i) xEx[i] = approxEx(x[i]);
    r.solution = Sym("lambda_1 = " + numStr(mu, 8) + ",   v_1 = " +
                     tupleText(xEx));
    r.functions.push_back("lambda_1 = " + numStr(mu, 8));
    r.functions.push_back("v_1 = " + tupleText(xEx));
    r.assumptions.push_back("assumption.generic_symbolic_pivots");
    return r;
}

// ---------------------------------------------------------------- QR algorithm
// Householder reduction of a symmetric matrix to tridiagonal form
// (Trefethen & Bau, Algorithm 26.1).  On a tridiagonal matrix a negligible
// subdiagonal entry really does split off a converged eigenvalue, which is
// what makes deflation in the shifted QR iteration valid.
void tridiagonalizeSymmetric(std::vector<std::vector<double>>& T) {
    const int n = (int)T.size();
    for (int k = 0; k + 2 < n; ++k) {
        double norm = 0.0;
        for (int i = k + 1; i < n; ++i) norm += T[i][k] * T[i][k];
        norm = std::sqrt(norm);
        if (norm < 1e-300) continue;
        std::vector<double> v(n - k - 1, 0.0);
        for (int i = k + 1; i < n; ++i) v[i - k - 1] = T[i][k];
        const double alpha = v[0] >= 0.0 ? -norm : norm;
        v[0] -= alpha;
        double vv = 0.0;
        for (double t : v) vv += t * t;
        if (vv < 1e-300) continue;
        std::vector<double> y(n, 0.0);
        for (int i = 0; i < n; ++i) {
            double s = 0.0;
            for (int t = 0; t < (int)v.size(); ++t)
                s += T[i][k + 1 + t] * v[t];
            y[i] = s;
        }
        double c = 0.0;
        for (int t = 0; t < (int)v.size(); ++t) c += v[t] * y[k + 1 + t];
        for (int i = k + 1; i < n; ++i)
            for (int j = k + 1; j < n; ++j)
                T[i][j] -= 2.0 / vv *
                               (v[i - k - 1] * y[j] + y[i] * v[j - k - 1]) -
                           4.0 * c / (vv * vv) * v[i - k - 1] * v[j - k - 1];
        for (int i = k + 2; i < n; ++i) T[i][k] = T[k][i] = 0.0;
        T[k + 1][k] = T[k][k + 1] = alpha;
    }
}

// Householder QR (Trefethen & Bau, Algorithm 10.1).  The shifted matrix in the
// QR iteration is nearly singular, and modified Gram-Schmidt loses
// orthogonality exactly there, so the reflector form is used instead.
void householderQr(const std::vector<std::vector<double>>& A,
                   std::vector<std::vector<double>>& Q,
                   std::vector<std::vector<double>>& R) {
    const int n = (int)A.size();
    R = A;
    Q.assign(n, std::vector<double>(n, 0.0));
    for (int i = 0; i < n; ++i) Q[i][i] = 1.0;
    for (int k = 0; k + 1 < n; ++k) {
        double norm = 0.0;
        for (int i = k; i < n; ++i) norm += R[i][k] * R[i][k];
        norm = std::sqrt(norm);
        if (norm < 1e-300) continue;
        std::vector<double> v(n - k, 0.0);
        for (int i = k; i < n; ++i) v[i - k] = R[i][k];
        const double alpha = v[0] >= 0.0 ? -norm : norm;
        v[0] -= alpha;
        double vv = 0.0;
        for (double t : v) vv += t * t;
        if (vv < 1e-300) continue;
        for (int j = k; j < n; ++j) {
            double d = 0.0;
            for (int i = k; i < n; ++i) d += v[i - k] * R[i][j];
            d = 2.0 * d / vv;
            for (int i = k; i < n; ++i) R[i][j] -= d * v[i - k];
        }
        for (int i = 0; i < n; ++i) {
            double d = 0.0;
            for (int t = k; t < n; ++t) d += Q[i][t] * v[t - k];
            d = 2.0 * d / vv;
            for (int t = k; t < n; ++t) Q[i][t] -= d * v[t - k];
        }
    }
}

Result linalgQrAlgorithm(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_qr_algorithm", "lambda");
    SMat A;
    std::string code;
    if (!parseMatrixText(req.text("matrix", "2, 1; 1, 2"), A, code, 4)) {
        failCode(r, code);
        return r;
    }
    if (A.size() != A.front().size()) {
        failCode(r, "linalg_not_square");
        return r;
    }
    if (!symmetricMatrix(A)) {
        failCode(r, "linalg_not_symmetric");
        return r;
    }
    addStep(r, "step.title.write_the_matrix", Sym("A = " + matText(A)));
    std::vector<std::vector<double>> N;
    if (!numericMatrix(A, N)) {
        failCode(r, "linalg_numeric_required");
        return r;
    }
    const int n = (int)N.size();
    double initialOff = 0.0;
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j) initialOff += N[i][j] * N[i][j];
    initialOff = std::sqrt(initialOff);
    std::vector<std::vector<double>> Ak = N;
    double finalOff = initialOff;
    tridiagonalizeSymmetric(Ak);
    // Shifted QR with deflation (Trefethen & Bau, Algorithm 25.2).  A shift
    // near the trailing eigenvalue makes the last subdiagonal entry vanish
    // quickly, and the active block then shrinks by one.
    int active = n;
    int iterations = 0;
    while (active > 1 && iterations < 200) {
        int lo = active - 1;
        while (lo > 0) {
            const double scale = std::fabs(Ak[lo - 1][lo - 1]) +
                                 std::fabs(Ak[lo][lo]);
            if (std::fabs(Ak[lo][lo - 1]) <= 1e-14 * std::max(1.0, scale))
                break;
            --lo;
        }
        if (lo == active - 1) {
            Ak[active - 1][active - 2] = 0.0;
            Ak[active - 2][active - 1] = 0.0;
            --active;
            continue;
        }
        ++iterations;
        const double a = Ak[active - 2][active - 2];
        const double b = Ak[active - 2][active - 1];
        const double d = Ak[active - 1][active - 1];
        double mu = d;
        if (std::fabs(b) > 1e-300) {
            const double delta = 0.5 * (a - d);
            const double sign = delta >= 0.0 ? 1.0 : -1.0;
            mu = d - b * b / (delta + sign * std::sqrt(delta * delta + b * b));
        }
        const int size = active - lo;
        std::vector<std::vector<double>> B(size, std::vector<double>(size, 0.0));
        for (int i = 0; i < size; ++i)
            for (int j = 0; j < size; ++j)
                B[i][j] = Ak[lo + i][lo + j] - (i == j ? mu : 0.0);
        std::vector<std::vector<double>> Qb, Rb;
        householderQr(B, Qb, Rb);
        const std::vector<std::vector<double>> C = multiplyNumeric(Rb, Qb);
        for (int i = 0; i < size; ++i)
            for (int j = 0; j < size; ++j)
                Ak[lo + i][lo + j] = C[i][j] + (i == j ? mu : 0.0);
        for (int i = lo; i < active; ++i)
            for (int j = i + 1; j < active; ++j) {
                const double avg = 0.5 * (Ak[i][j] + Ak[j][i]);
                Ak[i][j] = Ak[j][i] = avg;
            }
        // The QR step preserves the tridiagonal band; drop the round-off that
        // appears just outside it so the deflation test stays meaningful.
        for (int i = lo; i < active; ++i)
            for (int j = lo; j < active; ++j)
                if (i - j > 1 || j - i > 1) Ak[i][j] = 0.0;
        finalOff = 0.0;
        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j) finalOff += Ak[i][j] * Ak[i][j];
        finalOff = std::sqrt(finalOff);
        if (iterations == 1 || iterations % 5 == 0) {
            addStep(r, "step.title.qr_iteration",
                    Sym("A_" + std::to_string(iterations) + " = " + matText(
                            approxMatrix(Ak)) + ",   offdiag = " +
                        numStr(finalOff, 6)),
                    "step.note.the_qr_iteration_pushes_the_matrix_toward_triangular_form");
        }
    }
    finalOff = 0.0;
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j) finalOff += Ak[i][j] * Ak[i][j];
    finalOff = std::sqrt(finalOff);
    addCheck(r, "check.qr_iteration_reduces_the_off_diagonal_part",
             finalOff <= initialOff + 1e-9, "", "",
             "offdiag(A_k) decreases");
    if (finalOff > 1e-6) r.warnings.push_back("warning.qr_not_converged");
    std::vector<Ex> values(n, Num(0));
    std::string list;
    for (int i = 0; i < n; ++i) {
        values[i] = approxEx(Ak[i][i]);
        if (i) list += ",   ";
        list += "lambda_" + std::to_string(i + 1) + " = " +
                numStr(Ak[i][i], 8);
    }
    addStep(r, "step.title.find_the_eigenvalues", Sym(list),
            "step.note.a_converged_qr_iteration_exposes_the_eigenvalues_on_the_diagonal");
    r.solution = Sym(list);
    for (int i = 0; i < n; ++i)
        r.functions.push_back("lambda_" + std::to_string(i + 1) + " = " +
                              numStr(Ak[i][i], 8));
    r.assumptions.push_back("assumption.generic_symbolic_pivots");
    return r;
}

// ---------------------------------------------------------------- iterative solvers
Result linalgIterative(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_iterative", "x");
    SMat A;
    std::string code;
    if (!parseMatrixText(req.text("matrix", "4, 1; 1, 3"), A, code, 4)) {
        failCode(r, code);
        return r;
    }
    std::vector<Ex> b;
    if (!parseVectorText(req.text("rhs", "1, 2"), b, code)) {
        failCode(r, "linalg_bad_rhs");
        return r;
    }
    const int n = (int)A.size();
    if (A.size() != A.front().size() || (int)b.size() != n) {
        failCode(r, "linalg_dimension_mismatch");
        return r;
    }
    addStep(r, "step.title.write_the_matrix",
            Sym("A = " + matText(A) + ",   b = " + tupleText(b)));
    std::vector<std::vector<double>> N;
    if (!numericMatrix(A, N)) {
        failCode(r, "linalg_numeric_required");
        return r;
    }
    std::vector<double> rhs(n, 0.0);
    for (int i = 0; i < n; ++i) {
        if (hasSymbols(b[i])) {
            failCode(r, "linalg_numeric_required");
            return r;
        }
        bool ok = false;
        rhs[i] = evalReal(b[i], {}, &ok);
        if (!ok) {
            failCode(r, "linalg_numeric_required");
            return r;
        }
    }
    for (int i = 0; i < n; ++i)
        if (std::fabs(N[i][i]) < 1e-14) {
            failCode(r, "linalg_singular_matrix");
            return r;
        }
    const bool gaussSeidel = req.text("method", "jacobi") == "gauss_seidel";
    std::vector<double> x(n, 0.0);
    std::vector<double> old(n, 0.0);
    double initialResidual = 0.0;
    for (int i = 0; i < n; ++i) initialResidual += rhs[i] * rhs[i];
    initialResidual = std::sqrt(initialResidual);
    int lastIteration = 0;
    bool blewUp = false;
    for (int iter = 1; iter <= 40; ++iter) {
        lastIteration = iter;
        old = x;
        for (int i = 0; i < n; ++i) {
            double s = rhs[i];
            for (int j = 0; j < n; ++j) {
                if (j == i) continue;
                s -= N[i][j] * (gaussSeidel ? x[j] : old[j]);
            }
            x[i] = s / N[i][i];
        }
        double change = 0.0;
        for (int i = 0; i < n; ++i)
            change = std::max(change, std::fabs(x[i] - old[i]));
        if (iter == 1 || iter % 4 == 0 || change < 1e-14) {
            addStep(r, "step.title.iterate",
                    Sym("k = " + std::to_string(iter) + ",   x = " +
                        tupleText([&] {
                            std::vector<Ex> v(n, Num(0));
                            for (int i = 0; i < n; ++i) v[i] = approxEx(x[i]);
                            return v;
                        }())),
                    "step.note.jacobi_and_gauss_seidel_trade_storage_for_iterations");
        }
        double magnitude = 0.0;
        for (double v : x) magnitude = std::max(magnitude, std::fabs(v));
        if (magnitude > 1e6) {
            blewUp = true;
            break;
        }
        if (change < 1e-15) break;
    }
    std::vector<double> ax(n, 0.0);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) ax[i] += N[i][j] * x[j];
    double residual = 0.0;
    for (int i = 0; i < n; ++i) residual += (ax[i] - rhs[i]) * (ax[i] - rhs[i]);
    residual = std::sqrt(residual);
    addCheck(r, "check.iterative_residual_is_small", residual < 1e-5, "", "",
             "||A x - b|| = " + numStr(residual, 4));
    const bool diverged =
        blewUp || (initialResidual > 0.0 && residual > initialResidual);
    if (diverged) r.warnings.push_back("warning.iterative_diverged");
    std::vector<Ex> xEx(n, Num(0));
    for (int i = 0; i < n; ++i) xEx[i] = approxEx(x[i]);
    const std::string label = diverged
        ? "x_" + std::to_string(lastIteration)
        : "x";
    r.solution = Sym(label + " = " + tupleText(xEx) +
                     ",   ||A x - b|| = " + numStr(residual, 6));
    r.functions.push_back(label + " = " + tupleText(xEx));
    r.assumptions.push_back("assumption.generic_symbolic_pivots");
    return r;
}

// ---------------------------------------------------------------- linear programming
Result linalgLp(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_lp", "z");
    std::vector<Ex> c;
    std::string code;
    if (!parseVectorText(req.text("objective", "3, 2"), c, code) ||
        (int)c.size() != 2) {
        failCode(r, "linalg_dimension_mismatch");
        return r;
    }
    SMat A;
    if (!parseMatrixText(req.text("constraints", "1, 1; 1, 0; 0, 1"), A,
                         code, 8) ||
        (int)A.front().size() != 2) {
        failCode(r, "linalg_dimension_mismatch");
        return r;
    }
    std::vector<Ex> b;
    if (!parseVectorText(req.text("bounds", "4, 2, 3"), b, code) ||
        b.size() != A.size()) {
        failCode(r, "linalg_bad_rhs");
        return r;
    }
    const int m = (int)A.size();
    addStep(r, "step.title.write_the_problem",
            Sym("max " + tupleText(c) + " . x   s.t.   A x <= b,   x >= 0"),
            "step.note.the_simplex_method_walks_between_vertices");
    addStep(r, "step.title.write_the_matrix",
            Sym("A = " + matText(A) + ",   b = " + tupleText(b) +
                ",   c = " + tupleText(c)));

    std::vector<std::vector<double>> Ad(m, std::vector<double>(2, 0.0));
    std::vector<double> bd(m, 0.0), cd(2, 0.0);
    for (int i = 0; i < m; ++i) {
        if (hasSymbols(b[i])) {
            failCode(r, "linalg_numeric_required");
            return r;
        }
        bool ok = false;
        bd[i] = evalReal(b[i], {}, &ok);
        if (!ok) {
            failCode(r, "linalg_numeric_required");
            return r;
        }
        if (bd[i] < -1e-12) {
            failCode(r, "linalg_lp_negative_rhs");
            return r;
        }
        for (int j = 0; j < 2; ++j) {
            if (hasSymbols(A[i][j])) {
                failCode(r, "linalg_numeric_required");
                return r;
            }
            Ad[i][j] = evalReal(A[i][j], {}, &ok);
            if (!ok) {
                failCode(r, "linalg_numeric_required");
                return r;
            }
        }
    }
    for (int j = 0; j < 2; ++j) {
        if (hasSymbols(c[j])) {
            failCode(r, "linalg_numeric_required");
            return r;
        }
        bool ok = false;
        cd[j] = evalReal(c[j], {}, &ok);
        if (!ok) {
            failCode(r, "linalg_numeric_required");
            return r;
        }
    }

    const int cols = 2 + m + 1;
    std::vector<std::vector<double>> T(m + 1,
                                       std::vector<double>(cols, 0.0));
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < 2; ++j) T[i][j] = Ad[i][j];
        T[i][2 + i] = 1.0;
        T[i][cols - 1] = bd[i];
    }
    for (int j = 0; j < 2; ++j) T[m][j] = -cd[j];
    std::vector<int> basis(m, 0);
    for (int i = 0; i < m; ++i) basis[i] = 2 + i;
    addStep(r, "step.title.build_the_tableau",
            Sym("T = " + matText(approxMatrix(T))),
            "step.note.the_tableau_holds_the_slacks_and_the_reduced_costs");

    bool unbounded = false;
    for (int iter = 0; iter < 50; ++iter) {
        int enter = -1;
        double best = -1e-9;
        for (int j = 0; j < cols - 1; ++j)
            if (T[m][j] < best) {
                best = T[m][j];
                enter = j;
            }
        if (enter < 0) break;
        int leave = -1;
        double bestRatio = 1e18;
        for (int i = 0; i < m; ++i) {
            if (T[i][enter] > 1e-9) {
                const double ratio = T[i][cols - 1] / T[i][enter];
                if (ratio < bestRatio - 1e-12) {
                    bestRatio = ratio;
                    leave = i;
                }
            }
        }
        if (leave < 0) {
            unbounded = true;
            break;
        }
        const double pivot = T[leave][enter];
        for (int j = 0; j < cols; ++j) T[leave][j] /= pivot;
        for (int i = 0; i <= m; ++i) {
            if (i == leave) continue;
            const double factor = T[i][enter];
            if (std::fabs(factor) < 1e-15) continue;
            for (int j = 0; j < cols; ++j) T[i][j] -= factor * T[leave][j];
        }
        basis[leave] = enter;
        addStep(r, "step.title.eliminate",
                Sym("pivot row " + std::to_string(leave + 1) + ", column " +
                    std::to_string(enter + 1)),
                "step.note.the_simplex_method_walks_between_vertices");
    }
    if (unbounded) {
        failCode(r, "linalg_lp_unbounded");
        return r;
    }
    std::vector<double> x(2, 0.0);
    for (int i = 0; i < m; ++i)
        if (basis[i] < 2) x[basis[i]] = T[i][cols - 1];
    const double value = cd[0] * x[0] + cd[1] * x[1];
    bool feasible = true;
    for (int i = 0; i < m; ++i) {
        double lhs = Ad[i][0] * x[0] + Ad[i][1] * x[1];
        if (lhs > bd[i] + 1e-7) feasible = false;
    }
    if (!feasible) {
        failCode(r, "linalg_lp_infeasible");
        return r;
    }
    addCheck(r, "check.simplex_vertex_is_feasible", feasible, "", "",
             "A x <= b, x >= 0");
    addStep(r, "step.title.read_the_optimum",
            Sym("x = (" + numStr(x[0], 8) + ", " + numStr(x[1], 8) +
                "),   z = " + numStr(value, 8)),
            "step.note.the_optimum_is_attained_at_a_vertex");
    r.solution = Sym("x = (" + numStr(x[0], 6) + ", " + numStr(x[1], 6) +
                     "),   z = " + numStr(value, 6));
    r.assumptions.push_back("assumption.generic_symbolic_pivots");
    return r;
}

// ---------------------------------------------------------------- matrix games
Result linalgGame(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_game", "v");
    SMat A;
    std::string code;
    if (!parseMatrixText(req.text("matrix", "3, 0; 0, 2"), A, code, 4)) {
        failCode(r, code);
        return r;
    }
    if (A.size() != 2 || A.front().size() != 2) {
        failCode(r, "linalg_dimension_mismatch");
        return r;
    }
    addStep(r, "step.title.write_the_matrix", Sym("A = " + matText(A)));
    double a[2][2];
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j) {
            if (hasSymbols(A[i][j])) {
                failCode(r, "linalg_numeric_required");
                return r;
            }
            bool ok = false;
            a[i][j] = evalReal(A[i][j], {}, &ok);
            if (!ok) {
                failCode(r, "linalg_numeric_required");
                return r;
            }
        }
    const double rowMin0 = std::min(a[0][0], a[0][1]);
    const double rowMin1 = std::min(a[1][0], a[1][1]);
    const double maximin = std::max(rowMin0, rowMin1);
    const double colMax0 = std::max(a[0][0], a[1][0]);
    const double colMax1 = std::max(a[0][1], a[1][1]);
    const double minimax = std::min(colMax0, colMax1);

    if (std::fabs(maximin - minimax) < 1e-9) {
        const int row = rowMin0 >= rowMin1 ? 0 : 1;
        const int col = colMax0 <= colMax1 ? 0 : 1;
        addStep(r, "step.title.find_the_saddle_point",
                Sym("maximin = minimax = " + numStr(maximin, 8) +
                    ",   row " + std::to_string(row + 1) + ", column " +
                    std::to_string(col + 1)),
                "step.note.a_saddle_point_is_a_pure_strategy_equilibrium");
        // Report the pure equilibrium in the same strategy-vector shape as
        // the mixed case, so the answer always has one readable form.
        const std::string rowStrategy = row == 0 ? "(1, 0)" : "(0, 1)";
        const std::string colStrategy = col == 0 ? "(1, 0)" : "(0, 1)";
        r.solution = Sym("value = " + numStr(maximin, 8) +
                         ",   row strategy = " + rowStrategy +
                         ",   column strategy = " + colStrategy);
        return r;
    }

    const double denom = a[0][0] + a[1][1] - a[0][1] - a[1][0];
    if (std::fabs(denom) < 1e-12) {
        failCode(r, "linalg_numeric_required");
        return r;
    }
    const double value =
        (a[0][0] * a[1][1] - a[0][1] * a[1][0]) / denom;
    const double p = (a[1][1] - a[1][0]) / denom;
    const double q = (a[1][1] - a[0][1]) / denom;
    addStep(r, "step.title.solve_the_mixed_game",
            Sym("p = " + numStr(p, 8) + ",   q = " + numStr(q, 8) +
                ",   value = " + numStr(value, 8)),
            "step.note.mixed_strategies_equalize_the_expected_payoffs");
    const double rowPayoff = p * (q * a[0][0] + (1.0 - q) * a[0][1]) +
                             (1.0 - p) * (q * a[1][0] + (1.0 - q) * a[1][1]);
    addCheck(r, "check.mixed_strategies_attain_the_value",
             std::fabs(rowPayoff - value) < 1e-7, "", "",
             "expected payoff = " + numStr(rowPayoff, 6));
    r.solution = Sym("value = " + numStr(value, 8) + ",   row strategy = (" +
                     numStr(p, 6) + ", " + numStr(1.0 - p, 6) +
                     "),   column strategy = (" + numStr(q, 6) + ", " +
                     numStr(1.0 - q, 6) + ")");
    r.assumptions.push_back("assumption.generic_symbolic_pivots");
    return r;
}

// ---------------------------------------------------------------- spectral theorem
Result linalgSymmetric(const TopicRequest& req) {
    Result r;
    baseId(r, "linalg_symmetric", "Q");
    SMat A;
    std::string code;
    if (!parseMatrixText(req.text("matrix", "2, 1; 1, 2"), A, code, 4)) {
        failCode(r, code);
        return r;
    }
    if (A.size() != A.front().size()) {
        failCode(r, "linalg_not_square");
        return r;
    }
    if (!symmetricMatrix(A)) {
        failCode(r, "linalg_not_symmetric");
        return r;
    }
    const int n = (int)A.size();
    addStep(r, "step.title.write_the_matrix", Sym("A = " + matText(A)));
    std::vector<std::vector<double>> N;
    if (!numericMatrix(A, N)) {
        failCode(r, "linalg_numeric_required");
        return r;
    }
    std::vector<double> values;
    std::vector<std::vector<double>> vectors;
    jacobiEigen(N, values, vectors);
    std::vector<std::vector<double>> Q(n, std::vector<double>(n, 0.0));
    std::vector<std::vector<double>> D(n, std::vector<double>(n, 0.0));
    for (int i = 0; i < n; ++i) {
        D[i][i] = values[i];
        for (int j = 0; j < n; ++j) Q[i][j] = vectors[i][j];
    }
    std::string eigenText;
    for (int i = 0; i < n; ++i) {
        if (i) eigenText += ",   ";
        eigenText += "lambda_" + std::to_string(i + 1) + " = " +
                     numStr(values[i], 8);
    }
    addStep(r, "step.title.find_the_eigenvalues", Sym(eigenText),
            "step.note.a_symmetric_matrix_has_real_eigenvalues");
    addStep(r, "step.title.build_the_orthogonal_diagonalization",
            Sym("A = Q D Q^T,   Q = " + matText(approxMatrix(Q)) +
                ",   D = " + matText(approxMatrix(D))),
            "step.note.the_spectral_theorem_uses_orthonormal_eigenvectors");
    const std::vector<std::vector<double>> QtQ =
        multiplyNumeric(transposeNumeric(Q), Q);
    bool orthonormal = true;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            const double want = (i == j) ? 1.0 : 0.0;
            if (std::fabs(QtQ[i][j] - want) > 1e-7) orthonormal = false;
        }
    addCheck(r, "check.q_has_orthonormal_columns", orthonormal, "", "",
             "Q^T Q = I");
    const std::vector<std::vector<double>> rebuilt = multiplyNumeric(
        multiplyNumeric(Q, D), transposeNumeric(Q));
    addCheck(r, "check.spectral_factorization_reconstructs_the_matrix",
             closeMatrix(rebuilt, N), "", "", "Q D Q^T = A");
    r.solution = Sym("A = Q D Q^T,   " + eigenText);
    r.functions.push_back("Q = " + matText(approxMatrix(Q)));
    r.functions.push_back("D = " + matText(approxMatrix(D)));
    r.assumptions.push_back("assumption.generic_symbolic_pivots");
    return r;
}

} // namespace

Result linalgRankTopic(const TopicRequest& req) { return linalgRank(req); }
Result linalgDetTopic(const TopicRequest& req) { return linalgDet(req); }
Result linalgInverseTopic(const TopicRequest& req) { return linalgInverse(req); }
Result linalgLuTopic(const TopicRequest& req) { return linalgLu(req); }
Result linalgSolveTopic(const TopicRequest& req) { return linalgSolve(req); }
Result linalgSpanTopic(const TopicRequest& req) { return linalgSpan(req); }
Result linalgIndependenceTopic(const TopicRequest& req) {
    return linalgIndependence(req);
}
Result linalgBasisTopic(const TopicRequest& req) { return linalgBasis(req); }
Result linalgCoordinatesTopic(const TopicRequest& req) {
    return linalgCoordinates(req);
}
Result linalgChangeBasisTopic(const TopicRequest& req) {
    return linalgChangeBasis(req);
}
Result linalgSubspaceTopic(const TopicRequest& req) {
    return linalgSubspace(req);
}
Result linalgProjectionTopic(const TopicRequest& req) {
    return linalgProjection(req);
}
Result linalgGramSchmidtTopic(const TopicRequest& req) {
    return linalgGramSchmidt(req);
}
Result linalgQrTopic(const TopicRequest& req) { return linalgQr(req); }
Result linalgLeastSquaresTopic(const TopicRequest& req) {
    return linalgLeastSquares(req);
}
Result linalgDiagonalizeTopic(const TopicRequest& req) {
    return linalgDiagonalize(req);
}
Result linalgPowerTopic(const TopicRequest& req) { return linalgPower(req); }

Result linalgOrthogonalComplementTopic(const TopicRequest& req) {
    return linalgOrthogonalComplement(req);
}
Result linalgGraphTopic(const TopicRequest& req) { return linalgGraph(req); }
Result linalgFftTopic(const TopicRequest& req) { return linalgFft(req); }
Result linalgSimilarityTopic(const TopicRequest& req) {
    return linalgSimilarity(req);
}
Result linalgMatrixExpTopic(const TopicRequest& req) {
    return linalgMatrixExp(req);
}
Result linalgPositiveDefiniteTopic(const TopicRequest& req) {
    return linalgPositiveDefinite(req);
}
Result linalgSvdTopic(const TopicRequest& req) { return linalgSvd(req); }
Result linalgNormTopic(const TopicRequest& req) { return linalgNorm(req); }
Result linalgPowerIterationTopic(const TopicRequest& req) {
    return linalgPowerIteration(req);
}
Result linalgQrAlgorithmTopic(const TopicRequest& req) {
    return linalgQrAlgorithm(req);
}
Result linalgIterativeTopic(const TopicRequest& req) {
    return linalgIterative(req);
}
Result linalgLpTopic(const TopicRequest& req) { return linalgLp(req); }
Result linalgGameTopic(const TopicRequest& req) { return linalgGame(req); }
Result linalgSymmetricTopic(const TopicRequest& req) {
    return linalgSymmetric(req);
}

} // namespace ode
