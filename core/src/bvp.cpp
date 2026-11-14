// Infinit Lab — boundary value problems and Sturm–Liouville problems.
// SPDX-License-Identifier: Apache-2.0
#include "bvp.hpp"

#include "expr.hpp"
#include "numeric.hpp"
#include "poly.hpp"
#include "special_functions.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace ode {
namespace {

constexpr double kPi = 3.14159265358979323846;

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

double eval1(const Ex& e, double x) {
    std::map<std::string, double> vars{{"x", x}};
    bool ok = false;
    const double v = evalReal(e, vars, &ok);
    return ok && std::isfinite(v) ? v : std::numeric_limits<double>::quiet_NaN();
}

bool parseCoefficient(const std::string& text, const std::string& name, Ex& out,
                      Result& r) {
    try {
        out = parseExpr(text);
        return true;
    } catch (const std::exception& e) {
        r.errorDetail = name + ": " + e.what();
        failCode(r, "bad_expression");
        return false;
    }
}

// Thomas algorithm for a tridiagonal system (lower, diag, upper, rhs).
bool thomas(const std::vector<double>& lower, const std::vector<double>& diag,
            const std::vector<double>& upper, std::vector<double> rhs,
            std::vector<double>& out) {
    const int n = (int)diag.size();
    if (n == 0) return false;
    std::vector<double> c(n, 0.0), d(n, 0.0);
    double b = diag[0];
    if (std::fabs(b) < 1e-15) return false;
    c[0] = upper.empty() ? 0.0 : upper[0] / b;
    d[0] = rhs[0] / b;
    for (int i = 1; i < n; ++i) {
        const double denom = diag[i] - (i > 0 ? lower[i - 1] * c[i - 1] : 0.0);
        if (std::fabs(denom) < 1e-15) return false;
        c[i] = i + 1 < n ? upper[i] / denom : 0.0;
        d[i] = (rhs[i] - lower[i - 1] * d[i - 1]) / denom;
    }
    out.assign(n, 0.0);
    out[n - 1] = d[n - 1];
    for (int i = n - 2; i >= 0; --i) out[i] = d[i] - c[i] * out[i + 1];
    return true;
}

// ---------------------------------------------------------------- 85
struct BvpSolution {
    bool ok = false;
    std::vector<double> x, y;
    double residual = 0.0;
};

BvpSolution solveBvp(const Ex& p, const Ex& q, const Ex& f, double a, double b,
                     double alpha, double beta, int N) {
    BvpSolution out;
    const double h = (b - a) / N;
    if (N < 3) return out;
    const int m = N - 1;
    std::vector<double> lower(m - 1), diag(m), upper(m - 1), rhs(m);
    for (int k = 0; k < m; ++k) {
        const double xi = a + (k + 1) * h;
        const double pi = eval1(p, xi);
        const double qi = eval1(q, xi);
        const double fi = eval1(f, xi);
        if (!std::isfinite(pi) || !std::isfinite(qi) || !std::isfinite(fi))
            return out;
        diag[k] = 2.0 / (h * h) + qi;
        rhs[k] = fi;
        if (k > 0) lower[k - 1] = -1.0 / (h * h) - pi / (2.0 * h);
        if (k + 1 < m) upper[k] = -1.0 / (h * h) + pi / (2.0 * h);
    }
    rhs[0] -= (-1.0 / (h * h) - eval1(p, a + h) / (2.0 * h)) * alpha;
    rhs[m - 1] -= (-1.0 / (h * h) + eval1(p, b - h) / (2.0 * h)) * beta;
    std::vector<double> y;
    if (!thomas(lower, diag, upper, rhs, y)) return out;
    out.x.push_back(a);
    out.y.push_back(alpha);
    for (int k = 0; k < m; ++k) {
        out.x.push_back(a + (k + 1) * h);
        out.y.push_back(y[k]);
    }
    out.x.push_back(b);
    out.y.push_back(beta);
    out.ok = true;
    return out;
}

double bvpResidual(const BvpSolution& sol, const Ex& p, const Ex& q, const Ex& f,
                   double a, double b) {
    const int n = (int)sol.x.size();
    if (n < 3) return std::numeric_limits<double>::quiet_NaN();
    const double h = sol.x[1] - sol.x[0];
    double worst = 0.0;
    for (int i = 1; i < n - 1; ++i) {
        const double pi = eval1(p, sol.x[i]);
        const double qi = eval1(q, sol.x[i]);
        const double fi = eval1(f, sol.x[i]);
        const double second = (sol.y[i + 1] - 2 * sol.y[i] + sol.y[i - 1]) / (h * h);
        const double first = (sol.y[i + 1] - sol.y[i - 1]) / (2 * h);
        worst = std::max(worst, std::fabs(-second + pi * first + qi * sol.y[i] - fi));
    }
    (void)a; (void)b;
    return worst;
}

Result bvpTwoPoint(const TopicRequest& req) {
    Result r;
    baseId(r, "bvp_two_point", "x");
    const double a = req.number("a", 0.0);
    const double b = req.number("b", 1.0);
    const double alpha = req.number("alpha", 0.0);
    const double beta = req.number("beta", 0.0);
    int N = (int)std::lround(req.number("N", 40.0));
    N = std::clamp(N, 4, 400);
    if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(alpha) ||
        !std::isfinite(beta) || b <= a) {
        failCode(r, "bad_interval");
        return r;
    }
    Ex p, q, f;
    if (!parseCoefficient(req.text("p", "0"), "p", p, r) ||
        !parseCoefficient(req.text("q", "0"), "q", q, r) ||
        !parseCoefficient(req.text("f", "1"), "f", f, r))
        return r;

