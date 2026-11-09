// Regression tests for the calculus extensions: Taylor/Maclaurin, integral
// applications, partial derivatives and series convergence.  The tests check
// both the answers and that a worked solution (several steps) is produced.
#include "calculus_apps.hpp"
#include "expr.hpp"
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

static void expectNear(double got, double want, double tol, const std::string& what) {
    const bool ok = std::isfinite(got) && std::fabs(got - want) <= tol;
    if (!ok) {
        ++failures;
        std::cout << "FAIL: " << what << " (got " << got << ", want " << want << ")\n";
    }
}

static bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

static void taylorCases() {
    // exp(x) = 1 + x + x^2/2 + x^3/6 + x^4/24 + ...
    {
        Result r = taylorSeries("exp(x)", "x", 0.0, 4, "en");
        expect(r.ok, "Taylor exp(x) succeeds");
        expect(r.coefficients.size() == 5, "Taylor exp(x) has five coefficients");
        expect(r.steps.size() >= 6, "Taylor exp(x) shows the derivative steps");
        for (const auto& s : r.steps) {
            expect(s.title.empty(), "Taylor steps carry no title sentence");
            expect(!s.titleKey.empty(), "Taylor step titles are keys");
        }
        if (r.coefficients.size() == 5) {
            expect(toPlain(r.coefficients[0]) == "1", "c0 = 1");
            expect(toPlain(r.coefficients[1]) == "1", "c1 = 1");
            expect(toPlain(r.coefficients[2]) == "1/2", "c2 = 1/2");
            expect(toPlain(r.coefficients[3]) == "1/6", "c3 = 1/6");
            expect(toPlain(r.coefficients[4]) == "1/24", "c4 = 1/24");
        }
        expect(r.polynomial != nullptr, "Taylor exp(x) has a polynomial");
        expect(r.remainder != nullptr, "Taylor exp(x) has a Lagrange remainder");
    }
    // Maclaurin of sin(x): x - x^3/6 + x^5/120.
    {
        Result r = taylorSeries("sin(x)", "x", 0.0, 5, "en");
        expect(r.ok, "Maclaurin sin(x) succeeds");
        expect(r.application == "maclaurin", "sin about 0 is Maclaurin");
        bool ok = false;
        const double p = evalReal(r.polynomial, {{"x", 0.5}}, &ok);
        expect(ok, "sin polynomial evaluates");
        expectNear(p, std::sin(0.5), 2e-5, "sin Maclaurin approximates sin");
    }
    // Taylor of ln(x) about a = 1: (x-1) - (x-1)^2/2 + (x-1)^3/3.
    {
        Result r = taylorSeries("ln(x)", "x", 1.0, 3, "en");
        expect(r.ok, "Taylor ln(x) about 1 succeeds");
        expect(r.application == "taylor", "ln about 1 is Taylor");
        bool ok = false;
        const double p = evalReal(r.polynomial, {{"x", 1.2}}, &ok);
        expect(ok, "ln polynomial evaluates");
        expectNear(p, std::log(1.2), 2e-3, "ln Taylor approximates ln");
    }
    // A transcendental value at a non-zero centre must not overflow the
    // rational arithmetic.  sin about 1 and about pi/4 used to fail while the
    // coefficients were snapped to rational approximations.
    {
        Result r = taylorSeries("sin(x)", "x", 1.0, 5, "en");
        expect(r.ok, "Taylor sin(x) about 1 succeeds");
        bool ok = false;
        const double p = evalReal(r.polynomial, {{"x", 1.1}}, &ok);
        expect(ok, "sin polynomial about 1 evaluates");
        expectNear(p, std::sin(1.1), 1e-6, "sin Taylor about 1 approximates sin");
        expect(r.remainder != nullptr, "sin about 1 keeps the remainder");
    }
    {
        Result r = taylorSeries("sin(x)", "x", M_PI / 4.0, 5, "en");
        expect(r.ok, "Taylor sin(x) about pi/4 succeeds");
        expect(contains(r.point, "pi"), "the centre is recognised as a multiple of pi");
        bool ok = false;
        const double p = evalReal(r.polynomial, {{"x", M_PI / 4.0 + 0.1}}, &ok);
        expect(ok, "sin polynomial about pi/4 evaluates");
        expectNear(p, std::sin(M_PI / 4.0 + 0.1), 1e-6, "sin Taylor about pi/4 approximates sin");
    }
    {
        Result r = taylorSeries("exp(x)", "x", 1.0, 5, "en");
        expect(r.ok, "Taylor exp(x) about 1 succeeds");
        bool ok = false;
        const double p = evalReal(r.polynomial, {{"x", 1.2}}, &ok);
        expect(ok, "exp polynomial about 1 evaluates");
        expectNear(p, std::exp(1.2), 2e-4, "exp Taylor about 1 approximates exp");
    }
}

