// Infinit Lab — Taylor series, integral applications, partial derivatives and
// series convergence.  Each routine records every intermediate expression as a
// Step so the UI can show the reasoning, not just the final answer.
// SPDX-License-Identifier: Apache-2.0
#include "calculus_apps.hpp"

#include "calculus_ext.hpp"
#include "limits.hpp"
#include "numeric.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <sstream>

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

Ex numEx(double v) {
    if (!std::isfinite(v)) return nullptr;
    return Num(Rational::fromDouble(v));
}

// The API receives the Taylor centre as a double.  Recover the exact value
// behind common inputs (0, 1/2, 2/3, pi/4, pi/2, 2*pi, ...) so the polynomial
// prints `(x - 1/4*pi)` instead of a nine-digit rational approximation.
Ex exactCentre(double v) {
    if (!std::isfinite(v)) return nullptr;
    if (v == 0.0) return Num(0);
    const double pi = 3.14159265358979323846;
    for (int den = 1; den <= 12; ++den) {
        for (int num = -36; num <= 36; ++num) {
            if (num == 0) continue;
            const double candidate = (double)num / (double)den * pi;
            if (std::fabs(v - candidate) <= 1e-12 * (1.0 + std::fabs(candidate)))
                return Mul(Num(Rational(num, den)), Sym("pi"));
        }
    }
    for (int den = 1; den <= 64; ++den) {
        const double scaled = v * den;
        const double rounded = std::round(scaled);
        if (std::fabs(scaled - rounded) < 1e-12) {
            const long long n = (long long)std::llround(rounded);
            if (std::llabs(n) <= 1000000) return Num(Rational(n, den));
        }
    }
    return numEx(v);
}

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

double evalAt(const Ex& f, const std::string& x, double xv, bool* ok) {
    std::map<std::string, double> vars;
    vars[x] = xv;
    return evalReal(f, vars, ok);
}

double evalConst(const Ex& e, bool* ok) {
    std::map<std::string, double> vars;
    return evalReal(e, vars, ok);
}

double evalMany(const Ex& e, const std::vector<std::string>& vars,
                const std::vector<double>& values, bool* ok) {
    std::map<std::string, double> m;
    for (size_t i = 0; i < vars.size() && i < values.size(); ++i) m[vars[i]] = values[i];
    return evalReal(e, m, ok);
}

