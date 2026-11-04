// Infinit Lab — applications of the derivative
// SPDX-License-Identifier: Apache-2.0
#include "calculus_apps.hpp"

#include "limits.hpp"
#include "numeric.hpp"

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
    for (int den = 1; den <= 64; ++den) {
        const double scaled = v * den;
        const double rounded = std::round(scaled);
        if (std::fabs(scaled - rounded) < 1e-9) {
            const long long n = (long long)std::llround(rounded);
            if (std::llabs(n) <= 1000000) return Num(Rational(n, den));
        }
    }
    return Num(Rational::fromDouble(v));
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
                                 int samples = 4000) {
    if (identicallyZero(f, x, lo, hi)) return {};
    if (hasFreeSymbols(f, x)) return {};
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
                             bool strict = false) {
    if (identicallyZero(f, x, lo, hi)) return {};
    if (hasFreeSymbols(f, x)) return {};
    std::vector<double> out;
    for (const Ex& root : solvePoly(f, x, 6)) {
        bool ok = false;
        std::map<std::string, double> vars;
        vars[x] = 0;
        const double v = evalReal(root, vars, &ok);
        if (ok && std::isfinite(v) && v >= lo - 1e-9 && v <= hi + 1e-9) out.push_back(v);
    }
    for (double r : numericRoots(f, x, lo, hi)) out.push_back(r);
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

std::string trendName(int sign, bool en) {
    if (sign > 0) return en ? "increasing" : "صعودی";
    if (sign < 0) return en ? "decreasing" : "نزولی";
    return en ? "constant" : "ثابت";
}

void addStep(Result& r, bool en, const std::string& enText, const std::string& faText,
             const Ex& math = nullptr, const std::string& noteEn = "",
             const std::string& noteFa = "") {
    Step s;
    s.title = en ? enText : faText;
    s.math = math;
    s.note = en ? noteEn : noteFa;
    r.steps.push_back(std::move(s));
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
    const bool en = lang != "fa";
    Result r;
    r.mode = "behaviour";
    r.typeId = "behaviour";
    r.typeFa = "تحلیل رفتار تابع";
    r.variable = x;
    r.order = 1;
    try {
        if (!(xmin < xmax)) {
            r.error = en ? "The interval must satisfy a < b." : "باید a < b باشد.";
            return r;
        }
        Ex f = parseODEExpr(input, x, nullptr, {x, "e", "pi"});
        r.normalized = f;
        Ex df = simplify(diff(f, x));
        Ex ddf = simplify(diff(df, x));
        r.derivative = df;
        r.secondDerivative = ddf;
        addStep(r, en, "Function and derivatives", "تابع و مشتق‌ها", f,
                en ? ("f' = " + toPlain(df) + " and f'' = " + toPlain(ddf) + ".")
                   : "",
                en ? ""
                   : ("f′ = " + toPlain(df) + " و f″ = " + toPlain(ddf) + "."));

        std::vector<double> critical = allRoots(df, x, xmin, xmax);
        std::vector<CriticalPointView> points;
        std::string critFa, critEn;
        for (double c : critical) {
            const int left = signLeftOf(df, x, c, xmin, xmax);
            const int right = signRightOf(df, x, c, xmin, xmax);
            std::string kind;
            if (left > 0 && right < 0) kind = "maximum";
            else if (left < 0 && right > 0) kind = "minimum";
            else if (left == right && left != 0) kind = "inflection";
            else kind = "undecided";
            CriticalPointView cp = makePoint(snap(c), f, x, c, kind);
            points.push_back(cp);

            std::string kindText;
            if (kind == "maximum") kindText = en ? "local maximum" : "بیشینهٔ محلی";
            else if (kind == "minimum") kindText = en ? "local minimum" : "کمینهٔ محلی";
            else if (kind == "inflection") kindText = en ? "stationary turning point" : "نقطهٔ تخت";
            else kindText = en ? "undecided" : "نامشخص";
            if (!critFa.empty()) { critFa += "، "; critEn += ", "; }
            critFa += "x = " + toPlain(cp.x);
            critEn += "x = " + toPlain(cp.x);
            if (cp.value) {
                critFa += " (f = " + toPlain(cp.value) + "، " + kindText + ")";
                critEn += " (f = " + toPlain(cp.value) + ", " + kindText + ")";
            } else {
                critFa += " (" + kindText + ")";
                critEn += " (" + kindText + ")";
            }
        }
        if (points.empty()) {
            critFa = "در این بازه نقطهٔ بحرانی وجود ندارد.";
            critEn = "No critical point in this window.";
        }
        addStep(r, en, "Critical points", "نقاط بحرانی", df, critEn, critFa);

        std::vector<double> bends = allRoots(ddf, x, xmin, xmax);
        std::vector<CriticalPointView> inflections;
        std::string infFa, infEn;
        for (double c : bends) {
            const int left = signLeftOf(ddf, x, c, xmin, xmax);
            const int right = signRightOf(ddf, x, c, xmin, xmax);
            if (left == 0 || right == 0 || left == right) continue;
            CriticalPointView ip = makePoint(snap(c), f, x, c, "inflection");
            inflections.push_back(ip);
            if (!infFa.empty()) { infFa += "، "; infEn += ", "; }
            infFa += "x = " + toPlain(ip.x);
            infEn += "x = " + toPlain(ip.x);
            if (ip.value) {
                infFa += " (f = " + toPlain(ip.value) + ")";
                infEn += " (f = " + toPlain(ip.value) + ")";
            }
        }
        if (inflections.empty()) {
            infFa = "در این بازه نقطهٔ عطفی وجود ندارد.";
            infEn = "No inflection point in this window.";
        }
        addStep(r, en, "Inflection points", "نقاط عطف", ddf, infEn, infFa);

        // Monotonicity pieces: cut the window at the critical points.
        std::vector<double> cuts;
        for (const auto& cp : points) {
            bool ok = false;
            const double v = asDouble(cp.x, &ok);
            if (ok && std::isfinite(v)) cuts.push_back(v);
        }
        std::sort(cuts.begin(), cuts.end());
        std::vector<MonotonePiece> pieces;
        for (size_t i = 0; i <= cuts.size(); ++i) {
            MonotonePiece p;
            p.lo = (i == 0) ? xmin : cuts[i - 1];
            p.hi = (i == cuts.size()) ? xmax : cuts[i];
            if (p.hi - p.lo < 1e-9) continue;
            bool ok = false;
            const double dv = evalAt(df, x, 0.5 * (p.lo + p.hi), &ok);
            p.sign = (ok && std::isfinite(dv)) ? (dv > 0 ? 1 : (dv < 0 ? -1 : 0)) : 0;
            p.trend = trendName(p.sign, en);
            pieces.push_back(p);
        }
        std::string monoFa, monoEn;
        for (const auto& p : pieces) {
            if (!monoFa.empty()) { monoFa += "، "; monoEn += ", "; }
            monoFa += "[" + numStr(p.lo, 4) + "، " + numStr(p.hi, 4) + "] " + p.trend;
            monoEn += "[" + numStr(p.lo, 4) + ", " + numStr(p.hi, 4) + "] " + p.trend;
        }
        if (pieces.empty()) {
            monoFa = "بازهٔ یکنوایی مشخصی پیدا نشد.";
            monoEn = "No monotonicity interval was found.";
        }
        addStep(r, en, "Monotonicity intervals", "بازه‌های یکنوایی", nullptr, monoEn, monoFa);

        r.critical = points;
        r.inflections = inflections;
        r.pieces = pieces;
        r.solution = f;
        if (points.empty() && inflections.empty()) {
            r.warnings.push_back(en
                ? "No critical or inflection point was found in this window; the function is monotone across it."
                : "در این بازه نقطهٔ بحرانی یا عطفی پیدا نشد؛ تابع روی آن یکنوا است.");
        }
        r.ok = true;
    } catch (const std::exception& e) {
        r.error = en ? std::string("Behaviour analysis error: ") + e.what()
                     : std::string("خطا در تحلیل رفتار تابع: ") + e.what();
    }
    return r;
}

// ------------------------------------------------------------ tangent & normal
Result tangentAndNormal(const std::string& input, const std::string& x, const std::string& at,
                        const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "tangent";
    r.typeId = "tangent";
    r.typeFa = "مماس و خط عمود";
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
            r.error = en ? "The point a must be a real number." : "نقطهٔ a باید یک عدد حقیقی باشد.";
            return r;
        }
        bool okF = false, okM = false;
        const double fv = evalAt(f, x, av, &okF);
        const double mv = evalAt(df, x, av, &okM);
        if (!okF || !std::isfinite(fv)) {
            r.error = en ? "f is not defined at this point." : "تابع در این نقطه تعریف نشده است.";
            return r;
        }
        if (!okM || !std::isfinite(mv)) {
            r.error = en
                ? "f is not differentiable at this point, so it has no tangent line there."
                : "تابع در این نقطه مشتق‌پذیر نیست، پس مماس هم ندارد.";
            return r;
        }
        r.value = snap(fv);
        r.slope = snap(mv);
        r.tangentSlope = r.slope;

        addStep(r, en, "Curve and point of tangency", "منحنی و نقطهٔ مماس", f,
                "x = " + toPlain(a), "x = " + toPlain(a));
        addStep(r, en, "Slope of the tangent", "شیب مماس", df,
                en ? ("f(a) = " + toPlain(r.value) + " and f'(a) = " + toPlain(r.slope) + ".")
                   : "",
                en ? ""
                   : ("f(a) = " + toPlain(r.value) + " و f′(a) = " + toPlain(r.slope) + "."));

        Ex tangent = simplify(Add({r.value, Mul(r.slope, Add({Sym(x), Mul(Num(-1), a)}))}));
        r.tangent = tangent;
        addStep(r, en, "Tangent line: y = f(a) + f'(a)(x - a)",
                "خط مماس: y = f(a) + f′(a)(x − a)", tangent,
                en ? "Point-slope form anchored at the point of tangency." : "",
                en ? "" : "شکل نقطه‌شیب که در نقطهٔ مماس نوشته شده است.");

        if (std::fabs(mv) < 1e-12) {
            r.normal = a;
            addStep(r, en, "Normal line", "خط عمود", a,
                    en ? "The tangent is horizontal, so the normal is the vertical line x = a." : "",
                    en ? "" : "مماس افقی است، پس خط عمود خط قائم x = a است.");
        } else {
            Ex normalSlope = snap(-1.0 / mv);
            r.normalSlope = normalSlope;
            Ex normal = simplify(Add({r.value, Mul(normalSlope, Add({Sym(x), Mul(Num(-1), a)}))}));
            r.normal = normal;
            addStep(r, en, "Normal line", "خط عمود", normal,
                    en ? ("Perpendicular slopes multiply to -1, so the normal slope is " +
                          toPlain(normalSlope) + ".")
                       : "",
                    en ? ""
                       : ("حاصل‌ضرب شیب دو خط عمود −۱ است، پس شیب خط عمود " +
                          toPlain(normalSlope) + " است."));
        }
        r.ok = true;
    } catch (const std::exception& e) {
        r.error = en ? std::string("Tangent/normal error: ") + e.what()
                     : std::string("خطا در مماس و خط عمود: ") + e.what();
    }
    return r;
}

