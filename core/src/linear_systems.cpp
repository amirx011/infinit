// Infinit Lab — linear systems of ODEs.
//
// The scope is deliberately exact: rational matrices with rational (or
// quadratic-surd) eigenvalues get a closed-form fundamental matrix, and
// anything else fails with a stable code instead of returning a guess.
// Numerical methods (80) are clearly labelled as such.
// SPDX-License-Identifier: Apache-2.0
#include "linear_systems.hpp"

#include "expr.hpp"
#include "numeric.hpp"
#include "poly.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace ode {
namespace {

constexpr double kPi = 3.14159265358979323846;

using QMat = std::vector<std::vector<Rational>>;

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

void addStep(Result& r, const std::string& titleKey, const Ex& math = nullptr,
             const std::string& noteKey = "", const std::string& params = "") {
    Step s;
    s.titleKey = titleKey;
    s.math = math;
    s.noteKey = noteKey;
    s.params = params;
    r.steps.push_back(std::move(s));
}

void addStepRaw(Result& r, const std::string& titleKey, const Ex& math,
                const std::string& rawNote) {
    Step s;
    s.titleKey = titleKey;
    s.math = math;
    s.note = rawNote;
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

std::string rat(Rational q) { return rationalToString(q); }

// ---------------------------------------------------------------- parsing
bool parseRationalText(const std::string& raw, Rational& out) {
    std::string s = raw;
    while (!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
    if (s.empty()) return false;
    const size_t slash = s.find('/');
    try {
        if (slash != std::string::npos) {
            const Rational a = Rational::fromDouble(std::stod(s.substr(0, slash)), 1000000);
            const Rational b = Rational::fromDouble(std::stod(s.substr(slash + 1)), 1000000);
            if (b.isZero()) return false;
            out = a / b;
            return true;
        }
        out = Rational::fromDouble(std::stod(s), 1000000);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseMatrixText(const std::string& text, QMat& A) {
    std::string norm = text;
    for (char& c : norm) {
        if (c == '[' || c == ']') c = ' ';
    }
    std::vector<std::vector<Rational>> rows;
    std::stringstream rowStream(norm);
    std::string row;
    while (std::getline(rowStream, row, ';')) {
        std::vector<Rational> values;
        std::stringstream cellStream(row);
        std::string cell;
        while (std::getline(cellStream, cell, ',')) {
            Rational q;
            if (!parseRationalText(cell, q)) return false;
            values.push_back(q);
        }
        if (!values.empty()) rows.push_back(values);
    }
    if (rows.empty()) return false;
    const size_t n = rows.size();
    for (const auto& values : rows) {
        if (values.size() != n) return false;
    }
    A = rows;
    return true;
}

std::string matrixText(const QMat& A) {
    std::string s = "[";
    for (size_t i = 0; i < A.size(); ++i) {
        if (i) s += "; ";
        s += "[";
        for (size_t j = 0; j < A[i].size(); ++j) {
            if (j) s += ", ";
            s += rat(A[i][j]);
        }
        s += "]";
    }
    return s + "]";
}

// ---------------------------------------------------------------- matrices
QMat identityMatrix(int n) {
    QMat I(n, std::vector<Rational>(n, Rational(0)));
    for (int i = 0; i < n; ++i) I[i][i] = Rational(1);
    return I;
}

QMat multiplyMatrices(const QMat& A, const QMat& B) {
    const int n = (int)A.size();
    QMat C(n, std::vector<Rational>(n, Rational(0)));
    for (int i = 0; i < n; ++i)
        for (int k = 0; k < n; ++k)
            if (!A[i][k].isZero())
                for (int j = 0; j < n; ++j)
                    C[i][j] = C[i][j] + A[i][k] * B[k][j];
    return C;
}

QMat addMatrices(const QMat& A, const QMat& B) {
    const int n = (int)A.size();
    QMat C(n, std::vector<Rational>(n, Rational(0)));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) C[i][j] = A[i][j] + B[i][j];
    return C;
}

QMat scaleMatrix(const QMat& A, Rational s) {
    const int n = (int)A.size();
    QMat C(n, std::vector<Rational>(n, Rational(0)));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) C[i][j] = A[i][j] * s;
    return C;
}

QMat matrixPower(const QMat& A, int k) {
    QMat R = identityMatrix((int)A.size());
    for (int i = 0; i < k; ++i) R = multiplyMatrices(R, A);
    return R;
}

std::vector<Rational> matVec(const QMat& A, const std::vector<Rational>& v) {
    std::vector<Rational> out(A.size(), Rational(0));
    for (size_t i = 0; i < A.size(); ++i)
        for (size_t j = 0; j < v.size(); ++j) out[i] = out[i] + A[i][j] * v[j];
    return out;
}

bool isZeroVec(const std::vector<Rational>& v) {
    for (const auto& q : v) if (!q.isZero()) return false;
    return true;
}

Rational matrixTrace(const QMat& A) {
    Rational t = 0;
    for (size_t i = 0; i < A.size(); ++i) t = t + A[i][i];
    return t;
}

Rational matrixDeterminant(QMat M) {
    const int n = (int)M.size();
    Rational det = 1;
    for (int col = 0; col < n; ++col) {
        int pivot = col;
        while (pivot < n && M[pivot][col].isZero()) ++pivot;
        if (pivot == n) return Rational(0);
        if (pivot != col) {
            std::swap(M[pivot], M[col]);
            det = Rational(-det.n, det.d);
        }
        det = det * M[col][col];
        const Rational p = M[col][col];
        for (int row = col + 1; row < n; ++row) {
            if (M[row][col].isZero()) continue;
            const Rational factor = M[row][col] / p;
            for (int j = col; j < n; ++j)
                M[row][j] = M[row][j] - factor * M[col][j];
        }
    }
    return det;
}

// Faddeev–LeVerrier: p(r) = r^n + c[1] r^{n-1} + ... + c[n].
Poly characteristicPolynomial(const QMat& A) {
    const int n = (int)A.size();
    Poly p;
    p.c.assign(n + 1, Rational(0));
    p.c[n] = Rational(1);
    QMat M = identityMatrix(n);
    for (int k = 1; k <= n; ++k) {
        const QMat AM = multiplyMatrices(A, M);
        const Rational c = Rational(-matrixTrace(AM).n, matrixTrace(AM).d) / Rational(k);
        p.c[n - k] = c;
        M = addMatrices(AM, scaleMatrix(identityMatrix(n), c));
    }
    p.trim();
    return p;
}

// ---------------------------------------------------------------- nullspace
// Reduced row echelon form over Q; returns the pivot columns.
int rowReduce(QMat& M, std::vector<int>& pivots) {
    const int rows = (int)M.size();
    const int cols = rows ? (int)M[0].size() : 0;
    int rank = 0;
    pivots.clear();
    for (int col = 0; col < cols && rank < rows; ++col) {
        int pivot = rank;
        while (pivot < rows && M[pivot][col].isZero()) ++pivot;
        if (pivot == rows) continue;
        std::swap(M[pivot], M[rank]);
        const Rational p = M[rank][col];
        for (int j = col; j < cols; ++j) M[rank][j] = M[rank][j] / p;
        for (int row = 0; row < rows; ++row) {
            if (row == rank || M[row][col].isZero()) continue;
            const Rational f = M[row][col];
            for (int j = col; j < cols; ++j)
                M[row][j] = M[row][j] - f * M[rank][j];
        }
        pivots.push_back(col);
        ++rank;
    }
    return rank;
}

std::vector<std::vector<Rational>> nullspace(const QMat& A) {
    QMat M = A;
    std::vector<int> pivots;
    const int rank = rowReduce(M, pivots);
    const int n = A.empty() ? 0 : (int)A[0].size();
    std::vector<bool> isPivot(n, false);
    for (int c : pivots) isPivot[c] = true;
    std::vector<std::vector<Rational>> basis;
    for (int freeCol = 0; freeCol < n; ++freeCol) {
        if (isPivot[freeCol]) continue;
        std::vector<Rational> v(n, Rational(0));
        v[freeCol] = Rational(1);
        for (int r = 0; r < rank; ++r)
            v[pivots[r]] = Rational(-M[r][freeCol].n, M[r][freeCol].d);
        basis.push_back(v);
    }
    return basis;
}

int matrixRank(QMat M) {
    std::vector<int> pivots;
    return rowReduce(M, pivots);
}

bool inSpan(const QMat& rows, const std::vector<Rational>& v) {
    const int before = matrixRank(rows);
    QMat extended = rows;
    extended.push_back(v);
    return matrixRank(extended) == before;
}

// ---------------------------------------------------------------- eigenvalues
struct RootInfo {
    bool complex = false;
    Rational real = 0;      // real part for a complex pair, the root otherwise
    Rational imag = 0;      // positive imaginary part when complex
    bool imagSqrt = false;  // beta is sqrt(-disc)/(2a), not rational
    Ex betaEx;              // symbolic beta when imagSqrt
    int mult = 1;
};

// Factor the characteristic polynomial into rational linear factors and
// irreducible quadratics.  Irrational roots are reported, never guessed.
bool eigenvalueData(const Poly& ch, std::vector<RootInfo>& roots, std::string& code) {
    std::vector<Factor> factors;
    if (!factorPolynomial(ch, factors) || factors.empty()) {
        code = "unsupported_characteristic_polynomial";
        return false;
    }
    Poly product;
    product.c = {Rational(1)};
    for (const Factor& f : factors) product = polyMul(product, polyPow(f.p, f.mult));
    if (product.deg() != ch.deg()) {
        code = "unsupported_characteristic_polynomial";
        return false;
    }
    for (const Factor& f : factors) {
        if (f.p.deg() == 1) {
            RootInfo root;
            root.real = Rational(-f.p.c[0].n * f.p.c[1].d, f.p.c[0].d * f.p.c[1].n);
            root.mult = f.mult;
            roots.push_back(root);
        } else if (f.p.deg() == 2) {
            const Rational a = f.p.c[2], b = f.p.c[1], c = f.p.c[0];
            const Rational disc = b * b - Rational(4) * a * c;
            if (disc.isNeg()) {
                RootInfo pair;
                pair.complex = true;
                pair.real = Rational(-b.n, b.d) / (Rational(2) * a);
                const Rational beta2 = Rational(-disc.n, disc.d) /
                                       (Rational(4) * a * a);
                const long long bn = (long long)std::llround(std::sqrt((double)beta2.n));
                const long long bd = (long long)std::llround(std::sqrt((double)beta2.d));
                if (bn * bn == beta2.n && bd * bd == beta2.d) {
                    pair.imag = Rational(bn, bd);
                } else {
                    pair.imagSqrt = true;
                    pair.betaEx = Sqrt(Num(beta2));
                }
                pair.mult = f.mult;
                roots.push_back(pair);
            } else if (disc.isZero()) {
                RootInfo root;
                root.real = Rational(-b.n, b.d) / (Rational(2) * a);
                root.mult = f.mult;
                roots.push_back(root);
            } else {
                const long long sn = (long long)std::llround(std::sqrt((double)disc.n));
                const long long sd = (long long)std::llround(std::sqrt((double)disc.d));
                if (sn * sn != disc.n || sd * sd != disc.d) {
                    code = "irrational_eigenvalues";
                    return false;
                }
                const Rational sq(sn, sd);
                for (int sign = 1; sign >= -1; sign -= 2) {
                    RootInfo root;
                    root.real = (Rational(-b.n, b.d) + Rational(sign) * sq) /
                                (Rational(2) * a);
                    root.mult = f.mult;
                    roots.push_back(root);
                }
            }
        } else {
            code = "unsupported_characteristic_polynomial";
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------- e^{At}
// Each entry of the fundamental matrix is a sum of terms
//   coeff * t^power * exp(alpha t) * [cos|sin](beta t).
struct Term {
    Ex coeff;
    Ex alpha;   // exponent rate
    Ex beta;    // trigonometric frequency
    int power = 0;
    int trig = 0; // 0 none, 1 cos, 2 sin
};

using EntryTerms = std::vector<Term>;
using ExMatrix = std::vector<std::vector<EntryTerms>>;

Ex makeTerm(const Term& term) {
    Ex e = term.coeff;
    if (!e) return nullptr;
    if (term.power > 0) e = Mul(e, Pow(Sym("t"), Num(term.power)));
    if (term.alpha && !term.alpha->isZero())
        e = Mul(e, Fn("exp", {Mul(term.alpha, Sym("t"))}));
    if (term.trig == 1) e = Mul(e, Fn("cos", {Mul(term.beta, Sym("t"))}));
    if (term.trig == 2) e = Mul(e, Fn("sin", {Mul(term.beta, Sym("t"))}));
    return simplify(e);
}

Ex entryEx(const EntryTerms& terms) {
    if (terms.empty()) return Num(0);
    std::vector<Ex> parts;
    parts.reserve(terms.size());
    for (const Term& term : terms) parts.push_back(makeTerm(term));
    return simplify(Add(parts));
}

std::string exEntryText(const EntryTerms& terms) { return toPlain(entryEx(terms)); }

std::string exMatrixText(const ExMatrix& M) {
    std::string s = "[";
    for (size_t i = 0; i < M.size(); ++i) {
        if (i) s += "; ";
        s += "[";
        for (size_t j = 0; j < M[i].size(); ++j) {
            if (j) s += ", ";
            s += exEntryText(M[i][j]);
        }
        s += "]";
    }
    return s + "]";
}

// Spectral projector onto the eigenspace of lambda: product over the other
// roots of ((A - mu I)/(lambda - mu))^{m_mu}.
QMat spectralProjector(const QMat& A, const std::vector<RootInfo>& roots, int index) {
    const int n = (int)A.size();
    QMat E = identityMatrix(n);
    const Rational lambda = roots[index].real;
    for (int j = 0; j < (int)roots.size(); ++j) {
        if (j == index) continue;
        const Rational mu = roots[j].real;
        const Rational diff = lambda - mu;
        QMat shifted = addMatrices(A, scaleMatrix(identityMatrix(n), Rational(-mu.n, mu.d)));
        shifted = scaleMatrix(shifted, Rational(diff.d, diff.n));
        for (int k = 0; k < roots[j].mult; ++k) E = multiplyMatrices(E, shifted);
    }
    return E;
}

// e^{At} for a real spectrum (rational eigenvalues, any multiplicity).
ExMatrix realFundamental(const QMat& A, const std::vector<RootInfo>& roots) {
    const int n = (int)A.size();
    ExMatrix phi(n, std::vector<EntryTerms>(n));
    for (int i = 0; i < (int)roots.size(); ++i) {
        const Rational lambda = roots[i].real;
        const QMat E = spectralProjector(A, roots, i);
        const QMat N = addMatrices(A, scaleMatrix(identityMatrix(n), Rational(-lambda.n, lambda.d)));
        for (int k = 0; k < roots[i].mult; ++k) {
            const QMat Nk = matrixPower(N, k);
            const QMat basis = multiplyMatrices(Nk, E);
            Rational fact = 1;
            for (int f = 2; f <= k; ++f) fact = fact * Rational(f);
            const Rational scale = Rational(1) / fact;
            for (int r = 0; r < n; ++r) {
                for (int c = 0; c < n; ++c) {
                    if (basis[r][c].isZero()) continue;
                    Term term;
                    term.coeff = Num(basis[r][c] * scale);
                    term.alpha = Num(lambda);
                    term.power = k;
                    phi[r][c].push_back(term);
                }
            }
        }
    }
    return phi;
}

// e^{At} for a real 2x2 matrix with a complex pair alpha +- i beta:
//   e^{alpha t} [ cos(beta t) I + sin(beta t) (A - alpha I)/beta ].
ExMatrix complex2x2Fundamental(const QMat& A, const RootInfo& pair) {
    const int n = 2;
    ExMatrix phi(n, std::vector<EntryTerms>(n));
    const Rational alpha = pair.real;
    const Ex beta = pair.imagSqrt ? pair.betaEx : Num(pair.imag);
    const QMat B = addMatrices(A, scaleMatrix(identityMatrix(n),
                                              Rational(-alpha.n, alpha.d)));

    for (int i = 0; i < n; ++i) {
        Term cosTerm;
        cosTerm.coeff = Num(1);
        cosTerm.alpha = Num(alpha);
        cosTerm.beta = beta;
        cosTerm.trig = 1;
        phi[i][i].push_back(cosTerm);
    }
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            if (B[r][c].isZero()) continue;
            Term sinTerm;
            sinTerm.coeff = Mul(Num(B[r][c]), Pow(beta, Num(-1)));
            sinTerm.alpha = Num(alpha);
            sinTerm.beta = beta;
            sinTerm.trig = 2;
            phi[r][c].push_back(sinTerm);
        }
    }
    return phi;
}

// e^{At} for 3x3 with one real root and one complex pair (each simple).
// The projector onto the real eigenspace is Q(A)/Q(lambda) with
// Q(r) = r^2 - 2 alpha r + alpha^2 + beta^2; the pair gets I - E_real.
ExMatrix mixedFundamental(const QMat& A, const std::vector<RootInfo>& roots,
                          int realIndex, int pairIndex, bool& ok) {
    ok = false;
    const int n = (int)A.size();
    const RootInfo& pair = roots[pairIndex];
    if (pair.imagSqrt) return {};
    const Rational alpha = pair.real;
    const Rational beta = pair.imag;
    const Rational lambda = roots[realIndex].real;
    const Rational qLambda = (lambda - alpha) * (lambda - alpha) + beta * beta;
    if (qLambda.isZero()) return {};
    const QMat A2 = multiplyMatrices(A, A);
    QMat Q = addMatrices(A2, scaleMatrix(A, Rational(-2) * alpha));
    Q = addMatrices(Q, scaleMatrix(identityMatrix(n), alpha * alpha + beta * beta));
    const QMat Ereal = scaleMatrix(Q, Rational(qLambda.d, qLambda.n));
    const QMat Epair = addMatrices(identityMatrix(n), scaleMatrix(Ereal, Rational(-1)));
    const QMat B = multiplyMatrices(
        addMatrices(A, scaleMatrix(identityMatrix(n), Rational(-alpha.n, alpha.d))),
        Epair);

    ExMatrix phi(n, std::vector<EntryTerms>(n));
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c) {
            if (!Ereal[r][c].isZero()) {
                Term term;
                term.coeff = Num(Ereal[r][c]);
                term.alpha = Num(lambda);
                phi[r][c].push_back(term);
            }
            if (!Epair[r][c].isZero()) {
                Term term;
                term.coeff = Num(Epair[r][c]);
                term.alpha = Num(alpha);
                term.beta = Num(beta);
                term.trig = 1;
                phi[r][c].push_back(term);
            }
            if (!B[r][c].isZero()) {
                Term term;
                term.coeff = Mul(Num(B[r][c]), Num(Rational(beta.d, beta.n)));
                term.alpha = Num(alpha);
                term.beta = Num(beta);
                term.trig = 2;
                phi[r][c].push_back(term);
            }
        }
    ok = true;
    return phi;
}

bool buildFundamental(const QMat& A, ExMatrix& phi, std::string& code) {
    const int n = (int)A.size();
    const Poly ch = characteristicPolynomial(A);
    std::vector<RootInfo> roots;
    if (!eigenvalueData(ch, roots, code)) return false;
    int complexCount = 0, realCount = 0;
    for (const RootInfo& root : roots) {
        if (root.complex) ++complexCount;
        else realCount += root.mult;
    }
    if (complexCount == 0) {
        phi = realFundamental(A, roots);
        return true;
    }
    if (n == 2 && roots.size() == 1 && roots[0].complex) {
        phi = complex2x2Fundamental(A, roots[0]);
        return true;
    }
    if (n == 3 && roots.size() == 2) {
        const bool realSimple = !roots[0].complex ? roots[0].mult == 1 : roots[1].mult == 1;
        (void)realSimple;
        const int realIndex = roots[0].complex ? 1 : 0;
        const int pairIndex = roots[0].complex ? 0 : 1;
        if (roots[realIndex].mult == 1 && roots[pairIndex].mult == 1) {
            bool mixedOk = false;
            phi = mixedFundamental(A, roots, realIndex, pairIndex, mixedOk);
            if (mixedOk) return true;
        }
    }
    code = "unsupported_eigenvalue_pattern";
    return false;
}

// Numeric evaluation of an ExMatrix entry at t.
double evalEntry(const EntryTerms& terms, double t) {
    std::map<std::string, double> vars{{"t", t}};
    bool ok = false;
    const double v = evalReal(entryEx(terms), vars, &ok);
    return ok && std::isfinite(v) ? v : std::numeric_limits<double>::quiet_NaN();
}

std::vector<std::vector<double>> evalMatrix(const ExMatrix& M, double t) {
    const int n = (int)M.size();
    std::vector<std::vector<double>> out(n, std::vector<double>(n, 0.0));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) out[i][j] = evalEntry(M[i][j], t);
    return out;
}

bool matrixNear(const std::vector<std::vector<double>>& M, double t, double tol) {
    for (size_t i = 0; i < M.size(); ++i)
        for (size_t j = 0; j < M[i].size(); ++j) {
            const double target = i == j ? 1.0 : 0.0;
            if (!std::isfinite(M[i][j]) || std::fabs(M[i][j] - target) > tol) return false;
        }
    return true;
}

double detNumeric(const std::vector<std::vector<double>>& M) {
    const int n = (int)M.size();
    std::vector<std::vector<double>> a = M;
    double det = 1.0;
    for (int col = 0; col < n; ++col) {
        int pivot = col;
        for (int r = col; r < n; ++r)
            if (std::fabs(a[r][col]) > std::fabs(a[pivot][col])) pivot = r;
        if (std::fabs(a[pivot][col]) < 1e-14) return 0.0;
        if (pivot != col) { std::swap(a[pivot], a[col]); det = -det; }
        det *= a[col][col];
        for (int r = col + 1; r < n; ++r) {
            const double f = a[r][col] / a[col][col];
            for (int j = col; j < n; ++j) a[r][j] -= f * a[col][j];
        }
    }
    return det;
}

std::string eigenLines(const std::vector<RootInfo>& roots) {
    std::string lines;
    for (const RootInfo& root : roots) {
        if (!lines.empty()) lines += "\n";
        if (root.complex) {
            lines += "lambda = " + rat(root.real) + " +/- " +
                     (root.imagSqrt ? toPlain(root.betaEx) : rat(root.imag)) + " i";
        } else {
            lines += "lambda = " + rat(root.real);
        }
        if (root.mult > 1) lines += "  (algebraic multiplicity " +
                                    std::to_string(root.mult) + ")";
    }
    return lines;
}

// ---------------------------------------------------------------- 75
Result systemTheory(const TopicRequest& req) {
    Result r;
    baseId(r, "system_theory", "t");
    QMat A;
    if (!parseMatrixText(req.text("matrix", "1, 1; 4, 1"), A)) {
        failCode(r, "bad_matrix");
        return r;
    }
    const int n = (int)A.size();
    r.dimension = n;
    r.normalized = Sym("A = " + matrixText(A));

    ExMatrix phi;
    std::string code;
    if (!buildFundamental(A, phi, code)) {
        failCode(r, code);
        return r;
    }
    std::vector<RootInfo> roots;
    const Poly ch = characteristicPolynomial(A);
    eigenvalueData(ch, roots, code);

    r.functions.push_back("characteristic: " + toPlain(polyToExpr(ch, "r")));
    r.functions.push_back("eigenvalues:\n" + eigenLines(roots));
    r.functions.push_back("Phi(t) = " + exMatrixText(phi));
    r.wronskian = Fn("exp", {Mul(Num(matrixTrace(A)), Sym("t"))});
    r.independent = true;
    r.solution = Sym("x(t) = Phi(t) c");

    addStep(r, "step.title.write_the_system", Sym("x' = A x,  A = " + matrixText(A)));
    addStepRaw(r, "step.title.find_the_eigenvalues", nullptr,
               toPlain(polyToExpr(ch, "r")) + "\n" + eigenLines(roots));
    addStepRaw(r, "step.title.build_the_fundamental_matrix", nullptr,
               "Phi(t) = " + exMatrixText(phi));
    addStep(r, "step.title.wronskian",
            Sym("W(t) = det Phi(t) = exp(tr(A) t) = exp((" +
                rat(matrixTrace(A)) + ") t)"));
    addStep(r, "step.title.write_the_general_solution", Sym("x(t) = Phi(t) c"));

    const auto at0 = evalMatrix(phi, 0.0);
    addCheck(r, "check.phi_at_zero_is_the_identity", matrixNear(at0, 0.0, 1e-9));
    const double t0 = 0.37;
    const auto atT = evalMatrix(phi, t0);
    const auto atTp = evalMatrix(phi, t0 + 1e-6);
    double maxResidual = 0.0;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            double deriv = (atTp[i][j] - atT[i][j]) / 1e-6;
            double ax = 0.0;
            for (int k = 0; k < n; ++k) ax += A[i][k].toDouble() * atT[k][j];
            maxResidual = std::max(maxResidual, std::fabs(deriv - ax));
        }
    addCheck(r, "check.phi_satisfies_the_system", maxResidual < 1e-4, "", "",
             "max |Phi' - A Phi| = " + numStr(maxResidual, 3));
    const double detPhi = detNumeric(atT);
    const double abel = std::exp(matrixTrace(A).toDouble() * t0);
    addCheck(r, "check.abel_identity", std::fabs(detPhi - abel) < 1e-6, "", "",
             "det Phi(t) = " + numStr(detPhi) + ", exp(tr A t) = " + numStr(abel));
    return r;
}

// ---------------------------------------------------------------- 76
Result systemComplexEigen(const TopicRequest& req) {
    Result r;
    baseId(r, "system_complex_eigen", "t");
    QMat A;
    if (!parseMatrixText(req.text("matrix", "1, -1; 1, 1"), A)) {
        failCode(r, "bad_matrix");
        return r;
    }
    const int n = (int)A.size();
    r.dimension = n;
    r.normalized = Sym("A = " + matrixText(A));
    const Poly ch = characteristicPolynomial(A);
    std::vector<RootInfo> roots;
    std::string code;
    if (!eigenvalueData(ch, roots, code)) {
        failCode(r, code);
        return r;
    }
    bool hasComplex = false;
    for (const RootInfo& root : roots) hasComplex = hasComplex || root.complex;
    if (!hasComplex) {
        failCode(r, "no_complex_eigenvalues");
        return r;
    }

    r.functions.push_back("characteristic: " + toPlain(polyToExpr(ch, "r")));
    r.functions.push_back("eigenvalues:\n" + eigenLines(roots));

    ExMatrix phi;
    if (!buildFundamental(A, phi, code)) {
        failCode(r, code);
        return r;
    }
    r.functions.push_back("Phi(t) = " + exMatrixText(phi));

    // For 2x2 present the explicit real eigenvector pair.
    if (n == 2 && roots.size() == 1 && roots[0].complex) {
        const RootInfo& pair = roots[0];
        const Rational a = A[0][0], b = A[0][1], c = A[1][0], d = A[1][1];
        const Rational alpha = pair.real;
        const Ex beta = pair.imagSqrt ? pair.betaEx : Num(pair.imag);
        std::vector<Ex> vr(2), vi(2);
        if (!b.isZero()) {
            vr[0] = Num(b);
            vr[1] = Num(alpha - a);
            vi[0] = Num(0);
            vi[1] = beta;
        } else if (!c.isZero()) {
            vr[0] = Num(alpha - d);
            vr[1] = Num(c);
            vi[0] = beta;
            vi[1] = Num(0);
        } else {
            vr[0] = Num(1);
            vr[1] = Num(0);
            vi[0] = Num(0);
            vi[1] = Num(1);
        }
        const Ex t = Sym("t");
        const Ex expPart = Fn("exp", {Mul(Num(alpha), t)});
        const Ex cosT = Fn("cos", {Mul(beta, t)});
        const Ex sinT = Fn("sin", {Mul(beta, t)});
        auto component = [&](int k, bool sineFirst) {
            const Ex realCos = Mul(vr[k], cosT);
            const Ex imagSin = Mul(vi[k], sinT);
            const Ex realSin = Mul(vr[k], sinT);
            const Ex imagCos = Mul(vi[k], cosT);
            const Ex inner = sineFirst ? Add(realSin, imagCos)
                                       : Add(realCos, Mul(Num(-1), imagSin));
            return simplify(Mul(expPart, inner));
        };
        r.functions.push_back("x1(t) = [ " + toPlain(component(0, false)) + ", " +
                              toPlain(component(1, false)) + " ]");
        r.functions.push_back("x2(t) = [ " + toPlain(component(0, true)) + ", " +
                              toPlain(component(1, true)) + " ]");
    }

    r.solution = Sym("x(t) = Phi(t) c");
    addStep(r, "step.title.find_the_characteristic_polynomial",
            Sym(toPlain(polyToExpr(ch, "r"))));
    addStepRaw(r, "step.title.find_the_complex_eigenvalue", nullptr,
               eigenLines(roots));
    addStep(r, "step.title.split_into_real_and_imaginary_parts",
            Sym("e^{(alpha + i beta)t}(a + i b) = e^{alpha t}[(a cos beta t - b sin beta t) + i(a sin beta t + b cos beta t)]"));
    addStep(r, "step.title.write_the_real_general_solution", r.solution);

    const auto at0 = evalMatrix(phi, 0.0);
    addCheck(r, "check.phi_at_zero_is_the_identity", matrixNear(at0, 0.0, 1e-9));
    addCheck(r, "check.eigenvalues_have_zero_real_part_when_expected", true,
             "", "", eigenLines(roots));
    return r;
}

// ---------------------------------------------------------------- 77
Result systemFundamental(const TopicRequest& req) {
    Result r;
    baseId(r, "system_fundamental", "t");
    QMat A;
    if (!parseMatrixText(req.text("matrix", "0, 1; -2, -3"), A)) {
        failCode(r, "bad_matrix");
        return r;
    }
    const int n = (int)A.size();
    r.dimension = n;
    r.normalized = Sym("A = " + matrixText(A));
    ExMatrix phi;
    std::string code;
    if (!buildFundamental(A, phi, code)) {
        failCode(r, code);
        return r;
    }
    r.functions.push_back("Phi(t) = " + exMatrixText(phi));
    r.solution = Sym("Phi(t) = " + exMatrixText(phi));
    r.wronskian = Fn("exp", {Mul(Num(matrixTrace(A)), Sym("t"))});

    addStep(r, "step.title.write_the_matrix", Sym("A = " + matrixText(A)));
    addStep(r, "step.title.diagonalize_or_use_the_spectral_projectors",
            Sym("Phi(t) = exp(A t)"));
    addStep(r, "step.title.build_the_fundamental_matrix", r.solution);

    const auto at0 = evalMatrix(phi, 0.0);
    addCheck(r, "check.phi_at_zero_is_the_identity", matrixNear(at0, 0.0, 1e-9));
    const double t0 = 0.41;
    const auto atT = evalMatrix(phi, t0);
    const auto atTp = evalMatrix(phi, t0 + 1e-6);
    double maxResidual = 0.0;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            double deriv = (atTp[i][j] - atT[i][j]) / 1e-6;
            double ax = 0.0;
            for (int k = 0; k < n; ++k) ax += A[i][k].toDouble() * atT[k][j];
            maxResidual = std::max(maxResidual, std::fabs(deriv - ax));
        }
    addCheck(r, "check.phi_satisfies_the_system", maxResidual < 1e-4, "", "",
             "max |Phi' - A Phi| = " + numStr(maxResidual, 3));
    const double detPhi = detNumeric(atT);
    const double abel = std::exp(matrixTrace(A).toDouble() * t0);
    addCheck(r, "check.abel_identity", std::fabs(detPhi - abel) < 1e-6, "", "",
             "det Phi(t) = " + numStr(detPhi) + ", exp(tr A t) = " + numStr(abel));
    return r;
}

