// ODE Lab — numeric certificates (RPN compiled in C++, evaluated in C)
#pragma once
#include "expr.hpp"
#include "solver.hpp"
#include <map>

namespace ode {

double evalReal(const Ex& e, const std::map<std::string, double>& vars, bool* ok = nullptr);
bool numericallyZero(const Ex& e, const std::vector<std::string>& syms, int samples = 8, double tol = 1e-7);
bool numericallyEqual(const Ex& a, const Ex& b, const std::vector<std::string>& syms, int samples = 8,
                      double tol = 1e-7);
Ex numericSolutionCheck(const Ex& F, const Ex& yExpr, const std::string& x, const std::string& y,
                        const Check& placeholder = {});

} // namespace ode
