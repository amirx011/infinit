// Infinit Lab — expression constructors, canonical form, ordering, printing
// SPDX-License-Identifier: Apache-2.0
#include "expr.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <unordered_map>

namespace ode {

std::string rationalToString(Rational r) {
    if (r.d == 1) return std::to_string(r.n);
    return std::to_string(r.n) + "/" + std::to_string(r.d);
}

// ---------------------------------------------------------------- internals
static bool isSuperscriptableInt(const Ex& e) {
    return e->isNum() && e->num.isInt() && e->num.n >= -9 && e->num.n <= 9;
}

static int precedence(const Ex& a) {
    switch (a->kind) {
        case Kind::Add: return 1;
        case Kind::Mul: return 2;
        case Kind::Pow: return 3;
        default: return 4;
    }
}

static std::string sgn(bool neg) { return neg ? "−" : " + "; }

// numeric coefficient split of a product
// A decimal shown by smartNum is a number wrapped in a function node so it can
// carry a value no rational can.  Printing has to treat it as one: pull it out
// of a term so its sign lands on the term and it heads the product, exactly as
// an ordinary numeric coefficient would.
static bool stripDecimal(const Ex& t, double* value, int* digits, Ex* rest) {
    auto asDecimal = [&](const Ex& f, double* v, int* d) {
        if (!f || !f->isFunc("decimal") || f->args.empty() || !f->args[0]->isNum())
            return false;
        *v = f->args[0]->num.toDouble();
        if (f->args.size() > 1 && f->args[1]->isNum()) *d = (int)f->args[1]->num.n;
        return true;
    };
    double v = 0;
    int d = 6;
    if (asDecimal(t, &v, &d)) {
        *value = v;
        *digits = d;
        *rest = Num(1);
        return true;
    }
    if (!t->isMul()) return false;
    for (size_t i = 0; i < t->args.size(); ++i) {
        if (!asDecimal(t->args[i], &v, &d)) continue;
        *value = v;
        *digits = d;
        std::vector<Ex> others(t->args.begin(), t->args.end());
        others.erase(others.begin() + i);
        if (others.empty()) *rest = Num(1);
        else if (others.size() == 1) *rest = others[0];
        else *rest = Mul(others);
        return true;
    }
    return false;
}

static std::string decimalText(double v, int digits = 6) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*g", digits, v);
    std::string out(buf);
    if (out.find('.') != std::string::npos && out.find('e') == std::string::npos) {
        while (!out.empty() && out.back() == '0') out.pop_back();
        if (!out.empty() && out.back() == '.') out.pop_back();
    }
    return out;
}

static void splitCoeff(const Ex& t, Rational& coeff, Ex& rest) {
    if (t->isNum()) { coeff = t->num; rest = Num(1); return; }
    if (t->isMul() && !t->args.empty() && t->args[0]->isNum()) {
        coeff = t->args[0]->num;
        if (t->args.size() == 2) rest = t->args[1];
        else {
            std::vector<Ex> r(t->args.begin() + 1, t->args.end());
            rest = Mul(r);
        }
        return;
    }
    coeff = Rational(1);
    rest = t;
}

// ---------------------------------------------------------------- constructors
Ex Num(long long v) {
    auto e = std::make_shared<Expr>();
    e->kind = Kind::Num; e->num = Rational(v);
    return e;
}
Ex Num(Rational r) {
    auto e = std::make_shared<Expr>();
    e->kind = Kind::Num; e->num = r;
    return e;
}
Ex Int(long long v) { return Num(v); }
Ex Sym(const std::string& s) {
    static std::unordered_map<std::string, Ex> cache;
    auto it = cache.find(s);
    if (it != cache.end()) return it->second;
    auto e = std::make_shared<Expr>();
    e->kind = Kind::Sym; e->name = s;
    cache.emplace(s, e);
    return e;
}

bool equals(const Ex& a, const Ex& b) {
    if (a.get() == b.get()) return true;
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;
    if (a->kind == Kind::Num) return a->num == b->num;
    if (a->name != b->name) return false;
    if (a->args.size() != b->args.size()) return false;
    for (size_t i = 0; i < a->args.size(); ++i)
        if (!equals(a->args[i], b->args[i])) return false;
    return true;
}

size_t hashOf(const Ex& a) {
    size_t h = (size_t)a->kind * 1000003u;
    h ^= (size_t)a->num.n * 31u + (size_t)a->num.d * 17u;
    h ^= std::hash<std::string>{}(a->name) * 7u;
    for (auto& x : a->args) h = h * 1315423911u ^ hashOf(x);
    return h;
}

static int kindRank(Kind k) {
    switch (k) {
        case Kind::Num: return 0;
        case Kind::Sym: return 1;
        case Kind::Pow: return 2;
        case Kind::Func: return 3;
        case Kind::Mul: return 4;
        case Kind::Add: return 5;
    }
    return 6;
}

int compareExpr(const Ex& a, const Ex& b) {
    if (a.get() == b.get()) return 0;
    int ka = kindRank(a->kind), kb = kindRank(b->kind);
    if (ka != kb) return ka < kb ? -1 : 1;
    switch (a->kind) {
        case Kind::Num:
            if (a->num == b->num) return 0;
            return a->num < b->num ? -1 : 1;
        case Kind::Sym:
            if (a->name == b->name) return 0;
            return a->name < b->name ? -1 : 1;
        default: break;
    }
    if (a->name != b->name) return a->name < b->name ? -1 : 1;
    size_t n = std::min(a->args.size(), b->args.size());
    for (size_t i = 0; i < n; ++i) {
        int c = compareExpr(a->args[i], b->args[i]);
        if (c) return c;
    }
    if (a->args.size() == b->args.size()) return 0;
    return a->args.size() < b->args.size() ? -1 : 1;
}

