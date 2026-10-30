// Infinit Lab — single-variable calculus API (integrals and derivatives)
#include "numeric.hpp"
// SPDX-License-Identifier: MIT
#include "solver.hpp"

#include <string>

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

Result derivativeExpression(const std::string& input, const std::string& x, int order,
                            const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "derivative";
    r.typeId = "derivative";
    r.typeFa = order >= 2 ? "مشتق مرتبهٔ دوم" : "مشتق مرتبهٔ اول";
    r.engine = "native";
    r.order = order < 1 ? 1 : order;
    try {
        Ex f = parseExpr(input);
        r.normalized = f;
        r.variable = x;
        Ex d = f;
        for (int k = 0; k < r.order; ++k) d = simplify(diff(d, x));
        r.solution = d;
        r.isExplicit = true;
        r.explicitY = d;
        r.closedForm = true;
        r.steps.push_back({en ? "Function" : "تابع", f,
                           en ? "Differentiate with respect to " + x + "."
                              : "مشتق‌گیری نسبت به " + x + "."});
        r.steps.push_back({en ? "Derivative" : "مشتق", d});
        r.ok = true;
    } catch (const std::exception& e) {
        r.ok = false;
        r.error = (en ? std::string("Derivative error: ") : std::string("خطای مشتق‌گیری: ")) + e.what();
    }
    return r;
}

} // namespace ode