// --------------------------------------------------------------- optimisation
Result optimiseFunction(const std::string& input, const std::string& x, double xmin, double xmax,
                        const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "optimize";
    r.typeId = "optimize";
    r.typeFa = "مسایل بهینه‌سازی";
    r.variable = x;
    try {
        if (!(xmin < xmax)) {
            r.error = en ? "The interval must satisfy a < b." : "باید a < b باشد.";
            return r;
        }
        Ex f = parseODEExpr(input, x, nullptr, {x, "e", "pi"});
        r.normalized = f;
        Ex df = simplify(diff(f, x));
        r.derivative = df;
        addStep(r, en, "Objective function and derivative", "تابع هدف و مشتق", f,
                en ? ("f' = " + toPlain(df) + ".") : "",
                en ? "" : ("f′ = " + toPlain(df) + "."));

        std::vector<double> critical = allRoots(df, x, xmin, xmax);
        std::vector<double> candidates = {xmin, xmax};
        for (double c : critical) candidates.push_back(c);

        std::vector<CriticalPointView> all;
        double bestMax = -std::numeric_limits<double>::infinity();
        double bestMin = std::numeric_limits<double>::infinity();
        Ex bestMaxX = nullptr;
        Ex bestMinX = nullptr;
        std::string candFa, candEn;
        for (double c : candidates) {
            bool ok = false;
            const double v = evalAt(f, x, c, &ok);
            if (!ok || !std::isfinite(v)) continue;
            const bool interior = c > xmin + 1e-9 && c < xmax - 1e-9;
            const std::string kind = interior ? "stationary" : "endpoint";
            all.push_back(makePoint(snap(c), f, x, c, kind));
            if (!candFa.empty()) { candFa += "، "; candEn += ", "; }
            candFa += "x = " + numStr(c) + (interior ? " (داخلی)" : " (سر بازه)");
            candEn += "x = " + numStr(c) + (interior ? " (interior)" : " (endpoint)");
            if (v > bestMax) {
                bestMax = v;
                bestMaxX = snap(c);
            }
            if (v < bestMin) {
                bestMin = v;
                bestMinX = snap(c);
            }
        }
        r.candidates = all;
        if (bestMaxX) {
            r.maximum = snap(bestMax);
            r.maximumAt = bestMaxX;
        }
        if (bestMinX) {
            r.minimum = snap(bestMin);
            r.minimumAt = bestMinX;
        }
        addStep(r, en, "Candidates on the closed interval", "کاندیدها روی بازهٔ بسته", df,
                en ? ("Critical points of f'(x)=0 plus both endpoints: " + candEn +
                      ". An extreme value on a closed interval must occur at one of these.")
                   : "",
                en ? ""
                   : ("نقاط f′(x)=0 به‌همراه دو سر بازه: " + candFa +
                      ". اکسترمم روی بازهٔ بسته فقط در یکی از این‌ها رخ می‌دهد."));
        if (bestMaxX) {
            addStep(r, en, "Maximum point on the interval", "نقطهٔ بیشینه روی بازه",
                    bestMaxX, "f(" + toPlain(bestMaxX) + ") = " + toPlain(r.maximum) + ".",
                    "f(" + toPlain(bestMaxX) + ") = " + toPlain(r.maximum) + ".");
        }
        if (bestMinX) {
            addStep(r, en, "Minimum point on the interval", "نقطهٔ کمینه روی بازه",
                    bestMinX, "f(" + toPlain(bestMinX) + ") = " + toPlain(r.minimum) + ".",
                    "f(" + toPlain(bestMinX) + ") = " + toPlain(r.minimum) + ".");
        }
        r.ok = true;
    } catch (const std::exception& e) {
        r.error = en ? std::string("Optimisation error: ") + e.what()
                     : std::string("خطا در بهینه‌سازی: ") + e.what();
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
    const bool en = lang != "fa";
    Result r;
    r.mode = "related_rates";
    r.typeId = "related_rates";
    r.typeFa = "نرخ‌های مرتبط";
    const std::string time = t.empty() ? "t" : t;
    r.variable = time;
    try {
        const std::string q = var.empty() ? "x" : var;
        const std::string w = rateVar.empty() ? "y" : rateVar;
        if (q == w) {
            r.error = en ? "The two quantities must have different names."
                         : "دو کمیت باید نام‌های متفاوت داشته باشند.";
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
        addStep(r, en, "Relation and time derivative", "رابطه و مشتق زمانی", differentiated,
                en ? ("F = " + toPlain(F) + ". Differentiating F(q,w) = 0 with respect to time "
                      "gives A q' + B w' = 0, with A = " + toPlain(A) + " and B = " + toPlain(B) + ".")
                   : "",
                en ? ""
                   : ("F = " + toPlain(F) + ". مشتق‌گیری از F(q,w) = 0 نسبت به زمان "
                      "A q′ + B w′ = 0 را می‌دهد، با A = " + toPlain(A) + " و B = " + toPlain(B) + "."));

        if (A->isZero()) {
            r.warnings.push_back(en
                ? "The relation does not involve the requested quantity, so its rate is not fixed."
                : "رابطه شامل کمیت خواسته‌شده نیست، پس نرخ آن تعیین نمی‌شود.");
            r.ok = true;
            return r;
        }
        if (dependsOn(A, w) || dependsOn(B, q)) {
            r.warnings.push_back(en
                ? "The relation could not be isolated for the requested rate symbolically; the "
                  "differentiated relation is shown instead."
                : "رابطه برای نرخ خواسته‌شده به‌صورت نمادین جدا نشد؛ رابطهٔ مشتق‌گرفته‌شده نمایش داده می‌شود.");
            r.ok = true;
            return r;
        }

        Ex coefficient = simplify(Mul(Num(-1), B, Pow(A, Num(-1))));
        r.solution = coefficient;
        r.slope = coefficient;
        addStep(r, en, "Isolate the requested rate", "جدا کردن نرخ خواسته‌شده",
                coefficient,
                en ? "Dividing by A leaves the requested rate as a multiple of the known one." : "",
                en ? "" : "تقسیم بر A نرخ خواسته‌شده را به‌صورت ضریبی از نرخ معلوم باقی می‌گذارد.");

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
            r.error = en ? "The coefficient could not be evaluated at those values."
                         : "ضریب در آن مقادیر قابل محاسبه نبود.";
            return r;
        }

        const double answer = -(bNum / aNum) * givenRate;
        r.hasNumeric = true;
        r.numericValue = answer;
        r.value = numEx(answer);
        addStep(r, en, "Requested rate at this instant", "نرخ خواسته‌شده در این لحظه",
                numEx(answer),
                en ? ("With " + q + " = " + numStr(qValue) + ", " + w + " = " + numStr(wValue) +
                      ", A = " + numStr(aNum) + " and B = " + numStr(bNum) + ", the rate is " +
                      numStr(answer) + " per unit time.")
                   : "",
                en ? ""
                   : ("با " + q + " = " + numStr(qValue) + "، " + w + " = " + numStr(wValue) +
                      "، A = " + numStr(aNum) + " و B = " + numStr(bNum) + "، نرخ برابر " +
                      numStr(answer) + " در واحد زمان است."));
        r.ok = true;
    } catch (const std::exception& e) {
        r.error = en ? std::string("Related-rates error: ") + e.what()
                     : std::string("خطا در نرخ‌های مرتبط: ") + e.what();
    }
    return r;
}

// --------------------------------------------------------- mean value theorems
Result meanValueTheorems(const std::string& input, const std::string& x, double a, double b,
                         const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "mvt";
    r.typeId = "mvt";
    r.typeFa = "قضیه مقدار میانگین";
    r.variable = x;
    try {
        if (!(a < b)) {
            r.error = en ? "The interval must satisfy a < b." : "باید a < b باشد.";
            return r;
        }
        Ex f = parseODEExpr(input, x, nullptr, {x, "e", "pi"});
        r.normalized = f;
        bool okFa = false, okFb = false;
        const double fa = evalAt(f, x, a, &okFa);
        const double fb = evalAt(f, x, b, &okFb);
        if (!okFa || !okFb || !std::isfinite(fa) || !std::isfinite(fb)) {
            r.error = en ? "f must be defined at both endpoints."
                         : "تابع باید در دو سر بازه تعریف شده باشد.";
            return r;
        }
        r.valueA = snap(fa);
        r.valueB = snap(fb);
        addStep(r, en, "Function and endpoint values", "تابع و مقادیر دو سر", f,
                en ? ("f(a) = " + numStr(fa) + " and f(b) = " + numStr(fb) + ".")
                   : "",
                en ? ""
                   : ("f(a) = " + numStr(fa) + " و f(b) = " + numStr(fb) + "."));

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
        r.checks.push_back({en ? "Continuity on [a, b]" : "پیوستگی روی [a, b]", continuous,
                            continuous
                                ? (en ? "f is finite at every sampled point of the closed interval."
                                      : "تابع در همهٔ نمونه‌های بازهٔ بسته متناهی است.")
                                : (en ? "A sampled point of f was undefined."
                                      : "یک نمونه از f تعریف‌نشده بود.")});
        r.checks.push_back({en ? "Differentiability on (a, b)" : "مشتق‌پذیری روی (a, b)",
                            differentiable,
                            differentiable
                                ? (en ? "f' is finite inside the open interval."
                                      : "مشتق در داخل بازهٔ باز متناهی است.")
                                : (en ? "A sampled point of f' was undefined."
                                      : "یک نمونه از f′ تعریف‌نشده بود.")});

        Ex secant = simplify(Mul(Add({numEx(fb), numEx(-fa)}), numEx(1.0 / (b - a))));
        r.slope = secant;
        const bool rolle = std::fabs(fb - fa) < 1e-9;
        r.rolleApplies = rolle;
        addStep(r, en, "Secant slope (f(b) - f(a)) / (b - a)",
                "شیب خط قاطع (f(b) − f(a)) / (b − a)", secant,
                en ? (rolle ? "f(a) = f(b), so Rolle's theorem applies and the promised tangent is horizontal."
                            : "The theorem says some interior tangent is parallel to this secant.")
                   : "",
                en ? ""
                   : (rolle ? "چون f(a) = f(b) است قضیهٔ رول برقرار می‌شود و مماس تضمین‌شده افقی است."
                            : "قضیه می‌گوید مماسی در داخل بازه موازی این خط قاطع است."));

        Ex target = simplify(Add({df, Mul(Num(-1), secant)}));
        addStep(r, en, "Solve f'(c) = secant slope for c", "حل معادلهٔ f′(c) = شیب قاطع برای c",
                substitute(target, x, Sym("c")));
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
            r.warnings.push_back(en
                ? "More than 16 points satisfy the conclusion; only the first 16 are shown."
                : "بیش از ۱۶ نقطه نتیجهٔ قضیه را برآورده می‌کنند؛ فقط ۱۶ نقطهٔ اول نمایش داده شده‌اند.");
        }
        if (r.points.empty()) {
            r.warnings.push_back(en
                ? "No point satisfying the conclusion was found numerically. Check that the "
                  "hypotheses really hold across the whole interval."
                : "هیچ نقطه‌ای که نتیجهٔ قضیه را برآورده کند به‌صورت عددی پیدا نشد. بررسی کنید که "
                  "فرض‌های قضیه روی تمام بازه برقرار باشند.");
        } else {
            for (const auto& c : r.points) {
                bool ok = false;
                const double cv = asDouble(c, &ok);
                bool okv = false;
                const double fv = ok ? evalAt(f, x, cv, &okv) : 0;
                const bool haveValue = ok && okv && std::isfinite(fv);
                const std::string pointText = "c = " + toPlain(c) +
                    (haveValue ? (en ? ", f(c) = " : "، f(c) = ") + numStr(fv) : "");
                r.steps.push_back({
                    en ? "The guaranteed point c where f'(c) equals the secant slope"
                       : "نقطهٔ تضمین‌شدهٔ c که در آن f′(c) برابر شیب قاطع است",
                    c,
                    en ? (pointText + ". " + (everyPoint
                          ? "The equation holds at every interior point; the midpoint is reported as a representative."
                          : "This interior point is exactly what the Mean Value Theorem promises."))
                       : (pointText + ". " + (everyPoint
                          ? "معادله در هر نقطهٔ داخلی برقرار است؛ نقطهٔ میانی به‌عنوان نماینده گزارش می‌شود."
                          : "این نقطهٔ داخلی دقیقاً همان چیزی است که قضیهٔ مقدار میانگین تضمین می‌کند."))});
            }
        }
        r.ok = true;
    } catch (const std::exception& e) {
        r.error = en ? std::string("Mean-value error: ") + e.what()
                     : std::string("خطا در قضیهٔ مقدار میانگین: ") + e.what();
    }
    return r;
}