struct ExCmp {
    bool operator()(const Ex& a, const Ex& b) const { return compareExpr(a, b) < 0; }
};

// add numeric term last for readability
static bool termBefore(const Ex& a, const Ex& b) {
    if (a->isNum() != b->isNum()) return !a->isNum();
    return compareExpr(a, b) < 0;
}

// Flatten a sum, distributing a numeric coefficient over a sum:
// c·(a+b) -> c·a + c·b.  Without this (2x+1)-(2x+1) never cancels and every
// exactness test that depends on such a cancellation fails.
static void flattenAddTerms(const Ex& t, std::vector<Ex>& out) {
    if (!t) return;
    if (t->isAdd()) {
        for (auto& a : t->args) flattenAddTerms(a, out);
        return;
    }
    if (t->isMul() && t->args.size() >= 2 && t->args[0]->isNum()) {
        size_t addIdx = t->args.size();
        int addCount = 0;
        for (size_t i = 1; i < t->args.size(); ++i)
            if (t->args[i]->isAdd()) { addIdx = i; ++addCount; }
        if (addCount == 1) {
            std::vector<Ex> rest;
            rest.reserve(t->args.size() - 1);
            for (size_t i = 0; i < t->args.size(); ++i)
                if (i != addIdx) rest.push_back(t->args[i]);
            Ex factor = rest.size() == 1 ? rest[0] : Mul(rest);
            for (auto& a : t->args[addIdx]->args)
                flattenAddTerms(Mul(factor, a), out);
            return;
        }
    }
    out.push_back(t);
}

Ex Add(const std::vector<Ex>& terms) {
    std::vector<Ex> flat;
    flat.reserve(terms.size());
    for (auto& t : terms) flattenAddTerms(t, flat);
    if (flat.empty()) return Num(0);
    if (flat.size() == 1) return flat[0];

    std::map<Ex, Rational, ExCmp> acc;
    Rational cst(0);
    for (auto& t : flat) {
        Rational c; Ex rest;
        splitCoeff(t, c, rest);
        if (rest->isOne()) { cst = cst + c; continue; }
        auto it = acc.find(rest);
        if (it == acc.end()) acc.emplace(rest, c);
        else it->second = it->second + c;
    }
    std::vector<Ex> out;
    for (auto& kv : acc) {
        if (kv.second.isZero()) continue;
        if (kv.second.isOne()) out.push_back(kv.first);
        else out.push_back(Mul({Num(kv.second), kv.first}));
    }
    if (!cst.isZero()) out.push_back(Num(cst));
    if (out.empty()) return Num(0);
    if (out.size() == 1) return out[0];
    std::sort(out.begin(), out.end(), termBefore);
    auto e = std::make_shared<Expr>();
    e->kind = Kind::Add; e->args = std::move(out);
    return e;
}
Ex Add(Ex a, Ex b) { return Add(std::vector<Ex>{std::move(a), std::move(b)}); }

static Rational powRatInt(Rational base, long long p) {
    if (p == 0) return Rational(1);
    bool neg = p < 0;
    unsigned long long e = neg ? (unsigned long long)(-(p + 1)) + 1ULL : (unsigned long long)p;
    Rational r(1);
    Rational b = base;
    while (e) {
        if (e & 1) r = r * b;
        e >>= 1;
        if (e) b = b * b;
    }
    return neg ? r.inv() : r;
}

