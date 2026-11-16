// Infinit Lab — differentiation and exact integration
// SPDX-License-Identifier: Apache-2.0
#include "expr.hpp"
#include "poly.hpp"
#include "numeric.hpp"
#include <functional>
#include <set>

namespace ode {

bool polyCoeffs(const Ex& a, const std::string& v, std::vector<Ex>& out);

// ---------------------------------------------------------------- differentiation
Ex diff(const Ex& a, const std::string& v) {
    if (!a) return Num(0);
    switch (a->kind) {
        case Kind::Num: return Num(0);
        case Kind::Sym: return a->isSym(v) ? Num(1) : Num(0);
        case Kind::Add: {
            std::vector<Ex> t;
            for (auto& x : a->args) t.push_back(diff(x, v));
            return Add(t);
        }
        case Kind::Mul: {
            std::vector<Ex> terms;
            for (size_t i = 0; i < a->args.size(); ++i) {
                if (!dependsOn(a->args[i], v)) continue;
                std::vector<Ex> fs = a->args;
                fs[i] = diff(fs[i], v);
                terms.push_back(Mul(fs));
            }
            return Add(terms);
        }
        case Kind::Pow: {
            const Ex& b = a->args[0];
            const Ex& e = a->args[1];
            bool bc = !dependsOn(b, v), ec = !dependsOn(e, v);
            if (ec) return Mul(e, Pow(b, Add(e, Num(-1))), diff(b, v));
            if (bc) return Mul(a, Fn("ln", {b}), diff(e, v));
            return Mul(a, Add(Mul(diff(e, v), Fn("ln", {b})), Mul(e, diff(b, v), Pow(b, Num(-1)))));
        }
        case Kind::Func: {
            if (a->name == "Deriv") {
                if (a->args[1]->isSym(v))
                    return Deriv(a->args[0], v, derivOrder(a) + 1);
                return Num(0);
            }
            if (a->name == "Integral" && a->args.size() == 2 && a->args[1]->isSym(v))
                return a->args[0];
            const Ex& u = a->args[0];
            Ex du = diff(u, v);
            if (du->isZero()) return Num(0);
            const std::string& f = a->name;
            Ex outer;
            if (f == "sin") outer = Fn("cos", {u});
            else if (f == "cos") outer = Mul(Num(-1), Fn("sin", {u}));
            else if (f == "tan") outer = Pow(Fn("cos", {u}), Num(-2));
            else if (f == "cot") outer = Mul(Num(-1), Pow(Fn("sin", {u}), Num(-2)));
            else if (f == "sec") outer = Mul(Fn("sec", {u}), Fn("tan", {u}));
            else if (f == "csc") outer = Mul(Num(-1), Fn("csc", {u}), Fn("cot", {u}));
            else if (f == "asin") outer = Pow(Add(Num(1), Mul(Num(-1), Pow(u, Num(2)))), Num(Rational(-1, 2)));
            else if (f == "acos") outer = Mul(Num(-1), Pow(Add(Num(1), Mul(Num(-1), Pow(u, Num(2)))), Num(Rational(-1, 2))));
            else if (f == "atan") outer = Pow(Add(Num(1), Pow(u, Num(2))), Num(-1));
            else if (f == "sinh") outer = Fn("cosh", {u});
            else if (f == "cosh") outer = Fn("sinh", {u});
            else if (f == "tanh") outer = Pow(Fn("cosh", {u}), Num(-2));
            else if (f == "asinh") outer = Pow(Add(Pow(u, Num(2)), Num(1)), Num(Rational(-1, 2)));
            else if (f == "acosh") outer = Mul(Pow(Add(u, Num(-1)), Num(Rational(-1, 2))), Pow(Add(u, Num(1)), Num(Rational(-1, 2))));
            else if (f == "atanh") outer = Pow(Add(Num(1), Mul(Num(-1), Pow(u, Num(2)))), Num(-1));
            else if (f == "ln" || f == "log") {
                if (u->isFunc("abs")) return Mul(diff(u->args[0], v), Pow(u->args[0], Num(-1)));
                outer = Pow(u, Num(-1));
            }
            else if (f == "exp") outer = Fn("exp", {u});
            else if (f == "erf")
                outer = Mul(Num(2), Pow(Sym("pi"), Num(Rational(-1, 2))),
                            Fn("exp", {Mul(Num(-1), Pow(u, Num(2)))}));
            else if (f == "erfc")
                outer = Mul(Num(-2), Pow(Sym("pi"), Num(Rational(-1, 2))),
                            Fn("exp", {Mul(Num(-1), Pow(u, Num(2)))}));
            else if (f == "abs") {
                return Mul(du, u, Pow(a, Num(-1)));
            } else if (f == "u") {
                // distributional derivative of the unit step
                return Mul(Fn("delta", {u}), du);
            } else if (f == "delta") {
                return Num(0);
            } else {
                // unknown function: formal derivative
                return Deriv(Fn(f, a->args), v, 1);
            }
            return Mul(outer, du);
        }
    }
    return Num(0);
}

// ---------------------------------------------------------------- helpers
static bool linearIn(const Ex& e, const std::string& x, Ex& a, Ex& b) {
    Ex ex = expand(e);
    Ex d = simplify(diff(ex, x));
    if (dependsOn(d, x) || d->isZero()) return false;
    Ex b0 = simplify(substitute(ex, x, Num(0)));
    if (dependsOn(b0, x)) {
        // e might contain e.g. abs(x); reject
        return false;
    }
    a = d; b = b0;
    return true;
}

static Ex Integral(const Ex& f, const std::string& x) {
    return Fn("Integral", {f, Sym(x)});
}
static Ex lnAbs(const Ex& u) { return Fn("ln", {Fn("abs", {u})}); }


static bool hasIntegral(const Ex& a) {
    if (!a) return false;
    if (a->isFunc("Integral")) return true;
    for (auto& x : a->args) if (hasIntegral(x)) return true;
    return false;
}

static Ex substituteExpr(const Ex& a, const Ex& target, const Ex& rep) {
    if (equals(a, target)) return rep;
    if (a->args.empty()) return a;
    std::vector<Ex> v;
    for (auto& x : a->args) v.push_back(substituteExpr(x, target, rep));
    switch (a->kind) {
        case Kind::Add: return Add(v);
        case Kind::Mul: return Mul(v);
        case Kind::Pow: return Pow(v[0], v[1]);
        case Kind::Func: return Fn(a->name, v);
        default: return a;
    }
}

// sec, csc, tan and cot are quotients of sin and cos.  A product such as
// sin(x) sec(x)^3 has no table entry, but after the rewrite it is
// sin(x)/cos(x)^3, which the substitution rule integrates.  The pass leaves
// every other node untouched.
static Ex rewriteQuotients(const Ex& a) {
    if (!a) return a;
    if (a->isFunc() && a->args.size() == 1) {
        Ex u = rewriteQuotients(a->args[0]);
        const std::string& n = a->name;
        if (n == "sec") return Pow(Fn("cos", {u}), Num(-1));
        if (n == "csc") return Pow(Fn("sin", {u}), Num(-1));
        if (n == "tan") return Mul(Fn("sin", {u}), Pow(Fn("cos", {u}), Num(-1)));
        if (n == "cot") return Mul(Fn("cos", {u}), Pow(Fn("sin", {u}), Num(-1)));
        return Fn(n, {u});
    }
    if (a->args.empty()) return a;
    std::vector<Ex> v;
    for (auto& c : a->args) v.push_back(rewriteQuotients(c));
    switch (a->kind) {
        case Kind::Add: return Add(v);
        case Kind::Mul: return Mul(v);
        case Kind::Pow: return Pow(v[0], v[1]);
        case Kind::Func: return Fn(a->name, v);
        default: return a;
    }
}

static void collectSubCandidates(const Ex& a, const Ex& whole, std::vector<Ex>& out) {
    if (!a || a->args.empty()) return;
    if (!equals(a, whole)) {
        bool dup = false;
        for (auto& c : out) if (equals(c, a)) { dup = true; break; }
        if (!dup) out.push_back(a);
    }
    for (auto& x : a->args) collectSubCandidates(x, whole, out);
}

struct Integrator {
    std::string x;
    int depth = 0;
    std::string method;

