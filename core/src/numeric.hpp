// Infinit Lab — numeric certificates (RPN compiled in C++, evaluated in C)
#pragma once
#include "expr.hpp"
#include "solver.hpp"
#include <map>
#include <string>

namespace ode {

double evalReal(const Ex& e, const std::map<std::string, double>& vars, bool* ok = nullptr);
bool numericallyZero(const Ex& e, const std::vector<std::string>& syms, int samples = 8, double tol = 1e-7);
bool numericallyEqual(const Ex& a, const Ex& b, const std::vector<std::string>& syms, int samples = 8,
                      double tol = 1e-7);
Ex numericSolutionCheck(const Ex& F, const Ex& yExpr, const std::string& x, const std::string& y,
                        const Check& placeholder = {});

// A structured domain failure: a division by zero, a pole of tan/sec/cot/csc,
// a non-finite sample or runaway growth on [lo, hi].
struct DomainIssue {
    bool found = false;
    std::string code;      // singular
    double point = 0.0;
    std::string detail;    // technical text for the log
};

// Sample f densely on [lo, hi], also locating denominator zeros exactly enough
// to name them.  Returns true and fills `issue` when f is not defined/finite.
bool checkDomain(const Ex& f, const std::string& var,
                 const std::map<std::string, double>& fixed, double lo, double hi,
                 DomainIssue* issue, int samples = 4001);

} // namespace ode