Ex Pow(Ex base, Ex exp) {
    if (!base || !exp) return Num(0);
    // Euler's number: e^u is represented as exp(u) so that the calculus
    // routines can integrate/differentiate it uniformly.
    if (base->isSym("e") && !(exp->isNum() && exp->num.isInt()))
        return Fn("exp", {exp});
    if (base->isFunc("exp") && base->args.size() == 1 &&
        exp->isNum() && exp->num.isInt())
        return Fn("exp", {Mul(base->args[0], exp)});
    if (exp->isNum()) {
        const Rational& q = exp->num;
        if (q.isZero()) return Num(1);
        if (q.isOne()) return base;
        if (base->isNum()) {
            if (q.isInt()) {
                try { return Num(powRatInt(base->num, q.n)); }
                catch (...) { throw OverflowError("power too large"); }
            }
            // (n/d)^(-1/q) = (d/n)^(1/q)
            if (q.n == -1 && q.d > 1 && q.d <= 64)
                return Pow(Num(Rational(base->num.d, base->num.n)), Num(Rational(1, q.d)));
            // exact perfect roots for small rational exponents with numerator 1
            if (q.n == 1 && q.d > 1 && q.d <= 64) {
                long long n = base->num.n, d = base->num.d;
                if (n < 0) {
                    // An odd root of a negative number is real; an even root
                    // is not, so keep it as a formal power.  Rebuilding
                    // Pow(-1, exp) here recursed forever for exp = 1/2.
                    if (q.d % 2 == 1)
                        return Mul(Num(-1), Pow(Num(Rational(-n, d)), exp));
                    auto e = std::make_shared<Expr>();
                    e->kind = Kind::Pow;
                    e->args = {base, exp};
                    return e;
                }
                long long rn = (long long)std::llround(std::pow((double)n, 1.0 / (double)q.d));
                long long rd = (long long)std::llround(std::pow((double)d, 1.0 / (double)q.d));
                for (long long k = std::max(0LL, rn - 2); k <= rn + 2; ++k) {
                    if (k > 0 && powRatInt(Rational(k), q.d).n == n) rn = k;
                }
                for (long long k = std::max(1LL, rd - 2); k <= rd + 2; ++k) {
                    if (powRatInt(Rational(k), q.d).n == d) rd = k;
                }
                if (powRatInt(Rational(rn, rd), q.d).n == n && powRatInt(Rational(rn, rd), q.d).d == d)
                    return Num(Rational(rn, rd));
            }
        }
        // pull square factors out of a numeric radicand: sqrt(8)=2√2, sqrt(1/2)=√2/2
        if (q == Rational(1, 2) && base->isNum() && base->num.n > 0) {
            Rational r = base->num;
            long long m = r.n * r.d;
            long long c = 1, rest = m;
            for (long long k = 2; k * k <= rest && k <= 4096; ++k)
                while (rest % (k * k) == 0) { c *= k; rest /= (k * k); }
            if (c != 1 || r.d != 1) {
                Rational coef(c, r.d);
                if (rest == 1) return Num(coef);
                return Mul(Num(coef), Pow(Num(rest), Num(Rational(1, 2))));
            }
        }
        if (base->isOne()) return Num(1);
        if (base->isZero()) {
            if (q.isNeg()) throw std::runtime_error("division by zero (0^negative)");
            return Num(0);
        }
        // (x^a)^b -> x^(a*b) when b integer (safe) or a integer
        if (base->isPow()) {
            Ex a = base->args[1], b = exp;
            if (b->isInt() || a->isInt())
                return Pow(base->args[0], Mul(a, b));
        }
        // distribute integer powers over products: (2xy)^2
        if (base->isMul() && q.isInt()) {
            std::vector<Ex> parts;
            for (auto& f : base->args) parts.push_back(Pow(f, exp));
            return Mul(parts);
        }
        // sqrt(a^2·b) = a·sqrt(b): pull perfect squares out of a radicand.
        if (base->isMul() && q == Rational(1, 2)) {
            std::vector<Ex> outside, inside;
            for (auto& f : base->args) {
                if (f->isPow() && f->args[1]->isNum() && f->args[1]->num.isInt() &&
                    f->args[1]->num.n % 2 == 0 && f->args[1]->num.n != 0) {
                    outside.push_back(Pow(f->args[0], Num(f->args[1]->num.n / 2)));
                } else {
                    inside.push_back(f);
                }
            }
            if (!outside.empty()) {
                Ex root = Num(1);
                if (!inside.empty()) {
                    Ex rest = inside.size() == 1 ? inside[0] : Mul(inside);
                    root = Pow(rest, exp);
                }
                return Mul(Mul(outside), root);
            }
        }
    }
    auto e = std::make_shared<Expr>();
    e->kind = Kind::Pow;
    e->args = {base, exp};
    return e;
}

Ex Mul(const std::vector<Ex>& factors) {
    std::vector<Ex> flat;
    flat.reserve(factors.size());
    for (auto& f : factors) {
        if (!f) continue;
        if (f->isNum() && f->num.isZero()) return Num(0);
        if (f->isMul()) flat.insert(flat.end(), f->args.begin(), f->args.end());
        else flat.push_back(f);
    }
    if (flat.empty()) return Num(1);

    Rational coeff(1);
    std::map<Ex, Ex, ExCmp> powers;
    for (auto& f : flat) {
        if (f->isNum()) { coeff = coeff * f->num; continue; }
        Ex base, ex;
        if (f->isFunc("exp") && f->args.size() == 1) {
            // Treat exp(u) as e^u so that exp(a)·exp(b) = exp(a+b).
            base = Sym("e");
            ex = f->args[0];
        } else if (f->isPow()) { base = f->args[0]; ex = f->args[1]; }
        else { base = f; ex = Num(1); }
        auto it = powers.find(base);
        if (it == powers.end()) powers.emplace(base, ex);
        else it->second = Add(it->second, ex);
    }
    if (coeff.isZero()) return Num(0);
    std::vector<Ex> out;
    for (auto& kv : powers) {
        if (kv.second->isZero()) continue;
        if (kv.second->isOne()) out.push_back(kv.first);
        else out.push_back(Pow(kv.first, kv.second));
    }
    if (out.empty()) return Num(coeff);
    if (out.size() == 1 && coeff.isOne()) return out[0];
    if (coeff.isOne() && out.size() == 1) return out[0];
    std::sort(out.begin(), out.end(), [](const Ex& a, const Ex& b) { return compareExpr(a, b) < 0; });
    auto e = std::make_shared<Expr>();
    e->kind = Kind::Mul;
    std::vector<Ex> all;
    if (!coeff.isOne()) all.push_back(Num(coeff));
    all.insert(all.end(), out.begin(), out.end());
    e->args = std::move(all);
    return e;
}
Ex Mul(Ex a, Ex b) { return Mul(std::vector<Ex>{std::move(a), std::move(b)}); }

// --- exact trigonometry at rational multiples of pi
static bool piMultiple(const Ex& a, Rational& k) {
    if (a->isSym("pi")) { k = Rational(1); return true; }
    if (a->isMul()) {
        bool hasPi = false; Rational coeff(1);
        for (auto& f : a->args) {
            if (f->isSym("pi")) { if (hasPi) return false; hasPi = true; }
            else if (f->isNum()) coeff = coeff * f->num;
            else return false;
        }
        if (hasPi) { k = coeff; return true; }
    }
    return false;
}

static void reducePi(Rational& k) {
    // reduce to [0,2)
    long long den = k.d;
    long long num = k.n % (2 * den);
    if (num < 0) num += 2 * den;
    k = Rational(num, den);
}