    Ex run(const Ex& f0) {
        try {
            return runInner(f0);
        } catch (const std::exception&) {
            return Integral(f0, x);
        }
    }

    Ex runInner(const Ex& f) {
        if (!f || f->isZero()) return Num(0);
        // Substitution and parts recurse through this entry point.  Depth alone
        // is not enough: each level can expand the expression, so a modest
        // depth can still produce an astronomical simplify() call.  Bail out
        // with an unevaluated integral instead of hanging the solver.
        if (depth >= 12 || sizeOf(f) > 1200) return Integral(f, x);
        ++depth;
        Ex res = attempt(f);
        --depth;
        return res;
    }

    Ex attempt(const Ex& f) {
        if (!dependsOn(f, x)) {
            method = "constant";
            return Mul(f, Sym(x));
        }

        // Expand polynomial products before the table/rational passes:
        // ∫ p(p-1) dp must be integrated term by term.
        {
            Ex fe = expand(f);
            if (!equals(fe, f) && fe->isAdd() && sizeOf(fe) < 4000) {
                Ex r = run(fe);
                if (!hasIntegral(r)) {
                    method = "expand";
                    return r;
                }
            }
        }

        // additive linearity
        if (f->isAdd()) {
            std::vector<Ex> v;
            for (auto& t : f->args) v.push_back(run(t));
            method = "sum";
            return Add(v);
        }

        // pull out constant (in x) factors
        if (f->isMul()) {
            std::vector<Ex> cst, rest;
            for (auto& t : f->args) {
                if (dependsOn(t, x)) rest.push_back(t);
                else cst.push_back(t);
            }
            if (!cst.empty() && !rest.empty()) {
                Ex r = rest.size() == 1 ? rest[0] : Mul(rest);
                Ex c = cst.size() == 1 ? cst[0] : Mul(cst);
                return Mul(c, run(r));
            }
        }

        // elementary table
        Ex T = table(f);
        if (T) { method = "table"; return T; }

        // rational function
        {
            Ex g = tryRational(f);
            if (g) { method = "rational"; return g; }
        }
        // rational function with symbolic constants (e.g. √3 coefficients)
        {
            Ex g = trySymbolicQuadratic(f);
            if (g) { method = "rational"; return g; }
        }

        // u-substitution
        {
            Ex g = trySubstitution(f);
            if (g) { method = "substitution"; return g; }
        }

        // Rationalise sqrt(a + b x): with u = sqrt(a + b x) the differential
    // becomes 2u/b du, so an integrand that is rational in u is elementary.
    {
        Ex g = trySqrtRationalize(f);
        if (g) { method = "substitution"; return g; }
    }

    // exponential substitution t = e^x (sech, logistic-type quotients)
        {
            Ex g = tryExpSubstitution(f);
            if (g) { method = "substitution"; return g; }
        }

        // trigonometric quotients: sec -> 1/cos, tan -> sin/cos, ...
        {
            Ex rw = rewriteQuotients(f);
            if (rw && !equals(rw, f)) {
                Ex g = run(rw);
                if (!hasIntegral(g)) { method = "rewrite"; return g; }
            }
        }

        // trigonometric powers
        {
            Ex g = tryTrigPower(f);
            if (g) { method = "trig"; return g; }
        }

        // double-angle rewrite: sin(2u) = 2 sin u cos u, etc.
        {
            Ex g = tryDoubleAngle(f);
            if (g) { method = "trig"; return g; }
        }

        // integration by parts
        {
            Ex g = tryParts(f);
            if (g) { method = "parts"; return g; }
        }

        return Integral(f, x);
    }

