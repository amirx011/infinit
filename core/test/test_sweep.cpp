// Data-driven regression sweep.
//
// Every row is one independent ctest case (`odetest_sweep <index>`), so a
// failure names the exact input that broke instead of a whole section.  The
// table covers all 69 curriculum topics with their catalogue defaults, a
// second parameter set for most of them, the first- and second-order ODE
// families, derivatives, integrals, limits, Wronskians and orthogonal
// trajectories.
//
// The assertions are semantic: the engine must succeed, its own substitution
// checks must all pass, and it must actually produce the object the section is
// about.  Output formatting is asserted by the dedicated per-section suites,
// not here, so a cosmetic change never turns 250 tests red.
#include "expr.hpp"
#include "solver.hpp"
#include "topics.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace ode;

struct SweepCase {
    char kind;          // T topic, O ode, D derivative, I integral, L limit,
                        // W wronskian, G orthogonal trajectories
    const char* a;
    const char* b;
    const char* name;
};

static const SweepCase kCases[] = {
#include "sweep_cases.inc"
};

static const int kCaseCount = (int)(sizeof(kCases) / sizeof(kCases[0]));

static bool allChecksOk(const Result& r) {
    for (const auto& c : r.checks)
        if (!c.ok) return false;
    return true;
}

static std::vector<std::string> split(const std::string& s, char d) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == d) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

static Result runTopic(const std::string& spec) {
    const auto parts = split(spec, '|');
    TopicRequest req;
    req.topic = parts.empty() ? "" : parts[0];
    req.lang = "en";
    for (size_t i = 1; i < parts.size(); ++i) {
        const auto eq = parts[i].find('=');
        if (eq == std::string::npos) continue;
        const std::string key = parts[i].substr(0, eq);
        const std::string value = parts[i].substr(eq + 1);
        try {
            size_t used = 0;
            const double v = std::stod(value, &used);
            if (used == value.size()) {
                req.nums[key] = v;
                continue;
            }
        } catch (...) {
        }
        req.strs[key] = value;
    }
    return solveTopic(req);
}

static Result runCase(const SweepCase& c) {
    switch (c.kind) {
        case 'T':
            return runTopic(c.a);
        case 'O': {
            OdeOptions opt;
            opt.lang = "en";
            return solveODE(c.a, opt);
        }
        case 'D':
            return derivativeExpression(c.a, "x", 1, "en");
        case 'I':
            return integrateExpression(c.a, "x", "en");
        case 'L':
            return limitExpression(c.a, "x", c.b, "both", "en");
        case 'W':
            return solveWronskian(split(c.a, ','), "x", "en");
        case 'G':
            return solveOrthogonal(c.a, "c", "x", "y", "en");
        default:
            return Result();
    }
}

// The object each section must produce when it succeeds.
static bool produced(const SweepCase& c, const Result& r) {
    switch (c.kind) {
        case 'T':
            return r.solution != nullptr || !r.functions.empty() ||
                   r.wronskian != nullptr;
        case 'W':
            return r.wronskian != nullptr;
        default:
            return r.solution != nullptr;
    }
}

static int checkOne(const SweepCase& c, int index) {
    const Result r = runCase(c);
    std::string why;
    if (!r.ok)
        why = "engine reported failure " + r.errorCode;
    else if (!allChecksOk(r))
        why = "a substitution check failed";
    else if (!produced(c, r))
        why = "no answer object was produced";
    else if ((c.kind == 'O') && !r.verified)
        why = "the solution was not verified";
    else if ((c.kind == 'T' || c.kind == 'O') && r.steps.empty())
        why = "the solution path is empty";

    if (!why.empty()) {
        std::cout << "FAIL #" << index << " [" << c.kind << "] " << c.name
                  << "  (" << why << ")\n";
        if (!r.errorDetail.empty()) std::cout << "  detail: " << r.errorDetail << "\n";
        return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    int only = 0;
    if (argc > 1) only = std::atoi(argv[1]);

    int failures = 0;
    if (only > 0) {
        if (only > kCaseCount) {
            std::cout << "FAIL: case index " << only << " is out of range\n";
            return 1;
        }
        failures += checkOne(kCases[only - 1], only);
    } else {
        for (int i = 0; i < kCaseCount; ++i) failures += checkOne(kCases[i], i + 1);
    }

    if (failures == 0) {
        if (only > 0) std::cout << "PASS #" << only << " " << kCases[only - 1].name << "\n";
        else std::cout << "ALL " << kCaseCount << " SWEEP CASES PASSED\n";
    }
    return failures == 0 ? 0 : 1;
}