static bool exactTrig(const std::string& fn, const Rational& k0, Rational& out) {
    Rational k = k0;
    reducePi(k);
    // table for sin/cos at multiples of pi/6 and pi/4 handled via known values
    // use symmetry: sin(k*pi)
    auto sinVal = [](const Rational& t, Rational& v) -> bool {
        // t in [0,2)
        if (t.n == 0 || t.n == t.d || t.n == 2 * t.d) { v = 0; return true; }
        if (2 * t.n == t.d) { v = 1; return true; }
        if (t.d == 6) {
            if (t.n == 1) { v = Rational(1, 2); return true; }
            if (t.n == 5) { v = Rational(1, 2); return true; }
            return false;
        }
        if (t.d == 4 && t.n == 1) return false; // sqrt2/2 handled by printer? skip
        if (t.d == 3) {
            if (t.n == 1) return false;
            if (t.n == 2) return false;
        }
        return false;
    };
    if (fn == "sin") {
        if (k.n == 0) { out = 0; return true; }
        // sin(pi - a) = sin(a), sin(pi + a) = -sin(a)
        if (k.n == 3 && k.d == 2) { out = -1; return true; }
        if (k.n * 6 % k.d == 0) {
            Rational t = k;
            if (t.n > t.d) t = Rational(t.n - 2 * t.d, t.d);
            Rational s;
            if (sinVal(t, s)) {
                if (k.n > k.d) s = -s;
                out = s; return true;
            }
        }
        return false;
    }
    if (fn == "cos") {
        Rational kk = Rational(2 * k.n + k.d, 2 * k.d); // cos(k*pi) = sin((k + 1/2)*pi)
        Rational s;
        if (exactTrig("sin", kk, s)) { out = s; return true; }
        return false;
    }
    if (fn == "tan") {
        // tan(pi/4) and tan(3pi/4) = -+1 are exact even though sin and cos are not rational.
        if (k.d == 4 && (k.n == 1 || k.n == 5)) { out = 1; return true; }
        if (k.d == 4 && (k.n == 3 || k.n == 7)) { out = -1; return true; }
        Rational s, c;
        if (exactTrig("sin", k, s) && exactTrig("cos", k, c) && !c.isZero()) { out = s / c; return true; }
        return false;
    }
    return false;
}

Ex Fn(const std::string& name, const std::vector<Ex>& args) {
    // fold numeric arguments
    if (args.empty()) return Sym(name);
    if (name == "sqrt") return Pow(args[0], Num(Rational(1, 2)));
    if (name == "exp") {
        if (args[0]->isZero()) return Num(1);
        if (args[0]->isOne()) return Sym("e");
        if (args[0]->isFunc("ln")) return args[0]->args[0];
        // e^(a ln b) = b^a
        if (args[0]->isMul()) {
            const Ex* lnPart = nullptr; Ex other = Num(1);
            std::vector<Ex> rest;
            for (auto& f : args[0]->args) {
                if (f->isFunc("ln") && !lnPart) lnPart = &f;
                else rest.push_back(f);
            }
            if (lnPart) {
                Ex e2 = Mul(rest.empty() ? std::vector<Ex>{Num(1)} : rest);
                return Pow((*lnPart)->args[0], e2);
            }
        }
    }
    if (name == "ln" || name == "log") {
        if (args[0]->isOne()) return Num(0);
        if (args[0]->isSym("e")) return Num(1);
        if (args[0]->isSym("pi")) return Sym("ln(pi)"); // keep symbolic
        if (args[0]->isFunc("exp")) return args[0]->args[0];
        if (args[0]->isPow() && args[0]->args[0]->isSym("e")) return args[0]->args[1];
    }
    if (name == "abs") {
        if (args[0]->isNum()) return Num(args[0]->num.abs());
        if (args[0]->isFunc("abs")) return args[0];
    }
    if (name == "sin" || name == "cos" || name == "tan") {
        Rational k;
        if (piMultiple(args[0], k)) {
            Rational v;
            if (exactTrig(name, k, v)) return Num(v);
        }
        if (name == "sin" && args[0]->isFunc("asin")) return args[0]->args[0];
        if (name == "cos" && args[0]->isFunc("acos")) return args[0]->args[0];
        if (name == "tan" && args[0]->isFunc("atan")) return args[0]->args[0];
        if (name == "sin" && args[0]->isZero()) return Num(0);
        if (name == "cos" && args[0]->isZero()) return Num(1);
        if (name == "tan" && args[0]->isZero()) return Num(0);
        if (name == "sin") {
            // sin(-x) = -sin(x)
            Rational c; Ex rest;
            splitCoeff(args[0], c, rest);
            if (c.isNeg()) return Mul(Num(-1), Fn("sin", {Mul(Num(-c), rest)}));
        }
        if (name == "cos") {
            Rational c; Ex rest;
            splitCoeff(args[0], c, rest);
            if (c.isNeg()) return Fn("cos", {Mul(Num(-c), rest)});
        }
    }
    if (name == "sinh" || name == "tanh" || name == "atan" || name == "asin" || name == "acos") {
        if (args[0]->isZero()) {
            if (name == "sinh" || name == "tanh" || name == "atan" || name == "asin") return Num(0);
            if (name == "acos") return Mul(Num(Rational(1, 2)), Sym("pi"));
        }
    }
    if (name == "atan" && args[0]->isNum()) {
        // atan(+-1) is a rational multiple of pi, and initial conditions read
        // much better as pi/4 than as atan(1).
        if (args[0]->num == Rational(1)) return Mul(Num(Rational(1, 4)), Sym("pi"));
        if (args[0]->num == Rational(-1)) return Mul(Num(Rational(-1, 4)), Sym("pi"));
    }
    // The remaining odd and even functions, so that -2 asin(-x/2) reads as
    // 2 asin(x/2) instead of keeping a hidden double sign.
    if (name == "tan" || name == "cot" || name == "csc" || name == "sinh" ||
        name == "tanh" || name == "asin" || name == "atan") {
        Rational c; Ex rest;
        splitCoeff(args[0], c, rest);
        if (c.isNeg()) return Mul(Num(-1), Fn(name, {Mul(Num(-c), rest)}));
    }
    if (name == "abs" || name == "sec" || name == "cosh") {
        Rational c; Ex rest;
        splitCoeff(args[0], c, rest);
        if (c.isNeg()) return Fn(name, {Mul(Num(-c), rest)});
    }
    if (name == "erf") {
        if (args[0]->isZero()) return Num(0);
        if (args[0]->isSym("inf")) return Num(1);
        if (args[0]->isMul() && args[0]->args.size() == 2 &&
            args[0]->args[0]->isNum() && args[0]->args[0]->num == Rational(-1) &&
            args[0]->args[1]->isSym("inf"))
            return Num(-1);
    }
    if (name == "erfc") {
        if (args[0]->isZero()) return Num(1);
        if (args[0]->isSym("inf")) return Num(0);
    }
    if (name == "gamma" && args[0]->isNum()) {
        const Rational& q = args[0]->num;
        if (q.isInt() && q.n > 0) {
            long long r = 1;
            for (long long i = 2; i <= q.n - 1; ++i) r *= i;
            return Num(r);
        }
    }
    auto e = std::make_shared<Expr>();
    e->kind = Kind::Func; e->name = name; e->args = args;
    return e;
}
Ex Sqrt(Ex a) { return Pow(a, Num(Rational(1, 2))); }

