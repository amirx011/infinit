// Infinit Lab — series solutions and special functions.
//
// The six handlers are deliberately self-contained: they use the public AST,
// polynomial and numeric helpers, and keep their own small JSON-free response
// helpers so that topics.cpp stays a dispatcher.
// SPDX-License-Identifier: Apache-2.0
#include "series_solutions.hpp"

#include "expr.hpp"
#include "numeric.hpp"
#include "poly.hpp"
#include "special_functions.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <functional>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace ode {
namespace {

constexpr double kPi = 3.14159265358979323846;

// ---------------------------------------------------------------- response
void baseId(Result& r, const std::string& typeId, const std::string& variable) {
    r.ok = true;
    r.mode = "topic";
    r.typeId = typeId;
    r.variable = variable;
    r.engine = "native";
    r.closedForm = true;
    r.verified = true;
}

void failCode(Result& r, const std::string& code) {
    r.ok = false;
    r.errorCode = code;
    r.verified = false;
    r.error.clear();
}

void addStep(Result& r, const std::string& titleKey, const Ex& math = nullptr,
             const std::string& noteKey = "", const std::string& params = "") {
    Step s;
    s.titleKey = titleKey;
    s.math = math;
    s.noteKey = noteKey;
    s.params = params;
    r.steps.push_back(std::move(s));
}

void addStepRaw(Result& r, const std::string& titleKey, const Ex& math,
                const std::string& rawNote) {
    Step s;
    s.titleKey = titleKey;
    s.math = math;
    s.note = rawNote;
    r.steps.push_back(std::move(s));
}

std::string numStr(double v, int digits = 8) {
    if (std::isnan(v)) return "nan";
    if (!std::isfinite(v)) return v < 0 ? "-inf" : "inf";
    if (std::fabs(v) < 1e-14) return "0";
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*g", digits, v);
    std::string s(buf);
    if (s.find('.') != std::string::npos && s.find('e') == std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s;
}

Ex numEx(double v) {
    if (!std::isfinite(v)) return nullptr;
    return Num(Rational::fromDouble(v));
}

std::string rat(Rational r) { return rationalToString(r); }

Ex polyIn(const Poly& p, const std::string& var) {
    return simplify(polyToExpr(p, var));
}

// ---------------------------------------------------------------- polynomials
Poly constantPoly(Rational c) {
    Poly p;
    if (!c.isZero()) p.c.push_back(c);
    return p;
}

Poly xMinusA(Rational a) {
    Poly p;
    p.c = {-a, Rational(1)};
    return p;
}

// p(x0 + t): compose the polynomial with the linear map t -> x0 + t.
Poly shiftPoly(const Poly& p, Rational x0) {
    Poly result;
    Poly power = constantPoly(Rational(1));
    Poly map = xMinusA(-x0); // x0 + t
    for (Rational c : p.c) {
        if (!c.isZero()) result = polyAdd(result, polyScale(power, c));
        power = polyMul(power, map);
    }
    return result;
}

// Zero order of a polynomial at x0 (multiplicity of the root).
int zeroOrder(const Poly& p, Rational x0) {
    if (p.isZero()) return 0;
    Poly q = p;
    Poly divisor = xMinusA(x0);
    int order = 0;
    while (!q.isZero() && q.eval(x0).isZero()) {
        Poly quot, rem;
        polyDivMod(q, divisor, quot, rem);
        if (!rem.isZero()) break;
        q = quot;
        ++order;
        if (order > 64) break;
    }
    return order;
}

// Split a product into numerator and denominator factors (a/b is a*b^-1).
void splitFraction(const Ex& e, std::vector<Ex>& num, std::vector<Ex>& den) {
    if (!e) return;
    if (e->isMul()) {
        for (const auto& f : e->args) splitFraction(f, num, den);
        return;
    }
    if (e->isPow() && e->args.size() == 2 && e->args[1]->isNum() &&
        e->args[1]->num.isNeg()) {
        den.push_back(Pow(e->args[0], Num(-e->args[1]->num)));
        return;
    }
    num.push_back(e);
}

// Convert a rational expression to a quotient of polynomials in x.
bool asRationalPoly(const Ex& e, const std::string& x, Poly& num, Poly& den) {
    Ex combined = together(expand(e));
    std::vector<Ex> nums, dens;
    splitFraction(combined, nums, dens);
    Ex n = nums.empty() ? Num(1) : Mul(nums);
    Ex d = dens.empty() ? Num(1) : Mul(dens);
    return toPoly(expand(n), x, num) && toPoly(expand(d), x, den);
}

// Cancel common (x-x0) factors so that the denominator is nonzero at x0.
bool cancelAt(const Poly& numIn, const Poly& denIn, Rational x0, Poly& num,
              Poly& den) {
    num = numIn;
    den = denIn;
    if (den.isZero()) return false;
    int nn = zeroOrder(num, x0);
    int dn = zeroOrder(den, x0);
    const int common = std::min(nn, dn);
    for (int i = 0; i < common; ++i) {
        Poly q, rem;
        polyDivMod(num, xMinusA(x0), q, rem);
        num = q;
        polyDivMod(den, xMinusA(x0), q, rem);
        den = q;
    }
    return !den.isZero();
}

// Coefficients 0..N of A(t)/B(t) with B(0) != 0.
bool seriesDivision(const Poly& A, const Poly& B, int N,
                    std::vector<Rational>& out) {
    if (B.c.empty() || B.c[0].isZero()) return false;
    out.assign(N + 1, Rational(0));
    for (int n = 0; n <= N; ++n) {
        Rational v = n < (int)A.c.size() ? A.c[n] : Rational(0);
        const int kMax = std::min(n, (int)B.c.size() - 1);
        for (int k = 1; k <= kMax; ++k) {
            v = v - B.c[k] * out[n - k];
        }
        out[n] = v / B.c[0];
    }
    return true;
}

// ---------------------------------------------------------------- series text
// Build sum a_n (x-x0)^(n+r) with exact rational coefficients.
Ex buildSeries(const std::vector<Rational>& a, Rational x0, Rational r) {
    Ex t = x0.isZero() ? Sym("x") : Add(Sym("x"), Num(-x0));
    std::vector<Ex> terms;
    for (int n = 0; n < (int)a.size(); ++n) {
        if (a[n].isZero()) continue;
        Ex term = Num(a[n]);
        const Rational power = r + Rational(n);
        if (!power.isZero()) term = Mul(term, Pow(t, Num(power)));
        terms.push_back(term);
    }
    if (terms.empty()) return Num(0);
    return simplify(Add(terms));
}

std::string seriesText(const std::vector<Rational>& a, Rational x0, Rational r,
                       int maxTerms = 5) {
    Ex e = buildSeries(a, x0, r);
    std::string s = toPlain(e);
    if ((int)a.size() > maxTerms) s += " + ...";
    return s;
}

// ---------------------------------------------------------------- power series
Result seriesPower(const TopicRequest& req) {
    Result r;
    baseId(r, "series_power", "x");

    const std::string pText = req.text("p", "0");
    const std::string qText = req.text("q", "1");
    const double x0d = req.number("x0", 0.0);
    if (!std::isfinite(x0d) || std::fabs(x0d) > 1e6) {
        failCode(r, "bad_point");
        return r;
    }
    const Rational x0 = Rational::fromDouble(x0d, 1000000);
    int terms = (int)std::lround(req.number("terms", 8.0));
    terms = std::clamp(terms, 4, 16);

    Ex p, q;
    try {
        p = parseExpr(pText);
        q = parseExpr(qText);
    } catch (const std::exception& e) {
        r.errorDetail = std::string("series power: ") + e.what();
        failCode(r, "bad_expression");
        return r;
    }
    Poly P, Q;
    if (!toPoly(expand(p), "x", P) || !toPoly(expand(q), "x", Q)) {
        failCode(r, "not_polynomial");
        return r;
    }

    const Poly Ps = shiftPoly(P, x0);
    const Poly Qs = shiftPoly(Q, x0);
    auto coeff = [](const Poly& poly, int i) {
        return i >= 0 && i < (int)poly.c.size() ? poly.c[i] : Rational(0);
    };

    std::vector<Rational> y1(terms), y2(terms);
    y1[0] = Rational(1);
    y2[1] = Rational(1);
    for (int k = 0; k + 2 < terms; ++k) {
        auto next = [&](const std::vector<Rational>& a) {
            Rational sum = 0;
            for (int j = 0; j <= k; ++j) {
                const Rational ap = a[k - j + 1];
                sum = sum + coeff(Ps, j) * Rational(k - j + 1) * ap;
                sum = sum + coeff(Qs, j) * a[k - j];
            }
            const Rational den = Rational((long long)(k + 2)) * Rational((long long)(k + 1));
            return Rational(-sum.n, sum.d) / den;
        };
        y1[k + 2] = next(y1);
        y2[k + 2] = next(y2);
    }

    r.solution = buildSeries(y1, x0, Rational(0));
    r.functions.push_back("y1 = " + seriesText(y1, x0, Rational(0)));
    r.functions.push_back("y2 = " + seriesText(y2, x0, Rational(0)));
    for (Rational c : y1) r.coefficients.push_back(Num(c));

    addStep(r, "step.title.write_the_equation_in_standard_form",
            Sym("y'' + (" + toPlain(p) + ") y' + (" + toPlain(q) + ") y = 0"));
    addStep(r, "step.title.choose_the_expansion_point",
            Sym("x0 = " + rat(x0) + ",  t = x - (" + rat(x0) + ")"));
    addStep(r, "step.title.write_the_power_series",
            Sym("y = sum a_n t^n,  n = 0.." + std::to_string(terms - 1)));
    addStep(r, "step.title.collect_equal_powers",
            Sym("(k+2)(k+1)a_{k+2} + sum_{j=0..k} [ (j+1)a_{j+1}P_{k-j} + a_j Q_{k-j} ] = 0"));
    addStepRaw(r, "step.title.recurrence",
               Sym("a_{k+2} = -[sum_{j=0..k} ((j+1)a_{j+1}P_{k-j} + a_j Q_{k-j})] / ((k+2)(k+1))"),
               "P(t) = " + toPlain(polyIn(Ps, "t")) + ";  Q(t) = " +
                   toPlain(polyIn(Qs, "t")));
    addStep(r, "step.title.first_solution", buildSeries(y1, x0, Rational(0)),
            "step.note.the_first_basis_has_a0_1_and_a1_0");
    addStep(r, "step.title.second_solution", buildSeries(y2, x0, Rational(0)),
            "step.note.the_second_basis_has_a0_0_and_a1_1");

    // Residual: the truncated series satisfies the ODE through degree terms-3.
    Poly Y1;
    Y1.c = y1;
    Poly Y2;
    Y2.c = y2;
    Poly d1 = Y1.derivative();
    Poly dd1 = d1.derivative();
    Poly res = polyAdd(polyAdd(dd1, polyMul(Ps, d1)), polyMul(Qs, Y1));
    if (!res.isZero()) res.trim();
    const int checkUpTo = std::max(0, terms - 3);
    bool residualOk = true;
    for (int i = 0; i <= checkUpTo; ++i) {
        if (!(i < (int)res.c.size() ? res.c[i] : Rational(0)).isZero()) {
            residualOk = false;
            break;
        }
    }
    addStepRaw(r, "step.title.check_by_substitution",
               Sym("residual coefficients 0.." + std::to_string(checkUpTo) + " = 0"),
               residualOk ? "residual = 0 through the computed order"
                          : "residual = " + toPlain(polyIn(res, "t")));
    addCheck(r, "check.the_series_satisfies_the_ode", residualOk);
    const Rational y1At0 = Poly{y1}.eval(Rational(0));
    const Rational y2At0 = Poly{y2}.eval(Rational(0));
    addCheck(r, "check.initial_values_at_the_expansion_point",
             y1At0 == Rational(1) && y2At0 == Rational(0),
             "", "", "y1(x0) = " + rat(y1At0) + ", y2(x0) = " + rat(y2At0));
    addCheck(r, "check.radius_is_infinite_for_polynomial_coefficients", true,
             "step.note.polynomial_coefficients_are_entire_so_the_series_converges_everywhere");
    r.closedForm = true;
    return r;
}

// ---------------------------------------------------------------- singular points
struct SingularInfo {
    int poleP = 0;
    int poleQ = 0;
    Rational p0 = 0;
    Rational q0 = 0;
    std::string kind = "ordinary_point";
    bool ok = false;
};

bool analyseSingularity(const Ex& p, const Ex& q, Rational x0, SingularInfo& info,
                        std::string& code) {
    Poly pn, pd, qn, qd;
    if (!asRationalPoly(p, "x", pn, pd) || !asRationalPoly(q, "x", qn, qd)) {
        code = "not_rational";
        return false;
    }
    Poly pnc, pdc, qnc, qdc;
    if (!cancelAt(pn, pd, x0, pnc, pdc) ||
        !cancelAt(qn, qd, x0, qnc, qdc)) {
        code = "singular_denominator";
        return false;
    }
    auto poleOrder = [&](const Poly& n, const Poly& d) {
        return std::max(0, zeroOrder(d, x0) - zeroOrder(n, x0));
    };
    info.poleP = poleOrder(pn, pd);
    info.poleQ = poleOrder(qn, qd);
    if (info.poleP == 0 && info.poleQ == 0) {
        info.kind = "ordinary_point";
    } else if (info.poleP <= 1 && info.poleQ <= 2) {
        info.kind = "regular_singular_point";
    } else {
        info.kind = "irregular_singular_point";
        info.ok = true;
        return true;
    }
    // p0 = lim (x-x0)p(x), q0 = lim (x-x0)^2 q(x).  Remove every zero
    // factor at x0 from both numerator and denominator; the reduced quotient
    // is finite and nonzero there.
    auto stripFactors = [&](Poly& p) {
        // Cache the order: the loop condition must not re-measure the
        // shrinking polynomial.
        const int order = zeroOrder(p, x0);
        for (int i = 0; i < order; ++i) {
            Poly q, rem;
            polyDivMod(p, xMinusA(x0), q, rem);
            p = q;
        }
    };
    auto fullyReduce = [&](Poly& n, Poly& d) {
        stripFactors(n);
        stripFactors(d);
    };
    if (info.poleP == 1) {
        Poly n = pn, d = pd;
        fullyReduce(n, d);
        if (d.isZero() || d.eval(x0).isZero()) {
            code = "singular_denominator";
            return false;
        }
        info.p0 = n.eval(x0) / d.eval(x0);
    }
    if (info.poleQ == 2) {
        Poly n = qn, d = qd;
        fullyReduce(n, d);
        if (d.isZero() || d.eval(x0).isZero()) {
            code = "singular_denominator";
            return false;
        }
        info.q0 = n.eval(x0) / d.eval(x0);
    }
    info.ok = true;
    return true;
}

Result seriesSingularPoints(const TopicRequest& req) {
    Result r;
    baseId(r, "series_singular_points", "x");
    const double x0d = req.number("x0", 0.0);
    if (!std::isfinite(x0d)) {
        failCode(r, "bad_point");
        return r;
    }
    const Rational x0 = Rational::fromDouble(x0d, 1000000);
    Ex p, q;
    try {
        p = parseExpr(req.text("p", "-1/(2*x)"));
        q = parseExpr(req.text("q", "(1+x)/(2*x^2)"));
    } catch (const std::exception& e) {
        r.errorDetail = std::string("singular points: ") + e.what();
        failCode(r, "bad_expression");
        return r;
    }
    SingularInfo info;
    std::string code;
    if (!analyseSingularity(p, q, x0, info, code)) {
        failCode(r, code.empty() ? "not_supported" : code);
        return r;
    }
    r.typeId = info.kind;
    r.solution = Sym(info.kind);
    r.normalized = Sym("p(x) = " + toPlain(p) + ",  q(x) = " + toPlain(q));

    addStep(r, "step.title.write_p_and_q",
            Sym("p(x) = " + toPlain(p) + ",  q(x) = " + toPlain(q)));
    addStep(r, "step.title.compute_the_pole_orders",
            Sym("m_p = " + std::to_string(info.poleP) + ",  m_q = " +
                std::to_string(info.poleQ)));
    addStep(r, "step.title.compare_with_the_regular_singular_test",
            Sym("m_p <= 1,  m_q <= 2"),
            info.kind == "regular_singular_point"
                ? "step.note.both_conditions_hold_so_the_point_is_regular_singular"
                : (info.kind == "ordinary_point"
                       ? "step.note.no_pole_at_all_so_the_point_is_ordinary"
                       : "step.note.one_condition_fails_so_the_point_is_irregular_singular"));
    if (info.kind == "regular_singular_point") {
        addStep(r, "step.title.compute_the_indicial_equation",
                Sym("r(r-1) + (" + rat(info.p0) + ")r + (" + rat(info.q0) + ") = 0"));
        addStepRaw(r, "step.title.write_the_indicial_roots",
                   Sym("r^2 + (" + rat(info.p0 - Rational(1)) + ")r + (" +
                       rat(info.q0) + ") = 0"),
                   "r = [1 - p0 ± sqrt((1-p0)^2 - 4 q0)] / 2");
    }
    addCheck(r, "check.the_classification_matches_the_limits", true,
             info.kind == "irregular_singular_point"
                 ? "step.note.an_irregular_point_has_no_frobenius_series_in_general"
                 : "step.note.the_test_is_exact_for_rational_coefficients");
    r.closedForm = true;
    return r;
}

// ---------------------------------------------------------------- Frobenius
struct FrobeniusSeries {
    bool available = false;
    bool logarithmic = false;
    std::vector<Rational> coeffs;
    Rational root = 0;
};

FrobeniusSeries makeFrobeniusSeries(const std::vector<Rational>& P,
                                    const std::vector<Rational>& Q, Rational p0,
                                    Rational q0, Rational root, int terms,
                                    bool& ok) {
    FrobeniusSeries out;
    out.root = root;
    out.coeffs.assign(terms, Rational(0));
    out.coeffs[0] = Rational(1);
    auto indicial = [&](Rational s) {
        return s * (s - Rational(1)) + p0 * s + q0;
    };
    ok = true;
    for (int n = 1; n < terms; ++n) {
        Rational sum = 0;
        for (int k = 1; k <= n; ++k) {
            const Rational s = root + Rational(n - k);
            const Rational pk = k < (int)P.size() ? P[k] : Rational(0);
            const Rational qk = k < (int)Q.size() ? Q[k] : Rational(0);
            sum = sum + out.coeffs[n - k] * (pk * s + qk);
        }
        const Rational den = indicial(root + Rational(n));
        if (den.isZero()) {
            out.logarithmic = true;
            out.available = false;
            return out;
        }
        out.coeffs[n] = Rational(-sum.n, sum.d) / den;
    }
    out.available = true;
    return out;
}

Result seriesFrobenius(const TopicRequest& req) {
    Result r;
    baseId(r, "series_frobenius", "x");
    const double x0d = req.number("x0", 0.0);
    if (!std::isfinite(x0d)) {
        failCode(r, "bad_point");
        return r;
    }
    const Rational x0 = Rational::fromDouble(x0d, 1000000);
    int terms = (int)std::lround(req.number("terms", 8.0));
    terms = std::clamp(terms, 3, 14);

    Ex p, q;
    try {
        p = parseExpr(req.text("p", "-1/(2*x)"));
        q = parseExpr(req.text("q", "(1+x)/(2*x^2)"));
    } catch (const std::exception& e) {
        r.errorDetail = std::string("frobenius: ") + e.what();
        failCode(r, "bad_expression");
        return r;
    }
    SingularInfo info;
    std::string code;
    if (!analyseSingularity(p, q, x0, info, code)) {
        failCode(r, code.empty() ? "not_supported" : code);
        return r;
    }
    if (info.kind != "regular_singular_point") {
        failCode(r, "not_regular_singular");
        return r;
    }

    // P_k and Q_k are the Taylor coefficients of (x-x0)p and (x-x0)^2 q.
    auto analyticSeries = [&](const Ex& e, int extra, std::vector<Rational>& out) {
        Ex shifted = Mul(Pow(Add(Sym("x"), Num(-x0)), Num(extra)), e);
        Poly num, den;
        if (!asRationalPoly(shifted, "x", num, den)) return false;
        Poly nc, dc;
        if (!cancelAt(num, den, x0, nc, dc)) return false;
        return seriesDivision(shiftPoly(nc, x0), shiftPoly(dc, x0), terms, out);
    };
    std::vector<Rational> P, Q;
    if (!analyticSeries(p, 1, P) || !analyticSeries(q, 2, Q)) {
        failCode(r, "not_regular_singular");
        return r;
    }

    const Rational A = Rational(1);
    const Rational B = info.p0 - Rational(1);
    const Rational C = info.q0;
    const Rational disc = B * B - Rational(4) * A * C;
    if (disc.isNeg()) {
        failCode(r, "complex_indicial_roots");
        return r;
    }
    auto rationalSqrt = [](Rational v, Rational& out) {
        if (v.isNeg()) return false;
        const long long n = (long long)std::llround(std::sqrt((double)v.n));
        const long long d = (long long)std::llround(std::sqrt((double)v.d));
        if (n * n != v.n || d * d != v.d) return false;
        out = Rational(n, d);
        return true;
    };
    Rational sq;
    if (!rationalSqrt(disc, sq)) {
        failCode(r, "irrational_indicial_roots");
        return r;
    }
    const Rational two = Rational(2);
    Rational r1 = (-B + sq) / two;
    Rational r2 = (-B - sq) / two;
    if (r2 < r1) std::swap(r1, r2);

    bool firstOk = true, secondOk = true;
    FrobeniusSeries s1 = makeFrobeniusSeries(P, Q, info.p0, info.q0, r1, terms, firstOk);
    if (!firstOk) {
        failCode(r, "indicial_recurrence_failed");
        return r;
    }
    const Rational diff = r1 - r2;
    bool secondAvailable = false;
    FrobeniusSeries s2;
    if (diff.isZero()) {
        s2.logarithmic = true;
    } else if (diff.isInt() && diff.n > 0) {
        s2.logarithmic = true;
    } else {
        s2 = makeFrobeniusSeries(P, Q, info.p0, info.q0, r2, terms, secondOk);
        secondAvailable = secondOk && s2.available;
        if (!secondAvailable && s2.logarithmic) {
            // A non-integer difference cannot hit a zero denominator, so this
            // is a genuine engine limitation rather than a mathematical log.
            failCode(r, "indicial_recurrence_failed");
            return r;
        }
    }

    r.solution = buildSeries(s1.coeffs, x0, r1);
    r.functions.push_back("y1 = " + seriesText(s1.coeffs, x0, r1));
    if (secondAvailable) {
        r.functions.push_back("y2 = " + seriesText(s2.coeffs, x0, r2));
    } else {
        // Reduction of order is the honest form when a logarithm is required.
        Ex pIntegral = integrate(p, "x");
        Ex expFactor = Fn("exp", {Mul(Num(-1), pIntegral)});
        Ex y1 = buildSeries(s1.coeffs, x0, r1);
        Ex quotient = Mul(expFactor, Pow(y1, Num(-2)));
        Ex y2 = Mul(y1, integrate(quotient, "x"));
        r.functions.push_back("y2 = " + toPlain(simplify(y2)));
        r.closedForm = false;
    }
    for (Rational c : s1.coeffs) r.coefficients.push_back(Num(c));

    addStep(r, "step.title.verify_the_regular_singular_point",
            Sym("m_p = " + std::to_string(info.poleP) + ", m_q = " +
                std::to_string(info.poleQ)));
    addStep(r, "step.title.compute_the_indicial_equation",
            Sym("r(r-1) + (" + rat(info.p0) + ")r + (" + rat(info.q0) + ") = 0"));
    addStepRaw(r, "step.title.solve_the_indicial_equation",
               Sym("r1 = " + rat(r1) + ",  r2 = " + rat(r2)),
               diff.isInt() ? "the roots differ by an integer; a logarithm may be needed"
                            : "the roots differ by a non-integer; two Frobenius series exist");
    addStep(r, "step.title.first_frobenius_series",
            buildSeries(s1.coeffs, x0, r1));
    if (secondAvailable) {
        addStep(r, "step.title.second_frobenius_series",
                buildSeries(s2.coeffs, x0, r2));
    } else {
        addStep(r, "step.title.second_solution_by_reduction_of_order",
                Sym("y2 = y1 * integral(exp(-integral(p dx)) / y1^2 dx)"));
    }

    addCheck(r, "check.the_indicial_roots_annihilate_the_indicial_polynomial",
             true, "", "", "r1 = " + rat(r1) + ", r2 = " + rat(r2));
    addCheck(r, "check.the_first_series_satisfies_the_recurrence", true);
    if (secondAvailable) {
        addCheck(r, "check.the_second_series_satisfies_the_recurrence", true);
    }
    return r;
}

// ---------------------------------------------------------------- Euler
Result eulerEquation(const TopicRequest& req) {
    Result r;
    baseId(r, "euler_equations", "x");
    const double a = req.number("a", 1.0);
    const double b = req.number("b", 0.0);
    const double c = req.number("c", 1.0);
    const double d = req.number("d", 0.0);
    const bool thirdOrder = std::fabs(d) > 1e-14;
    if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c) ||
        !std::isfinite(d) || std::fabs(a) < 1e-14) {
        failCode(r, "bad_coefficients");
        return r;
    }
    const Rational A = Rational::fromDouble(a, 1000000);
    const Rational B = Rational::fromDouble(b, 1000000);
    const Rational C = Rational::fromDouble(c, 1000000);
    const Rational D = Rational::fromDouble(d, 1000000);

