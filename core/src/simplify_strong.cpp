// Infinit Lab — stronger algebraic simplification.
// SPDX-License-Identifier: Apache-2.0
//
// The canonical builder already folds like terms and cancels identical
// factors, but it cannot see through an expansion: (x²−1)/(x−1) stays as it is
// because the numerator never becomes (x+1)(x−1).  This pass adds the missing
// work: quotient laws, a polynomial normal form, a real polynomial GCD, and a
// final presentation pass that expands polynomials into descending degree, so
// rational expressions reach their shortest form instead of stopping one or two
// steps early.
#include "expr.hpp"
#include "poly.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <vector>

namespace ode {
namespace {

Rational ratGcd(Rational a, Rational b) {
    a = a.abs(); b = b.abs();
    if (a.isZero()) return b;
    if (b.isZero()) return a;
    long long n = std::gcd(a.n, b.n);
    long long d = std::lcm(a.d, b.d);
    return Rational(n, d);
}

size_t nodeCount(const Ex& a) {
    if (!a) return 0;
    size_t s = 1;
    for (auto& x : a->args) s += nodeCount(x);
    return s;
}

int depthOf(const Ex& a) {
    if (!a || a->args.empty()) return 0;
    int d = 0;
    for (auto& x : a->args) d = std::max(d, depthOf(x));
    return d + 1;
}

// Two forms with the same number of nodes are not equally readable: a nested
// fraction such as 1/(1/x + 1) hides a fraction inside a fraction, while
// x/(x+1) shows it once.  Penalising fractions that sit inside another
// fraction picks the flat form.
size_t fractionDepth(const Ex& a, int level) {
    if (!a) return 0;
    bool frac = a->isPow() && a->args[1]->isNum() && a->args[1]->num.isNeg();
    size_t s = (frac && level >= 1) ? 1 : 0;
    int next = level + (frac ? 1 : 0);
    for (auto& c : a->args) s += fractionDepth(c, next);
    return s;
}

size_t readability(const Ex& a) { return nodeCount(a) + 2 * fractionDepth(a, 0); }

// ---------------------------------------------------------------- fractions

// (p/q)^n  ->  p^n · q^(−n)  for integer n.  Growing the tree here is fine:
// the denominator/numerator cancel that follows usually shrinks it a lot more.
Ex flattenQuotientPowers(const Ex& a) {
    if (!a) return a;
    if (a->isPow()) {
        Ex base = flattenQuotientPowers(a->args[0]);
        Ex e = a->args[1];
        if (e->isNum() && e->num.isInt()) {
            Ex num = base, den = Num(1);
            if (base->isMul()) {
                std::vector<Ex> pos, neg;
                for (auto& f : base->args) {
                    if (f->isPow() && f->args[1]->isNum() && f->args[1]->num.isNeg())
                        neg.push_back(Pow(f->args[0], Num(-f->args[1]->num)));
                    else
                        pos.push_back(f);
                }
                if (!neg.empty()) {
                    num = pos.empty() ? Num(1) : Mul(pos);
                    den = neg.size() == 1 ? neg[0] : Mul(neg);
                }
            }
            if (!den->isOne() || (base->isPow() && base->args[1]->isNum() &&
                                  base->args[1]->num.isNeg())) {
                if (den->isOne()) {
                    num = Num(1);
                    den = Pow(base->args[0], Num(-base->args[1]->num));
                }
                long long n = e->num.n;
                Ex pn = Pow(num, Num(n < 0 ? -n : n));
                Ex pd = Pow(den, Num(n < 0 ? -n : n));
                return n < 0 ? Mul(pd, Pow(pn, Num(-1))) : Mul(pn, Pow(pd, Num(-1)));
            }
        }
        return Pow(base, e);
    }
    if (a->isMul()) {
        std::vector<Ex> v;
        for (auto& f : a->args) v.push_back(flattenQuotientPowers(f));
        return Mul(v);
    }
    if (a->isAdd()) {
        std::vector<Ex> v;
        for (auto& t : a->args) v.push_back(flattenQuotientPowers(t));
        return Add(v);
    }
    if (a->isFunc()) {
        std::vector<Ex> v;
        for (auto& f : a->args) v.push_back(flattenQuotientPowers(f));
        return Fn(a->name, v);
    }
    return a;
}

// Split a product-like expression into numerator and denominator (positive
// exponents only).  Negative powers that are not integers stay in the
// numerator, so they never take part in a polynomial cancellation.
bool splitFraction(const Ex& a, Ex& num, Ex& den) {
    num = Num(1);
    den = Num(1);
    std::vector<Ex> pos, neg;
    auto push = [&](const Ex& f) {
        if (f->isPow() && f->args[1]->isNum() && f->args[1]->num.isInt() &&
            f->args[1]->num.isNeg()) {
            neg.push_back(Pow(f->args[0], Num(-f->args[1]->num)));
            return;
        }
        if (f->isNum() && f->num.d != 1) {
            pos.push_back(Num(Rational(f->num.n, 1)));
            neg.push_back(Num(Rational(f->num.d, 1)));
            return;
        }
        pos.push_back(f);
    };
    if (a->isMul()) {
        for (auto& f : a->args) push(f);
    } else {
        push(a);
    }
    if (pos.empty()) num = Num(1);
    else num = pos.size() == 1 ? pos[0] : Mul(pos);
    if (neg.empty()) den = Num(1);
    else den = neg.size() == 1 ? neg[0] : Mul(neg);
    return !den->isOne();
}

// ---------------------------------------------------------------- polynomials

// Numeric content of an expanded polynomial: gcd of the numeric factors of
// every term.
Rational numericContent(const Ex& e) {
    Rational g(0);
    std::function<void(const Ex&)> scan = [&](const Ex& t) {
        Rational c(1);
        if (t->isNum()) c = t->num;
        else if (t->isMul()) {
            for (auto& f : t->args) {
                if (f->isNum()) { c = c * f->num; break; }
            }
        }
        g = g.isZero() ? c.abs() : ratGcd(g, c);
    };
    if (e->isAdd()) {
        for (auto& t : e->args) scan(t);
    } else {
        scan(e);
    }
    return g.isZero() ? Rational(1) : g;
}

// a / b where both are expressions in a field of rational functions; accepted
// only when b·(a/b) collapses back to a.
bool divideExact(const Ex& a, const Ex& b, Ex& out) {
    if (!b || b->isZero()) return false;
    try {
        Ex q = simplify(Mul(a, Pow(b, Num(-1))));
        Ex back = expand(Mul(q, b));
        Ex diff = simplify(Add(expand(a), Mul(Num(-1), back)));
        if (!diff->isZero()) return false;
        out = q;
        return true;
    } catch (...) {
        return false;
    }
}

// True when an expression collapses to zero after expansion.  Long division
// leaves remainders such as -(-2y-1)(-2y+1) + 4y^2 - 1 that are zero but not
// in normal form, so a bare structural test is not enough.
bool isZeroPoly(const Ex& e) {
    try {
        return simplify(expand(e))->isZero();
    } catch (...) {
        return false;
    }
}

// Long division in one variable, with expressions in the other variables as
// coefficients.  Returns false when the division is not exact.
bool dividePolyVar(const Ex& A, const Ex& B, const std::string& v, Ex& out) {
    std::vector<Ex> a, b;
    if (!polyCoeffs(expand(A), v, a)) return false;
    if (!polyCoeffs(expand(B), v, b)) return false;
    if (b.empty() || b.back()->isZero()) return false;
    long long db = (long long)b.size() - 1;
    if ((long long)a.size() - 1 < db) return false;
    std::vector<Ex> r = a;
    std::vector<Ex> q(r.size(), Num(0));
    for (int guard = 0; guard < 400; ++guard) {
        while (r.size() > 1 && r.back()->isZero()) r.pop_back();
        // A zero remainder means the division is over. Without this, dividing
        // by a constant would keep producing a zero quotient term that
        // overwrites the real constant coefficient.
        if (r.size() == 1 && isZeroPoly(r[0])) break;
        long long dr = (long long)r.size() - 1;
        if (dr < db) break;
        long long shift = dr - db;
        Ex c;
        if (!divideExact(r.back(), b.back(), c)) return false;
        q[(size_t)shift] = c;
        std::vector<Ex> next(r.size(), Num(0));
        for (size_t i = 0; i < r.size(); ++i) {
            Ex sub = Num(0);
            if ((long long)i >= shift && (size_t)(i - shift) < b.size())
                sub = Mul(c, b[i - shift]);
            next[i] = simplify(Add(r[i], Mul(Num(-1), sub)));
        }
        r.swap(next);
    }
    while (r.size() > 1 && r.back()->isZero()) r.pop_back();
    if (!(r.size() == 1 && isZeroPoly(r[0]))) return false;
    Ex Q = Num(0);
    for (size_t i = 0; i < q.size(); ++i) {
        if (q[i]->isZero()) continue;
        Q = Add(Q, Mul(q[i], Pow(Sym(v), Num((long long)i))));
    }
    out = simplify(Q);
    return true;
}

bool isPolynomialIn(const Ex& e, const std::string& v) {
    std::vector<Ex> c;
    return polyCoeffs(expand(e), v, c);
}

// A denominator whose leading coefficient is negative reads better with the
// sign moved in front: (x+1)/(-x+1) becomes (x+1)/(1-x).
bool negativeLead(const Ex& poly, const std::string& v) {
    std::vector<Ex> c;
    if (!polyCoeffs(poly, v, c) || c.empty()) return false;
    Rational q(1);
    auto lead = [&](const Ex& e, Rational& out) {
        if (e->isNum()) { out = e->num; return true; }
        if (e->isMul()) {
            for (auto& f : e->args)
                if (f->isNum()) { out = f->num; return true; }
        }
        return false;
    };
    if (!lead(c.back(), q)) return false;
    return q.isNeg();
}

// Cancel the common polynomial factor of num and den.  Univariate inputs use
// the exact Euclidean algorithm; for several variables the denominator (or the
// numerator) is tried as a divisor, which is the shape almost every textbook
// rational expression takes.
bool cancelPolynomialFactor(const Ex& numIn, const Ex& denIn, Ex& numOut, Ex& denOut) {
    Ex N = expand(numIn), D = expand(denIn);
    if (nodeCount(N) > 3000 || nodeCount(D) > 3000) return false;
    // Everything cancels: a numerator that expands to zero owns the answer.
    if (N->isZero()) {
        numOut = Num(0);
        denOut = Num(1);
        return !denIn->isOne();
    }
    bool changed = false;

    // Numeric content first: (2x+2)/(2x−2) → (x+1)/(x−1).
    Rational cn = numericContent(N), cd = numericContent(D);
    if (!cn.isOne() || !cd.isOne()) {
        Rational g = ratGcd(cn, cd);
        if (!g.isOne() && !g.isZero()) {
            N = simplify(Mul(N, Pow(Num(g), Num(-1))));
            D = simplify(Mul(D, Pow(Num(g), Num(-1))));
            changed = true;
        }
    }

    std::vector<std::string> sn, sd;
    collectSymbols(N, sn);
    collectSymbols(D, sd);
    for (auto& s : sn)
        if (std::find(sd.begin(), sd.end(), s) == sd.end()) sd.push_back(s);

    // One variable only: exact GCD over Q.
    if (sd.size() == 1 && sd[0] != "dx" && sd[0] != "dy") {
        Poly pn, pd;
        if (toPoly(N, sd[0], pn) && toPoly(D, sd[0], pd) && !pn.isZero() && !pd.isZero()) {
            Poly g = polyGcd(pn, pd);
            if (g.deg() >= 1 || (!g.isZero() && g.c.size() == 1 && !g.c[0].isOne())) {
                Poly qn, rn, qd, rd;
                polyDivMod(pn, g, qn, rn);
                polyDivMod(pd, g, qd, rd);
                if (rn.isZero() && rd.isZero()) {
                    N = polyToExpr(qn, sd[0]);
                    D = polyToExpr(qd, sd[0]);
                    changed = true;
                }
            }
        }
    } else if (sd.size() > 1) {
        for (auto& v : sd) {
            if (v == "dx" || v == "dy") continue;
            if (!isPolynomialIn(N, v) || !isPolynomialIn(D, v)) continue;
            Ex q;
            if (dividePolyVar(N, D, v, q) && !q->isZero()) {
                N = q;
                D = Num(1);
                changed = true;
                break;
            }
            if (dividePolyVar(D, N, v, q) && !q->isZero()) {
                D = q;
                N = Num(1);
                changed = true;
                break;
            }
        }
    }

    if (!changed && D->isOne()) return false;
    // Move a negative leading sign out of the denominator.
    if (!D->isOne()) {
        std::vector<std::string> dv;
        collectSymbols(D, dv);
        if (dv.size() != 1) dv.clear();
        for (auto& v : dv) {
            if (v == "dx" || v == "dy") continue;
            if (!negativeLead(D, v)) continue;
            N = simplify(Mul(Num(-1), N));
            D = simplify(Mul(Num(-1), D));
            break;
        }
    }
    numOut = N;
    denOut = D;
    return true;
}

// ---------------------------------------------------------------- exp / log

bool isLogOf(const Ex& t, Ex& arg) {
    if (t->isFunc("ln") && t->args.size() == 1) { arg = t->args[0]; return true; }
    if (t->isMul()) {
        for (auto& f : t->args) {
            if (f->isFunc("ln") && f->args.size() == 1) { arg = f->args[0]; return true; }
        }
    }
    return false;
}

// exp(c·ln u) = u^c and exp(ln u) = u.  A sum inside exp is split over a
// product so the log terms can meet their exponential.
Ex expLogRules(const Ex& a) {
    if (!a) return a;
    if (a->isFunc()) {
        std::vector<Ex> v;
        for (auto& f : a->args) v.push_back(expLogRules(f));
        if (a->name == "exp" && v.size() == 1) {
            Ex arg = v[0];
            if (arg->isZero()) return Num(1);
            if (arg->isFunc("ln") && arg->args.size() == 1) return arg->args[0];
            if (arg->isAdd()) {
                std::vector<Ex> out;
                Ex rest = Num(0);
                bool anyLog = false;
                for (auto& t : arg->args) {
                    Ex logArg;
                    if (!isLogOf(t, logArg)) { rest = Add(rest, t); continue; }
                    Ex coeff = Num(1);
                    if (t->isMul()) {
                        coeff = Num(1);
                        for (auto& f : t->args) {
                            if (!f->isFunc("ln") && f->isNum()) coeff = Mul(coeff, f);
                        }
                    }
                    anyLog = true;
                    Ex pow = coeff->isOne() ? logArg
                                            : simplify(Pow(logArg, coeff));
                    out.push_back(expLogRules(pow));
                }
                if (anyLog) {
                    if (!rest->isZero()) out.push_back(Fn("exp", {rest}));
                    if (out.empty()) return Num(1);
                    return out.size() == 1 ? out[0] : Mul(out);
                }
            }
            return Fn("exp", {arg});
        }
        if (a->name == "ln" && v.size() == 1) {
            Ex arg = v[0];
            if (arg->isOne()) return Num(0);
            if (arg->isSym("e")) return Num(1);
            if (arg->isFunc("exp") && arg->args.size() == 1) return arg->args[0];
            return Fn("ln", {arg});
        }
        return Fn(a->name, v);
    }
    if (a->isAdd()) {
        std::vector<Ex> v;
        for (auto& t : a->args) v.push_back(expLogRules(t));
        return Add(v);
    }
    if (a->isMul()) {
        std::vector<Ex> v;
        for (auto& f : a->args) v.push_back(expLogRules(f));
        return Mul(v);
    }
    if (a->isPow()) return Pow(expLogRules(a->args[0]), expLogRules(a->args[1]));
    return a;
}

// ---------------------------------------------------------------- driver

// A negative leading coefficient in the denominator reads better as a minus in
// front: (x+1)/(-x+1) becomes -(x+1)/(x-1).  Applied after the choice so the
// extra sign does not make a better form look bigger.
Ex positiveDenominator(const Ex& a) {
    Ex num, den;
    if (!splitFraction(a, num, den)) return a;
    std::vector<std::string> dv;
    collectSymbols(den, dv);
    if (dv.size() != 1) return a;
    for (auto& v : dv) {
        if (v == "dx" || v == "dy") continue;
        if (!negativeLead(den, v)) continue;
        Ex nn = simplify(Mul(Num(-1), num));
        // Distribute the sign over the denominator's terms so −x + 1 becomes
        // x − 1; a bare -(−x + 1) would keep the minus inside.
        Ex dd = den;
        if (den->isAdd()) {
            std::vector<Ex> terms;
            for (auto& t : den->args) terms.push_back(simplify(Mul(Num(-1), t)));
            dd = Add(terms);
        } else {
            dd = simplify(Mul(Num(-1), den));
        }
        Ex rebuilt = dd->isOne() ? nn : Mul(nn, Pow(dd, Num(-1)));
        // The explicit leading minus costs one node, so allow that much slack.
        if (readability(rebuilt) <= readability(a) + 1) return rebuilt;
    }
    return a;
}

// Pull a factor shared by several terms out of an expanded sum:
// 2*x*exp(x) + x^2*exp(x) becomes exp(x)*(x^2 + 2*x).  Only a factor that is
// itself an expression (a function or a composite) is considered: a bare
// symbol reads better left distributed over the polynomial.
Ex factorCommonTerm(const Ex& a) {
    if (!a->isAdd()) return a;
    if (a->args.size() < 2 || a->args.size() > 8) return a;
    // Work with integer coefficients: a rational common factor is best kept
    // outside the sum, because the canonical form distributes it again when it
    // sits inside a factor.
    Rational g = numericContent(a);
    if (g.isZero()) return a;
    Ex base = a;
    if (!g.isOne()) {
        // Scale term by term: multiplying the whole sum can wrap it in a
        // leading minus instead of returning an Add.
        std::vector<Ex> scaled;
        for (auto& t : a->args)
            scaled.push_back(simplify(Mul(t, Pow(Num(g), Num(-1)))));
        base = Add(scaled);
    }
    if (!base->isAdd()) return a;
    const auto& ts = base->args;

    // Split a factor into base and integer exponent, so x^2 and x^5 can be
    // treated as the same base.
    auto splitPow = [](const Ex& f, Ex& b, Rational& e) {
        if (f->isPow() && f->args[1]->isNum() && f->args[1]->num.isInt()) {
            b = f->args[0];
            e = f->args[1]->num;
        } else {
            b = f;
            e = Rational(1);
        }
    };
    auto stripPow = [&](const Ex& term, const std::string& key, Rational dec, Ex& out) {
        if (!term->isMul()) {
            Ex b;
            Rational e;
            splitPow(term, b, e);
            if (term->isNum() || toPlain(b) != key) return false;
            Rational left = e - dec;
            if (left.isZero()) {
                out = Num(1);
            } else {
                out = left.isOne() ? b : Pow(b, Num(left));
            }
            return true;
        }
        std::vector<Ex> rest;
        bool done = false;
        for (auto& f : term->args) {
            if (!done && !f->isNum()) {
                Ex b;
                Rational e;
                splitPow(f, b, e);
                if (toPlain(b) == key) {
                    Rational left = e - dec;
                    if (!left.isZero())
                        rest.push_back(left.isOne() ? b : Pow(b, Num(left)));
                    done = true;
                    continue;
                }
            }
            rest.push_back(f);
        }
        if (!done) return false;
        out = rest.empty() ? Num(1) : Mul(rest);
        return true;
    };

    struct Hit {
        Ex base;
        std::vector<size_t> terms;
        std::vector<Rational> exps;
    };
    std::map<std::string, Hit> found;
    for (size_t i = 0; i < ts.size(); ++i) {
        std::vector<Ex> facs;
        if (ts[i]->isMul()) {
            for (auto& f : ts[i]->args)
                if (!f->isNum()) facs.push_back(f);
        } else if (!ts[i]->isNum()) {
            facs.push_back(ts[i]);
        }
        for (auto& f : facs) {
            Ex b;
            Rational e;
            splitPow(f, b, e);
            auto& slot = found[toPlain(b)];
            if (slot.terms.empty() || slot.terms.back() != i) {
                slot.base = b;
                slot.terms.push_back(i);
                slot.exps.push_back(e);
            }
        }
    }
    Ex best = a;
    size_t bestRead = readability(a);
    for (auto& kv : found) {
        auto& hit = kv.second;
        if (hit.terms.size() < 2) continue;
        // Factoring a bare symbol out of a polynomial gives odd forms such as
        // x*(x + 3) + 2 or -1/6*x*(-2*x^2 + 9*x - 12); only a factor that is
        // itself an expression (a function or a composite) reads better.
        if (!hit.base->isFunc() && nodeCount(hit.base) < 3) continue;
        Rational minExp = hit.exps[0];
        for (auto& e : hit.exps) minExp = e < minExp ? e : minExp;
        if (minExp.isZero()) continue;
        std::vector<Ex> quot;
        bool ok = true;
        for (size_t i : hit.terms) {
            Ex q;
            if (!stripPow(ts[i], kv.first, minExp, q)) {
                ok = false;
                break;
            }
            quot.push_back(q);
        }
        if (!ok) continue;
        Ex factor = minExp.isOne() ? hit.base : Pow(hit.base, Num(minExp));
        Ex inner = quot.size() == 1 ? quot[0] : Add(quot);
        Ex factored = simplify(Mul(factor, inner));
        std::vector<Ex> rest;
        for (size_t i = 0; i < ts.size(); ++i) {
            if (std::find(hit.terms.begin(), hit.terms.end(), i) == hit.terms.end())
                rest.push_back(ts[i]);
        }
        Ex rebuilt = rest.empty() ? factored : Add({factored, rest.size() == 1 ? rest[0] : Add(rest)});
        rebuilt = simplify(rebuilt);
        if (!g.isOne()) rebuilt = simplify(Mul(Num(g), rebuilt));
        size_t read = readability(rebuilt);
        if (read < bestRead) {
            best = rebuilt;
            bestRead = read;
        }
    }
    return best;
}

// ---------------------------------------------------------------- presentation

// A plain monomial: a number, a symbol, or a product of those and of symbols
// raised to a positive rational power.  Fractional powers appear in series
// solutions (sqrt(x), x^(3/2)) and deserve the same tidy presentation.
bool monomialForm(const Ex& t, std::map<std::string, Rational>& exps) {
    if (!t) return false;
    if (t->isNum()) return true;
    if (t->isSym()) {
        exps[t->name] = exps[t->name] + Rational(1);
        return true;
    }
    if (t->isMul()) {
        for (auto& f : t->args)
            if (!monomialForm(f, exps)) return false;
        return true;
    }
    if (t->isPow() && t->args.size() == 2 && t->args[0]->isSym() && t->args[1]->isNum()) {
        const Rational e = t->args[1]->num;
        if (e.isNeg() || e.isZero()) return false;
        exps[t->args[0]->name] = exps[t->args[0]->name] + e;
        return true;
    }
    return false;
}

// A polynomial reads best multiplied out with its terms in descending degree:
// 1/3*x^3 - 3/2*x^2 + 2*x, not the canonical 3/2*x*(-x + 2/9*x^2 + 4/3).
// A series with fractional powers reads in the other direction, from the
// lowest power up: sqrt(x) - x^(3/2) + 1/6*x^(5/2).  Fractions and exponential
// answers are left alone, since there the factored shape carries real meaning
// (a denominator or an exponent).
Ex presentPolynomial(const Ex& a) {
    if (!a) return a;
    // Expanding a large product just to inspect it is not worth it.
    if (nodeCount(a) > 400) return a;
    Ex e;
    try {
        e = expand(a);
    } catch (...) {
        return a;
    }
    if (!e || !e->isAdd()) return a;
    std::vector<std::pair<std::map<std::string, Rational>, Ex>> terms;
    std::vector<std::string> names;
    for (auto& t : e->args) {
        std::map<std::string, Rational> exps;
        if (!monomialForm(t, exps)) return a;
        for (auto& kv : exps) names.push_back(kv.first);
        terms.emplace_back(std::move(exps), t);
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    bool allInteger = true;
    for (auto& p : terms)
        for (auto& kv : p.first)
            if (!kv.second.isInt()) allInteger = false;
    auto degree = [](const std::map<std::string, Rational>& m) {
        Rational d(0);
        for (auto& kv : m) d = d + kv.second;
        return d;
    };
    auto exponentOf = [](const std::map<std::string, Rational>& m, const std::string& n) {
        auto it = m.find(n);
        return it == m.end() ? Rational(0) : it->second;
    };
    std::stable_sort(terms.begin(), terms.end(), [&](const auto& x, const auto& y) {
        const Rational dx = degree(x.first), dy = degree(y.first);
        if (!(dx == dy)) return allInteger ? (dy < dx) : (dx < dy);
        for (auto& n : names) {
            const Rational ex = exponentOf(x.first, n), ey = exponentOf(y.first, n);
            if (!(ex == ey)) return allInteger ? (ey < ex) : (ex < ey);
        }
        return false;
    });
    auto node = std::make_shared<Expr>();
    node->kind = Kind::Add;
    for (auto& p : terms) node->args.push_back(p.second);
    return node;
}

// One normalisation round.  Anything that comes out harder to read than it
// went in is discarded, so this can only ever improve the expression.
Ex normalRound(const Ex& a) {
    Ex best = simplify(a);
    auto consider = [&](const Ex& cand) {
        if (!cand) return;
        if (readability(cand) < readability(best)) best = cand;
        // A cancelled sum often hides one factor repeated in every term.
        Ex grouped = factorCommonTerm(best);
        if (readability(grouped) < readability(best)) best = grouped;
    };
    try {
        Ex t = together(best);
        consider(t);
        Ex flat = flattenQuotientPowers(t);
        Ex ft = together(flat);
        consider(ft);
        Ex num, den;
        if (splitFraction(ft, num, den)) {
            Ex n2, d2;
            if (cancelPolynomialFactor(num, den, n2, d2)) {
                Ex nn = factorCommonTerm(simplify(n2));
                Ex dd = d2->isOne() ? d2 : factorCommonTerm(simplify(d2));
                Ex rebuilt = dd->isOne() ? nn : Mul(nn, Pow(dd, Num(-1)));
                consider(simplify(rebuilt));
                consider(expand(simplify(rebuilt)));
            }
        }
        consider(expLogRules(best));
        consider(expand(best));
        consider(factorCommonTerm(best));
    } catch (...) {
    }
    return best;
}

} // namespace

Ex simplifyStrong(const Ex& a) {
    if (!a) return a;
    // Expanding a machine-sized polynomial just to look for a common factor is
    // not worth it; those are already in normal form.
    if (nodeCount(a) > 800) return simplify(a);
    Ex cur = simplify(a);
    for (int round = 0; round < 8; ++round) {
        Ex next = normalRound(cur);
        if (equals(next, cur)) break;
        if (readability(next) > readability(cur)) break;
        cur = next;
    }
    return presentPolynomial(positiveDenominator(cur));
}

} // namespace ode
