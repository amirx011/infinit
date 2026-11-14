// Infinit Lab — singular Sturm–Liouville problems and orthogonal series.
// SPDX-License-Identifier: Apache-2.0
#include "special_series.hpp"

#include "expr.hpp"
#include "numeric.hpp"
#include "special_functions.hpp"

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

std::string numStr(double v, int digits = 8) {
    if (std::isnan(v)) return "nan";
    if (!std::isfinite(v)) return v < 0 ? "-inf" : "inf";
    if (std::fabs(v) < 1e-13) return "0";
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

std::string rat(Rational q) { return rationalToString(q); }

double eval1(const Ex& e, double x) {
    std::map<std::string, double> vars{{"x", x}};
    bool ok = false;
    const double v = evalReal(e, vars, &ok);
    return ok && std::isfinite(v) ? v : std::numeric_limits<double>::quiet_NaN();
}

// ---------------------------------------------------------------- quadrature
struct QuadRule {
    std::vector<double> nodes;
    std::vector<double> weights;
};

const QuadRule& legendreRule(int n) {
    static std::map<int, QuadRule> cache;
    auto it = cache.find(n);
    if (it != cache.end()) return it->second;
    QuadRule rule;
    gaussLegendre(n, rule.nodes, rule.weights);
    return cache.emplace(n, rule).first->second;
}

double integrateGL(const std::function<double(double)>& g, double a, double b,
                   int n = 64) {
    const QuadRule& rule = legendreRule(n);
    const double mid = 0.5 * (a + b);
    const double half = 0.5 * (b - a);
    double sum = 0.0;
    for (size_t i = 0; i < rule.nodes.size(); ++i)
        sum += rule.weights[i] * g(mid + half * rule.nodes[i]);
    return half * sum;
}

// ---------------------------------------------------------------- 93
struct SingularProblem {
    std::string family;
    double lambdaFirst = 0.0;
    std::vector<double> lambda;
    std::vector<std::string> lines;
    bool positive = true;
    bool ordered = true;
    double odeResidual = 0.0;
    double orthogonalDefect = 0.0;
    std::string weightText;
    std::string endpointText;
    std::string equationText;
};

bool buildBesselSingular(double nu, double L, int K, SingularProblem& p) {
    p.family = "bessel";
    p.equationText = "-(x y')' + (nu^2/x) y = lambda x y on (0, L)";
    p.weightText = "x";
    p.endpointText = "x = 0 is regular singular (bounded solution); x = L is "
                     "regular (Dirichlet y(L) = 0)";
    std::vector<double> alpha(K);
    for (int n = 0; n < K; ++n) {
        alpha[n] = besselZero(n + 1, nu);
        if (!std::isfinite(alpha[n])) return false;
        p.lambda.push_back(alpha[n] * alpha[n] / (L * L));
    }
    std::ostringstream table;
    table << "n\talpha_n\tlambda_n\n";
    for (int n = 0; n < K; ++n)
        table << n + 1 << "\t" << numStr(alpha[n], 8) << "\t"
              << numStr(p.lambda[n], 8) << "\n";
    p.lines.push_back(table.str());

    // ODE check: t^2 J'' + t J' + (t^2 - nu^2) J = 0 with t = alpha_n x / L.
    double worst = 0.0;
    for (int n = 0; n < K; ++n) {
        for (double frac : {0.25, 0.55, 0.8}) {
            const double x = frac * L;
            const double t = alpha[n] * x / L;
            const double h = 1e-5 * std::max(1.0, t);
            const double j0 = besselJ(nu, t);
            const double jp = besselJ(nu, t + h);
            const double jm = besselJ(nu, t - h);
            const double d1 = (jp - jm) / (2 * h);
            const double d2 = (jp - 2 * j0 + jm) / (h * h);
            const double residual = t * t * d2 + t * d1 + (t * t - nu * nu) * j0;
            worst = std::max(worst, std::fabs(residual));
        }
    }
    p.odeResidual = worst;

    // Orthogonality: integral_0^L x J_m J_n = delta_mn L^2 J_{nu+1}^2 / 2.
    double maxOff = 0.0;
    double maxNorm = 0.0;
    for (int n = 0; n < K; ++n) {
        for (int m = 0; m < K; ++m) {
            const double value = integrateGL(
                [&](double x) {
                    return x * besselJ(nu, alpha[m] * x / L) *
                           besselJ(nu, alpha[n] * x / L);
                },
                0.0, L, 96);
            if (m == n) {
                maxNorm = std::max(maxNorm, std::fabs(value));
            } else {
                maxOff = std::max(maxOff, std::fabs(value));
            }
        }
    }
    // Normalise the off-diagonal comparison against the largest diagonal term.
    p.orthogonalDefect = maxOff / std::max(1e-12, maxNorm);
    for (int n = 0; n < K; ++n)
        if (p.lambda[n] <= 0) p.positive = false;
    for (int n = 1; n < K; ++n)
        if (!(p.lambda[n] > p.lambda[n - 1])) p.ordered = false;
    return true;
}

bool buildLegendreSingular(int K, SingularProblem& p) {
    p.family = "legendre";
    p.equationText = "-((1 - x^2) y')' = lambda y on (-1, 1)";
    p.weightText = "1";
    p.endpointText = "both endpoints are regular singular; the eigenfunctions "
                     "stay bounded";
    for (int n = 0; n < K; ++n) p.lambda.push_back((double)n * (n + 1));
    std::ostringstream table;
    table << "n\tlambda_n\n";
    for (int n = 0; n < K; ++n) table << n << "\t" << numStr(p.lambda[n]) << "\n";
    p.lines.push_back(table.str());

    double worst = 0.0;
    for (int n = 0; n < K; ++n) {
        for (double x : {-0.7, -0.2, 0.35, 0.8}) {
            const double h = 1e-5;
            const double pm = legendreP(n, x - h), p0 = legendreP(n, x),
                         pp = legendreP(n, x + h);
            const double fluxP = (1 - (x + h / 2) * (x + h / 2)) * (pp - p0) / h;
            const double fluxM = (1 - (x - h / 2) * (x - h / 2)) * (p0 - pm) / h;
            const double residual = -(fluxP - fluxM) / h - p.lambda[n] * p0;
            worst = std::max(worst, std::fabs(residual));
        }
    }
    p.odeResidual = worst;

    double maxOff = 0.0, maxNorm = 0.0;
    for (int n = 0; n < K; ++n) {
        for (int m = 0; m < K; ++m) {
            const double value = integrateGL(
                [&](double x) { return legendreP(m, x) * legendreP(n, x); },
                -1.0, 1.0, 64);
            if (m == n) maxNorm = std::max(maxNorm, std::fabs(value));
            else maxOff = std::max(maxOff, std::fabs(value));
        }
    }
    p.orthogonalDefect = maxOff / std::max(1e-12, maxNorm);
    for (int n = 0; n < K; ++n)
        if (p.lambda[n] < 0) p.positive = false;
    for (int n = 1; n < K; ++n)
        if (!(p.lambda[n] > p.lambda[n - 1])) p.ordered = false;
    return true;
}

bool buildLaguerreSingular(int K, SingularProblem& p) {
    p.family = "laguerre";
    p.equationText = "-(x e^{-x} y')' = lambda e^{-x} y on (0, inf)";
    p.weightText = "e^{-x}";
    p.endpointText = "x = 0 is regular singular; the solution is bounded and "
                     "square integrable with respect to e^{-x}";
    for (int n = 0; n < K; ++n) p.lambda.push_back(n);
    std::ostringstream table;
    table << "n\tlambda_n\n";
    for (int n = 0; n < K; ++n) table << n << "\t" << numStr(p.lambda[n]) << "\n";
    p.lines.push_back(table.str());

    double worst = 0.0;
    for (int n = 0; n < K; ++n) {
        for (double x : {0.4, 1.1, 2.7}) {
            const double h = 1e-5;
            const double lm = laguerreL(n, x - h), l0 = laguerreL(n, x),
                         lp = laguerreL(n, x + h);
            const double d1 = (lp - lm) / (2 * h);
            const double d2 = (lp - 2 * l0 + lm) / (h * h);
            const double residual = x * d2 + (1 - x) * d1 + n * l0;
            worst = std::max(worst, std::fabs(residual));
        }
    }
    p.odeResidual = worst;

    // Gauss-Laguerre quadrature is exact for these polynomial products.
    std::vector<double> nodes, weights;
    gaussLaguerre(48, nodes, weights);
    double maxOff = 0.0, maxNorm = 0.0;
    for (int n = 0; n < K; ++n) {
        for (int m = 0; m < K; ++m) {
            double value = 0.0;
            for (size_t i = 0; i < nodes.size(); ++i)
                value += weights[i] * laguerreL(m, nodes[i]) * laguerreL(n, nodes[i]);
            if (m == n) maxNorm = std::max(maxNorm, std::fabs(value));
            else maxOff = std::max(maxOff, std::fabs(value));
        }
    }
    p.orthogonalDefect = maxOff / std::max(1e-12, maxNorm);
    for (int n = 0; n < K; ++n)
        if (p.lambda[n] < 0) p.positive = false;
    for (int n = 1; n < K; ++n)
        if (!(p.lambda[n] > p.lambda[n - 1])) p.ordered = false;
    return true;
}

bool buildCustomSingular(const Ex& pExp, const Ex& qExp, const Ex& wExp,
                         double a, double b, int N, int K,
                         SingularProblem& p, std::string& code) {
    p.family = "custom";
    p.equationText = "-(p y')' + q y = lambda w y on (a, b)";
    p.weightText = "w(x)";
    const double pa = eval1(pExp, a);
    const double pb = eval1(pExp, b);
    if (!std::isfinite(pa) || !std::isfinite(pb)) {
        code = "evaluation_failed";
        return false;
    }
    if (std::fabs(pa) > 1e-8 && std::fabs(pb) > 1e-8) {
        code = "not_singular_endpoint";
        return false;
    }
    p.endpointText = std::string(std::fabs(pa) <= 1e-8
                                     ? "x = a is singular (p(a) = 0)"
                                     : "x = b is singular (p(b) = 0)") +
                     "; the singular face contributes zero flux, a regular "
                     "endpoint keeps the Dirichlet truncation";

    // Cell-centred grid: the nodes sit at a + (k + 1/2) h and the faces at
    // a + k h.  The first face therefore lies exactly on the singular point
    // p(a) = 0, where the zero-flux condition is the natural one; a regular
    // endpoint keeps the Dirichlet truncation because the face value is zero.
    const double h = (b - a) / N;
    const int m = N;
    std::vector<double> diag(m), upper(m - 1), weight(m);
    std::vector<double> nodes(m);
    for (int k = 0; k < m; ++k) {
        const double x = a + (k + 0.5) * h;
        nodes[k] = x;
        const double pl = eval1(pExp, a + k * h);
        const double pr = eval1(pExp, a + (k + 1) * h);
        const double qi = eval1(qExp, x), wi = eval1(wExp, x);
        if (!std::isfinite(pl) || !std::isfinite(pr) || !std::isfinite(qi) ||
            !std::isfinite(wi) || wi <= 0 || pl < 0 || pr < 0) {
            code = "coefficient_not_admissible";
            return false;
        }
        diag[k] = (pl + pr) / (h * h) + qi;
        weight[k] = wi;
        if (k > 0) upper[k - 1] = -pl / (h * h);
    }
    std::vector<std::vector<double>> C(m, std::vector<double>(m, 0.0));
    for (int i = 0; i < m; ++i) {
        C[i][i] = diag[i] / weight[i];
        if (i + 1 < m) {
            const double v = upper[i] / std::sqrt(weight[i] * weight[i + 1]);
            C[i][i + 1] = C[i + 1][i] = v;
        }
    }
    std::vector<double> values;
    std::vector<std::vector<double>> vectors;
    symmetricEigen(C, values, vectors);
    std::vector<int> order(values.size());
    for (int i = 0; i < (int)order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(),
              [&](int i, int j) { return values[i] < values[j]; });
    std::ostringstream table;
    table << "n\tlambda_n\n";
    for (int k = 0; k < K && k < (int)order.size(); ++k) {
        p.lambda.push_back(values[order[k]]);
        table << k + 1 << "\t" << numStr(values[order[k]], 8) << "\n";
    }
    p.lines.push_back(table.str());
    for (double v : p.lambda)
        if (v < -1e-8) p.positive = false;
    for (int k = 1; k < (int)p.lambda.size(); ++k)
        if (!(p.lambda[k] > p.lambda[k - 1])) p.ordered = false;

    // Orthogonality of the discrete eigenvectors under the weight.
    std::vector<int> modes;
    for (int k = 0; k < K && k < (int)order.size(); ++k) modes.push_back(order[k]);
    double maxOff = 0.0, maxNorm = 0.0;
    for (size_t i = 0; i < modes.size(); ++i) {
        for (size_t j = 0; j < modes.size(); ++j) {
            double value = 0.0;
            for (int r = 0; r < m; ++r)
                value += weight[r] * vectors[r][modes[i]] * vectors[r][modes[j]] * h;
            if (i == j) maxNorm = std::max(maxNorm, std::fabs(value));
            else maxOff = std::max(maxOff, std::fabs(value));
        }
    }
    p.orthogonalDefect = maxOff / std::max(1e-12, maxNorm);

    // Discrete residual of A v = lambda W v for the first reported mode.
    if (!modes.empty()) {
        const int idx = modes.front();
        const double lambda = values[idx];
        double worst = 0.0, scale = 1.0;
        for (int r = 0; r < m; ++r) {
            double lhs = diag[r] * vectors[r][idx];
            if (r > 0) lhs += upper[r - 1] * vectors[r - 1][idx];
            if (r + 1 < m) lhs += upper[r] * vectors[r + 1][idx];
            worst = std::max(worst, std::fabs(lhs - lambda * weight[r] * vectors[r][idx]));
            scale = std::max(scale, std::fabs(lambda * weight[r] * vectors[r][idx]));
        }
        p.odeResidual = worst / scale;
    }
    return !p.lambda.empty();
}

Result sturmLiouvilleSingular(const TopicRequest& req) {
    Result r;
    baseId(r, "sturm_liouville_singular", "x");
    const std::string family = req.text("family", "bessel");
    int K = (int)std::lround(req.number("K", 4.0));
    K = std::clamp(K, 1, 12);
    SingularProblem p;
    std::string code;

    if (family == "bessel") {
        const double nu = req.number("nu", 0.0);
        const double L = req.number("L", 1.0);
        if (!std::isfinite(nu) || nu < 0 || !std::isfinite(L) || L <= 0) {
            failCode(r, "bad_arguments");
            return r;
        }
        if (!buildBesselSingular(nu, L, K, p)) {
            failCode(r, "bessel_zero_failed");
            return r;
        }
        r.functions.push_back("family: bessel, order nu = " + numStr(nu) +
                              ", interval (0, " + numStr(L) + "]");
    } else if (family == "legendre") {
        if (!buildLegendreSingular(K, p)) {
            failCode(r, "evaluation_failed");
            return r;
        }
        r.functions.push_back("family: legendre, interval (-1, 1)");
    } else if (family == "laguerre") {
        if (!buildLaguerreSingular(K, p)) {
            failCode(r, "evaluation_failed");
            return r;
        }
        r.functions.push_back("family: laguerre, interval (0, inf)");
    } else if (family == "custom") {
        const double a = req.number("a", 0.0);
        const double b = req.number("b", 1.0);
        int N = (int)std::lround(req.number("N", 80.0));
        N = std::clamp(N, 8, 300);
        if (!std::isfinite(a) || !std::isfinite(b) || b <= a) {
            failCode(r, "bad_interval");
            return r;
        }
        Ex pExp, qExp, wExp;
        try {
            pExp = parseExpr(req.text("p", "x*(1 - x)"));
            qExp = parseExpr(req.text("q", "0"));
            wExp = parseExpr(req.text("w", "1"));
        } catch (const std::exception& e) {
            r.errorDetail = std::string("custom coefficients: ") + e.what();
            failCode(r, "bad_expression");
            return r;
        }
        if (!buildCustomSingular(pExp, qExp, wExp, a, b, N, K, p, code)) {
            failCode(r, code);
            if (code == "not_singular_endpoint")
                r.errorDetail = "neither p(a) nor p(b) vanishes; use "
                                "sturm_liouville for a regular problem";
            return r;
        }
        r.functions.push_back("family: custom, interval (" + numStr(a) + ", " +
                              numStr(b) + "), N = " + std::to_string(N));
    } else {
        failCode(r, "unknown_family");
        return r;
    }

    r.functions.push_back("equation: " + p.equationText);
    r.functions.push_back("weight: " + p.weightText);
    r.functions.push_back("boundary behaviour: " + p.endpointText);
    for (const std::string& line : p.lines) r.functions.push_back(line);
    r.functions.push_back("weighted ODE residual = " + numStr(p.odeResidual, 3));
    r.functions.push_back("orthogonality defect = " + numStr(p.orthogonalDefect, 3));
    p.lambdaFirst = p.lambda.front();
    r.value = numEx(p.lambdaFirst);
    r.solution = Sym("eigenvalues: " + numStr(p.lambda.front()) + ", " +
                     (p.lambda.size() > 1 ? numStr(p.lambda[1]) : std::string("...")) +
                     ", ...");
    r.closedForm = false;
    r.numericOnly = true;
    r.numericNote = "The eigenvalues and eigenfunctions are evaluated "
                    "numerically; the ODE and orthogonality checks are discrete.";

    addStep(r, "step.title.identify_the_singular_endpoint",
            Sym(p.equationText + "  (" + p.endpointText + ")"));
    addStep(r, "step.title.write_the_special_function_eigenvalues",
            Sym(p.lines.empty() ? std::string() : p.lines.front()));
    addStep(r, "step.title.check_the_ode_and_orthogonality",
            Sym("max ODE residual = " + numStr(p.odeResidual, 3) +
                ", orthogonality defect = " + numStr(p.orthogonalDefect, 3)));
    addStep(r, "step.title.report_the_weight_and_boundary_behaviour",
            Sym("weight = " + p.weightText));

    addCheck(r, "check.the_eigenfunctions_satisfy_the_ode",
             p.family == "custom"
                 ? p.odeResidual < 1e-8
                 : p.odeResidual < 1e-4 * (1.0 + std::fabs(p.lambda.back())),
             "", "", "residual = " + numStr(p.odeResidual, 3));
    addCheck(r, "check.the_eigenfunctions_are_orthogonal",
             p.orthogonalDefect < 1e-6, "", "",
             "defect = " + numStr(p.orthogonalDefect, 3));
    addCheck(r, "check.the_eigenvalues_are_ordered", p.ordered && p.positive,
             "", "", std::to_string(p.lambda.size()) + " eigenvalues");
    return r;
}

// ---------------------------------------------------------------- Bessel series
struct BesselExpansion {
    double nu = 0.0;
    double a = 0.0;
    double L = 1.0;
    std::vector<double> alpha;
    std::vector<double> coeff;
    std::vector<double> norm2;
};

bool buildBesselExpansion(const Ex& f, double nu, double a, double L, int N,
                          BesselExpansion& out, std::string& code) {
    out.nu = nu;
    out.a = a;
    out.L = L;
    out.alpha.assign(N, 0.0);
    out.coeff.assign(N, 0.0);
    out.norm2.assign(N, 0.0);
    for (int n = 0; n < N; ++n) {
        const double alpha = besselZero(n + 1, nu);
        if (!std::isfinite(alpha)) {
            code = "bessel_zero_failed";
            return false;
        }
        out.alpha[n] = alpha;
        const double jn1 = besselJ(nu + 1, alpha);
        const double norm = 0.5 * L * L * jn1 * jn1;
        if (!(norm > 0) || !std::isfinite(norm)) {
            code = "bessel_normalisation_failed";
            return false;
        }
        out.norm2[n] = norm;
        const double integral = integrateGL(
            [&](double x) {
                return (x - a) * eval1(f, x) * besselJ(nu, alpha * (x - a) / L);
            },
            a, a + L, 96);
        out.coeff[n] = integral / norm;
        if (!std::isfinite(out.coeff[n])) {
            code = "evaluation_failed";
            return false;
        }
    }
    return true;
}

double besselSum(const BesselExpansion& e, double x, int terms) {
    double sum = 0.0;
    for (int n = 0; n < terms && n < (int)e.coeff.size(); ++n)
        sum += e.coeff[n] * besselJ(e.nu, e.alpha[n] * (x - e.a) / e.L);
    return sum;
}

Result besselSeries(const TopicRequest& req) {
    Result r;
    baseId(r, "bessel_series", "x");
    Ex f;
    try {
        f = parseExpr(req.text("f", "1 - x^2"));
    } catch (const std::exception& e) {
        r.errorDetail = std::string("f: ") + e.what();
        failCode(r, "bad_expression");
        return r;
    }
    const double nu = req.number("nu", 0.0);
    const double L = req.number("L", 1.0);
    const double x0 = req.number("x0", 0.5 * L);
    int N = (int)std::lround(req.number("N", 12.0));
    N = std::clamp(N, 1, 60);
    if (!std::isfinite(nu) || nu < 0 || !std::isfinite(L) || L <= 0) {
        failCode(r, "bad_arguments");
        return r;
    }
    BesselExpansion e;
    std::string code;
    if (!buildBesselExpansion(f, nu, 0.0, L, N, e, code)) {
        failCode(r, code);
        return r;
    }

    auto errors = [&](int terms, double& maxErr, double& l2Err) {
        maxErr = 0.0;
        for (int j = 0; j < 200; ++j) {
            const double x = (j + 0.5) * L / 200.0;
            const double fv = eval1(f, x);
            if (!std::isfinite(fv)) continue;
            maxErr = std::max(maxErr, std::fabs(fv - besselSum(e, x, terms)));
        }
        const double num = integrateGL(
            [&](double x) {
                const double diff = eval1(f, x) - besselSum(e, x, terms);
                return x * diff * diff;
            },
            0.0, L, 96);
        const double energy = integrateGL(
            [&](double x) {
                const double fv = eval1(f, x);
                return x * fv * fv;
            },
            0.0, L, 96);
        l2Err = std::sqrt(std::max(0.0, num) / std::max(energy, 1e-300));
        return energy;
    };
    double maxErr = 0.0, l2Err = 0.0, energy = 0.0;
    energy = errors(N, maxErr, l2Err);
    double maxHalf = 0.0, l2Half = 0.0;
    errors(std::max(1, N / 2), maxHalf, l2Half);

    double parsevalSum = 0.0;
    for (int n = 0; n < N; ++n) parsevalSum += e.coeff[n] * e.coeff[n] * e.norm2[n];
    // The truncated series misses the tail; Parseval then reads
    // energy = sum_{n<=N} c_n^2 ||phi_n||^2 + ||f - S_N||^2.
    const double tailEnergy = integrateGL(
        [&](double x) {
            const double diff = eval1(f, x) - besselSum(e, x, N);
            return x * diff * diff;
        },
        0.0, L, 96);
    const double parsevalDefect =
        std::fabs(parsevalSum - (energy - tailEnergy)) / std::max(energy, 1e-12);

    std::ostringstream table;
    table << "n\talpha_n\tc_n\n";
    for (int n = 0; n < N; ++n)
        table << n + 1 << "\t" << numStr(e.alpha[n], 8) << "\t"
              << numStr(e.coeff[n], 8) << "\n";

    std::ostringstream samples;
    samples << "x\tf(x)\tS_N(x)\n";
    for (int j = 1; j <= 5; ++j) {
        const double x = j * L / 6.0;
        samples << numStr(x) << "\t" << numStr(eval1(f, x)) << "\t"
                << numStr(besselSum(e, x, N)) << "\n";
    }
    const double boundary = besselSum(e, L, N);
    const double mid = besselSum(e, x0, N);

    r.functions.push_back("f(x) ~ sum_{n=1.." + std::to_string(N) +
                          "} c_n J_" + numStr(nu) + "(alpha_n x / L), L = " +
                          numStr(L));
    r.functions.push_back("c_n = 2/(L^2 J_{nu+1}(alpha_n)^2) integral_0^L "
                          "x f(x) J_nu(alpha_n x/L) dx");
    r.functions.push_back(table.str());
    r.functions.push_back("max |f - S_N| = " + numStr(maxErr, 3) +
                          ", relative L^2 error = " + numStr(l2Err, 3));
    r.functions.push_back("Parseval defect = " + numStr(parsevalDefect, 3));
    r.functions.push_back("S_N(" + numStr(x0) + ") = " + numStr(mid));
    r.functions.push_back("S_N(L) = " + numStr(boundary, 3));
    r.value = numEx(mid);
    r.solution = Sym("S_N(x) = sum c_n J_nu(alpha_n x/L), N = " +
                     std::to_string(N));
    r.closedForm = false;
    r.numericOnly = true;
    r.numericNote = "Coefficients come from Gauss-Legendre quadrature; the "
                    "Bessel values come from the convergent series.";

    addStep(r, "step.title.find_the_bessel_zeros",
            Sym("J_" + numStr(nu) + "(alpha_n) = 0, n = 1.." + std::to_string(N)));
    addStepRaw(r, "step.title.compute_the_coefficients", nullptr, table.str());
    addStep(r, "step.title.build_the_partial_sum",
            Sym("S_N(x) = sum c_n J_nu(alpha_n x/L)"));
    addStepRaw(r, "step.title.compare_with_the_function", nullptr, samples.str());
    addStep(r, "step.title.check_parseval_and_the_boundary",
            Sym("sum c_n^2 ||phi_n||^2 = integral x f^2, S_N(L) = 0"));

    addCheck(r, "check.the_partial_sum_vanishes_at_the_boundary",
             std::fabs(boundary) < 1e-8 * (1.0 + std::fabs(eval1(f, L))), "", "",
             "S_N(L) = " + numStr(boundary, 3));
    addCheck(r, "check.parseval_matches_the_energy", parsevalDefect < 1e-8, "",
             "", "relative defect = " + numStr(parsevalDefect, 3));
    addCheck(r, "check.the_error_does_not_grow_with_more_terms",
             l2Err <= l2Half * 1.05 + 1e-10, "", "",
             "L^2 error: N/2 = " + numStr(l2Half, 3) + ", N = " +
             numStr(l2Err, 3));
    return r;
}

// ---------------------------------------------------------------- 95
struct BasisSeries {
    int first = 0;
    std::vector<double> coeff;
    std::vector<double> norm2;
    std::function<double(int, double)> phi;
    std::function<double(double)> weight;

    double value(double x, int terms) const {
        double sum = 0.0;
        for (int k = 0; k < terms && k < (int)coeff.size(); ++k)
            sum += coeff[k] * phi(first + k, x);
        return sum;
    }
};

Result orthogonalSeries(const TopicRequest& req) {
    Result r;
    baseId(r, "orthogonal_series", "x");
    Ex f;
    try {
        f = parseExpr(req.text("f", "x"));
    } catch (const std::exception& e) {
        r.errorDetail = std::string("f: ") + e.what();
        failCode(r, "bad_expression");
        return r;
    }
    const std::string family = req.text("family", "legendre");
    const double a = req.number("a", -1.0);
    const double b = req.number("b", 1.0);
    const double x0 = req.number("x0", 0.5 * (a + b));
    int N = (int)std::lround(req.number("N", 8.0));
    N = std::clamp(N, 1, 40);
    if (!std::isfinite(a) || !std::isfinite(b) || b <= a) {
        failCode(r, "bad_interval");
        return r;
    }
    const double L = b - a;
    auto xOf = [&](double xi) { return 0.5 * (a + b) + 0.5 * L * xi; };
    auto xiOf = [&](double x) { return 2.0 * (x - a) / L - 1.0; };

    BasisSeries series;
    std::string familyText;
    std::string weightText;
    std::string normText;
    BesselExpansion besselExp;   // kept alive for the phi lambda below
    double besselNu = 0.0;

    if (family == "legendre") {
        familyText = "Legendre P_n";
        weightText = "w(x) = 1";
        normText = "||P_n||^2 = (b - a)/(2n + 1)";
        series.first = 0;
        series.coeff.assign(N + 1, 0.0);
        series.norm2.assign(N + 1, 0.0);
        series.weight = [](double) { return 1.0; };
        series.phi = [&](int n, double x) { return legendreP(n, xiOf(x)); };
        for (int n = 0; n <= N; ++n) {
            const double integral = integrateGL(
                [&](double xi) { return eval1(f, xOf(xi)) * legendreP(n, xi); },
                -1.0, 1.0, 64);
            series.coeff[n] = 0.5 * (2 * n + 1) * integral;
            series.norm2[n] = L / (2.0 * n + 1.0);
        }
    } else if (family == "chebyshev") {
        familyText = "Chebyshev T_n";
        weightText = "w(x) = 1/sqrt(1 - xi^2), xi = 2(x-a)/(b-a) - 1";
        normText = "||T_0||^2 = pi(b-a)/2, ||T_n||^2 = pi(b-a)/4 (n >= 1)";
        series.first = 0;
        series.coeff.assign(N + 1, 0.0);
        series.norm2.assign(N + 1, 0.0);
        series.weight = [&](double x) {
            const double xi = xiOf(x);
            const double denom = 1.0 - xi * xi;
            return denom > 0 ? 1.0 / std::sqrt(denom) : 0.0;
        };
        series.phi = [&](int n, double x) { return chebyshevT(n, xiOf(x)); };
        std::vector<double> nodes, weights;
        gaussChebyshev(64, nodes, weights);
        for (int n = 0; n <= N; ++n) {
            double integral = 0.0;
            for (size_t i = 0; i < nodes.size(); ++i)
                integral += weights[i] * eval1(f, xOf(nodes[i])) *
                            chebyshevT(n, nodes[i]);
            series.coeff[n] = (n == 0 ? 1.0 : 2.0) * integral / kPi;
            series.norm2[n] = n == 0 ? 0.5 * kPi * L : 0.25 * kPi * L;
        }
    } else if (family == "sine") {
        familyText = "sine series on (a, b)";
        weightText = "w(x) = 1";
        normText = "||sin(n pi (x-a)/L)||^2 = L/2";
        series.first = 1;
        series.coeff.assign(N, 0.0);
        series.norm2.assign(N, 0.0);
        series.weight = [](double) { return 1.0; };
        series.phi = [&](int n, double x) {
            return std::sin(n * kPi * (x - a) / L);
        };
        for (int n = 1; n <= N; ++n) {
            const double integral = integrateGL(
                [&](double x) { return eval1(f, x) * std::sin(n * kPi * (x - a) / L); },
                a, b, 64);
            series.coeff[n - 1] = 2.0 * integral / L;
            series.norm2[n - 1] = 0.5 * L;
        }
    } else if (family == "cosine") {
        familyText = "cosine series on (a, b)";
        weightText = "w(x) = 1";
        normText = "||1||^2 = L, ||cos(n pi (x-a)/L)||^2 = L/2";
        series.first = 0;
        series.coeff.assign(N + 1, 0.0);
        series.norm2.assign(N + 1, 0.0);
        series.weight = [](double) { return 1.0; };
        series.phi = [&](int n, double x) {
            return n == 0 ? 1.0 : std::cos(n * kPi * (x - a) / L);
        };
        for (int n = 0; n <= N; ++n) {
            const double integral = integrateGL(
                [&](double x) {
                    const double ph = n == 0 ? 1.0 : std::cos(n * kPi * (x - a) / L);
                    return eval1(f, x) * ph;
                },
                a, b, 64);
            series.coeff[n] = (n == 0 ? 1.0 : 2.0) * integral / L;
            series.norm2[n] = n == 0 ? L : 0.5 * L;
        }
    } else if (family == "bessel") {
        const double nu = req.number("nu", 0.0);
        if (!std::isfinite(nu) || nu < 0) {
            failCode(r, "bad_arguments");
            return r;
        }
        familyText = "Bessel J_nu on (a, b), nu = " + numStr(nu);
        weightText = "w(x) = x - a";
        normText = "||J_nu(alpha_n (x-a)/L)||_w^2 = L^2 J_{nu+1}(alpha_n)^2 / 2";
        std::string code;
        if (!buildBesselExpansion(f, nu, a, L, N, besselExp, code)) {
            failCode(r, code);
            return r;
        }
        besselNu = nu;
        series.first = 1;
        series.coeff = besselExp.coeff;
        series.norm2 = besselExp.norm2;
        series.weight = [&](double x) { return x - a; };
        series.phi = [&](int n, double x) {
            return besselJ(besselNu, besselExp.alpha[n - 1] * (x - a) / L);
        };
    } else {
        failCode(r, "unknown_family");
        return r;
    }

    // Quadrature adapted to the family: Gauss-Chebyshev handles the
    // endpoint singularity of the weight exactly; the others use Gauss-Legendre
    // with the weight applied explicitly.  `h` is the non-weight factor.
    auto quadWeighted = [&](const std::function<double(double)>& h) {
        if (family == "chebyshev") {
            std::vector<double> nodes, weights;
            gaussChebyshev(96, nodes, weights);
            double sum = 0.0;
            for (size_t i = 0; i < nodes.size(); ++i)
                sum += weights[i] * h(xOf(nodes[i]));
            return 0.5 * L * sum;
        }
        return integrateGL(
            [&](double x) { return series.weight(x) * h(x); }, a, b, 96);
    };

    auto errors = [&](int terms, double& maxErr, double& l2Err,
                      double& tailEnergy) {
        maxErr = 0.0;
        for (int j = 0; j < 200; ++j) {
            const double x = a + (j + 0.5) * L / 200.0;
            const double fv = eval1(f, x);
            if (!std::isfinite(fv)) continue;
            maxErr = std::max(maxErr, std::fabs(fv - series.value(x, terms)));
        }
        tailEnergy = quadWeighted([&](double x) {
            const double diff = eval1(f, x) - series.value(x, terms);
            return diff * diff;
        });
        const double energy = quadWeighted([&](double x) {
            const double fv = eval1(f, x);
            return fv * fv;
        });
        l2Err = std::sqrt(std::max(0.0, tailEnergy) / std::max(energy, 1e-300));
        return energy;
    };
    double maxErr = 0.0, l2Err = 0.0, tailEnergy = 0.0;
    const double energy = errors(N, maxErr, l2Err, tailEnergy);
    double maxHalf = 0.0, l2Half = 0.0, tailHalf = 0.0;
    errors(std::max(1, N / 2), maxHalf, l2Half, tailHalf);

    double parsevalSum = 0.0;
    for (size_t k = 0; k < series.coeff.size(); ++k)
        parsevalSum += series.coeff[k] * series.coeff[k] * series.norm2[k];
    const double parsevalDefect =
        std::fabs(parsevalSum - (energy - tailEnergy)) / std::max(energy, 1e-12);

    // Off-diagonal terms of the Gram matrix for the first few modes.
    const int gram = std::min(N, 6);
    double maxOff = 0.0, maxDiag = 0.0;
    for (int m = 0; m <= gram; ++m) {
        for (int n = 0; n <= gram; ++n) {
            if (series.first + m >= series.first + (int)series.coeff.size()) continue;
            if (series.first + n >= series.first + (int)series.coeff.size()) continue;
            const double value = quadWeighted([&](double x) {
                return series.phi(series.first + m, x) *
                       series.phi(series.first + n, x);
            });
            const double target =
                m == n ? series.norm2[m] : 0.0;
            if (m == n) maxDiag = std::max(maxDiag, std::fabs(target));
            else maxOff = std::max(maxOff, std::fabs(value));
        }
    }
    const double gramDefect = maxOff / std::max(1e-12, maxDiag);

    std::ostringstream table;
    table << "n\tc_n\t||phi_n||^2\n";
    for (size_t k = 0; k < series.coeff.size(); ++k)
        table << series.first + k << "\t" << numStr(series.coeff[k], 8) << "\t"
              << numStr(series.norm2[k], 8) << "\n";
    std::ostringstream samples;
    samples << "x\tf(x)\tS_N(x)\n";
    for (int j = 1; j <= 5; ++j) {
        const double x = a + j * L / 6.0;
        samples << numStr(x) << "\t" << numStr(eval1(f, x)) << "\t"
                << numStr(series.value(x, (int)series.coeff.size())) << "\n";
    }
    const double mid = series.value(x0, (int)series.coeff.size());

    r.functions.push_back("family: " + familyText + " on [" + numStr(a) + ", " +
                          numStr(b) + "], N = " + std::to_string(N));
    r.functions.push_back(weightText + "; " + normText);
    r.functions.push_back(table.str());
    r.functions.push_back("max |f - S_N| = " + numStr(maxErr, 3) +
                          ", relative L^2 error = " + numStr(l2Err, 3));
    r.functions.push_back("Parseval defect = " + numStr(parsevalDefect, 3));
    r.functions.push_back("Gram matrix off-diagonal defect = " +
                          numStr(gramDefect, 3));
    r.functions.push_back("S_N(" + numStr(x0) + ") = " + numStr(mid));
    r.value = numEx(mid);
    r.solution = Sym("S_N(x) = sum c_n phi_n(x), N = " + std::to_string(N));
    r.closedForm = false;
    r.numericOnly = true;
    r.numericNote = "Coefficients and norms come from Gaussian quadrature; the "
                    "Gram and Parseval checks are numerical.";

    addStep(r, "step.title.choose_the_orthogonal_family",
            Sym(familyText + "; " + weightText));
    addStepRaw(r, "step.title.compute_the_coefficients", nullptr, table.str());
    addStep(r, "step.title.build_the_partial_sum",
            Sym("S_N(x) = sum c_n phi_n(x)"));
    addStepRaw(r, "step.title.compare_with_the_function", nullptr, samples.str());
    addStep(r, "step.title.check_orthogonality_and_parseval",
            Sym("off-diagonal defect = " + numStr(gramDefect, 3) +
                ", Parseval defect = " + numStr(parsevalDefect, 3)));

    addCheck(r, "check.the_eigenfunctions_are_orthogonal", gramDefect < 1e-6,
             "", "", "defect = " + numStr(gramDefect, 3));
    addCheck(r, "check.parseval_matches_the_energy", parsevalDefect < 1e-6, "",
             "", "relative defect = " + numStr(parsevalDefect, 3));
    addCheck(r, "check.the_error_does_not_grow_with_more_terms",
             l2Err <= l2Half * 1.05 + 1e-10, "", "",
             "L^2 error: N/2 = " + numStr(l2Half, 3) + ", N = " +
             numStr(l2Err, 3));
    return r;
}

} // namespace

Result sturmLiouvilleSingularTopic(const TopicRequest& req) {
    return sturmLiouvilleSingular(req);
}
Result besselSeriesTopic(const TopicRequest& req) { return besselSeries(req); }
Result orthogonalSeriesTopic(const TopicRequest& req) {
    return orthogonalSeries(req);
}

} // namespace ode
