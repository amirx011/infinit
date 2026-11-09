// Infinit Lab — single/multiple variable calculus API
// (integrals: indefinite, definite, double and triple; derivatives: orders 1–10
//  and implicit differentiation)
#include "numeric.hpp"
// SPDX-License-Identifier: Apache-2.0
#include "calculus_ext.hpp"
#include "limits.hpp"
#include "localize.hpp"
#include "poles.hpp"
#include "solver.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace ode {
namespace {



std::string fmtReal(double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.12g", v);
    return buf;
}


std::string boundText(const Ex& e) { return e ? toPlain(e) : std::string("?"); }

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

void addStep(Result& r, const std::string& titleKey, const Ex& math = nullptr,
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

// Numeric value of the integrand at a point (used by the verification pass).
double evalAt(const Ex& f, const std::map<std::string, double>& env) {
    bool ok = false;
    const double v = evalReal(f, env, &ok);
    return ok ? v : std::nan("");
}


} // namespace

Result integrateExpression(const std::string& input, const std::string& x,
                           const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "integral";
    r.typeId = "integral";
    r.engine = "native";
    r.order = 0;
    r.vars = {x};
    r.dimension = 1;
    try {
        Ex f = parseExpr(input);
        r.normalized = f;
        r.variable = x;
        bool closed = false;
        std::string method;
        Ex F = integrate(f, x, &closed, &method);
        r.method = method;
        r.solution = F;
        r.isExplicit = true;
        r.explicitY = F;
        r.closedForm = closed;
        addStep(r, "step.title.method_detection", nullptr,
                "step.note.detected_method",
                "{\"method\":\"@ui.method." + method + "\"}");
        addStep(r, "step.title.indefinite_integral", F,
                "step.note.constant_of_integration");
        if (!closed) {
            r.warnings.push_back("step.note.no_elementary_antiderivative");
        } else {
            try {
                Ex residual = simplify(Add(diff(F, x), Mul(Num(-1), f)));
                bool ok = residual->isZero() || numericallyZero(residual, {x}, 8, 1e-6);
                addCheck(r, "check.differentiate_result", ok,
                         ok ? "check.derivative_matches" : "check.derivative_mismatch");
            } catch (...) {
                addCheck(r, "check.differentiate_result", false,
                         "check.verify_unavailable");
            }
        }
        r.ok = true;
    } catch (const std::exception& e) {
        setCodedFailure(r, e, "integral");
    }
    return r;
}

namespace {

// ---------------------------------------------------- definite integrals
//
// A definite integral is only equal to F(b)-F(a) when the integrand is defined
// and continuous on the whole interval.  The routines below collect the poles
// of the integrand, split the interval at them, and evaluate one-sided limits of
// the antiderivative.  Convergent improper integrals are reported as such;
// divergent ones are reported as divergent instead of returning a finite wrong
// number.

struct EdgeValue {
    bool divergent = false;
    bool exact = false;
    Ex value;
    bool numeric = false;
    double number = 0;
};

double evalPoint(const Ex& f, const std::string& x, double at, bool* ok) {
    bool good = false;
    double v = std::nan("");
    try {
        v = evalReal(f, {{x, at}}, &good);
    } catch (...) {
        good = false;
    }
    if (ok) *ok = good && std::isfinite(v);
    return v;
}

bool linearCoeffs(const Ex& e, const std::string& x, Ex* c1, Ex* c0) {
    std::vector<Ex> c;
    if (!polyCoeffs(expand(e), x, c)) return false;
    if (c.size() > 2) return false;
    *c0 = c.empty() ? Num(0) : c[0];
    *c1 = c.size() > 1 ? c[1] : Num(0);
    bool ok0 = false, ok1 = false;
    evalReal(*c0, {}, &ok0);
    evalReal(*c1, {}, &ok1);
    return ok0 && ok1 && !(*c1)->isZero();
}

// sin/cos of a linear argument: build the exact zeros ((2k+1)pi/2 for cos,
// k*pi for sin) and keep the ones inside the interval.
void addTrigRoots(const Ex& inner, const std::string& x, bool sine, double lo, double hi,
                  std::vector<PolePoint>& out) {
    Ex c1, c0;
    if (!linearCoeffs(inner, x, &c1, &c0)) return;
    const Ex inv = Pow(c1, Num(-1));
    for (int k = -512; k <= 512; ++k) {
        Ex root;
        if (sine) {
            root = simplify(Mul(Add(Mul(Num(k), Sym("pi")), Mul(Num(-1), c0)), inv));
        } else {
            root = simplify(Mul(
                Add(Mul(Num(2 * k + 1), Sym("pi"), Num(Rational(1, 2))), Mul(Num(-1), c0)), inv));
        }
        bool ok = false;
        const double at = evalReal(root, {}, &ok);
        if (!ok || !std::isfinite(at)) continue;
        if (at < lo - 1e-9 || at > hi + 1e-9) continue;
        out.push_back({root, at, true});
    }
}

void collectPoleBases(const Ex& e, const std::string& x, std::vector<Ex>& out) {
    if (!e) return;
    if (e->isPow() && e->args.size() == 2) {
        const Ex& p = e->args[1];
        if (p->isNum() && p->num.isNeg() && dependsOn(e->args[0], x))
            out.push_back(e->args[0]);
    }
    if (e->isFunc() && !e->args.empty()) {
        const std::string& n = e->name;
        if (n == "tan" || n == "sec") out.push_back(Fn("cos", {e->args[0]}));
        else if (n == "cot" || n == "csc") out.push_back(Fn("sin", {e->args[0]}));
        else if (n == "ln" || n == "log") out.push_back(e->args[0]);
    }
    for (const auto& a : e->args) collectPoleBases(a, x, out);
}

void addSymbolicPoles(const Ex& f, const std::string& x, double lo, double hi,
                      std::vector<PolePoint>& out) {
    std::vector<Ex> bases;
    collectPoleBases(f, x, bases);
    for (const auto& base : bases) {
        if (!dependsOn(base, x)) continue;
        if (base->isFunc() && base->args.size() == 1 &&
            (base->name == "cos" || base->name == "sin")) {
            addTrigRoots(base->args[0], x, base->name == "sin", lo, hi, out);
            continue;
        }
        std::vector<Ex> roots;
        try {
            roots = solvePoly(expand(base), x, 8);
        } catch (...) {
            continue;
        }
        for (const auto& root : roots) {
            bool ok = false;
            const double at = evalReal(root, {}, &ok);
            if (!ok || !std::isfinite(at)) continue;
            if (at < lo - 1e-9 || at > hi + 1e-9) continue;
            out.push_back({simplify(root), at, true});
        }
    }
}

// Does |f| grow without bound as we approach `at` from one side?  This is what
// distinguishes a pole from a merely large but smooth value (e.g. exp(10x)).
bool growsWithoutBound(const Ex& f, const std::string& x, double at, int dir) {
    double prev = 0;
    int count = 0, growth = 0;
    for (double d : {1e-2, 1e-4, 1e-6, 1e-8}) {
        bool ok = false;
        const double v = evalPoint(f, x, at + dir * d, &ok);
        // A failed evaluation here usually means the point is a domain
        // boundary, not a pole.  Treating it as growth produced phantom
        // asymptotes along the edge of a domain such as ln(x) for x < 0.
        if (!ok) return false;
        const double a = std::fabs(v);
        if (count > 0 && a > 5.0 * std::max(prev, 1.0)) ++growth;
        prev = a;
        ++count;
    }
    return count >= 2 && growth >= 2 && prev > 1e3;
}

void addNumericPoles(const Ex& f, const std::string& x, double lo, double hi,
                     std::vector<PolePoint>& out) {
    const int N = 1024;
    if (!(hi > lo)) return;
    std::vector<double> vals(N + 1, std::nan(""));
    std::vector<double> mags;
    mags.reserve(N + 1);
    for (int i = 0; i <= N; ++i) {
        const double t = lo + (hi - lo) * (double)i / N;
        bool ok = false;
        vals[i] = evalPoint(f, x, t, &ok);
        if (ok) mags.push_back(std::fabs(vals[i]));
        else vals[i] = std::nan("");
    }
    if (mags.empty()) return;
    std::sort(mags.begin(), mags.end());
    const double median = mags[mags.size() / 2];
    const double threshold = std::max(100.0, 50.0 * median);

    size_t added = 0;
    for (int i = 1; i < N && added < 8; ++i) {
        if (std::isfinite(vals[i]) && std::fabs(vals[i]) < threshold) continue;
        const double left = std::isfinite(vals[i - 1]) ? std::fabs(vals[i - 1]) : 1e300;
        const double right = std::isfinite(vals[i + 1]) ? std::fabs(vals[i + 1]) : 1e300;
        const double here = std::isfinite(vals[i]) ? std::fabs(vals[i]) : 1e300;
        if (here < left || here < right) continue;

        double a = lo + (hi - lo) * (double)(i - 1) / N;
        double b = lo + (hi - lo) * (double)(i + 1) / N;
        for (int it = 0; it < 70; ++it) {
            const double m1 = a + (b - a) / 3.0;
            const double m2 = b - (b - a) / 3.0;
            bool ok1 = false, ok2 = false;
            const double v1 = evalPoint(f, x, m1, &ok1);
            const double v2 = evalPoint(f, x, m2, &ok2);
            const double a1 = ok1 ? std::fabs(v1) : 1e300;
            const double a2 = ok2 ? std::fabs(v2) : 1e300;
            if (a1 < a2) a = m1;
            else b = m2;
        }
        const double at = 0.5 * (a + b);
        if (!growsWithoutBound(f, x, at, -1) && !growsWithoutBound(f, x, at, +1)) continue;
        out.push_back({nullptr, at, false});
        ++added;
    }
}

void dedupePoles(std::vector<PolePoint>& poles) {
    std::sort(poles.begin(), poles.end(),
              [](const PolePoint& a, const PolePoint& b) { return a.numericAt < b.numericAt; });
    std::vector<PolePoint> out;
    for (const auto& p : poles) {
        if (!out.empty() && std::fabs(p.numericAt - out.back().numericAt) < 1e-7) {
            if (p.exact && !out.back().exact) out.back() = p;
            continue;
        }
        out.push_back(p);
    }
    poles.swap(out);
}

std::vector<PolePoint> findPolesImpl(const Ex& f, const std::string& x, double lo, double hi,
                                     bool scanNumeric) {
    std::vector<PolePoint> poles;
    addSymbolicPoles(f, x, lo, hi, poles);
    if (scanNumeric) addNumericPoles(f, x, lo, hi, poles);
    dedupePoles(poles);
    return poles;
}

// A removable singularity (for example sin(x)/x at 0) has a finite limit and
// does not make the integral improper.
bool hasFiniteLimitImpl(const Ex& f, const std::string& x, const Ex& at, int dir) {
    try {
        LimitOutcome l = limitOf(f, x, at, dir, nullptr);
        if (l.kind != LimitOutcome::Finite) return false;
        if (l.value) {
            bool ok = false;
            const double d = evalReal(l.value, {}, &ok);
            return ok && std::isfinite(d);
        }
        return l.numeric && std::isfinite(l.number);
    } catch (...) {
        return false;
    }
}

double valueOf(const EdgeValue& v) {
    if (v.exact) {
        bool ok = false;
        const double d = evalReal(v.value, {}, &ok);
        if (ok && std::isfinite(d)) return d;
    }
    return v.number;
}

// Limit of the antiderivative at a finite point.  Direct substitution is used
// when the point is not a pole; otherwise both the symbolic one-sided limit and
// a numeric convergence probe are tried.
EdgeValue evalFiniteBoundary(const Ex& F, const std::string& x, const Ex& at, double atNum,
                             int dir, bool isPole) {
    EdgeValue out;
    if (!isPole) {
        try {
            Ex v = simplify(substitute(F, x, at));
            bool ok = false;
            const double d = evalReal(v, {}, &ok);
            if (ok && std::isfinite(d)) {
                out.exact = true;
                out.value = v;
                return out;
            }
        } catch (...) {}
    }
    try {
        LimitOutcome l = limitOf(F, x, at, dir, nullptr);
        if (l.kind == LimitOutcome::PosInf || l.kind == LimitOutcome::NegInf) {
            out.divergent = true;
            return out;
        }
        if (l.kind == LimitOutcome::Finite) {
            if (l.value) {
                bool ok = false;
                const double d = evalReal(l.value, {}, &ok);
                if (ok && std::isfinite(d)) {
                    out.exact = true;
                    out.value = l.value;
                    return out;
                }
            } else if (l.numeric) {
                out.numeric = true;
                out.number = l.number;
                return out;
            }
        }
    } catch (...) {}

    // Numeric fallback: a convergent limit has shrinking increments, while a
    // logarithmic or algebraic divergence keeps producing comparable jumps.
    double vals[6];
    int n = 0;
    for (double d : {1e-2, 1e-4, 1e-6, 1e-8, 1e-10, 1e-12}) {
        bool ok = false;
        const double v = evalPoint(F, x, atNum + dir * d, &ok);
        if (ok) vals[n++] = v;
    }
    if (n == 0) {
        out.divergent = true;
        return out;
    }
    if (std::fabs(vals[n - 1]) > 1e8) {
        out.divergent = true;
        return out;
    }
    int nonShrink = 0;
    double prevDiff = 1e18;
    for (int i = 1; i < n; ++i) {
        const double diff = std::fabs(vals[i] - vals[i - 1]);
        if (diff > prevDiff * 0.5) ++nonShrink;
        prevDiff = diff;
    }
    if (nonShrink >= 2) {
        out.divergent = true;
        return out;
    }
    out.numeric = true;
    out.number = vals[n - 1];
    return out;
}

EdgeValue evalInfiniteBoundary(const Ex& F, const std::string& x, int sign) {
    EdgeValue out;
    try {
        LimitOutcome l = limitOf(F, x, infinityPoint(sign), sign, nullptr);
        if (l.kind == LimitOutcome::PosInf || l.kind == LimitOutcome::NegInf) {
            out.divergent = true;
            return out;
        }
        if (l.kind == LimitOutcome::Finite) {
            if (l.value) {
                bool ok = false;
                const double d = evalReal(l.value, {}, &ok);
                if (ok && std::isfinite(d)) {
                    out.exact = true;
                    out.value = l.value;
                    return out;
                }
            } else if (l.numeric) {
                out.numeric = true;
                out.number = l.number;
                return out;
            }
        }
    } catch (...) {}

    double vals[6];
    int n = 0;
    for (double R : {1e1, 1e2, 1e3, 1e4, 1e5, 1e6}) {
        bool ok = false;
        const double v = evalPoint(F, x, sign * R, &ok);
        if (ok) vals[n++] = v;
    }
    if (n == 0) {
        out.divergent = true;
        return out;
    }
    int nonShrink = 0;
    double prevDiff = 1e18;
    for (int i = 1; i < n; ++i) {
        const double diff = std::fabs(vals[i] - vals[i - 1]);
        if (diff > prevDiff * 0.5) ++nonShrink;
        prevDiff = diff;
    }
    if (nonShrink >= 2 && std::fabs(vals[n - 1]) > std::fabs(vals[0])) {
        out.divergent = true;
        return out;
    }
    out.numeric = true;
    out.number = vals[n - 1];
    return out;
}

// Quadrature over an infinite tail.  The integral is sampled on growing
// truncations; a non-decaying sequence of increments means divergence.
double numericToInfinity(const Ex& f, const std::string& x, double a, int sign) {
    double prev = std::nan("");
    double prevDiff = 1e18;
    int nonShrink = 0;
    double last = std::nan("");
    for (double R : {10.0, 100.0, 1000.0, 1e4, 1e5}) {
        const double lo = sign > 0 ? a : a - R;
        const double hi = sign > 0 ? a + R : a;
        const double v = quadratureAdaptive([&](double t) { return evalAt(f, {{x, t}}); }, lo, hi,
                                            1e-10);
        if (!std::isfinite(v)) return std::nan("");
        if (std::isfinite(prev)) {
            const double diff = std::fabs(v - prev);
            if (diff > prevDiff * 0.5 && diff > 1e-7 * std::max(1.0, std::fabs(v))) ++nonShrink;
            prevDiff = diff;
        }
        prev = v;
        last = v;
    }
    if (nonShrink >= 2) return std::nan("");
    return last;
}

struct Edge {
    bool infinite = false;
    int infSign = 1;
    bool pole = false;
    Ex at;
    double num = 0;
};

Result integrateDefiniteSingle(const std::string& input, const std::string& x,
                               const std::string& loText, const std::string& hiText,
                               const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "integral";
    r.typeId = "integral_definite";
    r.engine = "native";
    r.variable = x;
    r.dimension = 1;
    r.definite = true;
    r.vars = {x};
    r.lower = {loText};
    r.upper = {hiText};
    try {
        Ex f = parseExpr(input);
        Ex lo = parseExpr(loText);
        Ex hi = parseExpr(hiText);
        r.normalized = f;
        addStep(r, "step.title.integrand", f, "step.note.integrate_with_respect_to",
                "{\"var\":\"" + x + "\"}");
        addStep(r, "step.title.bounds", nullptr, "step.note.from_to",
                "{\"lo\":\"" + boundText(lo) + "\",\"hi\":\"" +
                    boundText(hi) + "\"}");

        int loSign = 1, hiSign = 1;
        const bool loInf = isInfinityPoint(lo, &loSign);
        const bool hiInf = isInfinityPoint(hi, &hiSign);
        bool okLo = false, okHi = false;
        const double loNum = evalReal(lo, {}, &okLo);
        const double hiNum = evalReal(hi, {}, &okHi);
        const bool finiteBounds = !loInf && !hiInf && okLo && okHi;

        bool closed = false;
        std::string method;
        Ex F = integrate(f, x, &closed, &method);
        bool exact = closed && !hasUnevaluatedIntegral(F);

        // Look for poles: exact roots of denominators and, on a bounded
        // interval, numerical candidates refined by a divergence test.
        std::vector<PolePoint> poles;
        if (F) {
            const double scanLo = loInf ? -50.0 : loNum;
            const double scanHi = hiInf ? 50.0 : hiNum;
            poles = findPolesImpl(f, x, scanLo, scanHi, finiteBounds);
        }

        std::vector<PolePoint> interior;
        bool loPole = false, hiPole = false;
        for (const auto& p : poles) {
            const bool atLo = !loInf && std::fabs(p.numericAt - loNum) < 1e-7;
            const bool atHi = !hiInf && std::fabs(p.numericAt - hiNum) < 1e-7;
            if (atLo) { loPole = true; continue; }
            if (atHi) { hiPole = true; continue; }
            if ((loInf || p.numericAt > loNum) && (hiInf || p.numericAt < hiNum))
                interior.push_back(p);
        }

        // Drop removable singularities from the singular set.
        {
            std::vector<PolePoint> kept;
            for (const auto& p : poles) {
                int dir = 0;
                if (!loInf && std::fabs(p.numericAt - loNum) < 1e-7) dir = 1;
                else if (!hiInf && std::fabs(p.numericAt - hiNum) < 1e-7) dir = -1;
                if (p.exact && p.at && hasFiniteLimitImpl(f, x, p.at, dir)) continue;
                kept.push_back(p);
            }
            if (kept.size() != poles.size()) {
                poles.swap(kept);
                interior.clear();
                loPole = false;
                hiPole = false;
                for (const auto& p : poles) {
                    const bool atLo = !loInf && std::fabs(p.numericAt - loNum) < 1e-7;
                    const bool atHi = !hiInf && std::fabs(p.numericAt - hiNum) < 1e-7;
                    if (atLo) { loPole = true; continue; }
                    if (atHi) { hiPole = true; continue; }
                    if ((loInf || p.numericAt > loNum) && (hiInf || p.numericAt < hiNum))
                        interior.push_back(p);
                }
            }
        }

        bool integrandSingularLo = false, integrandSingularHi = false;
        bool improper = loInf || hiInf || loPole || hiPole || !interior.empty();
        // An endpoint where the antiderivative itself fails (e.g. ln x at 0)
        // is an improper integral even without a rational pole.
        if (exact && !improper && finiteBounds) {
            bool ok1 = false, ok2 = false;
            const double v1 = evalReal(simplify(substitute(F, x, lo)), {}, &ok1);
            const double v2 = evalReal(simplify(substitute(F, x, hi)), {}, &ok2);
            if ((!ok1 || !std::isfinite(v1)) || (!ok2 || !std::isfinite(v2))) improper = true;
        }
        // Likewise an integrand that is unbounded at an endpoint (logarithmic
        // or fractional-power singularity) makes the integral improper.
        if (exact && !improper && finiteBounds) {
            bool ok1 = false, ok2 = false;
            evalPoint(f, x, loNum, &ok1);
            evalPoint(f, x, hiNum, &ok2);
            if (!ok1 && !hasFiniteLimitImpl(f, x, lo, +1)) {
                improper = true;
                integrandSingularLo = true;
            }
            if (!ok2 && !hasFiniteLimitImpl(f, x, hi, -1)) {
                improper = true;
                integrandSingularHi = true;
            }
        }

        auto noteSingularity = [&](const std::string& point) {
            if (std::find(r.singularities.begin(), r.singularities.end(), point) ==
                r.singularities.end())
                r.singularities.push_back(point);
        };
        for (const auto& p : poles)
            noteSingularity(p.exact && p.at ? toPlain(p.at) : fmtReal(p.numericAt));
        if (integrandSingularLo) noteSingularity(toPlain(lo));
        if (integrandSingularHi) noteSingularity(toPlain(hi));

        if (exact && improper) {
            // Split the interval at every singularity and add one-sided limits.
            std::vector<Edge> edges;
            edges.push_back({loInf, loSign, loPole, lo, loNum});
            for (const auto& p : interior)
                edges.push_back({false, 1, true, p.at, p.numericAt});
            edges.push_back({hiInf, hiSign, hiPole, hi, hiNum});

            bool divergent = false;
            bool allExact = true;
            bool haveApprox = false;
            Ex exactTotal = Num(0);
            double approxTotal = 0.0;
            for (size_t i = 0; i + 1 < edges.size() && !divergent; ++i) {
                const Edge& left = edges[i];
                const Edge& right = edges[i + 1];
                EdgeValue a = left.infinite
                                  ? evalInfiniteBoundary(F, x, left.infSign)
                                  : evalFiniteBoundary(F, x, left.at, left.num, +1, left.pole);
                EdgeValue b = right.infinite
                                  ? evalInfiniteBoundary(F, x, right.infSign)
                                  : evalFiniteBoundary(F, x, right.at, right.num, -1, right.pole);
                if (a.divergent || b.divergent) {
                    divergent = true;
                    break;
                }
                if (a.exact && b.exact) {
                    exactTotal =
                        Add(exactTotal, Add(b.value, Mul(Num(-1), a.value)));
                } else {
                    allExact = false;
                }
                approxTotal += valueOf(b) - valueOf(a);
                if (a.numeric || b.numeric) haveApprox = true;
            }

            r.improper = true;
            r.method = "definite";
            if (divergent) {
                r.divergent = true;
                r.closedForm = false;
                r.hasNumeric = false;
                r.solution = nullptr;
                r.explicitY = nullptr;
                r.isExplicit = false;
                addStep(r, "step.title.singularity_analysis", nullptr,
                        "step.note.singular_diverges");
                r.warnings.push_back("step.note.definite_diverges");
                addCheck(r, "check.convergence_analysis", true,
                         "check.divergence_one_sided");
            } else {
                r.closedForm = allExact;
                if (allExact) {
                    Ex val = simplify(exactTotal);
                    bool ok = false;
                    const double value = evalReal(val, {}, &ok);
                    if (ok && std::isfinite(value)) {
                        r.solution = val;
                        r.explicitY = val;
                        r.isExplicit = true;
                        r.hasNumeric = true;
                        r.numericValue = value;
                    } else {
                        r.hasNumeric = std::isfinite(approxTotal);
                        r.numericValue = approxTotal;
                        r.closedForm = false;
                    }
                } else {
                    r.solution = nullptr;
                    r.explicitY = nullptr;
                    r.isExplicit = false;
                    r.hasNumeric = std::isfinite(approxTotal);
                    r.numericValue = approxTotal;
                    r.verified = false;
                }
                if (!interior.empty() || loPole || hiPole) {
                    std::string list;
                    for (const auto& p : poles) {
                        if (!list.empty()) list += ", ";
                        list += p.exact && p.at ? toPlain(p.at) : fmtReal(p.numericAt);
                    }
                    addStep(r, "step.title.improper_integral", nullptr,
                            "step.note.singular_points_split",
                            "{\"points\":\"" + list + "\"}");
                }
                if (loInf || hiInf) {
                    addStep(r, "step.title.infinite_bounds", nullptr,
                            "step.note.limit_at_infinity");
                }
                addCheck(r, "check.convergence_analysis", true,
                         "check.converges_one_sided");
                if (!allExact) {
                    r.warnings.push_back("step.note.improper_numeric_finite");
                }
            }
            r.ok = true;
            return r;
        }

        double numeric = std::nan("");
        if (finiteBounds) {
            numeric = quadratureAdaptive([&](double t) { return evalAt(f, {{x, t}}); }, loNum,
                                         hiNum, 1e-10);
        }

        if (exact) {
            Ex val = simplify(Add(substitute(F, x, hi), Mul(Num(-1), substitute(F, x, lo))));
            bool ok = false;
            const double value = evalReal(val, {}, &ok);
            if (ok && std::isfinite(value)) {
                r.solution = val;
                r.explicitY = val;
                r.isExplicit = true;
                r.method = "definite";
                r.closedForm = true;
                r.hasNumeric = true;
                r.numericValue = value;
                addStep(r, "step.title.antiderivative", F, "step.note.method",
                        "{\"method\":\"@ui.method." + method + "\"}");
                addStep(r, "step.title.apply_the_bounds", val, "", "",
                        "F(" + boundText(hi) + ") - F(" + boundText(lo) + ")");
                if (std::isfinite(numeric)) {
                    const double scale = std::max(1.0, std::fabs(numeric));
                    const bool match = std::fabs(numeric - value) <= 1e-6 * scale;
                    addCheck(r, "check.quadrature_independent", match,
                             "check.quadrature_value",
                             "{\"value\":" + fmtReal(numeric) + "}");
                }
                r.ok = true;
                return r;
            }
            exact = false;
        }

        if (improper) {
            // No elementary antiderivative: integrate numerically, but only
            // after the singular structure has been taken into account.
            bool divergent = false;
            double value = std::nan("");
            if (interior.empty() && !loPole && !hiPole) {
                if (loInf && hiInf) {
                    const double left = numericToInfinity(f, x, 0.0, -1);
                    const double right = numericToInfinity(f, x, 0.0, +1);
                    if (!std::isfinite(left) || !std::isfinite(right)) divergent = true;
                    else value = left + right;
                } else if (loInf) {
                    value = numericToInfinity(f, x, hiNum, -1);
                    if (!std::isfinite(value)) divergent = true;
                } else if (hiInf) {
                    value = numericToInfinity(f, x, loNum, +1);
                    if (!std::isfinite(value)) divergent = true;
                }
            }
            r.improper = true;
            r.method = "numeric";
            r.closedForm = false;
            if (divergent) {
                r.divergent = true;
                r.hasNumeric = false;
                r.solution = nullptr;
                r.warnings.push_back("step.note.definite_diverges");
            } else if (std::isfinite(value)) {
                r.hasNumeric = true;
                r.numericValue = value;
                r.solution = nullptr;
                r.explicitY = nullptr;
                r.isExplicit = false;
                r.verified = false;
                r.warnings.push_back("step.note.improper_numeric");
            } else {
                throw EngineError("unavailable", "the integrand is singular");
            }
            addStep(r, "step.title.numerical_improper_integral", nullptr,
                    "step.note.singular_points_excluded");
            r.ok = true;
            return r;
        }

        if (!std::isfinite(numeric))
            throw EngineError("unavailable", "quadrature failed");
        r.method = "numeric";
        r.closedForm = false;
        r.hasNumeric = true;
        r.numericValue = numeric;
        r.solution = nullptr;
        r.explicitY = nullptr;
        r.isExplicit = false;
        addStep(r, "step.title.quadrature", nullptr, "step.note.quadrature_value",
                "{\"value\":\"" + fmtReal(numeric) + "\"}");
        r.warnings.push_back("step.note.answer_numeric");
        r.ok = true;
    } catch (const std::exception& e) {
        setCodedFailure(r, e, "definite_integral");
    }
    return r;
}

} // namespace

std::vector<PolePoint> findPoles(const Ex& f, const std::string& x, double lo, double hi,
                                 bool scanNumeric) {
    return findPolesImpl(f, x, lo, hi, scanNumeric);
}

bool hasFiniteLimitAt(const Ex& f, const std::string& x, const Ex& at, int dir) {
    return hasFiniteLimitImpl(f, x, at, dir);
}

Result integrateMulti(const std::string& input, const std::vector<std::string>& vars,
                      bool definite, const std::vector<std::string>& lower,
                      const std::vector<std::string>& upper, const std::string& lang) {
    const bool en = lang != "fa";
    if (vars.empty()) return integrateExpression(input, "x", lang);

    if (!definite) {
        if (vars.size() == 1) return integrateExpression(input, vars[0], lang);
        Result r;
        r.mode = "integral";
        r.typeId = "integral_multiple";
        r.engine = "native";
        r.variable = vars[0];
        r.vars = vars;
        r.dimension = (int)vars.size();
        r.definite = false;
        try {
            Ex f = parseExpr(input);
            r.normalized = f;
            addStep(r, "step.title.integrand", f, "step.note.iterated_innermost");
            Ex g = f;
            bool closedAll = true;
            std::string lastMethod;
            for (const auto& v : vars) {
                bool closed = false;
                std::string method;
                Ex G = integrate(g, v, &closed, &method);
                lastMethod = method;
                if (!closed) closedAll = false;
                g = simplify(G);
                addStep(r, "step.title.integrate_with_respect_to", g,
                        "step.note.one_constant_per_variable",
                        "{\"var\":\"" + v + "\",\"method\":\"@ui.method." +
                            method + "\"}");
            }
            r.solution = g;
            r.explicitY = g;
            r.isExplicit = true;
            r.method = lastMethod;
            r.closedForm = closedAll;
            if (!closedAll)
                r.warnings.push_back("step.note.no_elementary_stage");
            addStep(r, "step.title.result", g, "step.note.constant_per_variable");
            r.ok = true;
        } catch (const std::exception& e) {
            r.ok = false;
            setCodedFailure(r, e, "multiple_integral");
        }
        return r;
    }

    // ------------------------------------------------------------- definite
    if (vars.size() == 1) {
        const std::string lo = lower.empty() ? "0" : lower[0];
        const std::string hi = upper.empty() ? "1" : upper[0];
        return integrateDefiniteSingle(input, vars[0], lo, hi, lang);
    }

    Result r;
    r.mode = "integral";
    r.typeId = "integral_multiple_definite";
    r.engine = "native";
    r.variable = vars[0];
    r.vars = vars;
    r.dimension = (int)vars.size();
    r.definite = true;
    r.lower = lower;
    r.upper = upper;
    if (lower.size() != vars.size() || upper.size() != vars.size()) {
        r.ok = false;
        r.errorCode = "bounds_missing";
        return r;
    }
    try {
        Ex f = parseExpr(input);
        r.normalized = f;
        addStep(r, "step.title.integrand", f, "step.note.iterated_definite");

        // 1) Try a fully symbolic evaluation of the iterated integral.
        Ex g = f;
        bool exact = true;
        for (size_t i = 0; i < vars.size(); ++i) {
            bool closed = false;
            std::string method;
            Ex G = integrate(g, vars[i], &closed, &method);
            if (!closed || hasUnevaluatedIntegral(G)) {
                exact = false;
                break;
            }
            Ex lo = parseExpr(lower[i]);
            Ex hi = parseExpr(upper[i]);
            g = simplify(Add(substitute(G, vars[i], hi), Mul(Num(-1), substitute(G, vars[i], lo))));
            addStep(r, "step.title.integrate_over", g, "step.note.from_to_method",
                    "{\"var\":\"" + vars[i] + "\",\"lo\":\"" + boundText(lo) +
                        "\",\"hi\":\"" + boundText(hi) + "\",\"method\":\"@ui.method." +
                        method + "\"}");
        }

        double value = std::nan("");
        if (exact) {
            bool ok = false;
            value = evalReal(g, {}, &ok);
            if (!ok || !std::isfinite(value)) exact = false;
        }

        if (exact) {
            r.solution = simplify(g);
            r.explicitY = r.solution;
            r.isExplicit = true;
            r.method = "definite";
            r.closedForm = true;
            r.hasNumeric = true;
            r.numericValue = value;
            addStep(r, "step.title.value", r.solution, "", "", fmtReal(value));
        } else {
            std::vector<double> lo, hi;
            for (size_t i = 0; i < vars.size(); ++i) {
                bool a = false, b = false;
                const double la = evalReal(parseExpr(lower[i]), {}, &a);
                const double ub = evalReal(parseExpr(upper[i]), {}, &b);
                if (!a || !b || !std::isfinite(la) || !std::isfinite(ub))
                    throw EngineError("bad_bounds", "the bounds must be numeric");
                lo.push_back(la);
                hi.push_back(ub);
            }
            value = numericIntegral(f, vars, lo, hi);
            if (!std::isfinite(value)) throw std::runtime_error("quadrature failed");
            r.method = "numeric";
            r.closedForm = false;
            r.hasNumeric = true;
            r.numericValue = value;
            r.solution = nullptr;
            r.explicitY = nullptr;
            r.isExplicit = false;
            addStep(r, "step.title.nested_quadrature", nullptr,
                    "step.note.nested_quadrature_value",
                    "{\"value\":\"" + fmtReal(value) + "\"}");
            r.warnings.push_back("step.note.answer_numeric");
        }
        r.ok = true;
    } catch (const std::exception& e) {
        r.ok = false;
        setCodedFailure(r, e, "multiple_integral");
    }
    return r;
}

Result derivativeExpression(const std::string& input, const std::string& x, int order,
                            const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "derivative";
    r.typeId = "derivative";
    r.engine = "native";
    r.variable = x;
    if (order < 1 || order > 10) {
        r.ok = false;
        r.order = order < 1 ? 0 : order;
        r.errorCode = "derivative_order";
        return r;
    }
    r.order = order;
    try {
        const bool equation = input.find('=') != std::string::npos;
        if (equation) {
            // ------------------------------------------------ implicit form
            r.implicit = true;
            r.typeId = "derivative_implicit";
            Ex F = parseEquation(input);
            r.normalized = F;

            // The dependent variable is the symbol that is neither the variable of
            // differentiation nor one of the known constants.
            std::vector<std::string> syms;
            collectSymbols(F, syms);
            std::string dep;
            // Prefer y as the dependent variable when the equation contains it;
            // otherwise use the first remaining symbol (e.g. u in F(x,u)=0).
            if (x != "y" && std::find(syms.begin(), syms.end(), "y") != syms.end()) dep = "y";
            if (dep.empty()) {
                for (const auto& s : syms) {
                    if (s == x || s == "pi" || s == "e") continue;
                    dep = s;
                    break;
                }
            }
            if (dep.empty())
                throw EngineError("implicit_no_dependent", "no dependent variable");
            r.method = "implicit";
            r.vars = {dep};

            Ex Fx = simplify(diffAst(F, x));
            Ex Fy = simplify(diffAst(F, dep));
            Ex first;
            Ex d = implicitDerivativeOf(F, x, dep, r.order, &first);
            r.solution = d;
            r.explicitY = d;
            r.isExplicit = true;
            r.closedForm = true;

            const std::string dy = "d" + dep + "/d" + x;
            addStep(r, "step.title.implicit_equation", F,
                    "step.note.differentiate_both_sides", "{\"var\":\"" + x + "\"}");
            addStep(r, "step.title.partial_derivative_f", Fx, "", "",
                    "F_" + x);
            addStep(r, "step.title.partial_derivative_f", Fy, "", "",
                    "F_" + dep);
            addStep(r, "step.title.first_derivative", first, "", "",
                    dy + " = -F_" + x + " / F_" + dep);
            if (r.order >= 2) {
                addStep(r, "step.title.second_derivative", d,
                        "step.note.differentiate_again",
                        "{\"dy\":\"" + dy + "\",\"dep\":\"" + dep +
                            "\",\"x\":\"" + x + "\"}");
            }
            if (r.order >= 3) {
                addStep(r, "step.title.third_derivative", d,
                        "step.note.total_derivative_again");
            }
            if (r.order > 3) {
                addStep(r, "step.title.derivative_of_order", d, "", "",
                        "D^" + std::to_string(r.order));
            }

            // Consistency check: F_x + F_y·y' must vanish.
            try {
                Ex check = simplify(Add(Fx, Mul(Fy, first)));
                const bool ok = check->isZero() || numericallyZero(check, {x, dep}, 8, 1e-6);
                addCheck(r, "check.implicit_relation", ok,
                         ok ? "check.implicit_ok" : "check.implicit_fail");
            } catch (...) {
            }
            r.ok = true;
            return r;
        }

        // ---------------------------------------------------- explicit form
        Ex f = parseExpr(input);
        r.normalized = f;
        std::vector<DerivationNote> notes;
        Ex current = f;
        std::vector<Ex> derivatives;
        for (int k = 1; k <= r.order; ++k) {
            current = simplify(diffAst(current, x, &notes));
            derivatives.push_back(current);
        }
        r.solution = current;
        r.explicitY = current;
        r.isExplicit = true;
        r.closedForm = true;

        // Describe the AST walk: which structural rules produced the answer.
        std::string rules;
        for (const auto& n : notes) {
            if (n.depth > 2) continue;
            const std::string name = n.rule;
            if (rules.find(name) != std::string::npos) continue;
            if (!rules.empty()) rules += ", ";
            rules += name;
        }
        addStep(r, "step.title.differentiation_rules", f,
                rules.empty() ? "step.note.differentiate_node_by_node"
                              : "step.note.differentiation_rules",
                "{\"rules\":\"" + rules + "\"}");
        if (derivatives.size() >= 1)
            addStep(r, "step.title.first_derivative", derivatives[0]);
        if (derivatives.size() >= 2)
            addStep(r, "step.title.second_derivative", derivatives[1]);
        if (derivatives.size() >= 3)
            addStep(r, "step.title.third_derivative", derivatives[2]);
        for (size_t k = 3; k < derivatives.size(); ++k) {
            addStep(r, "step.title.derivative_of_order", derivatives[k], "", "",
                    "D^" + std::to_string(k + 1));
        }

        // Cross-check against the ODE engine's differentiator.
        try {
            Ex ref = f;
            for (int k = 0; k < r.order; ++k) ref = simplify(diff(ref, x));
            const bool ok = numericallyEqual(ref, current, {x}, 8, 1e-6);
            addCheck(r, "check.symbolic_cross_check", ok,
                     ok ? "check.symbolic_agree" : "check.symbolic_disagree");
        } catch (...) {
        }
        r.ok = true;
    } catch (const std::exception& e) {
        r.ok = false;
        setCodedFailure(r, e, "derivative");
    }
    return r;
}

// ------------------------------------------------------ limits & continuity

namespace {

std::string lowerCopy(const std::string& s) {
    std::string out = s;
    for (auto& ch : out) ch = (char)std::tolower((unsigned char)ch);
    return out;
}

bool looksInfinite(const std::string& text, int* sign) {
    std::string t = trim(text);
    t.erase(std::remove_if(t.begin(), t.end(), [](unsigned char c) { return std::isspace(c); }),
            t.end());
    int s = 1;
    if (!t.empty() && (t[0] == '+' )) t.erase(t.begin());
    else if (!t.empty() && t[0] == '-') { s = -1; t.erase(t.begin()); }
    const std::string l = lowerCopy(t);
    if (t == "\xE2\x88\x9E" || l == "inf" || l == "infinity" || l == "oo") {
        if (sign) *sign = s;
        return true;
    }
    return false;
}

Ex parsePoint(const std::string& text) {
    int sign = 1;
    if (looksInfinite(text, &sign)) return infinityPoint(sign);
    return parseExpr(text);
}

std::string pointText(const Ex& point) {
    int sign = 1;
    if (isInfinityPoint(point, &sign)) return sign < 0 ? "-\xE2\x88\x9E" : "\xE2\x88\x9E";
    return toPlain(point);
}

std::string outcomeText(const LimitOutcome& l, bool en) {
    switch (l.kind) {
        case LimitOutcome::Finite:
            return l.value ? toPlain(l.value) : fmtReal(l.number);
        case LimitOutcome::PosInf: return "\xE2\x88\x9E";
        case LimitOutcome::NegInf: return "-\xE2\x88\x9E";
        default: return std::string();
    }
}

std::string limitMath(const LimitOutcome& l) {
    switch (l.kind) {
        case LimitOutcome::Finite:
            return l.value ? toPlain(l.value) : fmtReal(l.number);
        case LimitOutcome::PosInf: return "\xE2\x88\x9E";
        case LimitOutcome::NegInf: return "-\xE2\x88\x9E";
        default: return "";
    }
}

std::string safeText(const Ex& e) { return e ? toPlain(e) : std::string(); }

bool outcomeSame(const LimitOutcome& a, const LimitOutcome& b) {
    if (a.kind != b.kind) return false;
    if (a.kind != LimitOutcome::Finite) return true;
    if (!a.value || !b.value) {
        if (!a.value && !b.value) return std::fabs(a.number - b.number) <= 1e-9 * std::max(1.0, std::fabs(a.number));
        return false;
    }
    if (safeText(a.value) == safeText(b.value)) return true;
    try {
        return numericallyEqual(a.value, b.value, {}, 6, 1e-9);
    } catch (...) {
        return false;
    }
}

void publish(Result& r, const LimitOutcome& l) {
    r.limitExists = l.kind != LimitOutcome::Undefined;
    r.limitInfinite = l.kind == LimitOutcome::PosInf || l.kind == LimitOutcome::NegInf;
    r.limitSign = l.kind == LimitOutcome::NegInf ? -1 : 1;
    if (l.kind == LimitOutcome::Finite) {
        if (l.value) {
            r.solution = l.value;
            r.explicitY = l.value;
            r.isExplicit = true;
            r.closedForm = true;
        } else {
            r.closedForm = false;
        }
        if (l.numeric) {
            r.hasNumeric = true;
            r.numericValue = l.number;
        }
    }
}


} // namespace

std::string lawsParam(const std::vector<std::string>& laws) {
    std::string out = "{\"laws\":[";
    for (size_t i = 0; i < laws.size(); ++i) {
        if (i) out += ",";
        out += "\"@ui.law." + laws[i] + "\"";
    }
    out += "]}";
    return out;
}

Result limitExpression(const std::string& input, const std::string& x, const std::string& point,
                       const std::string& side, const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "limit";
    r.typeId = "limit";
    r.engine = "native";
    r.variable = x;
    r.point = point;
    r.side = side == "left" || side == "right" ? side : "both";
    try {
        Ex f = parseExpr(input);
        Ex a = parsePoint(point);
        r.normalized = f;
        r.vars = {x};
        r.point = pointText(a);

        std::vector<std::string> laws;
        const int dir = r.side == "left" ? -1 : (r.side == "right" ? 1 : 0);
        LimitOutcome L = limitOf(f, x, a, dir, &laws);
        publish(r, L);
        r.laws = laws;

        addStepKey(r, "ode.t.expression_and_limit_point", f,
                   "ode.n.expression_and_limit_point",
                   pjson({{"var", x},
                          {"point", pointText(a)},
                          {"side", "@ui.side." + r.side}}));

        if (r.side == "both" && !isInfinityPoint(a)) {
            LimitOutcome left = limitOf(f, x, a, -1, nullptr);
            LimitOutcome right = limitOf(f, x, a, 1, nullptr);
            r.leftExists = left.kind == LimitOutcome::Finite;
            r.rightExists = right.kind == LimitOutcome::Finite;
            r.leftInfinite = left.kind == LimitOutcome::PosInf || left.kind == LimitOutcome::NegInf;
            r.rightInfinite = right.kind == LimitOutcome::PosInf || right.kind == LimitOutcome::NegInf;
            r.leftSign = left.kind == LimitOutcome::NegInf ? -1 : 1;
            r.rightSign = right.kind == LimitOutcome::NegInf ? -1 : 1;
            if (left.kind == LimitOutcome::Finite) r.leftValue = left.value;
            if (right.kind == LimitOutcome::Finite) r.rightValue = right.value;
            if (left.kind == LimitOutcome::Finite) {
                addStepKey(r, "ode.t.left_limit", left.value, "", "",
                           limitMath(left));
            } else {
                addStepKey(r, "ode.t.left_limit", nullptr, "ode.n.does_not_exist");
            }
            if (right.kind == LimitOutcome::Finite) {
                addStepKey(r, "ode.t.right_limit", right.value, "", "",
                           limitMath(right));
            } else {
                addStepKey(r, "ode.t.right_limit", nullptr, "ode.n.does_not_exist");
            }
            const bool agree = outcomeSame(left, right);
            addCheck(r, "check.one_sided_agree", agree,
                     agree ? "check.limits_equal" : "check.limits_differ");
        }

        if (L.kind == LimitOutcome::Finite) {
            addStepKey(r, "ode.t.result", L.value, "", "", limitMath(L));
            if (L.numeric) r.warnings.push_back("ode.w.numeric_limit");
        } else if (L.kind == LimitOutcome::Undefined) {
            addStepKey(r, "ode.t.result", nullptr, "ode.n.limit_does_not_exist");
        } else {
            addStepKey(r, "ode.t.result", nullptr, "ode.n.limit_diverges",
                       pjson({{"value", outcomeText(L, en)}}));
        }
        if (!laws.empty()) {
            addStepKey(r, "ode.t.limit_laws", nullptr, "ode.n.limit_laws",
                       lawsParam(laws));
        }
        r.ok = true;
    } catch (const std::exception& e) {
        r.ok = false;
        setCodedFailure(r, e, "limit");
    }
    return r;
}

Result continuityExpression(const std::string& input, const std::string& x,
                            const std::string& point, const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "continuity";
    r.typeId = "continuity";
    r.engine = "native";
    r.variable = x;
    r.point = point;
    r.side = "both";
    try {
        Ex f = parseExpr(input);
        Ex a = parsePoint(point);
        if (isInfinityPoint(a))
            throw EngineError("continuity_needs_finite_point", "continuity needs a finite point");
        r.normalized = f;
        r.vars = {x};
        r.point = pointText(a);

        std::vector<std::string> laws;
        ContinuityOutcome c = continuityOf(f, x, a, &laws);
        r.laws = laws;

        r.leftExists = c.left.kind == LimitOutcome::Finite;
        r.rightExists = c.right.kind == LimitOutcome::Finite;
        r.leftInfinite = c.left.kind == LimitOutcome::PosInf || c.left.kind == LimitOutcome::NegInf;
        r.rightInfinite = c.right.kind == LimitOutcome::PosInf || c.right.kind == LimitOutcome::NegInf;
        r.leftSign = c.left.kind == LimitOutcome::NegInf ? -1 : 1;
        r.rightSign = c.right.kind == LimitOutcome::NegInf ? -1 : 1;
        if (c.left.kind == LimitOutcome::Finite) r.leftValue = c.left.value;
        if (c.right.kind == LimitOutcome::Finite) r.rightValue = c.right.value;
        r.valueDefined = c.valueDefined;
        r.valueAt = c.value;
        r.continuous = c.continuous;
        r.discontinuity = c.kind;
        r.limitExists = c.left.kind == LimitOutcome::Finite && c.right.kind == LimitOutcome::Finite &&
                        r.continuous;
        r.limitSign = c.left.kind == LimitOutcome::NegInf ? -1 : 1;
        if (r.continuous && c.value) {
            r.solution = c.value;
            r.explicitY = c.value;
            r.isExplicit = true;
            r.closedForm = true;
        } else if (!c.valueDefined) {
            r.closedForm = false;
        }

        addStepKey(r, "ode.t.function_and_point", f, "ode.n.function_and_point",
                   pjson({{"point", pointText(a)}}));
        if (c.left.kind == LimitOutcome::Finite) {
            addStepKey(r, "ode.t.left_limit", c.left.value, "", "",
                       limitMath(c.left));
        } else {
            addStepKey(r, "ode.t.left_limit", nullptr, "ode.n.does_not_exist");
        }
        if (c.right.kind == LimitOutcome::Finite) {
            addStepKey(r, "ode.t.right_limit", c.right.value, "", "",
                       limitMath(c.right));
        } else {
            addStepKey(r, "ode.t.right_limit", nullptr, "ode.n.does_not_exist");
        }
        if (c.valueDefined) {
            addStepKey(r, "ode.t.value_of_function", c.value, "", "",
                       safeText(c.value));
        } else {
            addStepKey(r, "ode.t.value_of_function", nullptr, "ode.n.value_undefined");
        }

        // The four conditions the definition of continuity requires.
        const bool sameLimits = r.leftExists && r.rightExists && outcomeSame(c.left, c.right);
        auto valueEqualsLimit = [&](const Ex& v, const LimitOutcome& l) {
            if (!v || l.kind != LimitOutcome::Finite) return false;
            if (!l.value) {
                bool ok = false;
                const double d = evalReal(v, {}, &ok);
                return ok && std::fabs(d - l.number) <=
                                 1e-9 * std::max(1.0, std::fabs(l.number));
            }
            if (safeText(v) == safeText(l.value)) return true;
            try {
                return numericallyEqual(v, l.value, {}, 6, 1e-9);
            } catch (...) {
                return false;
            }
        };
        const bool sameValue = sameLimits && r.valueDefined && valueEqualsLimit(c.value, c.left);
        {
            const std::string lm = limitMath(c.left);
            addCheck(r, "check.left_limit_exists", r.leftExists,
                     lm.empty() ? "check.does_not_exist" : "", "", lm);
        }
        {
            const std::string rm = limitMath(c.right);
            addCheck(r, "check.right_limit_exists", r.rightExists,
                     rm.empty() ? "check.does_not_exist" : "", "", rm);
        }
        addCheck(r, "check.left_equals_right", sameLimits,
                 sameLimits ? "check.limits_equal" : "check.limits_differ_or_missing");
        addCheck(r, "check.value_defined", r.valueDefined,
                 r.valueDefined ? "" : "check.value_undefined", "",
                 r.valueDefined ? safeText(c.value) : "");
        addCheck(r, "check.value_equals_limit", sameValue,
                 sameValue ? "check.value_equals" : "check.value_differs");

        const std::string verdictKey =
            r.continuous ? "ode.n.continuous"
            : c.kind == "removable" ? "ode.n.removable_discontinuity"
            : c.kind == "jump" ? "ode.n.jump_discontinuity"
            : c.kind == "infinite" ? "ode.n.infinite_discontinuity"
                                   : "ode.n.not_continuous";
        addStepKey(r, "ode.t.conclusion", nullptr, verdictKey);
        r.ok = true;
    } catch (const std::exception& e) {
        r.ok = false;
        setCodedFailure(r, e, "continuity");
    }
    return r;
}

} // namespace ode