    Ex table(const Ex& f) {
        Ex X = Sym(x);
        if (f->isSym(x)) return Mul(Num(Rational(1, 2)), Pow(X, Num(2)));

        // ∫ e^(a x) sin(b x) dx and ∫ e^(a x) cos(b x) dx.
        if (f->isMul()) {
            Ex expArg, trigArg;
            std::string trigName;
            for (auto& q : f->args) {
                if (q->isFunc("exp") && q->args.size() == 1) expArg = q->args[0];
                if ((q->isFunc("sin") || q->isFunc("cos")) && q->args.size() == 1) {
                    trigName = q->name;
                    trigArg = q->args[0];
                }
            }
            if (expArg && !trigName.empty()) {
                Ex a, b0, b, d0;
                if (linearIn(expArg, x, a, b0) && a->isNum() && b0->isZero() &&
                    linearIn(trigArg, x, b, d0) && b->isNum() && d0->isZero()) {
                    Rational aa = a->num, bb = b->num;
                    Rational den = aa * aa + bb * bb;
                    if (!den.isZero()) {
                        Ex e = Fn("exp", {expArg});
                        Ex sn = Fn("sin", {trigArg});
                        Ex cs = Fn("cos", {trigArg});
                        if (trigName == "sin")
                            return Mul(e, Pow(Num(den), Num(-1)),
                                       Add(Mul(Num(aa), sn), Mul(Num(-bb), cs)));
                        return Mul(e, Pow(Num(den), Num(-1)),
                                   Add(Mul(Num(aa), cs), Mul(Num(bb), sn)));
                    }
                }
            }
        }

        // Products of two trigonometric factors that collapse to a single
        // elementary term, e.g. sin(x) sec(x) = tan(x) and
        // cos(x) cot(x) = csc(x) - sin(x).
        if (f->isMul()) {
            Ex u1, u2, coeff = Num(1);
            std::string n1, n2;
            int funcCount = 0;
            bool mixable = true;
            for (auto& q : f->args) {
                const bool trig = q->isFunc() && q->args.size() == 1 &&
                                  (q->name == "sin" || q->name == "cos" || q->name == "tan" ||
                                   q->name == "sec" || q->name == "csc" || q->name == "cot");
                if (trig) {
                    if (funcCount == 0) { u1 = q->args[0]; n1 = q->name; }
                    else if (funcCount == 1) { u2 = q->args[0]; n2 = q->name; }
                    else { mixable = false; break; }
                    ++funcCount;
                } else if (!dependsOn(q, x)) {
                    coeff = Mul(coeff, q);
                } else {
                    mixable = false;
                    break;
                }
            }
            if (mixable && funcCount == 2 && equals(u1, u2)) {
                Ex a, b;
                if (linearIn(u1, x, a, b) && a->isNum()) {
                    const Rational k = a->num;
                    const Ex u = u1;
                    const std::string pair = n1 < n2 ? n1 + "*" + n2 : n2 + "*" + n1;
                    const Ex logCos = Fn("ln", {Fn("abs", {Fn("cos", {u})})});
                    const Ex logSin = Fn("ln", {Fn("abs", {Fn("sin", {u})})});
                    const Ex logSec = Fn("ln", {Fn("abs", {Add(Fn("sec", {u}), Fn("tan", {u}))})});
                    const Ex logCsc = Fn("ln", {Fn("abs", {Add(Fn("csc", {u}), Fn("cot", {u}))})});
                    Ex antiderivative;
                    if (pair == "cos*sec" || pair == "csc*sin") {
                        // The product is exactly 1, so the integral is x and
                        // must not inherit the 1/k factor used by the other
                        // entries below.
                        antiderivative = Mul(Num(k), Sym(x));
                    } else if (pair == "sec*sin") {
                        antiderivative = Mul(Num(-1), logCos);
                    } else if (pair == "cos*csc") {
                        antiderivative = logSin;
                    } else if (pair == "sec*tan") {
                        antiderivative = Fn("sec", {u});
                    } else if (pair == "cot*csc" || pair == "csc*cot") {
                        antiderivative = Mul(Num(-1), Fn("csc", {u}));
                    } else if (pair == "sin*tan") {
                        antiderivative = Add(logSec, Mul(Num(-1), Fn("sin", {u})));
                    } else if (pair == "cos*tan") {
                        antiderivative = Mul(Num(-1), Fn("cos", {u}));
                    } else if (pair == "cot*sin") {
                        antiderivative = Fn("sin", {u});
                    } else if (pair == "cos*cot") {
                        antiderivative = Add(Mul(Num(-1), logCsc), Fn("cos", {u}));
                    }
                    if (antiderivative)
                        return Mul(coeff, Num(k.inv()), antiderivative);
                }
            }
        }

        if (f->isPow()) {
            Ex b = f->args[0], e = f->args[1];
            // Reciprocal trigonometric powers: sec, csc, sec², csc².
            if (b->isFunc() && e->isNum() && e->num.isInt() && e->num.n < 0) {
                Ex la, lb;
                if (linearIn(b->args[0], x, la, lb) && la->isNum()) {
                    Ex u = b->args[0];
                    Rational kk = la->num;
                    long long p = e->num.n;
                    if (b->name == "cos" && p == -1)
                        return Mul(Num(kk.inv()),
                                   Fn("ln", {Fn("abs", {Add(Fn("sec", {u}), Fn("tan", {u}))})}));
                    if (b->name == "sin" && p == -1)
                        return Mul(Num(-kk.inv()),
                                   Fn("ln", {Fn("abs", {Add(Fn("csc", {u}), Fn("cot", {u}))})}));
                    if (b->name == "cos" && p == -2) return Mul(Num(kk.inv()), Fn("tan", {u}));
                    if (b->name == "sin" && p == -2) return Mul(Num(-kk.inv()), Fn("cot", {u}));
                    // tan^(-1) = cot, cot^(-1) = tan
                    if (b->name == "tan" && p == -1)
                        return Mul(Num(kk.inv()), Fn("ln", {Fn("abs", {Fn("sin", {u})})}));
                    if (b->name == "cot" && p == -1)
                        return Mul(Num(-kk.inv()), Fn("ln", {Fn("abs", {Fn("cos", {u})})}));
                }
            }
            if (!dependsOn(e, x) && e->isNum()) {
                Rational n = e->num;
                if (b->isSym(x)) {
                    if (n == Rational(-1)) return Fn("ln", {Fn("abs", {X})});
                    Rational np = n + Rational(1);
                    return Mul(Num(np.inv()), Pow(X, Num(np)));
                }
                Ex a, bb;
                if (linearIn(b, x, a, bb) && a->isNum()) {
                    if (n == Rational(-1))
                        return Mul(Num(a->num.inv()), lnAbs(b));
                    Rational np = n + Rational(1);
                    return Mul(Pow(b, Num(np)), Pow(Num(a->num * np), Num(-1)));
                }
            }
            // sqrt of quadratic: sqrt(a x^2 + b x + c)
            if (e->isNum() && e->num == Rational(1, 2)) {
                Ex a, bq, c;
                if (quadraticIn(f->args[0], x, a, bq, c)) {
                    Ex g = integrateSqrtQuad(a, bq, c);
                    if (g) return g;
                }
            }
            if (e->isNum() && e->num == Rational(-1, 2)) {
                Ex a, bq, c;
                if (quadraticIn(f->args[0], x, a, bq, c)) {
                    Ex g = integrateInvSqrtQuad(a, bq, c);
                    if (g) return g;
                }
            }
        }

        if (f->isFunc()) {
            const std::string& nm = f->name;
            Ex a, b;
            if ((nm == "exp" || nm == "sin" || nm == "cos" || nm == "sinh" || nm == "cosh" || nm == "tanh")
                && linearIn(f->args[0], x, a, b) && a->isNum()) {
                Rational k = a->num;
                Ex u = f->args[0];
                if (nm == "exp") return Mul(Num(k.inv()), f);
                if (nm == "sin") return Mul(Num(-k.inv()), Fn("cos", {u}));
                if (nm == "cos") return Mul(Num(k.inv()), Fn("sin", {u}));
                if (nm == "sinh") return Mul(Num(k.inv()), Fn("cosh", {u}));
                if (nm == "cosh") return Mul(Num(k.inv()), Fn("sinh", {u}));
                if (nm == "tanh") return Mul(Num(k.inv()), Fn("ln", {Fn("cosh", {u})}));
            }
            if ((nm == "tan" || nm == "cot" || nm == "sec" || nm == "csc") &&
                linearIn(f->args[0], x, a, b) && a->isNum()) {
                Rational k = a->num;
                Ex u = f->args[0];
                if (nm == "tan") return Mul(Num(-k.inv()), Fn("ln", {Fn("abs", {Fn("cos", {u})})}));
                if (nm == "cot") return Mul(Num(k.inv()), Fn("ln", {Fn("abs", {Fn("sin", {u})})}));
                if (nm == "sec")
                    return Mul(Num(k.inv()), Fn("ln", {Fn("abs", {Add(Fn("sec", {u}), Fn("tan", {u}))})}));
                if (nm == "csc")
                    return Mul(Num(-k.inv()), Fn("ln", {Fn("abs", {Add(Fn("csc", {u}), Fn("cot", {u}))})}));
            }
            // Gaussian: ∫ exp(-α x² + c) dx = √(π/α)/2 · e^c · erf(√α x)
            if (nm == "exp") {
                Ex qa, qb, qc;
                if (quadraticIn(f->args[0], x, qa, qb, qc) && qa->isNum() && qb->isNum() && qc->isNum()) {
                    Rational A = qa->num, B = qb->num, C = qc->num;
                    if (B.isZero() && A.isNeg()) {
                        Rational alpha(-A.n, A.d);
                        Ex rootA = Sqrt(Num(alpha));
                        Ex pref = Mul(Sqrt(Sym("pi")), Num(Rational(1, 2)), Pow(rootA, Num(-1)));
                        if (!C.isZero()) pref = Mul(pref, Fn("exp", {Num(C)}));
                        return Mul(pref, Fn("erf", {Mul(rootA, Sym(x))}));
                    }
                }
            }
            if (nm == "ln" && linearIn(f->args[0], x, a, b) && a->isNum()) {
                Ex u = f->args[0];
                return Mul(Num(a->num.inv()), Add(Mul(u, Fn("ln", {u})), Mul(Num(-1), u)));
            }
            if (nm == "atan" && linearIn(f->args[0], x, a, b) && a->isNum()) {
                Ex u = f->args[0];
                return Mul(Num(a->num.inv()), Add(Mul(u, Fn("atan", {u})),
                                                  Mul(Num(Rational(-1, 2)), Fn("ln", {Add(Num(1), Pow(u, Num(2))) }))));
            }
            if (nm == "asin" && linearIn(f->args[0], x, a, b) && a->isNum()) {
                Ex u = f->args[0];
                return Mul(Num(a->num.inv()), Add(Mul(u, Fn("asin", {u})),
                                                  Sqrt(Add(Num(1), Mul(Num(-1), Pow(u, Num(2)))))));
            }
        }

        // quadratic denominator with linear numerator handled by rational
        // sin^2 / cos^2 / tan^2
        if (f->isPow() && f->args[1]->isNum() && f->args[1]->num.n == 2) {
            Ex a, b;
            const Ex& base = f->args[0];
            if (base->isFunc() && linearIn(base->args[0], x, a, b) && a->isNum()) {
                Ex u = base->args[0];
                Rational k = a->num;
                if (base->name == "sin")
                    return Add(Mul(Num(Rational(1, 2)), X), Mul(Num(-1), Pow(Num(4), Num(-1)), Pow(Num(k), Num(-1)), Fn("sin", {Mul(Num(2), u)})));
                if (base->name == "cos")
                    return Add(Mul(Num(Rational(1, 2)), X), Mul(Pow(Num(4), Num(-1)), Pow(Num(k), Num(-1)), Fn("sin", {Mul(Num(2), u)})));
                if (base->name == "tan")
                    return Add(Mul(Pow(Num(k), Num(-1)), Fn("tan", {u})), Mul(Num(-1), X));
                if (base->name == "cot")
                    return Add(Mul(Num(-k.inv()), Fn("cot", {u})), Mul(Num(-1), X));
                if (base->name == "sec")
                    return Mul(Num(k.inv()), Fn("tan", {u}));
                if (base->name == "csc")
                    return Mul(Num(-k.inv()), Fn("cot", {u}));
            }
        }
        // ∫ sec³u du and ∫ csc³u du: the standard reduction formulas.
        if (f->isPow() && f->args[1]->isNum() && f->args[1]->num.n == 3) {
            const Ex& base = f->args[0];
            Ex a, b;
            if (base->isFunc() && linearIn(base->args[0], x, a, b) && a->isNum()) {
                Ex u = base->args[0];
                Rational k = a->num;
                const Ex logSec = Fn("ln", {Fn("abs", {Add(Fn("sec", {u}), Fn("tan", {u}))})});
                const Ex logCsc = Fn("ln", {Fn("abs", {Add(Fn("csc", {u}), Fn("cot", {u}))})});
                if (base->name == "sec")
                    return Mul(Num(Rational(1, 2) * k.inv()),
                               Add({Mul(Fn("sec", {u}), Fn("tan", {u})), logSec}));
                if (base->name == "csc")
                    return Mul(Num(Rational(-1, 2) * k.inv()),
                               Add({Mul(Fn("csc", {u}), Fn("cot", {u})), logCsc}));
            }
        }
        return nullptr;
    }

