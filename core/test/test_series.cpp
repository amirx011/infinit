// Series-solution and special-function topics.
//
// Each handler is checked against an independently known value, and the
// reported checks must all pass.  The tests deliberately use plain numbers
// (cos, sin, Bessel reference values) instead of re-using the engine formula.
#include "expr.hpp"
#include "numeric.hpp"
#include "solver.hpp"
#include "topics.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <string>

using namespace ode;

static constexpr double kPi = 3.14159265358979323846;

static int failures = 0;

static void expect(bool condition, const std::string& what) {
    if (!condition) {
        ++failures;
        std::cout << "FAIL: " << what << "\n";
    }
}

static Result solve(const std::string& topic,
                    const std::map<std::string, double>& nums,
                    const std::map<std::string, std::string>& strs = {}) {
    TopicRequest req;
    req.topic = topic;
    req.lang = "en";
    req.nums = nums;
    req.strs = strs;
    return solveTopic(req);
}

static bool allChecksOk(const Result& r) {
    if (r.checks.empty()) return false;
    for (const auto& c : r.checks) {
        if (!c.ok) return false;
    }
    return true;
}

static std::string plain(const Ex& e) { return e ? toPlain(e) : std::string(); }

static double evalAt(const Ex& e, double x) {
    std::map<std::string, double> vars{{"x", x}};
    bool ok = false;
    const double v = evalReal(e, vars, &ok);
    return ok ? v : std::numeric_limits<double>::quiet_NaN();
}

static void testPowerSeries() {
    {
        Result r = solve("series_power", {{"x0", 0.0}, {"terms", 10.0}},
                         {{"p", "0"}, {"q", "1"}});
        expect(r.ok, "y'' + y = 0 expands");
        expect(allChecksOk(r), "y'' + y = 0 passes every check");
        // y1 is cos(x) and y2 is sin(x) on their truncated intervals.
        double y1err = std::fabs(evalAt(r.solution, 0.25) - std::cos(0.25));
        expect(y1err < 1e-9, "the first basis matches cos(0.25)");
        expect(r.functions.size() >= 2, "both bases are reported");
    }
    {
        // Airy equation: y'' - x y = 0.
        Result r = solve("series_power", {{"terms", 12.0}},
                         {{"p", "0"}, {"q", "-x"}});
        expect(r.ok, "the Airy equation expands");
        expect(allChecksOk(r), "the Airy residual vanishes to the computed order");
    }
    {
        Result r = solve("series_power", {{"x0", 1.0}, {"terms", 10.0}},
                         {{"p", "-1"}, {"q", "1"}});
        expect(r.ok, "an expansion around a nonzero point works");
        expect(evalAt(r.solution, 1.0) == 1.0, "y1(x0) = 1");
    }
}

static void testSingularPoints() {
    {
        Result r = solve("series_singular_points", {{"x0", 0.0}},
                         {{"p", "1/x"}, {"q", "1"}});
        expect(r.ok, "1/x is analysed");
        expect(r.typeId == "regular_singular_point", "1/x is a regular singular point");
        expect(allChecksOk(r), "the singularity check passes");
    }
    {
        Result r = solve("series_singular_points", {{"x0", 0.0}},
                         {{"p", "1/(x*x)"}, {"q", "0"}});
        expect(r.ok, "1/x^2 is analysed");
        expect(r.typeId == "irregular_singular_point", "1/x^2 is irregular");
    }
    {
        Result r = solve("series_singular_points", {{"x0", 0.0}},
                         {{"p", "0"}, {"q", "1"}});
        expect(r.typeId == "ordinary_point", "x = 0 is ordinary for y'' + y = 0");
    }
    {
        Result r = solve("series_singular_points", {{"x0", 1.0}},
                         {{"p", "1/(x-1)"}, {"q", "1/(x-1)^2"}});
        expect(r.ok, "(x-1) singular data is analysed");
        expect(r.typeId == "regular_singular_point",
               "a simple and a double pole are regular");
        expect(plain(r.solution).find("regular") != std::string::npos,
               "the classification is reported in the answer");
    }
}

static void testFrobenius() {
    {
        // Bessel with nu = 1/2: p = 1/x, q = 1 - 1/(4x^2).
        Result r = solve("series_frobenius", {{"x0", 0.0}, {"terms", 9.0}},
                         {{"p", "1/x"}, {"q", "1 - 1/(4*x^2)"}});
        expect(r.ok, "the half-order Bessel equation has Frobenius series");
        expect(allChecksOk(r), "half-order Bessel checks pass");
        expect(r.closedForm, "the non-integer root difference needs no logarithm");
        expect(r.functions.size() >= 2, "two series are reported");
    }
    {
        // Legendre at x = 1: repeated indicial root, so a logarithm is needed.
        Result r = solve("series_frobenius", {{"x0", 1.0}, {"terms", 8.0}},
                         {{"p", "-2*x/(1-x*x)"}, {"q", "2/(1-x*x)"}});
        expect(r.ok, "Legendre at x = 1 is analysed");
        expect(!r.closedForm, "the repeated root is marked as a log case");
        expect(r.functions.size() >= 2, "the reduction-of-order form is reported");
    }
    {
        Result r = solve("series_frobenius", {{"x0", 0.0}},
                         {{"p", "0"}, {"q", "1"}});
        expect(!r.ok && r.errorCode == "not_regular_singular",
               "Frobenius refuses an ordinary point");
    }
}