Ex Deriv(const Ex& f, const std::string& x, int order) {
    if (order <= 0) return f;
    if (f->isNum()) return Num(0);
    auto e = std::make_shared<Expr>();
    e->kind = Kind::Func; e->name = "Deriv";
    e->args = {f, Sym(x), Num(order)};
    return e;
}
Ex makeDeriv(const Ex& f, const std::string& x, int order) { return Deriv(f, x, order); }
bool isDeriv(const Ex& a) { return a->isFunc("Deriv"); }
int derivOrder(const Ex& a) {
    if (!isDeriv(a)) return 0;
    return (int)a->args[2]->num.n;
}

// ---------------------------------------------------------------- queries
bool dependsOn(const Ex& a, const std::string& var) {
    if (!a) return false;
    if (a->isSym(var)) return true;
    if (a->isFunc("Deriv")) {
        // in ODE algebra, y' is an independent jet variable
        if (a->args[0]->isSym(var)) return false;
        return dependsOn(a->args[0], var);
    }
    for (auto& x : a->args) if (dependsOn(x, var)) return true;
    return false;
}
bool dependsOnAny(const Ex& a, const std::vector<std::string>& vars) {
    for (auto& v : vars) if (dependsOn(a, v)) return true;
    return false;
}
void collectSymbols(const Ex& a, std::vector<std::string>& out) {
    if (!a) return;
    if (a->isSym()) {
        if (a->name == "pi" || a->name == "e") return;
        if (std::find(out.begin(), out.end(), a->name) == out.end()) out.push_back(a->name);
    }
    for (auto& x : a->args) collectSymbols(x, out);
}
bool isConstWrt(const Ex& a, const std::string& var) { return !dependsOn(a, var); }

// ---------------------------------------------------------------- function names
static const char* kKnownFunctions[] = {
    "sin", "cos", "tan", "cot", "sec", "csc",
    "asin", "acos", "atan", "asinh", "acosh", "atanh",
    "sinh", "cosh", "tanh", "sech", "csch", "exp", "ln", "log", "sqrt", "abs",
    "erf", "erfc", "fact", "deriv", "integral", "u", "delta"
};

bool isKnownFunctionName(const std::string& name) {
    for (const char* known : kKnownFunctions) {
        if (name == known) return true;
    }
    return false;
}

std::string unknownFunction(const Ex& a) {
    if (!a) return "";
    if (a->isFunc() && !a->isFunc("Deriv") && !a->isFunc("Integral") &&
        !isKnownFunctionName(a->name)) {
        return a->name;
    }
    for (const auto& arg : a->args) {
        const std::string found = unknownFunction(arg);
        if (!found.empty()) return found;
    }
    return "";
}

// ---------------------------------------------------------------- substitution
Ex substitute(const Ex& a, const std::string& var, const Ex& rep) {
    if (!a) return a;
    if (a->isSym(var)) return rep;
    if (a->isFunc("Deriv")) {
        // y' is an independent symbol in ODE algebra: do not substitute inside it
        return a;
    }
    if (a->args.empty()) return a;
    std::vector<Ex> na;
    na.reserve(a->args.size());
    for (auto& x : a->args) na.push_back(substitute(x, var, rep));
    switch (a->kind) {
        case Kind::Add: return Add(na);
        case Kind::Mul: return Mul(na);
        case Kind::Pow: return Pow(na[0], na[1]);
        case Kind::Func: return Fn(a->name, na);
        default: return a;
    }
}
Ex substituteSubexpr(const Ex& a, const Ex& target, const Ex& rep) {
    if (!a) return a;
    if (equals(a, target)) return rep;
    if (a->args.empty()) return a;
    std::vector<Ex> na;
    na.reserve(a->args.size());
    for (auto& x : a->args) na.push_back(substituteSubexpr(x, target, rep));
    switch (a->kind) {
        case Kind::Add: return Add(na);
        case Kind::Mul: return Mul(na);
        case Kind::Pow: return Pow(na[0], na[1]);
        case Kind::Func: return Fn(a->name, na);
        default: return a;
    }
}