    bool quadraticIn(const Ex& e, const std::string& v, Ex& a, Ex& b, Ex& c) {
        std::vector<Ex> cs;
        if (!polyCoeffs(expand(e), v, cs)) return false;
        if (cs.size() != 3) return false;
        a = cs[2]; b = cs[1]; c = cs[0];
        if (!a->isNum() || !b->isNum() || !c->isNum()) return false;
        if (a->isZero()) return false;
        return true;
    }

    Ex integrateInvSqrtQuad(const Ex& a, const Ex& b, const Ex& c) {
        Rational A = a->num, B = b->num, C = c->num;
        Ex X = Sym(x);
        Rational disc = B * B - A * C * Rational(4);
        Ex u = Add(Add(Mul(Num(Rational(2 * A.n, A.d)), X), Num(B)), Num(0));
        if (A.isZero()) return nullptr;
        if (A.isNeg()) {
            if (disc.isNeg()) return nullptr;
            Ex s = Sqrt(Num(disc));
            Ex arg = Mul(u, Pow(s, Num(-1)));
            return Mul(Num(-1), Pow(Sqrt(Num(-A)), Num(-1)), Fn("asin", {arg}));
        }
        if (disc.isNeg()) {
            Ex s = Sqrt(Num(-disc));
            Ex arg = Mul(u, Pow(s, Num(-1)));
            return Mul(Pow(Sqrt(Num(A)), Num(-1)), Fn("asinh", {arg}));
        }
        if (disc.isZero()) {
            return Mul(Pow(Sqrt(Num(A)), Num(-1)), Fn("ln", {Fn("abs", {u})}));
        }
        // ∫ dx/√(Ax²+Bx+C) = (1/√A)·ln|2Ax+B + 2√(A·Q)|.  Using
        // 2Ax+B + √disc instead is only asymptotically right and was wrong
        // for finite x.
        Ex Q = Add({Mul(a, Pow(X, Num(2))), Mul(b, X), c});
        Ex arg = Add(u, Mul(Num(2), Sqrt(Num(A)), Sqrt(Q)));
        return Mul(Pow(Sqrt(Num(A)), Num(-1)), Fn("ln", {Fn("abs", {arg})}));
    }

