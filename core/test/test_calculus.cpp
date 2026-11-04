// Regression tests for derivatives, definite integrals and improper integrals.
// SPDX-License-Identifier: Apache-2.0
#include "numeric.hpp"
#include "solver.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

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

static bool closeAt(const Ex& e, double x, double want, double tol = 1e-9) {
    if (!e) return false;
    bool ok = false;
    const double got = evalReal(e, {{"x", x}}, &ok);
    return ok && std::fabs(got - want) <= tol * std::max(1.0, std::fabs(want));
}

static Result definite(const std::string& f, const std::string& lo, const std::string& hi) {
    return integrateMulti(f, {"x"}, true, {lo}, {hi}, "en");
}

int main() {
    // ---------------------------------------------------------- derivatives
    {
        Result r = derivativeExpression("x^6", "x", 5, "en");
        expect(r.ok, "5th derivative of x^6 succeeds");
        expect(r.order == 5, "order is not clamped to 3");
        expect(closeAt(r.solution, 2.0, 1440.0), "5th derivative of x^6 is 720x");
        expect(r.steps.size() == 6, "5th derivative emits rules + five derivative steps");
    }
    {
        Result r = derivativeExpression("x^10", "x", 10, "en");
        expect(r.ok, "10th derivative is accepted");
        expect(closeTo(r.solution, 3628800.0), "10th derivative of x^10 is 10!");
    }
    {
        Result r = derivativeExpression("x^3", "x", 0, "en");
        expect(!r.ok, "order 0 is rejected explicitly");
        expect(!r.error.empty(), "order 0 produces an error message");
    }
    {
        Result r = derivativeExpression("x^3", "x", 11, "en");
        expect(!r.ok, "order 11 is rejected explicitly");
        expect(!r.error.empty(), "order 11 produces an error message");
    }
    {
        Result r = derivativeExpression("x^2 + y^2 = 4", "x", 4, "en");
        expect(r.ok, "implicit derivative of order 4 succeeds");
        expect(r.order == 4, "implicit order is reported");
    }
    {
        Result r = derivativeExpression("x^2 + a^2 = y^2", "x", 1, "en");
        expect(r.ok, "implicit derivative with a parameter succeeds");
        expect(r.solution && toPlain(r.solution) == "x/y",
               "implicit derivative treats y as the dependent variable");
    }

    // ------------------------------------------------- definite integrals
    {
        Result r = definite("sin(x)", "0", "pi");
        expect(r.ok, "proper definite integral succeeds");
        expect(closeTo(r.solution, 2.0), "integral of sin over [0,pi] is 2");
        expect(!r.improper && !r.divergent, "proper integral is not marked improper");
    }
    {
        Result r = definite("1/x^2", "-1", "1");
        expect(r.ok, "singular definite integral is analysed");
        expect(r.divergent, "integral of 1/x^2 over [-1,1] diverges");
        expect(r.solution == nullptr, "divergent integral has no finite answer");
    }
    {
        Result r = definite("1/x", "-1", "1");
        expect(r.ok, "1/x over [-1,1] is analysed");
        expect(r.divergent, "integral of 1/x over [-1,1] diverges");
    }
    {
        Result r = definite("1/(x-0.5)", "0", "1");
        expect(r.ok, "interior pole is analysed");
        expect(r.divergent, "integral across 1/(x-1/2) diverges");
    }
    {
        Result r = definite("tan(x)", "0", "pi");
        expect(r.ok, "tan singularity is analysed");
        expect(r.divergent, "integral of tan over [0,pi] diverges");
    }
    {
        Result r = definite("1/sqrt(x)", "0", "1");
        expect(r.ok, "convergent improper integral succeeds");
        expect(!r.divergent, "1/sqrt(x) converges at 0");
        expect(r.improper, "1/sqrt(x) is flagged as improper");
        expect(closeTo(r.solution, 2.0), "integral of 1/sqrt(x) over [0,1] is 2");
    }
    {
        Result r = definite("ln(x)", "0", "1");
        expect(r.ok, "logarithmic endpoint singularity is analysed");
        expect(r.improper, "ln(x) at 0 is improper");
        expect(closeTo(r.solution, -1.0), "integral of ln(x) over [0,1] is -1");
    }
    {
        Result r = definite("1/x^2", "1", "inf");
        expect(r.ok, "improper integral with an infinite bound succeeds");
        expect(r.improper, "infinite bound is flagged as improper");
        expect(closeTo(r.solution, 1.0), "integral of 1/x^2 over [1,inf) is 1");
    }
    {
        Result r = definite("1/x", "1", "inf");
        expect(r.ok, "divergent infinite integral is analysed");
        expect(r.divergent, "integral of 1/x over [1,inf) diverges");
    }
    {
        Result r = definite("1/(x^2+1)", "-inf", "inf");
        expect(r.ok, "Gaussian-type integral over the whole line succeeds");
        expect(closeTo(r.solution, 3.14159265358979323846, 1e-12),
               "integral of 1/(1+x^2) over the line is pi");
    }
    {
        Result r = definite("sin(x)/x", "0", "1");
        expect(r.ok, "removable singularity does not block quadrature");
        expect(!r.improper, "sin(x)/x is not marked improper");
        expect(r.hasNumeric && std::fabs(r.numericValue - 0.946083070367) < 1e-6,
               "sin(x)/x over [0,1] is about 0.946083");
    }

    if (failures) {
        std::cout << failures << " calculus test(s) failed\n";
        return 1;
    }
    std::cout << "All calculus tests passed\n";
    return 0;
}
