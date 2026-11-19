// Transcendental functions and integration techniques (96, 97).
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
    for (const auto& c : r.checks)
        if (!c.ok) return false;
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

static void testTranscendental() {
    {
        Result r = solve("transcendental", {});
        expect(r.ok, "the default transcendental computation runs");
        expect(allChecksOk(r), "the transcendental checks pass");
        expect(!r.numericOnly, "the symbolic derivative stays exact");
        expect(r.derivative != nullptr, "the first derivative is reported");
        const std::string all = allFunctions(r);
        expect(all.find("f'(x)") != std::string::npos,
               "the derivative is printed");
        expect(all.find("integral f dx") != std::string::npos,
               "the antiderivative is printed");
        expect(all.find("Taylor polynomial") != std::string::npos,
               "the Taylor polynomial is printed");
        expect(all.find("family") != std::string::npos,
               "the transcendental family table is printed");
    }
    {
        Result r = solve("transcendental",
                         {{"a", 1.0}, {"x0", 2.0}, {"N", 4.0}},
                         {{"f", "ln(x)"}});
        expect(r.ok, "the logarithm is analysed about a = 1");
        expect(allChecksOk(r), "the logarithm checks pass");
        expect(std::fabs(valueOf(r.value) - std::log(2.0)) < 1e-9,
               "f(2) = ln 2");
    }
    {
        Result r = solve("transcendental", {{"N", 3.0}},
                         {{"f", "sinh(x)/cosh(x)"}});
        expect(r.ok, "a hyperbolic quotient is accepted");
        expect(allChecksOk(r), "the hyperbolic checks pass");
    }
}

static void testIntegrationTechniques() {
    {
        Result r = solve("integration_techniques", {});
        expect(r.ok, "the default integration example runs");
        expect(allChecksOk(r), "the default integration checks pass");
        expect(r.closedForm, "x exp(x) has a closed-form antiderivative");
        expect(std::fabs(valueOf(r.value) - 1.0) < 1e-9,
               "the definite integral of x exp(x) on [0,1] is 1");
        expect(allFunctions(r).find("parts") != std::string::npos,
               "the parts method is reported");
    }
    {
        Result r = solve("integration_techniques", {{"a", 0.0}, {"b", 1.0}},
                         {{"technique", "substitution"}});
        expect(r.ok, "the substitution example runs");
        expect(allChecksOk(r), "the substitution checks pass");
        expect(allFunctions(r).find("substitution") != std::string::npos,
               "the substitution technique is reported");
    }
    {
        Result r = solve("integration_techniques", {{"a", 0.0}, {"b", 1.0}},
                         {{"technique", "partial_fractions"}});
        expect(r.ok, "the partial-fraction example runs");
        expect(allChecksOk(r), "the partial-fraction checks pass");
        expect(allFunctions(r).find("partial_fractions") != std::string::npos,
               "the partial-fraction technique is listed");
    }
    {
        Result r = solve("integration_techniques", {{"a", 0.0}, {"b", 1.0}},
                         {{"technique", "trig_substitution"}});
        expect(r.ok, "the trigonometric-substitution example runs");
        expect(allChecksOk(r), "the trigonometric-substitution checks pass");
        // The definite integral of sqrt(1 - x^2) on [0,1] is pi/4.
        expect(std::fabs(valueOf(r.value) - 3.14159265358979323846 / 4.0) < 1e-6,
               "the quarter-circle area is pi/4");
    }
    {
        Result r = solve("integration_techniques", {{"a", 0.0}, {"b", 1.0}},
                         {{"technique", "table"}});
        expect(r.ok, "the table example runs");
        expect(allChecksOk(r), "the table checks pass");
    }
    {
        Result r = solve("integration_techniques", {{"a", 0.0}, {"b", 1.0}},
                         {{"f", "sin(x^2)"}});
        expect(r.ok, "a non-elementary integrand is still evaluated");
        expect(!r.closedForm && r.numericOnly,
               "the non-elementary integral is labelled numerical");
        expect(std::fabs(valueOf(r.value) - 0.3102683017) < 1e-5,
               "the adaptive quadrature value is reported");
    }
    {
        Result r = solve("integration_techniques", {{"a", 1.0}, {"b", 1.0}});
        expect(!r.ok && r.errorCode == "bad_interval",
               "an empty interval is refused");
    }
}

int main() {
    testTranscendental();
    testIntegrationTechniques();
    if (failures == 0) {
        std::cout << "all calculus topic tests passed\n";
        return 0;
    }
    std::cout << failures << " calculus topic test(s) failed\n";
    return 1;
}
