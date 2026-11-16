// Linear systems and numerical methods for systems (topics 75–80).
//
// The checks use reference formulas and reference values, never the engine's
// own intermediate strings.
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
                    const std::map<std::string, std::string>& strs,
                    const std::map<std::string, double>& nums = {}) {
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

static double evalValue(const Ex& e) {
    std::map<std::string, double> vars;
    bool ok = false;
    const double v = evalReal(e, vars, &ok);
    return ok ? v : std::numeric_limits<double>::quiet_NaN();
}

static void testSystemTheory() {
    {
        Result r = solve("system_theory", {{"matrix", "1, 1; 4, 1"}});
        expect(r.ok, "A = [[1,1],[4,1]] is analysed");
        expect(allChecksOk(r), "the fundamental matrix checks pass");
        expect(r.independent, "the solution set is independent");
        expect(allFunctions(r).find("3") != std::string::npos,
               "the eigenvalue 3 is reported");
        expect(allFunctions(r).find("-1") != std::string::npos,
               "the eigenvalue -1 is reported");
    }
    {
        Result r = solve("system_theory", {{"matrix", "0, 1; -2, -3"}});
        expect(r.ok, "a second 2x2 system is analysed");
        expect(allChecksOk(r), "the second system checks pass");
    }
    {
        Result r = solve("system_theory", {{"matrix", "1, 0, 0; 0, 2, 0; 0, 0, 3"}});
        expect(r.ok, "a diagonal 3x3 system is analysed");
        expect(allChecksOk(r), "the diagonal system checks pass");
    }
}

static void testComplexEigen() {
    {
        Result r = solve("system_complex_eigen", {{"matrix", "1, -1; 1, 1"}});
        expect(r.ok, "the rotation-like system is analysed");
        expect(allChecksOk(r), "complex eigen checks pass");
        expect(allFunctions(r).find("x1(t)") != std::string::npos,
               "the first real solution is reported");
        expect(allFunctions(r).find("cos") != std::string::npos,
               "the real solutions use cosine");
    }
    {
        Result r = solve("system_complex_eigen", {{"matrix", "0, -1; 1, 0"}});
        expect(r.ok, "the pure rotation is analysed");
        expect(allFunctions(r).find("sin") != std::string::npos,
               "the pure rotation uses sine");
    }
    {
        Result r = solve("system_complex_eigen", {{"matrix", "1, 0; 0, 2"}});
        expect(!r.ok && r.errorCode == "no_complex_eigenvalues",
               "a real spectrum is refused by this tool");
    }
}

static void testFundamental() {
    {
        Result r = solve("system_fundamental", {{"matrix", "0, 1; -1, 0"}});
        expect(r.ok, "the rotation matrix has a fundamental matrix");
        expect(allChecksOk(r), "Phi(0) = I and Phi' = A Phi hold");
        expect(allFunctions(r).find("cos") != std::string::npos,
               "the rotation matrix produces cosine entries");
        expect(r.wronskian != nullptr, "Abel's formula is reported");
    }
    {
        Result r = solve("system_fundamental", {{"matrix", "2, 0; 0, 5"}});
        expect(r.ok, "a diagonal matrix has a fundamental matrix");
        expect(allChecksOk(r), "the diagonal fundamental matrix checks pass");
    }
}

static void testRepeated() {
    {
        Result r = solve("system_repeated", {{"matrix", "3, 1; 0, 3"}});
        expect(r.ok, "a defective repeated eigenvalue is analysed");
        expect(allChecksOk(r), "the defective system checks pass");
        expect(allFunctions(r).find("t") != std::string::npos,
               "the defective solution contains a t factor");
    }
    {
        Result r = solve("system_repeated", {{"matrix", "3, 0; 0, 3"}});
        expect(r.ok, "a diagonal repeated eigenvalue is analysed");
        expect(allChecksOk(r), "the diagonal repeated system checks pass");
    }
    {
        Result r = solve("system_repeated", {{"matrix", "1, 0; 0, 2"}});
        expect(!r.ok && r.errorCode == "no_repeated_eigenvalue",
               "a simple spectrum is refused by this tool");
    }
}

static void testNonhomogeneous() {
    {
        Result r = solve("system_nonhomogeneous",
                         {{"matrix", "1, 0; 0, 2"}, {"f", "1, 0"}});
        expect(r.ok, "a constant forcing is solved");
        expect(allChecksOk(r), "the particular solution checks pass");
        expect(r.closedForm, "a polynomial forcing has a closed form");
        expect(allFunctions(r).find("x_p1") != std::string::npos,
               "the particular solution is reported");
    }
    {
        Result r = solve("system_nonhomogeneous",
                         {{"matrix", "1, 0; 0, 2"}, {"f", "exp(t), 0"}});
        expect(r.ok, "a resonant exponential forcing is solved");
        expect(allFunctions(r).find("x_p1") != std::string::npos,
               "the resonant particular solution is reported");
    }
}

static void testSystemRk4() {
    {
        Result r = solve("system_rk4",
                         {{"f", "x"}, {"g", "y"}, {"method", "rk4"}},
                         {{"t0", 0.0}, {"T", 1.0}, {"h", 0.05},
                          {"x0", 1.0}, {"y0", 1.0}});
        expect(r.ok, "the decoupled system integrates");
        expect(allChecksOk(r), "the RK4 checks pass");
        expect(!r.closedForm, "the numerical answer is not labelled exact");
        const double x = evalValue(r.value);
        expect(std::fabs(x - std::exp(1.0)) < 1e-6, "x(1) matches e");
    }
    {
        Result r = solve("system_rk4",
                         {{"f", "x - 0.5*x*y"}, {"g", "-y + 0.1*x*y"},
                          {"method", "euler"}},
                         {{"t0", 0.0}, {"T", 2.0}, {"h", 0.02},
                          {"x0", 2.0}, {"y0", 1.0}});
        expect(r.ok, "the predator-prey example integrates with Euler");
        expect(r.method == "euler", "the method is reported");
        expect(!r.closedForm, "Euler output is numerical");
    }
}

int main() {
    testSystemTheory();
    testComplexEigen();
    testFundamental();
    testRepeated();
    testNonhomogeneous();
    testSystemRk4();
    if (failures == 0) {
        std::cout << "all linear tests passed\n";
        return 0;
    }
    std::cout << failures << " linear test(s) failed\n";
    return 1;
}
