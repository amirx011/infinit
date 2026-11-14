// Boundary value problems and Sturm–Liouville problems (85, 90, 91).
#include "expr.hpp"
#include "numeric.hpp"
#include "solver.hpp"
#include "topics.hpp"

#include <cmath>
#include <iostream>
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

static void testBvpTwoPoint() {
    {
        Result r = solve("bvp_two_point",
                         {{"a", 0.0}, {"b", 1.0}, {"alpha", 0.0},
                          {"beta", 0.0}, {"N", 40.0}},
                         {{"p", "0"}, {"q", "0"}, {"f", "1"}});
        expect(r.ok, "-y'' = 1 solves");
        expect(allChecksOk(r), "the BVP checks pass");
        expect(!r.closedForm && r.numericOnly, "the grid answer is labelled numerical");
        expect(std::fabs(valueOf(r.value) - 0.125) < 2e-3,
               "the midpoint value is close to 1/8");
    }
    {
        Result r = solve("bvp_two_point",
                         {{"a", 0.0}, {"b", 1.0}, {"alpha", 0.0},
                          {"beta", 1.0}, {"N", 40.0}},
                         {{"p", "1"}, {"q", "0"}, {"f", "0"}});
        expect(r.ok, "-y'' + y' = 0 solves");
        expect(allChecksOk(r), "the first-order term checks pass");
        const double expected = (std::exp(0.5) - 1.0) / (std::exp(1.0) - 1.0);
        expect(std::fabs(valueOf(r.value) - expected) < 2e-3,
               "the midpoint matches (e^x - 1)/(e - 1)");
    }
    {
        Result r = solve("bvp_two_point",
                         {{"a", 0.0}, {"b", 1.0}, {"alpha", 0.0},
                          {"beta", 0.0}, {"N", 20.0}},
                         {{"p", "0"}, {"q", "1"}, {"f", "0"}});
        expect(r.ok, "-y'' + y = 0 solves");
        expect(allChecksOk(r), "the q term checks pass");
    }
}

static void testOccurrence() {
    {
        Result r = solve("bvp_occurrence", {}, {{"case", "heat_rod"}});
        expect(r.ok, "the heat rod is classified");
        expect(allFunctions(r).find("boundary value") != std::string::npos,
               "the rod is a BVP");
        expect(allFunctions(r).find("bvp_two_point") != std::string::npos,
               "the recommended solver is reported");
        expect(!r.closedForm, "no numeric answer is invented");
    }
    {
        Result r = solve("bvp_occurrence", {}, {{"case", "ivp"}});
        expect(r.ok, "the IVP is classified");
        expect(allFunctions(r).find("initial value") != std::string::npos,
               "the IVP is recognised");
    }
    {
        Result r = solve("bvp_occurrence", {}, {{"case", "eigen"}});
        expect(r.ok, "the eigenvalue problem is classified");
        expect(allFunctions(r).find("Sturm") != std::string::npos,
               "the Sturm–Liouville case is recognised");
    }
    {
        Result r = solve("bvp_occurrence", {}, {{"case", "beam"}});
        expect(r.ok, "the beam is classified");
        expect(allFunctions(r).find("fourth") != std::string::npos ||
                   allFunctions(r).find("higher-order") != std::string::npos,
               "the fourth-order equation is recognised");
    }
}

static void testSturmLiouville() {
    {
        Result r = solve("sturm_liouville",
                         {{"a", 0.0}, {"b", 1.0}, {"N", 80.0}, {"K", 3.0}},
                         {{"p", "1"}, {"q", "0"}, {"w", "1"}});
        expect(r.ok, "-y'' = lambda y is analysed");
        expect(allChecksOk(r), "the Sturm–Liouville checks pass");
        expect(!r.closedForm && r.numericOnly, "the eigenvalues are numerical");
        const std::string all = allFunctions(r);
        expect(all.find("9.86") != std::string::npos, "lambda_1 is about pi^2");
        expect(all.find("39.4") != std::string::npos, "lambda_2 is about 4 pi^2");
    }
    {
        Result r = solve("sturm_liouville",
                         {{"a", 0.0}, {"b", 1.0}, {"N", 60.0}, {"K", 2.0}},
                         {{"p", "1"}, {"q", "1"}, {"w", "1"}});
        expect(r.ok, "-y'' + y = lambda y is analysed");
        expect(allChecksOk(r), "the shifted problem checks pass");
        // lambda_n = n^2 pi^2 + 1.
        expect(allFunctions(r).find("10.86") != std::string::npos,
               "lambda_1 is about pi^2 + 1");
    }
}

static void testNonhomogeneousExpansion() {
    {
        Result r = solve("bvp_nonhomogeneous",
                         {{"a", 0.0}, {"b", 1.0}, {"N", 80.0}},
                         {{"p", "1"}, {"q", "0"}, {"f", "1"}});
        expect(r.ok, "-y'' = 1 with zero data is expanded");
        expect(allChecksOk(r), "the modal expansion checks pass");
        expect(!r.closedForm && r.numericOnly,
               "the modal expansion is labelled numerical");
        expect(std::fabs(valueOf(r.value) - 0.125) < 2e-3,
               "the midpoint value is close to 1/8");
        expect(allFunctions(r).find("lambda") != std::string::npos,
               "the eigenvalues of the basis are reported");
    }
    {
        Result r = solve("bvp_nonhomogeneous",
                         {{"a", 0.0}, {"b", 1.0}, {"N", 60.0}},
                         {{"p", "1"}, {"q", "1"}, {"f", "1"}});
        expect(r.ok, "-y'' + y = 1 is expanded");
        expect(allChecksOk(r), "the shifted forcing checks pass");
        // y = 1 - cosh(x) + (cosh(1) - 1) sinh(x) / sinh(1), value 0.1132 at 0.5.
        expect(std::fabs(valueOf(r.value) - 0.1132) < 5e-3,
               "the midpoint agrees with the exact solution");
    }
    {
        Result r = solve("bvp_nonhomogeneous",
                         {{"a", 0.0}, {"b", 1.0}, {"alpha", 0.0},
                          {"beta", 1.0}, {"N", 50.0}},
                         {{"p", "1"}, {"q", "0"}, {"f", "x"}});
        expect(r.ok, "inhomogeneous boundary data are handled");
        expect(allChecksOk(r), "the lifted problem checks pass");
        // y = (7x - x^3)/6, midpoint 0.5625.
        expect(std::fabs(valueOf(r.value) - 0.5625) < 2e-3,
               "the midpoint matches the lifted solution");
    }
    {
        Result r = solve("bvp_nonhomogeneous",
                         {{"a", 0.0}, {"b", 1.0}, {"N", 40.0}},
                         {{"p", "0"}, {"q", "1"}, {"f", "1"}});
        expect(!r.ok && r.errorCode == "degenerate_operator",
               "a vanishing p is refused honestly");
    }
}

int main() {
    testBvpTwoPoint();
    testOccurrence();
    testSturmLiouville();
    testNonhomogeneousExpansion();
    if (failures == 0) {
        std::cout << "all bvp tests passed\n";
        return 0;
    }
    std::cout << failures << " bvp test(s) failed\n";
    return 1;
}
