// Infinit Lab — single/multiple variable calculus API
// (integrals: indefinite, definite, double and triple; derivatives: orders 1–3
//  and implicit differentiation)
#include "numeric.hpp"
// SPDX-License-Identifier: Apache-2.0
#include "calculus_ext.hpp"
#include "limits.hpp"
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

std::string methodEn(const std::string& id) {
    if (id == "substitution") return "Substitution";
    if (id == "parts") return "Integration by parts";
    if (id == "rational") return "Rational functions";
    if (id == "trig") return "Trigonometric powers";
    if (id == "expand") return "Polynomial expansion";
    if (id == "sum") return "Term-by-term";
    if (id == "constant") return "Constant";
    if (id == "table") return "Standard table";
    return "Standard methods";
}

std::string methodFa(const std::string& id) {
    if (id == "substitution") return "تغییر متغیر";
    if (id == "parts") return "جزء‌به‌جزء";
    if (id == "rational") return "توابع گویا";
    if (id == "trig") return "توان‌های مثلثاتی";
    if (id == "expand") return "بسط چندجمله‌ای";
    if (id == "sum") return "جمله‌به‌جمله";
    if (id == "constant") return "تابع ثابت";
    if (id == "table") return "جدول استاندارد";
    return "روش‌های استاندارد";
}

std::string fmtReal(double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.12g", v);
    return buf;
}

std::string ruleName(const std::string& id, bool en) {
    if (id == "sum") return en ? "sum rule" : "قاعدهٔ جمع";
    if (id == "product") return en ? "product rule" : "قاعدهٔ ضرب";
    if (id == "product_general") return en ? "generalized product rule" : "قاعدهٔ ضرب تعمیم‌یافته";
    if (id == "constant_multiple") return en ? "constant multiple" : "ضریب ثابت";
    if (id == "power") return en ? "power rule" : "قاعدهٔ توان";
    if (id == "exponential") return en ? "exponential rule" : "قاعدهٔ نمایی";
    if (id == "general_power") return en ? "logarithmic differentiation" : "مشتق‌گیری لگاریتمی";
    if (id == "chain") return en ? "chain rule" : "قاعدهٔ زنجیره‌ای";
    if (id == "log_abs") return en ? "logarithm of an absolute value" : "لگاریتم قدر مطلق";
    if (id == "absolute_value") return en ? "absolute value" : "قدر مطلق";
    if (id == "formal_derivative") return en ? "formal derivative" : "مشتق صوری";
    if (id == "derivative_node") return en ? "derivative node" : "گرهٔ مشتق";
    if (id == "fundamental_theorem") return en ? "fundamental theorem" : "قضیهٔ اساسی";
    if (id == "variable") return en ? "variable" : "متغیر";
    if (id == "constant") return en ? "constant" : "ثابت";
    return id;
}

std::string boundText(const Ex& e) { return e ? toPlain(e) : std::string("?"); }

// Numeric value of the integrand at a point (used by the verification pass).
double evalAt(const Ex& f, const std::map<std::string, double>& env) {
    bool ok = false;
    const double v = evalReal(f, env, &ok);
    return ok ? v : std::nan("");
}

std::string integralTypeFa(int dimension, bool definite) {
    std::string base = dimension <= 1 ? "انتگرال یگانه"
                       : dimension == 2 ? "انتگرال دوگانه"
                                        : "انتگرال سه‌گانه";
    if (definite) base += "ٔ معین";
    return base;
}

} // namespace

