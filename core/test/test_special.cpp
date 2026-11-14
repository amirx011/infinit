// Singular Sturm–Liouville problems and orthogonal series (93, 94, 95).
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

static int failures = 0;

static void expect(bool condition, const std::string& what) {
    if (!condition) {
        ++failures;
        std::cout << "FAIL: " << what << "\n";
    }
}

static Result solve(const std::string& topic,
                    const std::map<std::string, double>& nums = {},
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

static std::string allFunctions(const Result& r) {
    std::string all;
    for (const auto& f : r.functions) all += f + "\n";
    return all;
}

static double valueOf(const Ex& e) {
    bool ok = false;
    const double v = evalReal(e, {}, &ok);
    return ok ? v : std::numeric_limits<double>::quiet_NaN();
}

static void testSingularSl() {
    {
        Result r = solve("sturm_liouville_singular",
                         {{"nu", 0.0}, {"L", 1.0}, {"K", 4.0}},
                         {{"family", "bessel"}});
        expect(r.ok, "the Bessel singular problem is analysed");
        expect(allChecksOk(r), "the Bessel singular checks pass");
        expect(!r.closedForm && r.numericOnly,
               "the singular eigenvalues are numerical");
        // lambda_1 = j_{0,1}^2 = 5.783185...
        expect(std::fabs(valueOf(r.value) - 5.7832) < 1e-3,
               "lambda_1 is the square of the first Bessel zero");
        expect(allFunctions(r).find("boundary") != std::string::npos,
               "the boundary behaviour is reported");
    }
    {
        Result r = solve("sturm_liouville_singular", {{"K", 3.0}},
                         {{"family", "legendre"}});
        expect(r.ok, "the Legendre singular problem is analysed");
        expect(allChecksOk(r), "the Legendre singular checks pass");
        const std::string all = allFunctions(r);
        expect(all.find("2") != std::string::npos, "lambda_1 = 2 is reported");
        expect(all.find("6") != std::string::npos, "lambda_2 = 6 is reported");
    }
    {
        Result r = solve("sturm_liouville_singular", {{"K", 3.0}},
                         {{"family", "laguerre"}});
        expect(r.ok, "the Laguerre singular problem is analysed");
        expect(allChecksOk(r), "the Laguerre singular checks pass");
        expect(allFunctions(r).find("e^-x") != std::string::npos ||
                   allFunctions(r).find("e^{-x}") != std::string::npos,
               "the Laguerre weight is reported");
    }
    {
        // Custom p = x with q = 0, w = 1 on (0,1) reduces to the Bessel case.
        Result r = solve("sturm_liouville_singular",
                         {{"a", 0.0}, {"b", 1.0}, {"N", 120.0}, {"K", 3.0}},
                         {{"family", "custom"}, {"p", "x"}, {"q", "0"},
                          {"w", "1"}});
        expect(r.ok, "a custom singular problem is analysed");
        expect(allChecksOk(r), "the custom singular checks pass");
        // -(x y')' = lambda y with y(1) = 0 has lambda_1 = (j_{0,1}/2)^2.
        expect(std::fabs(valueOf(r.value) - 1.4458) < 5e-2,
               "the custom p = x eigenvalue matches the Bessel zero");
    }
    {
        Result r = solve("sturm_liouville_singular",
                         {{"a", 0.0}, {"b", 1.0}, {"N", 60.0}, {"K", 2.0}},
                         {{"family", "custom"}, {"p", "1"}, {"q", "0"},
                          {"w", "1"}});
        expect(!r.ok && r.errorCode == "not_singular_endpoint",
               "a regular endpoint is refused by the singular tool");
    }
}

static void testBesselSeries() {
    {
        Result r = solve("bessel_series",
                         {{"nu", 0.0}, {"L", 1.0}, {"N", 12.0}},
                         {{"f", "1 - x^2"}});
        expect(r.ok, "the Bessel series of 1 - x^2 is computed");
        expect(allChecksOk(r), "the Bessel series checks pass");
        expect(!r.closedForm && r.numericOnly,
               "the Bessel coefficients are numerical");
        const std::string all = allFunctions(r);
        expect(all.find("alpha_n") != std::string::npos,
               "the Bessel zeros are tabulated");
        expect(all.find("Parseval") != std::string::npos,
               "Parseval is reported");
    }
    {
        Result r = solve("bessel_series",
                         {{"nu", 0.0}, {"L", 1.0}, {"N", 8.0}},
                         {{"f", "x"}});
        expect(r.ok, "the Bessel series of x is computed");
        expect(allChecksOk(r), "the linear forcing checks pass");
    }
    {
        Result r = solve("bessel_series",
                         {{"nu", 1.0}, {"L", 2.0}, {"N", 10.0}},
                         {{"f", "x*(1 - x/2)"}});
        expect(r.ok, "a first-order Bessel series on a longer interval works");
        expect(allChecksOk(r), "the nu = 1 checks pass");
    }
}

static void testOrthogonalSeries() {
    {
        // f = x is exactly P_1 on [-1, 1].
        Result r = solve("orthogonal_series",
                         {{"a", -1.0}, {"b", 1.0}, {"N", 4.0}, {"x0", 0.5}},
                         {{"family", "legendre"}, {"f", "x"}});
        expect(r.ok, "the Legendre series of x is computed");
        expect(allChecksOk(r), "the Legendre series checks pass");
        expect(std::fabs(valueOf(r.value) - 0.5) < 1e-9,
               "the Legendre partial sum reproduces x at 0.5");
        expect(!r.closedForm && r.numericOnly,
               "the orthogonal coefficients are numerical");
    }
    {
        Result r = solve("orthogonal_series",
                         {{"a", -1.0}, {"b", 1.0}, {"N", 5.0}, {"x0", 0.5}},
                         {{"family", "chebyshev"}, {"f", "x"}});
        expect(r.ok, "the Chebyshev series of x is computed");
        expect(allChecksOk(r), "the Chebyshev series checks pass");
        expect(std::fabs(valueOf(r.value) - 0.5) < 1e-9,
               "the Chebyshev partial sum reproduces x at 0.5");
    }
    {
        Result r = solve("orthogonal_series",
                         {{"a", 0.0}, {"b", 1.0}, {"N", 6.0}},
                         {{"family", "sine"}, {"f", "sin(pi*x)"}});
        expect(r.ok, "the sine series of sin(pi x) is computed");
        expect(allChecksOk(r), "the sine series checks pass");
        expect(std::fabs(valueOf(r.value) - 1.0) < 1e-9,
               "the sine partial sum reproduces the mode at 0.5");
    }
    {
        Result r = solve("orthogonal_series",
                         {{"a", 0.0}, {"b", 1.0}, {"N", 6.0}},
                         {{"family", "cosine"}, {"f", "cos(2*pi*x)"}});
        expect(r.ok, "the cosine series of cos(2 pi x) is computed");
        expect(allChecksOk(r), "the cosine series checks pass");
        expect(std::fabs(valueOf(r.value) - std::cos(3.14159265358979323846)) < 1e-9,
               "the cosine partial sum reproduces the mode at 0.5");
    }
    {
        Result r = solve("orthogonal_series",
                         {{"a", 0.0}, {"b", 1.0}, {"N", 10.0}, {"nu", 0.0}},
                         {{"family", "bessel"}, {"f", "1 - x^2"}});
        expect(r.ok, "the Bessel orthogonal series is computed");
        expect(allChecksOk(r), "the Bessel orthogonal checks pass");
    }
}

int main() {
    testSingularSl();
    testBesselSeries();
    testOrthogonalSeries();
    if (failures == 0) {
        std::cout << "all special series tests passed\n";
        return 0;
    }
    std::cout << failures << " special series test(s) failed\n";
    return 1;
}
