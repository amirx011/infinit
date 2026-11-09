// Regression tests for limits and continuity.
// SPDX-License-Identifier: Apache-2.0
#include "limits.hpp"
#include "numeric.hpp"
#include "solver.hpp"

#include <cmath>
#include <iostream>
#include <string>

using namespace ode;

static int failures = 0;

static void expect(bool condition, const std::string& what) {
    if (!condition) {
        ++failures;
        std::cout << "FAIL: " << what << "\n";
    }
}

static bool closeTo(const Ex& e, double want, double tol = 1e-9) {
    if (!e) return false;
    bool ok = false;
    const double got = evalReal(e, {}, &ok);
    return ok && std::fabs(got - want) <= tol * std::max(1.0, std::fabs(want));
}

// English requests must never leak Persian parser messages.
static bool hasPersian(const std::string& text) {
    for (size_t i = 0; i + 1 < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c >= 0xD8 && c <= 0xDB) return true;
    }
    return false;
}

int main() {
    // -------------------------------------------------------------- limits
    {
        Result r = limitExpression("sin(x)/x", "x", "0", "both", "en");
        expect(r.ok, "sin(x)/x at 0 succeeds");
        expect(r.limitExists, "sin(x)/x has a limit at 0");
        expect(closeTo(r.solution, 1.0), "sin(x)/x -> 1");
        expect(r.steps.size() == 5, "two-sided finite limit emits expression, sides, result and laws");
    }
    {
        Result r = limitExpression("(1-cos(x))/x^2", "x", "0", "both", "en");
        expect(r.ok, "(1-cos x)/x^2 at 0 succeeds");
        expect(closeTo(r.solution, 0.5), "(1-cos x)/x^2 -> 1/2");
    }
    {
        Result r = limitExpression("abs(x)/x", "x", "0", "both", "en");
        expect(r.ok, "abs(x)/x at 0 succeeds");
        expect(!r.limitExists, "abs(x)/x has no two-sided limit");
        expect(closeTo(r.leftValue, -1.0), "left limit of abs(x)/x is -1");
        expect(closeTo(r.rightValue, 1.0), "right limit of abs(x)/x is 1");
    }
    {
        Result r = limitExpression("sin(1/x)", "x", "0", "both", "en");
        expect(r.ok, "sin(1/x) at 0 succeeds");
        expect(!r.limitExists, "sin(1/x) oscillates without a limit");
    }
    {
        Result r = limitExpression("tan(x)", "x", "pi/2", "both", "en");
        expect(r.ok, "tan at pi/2 succeeds");
        expect(!r.limitExists, "tan has no two-sided limit at pi/2");
        expect(r.leftInfinite && r.rightInfinite, "both sides of tan diverge");
        expect(r.leftSign == 1 && r.rightSign == -1, "tan has +inf / -inf sides");
    }
    {
        Result r = limitExpression("atan(x)", "x", "inf", "both", "en");
        expect(r.ok, "atan at +inf succeeds");
        expect(r.limitExists, "atan has a limit at +inf");
        expect(closeTo(r.solution, 1.5707963267948966, 1e-12), "atan(+inf) = pi/2");
    }
    {
        Result r = limitExpression("atan(x)", "x", "-inf", "both", "en");
        expect(r.ok, "atan at -inf succeeds");
        expect(closeTo(r.solution, -1.5707963267948966, 1e-12), "atan(-inf) = -pi/2");
    }
    {
        Result r = limitExpression("x^2", "x", "inf", "both", "en");
        expect(r.ok, "x^2 at +inf succeeds");
        expect(r.limitInfinite && r.limitSign > 0, "x^2 diverges to +inf");
    }
    {
        Result r = limitExpression("sinh(x)", "x", "inf", "both", "en");
        expect(r.ok, "sinh at +inf succeeds");
        expect(r.limitInfinite && r.limitSign > 0, "sinh(+inf) = +inf");
    }
    {
        Result r = limitExpression("sinh(x)", "x", "-inf", "both", "en");
        expect(r.ok, "sinh at -inf succeeds");
        expect(r.limitInfinite && r.limitSign < 0, "sinh(-inf) = -inf");
    }
    {
        Result r = limitExpression("cosh(x)", "x", "-inf", "both", "en");
        expect(r.ok, "cosh at -inf succeeds");
        expect(r.limitInfinite && r.limitSign > 0, "cosh(-inf) = +inf");
    }
    {
        Result r = limitExpression("asinh(x)", "x", "-inf", "both", "en");
        expect(r.ok, "asinh at -inf succeeds");
        expect(r.limitInfinite && r.limitSign < 0, "asinh(-inf) = -inf");
    }
    {
        Result r = limitExpression("acosh(x)", "x", "inf", "both", "en");
        expect(r.ok, "acosh at +inf succeeds");
        expect(r.limitInfinite && r.limitSign > 0, "acosh(+inf) = +inf");
    }
    {
        Result r = limitExpression("x*sin(1/x)", "x", "0", "both", "en");
        expect(r.ok, "squeeze example succeeds");
        expect(r.limitExists && closeTo(r.solution, 0.0), "x*sin(1/x) -> 0");
        bool hasSqueeze = false;
        for (const auto& law : r.laws) if (law == "squeeze") hasSqueeze = true;
        expect(hasSqueeze, "the squeeze rule is reported by name");
    }

    // ------------------------------------------------------- error language
    {
        Result r = limitExpression("1/", "x", "0", "both", "en");
        expect(!r.ok, "a malformed limit fails");
        expect(!hasPersian(r.error), "English error messages stay English");
        expect(!r.errorCode.empty(), "the failure carries a stable code");
    }
    {
        Result r = derivativeExpression("1/", "x", 1, "en");
        expect(!r.ok, "a malformed derivative fails");
        expect(!hasPersian(r.error), "derivative errors are localised too");
    }

    // ---------------------------------------------------------- continuity
    {
        Result r = continuityExpression("x^2", "x", "2", "en");
        expect(r.ok, "continuity of x^2 succeeds");
        expect(r.continuous, "x^2 is continuous at 2");
        expect(closeTo(r.solution, 4.0), "continuity value is f(2)=4");
        expect(r.steps.size() == 5, "continuity check emits 5 steps");
    }
    {
        Result r = continuityExpression("1/x", "x", "0", "en");
        expect(r.ok, "continuity of 1/x at 0 succeeds");
        expect(!r.continuous, "1/x is not continuous at 0");
        expect(r.discontinuity == "infinite", "1/x has an infinite discontinuity");
    }
    {
        Result r = continuityExpression("(x^2-1)/(x-1)", "x", "1", "en");
        expect(r.ok, "continuity of a rational function succeeds");
        expect(!r.continuous, "the rational function is undefined at 1");
        expect(r.discontinuity == "removable", "the discontinuity is removable");
    }
    {
        Result r = continuityExpression("sin(x)/x", "x", "0", "en");
        expect(r.ok, "continuity of sin(x)/x at 0 succeeds");
        expect(r.discontinuity == "removable", "sin(x)/x has a removable discontinuity at 0");
    }

    if (failures) {
        std::cout << failures << " limit/continuity test(s) failed\n";
        return 1;
    }
    std::cout << "All limit/continuity tests passed\n";
    return 0;
}
