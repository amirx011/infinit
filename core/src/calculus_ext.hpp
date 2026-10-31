// Infinit Lab — AST differentiation, implicit derivatives and quadrature
// SPDX-License-Identifier: MIT
#pragma once
#include "expr.hpp"

#include <functional>
#include <string>
#include <vector>

namespace ode {

// One entry of the derivation trace: which AST rule fired and on which subtree.
struct DerivationNote {
    std::string rule;    // "sum", "product", "chain", "power", ...
    std::string subject; // printable subtree the rule was applied to
    int depth = 0;       // recursion depth (0 = whole expression)
};

// Differentiate an expression tree node by node, recording every rule that fires.
Ex diffAst(const Ex& a, const std::string& v, std::vector<DerivationNote>* notes = nullptr,
           int depth = 0);

// Implicit derivative of F(x, y) = 0 up to order 3: dy/dx, d²y/dx², d³y/dx³.
// `first` receives dy/dx when non-null.
Ex implicitDerivativeOf(const Ex& F, const std::string& x, const std::string& y, int order,
                        Ex* first = nullptr);

// Adaptive Gauss–Kronrod (15 point) quadrature on [a, b].
double quadratureAdaptive(const std::function<double(double)>& f, double a, double b,
                          double tol = 1e-10, int maxDepth = 18);

// Nested quadrature: vars[0] is the innermost variable, vars.back() the outermost.
double numericIntegral(const Ex& f, const std::vector<std::string>& vars,
                       const std::vector<double>& lower, const std::vector<double>& upper);

} // namespace ode