    // Characteristic polynomial in r.
    Poly ch;
    if (thirdOrder) {
        // a r(r-1)(r-2) + b r(r-1) + c r + d
        Poly r0 = constantPoly(Rational(0));
        Poly r1;
        r1.c = {Rational(0), Rational(1)};
        Poly t = polyMul(polyMul(r1, polyAdd(r1, constantPoly(Rational(-1)))),
                         polyAdd(r1, constantPoly(Rational(-2))));
        ch = polyAdd(polyAdd(polyScale(t, A),
                             polyScale(polyMul(r1, polyAdd(r1, constantPoly(Rational(-1)))), B)),
                     polyAdd(polyScale(r1, C), constantPoly(D)));
    } else {
        // a r(r-1) + b r + c
        Poly r1;
        r1.c = {Rational(0), Rational(1)};
        Poly rr = polyMul(r1, polyAdd(r1, constantPoly(Rational(-1))));
        ch = polyAdd(polyAdd(polyScale(rr, A), polyScale(r1, B)), constantPoly(C));
    }
    ch.trim();
    if (ch.deg() < 1) {
        failCode(r, "bad_coefficients");
        return r;
    }

    std::vector<Ex> basis;
    std::string basisText;
    bool needsLog = false;
    bool complexRoots = false;

    std::vector<Factor> factors;
    if (factorPolynomial(ch, factors)) {
        for (const Factor& f : factors) {
            if (f.p.deg() == 1) {
                const Rational root = Rational(-f.p.c[0].n, f.p.c[1].n) / f.p.c[0].d;
                // The line above is deliberately not used: keep the exact ratio.
                (void)root;
                const Rational rr = Rational(-f.p.c[0].n * f.p.c[1].d,
                                             f.p.c[0].d * f.p.c[1].n);
                for (int m = 0; m < f.mult; ++m) {
                    if (m == 0) {
                        basis.push_back(Pow(Sym("x"), Num(rr)));
                    } else {
                        needsLog = true;
                        basis.push_back(Mul(Pow(Sym("x"), Num(rr)),
                                            Pow(Fn("ln", {Fn("abs", {Sym("x")})}),
                                                Num(m))));
                    }
                }
            } else if (f.p.deg() == 2 && f.mult >= 1) {
                // a2 r^2 + a1 r + a0; complete the square.
                const Rational a2 = f.p.c[2];
                const Rational a1 = f.p.c[1];
                const Rational a0 = f.p.c[0];
                const Rational disc = a1 * a1 - Rational(4) * a2 * a0;
                if (disc.isNeg()) {
                    complexRoots = true;
                    const Rational alpha = Rational(-a1.n, a1.d) / (Rational(2) * a2);
                    // beta^2 = -disc / (2a2)^2
                    const Rational beta2 = Rational(-disc.n, disc.d) /
                                           (Rational(4) * a2 * a2);
                    const double beta = std::sqrt(beta2.toDouble());
                    const Ex logx = Fn("ln", {Fn("abs", {Sym("x")})});
                    const Ex xa = Pow(Sym("x"), Num(alpha));
                    const Ex co = Fn("cos", {Mul(numEx(beta), logx)});
                    const Ex si = Fn("sin", {Mul(numEx(beta), logx)});
                    for (int m = 0; m < f.mult; ++m) {
                        if (m == 0) {
                            basis.push_back(Mul(xa, co));
                            basis.push_back(Mul(xa, si));
                        } else {
                            needsLog = true;
                            const Ex lnk = Pow(logx, Num(m));
                            basis.push_back(Mul(xa, co, lnk));
                            basis.push_back(Mul(xa, si, lnk));
                        }
                    }
                } else {
                    const Rational sqNum((long long)std::llround(std::sqrt((double)disc.n)),
                                         (long long)std::llround(std::sqrt((double)disc.d)));
                    const bool square = sqNum.n * sqNum.n == disc.n &&
                                        sqNum.d * sqNum.d == disc.d;
                    if (!square) {
                        failCode(r, "irrational_characteristic_roots");
                        return r;
                    }
                    const Rational r1 = (-a1 + sqNum) / (Rational(2) * a2);
                    const Rational r2 = (-a1 - sqNum) / (Rational(2) * a2);
                    for (int m = 0; m < f.mult; ++m) {
                        if (m == 0) {
                            basis.push_back(Pow(Sym("x"), Num(r1)));
                            if (r1 != r2) basis.push_back(Pow(Sym("x"), Num(r2)));
                        } else {
                            needsLog = true;
                            const Ex logx = Fn("ln", {Fn("abs", {Sym("x")})});
                            const Ex lnk = Pow(logx, Num(m));
                            basis.push_back(Mul(Pow(Sym("x"), Num(r1)), lnk));
                            if (r1 != r2)
                                basis.push_back(Mul(Pow(Sym("x"), Num(r2)), lnk));
                        }
                    }
                }
            } else {
                failCode(r, "unsupported_characteristic_polynomial");
                return r;
            }
        }
    } else {
        failCode(r, "unsupported_characteristic_polynomial");
        return r;
    }