static void integralApplicationCases() {
    // Area between x and x^2 on [0, 1] is 1/6.
    {
        Result r = integralApplication("area", "x", "x^2", "x", 0.0, 1.0, "x", "en");
        expect(r.ok, "area x vs x^2 succeeds");
        expect(r.steps.size() >= 3, "area shows the setup and pieces");
        expectNear(r.numericValue, 1.0 / 6.0, 1e-9, "area x vs x^2 = 1/6");
    }
    // Area between sin(x) and 0 on [0, pi] is 2.
    {
        Result r = integralApplication("area", "sin(x)", "", "x", 0.0, M_PI, "x", "en");
        expect(r.ok, "area sin(x) succeeds");
        expectNear(r.numericValue, 2.0, 1e-6, "area sin on [0, pi] = 2");
    }
    // Crossing curves: x^2 and 2x on [0, 2], area = 4/3.
    {
        Result r = integralApplication("area", "x^2", "2*x", "x", 0.0, 2.0, "x", "en");
        expect(r.ok, "area with a crossing succeeds");
        expectNear(r.numericValue, 4.0 / 3.0, 1e-7, "area x^2 vs 2x = 4/3");
    }
    // Volume of revolution: pi * integral x^4 = pi/5.
    {
        Result r = integralApplication("volume", "x^2", "", "x", 0.0, 1.0, "x", "en");
        expect(r.ok, "disk volume succeeds");
        expectNear(r.numericValue, M_PI / 5.0, 1e-7, "volume x^2 about x = pi/5");
    }
    // Shells: 2*pi * integral x*x = 2*pi/3.
    {
        Result r = integralApplication("volume", "x", "", "x", 0.0, 1.0, "y", "en");
        expect(r.ok, "shell volume succeeds");
        expectNear(r.numericValue, 2.0 * M_PI / 3.0, 1e-7, "shell volume x about y = 2pi/3");
    }
    // Arc length of x^2 on [0, 1] is about 1.4789428575.
    {
        Result r = integralApplication("arc_length", "x^2", "", "x", 0.0, 1.0, "x", "en");
        expect(r.ok, "arc length succeeds");
        expect(r.steps.size() >= 2, "arc length shows the formula");
        expectNear(r.numericValue, 1.4789428575445975, 1e-6, "arc length of x^2 on [0,1]");
    }
    // An impossible interval is rejected instead of returning a wrong number.
    {
        Result r = integralApplication("area", "x", "0", "x", 1.0, 0.0, "x", "en");
        expect(!r.ok, "area rejects a > b");
        expect(r.errorCode == "bad_interval", "area reports the invalid interval code");
    }
}

static void partialCases() {
    Result r = partialDerivatives("x^2*y + y^3", {"x", "y"}, {1.0, 2.0}, {1.0, 1.0}, "en");
    expect(r.ok, "partial derivatives succeed");
    expect(r.gradient.size() == 2, "gradient has two components");
    if (r.gradient.size() == 2) {
        expect(toPlain(r.gradient[0]) == "4", "f_x(1,2) = 4");
        expect(toPlain(r.gradient[1]) == "13", "f_y(1,2) = 13");
    }
    expect(r.laplacian && toPlain(r.laplacian) == "16", "Laplacian at (1,2) = 16");
    expect(r.directional != nullptr, "directional derivative is present");
    if (r.directional) {
        bool ok = false;
        const double d = evalReal(r.directional, {}, &ok);
        expect(ok, "directional derivative evaluates");
        expectNear(d, 17.0 / std::sqrt(2.0), 1e-9, "directional derivative = 17/sqrt(2)");
    }
    // Clairaut symmetry is reported as a passing check.
    bool symmetry = false;
    for (const auto& c : r.checks) {
        if (c.nameKey == "check.clairaut_symmetry" && c.ok) symmetry = true;
        expect(c.name.empty(), "calculus checks carry no name sentence");
        expect(c.detail.empty(), "calculus checks carry no detail sentence");
    }
    expect(symmetry, "mixed partials agree (Clairaut)");
    expect(r.steps.size() >= 6, "partial derivatives show each derivative");

    // Three variables and no point still produce the symbolic partials.
    Result g = partialDerivatives("x*y*z", {"x", "y", "z"}, {}, {}, "en");
    expect(g.ok, "three-variable partials succeed");
    expect(g.partials.size() >= 3, "three first partials are present");
}

