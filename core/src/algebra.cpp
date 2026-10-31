// Infinit Lab — algebraic normalisation, expansion, simplification, root finding
// SPDX-License-Identifier: Apache-2.0
#include "expr.hpp"
#include <set>
#include <functional>

namespace ode {

// ---------------------------------------------------------------- size guard
static size_t sizeOf(const Ex& a) {
    if (!a) return 0;
    size_t s = 1;
    for (auto& x : a->args) s += sizeOf(x);
    return s;
}

// ---------------------------------------------------------------- expand
static Ex expandProduct(const Ex& a, const Ex& b) {
    std::vector<Ex> as = a->isAdd() ? a->args : std::vector<Ex>{a};
    std::vector<Ex> bs = b->isAdd() ? b->args : std::vector<Ex>{b};
    std::vector<Ex> out;
    for (auto& x : as)
        for (auto& y : bs) {
            out.push_back(Mul(x, y));
            if (out.size() > 4000) return Mul(a, b);
        }
    return Add(out);
}

Ex expand(const Ex& a) {
    if (!a) return a;
    switch (a->kind) {
        case Kind::Num:
        case Kind::Sym:
            return a;
        case Kind::Add: {
            std::vector<Ex> v;
            for (auto& t : a->args) v.push_back(expand(t));
            return Add(v);
        }
        case Kind::Mul: {
            std::vector<Ex> terms{Num(1)};
            for (auto& f : a->args) {
                Ex ef = expand(f);
                std::vector<Ex> sub;
                if (ef->isAdd()) sub = ef->args;
                else sub = {ef};
                std::vector<Ex> next;
                for (auto& t : terms)
                    for (auto& s : sub) {
                        next.push_back(Mul(t, s));
                        if (next.size() > 4000) return a;
                    }
                terms = std::move(next);
            }
            return Add(terms);
        }
        case Kind::Pow: {
            Ex b = expand(a->args[0]);
            Ex e = a->args[1];
            if (e->isNum() && e->num.isInt() && e->num.n > 1 && e->num.n <= 12) {
                Ex r = b;
                for (long long i = 1; i < e->num.n; ++i) r = expandProduct(r, b);
                return r;
            }
            return Pow(b, e);
        }
        case Kind::Func: {
            std::vector<Ex> v;
            for (auto& x : a->args) v.push_back(expand(x));
            return Fn(a->name, v);
        }
    }
    return a;
}

// ---------------------------------------------------------------- fractions
struct Frac { Ex num, den; };

static Frac asFrac(const Ex& a) {
    if (a->isMul()) {
        std::vector<Ex> pos, neg;
        for (auto& f : a->args) {
            if (f->isPow() && f->args[1]->isNum() && f->args[1]->num.isNeg()) {
                neg.push_back(Pow(f->args[0], Num(-f->args[1]->num)));
            } else pos.push_back(f);
        }
        if (!neg.empty()) {
            Ex n = pos.empty() ? Num(1) : Mul(pos);
            Ex d = neg.size() == 1 ? neg[0] : Mul(neg);
            return {n, d};
        }
    }
    if (a->isPow() && a->args[1]->isNum() && a->args[1]->num.isNeg())
        return {Num(1), Pow(a->args[0], Num(-a->args[1]->num))};
    if (a->isNum() && a->num.d != 1)
        return {Num(Rational(a->num.n, 1)), Num(Rational(a->num.d, 1))};
    return {a, Num(1)};
}

Ex cancelCommon(const Ex& a);

Ex together(const Ex& a) {
    if (!a || !a->isAdd()) {
        if (a && a->isMul()) {
            // normalize each factor and cancel
            std::vector<Ex> v;
            for (auto& f : a->args) v.push_back(together(f));
            return cancelCommon(Mul(v));
        }
        if (a && a->isPow()) return Pow(together(a->args[0]), together(a->args[1]));
        if (a && a->isFunc()) {
            std::vector<Ex> v;
            for (auto& f : a->args) v.push_back(together(f));
            return Fn(a->name, v);
        }
        return a;
    }
    if (a->args.size() > 6) return a;
    std::vector<Frac> fs;
    for (auto& t : a->args) fs.push_back(asFrac(together(t)));
    bool anyDen = false;
    for (auto& f : fs) if (!f.den->isOne()) { anyDen = true; break; }
    if (!anyDen) {
        return Add(std::vector<Ex>(a->args.begin(), a->args.end()));
    }
    Ex D = Num(1);
    for (auto& f : fs) if (!f.den->isOne()) D = Mul(D, f.den);
    if (sizeOf(D) > 400) return a;
    std::vector<Ex> nums;
    for (auto& f : fs) {
        Ex q = D;
        if (!f.den->isOne()) q = cancelCommon(Mul(D, Pow(f.den, Num(-1))));
        nums.push_back(Mul(f.num, q));
    }
    Ex N = Add(nums);
    return cancelCommon(Mul(N, Pow(D, Num(-1))));
}

// factor map of a product: base -> exponent
static void factorMap(const Ex& a, std::map<Ex, Ex, struct ExCmp2>& m, Ex& coeff);
struct ExCmp2 {
    bool operator()(const Ex& a, const Ex& b) const { return compareExpr(a, b) < 0; }
};

static void factorMap(const Ex& a, std::map<Ex, Ex, ExCmp2>& m, Ex& coeff) {
    if (a->isNum()) { coeff = Mul(coeff, a); return; }
    if (a->isMul()) {
        for (auto& f : a->args) factorMap(f, m, coeff);
        return;
    }
    Ex base = a, ex = Num(1);
    if (a->isPow()) { base = a->args[0]; ex = a->args[1]; }
    auto it = m.find(base);
    if (it == m.end()) m.emplace(base, ex);
    else it->second = Add(it->second, ex);
}

static Ex fromFactorMap(const std::map<Ex, Ex, ExCmp2>& m, Ex coeff) {
    std::vector<Ex> v;
    if (!coeff->isOne()) v.push_back(coeff);
    for (auto& kv : m) {
        if (kv.second->isZero()) continue;
        if (kv.second->isOne()) v.push_back(kv.first);
        else v.push_back(Pow(kv.first, kv.second));
    }
    if (v.empty()) return Num(1);
    return Mul(v);
}

// try to pull a common multiplicative factor out of a sum
static Ex factorOutCommon(const Ex& a) {
    if (!a->isAdd() || a->args.size() < 2 || a->args.size() > 24) return a;
    std::vector<std::map<Ex, Ex, ExCmp2>> maps(a->args.size());
    std::vector<Ex> coeffs(a->args.size(), Num(1));
    for (size_t i = 0; i < a->args.size(); ++i) factorMap(a->args[i], maps[i], coeffs[i]);
    std::map<Ex, Ex, ExCmp2> common = maps[0];
    for (size_t i = 1; i < maps.size(); ++i) {
        std::map<Ex, Ex, ExCmp2> next;
        for (auto& kv : common) {
            auto it = maps[i].find(kv.first);
            if (it == maps[i].end()) continue;
            // minimal exponent for integer exponents only
            if (kv.second->isNum() && it->second->isNum()) {
                long long e = std::min(kv.second->num.n, it->second->num.n);
                if (e != 0) next.emplace(kv.first, Num(e));
            }
        }
        common.swap(next);
        if (common.empty()) break;
    }
    // numeric gcd
    Rational g(0);
    for (auto& c : coeffs) if (c->isNum()) {
        Rational r = c->num.abs();
        if (g.isZero()) g = r;
        else {
            long long a1 = g.n, b1 = r.n;
            long long gg = std::gcd(a1, b1 == 0 ? 1 : b1);
            if (gg > 0 && g.d == 1 && r.d == 1) g = Rational(gg);
        }
    }
    std::vector<Ex> rest;
    for (size_t i = 0; i < a->args.size(); ++i) {
        Ex q = a->args[i];
        for (auto& kv : common) {
            q = Mul(q, Pow(kv.first, Mul(Num(-1), kv.second)));
        }
        if (!g.isOne() && g.n != 0) q = Mul(Num(g.inv()), q);
        rest.push_back(q);
    }
    Ex inner = Add(rest);
    if (common.empty()) {
        if (g.isOne() || g.isZero()) return a;
        return Mul(Num(g), inner);
    }
    return Mul(fromFactorMap(common, Num(g.isZero() ? 1 : g)), inner);
}

// Numeric content of a sum: a = content * prim, with prim's leading term
// positive.  Used to recognise e.g. 2 - 2xy = -2(xy - 1).
static Rational ratGcd(Rational a, Rational b) {
    a = a.abs(); b = b.abs();
    if (a.isZero()) return b;
    if (b.isZero()) return a;
    long long n = std::gcd(a.n, b.n);
    long long d = std::lcm(a.d, b.d);
    return Rational(n, d);
}

static bool addContent(const Ex& a, Rational& content, Ex& prim) {
    if (!a->isAdd() || a->args.size() < 2) return false;
    std::vector<Rational> cs;
    std::vector<Ex> rests;
    Rational g(0);
    for (auto& t : a->args) {
        Rational c(1); Ex rest = t;
        if (t->isNum()) { c = t->num; rest = Num(1); }
        else if (t->isMul() && t->args.size() >= 2 && t->args[0]->isNum()) {
            c = t->args[0]->num;
            std::vector<Ex> r(t->args.begin() + 1, t->args.end());
            rest = r.size() == 1 ? r[0] : Mul(r);
        }
        if (c.isZero()) continue;
        g = ratGcd(g, c);
        cs.push_back(c);
        rests.push_back(rest);
    }
    if (cs.empty() || g.isZero() || g.isOne()) return false;
    std::vector<Ex> parts;
    for (size_t i = 0; i < cs.size(); ++i) {
        Rational q = cs[i] / g;
        parts.push_back(q.isOne() ? rests[i] : Mul(Num(q), rests[i]));
    }
    Ex p0 = Add(parts);
    if (!p0->isAdd()) return false;
    // make the leading coefficient positive
    Rational lead(1);
    const Ex& first = p0->args[0];
    if (first->isNum()) lead = first->num;
    else if (first->isMul() && first->args[0]->isNum()) lead = first->args[0]->num;
    if (lead.isNeg()) {
        g = Rational(-g.n, g.d);
        std::vector<Ex> neg;
        for (auto& t : p0->args) neg.push_back(Mul(Num(-1), t));
        p0 = Add(neg);
    }
    content = g;
    prim = p0;
    return true;
}

Ex cancelCommon(const Ex& a) {
    if (!a) return a;
    if (a->isMul()) {
        std::vector<Ex> v;
        for (auto& f : a->args) v.push_back(cancelCommon(f));
        Ex m = Mul(v);
        if (!m->isMul()) return m;
        std::map<Ex, Ex, ExCmp2> pos, neg;
        Ex coeff = Num(1);
        auto bump = [](std::map<Ex, Ex, ExCmp2>& mp, const Ex& b, const Ex& e) {
            auto it = mp.find(b);
            if (it == mp.end()) mp.emplace(b, e);
            else it->second = Add(it->second, e);
        };
        for (auto& f : m->args) {
            if (f->isNum()) { coeff = Mul(coeff, f); continue; }
            if (f->isPow() && f->args[1]->isNum() && f->args[1]->num.isNeg()) {
                Ex b = f->args[0]; Ex e = Num(-f->args[1]->num);
                Rational ac; Ex prim;
                if (addContent(b, ac, prim)) {
                    coeff = Mul(coeff, Pow(Num(ac), Num(-1)));
                    bump(neg, prim, e);
                    continue;
                }
                bump(neg, b, e);
            } else {
                Ex b = f, e = Num(1);
                if (f->isPow()) { b = f->args[0]; e = f->args[1]; }
                Rational ac; Ex prim;
                if (addContent(b, ac, prim)) {
                    coeff = Mul(coeff, Num(ac));
                    bump(pos, prim, e);
                    continue;
                }
                bump(pos, b, e);
            }
        }
        // Cancel factors that agree up to sign: (y−x)/(x−y) = −1.
        std::set<Ex, ExCmp2> signUsed;
        for (auto it = pos.begin(); it != pos.end();) {
            if (!it->first->isAdd() || !equals(it->second, Num(1))) { ++it; continue; }
            bool cancelled = false;
            for (auto jt = neg.begin(); jt != neg.end(); ++jt) {
                if (!jt->first->isAdd() || !equals(jt->second, Num(1))) continue;
                if (!Add(it->first, jt->first)->isZero()) continue;
                signUsed.insert(jt->first);
                it = pos.erase(it);
                coeff = Mul(coeff, Num(-1));
                cancelled = true;
                break;
            }
            if (!cancelled) ++it;
        }
        std::vector<Ex> out;
        if (!coeff->isOne()) out.push_back(coeff);
        std::set<Ex, ExCmp2> used;
        for (auto& kv : pos) {
            auto it = neg.find(kv.first);
            Ex e = kv.second;
            if (it != neg.end()) {
                e = Add(e, Mul(Num(-1), it->second));
                used.insert(kv.first);
            }
            if (e->isZero()) continue;
            if (e->isOne()) out.push_back(kv.first);
            else out.push_back(Pow(kv.first, e));
        }
        for (auto& kv : neg) {
            if (used.count(kv.first) || signUsed.count(kv.first)) continue;
            out.push_back(Pow(kv.first, Mul(Num(-1), kv.second)));
        }
        return Mul(out);
    }
    if (a->isAdd()) {
        std::vector<Ex> v;
        for (auto& t : a->args) v.push_back(cancelCommon(t));
        Ex s = Add(v);
        // divide numerator sum by denominator if whole expression is a fraction
        return s;
    }
    if (a->isPow()) return Pow(cancelCommon(a->args[0]), cancelCommon(a->args[1]));
    if (a->isFunc()) {
        std::vector<Ex> v;
        for (auto& t : a->args) v.push_back(cancelCommon(t));
        return Fn(a->name, v);
    }
    return a;
}

Ex simplify(const Ex& a) {
    if (!a) return a;
    if (a->isNum() || a->isSym()) return a;
    if (a->isAdd()) {
        std::vector<Ex> v;
        for (auto& t : a->args) v.push_back(simplify(t));
        Ex s = Add(v);
        if (s->isAdd()) {
            // sin^2(u) + cos^2(u) = 1
            for (size_t i = 0; i < s->args.size(); ++i) {
                // Recognise c·sin²(u) or c·cos²(u) where the numeric
                // coefficient may appear at any position in the product.
                auto sinSq = [](const Ex& t, Ex& arg) -> Rational {
                    Rational coeff(1);
                    Ex sqbase;
                    auto visit = [&](const Ex& f) -> bool {
                        if (f->isNum()) { coeff = coeff * f->num; return true; }
                        Ex base;
                        if (f->isPow() && f->args[1]->isNum() && f->args[1]->num == Rational(2)) base = f->args[0];
                        if (base && base->isFunc() && (base->name == "sin" || base->name == "cos") && !sqbase) {
                            sqbase = base;
                            arg = base->args[0];
                            return true;
                        }
                        return false;
                    };
                    if (t->isMul()) {
                        for (auto& f : t->args) if (!visit(f)) return Rational(0);
                    } else if (!visit(t)) {
                        return Rational(0);
                    }
                    return sqbase ? coeff : Rational(0);
                };
                Ex a1;
                Rational c1 = sinSq(s->args[i], a1);
                if (c1.isZero()) continue;
                for (size_t j = i + 1; j < s->args.size(); ++j) {
                    Ex a2;
                    Rational c2 = sinSq(s->args[j], a2);
                    if (c2.isZero() || !equals(a1, a2)) continue;
                    bool one = s->args[i]->isPow() && s->args[j]->isPow() &&
                               s->args[i]->args[0]->name != s->args[j]->args[0]->name;
                    bool two = !one && c1 == c2;
                    if (one || two) {
                        Ex unit = one ? Num(1) : Num(c1);
                        Ex newTerm = Mul(unit, Add(Mul(unit, Num(0)), Num(1)));
                        // replace both terms by unit and 0
                        std::vector<Ex> nv;
                        for (size_t k = 0; k < s->args.size(); ++k) {
                            if (k == i) nv.push_back(unit);
                            else if (k == j) continue;
                            else nv.push_back(s->args[k]);
                        }
                        s = Add(nv);
                        break;
                    }
                }
                if (!s->isAdd()) break;
            }
            Ex t = together(s);
            if (t.get() != s.get()) t = cancelCommon(t);
            if (sizeOf(t) < sizeOf(s)) s = t;
            Ex g = factorOutCommon(s);
            if (sizeOf(g) <= sizeOf(s)) s = g;
        }
        return s;
    }
    if (a->isMul()) {
        std::vector<Ex> v;
        for (auto& f : a->args) v.push_back(simplify(f));
        return cancelCommon(Mul(v));
    }
    if (a->isPow()) {
        Ex b = simplify(a->args[0]), e = simplify(a->args[1]);
        // |u|^(2k) = u^(2k) over the reals.
        if (b->isFunc("abs") && b->args.size() == 1 && e->isNum() && e->num.isInt() &&
            e->num.n > 0 && e->num.n % 2 == 0)
            return Pow(b->args[0], e);
        // (e^u)^v = e^(u·v)
        if (b->isFunc("exp") && b->args.size() == 1)
            return Fn("exp", {simplify(Mul(b->args[0], e))});
        Ex p = Pow(b, e);
        if (p->isMul() || p->isPow()) return p;
        return p;
    }
    if (a->isFunc()) {
        std::vector<Ex> v;
        for (auto& f : a->args) v.push_back(simplify(f));
        Ex f = Fn(a->name, v);
        return f;
    }
    return a;
}

Ex factorOut(const Ex& a, const Ex& g) {
    if (equals(a, g)) return Num(1);
    if (a->isMul()) {
        std::vector<Ex> v;
        bool removed = false;
        for (auto& f : a->args) {
            if (!removed && equals(f, g)) { removed = true; continue; }
            v.push_back(f);
        }
        return removed ? Mul(v) : a;
    }
    return a;
}

// ---------------------------------------------------------------- polynomial coefficient extraction
bool polyCoeffs(const Ex& a, const std::string& v, std::vector<Ex>& out) {
    std::function<bool(const Ex&, std::vector<Ex>&)> rec = [&](const Ex& e, std::vector<Ex>& c) -> bool {
        if (!dependsOn(e, v)) { c = {e}; return true; }
        if (e->isSym(v)) { c = {Num(0), Num(1)}; return true; }
        if (e->isAdd()) {
            c = {Num(0)};
            for (auto& t : e->args) {
                std::vector<Ex> tc;
                if (!rec(t, tc)) return false;
                if (tc.size() > c.size()) c.resize(tc.size(), Num(0));
                for (size_t i = 0; i < tc.size(); ++i) c[i] = Add(c[i], tc[i]);
            }
            return true;
        }
        if (e->isMul()) {
            c = {Num(1)};
            for (auto& f : e->args) {
                std::vector<Ex> fc;
                if (!rec(f, fc)) return false;
                std::vector<Ex> nc(c.size() + fc.size() - 1, Num(0));
                for (size_t i = 0; i < c.size(); ++i)
                    for (size_t j = 0; j < fc.size(); ++j)
                        nc[i + j] = Add(nc[i + j], Mul(c[i], fc[j]));
                c.swap(nc);
                if (c.size() > 80) return false;
            }
            return true;
        }
        if (e->isPow() && e->args[1]->isNum() && e->args[1]->num.isInt() && e->args[1]->num.n >= 0 &&
            e->args[1]->num.n <= 24 && dependsOn(e->args[0], v)) {
            std::vector<Ex> base;
            if (!rec(e->args[0], base)) return false;
            std::vector<Ex> r{Num(1)};
            for (long long k = 0; k < e->args[1]->num.n; ++k) {
                std::vector<Ex> nc(r.size() + base.size() - 1, Num(0));
                for (size_t i = 0; i < r.size(); ++i)
                    for (size_t j = 0; j < base.size(); ++j)
                        nc[i + j] = Add(nc[i + j], Mul(r[i], base[j]));
                r.swap(nc);
                if (r.size() > 80) return false;
            }
            c = r;
            return true;
        }
        return false;
    };
    out.clear();
    if (!rec(a, out)) return false;
    while (out.size() > 1 && out.back()->isZero()) out.pop_back();
    return true;
}

// ---------------------------------------------------------------- linear solve
Ex solveLinear(const Ex& eq, const std::string& v) {
    Ex eqs = expand(eq);
    if (!dependsOn(eqs, v)) return nullptr;
    Ex c1 = simplify(diff(eqs, v));
    if (dependsOn(c1, v)) return nullptr;
    Ex c0 = simplify(expand(substitute(eqs, v, Num(0))));
    if (dependsOn(c0, v)) return nullptr;
    Ex sol = simplify(expand(Mul({Num(-1), c0, Pow(c1, Num(-1))})));
    // Verify c1·y + c0 = 0 structurally. Substituting back into the original
    // expanded equation can split fractions and hide a cancellation.
    Ex chk = simplify(Add(Mul(c1, sol), c0));
    if (!chk->isZero()) return nullptr;
    return sol;
}

// ---------------------------------------------------------------- roots
static std::vector<long long> divisors(long long n) {
    std::vector<long long> d;
    if (n < 0) n = -n;
    if (n == 0) return {0};
    for (long long i = 1; i * i <= n && i <= 1000000; ++i) {
        if (n % i == 0) {
            d.push_back(i);
            if (i * i != n) d.push_back(n / i);
        }
    }
    std::sort(d.begin(), d.end());
    return d;
}

static Ex sqrtExact(Rational q) {
    if (q.n < 0) return nullptr;
    long long rn = (long long)std::llround(std::sqrt((double)q.n));
    for (long long k = std::max(0LL, rn - 2); k <= rn + 2; ++k)
        if (k * k == q.n) rn = k;
    if (rn * rn != q.n) return nullptr;
    long long rd = (long long)std::llround(std::sqrt((double)q.d));
    for (long long k = std::max(1LL, rd - 2); k <= rd + 2; ++k)
        if (k * k == q.d) rd = k;
    if (rd * rd != q.d) return nullptr;
    return Num(Rational(rn, rd));
}

// solve quadratic a x^2 + b x + c = 0 (a,b,c numeric)
static std::vector<Ex> solveQuadratic(Rational a, Rational b, Rational c) {
    std::vector<Ex> roots;
    Rational disc = b * b - a * c * Rational(4);
    if (disc.isNeg()) return roots; // no real roots
    Ex s = Sqrt(Num(disc));
    Ex common = Mul(Add(Num(-b), s), Num(Rational(1, 2)), Pow(Num(a), Num(-1)));
    roots.push_back(simplify(common));
    if (!disc.isZero()) {
        Ex second = Mul(Add(Num(-b), Mul(Num(-1), s)), Num(Rational(1, 2)), Pow(Num(a), Num(-1)));
        roots.push_back(simplify(second));
    }
    return roots;
}

std::vector<Ex> solvePoly(const Ex& eq, const std::string& v, int maxDeg) {
    std::vector<Ex> out;
    Ex a = expand(eq);
    std::vector<Ex> c;
    if (!polyCoeffs(a, v, c)) return out;
    int deg = (int)c.size() - 1;
    if (deg < 1 || deg > maxDeg) return out;
    for (auto& x : c) if (!x->isNum()) return out;
    // clear denominators
    long long L = 1;
    for (auto& x : c) L = std::lcm(L, x->num.d);
    std::vector<long long> ic;
    for (auto& x : c) ic.push_back(x->num.n * (L / x->num.d));
    // rational roots, deflate
    std::function<void(std::vector<long long>, std::vector<Ex>&)> rec = [&](std::vector<long long> p, std::vector<Ex>& res) {
        while (p.size() > 1 && p.back() == 0) p.pop_back();
        if (p.size() == 1) return;
        if (p.size() == 2) {
            res.push_back(Num(Rational(-p[0], p[1])));
            return;
        }
        if (p.size() == 3) {
            for (auto& r : solveQuadratic(Rational(p[2]), Rational(p[1]), Rational(p[0]))) res.push_back(r);
            return;
        }
        bool found = false;
        long long a0 = p[0], an = p.back();
        if (a0 == 0) {
            res.push_back(Num(0));
            p.erase(p.begin());
            rec(p, res);
            return;
        }
        for (long long num : divisors(a0)) {
            if (num == 0) continue;
            for (long long den : divisors(an)) {
                for (int sgn = -1; sgn <= 1; sgn += 2) {
                    Rational r(sgn * num, den);
                    // evaluate
                    Rational val(0);
                    for (auto it = p.rbegin(); it != p.rend(); ++it) val = val * r + Rational(*it);
                    if (!val.isZero()) continue;
                    res.push_back(Num(r));
                    // synthetic division
                    std::vector<long long> q(p.size() - 1, 0);
                    q[q.size() - 1] = p.back();
                    for (int i = (int)p.size() - 2; i >= 1; --i)
                        q[i - 1] = p[i] + q[i] * r.n / r.d;
                    rec(q, res);
                    found = true;
                    return;
                }
            }
        }
        if (!found) {
            // leave unfactored: no closed-form roots found
        }
    };
    std::vector<long long> p = ic;
    while (p.size() > 1 && p.back() == 0) p.pop_back();
    std::vector<Ex> res;
    // strip zero roots
    while (p.size() > 1 && p[0] == 0) { res.push_back(Num(0)); p.erase(p.begin()); }
    rec(p, res);
    return res;
}

Ex limitAt(const Ex& a, const std::string& var, const Ex& point) {
    Ex s = substitute(a, var, point);
    try {
        Ex simp = simplify(s);
        if (!dependsOn(simp, var)) return simp;
    } catch (...) {}
    return s;
}

} // namespace ode
