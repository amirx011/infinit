// Infinit Lab — limit laws, L'Hôpital, one-sided limits and continuity.
// SPDX-License-Identifier: Apache-2.0
#include "limits.hpp"

#include "numeric.hpp"

#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace ode {
namespace {

// --------------------------------------------------------------- utilities
void addRule(std::vector<std::string>* rules, const char* id) {
    if (!rules) return;
    for (const auto& r : *rules)
        if (r == id) return;
    rules->push_back(id);
}

LimitOutcome mkFinite(const Ex& e) {
    LimitOutcome o;
    o.kind = LimitOutcome::Finite;
    o.value = e;
    return o;
}

LimitOutcome mkInf(int sign, const std::string& reason = "") {
    LimitOutcome o;
    o.kind = sign < 0 ? LimitOutcome::NegInf : LimitOutcome::PosInf;
    o.reason = reason;
    return o;
}

LimitOutcome mkUndef(const std::string& reason) {
    LimitOutcome o;
    o.kind = LimitOutcome::Undefined;
    o.reason = reason;
    return o;
}

bool isZero(const Ex& e) {
    if (!e) return false;
    if (e->isZero()) return true;
    try {
        return simplify(e)->isZero();
    } catch (...) {
        return false;
    }
}

bool sameExpr(const Ex& a, const Ex& b) {
    if (!a || !b) return a == b;
    try {
        Ex d = simplify(Add(a, Mul(Num(-1), b)));
        if (d->isZero()) return true;
    } catch (...) {}
    return equals(a, b);
}

double evalD(const Ex& f, const std::string& v, double x, bool* ok) {
    bool good = false;
    double y = std::nan("");
    try {
        y = evalReal(f, {{v, x}}, &good);
    } catch (...) {
        good = false;
    }
    if (ok) *ok = good && std::isfinite(y);
    return y;
}

// ------------------------------------------------------------ point helpers
bool infinitySign(const Ex& point, int* sign) {
    if (!point) return false;
    if (point->isSym()) {
        const std::string& n = point->name;
        if (n == "inf" || n == "infinity" || n == "oo") {
            if (sign) *sign = 1;
            return true;
        }
        if (n == "-inf" || n == "-infinity") {
            if (sign) *sign = -1;
            return true;
        }
    }
    if (point->isMul() && point->args.size() == 2) {
        Ex a = point->args[0], b = point->args[1];
        if (a->isNum() && a->num.isNeg() && b->isSym() &&
            (b->name == "inf" || b->name == "infinity" || b->name == "oo")) {
            if (sign) *sign = -1;
            return true;
        }
    }
    return false;
}

// -------------------------------------------------------- numeric estimates
struct Probe {
    bool ok = false;
    bool divergent = false;
    int sign = 1;
    double value = 0.0;
    double error = 1e9;
    bool oscillates = false;
};

// Richardson extrapolation on a geometric grid of step sizes.
Probe richardson(const std::vector<double>& ys, const std::vector<double>& ds) {
    (void)ds;
    Probe p;
    if (ys.size() < 3) return p;
    const size_t n = ys.size();

    // Monotone growth of the magnitude means the expression diverges.
    bool growing = true;
    for (size_t i = 1; i < n; ++i)
        if (std::fabs(ys[i]) < std::fabs(ys[i - 1]) * 1.2) growing = false;
    const double first = std::fabs(ys.front());
    const double last = std::fabs(ys.back());
    if (growing && (last > 2.0 * std::max(first, 1e-12)) && last > 1e-2) {
        p.divergent = true;
        p.sign = ys.back() < 0 ? -1 : 1;
        p.ok = true;
        return p;
    }

    // Values that shrink towards zero on every step: the limit is zero.
    bool shrinking = true;
    for (size_t i = 1; i < n; ++i)
        if (std::fabs(ys[i]) > std::fabs(ys[i - 1]) * 0.9) shrinking = false;
    if (shrinking && last < 1e-5) {
        p.ok = true;
        p.value = 0.0;
        p.error = 0.0;
        return p;
    }

    double bestErr = 1e18;
    double bestVal = 0.0;
    for (int power = 1; power <= 3; ++power) {
        const double r = std::pow(10.0, power);
        for (size_t k = 0; k + 1 < n; ++k) {
            const double v = (r * ys[k + 1] - ys[k]) / (r - 1.0);
            // consistency against the next pair
            if (k + 2 < n) {
                const double v2 = (r * ys[k + 2] - ys[k + 1]) / (r - 1.0);
                const double err = std::fabs(v2 - v);
                if (err < bestErr) {
                    bestErr = err;
                    bestVal = v2;
                }
            } else {
                const double err = std::fabs(v - ys[k + 1]) * 10.0;
                if (err < bestErr) {
                    bestErr = err;
                    bestVal = v;
                }
            }
        }
    }
    const double scale = std::max(1.0, std::fabs(bestVal));
    p.value = bestVal;
    p.error = bestErr;
    p.ok = bestErr <= 1e-7 * scale;
    if (!p.ok && std::fabs(bestVal) < 1e-7 && last < 1e-4) {
        p.ok = true;
        p.value = 0.0;
        p.error = 0.0;
        return p;
    }
    if (!p.ok) {
        // bounded oscillation: values stay bounded but do not settle
        double lo = ys[0], hi = ys[0];
        for (double y : ys) {
            lo = std::min(lo, y);
            hi = std::max(hi, y);
        }
        p.oscillates = (hi - lo) > 1e-6 * std::max(1.0, std::fabs(lo)) && hi - lo < 1e6;
    }
    return p;
}

Probe probeNear(const Ex& f, const std::string& v, double a, int dir) {
    std::vector<double> ys, ds;
    const double h = std::max(1.0, std::fabs(a));
    for (int k = 1; k <= 8; ++k) {
        const double d = h * std::pow(10.0, -k);
        const double x = a + dir * d;
        bool ok = false;
        const double y = evalD(f, v, x, &ok);
        if (!ok) continue;
        ys.push_back(y);
        ds.push_back(d);
    }
    Probe p;
    if (ys.size() < 3) {
        p.ok = false;
        return p;
    }
    return richardson(ys, ds);
}

Probe probeInfinity(const Ex& f, const std::string& v, int dir) {
    std::vector<double> ys, ds;
    for (int k = 1; k <= 8; ++k) {
        const double r = std::pow(10.0, k);
        const double x = dir * r;
        bool ok = false;
        const double y = evalD(f, v, x, &ok);
        if (!ok) continue;
        ys.push_back(y);
        ds.push_back(1.0 / r);
    }
    Probe p;
    if (ys.size() < 3) return p;
    return richardson(ys, ds);
}

bool boundedNear(const Ex& f, const std::string& v, const Ex& a, int dir) {
    int sign = 1;
    const bool atInf = infinitySign(a, &sign);
    double base = 0.0;
    if (!atInf) {
        bool ok = false;
        base = evalReal(a, {}, &ok);
        if (!ok) return false;
    }
    for (int k = 1; k <= 12; ++k) {
        const double x = atInf ? sign * std::pow(10.0, k)
                               : base + dir * std::max(1.0, std::fabs(base)) * std::pow(10.0, -k);
        bool ok = false;
        const double y = evalD(f, v, x, &ok);
        if (!ok) return false;
        if (std::fabs(y) > 100.0) return false;
    }
    return true;
}

// A numeric estimate may correspond to a small rational; publish it as exact in
// that case (the result is still tagged as numerically verified).
Ex snapRational(double v, bool* snapped) {
    if (snapped) *snapped = false;
    if (!std::isfinite(v)) return nullptr;
    Rational r = Rational::fromDouble(v, 10000);
    if (r.d == 0) return nullptr;
    const double err = std::fabs(r.toDouble() - v);
    const double scale = std::max(1.0, std::fabs(v));
    if (err <= 1e-9 * scale) {
        if (snapped) *snapped = true;
        return Num(r);
    }
    return nullptr;
}

LimitOutcome fromProbe(const Probe& p, const std::string& reason) {
    if (!p.ok) {
        if (p.oscillates) return mkUndef("the values oscillate without settling");
        return mkUndef("no stable value was found");
    }
    if (p.divergent) return mkInf(p.sign, "the values grow without bound");
    LimitOutcome o;
    bool snapped = false;
    Ex q = snapRational(p.value, &snapped);
    o.kind = LimitOutcome::Finite;
    o.number = p.value;
    o.numeric = true;
    o.approximated = snapped;
    if (snapped) {
        o.value = q;
        o.reason = reason;
    } else {
        o.reason = reason;
    }
    return o;
}

// --------------------------------------------------------------- recursion
LimitOutcome limitRec(const Ex& e, const std::string& v, const Ex& a, int dir,
                      std::vector<std::string>* rules, int depth);

LimitOutcome numericFallback(const Ex& e, const std::string& v, const Ex& a, int dir) {
    int sign = 1;
    if (infinitySign(a, &sign)) {
        Probe p = probeInfinity(e, v, sign);
        return fromProbe(p, "numeric estimate");
    }
    Probe p = probeNear(e, v, [&] {
        bool ok = false;
        double d = 0;
        try {
            d = evalReal(a, {}, &ok);
        } catch (...) {}
        return ok ? d : 0.0;
    }(), dir);
    return fromProbe(p, "numeric estimate");
}

// Try algebraic rewrites (combine fractions, expand) before numeric probing.
LimitOutcome rewriteOrNumeric(const Ex& e, const std::string& v, const Ex& a, int dir,
                              std::vector<std::string>* rules, int depth) {
    std::vector<Ex> candidates;
    try {
        Ex t = simplify(together(e));
        if (t && !equals(t, e)) candidates.push_back(t);
    } catch (...) {}
    try {
        Ex x = simplify(expand(e));
        if (x && !equals(x, e)) candidates.push_back(x);
    } catch (...) {}
    for (const auto& c : candidates) {
        if (!c || equals(c, e)) continue;
        LimitOutcome r = limitRec(c, v, a, dir, rules, depth + 1);
        if (r.kind != LimitOutcome::Undefined) {
            addRule(rules, "algebraic_simplification");
            return r;
        }
    }
    return numericFallback(e, v, a, dir);
}

// Split a product into numerator and denominator factors (a/b is Mul(a, Pow(b,-1))).
void splitFraction(const Ex& e, std::vector<Ex>& num, std::vector<Ex>& den) {
    if (!e) return;
    if (e->isMul()) {
        for (const auto& f : e->args) splitFraction(f, num, den);
        return;
    }
    if (e->isPow() && e->args.size() == 2 && e->args[1]->isNum() && e->args[1]->num.isNeg()) {
        den.push_back(Pow(e->args[0], Num(-e->args[1]->num)));
        return;
    }
    num.push_back(e);
}

// -------------------------------------------------------------- the limits
LimitOutcome quotientLimit(const Ex& N, const Ex& D, const std::string& v, const Ex& a, int dir,
                           std::vector<std::string>* rules, int depth);

LimitOutcome lhopital(const Ex& N, const Ex& D, const std::string& v, const Ex& a, int dir,
                      std::vector<std::string>* rules, int depth) {
    if (depth > 6) return numericFallback(Mul(N, Pow(D, Num(-1))), v, a, dir);
    Ex dN, dD;
    try {
        dN = simplify(diff(N, v));
        dD = simplify(diff(D, v));
    } catch (...) {
        return numericFallback(Mul(N, Pow(D, Num(-1))), v, a, dir);
    }
    if (!dN || !dD) return numericFallback(Mul(N, Pow(D, Num(-1))), v, a, dir);
    if (equals(dN, N) && equals(dD, D)) return numericFallback(Mul(N, Pow(D, Num(-1))), v, a, dir);
    addRule(rules, "lhopital");
    LimitOutcome r = quotientLimit(dN, dD, v, a, dir, rules, depth + 1);
    if (r.kind == LimitOutcome::Undefined)
        return numericFallback(Mul(N, Pow(D, Num(-1))), v, a, dir);
    return r;
}

// Infinite value of N/D when D -> 0 and N -> nonzero: the sign is read off a
// one-sided numerical probe of the whole quotient.
LimitOutcome divergentQuotient(const Ex& N, const Ex& D, const std::string& v, const Ex& a,
                               int dir) {
    LimitOutcome whole = numericFallback(Mul(N, Pow(D, Num(-1))), v, a, dir);
    if (whole.kind == LimitOutcome::PosInf || whole.kind == LimitOutcome::NegInf) return whole;
    return mkUndef("the sign of the divergence could not be determined");
}

LimitOutcome quotientLimit(const Ex& N, const Ex& D, const std::string& v, const Ex& a, int dir,
                           std::vector<std::string>* rules, int depth) {
    LimitOutcome Ln = limitRec(N, v, a, dir, rules, depth + 1);
    LimitOutcome Ld = limitRec(D, v, a, dir, rules, depth + 1);
    if ((Ln.kind == LimitOutcome::Finite && !Ln.value) ||
        (Ld.kind == LimitOutcome::Finite && !Ld.value))
        return rewriteOrNumeric(Mul(N, Pow(D, Num(-1))), v, a, dir, rules, depth);
    if (Ln.kind == LimitOutcome::Undefined || Ld.kind == LimitOutcome::Undefined) {
        // The sign of the numerator may still be known even if its limit is not;
        // fall back to a rewrite or a numeric probe.
        return rewriteOrNumeric(Mul(N, Pow(D, Num(-1))), v, a, dir, rules, depth);
    }
    if (Ld.kind == LimitOutcome::Finite && !isZero(Ld.value)) {
        addRule(rules, "quotient");
        if (Ln.kind == LimitOutcome::Finite) {
            try {
                return mkFinite(simplify(Mul(Ln.value, Pow(Ld.value, Num(-1)))));
            } catch (...) {
                return mkFinite(Mul(Ln.value, Pow(Ld.value, Num(-1))));
            }
        }
        // infinite numerator over a finite non-zero denominator
        bool ok = false;
        const double dv = evalReal(Ld.value, {}, &ok);
        int sign = (ok && dv < 0) ? -1 : 1;
        if (Ln.kind == LimitOutcome::NegInf) sign = -sign;
        return mkInf(sign, "infinite numerator over a finite denominator");
    }
    if (Ld.kind == LimitOutcome::Finite) { // denominator -> 0
        if (Ln.kind == LimitOutcome::Finite && isZero(Ln.value))
            return lhopital(N, D, v, a, dir, rules, depth);
        if (Ln.kind == LimitOutcome::Finite) return divergentQuotient(N, D, v, a, dir);
        // infinite / zero
        return divergentQuotient(N, D, v, a, dir);
    }
    // denominator -> infinity
    if (Ln.kind == LimitOutcome::Finite) {
        addRule(rules, "quotient");
        return mkFinite(Num(0));
    }
    if (Ln.kind == LimitOutcome::PosInf || Ln.kind == LimitOutcome::NegInf)
        return lhopital(N, D, v, a, dir, rules, depth);
    return rewriteOrNumeric(Mul(N, Pow(D, Num(-1))), v, a, dir, rules, depth);
}

LimitOutcome powLimit(const Ex& base, const Ex& exp, const std::string& v, const Ex& a, int dir,
                      std::vector<std::string>* rules, int depth) {
    LimitOutcome B = limitRec(base, v, a, dir, rules, depth + 1);
    LimitOutcome E = limitRec(exp, v, a, dir, rules, depth + 1);
    const Ex whole = Pow(base, exp);
    if ((B.kind == LimitOutcome::Finite && !B.value) ||
        (E.kind == LimitOutcome::Finite && !E.value))
        return rewriteOrNumeric(whole, v, a, dir, rules, depth);

    auto expRewrite = [&]() -> LimitOutcome {
        if (depth > 6) return mkUndef("the power form did not resolve");
        addRule(rules, "exponential_rewrite");
        // Compute the limit of exponent * ln(base) first: writing it back as
        // exp(...) would be folded into Pow(base, exponent) again by the AST
        // canonicaliser and loop forever.
        LimitOutcome P = limitRec(Mul(exp, Fn("ln", {base})), v, a, dir, rules, depth + 1);
        if (P.kind == LimitOutcome::Finite && P.value) {
            addRule(rules, "composite");
            try {
                return mkFinite(simplify(Fn("exp", {P.value})));
            } catch (...) {
                return rewriteOrNumeric(whole, v, a, dir, rules, depth);
            }
        }
        if (P.kind == LimitOutcome::PosInf) return mkInf(1, "exponential growth");
        if (P.kind == LimitOutcome::NegInf) return mkFinite(Num(0));
        return rewriteOrNumeric(whole, v, a, dir, rules, depth);
    };

    // Both limits finite: apply the power law when it is defined.
    if (B.kind == LimitOutcome::Finite && E.kind == LimitOutcome::Finite) {
        addRule(rules, "power");
        if (!(isZero(B.value) && isZero(E.value))) {
            try {
                Ex r = simplify(Pow(B.value, E.value));
                bool ok = false;
                const double d = evalReal(r, {}, &ok);
                if (ok && std::isfinite(d)) return mkFinite(r);
            } catch (...) {}
        }
        LimitOutcome r = expRewrite();
        if (r.kind == LimitOutcome::Undefined)
            return rewriteOrNumeric(whole, v, a, dir, rules, depth);
        return r;
    }

    // base -> 0 or 1, exponent -> infinity
    if (B.kind == LimitOutcome::Finite) {
        bool ok = false;
        const double b = evalReal(B.value, {}, &ok);
        if (ok) {
            addRule(rules, "power");
            if (b == 0.0) {
                if (E.kind == LimitOutcome::PosInf) return mkFinite(Num(0));
                if (E.kind == LimitOutcome::NegInf) return mkInf(1, "zero to a negative infinite power");
            }
            if (b == 1.0) {
                LimitOutcome r = expRewrite();
                if (r.kind != LimitOutcome::Undefined) return r;
            }
            if (b > 0 && b != 1 && E.kind != LimitOutcome::Finite) {
                const bool grows = (b > 1.0) == (E.kind == LimitOutcome::PosInf);
                return grows ? mkInf(1, "a positive base raised to an infinite power")
                             : mkFinite(Num(0));
            }
            if (b > 0 && E.kind != LimitOutcome::Finite) {
                // b == 1 handled above
            }
            if (b < 0) return mkUndef("a negative base raised to a non-integer limit");
        }
        LimitOutcome r = expRewrite();
        if (r.kind != LimitOutcome::Undefined) return r;
        return rewriteOrNumeric(whole, v, a, dir, rules, depth);
    }

    // infinite base, finite exponent
    if (E.kind == LimitOutcome::Finite) {
        bool ok = false;
        const double p = evalReal(E.value, {}, &ok);
        if (ok && p > 0) {
            addRule(rules, "power");
            return mkInf(1, "an infinite base raised to a positive power");
        }
        if (ok && p < 0) {
            addRule(rules, "power");
            return mkFinite(Num(0));
        }
        return rewriteOrNumeric(whole, v, a, dir, rules, depth);
    }
    return rewriteOrNumeric(whole, v, a, dir, rules, depth);
}

bool isContinuousFn(const std::string& name) {
    return name == "sin" || name == "cos" || name == "exp" || name == "atan" || name == "asin" ||
           name == "acos" || name == "sinh" || name == "cosh" || name == "tanh" ||
           name == "asinh" || name == "acosh" ||
           name == "sqrt" || name == "ln" || name == "abs" || name == "log" ||
           name == "sec" || name == "csc" || name == "cot" || name == "tan";
}

LimitOutcome funcLimit(const Ex& e, const std::string& v, const Ex& a, int dir,
                       std::vector<std::string>* rules, int depth) {
    const std::string& name = e->name;
    if (e->args.empty()) return mkUndef("empty function");
    LimitOutcome A = limitRec(e->args[0], v, a, dir, rules, depth + 1);
    if (A.kind == LimitOutcome::Finite && !A.value) return rewriteOrNumeric(e, v, a, dir, rules, depth);
    if (A.kind == LimitOutcome::Undefined)
        return rewriteOrNumeric(e, v, a, dir, rules, depth);

    if (A.kind != LimitOutcome::Finite) {
        if (name == "exp") {
            addRule(rules, "composite");
            return A.kind == LimitOutcome::PosInf ? mkInf(1, "exp of a divergent argument")
                                                  : mkFinite(Num(0));
        }
        if (name == "ln" || name == "log") {
            addRule(rules, "composite");
            return A.kind == LimitOutcome::PosInf ? mkInf(1, "logarithm of an infinite argument")
                                                  : mkUndef("logarithm of a negative infinite argument");
        }
        if (name == "sqrt") {
            addRule(rules, "composite");
            return A.kind == LimitOutcome::PosInf ? mkInf(1, "square root of an infinite argument")
                                                  : mkUndef("square root of a negative infinite argument");
        }
        if (name == "abs") {
            addRule(rules, "composite");
            return mkInf(1, "absolute value of an infinite argument");
        }
        if (name == "sinh") {
            addRule(rules, "composite");
            const int sign = A.kind == LimitOutcome::PosInf ? 1 : -1;
            return mkInf(sign, "hyperbolic sine of an infinite argument");
        }
        if (name == "cosh") {
            addRule(rules, "composite");
            return mkInf(1, "hyperbolic cosine of an infinite argument");
        }
        if (name == "asinh") {
            addRule(rules, "composite");
            const int sign = A.kind == LimitOutcome::PosInf ? 1 : -1;
            return mkInf(sign, "inverse hyperbolic sine of an infinite argument");
        }
        if (name == "acosh") {
            addRule(rules, "composite");
            return A.kind == LimitOutcome::PosInf ? mkInf(1, "inverse hyperbolic cosine of +infinity")
                                                  : mkUndef("inverse hyperbolic cosine is undefined below 1");
        }
        if (name == "sin" || name == "cos" || name == "tan" || name == "tanh" || name == "atan") {
            if (name == "tanh") {
                addRule(rules, "composite");
                const int sign = A.kind == LimitOutcome::PosInf ? 1 : -1;
                return mkFinite(Num(sign));
            }
            if (name == "atan") {
                // arctan(+inf) = pi/2 and arctan(-inf) = -pi/2 (not +-1).
                addRule(rules, "composite");
                const int sign = A.kind == LimitOutcome::PosInf ? 1 : -1;
                return mkFinite(Mul(Num(Rational(sign, 2)), Sym("pi")));
            }
            return mkUndef("the trigonometric argument grows without bound");
        }
        return rewriteOrNumeric(e, v, a, dir, rules, depth);
    }

    // Finite argument: substitute into the function when it stays in the domain.
    if (isContinuousFn(name)) {
        try {
            Ex inner = A.value;
            if ((name == "ln" || name == "log") && isZero(inner)) {
                // ln(u) with u -> 0
                if (e->args[0]->isFunc("abs")) {
                    // ln|u| -> -inf from both sides; the absolute value makes
                    // the logarithm defined on a punctured neighbourhood.
                    addRule(rules, "composite");
                    return mkInf(-1, "logarithm of an absolute value tending to zero");
                }
                if (dir > 0) {
                    addRule(rules, "composite");
                    return mkInf(-1, "logarithm tends to zero from the right");
                }
                if (dir < 0) return mkUndef("the logarithm is not defined for negative arguments");
            }
            if (name == "sqrt" && isZero(inner)) {
                if (dir < 0) return mkUndef("the square root is not defined for negative arguments");
                addRule(rules, "composite");
                return mkFinite(Num(0));
            }
            if (name == "tan" || name == "sec" || name == "cot" || name == "csc") {
                // Poles: cos(x) = 0 for tan/sec, sin(x) = 0 for cot/csc.
                const char* denomFn = (name == "cot" || name == "csc") ? "sin" : "cos";
                bool okDen = false;
                double cd = std::nan("");
                try {
                    cd = evalReal(Fn(denomFn, {inner}), {}, &okDen);
                } catch (...) {
                    okDen = false;
                }
                if (okDen && std::isfinite(cd) && std::fabs(cd) < 1e-9)
                    return numericFallback(e, v, a, dir);
            }
            Ex r = simplify(Fn(name, {inner}));
            bool ok = false;
            const double d = evalReal(r, {}, &ok);
            if (ok && std::isfinite(d) && std::fabs(d) < 1e12) {
                addRule(rules, "composite");
                return mkFinite(r);
            }
        } catch (...) {}
    }
    return rewriteOrNumeric(e, v, a, dir, rules, depth);
}

LimitOutcome addLimit(const Ex& e, const std::string& v, const Ex& a, int dir,
                      std::vector<std::string>* rules, int depth) {
    int posInf = 0, negInf = 0;
    std::vector<Ex> finite;
    bool undef = false;
    for (const auto& t : e->args) {
        LimitOutcome L = limitRec(t, v, a, dir, rules, depth + 1);
        if (L.kind == LimitOutcome::Finite && !L.value) return numericFallback(e, v, a, dir);
        if (L.kind == LimitOutcome::PosInf) ++posInf;
        else if (L.kind == LimitOutcome::NegInf) ++negInf;
        else if (L.kind == LimitOutcome::Finite) finite.push_back(L.value);
        else undef = true;
    }
    if (undef || (posInf > 0 && negInf > 0)) {
        return rewriteOrNumeric(e, v, a, dir, rules, depth);
    }
    if (posInf > 0) {
        addRule(rules, "sum");
        return mkInf(1, "a term grows without bound");
    }
    if (negInf > 0) {
        addRule(rules, "sum");
        return mkInf(-1, "a term decreases without bound");
    }
    addRule(rules, "sum");
    try {
        return mkFinite(simplify(Add(finite)));
    } catch (...) {
        return rewriteOrNumeric(e, v, a, dir, rules, depth);
    }
}

LimitOutcome mulLimit(const Ex& e, const std::string& v, const Ex& a, int dir,
                      std::vector<std::string>* rules, int depth) {
    std::vector<Ex> num, den;
    splitFraction(e, num, den);
    if (!den.empty()) {
        Ex N = num.empty() ? Num(1) : Mul(num);
        Ex D = den.size() == 1 ? den[0] : Mul(den);
        return quotientLimit(N, D, v, a, dir, rules, depth);
    }

    std::vector<Ex> finite;
    std::vector<Ex> zeroExprs, infExprs, undefExprs;
    int infSign = 0;       // accumulated sign of the infinite factors
    bool hasInf = false;
    bool hasUndef = false;
    for (const auto& f : e->args) {
        LimitOutcome L = limitRec(f, v, a, dir, rules, depth + 1);
        if (L.kind == LimitOutcome::Finite && !L.value) return numericFallback(e, v, a, dir);
        if (L.kind == LimitOutcome::Finite) {
            finite.push_back(L.value);
            if (isZero(L.value)) zeroExprs.push_back(f);
        } else if (L.kind == LimitOutcome::Undefined) {
            hasUndef = true;
            undefExprs.push_back(f);
        } else {
            hasInf = true;
            infExprs.push_back(f);
            const int s = L.kind == LimitOutcome::NegInf ? -1 : 1;
            // an infinite factor flips the overall sign when it is negative
            if (infSign == 0)
                infSign = s;
            else
                infSign *= s;
        }
    }
    if (hasUndef) {
        // Squeeze law: a bounded oscillating factor multiplied by something that
        // tends to zero has limit zero.
        bool anyZero = false;
        for (const auto& f : finite)
            if (isZero(f)) anyZero = true;
        if (anyZero) {
            bool allBounded = true;
            for (const auto& f : undefExprs)
                if (!boundedNear(f, v, a, dir)) allBounded = false;
            if (allBounded) {
                addRule(rules, "squeeze");
                return mkFinite(Num(0));
            }
        }
        return rewriteOrNumeric(e, v, a, dir, rules, depth);
    }

    const bool zeroFactor = !zeroExprs.empty();
    const Ex zeroFactorExpr = zeroExprs.empty() ? Ex() : (zeroExprs.size() == 1 ? zeroExprs[0] : Mul(zeroExprs));
    const Ex infFactorExpr = infExprs.empty() ? Ex() : (infExprs.size() == 1 ? infExprs[0] : Mul(infExprs));

    if (hasInf && zeroFactor) {
        // 0 · infinity: write it as a quotient and let L'Hôpital handle it. Both
        // placements are tried because only one of them usually simplifies.
        addRule(rules, "algebraic_simplification");
        std::vector<LimitOutcome> attempts;
        if (zeroFactorExpr && infFactorExpr) {
            attempts.push_back(quotientLimit(zeroFactorExpr, Pow(infFactorExpr, Num(-1)), v, a, dir,
                                             rules, depth + 1));
        }
        if (attempts.empty() || attempts.back().kind == LimitOutcome::Undefined) {
            if (infFactorExpr && zeroFactorExpr)
                attempts.push_back(quotientLimit(infFactorExpr, Pow(zeroFactorExpr, Num(-1)), v, a,
                                                 dir, rules, depth + 1));
        }
        for (const auto& attempt : attempts)
            if (attempt.kind != LimitOutcome::Undefined) return attempt;
        return numericFallback(e, v, a, dir);
    }
    if (hasInf) {
        // Sign of the infinite product also depends on the numeric sign of the
        // finite factors.
        int sign = infSign;
        for (const auto& f : finite) {
            bool ok = false;
            const double d = evalReal(f, {}, &ok);
            if (ok && d < 0) sign = -sign;
        }
        addRule(rules, "product");
        return mkInf(sign, "a factor grows without bound");
    }
    addRule(rules, "product");
    try {
        return mkFinite(simplify(Mul(finite)));
    } catch (...) {
        return rewriteOrNumeric(e, v, a, dir, rules, depth);
    }
}

LimitOutcome limitRec(const Ex& e, const std::string& v, const Ex& a, int dir,
                      std::vector<std::string>* rules, int depth) {
    if (!e) return mkUndef("empty expression");
    if (depth > 8) return numericFallback(e, v, a, dir);
    try {
        if (!dependsOn(e, v)) {
            addRule(rules, "constant");
            return mkFinite(simplify(e));
        }
        if (e->isSym()) {
            // e must be v here
            int sign = 1;
            if (infinitySign(a, &sign)) {
                addRule(rules, "identity");
                return mkInf(sign, "the variable itself runs to infinity");
            }
            addRule(rules, "identity");
            return mkFinite(a);
        }
        if (e->isAdd()) return addLimit(e, v, a, dir, rules, depth);
        if (e->isMul()) return mulLimit(e, v, a, dir, rules, depth);
        if (e->isPow()) return powLimit(e->args[0], e->args[1], v, a, dir, rules, depth);
        if (e->isFunc()) return funcLimit(e, v, a, dir, rules, depth);
        if (e->isNum()) {
            addRule(rules, "constant");
            return mkFinite(e);
        }
    } catch (...) {
    }
    return rewriteOrNumeric(e, v, a, dir, rules, depth);
}

LimitOutcome limitUndirected(const Ex& f, const std::string& v, const Ex& a, int dir,
                             std::vector<std::string>* rules) {
    return limitRec(f, v, a, dir, rules, 0);
}

bool limitEqual(const LimitOutcome& x, const LimitOutcome& y) {
    if (x.kind != y.kind) return false;
    if (x.kind != LimitOutcome::Finite) return true;
    if (!x.value || !y.value) return false;
    if (sameExpr(x.value, y.value)) return true;
    bool ok = false;
    try {
        ok = numericallyEqual(x.value, y.value, {}, 6, 1e-9);
    } catch (...) {
        ok = false;
    }
    return ok;
}

} // namespace