Ex substituteMany(const Ex& a, const std::map<std::string, Ex>& m) {
    Ex r = a;
    for (auto& kv : m) r = substitute(r, kv.first, kv.second);
    return r;
}

// ---------------------------------------------------------------- printing
static void fmtNum(const Ex& a, std::string& out, bool pretty, bool latex) {
    const Rational& r = a->num;
    if (!latex) {
        out += rationalToString(r);
        return;
    }
    if (r.d == 1) { out += std::to_string(r.n); return; }
    std::string sign = r.n < 0 ? "-" : "";
    out += sign + "\\frac{" + std::to_string(std::llabs(r.n)) + "}{" + std::to_string(r.d) + "}";
}

static bool hasDenominator(const Ex& a) {
    if (!a) return false;
    if (a->isPow() && a->args[1]->isNum() && a->args[1]->num.isNeg()) return true;
    for (auto& c : a->args) if (hasDenominator(c)) return true;
    return false;
}

static bool containsSquareRoot(const Ex& a) {
    if (!a) return false;
    if (a->isPow() && a->args.size() == 2 && a->args[1]->isNum() &&
        a->args[1]->num == Rational(1, 2))
        return true;
    for (const auto& c : a->args)
        if (containsSquareRoot(c)) return true;
    return false;
}

struct Printer {
    bool pretty = false, latex = false;

    std::string wrap(const Ex& a, int parentPrec, bool needParens = false) {
        std::string s = pr(a);
        if (needParens || precedence(a) < parentPrec) return (latex ? "\\left(" : "(") + s + (latex ? "\\right)" : ")");
        return s;
    }

    std::string mulSign() const { return pretty ? "·" : (latex ? "\\cdot " : "*"); }

