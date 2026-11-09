// Infinit Lab — ODE classification and exact solvers
// SPDX-License-Identifier: Apache-2.0
#include "expr.hpp"
#include "poly.hpp"
#include "solver.hpp"
#include "numeric.hpp"
#include "localize.hpp"
#include <algorithm>
#include <chrono>
#include <complex>
#include <set>
#include <functional>
#include <utility>

namespace ode {

// ---------------------------------------------------------------- helpers
static bool isZero(const Ex& e) {
    try { return simplify(e)->isZero(); } catch (...) { return false; }
}

static Ex Cst(int i = 1) {
    if (i <= 1) return Sym("C");
    return Sym("C" + std::to_string(i));
}

static size_t exprSize(const Ex& a) {
    if (!a) return 0;
    size_t n = 1;
    for (auto& x : a->args) n += exprSize(x);
    return n;
}

static bool dependsOnY(const Ex& e, const std::string& y) { return dependsOn(e, y); }

static bool isLinearIn(const Ex& e, const std::string& y) {
    // e is polynomial of degree <=1 in y
    std::vector<Ex> c;
    if (!polyCoeffs(expand(e), y, c)) return false;
    return c.size() <= 2;
}

struct ExLess {
    bool operator()(const Ex& a, const Ex& b) const { return compareExpr(a, b) < 0; }
};

struct TermPow {
    Ex exp;   // exponent of y
    Ex coeff; // coefficient
};

// decompose expanded expression into sum of coeff * y^exp
static bool powerTerms(const Ex& e0, const std::string& y, std::vector<TermPow>& out) {
    Ex e = expand(e0);
    std::vector<Ex> terms = e->isAdd() ? e->args : std::vector<Ex>{e};
    std::map<Ex, Ex, ExLess> m;
    for (auto& t : terms) {
        std::vector<Ex> facs = t->isMul() ? t->args : std::vector<Ex>{t};
        Ex coeff = Num(1), ex = Num(0);
        for (auto& f : facs) {
            if (f->isSym(y)) { ex = Add(ex, Num(1)); continue; }
            if (f->isPow() && f->args[0]->isSym(y) && f->args[1]->isNum()) {
                ex = Add(ex, f->args[1]);
                continue;
            }
            if (dependsOn(f, y)) return false;
            coeff = Mul(coeff, f);
        }
        auto it = m.find(ex);
        if (it == m.end()) m.emplace(ex, coeff);
        else it->second = Add(it->second, coeff);
    }
    out.clear();
    for (auto& kv : m) {
        if (kv.second->isZero()) continue;
        out.push_back({kv.first, kv.second});
    }
    return true;
}

static Ex coeffOf(const std::vector<TermPow>& ts, long long exp) {
    for (auto& t : ts) if (t.exp->isNum() && t.exp->num == Rational(exp)) return t.coeff;
    return Num(0);
}

static std::string jsonEscapeStr(const std::string& v) {
    std::string out;
    out.reserve(v.size() + 8);
    for (char c : v) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

static std::string pjson(
    std::initializer_list<std::pair<std::string, std::string>> kv) {
    std::string out = "{";
    for (const auto& item : kv) {
        if (out.size() > 1) out += ",";
        out += "\"" + item.first + "\":\"" + jsonEscapeStr(item.second) + "\"";
    }
    out += "}";
    return out;
}

static void addStepKey(Result& r, const std::string& titleKey, const Ex& math = nullptr,
                        const std::string& noteKey = "", const std::string& params = "",
                        const std::string& noteMath = "") {
    Step s;
    s.titleKey = titleKey;
    s.math = math;
    s.noteKey = noteKey;
    s.params = params;
    s.noteMath = noteMath;
    r.steps.push_back(std::move(s));
}

static void addCheckKey(Result& r, const std::string& nameKey, bool ok,
                        const std::string& detailKey = "", const std::string& params = "") {
    Check c;
    c.nameKey = nameKey;
    c.ok = ok;
    c.detailKey = detailKey;
    c.params = params;
    r.checks.push_back(std::move(c));
}

struct Builder {
    Result& r;
    OdeOptions opt;
    bool en = true;
    std::string x, y;
    Ex X, Y, P;

    Builder(Result& res, const OdeOptions& o) : r(res), opt(o), x(o.x), y(o.y) {
        en = o.lang != "fa";
        X = Sym(x); Y = Sym(y);
        P = Deriv(Y, x, 1);
    }

    void step(const std::string& titleKey, const Ex& math, const std::string& noteKey = "",
              const std::string& params = "", const std::string& noteMath = "") {
        if (!opt.steps) return;
        Step s;
        s.titleKey = titleKey;
        s.math = math;
        s.noteKey = noteKey;
        s.params = params;
        s.noteMath = noteMath;
        r.steps.push_back(std::move(s));
    }
    void text(const std::string& titleKey, const std::string& noteKey,
              const std::string& params = "", const std::string& noteMath = "") {
        step(titleKey, nullptr, noteKey, params, noteMath);
    }
    void warn(const std::string& key) { r.warnings.push_back(key); }
    void assume(const std::string& key) { r.assumptions.push_back(key); }
    void check(const std::string& nameKey, bool ok, const std::string& detailKey,
               const std::string& params = "") {
        Check c;
        c.nameKey = nameKey;
        c.ok = ok;
        c.detailKey = detailKey;
        c.params = params;
        r.checks.push_back(std::move(c));
    }
    void fail(const std::string& code, const std::string& params = "") {
        r.ok = false;
        r.errorCode = code;
        r.errorParams = params;
    }
};

static Ex expOf(const Ex& e) { return Fn("exp", {e}); }
static Ex lnAbs(const Ex& e) { return Fn("ln", {Fn("abs", {e})}); }

// solve y' + P y = Q exactly
static Ex solveLinearODE(const std::string& x, const Ex& Pcoef, const Ex& Q, Ex* muOut = nullptr,
                         Ex* intOut = nullptr) {
    Ex X = Sym(x);
    bool closed1 = false, closed2 = false;
    Ex intP = integrate(Pcoef, x, &closed1);
    // standard convention: x > 0 for the integrating factor
    {
        std::function<Ex(const Ex&)> pos = [&](const Ex& e) -> Ex {
            if (!e) return e;
            // μ is only defined up to sign, so drop every absolute value.
            if (e->isFunc("abs") && e->args.size() == 1) return e->args[0];
            if (e->args.empty()) return e;
            std::vector<Ex> v;
            for (auto& a : e->args) v.push_back(pos(a));
            switch (e->kind) {
                case Kind::Add: return Add(v);
                case Kind::Mul: return Mul(v);
                case Kind::Pow: return Pow(v[0], v[1]);
                case Kind::Func: return Fn(e->name, v);
                default: return e;
            }
        };
        intP = pos(intP);
    }
    Ex mu = simplify(expOf(intP));
    Ex rhs = simplify(Mul(mu, Q));
    Ex intRhs = integrate(rhs, x, &closed2);
    Ex sol = simplify(Mul(Add(Cst(1), intRhs), Pow(mu, Num(-1))));
    // Distribute the integrating factor when it collapses the answer
    // (e.g. e^{x²/2+x}(C + 2e^{-x²/2-x}) -> C·e^{x²/2+x} + 2).
    if (exprSize(sol) < 600) {
        Ex ex = simplify(expand(sol));
        if (exprSize(ex) < exprSize(sol)) sol = ex;
    }
    if (muOut) *muOut = mu;
    if (intOut) *intOut = intRhs;
    (void)closed1; (void)closed2; (void)X;
    return sol;
}

// try to turn an implicit equation expr = C into y = ...
static Ex numeratorOf(const Ex& e) {
    if (!e) return e;
    if (e->isPow() && e->args[1]->isNum() && e->args[1]->num.isInt() && e->args[1]->num.n < 0) return Num(1);
    if (e->isMul()) {
        std::vector<Ex> v;
        for (auto& f : e->args) {
            Ex n = numeratorOf(f);
            if (!n->isOne()) v.push_back(n);
        }
        return v.empty() ? Num(1) : Mul(v);
    }
    return e;
}

static Ex tryExplicit(const Ex& eq, const std::string& x, const std::string& y,
                      bool allowEvenPower = false) {
    Ex s = simplify(eq);
    if (!dependsOn(s, y)) return nullptr;
    Ex lin = solveLinear(s, y);
    if (lin) return simplify(lin);
    // Clear denominators and retry: e.g. −x − 1/y = C becomes linear in y.
    try {
        Ex frac = together(s);
        Ex num = numeratorOf(frac);
        if (num && !equals(num, s)) {
            Ex l2 = solveLinear(num, y);
            if (l2) return simplify(l2);
        }
    } catch (...) {}
    // logarithm form: sum of terms; isolate k*ln|y| (+ const in y) = rest(x)
    Ex e = expand(s);
    std::vector<Ex> terms = e->isAdd() ? e->args : std::vector<Ex>{e};
    Ex ypart = Num(0), rest = Num(0);
    for (auto& t : terms) {
        if (dependsOn(t, y)) ypart = Add(ypart, t);
        else rest = Add(rest, t);
    }
    if (ypart->isZero()) return nullptr;
    // pattern: k*ln(abs(y))
    Rational k(1);
    bool ok = false;
    std::vector<Ex> yf = ypart->isMul() ? ypart->args : std::vector<Ex>{ypart};
    for (auto& f : yf) {
        if (f->isNum()) { k = k * f->num; continue; }
        if (f->isFunc("ln") && f->args[0]->isFunc("abs") && f->args[0]->args[0]->isSym(y)) { ok = true; continue; }
        if (f->isFunc("ln") && f->args[0]->isSym(y)) { ok = true; continue; }
        ok = false;
        break;
    }
    if (ok && !k.isZero()) {
        // k ln|y| + rest = 0 -> |y| = exp(-rest/k) -> y = C exp(...)
        Ex arg = simplify(Mul(Num(-1), rest, Pow(Num(k), Num(-1))));
        return simplify(Mul(Cst(1), expOf(arg)));
    }
    // single invertible function of y: k·F(y) + rest = 0
    try {
        Ex kk = Num(1);
        std::string fname;
        Ex farg;
        std::vector<Ex> mf = ypart->isMul() ? ypart->args : std::vector<Ex>{ypart};
        bool good = true;
        for (auto& f : mf) {
            // A factor that does not depend on y is part of the coefficient,
            // even when it is symbolic (for example sqrt(2/C)).
            if (!dependsOn(f, y)) { kk = Mul(kk, f); continue; }
            if (f->isFunc() && f->args.size() == 1 && dependsOn(f->args[0], y) && fname.empty()) {
                fname = f->name;
                farg = f->args[0];
                continue;
            }
            good = false;
            break;
        }
        static const std::map<std::string, std::string> inv = {
            {"atan", "tan"}, {"tan", "atan"}, {"asin", "sin"}, {"sin", "asin"},
            {"acos", "cos"}, {"cos", "acos"}, {"asinh", "sinh"}, {"sinh", "asinh"},
            {"acosh", "cosh"}, {"cosh", "acosh"}, {"atanh", "tanh"}, {"tanh", "atanh"},
            {"exp", "ln"}
        };
        auto it = inv.find(fname);
        if (good && it != inv.end() && !kk->isZero()) {
            Ex rhs = simplify(expand(Mul(Num(-1), rest, Pow(kk, Num(-1)))));
            Ex target = simplify(Fn(it->second, {rhs}));
            Ex ysol = solveLinear(Add(farg, Mul(Num(-1), target)), y);
            if (ysol) return simplify(ysol);
        }
    } catch (...) {}
    // A single power of y: k·y^n + rest = 0, including negative n such as
    // p^(-2) from the separable equation p' = p^3.
    try {
        Ex kk = Num(1), base = nullptr, expo = nullptr;
        std::vector<Ex> mf = ypart->isMul() ? ypart->args : std::vector<Ex>{ypart};
        bool good = true;
        for (auto& f : mf) {
            if (!dependsOn(f, y)) { kk = Mul(kk, f); continue; }
            if (f->isSym(y) && !base) { base = f; expo = Num(1); continue; }
            if (f->isPow() && f->args[0]->isSym(y) && f->args[1]->isNum() && !base) {
                base = f->args[0];
                expo = f->args[1];
                continue;
            }
            good = false;
            break;
        }
        // Isolating an even positive power would lose one branch (y² = R
        // gives y = ±√R), so by default only odd or negative powers are
        // inverted.  The order-reduction path opts in because it needs one
        // branch of p to continue and the other is recovered by the final
        // integration constant.
        const bool invertible = expo && !expo->num.isZero() &&
                                (allowEvenPower || expo->num.isNeg() ||
                                 expo->num.n % 2 != 0);
        if (good && base && invertible && !kk->isZero()) {
            Ex rhs = simplify(expand(Mul(Num(-1), rest, Pow(kk, Num(-1)))));
            Ex inv = Pow(expo, Num(-1));
            return simplify(Pow(rhs, inv));
        }
    } catch (...) {}
    return nullptr;
}

// ---------------------------------------------------------------- simplification retry
// When none of the supported forms matches, try a small set of exact
// normalisations (expand, together, clear denominators) and run the full
// classifier again on each distinct candidate.
static Result trySimplifiedFallback(const Ex& F, const OdeOptions& opt) {
    if (opt.simplifyAttempts >= 2) return Result{};
    std::vector<Ex> candidates;
    std::set<std::string> dividedKeys;
    auto add = [&](const Ex& e) {
        if (!e) return;
        try {
            Ex s = simplify(e);
            std::string key = toPlain(s);
            if (key.empty()) return;
            for (auto& old : candidates)
                if (toPlain(old) == key) return;
            candidates.push_back(s);
        } catch (...) {}
    };
    // Dividing a common factor out of F=0 preserves the solution curves
    // (except on the zero set of the factor).  This is often what turns a
    // scaled exact/linear equation into its recognizable form.
    auto addDivisions = [&](const Ex& e) {
        if (!e || !e->isMul()) return;
        for (auto& factor : e->args) {
            if (factor->isNum() || (factor->isPow() && factor->args[1]->isNum() &&
                                    factor->args[1]->num.isNeg()))
                continue;
            try {
                Ex q = simplify(Mul(e, Pow(factor, Num(-1))));
                std::string key = toPlain(q);
                if (key.empty() || key == toPlain(F)) continue;
                dividedKeys.insert(key);
                add(q);
            } catch (...) {}
        }
    };
    try {
        Ex s0 = simplify(F);
        add(s0);
        addDivisions(s0);
        Ex e0 = simplify(expand(F));
        add(e0);
        addDivisions(e0);
        Ex t0 = together(s0);
        add(t0);
        addDivisions(t0);
        Ex n0 = numeratorOf(t0);
        add(n0);
        addDivisions(n0);
        add(expand(n0));
        Ex t1 = together(e0);
        add(t1);
        addDivisions(t1);
        add(numeratorOf(t1));
    } catch (...) {
        return Result{};
    }

    const std::string original = toPlain(F);
    for (auto& candidate : candidates) {
        std::string text = toPlain(candidate);
        if (text == original || text.size() > 2000) continue;
        OdeOptions o = opt;
        ++o.simplifyAttempts;
        Result rr = solveODE(text, o);
        if (rr.ok) {
            if (rr.steps.empty() || rr.steps.front().titleKey != "ode.t.simplification") {
                Step s;
                s.titleKey = "ode.t.simplification";
                s.math = candidate;
                s.noteKey = "ode.n.simplified";
                rr.steps.insert(rr.steps.begin(), s);
            }
            if (dividedKeys.count(text))
                rr.warnings.push_back("ode.w1");
            return rr;
        }
    }
    return Result{};
}

// ---------------------------------------------------------------- verification
void verifyExplicit(Result& r, const Ex& F, const Ex& yExpr, const std::string& x,
                    const std::string& y, bool en) {
    if (!r.checks.empty()) return;
    try {
        // The caller normally sets r.order, but the helper is also used
        // directly in tests; derive the required order from F when needed.
        int neededOrder = r.order;
        std::function<void(const Ex&)> scan = [&](const Ex& e) {
            if (!e) return;
            if (isDeriv(e) && e->args[1]->isSym(x)) neededOrder = std::max(neededOrder, derivOrder(e));
            for (auto& c : e->args) scan(c);
        };
        scan(F);
        std::map<std::string, Ex> sub;
        sub[y] = yExpr;
        Ex cur = yExpr;
        for (int k = 1; k <= neededOrder; ++k) {
            cur = simplify(diff(cur, x));
            sub["__d" + std::to_string(k)] = cur;
        }
        Ex Fx = F;
        // replace Deriv nodes by their explicit values
        std::function<Ex(const Ex&)> repl = [&](const Ex& e) -> Ex {
            if (!e) return e;
            if (isDeriv(e) && e->args[0]->isSym(y) && e->args[1]->isSym(x)) {
                int o = derivOrder(e);
                auto it = sub.find("__d" + std::to_string(o));
                return it != sub.end() ? it->second : e;
            }
            if (e->args.empty()) return e;
            std::vector<Ex> v;
            for (auto& a : e->args) v.push_back(repl(a));
            switch (e->kind) {
                case Kind::Add: return Add(v);
                case Kind::Mul: return Mul(v);
                case Kind::Pow: return Pow(v[0], v[1]);
                case Kind::Func: return Fn(e->name, v);
                default: return e;
            }
        };
        Ex res = simplify(repl(Fx));
        sub.erase(y);
        bool ok = res->isZero();
        if (!ok) {
            Ex chk = simplify(expand(substitute(res, y, yExpr)));
            // Sample the arbitrary constants too, not only x.  A solution such
            // as y = C2 + x - (C+x)ln(C+x) cannot be evaluated numerically
            // while C and C2 remain free symbols.
            std::vector<std::string> nsyms;
            collectSymbols(chk, nsyms);
            ok = chk->isZero() || numericallyZero(chk, nsyms, 8, 1e-6);
        }
        addCheckKey(r, "ode.c_substitution", ok,
                    ok ? "ode.c_substitution_ok" : "ode.c_substitution_fail");
    } catch (...) {
        addCheckKey(r, "ode.c_substitution", false, "ode.c_automatic_unavailable");
    }
}

static void verifyImplicit(Result& r, const Ex& u, const Ex& M, const Ex& N, const std::string& x,
                           const std::string& y, bool en = true) {
    try {
        Ex ux = simplify(diff(u, x));
        Ex uy = simplify(diff(u, y));
        Ex rel = simplify(Add(Mul(ux, N), Mul(Num(-1), Mul(uy, M))));
        std::vector<std::string> nsyms;
        collectSymbols(rel, nsyms);
        bool ok = rel->isZero() || numericallyZero(rel, nsyms, 8, 1e-6);
        addCheckKey(r, "ode.c_exactness", ok,
                    ok ? "ode.c_exact_ok" : "ode.c_exact_fail");
    } catch (...) {
        addCheckKey(r, "ode.c_exactness", false, "ode.c_automatic_unavailable");
    }
}

// ---------------------------------------------------------------- differential form
static bool extractMN(const Ex& F, const std::string& x, const std::string& y, Ex& M, Ex& N) {
    if (!F) return false;
    std::vector<std::string> syms;
    collectSymbols(F, syms);
    bool hasDx = false, hasDy = false;
    for (auto& s : syms) { if (s == "dx") hasDx = true; if (s == "dy") hasDy = true; }
    if (!hasDx || !hasDy) return false;
    Ex t = Sym("__q");
    Ex G = substituteSubexpr(F, Sym("dx"), t);
    std::vector<Ex> cs;
    if (!polyCoeffs(expand(G), "__q", cs) || cs.size() != 2) return false;
    M = cs[1];
    Ex rest = cs[0];
    if (dependsOn(rest, "dx")) return false;
    std::vector<Ex> cs2;
    if (!polyCoeffs(expand(rest), "dy", cs2)) return false;
    if (cs2.size() > 2) return false;
    N = cs2.size() == 2 ? cs2[1] : Num(0);
    if (cs2.size() >= 1 && !cs2[0]->isZero()) return false;
    if (!M || !N) return false;
    if (dependsOn(M, "dx") || dependsOn(M, "dy") || dependsOn(N, "dy")) return false;
    return true;
}

// ---------------------------------------------------------------- particular solution (Riccati)
static Ex findParticular(const Ex& Pc, const Ex& Q, const Ex& R, const std::string& x,
                         const std::string& y, const Ex& yp) {
    Ex X = Sym(x);
    if (R->isZero()) return Num(0);
    // constant coefficients -> real equilibrium roots
    if (Pc->isNum() && Q->isNum() && R->isNum()) {
        auto roots = solvePoly(Add({Mul(Pc, Pow(Sym(y), Num(2))), Mul(Q, Sym(y)), R}), y);
        for (auto& rr : roots) if (rr && rr->isNum()) return rr;
    }
    std::vector<Ex> cands = {
        Num(0), Num(1), Num(-1), Num(2), Num(-2),
        X, Mul(Num(-1), X), Add(X, Num(1)), Add(X, Num(-1)),
        Pow(X, Num(2)), Pow(X, Num(-1)),
        expOf(X), expOf(Mul(Num(-1), X)),
        Fn("sin", {X}), Fn("cos", {X}), Fn("tan", {X}), Fn("ln", {X}),
        Sqrt(X), Mul(Num(2), X), Add(Mul(Num(2), X), Num(1))
    };
    for (auto& c : cands) {
        try {
            Ex rhs = Add(Mul(Pc, Pow(c, Num(2))), Mul(Q, c), R);
            // A particular solution must satisfy the ODE itself, so its
            // derivative (not a formal substitution of y′) is the left side.
            Ex lhs = simplify(diff(c, x));
            Ex res = simplify(Add(lhs, Mul(Num(-1), rhs)));
            if (!dependsOn(res, y) && (res->isZero() || numericallyZero(res, {x}, 6, 1e-7))) return c;
        } catch (...) {}
    }
    // linear ansatz y = a x + b, constants a,b (search small rationals)
    for (int a = -3; a <= 3; ++a) {
        for (int b = -3; b <= 3; ++b) {
            Ex c = Add(Mul(Num(a), X), Num(b));
            try {
                Ex rhs = Add(Mul(Pc, Pow(c, Num(2))), Mul(Q, c), R);
                Ex lhs = simplify(diff(c, x));
                Ex res = simplify(Add(lhs, Mul(Num(-1), rhs)));
                if (!dependsOn(res, y) && (res->isZero() || numericallyZero(res, {x}, 6, 1e-7))) return c;
            } catch (...) {}
        }
    }
    (void)y;
    return nullptr;
}

// ---------------------------------------------------------------- first order
static void setSolution(Result& r, const std::string& id, const Ex& sol,
                        bool explicitForm, const Ex& yExpr = nullptr) {
    r.ok = true;
    r.typeId = id;
    r.solution = sol;
    r.isExplicit = explicitForm;
    r.explicitY = explicitForm ? (yExpr ? yExpr : sol) : nullptr;
    r.closedForm = !hasUnevaluatedIntegral(sol) && (!r.explicitY || !hasUnevaluatedIntegral(r.explicitY));
}

static bool tryClairautLagrange(Builder& b, const Ex& F, const Ex& p) {
    // Clairaut/Lagrange equations are nonlinear in y′ (degree ≥ 2, or
    // transcendental). A degree-1 equation such as (2x+y)+(x+2y)y′=0 is a
    // linear/exact equation and must be handled by those solvers.
    {
        Ex t = Sym("__p");
        Ex G = substituteSubexpr(F, p, t);
        std::vector<Ex> pc;
        if (polyCoeffs(expand(G), "__p", pc) && pc.size() <= 2) return false;
    }
    Ex ysol = solveLinear(F, b.y);
    if (!ysol) return false;
    Ex ye = expand(ysol);
    std::vector<Ex> cs;
    if (!polyCoeffs(ye, b.x, cs) || cs.size() != 2) return false;
    Ex g = simplify(cs[1]);
    Ex f = simplify(cs[0]);
    if (dependsOn(g, b.x) || dependsOn(f, b.x)) return false;
    if (g->isZero()) return false;
    bool gHasP = false;
    std::function<void(const Ex&)> scanP = [&](const Ex& e) {
        if (!e) return;
        if (isDeriv(e) && e->args[0]->isSym(b.y)) gHasP = true;
        for (auto& c : e->args) scanP(c);
    };
    scanP(g);
    if (!gHasP) return false;
    bool clairaut = equals(g, p);
    if (!clairaut) {
        // y = x·g(p) + f(p). If both g and f are at most linear in p the
        // equation is really a linear first-order ODE (e.g. 2y + x y′ = 0),
        // so let the linear/separable solvers handle it.
        std::vector<Ex> gc, fc;
        Ex gg0 = substituteSubexpr(g, p, Sym("p"));
        Ex ff0 = substituteSubexpr(f, p, Sym("p"));
        bool gLin = polyCoeffs(expand(gg0), "p", gc) && gc.size() <= 2;
        bool fLin = polyCoeffs(expand(ff0), "p", fc) && fc.size() <= 2;
        if (gLin && fLin) return false;
    }
    const size_t stepMark = b.r.steps.size();
    const size_t warnMark = b.r.warnings.size();
    b.r.order = 1;
    Ex tp = Sym("p");
    Ex gg = substituteSubexpr(g, p, tp);
    Ex ff = substituteSubexpr(f, p, tp);
    Ex gp0 = simplify(diff(gg, "p"));
    Ex fp0 = simplify(diff(ff, "p"));

    if (clairaut) {
        b.step("ode.t.type_identification", Add({Mul(Sym(b.x), gg), ff}),
               "ode.n.clairaut_form", pjson({{"f", toPretty(ff)}}));
    } else {
        b.step("ode.t.type_identification", Add({Mul(Sym(b.x), gg), ff}),
               "ode.n.lagrange_form",
               pjson({{"g", toPretty(gg)}, {"f", toPretty(ff)}}));
    }
    b.step("ode.t2", Add({Sym(b.y), Mul(Num(-1), Mul(Sym(b.x), gg)), Mul(Num(-1), ff)}), "ode.n1", pjson({{"n0", std::string(clairaut ? "p" : "g(p)")}}));

    if (clairaut) {
        b.step("ode.t3", Mul(Add({Sym(b.x), fp0}), Deriv(tp, b.x, 1)), "ode.n2");

        Ex gen = Add(Mul(Cst(1), Sym(b.x)), substituteSubexpr(f, p, Cst(1)));
        b.step("ode.t4", gen, "ode.n3");

        b.r.singular.clear();
        try {
            Ex fp = simplify(diff(ff, "p"));
            if (fp->isZero()) {
                b.step("ode.t5", nullptr, "ode.n4");
            } else {
                Ex px = Mul(Num(-1), fp);
                Ex py = simplify(Add(ff, Mul(Num(-1), Mul(tp, fp))));
                Ex elim = solveLinear(Add(px, Mul(Num(-1), Sym(b.x))), "p");
                if (elim) {
                    Ex sing = simplify(substituteSubexpr(py, tp, elim));
                    b.r.singular.push_back(sing);
                    b.step("ode.t6", sing, "ode.n5", pjson({{"n0", toPretty(px)}, {"n1", toPretty(py)}, {"n2", toPretty(elim)}}));
                } else {
                    b.r.paramX = px;
                    b.r.paramY = py;
                    b.r.paramName = "p";
                    b.r.paramSingular = true;
                    b.step("ode.t7", nullptr, "ode.n6", pjson({{"n0", toPretty(px)}, {"n1", toPretty(py)}}));
                }
            }
        } catch (...) {}
        setSolution(b.r, "clairaut", gen, true, gen);
        b.r.normalized = F;
        verifyExplicit(b.r, F, gen, b.x, b.y, b.en);
        return true;
    }

    // Lagrange: dx/dp - g'/(p-g) x = f'/(p-g)
    try {
        Ex gp = simplify(diff(gg, "p"));
        Ex fp = simplify(diff(ff, "p"));
        Ex den = simplify(Add(tp, Mul(Num(-1), gg)));
        if (den->isZero()) {
            b.r.steps.resize(stepMark);
            b.r.warnings.resize(warnMark);
            return false;
        }
        Ex Pc = simplify(Mul(Num(-1), gp, Pow(den, Num(-1))));
        Ex Qc = simplify(Mul(fp, Pow(den, Num(-1))));
        Ex mu, intRhs;
        Ex xp = solveLinearODE("p", Pc, Qc, &mu, &intRhs);
        (void)intRhs;
        Ex yp = simplify(Add(Mul(xp, gg), ff));
        b.step("ode.t8", Add({Deriv(Sym("x"), "p", 1), Mul(Pc, Sym("x")), Mul(Num(-1), Qc)}), "ode.n7", pjson({{"n0", toPretty(Pc)}, {"n1", toPretty(Qc)}}));
        b.step("ode.t9", xp, "ode.n8", pjson({{"n0", toPretty(mu)}}));
        b.step("ode.t10", yp, "ode.n9");
        b.r.paramX = xp;
        b.r.paramY = yp;
        b.r.paramName = "p";
        b.r.ok = true;
        b.r.typeId = "lagrange";
        b.r.closedForm = !hasUnevaluatedIntegral(xp) && !hasUnevaluatedIntegral(yp);
        b.r.normalized = F;
        // singular: p0 with g(p0)=p0
        auto roots = solvePoly(den, "p");
        for (auto& rr : roots) {
            Ex yy = simplify(Add(Mul(rr, Sym(b.x)), substituteSubexpr(ff, tp, rr)));
            b.r.singular.push_back(yy);
        }
        return true;
    } catch (...) {
        b.r.steps.resize(stepMark);
        b.r.warnings.resize(warnMark);
        return false;
    }
}

static bool tryRiccati(Builder& b, const Ex& yp, const std::vector<TermPow>& ts) {
    size_t stepMark = b.r.steps.size();
    size_t warnMark = b.r.warnings.size();
    Ex Pc = coeffOf(ts, 2);
    if (Pc->isZero()) return false;
    Ex Q = coeffOf(ts, 1);
    Ex R = coeffOf(ts, 0);
    for (auto& t : ts) {
        if (!t.exp->isNum() || t.exp->num.isInt() == false) return false;
        long long n = t.exp->num.n;
        if (n < 0 || n > 2) return false;
    }
    b.r.order = 1;
    b.step("ode.t11", Add({Mul(Pc, Pow(b.Y, Num(2))), Mul(Q, b.Y), R}), "ode.n10", pjson({{"n0", toPretty(Pc)}, {"n1", toPretty(Q)}, {"n2", toPretty(R)}}));
    Ex y1;
    if (!b.opt.particular.empty()) {
        std::string dep;
        y1 = parseODEExpr(b.opt.particular, b.x, &dep, {b.x, b.y, "c"});
    } else {
        y1 = findParticular(Pc, Q, R, b.x, b.y, yp);
    }
    if (y1) {
        b.step("ode.t12", y1, "ode.n11");
        Ex A = simplify(Add(Mul(Num(2), Pc, y1), Q));
        Ex B = simplify(Mul(Num(-1), Pc));
        Ex z = solveLinearODE(b.x, A, B);
        Ex ysol = simplify(Add(y1, Pow(z, Num(-1))));
        b.step("ode.t13", Add({Deriv(Sym("z"), b.x, 1), Mul(A, Sym("z")), Mul(Num(-1), B)}), "ode.n12");
        b.step("ode.t14", z);
        b.step("ode.t15", ysol);
        b.r.normalized = yp;
        setSolution(b.r, "riccati", ysol, true, ysol);
        if (b.r.closedForm) {
            verifyExplicit(b.r, Add(b.P, Mul(Num(-1), Add(Mul(Pc, Pow(Sym(b.y), Num(2))), Mul(Q, Sym(b.y)), R))),
                           ysol, b.x, b.y, b.en);
        } else {
            b.warn("ode.w1");
        }
        return true;
    }
    // transform to second order linear: y = -u'/(P u)
    try {
        Ex Pp = simplify(diff(Pc, b.x));
        Ex a1 = simplify(Mul(Num(-1), Add(Q, Mul(Pp, Pow(Pc, Num(-1))))));
        Ex a0 = simplify(Mul(Pc, R));
        b.step("ode.t16", nullptr, "ode.n13");
        b.step("ode.t.second_order_linear", nullptr, "", "",
               "u'' + (" + toPretty(a1) + ")u' + (" + toPretty(a0) + ")u = 0");
        if (a1->isNum() && a0->isNum()) {
            Rational A1 = a1->num, A0 = a0->num;
            Rational disc = A1 * A1 - A0 * Rational(4);
            Ex X = Sym(b.x);
            Ex u = nullptr;
            if (disc.isZero()) {
                Rational r = -A1 * Rational(1, 2);
                u = Add(Mul(Cst(1), expOf(Mul(Num(r), X))),
                        Mul(Cst(2), X, expOf(Mul(Num(r), X))));
            } else if (disc.isNeg()) {
                Rational alpha = -A1 * Rational(1, 2);
                Ex beta = simplify(Mul(Sqrt(Num(-disc)), Num(Rational(1, 2))));
                Ex env = expOf(Mul(Num(alpha), X));
                u = Add(Mul(Cst(1), env, Fn("cos", {Mul(beta, X)})),
                        Mul(Cst(2), env, Fn("sin", {Mul(beta, X)})));
            } else {
                Ex sq = Sqrt(Num(disc));
                Ex r1 = simplify(Mul(Add(Num(-A1), sq), Num(Rational(1, 2))));
                Ex r2 = simplify(Mul(Add(Num(-A1), Mul(Num(-1), sq)), Num(Rational(1, 2))));
                u = Add(Mul(Cst(1), expOf(Mul(r1, X))),
                        Mul(Cst(2), expOf(Mul(r2, X))));
            }
            if (u) {
                Ex up = simplify(diff(u, b.x));
                Ex ysol = simplify(Mul(Num(-1), up, Pow(Mul(Pc, u), Num(-1))));
                b.step("ode.t17", ysol, "ode.n14");
                setSolution(b.r, "riccati", ysol, true, ysol);
                return true;
            }
        }
        b.warn("ode.w2");
        b.r.steps.resize(stepMark);
        b.r.warnings.resize(warnMark);
        return false;
    } catch (...) {
        b.r.steps.resize(stepMark);
        b.r.warnings.resize(warnMark);
        return false;
    }
}

static bool tryBernoulli(Builder& b, const Ex& yp, const std::vector<TermPow>& ts) {
    if (!coeffOf(ts, 0)->isZero()) return false;
    Ex pcoef = coeffOf(ts, 1);
    Ex nEx = nullptr, qcoef = nullptr;
    int count = 0;
    for (auto& t : ts) {
        if (!t.exp->isNum()) return false;
        if (t.exp->num == Rational(0)) continue;
        if (t.exp->num == Rational(1)) continue;
        nEx = t.exp; qcoef = t.coeff;
        ++count;
    }
    if (!nEx || count != 1) return false;
    Rational n = nEx->num;
    // y' = -pcoef*y + qcoef*y^n  => y' + P y = Q y^n with P = pcoef, Q = qcoef
    Ex Pc = pcoef;
    Ex Q = qcoef;
    b.r.order = 1;
    b.step("ode.t18", Add({Mul(Pc, b.Y), Mul(Q, Pow(b.Y, nEx))}), "ode.n15", pjson({{"n0", rationalToString(n)}, {"n1", toPretty(Pc)}, {"n2", toPretty(Q)}}));
    // v = y^(1-n)
    Ex oneMinusN = Num(Rational(1) - n);
    Ex A = simplify(Mul(Num(n - Rational(1)), Pc));   // (1-n)*(-P)
    Ex B = simplify(Mul(oneMinusN, Q));
    Ex mu, intRhs;
    Ex vz = solveLinearODE(b.x, A, B, &mu, &intRhs);
    Ex ysol = simplify(Pow(vz, Pow(oneMinusN, Num(-1))));
    b.step("ode.t19", Add({Deriv(Sym("v"), b.x, 1), Mul(A, Sym("v")), Mul(Num(-1), B)}), "ode.n16", pjson({{"n0", toPretty(A)}, {"n1", toPretty(B)}}));
    b.step("ode.t20", vz, "ode.n17");
    // For odd integer n the power 1−n is even, so y ↦ y^(1−n) is not
    // one-to-one: solving for y explicitly would silently discard the
    // negative branch.  Keep the complete family implicitly instead.
    const bool losesBranch = n.isInt() && (n.n % 2 != 0);
    if (losesBranch) {
        // The linear solution is v = (C + intRhs)/mu, so the constant-free
        // family relation is mu·v - intRhs = C.  The UI appends `= C`.
        Ex implicit = simplify(Add(Mul(mu, Pow(b.Y, oneMinusN)),
                                   Mul(Num(-1), intRhs)));
        b.step("ode.t21", implicit, "ode.n18");
        b.r.singular.push_back(Num(0));
        setSolution(b.r, "bernoulli", implicit, false, nullptr);
        verifyImplicit(b.r, implicit,
                       Mul(Num(-1), Add(Mul(Pc, Sym(b.y)), Mul(Q, Pow(Sym(b.y), nEx)))),
                       Num(1), b.x, b.y, b.en);
        return true;
    }
    b.step("ode.t22", ysol, "ode.n19");
    b.r.singular.push_back(Num(0));
    setSolution(b.r, "bernoulli", ysol, true, ysol);
    if (b.r.closedForm)
        verifyExplicit(b.r, Add(b.P, Mul(Num(-1), Add(Mul(Pc, Sym(b.y)), Mul(Q, Pow(Sym(b.y), nEx))))),
                       ysol, b.x, b.y, b.en);
    else
        b.warn("ode.w3");
    return true;
}

static bool tryLinear(Builder& b, const Ex& yp, const std::vector<TermPow>& ts) {
    for (auto& t : ts) {
        if (!t.exp->isNum() || !t.exp->num.isInt()) return false;
        long long n = t.exp->num.n;
        if (n < 0 || n > 1) return false;
    }
    Ex pcoef = coeffOf(ts, 1);
    Ex q = coeffOf(ts, 0);
    Ex P = simplify(Mul(Num(-1), pcoef));
    b.r.order = 1;
    b.step("ode.t23", Add({b.P, Mul(P, b.Y), Mul(Num(-1), q)}), "ode.n20", pjson({{"n0", toPretty(P)}, {"n1", toPretty(q)}}));
    Ex mu, intRhs;
    Ex ysol = solveLinearODE(b.x, P, q, &mu, &intRhs);
    b.step("ode.t24", mu, "ode.n21");
    b.step("ode.t25", ysol, "ode.n22", pjson({{"n0", toPretty(intRhs)}}));
    setSolution(b.r, "linear", ysol, true, ysol);
    if (b.r.closedForm)
        verifyExplicit(b.r, Add(b.P, Mul(Num(-1), Add(Mul(pcoef, Sym(b.y)), q))), ysol, b.x, b.y, b.en);
    else
        b.warn("ode.w4");
    return true;
}

static std::vector<Ex> subCandidates(const Ex& a, const Ex& whole) {
    std::vector<Ex> out;
    std::function<void(const Ex&)> rec = [&](const Ex& e) {
        if (!e || e->args.empty()) return;
        if (!equals(e, whole)) {
            bool dup = false;
            for (auto& c : out) if (equals(c, e)) { dup = true; break; }
            if (!dup) out.push_back(e);
        }
        for (auto& x : e->args) rec(x);
    };
    rec(a);
    return out;
}

// Denominator of a rational expression, used by the affine-shift detector.
static Ex denominatorOf(const Ex& e) {
    if (!e) return Num(1);
    if (e->isPow() && e->args[1]->isNum() && e->args[1]->num.isInt() &&
        e->args[1]->num.n < 0)
        return Pow(e->args[0], Num(-e->args[1]->num.n));
    if (e->isMul()) {
        std::vector<Ex> v;
        for (auto& f : e->args) {
            Ex d = denominatorOf(f);
            if (!d->isOne()) v.push_back(d);
        }
        return v.empty() ? Num(1) : Mul(v);
    }
    return Num(1);
}

// Read e = a x + b y + c with constant a, b, c.
static bool affineCoeffs(const Ex& e, const std::string& x, const std::string& y,
                         Ex& a, Ex& b, Ex& c) {
    std::vector<Ex> cy;
    if (!polyCoeffs(expand(e), y, cy) || cy.size() > 2) return false;
    if (cy.size() == 2 && dependsOn(cy[1], x)) return false;
    b = cy.size() == 2 ? cy[1] : Num(0);
    std::vector<Ex> cx;
    if (!polyCoeffs(expand(cy[0]), x, cx) || cx.size() > 2) return false;
    a = cx.size() == 2 ? cx[1] : Num(0);
    c = cx[0];
    if (dependsOn(a, x) || dependsOn(a, y) || dependsOn(b, x) || dependsOn(b, y) ||
        dependsOn(c, x) || dependsOn(c, y))
        return false;
    return true;
}

// y' = (a x + b y + c)/(d x + e y + f) with non-parallel lines is reduced to
// a homogeneous equation by moving the origin to the intersection point.
static bool tryAffineHomogeneous(Builder& b, const Ex& yp) {
    Ex frac = together(yp);
    Ex num = numeratorOf(frac);
    Ex den = denominatorOf(frac);
    if (!num || !den || den->isOne()) return false;
    Ex a, bb, c, d, e, f;
    if (!affineCoeffs(num, b.x, b.y, a, bb, c)) return false;
    if (!affineCoeffs(den, b.x, b.y, d, e, f)) return false;
    if (c->isZero() && f->isZero()) return false;  // already homogeneous
    Ex det = simplify(Add(Mul(a, e), Mul(Num(-1), Mul(bb, d))));
    if (det->isZero()) return false;               // parallel lines
    Ex x0 = simplify(Mul(Add(Mul(bb, f), Mul(Num(-1), Mul(c, e))), Pow(det, Num(-1))));
    Ex y0 = simplify(Mul(Add(Mul(c, d), Mul(Num(-1), Mul(a, f))), Pow(det, Num(-1))));
    Ex X = Add(Sym(b.x), Mul(Num(-1), x0));
    Ex Y = Add(Sym(b.y), Mul(Num(-1), y0));
    // The recursive solve is in fresh variables; use the homogeneous form
    // (a x + b y)/(d x + e y) and shift its answer back afterwards.
    Ex rhs = simplify(Mul(Add(Mul(a, Sym(b.x)), Mul(bb, Sym(b.y))),
                          Pow(Add(Mul(d, Sym(b.x)), Mul(e, Sym(b.y))), Num(-1))));
    Result rr = solveODE("y' = " + toPlain(rhs), b.opt);
    if (!rr.ok) return false;
    auto back = [&](const Ex& ex) {
        if (!ex) return ex;
        return substitute(substitute(ex, b.x, X), b.y, Y);
    };
    if (rr.isExplicit) {
        // Y(X) is the solution in shifted coordinates; the original answer is
        // y = y0 + Y(x - x0), not Y(x - x0) itself.
        Ex shifted = substitute(rr.explicitY ? rr.explicitY : rr.solution, b.x, X);
        shifted = simplify(Add(y0, shifted));
        rr.solution = shifted;
        rr.explicitY = shifted;
    } else {
        rr.solution = back(rr.solution);
    }
    rr.paramX = rr.paramX ? simplify(Add(x0, rr.paramX)) : nullptr;
    rr.paramY = rr.paramY ? simplify(Add(y0, rr.paramY)) : nullptr;
    for (auto& s : rr.singular) s = back(s);
    for (auto& st : rr.steps) st.math = back(st.math);
    rr.normalized = yp;
    rr.typeId = "homogeneous";
    Step shift;
    shift.titleKey = "ode.t.shift_origin";
    shift.noteKey = "ode.n.shift_origin";
    shift.params = pjson({{"x0", toPlain(x0)}, {"y0", toPlain(y0)}});
    rr.steps.insert(rr.steps.begin(), shift);
    b.r = rr;
    return true;
}

static bool tryHomogeneous(Builder& b, const Ex& yp) {
    if (!dependsOn(yp, b.y)) return false;
    Ex v = Sym("v");
    Ex sub = substitute(yp, b.y, Mul(v, Sym(b.x)));
    sub = expand(simplify(sub));
    if (dependsOn(sub, b.x)) {
        // simplify sometimes leaves a common x factor uncancelled, e.g.
        // v·x/(x−v·x) + x/(x−v·x).  Homogeneity means the value cannot change
        // with x at fixed v, so probe two scales before giving up.
        bool homogeneous = true;
        for (double vv : {0.37, 1.23, -0.71, 2.1}) {
            bool ok1 = false, ok2 = false;
            std::map<std::string, double> at1{{"v", vv}, {b.x, 1.0}};
            std::map<std::string, double> at2{{"v", vv}, {b.x, 2.5}};
            const double f1 = evalReal(sub, at1, &ok1);
            const double f2 = evalReal(sub, at2, &ok2);
            if (!ok1 || !ok2 || !std::isfinite(f1) || !std::isfinite(f2) ||
                std::fabs(f1 - f2) > 1e-8 * (1.0 + std::fabs(f1))) {
                homogeneous = false;
                break;
            }
        }
        if (!homogeneous) return false;
        sub = substitute(sub, b.x, Num(1));
    }
    b.r.order = 1;
    b.step("ode.t26", sub, "ode.n23");
    b.step("ode.t27", Add({v, Mul(Sym(b.x), Deriv(v, b.x, 1))}), "ode.n24");
    Ex Fv = sub;
    Ex num = simplify(Add(Fv, Mul(Num(-1), v)));
    if (num->isZero()) {
        Ex sol = Mul(Cst(1), Sym(b.x));
        b.step("ode.t28", sol, "ode.n25");
        setSolution(b.r, "homogeneous", sol, true, sol);
        return true;
    }
    // F(v) - v can be a sum of fractions.  Integrating 1/(sum) directly hides
    // the rational structure from the integrator, so combine it first.
    Ex combined = together(num);
    if (combined && !combined->isZero()) num = combined;
    Ex integral = integrate(Pow(num, Num(-1)), "v");
    // Back-substituting v = y/x inside an unevaluated Integral would leave an
    // invalid integral whose variable is y/x; let another method try instead.
    if (hasUnevaluatedIntegral(integral)) return false;
    Ex sol = Add(integral, Mul(Num(-1), lnAbs(Sym(b.x))));
    b.step("ode.t29", num, "ode.n26");
    b.step("ode.t30", sol, "ode.n27");
    b.step("ode.t31", substituteSubexpr(sol, v, Mul(Sym(b.y), Pow(Sym(b.x), Num(-1)))), "ode.n28");
    // constant/singular solutions from F(v)-v=0
    auto roots = solvePoly(num, "v");
    for (auto& rr : roots) {
        if (!dependsOn(rr, "v")) {
            b.r.singular.push_back(simplify(Mul(Sym(b.x), rr)));
        }
    }
    // try explicit y
    Ex implicit = substituteSubexpr(sol, v, Mul(Sym(b.y), Pow(Sym(b.x), Num(-1))));
    Ex ex = tryExplicit(Add(implicit, Mul(Num(-1), Cst(1))), b.x, b.y);
    setSolution(b.r, "homogeneous", implicit, ex != nullptr, ex);
    b.r.normalized = yp;
    // verify via exact relation u_x N - u_y M, M=-F, N=1
    verifyImplicit(b.r, implicit, Mul(Num(-1), yp), Num(1), b.x, b.y, b.en);
    return true;
}

static bool trySeparable(Builder& b, const Ex& yp) {
    if (!dependsOn(yp, b.y)) return false;
    Ex yp_x = simplify(diff(yp, b.x));
    Ex yp_y = simplify(diff(yp, b.y));
    Ex yp_xy = simplify(diff(yp_x, b.y));
    Ex test = simplify(Add(Mul(yp, yp_xy), Mul(Num(-1), Mul(yp_x, yp_y))));
    if (!test->isZero()) return false;
    // g(y) = yp at x=1, f(x) = yp/g
    Ex ype = expand(yp);
    // Normalise at x = 0 when possible: it keeps g(y) free of stray constants.
    Ex g = nullptr;
    for (int pt : {0, 1, 2}) {
        try {
            Ex cand = expand(simplify(substitute(ype, b.x, Num(pt))));
            if (!cand->isZero() && !dependsOn(cand, b.x)) { g = cand; break; }
        } catch (...) {}
    }
    if (!g) return false;
    Ex f = simplify(Mul(ype, Pow(g, Num(-1))));
    if (dependsOn(f, b.y)) return false;
    // Strip a constant scale from g(y): the solution family is unchanged, but
    // the displayed split becomes sin(x)·cos(y) instead of sin(x)/sin(1).
    if (g->isMul()) {
        std::vector<Ex> cst, rest;
        for (auto& fac : g->args) {
            if (dependsOn(fac, b.y)) rest.push_back(fac);
            else cst.push_back(fac);
        }
        if (!cst.empty() && !rest.empty()) {
            Ex c = cst.size() == 1 ? cst[0] : Mul(cst);
            if (!c->isZero()) {
                g = rest.size() == 1 ? rest[0] : Mul(rest);
                f = simplify(Mul(f, c));
            }
        }
    }
    b.r.order = 1;
    b.step("ode.t32", Mul(f, g), "ode.n29", pjson({{"n0", toPretty(f)}, {"n1", toPretty(g)}}));
    Ex Ig = integrate(Pow(g, Num(-1)), b.y);
    Ex If = integrate(f, b.x);
    Ex sol = Add(Ig, Mul(Num(-1), If));
    b.step("ode.t33", Add({Mul(Pow(g, Num(-1)), b.P), Mul(Num(-1), f)}), "ode.n30");
    b.step("ode.t34", sol, "ode.n31");
    // singular constant solutions
    auto roots = solvePoly(g, b.y);
    for (auto& rr : roots) {
        if (dependsOn(rr, b.y)) continue;
        bool dup = false;
        for (auto& s : b.r.singular) if (equals(s, rr)) { dup = true; break; }
        if (!dup) b.r.singular.push_back(rr);
    }
    Ex ex = tryExplicit(Add(sol, Mul(Num(-1), Cst(1))), b.x, b.y);
    setSolution(b.r, "separable", sol, ex != nullptr, ex);
    b.r.normalized = yp;
    verifyImplicit(b.r, sol, Mul(Num(-1), yp), Num(1), b.x, b.y, b.en);
    return true;
}

static bool tryLinearSub(Builder& b, const Ex& yp0) {
    // y' = f(a x + y)
    Ex yp = expand(yp0);
    Ex yx = simplify(diff(yp, b.x));
    Ex yy = simplify(diff(yp, b.y));
    if (yy->isZero() || yx->isZero()) return false;
    Ex a = simplify(Mul(yx, Pow(yy, Num(-1))));
    if (dependsOn(a, b.x) || dependsOn(a, b.y)) return false;
    Ex u = Add(Mul(a, Sym(b.x)), Sym(b.y));
    // Expanded f contains no literal (a·x+y) subexpression, so substitute
    // y = u − a·x and simplify before checking dependence.
    Ex fu = simplify(substitute(yp, b.y, Add(Sym("u"), Mul(Num(-1), Mul(a, Sym(b.x))))));
    fu = simplify(expand(fu));
    if (dependsOn(fu, b.x) || dependsOn(fu, b.y)) return false;
    // u' = a + f(u)
    Ex rhs = simplify(Add(a, fu));
    Ex Ig = integrate(Pow(rhs, Num(-1)), "u");
    Ex solU = Add(Ig, Mul(Num(-1), Sym(b.x)));
    Ex sol = substituteSubexpr(solU, Sym("u"), u);
    b.r.order = 1;
    b.step("ode.t35", rhs, "ode.n32");
    b.step("ode.t36", u, "ode.n33");
    b.step("ode.t37", solU, "ode.n34");
    b.step("ode.t38", sol, "ode.n35");
    Ex ex = tryExplicit(Add(sol, Mul(Num(-1), Cst(1))), b.x, b.y);
    setSolution(b.r, "linear_sub", sol, ex != nullptr, ex);
    b.r.normalized = yp;
    return true;
}

// μ is only defined up to sign; removing |·| keeps the derivatives clean.
static Ex stripAbs(const Ex& e) {
    if (!e) return e;
    if (e->isFunc("abs") && e->args.size() == 1) return stripAbs(e->args[0]);
    if (e->args.empty()) return e;
    std::vector<Ex> v;
    for (auto& a : e->args) v.push_back(stripAbs(a));
    switch (e->kind) {
        case Kind::Add: return Add(v);
        case Kind::Mul: return Mul(v);
        case Kind::Pow: return Pow(v[0], v[1]);
        case Kind::Func: return Fn(e->name, v);
        default: return e;
    }
}

// exact / integrating factor from M dx + N dy = 0
static bool tryExact(Builder& b, const Ex& F, const Ex& M, const Ex& N, bool fromForm) {
    Ex My = simplify(diff(M, b.y));
    Ex Nx = simplify(diff(N, b.x));
    Ex d = simplify(Add(My, Mul(Num(-1), Nx)));
    b.r.order = 1;
    if (d->isZero()) {
        Ex u0 = integrate(M, b.x);
        Ex phiP = simplify(Add(N, Mul(Num(-1), diff(u0, b.y))));
        Ex phi = integrate(phiP, b.y);
        Ex u = simplify(Add(u0, phi));

        b.step("ode.t39", My, "ode.n36", pjson({{"n0", toPretty(My)}, {"n1", toPretty(Nx)}}));
        b.step("ode.t40", u, "ode.n37", pjson({{"n0", toPretty(phiP)}, {"n1", toPretty(phi)}}));
        b.step("ode.t41", u, "ode.n38");
        Ex ex = tryExplicit(Add(u, Mul(Num(-1), Cst(1))), b.x, b.y);
        setSolution(b.r, "exact", u, ex != nullptr, ex);
        b.r.normalized = F;
        verifyImplicit(b.r, u, M, N, b.x, b.y, b.en);
        return true;
    }
    // integrating factors
    struct Try { Ex mu; std::string kind; };
    std::vector<Try> tries;
    try {
        Ex f1 = simplify(Mul(d, Pow(N, Num(-1))));
        if (!dependsOn(f1, b.y)) {
            Ex mu = stripAbs(simplify(expOf(integrate(f1, b.x))));
            tries.push_back({mu, "μ(x) = exp(∫ (M_y − N_x)/N dx)"});
        }
    } catch (...) {}
    try {
        Ex f2 = simplify(Mul(Num(-1), d, Pow(M, Num(-1))));
        if (!dependsOn(f2, b.x)) {
            Ex mu = stripAbs(simplify(expOf(integrate(f2, b.y))));
            tries.push_back({mu, "μ(y) = exp(∫ (N_x − M_y)/M dy)"});
        }
    } catch (...) {}
    try {
        Ex den = simplify(Add(Mul(Sym(b.x), M), Mul(Num(-1), Mul(Sym(b.y), N))));
        // u = xy: μ'/μ = -(M_y - N_x)/(xM - yN), so the sign is negated here.
        Ex f3 = simplify(Mul(Num(-1), d, Pow(den, Num(-1))));
        Ex sub = substituteSubexpr(f3, Mul(Sym(b.x), Sym(b.y)), Sym("u"));
        if (!dependsOn(sub, b.x) && !dependsOn(sub, b.y)) {
            Ex mu = stripAbs(simplify(expOf(integrate(sub, "u"))));
            mu = substituteSubexpr(mu, Sym("u"), Mul(Sym(b.x), Sym(b.y)));
            tries.push_back({mu, "μ(xy) = exp(∫ f(u) du), u = xy"});
        }
    } catch (...) {}
    try {
        Ex den = simplify(Add(Mul(Num(-1), M), N)); // N-M
        Ex f4 = simplify(Mul(d, Pow(den, Num(-1))));
        Ex u = Add(Sym(b.x), Sym(b.y));
        Ex sub = substituteSubexpr(f4, u, Sym("u"));
        if (!dependsOn(sub, b.x) && !dependsOn(sub, b.y)) {
            Ex mu = stripAbs(simplify(expOf(integrate(sub, "u"))));
            mu = substituteSubexpr(mu, Sym("u"), u);
            tries.push_back({mu, "μ(x+y) = exp(∫ f(u) du), u = x+y"});
        }
    } catch (...) {}
    for (auto& tr : tries) {
        if (!tr.mu) continue;
        Ex M2 = simplify(Mul(tr.mu, M));
        Ex N2 = simplify(Mul(tr.mu, N));
        if (!isZero(Add(simplify(diff(M2, b.y)), Mul(Num(-1), simplify(diff(N2, b.x)))))) continue;
        Ex u0 = integrate(M2, b.x);
        Ex phiP = simplify(Add(N2, Mul(Num(-1), diff(u0, b.y))));
        Ex phi = integrate(phiP, b.y);
        Ex u = simplify(Add(u0, phi));
        b.step("ode.t42", d, "ode.n39", pjson({{"n0", toPretty(My)}, {"n1", toPretty(Nx)}}));
        b.step("ode.t43", tr.mu, "ode.n40", pjson({{"n0", tr.kind}}));
        b.step("ode.t44", u, "ode.n41", pjson({{"n0", toPretty(phiP)}, {"n1", toPretty(phi)}}));
        b.step("ode.t45", u, "ode.n42");
        Ex ex = tryExplicit(Add(u, Mul(Num(-1), Cst(1))), b.x, b.y);
        setSolution(b.r, "integrating_factor", u, ex != nullptr, ex);
        b.r.normalized = F;
        verifyImplicit(b.r, u, M2, N2, b.x, b.y, b.en);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------- higher order
// True when e contains any derivative of y with respect to x.  dependsOn()
// deliberately treats y′ as an independent jet variable, so a nonlinear term
// such as (y′)² would otherwise be mistaken for a forcing term.
static bool containsDerivOf(const Ex& e, const std::string& y, const std::string& x) {
    if (!e) return false;
    if (isDeriv(e) && e->args.size() == 3 && e->args[0]->isSym(y) && e->args[1]->isSym(x))
        return true;
    for (auto& c : e->args) if (containsDerivOf(c, y, x)) return true;
    return false;
}

static bool extractLinearODE(const Ex& F, const std::string& y, const std::string& x,
                             std::vector<Ex>& a, Ex& rhs) {
    Ex e = expand(F);
    std::vector<Ex> terms = e->isAdd() ? e->args : std::vector<Ex>{e};
    a.clear();
    rhs = Num(0);
    for (auto& t : terms) {
        // find exactly one y-related factor
        std::vector<Ex> facs = t->isMul() ? t->args : std::vector<Ex>{t};
        int order = -1;
        Ex coef = Num(1);
        bool bad = false;
        for (auto& f : facs) {
            if (isDeriv(f) && f->args[0]->isSym(y) && f->args[1]->isSym(x)) {
                if (order != -1) { bad = true; break; }
                order = derivOrder(f);
                if (order < 1 || order > 8) { bad = true; break; }
                continue;
            }
            if (f->isSym(y)) {
                if (order != -1) { bad = true; break; }
                order = 0;
                continue;
            }
            if (!dependsOn(f, y) && !containsDerivOf(f, y, x)) {
                coef = Mul(coef, f);
                continue;
            }
            bad = true;
            break;
        }
        if (bad) return false;
        if (order == -1) {
            if (dependsOn(coef, y) || containsDerivOf(coef, y, x)) return false;
            rhs = Add(rhs, Mul(Num(-1), coef));
            continue;
        }
        if ((int)a.size() <= order) a.resize(order + 1, Num(0));
        a[order] = Add(a[order], coef);
    }
    if (a.empty()) return false;
    while (a.size() > 1 && a.back()->isZero()) a.pop_back();
    return true;
}

static Ex constCoeffSolution(const std::vector<Ex>& a, const std::string& x, Builder& b,
                              bool emitGeneral = true) {
    int n = (int)a.size() - 1;
    if (n < 1) return nullptr;
    for (auto& c : a) if (!c->isNum()) return nullptr;
    Poly ch;
    for (auto& c : a) ch.c.push_back(c->num);
    // characteristic polynomial in r
    Ex charExpr = polyToExpr(ch, "r");
    std::vector<Factor> facs;
    if (!factorPolynomial(ch, facs)) {
        // fall back: rational roots only
        facs.clear();
    }
    // build roots with multiplicity
    struct Root { Ex re, im; int mult; bool cplx; };
    std::vector<Root> roots;
    for (auto& f : facs) {
        if (f.p.deg() == 1) {
            Rational r = -f.p.c[0] / f.p.c[1];
            roots.push_back({Num(r), Num(0), f.mult, false});
        } else if (f.p.deg() == 2) {
            Rational p = f.p.c[1], q = f.p.c[0];
            Rational disc = p * p - q * Rational(4);
            if (disc.isNeg()) {
                Rational alpha = -p * Rational(1, 2);
                Ex beta = simplify(Mul(Sqrt(Num(-disc)), Num(Rational(1, 2))));
                roots.push_back({Num(alpha), beta, f.mult, true});
            } else {
                Ex s = Sqrt(Num(disc));
                roots.push_back({simplify(Mul(Add(Num(-p), s), Num(Rational(1, 2)))), Num(0), f.mult, false});
                if (!disc.isZero())
                    roots.push_back({simplify(Mul(Add(Num(-p), Mul(Num(-1), s)), Num(Rational(1, 2)))), Num(0), f.mult, false});
            }
        } else {
            return nullptr;
        }
    }
    Ex X = Sym(x);
    std::vector<Ex> terms;
    int ci = 1;
    for (auto& rr : roots) {
        for (int m = 0; m < rr.mult; ++m) {
            Ex polyFactor = Pow(X, Num(m));
            if (rr.cplx) {
                Ex alpha = rr.re, beta = rr.im;
                Ex env = expOf(Mul(alpha, X));
                Ex cs = Mul(Cst(ci++), polyFactor, env, Fn("cos", {Mul(beta, X)}));
                Ex sn = Mul(Cst(ci++), polyFactor, env, Fn("sin", {Mul(beta, X)}));
                terms.push_back(cs);
                terms.push_back(sn);
            } else {
                terms.push_back(Mul(Cst(ci++), polyFactor, expOf(Mul(rr.re, X))));
            }
        }
    }
    if (terms.empty()) return nullptr;
    Ex sol = Add(terms);

    std::string rootsMath;
    for (auto& rt : roots) {
        Ex term = rt.cplx ? Add({rt.re, Mul(rt.im, Sym("i"))}) : rt.re;
        if (!rootsMath.empty()) rootsMath += ", ";
        rootsMath += "r = " + toPretty(term);
        if (rt.mult > 1) rootsMath += " (" + std::to_string(rt.mult) + ")";
    }
    const bool singleComplex = roots.size() == 1 && roots[0].cplx;
    b.step("ode.t46", charExpr,
           singleComplex ? "ode.n.roots_complex" : "ode.n.roots_real",
           pjson({{"roots", rootsMath}}));
    if (emitGeneral) b.step("ode.t47", sol, "ode.n44");
    return sol;
}

// ------------------------------------------------- non-homogeneous const coeff
// A small exact linear solver for the undetermined-coefficient systems. Every
// entry is a rational number, so Gaussian elimination stays exact.
static bool solveRationalSystem(std::vector<std::vector<Ex>> M, std::vector<Ex> rhs,
                                std::vector<Ex>& out) {
    const int m = (int)M.size();
    if (m == 0 || M[0].empty()) return false;
    const int n = (int)M[0].size();
    for (int col = 0; col < n; ++col) {
        int piv = -1;
        for (int r = col; r < m; ++r) {
            if (M[r][col] && !simplify(M[r][col])->isZero()) { piv = r; break; }
        }
        if (piv < 0) return false; // rank deficient ansatz
        if (piv != col) {
            std::swap(M[piv], M[col]);
            std::swap(rhs[piv], rhs[col]);
        }
        Ex pv = simplify(M[col][col]);
        if (pv->isZero()) return false;
        for (int j = col; j < n; ++j) M[col][j] = simplify(Mul(M[col][j], Pow(pv, Num(-1))));
        rhs[col] = simplify(Mul(rhs[col], Pow(pv, Num(-1))));
        for (int r = 0; r < m; ++r) {
            if (r == col) continue;
            Ex f = simplify(M[r][col]);
            if (f->isZero()) continue;
            for (int j = col; j < n; ++j) M[r][j] = simplify(Add(M[r][j], Mul(Num(-1), f, M[col][j])));
            rhs[r] = simplify(Add(rhs[r], Mul(Num(-1), f, rhs[col])));
        }
    }
    for (int r = n; r < m; ++r) {
        if (!simplify(rhs[r])->isZero()) return false; // inconsistent overdetermined system
    }
    out.assign(n, Num(0));
    for (int i = 0; i < n; ++i) out[i] = simplify(rhs[i]);
    return true;
}

static long long binomialSmall(int n, int k) {
    if (k < 0 || k > n) return 0;
    long long r = 1;
    for (int i = 1; i <= k; ++i) r = r * (n - k + i) / i;
    return r;
}

static Ex intPow(const Ex& base, int e) {
    if (e == 0) return Num(1);
    return Pow(base, Num(e));
}

static Ex diffN(const Ex& g, const std::string& x, int n) {
    Ex r = g;
    for (int i = 0; i < n; ++i) r = diff(r, x);
    return r;
}

// L_k[g] where L = sum a_j D^j and y = e^(k x) g. This avoids carrying the
// exponential through every derivative.
static Ex shiftedOperator(const std::vector<Ex>& a, const Ex& k, const Ex& g,
                          const std::string& x) {
    Ex total = Num(0);
    for (int j = 0; j < (int)a.size(); ++j) {
        if (!a[j] || a[j]->isZero()) continue;
        Ex inner = Num(0);
        for (int i = 0; i <= j; ++i) {
            Ex term = Mul(Num(binomialSmall(j, i)), intPow(k, j - i), diffN(g, x, i));
            inner = Add(inner, term);
        }
        total = Add(total, Mul(a[j], inner));
    }
    return simplify(expand(total));
}

// Multiplicity of k + i b as a root of the characteristic polynomial. The
// inputs are rational, so a numeric probe only has to decide which derivative
// is the first non-zero one.
static int characteristicMultiplicity(const std::vector<Ex>& a, const Ex& k, const Ex& b) {
    const int n = (int)a.size() - 1;
    if (n < 1 || !k->isNum() || !b->isNum()) return 0;
    const std::complex<double> z(k->num.toDouble(), b->num.toDouble());
    std::vector<double> c(n + 1, 0.0);
    for (int i = 0; i <= n; ++i) c[i] = a[i]->isNum() ? a[i]->num.toDouble() : 0.0;
    for (int j = 0; j <= n; ++j) {
        std::complex<double> val(0.0, 0.0);
        double scale = 0.0;
        for (int m = j; m <= n; ++m) {
            double fact = 1.0;
            for (int q = 0; q < j; ++q) fact *= (double)(m - q);
            std::complex<double> term = c[m] * fact * std::pow(z, m - j);
            val += term;
            scale += std::abs(term);
        }
        if (std::abs(val) > 1e-8 * (1.0 + scale)) return j;
    }
    return n + 1;
}

// Split a forcing term into e^(k x) * poly(x) * cos/sin(b x). Anything that is
// not of this shape is reported as unsupported so the caller can explain it.
static bool decomposeForcingTerm(const Ex& term, const std::string& x,
                                 Ex& k, Ex& b, int& trig, Ex& poly) {
    Ex t = expand(term);
    std::vector<Ex> facs = t->isMul() ? t->args : std::vector<Ex>{t};
    Rational ksum(0);
    bool haveExp = false, haveTrig = false;
    std::vector<Ex> polyFacs;
    b = Num(0);
    trig = 0;
    for (auto& f : facs) {
        if (f->isFunc("exp") && f->args.size() == 1) {
            Ex arg = expand(f->args[0]);
            std::vector<Ex> ac;
            if (!polyCoeffs(arg, x, ac) || ac.size() > 2) return false;
            Rational c0(0), c1(0);
            if (!ac[0]->isNum()) return false;
            c0 = ac[0]->num;
            if (ac.size() == 2) {
                if (!ac[1]->isNum()) return false;
                c1 = ac[1]->num;
            }
            if (!c0.isZero()) return false; // e^(k x + const) is not supported
            ksum = ksum + c1;
            haveExp = true;
            continue;
        }
        if ((f->isFunc("sin") || f->isFunc("cos")) && f->args.size() == 1) {
            if (haveTrig) return false;
            Ex arg = expand(f->args[0]);
            std::vector<Ex> ac;
            if (!polyCoeffs(arg, x, ac) || ac.size() > 2) return false;
            Rational c0(0), c1(0);
            if (!ac[0]->isNum()) return false;
            c0 = ac[0]->num;
            if (ac.size() == 2) {
                if (!ac[1]->isNum()) return false;
                c1 = ac[1]->num;
            }
            if (!c0.isZero() || c1.isZero()) return false;
            b = Num(c1);
            trig = f->isFunc("cos") ? 1 : 2;
            haveTrig = true;
            continue;
        }
        polyFacs.push_back(f);
    }
    if (haveExp) k = Num(ksum);
    else k = Num(0);
    if (polyFacs.empty()) poly = Num(1);
    else if (polyFacs.size() == 1) poly = polyFacs[0];
    else poly = Mul(polyFacs);
    poly = expand(poly);
    std::vector<Ex> pc;
    if (!polyCoeffs(poly, x, pc)) return false;
    for (auto& c : pc) if (!c->isNum()) return false;
    return true;
}

static bool extractTrigCoeffs(const Ex& h, const std::string& x, const Ex& b, int maxDeg,
                              std::vector<Ex>& cc, std::vector<Ex>& ss) {
    Ex C = Sym("__C"), S = Sym("__S");
    Ex X = Sym(x);
    Ex sub = substituteSubexpr(h, Fn("cos", {Mul(b, X)}), C);
    sub = substituteSubexpr(sub, Fn("sin", {Mul(b, X)}), S);
    sub = expand(sub);
    std::vector<Ex> pc;
    if (!polyCoeffs(sub, x, pc) || (int)pc.size() > maxDeg + 1) return false;
    cc.assign(maxDeg + 1, Num(0));
    ss.assign(maxDeg + 1, Num(0));
    for (int p = 0; p < (int)pc.size(); ++p) {
        Ex e = expand(pc[p]);
        Ex c0 = simplify(substitute(substitute(e, "__C", Num(1)), "__S", Num(0)));
        Ex c1 = simplify(substitute(substitute(e, "__C", Num(0)), "__S", Num(1)));
        if (dependsOn(c0, "__C") || dependsOn(c0, "__S") ||
            dependsOn(c1, "__C") || dependsOn(c1, "__S")) return false;
        cc[p] = c0;
        ss[p] = c1;
    }
    return true;
}

static Ex particularForTerm(const std::vector<Ex>& a, const Ex& k, const Ex& b, int trig,
                            const Ex& poly, const std::string& x, Builder& bld, bool en) {
    std::vector<Ex> pc;
    if (!polyCoeffs(poly, x, pc) || pc.empty()) return nullptr;
    for (auto& c : pc) if (!c->isNum()) return nullptr;
    const int d = (int)pc.size() - 1;
    const int s = characteristicMultiplicity(a, k, b);
    if (s < 0 || s > (int)a.size()) return nullptr;
    const int maxDeg = s + d;
    Ex X = Sym(x);
    Ex trigArg = Mul(b, X);

    const Ex expFactor = k->isZero() ? Num(1) : expOf(Mul(k, X));
    const std::string expLabel = k->isZero() ? "" : "e^(" + toPretty(k) + "·x)*";
    std::vector<Ex> basis;
    std::vector<std::string> labels;
    for (int j = s; j <= s + d; ++j) {
        Ex xj = Pow(X, Num(j));
        if (b->isZero()) {
            basis.push_back(xj);
            labels.push_back(expLabel + "x^" + std::to_string(j));
        } else {
            basis.push_back(Mul(xj, Fn("cos", {trigArg})));
            labels.push_back(expLabel + "x^" + std::to_string(j) + "*cos(" + toPretty(trigArg) + ")");
            basis.push_back(Mul(xj, Fn("sin", {trigArg})));
            labels.push_back(expLabel + "x^" + std::to_string(j) + "*sin(" + toPretty(trigArg) + ")");
        }
    }
    if (basis.empty()) return nullptr;

    const int ncol = (int)basis.size();
    const int nrow = b->isZero() ? (maxDeg + 1) : (2 * (maxDeg + 1));
    std::vector<std::vector<Ex>> M(nrow, std::vector<Ex>(ncol, Num(0)));
    std::vector<Ex> rhs(nrow, Num(0));

    for (int col = 0; col < ncol; ++col) {
        Ex h = shiftedOperator(a, k, basis[col], x);
        if (b->isZero()) {
            std::vector<Ex> hc;
            if (!polyCoeffs(h, x, hc)) return nullptr;
            for (int p = 0; p < (int)hc.size() && p <= maxDeg; ++p) M[p][col] = hc[p];
        } else {
            std::vector<Ex> cc, ss;
            if (!extractTrigCoeffs(h, x, b, maxDeg, cc, ss)) return nullptr;
            for (int p = 0; p <= maxDeg; ++p) {
                M[p][col] = cc[p];
                M[maxDeg + 1 + p][col] = ss[p];
            }
        }
    }
    for (int p = 0; p <= d; ++p) {
        if (b->isZero()) {
            rhs[p] = pc[p];
        } else if (trig == 1) {
            rhs[p] = pc[p];
        } else {
            rhs[maxDeg + 1 + p] = pc[p];
        }
    }

    std::vector<Ex> coef;
    if (!solveRationalSystem(M, rhs, coef)) return nullptr;
    Ex part = Num(0);
    for (int i = 0; i < ncol; ++i) part = Add(part, Mul(coef[i], expFactor, basis[i]));
    part = simplify(expand(part));

    std::string ansatz = "y_p = ";
    for (int i = 0; i < ncol; ++i) {
        if (i) ansatz += " + ";
        ansatz += (i < (int)labels.size() ? labels[i] : "term");
    }
    bld.step("ode.t.forcing_ansatz", poly, "ode.n.forcing_ansatz",
             pjson({{"mult", std::to_string(s)}, {"ansatz", ansatz}}));
    bld.step("ode.t.solve_coefficients", part, "ode.n.solve_coefficients");
    return part;
}

static Ex particularSolution(const std::vector<Ex>& a, const Ex& rhs, const std::string& x,
                             Builder& b, bool en) {
    Ex expanded = expand(rhs);
    std::vector<Ex> terms = expanded->isAdd() ? expanded->args : std::vector<Ex>{expanded};
    Ex total = Num(0);
    for (auto& term : terms) {
        if (term->isZero()) continue;
        Ex k, bb, poly;
        int trig = 0;
        if (!decomposeForcingTerm(term, x, k, bb, trig, poly)) return nullptr;
        Ex part = particularForTerm(a, k, bb, trig, poly, x, b, en);
        if (!part) return nullptr;
        total = Add(total, part);
    }
    return simplify(expand(total));
}

// A short symbol that does not occur in F, used for the reduction variable.
// Identifiers beginning with an underscore confuse the implicit-product
// preprocessor, so only plain letters are considered.
static std::string freshSymbol(const Ex& F) {
    std::vector<std::string> syms;
    collectSymbols(F, syms);
    for (const char* cand : {"p", "q", "r", "w", "u", "v", "s", "k"}) {
        bool used = false;
        for (auto& s : syms) if (s == cand) { used = true; break; }
        if (!used) return cand;
    }
    return "p";
}

// Highest derivative of y with respect to x that occurs in F.
static int maxOrderOf(const Ex& F, const std::string& x) {
    int best = 0;
    std::function<void(const Ex&)> scan = [&](const Ex& e) {
        if (!e) return;
        if (isDeriv(e) && e->args.size() == 3 && e->args[1]->isSym(x))
            best = std::max(best, derivOrder(e));
        for (auto& c : e->args) scan(c);
    };
    scan(F);
    return best;
}

// y'' = p·dp/dy, y' = p, ... for the missing-independent-variable reduction.
// Only first and second derivatives are needed by the second-order method.
static Ex reduceMissingX(const Ex& e, const std::string& y, const std::string& x,
                         const std::string& p, bool& ok) {
    if (!e) return e;
    if (isDeriv(e) && e->args.size() == 3 && e->args[0]->isSym(y) && e->args[1]->isSym(x)) {
        const int k = derivOrder(e);
        if (k == 1) return Sym(p);
        if (k == 2) return Mul(Sym(p), Deriv(Sym(p), y, 1));
        ok = false;
        return e;
    }
    if (e->args.empty()) return e;
    std::vector<Ex> v;
    for (auto& a : e->args) v.push_back(reduceMissingX(a, y, x, p, ok));
    switch (e->kind) {
        case Kind::Add: return Add(v);
        case Kind::Mul: return Mul(v);
        case Kind::Pow: return Pow(v[0], v[1]);
        case Kind::Func: return Fn(e->name, v);
        default: return e;
    }
}

// y^(k) = p^(k-1) for the missing-dependent-variable reduction (p = y').
static Ex reduceMissingY(const Ex& e, const std::string& y, const std::string& x,
                         const std::string& p) {
    if (!e) return e;
    if (isDeriv(e) && e->args.size() == 3 && e->args[0]->isSym(y) && e->args[1]->isSym(x)) {
        const int k = derivOrder(e);
        if (k <= 1) return Sym(p);
        return Deriv(Sym(p), x, k - 1);
    }
    if (e->args.empty()) return e;
    std::vector<Ex> v;
    for (auto& a : e->args) v.push_back(reduceMissingY(a, y, x, p));
    switch (e->kind) {
        case Kind::Add: return Add(v);
        case Kind::Mul: return Mul(v);
        case Kind::Pow: return Pow(v[0], v[1]);
        case Kind::Func: return Fn(e->name, v);
        default: return e;
    }
}

// ∫ dy/p(y) for the final step of the missing-x reduction.  The general
// integrator needs numeric quadratic coefficients, so a symbolic constant
// (the C from the p-equation) leaves forms such as 1/(y²+2C) unevaluated.
// Complete the square and return the elementary antiderivative in that case.
static Ex integrateReciprocal(const Ex& p, const std::string& y) {
    Ex q = simplify(Pow(p, Num(-1)));
    bool closed = false;
    Ex direct = integrate(q, y, &closed);
    if (direct && !hasUnevaluatedIntegral(direct)) return direct;

    Ex coeff = Num(1), den = nullptr, expo = nullptr;
    Ex t = simplify(q);
    std::vector<Ex> facs = t->isMul() ? t->args : std::vector<Ex>{t};
    for (auto& f : facs) {
        if (f->isPow() && f->args[1]->isNum() && f->args[1]->num.isNeg() && !den) {
            den = f->args[0];
            expo = f->args[1];
        } else if (!dependsOn(f, y)) {
            coeff = Mul(coeff, f);
        } else {
            den = nullptr;
            break;
        }
    }
    if (!den) return direct;
    // sqrt(Q)^(-1) may survive simplification as Q^(-1/2).
    if (den->isPow() && den->args[1]->isNum() && den->args[1]->num == Rational(1, 2) &&
        expo->isNum() && expo->num == Rational(-1)) {
        den = den->args[0];
        expo = Num(Rational(-1, 2));
    }
    std::vector<Ex> cs;
    if (!polyCoeffs(expand(den), y, cs) || cs.size() != 3) return direct;
    Ex A = cs[2], B = cs[1], C = cs[0];
    if (!A->isNum() || !B->isNum()) return direct;

    // den = A u^2 + K with u = y + B/(2A).
    Ex u = Add(Sym(y), Mul(B, Pow(Mul(Num(2), A), Num(-1))));
    Ex K = simplify(Add(C, Mul(Num(-1), Pow(B, Num(2)), Pow(Mul(Num(4), A), Num(-1)))));
    const bool negA = A->isNum() && A->num.isNeg();
    const bool negK = K->isNum() && K->num.isNeg();
    // ∫ du/(K + A u²): the coefficient carries the sign of A, and the
    // antiderivative switches from atan to artanh when A and K differ in sign.
    if (expo->isNum() && expo->num == Rational(-1)) {
        if (K->isNum() && (negA != negK)) {
            Ex out = Mul(coeff, negA ? Num(1) : Num(-1), Pow(Sqrt(Mul(Num(-1), A, K)), Num(-1)),
                         Fn("atanh", {Mul(u, Sqrt(Mul(Num(-1), A, Pow(K, Num(-1)))))}));
            return simplify(out);
        }
        // K is symbolic: present the branch where K has the sign of A, which
        // keeps the square root real.  The other sign is obtained by
        // analytically continuing the arbitrary constant.
        Ex out = Mul(coeff, negA ? Num(-1) : Num(1), Pow(Sqrt(Mul(A, K)), Num(-1)),
                     Fn("atan", {Mul(u, Sqrt(Mul(A, Pow(K, Num(-1)))))}));
        return simplify(out);
    }
    if (expo->isNum() && expo->num == Rational(-1, 2)) {
        if (negA) {
            // ∫ du/√(K − |A|u²) = asin(u√(|A|/K))/√|A| (K > 0 branch).
            Ex out = Mul(coeff, Pow(Sqrt(Mul(Num(-1), A)), Num(-1)),
                         Fn("asin", {Mul(u, Sqrt(Mul(Num(-1), A, Pow(K, Num(-1)))))}));
            return simplify(out);
        }
        if (K->isNum() && negK) {
            // ∫ du/√(A u² − |K|) = arcosh(u√(A/|K|))/√A.
            Ex out = Mul(coeff, Pow(Sqrt(A), Num(-1)),
                         Fn("acosh", {Mul(u, Sqrt(Mul(A, Pow(Mul(Num(-1), K), Num(-1)))))}));
            return simplify(out);
        }
        Ex out = Mul(coeff, Pow(Sqrt(A), Num(-1)),
                     Fn("asinh", {Mul(u, Sqrt(Mul(A, Pow(K, Num(-1)))))}));
        return simplify(out);
    }
    return direct;
}

// Isolate p from an implicit relation u(y,p) = C, including the quadratic
// energy relations produced by p·dp/dy = f(y).
static Ex isolatePFromRelation(const Ex& u, const std::string& indep, const std::string& p) {
    Ex lin = tryExplicit(Add(u, Mul(Num(-1), Cst(1))), indep, p);
    if (lin) return lin;
    std::vector<Ex> cs;
    if (!polyCoeffs(expand(u), p, cs) || cs.size() != 3) return nullptr;
    Ex A = cs[2], B = cs[1], C0 = cs[0];
    if (!A || A->isZero()) return nullptr;
    Ex C1 = Add(C0, Mul(Num(-1), Cst(1)));
    Ex disc = simplify(Add(Pow(B, Num(2)), Mul(Num(-4), A, C1)));
    Ex sq = Sqrt(disc);
    return simplify(Mul(Add(Mul(Num(-1), B), sq), Num(Rational(1, 2)), Pow(A, Num(-1))));
}

// Equations with no bare y: substitute p = y', so y^(k) = p^(k-1).  The
// reduced equation is one order lower; solve it and integrate p once.
static bool solveMissingDependent(Builder& b, const Ex& F, int order) {
    const std::string pName = freshSymbol(F);
    Ex G = reduceMissingY(F, b.y, b.x, pName);
    // If only the highest derivative occurs, this is plain repeated
    // integration y^(n) = f(x); keep the established type id for that case.
    bool onlyHighest = true;
    std::function<void(const Ex&)> scan = [&](const Ex& e) {
        if (!e) return;
        if (isDeriv(e) && e->args.size() == 3 && e->args[0]->isSym(b.y) &&
            e->args[1]->isSym(b.x) && derivOrder(e) < order)
            onlyHighest = false;
        for (auto& c : e->args) scan(c);
    };
    scan(F);
    const std::string typeId = onlyHighest ? "direct_integration" : "reduction_missing_y";

    // y^(n) = f(x): repeated quadrature is clearer than a p-substitution.
    if (onlyHighest) {
        std::vector<Ex> a;
        Ex rhs;
        if (extractLinearODE(F, b.y, b.x, a, rhs)) {
            const int n = (int)a.size() - 1;
            if (n == order && n >= 1 && !a[n]->isZero()) {
                Ex f = simplify(Mul(rhs, Pow(a[n], Num(-1))));
                b.step("ode.t48", F, "ode.n45", pjson({{"n0", std::to_string(n)}, {"n1", std::to_string(n)}}));
                Ex cur = f;
                for (int k = 1; k <= n; ++k) {
                    cur = integrate(cur, b.x);
                    cur = Add(cur, Cst(k));
                    cur = expand(cur);
                    const std::string cname = k == 1 ? "C" : "C" + std::to_string(k);
                    b.step("ode.t49", cur, "ode.n46", pjson({{"t0", std::to_string(k)}, {"n0", cname}}));
                }
                setSolution(b.r, typeId, cur, true, cur);
                b.r.normalized = F;
                b.r.checks.clear();
                verifyExplicit(b.r, F, cur, b.x, b.y, b.en);
                return true;
            }
        }
    }

    b.step("ode.t50", F, "ode.n47", pjson({{"n0", std::to_string(order)}}));
    b.step("ode.t51", G, "ode.n48", pjson({{"t0", pName}, {"n0", pName}, {"n1", pName}, {"n2", pName}, {"n3", pName}, {"n4", pName}}));

    OdeOptions o = b.opt;
    o.x = b.x;
    o.y = pName;
    o.particular.clear();
    o.simplifyAttempts = 0;
    Result rr;
    try {
        rr = solveODE(toPlain(G) + " = 0", o);
    } catch (...) {
        b.fail("ode.f1");
        return true;
    }
    if (rr.ok)
        b.text("ode.t52", "ode.n49", pjson({{"n0", pName}, {"n1", pName}}));
    for (auto& s : rr.steps) b.r.steps.push_back(s);
    if (!rr.ok) {
        b.fail("ode.f2", pjson({{"p0", pName}, {"p1", rr.error}}));
        return true;
    }

    Ex pSol = rr.isExplicit ? rr.explicitY : nullptr;
    if (!pSol && rr.solution)
        pSol = tryExplicit(Add(rr.solution, Mul(Num(-1), Cst(1))), b.x, pName, true);
    if (!pSol) {
        Ex rel = substituteSubexpr(rr.solution, Sym(pName), Deriv(b.Y, b.x, 1));
        b.step("ode.t53", rel, "ode.n50", pjson({{"n0", pName}, {"n1", pName}}));
        setSolution(b.r, typeId, rel, false, nullptr);
        b.r.normalized = F;
        b.r.checks.clear();
        return true;
    }

    Ex y = Add(integrate(pSol, b.x), Cst(order));
    // Keep square roots over the whole radicand: simplify() may split
    // sqrt(-2(C+x)) into sqrt(-2)sqrt(C+x), which obscures the real domain.
    y = expand(y);
    b.step("ode.t54", y, "ode.n51", pjson({{"n0", pName}, {"n1", std::to_string(order)}}));
    setSolution(b.r, typeId, y, true, y);
    b.r.normalized = F;
    b.r.checks.clear();
    verifyExplicit(b.r, F, y, b.x, b.y, b.en);
    return true;
}

// Equations with no explicit x: substitute y′ = p(y), y″ = p·dp/dy.  Solve the
// resulting first-order equation for p(y), then separate dy/p(y) = dx.
static bool solveMissingIndependent(Builder& b, const Ex& F, int order) {
    if (order != 2) {
        b.fail("ode.f3");
        return true;
    }
    const std::string pName = freshSymbol(F);
    bool reducible = true;
    Ex G = reduceMissingX(F, b.y, b.x, pName, reducible);
    if (!reducible) {
        b.fail("ode.f4");
        return true;
    }
    b.step("ode.t55", F, "ode.n52");
    b.step("ode.t56", G, "ode.n53", pjson({{"t0", pName}, {"n0", pName}, {"n1", pName}, {"n2", pName}, {"n3", pName}, {"n4", pName}, {"n5", pName}, {"n6", pName}}));

    OdeOptions o = b.opt;
    o.x = b.y;   // the reduced equation's independent variable is the old y
    o.y = pName;
    o.particular.clear();
    o.simplifyAttempts = 0;
    Result rr;
    try {
        rr = solveODE(toPlain(G) + " = 0", o);
    } catch (...) {
        b.fail("ode.f5", pjson({{"p0", pName}}));
        return true;
    }
    if (rr.ok)
        b.text("ode.t57", "ode.n54", pjson({{"n0", pName}, {"n1", pName}}));
    for (auto& s : rr.steps) b.r.steps.push_back(s);
    if (!rr.ok) {
        b.fail("ode.f6", pjson({{"p0", pName}, {"p1", rr.error}}));
        return true;
    }

    Ex pSol = rr.isExplicit ? rr.explicitY : nullptr;
    if (!pSol && rr.solution) pSol = isolatePFromRelation(rr.solution, b.y, pName);
    if (!pSol) {
        Ex rel = substituteSubexpr(rr.solution, Sym(pName), Deriv(b.Y, b.x, 1));
        b.step("ode.t58", rel, "ode.n55", pjson({{"n0", pName}, {"n1", pName}}));
        setSolution(b.r, "reduction_missing_x", rel, false, nullptr);
        b.r.normalized = F;
        b.r.checks.clear();
        return true;
    }
    b.step("ode.t59", pSol, "ode.n56", pjson({{"t0", pName}, {"n0", pName}}));

    // Separate: dy/p(y) = dx, then integrate both sides.
    Ex antider = integrateReciprocal(pSol, b.y);
    Ex relation = simplify(Add(antider, Mul(Num(-1), Sym(b.x)), Mul(Num(-1), Cst(2))));
    b.step("ode.t60", relation, "ode.n57", pjson({{"n0", pName}, {"n1", pName}}));
    Ex ysol = tryExplicit(relation, b.x, b.y);
    b.r.normalized = F;
    b.r.checks.clear();
    if (ysol) {
        b.step("ode.t61", ysol, "ode.n58");
        setSolution(b.r, "reduction_missing_x", ysol, true, ysol);
        verifyExplicit(b.r, F, ysol, b.x, b.y, b.en);
    } else {
        setSolution(b.r, "reduction_missing_x", relation, false, nullptr);
        // u(x,y) = 0 solves y' = p(y) when u_x + p(y) u_y = 0.
        Ex ux = simplify(diff(relation, b.x));
        Ex uy = simplify(diff(relation, b.y));
        Ex cond = simplify(Add(ux, Mul(pSol, uy)));
        bool good = cond->isZero() || numericallyZero(cond, {b.x, b.y}, 8, 1e-6);
        addCheckKey(b.r, "ode.c_implicit", good,
                    good ? "ode.c_implicit_ok" : "ode.c_implicit_fail");
    }
    return true;
}

// Honest numeric fallback for a second-order equation with no supported closed
// form. The RHS is isolated as y'' = f(x, y, y') and the UI integrates it with
// RK4; no symbolic formula is fabricated.
static bool tryNumericSecondOrder(Builder& b, const Ex& F) {
    if (maxOrderOf(F, b.x) != 2) return false;
    Ex marker = Sym("__q");
    Ex G = substituteSubexpr(F, Deriv(Sym(b.y), b.x, 2), marker);
    std::vector<Ex> cs;
    if (!polyCoeffs(expand(G), "__q", cs) || cs.size() != 2 || cs[1]->isZero()) return false;
    Ex rhs = simplify(expand(Mul(Num(-1), cs[0], Pow(cs[1], Num(-1)))));
    Ex velocity = Sym("__v");
    Ex rhsV = substituteSubexpr(rhs, Deriv(Sym(b.y), b.x, 1), velocity);
    // Keep the fallback for autonomous equations. Variable-coefficient forms
    // such as Euler–Cauchy stay in the honest "unsupported" bucket instead of
    // being approximated by a trajectory that hides their exact structure.
    if (dependsOn(rhsV, b.x)) return false;

    b.r.ok = true;
    b.r.order = 2;
    b.r.normalized = F;
    b.r.typeId = "numeric_second_order";
    b.r.method = "runge_kutta";
    b.r.closedForm = false;
    b.r.verified = false;
    b.r.numericOnly = true;
    b.r.numericRhs = toPlain(rhs);
    b.r.numericNote = "ode.n.numeric_second_order";
    b.r.numericNoteParams = pjson({{"rhs", toPlain(rhs)}});
    b.r.steps.clear();
    b.step("ode.t62", F, "ode.n59");
    b.step("ode.t63", Add({Deriv(Sym(b.y), b.x, 1), Mul(Num(-1), velocity)}), "ode.n60");
    b.step("ode.t64", Sym(b.y), "ode.n61");
    addCheckKey(b.r, "ode.c11", true, "ode.c12");
    b.r.warnings.push_back("ode.w2");
    return true;
}

static bool solveHigherOrder(Builder& b, const Ex& F) {
    const int order = maxOrderOf(F, b.x);
    if (order < 2) return false;
    b.r.order = order;
    b.r.normalized = F;
    // No bare y: reduce with p = y' before trying the linear classifier.
    if (!dependsOn(F, b.y)) return solveMissingDependent(b, F, order);

    std::vector<Ex> a;
    Ex rhs;
    if (!extractLinearODE(F, b.y, b.x, a, rhs)) {
        if (!dependsOn(F, b.x)) return solveMissingIndependent(b, F, order);
        return false;
    }
    int n = (int)a.size() - 1;
    if (n < 2) {
        if (!dependsOn(F, b.x)) return solveMissingIndependent(b, F, order);
        return false;
    }
    b.r.order = n;
    b.r.normalized = F;
    bool constCoef = true;
    for (auto& c : a) if (!c->isNum()) constCoef = false;
    if (!constCoef) {
        b.r.ok = false;
        b.r.typeId = "linear_higher";
        b.fail("ode.f7");
        return true;
    }
    if (n == 2 && a.size() == 3 && a[0]->isZero() && a[1]->isZero() && !rhs->isZero()) {
        Ex f = simplify(Mul(rhs, Pow(a[2], Num(-1))));
        Ex y1 = integrate(f, b.x);
        Ex y = Add(integrate(y1, b.x), Mul(Cst(1), Sym(b.x)), Cst(2));
        b.step("ode.t65", F, "ode.n62");
        b.step("ode.t66", y1, "ode.n63");
        b.step("ode.t67", y, "ode.n64");
        setSolution(b.r, "direct_integration", simplify(y), true, simplify(y));
        verifyExplicit(b.r, F, simplify(y), b.x, b.y, b.en);
        return true;
    }
    if (!rhs->isZero()) {
        b.step("ode.t68", F, "ode.n65", pjson({{"n0", std::to_string(n)}}));
        Ex hom = constCoeffSolution(a, b.x, b, false);
        if (!hom) {
            b.r.ok = false;
            b.r.typeId = "linear_higher";
            b.fail("ode.f8");
            return true;
        }
        b.step("ode.t69", hom, "ode.n66");
        Ex part = particularSolution(a, rhs, b.x, b, b.en);
        if (!part) {
            b.r.ok = false;
            b.r.typeId = "linear_higher";
            b.fail("ode.f9");
            return true;
        }
        Ex sol = simplify(Add(hom, part));
        b.step("ode.t70", sol, "ode.n67");
        setSolution(b.r, "linear_cc", sol, true, sol);
        verifyExplicit(b.r, F, sol, b.x, b.y, b.en);
        return true;
    }
    b.step("ode.t.type_identification", F, "ode.n.linear_homogeneous_order",
           pjson({{"n", std::to_string(n)}}));
    Ex sol = constCoeffSolution(a, b.x, b);
    if (!sol) {
        b.r.ok = false;
        b.r.typeId = "linear_higher";
        b.fail("ode.f10");
        return true;
    }
    setSolution(b.r, "linear_cc", sol, true, sol);
    verifyExplicit(b.r, F, sol, b.x, b.y, b.en);
    return true;
}

// ---------------------------------------------------------------- API
Ex parseODE(const std::string& input, const std::string& x, std::string* dep) {
    return parseODEExpr(input, x, dep, {x, "y", "c"});
}

bool isClosedForm(const Ex& e) { return !hasUnevaluatedIntegral(e); }

std::string typeNameFa(const std::string&) { return std::string(); }

std::string typeNameEn(const std::string&) { return std::string(); }

Result solveODE(const std::string& input, const OdeOptions& opt) {
    auto t0 = std::chrono::steady_clock::now();
    Result r;
    r.mode = "ode";
    r.engine = "native";
    Builder b(r, opt);
    try {
        std::string dep;
        Ex F = parseODEExpr(input, opt.x, &dep, {opt.x, opt.y, "c"});
        if (!dep.empty() && dep != opt.y && dep != "dx" && dep != "dy") {
            b.y = dep;
            b.Y = Sym(b.y);
            b.P = Deriv(b.Y, b.x, 1);
        }
        r.normalized = F;
        std::vector<std::string> syms;
        collectSymbols(F, syms);
        bool diffForm = false;
        for (auto& s : syms) if (s == "dx" || s == "dy") diffForm = true;

        Ex M, N;
        if (diffForm) {
            if (!extractMN(F, b.x, b.y, M, N)) {
                b.fail("ode.f11");
                r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                return r;
            }
            b.step("ode.t.differential_form", nullptr, "ode.n.differential_form",
                   pjson({{"M", toPretty(M)}, {"N", toPretty(N)}}));
            r.order = 1;
            // try exact / IF directly
            if (tryExact(b, F, M, N, true)) {
                r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                return r;
            }
            // try as explicit y' = -M/N
            Ex yp = simplify(Mul(Num(-1), M, Pow(N, Num(-1))));
            std::vector<TermPow> tv;
            powerTerms(yp, b.y, tv);
            if (tryRiccati(b, yp, tv)) {
                r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                return r;
            }
            // Not exact and not Riccati: run the full explicit first-order
            // pipeline on y' = -M/N (separable, homogeneous, linear, ...).
            try {
                Result rr = solveODE("y' = " + toPlain(yp), opt);
                if (rr.ok) {
                    Step form;
                    form.titleKey = "ode.t.differential_form";
                    form.noteKey = "ode.n.differential_form_not_exact";
                    rr.steps.insert(rr.steps.begin(), form);
                    rr.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                    rr.normalized = F;
                    return rr;
                }
            } catch (...) {}
            b.fail("ode.f12");
            r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            return r;
        }

        // derivative order
        int maxOrder = 0;
        std::function<void(const Ex&)> scan = [&](const Ex& e) {
            if (!e) return;
            if (isDeriv(e) && e->args[1]->isSym(b.x)) maxOrder = std::max(maxOrder, derivOrder(e));
            for (auto& c : e->args) scan(c);
        };
        scan(F);
        if (maxOrder == 0) {
            b.fail("ode.f13");
            r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            return r;
        }
        r.order = maxOrder;

        if (maxOrder >= 2) {
            if (solveHigherOrder(b, F)) {
                // The symbolic route may have reported an honest failure. For a
                // second-order equation the numeric fallback still gives a real
                // answer, so replace the error instead of stopping there.
                if (!r.ok && maxOrder == 2 && tryNumericSecondOrder(b, F)) {
                    r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                    return r;
                }
                r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                return r;
            }
            // A second-order equation outside the symbolic catalogue still has
            // a real, checkable answer: numeric RK4 trajectories.
            if (maxOrder == 2 && tryNumericSecondOrder(b, F)) {
                r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                return r;
            }
            // y'' = f(x)
            std::vector<Ex> cs;
            if (maxOrder == 2 && polyCoeffs(expand(F), "__p2", cs)) {
                // not applicable; try direct: F is linear in y''
            }
            r.ok = false;
            b.fail("ode.f14");
            r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            return r;
        }

        // ---- order 1
        Ex p = Deriv(b.Y, b.x, 1);
        if (tryClairautLagrange(b, F, p)) {
            r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            return r;
        }
        // solve for y'
        Ex t = Sym("__p");
        Ex G = substituteSubexpr(F, p, t);
        std::vector<Ex> cs;
        Ex yp;
        if (polyCoeffs(expand(G), "__p", cs) && cs.size() >= 2) {
            if (cs.size() == 2 && !cs[1]->isZero()) {
                yp = simplify(Mul(Num(-1), cs[0], Pow(cs[1], Num(-1))));
            } else if (cs.size() == 3 && !cs[2]->isZero()) {
                // nonlinear in y' (quadratic) — try symbolic quadratic formula
                Ex disc = simplify(Add(Pow(cs[1], Num(2)), Mul(Num(-4), cs[2], cs[0])));
                Ex sq = Sqrt(disc);
                Ex root1 = simplify(Mul(Add(Mul(Num(-1), cs[1]), sq), Num(Rational(1, 2)), Pow(cs[2], Num(-1))));
                Ex root2 = simplify(Mul(Add(Mul(Num(-1), cs[1]), Mul(Num(-1), sq)), Num(Rational(1, 2)), Pow(cs[2], Num(-1))));
                // solve each branch independently (without steps duplication)
                const bool twoBranches = !equals(root1, root2);
                Result best;
                for (auto& br : {root1, root2}) {
                    OdeOptions o2 = opt;
                    o2.steps = opt.steps;
                    Result rr = solveODE("y' = " + toPlain(br), o2);
                    if (rr.ok) { best = rr; break; }
                }
                if (best.ok) {
                    if (twoBranches) {
                        best.warnings.push_back("ode.w.two_branches");
                    }
                    best.engine = "native";
                    best.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                    return best;
                }
                yp = nullptr;
            } else {
                yp = nullptr;
            }
        }
        if (!yp) {
            b.fail("ode.f15");
            r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            return r;
        }
        r.normalized = F;

        std::vector<TermPow> ts;
        if (!powerTerms(yp, b.y, ts)) ts.clear();
        // An exact equation written as M(x,y) + N(x,y)y′ = 0 should be solved
        // by the potential function, not by the linear integrating factor
        // (which often leaves a needlessly complicated integral).
        if (cs.size() == 2) {
            Ex M0 = cs[0], N0 = cs[1];
            Ex d0 = simplify(Add(simplify(diff(M0, b.y)), Mul(Num(-1), simplify(diff(N0, b.x)))));
            if (d0->isZero() && (dependsOn(M0, b.y) || dependsOn(N0, b.y))) {
                if (tryExact(b, F, M0, N0, false)) {
                    r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                    return r;
                }
            }
        }
        // Linear and separable forms are the most direct; try them first so
        // that e.g. y′ = 1 + y² is solved by separation instead of an awkward
        // Riccati transformation.
        if (!ts.empty() && tryLinear(b, yp, ts)) {
            r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            return r;
        }
        // Bernoulli first: separation would leave an integral such as
        // ∫dy/(√y−y), while v=y^(1−n) gives an elementary closed form.
        if (!ts.empty() && tryBernoulli(b, yp, ts)) {
            r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            return r;
        }
        if (tryAffineHomogeneous(b, yp)) {
            r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            return r;
        }
        if (trySeparable(b, yp)) {
            r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            return r;
        }
        if (tryHomogeneous(b, yp)) {
            r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            return r;
        }
        if (!ts.empty()) {
            if (tryRiccati(b, yp, ts)) {
                r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                return r;
            }
        }
        if (tryLinearSub(b, yp)) {
            r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            return r;
        }
        // exact / integrating factor from coefficients in y'
        if (cs.size() == 2) {
            Ex M = cs[0], N = cs[1];
            if (tryExact(b, F, M, N, false)) {
                r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                return r;
            }
        }
        // pure quadrature
        if (!dependsOn(yp, b.y)) {
            Ex sol = Add(integrate(yp, b.x), Cst(1));
            b.step("ode.t71", sol, "ode.n68");
            setSolution(b.r, "quadrature", sol, true, sol);
            b.r.normalized = F;
            return b.r;
        }
        {
            Result simplified = trySimplifiedFallback(F, opt);
            if (simplified.ok) {
                simplified.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - t0).count();
                return simplified;
            }
        }
        r.ok = false;
        b.fail("ode.f16");
        r.normalized = F;
    } catch (const OverflowError& e) {
        b.fail("ode.f17", pjson({{"p0", e.what()}}));
    } catch (const std::exception& e) {
        b.fail("ode.f18", pjson({{"p0", e.what()}}));
    }
    r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    return r;
}

// ---------------------------------------------------------------- Wronskian
static Ex determinant(std::vector<std::vector<Ex>> m) {
    int n = (int)m.size();
    if (n == 1) return m[0][0];
    if (n == 2) return simplify(Add(Mul(m[0][0], m[1][1]), Mul(Num(-1), Mul(m[0][1], m[1][0]))));
    Ex total = Num(0);
    for (int col = 0; col < n; ++col) {
        std::vector<std::vector<Ex>> sub;
        for (int i = 1; i < n; ++i) {
            std::vector<Ex> row;
            for (int j = 0; j < n; ++j) if (j != col) row.push_back(m[i][j]);
            sub.push_back(row);
        }
        Ex cof = Mul(m[0][col], determinant(sub));
        if (col % 2) cof = Mul(Num(-1), cof);
        total = Add(total, cof);
    }
    return simplify(total);
}

Result solveWronskian(const std::vector<std::string>& functions, const std::string& x,
                      const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "wronskian";
    r.typeId = "wronskian";
    try {
        if (functions.empty()) {
            r.errorCode = "ode.f1001";
            return r;
        }
        if (functions.size() > 6) {
            r.errorCode = "ode.f1002";
            return r;
        }
        Ex X = Sym(x);
        std::vector<Ex> fs;
        for (auto& s : functions) {
            std::string t = trim(s);
            if (t.empty()) continue;
            fs.push_back(parseExpr(t));
        }
        if (fs.empty()) {
            r.errorCode = "ode.f1003";
            return r;
        }
        int n = (int)fs.size();
        std::vector<std::vector<Ex>> M(n, std::vector<Ex>(n));
        for (int j = 0; j < n; ++j) {
            Ex cur = fs[j];
            for (int i = 0; i < n; ++i) {
                M[i][j] = (i == 0) ? cur : simplify(diff(cur, x));
                cur = M[i][j];
            }
        }
        for (int j = 0; j < n; ++j) {
            for (int i = 1; i < n; ++i) M[i][j] = simplify(M[i][j]);
        }
        Ex W = simplify(expand(determinant(M)));
        r.wronskian = W;
        r.functions = functions;
        r.order = n - 1;
        Ex expansion = nullptr;
        if (n == 1) {
            expansion = M[0][0];
        } else if (n == 2) {
            expansion = Add(Mul(M[0][0], M[1][1]),
                            Mul(Num(-1), Mul(M[0][1], M[1][0])));
        } else if (n == 3) {
            std::vector<Ex> terms;
            for (int col = 0; col < 3; ++col) {
                std::vector<std::vector<Ex>> sub;
                for (int i = 1; i < 3; ++i) {
                    std::vector<Ex> row;
                    for (int j = 0; j < 3; ++j) if (j != col) row.push_back(M[i][j]);
                    sub.push_back(row);
                }
                Ex minor = determinant(sub);
                Ex term = Mul(M[0][col], minor);
                if (col % 2) term = Mul(Num(-1), term);
                terms.push_back(term);
            }
            expansion = Add(terms);
        }
        if (expansion) expansion = simplify(expand(expansion));

        std::string matrixMath = "[";
        for (int i = 0; i < n; ++i) {
            if (i) matrixMath += "; ";
            for (int j = 0; j < n; ++j) {
                if (j) matrixMath += ", ";
                matrixMath += toPretty(M[i][j]);
            }
        }
        matrixMath += "]";
        addStepKey(r, "ode.t.wronskian_matrix", nullptr, "", "", matrixMath);
        addStepKey(r, "ode.t.expand_determinant", expansion,
                   "ode.n.cofactor_expansion");
        addStepKey(r, "ode.t.wronskian_determinant", W, "", "",
                   "W = det[f_i^(j)]");
        // Only a symbolic zero proves linear dependence.  A numerically tiny
        // W can come from small coefficients (e.g. 1e-9*x), so it is reported
        // as a caveat rather than asserted as a proof.
        const bool zero = W->isZero();
        if (!zero && numericallyZero(W, {x}, 8, 1e-7)) {
            r.warnings.push_back("ode.w3");
        }
        r.independent = !zero;
        // Cross-check the symbolic determinant against a direct numeric one.
        bool numericCheck = true;
        int checked = 0;
        for (double xv : {0.37, 1.13, 2.71}) {
            std::vector<std::vector<double>> dm(n, std::vector<double>(n, 0.0));
            bool ok = true;
            for (int i = 0; i < n && ok; ++i) {
                for (int j = 0; j < n; ++j) {
                    std::map<std::string, double> vars;
                    vars[x] = xv;
                    bool o = false;
                    dm[i][j] = evalReal(M[i][j], vars, &o);
                    if (!o || !std::isfinite(dm[i][j])) { ok = false; break; }
                }
            }
            if (!ok) continue;
            double det = 1.0;
            for (int col = 0; col < n; ++col) {
                int piv = col;
                for (int row = col + 1; row < n; ++row)
                    if (std::fabs(dm[row][col]) > std::fabs(dm[piv][col])) piv = row;
                if (std::fabs(dm[piv][col]) < 1e-12) { det = 0.0; break; }
                if (piv != col) { std::swap(dm[piv], dm[col]); det = -det; }
                det *= dm[col][col];
                for (int row = col + 1; row < n; ++row) {
                    const double f = dm[row][col] / dm[col][col];
                    for (int k = col; k < n; ++k) dm[row][k] -= f * dm[col][k];
                }
            }
            std::map<std::string, double> vars;
            vars[x] = xv;
            bool okW = false;
            const double wv = evalReal(W, vars, &okW);
            if (!okW || !std::isfinite(wv) ||
                std::fabs(wv - det) > 1e-6 * (1.0 + std::fabs(det))) {
                numericCheck = false;
            }
            ++checked;
        }
        if (checked > 0) {
            addCheckKey(r, "ode.c13", numericCheck, "ode.c14");
        }
        addStepKey(r, "ode.t.conclusion", W,
                   zero ? "ode.n.wronskian_zero" : "ode.n.wronskian_nonzero");
        r.assumptions.push_back("ode.a1");
        addCheckKey(r, "ode.c_wronskian", !zero,
                    zero ? "ode.c_wronskian_zero" : "ode.c_wronskian_nonzero");
        r.solution = W;
        r.ok = true;
    } catch (const std::exception& e) {
        setCodedFailure(r, e, "wronskian");
    }
    return r;
}

// Symbolic roots of a linear or quadratic family equation; defined below.
static std::vector<Ex> quadraticBranches(const Ex& F, const std::string& var);

// ---------------------------------------------------------------- envelope / family
Result solveEnvelope(const std::string& input, const std::string& c, bool orthogonal,
                     const std::string& x, const std::string& y, const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "envelope";
    r.typeId = "envelope";
    try {
        std::string dep;
        Ex F = parseODEExpr(input, x, &dep, {x, y, c});
        r.family = input;
        r.normalized = F;
        Ex Fc = simplify(diff(F, c));
        addStepKey(r, "ode.t.family_of_curves", F, "ode.n.family_parameter_c");
        std::vector<Ex> cvals;
        Ex lin = solveLinear(Fc, c);
        if (lin) {
            cvals.push_back(lin);
        } else {
            // A quadratic in c with symbolic coefficients (e.g. y = c x + c^3
            // gives -x - 3 c^2) still has a usable closed form.
            cvals = quadraticBranches(Fc, c);
            if (cvals.empty()) {
                auto roots = solvePoly(Fc, c);
                for (auto& rr : roots) cvals.push_back(rr);
            }
        }
        if (cvals.empty()) {
            // resultant-free fallback: solve linear in c if possible
            std::vector<Ex> cs;
            if (polyCoeffs(expand(Fc), c, cs) && cs.size() == 2)
                cvals.push_back(simplify(Mul(Num(-1), cs[0], Pow(cs[1], Num(-1)))));
        }
        if (cvals.empty()) {
            // No envelope candidate.  The family can still be meaningful, and
            // orthogonal trajectories are computed from F itself below.
            r.warnings.push_back("ode.w4");
        }
        for (auto& cv : cvals) {
            // If ∂F/∂c = 0 pins c to a value independent of x and y, the
            // resulting curve is just one member of the family (degenerate),
            // never an envelope.
            if (!dependsOn(cv, x) && !dependsOn(cv, y)) {
                r.warnings.push_back("ode.w.family_member");
                continue;
            }
            Ex curve = simplify(substitute(F, c, cv));
            // degenerate point (e.g. c=0 in concentric circles) is not an envelope
            // A curve that depends on at least one of x/y is a valid envelope;
            // horizontal or vertical envelopes (e.g. y^2 - 1 = 0) are allowed.
            if (curve->isZero() || (!dependsOn(curve, x) && !dependsOn(curve, y))) {
                r.warnings.push_back("ode.w.degenerate_curve");
                continue;
            }
            r.envelope.push_back(curve);
            addStepKey(r, "ode.t.eliminate_parameter_c", curve,
                       "ode.n.tangency_candidate", pjson({{"c", toPretty(cv)}}));
            // The envelope must be tangent to every member of the family. Compare
            // the implicit slopes of the candidate and of the family at c = c(x,y).
            {
                Ex Fx = simplify(substitute(diff(F, x), c, cv));
                Ex Fy = simplify(substitute(diff(F, y), c, cv));
                Ex Cx = simplify(diff(curve, x));
                Ex Cy = simplify(diff(curve, y));
                bool tangencyKnown = false;
                bool tangent = false;
                if (!Fy->isZero() && !Cy->isZero()) {
                    Ex sF = simplify(Mul(Num(-1), Fx, Pow(Fy, Num(-1))));
                    Ex sC = simplify(Mul(Num(-1), Cx, Pow(Cy, Num(-1))));
                    sF = simplify(substitute(sF, c, cv));
                    Ex gap = simplify(Add({sF, Mul(Num(-1), sC)}));
                    tangent = gap->isZero() || numericallyZero(gap, {x, y}, 8, 1e-7);
                    tangencyKnown = true;
                } else if (Fy->isZero() && Cy->isZero()) {
                    tangent = true; // both tangents are vertical
                    tangencyKnown = true;
                }
                if (tangencyKnown) {
                    addCheckKey(r, "ode.c17", tangent, "ode.c18");
                    addStepKey(r, "ode.t.tangency_check", nullptr,
                               tangent ? "ode.n.tangent_ok" : "ode.n.tangent_fail");
                }
            }
        }
        r.assumptions.push_back("ode.a2");
        r.solution = r.envelope.empty() ? nullptr : r.envelope[0];
        r.isExplicit = false;
        r.ok = true;
        if (r.envelope.empty())
            addStepKey(r, "ode.t.conclusion", nullptr, "ode.n.no_envelope");
        r.closedForm = true;
        if (orthogonal) {
            // slope of family: y' = -F_x/F_y (with c eliminated if possible)
            Ex Fx = simplify(diff(F, x));
            Ex Fy = simplify(diff(F, y));
            if (Fy->isZero()) {
                r.warnings.push_back("ode.w5");
            } else {
                Ex slope = simplify(Mul(Num(-1), Fx, Pow(Fy, Num(-1))));
                Ex cElim = nullptr;
                Ex clin = solveLinear(F, c);
                if (clin) cElim = clin;
                else {
                    auto rts = solvePoly(F, c);
                    if (!rts.empty()) cElim = rts[0];
                }
                if (cElim) slope = simplify(substitute(slope, c, cElim));
                if (dependsOn(slope, c)) {
                    r.warnings.push_back("ode.w6");
                    addStepKey(r, "ode.t.orthogonal_eq", nullptr, "", "",
                               "y' = -1 / (" + toPretty(slope) + ")");
                } else {
                    Ex ortho = simplify(Mul(Num(-1), Pow(slope, Num(-1))));
                    addStepKey(r, "ode.s9", slope);
                    addStepKey(r, "ode.t.orthogonal_eq", nullptr, "", "",
                               "y' = " + toPretty(ortho));
                    OdeOptions o; o.x = x; o.y = y; o.lang = lang;
                    Result rr = solveODE("y' = " + toPlain(ortho), o);
                    if (rr.ok) {
                        r.orthogonal = rr.isExplicit && rr.explicitY ? rr.explicitY : rr.solution;
                        r.orthogonalExplicit = rr.isExplicit;
                        addStepKey(r, "ode.t.solve_orthogonal", rr.solution,
                                   rr.isExplicit ? "ode.n.orthogonal_explicit"
                                                 : "ode.n.orthogonal_implicit");
                        r.assumptions.insert(r.assumptions.end(), rr.assumptions.begin(), rr.assumptions.end());
                        r.checks.insert(r.checks.end(), rr.checks.begin(), rr.checks.end());
                    } else {
                        r.warnings.push_back("ode.w7");
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        setCodedFailure(r, e, "envelope");
    }
    return r;
}

// ---------------------------------------------------------------- orthogonal
// Solve F(x,y,c) = 0 for a variable when F is at most quadratic in it. The
// coefficients may depend on the other symbols, which covers families such as
// (y-c)^2 = x and x^2 + y^2 = c^2 that the rational-root solver rejects because
// its coefficients are not numeric.
static std::vector<Ex> quadraticBranches(const Ex& F, const std::string& var) {
    std::vector<Ex> out;
    std::vector<Ex> a;
    if (!polyCoeffs(expand(F), var, a)) return out;
    while (a.size() > 1 && a.back()->isZero()) a.pop_back();
    if (a.size() == 2 && !a[1]->isZero()) {
        out.push_back(simplify(Mul(Num(-1), a[0], Pow(a[1], Num(-1)))));
        return out;
    }
    if (a.size() == 3 && !a[2]->isZero()) {
        Ex disc = simplify(Add(Pow(a[1], Num(2)), Mul(Num(-4), a[2], a[0])));
        Ex sq = Sqrt(disc);
        Ex den = Pow(Mul(Num(2), a[2]), Num(-1));
        Ex r1 = simplify(Mul(Add(Mul(Num(-1), a[1]), sq), den));
        Ex r2 = simplify(Mul(Add(Mul(Num(-1), a[1]), Mul(Num(-1), sq)), den));
        out.push_back(r1);
        if (!equals(r1, r2)) out.push_back(r2);
    }
    return out;
}

// ---------------------------------------------------------------- trajectories
// tan(alpha) as an exact expression for the common angles and a bounded
// rational approximation otherwise.  Exact 30/45/60 keep the derivation
// readable; arbitrary angles still solve numerically through the same engine.
static Ex tanDegrees(double angleDeg) {
    const double rad = angleDeg * 3.14159265358979323846 / 180.0;
    const double t = std::tan(rad);
    if (std::fabs(t) < 1e-14) return Num(0);
    if (std::fabs(t - 1.0) < 1e-12) return Num(1);
    if (std::fabs(t + 1.0) < 1e-12) return Num(-1);
    const double s3 = std::sqrt(3.0);
    if (std::fabs(t - s3) < 1e-10) return Sqrt(Num(3));
    if (std::fabs(t + s3) < 1e-10) return Mul(Num(-1), Sqrt(Num(3)));
    if (std::fabs(t - 1.0 / s3) < 1e-10) {
        return simplify(Mul(Sqrt(Num(3)), Pow(Num(3), Num(-1))));
    }
    if (std::fabs(t + 1.0 / s3) < 1e-10) {
        return Mul(Num(-1), simplify(Mul(Sqrt(Num(3)), Pow(Num(3), Num(-1)))));
    }
    return Num(Rational::fromDouble(t, 1000));
}

// Slope of the trajectory family from the family slope m.  A 90° crossing is
// the orthogonal case y′ = −1/m; otherwise tan α = (m_t − m)/(1 + m_t m)
// gives m_t = (m + tan α)/(1 − m tan α).
static Ex trajectorySlopeFrom(const Ex& familySlope, double angleDeg, bool orthogonal,
                              bool& vertical) {
    vertical = false;
    const bool rightAngle = orthogonal || std::fabs(angleDeg - 90.0) < 1e-9;
    if (rightAngle) {
        if (familySlope->isZero()) {
            vertical = true;
            return nullptr;
        }
        return simplify(Mul(Num(-1), Pow(familySlope, Num(-1))));
    }
    if (std::fabs(angleDeg) < 1e-9) return familySlope;
    Ex t = tanDegrees(angleDeg);
    Ex denom = simplify(Add(Num(1), Mul(Num(-1), familySlope, t)));
    if (denom->isZero()) {
        vertical = true;
        return nullptr;
    }
    return simplify(Mul(Add(familySlope, t), Pow(denom, Num(-1))));
}

static Result solveTrajectory(const std::string& input, const std::string& c,
                              double angleDeg, const std::string& x,
                              const std::string& y, const std::string& lang,
                              bool orthogonal) {
    const bool en = lang != "fa";
    const bool rightAngle = orthogonal || std::fabs(angleDeg - 90.0) < 1e-9;
    Result r;
    r.mode = orthogonal ? "orthogonal" : "oblique";
    r.typeId = r.mode;
    r.trajectoryMode = r.mode;
    r.trajectoryAngle = rightAngle ? 90.0 : angleDeg;
    r.family = input;

    const std::string eqTitle =
        orthogonal ? "ode.t.orthogonal_eq" : "ode.t.oblique_eq";
    const std::string familyTitle =
        orthogonal ? "ode.t.orthogonal_family" : "ode.t.oblique_family";
    const std::string trajTitle =
        orthogonal ? "ode.t.orthogonal_traj" : "ode.t.oblique_traj";
    auto familyNote = [&](bool explicitY) -> std::string {
        if (orthogonal) {
            return explicitY ? "ode.n.orthogonal_explicit"
                             : "ode.n.orthogonal_implicit";
        }
        return explicitY ? "ode.n.oblique_explicit" : "ode.n.oblique_implicit";
    };
    auto publish = [&]() {
        if (rightAngle) {
            r.orthogonal = r.trajectory;
            r.orthogonalExplicit = r.trajectoryExplicit;
            r.orthogonalSlope = r.trajectorySlope;
        }
    };
    auto finishWith = [&](const Ex& traj, bool explicitY, const Ex& slope) {
        r.trajectory = traj;
        r.trajectoryExplicit = explicitY;
        r.trajectorySlope = slope;
        r.solution = traj;
        r.ok = true;
        r.closedForm = true;
        publish();
    };

    try {
        std::string dep;
        Ex F = parseODEExpr(input, x, &dep, {x, y, c});
        r.normalized = F;
        // Explicit y(x,c) branches let the UI overlay the original family on
        // the trajectory direction field. Linear and quadratic families are
        // covered; a vertical family has no y-branch and is simply skipped.
        r.familyBranches = quadraticBranches(F, y);
        Ex Fx = simplify(diff(F, x));
        Ex Fy = simplify(diff(F, y));

        if (Fy->isZero()) {
            if (Fx->isZero()) {
                r.errorCode = "ode.f1004";
                return r;
            }
            addStepKey(r, "ode.t.vertical_family", F,
                       "ode.n.vertical_family_no_slope");
            if (rightAngle) {
                addStepKey(r, trajTitle, Sym(y), "ode.n.vertical_to_horizontal");
                finishWith(Sym(y), false, Num(0));
                return r;
            }
            if (std::fabs(angleDeg) < 1e-9) {
                addStepKey(r, trajTitle, Sym(x), "ode.n.zero_angle_vertical");
                finishWith(Sym(x), false, nullptr);
                return r;
            }
            Ex cot = simplify(Pow(tanDegrees(angleDeg), Num(-1)));
            addStepKey(r, eqTitle, Add({Deriv(Sym(y), x, 1), Mul(Num(-1), cot)}),
                       "ode.n.vertical_slope_cot");
            OdeOptions o;
            o.x = x;
            o.y = y;
            o.lang = lang;
            Result rr = solveODE("y' = " + toPlain(cot), o);
            if (!rr.ok) {
                r.warnings.push_back("ode.w8");
                r.ok = true;
                r.closedForm = false;
                return r;
            }
            bool renamedFinal = false;
            if (!rr.steps.empty()) {
                Step& last = rr.steps.back();
                last.titleKey = familyTitle;
                last.noteKey = familyNote(rr.isExplicit);
                last.note.clear();
                renamedFinal = true;
            }
            for (auto& s : rr.steps) r.steps.push_back(s);
            r.assumptions.insert(r.assumptions.end(), rr.assumptions.begin(), rr.assumptions.end());
            r.checks.insert(r.checks.end(), rr.checks.begin(), rr.checks.end());
            Ex traj = rr.isExplicit && rr.explicitY ? rr.explicitY : rr.solution;
            if (!renamedFinal) addStepKey(r, familyTitle, traj, familyNote(rr.isExplicit));
            r.trajectory = traj;
            r.trajectoryExplicit = rr.isExplicit;
            r.trajectorySlope = cot;
            r.solution = traj;
            r.closedForm = rr.closedForm;
            r.ok = true;
            publish();
            return r;
        }

        Ex slope = simplify(Mul(Num(-1), Fx, Pow(Fy, Num(-1))));

        Ex cElim = nullptr;
        if (dependsOn(slope, c)) {
            std::vector<Ex> branches = quadraticBranches(F, c);
            if (branches.empty()) {
                Ex clin = solveLinear(F, c);
                if (clin) branches.push_back(clin);
            }
            // Keep only branches that actually remove c from the slope. A branch
            // that leaves c behind cannot describe a single trajectory family.
            std::vector<Ex> usable;
            for (auto& br : branches) {
                Ex cand = simplify(substitute(slope, c, br));
                if (!dependsOn(cand, c)) usable.push_back(br);
            }
            if (usable.empty()) {
                addStepKey(r, eqTitle, nullptr, "", "",
                           orthogonal
                               ? "y' = -1 / (" + toPretty(slope) + ")"
                               : "y' = (" + toPretty(slope) +
                                     " + tan(a)) / (1 - " + toPretty(slope) +
                                     " tan(a))");
                r.warnings.push_back("ode.w9");
                r.ok = true;
                r.closedForm = false;
                return r;
            }
            if (usable.size() > 1) {
                r.warnings.push_back("ode.w10");
            }
            cElim = usable.front();
            slope = simplify(substitute(slope, c, cElim));
        }

        if (dependsOn(slope, c)) {
            addStepKey(r, eqTitle, nullptr, "", "",
                       orthogonal
                           ? "y' = -1 / (" + toPretty(slope) + ")"
                           : "y' = (" + toPretty(slope) +
                                 " + tan(a)) / (1 - " + toPretty(slope) +
                                 " tan(a))");
            r.warnings.push_back("ode.w11");
            r.ok = true;
            r.closedForm = false;
            return r;
        }

        const std::string elimTitle = "ode.t.eliminate_c";
        if (cElim) {
            addStepKey(r, elimTitle, Add({Deriv(Sym(y), x, 1), Mul(Num(-1), slope)}),
                       "ode.n.eliminate_c_value", pjson({{"c", toPretty(cElim)}}));
        } else {
            addStepKey(r, elimTitle, Add({Deriv(Sym(y), x, 1), Mul(Num(-1), slope)}),
                       "ode.n.eliminate_c_direct");
        }

        if (slope->isZero()) {
            if (rightAngle) {
                addStepKey(r, trajTitle, Sym(x), "ode.n.horizontal_family");
                finishWith(Sym(x), false, nullptr);
                return r;
            }
            if (std::fabs(angleDeg) < 1e-9) {
                addStepKey(r, trajTitle, Sym(y), "ode.n.zero_angle_horizontal");
                finishWith(Sym(y), false, Num(0));
                return r;
            }
        }

        bool vertical = false;
        Ex trajSlope = trajectorySlopeFrom(slope, angleDeg, orthogonal, vertical);
        if (vertical) {
            addStepKey(r, trajTitle, Sym(x), "ode.n.infinite_trajectory_slope");
            finishWith(Sym(x), false, nullptr);
            return r;
        }
        r.trajectorySlope = trajSlope;

        if (orthogonal) {
            addStepKey(r, eqTitle, Add({Deriv(Sym(y), x, 1), Mul(Num(-1), trajSlope)}),
                       "ode.n.perpendicular_slopes_ode");
        } else {
            addStepKey(r, eqTitle, Add({Deriv(Sym(y), x, 1), Mul(Num(-1), trajSlope)}),
                       "ode.n.oblique_angle_ode",
                       pjson({{"tan", toPretty(tanDegrees(angleDeg))}}));
        }

        OdeOptions o;
        o.x = x;
        o.y = y;
        o.lang = lang;
        Result rr = solveODE("y' = " + toPlain(trajSlope), o);
        if (!rr.ok) {
            r.warnings.push_back("ode.w12");
            r.ok = true;
            r.closedForm = false;
            return r;
        }
        bool renamedFinal = false;
        if (!rr.steps.empty()) {
            Step& last = rr.steps.back();
            last.titleKey = familyTitle;
            last.noteKey = familyNote(rr.isExplicit);
            last.note.clear();
            renamedFinal = true;
        }
        for (auto& s : rr.steps) r.steps.push_back(s);
        r.assumptions.insert(r.assumptions.end(), rr.assumptions.begin(), rr.assumptions.end());
        r.checks.insert(r.checks.end(), rr.checks.begin(), rr.checks.end());
        Ex traj = rr.isExplicit && rr.explicitY ? rr.explicitY : rr.solution;
        if (!renamedFinal) r.steps.push_back({familyTitle, traj, familyNote(rr.isExplicit)});
        r.trajectory = traj;
        r.trajectoryExplicit = rr.isExplicit;
        r.trajectorySlope = trajSlope;
        r.solution = traj;
        r.closedForm = rr.closedForm;
        r.ok = true;
        publish();
        r.assumptions.push_back("ode.a3");
    } catch (const std::exception& e) {
        setCodedFailure(r, e, "trajectory");
    }
    return r;
}

Result solveOrthogonal(const std::string& input, const std::string& c,
                       const std::string& x, const std::string& y,
                       const std::string& lang) {
    return solveTrajectory(input, c, 90.0, x, y, lang, true);
}

Result solveOblique(const std::string& input, const std::string& c, double angleDeg,
                    const std::string& x, const std::string& y,
                    const std::string& lang) {
    return solveTrajectory(input, c, angleDeg, x, y, lang, false);
}

} // namespace ode