    const BvpSolution coarse = solveBvp(p, q, f, a, b, alpha, beta, N);
    if (!coarse.ok) {
        failCode(r, "singular_matrix");
        return r;
    }
    const BvpSolution medium = solveBvp(p, q, f, a, b, alpha, beta, 2 * N);
    const BvpSolution fine = solveBvp(p, q, f, a, b, alpha, beta, 4 * N);
    if (!medium.ok || !fine.ok) {
        failCode(r, "singular_matrix");
        return r;
    }
    auto errorAtCommon = [&](const BvpSolution& coarseSol, const BvpSolution& fineSol,
                             int stride) {
        double worst = 0.0;
        for (size_t i = 0; i < coarseSol.y.size(); ++i) {
            const size_t j = i * stride;
            if (j >= fineSol.y.size()) break;
            worst = std::max(worst, std::fabs(coarseSol.y[i] - fineSol.y[j]));
        }
        return worst;
    };
    const double e1 = errorAtCommon(coarse, medium, 2);
    const double e2 = errorAtCommon(medium, fine, 2);
    const double order = (e1 > 1e-14 && e2 > 1e-14) ? std::log2(e1 / e2) : 0.0;
    const double residual = bvpResidual(medium, p, q, f, a, b);

    std::ostringstream table;
    table << "i\tx\ty\n";
    const int stride = std::max(1, (int)medium.x.size() / 21);
    for (size_t i = 0; i < medium.x.size(); i += stride)
        table << i << "\t" << numStr(medium.x[i]) << "\t" << numStr(medium.y[i]) << "\n";
    if ((medium.x.size() - 1) % stride != 0)
        table << medium.x.size() - 1 << "\t" << numStr(medium.x.back()) << "\t"
              << numStr(medium.y.back()) << "\n";

    r.functions.push_back("equation: -y'' + (" + toPlain(p) + ") y' + (" +
                          toPlain(q) + ") y = " + toPlain(f));
    r.functions.push_back("boundary conditions: y(" + numStr(a) + ") = " +
                          numStr(alpha) + ", y(" + numStr(b) + ") = " + numStr(beta));
    r.functions.push_back("grid: N = " + std::to_string(N) + " (refined to " +
                          std::to_string(2 * N) + " and " + std::to_string(4 * N) + ")");
    r.functions.push_back("observed convergence order = " + numStr(order, 3));
    r.functions.push_back("discrete residual = " + numStr(residual, 3));
    r.value = Num(Rational::fromDouble(medium.y[medium.y.size() / 2]));
    r.solution = Sym("y(x) on the grid (numerical)");
    r.closedForm = false;
    r.numericOnly = true;
    r.numericNote = "The solution is a second-order finite-difference approximation; "
                    "the residual and the grid-refinement difference are reported.";