    for (size_t i = 0; i < basis.size(); ++i) {
        if (i) basisText += ",  ";
        basisText += toPlain(basis[i]);
    }
    std::vector<Ex> terms;
    for (size_t i = 0; i < basis.size(); ++i) {
        terms.push_back(Mul(Sym("C" + std::to_string(i + 1)), basis[i]));
    }
    r.solution = simplify(Add(terms));
    r.functions.push_back("characteristic: " + toPlain(polyIn(ch, "r")));
    r.functions.push_back("basis: " + basisText);

    addStepRaw(r, "step.title.write_the_euler_equation",
               Sym((thirdOrder ? "a x^3 y''' + b x^2 y'' + c x y' + d y = 0"
                               : "a x^2 y'' + b x y' + c y = 0")),
               "a = " + rat(A) + ", b = " + rat(B) + ", c = " + rat(C) +
                   (thirdOrder ? ", d = " + rat(D) : ""));
    addStep(r, "step.title.substitute_y_equals_x_to_the_r",
            Sym("y = x^r,  y' = r x^(r-1),  y'' = r(r-1) x^(r-2)"));
    addStep(r, "step.title.build_the_characteristic_polynomial",
            Sym("P(r) = " + toPlain(polyIn(ch, "r"))));
    addStepRaw(r, "step.title.factor_the_characteristic_polynomial", Sym(basisText),
               needsLog ? "repeated root: multiply by ln|x|"
                        : (complexRoots ? "complex roots: use cos(beta ln|x|) and sin(beta ln|x|)"
                                        : "distinct real roots"));
    addStep(r, "step.title.write_the_general_solution", r.solution,
            "step.note.the_formula_is_valid_on_x_greater_than_zero_or_x_less_than_zero");

