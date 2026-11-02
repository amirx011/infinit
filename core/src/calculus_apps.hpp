// Infinit Lab — applications of the derivative
// SPDX-License-Identifier: Apache-2.0
//
// Every routine here answers a question about a real function f on an interval:
// where it rises, where it bends, which line touches it, which input makes it
// largest, how fast a related quantity moves, and what the mean value theorems
// guarantee. Each one also records the reasoning as Steps so the UI can show a
// derivation instead of a bare number.
#pragma once

#include "expr.hpp"
#include "solver.hpp"

#include <string>
#include <vector>

namespace ode {

// ---------------------------------------------------------------- behaviour
// Rising/falling intervals, extrema, concavity and inflection points of f.
Result analyseFunction(const std::string& input, const std::string& x, double xmin, double xmax,
                       const std::string& lang);

// --------------------------------------------------------- tangent & normal
// Tangent and normal line of y = f(x) at x = a, with the derivative and the
// line equations. `yExpr` may be given as "x^2" or as an equation solved for y.
Result tangentAndNormal(const std::string& input, const std::string& x, const std::string& at,
                        const std::string& lang);

// ------------------------------------------------------------- optimisation
// Critical points, extrema and the objective value of f on [xmin, xmax].
Result optimiseFunction(const std::string& input, const std::string& x, double xmin, double xmax,
                        const std::string& lang);

// ------------------------------------------------------------- related rates
// Differentiates a relation between two quantities with respect to time and
// solves for the requested rate at the supplied instant.
// q' = -(F_w / F_q) w'. Supply qValue, wValue and givenRate to also get a
// number; pass NaN for any of them to keep the answer symbolic.
Result relatedRates(const std::string& relation, const std::string& var, const std::string& rateVar,
                    const std::string& t, double qValue, double wValue, double givenRate,
                    const std::string& lang);

// ------------------------------------------------------------- mean value theorems
// Verifies Rolle, Lagrange (MVT) and Cauchy on [a, b] by solving for the point(s)
// the theorems promise, then checking the hypotheses explicitly.
Result meanValueTheorems(const std::string& input, const std::string& x, double a, double b,
                         const std::string& lang);

// Verifies Cauchy's generalised MVT for the pair (f, g) on [a, b].
Result cauchyMeanValue(const std::string& f, const std::string& g, const std::string& x, double a,
                       double b, const std::string& lang);

// ------------------------------------------------------------- L'Hopital
// Evaluates lim f/g at a point, showing each L'Hopital application and whether
// the quotient was actually indeterminate before differentiating.
Result lhopitalLimit(const std::string& numerator, const std::string& denominator,
                     const std::string& x, const std::string& point, const std::string& lang);

} // namespace ode
