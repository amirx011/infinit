// Infinit Lab — expression constructors, canonical form, ordering, printing
// SPDX-License-Identifier: MIT
#include "expr.hpp"
#include <sstream>
#include <cassert>
#include <cstring>
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
                if (n < 0) return Mul(Pow(Num(-1), exp), Pow(Num(Rational(-n, d)), exp));
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
        if (t.n == 0) { v = 0; return true; }
        if (t.n == 2 * t.d) { v = 0; return true; }
        if (t.n == t.d / 2 || (t.d == 2 && t.n == 1)) { v = 1; return true; }
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
        Rational kk = Rational(k.n + k.d, k.d); // cos(k) = sin(k + pi/2)
        Rational s;
        if (exactTrig("sin", kk, s)) { out = s; return true; }
        return false;
    }
    if (fn == "tan") {
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
                if (a->name == "pi") return latex ? "\\pi" : (pretty ? "π" : "pi");
                if (a->name == "e" && pretty) return "e";
                return a->name;
            }
            case Kind::Add: {
                std::string s;
                bool first = true;
                for (auto& t : a->args) {
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
                std::string s = head();
                s += render(num);
                return s;
            }
            case Kind::Pow: {
                const Ex& b = a->args[0];
                const Ex& e = a->args[1];
                if (e->isNum() && e->num.n == 1 && e->num.d == 2) {
                    if (latex) return "\\sqrt{" + pr(b) + "}";
                    return (pretty ? "√" : "sqrt(") + wrap(b, 4, b->isAdd() || b->isMul()) + (pretty ? "" : ")");
                }
                if (e->isNum() && e->num.n == -1 && e->num.d == 2) {
                    if (latex) return "\\frac{1}{\\sqrt{" + pr(b) + "}}";
                    return (pretty ? "1/√" : "1/sqrt(") + wrap(b, 4, b->isAdd() || b->isMul()) + (pretty ? "" : ")");
                }
                if (e->isNum() && e->num.isInt() && e->num.n == -1) {
                    if (latex) return "\\frac{1}{" + pr(b) + "}";
                    if (pretty) return "1/" + wrap(b, 3, b->isAdd());
                    return "1/" + wrap(b, 4, b->isAdd() || b->isMul());
                }
                if (pretty && isSuperscriptableInt(e)) {
                    static const char* sup[] = {"⁰","¹","²","³","⁴","⁵","⁶","⁷","⁸","⁹"};
                    long long n = e->num.n;
                    std::string s;
                    if (n < 0) s += "⁻";
                    n = std::llabs(n);
                    s += sup[n];
                    return wrap(b, 4, b->isAdd() || b->isMul() || b->isPow()) + s;
                }
                if (latex) {
                    return "{" + wrap(b, 4, b->isAdd() || b->isMul() || b->isPow()) + "}^{" + pr(e) + "}";
                }
                if (e->isNum() && e->num == Rational(-1)) {
                    return "1/" + wrap(b, 4, b->isAdd() || b->isMul());
                }
                if (e->isNum() && e->num.isInt()) {
                    return wrap(b, 4, b->isAdd() || b->isMul() || b->isPow()) + "^" + std::to_string(e->num.n);
                }
                return wrap(b, 4, b->isAdd() || b->isMul() || b->isPow()) + "^(" + pr(e) + ")";
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
                if (a->name == "exp") {
                    if (latex) return "e^{" + pr(a->args[0]) + "}";
                    return (pretty ? "exp(" : "exp(") + pr(a->args[0]) + ")";
                }
                if (a->name == "ln" || a->name == "log") {
                    if (latex) return "\\ln\\left(" + pr(a->args[0]) + "\\right)";
                    return "ln(" + pr(a->args[0]) + ")";
                }
                static const std::map<std::string, std::string> ltx = {
                    {"sin","\\sin"},{"cos","\\cos"},{"tan","\\tan"},{"cot","\\cot"},
                    {"sec","\\sec"},{"csc","\\csc"},{"asin","\\arcsin"},{"acos","\\arccos"},
                    {"atan","\\arctan"},{"sinh","\\sinh"},{"cosh","\\cosh"},{"tanh","\\tanh"},
                    {"asinh","\\operatorname{asinh}"},{"acosh","\\operatorname{acosh}"},{"atanh","\\operatorname{atanh}"}
                };
                std::string nm = a->name;
                if (latex) {
                    auto it = ltx.find(nm);
                    std::string head = it != ltx.end() ? it->second + "\\left(" : "\\operatorname{" + nm + "}\\left(";
                    std::string s = head;
                    for (size_t i = 0; i < a->args.size(); ++i) {
                        if (i) s += ",";
                        s += pr(a->args[i]);
                    }
                    return s + "\\right)";
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