Result cauchyMeanValue(const std::string& fIn, const std::string& gIn, const std::string& x, double a,
                       double b, const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "cauchy";
    r.typeId = "cauchy";
    r.typeFa = "قضیهٔ کوشی";
    r.variable = x;
    try {
        if (!(a < b)) {
            r.error = en ? "The interval must satisfy a < b." : "باید a < b باشد.";
            return r;
        }
        Ex f = parseODEExpr(fIn, x, nullptr, {x, "e", "pi"});
        Ex g = parseODEExpr(gIn, x, nullptr, {x, "e", "pi"});
        r.functionF = f;
        r.functionG = g;
        addStep(r, en, "Functions f and g", "توابع f و g", f,
                en ? ("g = " + toPlain(g) + " (the denominator).") : "",
                en ? "" : ("g = " + toPlain(g) + " (مخرج)."));

        bool ok = false;
        const double fa = evalConst(substitute(f, x, numEx(a)), &ok);
        const double fb = evalConst(substitute(f, x, numEx(b)), &ok);
        const double ga = evalConst(substitute(g, x, numEx(a)), &ok);
        const double gb = evalConst(substitute(g, x, numEx(b)), &ok);
        if (!std::isfinite(fa) || !std::isfinite(fb) || !std::isfinite(ga) || !std::isfinite(gb)) {
            r.error = en ? "Both functions must be defined at the endpoints."
                         : "هر دو تابع باید در دو سر بازه تعریف شده باشند.";
            return r;
        }
        if (std::fabs(gb - ga) < 1e-12) {
            r.error = en ? "g(a) = g(b), so Cauchy's theorem does not apply."
                         : "چون g(a) = g(b) است قضیهٔ کوشی برقرار نمی‌شود.";
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
        addStep(r, en, "Derivatives and the ratio of increments",
                "مشتق‌ها و نسبت تغییرات", Add({df, dg}),
                en ? ("f' = " + toPlain(df) + ", g' = " + toPlain(dg) +
                      "; the target ratio is (f(b)-f(a))/(g(b)-g(a)) = " + numStr(ratio) + ".")
                   : "",
                en ? ""
                   : ("f′ = " + toPlain(df) + "، g′ = " + toPlain(dg) +
                      "؛ نسبت هدف (f(b)−f(a))/(g(b)−g(a)) = " + numStr(ratio) + " است."));

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
            r.warnings.push_back(en
                ? "More than 16 points satisfy the conclusion; only the first 16 are shown."
                : "بیش از ۱۶ نقطه نتیجهٔ قضیه را برآورده می‌کنند؛ فقط ۱۶ نقطهٔ اول نمایش داده شده‌اند.");
        }
        if (r.points.empty()) {
            r.warnings.push_back(en ? "No such c was found numerically."
                                    : "چنین نقطه‌ای به‌صورت عددی پیدا نشد.");
        } else {
            for (const auto& c : r.points) {
                r.steps.push_back({
                    en ? "The point c guaranteed by Cauchy's theorem"
                       : "نقطهٔ c که قضیهٔ کوشی تضمین می‌کند",
                    c,
                    en ? (everyPoint
                          ? "The ratio condition holds at every interior point; the midpoint is reported as a representative."
                          : "At this point the ratio of the derivatives matches the ratio of the function increments.")
                       : (everyPoint
                          ? "شرط نسبت در هر نقطهٔ داخلی برقرار است؛ نقطهٔ میانی به‌عنوان نماینده گزارش می‌شود."
                          : "در این نقطه نسبت مشتق‌ها با نسبت تغییرات توابع برابر است.")});
            }
        }
        r.ok = true;
    } catch (const std::exception& e) {
        r.error = en ? std::string("Cauchy MVT error: ") + e.what()
                     : std::string("خطا در قضیهٔ کوشی: ") + e.what();
    }
    return r;
}

