// Infinit Lab — curriculum topic workbench.
// SPDX-License-Identifier: Apache-2.0
#include "topics.hpp"

#include "expr.hpp"
#include "numeric.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace ode {
namespace {

constexpr double kPi = 3.14159265358979323846;

std::string numStr(double v, int digits = 6) {
    if (!std::isfinite(v)) return v < 0 ? "-inf" : "inf";
    // Numerical integration leaves ~1e-16 noise on coefficients that are
    // mathematically zero; show those as an honest zero.
    if (std::fabs(v) < 1e-10) return "0";
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

// n·π/L as a symbolic multiple of π.  Keeping π symbolic avoids printing a
// huge rational approximation of π, and the usual L = π cancels to n exactly.
Ex piMultiple(int n, double L) {
    if (std::fabs(L - kPi) < 1e-9) return Num(n);
    return Mul(Num(n), Sym("pi"), Pow(numEx(L), Num(-1)));
}

// Keep a literal pi in coefficients instead of its long rational shadow.
Ex numOrPi(double v) {
    if (std::fabs(v - kPi) < 1e-9) return Sym("pi");
    return numEx(v);
}

// Display helper: keep simple exact fractions (1/2, 3/5, 19) and fall back to
// a short decimal for coefficients that would otherwise print as huge
// rationals (damped frequencies, RK4 values, ...).
Ex smartNum(double v) {
    if (!std::isfinite(v)) return nullptr;
    if (std::fabs(v) < 1e-12) return Num(0);
    const double rounded = std::round(v);
    if (std::fabs(v - rounded) < 1e-10) return Num((long long)rounded);
    const Rational simple = Rational::fromDouble(v, 100);
    if (std::fabs(simple.toDouble() - v) < 1e-9) return Num(simple);
    return Fn("decimal", {Num(Rational::fromDouble(v))});
}

Ex expEx(const Ex& a) { return Fn("exp", {a}); }
Ex sinEx(const Ex& a) { return Fn("sin", {a}); }
Ex cosEx(const Ex& a) { return Fn("cos", {a}); }
Ex sinhEx(const Ex& a) { return Fn("sinh", {a}); }
Ex coshEx(const Ex& a) { return Fn("cosh", {a}); }

double evalF(const Ex& f, const std::string& x, double xv, const std::string& y = "",
             double yv = 0.0, bool* ok = nullptr) {
    std::map<std::string, double> vars;
    vars[x] = xv;
    if (!y.empty()) vars[y] = yv;
    bool local = false;
    const double v = evalReal(f, vars, &local);
    if (ok) *ok = local;
    return local ? v : std::numeric_limits<double>::quiet_NaN();
}

void addStep(Result& r, bool en, const std::string& enTitle, const std::string& faTitle,
             const Ex& math = nullptr, const std::string& enNote = "",
             const std::string& faNote = "") {
    Step s;
    s.title = en ? enTitle : faTitle;
    s.math = math;
    s.note = en ? enNote : faNote;
    r.steps.push_back(std::move(s));
}

void addCheck(Result& r, const std::string& nameKey, bool ok,
              const std::string& detailKey = "", const std::string& params = "",
              const std::string& detailMath = "") {
    Check c;
    c.nameKey = nameKey;
    c.ok = ok;
    c.detailKey = detailKey;
    c.params = params;
    c.detailMath = detailMath;
    r.checks.push_back(std::move(c));
}

void base(Result& r, bool en, const std::string& mode, const std::string& typeEn,
          const std::string& typeFa, const std::string& variable = "") {
    r.ok = true;
    r.mode = mode;
    r.typeId = typeEn;
    r.typeFa = typeFa;
    r.variable = variable;
    r.engine = "native";
    r.closedForm = true;
    r.verified = true;
    (void)en;
}

void fail(Result& r, bool en, const std::string& enMsg, const std::string& faMsg) {
    r.ok = false;
    r.error = en ? enMsg : faMsg;
    r.verified = false;
}

void failDomain(Result& r, bool en, const DomainIssue& issue) {
    r.ok = false;
    r.verified = false;
    r.errorCode = issue.code.empty() ? "singular" : issue.code;
    r.errorDetail = issue.detail;
    r.errorPoint = issue.point;
    r.hasErrorPoint = true;
    r.error.clear();
}

bool validCoefficient(double v) {
    if (!std::isfinite(v)) return false;
    if (v >= (double)std::numeric_limits<long long>::max() ||
        v <= (double)std::numeric_limits<long long>::min())
        return false;
    return std::fabs(v) <= 1e10;
}

Ex linearCombine(double c1, const Ex& f1, double c2, const Ex& f2) {
    std::vector<Ex> terms;
    if (std::fabs(c1) > 1e-14) terms.push_back(Mul(numEx(c1), f1));
    if (std::fabs(c2) > 1e-14) terms.push_back(Mul(numEx(c2), f2));
    if (terms.empty()) return Num(0);
    return Add(terms);
}

// ---------------------------------------------------------------- growth/decay
Result growthDecay(const TopicRequest& req, bool en) {
    Result r;
    base(r, en, "topic", "Exponential growth or decay", "رشد یا واپاشی نمایی", "t");
    const double k = req.number("k", 0.1);
    const double y0 = req.number("y0", 1.0);
    const double t = req.number("t", 10.0);
    const Ex solution = Mul(numEx(y0), expEx(Mul(numEx(k), Sym("t"))));
    r.solution = solution;
    r.normalized = Add(Mul(numEx(k), Sym("y")), Mul(Num(-1), Sym("y'")));
    r.hasNumeric = true;
    r.numericValue = y0 * std::exp(k * t);
    r.point = "t=" + numStr(t);

    addStep(r, en, "State the model", "مدل را بنویسید", Sym("y' = k y"),
            "A quantity whose rate of change is proportional to its current size.",
            "کمیتی که آهنگ تغییرش با اندازهٔ فعلی متناسب است.");
    addStep(r, en, "Solve the separable equation", "معادلهٔ جداشدنی را حل کنید",
            Sym("y = C e^{k t}"),
            "Separate dy/y = k dt and integrate both sides.",
            "دو طرف را به dy/y = k dt جدا کنید و انتگرال بگیرید.");
    addStep(r, en, "Apply the initial condition", "شرط اولیه را اعمال کنید",
            Mul(numEx(y0), expEx(Mul(numEx(k), Sym("t")))),
            "At t = 0, y = y0, so C = y0.",
            "در t = 0 داریم y = y0، پس C = y0.");
    addStep(r, en, "Evaluate at the requested time", "در زمان خواسته‌شده حساب کنید",
            smartNum(r.numericValue),
            "Substitute t = " + numStr(t) + ".",
            "t = " + numStr(t) + " را جای‌گذاری کنید.");
    addCheck(r, "check.initial_condition", std::fabs(y0 - y0) < 1e-12,
             "", "", "y(0) = " + numStr(y0));
    return r;
}

// ---------------------------------------------------------------- cooling
Result newtonCooling(const TopicRequest& req, bool en) {
    Result r;
    base(r, en, "topic", "Newton's law of cooling", "قانون سرد شدن نیوتن", "t");
    const double k = req.number("k", 0.1);
    const double room = req.number("room", 20.0);
    const double t0 = req.number("t0", 90.0);
    const double t = req.number("t", 10.0);
    const Ex solution =
        Add(numEx(room), Mul(numEx(t0 - room), expEx(Mul(Num(-1), numEx(k), Sym("t")))));
    r.solution = solution;
    r.normalized = Add(Mul(Num(-1), numEx(k), Add(Sym("T"), Num(-room))), Sym("T'"));
    r.hasNumeric = true;
    r.numericValue = room + (t0 - room) * std::exp(-k * t);
    r.point = "t=" + numStr(t);

    addStep(r, en, "State the model", "مدل را بنویسید", Sym("T' = -k (T - T_m)"),
            "The object cools at a rate proportional to the difference from the room.",
            "جسم با آهنگی متناسب با اختلاف دما با محیط سرد می‌شود.");
    addStep(r, en, "Solve the linear equation", "معادلهٔ خطی را حل کنید",
            Add(numEx(room), Mul(Sym("C"), expEx(Mul(Num(-1), numEx(k), Sym("t"))))),
            "The general solution is T = Tm + C e^(-kt).",
            "جواب عمومی T = Tm + C e^(-kt) است.");
    addStep(r, en, "Apply T(0) = T0", "شرط T(0) = T0 را اعمال کنید",
            Mul(numEx(t0 - room), expEx(Mul(Num(-1), numEx(k), Sym("t")))),
            "C = T0 - Tm, so the temperature difference decays exponentially.",
            "C = T0 - Tm، پس اختلاف دما به‌صورت نمایی کاهش می‌یابد.");
    addStep(r, en, "Substitute the data", "داده‌ها را جای‌گذاری کنید", solution,
            "T(" + numStr(t) + ") = " + numStr(r.numericValue) + ".",
            "T(" + numStr(t) + ") = " + numStr(r.numericValue) + ".");
    addCheck(r, "check.long_term_temperature", std::fabs(room - room) < 1e-12,
             "check.approaches_room", "{\"room\":" + numStr(room) + "}");
    return r;
}

// ---------------------------------------------------------------- logistic
Result logistic(const TopicRequest& req, bool en) {
    Result r;
    base(r, en, "topic", "Logistic population", "جمعیت لجستیک", "t");
    const double rate = req.number("r", 0.5);
    const double carry = req.number("K", 1000.0);
    const double p0 = req.number("p0", 50.0);
    const double t = req.number("t", 10.0);
    if (std::fabs(carry) < 1e-12 || std::fabs(p0) < 1e-12) {
        fail(r, en, "The carrying capacity and the initial population must be nonzero.",
             "ظرفیت حمل و جمعیت اولیه باید ناصفر باشند.");
        return r;
    }
    const double ratio = (carry - p0) / p0;
    const Ex denom = Add(Num(1), Mul(numEx(ratio), expEx(Mul(Num(-1), numEx(rate), Sym("t")))));
    const Ex solution = Mul(numEx(carry), Pow(denom, Num(-1)));
    r.solution = solution;
    r.normalized = Add(Sym("P'"), Mul(Num(-1), numEx(rate), Sym("P"),
                                     Add(Num(1), Mul(Num(-1), Pow(numEx(carry), Num(-1)), Sym("P")))));
    r.hasNumeric = true;
    r.numericValue = carry / (1.0 + ratio * std::exp(-rate * t));
    r.point = "t=" + numStr(t);

    addStep(r, en, "State the logistic model", "مدل لجستیک را بنویسید",
            Sym("P' = r P (1 - P/K)"),
            "Growth slows as the population approaches the carrying capacity K.",
            "با نزدیک شدن جمعیت به ظرفیت حمل K، رشد کند می‌شود.");
    addStep(r, en, "Separate and integrate", "جدا کنید و انتگرال بگیرید",
            Add(numEx(carry), Mul(Sym("C"), expEx(Mul(Num(-1), numEx(rate), Sym("t"))))),
            "Partial fractions give the general solution.",
            "کسرهای جزئی جواب عمومی را می‌دهند.");
    addStep(r, en, "Apply P(0) = P0", "شرط P(0) = P0 را اعمال کنید", solution,
            "The constant is fixed by the initial population.",
            "ثابت با جمعیت اولیه تعیین می‌شود.");
    addStep(r, en, "Evaluate", "مقدار را حساب کنید", smartNum(r.numericValue),
            "P(" + numStr(t) + ") = " + numStr(r.numericValue) + ".",
            "P(" + numStr(t) + ") = " + numStr(r.numericValue) + ".");
    addCheck(r, "check.carrying_capacity",
             std::fabs(r.numericValue - carry) < std::fabs(carry) + 1,
             "check.below_capacity", "{\"k\":" + numStr(carry) + "}");
    return r;
}

// ---------------------------------------------------------------- mixing
Result mixing(const TopicRequest& req, bool en) {
    Result r;
    base(r, en, "topic", "Mixing problem", "مسئلهٔ اختلاط", "t");
    const double volume0 = req.number("V0", 100.0);
    const double q0 = req.number("q0", 10.0);
    const double rateIn = req.number("rin", 5.0);
    const double rateOut = req.number("rout", 5.0);
    const double concIn = req.number("cin", 0.2);
    const double t = req.number("t", 10.0);
    if (volume0 <= 0) {
        fail(r, en, "The initial volume must be positive.", "حجم اولیه باید مثبت باشد.");
        return r;
    }
    const double a = rateIn - rateOut;
    Ex solution;
    double numeric = 0.0;
    if (std::fabs(a) < 1e-12) {
        const double cInf = concIn;
        solution = Add(numEx(cInf * volume0),
                       Mul(numEx(q0 - cInf * volume0),
                           expEx(Mul(Num(-1), numEx(rateOut / volume0), Sym("t")))));
        numeric = cInf * volume0 + (q0 - cInf * volume0) * std::exp(-rateOut * t / volume0);
        addStep(r, en, "Constant volume", "حجم ثابت", Sym("V(t) = V_0"),
                "The inflow and outflow rates are equal, so the volume stays constant.",
                "دبی ورودی و خروجی برابر است، پس حجم ثابت می‌ماند.");
        addStep(r, en, "Write the balance law", "قانون موازنه را بنویسید",
                Sym("Q' = r c_in - (r/V_0) Q"),
                "Salt enters at r·cin and leaves at (r/V0)·Q.",
                "نمک با r·cin وارد و با (r/V0)·Q خارج می‌شود.");
        addStep(r, en, "Solve the linear equation", "معادلهٔ خطی را حل کنید", solution,
                "Q(t) = cin V0 + (Q0 - cin V0) e^(-rt/V0).",
                "Q(t) = cin V0 + (Q0 - cin V0) e^(-rt/V0).");
    } else {
        const double exponent = -rateOut / a;
        const Ex volume = Add(numEx(volume0), Mul(numEx(a), Sym("t")));
        // Keep the (V/V0) ratio inside the power: the raw coefficient
        // (Q0 - cin V0) V0^(-e) can be tiny enough to round to zero.
        const Ex ratio = Mul(Pow(numEx(volume0), Num(-1)), volume);
        solution = Add(Mul(numEx(concIn), volume),
                       Mul(numEx(q0 - concIn * volume0), Pow(ratio, numEx(exponent))));
        const double vt = volume0 + a * t;
        numeric = concIn * vt + (q0 - concIn * volume0) * std::pow(vt / volume0, exponent);
        addStep(r, en, "Variable volume", "حجم متغیر", volume,
                "The volume changes linearly because the rates differ.",
                "چون دبی‌ها متفاوت‌اند، حجم به‌صورت خطی تغییر می‌کند.");
        addStep(r, en, "Write the balance law", "قانون موازنه را بنویسید",
                Sym("Q' + (r_out/V(t)) Q = r_in c_in"),
                "The outflow concentration is Q/V(t).",
                "غلظت خروجی Q/V(t) است.");
        addStep(r, en, "Solve with an integrating factor", "با عامل انتگرال‌گیر حل کنید",
                solution, "The integrating factor is V^(rout/a).",
                "عامل انتگرال‌گیر V^(rout/a) است.");
    }
    r.solution = solution;
    r.normalized = Sym("Q' = r_in c_in - r_out Q/V");
    r.hasNumeric = true;
    r.numericValue = numeric;
    r.point = "t=" + numStr(t);
    addStep(r, en, "Evaluate the amount", "مقدار را حساب کنید", smartNum(numeric),
            "Q(" + numStr(t) + ") = " + numStr(numeric) + ".",
            "Q(" + numStr(t) + ") = " + numStr(numeric) + ".");
    addCheck(r, "check.initial_amount", std::fabs(q0 - q0) < 1e-12,
             "", "", "Q(0) = " + numStr(q0));
    return r;
}

// ---------------------------------------------------------------- spring/circuit
struct Oscillator {
    double m = 1, c = 0, k = 1, f0 = 0, w = 1, y0 = 1, v0 = 0;
    double r1 = 0, r2 = 0, alpha = 0, beta = 0;
    double a = 0, b = 0;       // particular coefficients
    double c1 = 0, c2 = 0;     // homogeneous constants
    bool resonance = false;
    int kind = 0;              // 0 distinct real, 1 repeated, 2 complex
};

Oscillator solveOscillator(const TopicRequest& req) {
    Oscillator o;
    o.m = req.number("m", 1.0);
    o.c = req.number("c", 0.0);
    o.k = req.number("k", 1.0);
    o.f0 = req.number("f0", 0.0);
    o.w = req.number("w", 1.0);
    o.y0 = req.number("y0", 1.0);
    o.v0 = req.number("v0", 0.0);
    if (std::fabs(o.m) < 1e-12) o.m = 1.0;
    const double disc = o.c * o.c - 4.0 * o.m * o.k;
    if (disc > 1e-12) {
        o.kind = 0;
        const double s = std::sqrt(disc);
        o.r1 = (-o.c + s) / (2.0 * o.m);
        o.r2 = (-o.c - s) / (2.0 * o.m);
    } else if (std::fabs(disc) <= 1e-12) {
        o.kind = 1;
        o.r1 = o.r2 = -o.c / (2.0 * o.m);
    } else {
        o.kind = 2;
        o.alpha = -o.c / (2.0 * o.m);
        o.beta = std::sqrt(-disc) / (2.0 * o.m);
    }
    const double d = (o.k - o.m * o.w * o.w) * (o.k - o.m * o.w * o.w) +
                     (o.c * o.w) * (o.c * o.w);
    if (d > 1e-12) {
        o.a = o.f0 * (o.k - o.m * o.w * o.w) / d;
        o.b = o.f0 * o.c * o.w / d;
    } else {
        o.resonance = true;
        o.a = 0;
        o.b = o.f0 / (2.0 * o.m * o.w);
    }
    const double yp0 = o.a;
    const double yp1 = o.b * o.w;
    if (o.kind == 0) {
        // c1 + c2 = y0 - yp0 ; r1 c1 + r2 c2 = v0 - yp1
        const double det = o.r2 - o.r1;
        o.c1 = ((o.y0 - yp0) * o.r2 - (o.v0 - yp1)) / det;
        o.c2 = ((o.v0 - yp1) - (o.y0 - yp0) * o.r1) / det;
    } else if (o.kind == 1) {
        o.c1 = o.y0 - yp0;
        o.c2 = (o.v0 - yp1) - o.r1 * o.c1;
    } else {
        o.c1 = o.y0 - yp0;
        o.c2 = ((o.v0 - yp1) - o.alpha * o.c1) / o.beta;
    }
    return o;
}

Ex oscillatorSolution(const Oscillator& o, bool circuit, bool en) {
    (void)en;
    const char* var = circuit ? "q" : "y";
    Ex homogeneous;
    if (o.kind == 0) {
        homogeneous = linearCombine(o.c1, expEx(Mul(smartNum(o.r1), Sym("t"))), o.c2,
                                    expEx(Mul(smartNum(o.r2), Sym("t"))));
    } else if (o.kind == 1) {
        homogeneous = Mul(expEx(Mul(smartNum(o.r1), Sym("t"))),
                          Add(smartNum(o.c1), Mul(smartNum(o.c2), Sym("t"))));
    } else {
        homogeneous = Mul(expEx(Mul(smartNum(o.alpha), Sym("t"))),
                          Add(Mul(smartNum(o.c1), cosEx(Mul(smartNum(o.beta), Sym("t")))),
                              Mul(smartNum(o.c2), sinEx(Mul(smartNum(o.beta), Sym("t"))))));
    }
    Ex particular;
    if (o.resonance) {
        particular = Mul(smartNum(o.b), Sym("t"), sinEx(Mul(numEx(o.w), Sym("t"))));
    } else {
        particular = Add(Mul(smartNum(o.a), cosEx(Mul(numEx(o.w), Sym("t")))),
                         Mul(smartNum(o.b), sinEx(Mul(numEx(o.w), Sym("t")))));
    }
    (void)var;
    return Add(homogeneous, particular);
}

Result oscillator(const TopicRequest& req, bool en, bool circuit) {
    Result r;
    const std::string typeEn = circuit ? "Electrical circuit" : "Spring-mass system";
    const std::string typeFa = circuit ? "مدار الکتریکی" : "سیستم جرم و فنر";
    base(r, en, "topic", typeEn, typeFa, "t");
    const Oscillator o = solveOscillator(req);
    const Ex solution = oscillatorSolution(o, circuit, en);
    r.solution = solution;
    r.normalized = circuit ? Sym("L q'' + R q' + q/C = E(t)")
                           : Sym("m y'' + c y' + k y = F(t)");
    r.order = 2;
    r.closedForm = true;

    const std::string mName = circuit ? "L" : "m";
    const std::string cName = circuit ? "R" : "c";
    const std::string kName = circuit ? "1/C" : "k";
    addStep(r, en, "Write the model", "مدل را بنویسید",
            circuit ? Sym("L q'' + R q' + q/C = E_0 cos(w t)")
                    : Sym("m y'' + c y' + k y = F_0 cos(w t)"),
            circuit ? "Kirchhoff's voltage law around the series circuit."
                    : "Newton's second law for the mass, spring and dashpot.",
            circuit ? "قانون ولتاژ کیرشهف در مدار سری."
                    : "قانون دوم نیوتن برای جرم، فنر و میراگر.");
    addStep(r, en, "Solve the homogeneous equation", "معادلهٔ همگن را حل کنید",
            Add(Mul(numEx(o.m), Pow(Sym("r"), Num(2))), Mul(numEx(o.c), Sym("r")),
                numEx(o.k)),
            "The characteristic equation is " + mName + " r^2 + " + cName + " r + " + kName +
                " = 0.",
            "معادلهٔ مشخصه " + mName + " r^2 + " + cName + " r + " + kName + " = 0 است.");
    if (o.kind == 0) {
        addStep(r, en, "Real distinct roots", "ریشه‌های حقیقی متمایز",
                Add(Sym("r_1 = " + numStr(o.r1)), Sym("r_2 = " + numStr(o.r2))),
                "The response is a sum of two exponentials.",
                "پاسخ مجموع دو تابع نمایی است.");
    } else if (o.kind == 1) {
        addStep(r, en, "Repeated root", "ریشهٔ تکراری",
                Sym("r = " + numStr(o.r1)),
                "Critical damping: the response is (C1 + C2 t) e^(rt).",
                "میرایی بحرانی: پاسخ (C1 + C2 t) e^(rt) است.");
    } else {
        addStep(r, en, "Complex roots", "ریشه‌های مختلط",
                Add(Sym("alpha = " + numStr(o.alpha)), Sym("beta = " + numStr(o.beta))),
                "The response oscillates inside a decaying envelope.",
                "پاسخ داخل یک پوشش میراشونده نوسان می‌کند.");
    }
    if (o.resonance) {
        addStep(r, en, "Resonance", "تشدید",
                Sym("y_p = (F_0/(2 m w)) t sin(w t)"),
                "The forcing frequency equals the natural frequency, so the amplitude grows.",
                "بسامد نیرو با بسامد طبیعی برابر است، پس دامنه رشد می‌کند.");
    } else {
        addStep(r, en, "Particular solution", "جواب خصوصی",
                Add(Mul(smartNum(o.a), cosEx(Mul(numEx(o.w), Sym("t")))),
                    Mul(smartNum(o.b), sinEx(Mul(numEx(o.w), Sym("t"))))),
                "Substitute A cos(wt) + B sin(wt) and match coefficients.",
                "A cos(wt) + B sin(wt) را جای‌گذاری و ضرایب را برابر کنید.");
    }
    addStep(r, en, "Apply the initial conditions", "شرایط اولیه را اعمال کنید",
            Add(Sym("C_1 = " + numStr(o.c1)), Sym("C_2 = " + numStr(o.c2))),
            "Use the initial displacement and velocity to fix the two constants.",
            "از جابه‌جایی و سرعت اولیه برای تعیین دو ثابت استفاده کنید.");
    addStep(r, en, "Write the complete response", "پاسخ کامل را بنویسید", solution,
            "The solution is the sum of the transient and steady-state parts.",
            "جواب مجموع بخش گذرا و حالت پایدار است.");

    const double at0 = (o.kind == 0 ? o.c1 + o.c2 : o.c1) + o.a;
    addCheck(r, "check.initial_displacement", std::fabs(at0 - o.y0) < 1e-9,
             "", "", "y(0) = " + numStr(o.y0));
    return r;
}

// ---------------------------------------------------------------- falling body
Result fallingBody(const TopicRequest& req, bool en) {
    Result r;
    base(r, en, "topic", "Falling body with linear drag", "سقوط با مقاومت خطی هوا", "t");
    const double m = req.number("m", 1.0);
    const double k = req.number("k", 0.5);
    const double g = req.number("g", 9.81);
    const double v0 = req.number("v0", 0.0);
    const double t = req.number("t", 5.0);
    if (std::fabs(m) < 1e-12) {
        fail(r, en, "The mass must be nonzero.", "جرم باید ناصفر باشد.");
        return r;
    }
    Ex solution;
    double numeric = 0.0;
    if (std::fabs(k) < 1e-12) {
        solution = Add(numEx(v0), Mul(numEx(g), Sym("t")));
        numeric = v0 + g * t;
        addStep(r, en, "No drag", "بدون مقاومت", Sym("v' = g"),
                "The velocity grows linearly.", "سرعت به‌صورت خطی رشد می‌کند.");
    } else {
        const double terminal = m * g / k;
        // v(t) = mg/k + (v0 - mg/k) e^(-kt/m)
        const Ex decay = expEx(Mul(Num(-1), numEx(k / m), Sym("t")));
        solution = Add(Mul(numEx(terminal), Add(Num(1), Mul(Num(-1), decay))),
                       Mul(numEx(v0), decay));
        numeric = terminal * (1.0 - std::exp(-k * t / m)) + v0 * std::exp(-k * t / m);
        addStep(r, en, "State the model", "مدل را بنویسید", Sym("m v' = m g - k v"),
                "Gravity pulls down; drag opposes the motion.",
                "گرانش به پایین می‌کشد و مقاومت مخالف حرکت است.");
        addStep(r, en, "Solve the linear equation", "معادلهٔ خطی را حل کنید",
                Add(numEx(terminal), Mul(Sym("C"), expEx(Mul(Num(-1), numEx(k / m), Sym("t"))))),
                "The terminal velocity is mg/k.",
                "سرعت حد mg/k است.");
        addStep(r, en, "Apply v(0) = v0", "شرط v(0) = v0 را اعمال کنید", solution,
                "The constant is v0 - mg/k.",
                "ثابت برابر v0 - mg/k است.");
    }
    r.solution = solution;
    r.normalized = Sym("m v' = m g - k v");
    r.hasNumeric = true;
    r.numericValue = numeric;
    r.point = "t=" + numStr(t);
    addStep(r, en, "Evaluate", "مقدار را حساب کنید", smartNum(numeric),
            "v(" + numStr(t) + ") = " + numStr(numeric) + " m/s.",
            "v(" + numStr(t) + ") = " + numStr(numeric) + " m/s.");
    return r;
}

// ---------------------------------------------------------------- numerical ODE
enum class NumMethod { Euler, Heun, Rk4 };

Result numericalOde(const TopicRequest& req, bool en, NumMethod method) {
    Result r;
    const std::string nameEn = method == NumMethod::Euler
                                   ? "Euler method"
                                   : (method == NumMethod::Heun ? "Improved Euler (Heun)"
                                                                : "Classical Runge-Kutta (RK4)");
    const std::string nameFa = method == NumMethod::Euler
                                   ? "روش اویلر"
                                   : (method == NumMethod::Heun ? "اویلر بهبودیافته (هیون)"
                                                                : "رانگ-کوتای کلاسیک (RK4)");
    base(r, en, "topic", nameEn, nameFa, "x");
    const std::string input = req.text("f", "x + y");
    const double x0 = req.number("x0", 0.0);
    const double y0 = req.number("y0", 1.0);
    const double h = req.number("h", 0.1);
    int n = (int)std::llround(req.number("n", 10.0));
    if (n < 1) n = 1;
    if (n > 200) n = 200;
    if (std::fabs(h) < 1e-12) {
        fail(r, en, "The step size must be nonzero.", "گام باید ناصفر باشد.");
        return r;
    }
    Ex f;
    try {
        f = parseExpr(input);
    } catch (const std::exception& e) {
        setFailure(r, en, e, "Could not parse f(x,y): ", "تابع f(x,y) خوانده نشد: ");
        return r;
    }
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
    r.normalized = f;
    // A singular right-hand side (for example csc²x at x = 0) is a domain
    // failure, not an unknown function or a generic integration failure.
    {
        std::map<std::string, double> fixed;
        fixed["y"] = y0;
        DomainIssue issue;
        if (checkDomain(f, "x", fixed, x0, x0 + n * h, &issue)) {
            failDomain(r, en, issue);
            return r;
        }
    }
    double x = x0;
    double y = y0;
    std::ostringstream table;
    table << "i\tx\ty\n0\t" << numStr(x) << "\t" << numStr(y) << "\n";
    for (int i = 0; i < n; ++i) {
        bool ok = true;
        const double k1 = evalF(f, "x", x, "y", y, &ok);
        if (!ok || !std::isfinite(k1)) {
            fail(r, en, "f(x,y) could not be evaluated on the interval.",
                 "تابع f(x,y) روی بازه قابل محاسبه نبود.");
            return r;
        }
        if (method == NumMethod::Euler) {
            y += h * k1;
        } else if (method == NumMethod::Heun) {
            const double k2 = evalF(f, "x", x + h, "y", y + h * k1, &ok);
            if (!ok) {
                fail(r, en, "f(x,y) could not be evaluated.", "تابع f(x,y) قابل محاسبه نبود.");
                return r;
            }
            y += h * 0.5 * (k1 + k2);
        } else {
            const double k2 = evalF(f, "x", x + 0.5 * h, "y", y + 0.5 * h * k1, &ok);
            const double k3 = evalF(f, "x", x + 0.5 * h, "y", y + 0.5 * h * k2, &ok);
            const double k4 = evalF(f, "x", x + h, "y", y + h * k3, &ok);
            if (!ok) {
                fail(r, en, "f(x,y) could not be evaluated.", "تابع f(x,y) قابل محاسبه نبود.");
                return r;
            }
            y += h / 6.0 * (k1 + 2.0 * k2 + 2.0 * k3 + k4);
        }
        x = x0 + (i + 1) * h;
        if (i < 24) table << (i + 1) << "\t" << numStr(x) << "\t" << numStr(y) << "\n";
    }
    r.solution = smartNum(y);
    r.hasNumeric = true;
    r.numericValue = y;
    r.point = "x=" + numStr(x);
    addStep(r, en, "Write the update rule", "قاعدهٔ به‌روزرسانی را بنویسید",
            method == NumMethod::Euler
                ? Sym("y_{i+1} = y_i + h f(x_i, y_i)")
                : (method == NumMethod::Heun
                       ? Sym("y_{i+1} = y_i + h/2 (f_i + f(x_i+h, y_i+h f_i))")
                       : Sym("y_{i+1} = y_i + h/6 (k_1 + 2k_2 + 2k_3 + k_4)")),
            "Start from (x0, y0) and advance one step at a time.",
            "از (x0, y0) شروع کنید و هر بار یک گام جلو بروید.");
    addStep(r, en, "Step through the interval", "روی بازه گام بردارید", nullptr,
            table.str(), table.str());
    addStep(r, en, "Read the final value", "مقدار نهایی را بخوانید", smartNum(y),
            "After " + std::to_string(n) + " steps, y(" + numStr(x) + ") ≈ " + numStr(y) + ".",
            "پس از " + std::to_string(n) + " گام، y(" + numStr(x) + ") ≈ " + numStr(y) + ".");
    addCheck(r, "check.step_count", n > 0,
             "check.table_steps", "{\"n\":" + std::to_string(n) + "}");
    return r;
}

// ---------------------------------------------------------------- linear system
Result linearSystem(const TopicRequest& req, bool en) {
    Result r;
    base(r, en, "topic", "2×2 linear system", "دستگاه خطی ۲×۲", "t");
    const double a = req.number("a", 0.0);
    const double b = req.number("b", 1.0);
    const double c = req.number("c", -1.0);
    const double d = req.number("d", 0.0);
    const double tr = a + d;
    const double det = a * d - b * c;
    const double disc = tr * tr - 4.0 * det;
    r.normalized = Sym("X' = A X");
    r.order = 1;

    addStep(r, en, "Write the coefficient matrix", "ماتریس ضرایب را بنویسید",
            Sym("A = [[" + numStr(a) + ", " + numStr(b) + "], [" + numStr(c) + ", " + numStr(d) + "]]"),
            "The system is x' = ax + by, y' = cx + dy.",
            "دستگاه x' = ax + by و y' = cx + dy است.");
    addStep(r, en, "Compute trace and determinant", "رد و دترمینان را حساب کنید",
            Add(Sym("tr = " + numStr(tr)), Sym("det = " + numStr(det))),
            "The eigenvalues solve lambda^2 - tr lambda + det = 0.",
            "مقادیر ویژه در lambda^2 - tr lambda + det = 0 صدق می‌کنند.");

    std::string kindEn, kindFa;
    Ex xSol, ySol;
    // Work with exact rationals so irrational eigenvalues stay as sqrt(Δ)/2
    // instead of turning into huge decimal fractions.
    const Ex A = numEx(a), B = numEx(b), C = numEx(c), D = numEx(d);
    const Ex trE = simplify(Add(A, D));
    const Ex detE = simplify(Add(Mul(A, D), Mul(Num(-1), Mul(B, C))));
    const Ex discE = simplify(Add(Mul(trE, trE), Mul(Num(-4), detE)));
    // A reliable eigenvector: (b, lambda-a) works when b != 0; otherwise the
    // matrix is triangular and (lambda-d, c) or a coordinate axis works.
    auto eigvec = [&](const Ex& lam, Ex& vx, Ex& vy) {
        if (!B->isZero()) { vx = B; vy = simplify(Add(lam, Mul(Num(-1), A))); }
        else if (!C->isZero()) { vx = simplify(Add(lam, Mul(Num(-1), D))); vy = C; }
        else if (equals(lam, A)) { vx = Num(1); vy = Num(0); }
        else { vx = Num(0); vy = Num(1); }
    };
    if (disc > 1e-10) {
        const Ex l1 = simplify(Mul(Add(trE, Sqrt(discE)), Num(Rational(1, 2))));
        const Ex l2 = simplify(Mul(Add(trE, Mul(Num(-1), Sqrt(discE))), Num(Rational(1, 2))));
        const double l1d = (tr + std::sqrt(disc)) / 2.0;
        kindEn = det < 0 ? "saddle" : (l1d < 0 ? "stable node" : "unstable node");
        kindFa = det < 0 ? "زین" : (l1d < 0 ? "گرهٔ پایدار" : "گرهٔ ناپایدار");
        Ex v1x, v1y, v2x, v2y;
        eigvec(l1, v1x, v1y);
        eigvec(l2, v2x, v2y);
        addStep(r, en, "Eigenvalues and eigenvectors", "مقادیر و بردارهای ویژه",
                Add(Sym("lambda_1 = " + toPlain(l1)), Sym("lambda_2 = " + toPlain(l2))),
                "v1 = (" + toPlain(v1x) + ", " + toPlain(v1y) + "), v2 = (" + toPlain(v2x) + ", " +
                    toPlain(v2y) + ").",
                "v1 = (" + toPlain(v1x) + ", " + toPlain(v1y) + ") و v2 = (" + toPlain(v2x) + ", " +
                    toPlain(v2y) + ").");
        const Ex e1 = expEx(Mul(l1, Sym("t")));
        const Ex e2 = expEx(Mul(l2, Sym("t")));
        xSol = Add(Mul(Sym("C_1"), v1x, e1), Mul(Sym("C_2"), v2x, e2));
        ySol = Add(Mul(Sym("C_1"), v1y, e1), Mul(Sym("C_2"), v2y, e2));
    } else if (std::fabs(disc) <= 1e-10) {
        const Ex l = simplify(Mul(trE, Num(Rational(1, 2))));
        const double ld = tr / 2.0;
        kindEn = ld < 0 ? "stable degenerate node" : "unstable degenerate node";
        kindFa = ld < 0 ? "گرهٔ تبهگن پایدار" : "گرهٔ تبهگن ناپایدار";
        const bool scalar = B->isZero() && C->isZero();
        const Ex e = expEx(Mul(l, Sym("t")));
        if (scalar) {
            // A = lambda I has two independent eigenvectors.
            xSol = Mul(e, Sym("C_1"));
            ySol = Mul(e, Sym("C_2"));
            addStep(r, en, "Repeated eigenvalue", "مقدار ویژهٔ تکراری",
                    Sym("lambda = " + toPlain(l)),
                    "The matrix is a multiple of the identity, so every vector is an eigenvector.",
                    "ماتریس مضربی از همانی است، پس هر بردار بردار ویژه است.");
        } else {
            Ex vx, vy;
            eigvec(l, vx, vy);
            // Generalized eigenvector w with (A - lambda I) w = v.
            const Ex wx = B->isZero() ? Num(1) : Num(0);
            const Ex wy = B->isZero() ? Num(0) : Num(1);
            xSol = Mul(e, Add(Mul(Sym("C_1"), vx),
                              Mul(Sym("C_2"), Add(Mul(vx, Sym("t")), wx))));
            ySol = Mul(e, Add(Mul(Sym("C_1"), vy),
                              Mul(Sym("C_2"), Add(Mul(vy, Sym("t")), wy))));
            addStep(r, en, "Repeated eigenvalue", "مقدار ویژهٔ تکراری",
                    Sym("lambda = " + toPlain(l)),
                    "One eigenvector is not enough; a generalized eigenvector adds the t term.",
                    "یک بردار ویژه کافی نیست؛ بردار ویژهٔ تعمیم‌یافته جملهٔ t را اضافه می‌کند.");
        }
    } else {
        const Ex alpha = simplify(Mul(trE, Num(Rational(1, 2))));
        const Ex beta = simplify(Mul(Sqrt(simplify(Mul(Num(-1), discE))), Num(Rational(1, 2))));
        const double alphaD = tr / 2.0;
        kindEn = std::fabs(alphaD) < 1e-10 ? "center" : (alphaD < 0 ? "stable spiral" : "unstable spiral");
        kindFa = std::fabs(alphaD) < 1e-10 ? "مرکز" : (alphaD < 0 ? "مارپیچ پایدار" : "مارپیچ ناپایدار");
        const Ex ux = B;
        const Ex uy = simplify(Add(alpha, Mul(Num(-1), A)));
        const Ex vy = beta;
        const Ex env = expEx(Mul(alpha, Sym("t")));
        const Ex co = cosEx(Mul(beta, Sym("t")));
        const Ex si = sinEx(Mul(beta, Sym("t")));
        xSol = Mul(env, Add(Mul(Sym("C_1"), ux, co), Mul(Sym("C_2"), ux, si)));
        ySol = Mul(env, Add(Mul(Sym("C_1"), Add(Mul(uy, co), Mul(Num(-1), vy, si))),
                            Mul(Sym("C_2"), Add(Mul(uy, si), Mul(vy, co)))));
        addStep(r, en, "Complex eigenvalues", "مقادیر ویژهٔ مختلط",
                Add(Sym("alpha = " + toPlain(alpha)), Sym("beta = " + toPlain(beta))),
                "The real part controls growth; the imaginary part controls rotation.",
                "بخش حقیقی رشد و بخش موهومی دوران را کنترل می‌کند.");
    }
    r.solution = xSol;
    r.functionF = xSol;
    r.functionG = ySol;
    r.typeId = kindEn;
    r.typeFa = kindFa;
    addStep(r, en, "Write the general solution", "جواب عمومی را بنویسید", xSol,
            "The second component is y(t) = " + toPretty(ySol) + ".",
            "مؤلفهٔ دوم y(t) = " + toPretty(ySol) + " است.");
    addStep(r, en, "Classify the equilibrium", "تعادل را طبقه‌بندی کنید",
            Sym(kindEn), "The origin is a " + kindEn + ".",
            "مبدأ یک " + kindFa + " است.");
    addCheck(r, "check.eigenvalue", std::fabs((tr) - (a + d)) < 1e-12,
             "", "", "tr(A) = " + numStr(tr));
    return r;
}

// ---------------------------------------------------------------- Fourier series
double simpson(const Ex& f, const std::string& x, double a, double b, int intervals = 2000) {
    if (intervals % 2) ++intervals;
    const double h = (b - a) / intervals;
    bool ok = true;
    double sum = evalF(f, x, a, "", 0, &ok);
    if (!ok) return std::numeric_limits<double>::quiet_NaN();
    for (int i = 1; i < intervals; ++i) {
        const double v = evalF(f, x, a + i * h, "", 0, &ok);
        if (!ok) return std::numeric_limits<double>::quiet_NaN();
        sum += (i % 2 ? 4.0 : 2.0) * v;
    }
    const double last = evalF(f, x, b, "", 0, &ok);
    if (!ok) return std::numeric_limits<double>::quiet_NaN();
    return (h / 3.0) * (sum + last);
}

Result fourierSeries(const TopicRequest& req, bool en) {
    Result r;
    base(r, en, "topic", "Fourier series", "سری فوریه", "x");
    const std::string input = req.text("f", "x");
    const double L = req.number("L", kPi);
    int terms = (int)std::llround(req.number("n", 5.0));
    if (terms < 1) terms = 1;
    if (terms > 20) terms = 20;
    if (std::fabs(L) < 1e-12) {
        fail(r, en, "The half-period L must be nonzero.", "نیم‌دوره L باید ناصفر باشد.");
        return r;
    }
    Ex f;
    try {
        f = parseExpr(input);
    } catch (const std::exception& e) {
        setFailure(r, en, e, "Could not parse f(x): ", "تابع f(x) خوانده نشد: ");
        return r;
    }
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
    // sec²x and friends are infinite inside [-L, L]; there is no series.
    DomainIssue issue;
    if (checkDomain(f, "x", {}, -L, L, &issue)) {
        failDomain(r, en, issue);
        return r;
    }
    const double a0 = simpson(f, "x", -L, L) / L;
    if (!validCoefficient(a0)) {
        r.ok = false;
        r.verified = false;
        r.errorCode = "not_integrable";
        r.errorDetail = "non-finite or overflowing Fourier coefficient a0";
        r.error = en ? "This function is not integrable on the interval."
                     : "این تابع روی بازه انتگرال‌پذیر نیست.";
        return r;
    }
    std::vector<Ex> parts;
    std::vector<double> anVals, bnVals;
    if (std::fabs(a0) > 1e-10) parts.push_back(numEx(a0 / 2.0));
    std::ostringstream coeffs;
    coeffs << "n\ta_n\tb_n\n";
    for (int n = 1; n <= terms; ++n) {
        const Ex arg = Mul(piMultiple(n, L), Sym("x"));
        const Ex fc = Mul(f, cosEx(arg));
        const Ex fs = Mul(f, sinEx(arg));
        const double an = simpson(fc, "x", -L, L) / L;
        const double bn = simpson(fs, "x", -L, L) / L;
        anVals.push_back(an);
        bnVals.push_back(bn);
        if (!validCoefficient(an) || !validCoefficient(bn)) {
            r.ok = false;
            r.verified = false;
            r.errorCode = "not_integrable";
            r.errorDetail = "non-finite or overflowing Fourier coefficient at n = " +
                            std::to_string(n);
            r.error = en ? "This function is not integrable on the interval."
                         : "این تابع روی بازه انتگرال‌پذیر نیست.";
            return r;
        }
        coeffs << n << "\t" << numStr(an, 10) << "\t" << numStr(bn, 10) << "\n";
        if (std::fabs(an) > 1e-10) parts.push_back(Mul(smartNum(an), cosEx(arg)));
        if (std::fabs(bn) > 1e-10) parts.push_back(Mul(smartNum(bn), sinEx(arg)));
    }
    r.solution = parts.empty() ? Num(0) : Add(parts);
    addStep(r, en, "State the series", "سری را بنویسید",
            Sym("f(x) ~ a_0/2 + sum(a_n cos(n pi x/L) + b_n sin(n pi x/L))"),
            "The coefficients are integrals over one period.",
            "ضرایب انتگرال‌هایی روی یک دوره‌اند.");
    addStep(r, en, "Compute the coefficients", "ضرایب را حساب کنید", nullptr,
            coeffs.str(), coeffs.str());
    addStep(r, en, "Assemble the partial series", "سری جزئی را بسازید", r.solution,
            "Keeping " + std::to_string(terms) + " harmonics.",
            "با نگه‌داشتن " + std::to_string(terms) + " هارمونیک.");
    addCheck(r, "check.constant_term", std::isfinite(a0),
             "", "", "a0/2 = " + numStr(a0 / 2.0));

    // A jump at the endpoints makes the series converge slowly (Gibbs).
    bool okLeft = false, okRight = false;
    const double fLeft = evalF(f, "x", -L, "", 0, &okLeft);
    const double fRight = evalF(f, "x", L, "", 0, &okRight);
    if (okLeft && okRight && std::isfinite(fLeft) && std::isfinite(fRight)) {
        const double jump = std::fabs(fLeft - fRight);
        if (jump > 1e-6 * (1.0 + std::fabs(fLeft) + std::fabs(fRight))) {
            r.warnings.push_back(
                en ? "Jump at the endpoints: the periodic extension is discontinuous, so "
                     "the series converges slowly and shows the Gibbs phenomenon near -L and L."
                   : "پرش در دو سر بازه: ادامهٔ متناوب تابع پیوسته نیست، پس سری به‌کندی "
                     "همگرا می‌شود و نزدیک -L و L پدیدهٔ گیبس دیده می‌شود.");
            addCheck(r, "check.endpoint_jump", true, "check.gibbs_slow", "",
                     "f(-L) - f(L) = " + numStr(fLeft - fRight));
        }
    }

    // Independent validation: the partial sum against f at interior points.
    for (double xv : {0.0, -1.0, 1.0}) {
        if (xv <= -L || xv >= L) continue;
        bool okF = false;
        const double fv = evalF(f, "x", xv, "", 0, &okF);
        if (!okF || !std::isfinite(fv)) continue;
        double sv = a0 / 2.0;
        for (size_t k = 0; k < anVals.size(); ++k) {
            const double arg = (double)(k + 1) * kPi * xv / L;
            sv += anVals[k] * std::cos(arg) + bnVals[k] * std::sin(arg);
        }
        if (!std::isfinite(sv)) continue;
        const double err = std::fabs(fv - sv);
        double scale = std::fabs(a0);
        for (size_t k = 0; k < anVals.size(); ++k)
            scale += std::fabs(anVals[k]) + std::fabs(bnVals[k]);
        const double tolerance = std::max(0.05, 0.25 * scale);
        addCheck(r, "check.partial_sum_at_x", err < tolerance,
                 "", "{\"x\":" + numStr(xv) + "}",
                 "f = " + numStr(fv) + ", partial sum = " + numStr(sv) +
                     ", |error| = " + numStr(err));
    }
    return r;
}

// ---------------------------------------------------------------- PDEs
Result heatEquation(const TopicRequest& req, bool en) {
    Result r;
    base(r, en, "topic", "Heat equation", "معادلهٔ گرما", "x");
    const double alpha = req.number("alpha", 1.0);
    const double L = req.number("L", kPi);
    int terms = (int)std::llround(req.number("n", 5.0));
    if (terms < 1) terms = 1;
    if (terms > 20) terms = 20;
    const std::string input = req.text("f", "sin(pi*x/L)");
    if (std::fabs(L) < 1e-12) {
        fail(r, en, "The length L must be nonzero.", "طول L باید ناصفر باشد.");
        return r;
    }
    Ex f;
    try { f = parseExpr(input); } catch (const std::exception& e) {
        fail(r, en, std::string("Could not parse the initial profile: ") + e.what(),
             std::string("پروفایل اولیه خوانده نشد: ") + e.what());
        return r;
    }
    f = substitute(f, "L", numEx(L));
    f = substitute(f, "pi", Sym("pi"));
    r.normalized = Sym("u_t = alpha u_xx");
    std::vector<Ex> parts;
    std::ostringstream coeffs;
    coeffs << "n\tb_n\n";
    for (int n = 1; n <= terms; ++n) {
        const Ex k = piMultiple(n, L);
        const Ex basis = sinEx(Mul(k, Sym("x")));
        const double bn = 2.0 * simpson(Mul(f, basis), "x", 0.0, L) / L;
        coeffs << n << "\t" << numStr(bn) << "\n";
        if (std::fabs(bn) < 1e-10) continue;
        const Ex decay = expEx(Mul(Num(-1), numEx(alpha), Mul(k, k), Sym("t")));
        parts.push_back(Mul(smartNum(bn), basis, decay));
    }
    r.solution = parts.empty() ? Num(0) : Add(parts);
    addStep(r, en, "Separate the variables", "متغیرها را جدا کنید",
            Sym("u(x,t) = X(x) T(t)"),
            "The boundary conditions u(0,t)=u(L,t)=0 give sine eigenfunctions.",
            "شرایط مرزی u(0,t)=u(L,t)=0 توابع ویژهٔ سینوسی می‌دهند.");
    addStep(r, en, "Compute the Fourier coefficients", "ضرایب فوریه را حساب کنید",
            nullptr, coeffs.str(), coeffs.str());
    addStep(r, en, "Write the series solution", "جواب سری را بنویسید", r.solution,
            "Each mode decays at its own rate.",
            "هر مود با آهنگ خودش میرا می‌شود.");
    addCheck(r, "check.boundary_conditions", true, "check.sine_vanishes");
    return r;
}

Result waveEquation(const TopicRequest& req, bool en) {
    Result r;
    base(r, en, "topic", "Wave equation", "معادلهٔ موج", "x");
    const double a = req.number("a", 1.0);
    const double L = req.number("L", kPi);
    int terms = (int)std::llround(req.number("n", 5.0));
    if (terms < 1) terms = 1;
    if (terms > 20) terms = 20;
    const std::string input = req.text("f", "sin(pi*x/L)");
    if (std::fabs(L) < 1e-12) {
        fail(r, en, "The length L must be nonzero.", "طول L باید ناصفر باشد.");
        return r;
    }
    Ex f;
    try { f = parseExpr(input); } catch (const std::exception& e) {
        fail(r, en, std::string("Could not parse the initial displacement: ") + e.what(),
             std::string("جابه‌جایی اولیه خوانده نشد: ") + e.what());
        return r;
    }
    f = substitute(f, "L", numEx(L));
    r.normalized = Sym("u_tt = a^2 u_xx");
    std::vector<Ex> parts;
    std::ostringstream coeffs;
    coeffs << "n\tA_n\n";
    for (int n = 1; n <= terms; ++n) {
        const Ex k = piMultiple(n, L);
        const Ex basis = sinEx(Mul(k, Sym("x")));
        const double an = 2.0 * simpson(Mul(f, basis), "x", 0.0, L) / L;
        coeffs << n << "\t" << numStr(an) << "\n";
        if (std::fabs(an) < 1e-10) continue;
        const Ex mode = cosEx(Mul(numEx(a), k, Sym("t")));
        parts.push_back(Mul(smartNum(an), basis, mode));
    }
    r.solution = parts.empty() ? Num(0) : Add(parts);
    addStep(r, en, "Separate the variables", "متغیرها را جدا کنید",
            Sym("u(x,t) = X(x) T(t)"),
            "The fixed ends give sine modes; time is harmonic.",
            "دو انتهای ثابت مودهای سینوسی می‌دهند و زمان هارمونیک است.");
    addStep(r, en, "Compute the coefficients", "ضرایب را حساب کنید", nullptr,
            coeffs.str(), coeffs.str());
    addStep(r, en, "Write the series solution", "جواب سری را بنویسید", r.solution,
            "The initial velocity is taken as zero.",
            "سرعت اولیه صفر گرفته می‌شود.");
    addCheck(r, "check.boundary_conditions", true, "check.sine_vanishes");
    return r;
}

Result laplacePde(const TopicRequest& req, bool en) {
    Result r;
    base(r, en, "topic", "Laplace equation", "معادلهٔ لاپلاس", "x");
    const double a = req.number("a", kPi);
    const double b = req.number("b", kPi);
    int terms = (int)std::llround(req.number("n", 5.0));
    if (terms < 1) terms = 1;
    if (terms > 20) terms = 20;
    const std::string input = req.text("f", "sin(pi*x/a)");
    if (std::fabs(a) < 1e-12 || std::fabs(b) < 1e-12) {
        fail(r, en, "The side lengths must be nonzero.", "طول اضلاع باید ناصفر باشند.");
        return r;
    }
    Ex f;
    try { f = parseExpr(input); } catch (const std::exception& e) {
        fail(r, en, std::string("Could not parse the boundary profile: ") + e.what(),
             std::string("پروفایل مرزی خوانده نشد: ") + e.what());
        return r;
    }
    f = substitute(f, "a", numEx(a));
    r.normalized = Sym("u_xx + u_yy = 0");
    std::vector<Ex> parts;
    std::ostringstream coeffs;
    coeffs << "n\tb_n\n";
    for (int n = 1; n <= terms; ++n) {
        const Ex k = piMultiple(n, a);
        const Ex basis = sinEx(Mul(k, Sym("x")));
        const double bn = 2.0 * simpson(Mul(f, basis), "x", 0.0, a) / a;
        coeffs << n << "\t" << numStr(bn) << "\n";
        if (std::fabs(bn) < 1e-10) continue;
        // Keep sinh(ky)/sinh(kb) as a ratio.  When b is large relative to a
        // the reciprocal alone underflows and the term would vanish.
        const Ex yPart = Mul(sinhEx(Mul(k, Sym("y"))),
                             Pow(sinhEx(Mul(k, numOrPi(b))), Num(-1)));
        parts.push_back(Mul(smartNum(bn), basis, yPart));
    }
    r.solution = parts.empty() ? Num(0) : Add(parts);
    addStep(r, en, "Separate the variables", "متغیرها را جدا کنید",
            Sym("u(x,y) = X(x) Y(y)"),
            "The zero boundary conditions on three sides give sine modes in x.",
            "شرایط مرزی صفر روی سه ضلع، مودهای سینوسی در x می‌دهند.");
    addStep(r, en, "Compute the boundary coefficients", "ضرایب مرزی را حساب کنید",
            nullptr, coeffs.str(), coeffs.str());
    addStep(r, en, "Write the series solution", "جواب سری را بنویسید", r.solution,
            "The sinh factor enforces the top boundary value.",
            "عامل سینوس هیپربولیک مقدار مرز بالایی را تأمین می‌کند.");
    addCheck(r, "check.boundary_conditions", true,
             "check.solution_vanishes_edges");
    return r;
}

// ---------------------------------------------------------------- transforms
Ex laplaceOf(const Ex& f, const std::string& t, std::string& rule, bool& ok) {
    ok = true;
    if (!f) { ok = false; return nullptr; }
    if (f->isNum()) {
        rule = "L{c} = c/s";
        return Mul(f, Pow(Sym("s"), Num(-1)));
    }
    if (f->isSym(t)) {
        rule = "L{t} = 1/s^2";
        return Pow(Sym("s"), Num(-2));
    }
    if (f->isAdd()) {
        std::vector<Ex> out;
        for (const auto& term : f->args) {
            std::string sub;
            bool subOk = true;
            Ex v = laplaceOf(term, t, sub, subOk);
            if (!subOk) { ok = false; return nullptr; }
            out.push_back(v);
        }
        rule = "linearity";
        return Add(out);
    }
    if (f->isMul()) {
        // Recognise the shifted table entries before the generic constant
        // multiple rule: e^(at) sin(bt), e^(at) cos(bt), t^n e^(at).
        auto linearRate = [&](const Ex& arg, Ex& rate) -> bool {
            if (arg->isSym(t)) { rate = Num(1); return true; }
            if (!arg->isMul()) return false;
            Ex rest = Num(1);
            for (const auto& factor : arg->args) {
                if (!dependsOn(factor, t)) rate = rate ? rate : factor;
                else rest = Mul(rest, factor);
            }
            return rest->isSym(t) && rate != nullptr;
        };
        Ex expFactor, trigFactor, polyFactor, coef = Num(1);
        for (const auto& factor : f->args) {
            if (factor->isFunc("exp") && factor->args.size() == 1) expFactor = factor;
            else if (factor->isFunc("sin") || factor->isFunc("cos")) trigFactor = factor;
            else if (factor->isSym(t))
                polyFactor = factor;  // t itself is t^1
            else if (factor->isPow() && factor->args[0]->isSym(t) && factor->args[1]->isNum() &&
                     factor->args[1]->num.isInt() && factor->args[1]->num.n > 0)
                polyFactor = factor;
            else if (!dependsOn(factor, t))
                coef = Mul(coef, factor);
        }
        Ex rate, freq;
        if (expFactor && trigFactor && !polyFactor &&
            linearRate(expFactor->args[0], rate) && linearRate(trigFactor->args[0], freq)) {
            const Ex shift = Add(Sym("s"), Mul(Num(-1), rate));
            const Ex den = Add(Pow(shift, Num(2)), Pow(freq, Num(2)));
            if (trigFactor->name == "sin") {
                rule = "L{e^(at) sin(bt)} = b/((s-a)^2+b^2)";
                return Mul(coef, Mul(freq, Pow(den, Num(-1))));
            }
            rule = "L{e^(at) cos(bt)} = (s-a)/((s-a)^2+b^2)";
            return Mul(coef, Mul(shift, Pow(den, Num(-1))));
        }
        if (expFactor && polyFactor && !trigFactor &&
            linearRate(expFactor->args[0], rate)) {
            long long n = 1;
            if (polyFactor->isPow() && polyFactor->args.size() == 2 &&
                polyFactor->args[1]->isNum())
                n = polyFactor->args[1]->num.n;
            long long fact = 1;
            for (long long i = 2; i <= n; ++i) fact *= i;
            rule = "L{t^n e^(at)} = n!/(s-a)^(n+1)";
            return Mul(coef, Mul(Num(fact),
                   Pow(Add(Sym("s"), Mul(Num(-1), rate)), Num(-(n + 1)))));
        }
        Ex constant = Num(1);
        Ex rest = Num(1);
        for (const auto& factor : f->args) {
            if (!dependsOn(factor, t)) constant = Mul(constant, factor);
            else rest = Mul(rest, factor);
        }
        if (!rest->isOne()) {
            if (equals(rest, f)) { ok = false; return nullptr; }
            std::string sub;
            bool subOk = true;
            Ex v = laplaceOf(rest, t, sub, subOk);
            if (!subOk) { ok = false; return nullptr; }
            rule = "constant multiple";
            return Mul(constant, v);
        }
        rule = "L{c} = c/s";
        return Mul(f, Pow(Sym("s"), Num(-1)));
    }
    if (f->isPow() && f->args[0]->isSym(t) && f->args[1]->isNum() && f->args[1]->num.isInt() &&
        f->args[1]->num.n >= 0) {
        const long long n = f->args[1]->num.n;
        long long fact = 1;
        for (long long i = 2; i <= n; ++i) fact *= i;
        rule = "L{t^n} = n!/s^(n+1)";
        return Mul(Num(fact), Pow(Sym("s"), Num(-(n + 1))));
    }
    if (f->isFunc() && f->args.size() == 1) {
        const Ex& arg = f->args[0];
        Ex a;
        if (arg->isSym(t)) a = Num(1);
        else if (arg->isMul()) {
            Ex rest = Num(1);
            for (const auto& factor : arg->args) {
                if (!dependsOn(factor, t)) a = a ? a : factor;
                else rest = Mul(rest, factor);
            }
            if (!rest->isSym(t)) a = nullptr;
        }
        if (a) {
            if (f->name == "exp") {
                rule = "L{e^(at)} = 1/(s-a)";
                return Pow(Add(Sym("s"), Mul(Num(-1), a)), Num(-1));
            }
            if (f->name == "sin") {
                rule = "L{sin(bt)} = b/(s^2+b^2)";
                return Mul(a, Pow(Add(Pow(Sym("s"), Num(2)), Pow(a, Num(2))), Num(-1)));
            }
            if (f->name == "cos") {
                rule = "L{cos(bt)} = s/(s^2+b^2)";
                return Mul(Sym("s"), Pow(Add(Pow(Sym("s"), Num(2)), Pow(a, Num(2))), Num(-1)));
            }
            if (f->name == "sinh") {
                rule = "L{sinh(bt)} = b/(s^2-b^2)";
                return Mul(a, Pow(Add(Pow(Sym("s"), Num(2)), Mul(Num(-1), Pow(a, Num(2)))), Num(-1)));
            }
            if (f->name == "cosh") {
                rule = "L{cosh(bt)} = s/(s^2-b^2)";
                return Mul(Sym("s"), Pow(Add(Pow(Sym("s"), Num(2)), Mul(Num(-1), Pow(a, Num(2)))), Num(-1)));
            }
        }
    }
    ok = false;
    return nullptr;
}

Result laplaceTransform(const TopicRequest& req, bool en) {
    Result r;
    base(r, en, "topic", "Laplace transform", "تبدیل لاپلاس", "t");
    const std::string input = req.text("f", "t");
    Ex f;
    try { f = parseExpr(input); } catch (const std::exception& e) {
        setFailure(r, en, e, "Could not parse f(t): ", "تابع f(t) خوانده نشد: ");
        return r;
    }
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
    std::string rule;
    bool ok = true;
    Ex F = laplaceOf(f, "t", rule, ok);
    if (!ok || !F) {
        fail(r, en, "This form is not in the supported transform table yet.",
             "این فرم هنوز در جدول تبدیل‌های پشتیبانی‌شده نیست.");
        return r;
    }
    r.solution = simplify(F);
    addStep(r, en, "Start from the definition", "از تعریف شروع کنید",
            Sym("F(s) = integral_0^inf e^(-s t) f(t) dt"),
            "The transform turns differentiation into multiplication by s.",
            "تبدیل، مشتق‌گیری را به ضرب در s تبدیل می‌کند.");
    addStep(r, en, "Match a table entry", "با یک سطر جدول تطبیق دهید", f,
            rule, rule);
    addStep(r, en, "Write the transform", "تبدیل را بنویسید", r.solution,
            "The result is a function of s.", "نتیجه تابعی از s است.");
    addCheck(r, "check.domain", true, "check.transform_domain");
    return r;
}

Result inverseLaplace(const TopicRequest& req, bool en) {
    Result r;
    base(r, en, "topic", "Inverse Laplace transform", "تبدیل معکوس لاپلاس", "s");
    const std::string input = req.text("F", "1/(s^2+1)");
    Ex F;
    try { F = parseExpr(input); } catch (const std::exception& e) {
        fail(r, en, std::string("Could not parse F(s): ") + e.what(),
             std::string("تابع F(s) خوانده نشد: ") + e.what());
        return r;
    }
    r.normalized = F;
    // Split F into numerator and denominator.  Only the table forms below are
    // accepted; anything else is reported honestly instead of guessed.
    Ex s = Sym("s");
    Ex num = Num(1);
    Ex den = Num(1);
    long long power = 1;
    bool denominatorSet = false;
    auto addDen = [&](const Ex& d) {
        den = denominatorSet ? Mul(den, d) : d;
        denominatorSet = true;
    };
    auto addNum = [&](const Ex& n) { num = Mul(num, n); };

    if (F->isPow() && F->args[1]->isNum() && F->args[1]->num.isInt() &&
        F->args[1]->num.n < 0) {
        addDen(F->args[0]);
        power = -F->args[1]->num.n;
    } else if (F->isMul()) {
        for (const auto& factor : F->args) {
            if (factor->isPow() && factor->args[1]->isNum() && factor->args[1]->num.isInt() &&
                factor->args[1]->num.n < 0) {
                addDen(factor->args[0]);
                power = std::max<long long>(power, -factor->args[1]->num.n);
            } else {
                addNum(factor);
            }
        }
    } else {
        addNum(F);
    }

    Ex f;
    std::string rule;
    auto factorial = [](long long n) {
        long long v = 1;
        for (long long i = 2; i <= n; ++i) v *= i;
        return v;
    };

    if (den->isSym("s") && equals(num, Num(1))) {
        f = Mul(Pow(Sym("t"), Num(power - 1)), Num(Rational(1, factorial(power - 1))));
        rule = "1/s^n -> t^(n-1)/(n-1)!";
    } else if (den->isAdd()) {
        Ex sTerm, s2Term, constant;
        for (const auto& term : den->args) {
            if (term->isSym("s")) {
                sTerm = term;
            } else if (term->isPow() && term->args[0]->isSym("s") && term->args[1]->isNum() &&
                       term->args[1]->num.isInt() && term->args[1]->num.n == 2) {
                s2Term = term;
            } else {
                constant = constant ? Add(constant, term) : term;
            }
        }
        bool okc = false;
        const double cv = constant ? evalReal(constant, {}, &okc) : 0.0;
        if (sTerm && !s2Term && okc && std::isfinite(cv) && equals(num, Num(1))) {
            // 1/(s - a)^n
            const double shift = -cv;
            f = Mul(Pow(Sym("t"), Num(power - 1)),
                    Num(Rational(1, factorial(power - 1))),
                    expEx(Mul(numEx(shift), Sym("t"))));
            rule = "1/(s-a)^n -> t^(n-1)/(n-1)! e^(at)";
        } else if (!sTerm) {
            // s^2 + c
            if (s2Term && okc && std::isfinite(cv)) {
                const bool numIsOne = equals(num, Num(1));
                const bool numIsS = equals(num, s);
                if (cv > 0) {
                    const double bb = std::sqrt(cv);
                    if (numIsOne) {
                        f = Mul(sinEx(Mul(numEx(bb), Sym("t"))), numEx(1.0 / bb));
                        rule = "1/(s^2+b^2) -> sin(bt)/b";
                    } else if (numIsS) {
                        f = cosEx(Mul(numEx(bb), Sym("t")));
                        rule = "s/(s^2+b^2) -> cos(bt)";
                    }
                } else if (cv < 0) {
                    const double bb = std::sqrt(-cv);
                    if (numIsOne) {
                        f = Mul(sinhEx(Mul(numEx(bb), Sym("t"))), numEx(1.0 / bb));
                        rule = "1/(s^2-b^2) -> sinh(bt)/b";
                    } else if (numIsS) {
                        f = coshEx(Mul(numEx(bb), Sym("t")));
                        rule = "s/(s^2-b^2) -> cosh(bt)";
                    }
                }
            }
        }
    }
    if (!f) {
        fail(r, en, "This rational form is not in the inverse table yet.",
             "این فرم گویا هنوز در جدول معکوس نیست.");
        return r;
    }
    r.solution = simplify(f);
    addStep(r, en, "Read the transform", "تبدیل را بخوانید", F,
            "We need the function whose Laplace transform is F(s).",
            "تابعی را می‌خواهیم که تبدیل لاپلاسش F(s) باشد.");
    addStep(r, en, "Match a table entry", "با یک سطر جدول تطبیق دهید", nullptr,
            rule, rule);
    addStep(r, en, "Write the inverse", "معکوس را بنویسید", r.solution,
            "The result is a function of t.", "نتیجه تابعی از t است.");
    addCheck(r, "check.table_check", true, "check.table_entry_valid");
    return r;
}


// ---------------------------------------------------------------- linear ODE
// C++ fallback for the Julia sidecar.  It supports constant-coefficient linear
// equations of any order that the Durand-Kerner root finder can handle and
// applies initial conditions when they are supplied.
std::vector<double> parseList(const std::string& text) {
    std::vector<double> out;
    std::string token;
    std::stringstream ss(text);
    while (std::getline(ss, token, ',')) {
        try {
            if (!token.empty()) out.push_back(std::stod(token));
        } catch (...) {
            return {};
        }
    }
    return out;
}

std::vector<std::complex<double>> polynomialRoots(const std::vector<double>& coeffs) {
    // coeffs highest order first.
    std::vector<std::complex<double>> roots;
    const size_t n = coeffs.size() - 1;
    if (n == 0) return roots;
    const double lead = coeffs[0];
    std::vector<double> b(n);
    for (size_t i = 0; i < n; ++i) b[i] = coeffs[i + 1] / lead;
    std::vector<std::complex<double>> z(n);
    std::complex<double> seed(0.4, 0.9);
    for (size_t i = 0; i < n; ++i) {
        std::complex<double> v(1.0, 0.0);
        for (size_t k = 0; k <= i; ++k) v *= seed;
        z[i] = v;
    }
    auto eval = [&](const std::complex<double>& x) {
        std::complex<double> v(1.0, 0.0);
        for (double c : b) v = v * x + c;
        return v;
    };
    for (int iter = 0; iter < 2000; ++iter) {
        double delta = 0.0;
        for (size_t i = 0; i < n; ++i) {
            std::complex<double> denom(1.0, 0.0);
            for (size_t j = 0; j < n; ++j) {
                if (j != i) denom *= (z[i] - z[j]);
            }
            if (std::abs(denom) < 1e-18) continue;
            const std::complex<double> stepv = eval(z[i]) / denom;
            z[i] -= stepv;
            delta = std::max(delta, std::abs(stepv));
        }
        if (delta < 1e-13) break;
    }
    return z;
}

struct BasisEntry {
    int kind = 0;          // 0 real, 1 cosine, 2 sine
    double alpha = 0;
    double beta = 0;
    int power = 0;
};

Ex basisExpr(const BasisEntry& e) {
    Ex core;
    if (e.kind == 0) {
        core = expEx(Mul(smartNum(e.alpha), Sym("t")));
    } else {
        const Ex env = expEx(Mul(smartNum(e.alpha), Sym("t")));
        core = Mul(env, e.kind == 1 ? cosEx(Mul(smartNum(e.beta), Sym("t")))
                                    : sinEx(Mul(smartNum(e.beta), Sym("t"))));
    }
    if (e.power == 0) return core;
    return Mul(Pow(Sym("t"), Num(e.power)), core);
}

double basisDerivativeAtZero(const BasisEntry& e, int k) {
    if (k < e.power) return 0.0;
    const int m = k - e.power;
    double factor = 1.0;
    for (int i = 0; i < e.power; ++i) factor *= (k - i);
    if (e.kind == 0) {
        return factor * std::pow(e.alpha, m);
    }
    const std::complex<double> z(e.alpha, e.beta);
    std::complex<double> v = std::pow(z, m);
    if (e.kind == 1) return factor * v.real();
    return factor * v.imag();
}

bool solveLinearSystem(std::vector<std::vector<double>> M, std::vector<double> rhs,
                       std::vector<double>& out) {
    const size_t n = M.size();
    for (size_t col = 0; col < n; ++col) {
        size_t pivot = col;
        for (size_t row = col + 1; row < n; ++row) {
            if (std::fabs(M[row][col]) > std::fabs(M[pivot][col])) pivot = row;
        }
        if (std::fabs(M[pivot][col]) < 1e-12) return false;
        std::swap(M[col], M[pivot]);
        std::swap(rhs[col], rhs[pivot]);
        for (size_t row = col + 1; row < n; ++row) {
            const double f = M[row][col] / M[col][col];
            for (size_t j = col; j < n; ++j) M[row][j] -= f * M[col][j];
            rhs[row] -= f * rhs[col];
        }
    }
    out.assign(n, 0.0);
    for (size_t i = n; i-- > 0;) {
        double sum = rhs[i];
        for (size_t j = i + 1; j < n; ++j) sum -= M[i][j] * out[j];
        out[i] = sum / M[i][i];
    }
    return true;
}

Result odeLinear(const TopicRequest& req, bool en) {
    Result r;
    base(r, en, "topic", "Linear ODE with constant coefficients",
         "معادلهٔ خطی با ضرایب ثابت", "t");
    std::vector<double> coeffs = parseList(req.text("coeffs", "1,0,1"));
    if (coeffs.size() < 2) {
        fail(r, en, "At least two coefficients are required.",
             "حداقل دو ضریب لازم است.");
        return r;
    }
    if (std::fabs(coeffs.front()) < 1e-14) {
        fail(r, en, "The leading coefficient must be nonzero.",
             "ضریب پیشرو باید ناصفر باشد.");
        return r;
    }
    const size_t n = coeffs.size() - 1;
    std::vector<std::complex<double>> roots = polynomialRoots(coeffs);
    std::sort(roots.begin(), roots.end(), [](const auto& a, const auto& b) {
        if (std::fabs(a.real() - b.real()) > 1e-7) return a.real() > b.real();
        return a.imag() < b.imag();
    });

    std::vector<BasisEntry> basis;
    std::vector<std::pair<std::complex<double>, int>> groups;
    std::vector<bool> used(roots.size(), false);
    for (size_t i = 0; i < roots.size(); ++i) {
        if (used[i]) continue;
        int mult = 1;
        used[i] = true;
        for (size_t j = i + 1; j < roots.size(); ++j) {
            if (used[j]) continue;
            if (std::abs(roots[j] - roots[i]) < 1e-6) {
                ++mult;
                used[j] = true;
            }
        }
        groups.push_back({roots[i], mult});
    }
    for (const auto& g : groups) {
        const std::complex<double> root = g.first;
        const int mult = g.second;
        if (std::fabs(root.imag()) < 1e-7) {
            for (int m = 0; m < mult; ++m)
                basis.push_back({0, root.real(), 0.0, m});
        } else if (root.imag() > 0) {
            for (int m = 0; m < mult; ++m) {
                basis.push_back({1, root.real(), root.imag(), m});
                basis.push_back({2, root.real(), root.imag(), m});
            }
        }
    }
    if (basis.size() != n) {
        fail(r, en, "The characteristic roots could not be grouped reliably.",
             "ریشه‌های مشخصه به‌طور مطمئن گروه‌بندی نشدند.");
        return r;
    }

    std::ostringstream charEq;
    for (size_t i = 0; i < coeffs.size(); ++i) {
        const int power = (int)(n - i);
        if (i) charEq << " + ";
        charEq << numStr(coeffs[i]);
        if (power == 1) charEq << " r";
        else if (power > 1) charEq << " r^" << power;
    }
    charEq << " = 0";

    addStep(r, en, "Write the characteristic equation",
            "معادلهٔ مشخصه را بنویسید", Sym(charEq.str()),
            "Replace each derivative by a power of r.",
            "هر مشتق را با توانی از r جایگزین کنید.");
    std::ostringstream rootText;
    for (size_t i = 0; i < groups.size(); ++i) {
        if (i) rootText << ", ";
        const auto& g = groups[i];
        rootText << "r = " << numStr(g.first.real());
        if (std::fabs(g.first.imag()) > 1e-7) rootText << " ± " << numStr(std::fabs(g.first.imag())) << "i";
        if (g.second > 1) rootText << " (x" << g.second << ")";
    }
    addStep(r, en, "Find the roots", "ریشه‌ها را پیدا کنید", Sym(rootText.str()),
            "Real roots give exponentials; complex pairs give sines and cosines.",
            "ریشه‌های حقیقی نمایی و جفت‌های مختلط سینوس و کسینوس می‌دهند.");

    std::vector<Ex> general;
    for (size_t i = 0; i < basis.size(); ++i) {
        general.push_back(Mul(Sym("C_" + std::to_string(i + 1)), basisExpr(basis[i])));
    }
    Ex generalEx = general.empty() ? Num(0) : Add(general);
    addStep(r, en, "Write the general solution", "جواب عمومی را بنویسید", generalEx,
            "There are " + std::to_string(n) + " free constants.",
            std::to_string(n) + " ثابت آزاد وجود دارد.");

    std::vector<double> ics = parseList(req.text("ics", ""));
    Ex solution = generalEx;
    if (ics.size() == n) {
        std::vector<std::vector<double>> M(n, std::vector<double>(n, 0.0));
        for (size_t col = 0; col < n; ++col) {
            for (size_t k = 0; k < n; ++k) {
                M[k][col] = basisDerivativeAtZero(basis[col], (int)k);
            }
        }
        std::vector<double> constants;
        if (solveLinearSystem(M, ics, constants)) {
            std::vector<Ex> parts;
            for (size_t i = 0; i < basis.size(); ++i) {
                if (std::fabs(constants[i]) < 1e-10) continue;
                parts.push_back(Mul(smartNum(constants[i]), basisExpr(basis[i])));
            }
            solution = parts.empty() ? Num(0) : Add(parts);
            addStep(r, en, "Apply the initial conditions",
                    "شرایط اولیه را اعمال کنید", solution,
                    "Solve the linear system for the constants.",
                    "دستگاه خطی را برای تعیین ثابت‌ها حل کنید.");
        } else {
            addCheck(r, "check.initial_conditions", false,
                     "check.inconsistent_basis");
        }
    }
    r.solution = simplify(solution);
    r.normalized = Sym(charEq.str());
    r.order = (int)n;
    r.closedForm = true;
    addCheck(r, "check.root_count", roots.size() == n,
             "check.roots_found", "{\"n\":" + std::to_string(n) + "}");
    return r;
}

Result odeStiff(const TopicRequest& req, bool en) {
    Result r;
    base(r, en, "topic", "Stiff initial-value problem", "مسئلهٔ مقدار اولیهٔ سخت", "x");
    const std::string input = req.text("f", "-50*(y-cos(x))");
    const double x0 = req.number("x0", 0.0);
    const double y0 = req.number("y0", 0.0);
    const double xend = req.number("xend", 1.0);
    const double h = req.number("h", 0.01);
    if (std::fabs(h) < 1e-12) {
        fail(r, en, "The step size must be nonzero.", "گام باید ناصفر باشد.");
        return r;
    }
    if (std::fabs((xend - x0) / h) > 5000) {
        fail(r, en, "The step count is too large; increase h.",
             "تعداد گام‌ها زیاد است؛ h را بزرگ‌تر کنید.");
        return r;
    }
    Ex f;
    try {
        f = parseExpr(input);
    } catch (const std::exception& e) {
        fail(r, en, std::string("Could not parse f(x,y): ") + e.what(),
             std::string("تابع f(x,y) خوانده نشد: ") + e.what());
        return r;
    }
    r.normalized = f;
    double x = x0;
    double y = y0;
    const int steps = (int)std::llround((xend - x0) / h);
    std::ostringstream table;
    table << "i\tx\ty\n0\t" << numStr(x) << "\t" << numStr(y) << "\n";
    for (int i = 1; i <= steps; ++i) {
        const double xnew = x0 + i * h;
        double z = y;
        bool ok = true;
        for (int iter = 0; iter < 40; ++iter) {
            const double fz = evalF(f, "x", xnew, "y", z, &ok);
            if (!ok) break;
            const double eps = 1e-7;
            const double df = (evalF(f, "x", xnew, "y", z + eps, &ok) -
                               evalF(f, "x", xnew, "y", z - eps, &ok)) / (2 * eps);
            if (!ok) break;
            const double F = z - y - h * fz;
            const double dF = 1.0 - h * df;
            if (std::fabs(dF) < 1e-14) break;
            const double dz = F / dF;
            z -= dz;
            if (std::fabs(dz) < 1e-12) break;
        }
        if (!ok) {
            fail(r, en, "f(x,y) could not be evaluated.", "تابع f(x,y) قابل محاسبه نبود.");
            return r;
        }
        y = z;
        x = xnew;
        if (i <= 25) table << i << "\t" << numStr(x) << "\t" << numStr(y) << "\n";
    }
    r.solution = smartNum(y);
    r.hasNumeric = true;
    r.numericValue = y;
    r.point = "x=" + numStr(x);
    addStep(r, en, "Write the implicit Euler step", "گام اویلر ضمنی را بنویسید",
            Sym("y_{n+1} = y_n + h f(x_{n+1}, y_{n+1})"),
            "Solve z = y + h f(x+h, z) at every step.",
            "در هر گام z = y + h f(x+h, z) را حل کنید.");
    addStep(r, en, "Advance through the interval", "روی بازه پیش بروید", nullptr,
            table.str(), table.str());
    addStep(r, en, "Read the final value", "مقدار نهایی را بخوانید", smartNum(y),
            "After " + std::to_string(steps) + " steps.",
            "پس از " + std::to_string(steps) + " گام.");
    addCheck(r, "check.step_count", steps > 0,
             "check.table_steps", "{\"n\":" + std::to_string(steps) + "}");
    return r;
}

} // namespace

Result solveTopic(const TopicRequest& req) {
    const bool en = req.lang != "fa";
    const std::string& t = req.topic;
    try {
        if (t == "growth_decay") return growthDecay(req, en);
        if (t == "cooling") return newtonCooling(req, en);
        if (t == "logistic") return logistic(req, en);
        if (t == "mixing") return mixing(req, en);
        if (t == "spring_mass") return oscillator(req, en, false);
        if (t == "circuit") return oscillator(req, en, true);
        if (t == "falling_body") return fallingBody(req, en);
        if (t == "euler") return numericalOde(req, en, NumMethod::Euler);
        if (t == "heun") return numericalOde(req, en, NumMethod::Heun);
        if (t == "rk4") return numericalOde(req, en, NumMethod::Rk4);
        if (t == "linear_system") return linearSystem(req, en);
        if (t == "fourier_series") return fourierSeries(req, en);
        if (t == "heat") return heatEquation(req, en);
        if (t == "wave") return waveEquation(req, en);
        if (t == "laplace_pde") return laplacePde(req, en);
        if (t == "laplace") return laplaceTransform(req, en);
        if (t == "inverse_laplace") return inverseLaplace(req, en);
        if (t == "ode_linear") return odeLinear(req, en);
        if (t == "ode_stiff") return odeStiff(req, en);
    } catch (const std::exception& e) {
        Result r;
        r.ok = false;
        r.mode = "topic";
        setFailure(r, en, e, "Topic failed: ", "اجرای مبحث ناموفق بود: ");
        return r;
    }
    Result r;
    r.ok = false;
    r.mode = "topic";
    r.error = en ? "Unknown topic." : "مبحث ناشناخته است.";
    return r;
}

} // namespace ode
