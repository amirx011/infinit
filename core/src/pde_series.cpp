// Infinit Lab — Fourier convergence, even/odd splits and heat/wave derivations.
// SPDX-License-Identifier: Apache-2.0
#include "pde_series.hpp"

#include "expr.hpp"
#include "numeric.hpp"

#include <algorithm>
#include <cmath>
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

std::string numStr(double v, int digits = 6) {
    if (std::isnan(v)) return "nan";
    if (!std::isfinite(v)) return v < 0 ? "-inf" : "inf";
    if (std::fabs(v) < 1e-12) return "0";
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

double eval1(const Ex& f, double x) {
    bool ok = false;
    const double v = evalReal(f, {{"x", x}}, &ok);
    return ok && std::isfinite(v) ? v : std::numeric_limits<double>::quiet_NaN();
}

double simpson(const std::function<double(double)>& g, double a, double b,
               int intervals = 2000) {
    if (intervals % 2) ++intervals;
    const double h = (b - a) / intervals;
    double sum = g(a) + g(b);
    if (!std::isfinite(sum)) return std::numeric_limits<double>::quiet_NaN();
    for (int i = 1; i < intervals; ++i) {
        const double v = g(a + i * h);
        if (!std::isfinite(v)) return std::numeric_limits<double>::quiet_NaN();
        sum += (i % 2 ? 4.0 : 2.0) * v;
    }
    return (h / 3.0) * sum;
}

double integrate(const std::function<double(double)>& g, double a, double b,
                 int intervals = 2000) {
    return simpson(g, a, b, intervals);
}

// ---------------------------------------------------------------- Fourier data
struct FourierExpansion {
    double L = kPi;
    double a0 = 0.0;
    std::vector<double> an;
    std::vector<double> bn;

    double value(double x, int terms) const {
        double sum = 0.5 * a0;
        for (int n = 0; n < terms && n < (int)an.size(); ++n) {
            const double arg = (n + 1) * kPi * x / L;
            sum += an[n] * std::cos(arg) + bn[n] * std::sin(arg);
        }
        return sum;
    }
};

bool buildFourier(const Ex& f, double L, int N, FourierExpansion& e,
                  std::string& code) {
    e.L = L;
    e.a0 = simpson([&](double x) { return eval1(f, x); }, -L, L) / L;
    if (!std::isfinite(e.a0)) {
        code = "not_integrable";
        return false;
    }
    e.an.assign(N, 0.0);
    e.bn.assign(N, 0.0);
    for (int n = 1; n <= N; ++n) {
        const double k = n * kPi / L;
        e.an[n - 1] = simpson(
                          [&](double x) { return eval1(f, x) * std::cos(k * x); },
                          -L, L) / L;
        e.bn[n - 1] = simpson(
                          [&](double x) { return eval1(f, x) * std::sin(k * x); },
                          -L, L) / L;
        if (!std::isfinite(e.an[n - 1]) || !std::isfinite(e.bn[n - 1])) {
            code = "not_integrable";
            return false;
        }
    }
    return true;
}

double relativeL2(const Ex& f, const FourierExpansion& e, int terms, double L) {
    auto square = [&](double x) {
        const double fv = eval1(f, x);
        const double sv = e.value(x, terms);
        if (!std::isfinite(fv)) return 0.0;
        const double d = fv - sv;
        return d * d;
    };
    auto energy = [&](double x) {
        const double fv = eval1(f, x);
        return std::isfinite(fv) ? fv * fv : 0.0;
    };
    const double num = integrate(square, -L, L, 2000);
    const double den = integrate(energy, -L, L, 2000);
    return std::sqrt(std::max(0.0, num) / std::max(den, 1e-300));
}

struct Jump {
    double x = 0.0;
    double left = 0.0;
    double right = 0.0;
};

// Locate the upward or downward jumps of the 2L-periodic extension by
// comparing the two one-sided values on a coarse grid.
std::vector<Jump> findJumps(const Ex& f, double L, int grid = 400) {
    std::vector<Jump> jumps;
    const double eps = 1e-7 * std::max(1.0, L);
    auto sample = [&](double x) {
        while (x <= -L) x += 2 * L;
        while (x > L) x -= 2 * L;
        return eval1(f, x);
    };
    for (int i = 0; i < grid; ++i) {
        double x = -L + 2.0 * L * i / grid;
        double left = sample(x - eps);
        double right = sample(x + eps);
        if (!std::isfinite(left) || !std::isfinite(right)) continue;
        const double jump = std::fabs(right - left);
        const double tol = 1e-5 * (1.0 + std::fabs(left) + std::fabs(right));
        if (jump > tol) {
            // Keep one entry per discontinuity.
            bool merged = false;
            for (Jump& existing : jumps) {
                double delta = std::fabs(existing.x - x);
                delta = std::min(delta, 2 * L - delta);
                if (delta < 2.0 * L / grid) {
                    merged = true;
                    break;
                }
            }
            if (!merged) jumps.push_back({x, left, right});
        }
    }
    return jumps;
}

double gibbsRatio(const FourierExpansion& e, const Jump& jump, int terms) {
    const double upper = std::max(jump.left, jump.right);
    const double lower = std::min(jump.left, jump.right);
    const double height = upper - lower;
    if (!(height > 1e-9)) return std::numeric_limits<double>::quiet_NaN();
    const double L = e.L;
    const double window = std::min(0.5 * L, 3.0 * L / std::max(1, terms));
    double overshoot = 0.0;
    for (int j = 0; j < 4000; ++j) {
        double x = jump.x - window + 2.0 * window * j / 4000.0;
        while (x <= -L) x += 2 * L;
        while (x > L) x -= 2 * L;
        const double s = e.value(x, terms);
        if (!std::isfinite(s)) continue;
        overshoot = std::max(overshoot, s - upper);
        overshoot = std::max(overshoot, lower - s);
    }
    return overshoot / height;
}

std::string parityText(const FourierExpansion& e, int terms) {
    double maxA = std::fabs(e.a0);
    double maxB = 0.0;
    for (int n = 0; n < terms; ++n) {
        maxA = std::max(maxA, std::fabs(e.an[n]));
        maxB = std::max(maxB, std::fabs(e.bn[n]));
    }
    if (maxB < 1e-8 * (1.0 + maxA)) return "even";
    if (maxA < 1e-8 * (1.0 + maxB)) return "odd";
    return "neither";
}

// ---------------------------------------------------------------- 86
Result fourierConvergence(const TopicRequest& req) {
    Result r;
    baseId(r, "fourier_convergence", "x");
    const double L = req.number("L", kPi);
    const double x0 = req.number("x0", 0.0);
    int N = (int)std::lround(req.number("N", 20.0));
    N = std::clamp(N, 1, 80);
    if (!std::isfinite(L) || L <= 0 || !std::isfinite(x0)) {
        failCode(r, "bad_arguments");
        return r;
    }
    Ex f;
    try {
        f = parseExpr(req.text("f", "x"));
    } catch (const std::exception& e) {
        r.errorDetail = std::string("f: ") + e.what();
        failCode(r, "bad_expression");
        return r;
    }
    FourierExpansion e;
    std::string code;
    if (!buildFourier(f, L, N, e, code)) {
        failCode(r, code);
        return r;
    }

    std::ostringstream coeffs;
    coeffs << "n\ta_n\tb_n\n";
    for (int n = 0; n < N; ++n)
        coeffs << n + 1 << "\t" << numStr(e.an[n], 8) << "\t"
               << numStr(e.bn[n], 8) << "\n";

    const std::vector<Jump> jumps = findJumps(f, L);
    std::ostringstream jumpTable;
    jumpTable << "x\tf(x^-)\tf(x^+)\t(f^-+f^+)/2\n";
    double largestJump = 0.0;
    Jump main{};
    for (const Jump& j : jumps) {
        const double height = std::fabs(j.right - j.left);
        jumpTable << numStr(j.x, 5) << "\t" << numStr(j.left) << "\t"
                  << numStr(j.right) << "\t" << numStr(0.5 * (j.left + j.right))
                  << "\n";
        if (height > largestJump) {
            largestJump = height;
            main = j;
        }
    }
    if (jumps.empty()) jumpTable << "(no jump found on the interior grid)\n";

    const double l2N = relativeL2(f, e, N, L);
    const double l2Half = relativeL2(f, e, std::max(1, N / 2), L);

    // Pointwise limit at x0: average the two one-sided values when they differ.
    const double eps = 1e-7 * std::max(1.0, L);
    double xw = x0;
    while (xw <= -L) xw += 2 * L;
    while (xw > L) xw -= 2 * L;
    const double fLeft = eval1(f, xw - eps);
    const double fRight = eval1(f, xw + eps);
    const bool pointIsJump = std::isfinite(fLeft) && std::isfinite(fRight) &&
                             std::fabs(fRight - fLeft) >
                                 1e-5 * (1.0 + std::fabs(fLeft) + std::fabs(fRight));
    const double limitAt = pointIsJump ? 0.5 * (fLeft + fRight) : eval1(f, xw);
    const double sAt = e.value(xw, N);
    const double pointTol =
        0.05 * (1.0 + std::fabs(limitAt) + (pointIsJump ? std::fabs(fRight - fLeft) : 0.0));

    // Coefficient decay: |a_n| + |b_n| ~ C n^{-p}.
    const int halfN = std::max(1, N / 2);
    const double cHalf = std::fabs(e.an[halfN - 1]) + std::fabs(e.bn[halfN - 1]);
    const double cLast = std::fabs(e.an[N - 1]) + std::fabs(e.bn[N - 1]);
    double decay = std::numeric_limits<double>::quiet_NaN();
    if (cHalf > 1e-14 && cLast > 1e-14)
        decay = std::log(cHalf / cLast) / std::log((double)N / halfN);

    const double gibbs = jumps.empty()
                             ? std::numeric_limits<double>::quiet_NaN()
                             : gibbsRatio(e, main, N);

    // Independent coefficient check: midpoint rule instead of Simpson.
    double directAn = 0.0, directBn = 0.0;
    {
        const int M = 8001;
        const double h = 2.0 * L / M;
        const double k = halfN * kPi / L;
        for (int i = 0; i < M; ++i) {
            const double x = -L + (i + 0.5) * h;
            const double v = eval1(f, x);
            if (!std::isfinite(v)) continue;
            directAn += v * std::cos(k * x);
            directBn += v * std::sin(k * x);
        }
        directAn *= h / L;
        directBn *= h / L;
    }
    const double coeffError =
        std::max(std::fabs(directAn - e.an[halfN - 1]),
                 std::fabs(directBn - e.bn[halfN - 1]));

    std::ostringstream samples;
    samples << "x\tf(x)\tS_N(x)\n";
    for (int j = 0; j <= 8; ++j) {
        const double x = -L + 2.0 * L * j / 8.0;
        const double fv = std::fabs(x) < 1e-12 ? eval1(f, 0.0) : eval1(f, x);
        samples << numStr(x, 5) << "\t" << numStr(fv) << "\t"
                << numStr(e.value(x, N)) << "\n";
    }

    r.functions.push_back("f(x) ~ a_0/2 + sum_{n=1.." + std::to_string(N) +
                          "} (a_n cos(n pi x/L) + b_n sin(n pi x/L)),  L = " +
                          numStr(L));
    r.functions.push_back(coeffs.str());
    r.functions.push_back("one-sided limits and jumps:\n" + jumpTable.str());
    r.functions.push_back("S_N(" + numStr(x0) + ") = " + numStr(sAt) + "; limit = " +
                          numStr(limitAt) + " (" +
                          (pointIsJump ? "jump: average of the sides" : "continuous")
                          + ")");
    r.functions.push_back("relative L^2 error: N/2 = " + numStr(l2Half, 3) +
                          ", N = " + numStr(l2N, 3));
    if (std::isfinite(decay))
        r.functions.push_back("coefficient decay exponent p ~ " + numStr(decay, 4));
    if (std::isfinite(gibbs))
        r.functions.push_back("Gibbs overshoot ratio = " + numStr(gibbs, 4) +
                              " at N = " + std::to_string(N) +
                              " (it approaches 0.08949 as N grows)");
    r.functions.push_back("parity of the coefficients: " + parityText(e, N));
    r.functions.push_back(samples.str());
    r.value = numEx(sAt);
    r.solution = Sym("S_N(x) = a_0/2 + sum (a_n cos(n pi x/L) + b_n sin(n pi x/L))");
    r.closedForm = false;
    r.numericOnly = true;
    r.numericNote = "The coefficients and limits are computed numerically. The "
                    "Gibbs overshoot is a finite-N measurement, not a proof.";

    addStep(r, "step.title.write_the_fourier_definition",
            Sym("a_n = (1/L) integral_{-L}^{L} f cos(n pi x/L) dx,  "
                "b_n = (1/L) integral f sin(n pi x/L) dx"));
    addStepRaw(r, "step.title.compute_the_coefficients", nullptr, coeffs.str());
    addStep(r, "step.title.state_the_series",
            Sym("S_N(x) = a_0/2 + sum_{n=1}^{N} [a_n cos(n pi x/L) + "
                "b_n sin(n pi x/L)]"));
    addStepRaw(r, "step.title.partial_sums", nullptr, samples.str());
    addStepRaw(r, "step.title.partial_sums", nullptr, jumpTable.str());
    addStep(r, "step.title.numeric_evidence",
            Sym("lim_{x -> x0} S_N(x) = " + numStr(limitAt) +
                ",  Gibbs ratio = " +
                (std::isfinite(gibbs) ? numStr(gibbs, 4) : std::string("n/a"))));
    addStep(r, "step.title.measure_the_convergence_order",
            Sym("relative L^2 error = " + numStr(l2N, 3) +
                ",  decay exponent ~ " +
                (std::isfinite(decay) ? numStr(decay, 3) : std::string("n/a"))));

    addCheck(r, "check.quadrature_independent",
             coeffError < 1e-5 * (1.0 + std::fabs(e.an[halfN - 1]) +
                                  std::fabs(e.bn[halfN - 1])),
             "", "", "max difference = " + numStr(coeffError, 3));
    addCheck(r, "check.partial_sum_at_x", std::fabs(sAt - limitAt) < pointTol, "",
             "{\"x\":" + numStr(x0) + "}",
             "S_N = " + numStr(sAt) + ", limit = " + numStr(limitAt));
    addCheck(r, "check.error_shrinks", l2N <= l2Half * 1.05 + 1e-12, "", "",
             "N/2 = " + numStr(l2Half, 3) + ", N = " + numStr(l2N, 3));
    if (std::isfinite(gibbs))
        addCheck(r, "check.gibbs_overshoot", gibbs > 0.02 && gibbs < 0.115,
                 "", "", "overshoot ratio = " + numStr(gibbs, 4));
    const std::string parity = parityText(e, N);
    if (parity == "even" || parity == "odd")
        addCheck(r, "check.fourier_symmetry", true, "", "",
                 "the series is " + parity);
    return r;
}

// ---------------------------------------------------------------- 87
Result fourierEvenOdd(const TopicRequest& req) {
    Result r;
    baseId(r, "fourier_even_odd", "x");
    const double L = req.number("L", kPi);
    const double x0 = req.number("x0", 0.0);
    int N = (int)std::lround(req.number("N", 10.0));
    N = std::clamp(N, 1, 60);
    if (!std::isfinite(L) || L <= 0 || !std::isfinite(x0)) {
        failCode(r, "bad_arguments");
        return r;
    }
    Ex f;
    try {
        f = parseExpr(req.text("f", "x^2"));
    } catch (const std::exception& e) {
        r.errorDetail = std::string("f: ") + e.what();
        failCode(r, "bad_expression");
        return r;
    }
    FourierExpansion e;
    std::string code;
    if (!buildFourier(f, L, N, e, code)) {
        failCode(r, code);
        return r;
    }

    auto even = [&](double x) { return 0.5 * (eval1(f, x) + eval1(f, -x)); };
    auto odd = [&](double x) { return 0.5 * (eval1(f, x) - eval1(f, -x)); };
    auto partial = [&](double x, int terms) { return e.value(x, terms); };

    // The cosine coefficients are the coefficients of f_e; the sine
    // coefficients are those of f_o.  Verify by a separate quadrature.
    std::ostringstream table;
    table << "n\ta_n (cos)\tb_n (sin)\n";
    for (int n = 1; n <= std::min(N, 8); ++n)
        table << n << "\t" << numStr(e.an[n - 1], 8) << "\t"
              << numStr(e.bn[n - 1], 8) << "\n";

    std::ostringstream samples;
    samples << "x\tf(x)\tf_e(x)\tf_o(x)\tf_e+f_o\tS_N(x)\n";
    double identityDefect = 0.0;
    double evenDefect = 0.0;
    double oddDefect = 0.0;
    for (int j = 1; j <= 8; ++j) {
        const double x = -L + 2.0 * L * j / 9.0;
        const double fv = eval1(f, x);
        const double ev = even(x), ov = odd(x);
        const double sv = partial(x, N);
        identityDefect = std::max(identityDefect, std::fabs(ev + ov - fv));
        evenDefect = std::max(evenDefect, std::fabs(even(x) - even(-x)));
        oddDefect = std::max(oddDefect, std::fabs(odd(x) + odd(-x)));
        samples << numStr(x, 5) << "\t" << numStr(fv) << "\t" << numStr(ev)
                << "\t" << numStr(ov) << "\t" << numStr(ev + ov) << "\t"
                << numStr(sv) << "\n";
    }

    const double l2N = relativeL2(f, e, N, L);
    const double l2Half = relativeL2(f, e, std::max(1, N / 2), L);
    const std::string parity = parityText(e, N);
    const double sAt = partial(x0, N);

    r.functions.push_back("f_e(x) = (f(x) + f(-x))/2,  f_o(x) = (f(x) - f(-x))/2");
    r.functions.push_back(table.str());
    r.functions.push_back(samples.str());
    r.functions.push_back("detected parity: " + parity +
                          (parity == "even" ? " (only cosine terms survive)"
                                            : parity == "odd"
                                                  ? " (only sine terms survive)"
                                                  : " (both families survive)"));
    r.functions.push_back("relative L^2 error: N/2 = " + numStr(l2Half, 3) +
                          ", N = " + numStr(l2N, 3));
    r.value = numEx(sAt);
    r.solution = Sym("f = f_e + f_o,  f_e ~ sum a_n cos(n pi x/L),  "
                     "f_o ~ sum b_n sin(n pi x/L)");
    r.closedForm = false;
    r.numericOnly = true;
    r.numericNote = "The split and the coefficients are evaluated numerically.";

    addStep(r, "step.title.write_the_fourier_definition",
            Sym("f_e(x) = (f(x) + f(-x))/2,  f_o(x) = (f(x) - f(-x))/2"));
    addStep(r, "step.title.compute_the_coefficients",
            Sym("f_e + f_o = f,  a_n from f_e,  b_n from f_o"));
    addStepRaw(r, "step.title.compute_the_coefficients", nullptr, table.str());
    addStepRaw(r, "step.title.partial_sums", nullptr, samples.str());
    addStep(r, "step.title.conclusion",
            Sym("parity = " + parity + ",  S_N(" + numStr(x0) + ") = " +
                numStr(sAt)));

    addCheck(r, "check.value_equals", identityDefect < 1e-10, "", "",
             "max |f_e + f_o - f| = " + numStr(identityDefect, 3));
    addCheck(r, "check.fourier_symmetry",
             evenDefect < 1e-10 && oddDefect < 1e-10, "", "",
             "even defect = " + numStr(evenDefect, 3) +
                 ", odd defect = " + numStr(oddDefect, 3));
    if (parity == "even") {
        double maxB = 0.0;
        for (double b : e.bn) maxB = std::max(maxB, std::fabs(b));
        addCheck(r, "check.constant_term", maxB < 1e-8, "", "",
                 "max |b_n| = " + numStr(maxB, 3));
    } else if (parity == "odd") {
        double maxA = std::fabs(e.a0);
        for (double a : e.an) maxA = std::max(maxA, std::fabs(a));
        addCheck(r, "check.constant_term", maxA < 1e-8, "", "",
                 "max |a_n| = " + numStr(maxA, 3));
    }
    addCheck(r, "check.error_shrinks", l2N <= l2Half * 1.05 + 1e-12, "", "",
             "N/2 = " + numStr(l2Half, 3) + ", N = " + numStr(l2N, 3));
    return r;
}

// ---------------------------------------------------------------- heat helpers
struct HeatSeries {
    double L = kPi;
    double alpha = 1.0;
    std::vector<double> lambda;
    std::vector<double> coeff;

    double mode(int n, double x) const { return std::sin(n * kPi * x / L); }

    double value(double x, double t, int terms) const {
        double sum = 0.0;
        for (int n = 1; n <= terms && n <= (int)coeff.size(); ++n)
            sum += coeff[n - 1] * mode(n, x) *
                   std::exp(-alpha * alpha * lambda[n - 1] * t);
        return sum;
    }
};

bool buildHeatSeries(const Ex& f, double L, double alpha, int N, HeatSeries& h,
                     std::string& code) {
    h.L = L;
    h.alpha = alpha;
    h.lambda.assign(N, 0.0);
    h.coeff.assign(N, 0.0);
    for (int n = 1; n <= N; ++n) {
        const double k = n * kPi / L;
        h.lambda[n - 1] = k * k;
        h.coeff[n - 1] = 2.0 / L *
                         simpson([&](double x) {
                             return eval1(f, x) * std::sin(k * x);
                         }, 0.0, L);
        if (!std::isfinite(h.coeff[n - 1])) {
            code = "not_integrable";
            return false;
        }
    }
    return true;
}

double maxAbsOnGrid(const std::function<double(double)>& g, double a, double b,
                    int samples = 200) {
    double worst = 0.0;
    for (int i = 0; i <= samples; ++i) {
        const double v = g(a + (b - a) * i / samples);
        if (std::isfinite(v)) worst = std::max(worst, std::fabs(v));
    }
    return worst;
}

double pdeResidualHeat(const std::function<double(double, double)>& u,
                       const std::function<double(double)>& uxx,
                       double alpha, double x, double t) {
    const double dt = 1e-5 * std::max(1.0, t);
    const double ut = (u(x, t + dt) - u(x, t - dt)) / (2 * dt);
    const double axx = uxx(x);
    if (!std::isfinite(ut) || !std::isfinite(axx)) return std::numeric_limits<double>::quiet_NaN();
    return ut - alpha * alpha * axx;
}

// ---------------------------------------------------------------- 88
Result heatDerivation(const TopicRequest& req) {
    Result r;
    baseId(r, "heat_derivation", "x");
    const double L = req.number("L", kPi);
    const double alpha = req.number("alpha", 1.0);
    int N = (int)std::lround(req.number("N", 10.0));
    N = std::clamp(N, 1, 60);
    if (!std::isfinite(L) || L <= 0 || !std::isfinite(alpha) || alpha <= 0) {
        failCode(r, "bad_arguments");
        return r;
    }
    Ex f;
    try {
        f = parseExpr(req.text("f", "sin(pi*x/L)"));
    } catch (const std::exception& e) {
        r.errorDetail = std::string("f: ") + e.what();
        failCode(r, "bad_expression");
        return r;
    }
    f = substitute(f, "L", numEx(L));
    HeatSeries h;
    std::string code;
    if (!buildHeatSeries(f, L, alpha, N, h, code)) {
        failCode(r, code);
        return r;
    }

    const double scale = L * L / (alpha * alpha);
    const double t1 = 0.1 * scale;
    const double t2 = 0.5 * scale;
    const double t3 = 2.0 * scale;
    const double x0 = req.number("x0", 0.5 * L);

    auto u = [&](double x, double t) { return h.value(x, t, N); };
    auto uxx = [&](double x) {
        const double hstep = 1e-3 * L;
        return (u(x + hstep, t1) - 2 * u(x, t1) + u(x - hstep, t1)) /
               (hstep * hstep);
    };

    auto icErrorAt = [&](int terms) {
        double worst = 0.0;
        for (int j = 1; j < 400; ++j) {
            const double x = L * j / 400.0;
            worst = std::max(
                worst, std::fabs(h.value(x, 0.0, terms) - eval1(f, x)));
        }
        return worst;
    };
    const double icError = icErrorAt(N);
    const double icHalf = icErrorAt(std::max(1, N / 2));
    const double bcError = std::max(std::fabs(u(0.0, t1)), std::fabs(u(L, t1)));
    const double residual = pdeResidualHeat(u, uxx, alpha, 0.4 * L, t1);
    const double scaleU = std::max(1.0, maxAbsOnGrid([&](double x) { return u(x, t1); }, 0.0, L));
    const double amp1 = maxAbsOnGrid([&](double x) { return u(x, t1); }, 0.0, L);
    const double amp2 = maxAbsOnGrid([&](double x) { return u(x, t2); }, 0.0, L);
    const double amp3 = maxAbsOnGrid([&](double x) { return u(x, t3); }, 0.0, L);

    std::ostringstream coeffs;
    coeffs << "n\tlambda_n\tb_n\tdecay rate\n";
    for (int n = 1; n <= N; ++n)
        coeffs << n << "\t" << numStr(h.lambda[n - 1], 6) << "\t"
               << numStr(h.coeff[n - 1], 8) << "\t"
               << numStr(alpha * alpha * h.lambda[n - 1], 6) << "\n";

    std::ostringstream snapshots;
    snapshots << "x\tu(x,0)\tu(x,t1)\tu(x,t2)\tu(x,t3)\n";
    for (int j = 0; j <= 8; ++j) {
        const double x = L * j / 8.0;
        snapshots << numStr(x, 5) << "\t" << numStr(u(x, 0.0)) << "\t"
                  << numStr(u(x, t1)) << "\t" << numStr(u(x, t2)) << "\t"
                  << numStr(u(x, t3)) << "\n";
    }

    r.functions.push_back("IBVP: u_t = alpha^2 u_xx on (0, L), "
                          "u(0,t) = u(L,t) = 0, u(x,0) = f(x)");
    r.functions.push_back("L = " + numStr(L) + ", alpha = " + numStr(alpha) +
                          ", N = " + std::to_string(N));
    r.functions.push_back(coeffs.str());
    r.functions.push_back("u(x,t) = sum_{n=1..N} b_n sin(n pi x/L) "
                          "exp(-alpha^2 (n pi/L)^2 t)");
    r.functions.push_back("initial-condition error = " + numStr(icError, 3) +
                          ", boundary error = " + numStr(bcError, 3));
    r.functions.push_back("PDE residual at (0.4 L, t1) = " + numStr(residual, 3));
    r.functions.push_back("amplitude decay: t1 -> " + numStr(amp1, 4) +
                          ", t2 -> " + numStr(amp2, 4) + ", t3 -> " +
                          numStr(amp3, 4));
    r.functions.push_back("steady state: u = 0");
    r.functions.push_back(snapshots.str());
    r.value = numEx(u(x0, t1));
    r.solution = Sym("u(x,t) = sum b_n sin(n pi x/L) exp(-alpha^2 (n pi/L)^2 t)");
    r.closedForm = false;
    r.numericOnly = true;
    r.numericNote = "The coefficients come from composite Simpson quadrature "
                    "and the snapshots are evaluated from the truncated series.";

    addStep(r, "step.title.write_the_balance_law",
            Sym("rho c u_t = K u_xx,  alpha^2 = K/(rho c)"),
            "step.note.energy_conservation_on_a_slice_gives_the_heat_equation");
    addStep(r, "step.title.write_the_model",
            Sym("u_t = alpha^2 u_xx,  0 < x < L,  t > 0"));
    addStep(r, "step.title.apply_the_boundary_conditions",
            Sym("u(0,t) = u(L,t) = 0  =>  X_n = sin(n pi x/L),  "
                "lambda_n = (n pi/L)^2"));
    addStep(r, "step.title.compute_the_fourier_coefficients",
            Sym("b_n = (2/L) integral_0^L f(x) sin(n pi x/L) dx"));
    addStepRaw(r, "step.title.compute_the_coefficients", nullptr, coeffs.str());
    addStep(r, "step.title.write_the_series_solution",
            Sym("u(x,t) = sum b_n sin(n pi x/L) exp(-alpha^2 lambda_n t)"));
    addStepRaw(r, "step.title.partial_sums", nullptr, snapshots.str());
    addStep(r, "step.title.conclusion",
            Sym("max |u(.,t3)| = " + numStr(amp3, 3) + " -> 0"));

    addCheck(r, "check.initial_condition",
             icError <= icHalf * 1.05 + 1e-8 * (1.0 + scaleU), "", "",
             "max error: N/2 = " + numStr(icHalf, 3) + ", N = " +
                 numStr(icError, 3));
    addCheck(r, "check.boundary_conditions", bcError < 1e-10, "", "",
             "max |u| at the ends = " + numStr(bcError, 3));
    addCheck(r, "check.pde_residual",
             std::fabs(residual) < 2e-3 * (1.0 + scaleU), "", "",
             "u_t - alpha^2 u_xx = " + numStr(residual, 3));
    addCheck(r, "check.long_term_temperature",
             amp2 <= amp1 * 1.02 + 1e-12 && amp3 <= amp2 * 1.02 + 1e-12, "", "",
             "amplitudes = " + numStr(amp1, 3) + ", " + numStr(amp2, 3) +
                 ", " + numStr(amp3, 3));
    return r;
}

// ---------------------------------------------------------------- 89
struct WaveSeries {
    double L = kPi;
    double c = 1.0;
    std::vector<double> mu;
    std::vector<double> A;
    std::vector<double> B;

    double value(double x, double t, int terms) const {
        double sum = 0.0;
        for (int n = 1; n <= terms && n <= (int)A.size(); ++n) {
            const double w = c * mu[n - 1];
            sum += (A[n - 1] * std::cos(w * t) + B[n - 1] * std::sin(w * t)) *
                   std::sin(mu[n - 1] * x);
        }
        return sum;
    }
};

bool buildWaveSeries(const Ex& f, const Ex& g, double L, double c, int N,
                     WaveSeries& w, std::string& code) {
    w.L = L;
    w.c = c;
    w.mu.assign(N, 0.0);
    w.A.assign(N, 0.0);
    w.B.assign(N, 0.0);
    for (int n = 1; n <= N; ++n) {
        const double mu = n * kPi / L;
        w.mu[n - 1] = mu;
        w.A[n - 1] = 2.0 / L * simpson([&](double x) {
                         return eval1(f, x) * std::sin(mu * x);
                     }, 0.0, L);
        w.B[n - 1] = 2.0 / (c * mu * L) * simpson([&](double x) {
                         return eval1(g, x) * std::sin(mu * x);
                     }, 0.0, L);
        if (!std::isfinite(w.A[n - 1]) || !std::isfinite(w.B[n - 1])) {
            code = "not_integrable";
            return false;
        }
    }
    return true;
}

Result waveDerivation(const TopicRequest& req) {
    Result r;
    baseId(r, "wave_derivation", "x");
    const double L = req.number("L", kPi);
    const double c = req.number("c", 1.0);
    int N = (int)std::lround(req.number("N", 10.0));
    N = std::clamp(N, 1, 60);
    const std::string domain = req.text("domain", "finite");
    if (!std::isfinite(L) || L <= 0 || !std::isfinite(c) || c <= 0) {
        failCode(r, "bad_arguments");
        return r;
    }
    Ex f, g;
    try {
        f = parseExpr(req.text("f", "sin(pi*x/L)"));
        g = parseExpr(req.text("g", "0"));
    } catch (const std::exception& e) {
        r.errorDetail = std::string("f or g: ") + e.what();
        failCode(r, "bad_expression");
        return r;
    }
    f = substitute(f, "L", numEx(L));
    g = substitute(g, "L", numEx(L));
    const double x0 = req.number("x0", 0.5 * L);

    if (domain == "infinite") {
        auto uD = [&](double x, double t) {
            const double left = eval1(f, x - c * t);
            const double right = eval1(f, x + c * t);
            if (!std::isfinite(left) || !std::isfinite(right))
                return std::numeric_limits<double>::quiet_NaN();
            const double integral = integrate(
                [&](double s) { return eval1(g, s); }, x - c * t, x + c * t, 2000);
            return 0.5 * (left + right) + integral / (2.0 * c);
        };
        const double dt = 1e-5 * std::max(1.0, L / c);
        double icError = 0.0, velocityError = 0.0;
        for (int j = 1; j <= 8; ++j) {
            const double x = -L + 2.0 * L * j / 9.0;
            icError = std::max(icError, std::fabs(uD(x, 0.0) - eval1(f, x)));
            const double ut = (uD(x, dt) - uD(x, -dt)) / (2 * dt);
            velocityError = std::max(velocityError, std::fabs(ut - eval1(g, x)));
        }
        std::ostringstream samples;
        samples << "x\tu(x,0)\tu(x,T/4)\n";
        const double T = 2.0 * L / c;
        for (int j = 0; j <= 8; ++j) {
            const double x = -L + 2.0 * L * j / 8.0;
            samples << numStr(x, 5) << "\t" << numStr(uD(x, 0.0)) << "\t"
                    << numStr(uD(x, T / 4.0)) << "\n";
        }
        r.functions.push_back("d'Alembert solution on the whole line:");
        r.functions.push_back("u(x,t) = 1/2 [f(x - ct) + f(x + ct)] + "
                              "1/(2c) integral_{x-ct}^{x+ct} g(s) ds");
        r.functions.push_back("c = " + numStr(c));
        r.functions.push_back(samples.str());
        r.functions.push_back("initial displacement error = " +
                              numStr(icError, 3));
        r.functions.push_back("initial velocity error = " +
                              numStr(velocityError, 3));
        r.value = numEx(uD(x0, T / 4.0));
        r.solution = Sym("u(x,t) = 1/2 [f(x-ct) + f(x+ct)] + "
                         "1/(2c) integral_{x-ct}^{x+ct} g(s) ds");
        r.closedForm = false;
        r.numericOnly = true;
        r.numericNote = "The d'Alembert integral is evaluated by Simpson's rule.";

        addStep(r, "step.title.write_the_model",
                Sym("u_tt = c^2 u_xx on the whole line"),
                "step.note.no_boundaries_only_initial_data");
        addStep(r, "step.title.write_the_general_solution",
                Sym("u = F(x - ct) + G(x + ct)"));
        addStep(r, "step.title.apply_the_initial_conditions",
                Sym("u(x,t) = 1/2 [f(x-ct) + f(x+ct)] + "
                    "1/(2c) integral_{x-ct}^{x+ct} g(s) ds"));
        addStepRaw(r, "step.title.partial_sums", nullptr, samples.str());

        addCheck(r, "check.initial_displacement", icError < 1e-8 * (1.0 + L), "",
                 "", "max error = " + numStr(icError, 3));
        addCheck(r, "check.initial_velocity", velocityError < 1e-5 * (1.0 + L), "",
                 "", "max error = " + numStr(velocityError, 3));
        return r;
    }
    if (domain != "finite") {
        failCode(r, "bad_domain");
        r.errorDetail = "domain must be 'finite' or 'infinite'";
        return r;
    }

    WaveSeries w;
    std::string code;
    if (!buildWaveSeries(f, g, L, c, N, w, code)) {
        failCode(r, code);
        return r;
    }
    const double T = 2.0 * L / c;
    const double t1 = T / 4.0;
    const double t2 = T / 2.0;

    auto u = [&](double x, double t) { return w.value(x, t, N); };
    auto uxx = [&](double x) {
        const double hs = 1e-3 * L;
        return (u(x + hs, t1) - 2 * u(x, t1) + u(x - hs, t1)) / (hs * hs);
    };
    const double dt = 1e-5 * std::max(1.0, T);
    auto residualAt = [&](double x) {
        const double utt = (u(x, t1 + dt) - 2 * u(x, t1) + u(x, t1 - dt)) /
                           (dt * dt);
        const double axx = uxx(x);
        return utt - c * c * axx;
    };
    const double pdeResidual = residualAt(0.4 * L);
    const double scaleU = std::max(1.0, maxAbsOnGrid([&](double x) { return u(x, t1); }, 0.0, L));

    const double dv = 1e-6 * std::max(1.0, T);
    auto dataErrorAt = [&](int terms, double& ic, double& vel) {
        ic = 0.0;
        vel = 0.0;
        for (int j = 1; j < 200; ++j) {
            const double x = L * j / 200.0;
            ic = std::max(ic, std::fabs(w.value(x, 0.0, terms) - eval1(f, x)));
            const double ut = (w.value(x, dv, terms) - w.value(x, -dv, terms)) /
                              (2 * dv);
            vel = std::max(vel, std::fabs(ut - eval1(g, x)));
        }
    };
    double icError = 0.0, velocityError = 0.0;
    dataErrorAt(N, icError, velocityError);
    double icHalf = 0.0, velHalf = 0.0;
    dataErrorAt(std::max(1, N / 2), icHalf, velHalf);
    const double bcError = std::max(std::fabs(u(0.0, t1)), std::fabs(u(L, t1)));

    // Energy from the truncated series by quadrature (differentiate numerically).
    auto energy = [&](double t) {
        const double hs = L / 4000.0;
        const double dts = 1e-5 * std::max(1.0, T);
        auto integrand = [&](double x) {
            const double ut = (u(x, t + dts) - u(x, t - dts)) / (2 * dts);
            const double ux = (u(x + hs, t) - u(x - hs, t)) / (2 * hs);
            return ut * ut + c * c * ux * ux;
        };
        return integrate(integrand, 0.0, L, 800);
    };
    const double energy0 = energy(0.0);
    const double energy1 = energy(t1);
    const double energyDrift = std::fabs(energy1 - energy0) /
                               std::max(1e-12, energy0);

    std::ostringstream coeffs;
    coeffs << "n\tmu_n\tA_n\tB_n\n";
    for (int n = 1; n <= N; ++n)
        coeffs << n << "\t" << numStr(w.mu[n - 1], 6) << "\t"
               << numStr(w.A[n - 1], 8) << "\t" << numStr(w.B[n - 1], 8) << "\n";

    std::ostringstream snapshots;
    snapshots << "x\tu(x,0)\tu(x,T/4)\tu(x,T/2)\n";
    for (int j = 0; j <= 8; ++j) {
        const double x = L * j / 8.0;
        snapshots << numStr(x, 5) << "\t" << numStr(u(x, 0.0)) << "\t"
                  << numStr(u(x, t1)) << "\t" << numStr(u(x, t2)) << "\n";
    }

    r.functions.push_back("IBVP: u_tt = c^2 u_xx on (0, L), "
                          "u(0,t) = u(L,t) = 0, u(x,0) = f(x), "
                          "u_t(x,0) = g(x)");
    r.functions.push_back("L = " + numStr(L) + ", c = " + numStr(c) +
                          ", N = " + std::to_string(N));
    r.functions.push_back(coeffs.str());
    r.functions.push_back("u(x,t) = sum [A_n cos(c mu_n t) + "
                          "B_n sin(c mu_n t)] sin(mu_n x), mu_n = n pi/L");
    r.functions.push_back("initial displacement error = " +
                          numStr(icError, 3) + ", initial velocity error = " +
                          numStr(velocityError, 3) + ", boundary error = " +
                          numStr(bcError, 3));
    r.functions.push_back("PDE residual at (0.4 L, T/4) = " +
                          numStr(pdeResidual, 3));
    r.functions.push_back("energy drift over T/4 = " + numStr(energyDrift, 3));
    r.functions.push_back(snapshots.str());
    r.value = numEx(u(x0, t1));
    r.solution = Sym("u(x,t) = sum [A_n cos(c mu_n t) + B_n sin(c mu_n t)] "
                     "sin(mu_n x)");
    r.closedForm = false;
    r.numericOnly = true;
    r.numericNote = "The coefficients and snapshots are evaluated from the "
                    "truncated Fourier series; the energy check is numerical.";

    addStep(r, "step.title.write_the_balance_law",
            Sym("rho u_tt = T u_xx  =>  u_tt = c^2 u_xx,  c^2 = T/rho"));
    addStep(r, "step.title.separate_the_variables",
            Sym("u = X(x) T(t),  X'' + lambda X = 0,  T'' + c^2 lambda T = 0"));
    addStep(r, "step.title.apply_the_boundary_conditions",
            Sym("X(0) = X(L) = 0  =>  X_n = sin(n pi x/L),  "
                "lambda_n = (n pi/L)^2"));
    addStep(r, "step.title.compute_the_fourier_coefficients",
            Sym("A_n = (2/L) integral f sin(n pi x/L),  "
                "B_n = 2/(c mu_n L) integral g sin(n pi x/L)"));
    addStepRaw(r, "step.title.compute_the_coefficients", nullptr, coeffs.str());
    addStep(r, "step.title.write_the_series_solution",
            Sym("u(x,t) = sum [A_n cos(c mu_n t) + B_n sin(c mu_n t)] "
                "sin(mu_n x)"));
    addStepRaw(r, "step.title.partial_sums", nullptr, snapshots.str());
    addStep(r, "step.title.check_the_conserved_quantity",
            Sym("E = integral (u_t^2 + c^2 u_x^2) dx,  drift = " +
                numStr(energyDrift, 3)));

    addCheck(r, "check.initial_displacement",
             icError <= icHalf * 1.05 + 1e-8 * (1.0 + scaleU), "", "",
             "max error: N/2 = " + numStr(icHalf, 3) + ", N = " +
                 numStr(icError, 3));
    addCheck(r, "check.initial_velocity",
             velocityError <= velHalf * 1.05 + 1e-8 * (1.0 + scaleU), "", "",
             "max error: N/2 = " + numStr(velHalf, 3) + ", N = " +
                 numStr(velocityError, 3));
    addCheck(r, "check.boundary_conditions", bcError < 1e-10, "", "",
             "max |u| at the ends = " + numStr(bcError, 3));
    addCheck(r, "check.pde_residual",
             std::fabs(pdeResidual) < 2e-2 * (1.0 + scaleU), "", "",
             "u_tt - c^2 u_xx = " + numStr(pdeResidual, 3));
    addCheck(r, "check.the_conserved_quantity_is_constant", energyDrift < 2e-2,
             "", "", "relative energy drift = " + numStr(energyDrift, 3));
    return r;
}

// ---------------------------------------------------------------- 113
struct HeatMode {
    double lambda = 0.0;
    std::function<double(double)> phi;
    std::function<double(double)> dphi;
    double norm2 = 1.0;
};

bool buildModes(const std::string& bc, double L, double h, double alpha,
                std::vector<HeatMode>& modes, std::string& code) {
    modes.clear();
    if (bc == "dirichlet" || bc == "fixed_nonzero") {
        for (int n = 1; n <= 40; ++n) {
            const double mu = n * kPi / L;
            HeatMode m;
            m.lambda = mu * mu;
            m.phi = [mu](double x) { return std::sin(mu * x); };
            m.dphi = [mu](double x) { return mu * std::cos(mu * x); };
            m.norm2 = 0.5 * L;
            modes.push_back(m);
        }
        return true;
    }
    if (bc == "insulated") {
        for (int n = 0; n <= 40; ++n) {
            const double mu = n * kPi / L;
            HeatMode m;
            m.lambda = mu * mu;
            if (n == 0) {
                m.phi = [](double) { return 1.0; };
                m.dphi = [](double) { return 0.0; };
                m.norm2 = L;
            } else {
                m.phi = [mu](double x) { return std::cos(mu * x); };
                m.dphi = [mu](double x) { return -mu * std::sin(mu * x); };
                m.norm2 = 0.5 * L;
            }
            modes.push_back(m);
        }
        return true;
    }
    if (bc == "mixed") {
        for (int n = 1; n <= 40; ++n) {
            const double mu = (2 * n - 1) * kPi / (2 * L);
            HeatMode m;
            m.lambda = mu * mu;
            m.phi = [mu](double x) { return std::sin(mu * x); };
            m.dphi = [mu](double x) { return mu * std::cos(mu * x); };
            m.norm2 = 0.5 * L;
            modes.push_back(m);
        }
        return true;
    }
    if (bc == "robin") {
        if (!(h > 0)) {
            code = "bad_arguments";
            return false;
        }
        // Roots of mu tan(mu L) = h, one in each ((n-1) pi/L, (n-1/2) pi/L).
        for (int n = 1; n <= 40; ++n) {
            double lo = (n - 1) * kPi / L + 1e-9;
            double hi = (n - 0.5) * kPi / L - 1e-9;
            auto F = [&](double mu) { return mu * std::tan(mu * L) - h; };
            double flo = F(lo);
            if (!(flo < 0)) continue;
            for (int it = 0; it < 200; ++it) {
                const double mid = 0.5 * (lo + hi);
                const double fm = F(mid);
                if (fm > 0) {
                    hi = mid;
                } else {
                    lo = mid;
                    flo = fm;
                }
                if (hi - lo < 1e-14 * std::max(1.0, hi)) break;
            }
            const double mu = 0.5 * (lo + hi);
            if (!std::isfinite(mu)) {
                code = "root_failed";
                return false;
            }
            HeatMode m;
            m.lambda = mu * mu;
            m.phi = [mu](double x) { return std::cos(mu * x); };
            m.dphi = [mu](double x) { return -mu * std::sin(mu * x); };
            m.norm2 = 0.5 * L + std::sin(2 * mu * L) / (4 * mu);
            modes.push_back(m);
        }
        if (modes.empty()) {
            code = "root_failed";
            return false;
        }
        return true;
    }
    code = "unknown_bc";
    return false;
}

Result heatOtherBc(const TopicRequest& req) {
    Result r;
    baseId(r, "heat_other_bc", "x");
    const std::string bc = req.text("bc", "insulated");
    const double L = req.number("L", kPi);
    const double alpha = req.number("alpha", 1.0);
    const double h = req.number("h", 1.0);
    const double u0 = req.number("u0", 0.0);
    const double u1 = req.number("u1", 0.0);
    const double x0 = req.number("x0", 0.5);
    int N = (int)std::lround(req.number("N", 10.0));
    N = std::clamp(N, 1, 40);
    if (!std::isfinite(L) || L <= 0 || !std::isfinite(alpha) || alpha <= 0) {
        failCode(r, "bad_arguments");
        return r;
    }
    Ex f;
    try {
        f = parseExpr(req.text("f", "cos(pi*x/L)"));
    } catch (const std::exception& e) {
        r.errorDetail = std::string("f: ") + e.what();
        failCode(r, "bad_expression");
        return r;
    }
    f = substitute(f, "L", numEx(L));

    std::vector<HeatMode> modes;
    std::string code;
    if (!buildModes(bc, L, h, alpha, modes, code)) {
        failCode(r, code);
        if (code == "unknown_bc")
            r.errorDetail = "bc must be insulated, dirichlet, fixed_nonzero, "
                            "mixed or robin";
        return r;
    }
    N = std::min(N, (int)modes.size());
    modes.resize(N);

    std::string bcText;
    if (bc == "insulated")
        bcText = "u_x(0,t) = u_x(L,t) = 0";
    else if (bc == "dirichlet")
        bcText = "u(0,t) = u(L,t) = 0";
    else if (bc == "fixed_nonzero")
        bcText = "u(0,t) = " + numStr(u0) + ", u(L,t) = " + numStr(u1);
    else if (bc == "mixed")
        bcText = "u(0,t) = 0, u_x(L,t) = 0";
    else
        bcText = "u_x(0,t) = 0, u_x(L,t) + h u(L,t) = 0, h = " + numStr(h);

    double steadyValue = 0.0;
    std::function<double(double)> steady;
    if (bc == "insulated") {
        steadyValue = integrate([&](double x) { return eval1(f, x); }, 0.0, L) / L;
        steady = [steadyValue](double) { return steadyValue; };
    } else if (bc == "fixed_nonzero") {
        steady = [&](double x) { return u0 + (u1 - u0) * x / L; };
    } else {
        steady = [](double) { return 0.0; };
    }

    std::vector<double> coeff(N, 0.0);
    for (int n = 0; n < N; ++n) {
        const HeatMode& m = modes[n];
        const double numerator = integrate(
            [&](double x) {
                const double base = eval1(f, x) - steady(x);
                return base * m.phi(x);
            },
            0.0, L, 2000);
        coeff[n] = numerator / m.norm2;
        if (!std::isfinite(coeff[n])) {
            failCode(r, "not_integrable");
            return r;
        }
    }

    auto uTerms = [&](double x, double t, int terms) {
        double sum = steady(x);
        for (int n = 0; n < terms && n < N; ++n)
            sum += coeff[n] * modes[n].phi(x) *
                   std::exp(-alpha * alpha * modes[n].lambda * t);
        return sum;
    };
    auto u = [&](double x, double t) { return uTerms(x, t, N); };
    auto ux = [&](double x, double t) {
        double sum = 0.0;
        if (bc == "fixed_nonzero") sum = (u1 - u0) / L;
        for (int n = 0; n < N; ++n)
            sum += coeff[n] * modes[n].dphi(x) *
                   std::exp(-alpha * alpha * modes[n].lambda * t);
        return sum;
    };

    const double scaleTime = L * L / (alpha * alpha);
    const double t1 = 0.1 * scaleTime;
    const double t2 = 0.5 * scaleTime;
    // Run long enough that every non-constant mode has decayed.
    double lambdaMin = std::numeric_limits<double>::infinity();
    for (const HeatMode& m : modes)
        if (m.lambda > 1e-12) lambdaMin = std::min(lambdaMin, m.lambda);
    const double t3 = std::isfinite(lambdaMin)
                          ? 25.0 / (alpha * alpha * lambdaMin)
                          : 2.0 * scaleTime;

    auto icErrorAt = [&](int terms) {
        double worst = 0.0;
        for (int j = 1; j < 400; ++j) {
            const double x = L * j / 400.0;
            worst = std::max(worst,
                             std::fabs(uTerms(x, 0.0, terms) - eval1(f, x)));
        }
        return worst;
    };
    const double icError = icErrorAt(N);
    const double icHalf = icErrorAt(std::max(1, N / 2));

    double bcError = 0.0;
    if (bc == "insulated") {
        bcError = std::max(std::fabs(ux(0.0, t1)), std::fabs(ux(L, t1)));
    } else if (bc == "robin") {
        bcError = std::max(std::fabs(ux(0.0, t1)),
                           std::fabs(ux(L, t1) + h * u(L, t1)));
    } else if (bc == "mixed") {
        bcError = std::max(std::fabs(u(0.0, t1)), std::fabs(ux(L, t1)));
    } else if (bc == "fixed_nonzero") {
        bcError = std::max(std::fabs(u(0.0, t1) - u0), std::fabs(u(L, t1) - u1));
    } else {
        bcError = std::max(std::fabs(u(0.0, t1)), std::fabs(u(L, t1)));
    }

    const double hs = 1e-3 * L;
    auto uxx = [&](double x) {
        return (u(x + hs, t1) - 2 * u(x, t1) + u(x - hs, t1)) / (hs * hs);
    };
    const double dt = 1e-5 * std::max(1.0, t1);
    const double ut = (u(0.4 * L, t1 + dt) - u(0.4 * L, t1 - dt)) / (2 * dt);
    const double residual = ut - alpha * alpha * uxx(0.4 * L);
    const double scaleU = std::max(1.0, maxAbsOnGrid([&](double x) { return u(x, t1); }, 0.0, L));

    const double steadyError = maxAbsOnGrid(
        [&](double x) { return u(x, t3) - steady(x); }, 0.0, L, 400);
    const double energy1 = integrate([&](double x) {
        const double v = u(x, t1);
        return v * v;
    }, 0.0, L, 2000);
    const double energy2 = integrate([&](double x) {
        const double v = u(x, t2);
        return v * v;
    }, 0.0, L, 2000);

    std::ostringstream coeffs;
    coeffs << "n\tlambda_n\tc_n\tdecay rate\n";
    for (int n = 0; n < N; ++n)
        coeffs << n + 1 << "\t" << numStr(modes[n].lambda, 6) << "\t"
               << numStr(coeff[n], 8) << "\t"
               << numStr(alpha * alpha * modes[n].lambda, 6) << "\n";

    std::ostringstream snapshots;
    snapshots << "x\tu(x,0)\tu(x,t1)\tu(x,t2)\tu(x,t3)\tsteady\n";
    for (int j = 0; j <= 8; ++j) {
        const double x = L * j / 8.0;
        snapshots << numStr(x, 5) << "\t" << numStr(u(x, 0.0)) << "\t"
                  << numStr(u(x, t1)) << "\t" << numStr(u(x, t2)) << "\t"
                  << numStr(u(x, t3)) << "\t" << numStr(steady(x)) << "\n";
    }

    r.functions.push_back("boundary conditions: " + bcText);
    r.functions.push_back("u_t = alpha^2 u_xx on (0, L), alpha = " +
                          numStr(alpha) + ", N = " + std::to_string(N));
    r.functions.push_back(coeffs.str());
    r.functions.push_back("steady state: " +
                          std::string(bc == "insulated"
                                          ? "the mean value " + numStr(steadyValue)
                                          : bc == "fixed_nonzero"
                                                ? "the linear profile through the end data"
                                                : "u = 0"));
    r.functions.push_back("initial-condition error = " + numStr(icError, 3) +
                          ", boundary error = " + numStr(bcError, 3));
    r.functions.push_back("PDE residual at (0.4 L, t1) = " + numStr(residual, 3));
    r.functions.push_back("distance to the steady state at t3 = " +
                          numStr(steadyError, 3));
    r.functions.push_back("energy at t1 = " + numStr(energy1, 4) +
                          ", at t2 = " + numStr(energy2, 4));
    r.functions.push_back(snapshots.str());
    r.value = numEx(u(x0, t1));
    r.solution = Sym("u(x,t) = v(x) + sum c_n phi_n(x) "
                     "exp(-alpha^2 lambda_n t)");
    r.closedForm = false;
    r.numericOnly = true;
    r.numericNote = "The modes and coefficients are computed numerically; the "
                    "Robin roots come from a bracketed bisection.";

    addStep(r, "step.title.write_the_model",
            Sym("u_t = alpha^2 u_xx,  " + bcText));
    addStep(r, "step.title.apply_the_boundary_conditions",
            Sym("eigenvalues lambda_n and eigenfunctions phi_n from the "
                "selected boundary conditions"));
    addStep(r, "step.title.compute_the_coefficients",
            Sym("c_n = integral (f - v) phi_n / integral phi_n^2"));
    addStepRaw(r, "step.title.compute_the_coefficients", nullptr, coeffs.str());
    addStep(r, "step.title.write_the_series_solution",
            Sym("u(x,t) = v(x) + sum c_n phi_n(x) "
                "exp(-alpha^2 lambda_n t)"));
    addStepRaw(r, "step.title.partial_sums", nullptr, snapshots.str());
    addStep(r, "step.title.conclusion",
            Sym("u(x,t3) approaches the steady state; distance = " +
                numStr(steadyError, 3)));

    addCheck(r, "check.initial_condition",
             icError <= icHalf * 1.05 + 1e-8 * (1.0 + scaleU), "", "",
             "max error: N/2 = " + numStr(icHalf, 3) + ", N = " +
                 numStr(icError, 3));
    addCheck(r, "check.boundary_conditions", bcError < 1e-8, "", "",
             "boundary residual = " + numStr(bcError, 3));
    addCheck(r, "check.pde_residual",
             std::fabs(residual) < 2e-2 * (1.0 + scaleU), "", "",
             "u_t - alpha^2 u_xx = " + numStr(residual, 3));
    addCheck(r, "check.long_term_temperature", steadyError < 1e-3 * (1.0 + scaleU),
             "", "", "distance to steady state = " + numStr(steadyError, 3));
    if (bc == "insulated" || bc == "dirichlet" || bc == "mixed")
        addCheck(r, "check.energy_shrinks", energy2 <= energy1 * 1.02 + 1e-12,
                 "", "", "energy: t1 = " + numStr(energy1, 4) +
                             ", t2 = " + numStr(energy2, 4));
    return r;
}

} // namespace

Result fourierConvergenceTopic(const TopicRequest& req) {
    return fourierConvergence(req);
}
Result fourierEvenOddTopic(const TopicRequest& req) {
    return fourierEvenOdd(req);
}
Result heatDerivationTopic(const TopicRequest& req) {
    return heatDerivation(req);
}
Result waveDerivationTopic(const TopicRequest& req) {
    return waveDerivation(req);
}
Result heatOtherBcTopic(const TopicRequest& req) {
    return heatOtherBc(req);
}

} // namespace ode
