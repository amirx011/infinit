// Fourier convergence, even/odd splits and heat/wave derivations (86-89, 113).
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

static void testFourierConvergence() {
    {
        Result r = solve("fourier_convergence",
                         {{"L", 3.141592653589793}, {"N", 40.0}},
                         {{"f", "x"}});
        expect(r.ok, "the sawtooth Fourier series is analysed");
        expect(allChecksOk(r), "the convergence checks pass");
        expect(!r.closedForm && r.numericOnly,
               "the convergence evidence is labelled numerical");
        const std::string all = allFunctions(r);
        expect(all.find("odd") != std::string::npos,
               "the odd parity is detected");
        expect(all.find("Gibbs") != std::string::npos,
               "the Gibbs overshoot is reported");
        expect(std::fabs(valueOf(r.value)) < 1e-9,
               "the partial sum at 0 is 0");
    }
    {
        Result r = solve("fourier_convergence",
                         {{"L", 3.141592653589793}, {"N", 20.0}},
                         {{"f", "x^2"}});
        expect(r.ok, "the even Fourier series is analysed");
        expect(allChecksOk(r), "the even checks pass");
        expect(allFunctions(r).find("even") != std::string::npos,
               "the even parity is detected");
    }
    {
        Result r = solve("fourier_convergence",
                         {{"L", 3.0}, {"N", 10.0}},
                         {{"f", "x"}});
        expect(r.ok, "a different half-period is accepted");
        expect(allChecksOk(r), "the short-period checks pass");
    }
}

static void testFourierEvenOdd() {
    {
        Result r = solve("fourier_even_odd",
                         {{"L", 3.141592653589793}, {"N", 12.0}},
                         {{"f", "x^2"}});
        expect(r.ok, "the even/odd split of x^2 is computed");
        expect(allChecksOk(r), "the even/odd checks pass");
        expect(allFunctions(r).find("even") != std::string::npos,
               "x^2 is reported as even");
    }
    {
        Result r = solve("fourier_even_odd",
                         {{"L", 3.141592653589793}, {"N", 12.0}},
                         {{"f", "x"}});
        expect(r.ok, "the even/odd split of x is computed");
        expect(allChecksOk(r), "the odd checks pass");
        expect(allFunctions(r).find("odd") != std::string::npos,
               "x is reported as odd");
    }
}

static void testHeatDerivation() {
    {
        Result r = solve("heat_derivation", {{"N", 10.0}});
        expect(r.ok, "the heat derivation runs with the default data");
        expect(allChecksOk(r), "the heat checks pass");
        expect(!r.closedForm && r.numericOnly,
               "the heat series is labelled numerical");
        // u(pi/2, t1) = exp(-0.1 pi^2) for f = sin(x).
        expect(std::fabs(valueOf(r.value) - std::exp(-0.1 * 9.8696044)) < 1e-6,
               "the midpoint value follows the first mode");
        expect(allFunctions(r).find("steady state") != std::string::npos,
               "the steady state is reported");
    }
    {
        Result r = solve("heat_derivation", {{"N", 20.0}},
                         {{"f", "1"}});
        expect(r.ok, "a constant initial temperature is accepted");
        expect(allChecksOk(r), "the constant-data checks pass");
        expect(allFunctions(r).find("boundary") != std::string::npos,
               "the boundary data are reported");
    }
}

static void testWaveDerivation() {
    {
        Result r = solve("wave_derivation", {{"N", 10.0}});
        expect(r.ok, "the wave derivation runs with the default data");
        expect(allChecksOk(r), "the wave checks pass");
        expect(std::fabs(valueOf(r.value)) < 1e-9,
               "the quarter-period midpoint value is 0");
        expect(allFunctions(r).find("energy") != std::string::npos,
               "the energy is reported");
    }
    {
        Result r = solve("wave_derivation", {{"N", 20.0}},
                         {{"f", "0"}, {"g", "sin(pi*x/L)"}});
        expect(r.ok, "an initial velocity is handled");
        expect(allChecksOk(r), "the velocity-data checks pass");
        // u = sin(t) sin(x), at x = pi/2 and t = pi/2 the value is 1.
        expect(std::fabs(valueOf(r.value) - 1.0) < 1e-6,
               "the pure velocity mode reaches 1 at the quarter period");
    }
    {
        Result r = solve("wave_derivation", {{"N", 10.0}},
                         {{"domain", "infinite"}});
        expect(r.ok, "the d'Alembert form is available");
        expect(allChecksOk(r), "the d'Alembert checks pass");
        expect(allFunctions(r).find("d'Alembert") != std::string::npos,
               "the d'Alembert formula is reported");
    }
}

static void testHeatOtherBc() {
    {
        Result r = solve("heat_other_bc", {{"N", 10.0}});
        expect(r.ok, "the insulated problem is solved");
        expect(allChecksOk(r), "the insulated checks pass");
        const std::string all = allFunctions(r);
        expect(all.find("mean value") != std::string::npos,
               "the insulated steady state is the mean");
        expect(all.find("lambda_n") != std::string::npos,
               "the eigenvalues are tabulated");
    }
    {
        Result r = solve("heat_other_bc",
                         {{"N", 10.0}, {"x0", 1.5707963267948966}},
                         {{"bc", "dirichlet"}, {"f", "sin(pi*x/L)"}});
        expect(r.ok, "the zero-end problem is solved");
        expect(allChecksOk(r), "the zero-end checks pass");
        expect(std::fabs(valueOf(r.value)) < 1e-9 ||
                   std::fabs(valueOf(r.value) - std::exp(-0.1 * 9.8696044)) < 1e-6,
               "the zero-end solution matches the decaying mode");
    }
    {
        Result r = solve("heat_other_bc", {{"N", 12.0}},
                         {{"bc", "mixed"}, {"f", "1"}});
        expect(r.ok, "the mixed boundary problem is solved");
        expect(allChecksOk(r), "the mixed checks pass");
    }
    {
        Result r = solve("heat_other_bc", {{"N", 10.0}, {"h", 1.0}},
                         {{"bc", "robin"}, {"f", "cos(pi*x/L)"}});
        expect(r.ok, "the Robin problem is solved");
        expect(allChecksOk(r), "the Robin checks pass");
        // First root of mu tan(mu L) = 1 with L = pi is about 0.8603.
        expect(allFunctions(r).find("0.147") != std::string::npos,
               "the first Robin eigenvalue is about 0.147");
    }
    {
        Result r = solve("heat_other_bc",
                         {{"N", 10.0}, {"u0", 1.0}, {"u1", 0.0}},
                         {{"bc", "fixed_nonzero"}, {"f", "1"}});
        expect(r.ok, "the nonzero fixed-end problem is solved");
        expect(allChecksOk(r), "the nonzero fixed-end checks pass");
        expect(allFunctions(r).find("linear profile") != std::string::npos,
               "the linear steady state is reported");
    }
    {
        Result r = solve("heat_other_bc", {{"N", 8.0}}, {{"bc", "unknown"}});
        expect(!r.ok && r.errorCode == "unknown_bc",
               "an unknown boundary condition is refused");
    }
}

int main() {
    testFourierConvergence();
    testFourierEvenOdd();
    testHeatDerivation();
    testWaveDerivation();
    testHeatOtherBc();
    if (failures == 0) {
        std::cout << "all pde series tests passed\n";
        return 0;
    }
    std::cout << failures << " pde series test(s) failed\n";
    return 1;
}