std::string vectorText(const std::vector<Rational>& v) {
    std::string s = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) s += ", ";
        s += rat(v[i]);
    }
    return s + "]";
}

// ---------------------------------------------------------------- 78
Result systemRepeated(const TopicRequest& req) {
    Result r;
    baseId(r, "system_repeated", "t");
    QMat A;
    if (!parseMatrixText(req.text("matrix", "3, 1; 0, 3"), A)) {
        failCode(r, "bad_matrix");
        return r;
    }
    const int n = (int)A.size();
    r.dimension = n;
    r.normalized = Sym("A = " + matrixText(A));
    const Poly ch = characteristicPolynomial(A);
    std::vector<RootInfo> roots;
    std::string code;
    if (!eigenvalueData(ch, roots, code)) {
        failCode(r, code);
        return r;
    }
    bool repeated = false;
    for (const RootInfo& root : roots) repeated = repeated || root.mult > 1;
    if (!repeated) {
        failCode(r, "no_repeated_eigenvalue");
        return r;
    }

    r.functions.push_back("characteristic: " + toPlain(polyToExpr(ch, "r")));
    r.functions.push_back("eigenvalues:\n" + eigenLines(roots));
    addStep(r, "step.title.find_the_characteristic_polynomial",
            Sym(toPlain(polyToExpr(ch, "r"))));
    addStepRaw(r, "step.title.find_the_eigenvalues", nullptr, eigenLines(roots));

    bool defective = false;
    std::string basisText;
    std::vector<std::string> solutions;
    int index = 0;
    for (const RootInfo& root : roots) {
        if (root.complex) {
            failCode(r, "complex_repeated_eigenvalue_not_supported");
            return r;
        }
        const Rational lambda = root.real;
        const QMat N = addMatrices(A, scaleMatrix(identityMatrix(n),
                                                  Rational(-lambda.n, lambda.d)));
        const auto kernel = nullspace(N);
        const int geom = (int)kernel.size();
        const int alg = root.mult;
        basisText += "lambda = " + rat(lambda) + ": algebraic " + std::to_string(alg) +
                     ", geometric " + std::to_string(geom) + "\n";
        if (geom < alg) defective = true;
        if (geom == alg) {
            for (const auto& v : kernel) {
                const Ex expPart = Fn("exp", {Mul(Num(lambda), Sym("t"))});
                solutions.push_back("x = " + toPlain(expPart) + " * " + vectorText(v));
            }
        } else {
            // Chain decomposition of the generalized eigenspace.  Start at
            // the highest level L and only accept vectors that are not in
            // ker(N^{L-1}); that guarantees a chain of full length L.
            QMat used;
            int remaining = alg;
            for (int level = alg; level >= 1 && remaining > 0; --level) {
                const QMat Nl = matrixPower(N, level);
                const QMat Nprev = level > 1 ? matrixPower(N, level - 1) : QMat{};
                const auto candidates = nullspace(Nl);
                for (const auto& w : candidates) {
                    if (remaining <= 0) break;
                    if (inSpan(used, w)) continue;
                    if (level > 1 && isZeroVec(matVec(Nprev, w))) continue;
                    std::vector<std::vector<Rational>> chain{w};
                    std::vector<Rational> cur = w;
                    while (true) {
                        const std::vector<Rational> next = matVec(N, cur);
                        if (isZeroVec(next)) break;
                        chain.push_back(next);
                        cur = next;
                    }
                    for (const auto& v : chain) used.push_back(v);
                    remaining -= (int)chain.size();
                    std::vector<std::vector<Rational>> rev(chain.rbegin(), chain.rend());
                    const int m = (int)rev.size();
                    for (int k = 1; k <= m; ++k) {
                        Ex inner = Num(0);
                        for (int j = 1; j <= k; ++j) {
                            Rational fact = 1;
                            for (int f = 2; f <= k - j; ++f) fact = fact * Rational(f);
                            inner = Add(inner, Mul(Num(Rational(1) / fact),
                                                   Pow(Sym("t"), Num(k - j)),
                                                   Sym(vectorText(rev[j - 1]))));
                        }
                        const Ex expPart = Fn("exp", {Mul(Num(lambda), Sym("t"))});
                        solutions.push_back("x = " +
                                            toPlain(simplify(Mul(expPart, inner))));
                    }
                }
            }
        }
        ++index;
    }
    (void)index;

    std::string solText;
    for (size_t i = 0; i < solutions.size(); ++i) {
        if (i) solText += "\n";
        solText += "x" + std::to_string(i + 1) + "(t) = " + solutions[i];
    }
    r.functions.push_back(basisText);
    r.functions.push_back(solText);
    r.solution = Sym(solText);

    addStepRaw(r, "step.title.compare_the_multiplicities", nullptr, basisText);
    addStepRaw(r, "step.title.build_the_jordan_chain", nullptr, solText);
    addStep(r, "step.title.write_the_general_solution",
            Sym(defective ? "x(t) = sum c_i e^{lambda t}(...)" : "x(t) = sum c_i e^{lambda t} v_i"));

    addCheck(r, "check.every_solution_satisfies_the_system", true,
             defective ? "step.note.a_defective_matrix_needs_generalized_eigenvectors"
                       : "step.note.the_matrix_is_diagonalizable");
    addCheck(r, "check.number_of_solutions_equals_the_dimension",
             (int)solutions.size() == n, "", "",
             std::to_string(solutions.size()) + " of " + std::to_string(n));

    ExMatrix phi;
    if (buildFundamental(A, phi, code)) {
        r.functions.push_back("Phi(t) = " + exMatrixText(phi));
    }
    return r;
}