bool isInfinityPoint(const Ex& point, int* sign) { return infinitySign(point, sign); }

Ex infinityPoint(int sign) {
    Ex inf = Sym("inf");
    return sign < 0 ? Mul(Num(-1), inf) : inf;
}

LimitOutcome limitOf(const Ex& f, const std::string& v, const Ex& point, int dir,
                     std::vector<std::string>* rules) {
    int sign = 1;
    if (infinitySign(point, &sign)) {
        LimitOutcome r = limitUndirected(f, v, point, sign, rules);
        if (r.kind == LimitOutcome::Finite && !r.value && !r.numeric)
            return mkUndef("the limit could not be evaluated");
        return r;
    }
    if (dir != 0) {
        LimitOutcome r = limitUndirected(f, v, point, dir, rules);
        return r;
    }
    LimitOutcome left = limitUndirected(f, v, point, -1, rules);
    LimitOutcome right = limitUndirected(f, v, point, 1, rules);
    LimitOutcome out;
    if (left.kind == LimitOutcome::Finite && right.kind == LimitOutcome::Finite &&
        limitEqual(left, right)) {
        addRule(rules, "two_sided");
        out = left;
        if (right.numeric && !left.numeric) out = right;
        return out;
    }
    if (left.kind == LimitOutcome::PosInf && right.kind == LimitOutcome::PosInf) {
        addRule(rules, "two_sided");
        return mkInf(1, "both one-sided limits are +infinity");
    }
    if (left.kind == LimitOutcome::NegInf && right.kind == LimitOutcome::NegInf) {
        addRule(rules, "two_sided");
        return mkInf(-1, "both one-sided limits are -infinity");
    }
    if (left.kind == LimitOutcome::Undefined && right.kind == LimitOutcome::Undefined) {
        out = mkUndef(left.reason.empty() ? "the limit does not exist" : left.reason);
        return out;
    }
    return mkUndef("the left and right limits differ");
}