Result integrateExpression(const std::string& input, const std::string& x,
                           const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "integral";
    r.typeId = "integral";
    r.typeFa = "انتگرال نامعین";
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
        r.steps.push_back({en ? "Method detection" : "تشخیص روش", nullptr,
                           en ? ("Detected method: " + methodEn(method))
                              : ("روش تشخیص‌داده‌شده: " + methodFa(method))});
        r.steps.push_back({en ? "Indefinite integral" : "انتگرال نامعین", F,
                           en ? "The constant of integration C is added by convention."
                              : "ثابت انتگرال‌گیری C طبق قرارداد اضافه می‌شود."});
        if (!closed) {
            r.warnings.push_back(en
                ? "No elementary antiderivative was found; the result is an exact quadrature."
                : "پادمشتق مقدماتی پیدا نشد؛ جواب به صورت کوادراتور دقیق ارائه شده است.");
        } else {
            try {
                Ex residual = simplify(Add(diff(F, x), Mul(Num(-1), f)));
                bool ok = residual->isZero() || numericallyZero(residual, {x}, 8, 1e-6);
                r.checks.push_back({en ? "Differentiate the result" : "مشتق‌گیری از جواب", ok,
                                    en ? (ok ? "The derivative equals the integrand."
                                             : "The derivative did not simplify to the integrand.")
                                       : (ok ? "مشتق جواب برابر انتگرال‌ده است."
                                             : "مشتق جواب به انتگرال‌ده ساده نشد.")});
            } catch (...) {
                r.checks.push_back({en ? "Differentiate the result" : "مشتق‌گیری از جواب", false,
                                    en ? "Automatic verification was not possible."
                                       : "بررسی خودکار ممکن نشد."});
            }
        }
        r.ok = true;
    } catch (const std::exception& e) {
        r.ok = false;
        r.error = (en ? std::string("Integration error: ") : std::string("خطای انتگرال‌گیری: ")) + e.what();
    }
    return r;
}

