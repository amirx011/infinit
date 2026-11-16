// Nonlinear systems and chaotic flows (topics 81, 82, 83, 84, 112).
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

static void testCompetingSpecies() {
    {
        Result r = solve("competing_species",
                         {{"a", 1.0}, {"b", 0.3}, {"c", 0.2},
                          {"d", 0.8}, {"e", 0.4}, {"f", 0.3}});
        expect(r.ok, "the default competing-species system is analysed");
        expect(allChecksOk(r), "the equilibrium checks pass");
        const std::string all = allFunctions(r);
        expect(all.find("(0, 0)") != std::string::npos, "the origin is reported");
        expect(all.find("E") != std::string::npos, "the equilibria are numbered");
    }
    {
        // Nonsingular competition: interior equilibrium at (1, 2).
        Result r = solve("competing_species",
                         {{"a", 4.0}, {"b", 2.0}, {"c", 1.0},
                          {"d", 3.0}, {"e", 1.0}, {"f", 1.0}});
        expect(r.ok, "a nonsingular competition is analysed");
        expect(allFunctions(r).find("(1, 2)") != std::string::npos,
               "the interior equilibrium is (1, 2)");
    }
}

static void testLiapunov() {
    {
        Result r = solve("liapunov", {{"x0", 0.0}, {"y0", 0.0}},
                         {{"f", "-x"}, {"g", "-y"}});
        expect(r.ok, "the linear stable node is analysed");
        expect(allChecksOk(r), "the Lyapunov checks pass");
        expect(allFunctions(r).find("asymptotically stable") != std::string::npos,
               "the conclusion is asymptotic stability");
        expect(!r.closedForm, "the sign test is labelled as numeric evidence");
    }
    {
        Result r = solve("liapunov", {{"x0", 0.0}, {"y0", 0.0}},
                         {{"f", "x"}, {"g", "y"}});
        expect(r.ok, "the linear unstable node is analysed");
        expect(allFunctions(r).find("inconclusive") != std::string::npos ||
                   allFunctions(r).find("not positive definite") != std::string::npos,
               "an unstable linearization is reported honestly");
    }
    {
        // Explicit candidate for the same stable node.
        Result r = solve("liapunov", {{"x0", 0.0}, {"y0", 0.0}},
                         {{"f", "-x"}, {"g", "-y"}, {"V", "x^2 + y^2"}});
        expect(r.ok, "a user-supplied candidate is analysed");
        expect(allFunctions(r).find("V'(x,y)") != std::string::npos,
               "V' is reported");
    }
}

static void testLimitCycle() {
    {
        Result r = solve("limit_cycle",
                         {{"x0", 2.0}, {"y0", 0.0}, {"tmax", 40.0}, {"h", 0.005}},
                         {{"f", "y"}, {"g", "(1 - x^2)*y - x"}});
        expect(r.ok, "the Van der Pol limit cycle is detected");
        expect(allChecksOk(r), "the limit-cycle checks pass");
        expect(!r.closedForm && r.numericOnly, "the orbit is numerical evidence");
        expect(allFunctions(r).find("not a proof") != std::string::npos ||
                   allFunctions(r).find("Numerical evidence") != std::string::npos,
               "the honesty note is present");
        const double period = evalReal(r.value, {}, nullptr);
        expect(std::fabs(period - 6.663) < 1.0, "the Van der Pol period is about 6.66");
    }
    {
        // A stable focus has no transversal returns.
        Result r = solve("limit_cycle",
                         {{"x0", 1.0}, {"y0", 1.0}, {"tmax", 5.0}, {"h", 0.01}},
                         {{"f", "-x"}, {"g", "-y"}});
        expect(!r.ok && r.errorCode == "no_closed_orbit_detected",
               "a focus reports no closed orbit");
    }
}

static void testChaos() {
    {
        Result r = solve("chaos_attractor",
                         {{"x0", 1.0}, {"y0", 1.0}, {"z0", 1.0},
                          {"tmax", 10.0}, {"h", 0.01}});
        expect(r.ok, "the Lorenz system integrates");
        expect(allChecksOk(r), "the Lorenz checks pass");
        expect(!r.closedForm && r.numericOnly, "the Lorenz output is numerical");
        const double lambda = evalReal(r.value, {}, nullptr);
        expect(std::isfinite(lambda), "the finite-time exponent is finite");
        expect(allFunctions(r).find("Finite-time") != std::string::npos,
               "the finite-time warning is present");
    }
    {
        Result r = solve("chaos_attractor",
                         {{"x0", 1.0}, {"y0", 1.0}, {"z0", 0.0},
                          {"tmax", 8.0}, {"h", 0.01}},
                         {{"system", "rossler"}});
        expect(r.ok, "the Rossler system integrates");
        expect(allChecksOk(r), "the Rossler checks pass");
    }
}

static void testLocalLinearization() {
    {
        Result r = solve("local_linearization",
                         {{"xmin", -1.0}, {"xmax", 2.0},
                          {"ymin", -1.0}, {"ymax", 2.0}},
                         {{"f", "x - x^2 - x*y"}, {"g", "y*x - 0.5*y"}});
        expect(r.ok, "the default planar field is analysed");
        expect(allChecksOk(r), "the equilibrium checks pass");
        expect(allFunctions(r).find("J =") != std::string::npos,
               "the Jacobian is reported at each point");
    }
    {
        // Linear field: one hyperbolic saddle at the origin.
        Result r = solve("local_linearization",
                         {{"xmin", -2.0}, {"xmax", 2.0},
                          {"ymin", -2.0}, {"ymax", 2.0}},
                         {{"f", "x"}, {"g", "-y"}});
        expect(r.ok, "the linear saddle is analysed");
        expect(allFunctions(r).find("saddle") != std::string::npos,
               "the saddle is classified");
    }
}

int main() {
    testCompetingSpecies();
    testLiapunov();
    testLimitCycle();
    testChaos();
    testLocalLinearization();
    if (failures == 0) {
        std::cout << "all dynamical tests passed\n";
        return 0;
    }
    std::cout << failures << " dynamical test(s) failed\n";
    return 1;
}
