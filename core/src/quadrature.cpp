// Infinit Lab — deterministic numerical quadrature for definite integrals.
//
// Gauss–Kronrod 7/15 rule with adaptive bisection for one variable, and nested
// quadrature for double / triple integrals (the outer panel is adaptive, the
// inner panels use the fixed 15-point rule).
// SPDX-License-Identifier: Apache-2.0
#include "calculus_ext.hpp"
#include "numeric.hpp"

#include <cmath>
#include <map>

namespace ode {
namespace {

const double kXgk[8] = {0.9914553711208126, 0.9491079123427585, 0.8648644233597691,
                        0.7415311855993945, 0.5860872354676911, 0.4058451513773972,
                        0.2077849550078985, 0.0};
const double kWgk[8] = {0.02293532201052922, 0.06309209262997855, 0.1047900103222502,
                        0.1406532597155259, 0.1690047266392679, 0.1903505780647854,
                        0.2044329400752989, 0.2094821410847278};
const double kWg[4] = {0.1294849661688697, 0.2797053914892767, 0.3818300505051189,
                       0.4179591836734694};

// One 15-point Kronrod panel with the embedded 7-point Gauss estimate.
void kronrod15(const std::function<double(double)>& f, double a, double b, double& result,
               double& error) {
    const double centre = 0.5 * (a + b);
    const double half = 0.5 * (b - a);
    const double fc = f(centre);
    if (!std::isfinite(fc)) {
        result = std::nan("");
        error = std::nan("");
        return;
    }
    // The Kronrod abscissae alternate: even-numbered entries are Kronrod-only
    // nodes, odd-numbered ones are the embedded 7-point Gauss nodes (1-based).
    double resGauss = kWg[3] * fc;
    double resKronrod = kWgk[7] * fc;
    for (int i = 0; i < 7; ++i) {
        const double dx = kXgk[i] * half;
        const double f1 = f(centre - dx);
        const double f2 = f(centre + dx);
        if (!std::isfinite(f1) || !std::isfinite(f2)) {
            result = std::nan("");
            error = std::nan("");
            return;
        }
        const double sum = f1 + f2;
        resKronrod += kWgk[i] * sum;
        if (i == 1) resGauss += kWg[0] * sum;
        else if (i == 3) resGauss += kWg[1] * sum;
        else if (i == 5) resGauss += kWg[2] * sum;
    }
    result = resKronrod * half;
    error = std::fabs(resKronrod - resGauss) * half;
}

// `budget` is the absolute error allowed for this panel; children share it.
double adapt(const std::function<double(double)>& f, double a, double b, double budget, int depth,
             int maxDepth) {
    double res = 0, err = 0;
    kronrod15(f, a, b, res, err);
    if (!std::isfinite(res)) return std::nan("");
    if (err <= budget || err <= 1e-15 * std::fabs(res) || depth >= maxDepth ||
        b - a == 0.0)
        return res;
    const double mid = 0.5 * (a + b);
    const double left = adapt(f, a, mid, budget * 0.5, depth + 1, maxDepth);
    const double right = adapt(f, mid, b, budget * 0.5, depth + 1, maxDepth);
    return left + right;
}

} // namespace

double quadratureAdaptive(const std::function<double(double)>& f, double a, double b, double tol,
                          int maxDepth) {
    if (!std::isfinite(a) || !std::isfinite(b)) return std::nan("");
    if (a == b) return 0.0;
    if (b < a) return -quadratureAdaptive(f, b, a, tol, maxDepth);
    double res = 0, err = 0;
    kronrod15(f, a, b, res, err);
    if (!std::isfinite(res)) return std::nan("");
    const double budget = std::max(tol * std::max(1.0, std::fabs(res)), 1e-14);
    return adapt(f, a, b, budget, 0, maxDepth);
}

double numericIntegral(const Ex& f, const std::vector<std::string>& vars,
                       const std::vector<double>& lower, const std::vector<double>& upper) {
    if (vars.size() != lower.size() || vars.size() != upper.size() || vars.empty())
        return std::nan("");
    std::map<std::string, double> env;
    // Safety net: a pathological integrand must not freeze the UI.
    size_t evaluations = 0;
    const size_t maxEvaluations = 2000000;
    std::function<double(size_t)> layer = [&](size_t k) -> double {
        if (k == vars.size()) {
            if (++evaluations > maxEvaluations) return std::nan("");
            bool ok = false;
            const double v = evalReal(f, env, &ok);
            return ok ? v : std::nan("");
        }
        const double lo = lower[k];
        const double hi = upper[k];
        auto inner = [&](double t) -> double {
            env[vars[k]] = t;
            return layer(k + 1);
        };
        // Every layer is adaptive so the whole nest is error-controlled.
        return quadratureAdaptive(inner, lo, hi, 1e-10);
    };
    return layer(0);
}

} // namespace ode