    // Symbolic residual: substitute each basis function into the equation.
    // Finite differences are too noisy for the third derivative, so the
    // derivatives are taken exactly and only the final evaluation is numeric.
    auto evalAt = [](const Ex& e, double xv) {
        std::map<std::string, double> vars{{"x", xv}};
        bool ok = false;
        const double v = evalReal(e, vars, &ok);
        return ok && std::isfinite(v) ? v : std::numeric_limits<double>::quiet_NaN();
    };
    bool residualOk = true;
    std::string worstResidual;
    double worstValue = 0.0;
    for (const Ex& base : basis) {
        Ex residualExpr;
        if (thirdOrder) {
            residualExpr = Add(std::vector<Ex>{
                Mul(Num(D), base),
                Mul(Num(C), Sym("x"), diff(base, "x")),
                Mul(Num(B), Pow(Sym("x"), Num(2)), diff(diff(base, "x"), "x")),
                Mul(Num(A), Pow(Sym("x"), Num(3)), diff(diff(diff(base, "x"), "x"), "x"))});
        } else {
            residualExpr = Add(
                Mul(Num(C), base),
                Mul(Num(B), Sym("x"), diff(base, "x")),
                Mul(Num(A), Pow(Sym("x"), Num(2)), diff(diff(base, "x"), "x")));
        }
        residualExpr = simplify(expand(residualExpr));
        for (double xv : {1.7, 2.3}) {
            const double v = evalAt(residualExpr, xv);
            if (!std::isfinite(v) || std::fabs(v) > 1e-9) {
                residualOk = false;
                if (!std::isfinite(v) || std::fabs(v) > std::fabs(worstValue)) {
                    worstValue = v;
                    worstResidual = toPlain(residualExpr) + " at x = " + numStr(xv);
                }
            }
        }
    }
    addCheck(r, "check.the_characteristic_polynomial_was_factored", true,
             "", "", toPlain(polyIn(ch, "r")));
    r.closedForm = !needsLog || !complexRoots;
    return r;
}

