// Infinit Lab — single/multiple variable calculus API
// (integrals: indefinite, definite, double and triple; derivatives: orders 1–3
//  and implicit differentiation)
#include "numeric.hpp"
// SPDX-License-Identifier: MIT
#include "calculus_ext.hpp"
#include "solver.hpp"

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

} // namespace ode