    Ex integrateSqrtQuad(const Ex& a, const Ex& b, const Ex& c) {
        Rational A = a->num, B = b->num, C = c->num;
        Ex X = Sym(x);
        Ex Q = Add({Mul(Num(A), Pow(X, Num(2))), Mul(Num(B), X), Num(C)});
        Rational disc = B * B - A * C * Rational(4);
        Ex inv = integrateInvSqrtQuad(a, b, c);
        if (!inv) return nullptr;
        Ex first = Mul(Add(Mul(Num(Rational(2 * A.n, A.d)), X), Num(B)), Sqrt(Q), Pow(Num(A * Rational(4)), Num(-1)));
        Ex second = Mul(Num(-disc), Pow(Num(A * Rational(8)), Num(-1)), inv);
        return Add(first, second);
    }

    Ex tryRational(const Ex& f) {
        // together & split into numerator/denominator
        Ex t = together(f);
        Ex num = t, den = Num(1);
        if (t->isMul()) {
            std::vector<Ex> nn, dd;
            for (auto& q : t->args) {
                if (q->isPow() && q->args[1]->isNum() && q->args[1]->num.isNeg())
                    dd.push_back(Pow(q->args[0], Num(-q->args[1]->num)));
                else nn.push_back(q);
            }
            if (!dd.empty()) {
                num = nn.empty() ? Num(1) : Mul(nn);
                den = dd.size() == 1 ? dd[0] : Mul(dd);
            }
        } else if (t->isPow() && t->args[1]->isNum() && t->args[1]->num.isNeg()) {
            num = Num(1);
            den = Pow(t->args[0], Num(-t->args[1]->num));
        } else if (t->isNum() && t->num.d != 1) {
            num = Num(Rational(t->num.n, 1));
            den = Num(t->num.d);
        }
        Poly N, D;
        if (!toPoly(num, x, N) || !toPoly(den, x, D)) return nullptr;
        if (D.deg() < 1) return nullptr;
        // avoid duplicating simple power rule: 1/(linear) etc handled already, but fine
        Ex r = integrateRational(num, den, x);
        if (r && !hasIntegral(r)) return r;
        return nullptr;
    }