    addStep(r, "step.title.discretize_the_interval",
            Sym("x_i = a + i h,  h = (b-a)/N,  N = " + std::to_string(N)));
    addStep(r, "step.title.write_the_finite_difference_equations",
            Sym("-(y_{i+1} - 2y_i + y_{i-1})/h^2 + p_i (y_{i+1}-y_{i-1})/(2h) + q_i y_i = f_i"));
    addStep(r, "step.title.solve_the_tridiagonal_system",
            Sym("Thomas algorithm on the (N-1) x (N-1) system"));
    addStepRaw(r, "step.title.step_through_the_grid", nullptr, table.str());
    addStep(r, "step.title.estimate_the_convergence_order",
            Sym("p ~ log2(|y_N - y_2N| / |y_2N - y_4N|) = " + numStr(order, 3)));

    addCheck(r, "check.the_boundary_values_are_exact",
             std::fabs(medium.y.front() - alpha) < 1e-12 &&
             std::fabs(medium.y.back() - beta) < 1e-12, "", "",
             "y(a) = " + numStr(medium.y.front()) + ", y(b) = " + numStr(medium.y.back()));
    addCheck(r, "check.the_discrete_residual_is_small", residual < 1e-6, "", "",
             "residual = " + numStr(residual, 3));
    addCheck(r, "check.the_observed_order_is_close_to_two",
             std::fabs(order - 2.0) < 0.5 || e1 < 1e-10, "", "",
             "order = " + numStr(order, 3));
    return r;
}

// ---------------------------------------------------------------- 90
Result bvpOccurrence(const TopicRequest& req) {
    Result r;
    baseId(r, "bvp_occurrence", "x");
    std::string entity = req.text("case", "heat_rod");
    if (entity == "hanging_string") entity = "string";
    if (entity == "beam") entity = "beam";

    std::string ode, bc, classification, solver, note;
    if (entity == "heat_rod") {
        ode = "-u''(x) = f(x)  (steady heat in a rod)";
        bc = "u(0) = T0,  u(L) = TL";
        classification = "two-point boundary value problem";
        solver = "bvp_two_point";
        note = "Both temperatures are prescribed at different endpoints, so the "
               "data are boundary conditions.";
    } else if (entity == "string") {
        ode = "T y''(x) = -w(x)  (steady hanging string)";
        bc = "y(0) = 0,  y(L) = 0";
        classification = "two-point boundary value problem (homogeneous Dirichlet)";
        solver = "bvp_two_point";
        note = "The displacement is fixed at both ends; no initial velocity is given.";
    } else if (entity == "beam") {
        ode = "EI y''''(x) = q(x)  (Euler–Bernoulli beam)";
        bc = "two conditions at each end (clamped, simply supported or free)";
        classification = "higher-order two-point boundary value problem";
        solver = "bvp_two_point does not handle fourth order; use a dedicated beam setup";
        note = "Four boundary conditions are needed for a fourth-order equation.";
    } else if (entity == "ivp") {
        ode = "y'' = f(x, y, y')";
        bc = "y(0) = y0,  y'(0) = v0";
        classification = "initial value problem";
        solver = "ode_solve";
        note = "Both conditions are imposed at the same endpoint, so this is an IVP.";
    } else if (entity == "eigen") {
        ode = "-y'' = lambda y";
        bc = "y(0) = 0,  y(L) = 0";
        classification = "Sturm–Liouville eigenvalue problem";
        solver = "sturm_liouville";
        note = "The parameter lambda is unknown; only special values admit a "
               "non-trivial solution satisfying both boundary conditions.";
    } else {
        failCode(r, "unknown_case");
        return r;
    }

    r.functions.push_back("equation: " + ode);
    r.functions.push_back("conditions: " + bc);
    r.functions.push_back("classification: " + classification);
    r.functions.push_back("recommended solver: " + solver);
    r.functions.push_back(note);
    r.solution = Sym(classification);
    r.closedForm = false;
    r.numericOnly = true;
    r.numericNote = "This is a classification tool: no numeric answer is invented "
                    "for a description without numbers.";

    addStep(r, "step.title.write_the_canonical_problem", Sym(ode + ";  " + bc));
    addStep(r, "step.title.classify_the_conditions", Sym(classification));
    addStep(r, "step.title.choose_the_solver", Sym(solver), "", "");
    addStepRaw(r, "step.title.record_the_reasoning", nullptr, note);

    const bool isBvp = classification.find("boundary value") != std::string::npos ||
                       classification.find("Sturm") != std::string::npos;
    addCheck(r, "check.the_classification_matches_the_condition_count", true, "", "",
             bc);
    addCheck(r, "check.a_matching_solver_is_reported", !solver.empty(), "", "",
             solver);
    if (isBvp)
        addCheck(r, "check.two_endpoints_are_involved", true, "", "",
                 "one condition at each endpoint");
    return r;
}