static void seriesCases() {
    // p-series 1/n^2 converges to pi^2/6.
    {
        Result r = seriesAnalysis("1/n^2", "n", 1, "en");
        expect(r.ok, "1/n^2 succeeds");
        expect(r.converges, "1/n^2 converges");
        expect(r.seriesTest == "p-series", "1/n^2 uses the p-series test");
        expect(r.seriesSum && toPlain(r.seriesSum) == "1/6*pi^2", "sum 1/n^2 = pi^2/6");
    }
    // Harmonic series diverges.
    {
        Result r = seriesAnalysis("1/n", "n", 1, "en");
        expect(r.ok, "1/n succeeds");
        expect(!r.converges, "1/n diverges");
        expect(r.seriesTest == "p-series", "1/n uses the p-series test");
    }
    // Geometric series 1/2^n sums to 1.
    {
        Result r = seriesAnalysis("1/2^n", "n", 1, "en");
        expect(r.ok, "1/2^n succeeds");
        expect(r.converges, "1/2^n converges");
        expect(r.seriesTest == "geometric", "1/2^n uses the geometric test");
        expect(r.seriesSum && toPlain(r.seriesSum) == "1", "sum 1/2^n = 1");
    }
    // Alternating harmonic series converges (conditionally).
    {
        Result r = seriesAnalysis("(-1)^n/n", "n", 1, "en");
        expect(r.ok, "alternating harmonic succeeds");
        expect(r.converges, "alternating harmonic converges");
        expect(r.seriesTest == "alternating", "alternating harmonic uses Leibniz");
    }
    // (-1)^n n has growing terms, so it diverges.
    {
        Result r = seriesAnalysis("(-1)^n*n", "n", 1, "en");
        expect(r.ok, "(-1)^n n succeeds");
        expect(!r.converges, "(-1)^n n diverges");
    }
    // A constant exponent such as pi is still a p-series.
    {
        Result r = seriesAnalysis("1/n^pi", "n", 1, "en");
        expect(r.ok, "1/n^pi succeeds");
        expect(r.seriesTest == "p-series", "1/n^pi is a p-series");
        expect(r.converges, "1/n^pi converges because pi > 1");
    }
    expect(seriesAnalysis("1/n^2", "n", 1, "en").steps.size() >= 5,
           "series shows the tests and the conclusion");

    // A term that is undefined at the chosen start is rejected with a hint.
    {
        Result r = seriesAnalysis("1/(n*ln(n))", "n", 1, "en");
        expect(!r.ok, "undefined first term is rejected");
        expect(r.errorCode == "series_start", "the series error carries a start hint");
        expect(r.errorParams.find("\"suggestion\":2") != std::string::npos,
               "the error suggests n = 2");
    }
    // A term no elementary test recognises stays honestly inconclusive.
    {
        Result r = seriesAnalysis("n/(n^2+1)", "n", 1, "en");
        expect(r.ok, "unrecognised term still produces evidence");
        expect(r.seriesTest == "numeric", "unrecognised term falls back to numeric evidence");
        expect(r.seriesVerdict == "inconclusive", "numeric evidence stays inconclusive");
    }
    // Factorial notation is parsed and the ratio test handles it symbolically:
    // 1/n! and n!/n^n converge, 1/(2n)! converges.
    {
        Result r = seriesAnalysis("1/n!", "n", 1, "en");
        expect(r.ok, "1/n! succeeds");
        expect(r.normalized && contains(toPlain(r.normalized), "!"), "the term prints as n!");
        expect(r.seriesTest == "ratio", "1/n! uses the ratio test");
        expect(r.converges, "1/n! converges");
    }
    {
        Result r = seriesAnalysis("n!/n^n", "n", 1, "en");
        expect(r.ok, "n!/n^n succeeds");
        expect(r.seriesTest == "ratio", "n!/n^n uses the ratio test");
        expect(r.converges, "n!/n^n converges by the ratio test");
    }
    {
        Result r = seriesAnalysis("1/(2*n)!", "n", 0, "en");
        expect(r.ok, "1/(2n)! succeeds");
        expect(r.seriesTest == "ratio", "1/(2n)! uses the ratio test");
        expect(r.converges, "1/(2n)! converges");
    }
    // An integer factorial simplifies to its value.
    {
        Ex e = simplify(parseODEExpr("5!", "x", nullptr, {"x", "e", "pi"}));
        expect(toPlain(e) == "120", "5! evaluates to 120");
    }
    // A shifted linear base is still a p-series: 1/(n+1) diverges, 1/(n+1)^2
    // converges, and 1/(2n+1) diverges.
    {
        Result r = seriesAnalysis("1/(n+1)", "n", 1, "en");
        expect(r.ok, "1/(n+1) succeeds");
        expect(r.seriesTest == "p-series", "1/(n+1) is a p-series");
        expect(!r.converges, "1/(n+1) diverges");
    }
    {
        Result r = seriesAnalysis("1/(n+1)^2", "n", 1, "en");
        expect(r.ok, "1/(n+1)^2 succeeds");
        expect(r.seriesTest == "p-series", "1/(n+1)^2 is a p-series");
        expect(r.converges, "1/(n+1)^2 converges");
        expect(r.seriesSum == nullptr, "a shifted p-series has no Basel exact sum");
    }
    {
        Result r = seriesAnalysis("1/(2*n+1)", "n", 1, "en");
        expect(r.ok, "1/(2n+1) succeeds");
        expect(r.seriesTest == "p-series", "1/(2n+1) is a p-series");
        expect(!r.converges, "1/(2n+1) diverges");
    }
    // A factorial quotient reduces to a p-series: n!/(n+1)! = 1/(n+1).
    {
        Result r = seriesAnalysis("n!/(n+1)!", "n", 1, "en");
        expect(r.ok, "n!/(n+1)! succeeds");
        expect(r.normalized && toPlain(r.normalized) == "1/(n + 1)",
               "n!/(n+1)! simplifies to 1/(n+1)");
        expect(r.seriesTest == "p-series", "n!/(n+1)! is a p-series");
        expect(!r.converges, "n!/(n+1)! diverges");
    }
    // An unbounded ratio also decides divergence.
    {
        Result r = seriesAnalysis("(2*n)!/n!", "n", 1, "en");
        expect(r.ok, "(2n)!/n! succeeds");
        expect(r.seriesTest == "ratio", "(2n)!/n! uses the ratio test");
        expect(!r.converges, "(2n)!/n! diverges");
    }
    // The root test decides a term whose ratio limit would be 1.
    {
        Result r = seriesAnalysis("(n/(2*n+1))^n", "n", 1, "en");
        expect(r.ok, "(n/(2n+1))^n succeeds");
        expect(r.seriesTest == "root", "(n/(2n+1))^n uses the root test");
        expect(r.converges, "(n/(2n+1))^n converges because the root limit is 1/2");
    }
}