    // ∫ (A + B v)/(a + b v + c v²) dv with coefficients that may contain
    // constants such as √3.  The numeric Poly path rejects those coefficients,
    // which used to leave the oblique-trajectory answers as an unevaluated
    // Integral.
    Ex trySymbolicQuadratic(const Ex& f) {
        Ex t = together(f);
        Ex num = t, den = Num(1);
        if (t->isMul()) {
            std::vector<Ex> nn, dd;
            for (auto& q : t->args) {
                if (q->isPow() && q->args[1]->isNum() && q->args[1]->num.isNeg())
                    dd.push_back(Pow(q->args[0], Num(-q->args[1]->num)));
                else nn.push_back(q);
            }
            if (!dd.empty()) {
                num = nn.empty() ? Num(1) : Mul(nn);
                den = dd.size() == 1 ? dd[0] : Mul(dd);
            }
        } else if (t->isPow() && t->args[1]->isNum() && t->args[1]->num.isNeg()) {
            num = Num(1);
            den = Pow(t->args[0], Num(-t->args[1]->num));
        }
        std::vector<Ex> N, D;
        if (!polyCoeffs(expand(num), x, N)) return nullptr;
        if (!polyCoeffs(expand(den), x, D)) return nullptr;
        if (D.size() < 2 || D.size() > 3 || D.back()->isZero()) return nullptr;
        if (N.size() > 2) return nullptr;
        bool symbolic = false;
        for (auto& c : N) if (!c->isNum()) symbolic = true;
        for (auto& c : D) if (!c->isNum()) symbolic = true;
        if (!symbolic) return nullptr;
        Ex A = N.empty() ? Num(0) : N[0];
        Ex B = N.size() > 1 ? N[1] : Num(0);
        Ex v = Sym(x);
        if (D.size() == 2) {
            // ∫ (A + B v)/(a + b v) dv = (B/b) v + ((A − B a/b)/b) ln|a + b v|.
            Ex b1 = D[1], a0 = D[0];
            Ex lin = Add(a0, Mul(b1, v));
            Ex k = simplify(Add(A, Mul(Num(-1), B, a0, Pow(b1, Num(-1)))));
            return simplify(Add(Mul(B, Pow(b1, Num(-1)), v),
                                Mul(k, Pow(b1, Num(-1)), lnAbs(lin))));
        }
        Ex c2 = D[2], b1 = D[1], a0 = D[0];
        Ex Q = Add({a0, Mul(b1, v), Mul(c2, Pow(v, Num(2)))});
        Ex twoC = Mul(Num(2), c2);
        Ex disc = simplify(Add({Mul(Num(4), a0, c2), Mul(Num(-1), Pow(b1, Num(2)))}));
        Ex K = simplify(Add({A, Mul(Num(-1), B, b1, Pow(twoC, Num(-1)))}));
        Ex logPart = Mul(B, Pow(twoC, Num(-1)), lnAbs(Q));
        double discVal = 0.0;
        bool numericDisc = false;
        {
            bool ok = false;
            discVal = evalReal(disc, {}, &ok);
            numericDisc = ok && std::isfinite(discVal);
        }
        Ex invQ;
        if (numericDisc && std::fabs(discVal) < 1e-12) {
            Ex root = Mul(Num(-1), b1, Pow(twoC, Num(-1)));
            Ex lin = Add(v, Mul(Num(-1), root));
            invQ = Mul(Num(-1), Pow(c2, Num(-1)), Pow(lin, Num(-1)));
        } else if (numericDisc && discVal < 0) {
            Ex root = Sqrt(Mul(Num(-1), disc));
            Ex u = Add(Mul(twoC, v), b1);
            Ex ratio = Mul(Add(u, Mul(Num(-1), root)), Pow(Add(u, root), Num(-1)));
            invQ = Mul(lnAbs(ratio), Pow(root, Num(-1)));
        } else {
            Ex s = Sqrt(disc);
            Ex u = Add(Mul(twoC, v), b1);
            invQ = Mul(Num(2), Pow(s, Num(-1)), Fn("atan", {Mul(u, Pow(s, Num(-1)))}));
        }
        return simplify(Add({logPart, Mul(K, invQ)}));
    }

    Ex trySubstitution(const Ex& f) {
        std::vector<Ex> cands;
        collectSubCandidates(f, f, cands);
        // prefer larger/innermost subexpressions
        std::sort(cands.begin(), cands.end(), [](const Ex& a, const Ex& b) { return compareExpr(a, b) < 0; });
        int tries = 0;
        for (auto& g : cands) {
            if (++tries > 24) break;
            if (!dependsOn(g, x) || g->isSym(x)) continue;
            if (g->isNum()) continue;
            Ex gp = simplify(diff(g, x));
            if (gp->isZero()) continue;
            Ex q = simplify(Mul(f, Pow(gp, Num(-1))));
            if (hasIntegral(q)) continue;
            Ex tt = Sym("__t");
            Ex qsub = substituteExpr(q, g, tt);
            if (dependsOn(qsub, x)) continue;
            if (!dependsOn(qsub, "__t")) continue;
            Ex F = Integrator{ "__t", depth + 1 }.run(qsub);
            if (hasIntegral(F)) continue;
            Ex out = substituteExpr(F, tt, g);
            return out;
        }
        return nullptr;
    }

    // ∫ R(e^x) dx.  With t = e^x the differential is dt/t, so the integral
    // becomes a rational one in t; this covers sech x, 1/(e^x + e^-x) and
    // similar exponential quotients that no elementary table entry matches.
    Ex trySqrtRationalize(const Ex& f) {
        std::vector<Ex> roots;
        std::function<void(const Ex&)> collect = [&](const Ex& e) {
            if (!e) return;
            const bool isSqrt = e->isFunc("sqrt") ||
                                (e->isPow() && e->args[1]->isNum() &&
                                 e->args[1]->num == Rational(1, 2));
            if (isSqrt) roots.push_back(e);
            for (auto& c : e->args) collect(c);
        };
        collect(f);
        for (auto& root : roots) {
            Ex radicand = root->isFunc("sqrt") ? root->args[0] : root->args[0];
            Ex a, b;
            if (!linearIn(radicand, x, a, b)) continue;
            if (!a->isNum() || !b->isNum() || b->num.isZero()) continue;
            if (dependsOn(a, "__u") || dependsOn(b, "__u")) continue;
            Ex u = Sym("__u");
            Ex f1 = substituteExpr(f, root, u);
            Ex xrep = Mul(Add(Pow(u, Num(2)), Mul(Num(-1), a)), Pow(b, Num(-1)));
            Ex f2 = substituteExpr(f1, Sym(x), xrep);
            if (dependsOn(f2, x)) continue;
            Ex q = simplify(Mul(f2, Num(2), u, Pow(b, Num(-1))));
            if (!q || sizeOf(q) > 1200) continue;
            Ex F = Integrator{"__u", depth + 1}.run(q);
            if (hasIntegral(F)) continue;
            Ex out = substituteExpr(F, u, root);
            if (hasIntegral(out)) continue;
            return out;
        }
        return nullptr;
    }