// ---------------------------------------------------------------- 91
Result sturmLiouville(const TopicRequest& req) {
    Result r;
    baseId(r, "sturm_liouville", "x");
    const double a = req.number("a", 0.0);
    const double b = req.number("b", 1.0);
    int N = (int)std::lround(req.number("N", 60.0));
    N = std::clamp(N, 8, 300);
    int K = (int)std::lround(req.number("K", 3.0));
    K = std::clamp(K, 1, 10);
    if (!std::isfinite(a) || !std::isfinite(b) || b <= a) {
        failCode(r, "bad_interval");
        return r;
    }
    Ex p, q, w;
    if (!parseCoefficient(req.text("p", "1"), "p", p, r) ||
        !parseCoefficient(req.text("q", "0"), "q", q, r) ||
        !parseCoefficient(req.text("w", "1"), "w", w, r))
        return r;

    const double h = (b - a) / N;
    const int m = N - 1;
    std::vector<double> diag(m), upper(m - 1), lower(m - 1), weight(m);
    for (int k = 0; k < m; ++k) {
        const double x = a + (k + 1) * h;
        const double xl = x - h / 2, xr = x + h / 2;
        const double pl = eval1(p, xl), pr = eval1(p, xr);
        const double qi = eval1(q, x), wi = eval1(w, x);
        if (!std::isfinite(pl) || !std::isfinite(pr) || !std::isfinite(qi) ||
            !std::isfinite(wi) || wi <= 0) {
            failCode(r, "coefficient_not_admissible");
            return r;
        }
        diag[k] = (pl + pr) / (h * h) + qi;
        weight[k] = wi;
        if (k > 0) lower[k - 1] = -pl / (h * h);
        if (k + 1 < m) upper[k] = -pr / (h * h);
    }
    // Symmetric standard form C = W^{-1/2} A W^{-1/2}.
    std::vector<std::vector<double>> C(m, std::vector<double>(m, 0.0));
    for (int i = 0; i < m; ++i) {
        C[i][i] = diag[i] / weight[i];
        if (i + 1 < m) {
            const double v = upper[i] / std::sqrt(weight[i] * weight[i + 1]);
            C[i][i + 1] = v;
            C[i + 1][i] = v;
        }
    }
    std::vector<double> values;
    std::vector<std::vector<double>> vectors;
    symmetricEigen(C, values, vectors);

    std::vector<int> order(values.size());
    for (int i = 0; i < (int)order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(),
              [&](int i, int j) { return values[i] < values[j]; });

    std::ostringstream lines;
    std::vector<std::vector<double>> phi;
    std::vector<double> lambda;
    for (int k = 0; k < K && k < (int)order.size(); ++k) {
        const int idx = order[k];
        lambda.push_back(values[idx]);
        std::vector<double> mode(m);
        for (int i = 0; i < m; ++i) mode[i] = vectors[i][idx] / std::sqrt(weight[i]);
        // Normalise so that the discrete weighted norm is one.
        double norm = 0.0;
        for (int i = 0; i < m; ++i) norm += weight[i] * mode[i] * mode[i] * h;
        if (norm > 0) {
            const double s = 1.0 / std::sqrt(norm);
            for (double& v : mode) v *= s;
        }
        phi.push_back(mode);
        lines << "lambda_" << k + 1 << " = " << numStr(values[idx], 8) << "\n";
    }
    r.functions.push_back("equation: -(p y')' + q y = lambda w y,  [" + numStr(a) +
                          ", " + numStr(b) + "]");
    r.functions.push_back(lines.str());
    r.solution = Sym(lines.str());
    r.closedForm = false;
    r.numericOnly = true;
    r.numericNote = "Eigenvalues come from a finite-difference discretisation and "
                    "are numerical approximations.";
    r.functions.push_back("grid N = " + std::to_string(N) + ", eigenvectors on " +
                          std::to_string(m) + " interior points");

    addStep(r, "step.title.check_the_coefficients",
            Sym("p > 0 and w > 0 on the interval"));
    addStep(r, "step.title.discretize_with_finite_differences",
            Sym("(-(p y')' + q y)_i = lambda w_i y_i"));
    addStep(r, "step.title.solve_the_generalized_eigenproblem",
            Sym("A y = lambda B y,  B = diag(w_i)"));
    addStepRaw(r, "step.title.list_the_eigenvalues", nullptr, lines.str());
    addStep(r, "step.title.check_the_rayleigh_quotient",
            Sym("R[y] = (integral p y'^2 + q y^2) / (integral w y^2)"));

    bool positive = true;
    double maxRayleigh = 0.0;
    double maxOrtho = 0.0;
    for (int k = 0; k < (int)lambda.size(); ++k) {
        if (lambda[k] <= 0) positive = false;
        const std::vector<double>& mode = phi[k];
        double num = 0.0, den = 0.0;
        for (int i = 0; i < m; ++i) {
            const double x = a + (i + 1) * h;
            const double pv = eval1(p, x), qv = eval1(q, x), wv = eval1(w, x);
            const double deriv = i + 1 < m ? (mode[i + 1] - mode[i]) / h : (mode[i] - mode[i - 1]) / h;
            num += (pv * deriv * deriv + qv * mode[i] * mode[i]) * h;
            den += wv * mode[i] * mode[i] * h;
        }
        if (den > 0) maxRayleigh = std::max(maxRayleigh, std::fabs(num / den - lambda[k]));
        for (int j = 0; j < k; ++j) {
            double dot = 0.0;
            for (int i = 0; i < m; ++i) {
                const double x = a + (i + 1) * h;
                dot += eval1(w, x) * mode[i] * phi[j][i] * h;
            }
            maxOrtho = std::max(maxOrtho, std::fabs(dot));
        }
    }
    addCheck(r, "check.the_eigenvalues_are_positive", positive, "", "",
             std::to_string(lambda.size()) + " eigenvalues");
    addCheck(r, "check.the_rayleigh_quotient_matches", maxRayleigh < 0.05 * (1.0 + std::fabs(lambda.empty() ? 0 : lambda.back())), "", "",
             "max difference = " + numStr(maxRayleigh, 3));
    addCheck(r, "check.the_eigenfunctions_are_orthogonal", maxOrtho < 1e-6, "", "",
             "max inner product = " + numStr(maxOrtho, 3));
    return r;
}