    std::string pr(const Ex& a) {
        if (!a) return "0";
        switch (a->kind) {
            case Kind::Num: {
                std::string s; fmtNum(a, s, pretty, latex);
                return s;
            }
            case Kind::Sym: {
                const std::string& nm = a->name;
                if (nm == "pi") return latex ? "\\pi" : (pretty ? "π" : "pi");
                if (nm == "e" && pretty) return "e";
                const bool compound =
                    nm.find(' ') != std::string::npos ||
                    nm.find('=') != std::string::npos ||
                    nm.find('^') != std::string::npos ||
                    nm.find('_') != std::string::npos ||
                    nm.find('\'') != std::string::npos ||
                    nm.find('(') != std::string::npos ||
                    nm.find('/') != std::string::npos ||
                    nm.find('*') != std::string::npos;
                if (latex) {
                    if (compound) {
                        if (nm.find('\n') != std::string::npos ||
                            nm.find('\t') != std::string::npos)
                            return nm;
                        return toLatexText(nm);
                    }
                    const std::string g = greekSymbolLatex(nm);
                    if (!g.empty()) return g;
                    return nm;
                }
                if (pretty) {
                    if (compound) return greekTextToUnicode(nm);
                    const std::string g = greekSymbolUnicode(nm);
                    if (!g.empty()) return g;
                }
                return nm;
            }
            case Kind::Add: {
                std::string s;
                bool first = true;
                for (auto& t : a->args) {
                    double dv = 0;
                    int ddigits = 6;
                    Ex decRest;
                    if (stripDecimal(t, &dv, &ddigits, &decRest)) {
                        const bool decNeg = dv < 0;
                        if (first) {
                            if (decNeg) s += (latex ? "-" : (pretty ? "−" : "-"));
                            first = false;
                        } else {
                            s += decNeg ? (latex ? " - " : (pretty ? " − " : " - "))
                                        : (latex ? " + " : " + ");
                        }
                        const double mag = std::fabs(dv);
                        const bool showMag = decRest->isOne() || std::fabs(mag - 1.0) > 1e-12;
                        if (showMag) {
                            s += decimalText(mag, ddigits);
                            if (!decRest->isOne()) {
                                if (!latex) s += mulSign();
                                s += wrap(decRest, 2);
                            }
                        } else {
                            s += wrap(decRest, 2);
                        }
                        continue;
                    }
                    Rational c; Ex rest;
                    splitCoeff(t, c, rest);
                    bool neg = c.isNeg();
                    Rational ac = c.abs();
                    if (first) {
                        if (neg) s += "-";
                        first = false;
                    } else {
                        s += neg ? (latex ? " - " : (pretty ? " − " : " - "))
                                 : (latex ? " + " : " + ");
                    }
                    if (rest->isOne()) {
                        std::string ns; Ex nn = Num(ac); fmtNum(nn, ns, pretty, latex); s += ns;
                    } else if (ac.isOne()) {
                        s += wrap(rest, 2);
                    } else if (hasDenominator(rest)) {
                        // 2 · 1/(s²+16) belongs in one fraction, exactly like
                        // 1/2 · s/(s²+4); printing the unit numerator is noise.
                        s += pr(Mul(Num(ac), rest));
                    } else {
                        std::string ns; Ex nn = Num(ac); fmtNum(nn, ns, pretty, latex);
                        s += ns;
                        if (!latex) s += pretty ? "·" : "*";
                        s += wrap(rest, 2);
                    }
                }
                if (latex) return s;
                return s;
            }
            case Kind::Mul: {
                // Split the product into numeric coefficient, numerator
                // factors and denominator factors so that negative powers are
                // rendered as real fractions.
                Rational coef(1);
                std::vector<Ex> num, den;
                for (auto& f : a->args) {
                    if (f->isNum()) { coef = coef * f->num; continue; }
                    if (f->isPow() && f->args[1]->isNum() && f->args[1]->num.isNeg()) {
                        den.push_back(Pow(f->args[0], Num(-f->args[1]->num)));
                        continue;
                    }
                    num.push_back(f);
                }
                // A decimal coefficient heads the product and owns its sign.
                bool hasDec = false;
                double decVal = 1.0;
                int decDigits = 6;
                Ex decNode;
                for (size_t i = 0; i < num.size(); ++i) {
                    double dv = 0;
                    int d = 6;
                    Ex rest;
                    if (!stripDecimal(num[i], &dv, &d, &rest)) continue;
                    hasDec = true;
                    decVal = dv;
                    decDigits = d;
                    decNode = num[i];
                    num.erase(num.begin() + i);
                    if (!rest->isOne()) num.insert(num.begin() + i, rest);
                    break;
                }
                auto render = [&](const std::vector<Ex>& v) {
                    std::string r;
                    for (size_t i = 0; i < v.size(); ++i) {
                        if (i) r += mulSign();
                        r += wrap(v[i], 3, v[i]->isAdd());
                    }
                    return r;
                };
                bool neg = coef.isNeg();
                Rational ac = coef.abs();
                auto head = [&]() {
                    std::string s = neg ? (latex ? "-" : (pretty ? "−" : "-")) : "";
                    if (!ac.isOne() || num.empty()) {
                        std::string ns; Ex nn = Num(ac); fmtNum(nn, ns, pretty, latex);
                        s += ns;
                        if (!num.empty() && !latex) s += mulSign();
                    }
                    return s;
                };
                if (!den.empty()) {
                    if (hasDec) num.insert(num.begin(), decNode);
                    std::vector<Ex> dparts = den;
                    Rational an = ac;
                    // Fold a rational coefficient's denominator into the
                    // denominator, so 1/2 · 1/x prints as 1/(2·x).
                    if (ac.d != 1) {
                        dparts.insert(dparts.begin(), Num(ac.d));
                        an = Rational(ac.n, 1);
                    }
                    std::string nd = render(num);
                    if (num.empty()) {
                        nd = rationalToString(an);
                    } else if (!an.isOne()) {
                        nd = rationalToString(an) + mulSign() + nd;
                    }
                    std::string dd = render(dparts);
                    if (dparts.size() > 1 && !latex) dd = "(" + dd + ")";
                    std::string sign = neg ? (latex ? "-" : (pretty ? "−" : "-")) : "";
                    if (latex) return sign + "\\frac{" + nd + "}{" + dd + "}";
                    return sign + nd + "/" + dd;
                }
                // A unit fraction times a factor that contains a square root
                // is a quadratic-formula root.  Printing it as one fraction
                // ((1 + sqrt(5))/2, x*(1 + sqrt(5))/2) is shorter and matches
                // the textbook form; the generic coefficient-first shape stays
                // for everything else so existing outputs do not churn.
                bool sqrtSum = false;
                for (const auto& f : num)
                    if (f->isAdd() && containsSquareRoot(f)) sqrtSum = true;
                if (den.empty() && !num.empty() && sqrtSum && ac.n == 1 &&
                    ac.d != 1) {
                    const std::string nd = render(num);
                    const std::string sign =
                        neg ? (latex ? "-" : (pretty ? "−" : "-")) : "";
                    const std::string dd = std::to_string(ac.d);
                    if (latex) return sign + "\\frac{" + nd + "}{" + dd + "}";
                    return sign + nd + "/" + dd;
                }
                std::string s = head();
                if (hasDec) {
                    const double v = decVal * coef.toDouble();
                    const double mag = std::fabs(v);
                    s = v < 0 ? (latex ? "-" : (pretty ? "−" : "-")) : "";
                    if (num.empty() || std::fabs(mag - 1.0) > 1e-12) {
                        s += decimalText(mag, decDigits);
                        if (!num.empty() && !latex) s += mulSign();
                    }
                }
                s += render(num);
                return s;
            }
            case Kind::Pow: {
                const Ex& b = a->args[0];
                const Ex& e = a->args[1];
                const bool parenBase = b->isAdd() || b->isMul() || b->isPow() ||
                                       (b->isNum() && b->num.isNeg());
                if (e->isNum() && e->num.n == 1 && e->num.d == 2) {
                    if (latex) return "\\sqrt{" + pr(b) + "}";
                    return (pretty ? "√" : "sqrt(") + wrap(b, 4, b->isAdd() || b->isMul()) + (pretty ? "" : ")");
                }
                if (e->isNum() && e->num.n == -1 && e->num.d == 2) {
                    if (latex) return "\\frac{1}{\\sqrt{" + pr(b) + "}}";
                    return (pretty ? "1/√" : "1/sqrt(") + wrap(b, 4, b->isAdd() || b->isMul()) + (pretty ? "" : ")");
                }
                if (e->isNum() && e->num.isInt() && e->num.n < 0 && !b->isNum()) {
                    // A lone s^-2 reads as a power, while the same factor in a
                    // product is already printed as a fraction.  Keep the two
                    // shapes consistent.
                    const Rational n(-e->num.n, 1);
                    Ex pos = (n == Rational(1)) ? b : Pow(b, Num(n));
                    if (latex) return "\\frac{1}{" + pr(pos) + "}";
                    return "1/" + (pos->isAdd() ? "(" + pr(pos) + ")" : pr(pos));
                }
                if (pretty && isSuperscriptableInt(e)) {
                    static const char* sup[] = {"⁰","¹","²","³","⁴","⁵","⁶","⁷","⁸","⁹"};
                    long long n = e->num.n;
                    std::string s;
                    if (n < 0) s += "⁻";
                    n = std::llabs(n);
                    s += sup[n];
                    return wrap(b, 4, parenBase) + s;
                }
                if (latex) {
                    return "{" + wrap(b, 4, parenBase) + "}^{" + pr(e) + "}";
                }
                if (e->isNum() && e->num == Rational(-1)) {
                    return "1/" + wrap(b, 4, b->isAdd() || b->isMul());
                }
                if (e->isNum() && e->num.isInt()) {
                    return wrap(b, 4, parenBase) + "^" + std::to_string(e->num.n);
                }
                return wrap(b, 4, parenBase) + "^(" + pr(e) + ")";
            }
            case Kind::Func: {
                if (a->name == "Deriv") {
                    std::string s = pr(a->args[0]);
                    int o = derivOrder(a);
                    for (int i = 0; i < o; ++i) s += pretty ? "′" : "'";
                    return s;
                }
                if (a->name == "Integral") {
                    // args: integrand, variable
                    Ex f = a->args[0], v = a->args[1];
                    std::string body = wrap(f, 2, f->isAdd());
                    if (latex) return "\\int " + body + "\\, d" + pr(v);
                    return (pretty ? "∫ " : "Integral(") + body + (pretty ? " d" : ", ") + pr(v) + (pretty ? "" : ")");
                }
                if (a->name == "abs") {
                    if (latex) return "\\left|" + pr(a->args[0]) + "\\right|";
                    return (pretty ? "|" : "abs(") + pr(a->args[0]) + (pretty ? "|" : ")");
                }
                if (a->name == "fact" && a->args.size() == 1) {
                    const Ex& u = a->args[0];
                    const bool paren = u->isAdd() || u->isMul() || u->isPow() ||
                                       (u->isNum() && u->num.isNeg());
                    if (latex) return (paren ? "\\left(" + pr(u) + "\\right)" : pr(u)) + "!";
                    return (paren ? "(" + pr(u) + ")" : pr(u)) + "!";
                }
                if (a->name == "decimal" && !a->args.empty() && a->args[0]->isNum()) {
                    int digits = 6;
                    if (a->args.size() > 1 && a->args[1]->isNum())
                        digits = (int)a->args[1]->num.n;
                    return decimalText(a->args[0]->num.toDouble(), digits);
                }
                if (a->name == "exp") {
                    if (latex) return "e^{" + pr(a->args[0]) + "}";
                    // Pretty output reads like mathematics; the canonical
                    // plain form keeps exp(...) because callers and oracles
                    // parse it back.
                    if (!pretty) return "exp(" + pr(a->args[0]) + ")";
                    const Ex& u = a->args[0];
                    const std::string body = pr(u);
                    const bool paren = u->isAdd() || u->isMul() || u->isPow() ||
                                       (u->isNum() && u->num.isNeg());
                    return "e^" + (paren ? "(" + body + ")" : body);
                }
                if (a->name == "ln" || a->name == "log") {
                    if (latex) return "\\ln\\left(" + pr(a->args[0]) + "\\right)";
                    return "ln(" + pr(a->args[0]) + ")";
                }
                static const std::map<std::string, std::string> ltx = {
                    {"sin","\\sin"},{"cos","\\cos"},{"tan","\\tan"},{"cot","\\cot"},
                    {"sec","\\sec"},{"csc","\\csc"},{"asin","\\arcsin"},{"acos","\\arccos"},
                    {"atan","\\arctan"},{"sinh","\\sinh"},{"cosh","\\cosh"},{"tanh","\\tanh"},
                    {"asinh","\\mathrm{asinh}"},{"acosh","\\mathrm{acosh}"},{"atanh","\\mathrm{atanh}"}
                };
                std::string nm = a->name;
                if (latex) {
                    const std::string g = greekSymbolLatex(nm);
                    if (!g.empty()) {
                        std::string s = g + "\\left(";
                        for (size_t i = 0; i < a->args.size(); ++i) {
                            if (i) s += ",";
                            s += pr(a->args[i]);
                        }
                        return s + "\\right)";
                    }
                    auto it = ltx.find(nm);
                    std::string head = it != ltx.end() ? it->second + "\\left(" : "\\mathrm{" + nm + "}\\left(";
                    std::string s = head;
                    for (size_t i = 0; i < a->args.size(); ++i) {
                        if (i) s += ",";
                        s += pr(a->args[i]);
                    }
                    return s + "\\right)";
                }
                if (pretty) {
                    const std::string g = greekSymbolUnicode(nm);
                    if (!g.empty()) nm = g;
                }
                std::string s = nm + "(";
                for (size_t i = 0; i < a->args.size(); ++i) {
                    if (i) s += ",";
                    s += pr(a->args[i]);
                }
                return s + ")";
            }
        }
        return "?";
    }
};

std::string toPlain(const Ex& a) { Printer p; return p.pr(a); }
std::string toPretty(const Ex& a) { Printer p; p.pretty = true; return p.pr(a); }
std::string toLatex(const Ex& a) { Printer p; p.latex = true; return p.pr(a); }
std::string toText(const Ex& a) { return toPretty(a); }

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}
std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    size_t p = 0;
    while ((p = s.find(from, p)) != std::string::npos) {
        s.replace(p, from.size(), to);
        p += to.size();
    }
    return s;
}

} // namespace ode