    Ex tryExpSubstitution(const Ex& f) {
        std::vector<Ex> nodes;
        std::function<void(const Ex&)> collect = [&](const Ex& e) {
            if (!e) return;
            if (e->isFunc("exp") && e->args.size() == 1) {
                nodes.push_back(e);
                return;
            }
            for (auto& c : e->args) collect(c);
        };
        collect(f);
        if (nodes.empty()) return nullptr;
        Ex tt = Sym("__e");
        Ex g = f;
        for (auto& nd : nodes) {
            Ex a, b;
            if (!linearIn(nd->args[0], x, a, b)) return nullptr;
            if (!a->isNum() || a->num.isZero() || !a->num.isInt()) return nullptr;
            if (std::llabs(a->num.n) > 20) return nullptr;
            Ex rep = Mul(Fn("exp", {b}), Pow(tt, Num(a->num)));
            g = substituteExpr(g, nd, rep);
        }
        if (dependsOn(g, x)) return nullptr;
        Ex q = Mul(g, Pow(tt, Num(-1)));
        if (sizeOf(q) > 1200) return nullptr;
        Ex F = Integrator{ "__e", depth + 1 }.run(q);
        if (hasIntegral(F)) return nullptr;
        Ex out = substituteExpr(F, tt, Fn("exp", {Sym(x)}));
        if (hasIntegral(out)) return nullptr;
        return out;
    }

    Ex tryTrigPower(const Ex& f) {
        // sin^m(u) cos^n(u) u linear
        Ex m, n;
        Ex sinPart, cosPart;
        if (!splitTrigProduct(f, sinPart, cosPart, m, n)) return nullptr;
        if (!sinPart && !cosPart) return nullptr;
        const Ex& u = sinPart ? sinPart->args[0] : cosPart->args[0];
        if (sinPart && cosPart && !equals(sinPart->args[0], cosPart->args[0])) return nullptr;
        Ex a, b;
        if (!linearIn(u, x, a, b) || !a->isNum()) return nullptr;
        int mm = m ? (int)m->num.n : 0;
        int nn = n ? (int)n->num.n : 0;
        if (mm < 0 || nn < 0 || mm + nn > 8) return nullptr;
        Rational k = a->num;
        if (mm % 2 == 1) {
            // substitute cos
            Ex inner = cosPart ? cosPart : Fn("cos", {u});
            Ex poly = Num(0);
            // ∫ sin^mm cos^nn = -1/k ∫ (1-cos^2)^((mm-1)/2) cos^nn d(cos)
            std::vector<Ex> terms;
            int p = (mm - 1) / 2;
            for (int j = 0; j <= p; ++j) {
                long long bin = 1;
                for (int t = 0; t < j; ++t) bin = bin * (p - t) / (t + 1);
                Ex c = Num(Rational((j % 2 == 0 ? 1 : -1) * bin));
                int pw = nn + 2 * j;
                Ex cc = Pow(inner, Num(pw + 1));
                terms.push_back(Mul(c, Pow(Num(pw + 1), Num(-1)), cc));
            }
            Ex r = Add(terms);
            return Mul(Num(-k.inv()), r);
        }
        if (nn % 2 == 1) {
            Ex inner = sinPart ? sinPart : Fn("sin", {u});
            int p = (nn - 1) / 2;
            std::vector<Ex> terms;
            for (int j = 0; j <= p; ++j) {
                long long bin = 1;
                for (int t = 0; t < j; ++t) bin = bin * (p - t) / (t + 1);
                Ex c = Num(Rational((j % 2 == 0 ? 1 : -1) * bin));
                int pw = mm + 2 * j;
                terms.push_back(Mul(c, Pow(Num(pw + 1), Num(-1)), Pow(inner, Num(pw + 1))));
            }
            Ex r = Add(terms);
            return Mul(Num(k.inv()), r);
        }
        if (mm % 2 == 0 && nn % 2 == 0) {
            return trigEvenPower(mm, nn, u, k);
        }
        return nullptr;
    }