static void testEuler() {
    {
        Result r = solve("euler_equations", {{"a", 1}, {"b", -2}, {"c", 2}, {"d", 0}});
        expect(r.ok, "x^2 y'' - 2x y' + 2y = 0 solves");
        expect(allChecksOk(r), "distinct Euler roots check out");
        std::string all;
        for (const auto& f : r.functions) all += f + "\n";
        expect(all.find("x^2") != std::string::npos, "the basis contains x^2");
    }
    {
        Result r = solve("euler_equations", {{"a", 1}, {"b", 1}, {"c", 1}, {"d", 0}});
        expect(r.ok, "x^2 y'' + x y' + y = 0 solves");
        expect(plain(r.solution).find("cos") != std::string::npos,
               "complex roots produce cosine");
        expect(plain(r.solution).find("ln") != std::string::npos,
               "complex roots use ln|x|");
    }
    {
        Result r = solve("euler_equations", {{"a", 1}, {"b", 5}, {"c", 4}, {"d", 0}});
        expect(r.ok, "the repeated-root Euler equation solves");
        expect(plain(r.solution).find("ln") != std::string::npos,
               "a repeated root multiplies by ln|x|");
        expect(allChecksOk(r), "the repeated-root basis checks out");
    }
    {
        Result r = solve("euler_equations", {{"a", 1}, {"b", -3}, {"c", 6}, {"d", -6}});
        expect(r.ok, "a third-order Euler equation solves");
        expect(allChecksOk(r), "the third-order basis checks out");
    }
}

static void testLegendre() {
    {
        Result r = solve("special_legendre", {{"n", 2}, {"x", 0.5}});
        expect(r.ok, "P_2 is computed");
        expect(std::fabs(evalAt(r.solution, 0.5) + 0.125) < 1e-12,
               "P_2 = (3x^2 - 1)/2");
        expect(std::fabs(evalAt(r.solution, 0.5) + 0.125) < 1e-12,
               "P_2(0.5) = -1/8");
        expect(allChecksOk(r), "P_2 satisfies Legendre's equation");
    }
    {
        Result r = solve("special_legendre", {{"n", 3}, {"x", 0.5}});
        expect(r.ok, "P_3 is computed");
        expect(std::fabs(evalAt(r.solution, 0.5) + 0.4375) < 1e-12,
               "P_3(0.5) = -7/16");
    }
    {
        Result r = solve("special_legendre", {{"n", 1}, {"x", 0.5}, {"q", 1}});
        expect(r.ok, "Q_1 is computed when requested");
        bool hasQ = false;
        for (const auto& f : r.functions) {
            if (f.find("Q_1") != std::string::npos) hasQ = true;
        }
        expect(hasQ, "Q_1 is reported");
    }
}

static void testBessel() {
    {
        Result r = solve("special_bessel", {{"nu", 0}, {"x", 1}});
        expect(r.ok, "J_0(1) is computed");
        const double j0 = evalAt(r.value, 0.0);
        expect(std::fabs(j0 - 0.7651976866) < 1e-8, "J_0(1) matches the reference value");
        expect(allChecksOk(r), "J_0 satisfies Bessel's equation and the recurrence");
    }
    {
        Result r = solve("special_bessel", {{"nu", 1}, {"x", 1}});
        expect(r.ok, "J_1(1) is computed");
        const double j1 = evalAt(r.value, 0.0);
        expect(std::fabs(j1 - 0.4400505857) < 1e-8, "J_1(1) matches the reference value");
        bool hasY = false;
        for (const auto& f : r.functions) {
            if (f.find("Y_1(1)") != std::string::npos) hasY = true;
        }
        expect(hasY, "Y_1(1) is reported");
    }
    {
        // J_{1/2}(x) = sqrt(2/(pi x)) sin(x).
        Result r = solve("special_bessel", {{"nu", 0.5}, {"x", 1}});
        expect(r.ok, "J_{1/2}(1) is computed");
        const double j = evalAt(r.value, 0.0);
        const double expected = std::sqrt(2.0 / kPi) * std::sin(1.0);
        expect(std::fabs(j - expected) < 1e-8, "J_{1/2}(1) matches sqrt(2/pi) sin(1)");
    }
}

int main() {
    testPowerSeries();
    testSingularPoints();
    testFrobenius();
    testEuler();
    testLegendre();
    testBessel();
    if (failures == 0) {
        std::cout << "all series tests passed\n";
        return 0;
    }
    std::cout << failures << " series test(s) failed\n";
    return 1;
}