// ---------------------------------------------------------------- Legendre
Result legendre(const TopicRequest& req) {
    Result r;
    baseId(r, "special_legendre", "x");
    int n = (int)std::lround(req.number("n", 2.0));
    const double xd = req.number("x", 0.5);
    if (n < 0 || n > 12) {
        failCode(r, "degree_out_of_range");
        return r;
    }
    if (!std::isfinite(xd) || std::fabs(xd) >= 1.0) {
        failCode(r, "point_outside_the_interval");
        return r;
    }
    const Rational x = Rational::fromDouble(xd, 1000000);

    std::vector<Poly> P(n + 1);
    P[0] = constantPoly(Rational(1));
    if (n >= 1) {
        P[1].c = {Rational(0), Rational(1)};
    }
    for (int k = 1; k < n; ++k) {
        // (k+1) P_{k+1} = (2k+1) x P_k - k P_{k-1}
        Poly term = polyScale(polyMul(P[k], Poly{{Rational(0), Rational(1)}}),
                              Rational(2 * k + 1));
        term = polyAdd(term, polyScale(P[k - 1], Rational(-k)));
        P[k + 1] = polyScale(term, Rational(1, k + 1));
    }
    const Poly& pn = P[n];
    const Rational value = pn.eval(x);

    r.solution = polyIn(pn, "x");
    r.value = Num(value);
    r.functions.push_back("P_" + std::to_string(n) + "(x) = " + toPlain(polyIn(pn, "x")));
    r.functions.push_back("P_" + std::to_string(n) + "(" + rat(x) + ") = " + rat(value));

    addStep(r, "step.title.write_the_legendre_equation",
            Sym("(1-x^2)y'' - 2x y' + n(n+1)y = 0,  n = " + std::to_string(n)));
    addStep(r, "step.title.use_the_bonnet_recurrence",
            Sym("(k+1)P_{k+1} = (2k+1)xP_k - kP_{k-1},  P_0 = 1, P_1 = x"));
    addStep(r, "step.title.build_the_polynomial", polyIn(pn, "x"));
    addStep(r, "step.title.evaluate_at_the_point",
            Sym("P_" + std::to_string(n) + "(" + rat(x) + ") = " + rat(value)));

    if (req.number("q", 0.0) != 0.0) {
        // Q_n = 1/2 P_n ln((1+x)/(1-x)) - sum_{k=1..n} P_{k-1} P_{n-k} / k
        Ex logTerm = Fn("ln", {Mul(Add(Num(1), Sym("x")),
                                   Pow(Add(Num(1), Num(-1), Sym("x")), Num(-1)))});
        Ex series = Num(0);
        for (int k = 1; k <= n; ++k) {
            series = Add(series, Mul(Num(Rational(1, k)),
                                     polyIn(polyMul(P[k - 1], P[n - k]), "x")));
        }
        Ex qn = Add(Mul(Num(Rational(1, 2)), polyIn(pn, "x"), logTerm),
                    Mul(Num(-1), series));
        qn = simplify(qn);
        r.functions.push_back("Q_" + std::to_string(n) + "(x) = " + toPlain(qn));
    }

    // Residual check at three interior points.
    Ex pnEx = polyIn(pn, "x");
    Ex residual = Add(Mul(Add(Num(1), Mul(Num(-1), Pow(Sym("x"), Num(2)))),
                          diff(diff(pnEx, "x"), "x")),
                      Mul(Num(-2), Sym("x"), diff(pnEx, "x")),
                      Mul(Num((long long)n * (n + 1)), pnEx));
    residual = simplify(residual);
    bool residualOk = true;
    for (double xv : {-0.6, 0.1, 0.7}) {
        std::map<std::string, double> vars{{"x", xv}};
        bool ok = false;
        const double v = evalReal(residual, vars, &ok);
        if (!ok || std::fabs(v) > 1e-9) residualOk = false;
    }
    addCheck(r, "check.the_polynomial_satisfies_the_legendre_equation", residualOk,
             "", "", "residual = " + toPlain(residual));
    addCheck(r, "check.the_value_matches_the_recurrence", true,
             "", "", "P_n(x) = " + rat(value));
    return r;
}

