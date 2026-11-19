// Infinit Lab — polynomials, factoring and exact rational integration
// SPDX-License-Identifier: Apache-2.0
#include "poly.hpp"
#include <functional>
#include <set>

namespace ode {

Poly polyAdd(const Poly& a, const Poly& b) {
    Poly r;
    r.c.resize(std::max(a.c.size(), b.c.size()), Rational(0));
    for (size_t i = 0; i < a.c.size(); ++i) r.c[i] = r.c[i] + a.c[i];
    for (size_t i = 0; i < b.c.size(); ++i) r.c[i] = r.c[i] + b.c[i];
    r.trim();
    return r;
}
Poly polySub(const Poly& a, const Poly& b) {
    Poly nb = b;
    for (auto& x : nb.c) x = Rational(-x.n, x.d);
    return polyAdd(a, nb);
}
Poly polyMul(const Poly& a, const Poly& b) {
    if (a.isZero() || b.isZero()) return Poly{};
    Poly r;
    r.c.assign(a.c.size() + b.c.size() - 1, Rational(0));
    for (size_t i = 0; i < a.c.size(); ++i)
        for (size_t j = 0; j < b.c.size(); ++j)
            r.c[i + j] = r.c[i + j] + a.c[i] * b.c[j];
    r.trim();
    return r;
}
Poly polyScale(const Poly& a, Rational k) {
    Poly r = a;
    for (auto& x : r.c) x = x * k;
    r.trim();
    return r;
}
void polyDivMod(const Poly& a, const Poly& b, Poly& q, Poly& r) {
    if (b.isZero()) throw std::runtime_error("polynomial division by zero");
    q = Poly{};
    r = a;
    int db = b.deg();
    while (!r.isZero() && r.deg() >= db) {
        int shift = r.deg() - db;
        Rational factor = r.lead() / b.lead();
        if (q.c.size() < (size_t)shift + 1) q.c.resize(shift + 1, Rational(0));
        q.c[shift] = factor;
        Poly t;
        t.c.assign(shift + 1, Rational(0));
        t.c[shift] = factor;
        r = polySub(r, polyMul(b, t));
    }
    q.trim(); r.trim();
}
Poly polyGcd(Poly a, Poly b) {
    a.trim(); b.trim();
    while (!b.isZero()) {
        Poly q, r;
        polyDivMod(a, b, q, r);
        a = b;
        b = r;
    }
    if (a.isZero()) return a;
    return a.monic();
}
Poly polyPow(const Poly& a, int n) {
    Poly r;
    r.c = {Rational(1)};
    for (int i = 0; i < n; ++i) r = polyMul(r, a);
    return r;
}

bool toPoly(const Ex& e, const std::string& x, Poly& out) {
    std::vector<Ex> coeffs;
    if (!polyCoeffs(e, x, coeffs)) return false;
    out.c.clear();
    out.c.reserve(coeffs.size());
    for (auto& c : coeffs) {
        if (!c->isNum()) return false;
        out.c.push_back(c->num);
    }
    out.trim();
    return true;
}

Ex polyToExpr(const Poly& p, const std::string& x) {
    if (p.isZero()) return Num(0);
    std::vector<Ex> terms;
    for (int i = 0; i < (int)p.c.size(); ++i) {
        if (p.c[i].isZero()) continue;
        Ex t = Num(p.c[i]);
        if (i == 1) t = Mul(t, Sym(x));
        else if (i > 1) t = Mul(t, Pow(Sym(x), Num(i)));
        terms.push_back(t);
    }
    return Add(terms);
}

Ex integratePolynomial(const Poly& p, const std::string& x) {
    std::vector<Ex> terms;
    for (int i = 0; i < (int)p.c.size(); ++i) {
        if (p.c[i].isZero()) continue;
        Rational c = p.c[i] / Rational(i + 1);
        Ex t;
        if (i + 1 == 1) t = Mul(Num(c), Sym(x));
        else t = Mul(Num(c), Pow(Sym(x), Num(i + 1)));
        terms.push_back(t);
    }
    return Add(terms);
}

// ---------------------------------------------------------------- factoring
static std::vector<long long> divisorsLL(long long n) {
    std::vector<long long> d;
    if (n < 0) n = -n;
    if (n == 0) return {0};
    for (long long i = 1; i * i <= n && i <= 2000000; ++i) {
        if (n % i == 0) {
            d.push_back(i);
            if (i * i != n) d.push_back(n / i);
        }
    }
    std::sort(d.begin(), d.end());
    return d;
}

static bool findRationalRoot(const Poly& p0, Rational& root) {
    Poly p = p0;
    p.trim();
    if (p.deg() < 1) return false;
    long long L = p.contentDen();
    std::vector<long long> a;
    for (auto& x : p.c) a.push_back(x.n * (L / x.d));
    if (a[0] == 0) { root = Rational(0); return true; }
    long long a0 = std::llabs(a[0]), an = std::llabs(a.back());
    auto nums = divisorsLL(a0);
    auto dens = divisorsLL(an);
    for (long long num : nums) {
        if (num == 0) continue;
        for (long long den : dens) {
            for (int s = -1; s <= 1; s += 2) {
                Rational r(s * num, den);
                if (p.eval(r).isZero()) { root = r; return true; }
            }
        }
    }
    return false;
}

static bool factorMonic(const Poly& p0, std::vector<Factor>& facs, int depth = 0) {
    if (depth > 20) return false;
    Poly p = p0.monic();
    if (p.deg() <= 0) return true;
    Rational r;
    if (findRationalRoot(p, r)) {
        Poly f;
        f.c = {Rational(-r.n, r.d), Rational(1)};
        int m = 0;
        while (p.deg() >= 1) {
            Poly q, rem;
            polyDivMod(p, f, q, rem);
            if (!rem.isZero()) break;
            p = q;
            ++m;
            if (m > 20) break;
        }
        facs.push_back({f, m});
        return factorMonic(p, facs, depth + 1);
    }
    if (p.deg() == 1) { facs.push_back({p, 1}); return true; }
    if (p.deg() == 2) { facs.push_back({p, 1}); return true; }
    // try splitting with a monic quadratic x^2 + b x + c
    if (p.deg() >= 3 && p.deg() <= 8) {
        long long L = p.contentDen();
        std::vector<long long> a;
        for (auto& x : p.c) a.push_back(x.n * (L / x.d));
        if (p.c.size() > 0) {
            long long c0 = a[0];
            for (long long c : divisorsLL(c0)) {
                if (c == 0) continue;
                for (int s : {1, -1}) {
                    long long cc = s * c;
                    for (long long b = -40; b <= 40; ++b) {
                        Poly q;
                        q.c = {Rational(cc), Rational(b), Rational(1)};
                        Poly quot, rem;
                        polyDivMod(p, q, quot, rem);
                        if (rem.isZero() && !quot.isZero()) {
                            facs.push_back({q, 1});
                            return factorMonic(quot, facs, depth + 1);
                        }
                    }
                }
            }
        }
    }
    return false;
}

bool factorPolynomial(const Poly& p, std::vector<Factor>& out) {
    out.clear();
    if (p.deg() < 1) return true;
    return factorMonic(p.monic(), out);
}

static bool samePoly(const Poly& a, const Poly& b) {
    if (a.c.size() != b.c.size()) return false;
    for (size_t i = 0; i < a.c.size(); ++i)
        if (!(a.c[i] == b.c[i])) return false;
    return true;
}

// The recursive splitter can return the same irreducible factor twice (for
// example (s^2+1)^2 splits into two copies of s^2+1).  Partial fractions need
// one entry with the combined multiplicity.
static void mergeFactors(std::vector<Factor>& facs) {
    std::vector<Factor> out;
    for (const auto& f : facs) {
        bool merged = false;
        for (auto& g : out) {
            if (samePoly(f.p, g.p)) {
                g.mult += f.mult;
                merged = true;
                break;
            }
        }
        if (!merged) out.push_back(f);
    }
    facs = out;
}

// ---------------------------------------------------------------- linear solve over Q
static bool solveRationalSystem(std::vector<std::vector<Rational>> A, std::vector<Rational> b,
                                std::vector<Rational>& x) {
    int n = (int)b.size();
    if (n == 0) { x.clear(); return true; }
    int m = (int)A[0].size();
    for (int col = 0, row = 0; col < m && row < n; ++col) {
        int piv = -1;
        for (int i = row; i < n; ++i) if (!A[i][col].isZero()) { piv = i; break; }
        if (piv < 0) continue;
        std::swap(A[piv], A[row]); std::swap(b[piv], b[row]);
        Rational pv = A[row][col];
        for (int j = col; j < m; ++j) A[row][j] = A[row][j] / pv;
        b[row] = b[row] / pv;
        for (int i = 0; i < n; ++i) {
            if (i == row || A[i][col].isZero()) continue;
            Rational f = A[i][col];
            for (int j = col; j < m; ++j) A[i][j] = A[i][j] - f * A[row][j];
            b[i] = b[i] - f * b[row];
        }
        ++row;
    }
    x.assign(m, Rational(0));
    // consistency: zero row with nonzero rhs
    for (int i = 0; i < n; ++i) {
        bool allZero = true;
        for (int j = 0; j < m; ++j) if (!A[i][j].isZero()) { allZero = false; break; }
        if (allZero && !b[i].isZero()) return false;
    }
    // read pivots
    for (int i = (int)std::min(n, m) - 1; i >= 0; --i) {
        int piv = -1;
        for (int j = 0; j < m; ++j) if (!A[i][j].isZero()) { piv = j; break; }
        if (piv < 0) continue;
        Rational v = b[i];
        for (int j = piv + 1; j < m; ++j) v = v - A[i][j] * x[j];
        x[piv] = v;
    }
    return true;
}

static Ex lnAbs(const Ex& u) { return Fn("ln", {Fn("abs", {u})}); }

static Ex sqrtOfRational(Rational q) {
    return Sqrt(Num(q));
}

// integrate (A x + B)/Q^k where Q = x^2 + p x + q
static Ex integrateQuadraticPower(Rational A, Rational B, Rational p, Rational q, int k,
                                  const std::string& x) {
    Rational half(1, 2);
    Rational C = B - A * p * half;
    Ex X = Sym(x);
    Ex Q = Add({Pow(X, Num(2)), Mul(Num(p), X), Num(q)});
    Ex out = Num(0);
    if (!A.isZero()) {
        // A/2 * (2x+p)/Q^k
        if (k == 1) out = Add(out, Mul(Num(A * half), lnAbs(Q)));
        else out = Add(out, Mul(Num(A * half * Rational(1, 1 - k)), Pow(Q, Num(1 - k))));
    }
    Rational Delta = q * Rational(4) - p * p;
    if (C.isZero()) return out;
    if (Delta.isZero()) {
        // Q = (x+p/2)^2
        Ex U = Add(X, Num(p * half));
        int e = 2 * k;
        out = Add(out, Mul(Num(C * Rational(2, -1)), Pow(U, Num(1 - e))));
        return out;
    }
    if (k == 1) {
        if (!Delta.isNeg()) {
            Ex s = sqrtOfRational(Delta);
            Ex arg = Mul(Add(Mul(Num(2), X), Num(p)), Pow(s, Num(-1)));
            out = Add(out, Mul(Num(C * Rational(2)), Pow(s, Num(-1)), Fn("atan", {arg})));
        } else {
            Ex s = sqrtOfRational(-Delta);
            Ex num = Add(Add(Mul(Num(2), X), Num(p)), Mul(Num(-1), s));
            Ex den = Add(Add(Mul(Num(2), X), Num(p)), s);
            out = Add(out, Mul(Num(C), Pow(s, Num(-1)), lnAbs(Mul(num, Pow(den, Num(-1))))));
        }
        return out;
    }
    // recurrence for k>1
    // ∫ dx/Q^k = (2x+p)/((k-1)Δ Q^{k-1}) + (2k-3)/((k-1)Δ) ∫ dx/Q^{k-1}
    Ex term = Mul(Add(Mul(Num(2), X), Num(p)),
                  Num(Rational(1, (k - 1))), Pow(Num(Delta), Num(-1)), Pow(Q, Num(1 - k)));
    Ex prev = integrateQuadraticPower(Rational(0), Rational(1), p, q, k - 1, x);
    term = Add(term, Mul(Num(Rational(2 * k - 3, k - 1)), Pow(Num(Delta), Num(-1)), prev));
    out = Add(out, Mul(Num(C), term));
    return out;
}

Ex integrateRational(const Ex& numE, const Ex& denE, const std::string& x) {    Poly N, D;
    if (!toPoly(numE, x, N) || !toPoly(denE, x, D)) return nullptr;
    if (D.isZero()) return nullptr;
    if (D.deg() == 0) {
        Rational k = D.c[0];
        return integratePolynomial(polyScale(N, k.inv()), x);
    }
    // polynomial part
    Poly Q, R;
    polyDivMod(N, D, Q, R);
    Ex out = integratePolynomial(Q, x);
    if (R.isZero()) return out;

    // factor denominator
    Rational K = D.lead();
    Poly Dm = D.monic();
    std::vector<Factor> facs;
    if (!factorMonic(Dm, facs)) return nullptr;
    mergeFactors(facs);
    // verify product
    Poly prod;
    prod.c = {Rational(1)};
    for (auto& f : facs) prod = polyMul(prod, polyPow(f.p, f.mult));
    if (polyGcd(prod, Dm).deg() != Dm.deg()) return nullptr;

    // unknowns: for each factor & power, a polynomial of degree < deg(f)
    struct Term { int fi; int k; int off; };
    std::vector<Term> terms;
    int nUnknown = 0;
    for (int i = 0; i < (int)facs.size(); ++i)
        for (int k = 1; k <= facs[i].mult; ++k) {
            terms.push_back({i, k, nUnknown});
            nUnknown += facs[i].p.deg();
        }
    int rows = Dm.deg();
    std::vector<std::vector<Rational>> A(rows, std::vector<Rational>(nUnknown, Rational(0)));
    std::vector<Rational> rhs(rows, Rational(0));
    for (int i = 0; i < R.deg() + 1 && i < rows + 1; ++i)
        if (i < (int)R.c.size() && i < rows) rhs[i] = R.c[i] / K;
    // build multipliers
    for (auto& tm : terms) {
        Poly F = facs[tm.fi].p;
        Poly M;
        M.c = {Rational(1)};
        M = polyPow(F, facs[tm.fi].mult - tm.k);
        for (int j = 0; j < (int)facs.size(); ++j) {
            if (j == tm.fi) continue;
            M = polyMul(M, polyPow(facs[j].p, facs[j].mult));
        }
        for (int t = 0; t < F.deg(); ++t) {
            // x^t * M
            Poly shift = M;
            for (int s = 0; s < t; ++s) shift.c.insert(shift.c.begin(), Rational(0));
            for (int d = 0; d <= shift.deg() && d < rows; ++d)
                A[d][tm.off + t] = A[d][tm.off + t] + shift.c[d];
        }
    }
    std::vector<Rational> sol;
    if (!solveRationalSystem(A, rhs, sol)) return nullptr;

    // integrate each partial fraction
    for (auto& tm : terms) {
        const Factor& f = facs[tm.fi];
        if (f.p.deg() == 1) {
            Rational a = sol[tm.off];
            Rational r = -f.p.c[0] / f.p.c[1];
            Ex X = Sym(x);
            if (a.isZero()) continue;
            if (tm.k == 1) out = Add(out, Mul(Num(a), lnAbs(Add(X, Num(-r)))));
            else out = Add(out, Mul(Num(a * Rational(1, 1 - tm.k)),
                                    Pow(Add(X, Num(-r)), Num(1 - tm.k))));
        } else {
            // U(x) = b + a*x  (column order is constant, then x)
            Rational b = sol[tm.off];
            Rational a = (tm.off + 1 < (int)sol.size()) ? sol[tm.off + 1] : Rational(0);
            out = Add(out, integrateQuadraticPower(a, b, f.p.c[1], f.p.c[0], tm.k, x));
        }
    }
    return simplify(out);
}

// Inverse Laplace of a proper rational function by partial fractions.
//
// The denominator is factored exactly over Q with the same machinery used by
// integrateRational.  A linear factor (s - r)^k contributes
//     A t^(k-1)/(k-1)! e^(r t),
// and an irreducible quadratic (s-a)^2 + b^2 contributes
//     e^(a t) (A cos(bt) + (B + A a) sin(bt)/b).
// Repeated quadratic factors need convolution, so they are reported as
// unsupported rather than guessed.
Ex inverseLaplaceRational(const Ex& numE, const Ex& denE, const std::string& s,
                          const std::string& t) {
    Poly N, D;
    if (!toPoly(numE, s, N) || !toPoly(denE, s, D)) return nullptr;
    if (D.isZero() || D.deg() <= 0) return nullptr;
    Poly Q, R;
    polyDivMod(N, D, Q, R);
    if (!Q.isZero()) return nullptr;  // polynomial part means impulses at t = 0
    if (R.isZero()) return Num(0);

    const Rational K = D.lead();
    Poly Dm = D.monic();
    std::vector<Factor> facs;
    if (!factorMonic(Dm, facs)) return nullptr;
    mergeFactors(facs);
    Poly prod;
    prod.c = {Rational(1)};
    for (auto& f : facs) prod = polyMul(prod, polyPow(f.p, f.mult));
    if (polyGcd(prod, Dm).deg() != Dm.deg()) return nullptr;

    struct Term { int fi; int k; int off; };
    std::vector<Term> terms;
    int nUnknown = 0;
    for (int i = 0; i < (int)facs.size(); ++i)
        for (int k = 1; k <= facs[i].mult; ++k) {
            terms.push_back({i, k, nUnknown});
            nUnknown += facs[i].p.deg();
        }
    const int rows = Dm.deg();
    std::vector<std::vector<Rational>> A(rows, std::vector<Rational>(nUnknown, Rational(0)));
    std::vector<Rational> rhs(rows, Rational(0));
    for (int i = 0; i <= R.deg() && i < rows; ++i)
        if (i < (int)R.c.size()) rhs[i] = R.c[i] / K;
    for (auto& tm : terms) {
        const Poly& F = facs[tm.fi].p;
        Poly M = polyPow(F, facs[tm.fi].mult - tm.k);
        for (int j = 0; j < (int)facs.size(); ++j) {
            if (j == tm.fi) continue;
            M = polyMul(M, polyPow(facs[j].p, facs[j].mult));
        }
        for (int p = 0; p < F.deg(); ++p) {
            Poly shift = M;
            for (int k = 0; k < p; ++k) shift.c.insert(shift.c.begin(), Rational(0));
            for (int d = 0; d <= shift.deg() && d < rows; ++d)
                A[d][tm.off + p] = A[d][tm.off + p] + shift.c[d];
        }
    }
    std::vector<Rational> sol;
    if (!solveRationalSystem(A, rhs, sol)) return nullptr;

    Ex out = Num(0);
    Ex T = Sym(t);
    for (auto& tm : terms) {
        const Factor& f = facs[tm.fi];
        if (f.p.deg() == 1) {
            const Rational a = sol[tm.off];
            if (a.isZero()) continue;
            const Rational r = -f.p.c[0] / f.p.c[1];
            long long fact = 1;
            for (int i = 2; i < tm.k; ++i) fact *= i;
            Ex tPow = tm.k == 1 ? Num(1)
                                : Mul(Pow(T, Num(tm.k - 1)), Num(Rational(1, fact)));
            out = Add(out, Mul(Num(a), tPow,
                               Fn("exp", {Mul(Num(r), T)})));
        } else if (f.p.deg() == 2) {
            const Rational Acoef = sol[tm.off + 1];
            const Rational Bcoef = sol[tm.off];
            const Rational B = f.p.c[1];
            const Rational C = f.p.c[0];
            const Rational alphaR = Rational(-B.n, B.d) / Rational(2);
            const Rational beta2R = C - alphaR * alphaR;
            if (beta2R.n == 0) return nullptr;
            const Ex alpha = Num(alphaR);
            const Ex env = Fn("exp", {Mul(alpha, T)});
            Ex I, J;
            if (beta2R.n * beta2R.d > 0) {
                const Ex beta = Sqrt(Num(beta2R));
                I = Mul(env, Fn("sin", {Mul(beta, T)}), Pow(beta, Num(-1)));
                J = Mul(env, Fn("cos", {Mul(beta, T)}));
            } else {
                // Real irrational roots: (s-a)^2 - g^2.  The same recurrence
                // holds by analytic continuation, with sinh/cosh in place of
                // sin/cos.
                const Rational gamma2R = -beta2R;
                const Ex gamma = Sqrt(Num(gamma2R));
                I = Mul(env, Fn("sinh", {Mul(gamma, T)}), Pow(gamma, Num(-1)));
                J = Mul(env, Fn("cosh", {Mul(gamma, T)}));
            }
            // I_1 = e^(at) sin(bt)/b and J_1 = e^(at) cos(bt), where
            // J_k = L^-1{(s-a)/Q^k}.  The recurrences follow from the
            // derivative rule L{t f(t)} = -F'(s):
            //   J_k = t I_(k-1)/(2(k-1))
            //   I_k = (-t J_(k-1) + (2k-3) I_(k-1))/(2(k-1)b^2).
            for (int k = 2; k <= tm.k; ++k) {
                Ex Jnext = Mul(T, I, Num(Rational(1, 2 * (k - 1))));
                const Rational scale = Rational(1, 2 * (k - 1)) / beta2R;
                Ex Inext = Mul(Add(Mul(Num(-1), T, J), Mul(Num(2 * k - 3), I)),
                               Num(scale));
                I = Inext;
                J = Jnext;
            }
            const Rational numUR = Acoef;
            const Rational numCR = Bcoef + Acoef * alphaR;
            out = Add(out, simplify(Add(Mul(Num(numUR), J), Mul(Num(numCR), I))));
        } else {
            return nullptr;
        }
    }
    return simplify(out);
}

} // namespace ode
