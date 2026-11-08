// Infinit Lab — exact and numeric pole detection
// SPDX-License-Identifier: Apache-2.0
//
// A pole is a point where the function grows without bound.  The symbolic pass
// finds exact candidates from denominators and from the zeros of cos/sin inside
// tan/sec/cot/csc, so a result can print `x = 1` or `x = pi/2` instead of a
// grid approximation.  The numeric pass refines any pole the symbolic pass
// missed.  Both the integral and the behaviour modules use this list.
#pragma once

#include "expr.hpp"

#include <string>
#include <vector>

namespace ode {

struct PolePoint {
    Ex at;                 // exact point when available
    double numericAt = 0;  // always usable
    bool exact = false;
};

// Poles of f in [lo, hi].  `scanNumeric` adds a bisection search for poles that
// the symbolic pass cannot recognise; it is slower and can only return a
// numeric location.
std::vector<PolePoint> findPoles(const Ex& f, const std::string& x, double lo, double hi,
                                 bool scanNumeric);

// True when f has a finite limit at `at` from the given side (dir = -1 left,
// +1 right).  This separates a removable hole such as sin(x)/x at 0 from a
// genuine pole.
bool hasFiniteLimitAt(const Ex& f, const std::string& x, const Ex& at, int dir);

} // namespace ode