static void behaviourCases() {
    // A pole and a horizontal asymptote.
    {
        Result r = analyseFunction("1/(x-1)", "x", -5.0, 5.0, "en");
        expect(r.ok, "behaviour 1/(x-1) succeeds");
        std::string all;
        for (const auto& a : r.asymptotes) all += a + "|";
        expect(contains(all, "x = 1"), "1/(x-1) has the vertical asymptote x = 1");
        expect(contains(all, "y = 0"), "1/(x-1) has the horizontal asymptote y = 0");
        expect(!r.sketch.empty(), "behaviour produces a sketch summary");
    }
    // An oblique asymptote y = x for (x^2+1)/x.
    {
        Result r = analyseFunction("(x^2+1)/x", "x", -10.0, 10.0, "en");
        expect(r.ok, "behaviour (x^2+1)/x succeeds");
        std::string all;
        for (const auto& a : r.asymptotes) all += a + "|";
        expect(contains(all, "y = x"), "(x^2+1)/x has the oblique asymptote y = x");
        expect(r.pieces.size() >= 4, "monotonicity is split at the pole");
    }
    // A polynomial has no asymptotes but does have a y-intercept.
    {
        Result r = analyseFunction("x^2 - 4", "x", -5.0, 5.0, "en");
        expect(r.ok, "behaviour polynomial succeeds");
        expect(r.asymptotes.empty(), "a polynomial has no asymptote");
        expect(r.hasYIntercept && toPlain(r.yIntercept) == "-4", "y-intercept of x^2-4 is -4");
    }
    // Poles are reported exactly and never leak into inflection points or
    // x-intercepts: 1/(x^2-1) has no zero and no bend.
    {
        Result r = analyseFunction("1/(x^2-1)", "x", -3.0, 3.0, "en");
        expect(r.ok, "behaviour 1/(x^2-1) succeeds");
        std::string all;
        for (const auto& a : r.asymptotes) all += a + "|";
        expect(contains(all, "x = -1"), "1/(x^2-1) has the pole x = -1");
        expect(contains(all, "x = 1"), "1/(x^2-1) has the pole x = 1");
        expect(!contains(all, "2001/2000"), "the pole is exact, not a grid approximation");
        expect(r.xIntercepts.empty(), "a pole is not an x-intercept");
        expect(r.inflections.empty(), "a pole is not an inflection point");
        for (const auto& line : r.sketch) {
            expect(!contains(line, "[-1, -1]"), "no degenerate concavity interval at the pole");
            expect(!contains(line, "[1, 1]"), "no degenerate concavity interval at the pole");
        }
    }
    // tan has poles at ±pi/2 and a single inflection at 0.
    {
        Result r = analyseFunction("tan(x)", "x", -2.0, 2.0, "en");
        expect(r.ok, "behaviour tan(x) succeeds");
        std::string all;
        for (const auto& a : r.asymptotes) all += a + "|";
        expect(contains(all, "pi"), "tan reports the exact pi poles");
        expect(!contains(all, "1571/1000"), "tan poles are not grid approximations");
        expect(r.inflections.size() == 1, "tan has one inflection in [-2, 2]");
        if (r.inflections.size() == 1)
            expect(toPlain(r.inflections[0].x) == "0", "tan bends at x = 0");
        expect(r.xIntercepts.size() == 1, "tan has one x-intercept in [-2, 2]");
    }
    // A rational function with a pole and an oblique asymptote.
    {
        Result r = analyseFunction("x^2/(x-1)", "x", -3.0, 3.0, "en");
        expect(r.ok, "behaviour x^2/(x-1) succeeds");
        std::string all;
        for (const auto& a : r.asymptotes) all += a + "|";
        expect(contains(all, "x = 1"), "x^2/(x-1) has the pole x = 1");
        expect(contains(all, "y = x + 1"), "x^2/(x-1) has the oblique asymptote y = x + 1");
        expect(r.xIntercepts.size() == 1 && toPlain(r.xIntercepts[0]) == "0",
               "the only x-intercept is 0");
        expect(r.inflections.empty(), "x^2/(x-1) has no inflection point");
    }
    // A logarithmic domain boundary is a vertical asymptote, and the undefined
    // half of the window must not create phantom poles.
    {
        Result r = analyseFunction("ln(x)", "x", -1.0, 10.0, "en");
        expect(r.ok, "behaviour ln(x) succeeds");
        expect(r.asymptotes.size() == 1, "ln(x) has exactly one vertical asymptote");
        if (r.asymptotes.size() == 1)
            expect(r.asymptotes[0] == "x = 0", "the asymptote of ln(x) is x = 0");
        expect(r.critical.empty(), "ln(x) has no critical point");
        bool sawOutside = false;
        for (const auto& line : r.sketch) {
            if (contains(line, "[-1, 0]")) sawOutside = true;
        }
        expect(!sawOutside, "monotonicity/concavity skip the undefined half of the window");
    }
    // Roots that are multiples of pi print exactly, not as huge fractions.
    {
        Result r = analyseFunction("tan(x)", "x", -4.0, 4.0, "en");
        expect(r.ok, "behaviour tan(x) on [-4,4] succeeds");
        bool sawPi = false, sawFraction = false;
        for (const auto& root : r.xIntercepts) {
            const std::string s = toPlain(root);
            if (contains(s, "pi")) sawPi = true;
            if (contains(s, "80143857")) sawFraction = true;
        }
        expect(sawPi, "tan zeros print as multiples of pi");
        expect(!sawFraction, "tan zeros are not printed as a huge fraction");
    }
    // A simple radical is recovered exactly: the inflections of exp(-x^2)
    // are at x = ±1/sqrt(2).
    {
        Result r = analyseFunction("exp(-x^2)", "x", -3.0, 3.0, "en");
        expect(r.ok, "behaviour exp(-x^2) succeeds");
        expect(r.inflections.size() == 2, "exp(-x^2) has two inflection points");
        for (const auto& ip : r.inflections)
            expect(contains(toPlain(ip.x), "sqrt(2)"),
                   "the inflection of exp(-x^2) prints as a radical");
    }
    // sqrt(x) is undefined for x < 0; that edge is not a pole.
    {
        Result r = analyseFunction("sqrt(x)", "x", -1.0, 10.0, "en");
        expect(r.ok, "behaviour sqrt(x) succeeds");
        expect(r.asymptotes.empty(), "sqrt(x) has no vertical asymptote");
        bool sawOutside = false;
        for (const auto& line : r.sketch) {
            if (contains(line, "[-1, 0]")) sawOutside = true;
        }
        expect(!sawOutside, "sqrt(x) does not describe the undefined negative half");
    }
}

int main() {
    taylorCases();
    integralApplicationCases();
    partialCases();
    seriesCases();
    behaviourCases();
    if (failures == 0) {
        std::cout << "All calculus-extension tests passed.\n";
    } else {
        std::cout << failures << " failure(s).\n";
    }
    return failures == 0 ? 0 : 1;
}
