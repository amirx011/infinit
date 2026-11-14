// Infinit Lab — transcendental functions and integration techniques.
// SPDX-License-Identifier: Apache-2.0
#include "calculus_topics.hpp"

#include "calculus_apps.hpp"
#include "calculus_ext.hpp"
#include "expr.hpp"
#include "numeric.hpp"
#include "solver.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace ode {
namespace {

void baseId(Result& r, const std::string& typeId, const std::string& variable) {
    r.ok = true;
    r.mode = "topic";
    r.typeId = typeId;
    r.variable = variable;
    r.engine = "native";
    r.closedForm = true;
    r.verified = true;
}

void failCode(Result& r, const std::string& code) {
    r.ok = false;
    r.errorCode = code;
    r.verified = false;
    r.error.clear();
}

void addStep(Result& r, const std::string& titleKey, const Ex& math = nullptr,
             const std::string& noteKey = "", const std::string& params = "") {
    Step s;
    s.titleKey = titleKey;
    s.math = math;
    s.noteKey = noteKey;
    s.params = params;
    r.steps.push_back(std::move(s));
}

void addStepRaw(Result& r, const std::string& titleKey, const Ex& math,
                const std::string& rawNote) {
    Step s;
    s.titleKey = titleKey;
    s.math = math;
    s.note = rawNote;
    r.steps.push_back(std::move(s));
}

std::string numStr(double v, int digits = 6) {
    if (std::isnan(v)) return "nan";
    if (!std::isfinite(v)) return v < 0 ? "-inf" : "inf";
    if (std::fabs(v) < 1e-12) return "0";
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

double eval1(const Ex& f, const std::string& x, double at) {
    bool ok = false;
    const double v = evalReal(f, {{x, at}}, &ok);
    return ok && std::isfinite(v) ? v : std::numeric_limits<double>::quiet_NaN();
}

double maxDerivativeResidual(const Ex& F, const Ex& f, const std::string& x,
                             const std::vector<double>& points) {
    const Ex dF = simplify(diff(F, x));
    double worst = 0.0;
    for (double xv : points) {
        const double lhs = eval1(dF, x, xv);
        const double rhs = eval1(f, x, xv);
        if (!std::isfinite(lhs) || !std::isfinite(rhs)) continue;
        worst = std::max(worst, std::fabs(lhs - rhs));
    }
    return worst;
}

std::string familyTable() {
    return "family\tdefinition\tderivative\tintegral\n"
           "exp\texp(x)\texp(x)\texp(x)\n"
           "ln\tln(x)\t1/x\tx ln(x) - x\n"
           "sin\tsin(x)\tcos(x)\t-cos(x)\n"
           "cos\tcos(x)\t-sin(x)\tsin(x)\n"
           "tan\ttan(x)\t1/cos(x)^2\t-ln|cos(x)|\n"
           "arcsin\tarcsin(x)\t1/sqrt(1 - x^2)\tx arcsin(x) + sqrt(1 - x^2)\n"
           "arctan\tarctan(x)\t1/(1 + x^2)\tx arctan(x) - ln(1 + x^2)/2\n"
           "sinh\tsinh(x)\tcosh(x)\tcosh(x)\n"
           "cosh\tcosh(x)\tsinh(x)\tsinh(x)\n"
           "tanh\ttanh(x)\t1/cosh(x)^2\tln(cosh(x))";
}

} // namespace

// ---------------------------------------------------------------- 96
Result transcendentalTopic(const TopicRequest& req) {
    Result r;
    const std::string x = req.text("var", "x");
    baseId(r, "transcendental", x);
    const std::string input = req.text("f", "exp(x)*sin(x)");
    const double a = req.number("a", 0.0);
    const double x0 = req.number("x0", 0.5);
    int order = (int)std::lround(req.number("N", 5.0));
    order = std::clamp(order, 1, 10);

    Ex f;
    try {
        f = parseExpr(input);
    } catch (const std::exception& e) {
        r.errorDetail = std::string("f: ") + e.what();
        failCode(r, "bad_expression");
        return r;
    }

    Ex f1 = simplify(diff(f, x));
    Ex f2 = simplify(diff(f1, x));

    Result integ = integrateExpression(input, x, req.lang);
    Result tay = taylorSeries(input, x, a, order, req.lang);

    const std::string antiderivative =
        integ.ok ? (integ.solution ? toPlain(integ.solution) : std::string("(none)"))
                 : std::string("(unavailable)");
    const bool haveAntiderivative = integ.ok && integ.closedForm;
    const Ex F = haveAntiderivative ? integ.solution : nullptr;

    std::ostringstream coeffs;
    coeffs << "n\tf^(n)(a)\tc_n\n";
    if (!tay.ok || tay.coefficients.empty()) {
        coeffs << "(Taylor coefficients unavailable)\n";
    } else {
        for (size_t k = 0; k < tay.coefficients.size(); ++k)
            coeffs << k << "\t" << toPlain(tay.coefficients[k]) << "\t"
                   << toPlain(tay.coefficients[k]) << " / " << k << "!\n";
    }

    std::ostringstream samples;
    samples << "x\tf(x)\tP_N(x)\n";
    double taylorError = 0.0;
    for (int j = 0; j <= 6; ++j) {
        const double xv = a + (j - 3) * 0.15;
        const double fv = eval1(f, x, xv);
        const double pv = tay.ok && tay.polynomial
                              ? eval1(tay.polynomial, x, xv)
                              : std::numeric_limits<double>::quiet_NaN();
        samples << numStr(xv, 4) << "\t" << numStr(fv) << "\t" << numStr(pv)
                << "\n";
        if (std::isfinite(fv) && std::isfinite(pv))
            taylorError = std::max(taylorError, std::fabs(fv - pv));
    }

    const double hv = 1e-5 * (1.0 + std::fabs(x0));
    const double derivativeAt = eval1(f1, x, x0);
    const double finiteDifference =
        (eval1(f, x, x0 + hv) - eval1(f, x, x0 - hv)) / (2 * hv);
    const double derivativeError =
        std::isfinite(derivativeAt) && std::isfinite(finiteDifference)
            ? std::fabs(derivativeAt - finiteDifference)
            : std::numeric_limits<double>::quiet_NaN();
    const double integralResidual =
        haveAntiderivative
            ? maxDerivativeResidual(F, f, x, {-0.7, -0.2, 0.3, 0.8, 1.4})
            : std::numeric_limits<double>::quiet_NaN();

    r.functions.push_back("f(x) = " + input);
    r.functions.push_back("f'(x) = " + toPlain(f1));
    r.functions.push_back("f''(x) = " + toPlain(f2));
    r.functions.push_back("integral f dx = " + antiderivative +
                          (haveAntiderivative
                               ? " + C   (method: " + integ.method + ")"
                               : "   (no elementary antiderivative detected)"));
    r.functions.push_back("Taylor polynomial about a = " + numStr(a) + ":\n" +
                          (tay.ok && tay.polynomial
                               ? toPlain(tay.polynomial)
                               : std::string("(unavailable)")));
    r.functions.push_back(coeffs.str());
    r.functions.push_back(samples.str());
    r.functions.push_back("transcendental family table:\n" + familyTable());
    r.functions.push_back("f(" + numStr(x0) + ") = " + numStr(eval1(f, x, x0)));
    r.value = numEx(eval1(f, x, x0));
    r.derivative = f1;
    r.secondDerivative = f2;
    r.solution = tay.ok ? tay.polynomial : F;
    r.closedForm = haveAntiderivative && tay.ok;
    r.numericOnly = false;
    r.method = haveAntiderivative ? integ.method : "";

    addStep(r, "step.title.function_and_derivatives",
            Sym("f = " + input + ",  f' = " + toPlain(f1) +
                ",  f'' = " + toPlain(f2)));
    addStep(r, "step.title.indefinite_integral",
            F ? F : Sym("integral f dx  (no elementary form)"),
            "step.note.constant_of_integration");
    addStep(r, "step.title.function_taylor_a",
            tay.ok ? tay.polynomial : Sym("P_N(x)  (unavailable)"),
            "step.note.taylor_expansion_about_a",
            "{\"n\":" + std::to_string(order) + "}");
    addStepRaw(r, "step.title.coefficients", nullptr, coeffs.str());
    addStepRaw(r, "step.title.evaluate", nullptr, samples.str());
    addStep(r, "step.title.numeric_evidence",
            Sym("f'(" + numStr(x0) + ") = " + numStr(derivativeAt) +
                ", central difference = " + numStr(finiteDifference)));

    addCheck(r, "check.derivative_matches",
             std::isfinite(derivativeError) &&
                 derivativeError < 1e-4 * (1.0 + std::fabs(derivativeAt)),
             "check.derivative_finite", "",
             "|f' - finite difference| = " + numStr(derivativeError, 3));
    if (haveAntiderivative)
        addCheck(r, "check.differentiate_result",
                 std::isfinite(integralResidual) &&
                     integralResidual < 1e-6 * (1.0 + std::fabs(eval1(f, x, 1.0))),
                 "check.derivative_matches", "",
                 "max |d/dx integral - f| = " + numStr(integralResidual, 3));
    addCheck(r, "check.polynomial_match",
             tay.ok && std::isfinite(taylorError) &&
                 taylorError < 0.05 * (1.0 + std::fabs(eval1(f, x, x0))),
             "check.polynomial_match", "",
             "max |f - P_N| near a = " + numStr(taylorError, 3));
    return r;
}

// ---------------------------------------------------------------- 97
namespace {

struct Technique {
    std::string id;
    std::string when;
    std::string example;
};

const std::vector<Technique>& techniques() {
    static const std::vector<Technique> list = {
        {"substitution", "the integrand contains g(u(x)) u'(x)",
         "2*x*cos(x^2)"},
        {"parts", "a product of a polynomial with exp, log or a trigonometric factor",
         "x*exp(x)"},
        {"partial_fractions", "a rational function whose denominator factors",
         "1/(x^2 - 4)"},
        {"trig_substitution", "a radical sqrt(a^2 - x^2) or sqrt(x^2 + a^2)",
         "sqrt(1 - x^2)"},
        {"table", "the integrand matches a standard elementary antiderivative",
         "cos(x)*exp(x)"},
    };
    return list;
}

std::string exampleFor(const std::string& id) {
    for (const Technique& t : techniques())
        if (t.id == id) return t.example;
    return "x*exp(x)";
}

std::string techniqueTable(const std::string& detected, const std::string& input) {
    std::ostringstream out;
    out << "technique\twhen it applies\texample\n";
    for (const Technique& t : techniques())
        out << t.id << "\t" << t.when << "\t" << t.example
            << (t.id == detected ? "   <- this integrand" : "") << "\n";
    out << "detected for " << input << ": "
        << (detected.empty() ? "no elementary method" : detected) << "\n";
    return out.str();
}

} // namespace

Result integrationTechniquesTopic(const TopicRequest& req) {
    Result r;
    const std::string x = req.text("var", "x");
    baseId(r, "integration_techniques", x);
    const std::string requested = req.text("technique", "parts");
    const std::string input = req.text("f", exampleFor(requested));
    const double a = req.number("a", 0.0);
    const double b = req.number("b", 1.0);
    if (!std::isfinite(a) || !std::isfinite(b) || b <= a) {
        failCode(r, "bad_interval");
        return r;
    }

    Result integ = integrateExpression(input, x, req.lang);
    if (!integ.ok) {
        failCode(r, integ.errorCode.empty() ? "unavailable" : integ.errorCode);
        r.errorDetail = integ.errorDetail;
        return r;
    }
    const Ex F = integ.solution;
    const bool closed = integ.closedForm;
    const std::string method = integ.method;

    double numeric = std::numeric_limits<double>::quiet_NaN();
    double exactValue = std::numeric_limits<double>::quiet_NaN();
    double endpointResidual = std::numeric_limits<double>::quiet_NaN();
    numeric = quadratureAdaptive(
        [&](double xv) {
            bool ok = false;
            const double v = evalReal(integ.normalized, {{x, xv}}, &ok);
            return ok ? v : std::numeric_limits<double>::quiet_NaN();
        },
        a, b, 1e-10, 16);
    if (closed && F) {
        exactValue = eval1(F, x, b) - eval1(F, x, a);
        endpointResidual = std::fabs(exactValue - numeric);
    }

    const double derivativeResidual =
        closed && F ? maxDerivativeResidual(F, integ.normalized, x,
                                            {a + 0.1 * (b - a),
                                             a + 0.35 * (b - a),
                                             a + 0.6 * (b - a),
                                             a + 0.85 * (b - a)})
                    : std::numeric_limits<double>::quiet_NaN();

    std::string detected = method;
    if (method == "rational") detected = "partial_fractions";
    else if (method == "trig") detected = "trig_substitution";
    r.functions.push_back("f(x) = " + input);
    r.functions.push_back("integral f dx = " +
                          (F ? toPlain(F) : std::string("(unevaluated)")) +
                          (closed ? " + C" : "   (no elementary antiderivative)"));
    r.functions.push_back("method reported by the engine: " +
                          (method.empty() ? std::string("none") : method));
    r.functions.push_back(techniqueTable(detected, input));
    if (std::isfinite(exactValue) && std::isfinite(numeric))
        r.functions.push_back("definite integral on [" + numStr(a) + ", " +
                              numStr(b) + "]: F(b) - F(a) = " +
                              numStr(exactValue) + ", adaptive quadrature = " +
                              numStr(numeric));
    else if (std::isfinite(numeric))
        r.functions.push_back("definite integral on [" + numStr(a) + ", " +
                              numStr(b) + "] = " + numStr(numeric) +
                              " (adaptive quadrature; no elementary form)");
    if (closed)
        r.functions.push_back("max |d/dx integral - f| on the interval = " +
                              numStr(derivativeResidual, 3));
    r.value = numEx(std::isfinite(numeric) ? numeric : exactValue);
    r.solution = F;
    r.closedForm = closed;
    r.numericOnly = !closed;
    r.numericNote = closed
                        ? ""
                        : "The engine did not find an elementary antiderivative; "
                          "the definite value is the adaptive quadrature result.";
    r.method = method;

    addStep(r, "step.title.method_detection", nullptr,
            "step.note.detected_method",
            "{\"method\":\"@ui.method." + (method.empty() ? "unknown" : method) +
                "\"}");
    addStep(r, "step.title.indefinite_integral",
            F ? F : Sym("integral f dx  (no elementary form)"),
            "step.note.constant_of_integration");
    if (std::isfinite(exactValue))
        addStep(r, "step.title.evaluate_the_integral",
                Sym("F(b) - F(a) = " + numStr(exactValue)));
    addStep(r, "step.title.numeric_evidence",
            Sym("adaptive quadrature = " + numStr(numeric) +
                ",  |F(b) - F(a) - Q| = " + numStr(endpointResidual, 3)));

    if (closed)
        addCheck(r, "check.differentiate_result",
                 std::isfinite(derivativeResidual) &&
                     derivativeResidual < 1e-6 *
                                             (1.0 + std::fabs(eval1(integ.normalized, x, 0.5))),
                 "check.derivative_matches", "",
                 "max residual = " + numStr(derivativeResidual, 3));
    if (std::isfinite(exactValue) && std::isfinite(numeric))
        addCheck(r, "check.quadrature_value",
                 std::isfinite(endpointResidual) &&
                     endpointResidual < 1e-6 * (1.0 + std::fabs(numeric)),
                 "check.quadrature_adaptive", "",
                 "|F(b) - F(a) - Q| = " + numStr(endpointResidual, 3));
    bool recognised = false;
    for (const Technique& t : techniques())
        if (t.id == detected) recognised = true;
    addCheck(r, "check.table_entry_valid",
             closed ? (!method.empty() && recognised) : true,
             "check.table_check", "",
             method.empty() ? std::string("no elementary technique")
                            : "method = " + method);
    return r;
}

} // namespace ode