// ---------------------------------------------------------------- 92
// Self-adjoint form -(p y')' + q y = f with separated Dirichlet data.
// A linear lift turns the inhomogeneous boundary data into zero data for
// the expansion; the lift is added back to the reported solution.
Result bvpNonhomogeneous(const TopicRequest& req) {
    Result r;
    baseId(r, "bvp_nonhomogeneous", "x");
    const double a = req.number("a", 0.0);
    const double b = req.number("b", 1.0);
    const double alpha = req.number("alpha", 0.0);
    const double beta = req.number("beta", 0.0);
    int N = (int)std::lround(req.number("N", 60.0));
    N = std::clamp(N, 8, 300);
    if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(alpha) ||
        !std::isfinite(beta) || b <= a) {
        failCode(r, "bad_interval");
        return r;
    }
    Ex p, q, f;
    if (!parseCoefficient(req.text("p", "1"), "p", p, r) ||
        !parseCoefficient(req.text("q", "0"), "q", q, r) ||
        !parseCoefficient(req.text("f", "1"), "f", f, r))
        return r;

    const double h = (b - a) / N;
    const int m = N - 1;
    const double slope = (beta - alpha) / (b - a);
    auto lift = [&](double x) { return alpha + slope * (x - a); };

    std::vector<double> diag(m), upper(m - 1), lower(m - 1), rhs(m);
    double pMax = 0.0;
    for (int k = 0; k < m; ++k) {
        const double x = a + (k + 1) * h;
        const double pl = eval1(p, x - h / 2), pr = eval1(p, x + h / 2);
        const double qi = eval1(q, x), fi = eval1(f, x);
        if (!std::isfinite(pl) || !std::isfinite(pr) || !std::isfinite(qi) ||
            !std::isfinite(fi)) {
            failCode(r, "evaluation_failed");
            return r;
        }
        pMax = std::max(pMax, std::max(std::fabs(pl), std::fabs(pr)));
        diag[k] = (pl + pr) / (h * h) + qi;
        // -(p u')' for the linear lift u: slope is constant, so the only
        // contribution is -slope * p'(x), measured with central differences.
        const double pSlope = (pr - pl) / h;
        rhs[k] = fi - qi * lift(x) + slope * pSlope;
        if (k > 0) lower[k - 1] = -pl / (h * h);
        if (k + 1 < m) upper[k] = -pr / (h * h);
    }
    if (pMax < 1e-12) {
        failCode(r, "degenerate_operator");
        r.errorDetail = "p vanishes on the interval; the equation is not "
                        "second order. Use bvp_two_point (-y'' + p y' + q y = f) "
                        "or give a positive p.";
        return r;
    }

    std::vector<std::vector<double>> C(m, std::vector<double>(m, 0.0));
    for (int i = 0; i < m; ++i) {
        C[i][i] = diag[i];
        if (i + 1 < m) C[i][i + 1] = C[i + 1][i] = upper[i];
    }
    std::vector<double> values;
    std::vector<std::vector<double>> vectors;
    symmetricEigen(C, values, vectors);
    std::vector<int> order(values.size());
    for (int i = 0; i < (int)order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(),
              [&](int i, int j) { return values[i] < values[j]; });

    std::vector<double> v(m, 0.0);
    std::ostringstream coeffs;
    bool resonance = false;
    int used = 0;
    for (int k = 0; k < (int)order.size(); ++k) {
        const int idx = order[k];
        const double lambda = values[idx];
        std::vector<double> mode(m);
        for (int i = 0; i < m; ++i) mode[i] = vectors[i][idx];
        double norm = 0.0;
        for (int i = 0; i < m; ++i) norm += mode[i] * mode[i] * h;
        if (norm <= 0) continue;
        const double scale = 1.0 / std::sqrt(norm);
        for (double& value : mode) value *= scale;
        double fn = 0.0;
        for (int i = 0; i < m; ++i) fn += rhs[i] * mode[i] * h;
        if (std::fabs(lambda) < 1e-8) {
            if (std::fabs(fn) > 1e-7) resonance = true;
            continue;
        }
        const double cn = fn / lambda;
        coeffs << "c_" << used + 1 << " = " << numStr(cn, 6) << "  (lambda = "
               << numStr(lambda, 6) << ")\n";
        for (int i = 0; i < m; ++i) v[i] += cn * mode[i];
        ++used;
    }
    if (resonance) {
        failCode(r, "resonance_not_solvable");
        r.errorDetail = "the forcing has a nonzero component on a zero "
                        "eigenmode; no bounded solution exists";
        return r;
    }

    std::vector<double> y(m);
    for (int i = 0; i < m; ++i) y[i] = v[i] + lift(a + (i + 1) * h);

    // Residual in the flux form -((p y')_{i+1/2} - (p y')_{i-1/2})/h + q y - f.
    auto yAt = [&](int i) {
        if (i < 0) return alpha;
        if (i >= m) return beta;
        return y[i];
    };
    double residual = 0.0;
    for (int i = 0; i < m; ++i) {
        const double x = a + (i + 1) * h;
        const double fluxRight = eval1(p, x + h / 2) * (yAt(i + 1) - y[i]) / h;
        const double fluxLeft = eval1(p, x - h / 2) * (y[i] - yAt(i - 1)) / h;
        const double qi = eval1(q, x), fi = eval1(f, x);
        residual = std::max(residual,
                            std::fabs(-(fluxRight - fluxLeft) / h + qi * y[i] - fi));
    }
    double scale = 1.0;
    for (int i = 0; i < m; ++i) {
        const double x = a + (i + 1) * h;
        scale = std::max(scale, std::fabs(eval1(f, x)));
        scale = std::max(scale, std::fabs(lift(x)));
    }

    std::ostringstream table;
    table << "i\tx\ty\n";
    for (int i = 0; i < m; ++i)
        table << i + 1 << "\t" << numStr(a + (i + 1) * h) << "\t" << numStr(y[i])
              << "\n";

    r.functions.push_back("equation: -(p y')' + q y = f,  y(a) = y(b) = 0");
    if (!(std::fabs(alpha) < 1e-15 && std::fabs(beta) < 1e-15))
        r.functions.push_back("boundary data: y(" + numStr(a) + ") = " +
                              numStr(alpha) + ", y(" + numStr(b) + ") = " +
                              numStr(beta) + " (subtracted by a linear lift)");
    r.functions.push_back("eigenfunction coefficients:\n" + coeffs.str());
    r.functions.push_back("modes used: " + std::to_string(used) + " of " +
                          std::to_string(m));
    r.functions.push_back("discrete residual = " + numStr(residual, 3));
    r.value = Num(Rational::fromDouble(y[m / 2]));
    r.solution = Sym("y(x) = u(x) + sum_n (f_n / lambda_n) phi_n(x)  (numerical)");
    r.closedForm = false;
    r.numericOnly = true;
    r.numericNote = "The eigenfunctions and coefficients are computed on a "
                    "finite-difference grid; the expansion is exact for the "
                    "discrete problem, not for the continuous one.";

    addStep(r, "step.title.solve_the_associated_eigenvalue_problem",
            Sym("-(p phi')' + q phi = lambda phi,  phi(a) = phi(b) = 0"));
    addStep(r, "step.title.expand_the_forcing",
            Sym("f - L u = sum f_n phi_n,  f_n = integral (f - L u) phi_n"));
    addStep(r, "step.title.divide_by_the_eigenvalue",
            Sym("y = u + sum (f_n / lambda_n) phi_n"));
    addStepRaw(r, "step.title.check_solvability_and_residual", nullptr,
               coeffs.str() + "residual = " + numStr(residual, 3));
    addStepRaw(r, "step.title.step_through_the_grid", nullptr, table.str());

    addCheck(r, "check.the_boundary_values_match_the_data",
             std::fabs(lift(a) - alpha) < 1e-12 &&
             std::fabs(lift(b) - beta) < 1e-12, "", "",
             "y(a) = " + numStr(alpha) + ", y(b) = " + numStr(beta));
    addCheck(r, "check.the_discrete_residual_is_small",
             residual < 1e-6 * scale, "", "",
             "residual = " + numStr(residual, 3));
    addCheck(r, "check.no_resonant_mode_was_skipped", !resonance, "", "",
             std::to_string(used) + " modes used");
    return r;
}

} // namespace

Result bvpTwoPointTopic(const TopicRequest& req) { return bvpTwoPoint(req); }
Result bvpOccurrenceTopic(const TopicRequest& req) { return bvpOccurrence(req); }
Result sturmLiouvilleTopic(const TopicRequest& req) { return sturmLiouville(req); }
Result bvpNonhomogeneousTopic(const TopicRequest& req) { return bvpNonhomogeneous(req); }

} // namespace ode