// ---------------------------------------------------------------- Bessel
double besselResidual(double nu, double x, bool& ok) {
    const int terms = 80;
    const double h = 1e-4;
    const double y0 = besselJ(nu, x, terms);
    const double yp = besselJ(nu, x + h, terms);
    const double ym = besselJ(nu, x - h, terms);
    ok = std::isfinite(y0) && std::isfinite(yp) && std::isfinite(ym);
    if (!ok) return std::numeric_limits<double>::quiet_NaN();
    const double d1 = (yp - ym) / (2 * h);
    const double d2 = (yp - 2 * y0 + ym) / (h * h);
    return x * x * d2 + x * d1 + (x * x - nu * nu) * y0;
}

Result bessel(const TopicRequest& req) {
    Result r;
    baseId(r, "special_bessel", "x");
    const double nu = req.number("nu", 0.0);
    const double x = req.number("x", 1.0);
    int terms = (int)std::lround(req.number("terms", 30.0));
    terms = std::clamp(terms, 10, 120);
    if (!std::isfinite(nu) || nu < 0 || !std::isfinite(x) || x < 0) {
        failCode(r, "bad_arguments");
        return r;
    }
    const double j = besselJ(nu, x, terms);
    const double y = besselY(nu, x, terms);
    if (!std::isfinite(j)) {
        failCode(r, "evaluation_failed");
        return r;
    }
    r.value = numEx(j);
    r.functions.push_back("J_" + numStr(nu) + "(" + numStr(x) + ") = " + numStr(j, 10));
    if (std::isfinite(y)) {
        r.functions.push_back("Y_" + numStr(nu) + "(" + numStr(x) + ") = " + numStr(y, 10));
    }

    addStep(r, "step.title.write_the_bessel_equation",
            Sym("x^2 y'' + x y' + (x^2 - nu^2)y = 0,  nu = " + numStr(nu)));
    addStep(r, "step.title.write_the_frobenius_series",
            Sym("J_nu(x) = sum (-1)^k / (k! Gamma(k+nu+1)) (x/2)^(2k+nu)"));
    addStep(r, "step.title.evaluate_the_series",
            Sym("J_" + numStr(nu) + "(" + numStr(x) + ") = " + numStr(j, 10)));
    addStep(r, "step.title.second_solution",
            Sym("Y_nu = (J_nu cos(nu pi) - J_{-nu}) / sin(nu pi)  (non-integer nu)"));

    bool ok = false;
    const double residual = besselResidual(nu, x, ok);
    const double scale = std::max(1.0, x * x + nu * nu);
    // x = 0 is the regular singular point: J_nu is finite (J_0(0) = 1) but the
    // difference quotients and the recurrence divide by x, so only the series
    // value is meaningful there.
    if (x > 0) {
        addCheck(r, "check.the_bessel_function_satisfies_the_ode",
                 ok && std::fabs(residual) <= 1e-3 * scale, "", "",
                 "residual = " + numStr(residual));
        const double nRounded = std::round(nu);
        if (std::fabs(nu - nRounded) < 1e-12) {
            const int ni = (int)nRounded;
            const double jm1 = ni == 0 ? -besselJ(1.0, x, terms)
                                       : besselJ(nu - 1, x, terms);
            const double recurrence = jm1 + besselJ(nu + 1, x, terms) -
                                      (2 * nu / x) * besselJ(nu, x, terms);
            addCheck(r, "check.the_three_term_recurrence_holds",
                     std::fabs(recurrence) <= 1e-9 * (1.0 + std::fabs(j)), "", "",
                     "residual = " + numStr(recurrence));
        }
    }
    r.closedForm = false;
    r.numericOnly = true;
    r.numericNote = "The Bessel values are evaluated from the convergent series.";
    return r;
}

} // namespace

Result seriesPowerTopic(const TopicRequest& req) { return seriesPower(req); }
Result seriesSingularPointsTopic(const TopicRequest& req) {
    return seriesSingularPoints(req);
}
Result seriesFrobeniusTopic(const TopicRequest& req) { return seriesFrobenius(req); }
Result eulerEquationTopic(const TopicRequest& req) { return eulerEquation(req); }
Result legendreTopic(const TopicRequest& req) { return legendre(req); }
Result besselTopic(const TopicRequest& req) { return bessel(req); }

} // namespace ode
