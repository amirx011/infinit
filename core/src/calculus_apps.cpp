// Infinit Lab — applications of the derivative
// SPDX-License-Identifier: Apache-2.0
#include "calculus_apps.hpp"

#include "graph.hpp"
#include "limits.hpp"
#include "numeric.hpp"
#include "poles.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>

namespace ode {
namespace {

std::string numStr(double v, int digits = 6) {
    if (!std::isfinite(v)) return v < 0 ? "-inf" : "inf";
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*g", digits, v);
    std::string s(buf);
    if (s.find('.') != std::string::npos && s.find('e') == std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s;
}

double evalAt(const Ex& f, const std::string& x, double xv, bool* ok) {
    std::map<std::string, double> vars;
    vars[x] = xv;
    return evalReal(f, vars, ok);
}

double evalConst(const Ex& e, bool* ok) {
    std::map<std::string, double> vars;
    return evalReal(e, vars, ok);
}

// Num(double) would silently truncate through the Num(long long) overload, so
// every real value has to travel through Rational::fromDouble.
Ex numEx(double v) {
    if (!std::isfinite(v)) return nullptr;
    return Num(Rational::fromDouble(v));
}

// Snap a numerical value to a nearby simple rational so the answer prints as an
// exact fraction when it really is one, and as a decimal otherwise.
Ex snap(double v) {
    if (!std::isfinite(v)) return nullptr;
    // Recover the constants e and pi before falling back to a decimal.  A
    // double such as 2.718281828... would otherwise be turned into a huge
    // exact fraction and overflow the rational arithmetic in later steps.
    const double pi = 3.14159265358979323846;
    const double e = 2.71828182845904523536;
    if (std::fabs(v - e) < 1e-12) return Sym("e");
    if (std::fabs(v + e) < 1e-12) return Mul(Num(-1), Sym("e"));
    // Recover small rational multiples of pi (x = pi, x = -pi/2, ...).  Roots
    // such as tan(x) = 0 at x = pi would otherwise print as a huge fraction.
    const double ratio = v / pi;
    for (int den = 1; den <= 64; ++den) {
        const double scaled = ratio * den;
        const double rounded = std::round(scaled);
        if (std::fabs(scaled - rounded) < 1e-10) {
            const long long num = (long long)std::llround(rounded);
            if (num != 0 && std::llabs(num) <= 256)
                return Mul(Num(Rational(num, den)), Sym("pi"));
        }
    }
    for (int den = 1; den <= 64; ++den) {
        const double scaled = v * den;
        const double rounded = std::round(scaled);
        if (std::fabs(scaled - rounded) < 1e-9) {
            const long long n = (long long)std::llround(rounded);
            if (std::llabs(n) <= 1000000) return Num(Rational(n, den));
        }
    }
    // Recover simple radicals: 1/sqrt(2), sqrt(3), sqrt(3)/2, ...
    const double sq = v * v;
    for (int den = 1; den <= 64; ++den) {
        const double scaled = sq * den;
        const double rounded = std::round(scaled);
        if (std::fabs(scaled - rounded) < 1e-9) {
            const long long n = (long long)std::llround(rounded);
            if (n <= 0 || n > 1000000) continue;
            Ex root = Pow(Num(Rational(n, den)), Num(Rational(1, 2)));
            bool ok = false;
            const double check = evalConst(root, &ok);
            if (!ok || std::fabs(check - std::fabs(v)) > 1e-9 * (1.0 + std::fabs(v)))
                continue;
            return v < 0 ? Mul(Num(-1), root) : root;
        }
    }
    return Fn("decimal", {Num(Rational::fromDouble(v))});
}

double asDouble(const Ex& e, bool* ok) { return evalConst(e, ok); }

int signRightOf(const Ex& df, const std::string& x, double at, double lo, double hi) {
    const double step = std::max(1e-5, (hi - lo) * 1e-4);
    for (int k = 1; k <= 8; ++k) {
        const double probe = at + step * k;
        if (probe >= hi) break;
        bool ok = false;
        const double v = evalAt(df, x, probe, &ok);
        if (ok && std::isfinite(v) && std::fabs(v) > 1e-10) return v > 0 ? 1 : -1;
    }
    return 0;
}

int signLeftOf(const Ex& df, const std::string& x, double at, double lo, double hi) {
    const double step = std::max(1e-5, (hi - lo) * 1e-4);
    for (int k = 1; k <= 8; ++k) {
        const double probe = at - step * k;
        if (probe <= lo) break;
        bool ok = false;
        const double v = evalAt(df, x, probe, &ok);
        if (ok && std::isfinite(v) && std::fabs(v) > 1e-10) return v > 0 ? 1 : -1;
    }
    return 0;
}

// The numeric evaluator silently binds unlisted symbols to zero. For a calculus
// analysis that would turn `2*y` into the constant zero, so expressions with
// free parameters are excluded from the root search instead.
bool hasFreeSymbols(const Ex& f, const std::string& x) {
    std::vector<std::string> syms;
    collectSymbols(f, syms);
    for (const auto& s : syms) {
        if (s == x || s == "e" || s == "pi" || s == "inf") continue;
        if (s.rfind("C", 0) == 0) continue;
        return true;
    }
    return false;
}

// A function that is zero at every probe is treated as identically zero on the
// interval. Such a function has infinitely many roots, and enumerating the
// sample grid would emit thousands of duplicates (the `2XY` crash).
bool identicallyZero(const Ex& f, const std::string& x, double lo, double hi) {
    if (!f || f->isZero()) return true;
    if (hasFreeSymbols(f, x)) return false;
    // The offsets are deliberately irregular: a periodic function cannot line
    // up with all of them, so a non-zero function cannot hide between samples.
    static const double fractions[] = {0.071, 0.173, 0.291, 0.417, 0.563,
                                       0.619, 0.733, 0.857, 0.941};
    for (double t : fractions) {
        bool ok = false;
        const double v = evalAt(f, x, lo + (hi - lo) * t, &ok);
        if (!ok || !std::isfinite(v) || std::fabs(v) > 1e-9) return false;
    }
    return true;
}

constexpr size_t kMaxRoots = 64;

// Real roots on [lo, hi] from sign changes, refined by bisection. This backs up
// solvePoly for derivatives whose roots are irrational.
std::vector<double> numericRoots(const Ex& f, const std::string& x, double lo, double hi,
                                 int samples = 4000, bool allowFreeSymbols = false) {
    if (identicallyZero(f, x, lo, hi)) return {};
    if (hasFreeSymbols(f, x) && !allowFreeSymbols) return {};
    std::vector<double> roots;
    const double h = (hi - lo) / samples;
    for (int i = 1; i <= samples && roots.size() < kMaxRoots; ++i) {
        const double a0 = lo + h * (i - 1);
        const double b0 = lo + h * i;
        bool okA = false, okB = false;
        const double ya = evalAt(f, x, a0, &okA);
        const double yb = evalAt(f, x, b0, &okB);
        if (!okA || !okB || !std::isfinite(ya) || !std::isfinite(yb)) continue;
        if (ya == 0) {
            roots.push_back(a0);
            continue;
        }
        if (ya * yb >= 0) continue;
        double a = a0, b = b0, fa = ya;
        for (int it = 0; it < 90; ++it) {
            const double mid = 0.5 * (a + b);
            bool ok = false;
            const double fm = evalAt(f, x, mid, &ok);
            if (!ok || !std::isfinite(fm)) break;
            if (fm == 0) {
                a = b = mid;
                break;
            }
            if (fa * fm < 0) {
                b = mid;
            } else {
                a = mid;
                fa = fm;
            }
        }
        roots.push_back(0.5 * (a + b));
    }
    std::sort(roots.begin(), roots.end());
    std::vector<double> merged;
    for (double r : roots) {
        if (r < lo - 1e-9 || r > hi + 1e-9) continue;
        if (!merged.empty() && std::fabs(r - merged.back()) < 1e-6) continue;
        merged.push_back(r);
    }
    return merged;
}

// Rational roots from solvePoly plus the numeric roots, merged and clipped.
// `strict` drops the endpoints, which is what the mean-value theorems require:
// the promised point c always lies in the OPEN interval (a, b).
std::vector<double> allRoots(const Ex& f, const std::string& x, double lo, double hi,
                             bool strict = false, bool allowFreeSymbols = false) {
    if (identicallyZero(f, x, lo, hi)) return {};
    if (hasFreeSymbols(f, x) && !allowFreeSymbols) return {};
    std::vector<double> out;
    for (const Ex& root : solvePoly(f, x, 6)) {
        bool ok = false;
        std::map<std::string, double> vars;
        vars[x] = 0;
        const double v = evalReal(root, vars, &ok);
        if (ok && std::isfinite(v) && v >= lo - 1e-9 && v <= hi + 1e-9) out.push_back(v);
    }
    for (double r : numericRoots(f, x, lo, hi, 4000, allowFreeSymbols)) out.push_back(r);
    std::sort(out.begin(), out.end());
    std::vector<double> merged;
    for (double r : out) {
        if (!merged.empty() && std::fabs(r - merged.back()) < 1e-6) continue;
        merged.push_back(r);
    }
    if (strict) {
        const double eps = std::max(1e-9, (hi - lo) * 1e-9);
        merged.erase(std::remove_if(merged.begin(), merged.end(),
                                    [&](double v) { return v <= lo + eps || v >= hi - eps; }),
                     merged.end());
    }
    if (merged.size() > kMaxRoots) merged.resize(kMaxRoots);
    return merged;
}

std::string trendCode(int sign, bool zero) {
    if (sign > 0) return "increasing";
    if (sign < 0) return "decreasing";
    return zero ? "constant" : "undecided";
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

void setAppFailure(Result& r, const std::exception& e, const char* where) {
    if (const auto* coded = dynamic_cast<const EngineError*>(&e)) {
        r.errorCode = coded->code;
        r.errorDetail = coded->detail;
        return;
    }
    r.errorCode = "unavailable";
    r.errorDetail = std::string(where) + ": " + e.what();
}

CriticalPointView makePoint(const Ex& xExpr, const Ex& f, const std::string& x, double xv,
                            const std::string& kind) {
    CriticalPointView cp;
    cp.x = (xExpr ? xExpr : snap(xv));
    bool ok = false;
    const double fv = evalAt(f, x, xv, &ok);
    cp.value = (ok && std::isfinite(fv)) ? snap(fv) : nullptr;
    cp.kind = kind;
    return cp;
}

} // namespace

// ------------------------------------------------------------------ behaviour
Result analyseFunction(const std::string& input, const std::string& x, double xmin, double xmax,
                       const std::string& lang) {
    Result r;
    r.mode = "behaviour";
    r.typeId = "behaviour";
    r.variable = x;
    r.order = 1;
    try {
        if (!(xmin < xmax)) {
            r.errorCode = "bad_interval";
            return r;
        }
        Ex f = parseODEExpr(input, x, nullptr, {x, "e", "pi"});
        r.normalized = f;
        Ex df = simplify(diff(f, x));
        Ex ddf = simplify(diff(df, x));
        r.derivative = df;
        r.secondDerivative = ddf;
        addStep(r, "step.title.function_and_derivatives", f, "", "",
                "f' = " + toPlain(df) + ", f'' = " + toPlain(ddf));

        // Poles first: every later list (critical points, inflections,
        // intercepts, cuts) must ignore points where the function is undefined.
        // The symbolic pass gives exact locations such as x = 1 or x = pi/2;
        // a removable hole (for example sin(x)/x at 0) is not a pole.
        auto growsNear = [&](double at, int dir) {
            double prev = 0;
            int count = 0, growth = 0;
            for (double d : {1e-2, 1e-4, 1e-6, 1e-8}) {
                bool ok = false;
                const double v = evalAt(f, x, at + dir * d, &ok);
                // Undefined on this side means a domain boundary, not growth.
                if (!ok || !std::isfinite(v)) return false;
                const double a = std::fabs(v);
                if (count > 0 && a > 5.0 * std::max(prev, 1.0)) ++growth;
                prev = a;
                ++count;
            }
            return count >= 2 && growth >= 2 && prev > 1e3;
        };
        std::vector<PolePoint> poles;
        for (const auto& p : findPoles(f, x, xmin, xmax, true)) {
            if (!std::isfinite(p.numericAt)) continue;
            if (p.numericAt < xmin - 1e-9 || p.numericAt > xmax + 1e-9) continue;
            if (p.exact && p.at) {
                // A symbolic candidate is a genuine pole unless the function
                // has a finite one-sided limit there (a removable hole).
                if (hasFiniteLimitAt(f, x, p.at, -1) || hasFiniteLimitAt(f, x, p.at, +1))
                    continue;
            } else if (!growsNear(p.numericAt, -1) && !growsNear(p.numericAt, +1)) {
                continue;
            }
            poles.push_back(p);
        }
        std::sort(poles.begin(), poles.end(),
                  [](const PolePoint& a, const PolePoint& b) {
                      return a.numericAt < b.numericAt;
                  });
        std::vector<double> poleCuts;
        for (const auto& p : poles) poleCuts.push_back(p.numericAt);
        // Domain boundaries (for example x = 0 for ln(x)) are cuts too, and
        // intervals that lie wholly outside the domain must not be described as
        // monotone or concave.
        std::vector<double> domainCuts;
        try {
            DomainOutput domainScan = analyseDomain(input, x, xmin, xmax, 2000);
            if (domainScan.ok) {
                for (const auto& in : domainScan.domain) {
                    if (!in.loInfinite && in.lo > xmin + 1e-9 && in.lo < xmax - 1e-9)
                        domainCuts.push_back(in.lo);
                    if (!in.hiInfinite && in.hi > xmin + 1e-9 && in.hi < xmax - 1e-9)
                        domainCuts.push_back(in.hi);
                }
            }
        } catch (...) {
        }
        auto intervalDefined = [&](double lo, double hi) {
            for (int k = 1; k < 8; ++k) {
                const double t = lo + (hi - lo) * k / 8.0;
                bool ok = false;
                const double v = evalAt(f, x, t, &ok);
                if (ok && std::isfinite(v)) return true;
            }
            return false;
        };
        auto nearPole = [&](double v) {
            for (double p : poleCuts) {
                if (std::fabs(v - p) <= 1e-6 * (1.0 + std::fabs(v) + std::fabs(p))) return true;
            }
            return false;
        };
        auto finiteAt = [&](const Ex& g, double v) {
            bool ok = false;
            const double r = evalAt(g, x, v, &ok);
            return ok && std::isfinite(r);
        };
        auto dedupeCuts = [](std::vector<double>& v) {
            std::sort(v.begin(), v.end());
            std::vector<double> out;
            for (double c : v) {
                if (!out.empty() &&
                    std::fabs(c - out.back()) <= 1e-6 * (1.0 + std::fabs(c) + std::fabs(out.back())))
                    continue;
                out.push_back(c);
            }
            v.swap(out);
        };

        std::vector<double> critical = allRoots(df, x, xmin, xmax);
        std::vector<CriticalPointView> points;
        std::string critMath;
        for (double c : critical) {
            if (nearPole(c) || !finiteAt(f, c) || !finiteAt(df, c)) continue;
            const int left = signLeftOf(df, x, c, xmin, xmax);
            const int right = signRightOf(df, x, c, xmin, xmax);
            std::string kind;
            if (left > 0 && right < 0) kind = "maximum";
            else if (left < 0 && right > 0) kind = "minimum";
            else if (left == right && left != 0) kind = "inflection";
            else kind = "undecided";
            CriticalPointView cp = makePoint(snap(c), f, x, c, kind);
            points.push_back(cp);

            const std::string kindCode = kind == "maximum" ? "max"
                : kind == "minimum" ? "min"
                : kind == "inflection" ? "flat" : "?";
            if (!critMath.empty()) critMath += ", ";
            critMath += "x = " + toPlain(cp.x);
            if (cp.value) critMath += ", f = " + toPlain(cp.value);
            critMath += " (" + kindCode + ")";
        }
        addStep(r, "step.title.critical_points", df,
                points.empty() ? "step.note.no_critical_points"
                               : "step.note.critical_points",
                "", critMath);

        std::vector<double> bends = allRoots(ddf, x, xmin, xmax);
        std::vector<CriticalPointView> inflections;
        std::string inflectionMath;
        for (double c : bends) {
            if (nearPole(c) || !finiteAt(f, c) || !finiteAt(ddf, c)) continue;
            const int left = signLeftOf(ddf, x, c, xmin, xmax);
            const int right = signRightOf(ddf, x, c, xmin, xmax);
            if (left == 0 || right == 0 || left == right) continue;
            CriticalPointView ip = makePoint(snap(c), f, x, c, "inflection");
            inflections.push_back(ip);
            if (!inflectionMath.empty()) inflectionMath += ", ";
            inflectionMath += "x = " + toPlain(ip.x);
            if (ip.value) inflectionMath += ", f = " + toPlain(ip.value);
        }
        addStep(r, "step.title.inflection_points", ddf,
                inflections.empty() ? "step.note.no_inflection_points"
                                    : "step.note.inflection_points",
                "", inflectionMath);

        // ------------------------------------------------------------- asymptotes
        std::vector<std::string> asymptotes;
        for (const auto& p : poles) {
            asymptotes.push_back("x = " +
                                 (p.exact ? toPlain(p.at) : toPlain(snap(p.numericAt))));
        }
        for (int sign = -1; sign <= 1; sign += 2) {
            const std::string side = sign > 0 ? "x -> +inf" : "x -> -inf";
            Ex inf = infinityPoint(sign);
            LimitOutcome horizontal = limitOf(f, x, inf, 0);
            bool added = false;
            if (horizontal.kind == LimitOutcome::Finite && horizontal.value) {
                bool ok = false;
                const double hv = evalConst(horizontal.value, &ok);
                if (ok && std::isfinite(hv)) {
                    asymptotes.push_back("y = " + toPlain(snap(hv)) + " (" + side + ")");
                    added = true;
                }
            }
            if (added) continue;
            Ex slopeExpr = simplify(Mul(f, Pow(Sym(x), Num(-1))));
            LimitOutcome slope = limitOf(slopeExpr, x, inf, 0);
            if (slope.kind != LimitOutcome::Finite || !slope.value) continue;
            bool okM = false;
            const double mv = evalConst(slope.value, &okM);
            if (!okM || !std::isfinite(mv) || std::fabs(mv) < 1e-9) continue;
            Ex interceptExpr = simplify(Add({f, Mul(Num(-1), Mul(slope.value, Sym(x)))}));
            LimitOutcome intercept = limitOf(interceptExpr, x, inf, 0);
            if (intercept.kind != LimitOutcome::Finite || !intercept.value) continue;
            bool okB = false;
            const double bv = evalConst(intercept.value, &okB);
            if (!okB || !std::isfinite(bv)) continue;
            Ex line = simplify(Add({Mul(snap(mv), Sym(x)), snap(bv)}));
            asymptotes.push_back("y = " + toPlain(line) + " (" + side + ")");
        }
        std::sort(asymptotes.begin(), asymptotes.end());
        asymptotes.erase(std::unique(asymptotes.begin(), asymptotes.end()), asymptotes.end());
        std::string asyMath;
        for (const auto& a : asymptotes) {
            if (!asyMath.empty()) asyMath += ", ";
            asyMath += a;
        }
        addStep(r, "step.title.asymptotes", nullptr,
                asymptotes.empty() ? "step.note.no_asymptotes" : "step.note.asymptotes",
                "", asyMath);
        r.asymptotes = asymptotes;

        // ------------------------------------------------------- curve sketch
        std::vector<Ex> xIntercepts;
        for (double root : allRoots(f, x, xmin, xmax)) {
            if (nearPole(root)) continue;
            bool ok = false;
            const double v = evalAt(f, x, root, &ok);
            if (!ok || !std::isfinite(v) || std::fabs(v) > 1e-3) continue;
            xIntercepts.push_back(snap(root));
        }
        r.xIntercepts = xIntercepts;
        bool yOk = false;
        const double yv = evalAt(f, x, 0.0, &yOk);
        if (yOk && std::isfinite(yv)) {
            r.yIntercept = snap(yv);
            r.hasYIntercept = true;
        }
        std::string interceptMath;
        for (size_t i = 0; i < xIntercepts.size(); ++i) {
            if (i) interceptMath += ", ";
            interceptMath += "x = " + toPlain(xIntercepts[i]);
        }
        if (r.hasYIntercept) {
            if (!interceptMath.empty()) interceptMath += ", ";
            interceptMath += "(0, " + toPlain(r.yIntercept) + ")";
        }

        // Sample several interior points so a single undefined midpoint (for
        // example the pole of 1/x) does not masquerade as a sign.
        auto signIn = [&](const Ex& g, double lo, double hi) {
            for (int k = 1; k <= 11; ++k) {
                const double t = lo + (hi - lo) * k / 12.0;
                bool ok = false;
                const double v = evalAt(g, x, t, &ok);
                if (ok && std::isfinite(v) && std::fabs(v) > 1e-10) return v > 0 ? 1 : -1;
            }
            return 0;
        };

        // Concavity pieces, cut at the inflection points and vertical asymptotes.
        std::vector<double> bendCuts;
        for (const auto& ip : inflections) {
            bool ok = false;
            const double v = asDouble(ip.x, &ok);
            if (ok && std::isfinite(v) && v > xmin + 1e-9 && v < xmax - 1e-9)
                bendCuts.push_back(v);
        }
        for (double a : poleCuts) bendCuts.push_back(a);
        for (double a : domainCuts) bendCuts.push_back(a);
        dedupeCuts(bendCuts);
        std::string concavityMath;
        for (size_t i = 0; i <= bendCuts.size(); ++i) {
            const double lo = (i == 0) ? xmin : bendCuts[i - 1];
            const double hi = (i == bendCuts.size()) ? xmax : bendCuts[i];
            if (hi - lo < 1e-9) continue;
            if (!intervalDefined(lo, hi)) continue;
            const int sign = signIn(ddf, lo, hi);
            if (!concavityMath.empty()) concavityMath += ", ";
            concavityMath += "[" + numStr(lo, 4) + ", " + numStr(hi, 4) + "]: " +
                             (sign > 0 ? "f'' > 0" : (sign < 0 ? "f'' < 0" : "f'' = 0"));
        }

        std::vector<std::string> sketch;
        sketch.push_back("x in [" + numStr(xmin, 4) + ", " + numStr(xmax, 4) + "]");
        if (!interceptMath.empty()) sketch.push_back(interceptMath);
        if (!asymptotes.empty()) sketch.push_back(asyMath);
        if (!points.empty()) sketch.push_back(critMath);
        if (!inflections.empty()) sketch.push_back(inflectionMath);
        r.sketch = sketch;

        // Monotonicity pieces: cut the window at critical points and poles.
        std::vector<double> cuts;
        for (const auto& cp : points) {
            bool ok = false;
            const double v = asDouble(cp.x, &ok);
            if (ok && std::isfinite(v)) cuts.push_back(v);
        }
        for (double a : poleCuts) cuts.push_back(a);
        for (double a : domainCuts) cuts.push_back(a);
        dedupeCuts(cuts);
        std::vector<MonotonePiece> pieces;
        for (size_t i = 0; i <= cuts.size(); ++i) {
            MonotonePiece p;
            p.lo = (i == 0) ? xmin : cuts[i - 1];
            p.hi = (i == cuts.size()) ? xmax : cuts[i];
            if (p.hi - p.lo < 1e-9) continue;
            if (!intervalDefined(p.lo, p.hi)) continue;
            p.sign = signIn(df, p.lo, p.hi);
            p.trend = trendCode(p.sign, identicallyZero(df, x, p.lo, p.hi));
            pieces.push_back(p);
        }
        std::string monoMath;
        for (const auto& p : pieces) {
            if (!monoMath.empty()) monoMath += ", ";
            monoMath += "[" + numStr(p.lo, 4) + ", " + numStr(p.hi, 4) + "]: " +
                        (p.sign > 0 ? "f' > 0" : (p.sign < 0 ? "f' < 0" : "f' = 0"));
        }
        addStep(r, "step.title.monotonicity_intervals", nullptr,
                pieces.empty() ? "step.note.no_monotonicity_interval"
                               : "step.note.monotonicity_sign",
                "", monoMath);

        sketch.push_back(monoMath);
        sketch.push_back(concavityMath);
        r.sketch = sketch;
        addStep(r, "step.title.curve_sketch", f,
                interceptMath.empty() ? "step.note.no_intercepts" : "step.note.intercepts",
                "", interceptMath);

        r.critical = points;
        r.inflections = inflections;
        r.pieces = pieces;
        r.solution = f;
        if (points.empty() && inflections.empty()) {
            r.warnings.push_back("step.note.no_critical_or_inflection");
        }
        r.ok = true;
    } catch (const std::exception& e) {
        setAppFailure(r, e, "behaviour");
    }
    return r;
}

// ------------------------------------------------------------ tangent & normal
Result tangentAndNormal(const std::string& input, const std::string& x, const std::string& at,
                        const std::string& lang) {
    Result r;
    r.mode = "tangent";
    r.typeId = "tangent";
    r.variable = x;
    r.point = at;
    try {
        Ex f = parseODEExpr(input, x, nullptr, {x, "e", "pi"});
        Ex a = parseODEExpr(at, x, nullptr, {x, "e", "pi"});
        r.normalized = f;
        Ex df = simplify(diff(f, x));
        r.derivative = df;

        bool okA = false;
        const double av = asDouble(a, &okA);
        if (!okA || !std::isfinite(av)) {
            r.errorCode = "point_not_real";
            return r;
        }
        bool okF = false, okM = false;
        const double fv = evalAt(f, x, av, &okF);
        const double mv = evalAt(df, x, av, &okM);
        if (!okF || !std::isfinite(fv)) {
            r.errorCode = "not_defined_at_point";
            return r;
        }
        if (!okM || !std::isfinite(mv)) {
            r.errorCode = "not_differentiable_at_point";
            return r;
        }
        r.value = snap(fv);
        r.slope = snap(mv);
        r.tangentSlope = r.slope;

        addStep(r, "step.title.curve_and_point_of_tangency", f, "", "",
                "x = " + toPlain(a));
        addStep(r, "step.title.slope_of_the_tangent", df, "", "",
                "f(a) = " + toPlain(r.value) + ", f'(a) = " + toPlain(r.slope));

        Ex tangent = simplify(Add({r.value, Mul(r.slope, Add({Sym(x), Mul(Num(-1), a)}))}));
        r.tangent = tangent;
        addStep(r, "step.title.tangent_line", tangent, "step.note.point_slope_form");

        if (std::fabs(mv) < 1e-12) {
            r.normal = a;
            addStep(r, "step.title.normal_line", a,
                    "step.note.horizontal_tangent_vertical_normal");
        } else {
            Ex normalSlope = snap(-1.0 / mv);
            r.normalSlope = normalSlope;
            Ex normal = simplify(Add({r.value, Mul(normalSlope, Add({Sym(x), Mul(Num(-1), a)}))}));
            r.normal = normal;
            addStep(r, "step.title.normal_line", normal,
                    "step.note.perpendicular_slopes", "",
                    "m_normal = " + toPlain(normalSlope));
        }
        r.ok = true;
    } catch (const std::exception& e) {
        setAppFailure(r, e, "tangent");
    }
    return r;
}

// --------------------------------------------------------------- optimisation
Result optimiseFunction(const std::string& input, const std::string& x, double xmin, double xmax,
                        const std::string& lang) {
    Result r;
    r.mode = "optimize";
    r.typeId = "optimize";
    r.variable = x;
    try {
        if (!(xmin < xmax)) {
            r.errorCode = "bad_interval";
            return r;
        }
        Ex f = parseODEExpr(input, x, nullptr, {x, "e", "pi"});
        r.normalized = f;
        const std::string unknown = unknownFunction(f);
        if (!unknown.empty()) {
            r.ok = false;
            r.verified = false;
            r.errorCode = "unknown_function";
            r.errorDetail = "unknown function: " + unknown;
            return r;
        }
        Ex df = simplify(diff(f, x));
        r.derivative = df;
        addStep(r, "step.title.objective_function_and_derivative", f, "", "",
                "f' = " + toPlain(df));

        // E4: extra symbols are still bound to zero by the numeric evaluator.
        // Say so instead of hiding the substitution.
        std::vector<std::string> symbols;
        collectSymbols(f, symbols);
        std::vector<std::string> extras;
        for (const auto& s : symbols) {
            if (s == x || s == "e" || s == "pi" || s.rfind("C", 0) == 0) continue;
            extras.push_back(s);
        }
        if (!extras.empty()) {
            r.warnings.push_back("step.note.extra_symbols_zero");
        }

        // E1: a pole or runaway sample means the closed-interval extremum may
        // not exist.  Find the blow-up sign so only the affected side is lost.
        DomainIssue issue;
        const bool singular = checkDomain(f, x, {}, xmin, xmax, &issue);
        int blow = 0;
        if (singular) {
            const double base = std::max(1e-7, (xmax - xmin) * 1e-5);
            int positive = 0, negative = 0;
            for (int k = 1; k <= 8; ++k) {
                for (int side = -1; side <= 1; side += 2) {
                    const double probe = issue.point + side * base * k;
                    if (probe <= xmin || probe >= xmax) continue;
                    bool ok = false;
                    const double v = evalAt(f, x, probe, &ok);
                    if (!ok || !std::isfinite(v)) continue;
                    if (v > 0) ++positive;
                    if (v < 0) ++negative;
                }
            }
            if (positive && !negative) blow = 1;
            else if (negative && !positive) blow = -1;
            r.warnings.push_back("step.note.singular_point_unbounded");
            CriticalPointView sp;
            sp.x = snap(issue.point);
            sp.value = nullptr;
            sp.kind = "singular";
            r.candidates.push_back(sp);
        }

        std::vector<double> critical = allRoots(df, x, xmin, xmax, false, true);
        std::vector<double> candidates = {xmin, xmax};
        for (double c : critical) candidates.push_back(c);

        std::vector<CriticalPointView> all;
        double bestMax = -std::numeric_limits<double>::infinity();
        double bestMin = std::numeric_limits<double>::infinity();
        Ex bestMaxX = nullptr;
        Ex bestMinX = nullptr;
        std::string candMath;
        for (double c : candidates) {
            if (singular &&
                std::fabs(c - issue.point) <= 1e-6 * (1.0 + std::fabs(issue.point)))
                continue;
            bool ok = false;
            const double v = evalAt(f, x, c, &ok);
            if (!ok || !std::isfinite(v)) continue;
            const bool interior = c > xmin + 1e-9 && c < xmax - 1e-9;
            const std::string kind = interior ? "stationary" : "endpoint";
            all.push_back(makePoint(snap(c), f, x, c, kind));
            if (!candMath.empty()) candMath += ", ";
            candMath += "x = " + numStr(c) +
                        (interior ? " ∈ (" + numStr(xmin) + ", " + numStr(xmax) + ")"
                                  : " ∈ {" + numStr(xmin) + ", " + numStr(xmax) + "}");
            if (v > bestMax) {
                bestMax = v;
                bestMaxX = snap(c);
            }
            if (v < bestMin) {
                bestMin = v;
                bestMinX = snap(c);
            }
        }
        if (singular) {
            if (blow > 0) { bestMax = -std::numeric_limits<double>::infinity(); bestMaxX = nullptr; }
            else if (blow < 0) { bestMin = std::numeric_limits<double>::infinity(); bestMinX = nullptr; }
            else { bestMaxX = nullptr; bestMinX = nullptr; }
        }

        // E9/E3: a dense sample may not beat a reported extremum.
        double gridMax = -std::numeric_limits<double>::infinity();
        double gridMin = std::numeric_limits<double>::infinity();
        int gridMaxIndex = -1, gridMinIndex = -1;
        const int gridSamples = 1500;
        for (int i = 1; i < gridSamples; ++i) {
            const double gx = xmin + (xmax - xmin) * i / gridSamples;
            if (singular && std::fabs(gx - issue.point) < 1e-3 * (1.0 + std::fabs(issue.point)))
                continue;
            bool ok = false;
            const double v = evalAt(f, x, gx, &ok);
            if (!ok || !std::isfinite(v)) continue;
            if (v > gridMax) { gridMax = v; gridMaxIndex = i; }
            if (v < gridMin) { gridMin = v; gridMinIndex = i; }
        }
        if (bestMaxX && std::isfinite(gridMax) &&
            gridMax > bestMax + 1e-9 * (1.0 + std::fabs(bestMax))) {
            bestMax = gridMax;
            bestMaxX = snap(xmin + (xmax - xmin) * gridMaxIndex / gridSamples);
        }
        if (bestMinX && std::isfinite(gridMin) &&
            gridMin < bestMin - 1e-9 * (1.0 + std::fabs(bestMin))) {
            bestMin = gridMin;
            bestMinX = snap(xmin + (xmax - xmin) * gridMinIndex / gridSamples);
        }

        for (const auto& point : all) r.candidates.push_back(point);
        if (bestMaxX) {
            r.maximum = snap(bestMax);
            r.maximumAt = bestMaxX;
        }
        if (bestMinX) {
            r.minimum = snap(bestMin);
            r.minimumAt = bestMinX;
        }
        addStep(r, "step.title.candidates_on_closed_interval", df,
                "step.note.closed_interval_candidates", "", candMath);
        if (bestMaxX) {
            addStep(r, "step.title.maximum_point_on_interval", bestMaxX, "", "",
                    "f(" + toPlain(bestMaxX) + ") = " + toPlain(r.maximum));
        } else {
            addStep(r, "step.title.no_maximum_on_interval", nullptr,
                    "step.note.no_maximum_on_interval");
        }
        if (bestMinX) {
            addStep(r, "step.title.minimum_point_on_interval", bestMinX, "", "",
                    "f(" + toPlain(bestMinX) + ") = " + toPlain(r.minimum));
        } else {
            addStep(r, "step.title.no_minimum_on_interval", nullptr,
                    "step.note.no_minimum_on_interval");
        }
        const bool gridOk =
            (!bestMaxX || !std::isfinite(gridMax) ||
             gridMax <= bestMax + 1e-8 * (1.0 + std::fabs(bestMax))) &&
            (!bestMinX || !std::isfinite(gridMin) ||
             gridMin >= bestMin - 1e-8 * (1.0 + std::fabs(bestMin)));
        addCheck(r, "check.sample_grid_bound", gridOk,
                 gridOk ? "check.no_grid_sample_beats" : "check.grid_sample_beats");
        r.ok = true;
    } catch (const std::exception& e) {
        setAppFailure(r, e, "optimize");
    }
    return r;
}

// -------------------------------------------------------------- related rates
// F(q, w) = 0 ties two time-dependent quantities together. Differentiating with
// the chain rule gives A q' + B w' = 0 with A = dF/dq and B = dF/dw, so the
// requested rate is q' = -(B/A) w'. When the caller supplies the instant and the
// known rate, the coefficient is evaluated and the answer becomes a number.
Result relatedRates(const std::string& relation, const std::string& var, const std::string& rateVar,
                    const std::string& t, double qValue, double wValue, double givenRate,
                    const std::string& lang) {
    Result r;
    r.mode = "related_rates";
    r.typeId = "related_rates";
    const std::string time = t.empty() ? "t" : t;
    r.variable = time;
    try {
        const std::string q = var.empty() ? "x" : var;
        const std::string w = rateVar.empty() ? "y" : rateVar;
        if (q == w) {
            r.errorCode = "related_rates_same_names";
            return r;
        }
        Ex F = parseODEExpr(relation, q, nullptr, {q, w, time, "e", "pi"});
        r.normalized = F;
        Ex dq = Deriv(Sym(q), time, 1);
        Ex dw = Deriv(Sym(w), time, 1);
        Ex A = simplify(diff(F, q));
        Ex B = simplify(diff(F, w));
        Ex differentiated = simplify(Add({Mul(A, dq), Mul(B, dw)}));
        r.derivative = differentiated;
        addStep(r, "step.title.relation_and_time_derivative", differentiated,
                "step.note.differentiate_relation_in_time", "",
                "F = " + toPlain(F) + ", A = " + toPlain(A) + ", B = " + toPlain(B) +
                ", A " + q + "' + B " + w + "' = 0");

        if (A->isZero()) {
            r.warnings.push_back("step.note.relation_missing_quantity");
            r.ok = true;
            return r;
        }
        Ex coefficient = simplify(Mul(Num(-1), B, Pow(A, Num(-1))));
        r.solution = coefficient;
        r.slope = coefficient;
        addStep(r, "step.title.isolate_requested_rate", coefficient,
                "step.note.divide_by_coefficient", "",
                q + "' = -(" + toPlain(B) + "/" + toPlain(A) + ") " + w + "'");

        const bool haveInstant =
            std::isfinite(qValue) && std::isfinite(wValue) && std::isfinite(givenRate);
        if (!haveInstant) {
            r.ok = true;
            return r;
        }

        bool okA = false, okB = false;
        Ex Aat = substituteMany(A, {{q, numEx(qValue)}, {w, numEx(wValue)}});
        Ex Bat = substituteMany(B, {{q, numEx(qValue)}, {w, numEx(wValue)}});
        const double aNum = evalConst(simplify(Aat), &okA);
        const double bNum = evalConst(simplify(Bat), &okB);
        if (!okA || !okB || !std::isfinite(aNum) || !std::isfinite(bNum) || std::fabs(aNum) < 1e-12) {
            r.errorCode = "coefficient_not_evaluable";
            return r;
        }

        const double answer = -(bNum / aNum) * givenRate;
        r.hasNumeric = true;
        r.numericValue = answer;
        r.value = numEx(answer);
        addStep(r, "step.title.requested_rate_at_this_instant", numEx(answer),
                "step.note.rate_at_this_instant", "",
                q + " = " + numStr(qValue) + ", " + w + " = " + numStr(wValue) +
                ", A = " + numStr(aNum) + ", B = " + numStr(bNum) +
                ", rate = " + numStr(answer));
        r.ok = true;
    } catch (const std::exception& e) {
        setAppFailure(r, e, "related_rates");
    }
    return r;
}

// --------------------------------------------------------- mean value theorems
Result meanValueTheorems(const std::string& input, const std::string& x, double a, double b,
                         const std::string& lang) {
    Result r;
    r.mode = "mvt";
    r.typeId = "mvt";
    r.variable = x;
    try {
        if (!(a < b)) {
            r.errorCode = "bad_interval";
            return r;
        }
        Ex f = parseODEExpr(input, x, nullptr, {x, "e", "pi"});
        r.normalized = f;
        bool okFa = false, okFb = false;
        const double fa = evalAt(f, x, a, &okFa);
        const double fb = evalAt(f, x, b, &okFb);
        if (!okFa || !okFb || !std::isfinite(fa) || !std::isfinite(fb)) {
            r.errorCode = "mvt_endpoints_undefined";
            return r;
        }
        r.valueA = snap(fa);
        r.valueB = snap(fb);
        addStep(r, "step.title.function_and_endpoint_values", f,
                "step.note.endpoint_values", "",
                "f(a) = " + numStr(fa) + ", f(b) = " + numStr(fb));

        const int probes = 32;
        bool continuous = true;
        for (int i = 0; i <= probes; ++i) {
            bool ok = false;
            const double v = evalAt(f, x, a + (b - a) * i / probes, &ok);
            if (!ok || !std::isfinite(v)) {
                continuous = false;
                break;
            }
        }
        Ex df = simplify(diff(f, x));
        r.derivative = df;
        bool differentiable = true;
        for (int i = 1; i < probes; ++i) {
            bool ok = false;
            const double v = evalAt(df, x, a + (b - a) * i / probes, &ok);
            if (!ok || !std::isfinite(v)) {
                differentiable = false;
                break;
            }
        }
        r.hypothesesContinuous = continuous;
        r.hypothesesDifferentiable = differentiable;
        addCheck(r, "check.continuity_closed_interval", continuous,
                 continuous ? "check.continuous_all_points" : "check.undefined_sample");
        addCheck(r, "check.differentiability_open_interval", differentiable,
                 differentiable ? "check.derivative_finite"
                                : "check.derivative_undefined_sample");

        Ex secant = simplify(Mul(Add({numEx(fb), numEx(-fa)}), numEx(1.0 / (b - a))));
        r.slope = secant;
        const bool rolle = std::fabs(fb - fa) < 1e-9;
        r.rolleApplies = rolle;
        addStep(r, "step.title.secant_slope", secant,
                rolle ? "step.note.rolle_applies" : "step.note.mvt_promise", "",
                "m_secant = " + toPlain(secant));

        Ex target = simplify(Add({df, Mul(Num(-1), secant)}));
        addStep(r, "step.title.solve_for_c", substitute(target, x, Sym("c")));
        std::vector<double> roots;
        const bool everyPoint = identicallyZero(target, x, a, b);
        if (everyPoint) {
            // A linear function has f'(x) equal to the secant slope everywhere,
            // and a constant function has both sides zero. Reporting one
            // representative interior point is the honest answer.
            roots.push_back(0.5 * (a + b));
        } else {
            roots = allRoots(target, x, a, b, true);
        }
        constexpr size_t kMaxTheoremPoints = 16;
        const bool truncated = roots.size() > kMaxTheoremPoints;
        if (truncated) roots.resize(kMaxTheoremPoints);
        for (double c : roots) {
            r.points.push_back(snap(c));
            bool okF = false;
            const double fv = evalAt(f, x, c, &okF);
            r.pointValues.push_back((okF && std::isfinite(fv)) ? snap(fv) : nullptr);
        }
        if (truncated) {
            r.warnings.push_back("step.note.more_than_16_points");
        }
        if (r.points.empty()) {
            r.warnings.push_back("step.note.no_point_satisfies_conclusion");
        } else {
            for (const auto& c : r.points) {
                bool ok = false;
                const double cv = asDouble(c, &ok);
                bool okv = false;
                const double fv = ok ? evalAt(f, x, cv, &okv) : 0;
                const bool haveValue = ok && okv && std::isfinite(fv);
                std::string pointMath = "c = " + toPlain(c);
                if (haveValue) pointMath += ", f(c) = " + numStr(fv);
                addStep(r, "step.title.guaranteed_point_of_mvt", c,
                        everyPoint ? "step.note.mvt_every_point"
                                   : "step.note.mvt_guaranteed_point",
                        "", pointMath);
            }
        }
        r.ok = true;
    } catch (const std::exception& e) {
        setAppFailure(r, e, "mvt");
    }
    return r;
}

Result cauchyMeanValue(const std::string& fIn, const std::string& gIn, const std::string& x, double a,
                       double b, const std::string& lang) {
    Result r;
    r.mode = "cauchy";
    r.typeId = "cauchy";
    r.variable = x;
    try {
        if (!(a < b)) {
            r.errorCode = "bad_interval";
            return r;
        }
        Ex f = parseODEExpr(fIn, x, nullptr, {x, "e", "pi"});
        Ex g = parseODEExpr(gIn, x, nullptr, {x, "e", "pi"});
        r.functionF = f;
        r.functionG = g;
        addStep(r, "step.title.functions_f_and_g", f, "", "",
                "g = " + toPlain(g));

        bool ok = false;
        const double fa = evalConst(substitute(f, x, numEx(a)), &ok);
        const double fb = evalConst(substitute(f, x, numEx(b)), &ok);
        const double ga = evalConst(substitute(g, x, numEx(a)), &ok);
        const double gb = evalConst(substitute(g, x, numEx(b)), &ok);
        if (!std::isfinite(fa) || !std::isfinite(fb) || !std::isfinite(ga) || !std::isfinite(gb)) {
            r.errorCode = "cauchy_endpoints_undefined";
            return r;
        }
        if (std::fabs(gb - ga) < 1e-12) {
            r.errorCode = "cauchy_equal_endpoints";
            return r;
        }
        r.valueA = snap(fa);
        r.valueB = snap(fb);
        r.functionValueA = snap(ga);
        r.functionValueB = snap(gb);
        Ex df = simplify(diff(f, x));
        Ex dg = simplify(diff(g, x));
        r.derivative = df;
        r.secondDerivative = dg;
        const double ratio = (fb - fa) / (gb - ga);
        addStep(r, "step.title.derivatives_and_ratio_of_increments", Add({df, dg}),
                "step.note.ratio_of_increments", "",
                "f' = " + toPlain(df) + ", g' = " + toPlain(dg) +
                ", ratio = " + numStr(ratio));

        Ex target = simplify(Add({df, Mul(Num(-1), Mul(dg, numEx(ratio)))}));
        std::vector<double> roots;
        const bool everyPoint = identicallyZero(target, x, a, b);
        if (everyPoint) {
            roots.push_back(0.5 * (a + b));
        } else {
            roots = allRoots(target, x, a, b, true);
        }
        constexpr size_t kMaxTheoremPoints = 16;
        const bool truncated = roots.size() > kMaxTheoremPoints;
        if (truncated) roots.resize(kMaxTheoremPoints);
        for (double c : roots) {
            r.points.push_back(snap(c));
            bool okF = false;
            const double fv = evalAt(f, x, c, &okF);
            r.pointValues.push_back((okF && std::isfinite(fv)) ? snap(fv) : nullptr);
        }
        if (truncated) {
            r.warnings.push_back("step.note.more_than_16_points");
        }
        if (r.points.empty()) {
            r.warnings.push_back("step.note.no_cauchy_point");
        } else {
            for (const auto& c : r.points) {
                addStep(r, "step.title.guaranteed_point_of_cauchy", c,
                        everyPoint ? "step.note.cauchy_every_point"
                                   : "step.note.cauchy_guaranteed_point",
                        "", "c = " + toPlain(c));
            }
        }
        r.ok = true;
    } catch (const std::exception& e) {
        setAppFailure(r, e, "cauchy");
    }
    return r;
}

// ----------------------------------------------------------------- L'Hopital
Result lhopitalLimit(const std::string& numerator, const std::string& denominator,
                     const std::string& x, const std::string& point, const std::string& lang) {
    Result r;
    r.mode = "lhopital";
    r.typeId = "lhopital";
    r.variable = x;
    r.point = point;
    try {
        Ex N = parseODEExpr(numerator, x, nullptr, {x, "e", "pi"});
        Ex D = parseODEExpr(denominator, x, nullptr, {x, "e", "pi"});
        Ex a = parseODEExpr(point, x, nullptr, {x, "e", "pi", "inf"});
        r.normalized = N;
        r.functionF = N;
        r.functionG = D;
        Ex quotient = Mul(N, Pow(D, Num(-1)));
        addStep(r, "step.title.quotient_f_over_g", quotient, "", "",
                "f = " + toPlain(N) + ", g = " + toPlain(D));

        std::vector<std::string> rules;
        LimitOutcome out = limitOf(quotient, x, a, 0, &rules);
        bool used = false;
        for (const auto& rule : rules) {
            if (rule == "lhopital") used = true;
        }
        if (used) {
            addStep(r, "step.title.indeterminate_form", nullptr,
                    "step.note.lhopital_applies");
        } else {
            addStep(r, "step.title.direct_substitution", nullptr,
                    "step.note.lhopital_unjustified");
        }

        if (out.kind == LimitOutcome::Finite) {
            r.solution = out.value;
            r.limitExists = true;
            addStep(r, "step.title.limit_of_the_quotient", out.value);
        } else if (out.kind == LimitOutcome::PosInf || out.kind == LimitOutcome::NegInf) {
            r.limitInfinite = true;
            r.limitExists = true;
            r.limitSign = out.kind == LimitOutcome::PosInf ? 1 : -1;
            addStep(r, "step.title.quotient_diverges");
        } else {
            r.divergent = true;
            addStep(r, "step.title.limit_does_not_exist");
        }
        r.ok = true;
    } catch (const std::exception& e) {
        setAppFailure(r, e, "lhopital");
    }
    return r;
}

} // namespace ode
