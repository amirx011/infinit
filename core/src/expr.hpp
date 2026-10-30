// Infinit Lab — exact symbolic expression core
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace ode {

struct OverflowError : std::runtime_error {
    explicit OverflowError(const std::string& s) : std::runtime_error(s) {}
};

// ---------------------------------------------------------------- Rational
struct Rational {
    long long n = 0, d = 1;

    Rational() = default;
    Rational(long long v) : n(v), d(1) {}                      // NOLINT
    Rational(long long a, long long b) : n(a), d(b) { norm(); }

    static Rational fromDouble(double v, long long maxDen = 1000000000LL) {
        // exact rational approximation (Stern–Brocot style, bounded denominator)
        if (!std::isfinite(v)) return Rational(0);
        bool neg = v < 0; v = std::fabs(v);
        long long ip = (long long)std::floor(v);
        double frac = v - (double)ip;
        long long bestN = 0, bestD = 1; double bestErr = 1e18;
        // continued fractions
        double x = frac;
        long long p0 = 0, q0 = 1, p1 = 1, q1 = 0;
        for (int i = 0; i < 40 && x > 1e-15; ++i) {
            long long a = (long long)std::floor(x);
            long long p2 = a * p1 + p0, q2 = a * q1 + q0;
            if (q2 > maxDen) break;
            p0 = p1; q0 = q1; p1 = p2; q1 = q2;
            double approx = (double)p1 / (double)q1;
            double err = std::fabs(approx - frac);
            if (err < bestErr) { bestErr = err; bestN = p1; bestD = q1; }
            if (err < 1e-14) break;
            x = 1.0 / (x - (double)a);
        }
        if (bestD == 0) { bestN = 0; bestD = 1; }
        Rational r(ip * bestD + bestN, bestD);
        return neg ? Rational(-r.n, r.d) : r;
    }

    void norm() {
        if (d == 0) throw std::runtime_error("division by zero");
        if (d < 0) { n = -n; d = -d; }
        long long g = std::gcd(n < 0 ? -n : n, d);
        if (g > 1) { n /= g; d /= g; }
    }
    double toDouble() const { return (double)n / (double)d; }
    bool isZero() const { return n == 0; }
    bool isOne() const { return n == 1 && d == 1; }
    bool isInt() const { return d == 1; }
    bool isNeg() const { return n < 0; }
    Rational abs() const { return Rational(n < 0 ? -n : n, d); }
    Rational inv() const {
        if (n == 0) throw std::runtime_error("division by zero");
        return Rational(d, n);
    }
    explicit operator double() const { return toDouble(); }
};

inline Rational operator+(Rational a, Rational b) {
    __int128 n = (__int128)a.n * b.d + (__int128)b.n * a.d;
    __int128 d = (__int128)a.d * b.d;
    if (n > INT64_MAX || n < INT64_MIN || d > INT64_MAX) throw OverflowError("integer coefficient too large");
    return Rational((long long)n, (long long)d);
}
inline Rational operator-(Rational a, Rational b) {
    __int128 n = (__int128)a.n * b.d - (__int128)b.n * a.d;
    __int128 d = (__int128)a.d * b.d;
    if (n > INT64_MAX || n < INT64_MIN || d > INT64_MAX) throw OverflowError("integer coefficient too large");
    return Rational((long long)n, (long long)d);
}
inline Rational operator-(Rational a) { return Rational(-a.n, a.d); }
inline Rational operator*(Rational a, Rational b) {
    __int128 n = (__int128)a.n * b.n;
    __int128 d = (__int128)a.d * b.d;
    if (n > INT64_MAX || n < INT64_MIN || d > INT64_MAX) throw OverflowError("integer coefficient too large");
    return Rational((long long)n, (long long)d);
}
inline Rational operator/(Rational a, Rational b) {
    if (b.n == 0) throw std::runtime_error("division by zero");
    return a * b.inv();
}
inline bool operator==(Rational a, Rational b) { return a.n == b.n && a.d == b.d; }
inline bool operator!=(Rational a, Rational b) { return !(a == b); }
inline bool operator<(Rational a, Rational b) {
    return (__int128)a.n * b.d < (__int128)b.n * a.d;
}

std::string rationalToString(Rational r); // "3", "-1/2"

// ---------------------------------------------------------------- Expr
enum class Kind { Num, Sym, Add, Mul, Pow, Func };

struct Expr;
using Ex = std::shared_ptr<const Expr>;

struct Expr {
    Kind kind = Kind::Num;
    Rational num;
    std::string name;      // Sym name / Func name
    std::vector<Ex> args;  // Add, Mul, Pow [base, exp], Func args

    bool isNum() const { return kind == Kind::Num; }
    bool isNum(long long v) const { return kind == Kind::Num && num.n == v && num.d == 1; }
    bool isSym() const { return kind == Kind::Sym; }
    bool isSym(const std::string& s) const { return kind == Kind::Sym && name == s; }
    bool isAdd() const { return kind == Kind::Add; }
    bool isMul() const { return kind == Kind::Mul; }
    bool isPow() const { return kind == Kind::Pow; }
    bool isFunc() const { return kind == Kind::Func; }
    bool isFunc(const std::string& s) const { return kind == Kind::Func && name == s; }
    bool isZero() const { return isNum() && num.isZero(); }
    bool isOne() const { return isNum() && num.isOne(); }
    bool isMinusOne() const { return isNum() && num.n == -1 && num.d == 1; }
    bool isInt() const { return isNum() && num.isInt(); }
    Ex arg(size_t i) const { return args.at(i); }
};

// constructors (canonicalising)
Ex Num(long long v);
Ex Num(Rational r);
Ex Sym(const std::string& s);
Ex Add(const std::vector<Ex>& terms);
Ex Add(Ex a, Ex b);
inline Ex Add(Ex a, Ex b, Ex c) { return Add(std::vector<Ex>{a, b, c}); }
Ex Mul(const std::vector<Ex>& factors);
Ex Mul(Ex a, Ex b);
inline Ex Mul(Ex a, Ex b, Ex c) { return Mul(std::vector<Ex>{a, b, c}); }
inline Ex Mul(Ex a, Ex b, Ex c, Ex d) { return Mul(std::vector<Ex>{a, b, c, d}); }
Ex Pow(Ex base, Ex exp);
Ex Fn(const std::string& name, const std::vector<Ex>& args);
Ex Sqrt(Ex a);
Ex Int(long long v); // integer literal

// helpers
bool equals(const Ex& a, const Ex& b);
size_t hashOf(const Ex& a);
int compareExpr(const Ex& a, const Ex& b);
bool dependsOn(const Ex& a, const std::string& var);
bool dependsOnAny(const Ex& a, const std::vector<std::string>& vars);
void collectSymbols(const Ex& a, std::vector<std::string>& out);
bool isConstWrt(const Ex& a, const std::string& var);

// structural transformations
Ex substitute(const Ex& a, const std::string& var, const Ex& rep);
Ex substituteSubexpr(const Ex& a, const Ex& target, const Ex& rep);
Ex substituteMany(const Ex& a, const std::map<std::string, Ex>& m);
Ex diff(const Ex& a, const std::string& var);
Ex simplify(const Ex& a);   // canonical + light cleanup, idempotent
Ex expand(const Ex& a);
Ex together(const Ex& a);   // combine sums of fractions over one denominator
Ex cancelCommon(const Ex& a);
Ex factorOut(const Ex& a, const Ex& g);

// integration (exact; may contain an Integral node when no closed form is found)
Ex integrate(const Ex& f, const std::string& var, bool* closed = nullptr,
             std::string* method = nullptr);
bool hasUnevaluatedIntegral(const Ex& a);
bool polyCoeffs(const Ex& a, const std::string& var, std::vector<Ex>& coeffs);

// reasoning
Ex solveLinear(const Ex& a, const std::string& var); // solve a=0 if linear in var
std::vector<Ex> solvePoly(const Ex& a, const std::string& var, int maxDeg = 4); // rational/quadratic roots
Ex limitAt(const Ex& a, const std::string& var, const Ex& point);

// printing
std::string toPlain(const Ex& a);
std::string toPretty(const Ex& a);
std::string toLatex(const Ex& a);
std::string toText(const Ex& a); // pretty with ascii fallback

// parsing
Ex parseExpr(const std::string& s);
// parse an equation; returns lhs-rhs
Ex parseEquation(const std::string& s, std::string* lhsOut = nullptr, std::string* rhsOut = nullptr);
// derivative-aware parse (y', y'', dy/dx); independent variable
Ex parseODEExpr(const std::string& s, const std::string& indep = "x", std::string* depOut = nullptr,
                const std::vector<std::string>& implicitVars = {});

// derivative node helpers
bool isDeriv(const Ex& a);
Ex Deriv(const Ex& f, const std::string& x, int order);
Ex makeDeriv(const Ex& f, const std::string& x, int order); // canonical, folds order 0
int derivOrder(const Ex& a);

// misc
std::string trim(const std::string& s);
std::string replaceAll(std::string s, const std::string& from, const std::string& to);

} // namespace ode