Ex atPoint(const Ex& e, const std::vector<std::string>& vars,
           const std::vector<double>& values) {
    std::map<std::string, Ex> m;
    for (size_t i = 0; i < vars.size() && i < values.size(); ++i) m[vars[i]] = numEx(values[i]);
    return simplify(substituteMany(e, m));
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

long long factorialLL(int n) {
    long long f = 1;
    for (int i = 2; i <= n; ++i) f *= i;
    return f;
}

bool sameNumber(const Ex& a, const Ex& b) {
    bool ok = false;
    const double av = evalConst(a, &ok);
    if (!ok || !std::isfinite(av)) return false;
    const double bv = evalConst(b, &ok);
    if (!ok || !std::isfinite(bv)) return false;
    return std::fabs(av - bv) < 1e-9 * (1.0 + std::fabs(av) + std::fabs(bv));
}

// ---------------------------------------------------------------- Taylor series
Result taylorSeriesImpl(const std::string& input, const std::string& x, double a, int order,
                        const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "taylor";
    r.typeId = a == 0.0 ? "maclaurin" : "taylor";
    r.typeFa = a == 0.0 ? "سری مکلورن" : "چندجمله‌ای تیلور";
    r.variable = x;
    r.application = a == 0.0 ? "maclaurin" : "taylor";
    try {
        if (order < 0) order = 0;
        if (order > 12) order = 12;
        Ex f = parseODEExpr(input, x, nullptr, {x, "e", "pi"});
        Ex center = exactCentre(a);
        r.normalized = f;
        const std::string unknown = unknownFunction(f);
        if (!unknown.empty()) {
            r.ok = false;
            r.verified = false;
            r.errorCode = "unknown_function";
            r.errorDetail = "unknown function: " + unknown;
            r.error = en ? ("Unknown function: " + unknown)
                         : ("تابع " + unknown + " ناشناخته است.");
            return r;
        }
        // E4: name any extra free symbols instead of evaluating them as 0.
        std::vector<std::string> symbols;
        collectSymbols(f, symbols);
        std::vector<std::string> extras;
        for (const auto& s : symbols) {
            if (s == x || s == "e" || s == "pi" || s.rfind("C", 0) == 0) continue;
            extras.push_back(s);
        }
        if (!extras.empty()) {
            std::string names;
            for (size_t i = 0; i < extras.size(); ++i) {
                if (i) names += ", ";
                names += extras[i];
            }
            r.warnings.push_back(
                en ? "Extra symbol(s) " + names +
                         " are free; the validation evaluates them at non-zero sample values."
                   : "نماد(های) اضافی " + names +
                         " آزاد هستند؛ در بررسی عددی با مقادیر نمونهٔ ناصفر ارزیابی می‌شوند.");
        }
        r.order = order;
        r.point = toPlain(center);
        r.value = simplify(substitute(f, x, center));
        addStep(r, en,
                a == 0.0 ? "Function and Maclaurin centre a = 0"
                         : "Function and Taylor centre a",
                a == 0.0 ? "تابع و مرکز مکلورن a = 0" : "تابع و مرکز تیلور a",
                f,
                en ? ("Expand f about a = " + toPlain(center) + " up to order " + std::to_string(order) +
                      ". Maclaurin is the special case a = 0.")
                   : ("تابع را حول a = " + toPlain(center) + " تا مرتبهٔ " + std::to_string(order) +
                      " بسط می‌دهیم. مکلورن حالت خاص a = 0 است."));

        Ex d = f;
        std::vector<Ex> coeffs;
        std::string coeffEn, coeffFa;
        for (int k = 0; k <= order; ++k) {
            Ex valueAt = simplify(substitute(d, x, center));
            // Keep the exact value (sin(1), sqrt(2), 1/3, ...) instead of
            // snapping to a rational approximation.  Combining approximations
            // of transcendental values can overflow the 64-bit rational
            // denominator when the terms are added together.
            Ex coeff = simplify(Mul(valueAt, Num(Rational(1, factorialLL(k)))));
            coeffs.push_back(coeff);
            if (k > 0) { coeffEn += ", "; coeffFa += "، "; }
            coeffEn += "c" + std::to_string(k) + " = " + toPlain(coeff);
            coeffFa += "c" + std::to_string(k) + " = " + toPlain(coeff);

            if (k == 0) {
                addStep(r, en, "Value at the centre: c0 = f(a)", "مقدار در مرکز: c0 = f(a)",
                        valueAt,
                        en ? ("f(" + toPlain(center) + ") = " + toPlain(valueAt) + ".")
                           : ("f(" + toPlain(center) + ") = " + toPlain(valueAt) + "."),
                        "");
            } else {
                const std::string label = std::to_string(k);
                addStep(r, en, "Derivative of order " + label + " at the centre",
                        "مشتق مرتبهٔ " + label + " در مرکز", d,
                        en ? ("f^(" + label + ")(" + toPlain(center) + ") = " + toPlain(valueAt) +
                              ", so c" + label + " = " + toPlain(coeff) + ".")
                           : ("f^(" + label + ")(" + toPlain(center) + ") = " + toPlain(valueAt) +
                              "، پس c" + label + " = " + toPlain(coeff) + "."),
                        "");
            }
            d = simplify(diff(d, x));
        }
        r.coefficients = coeffs;

        addStep(r, en, "Coefficients c_k = f^(k)(a) / k!", "ضرایب c_k = f^(k)(a) / k!",
                nullptr, coeffEn, coeffFa);

        std::vector<Ex> terms;
        Ex shift = Add({Sym(x), Mul(Num(-1), center)});
        for (int k = 0; k <= order; ++k) {
            if (k == 0) {
                terms.push_back(coeffs[k]);
                continue;
            }
            Ex power = Pow(shift, Num(k));
            Ex term;
            if (coeffs[k]->isOne()) term = power;
            else if (coeffs[k]->isMinusOne()) term = Mul(Num(-1), power);
            else term = Mul(coeffs[k], power);
            terms.push_back(term);
        }
        // Expand when the centre is the origin (the familiar Maclaurin form)
        // and, when possible, for other centres.  Expanding powers of a centre
        // stored as a large rational can overflow, so the unexpanded Taylor
        // form sum c_k (x - a)^k is the safe fallback.
        Ex polynomial = Add(terms);
        try {
            polynomial = simplify(polynomial);
        } catch (const OverflowError&) {
            polynomial = Add(terms);
        }
        r.polynomial = polynomial;
        r.solution = polynomial;
        addStep(r, en,
                a == 0.0 ? "Maclaurin polynomial" : "Taylor polynomial",
                a == 0.0 ? "چندجمله‌ای مکلورن" : "چندجمله‌ای تیلور",
                polynomial,
                en ? ("P" + std::to_string(order) + "(x) = " + toPlain(polynomial) + ".")
                   : ("P" + std::to_string(order) + "(x) = " + toPlain(polynomial) + "."),
                "");

        Ex nextDerivative = d;
        Ex xi = Sym("xi");
        Ex remainder = Mul(
            substitute(nextDerivative, x, xi),
            Num(Rational(1, factorialLL(order + 1))),
            Pow(shift, Num(order + 1)));
        try {
            remainder = simplify(remainder);
        } catch (const OverflowError&) {
            // Keep the product form; expanding (x - a)^(n+1) with a large
            // rational centre is not worth an overflow.
        }
        r.remainder = remainder;
        addStep(r, en, "Lagrange remainder", "باقی‌ماندهٔ لاگرانژ", remainder,
                en ? ("The error after order " + std::to_string(order) +
                      " is R" + std::to_string(order) + "(x) = f^(" +
                      std::to_string(order + 1) + ")(xi)/( " +
                      std::to_string(order + 1) + ")! (x - a)^(" +
                      std::to_string(order + 1) + "), with xi between a and x.")
                   : ("خطا پس از مرتبهٔ " + std::to_string(order) + " برابر است با R" +
                      std::to_string(order) + "(x) = f^(" + std::to_string(order + 1) +
                      ")(xi)/(" + std::to_string(order + 1) + ")! (x - a)^(" +
                      std::to_string(order + 1) + ")، که xi بین a و x است."),
                "");
        r.assumptions.push_back(en ? "xi lies between a and x."
                                   : "xi بین a و x قرار دارد.");

        // Numeric evidence: compare the polynomial with f at a few points away
        // from the centre.  Extra symbols get non-zero values so every term is
        // exercised instead of silently vanishing.
        std::vector<std::string> evalVars = {x};
        std::vector<double> extraValues;
        for (size_t i = 0; i < extras.size(); ++i) {
            evalVars.push_back(extras[i]);
            extraValues.push_back(0.7 + 0.3 * (double)i);
        }
        bool firstProbe = true;
        for (double probe : {a + 0.1, a - 0.1, a + 0.2}) {
            std::vector<double> values = {probe};
            values.insert(values.end(), extraValues.begin(), extraValues.end());
            bool okF = false, okP = false;
            const double fv = evalMany(f, evalVars, values, &okF);
            const double pv = evalMany(polynomial, evalVars, values, &okP);
            if (!okF || !okP || !std::isfinite(fv) || !std::isfinite(pv)) continue;
            const double err = std::fabs(fv - pv);
            const bool ok = err < 0.05 * (1.0 + std::fabs(fv));
            r.checks.push_back({
                en ? "Polynomial agrees with f at x = " + numStr(probe)
                   : "تطابق چندجمله‌ای با f در x = " + numStr(probe),
                ok,
                en ? ("f = " + numStr(fv) + ", P = " + numStr(pv) +
                      "; |error| = " + numStr(err) + ".")
                   : ("f = " + numStr(fv) + "، P = " + numStr(pv) +
                      "؛ |خطا| = " + numStr(err) + "."),
            });
            if (firstProbe) {
                r.hasNumeric = true;
                r.numericValue = pv;
                firstProbe = false;
            }
        }
        r.ok = true;
    } catch (const std::exception& e) {
        setFailure(r, en, e, "Taylor-series error: ", "خطا در سری تیلور: ");
    }
    return r;
}

// ------------------------------------------------------------ integral helpers
double quad(const Ex& g, const std::string& x, double a, double b) {
    return quadratureAdaptive(
        [&](double t) {
            bool ok = false;
            const double v = evalAt(g, x, t, &ok);
            return ok && std::isfinite(v) ? v : std::numeric_limits<double>::quiet_NaN();
        },
        a, b);
}

// Definite integral with an exact antiderivative when one exists.
Ex definite(const Ex& g, const std::string& x, double a, double b, bool* exactOk,
            std::string* method) {
    bool closed = false;
    std::string how;
    Ex F = integrate(g, x, &closed, &how);
    if (method) *method = how;
    if (!closed || hasUnevaluatedIntegral(F)) {
        if (exactOk) *exactOk = false;
        return nullptr;
    }
    Ex value = simplify(Add({substitute(F, x, numEx(b)),
                             Mul(Num(-1), substitute(F, x, numEx(a)))}));
    bool ok = false;
    const double v = evalConst(value, &ok);
    if (exactOk) *exactOk = ok && std::isfinite(v);
    return value;
}

// Roots of g on [a, b] by sign change and bisection.
std::vector<double> crossings(const Ex& g, const std::string& x, double a, double b) {
    std::vector<double> out;
    const int samples = 2000;
    const double h = (b - a) / samples;
    double prev = 0;
    bool havePrev = false;
    for (int i = 0; i <= samples; ++i) {
        const double t = a + h * i;
        bool ok = false;
        const double v = evalAt(g, x, t, &ok);
        if (!ok || !std::isfinite(v)) { havePrev = false; continue; }
        if (havePrev && prev * v < 0) {
            double lo = t - h, hi = t, flo = prev;
            for (int it = 0; it < 80; ++it) {
                const double mid = 0.5 * (lo + hi);
                bool mok = false;
                const double fm = evalAt(g, x, mid, &mok);
                if (!mok || !std::isfinite(fm)) break;
                if (flo * fm <= 0) hi = mid;
                else { lo = mid; flo = fm; }
            }
            out.push_back(0.5 * (lo + hi));
        }
        prev = v;
        havePrev = true;
    }
    return out;
}

Result integralApplicationImpl(const std::string& kind, const std::string& fIn,
                               const std::string& gIn, const std::string& x, double a, double b,
                               const std::string& axis, const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "integral_apps";
    r.application = kind;
    r.variable = x;
    r.lower = {numStr(a)};
    r.upper = {numStr(b)};
    r.definite = true;
    try {
        if (!(a < b)) {
            r.error = en ? "The interval must satisfy a < b." : "باید a < b باشد.";
            return r;
        }
        Ex f = parseODEExpr(fIn, x, nullptr, {x, "e", "pi"});
        Ex g = gIn.empty() ? Num(0) : parseODEExpr(gIn, x, nullptr, {x, "e", "pi"});
        r.functionF = f;
        if (!gIn.empty()) r.functionG = g;

        if (kind == "arc_length") {
            r.typeId = "arc_length";
            r.typeFa = "طول قوس";
            Ex df = simplify(diff(f, x));
            r.derivative = df;
            Ex integrand = simplify(Sqrt(Add({Num(1), Pow(df, Num(2))})));
            r.integrand = integrand;
            addStep(r, en, "Arc-length formula", "فرمول طول قوس", integrand,
                    en ? ("L = integral from " + numStr(a) + " to " + numStr(b) +
                          " of sqrt(1 + (f')^2) dx, with f' = " + toPlain(df) + ".")
                       : ("L = انتگرال از " + numStr(a) + " تا " + numStr(b) +
                          " از sqrt(1 + (f′)^2) dx، با f′ = " + toPlain(df) + "."),
                    "");
            bool exactOk = false;
            std::string method;
            Ex antiderivative = definite(integrand, x, a, b, &exactOk, &method);
            const double numeric = quad(integrand, x, a, b);
            r.solution = antiderivative;
            r.hasNumeric = std::isfinite(numeric);
            r.numericValue = numeric;
            r.value = exactOk ? antiderivative : snap(numeric);
            addStep(r, en, "Evaluate the integral", "محاسبهٔ انتگرال", r.value,
                    en ? ("The arc length is approximately " + numStr(numeric) +
                          (exactOk ? " (exact value shown)." : "."))
                       : ("طول قوس تقریباً " + numStr(numeric) +
                          (exactOk ? " است (مقدار دقیق نمایش داده شده)." : " است.")),
                    "");
            if (std::isfinite(numeric)) {
                r.checks.push_back({
                    en ? "Numeric quadrature" : "انتگرال‌گیری عددی", true,
                    en ? ("Adaptive quadrature gives " + numStr(numeric) + ".")
                       : ("انتگرال‌گیری تطبیقی مقدار " + numStr(numeric) + " را می‌دهد."),
                });
            }
            r.ok = true;
            return r;
        }

        if (kind == "volume") {
            r.typeId = "volume";
            r.typeFa = "حجم دوران";
            const bool aboutY = axis == "y";
            Ex integrand;
            if (aboutY) {
                Ex radius = Mul(Num(2), Sym("pi"), Sym(x));
                Ex height = simplify(Add(f, Mul(Num(-1), g)));
                integrand = simplify(Mul(radius, height));
                addStep(r, en, "Shell method about the y-axis", "روش پوسته حول محور y",
                        integrand,
                        en ? ("V = 2*pi * integral of x*(f - g) dx on [" + numStr(a) + ", " +
                              numStr(b) + "]. This needs 0 <= a < b.")
                           : ("V = 2π × انتگرال x(f − g) dx روی [" + numStr(a) + "، " +
                              numStr(b) + "]. این روش به 0 ≤ a < b نیاز دارد."),
                        "");
                if (a < 0) {
                    r.warnings.push_back(en
                        ? "Part of the interval is negative; the shell radius x changes sign and "
                          "the geometric interpretation is only valid for x >= 0."
                        : "بخشی از بازه منفی است؛ شعاع پوسته x علامت عوض می‌کند و تفسیر هندسی "
                          "فقط برای x ≥ 0 معتبر است.");
                }
            } else if (!gIn.empty()) {
                Ex outer = Pow(f, Num(2));
                Ex inner = Pow(g, Num(2));
                integrand = simplify(Mul(Sym("pi"), Add(outer, Mul(Num(-1), inner))));
                addStep(r, en, "Washer method about the x-axis", "روش واشر حول محور x",
                        integrand,
                        en ? ("V = pi * integral of (f^2 - g^2) dx on [" + numStr(a) + ", " +
                              numStr(b) + "], with f the outer radius and g the inner radius.")
                           : ("V = π × انتگرال (f² − g²) dx روی [" + numStr(a) + "، " +
                              numStr(b) + "]، که f شعاع بیرونی و g شعاع درونی است."),
                        "");
            } else {
                integrand = simplify(Mul(Sym("pi"), Pow(f, Num(2))));
                addStep(r, en, "Disk method about the x-axis", "روش دیسک حول محور x",
                        integrand,
                        en ? ("V = pi * integral of f^2 dx on [" + numStr(a) + ", " + numStr(b) +
                              "].")
                           : ("V = π × انتگرال f² dx روی [" + numStr(a) + "، " + numStr(b) + "]."),
                        "");
            }
            r.integrand = integrand;
            bool exactOk = false;
            std::string method;
            Ex antiderivative = definite(integrand, x, a, b, &exactOk, &method);
            const double numeric = quad(integrand, x, a, b);
            r.solution = antiderivative;
            r.hasNumeric = std::isfinite(numeric);
            r.numericValue = numeric;
            r.value = exactOk ? antiderivative : snap(numeric);
            addStep(r, en, "Volume", "حجم", r.value,
                    en ? ("V is approximately " + numStr(numeric) +
                          (exactOk ? " (exact value shown)." : "."))
                       : ("حجم تقریباً " + numStr(numeric) +
                          (exactOk ? " است (مقدار دقیق نمایش داده شده)." : " است.")),
                    "");
            if (std::isfinite(numeric)) {
                r.checks.push_back({
                    en ? "Numeric quadrature" : "انتگرال‌گیری عددی", true,
                    en ? ("Adaptive quadrature gives " + numStr(numeric) + ".")
                       : ("انتگرال‌گیری تطبیقی مقدار " + numStr(numeric) + " را می‌دهد."),
                });
            }
            r.ok = true;
            return r;
        }

        // Area between two curves.
        r.typeId = "area";
        r.typeFa = "مساحت بین دو منحنی";
        Ex diffExpr = simplify(Add(f, Mul(Num(-1), g)));
        r.integrand = diffExpr;
        addStep(r, en, "Difference of the curves", "تفاضل دو منحنی", diffExpr,
                en ? ("Area = integral of |f - g| dx on [" + numStr(a) + ", " + numStr(b) +
                      "]. The upper curve is chosen on each subinterval.")
                   : ("مساحت = انتگرال |f − g| dx روی [" + numStr(a) + "، " + numStr(b) +
                      "]. روی هر زیربازه منحنی بالایی انتخاب می‌شود."),
                "");
        std::vector<double> cuts = {a};
        for (double c : crossings(diffExpr, x, a, b)) cuts.push_back(c);
        cuts.push_back(b);
        std::sort(cuts.begin(), cuts.end());
        cuts.erase(std::unique(cuts.begin(), cuts.end(),
                               [](double p, double q) { return std::fabs(p - q) < 1e-9; }),
                   cuts.end());
        double total = 0.0;
        Ex exactTotal = Num(0);
        bool allExact = true;
        for (size_t i = 0; i + 1 < cuts.size(); ++i) {
            const double lo = cuts[i], hi = cuts[i + 1];
            if (hi - lo < 1e-9) continue;
            bool ok = false;
            const double mid = 0.5 * (lo + hi);
            const double sign = evalAt(diffExpr, x, mid, &ok);
            if (!ok || !std::isfinite(sign) || std::fabs(sign) < 1e-12) continue;
            const Ex piece = sign >= 0 ? diffExpr : simplify(Mul(Num(-1), diffExpr));
            bool exactOk = false;
            std::string method;
            Ex antiderivative = definite(piece, x, lo, hi, &exactOk, &method);
            const double numeric = quad(piece, x, lo, hi);
            total += numeric;
            if (exactOk && antiderivative) exactTotal = Add(exactTotal, antiderivative);
            else allExact = false;
            addStep(r, en,
                    "Piece " + std::to_string(i + 1) + " on [" + numStr(lo) + ", " + numStr(hi) +
                        "]",
                    "بخش " + std::to_string(i + 1) + " روی [" + numStr(lo) + "، " + numStr(hi) +
                        "]",
                    piece,
                    en ? ((sign >= 0 ? "f is above g" : "g is above f") +
                          std::string("; the integral on this piece is ") + numStr(numeric) + ".")
                       : ((sign >= 0 ? "f بالای g است" : "g بالای f است") +
                          std::string("؛ انتگرال این بخش ") + numStr(numeric) + " است."));
        }
        r.hasNumeric = std::isfinite(total);
        r.numericValue = total;
        r.value = allExact ? simplify(exactTotal) : snap(total);
        r.solution = r.value;
        addStep(r, en, "Total area", "مساحت کل", r.value,
                en ? ("The area is approximately " + numStr(total) +
                      (allExact ? " (exact value shown)." : "."))
                   : ("مساحت تقریباً " + numStr(total) +
                      (allExact ? " است (مقدار دقیق نمایش داده شده)." : " است.")),
                "");
        if (std::isfinite(total)) {
            r.checks.push_back({
                en ? "Numeric quadrature" : "انتگرال‌گیری عددی", true,
                en ? ("Adaptive quadrature gives " + numStr(total) + ".")
                   : ("انتگرال‌گیری تطبیقی مقدار " + numStr(total) + " را می‌دهد."),
            });
        }
        r.ok = true;
    } catch (const std::exception& e) {
        r.error = en ? std::string("Integral-application error: ") + e.what()
                     : std::string("خطا در کاربرد انتگرال: ") + e.what();
    }
    return r;
}

// ------------------------------------------------------------ partial derivatives
Result partialDerivativesImpl(const std::string& input, const std::vector<std::string>& varsIn,
                              const std::vector<double>& point,
                              const std::vector<double>& direction, const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "partial";
    r.typeId = "partial";
    r.typeFa = "مشتق جزئی و گرادیان";
    try {
        std::vector<std::string> vars = varsIn;
        if (vars.empty()) vars = {"x", "y"};
        if (vars.size() > 3) vars.resize(3);
        std::vector<std::string> allowed = vars;
        allowed.push_back("e");
        allowed.push_back("pi");
        Ex f = parseODEExpr(input, vars[0], nullptr, allowed);
        r.normalized = f;
        r.vars = vars;
        r.variable = vars[0];
        r.functionF = f;

        addStep(r, en, "Function and variables", "تابع و متغیرها", f,
                en ? ("f depends on " + [&] {
                          std::string s;
                          for (size_t i = 0; i < vars.size(); ++i) {
                              if (i) s += ", ";
                              s += vars[i];
                          }
                          return s;
                      }() + ".")
                   : ("f به متغیرهای " + [&] {
                          std::string s;
                          for (size_t i = 0; i < vars.size(); ++i) {
                              if (i) s += "، ";
                              s += vars[i];
                          }
                          return s;
                      }() + " وابسته است."),
                "");

        std::vector<Ex> first;
        std::string firstEn, firstFa;
        for (const auto& v : vars) {
            Ex d = simplify(diff(f, v));
            first.push_back(d);
            PartialView pv;
            pv.label = "f_" + v;
            pv.value = d;
            r.partials.push_back(pv);
            if (!firstEn.empty()) { firstEn += ", "; firstFa += "، "; }
            firstEn += "f_" + v + " = " + toPlain(d);
            firstFa += "f_" + v + " = " + toPlain(d);
            addStep(r, en, "First partial derivative with respect to " + v,
                    "مشتق جزئی مرتبهٔ اول نسبت به " + v, d,
                    en ? ("f_" + v + " = " + toPlain(d) + ".") : "",
                    en ? "" : ("f_" + v + " = " + toPlain(d) + "."));
        }
        r.derivative = first.front();

        // Second partials (including the mixed ones).
        std::vector<Ex> diagonal;
        for (size_t i = 0; i < vars.size(); ++i) {
            Ex d = simplify(diff(first[i], vars[i]));
            diagonal.push_back(d);
            PartialView pv;
            pv.label = "f_" + vars[i] + vars[i];
            pv.value = d;
            r.partials.push_back(pv);
            addStep(r, en, "Second partial derivative in " + vars[i],
                    "مشتق جزئی مرتبهٔ دوم نسبت به " + vars[i], d,
                    en ? ("f_" + vars[i] + vars[i] + " = " + toPlain(d) + ".") : "",
                    en ? "" : ("f_" + vars[i] + vars[i] + " = " + toPlain(d) + "."));
        }
        for (size_t i = 0; i < vars.size(); ++i) {
            for (size_t j = i + 1; j < vars.size(); ++j) {
                Ex mixed = simplify(diff(first[i], vars[j]));
                Ex mixedOther = simplify(diff(first[j], vars[i]));
                PartialView pv;
                pv.label = "f_" + vars[i] + vars[j];
                pv.value = mixed;
                r.partials.push_back(pv);
                addStep(r, en, "Mixed partial derivative f_" + vars[i] + vars[j],
                        "مشتق جزئی مخلوط f_" + vars[i] + vars[j], mixed,
                        en ? ("Differentiating f_" + vars[i] + " with respect to " + vars[j] +
                              " gives " + toPlain(mixed) + "; f_" + vars[j] + vars[i] + " = " +
                              toPlain(mixedOther) + ".")
                           : ("مشتق f_" + vars[i] + " نسبت به " + vars[j] + " برابر " +
                              toPlain(mixed) + " است؛ f_" + vars[j] + vars[i] + " = " +
                              toPlain(mixedOther) + "."),
                        "");
                const bool equal = sameNumber(
                    simplify(Add({mixed, Mul(Num(-1), mixedOther)})), Num(0));
                r.checks.push_back({
                    en ? "Clairaut symmetry for " + vars[i] + "," + vars[j]
                       : "تقارن کلرو برای " + vars[i] + "، " + vars[j],
                    equal,
                    en ? (equal ? "The two mixed partials agree."
                                : "The two mixed partials differ; the function may not be C2 there.")
                       : (equal ? "دو مشتق مخلوط برابرند."
                                : "دو مشتق مخلوط متفاوت‌اند؛ تابع ممکن است در آن ناحیه C2 نباشد."),
                });
            }
        }
        r.secondDerivative = diagonal.front();

        if (point.size() == vars.size()) {
            r.pointLabel = "(";
            for (size_t i = 0; i < vars.size(); ++i) {
                if (i) r.pointLabel += ", ";
                r.pointLabel += numStr(point[i]);
            }
            r.pointLabel += ")";
            r.point = r.pointLabel;
            Ex evaluated = atPoint(f, vars, point);
            r.functionValue = evaluated;
            bool ok = false;
            const double fv = evalMany(f, vars, point, &ok);
            if (ok && std::isfinite(fv)) r.value = snap(fv);
            std::string gradEn, gradFa;
            for (size_t i = 0; i < first.size(); ++i) {
                Ex value = atPoint(first[i], vars, point);
                r.gradient.push_back(value);
                if (i) { gradEn += ", "; gradFa += "، "; }
                gradEn += "f_" + vars[i] + r.pointLabel + " = " + toPlain(value);
                gradFa += "f_" + vars[i] + r.pointLabel + " = " + toPlain(value);
            }
            addStep(r, en, "Gradient at the point", "گرادیان در نقطه", evaluated,
                    en ? ("f" + r.pointLabel + " = " + toPlain(evaluated) + "; " + gradEn + ".")
                       : ("f" + r.pointLabel + " = " + toPlain(evaluated) + "؛ " + gradEn + "."),
                    "");
            Ex lap = Num(0);
            for (const auto& d : diagonal) lap = Add(lap, atPoint(d, vars, point));
            r.laplacian = simplify(lap);
            addStep(r, en, "Laplacian at the point", "لاپلاسیان در نقطه", r.laplacian,
                    en ? ("The Laplacian is the sum of the pure second partials: " +
                          toPlain(r.laplacian) + ".")
                       : ("لاپلاسیان مجموع مشتق‌های دوم خالص است: " + toPlain(r.laplacian) + "."),
                    "");

            if (direction.size() == vars.size()) {
                double norm = 0;
                for (double d : direction) norm += d * d;
                norm = std::sqrt(norm);
                if (norm > 1e-12) {
                    std::vector<double> unit;
                    for (double d : direction) unit.push_back(d / norm);
                    Ex dot = Num(0);
                    Ex normSquared = Num(0);
                    for (size_t i = 0; i < unit.size(); ++i) {
                        dot = Add(dot, Mul(r.gradient[i], snap(direction[i])));
                        normSquared = Add(normSquared, Pow(snap(direction[i]), Num(2)));
                    }
                    r.directional = simplify(
                        Mul(dot, Pow(Sqrt(normSquared), Num(-1))));
                    std::string unitEn, unitFa;
                    for (size_t i = 0; i < unit.size(); ++i) {
                        if (i) { unitEn += ", "; unitFa += "، "; }
                        unitEn += numStr(unit[i]);
                        unitFa += numStr(unit[i]);
                    }
                    addStep(r, en, "Directional derivative", "مشتق جهت‌دار", r.directional,
                            en ? ("The unit direction is (" + unitEn +
                                  "); D_u f = grad f . u = " + toPlain(r.directional) + ".")
                               : ("جهت یکه (" + unitFa + ") است؛ D_u f = گرادیان · u = " +
                                  toPlain(r.directional) + "."),
                            "");
                    bool dok = false;
                    const double dv = evalConst(r.directional, &dok);
                    if (dok && std::isfinite(dv)) {
                        r.hasNumeric = true;
                        r.numericValue = dv;
                    }
                } else {
                    r.warnings.push_back(en ? "The direction vector is zero, so it has no unit vector."
                                            : "بردار جهت صفر است، پس بردار یکه ندارد.");
                }
            }
        } else if (!point.empty()) {
            r.warnings.push_back(en
                ? "The point must supply one value per variable; the gradient was not evaluated."
                : "نقطه باید برای هر متغیر یک مقدار داشته باشد؛ گرادیان محاسبه نشد.");
        }
        r.ok = true;
    } catch (const std::exception& e) {
        r.error = en ? std::string("Partial-derivative error: ") + e.what()
                     : std::string("خطا در مشتق جزئی: ") + e.what();
    }
    return r;
}

// ---------------------------------------------------------------- series
bool containsAlternating(const Ex& a) {
    if (!a) return false;
    if (a->isPow() && a->arg(0)->isNum(-1)) return true;
    if (a->isMul()) {
        for (const auto& f : a->args)
            if (containsAlternating(f)) return true;
    }
    return false;
}

// True when a = c / (m*n + b)^p with c and m independent of n.  The exponent
// p alone decides convergence.  `pure` is false when b is non-zero: the term is
// still a p-series, but a shifted one, so no Basel-type exact sum applies.
bool pSeriesExponent(const Ex& a, const std::string& n, double* p, Ex* coeff, bool* pure) {
    Ex scale = Num(1);
    bool shifted = false;
    auto powerExp = [&](const Ex& e, double* out) -> bool {
        if (!e) return false;
        Ex base = e;
        Ex exponent = Num(1);
        if (e->isPow()) {
            base = e->arg(0);
            exponent = e->arg(1);
        }
        if (dependsOn(exponent, n)) return false;
        bool expOk = false;
        const double ev = evalConst(exponent, &expOk);
        if (!expOk || !std::isfinite(ev) || ev >= 0) return false;
        // The base must be linear: m*n + b with m, b constant and m != 0.
        std::vector<Ex> cs;
        if (!polyCoeffs(base, n, cs) || cs.size() != 2) return false;
        if (dependsOn(cs[0], n) || dependsOn(cs[1], n)) return false;
        bool mOk = false;
        const double m = evalConst(cs[1], &mOk);
        if (!mOk || std::fabs(m) < 1e-12) return false;
        if (!cs[0]->isZero()) shifted = true;
        *out = -ev;
        // (m*n + b)^exponent contributes m^exponent to the constant factor
        // only when b = 0; for a shifted base the factor is not a pure power
        // of n, so the exact-sum shortcut is disabled by `pure`.
        if (!shifted) scale = Mul(scale, Pow(cs[1], exponent));
        return true;
    };
    auto finish = [&](const Ex& c) -> bool {
        if (dependsOn(c, n)) return false;
        *coeff = simplify(Mul(scale, c));
        *pure = !shifted;
        return true;
    };
    if (a->isPow()) {
        double e = 0;
        if (powerExp(a, &e)) {
            *p = e;
            return finish(Num(1));
        }
        return false;
    }
    if (a->isMul()) {
        double e = 0;
        Ex c = Num(1);
        bool found = false;
        for (const auto& f : a->args) {
            if (powerExp(f, &e)) {
                *p = e;
                found = true;
            } else {
                c = Mul(c, f);
            }
        }
        if (found) return finish(c);
    }
    return false;
}

// a^e * b^(-e) = (a/b)^e.  The limit engine understands (n/(n+1))^n but not
// n^n/(n+1)^n, so the ratio test folds opposite powers before taking the
// limit.  This is what lets the factorial terms (n!/n^n, 2^n/n!, ...) reach
// the ratio test instead of the numeric fallback.
Ex combineOppositePowers(const Ex& ratio) {
    if (!ratio || !ratio->isMul()) return ratio;
    std::vector<Ex> fs(ratio->args.begin(), ratio->args.end());
    for (size_t i = 0; i < fs.size(); ++i) {
        if (!fs[i] || !fs[i]->isPow()) continue;
        const Ex bi = fs[i]->args[0];
        const Ex ei = fs[i]->args[1];
        for (size_t j = i + 1; j < fs.size(); ++j) {
            if (!fs[j] || !fs[j]->isPow()) continue;
            const Ex bj = fs[j]->args[0];
            const Ex ej = fs[j]->args[1];
            if (!Add(ei, ej)->isZero()) continue;
            fs[i] = Pow(Mul(bi, Pow(bj, Num(-1))), ei);
            fs.erase(fs.begin() + static_cast<long>(j));
            --j;
        }
    }
    return Mul(fs);
}

Result seriesAnalysisImpl(const std::string& input, const std::string& n, int start,
                          const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "series";
    r.typeId = "series";
    r.typeFa = "سری و همگرایی";
    r.variable = n;
    try {
        if (start < 0) start = 0;
        Ex a = simplify(parseODEExpr(input, n, nullptr, {n, "e", "pi"}));
        r.normalized = a;
        r.lower = {std::to_string(start)};
        r.upper = {"inf"};
        addStep(r, en, "Series term", "جملهٔ سری", a,
                en ? ("Study the convergence of the sum from n = " + std::to_string(start) +
                      " to infinity of a_n, where a_n = " + toPlain(a) + ".")
                   : ("همگرایی مجموع از n = " + std::to_string(start) + " تا بی‌نهایت از a_n را "
                      "بررسی می‌کنیم، که a_n = " + toPlain(a) + "."),
                "");

        // A term that is undefined at the first index makes every partial sum
        // meaningless, so reject it with a concrete starting point instead of
        // reporting a fake zero sum.
        bool firstOk = false;
        const double firstValue = evalAt(a, n, (double)start, &firstOk);
        if (!firstOk || !std::isfinite(firstValue)) {
            int suggestion = -1;
            for (int k = start + 1; k <= start + 200; ++k) {
                bool ok = false;
                const double v = evalAt(a, n, (double)k, &ok);
                if (ok && std::isfinite(v)) { suggestion = k; break; }
            }
            std::string msg = en
                ? "The term is undefined at n = " + std::to_string(start) + "."
                : "جمله در n = " + std::to_string(start) + " تعریف نشده است.";
            if (suggestion >= 0) {
                msg += en ? " Start at n = " + std::to_string(suggestion) + " instead."
                          : " به‌جای آن از n = " + std::to_string(suggestion) + " شروع کنید.";
            }
            r.error = msg;
            return r;
        }

        // Numeric partial sum and term behaviour.
        double sum = 0.0, last = 0.0;
        bool numericOk = true;
        r.partialSums.reserve(400);
        for (int k = start; k < start + 400; ++k) {
            bool ok = false;
            const double v = evalAt(a, n, (double)k, &ok);
            if (!ok || !std::isfinite(v)) { numericOk = false; break; }
            sum += v;
            last = v;
            if (std::fabs(sum) > 1e14) { numericOk = false; break; }
            r.partialSums.push_back(sum);
        }
        r.partialSum = sum;
        r.hasNumeric = numericOk;
        r.numericValue = sum;
        addStep(r, en, "Partial sums", "مجموع‌های جزئی", snap(sum),
                en ? ("After 400 terms the partial sum is " + numStr(sum) +
                      " and the last term is " + numStr(last) + ".")
                   : ("پس از ۴۰۰ جمله، مجموع جزئی " + numStr(sum) + " و جملهٔ آخر " +
                      numStr(last) + " است."),
                "");

        // Geometric test: a_(n+1)/a_n is a constant independent of n.
        Ex ratio = simplify(Mul(substitute(a, n, Add(Sym(n), Num(1))), Pow(a, Num(-1))));
        Ex ratioForLimit = combineOppositePowers(ratio);
        bool ratioConstant = !dependsOn(ratio, n);
        bool okRatio = false;
        const double rv = ratioConstant ? evalConst(ratio, &okRatio) : 0.0;
        bool decided = false;
        if (ratioConstant && okRatio && std::isfinite(rv)) {
            r.seriesTest = "geometric";
            r.converges = std::fabs(rv) < 1.0;
            decided = true;
            Ex firstTerm = simplify(substitute(a, n, Num(start)));
            if (r.converges) {
                r.seriesSum = simplify(Mul(firstTerm, Pow(Add(Num(1), Mul(Num(-1), snap(rv))), Num(-1))));
            }
            addStep(r, en, "Geometric ratio", "نسبت هندسی", snap(rv),
                    en ? ("a_(n+1)/a_n = " + numStr(rv) + ", independent of n, so the series is "
                          "geometric. It converges exactly when |r| < 1.")
                       : ("a_(n+1)/a_n = " + numStr(rv) + " و مستقل از n است، پس سری هندسی است. "
                          "دقیقاً وقتی |r| < 1 همگرا می‌شود."),
                    "");
        }

        // Alternating series test.  A sign pattern alone is not enough: the
        // magnitudes must decrease to zero, which is checked numerically.
        if (!decided && containsAlternating(a)) {
            double previous = std::numeric_limits<double>::infinity();
            bool decreasing = true;
            bool sampled = true;
            for (int k = start; k < start + 5000; ++k) {
                bool ok = false;
                const double v = evalAt(a, n, (double)k, &ok);
                if (!ok || !std::isfinite(v)) { sampled = false; break; }
                const double magnitude = std::fabs(v);
                if (magnitude > previous + 1e-12) decreasing = false;
                previous = magnitude;
            }
            const bool tendsToZero = sampled && previous < 0.05;
            if (decreasing && tendsToZero) {
                r.seriesTest = "alternating";
                r.converges = true;
                decided = true;
                addStep(r, en, "Alternating-series test", "آزمون سری متناوب", a,
                        en ? ("The term alternates in sign and |a_n| decreases to 0, so "
                              "Leibniz's test guarantees convergence (possibly conditional).")
                           : ("جمله علامت متناوب دارد و |a_n| نزولی به صفر می‌رود، پس آزمون "
                              "لایب‌نیتس همگرایی (احتمالاً شرطی) را تضمین می‌کند."),
                        "");
            }
        }

        // p-series: a_n = c / (m*n + b)^p.  Only the exponent p matters for
        // convergence; a shifted base (b != 0) has no Basel-type exact sum.
        if (!decided) {
            double p = 0;
            Ex coeff = nullptr;
            bool pure = true;
            if (pSeriesExponent(a, n, &p, &coeff, &pure)) {
                r.seriesTest = "p-series";
                r.converges = p > 1.0;
                decided = true;
                addStep(r, en, "p-series", "سری p", numEx(p),
                        en ? ("The term behaves like a constant times 1/n^p with p = " +
                              numStr(p) + ". A p-series converges exactly when p > 1.")
                           : ("جمله مانند ضریبی از 1/n^p با p = " + numStr(p) +
                              " رفتار می‌کند. سری p دقیقاً وقتی p > 1 همگرا می‌شود."),
                        "");
                if (pure && r.converges && std::fabs(p - 2.0) < 1e-9) {
                    Ex known = simplify(Mul(Sym("pi"), Sym("pi"), Num(Rational(1, 6))));
                    bool cok = false;
                    const double cv = evalConst(coeff, &cok);
                    if (cok && std::isfinite(cv)) {
                        r.seriesSum = simplify(Mul(snap(cv), known));
                        addStep(r, en, "Exact sum", "مجموع دقیق", r.seriesSum,
                                en ? "The Basel sum sum 1/n^2 = pi^2/6 is used here."
                                   : "از مجموع بازل، sum 1/n^2 = π²/6، استفاده شده است.",
                                "");
                    }
                }
            }
        }

        // Ratio test with an actual limit.
        if (!decided && ratioConstant && okRatio && std::fabs(rv - 1.0) < 1e-9) {
            // fall through to the root/alternating/numeric checks
        }
        if (!decided) {
            Ex absRatio = Fn("abs", {ratioForLimit});
            LimitOutcome lim = limitOf(absRatio, n, infinityPoint(1), 0);
            if (lim.kind == LimitOutcome::PosInf) {
                r.seriesTest = "ratio";
                r.converges = false;
                decided = true;
                addStep(r, en, "Ratio test", "آزمون نسبت", nullptr,
                        en ? "lim |a_(n+1)/a_n| grows without bound, so the series diverges."
                           : "حد |a_(n+1)/a_n| بی‌کران رشد می‌کند، پس سری واگرا می‌شود.",
                        "");
            }
            bool limitOk = lim.kind == LimitOutcome::Finite && lim.value;
            double lv = 0;
            if (limitOk) {
                bool ok = false;
                lv = evalConst(lim.value, &ok);
                limitOk = ok && std::isfinite(lv);
            }
            if (limitOk) {
                r.seriesTest = "ratio";
                if (lv < 1.0 - 1e-9) { r.converges = true; decided = true; }
                else if (lv > 1.0 + 1e-9) { r.converges = false; decided = true; }
                addStep(r, en, "Ratio test", "آزمون نسبت", lim.value,
                        en ? ("lim |a_(n+1)/a_n| = " + numStr(lv) +
                              (decided ? (r.converges ? "; smaller than 1, so the series converges."
                                                      : "; larger than 1, so the series diverges.")
                                       : "; equal to 1, so this test is inconclusive."))
                           : ("حد |a_(n+1)/a_n| = " + numStr(lv) +
                              (decided ? (r.converges ? "؛ کوچک‌تر از ۱ است، پس سری همگرا می‌شود."
                                                      : "؛ بزرگ‌تر از ۱ است، پس سری واگرا می‌شود.")
                                       : "؛ برابر ۱ است، پس این آزمون بی‌نتیجه است.")),
                        "");
            }
        }

        // Root test: L = lim |a_n|^(1/n).  It decides some terms whose ratio
        // limit is 1, and it is attempted only when the limit engine returns a
        // concrete value.
        if (!decided) {
            Ex rootExpr = Pow(Fn("abs", {a}), Pow(Sym(n), Num(-1)));
            LimitOutcome lim = limitOf(rootExpr, n, infinityPoint(1), 0);
            bool rootOk = lim.kind == LimitOutcome::Finite && lim.value;
            double lv = 0;
            if (rootOk) {
                bool ok = false;
                lv = evalConst(lim.value, &ok);
                rootOk = ok && std::isfinite(lv);
            }
            if (rootOk) {
                r.seriesTest = "root";
                if (lv < 1.0 - 1e-9) { r.converges = true; decided = true; }
                else if (lv > 1.0 + 1e-9) { r.converges = false; decided = true; }
                addStep(r, en, "Root test", "آزمون ریشه", lim.value,
                        en ? ("lim |a_n|^(1/n) = " + numStr(lv) +
                              (decided ? (r.converges ? "; smaller than 1, so the series converges."
                                                      : "; larger than 1, so the series diverges.")
                                       : "; equal to 1, so this test is inconclusive."))
                           : ("حد |a_n|^(1/n) = " + numStr(lv) +
                              (decided ? (r.converges ? "؛ کوچک‌تر از ۱ است، پس سری همگرا می‌شود."
                                                      : "؛ بزرگ‌تر از ۱ است، پس سری واگرا می‌شود.")
                                       : "؛ برابر ۱ است، پس این آزمون بی‌نتیجه است.")),
                        "");
            }
        }

        // Numeric fallback: does the tail shrink and the partial sum settle?
        if (!decided) {
            bool firstOk = false;
            const double firstValue = evalAt(a, n, (double)start, &firstOk);
            const bool termsGrow = numericOk && firstOk && std::isfinite(firstValue) &&
                                   std::fabs(last) > 1e-2 &&
                                   std::fabs(last) > 1.01 * std::fabs(firstValue);
            if (termsGrow) {
                r.seriesTest = "nth-term";
                r.converges = false;
                addStep(r, en, "nth-term test", "آزمون جملهٔ nام", snap(last),
                        en ? ("The terms do not approach 0 (the last sampled term is " +
                              numStr(last) + "), so the series diverges.")
                           : ("جمله‌ها به صفر نزدیک نمی‌شوند (جملهٔ نمونهٔ آخر " + numStr(last) +
                              " است)، پس سری واگرا می‌شود."),
                        "");
            } else {
                r.seriesTest = "numeric";
                r.converges = numericOk && std::fabs(last) < 1e-3 &&
                              std::isfinite(sum);
                addStep(r, en, "Numeric evidence", "شواهد عددی", snap(sum),
                        en ? ("No elementary test decided the series. The last term is " +
                              numStr(last) + " and the partial sum is " + numStr(sum) +
                              "; this is evidence, not a proof.")
                           : ("هیچ آزمون مقدماتی دربارهٔ سری تصمیم نگرفت. جملهٔ آخر " + numStr(last) +
                              " و مجموع جزئی " + numStr(sum) + " است؛ این شواهد است، نه اثبات."),
                        "");
            }
        }

        r.seriesVerdict = r.converges ? "converges" : "diverges";
        if (r.seriesTest == "numeric") r.seriesVerdict = "inconclusive";
        if (r.seriesVerdict == "inconclusive") {
            addStep(r, en, "Conclusion", "نتیجه", snap(r.partialSum),
                    en ? "No elementary test decided this series. The numeric partial sums are "
                         "evidence only, so the verdict stays inconclusive."
                       : "هیچ آزمون مقدماتی دربارهٔ این سری تصمیم نگرفت. مجموع‌های جزئی عددی "
                         "فقط شواهد هستند، پس نتیجه بی‌نتیجه می‌ماند.",
                    "");
        } else {
            addStep(r, en, "Conclusion", "نتیجه", r.seriesSum,
                    en ? ("The series " + std::string(r.converges ? "converges" : "diverges") +
                          " by the " + r.seriesTest + " test.")
                       : ("سری بر پایهٔ آزمون " + r.seriesTest + " " +
                          std::string(r.converges ? "همگرا" : "واگرا") + " است."),
                    "");
        }
        if (r.seriesSum) {
            r.solution = r.seriesSum;
            addStep(r, en, "Exact sum", "مجموع دقیق", r.seriesSum);
        }
        r.ok = true;
    } catch (const std::exception& e) {
        r.error = en ? std::string("Series error: ") + e.what()
                     : std::string("خطا در سری: ") + e.what();
    }
    return r;
}

} // namespace

Result taylorSeries(const std::string& input, const std::string& x, double a, int order,
                    const std::string& lang) {
    return taylorSeriesImpl(input, x, a, order, lang);
}

Result integralApplication(const std::string& kind, const std::string& fIn,
                           const std::string& gIn, const std::string& x, double a, double b,
                           const std::string& axis, const std::string& lang) {
    return integralApplicationImpl(kind, fIn, gIn, x, a, b, axis, lang);
}

Result partialDerivatives(const std::string& input, const std::vector<std::string>& vars,
                          const std::vector<double>& point,
                          const std::vector<double>& direction, const std::string& lang) {
    return partialDerivativesImpl(input, vars, point, direction, lang);
}

Result seriesAnalysis(const std::string& input, const std::string& n, int start,
                      const std::string& lang) {
    return seriesAnalysisImpl(input, n, start, lang);
}

} // namespace ode
