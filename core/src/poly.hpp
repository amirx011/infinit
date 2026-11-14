// Infinit Lab — univariate polynomials over Q, factoring, partial fractions
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "expr.hpp"

namespace ode {

struct Poly {
    std::vector<Rational> c; // low -> high

    int deg() const { return (int)c.size() - 1; }
    bool isZero() const { return c.empty(); }
    Rational lead() const { return c.empty() ? Rational(0) : c.back(); }

    void trim() {
        while (!c.empty() && c.back().isZero()) c.pop_back();
    }
    Rational eval(Rational x) const {
        Rational r(0);
        for (auto it = c.rbegin(); it != c.rend(); ++it) r = r * x + *it;
        return r;
    }
    Poly derivative() const {
        Poly d;
        if (c.size() <= 1) return d;
        d.c.resize(c.size() - 1);
        for (size_t i = 1; i < c.size(); ++i) d.c[i - 1] = c[i] * Rational((long long)i);
        return d;
    }
    long long contentDen() const {
        long long L = 1;
        for (auto& x : c) L = std::lcm(L, x.d);
        return L;
    }
    Poly monic() const {
        Poly p = *this;
        if (p.c.empty()) return p;
        Rational l = p.lead();
        for (auto& x : p.c) x = x / l;
        return p;
    }
};

Poly polyAdd(const Poly& a, const Poly& b);
Poly polyMul(const Poly& a, const Poly& b);
Poly polySub(const Poly& a, const Poly& b);
Poly polyScale(const Poly& a, Rational k);
void polyDivMod(const Poly& a, const Poly& b, Poly& q, Poly& r);
Poly polyGcd(Poly a, Poly b);
Poly polyPow(const Poly& a, int n);

struct Factor { Poly p; int mult = 1; };
// factor a rational polynomial into linear/quadratic factors over Q; false if impossible
bool factorPolynomial(const Poly& p, std::vector<Factor>& out);
bool toPoly(const Ex& e, const std::string& x, Poly& out);
Ex polyToExpr(const Poly& p, const std::string& x);
Ex integratePolynomial(const Poly& p, const std::string& x);
// integrate a rational function n/d exactly (or return nullptr)
Ex integrateRational(const Ex& num, const Ex& den, const std::string& x);
// Inverse Laplace transform of N(s)/D(s) by exact partial fractions over Q.
// Handles proper rational functions whose denominator factors into linear and
// irreducible quadratic factors (repeated linear roots included).  Returns
// nullptr when the form is outside that class.
Ex inverseLaplaceRational(const Ex& num, const Ex& den, const std::string& s,
                          const std::string& t);

} // namespace ode
