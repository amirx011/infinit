// Infinit Lab — limits and continuity on the expression tree
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "expr.hpp"

#include <string>
#include <vector>

namespace ode {

// Outcome of a limit computation. `Finite` carries an exact expression; the
// infinite kinds describe divergence; `Undefined` means the limit does not
// exist (different one-sided values, oscillation, or a failed analysis).
struct LimitOutcome {
    enum Kind { Finite, PosInf, NegInf, Undefined } kind = Undefined;
    Ex value;              // valid when kind == Finite
    bool numeric = false;  // value came from a numerical estimate
    bool approximated = false; // numeric estimate snapped to a small rational
    double number = 0.0;   // numeric estimate (valid when numeric)
    std::string reason;    // short explanation, English
};

// dir: -1 = left, +1 = right, 0 = two-sided (both are computed and compared).
// `point` may be an ordinary expression or the symbolic infinity Sym("inf") /
// Mul(-1, Sym("inf")).
LimitOutcome limitOf(const Ex& f, const std::string& v, const Ex& point, int dir,
                     std::vector<std::string>* rules = nullptr);

struct ContinuityOutcome {
    LimitOutcome left;
    LimitOutcome right;
    bool valueDefined = false; // f(point) exists and is finite
    Ex value;                  // f(point) when defined
    bool continuous = false;
    // "continuous", "removable", "jump", "infinite", "oscillatory", "domain"
    std::string kind = "oscillatory";
    std::string note;
};

ContinuityOutcome continuityOf(const Ex& f, const std::string& v, const Ex& point,
                               std::vector<std::string>* rules = nullptr);

// True when `point` denotes +inf (sign +1) or -inf (sign -1).
bool isInfinityPoint(const Ex& point, int* sign = nullptr);
Ex infinityPoint(int sign = 1);

} // namespace ode