namespace {

// Definite integral of one variable: exact antiderivative evaluation first, then
// adaptive Gauss–Kronrod quadrature when no elementary antiderivative exists.
Result integrateDefiniteSingle(const std::string& input, const std::string& x,
                               const std::string& loText, const std::string& hiText,
                               const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "integral";
    r.typeId = "integral_definite";
    r.typeFa = "انتگرال معین";
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
        r.steps.push_back({en ? "Integrand" : "انتگرال‌ده", f,
                           en ? "Integrate with respect to " + x + "."
                              : "انتگرال‌گیری نسبت به " + x + "."});
        r.steps.push_back({en ? "Bounds" : "کران‌ها", nullptr,
                           (en ? "From " : "از ") + boundText(lo) + (en ? " to " : " تا ") +
                               boundText(hi)});

        bool closed = false;
        std::string method;
        Ex F = integrate(f, x, &closed, &method);
        bool exact = closed && !hasUnevaluatedIntegral(F);

        double numeric = std::nan("");
        {
            bool okLo = false, okHi = false;
            const double a = evalReal(lo, {}, &okLo);
            const double b = evalReal(hi, {}, &okHi);
            if (okLo && okHi && std::isfinite(a) && std::isfinite(b)) {
                numeric = quadratureAdaptive([&](double t) { return evalAt(f, {{x, t}}); }, a, b,
                                             1e-10);
            }
        }

        if (exact) {
            Ex val = simplify(Add(substitute(F, x, hi), Mul(Num(-1), substitute(F, x, lo))));
            bool ok = false;
            const double value = evalReal(val, {}, &ok);
            if (!ok || !std::isfinite(value)) {
                exact = false;
            } else {
                r.solution = val;
                r.explicitY = val;
                r.isExplicit = true;
                r.method = "definite";
                r.closedForm = true;
                r.hasNumeric = true;
                r.numericValue = value;
                r.steps.push_back({en ? "Antiderivative" : "پادمشتق", F,
                                   en ? ("Method: " + methodEn(method))
                                      : ("روش: " + methodFa(method))});
                r.steps.push_back({en ? "Apply the bounds" : "جای‌گذاری کران‌ها", val,
                                   "F(" + boundText(hi) + ") − F(" + boundText(lo) + ")"});
                if (std::isfinite(numeric)) {
                    const double scale = std::max(1.0, std::fabs(numeric));
                    const bool match = std::fabs(numeric - value) <= 1e-6 * scale;
                    r.checks.push_back(
                        {en ? "Quadrature cross-check" : "بررسی با کوادراتور", match,
                         en ? ("Independent quadrature gives " + fmtReal(numeric) + ".")
                            : ("کوادراتور مستقل مقدار " + fmtReal(numeric) + " را می‌دهد.")});
                    if (!match)
                        r.warnings.push_back(
                            en ? "The exact value and the numeric quadrature disagree; the "
                                 "integrand may be singular on the interval."
                               : "مقدار دقیق و کوادراتور عددی با هم نمی‌خوانند؛ ممکن است "
                                 "انتگرال‌ده روی بازه تکین باشد.");
                }
            }
        }

        if (!exact) {
            if (!std::isfinite(numeric)) throw std::runtime_error("quadrature failed");
            r.method = "numeric";
            r.closedForm = false;
            r.hasNumeric = true;
            r.numericValue = numeric;
            // No closed form: the answer is the numeric value itself, so no
            // symbolic solution is published (a rounded fraction would look exact).
            r.solution = nullptr;
            r.explicitY = nullptr;
            r.isExplicit = false;
            r.steps.push_back(
                {en ? "Quadrature" : "کوادراتور", nullptr,
                 en ? ("No elementary antiderivative; adaptive Gauss–Kronrod gives " +
                       fmtReal(numeric) + ".")
                    : ("پادمشتق مقدماتی موجود نیست؛ گاوس–کرانرود تطبیقی مقدار " +
                       fmtReal(numeric) + " را می‌دهد.")});
            r.warnings.push_back(en
                ? "The answer is a high-accuracy numerical value, not a closed form."
                : "جواب یک مقدار عددی با دقت بالا است، نه فرم بسته.");
        }
        r.ok = true;
    } catch (const std::exception& e) {
        r.ok = false;
        r.error = (en ? std::string("Definite integral error: ")
                      : std::string("خطای انتگرال معین: ")) + e.what();
    }
    return r;
}

} // namespace

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
        r.typeFa = integralTypeFa((int)vars.size(), false);
        r.engine = "native";
        r.variable = vars[0];
        r.vars = vars;
        r.dimension = (int)vars.size();
        r.definite = false;
        try {
            Ex f = parseExpr(input);
            r.normalized = f;
            r.steps.push_back({en ? "Integrand" : "انتگرال‌ده", f,
                               en ? "Iterated integral, innermost variable first."
                                  : "انتگرال تکراری، از درونی‌ترین متغیر آغاز می‌شود."});
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
                r.steps.push_back(
                    {en ? ("Integrate with respect to " + v) : ("انتگرال نسبت به " + v), g,
                     en ? ("Method: " + methodEn(method) + "; one constant per variable is added.")
                        : ("روش: " + methodFa(method) + "؛ برای هر متغیر یک ثابت اضافه می‌شود.")});
            }
            r.solution = g;
            r.explicitY = g;
            r.isExplicit = true;
            r.method = lastMethod;
            r.closedForm = closedAll;
            if (!closedAll)
                r.warnings.push_back(en ? "At least one stage has no elementary antiderivative."
                                        : "حداقل یک مرحله پادمشتق مقدماتی ندارد.");
            r.steps.push_back({en ? "Result" : "نتیجه", g,
                               en ? "Add one integration constant per variable."
                                  : "برای هر متغیر یک ثابت انتگرال‌گیری اضافه می‌شود."});
            r.ok = true;
        } catch (const std::exception& e) {
            r.ok = false;
            r.error = (en ? std::string("Integration error: ") : std::string("خطای انتگرال‌گیری: ")) + e.what();
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
    r.typeFa = integralTypeFa((int)vars.size(), true);
    r.engine = "native";
    r.variable = vars[0];
    r.vars = vars;
    r.dimension = (int)vars.size();
    r.definite = true;
    r.lower = lower;
    r.upper = upper;
    if (lower.size() != vars.size() || upper.size() != vars.size()) {
        r.ok = false;
        r.error = en ? "Bounds are missing for the integration variables."
                     : "کران‌ها برای متغیرهای انتگرال‌گیری کامل نیست.";
        return r;
    }
    try {
        Ex f = parseExpr(input);
        r.normalized = f;
        r.steps.push_back({en ? "Integrand" : "انتگرال‌ده", f,
                           en ? "Iterated definite integral." : "انتگرال معین تکراری."});

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
            r.steps.push_back(
                {en ? ("Integrate over " + vars[i]) : ("انتگرال نسبت به " + vars[i]), g,
                 en ? ("From " + boundText(lo) + " to " + boundText(hi) + " (" + methodEn(method) + ").")
                    : ("از " + boundText(lo) + " تا " + boundText(hi) + " (" + methodFa(method) + ").")});
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
            r.steps.push_back({en ? "Value" : "مقدار", r.solution, fmtReal(value)});
        } else {
            std::vector<double> lo, hi;
            for (size_t i = 0; i < vars.size(); ++i) {
                bool a = false, b = false;
                const double la = evalReal(parseExpr(lower[i]), {}, &a);
                const double ub = evalReal(parseExpr(upper[i]), {}, &b);
                if (!a || !b || !std::isfinite(la) || !std::isfinite(ub))
                    throw std::runtime_error(en ? "the bounds must be numeric"
                                                : "کران‌ها باید عددی باشند");
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
            r.steps.push_back(
                {en ? "Nested quadrature" : "کوادراتور تودرتو", nullptr,
                 en ? ("No closed form at every stage; nested Gauss–Kronrod gives " +
                       fmtReal(value) + ".")
                    : ("در همهٔ مراحل فرم بسته پیدا نشد؛ گاوس–کرانرود تودرتو مقدار " +
                       fmtReal(value) + " را می‌دهد.")});
            r.warnings.push_back(en
                ? "The answer is a high-accuracy numerical value, not a closed form."
                : "جواب یک مقدار عددی با دقت بالا است، نه فرم بسته.");
        }
        r.ok = true;
    } catch (const std::exception& e) {
        r.ok = false;
        r.error = (en ? std::string("Multiple integral error: ")
                      : std::string("خطای انتگرال چندگانه: ")) + e.what();
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
    r.order = order < 1 ? 1 : (order > 3 ? 3 : order);
    r.variable = x;
    r.typeFa = r.order == 1 ? "مشتق مرتبهٔ اول"
                            : (r.order == 2 ? "مشتق مرتبهٔ دوم" : "مشتق مرتبهٔ سوم");
    try {
        const bool equation = input.find('=') != std::string::npos;
        if (equation) {
            // ------------------------------------------------ implicit form
            r.implicit = true;
            r.typeId = "derivative_implicit";
            r.typeFa = "مشتق ضمنی";
            Ex F = parseEquation(input);
            r.normalized = F;

            // The dependent variable is the symbol that is neither the variable of
            // differentiation nor one of the known constants.
            std::vector<std::string> syms;
            collectSymbols(F, syms);
            std::string dep;
            for (const auto& s : syms) {
                if (s == x || s == "pi" || s == "e") continue;
                dep = s;
                break;
            }
            if (dep.empty())
                throw std::runtime_error(en ? "the equation has no dependent variable"
                                            : "معادله متغیر وابسته ندارد");
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
            r.steps.push_back({en ? "Implicit equation" : "معادلهٔ ضمنی", F,
                               en ? "Differentiate both sides with respect to " + x + "."
                                  : "دو طرف را نسبت به " + x + " مشتق می‌گیریم."});
            r.steps.push_back({en ? "Partial derivative F_" + x : "مشتق جزئی F_" + x, Fx});
            r.steps.push_back({en ? "Partial derivative F_" + dep : "مشتق جزئی F_" + dep, Fy});
            r.steps.push_back({en ? "First derivative" : "مشتق اول", first,
                               dy + " = -F_" + x + " / F_" + dep});
            if (r.order >= 2)
                r.steps.push_back({en ? "Second derivative" : "مشتق دوم", d,
                                   en ? ("Differentiate " + dy + " again, treating " + dep +
                                         " as a function of " + x + ".")
                                      : ("از " + dy + " یک بار دیگر مشتق می‌گیریم و " + dep +
                                         " را تابعی از " + x + " می‌دانیم.")});
            if (r.order >= 3)
                r.steps.push_back({en ? "Third derivative" : "مشتق سوم", d,
                                   en ? "Apply the total derivative once more."
                                      : "بار دیگر مشتق کل را اعمال می‌کنیم."});

            // Consistency check: F_x + F_y·y' must vanish.
            try {
                Ex check = simplify(Add(Fx, Mul(Fy, first)));
                const bool ok = check->isZero() || numericallyZero(check, {x, dep}, 8, 1e-6);
                r.checks.push_back(
                    {en ? "Implicit relation" : "رابطهٔ ضمنی", ok,
                     en ? (ok ? "F_x + F_y·y' simplifies to zero."
                              : "F_x + F_y·y' did not vanish; the input may not be algebraic.")
                        : (ok ? "F_x + F_y·y' صفر می‌شود."
                              : "F_x + F_y·y' صفر نشد؛ ممکن است ورودی جبری نباشد.")});
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
        Ex d1, d2, d3;
        for (int k = 1; k <= r.order; ++k) {
            current = simplify(diffAst(current, x, &notes));
            if (k == 1) d1 = current;
            if (k == 2) d2 = current;
            if (k == 3) d3 = current;
        }
        r.solution = current;
        r.explicitY = current;
        r.isExplicit = true;
        r.closedForm = true;

        // Describe the AST walk: which structural rules produced the answer.
        std::string rules;
        for (const auto& n : notes) {
            if (n.depth > 2) continue;
            const std::string name = ruleName(n.rule, en);
            if (rules.find(name) != std::string::npos) continue;
            if (!rules.empty()) rules += en ? ", " : "، ";
            rules += name;
        }
        r.steps.push_back({en ? "Expression tree" : "درخت عبارت", f,
                           en ? "Walk the AST and differentiate node by node."
                              : "درخت عبارت پیمایش و گره‌به‌گره مشتق‌گیری می‌شود."});
        if (!rules.empty())
            r.steps.push_back({en ? "Rules applied" : "قواعد به‌کاررفته", nullptr,
                               en ? ("Rules: " + rules) : ("قواعد: " + rules)});
        if (d1) r.steps.push_back({en ? "First derivative" : "مشتق اول", d1});
        if (d2) r.steps.push_back({en ? "Second derivative" : "مشتق دوم", d2});
        if (d3) r.steps.push_back({en ? "Third derivative" : "مشتق سوم", d3});

        // Cross-check against the ODE engine's differentiator.
        try {
            Ex ref = f;
            for (int k = 0; k < r.order; ++k) ref = simplify(diff(ref, x));
            const bool ok = numericallyEqual(ref, current, {x}, 8, 1e-6);
            r.checks.push_back({en ? "Symbolic cross-check" : "بررسی نمادین", ok,
                                en ? (ok ? "An independent differentiator agrees."
                                         : "The independent differentiator disagrees.")
                                   : (ok ? "یک مشتق‌گیر مستقل همان جواب را می‌دهد."
                                         : "مشتق‌گیر مستقل جواب دیگری می‌دهد.")});
        } catch (...) {
        }
        r.ok = true;
    } catch (const std::exception& e) {
        r.ok = false;
        r.error = (en ? std::string("Derivative error: ") : std::string("خطای مشتق‌گیری: ")) + e.what();
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
    if (t == "\xE2\x88\x9E" || l == "inf" || l == "infinity" || l == "oo" || l == "INF") {
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
        default: return en ? "does not exist" : "وجود ندارد";
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

const char* lawEn(const std::string& id) {
    if (id == "constant") return "constant law";
    if (id == "identity") return "identity law";
    if (id == "sum") return "sum law";
    if (id == "product") return "product law";
    if (id == "quotient") return "quotient law";
    if (id == "power") return "power law";
    if (id == "composite") return "composition (continuity)";
    if (id == "lhopital") return "L'H\u00f4pital's rule";
    if (id == "algebraic_simplification") return "algebraic simplification";
    if (id == "exponential_rewrite") return "exponential rewrite";
    if (id == "two_sided") return "two-sided comparison";
    return "limit law";
}

const char* lawFa(const std::string& id) {
    if (id == "constant") return "\u0642\u0627\u0639\u062f\u0647\u0654 \u062b\u0627\u0628\u062a";
    if (id == "identity") return "\u0642\u0627\u0639\u062f\u0647\u0654 \u0645\u062a\u063a\u06cc\u0631";
    if (id == "sum") return "\u0642\u0627\u0639\u062f\u0647\u0654 \u062c\u0645\u0639";
    if (id == "product") return "\u0642\u0627\u0639\u062f\u0647\u0654 \u0636\u0631\u0628";
    if (id == "quotient") return "\u0642\u0627\u0639\u062f\u0647\u0654 \u062e\u0627\u0631\u062c\u200c\u0642\u0633\u0645\u062a";
    if (id == "power") return "\u0642\u0627\u0639\u062f\u0647\u0654 \u062a\u0648\u0627\u0646";
    if (id == "composite") return "\u062a\u0631\u06a9\u06cc\u0628 (\u067e\u06cc\u0648\u0633\u062a\u06af\u06cc)";
    if (id == "lhopital") return "\u0642\u0627\u0639\u062f\u0647\u0654 \u0647\u0648\u067e\u06cc\u062a\u0627\u0644";
    if (id == "algebraic_simplification") return "\u0633\u0627\u062f\u0647\u200c\u0633\u0627\u0632\u06cc \u062c\u0628\u0631\u06cc";
    if (id == "exponential_rewrite") return "\u0628\u0627\u0632\u0646\u0648\u06cc\u0633\u06cc \u0646\u0645\u0627\u06cc\u06cc";
    if (id == "two_sided") return "\u0645\u0642\u0627\u06cc\u0633\u0647\u0654 \u062f\u0648\u0637\u0631\u0641\u0647";
    return "\u0642\u0627\u0639\u062f\u0647\u0654 \u062d\u062f";
}

std::string lawsText(const std::vector<std::string>& laws, bool en) {
    std::string out;
    for (const auto& l : laws) {
        if (!out.empty()) out += en ? ", " : "\u060c ";
        out += en ? lawEn(l) : lawFa(l);
    }
    return out;
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

std::string sideText(const std::string& side, bool en) {
    if (side == "left") return en ? "from the left" : "\u0627\u0632 \u0633\u0645\u062a \u0686\u067e";
    if (side == "right") return en ? "from the right" : "\u0627\u0632 \u0633\u0645\u062a \u0631\u0627\u0633\u062a";
    return en ? "two-sided" : "\u062f\u0648\u0637\u0631\u0641\u0647";
}

} // namespace

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
    r.typeFa = r.side == "left" ? "\u062d\u062f \u0627\u0632 \u0686\u067e"
               : r.side == "right" ? "\u062d\u062f \u0627\u0632 \u0631\u0627\u0633\u062a"
                                   : "\u062d\u062f";
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

        r.steps.push_back({en ? "Expression" : "\u0639\u0628\u0627\u0631\u062a", f,
                           en ? "The expression whose limit is computed."
                              : "\u0639\u0628\u0627\u0631\u062a\u06cc \u06a9\u0647 \u062d\u062f \u0622\u0646 \u0645\u062d\u0627\u0633\u0628\u0647 \u0645\u06cc\u200c\u0634\u0648\u062f."});
        r.steps.push_back(
            {en ? "Approach" : "\u0646\u0632\u062f\u06cc\u06a9\u200c\u0634\u062f\u0646", nullptr,
             x + " \u2192 " + pointText(a) + "  (" + sideText(r.side, en) + ")"});

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
            r.steps.push_back({en ? "Left limit" : "\u062d\u062f \u0686\u067e", left.value,
                               outcomeText(left, en)});
            r.steps.push_back({en ? "Right limit" : "\u062d\u062f \u0631\u0627\u0633\u062a", right.value,
                               outcomeText(right, en)});
            const bool agree = outcomeSame(left, right);
            r.checks.push_back(
                {en ? "One-sided limits agree" : "\u062d\u062f\u0647\u0627\u06cc \u06cc\u06a9\u200c\u0637\u0631\u0641\u0647 \u0645\u0648\u0627\u0641\u0642\u0646\u062f", agree,
                 agree ? (en ? "The two one-sided limits are equal." : "\u062f\u0648 \u062d\u062f \u06cc\u06a9\u200c\u0637\u0631\u0641\u0647 \u0628\u0631\u0627\u0628\u0631\u0646\u062f.")
                       : (en ? "The one-sided limits differ, so the two-sided limit does not exist."
                             : "\u062d\u062f\u0647\u0627\u06cc \u06cc\u06a9\u200c\u0637\u0631\u0641\u0647 \u0645\u062a\u0641\u0627\u0648\u062a\u0646\u062f\u061b \u067e\u0633 \u062d\u062f \u062f\u0648\u0637\u0631\u0641\u0647 \u0648\u062c\u0648\u062f \u0646\u062f\u0627\u0631\u062f.")});
        }

        if (!laws.empty())
            r.steps.push_back({en ? "Limit laws applied" : "\u0642\u0648\u0627\u0646\u06cc\u0646 \u062d\u062f \u0628\u0647\u200c\u06a9\u0627\u0631\u0631\u0641\u062a\u0647", nullptr,
                               lawsText(laws, en)});

        if (L.kind == LimitOutcome::Finite) {
            r.steps.push_back({en ? "Result" : "\u0646\u062a\u06cc\u062c\u0647", L.value,
                               outcomeText(L, en)});
            if (L.numeric) {
                r.warnings.push_back(
                    en ? "This value was established numerically; no closed form was found."
                       : "\u0627\u06cc\u0646 \u0645\u0642\u062f\u0627\u0631 \u0628\u0647\u200c\u0635\u0648\u0631\u062a \u0639\u062f\u062f\u06cc \u0628\u0647\u200c\u062f\u0633\u062a \u0622\u0645\u062f\u0647 \u0627\u0633\u062a\u061b \u0641\u0631\u0645 \u0628\u0633\u062a\u0647\u200c\u0627\u06cc \u067e\u06cc\u062f\u0627 \u0646\u0634\u062f.");
            }
        } else if (L.kind == LimitOutcome::Undefined) {
            r.steps.push_back(
                {en ? "Result" : "\u0646\u062a\u06cc\u062c\u0647", nullptr,
                 (en ? "The limit does not exist: " : "\u062d\u062f \u0648\u062c\u0648\u062f \u0646\u062f\u0627\u0631\u062f: ") +
                     L.reason});
        } else {
            r.steps.push_back({en ? "Result" : "\u0646\u062a\u06cc\u062c\u0647", nullptr,
                               (en ? "The expression diverges to " : "\u0639\u0628\u0627\u0631\u062a \u0628\u0647 ") +
                                   outcomeText(L, en)});
        }
        r.ok = true;
    } catch (const std::exception& e) {
        r.ok = false;
        r.error = (en ? std::string("Limit error: ") : std::string("\u062e\u0637\u0627\u06cc \u062d\u062f: ")) +
                  e.what();
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
    r.typeFa = "\u067e\u06cc\u0648\u0633\u062a\u06af\u06cc";
    try {
        Ex f = parseExpr(input);
        Ex a = parsePoint(point);
        if (isInfinityPoint(a))
            throw std::runtime_error(en ? "continuity needs a finite point"
                                        : "\u067e\u06cc\u0648\u0633\u062a\u06af\u06cc \u062f\u0631 \u0646\u0642\u0637\u0647\u0654 \u0645\u062a\u0646\u0627\u0647\u06cc \u0645\u0639\u0646\u0627 \u0646\u062f\u0627\u0631\u062f");
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

        r.steps.push_back({en ? "Function" : "\u062a\u0627\u0628\u0639", f,
                           en ? "Continuity is tested at a single point."
                              : "\u067e\u06cc\u0648\u0633\u062a\u06af\u06cc \u062f\u0631 \u06cc\u06a9 \u0646\u0642\u0637\u0647 \u0628\u0631\u0631\u0633\u06cc \u0645\u06cc\u200c\u0634\u0648\u062f."});
        r.steps.push_back({en ? "Point" : "\u0646\u0642\u0637\u0647", a, x + " = " + pointText(a)});
        r.steps.push_back({en ? "Left limit" : "\u062d\u062f \u0686\u067e", c.left.value,
                           outcomeText(c.left, en)});
        r.steps.push_back({en ? "Right limit" : "\u062d\u062f \u0631\u0627\u0633\u062a", c.right.value,
                           outcomeText(c.right, en)});
        r.steps.push_back(
            {en ? "Value of the function" : "\u0645\u0642\u062f\u0627\u0631 \u062a\u0627\u0628\u0639", c.value,
             c.valueDefined ? safeText(c.value)
                            : (en ? "not defined at this point" : "\u062f\u0631 \u0627\u06cc\u0646 \u0646\u0642\u0637\u0647 \u062a\u0639\u0631\u06cc\u0641 \u0646\u0634\u062f\u0647")});

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
        r.checks.push_back({en ? "Left limit exists" : "\u062d\u062f \u0686\u067e \u0645\u0648\u062c\u0648\u062f \u0627\u0633\u062a", r.leftExists,
                            outcomeText(c.left, en)});
        r.checks.push_back({en ? "Right limit exists" : "\u062d\u062f \u0631\u0627\u0633\u062a \u0645\u0648\u062c\u0648\u062f \u0627\u0633\u062a", r.rightExists,
                            outcomeText(c.right, en)});
        r.checks.push_back({en ? "Left limit = right limit" : "\u062d\u062f \u0686\u067e = \u062d\u062f \u0631\u0627\u0633\u062a", sameLimits,
                            sameLimits ? (en ? "The one-sided limits agree." : "\u062d\u062f\u0647\u0627\u06cc \u06cc\u06a9\u200c\u0637\u0631\u0641\u0647 \u0628\u0631\u0627\u0628\u0631\u0646\u062f.")
                                       : (en ? "They differ or one of them does not exist."
                                             : "\u0645\u062a\u0641\u0627\u0648\u062a\u0646\u062f \u06cc\u0627 \u06cc\u06a9\u06cc \u0648\u062c\u0648\u062f \u0646\u062f\u0627\u0631\u062f.")});
        r.checks.push_back({en ? "Value at the point is defined" : "\u0645\u0642\u062f\u0627\u0631 \u062f\u0631 \u0646\u0642\u0637\u0647 \u062a\u0639\u0631\u06cc\u0641 \u0634\u062f\u0647 \u0627\u0633\u062a", r.valueDefined,
                            r.valueDefined ? safeText(c.value)
                                           : (en ? "f is not defined there." : "\u062a\u0627\u0628\u0639 \u0622\u0646\u062c\u0627 \u062a\u0639\u0631\u06cc\u0641 \u0646\u0634\u062f\u0647 \u0627\u0633\u062a.")});
        r.checks.push_back({en ? "Value equals the limit" : "\u0645\u0642\u062f\u0627\u0631 = \u062d\u062f", sameValue,
                            sameValue ? (en ? "f(a) equals the common limit." : "f(a) \u0628\u0631\u0627\u0628\u0631 \u062d\u062f \u0645\u0634\u062a\u0631\u06a9 \u0627\u0633\u062a.")
                                      : (en ? "f(a) is different from the limit." : "f(a) \u0628\u0627 \u062d\u062f \u0641\u0631\u0642 \u062f\u0627\u0631\u062f.")});

        std::string verdict;
        if (r.continuous) {
            verdict = en ? "Continuous at this point." : "\u062f\u0631 \u0627\u06cc\u0646 \u0646\u0642\u0637\u0647 \u067e\u06cc\u0648\u0633\u062a\u0647 \u0627\u0633\u062a.";
        } else if (c.kind == "removable") {
            verdict = en ? "Removable discontinuity." : "\u0627\u0646\u0642\u0637\u0627\u0639 \u0631\u0641\u0639\u200c\u0634\u062f\u0646\u06cc.";
        } else if (c.kind == "jump") {
            verdict = en ? "Jump discontinuity." : "\u0627\u0646\u0642\u0637\u0627\u0639 \u062c\u0647\u0634\u06cc.";
        } else if (c.kind == "infinite") {
            verdict = en ? "Infinite discontinuity (vertical asymptote)."
                         : "\u0627\u0646\u0642\u0637\u0627\u0639 \u0646\u0627\u0645\u062a\u0646\u0627\u0647\u06cc (\u0645\u062c\u0627\u0633\u0645 \u0639\u0645\u0648\u062f\u06cc).";
        } else {
            verdict = en ? "The limit does not exist, so the function is not continuous here."
                         : "\u062d\u062f \u0648\u062c\u0648\u062f \u0646\u062f\u0627\u0631\u062f\u060c \u067e\u0633 \u062a\u0627\u0628\u0639 \u0627\u06cc\u0646\u062c\u0627 \u067e\u06cc\u0648\u0633\u062a\u0647 \u0646\u06cc\u0633\u062a.";
        }
        r.steps.push_back({en ? "Conclusion" : "\u0646\u062a\u06cc\u062c\u0647", nullptr,
                           verdict + " " + c.note});
        r.ok = true;
    } catch (const std::exception& e) {
        r.ok = false;
        r.error = (en ? std::string("Continuity error: ")
                      : std::string("\u062e\u0637\u0627\u06cc \u067e\u06cc\u0648\u0633\u062a\u06af\u06cc: ")) +
                  e.what();
    }
    return r;
}

} // namespace ode