// ----------------------------------------------------------------- L'Hopital
Result lhopitalLimit(const std::string& numerator, const std::string& denominator,
                     const std::string& x, const std::string& point, const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "lhopital";
    r.typeId = "lhopital";
    r.typeFa = "قاعدهٔ هوپیتال";
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
        addStep(r, en, "Quotient f/g", "کسر f/g", quotient,
                en ? ("f = " + toPlain(N) + " and g = " + toPlain(D) + ".") : "",
                en ? "" : ("f = " + toPlain(N) + " و g = " + toPlain(D) + "."));

        std::vector<std::string> rules;
        LimitOutcome out = limitOf(quotient, x, a, 0, &rules);
        bool used = false;
        for (const auto& rule : rules) {
            if (rule == "lhopital") used = true;
        }
        if (used) {
            addStep(r, en, "The quotient is an indeterminate form, so L'Hopital's rule applies",
                    "کسر شکل مبهم دارد، پس قاعدهٔ هوپیتال اعمال می‌شود", nullptr,
                    en ? "Differentiating numerator and denominator separately is allowed only after "
                         "confirming the indeterminate form."
                       : "",
                    en ? "" : "مشتق‌گیری جداگانه از صورت و مخرج تنها پس از تأیید شکل مبهم مجاز است.");
        } else {
            addStep(r, en, "The quotient is not indeterminate here, so the limit is direct",
                    "کسر در این نقطه مبهم نیست، پس حد مستقیم است", nullptr,
                    en ? "L'Hopital would be unjustified; the value follows by substitution."
                       : "",
                    en ? "" : "استفاده از هوپیتال توجیه ندارد؛ مقدار با جای‌گذاری به دست می‌آید.");
        }

        if (out.kind == LimitOutcome::Finite) {
            r.solution = out.value;
            r.limitExists = true;
            addStep(r, en, "Limit of the quotient", "حد کسر", out.value);
        } else if (out.kind == LimitOutcome::PosInf || out.kind == LimitOutcome::NegInf) {
            r.limitInfinite = true;
            r.limitExists = true;
            r.limitSign = out.kind == LimitOutcome::PosInf ? 1 : -1;
            addStep(r, en, "The quotient diverges", "کسر واگرا می‌شود");
        } else {
            r.divergent = true;
            addStep(r, en, "The limit does not exist", "حد وجود ندارد");
        }
        r.ok = true;
    } catch (const std::exception& e) {
        r.error = en ? std::string("L'Hopital error: ") + e.what()
                     : std::string("خطا در قاعدهٔ هوپیتال: ") + e.what();
    }
    return r;
}

} // namespace ode