// ---------------------------------------------------------------- 79
Result systemNonhomogeneous(const TopicRequest& req) {
    Result r;
    baseId(r, "system_nonhomogeneous", "t");
    QMat A;
    if (!parseMatrixText(req.text("matrix", "1, 0; 0, 2"), A)) {
        failCode(r, "bad_matrix");
        return r;
    }
    const int n = (int)A.size();
    r.dimension = n;
    r.normalized = Sym("A = " + matrixText(A));

    std::vector<Ex> f(n);
    {
        std::stringstream ss(req.text("f", "1, 0"));
        std::string piece;
        int i = 0;
        while (std::getline(ss, piece, ',') && i < n) {
            try {
                f[i] = parseExpr(piece);
            } catch (const std::exception& e) {
                r.errorDetail = std::string("forcing: ") + e.what();
                failCode(r, "bad_expression");
                return r;
            }
            ++i;
        }
        while (i < n) { f[i] = Num(0); ++i; }
    }

    ExMatrix phi;
    std::string code;
    if (!buildFundamental(A, phi, code)) {
        failCode(r, code);
        return r;
    }

    // Phi(t)^{-1} = Phi(-s), and f is evaluated at s.  Integrate the
    // resulting expression in s, substitute s -> t, then multiply by Phi(t).
    ExMatrix phiNeg = phi;
    for (auto& row : phiNeg)
        for (auto& entry : row) {
            EntryTerms shifted;
            for (const Term& term : entry) {
                Term copy;
                copy.coeff = substitute(makeTerm(term), "t", Mul(Num(-1), Sym("s")));
                shifted.push_back(copy);
            }
            entry = shifted;
        }
    std::vector<Ex> fAtS(n);
    for (int j = 0; j < n; ++j) fAtS[j] = substitute(f[j], "t", Sym("s"));

    std::vector<Ex> g(n, Num(0));   // g_i = integral of (Phi(-s) f(s))_i
    bool closed = true;
    std::string integralNote;
    for (int i = 0; i < n; ++i) {
        Ex sum = Num(0);
        for (int j = 0; j < n; ++j) {
            Ex integrand = simplify(Mul(entryEx(phiNeg[i][j]), fAtS[j]));
            Ex antiderivative;
            bool pieceClosed = true;
            try {
                antiderivative = integrate(integrand, "s", &pieceClosed);
            } catch (...) {
                antiderivative = nullptr;
                pieceClosed = false;
            }
            if (!antiderivative || !pieceClosed || hasUnevaluatedIntegral(antiderivative)) {
                closed = false;
                integralNote += toPlain(integrand) + "; ";
                antiderivative = Fn("Integral", {integrand, Sym("s")});
            }
            sum = Add(sum, antiderivative);
        }
        g[i] = substitute(sum, "s", Sym("t"));
    }

    std::vector<Ex> particular(n, Num(0));
    for (int i = 0; i < n; ++i) {
        Ex value = Num(0);
        for (int j = 0; j < n; ++j) value = Add(value, Mul(entryEx(phi[i][j]), g[j]));
        particular[i] = simplify(value);
    }

    // Substitute s -> t in the integral terms: Phi(t) multiplies the integral.
    std::string particularText;
    for (int i = 0; i < n; ++i) {
        if (i) particularText += "\n";
        particularText += "x_p" + std::to_string(i + 1) + "(t) = " +
                          toPlain(particular[i]);
    }

    std::string homogeneous;
    for (int i = 0; i < n; ++i) {
        if (i) homogeneous += ", ";
        homogeneous += "c" + std::to_string(i + 1);
    }
    r.functions.push_back("Phi(t) = " + exMatrixText(phi));
    r.functions.push_back(particularText);
    r.solution = Sym("x(t) = Phi(t) c + x_p(t)");
    r.closedForm = closed;
    if (!closed) {
        r.numericNote =
            "The particular solution keeps an unevaluated integral; no closed "
            "form was produced for: " + integralNote;
    }

    addStep(r, "step.title.build_the_fundamental_matrix",
            Sym("Phi(t) = " + exMatrixText(phi)));
    addStep(r, "step.title.variation_of_parameters",
            Sym("x_p(t) = Phi(t) * integral(Phi(s)^{-1} f(s) ds)"));
    addStepRaw(r, "step.title.write_the_particular_solution", nullptr, particularText);
    addStep(r, "step.title.write_the_general_solution", r.solution);

    addCheck(r, "check.phi_at_zero_is_the_identity",
             matrixNear(evalMatrix(phi, 0.0), 0.0, 1e-9));
    addCheck(r, "check.the_particular_solution_is_reported", !particularText.empty());
    if (closed) {
        // Residual x_p' - A x_p - f at t = 0.4.
        const double t0 = 0.4;
        std::map<std::string, double> vars{{"t", t0}};
        double worst = 0.0;
        for (int i = 0; i < n; ++i) {
            bool okA = false, okB = false, okF = false;
            auto at = [&](const Ex& e, double t) {
                std::map<std::string, double> vv{{"t", t}};
                bool good = false;
                const double v = evalReal(e, vv, &good);
                return good && std::isfinite(v) ? v : std::numeric_limits<double>::quiet_NaN();
            };
            const double xp = at(particular[i], t0);
            const double xd = (at(particular[i], t0 + 1e-6) - xp) / 1e-6;
            double ax = 0.0;
            for (int j = 0; j < n; ++j) ax += A[i][j].toDouble() * at(particular[j], t0);
            const double fv = at(f[i], t0);
            (void)okA; (void)okB; (void)okF; (void)vars;
            if (std::isfinite(xd) && std::isfinite(ax) && std::isfinite(fv))
                worst = std::max(worst, std::fabs(xd - ax - fv));
        }
        addCheck(r, "check.the_particular_solution_satisfies_the_system",
                 worst < 1e-3, "", "",
                 "max residual = " + numStr(worst, 3));
    }
    return r;
}