    // ∫ sin^m(u) cos^n(u) du for even m,n via product-to-sum reduction.
    Ex trigEvenPower(int mm, int nn, const Ex& u, const Rational& k) {
        struct T { Rational c; char kind; long long m; };
        std::vector<T> terms{{Rational(1), '1', 0}};
        auto addTerm = [](std::vector<T>& v, Rational c, char kd, long long m) {
            if (c.isZero()) return;
            if (m < 0) { m = -m; if (kd == 's') c = Rational(-c.n, c.d); }
            if (kd == 's' && m == 0) return;
            if (kd == 'c' && m == 0) { kd = '1'; m = 0; }
            for (auto& t : v)
                if (t.kind == kd && t.m == m) { t.c = t.c + c; return; }
            v.push_back({c, kd, m});
        };
        for (int i = 0; i < mm + nn; ++i) {
            char atom = i < mm ? 's' : 'c';
            std::vector<T> next;
            for (auto& t : terms) {
                Rational h = t.c * Rational(1, 2);
                Rational nh = Rational(-h.n, h.d);
                long long a = t.m;
                if (t.kind == '1') { addTerm(next, t.c, atom, 1); continue; }
                if (t.kind == 's' && atom == 's') {
                    addTerm(next, h, 'c', a - 1);
                    addTerm(next, nh, 'c', a + 1);
                } else if (t.kind == 'c' && atom == 'c') {
                    addTerm(next, h, 'c', a - 1);
                    addTerm(next, h, 'c', a + 1);
                } else if (t.kind == 's' && atom == 'c') {
                    addTerm(next, h, 's', a + 1);
                    addTerm(next, h, 's', a - 1);
                } else {
                    addTerm(next, h, 's', a + 1);
                    addTerm(next, nh, 's', a - 1);
                }
            }
            terms = next;
        }
        Ex r = Num(0);
        for (auto& t : terms) {
            if (t.c.isZero()) continue;
            if (t.kind == '1') { r = Add(r, Mul(Num(t.c), Sym(x))); continue; }
            Rational denom = Rational(t.m) * k;
            if (denom.isZero()) continue;
            // ∫cos(pu) = sin(pu)/(pk),  ∫sin(pu) = -cos(pu)/(pk)
            Ex trig = Fn(t.kind == 'c' ? "sin" : "cos", {Mul(Num(t.m), u)});
            Rational cf = t.c / denom;
            if (t.kind == 's') cf = Rational(-cf.n, cf.d);
            r = Add(r, Mul(Num(cf), trig));
        }
        return r;
    }

    bool splitTrigProduct(const Ex& f, Ex& sinPart, Ex& cosPart, Ex& m, Ex& n) {
        sinPart = nullptr; cosPart = nullptr;
        std::vector<Ex> facs;
        if (f->isMul()) facs = f->args;
        else facs = {f};
        for (auto& fct : facs) {
            Ex base = fct, e = Num(1);
            if (fct->isPow()) { base = fct->args[0]; e = fct->args[1]; }
            if (!base->isFunc() || (base->name != "sin" && base->name != "cos")) return false;
            if (!e->isNum() || !e->num.isInt()) return false;
            if (base->name == "sin") { sinPart = base; m = e; }
            else { cosPart = base; n = e; }
        }
        if (sinPart && cosPart) return true;
        if (sinPart) { n = Num(0); cosPart = nullptr; return true; }
        if (cosPart) { m = Num(0); sinPart = nullptr; return true; }
        return false;
    }

    // Rewrite sin(2u)/cos(2u) so that mixed products can be handled by the
    // table or by trigonometric powers.
    Ex tryDoubleAngle(const Ex& f) {
        std::function<Ex(const Ex&)> rw = [&](const Ex& e) -> Ex {
            if (!e) return e;
            if (e->isFunc() && (e->name == "sin" || e->name == "cos") && e->args.size() == 1) {
                Rational c(1); Ex rest;
                if (e->args[0]->isMul() && e->args[0]->args.size() >= 2 &&
                    e->args[0]->args[0]->isNum()) {
                    c = e->args[0]->args[0]->num;
                    std::vector<Ex> r(e->args[0]->args.begin() + 1, e->args[0]->args.end());
                    rest = r.size() == 1 ? r[0] : Mul(r);
                }
                if (c == Rational(2)) {
                    Ex su = Fn("sin", {rest}), cu = Fn("cos", {rest});
                    if (e->name == "sin") return Mul(Num(2), su, cu);
                    return Add(Pow(cu, Num(2)), Mul(Num(-1), Pow(su, Num(2))));
                }
                return e;
            }
            if (e->args.empty()) return e;
            std::vector<Ex> v;
            for (auto& a : e->args) v.push_back(rw(a));
            switch (e->kind) {
                case Kind::Add: return Add(v);
                case Kind::Mul: return Mul(v);
                case Kind::Pow: return Pow(v[0], v[1]);
                case Kind::Func: return Fn(e->name, v);
                default: return e;
            }
        };
        Ex g = rw(f);
        if (equals(g, f)) return nullptr;
        g = simplify(expand(g));
        if (equals(g, f)) return nullptr;
        Ex r = run(g);
        if (hasIntegral(r)) return nullptr;
        return r;
    }

    Ex tryParts(const Ex& f) {
        if (!f->isMul() || f->args.size() < 2) return nullptr;
        // split into u (to differentiate) and dv (to integrate)
        std::vector<Ex> facs = f->args;
        auto score = [](const Ex& e) -> int {
            if (e->isFunc() && (e->name == "ln" || e->name == "log" || e->name == "atan" || e->name == "asin" || e->name == "acos")) return 5;
            if (e->isPow() && e->args[0]->isSym() && e->args[1]->isNum()) return 3;
            if (e->isSym()) return 3;
            if (e->isFunc()) return 2;
            if (e->isPow()) return 1;
            return 0;
        };
        int best = -1, bs = -1;
        for (int i = 0; i < (int)facs.size(); ++i) {
            int s = score(facs[i]);
            if (s > bs) { bs = s; best = i; }
        }
        if (best < 0 || bs == 0) return nullptr;
        Ex u = facs[best];
        std::vector<Ex> rest;
        for (int i = 0; i < (int)facs.size(); ++i) if (i != best) rest.push_back(facs[i]);
        Ex dv = rest.size() == 1 ? rest[0] : Mul(rest);
        Ex v = run(dv);
        if (hasIntegral(v)) return nullptr;
        Ex du = simplify(diff(u, x));
        Ex rem = simplify(Mul(v, du));
        if (sizeOf(rem) > 3000) return nullptr;
        Ex w = run(rem);
        if (hasIntegral(w)) return nullptr;
        return simplify(Add(Mul(u, v), Mul(Num(-1), w)));
    }

    static size_t sizeOf(const Ex& a) {
        size_t s = 1;
        for (auto& x : a->args) s += sizeOf(x);
        return s;
    }
};

Ex integrate(const Ex& f, const std::string& x, bool* closed, std::string* method) {
    Integrator I{x, 0, ""};
    Ex r = I.run(f);
    if (closed) *closed = !hasIntegral(r);
    if (method) *method = I.method;
    return r;
}

bool hasUnevaluatedIntegral(const Ex& a) { return hasIntegral(a); }

} // namespace ode