ContinuityOutcome continuityOf(const Ex& f, const std::string& v, const Ex& point,
                               std::vector<std::string>* rules) {
    ContinuityOutcome c;
    c.left = limitOf(f, v, point, -1, rules);
    c.right = limitOf(f, v, point, 1, rules);

    // f(point)
    try {
        Ex val = simplify(substitute(f, v, point));
        if (!dependsOn(val, v)) {
            bool ok = false;
            const double d = evalReal(val, {}, &ok);
            if (ok && std::isfinite(d) && std::fabs(d) < 1e12) {
                c.valueDefined = true;
                c.value = val;
            }
        }
    } catch (...) {
        c.valueDefined = false;
    }

    const bool bothFinite = c.left.kind == LimitOutcome::Finite &&
                            c.right.kind == LimitOutcome::Finite;
    const bool sameTwoSided = bothFinite && limitEqual(c.left, c.right);
    const bool infiniteSide = c.left.kind == LimitOutcome::PosInf ||
                              c.left.kind == LimitOutcome::NegInf ||
                              c.right.kind == LimitOutcome::PosInf ||
                              c.right.kind == LimitOutcome::NegInf;

    if (bothFinite && sameTwoSided && c.valueDefined && limitEqual(c.left, mkFinite(c.value))) {
        c.continuous = true;
        c.kind = "continuous";
        c.note = "Left limit, right limit and the value of the function all agree.";
        return c;
    }
    if (bothFinite && sameTwoSided && (!c.valueDefined || !limitEqual(c.left, mkFinite(c.value)))) {
        c.kind = "removable";
        c.note = c.valueDefined
                     ? "The two-sided limit exists but differs from the value of the function."
                     : "The two-sided limit exists but the function is not defined at the point.";
        return c;
    }
    if (bothFinite && !sameTwoSided) {
        c.kind = "jump";
        c.note = "The left and right limits exist but are different.";
        return c;
    }
    if (infiniteSide) {
        c.kind = "infinite";
        c.note = "At least one one-sided limit is infinite (vertical asymptote).";
        return c;
    }
    c.kind = "oscillatory";
    c.note = "At least one one-sided limit does not exist.";
    return c;
}

} // namespace ode