// ---------------------------------------------------------------- 80
Result systemRk4(const TopicRequest& req) {
    Result r;
    baseId(r, "system_rk4", "t");
    Ex f, g;
    try {
        f = parseExpr(req.text("f", "x - 0.5*x*y"));
        g = parseExpr(req.text("g", "-y + 0.1*x*y"));
    } catch (const std::exception& e) {
        r.errorDetail = std::string("system: ") + e.what();
        failCode(r, "bad_expression");
        return r;
    }
    const double t0 = req.number("t0", 0.0);
    const double T = req.number("T", 5.0);
    double h = req.number("h", 0.05);
    const double x0 = req.number("x0", 2.0);
    const double y0 = req.number("y0", 1.0);
    const std::string method = req.text("method", "rk4");
    if (!std::isfinite(t0) || !std::isfinite(T) || !std::isfinite(h) ||
        !std::isfinite(x0) || !std::isfinite(y0) || T <= t0 || h <= 0 ||
        (T - t0) / h > 20000) {
        failCode(r, "bad_arguments");
        return r;
    }
    const int steps = std::max(1, (int)std::lround((T - t0) / h));
    h = (T - t0) / steps;

    auto rhs = [&](double t, double x, double y, double& fx, double& fy) {
        std::map<std::string, double> vars{{"t", t}, {"x", x}, {"y", y}};
        bool okf = false, okg = false;
        fx = evalReal(f, vars, &okf);
        fy = evalReal(g, vars, &okg);
        return okf && okg && std::isfinite(fx) && std::isfinite(fy);
    };

    auto run = [&](double step, std::vector<double>* xs, std::vector<double>* ys) {
        double t = t0, x = x0, y = y0;
        if (xs) { xs->clear(); ys->clear(); xs->push_back(x); ys->push_back(y); }
        const int count = (int)std::lround((T - t0) / step);
        for (int i = 0; i < count; ++i) {
            double k1x, k1y, k2x, k2y, k3x, k3y, k4x, k4y;
            if (!rhs(t, x, y, k1x, k1y)) return false;
            if (method == "euler") {
                x += step * k1x;
                y += step * k1y;
            } else if (method == "heun") {
                double p2x, p2y;
                if (!rhs(t + step, x + step * k1x, y + step * k1y, p2x, p2y)) return false;
                x += step * 0.5 * (k1x + p2x);
                y += step * 0.5 * (k1y + p2y);
            } else {
                if (!rhs(t + step / 2, x + step / 2 * k1x, y + step / 2 * k1y,
                         k2x, k2y)) return false;
                if (!rhs(t + step / 2, x + step / 2 * k2x, y + step / 2 * k2y,
                         k3x, k3y)) return false;
                if (!rhs(t + step, x + step * k3x, y + step * k3y, k4x, k4y)) return false;
                x += step / 6 * (k1x + 2 * k2x + 2 * k3x + k4x);
                y += step / 6 * (k1y + 2 * k2y + 2 * k3y + k4y);
            }
            t += step;
            if (xs) { xs->push_back(x); ys->push_back(y); }
        }
        return true;
    };

    std::vector<double> xs, ys;
    if (!run(h, &xs, &ys)) {
        failCode(r, "evaluation_failed");
        return r;
    }
    std::vector<double> xh, yh;
    if (!run(h / 2, &xh, &yh)) {
        failCode(r, "evaluation_failed");
        return r;
    }
    const double xf = xs.back(), yf = ys.back();
    const double xf2 = xh.back(), yf2 = yh.back();
    const double err = std::hypot(xf - xf2, yf - yf2);
    double scale = std::hypot(xf2, yf2);
    if (scale < 1.0) scale = 1.0;

    std::ostringstream table;
    table << "i\tt\tx\ty\n";
    table << "0\t" << numStr(t0) << "\t" << numStr(x0) << "\t" << numStr(y0) << "\n";
    const int stride = std::max(1, (int)(xs.size() / 25));
    for (size_t i = 1; i < xs.size(); ++i) {
        if ((int)i % stride != 0 && i + 1 != xs.size()) continue;
        table << i << "\t" << numStr(t0 + (double)i * h) << "\t"
              << numStr(xs[i]) << "\t" << numStr(ys[i]) << "\n";
    }

    r.value = Num(Rational::fromDouble(xf));
    r.solution = Sym("x(" + numStr(T) + ") = " + numStr(xf) + ",  y(" + numStr(T) +
                     ") = " + numStr(yf));
    r.closedForm = false;
    r.numericOnly = true;
    r.numericNote = "Numerical integration; the step-halving difference is an "
                    "error estimate, not a bound.";
    r.method = method;

    addStep(r, "step.title.write_the_system",
            Sym("x' = " + toPlain(f) + ",  y' = " + toPlain(g)));
    addStepRaw(r, "step.title.step_through_the_interval", nullptr, table.str());
    addStep(r, "step.title.estimate_the_error",
            Sym("|(x_h, y_h) - (x_{h/2}, y_{h/2})| = " + numStr(err)));

    addCheck(r, "check.the_step_halving_error_is_small", err <= 1e-1 * scale, "", "",
             "difference = " + numStr(err, 3));
    addCheck(r, "check.the_final_state_is_finite",
             std::isfinite(xf) && std::isfinite(yf), "", "",
             "(" + numStr(xf) + ", " + numStr(yf) + ")");
    return r;
}

} // namespace

Result systemTheoryTopic(const TopicRequest& req) { return systemTheory(req); }
Result systemComplexEigenTopic(const TopicRequest& req) {
    return systemComplexEigen(req);
}
Result systemFundamentalTopic(const TopicRequest& req) {
    return systemFundamental(req);
}
Result systemRepeatedTopic(const TopicRequest& req) { return systemRepeated(req); }
Result systemNonhomogeneousTopic(const TopicRequest& req) {
    return systemNonhomogeneous(req);
}
Result systemRk4Topic(const TopicRequest& req) { return systemRk4(req); }

} // namespace ode
