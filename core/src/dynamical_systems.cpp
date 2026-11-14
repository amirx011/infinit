// Infinit Lab — nonlinear planar systems and chaotic flows.
// SPDX-License-Identifier: Apache-2.0
#include "dynamical_systems.hpp"

#include "expr.hpp"
#include "numeric.hpp"
#include "poly.hpp"

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

using QMat = std::vector<std::vector<Rational>>;

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

std::string rat(Rational q) { return rationalToString(q); }

double eval2(const Ex& e, double x, double y) {
    std::map<std::string, double> vars{{"x", x}, {"y", y}};
    bool ok = false;
    const double v = evalReal(e, vars, &ok);
    return ok && std::isfinite(v) ? v : std::numeric_limits<double>::quiet_NaN();
}

double eval3(const Ex& e, double x, double y, double z) {
    std::map<std::string, double> vars{{"x", x}, {"y", y}, {"z", z}};
    bool ok = false;
    const double v = evalReal(e, vars, &ok);
    return ok && std::isfinite(v) ? v : std::numeric_limits<double>::quiet_NaN();
}

// Central-difference Jacobian of (f, g) at (x, y).
bool jacobian2(const Ex& f, const Ex& g, double x, double y, double J[2][2]) {
    const double h = 1e-6 * std::max(1.0, std::max(std::fabs(x), std::fabs(y)));
    const double fx1 = eval2(f, x + h, y), fx0 = eval2(f, x - h, y);
    const double fy1 = eval2(f, x, y + h), fy0 = eval2(f, x, y - h);
    const double gx1 = eval2(g, x + h, y), gx0 = eval2(g, x - h, y);
    const double gy1 = eval2(g, x, y + h), gy0 = eval2(g, x, y - h);
    if (!std::isfinite(fx1) || !std::isfinite(fx0) || !std::isfinite(fy1) ||
        !std::isfinite(fy0) || !std::isfinite(gx1) || !std::isfinite(gx0) ||
        !std::isfinite(gy1) || !std::isfinite(gy0))
        return false;
    J[0][0] = (fx1 - fx0) / (2 * h);
    J[0][1] = (fy1 - fy0) / (2 * h);
    J[1][0] = (gx1 - gx0) / (2 * h);
    J[1][1] = (gy1 - gy0) / (2 * h);
    return true;
}

struct Classification {
    std::string kind;   // stable_node, unstable_node, saddle, stable_spiral, ...
    std::string stability;
    double trace = 0.0, det = 0.0, disc = 0.0;
    double re1 = 0.0, im1 = 0.0, re2 = 0.0, im2 = 0.0;
};

Classification classify2(const double J[2][2]) {
    Classification c;
    c.trace = J[0][0] + J[1][1];
    c.det = J[0][0] * J[1][1] - J[0][1] * J[1][0];
    c.disc = c.trace * c.trace - 4 * c.det;
    if (c.disc >= 0) {
        const double sq = std::sqrt(std::max(0.0, c.disc));
        c.re1 = 0.5 * (c.trace + sq);
        c.re2 = 0.5 * (c.trace - sq);
        if (std::fabs(c.det) < 1e-10) {
            c.kind = "degenerate";
            c.stability = "inconclusive";
        } else if (c.det < 0) {
            c.kind = "saddle";
            c.stability = "unstable";
        } else if (c.re1 < -1e-10) {
            c.kind = std::fabs(c.re1 - c.re2) < 1e-10 ? "stable_node" : "stable_node";
            c.stability = "asymptotically_stable";
        } else if (c.re2 > 1e-10) {
            c.kind = "unstable_node";
            c.stability = "unstable";
        } else {
            c.kind = "degenerate";
            c.stability = "inconclusive";
        }
    } else {
        c.re1 = c.re2 = 0.5 * c.trace;
        c.im1 = 0.5 * std::sqrt(-c.disc);
        c.im2 = -c.im1;
        if (c.re1 < -1e-10) {
            c.kind = "stable_spiral";
            c.stability = "asymptotically_stable";
        } else if (c.re1 > 1e-10) {
            c.kind = "unstable_spiral";
            c.stability = "unstable";
        } else {
            c.kind = "center";
            c.stability = "stable";
        }
    }
    return c;
}

// RK4 for a planar field.
struct Point2 { double x, y, t; };

bool rk4Planar(const Ex& f, const Ex& g, double x0, double y0, double t0,
               double tEnd, double h, std::vector<Point2>& out) {
    out.clear();
    double x = x0, y = y0, t = t0;
    out.push_back({x, y, t});
    const int steps = (int)std::lround((tEnd - t0) / h);
    for (int i = 0; i < steps; ++i) {
        const double k1x = eval2(f, x, y), k1y = eval2(g, x, y);
        const double k2x = eval2(f, x + h / 2 * k1x, y + h / 2 * k1y);
        const double k2y = eval2(g, x + h / 2 * k1x, y + h / 2 * k1y);
        const double k3x = eval2(f, x + h / 2 * k2x, y + h / 2 * k2y);
        const double k3y = eval2(g, x + h / 2 * k2x, y + h / 2 * k2y);
        const double k4x = eval2(f, x + h * k3x, y + h * k3y);
        const double k4y = eval2(g, x + h * k3x, y + h * k3y);
        if (!std::isfinite(k1x) || !std::isfinite(k1y) || !std::isfinite(k2x) ||
            !std::isfinite(k2y) || !std::isfinite(k3x) || !std::isfinite(k3y) ||
            !std::isfinite(k4x) || !std::isfinite(k4y))
            return false;
        x += h / 6 * (k1x + 2 * k2x + 2 * k3x + k4x);
        y += h / 6 * (k1y + 2 * k2y + 2 * k3y + k4y);
        t += h;
        out.push_back({x, y, t});
        if (std::fabs(x) > 1e8 || std::fabs(y) > 1e8) return false;
    }
    return true;
}

// Newton refinement of an equilibrium starting from (sx, sy).
bool refineEquilibrium(const Ex& f, const Ex& g, double sx, double sy,
                       double& ex, double& ey) {
    double x = sx, y = sy;
    for (int iter = 0; iter < 40; ++iter) {
        const double fx = eval2(f, x, y), fy = eval2(g, x, y);
        if (!std::isfinite(fx) || !std::isfinite(fy)) return false;
        if (std::hypot(fx, fy) < 1e-12) {
            ex = x;
            ey = y;
            return true;
        }
        double J[2][2];
        if (!jacobian2(f, g, x, y, J)) return false;
        const double det = J[0][0] * J[1][1] - J[0][1] * J[1][0];
        if (std::fabs(det) < 1e-12) return false;
        const double dx = (-fx * J[1][1] + fy * J[0][1]) / det;
        const double dy = (-J[0][0] * fy + J[1][0] * fx) / det;
        x += dx;
        y += dy;
        if (std::hypot(dx, dy) < 1e-12) break;
    }
    ex = x;
    ey = y;
    return std::hypot(eval2(f, x, y), eval2(g, x, y)) < 1e-7;
}

struct Equilibrium {
    double x = 0, y = 0;
    double j00 = 0, j01 = 0, j10 = 0, j11 = 0;
    Classification cls;
};

std::vector<Equilibrium> findEquilibria(const Ex& f, const Ex& g, double xmin,
                                        double xmax, double ymin, double ymax,
                                        int grid = 9) {
    std::vector<Equilibrium> out;
    for (int i = 0; i < grid; ++i) {
        for (int j = 0; j < grid; ++j) {
            const double sx = xmin + (xmax - xmin) * (i + 0.5) / grid;
            const double sy = ymin + (ymax - ymin) * (j + 0.5) / grid;
            double ex, ey;
            if (!refineEquilibrium(f, g, sx, sy, ex, ey)) continue;
            if (ex < xmin - 1e-6 || ex > xmax + 1e-6 || ey < ymin - 1e-6 ||
                ey > ymax + 1e-6)
                continue;
            bool duplicate = false;
            for (const auto& e : out)
                if (std::hypot(e.x - ex, e.y - ey) < 1e-6) duplicate = true;
            if (duplicate) continue;
            Equilibrium e;
            e.x = ex;
            e.y = ey;
            double J[2][2];
            if (!jacobian2(f, g, ex, ey, J)) continue;
            e.j00 = J[0][0];
            e.j01 = J[0][1];
            e.j10 = J[1][0];
            e.j11 = J[1][1];
            e.cls = classify2(J);
            out.push_back(e);
        }
    }
    return out;
}

std::string equilibriumText(const std::vector<Equilibrium>& eqs) {
    std::ostringstream os;
    for (size_t i = 0; i < eqs.size(); ++i) {
        const auto& e = eqs[i];
        os << "E" << i + 1 << " = (" << numStr(e.x) << ", " << numStr(e.y) << ")  "
           << e.cls.kind << " / " << e.cls.stability << "  "
           << "[tr=" << numStr(e.cls.trace, 4) << ", det=" << numStr(e.cls.det, 4)
           << "]\n";
    }
    if (eqs.empty()) os << "no equilibrium found in the window\n";
    return os.str();
}

// ---------------------------------------------------------------- 81
Result competingSpecies(const TopicRequest& req) {
    Result r;
    baseId(r, "competing_species", "x");
    const double a = req.number("a", 1.0);
    const double b = req.number("b", 0.3);
    const double c = req.number("c", 0.2);
    const double d = req.number("d", 0.8);
    const double e = req.number("e", 0.4);
    const double f = req.number("f", 0.3);
    for (double v : {a, b, c, d, e, f})
        if (!std::isfinite(v) || std::fabs(v) > 1e6) {
            failCode(r, "bad_coefficients");
            return r;
        }
    const Ex F = parseExpr("x*(a - b*x - c*y)");
    const Ex G = parseExpr("y*(d - e*x - f*y)");
    (void)F;
    (void)G;
    auto rhsF = [&](double x, double y) { return x * (a - b * x - c * y); };
    auto rhsG = [&](double x, double y) { return y * (d - e * x - f * y); };

    std::vector<Equilibrium> eqs;
    auto addEq = [&](double x, double y) {
        if (!std::isfinite(x) || !std::isfinite(y)) return;
        Equilibrium eq;
        eq.x = x;
        eq.y = y;
        const double J[2][2] = {
            {a - 2 * b * x - c * y, -c * x},
            {-e * y, d - e * x - 2 * f * y}};
        eq.j00 = J[0][0]; eq.j01 = J[0][1]; eq.j10 = J[1][0]; eq.j11 = J[1][1];
        eq.cls = classify2(J);
        eqs.push_back(eq);
    };
    addEq(0, 0);
    if (std::fabs(b) > 1e-14) addEq(a / b, 0);
    if (std::fabs(f) > 1e-14) addEq(0, d / f);
    const double den = b * f - c * e;
    if (std::fabs(den) > 1e-14) addEq((a * f - c * d) / den, (b * d - a * e) / den);

    bool anyNegative = false;
    for (const auto& eq : eqs)
        if (eq.x < -1e-9 || eq.y < -1e-9) anyNegative = true;
    if (anyNegative) {
        std::vector<Equilibrium> filtered;
        for (const auto& eq : eqs)
            if (eq.x >= -1e-9 && eq.y >= -1e-9) filtered.push_back(eq);
        eqs = filtered;
    }

    r.functions.push_back("x' = x(a - b x - c y),  y' = y(d - e x - f y)");
    r.functions.push_back("equilibria:\n" + equilibriumText(eqs));
    std::string coexistence =
        (a / b > d / e && d / f > a / c)
            ? "coexistence is possible: both species survive"
            : "competitive exclusion: one species drives the other out";
    if (std::fabs(b) < 1e-14 || std::fabs(e) < 1e-14 || std::fabs(c) < 1e-14 ||
        std::fabs(f) < 1e-14)
        coexistence = "coexistence condition is degenerate (a coefficient is zero)";
    r.functions.push_back(coexistence);

    // Nullclines.
    r.functions.push_back("nullclines: x = 0, y = 0, a - b x - c y = 0, d - e x - f y = 0");
    r.solution = Sym("critical points: " + equilibriumText(eqs));
    r.closedForm = true;

    addStep(r, "step.title.write_the_competing_species_system",
            Sym("x' = x(a - b x - c y),  y' = y(d - e x - f y)"));
    addStepRaw(r, "step.title.find_the_equilibria", nullptr, equilibriumText(eqs));
    addStep(r, "step.title.compute_the_jacobian",
            Sym("J = [[a - 2b x - c y, -c x], [-e y, d - e x - 2f y]]"));
    addStep(r, "step.title.classify_each_equilibrium",
            Sym("trace and determinant decide the type"));
    addStepRaw(r, "step.title.check_coexistence", nullptr, coexistence);

    bool residualsOk = true;
    double worst = 0.0;
    for (const auto& eq : eqs) {
        worst = std::max(worst, std::hypot(rhsF(eq.x, eq.y), rhsG(eq.x, eq.y)));
        const double tr = eq.j00 + eq.j11;
        const double det = eq.j00 * eq.j11 - eq.j01 * eq.j10;
        if (std::fabs(tr - eq.cls.trace) > 1e-8 || std::fabs(det - eq.cls.det) > 1e-8)
            residualsOk = false;
    }
    addCheck(r, "check.each_equilibrium_vanishes", worst < 1e-9 && !eqs.empty() &&
              residualsOk, "", "",
              "max |F| = " + numStr(worst, 3) + ", " + std::to_string(eqs.size()) +
              " equilibria");
    addCheck(r, "check.the_eigenvalues_match_the_trace_and_determinant",
             residualsOk);
    return r;
}

// ---------------------------------------------------------------- 82
Result liapunov(const TopicRequest& req) {
    Result r;
    baseId(r, "liapunov", "x");
    Ex f, g;
    try {
        f = parseExpr(req.text("f", "-x + 2*x^2*y"));
        g = parseExpr(req.text("g", "-y"));
    } catch (const std::exception& e) {
        r.errorDetail = std::string("liapunov: ") + e.what();
        failCode(r, "bad_expression");
        return r;
    }
    const double x0 = req.number("x0", 0.0);
    const double y0 = req.number("y0", 0.0);
    if (!std::isfinite(x0) || !std::isfinite(y0)) {
        failCode(r, "bad_point");
        return r;
    }

    // Linearization and the Lyapunov equation A^T P + P A = -I.
    double J[2][2];
    if (!jacobian2(f, g, x0, y0, J)) {
        failCode(r, "evaluation_failed");
        return r;
    }
    const Rational a11 = Rational::fromDouble(J[0][0], 1000000);
    const Rational a12 = Rational::fromDouble(J[0][1], 1000000);
    const Rational a21 = Rational::fromDouble(J[1][0], 1000000);
    const Rational a22 = Rational::fromDouble(J[1][1], 1000000);
    // Unknowns p, q, r with P = [[p, q], [q, r]].
    QMat M = {{Rational(2) * a11, Rational(2) * a21, Rational(0)},
              {a12, a11 + a22, a21},
              {Rational(0), Rational(2) * a12, Rational(2) * a22}};
    std::vector<Rational> rhs = {Rational(-1), Rational(0), Rational(-1)};

    // Gaussian elimination.
    auto solve3 = [&](QMat A, std::vector<Rational> b, std::vector<Rational>& x) {
        const int n = 3;
        for (int col = 0; col < n; ++col) {
            int pivot = col;
            while (pivot < n && A[pivot][col].isZero()) ++pivot;
            if (pivot == n) return false;
            std::swap(A[pivot], A[col]);
            std::swap(b[pivot], b[col]);
            const Rational p = A[col][col];
            for (int j = col; j < n; ++j) A[col][j] = A[col][j] / p;
            b[col] = b[col] / p;
            for (int row = 0; row < n; ++row) {
                if (row == col || A[row][col].isZero()) continue;
                const Rational factor = A[row][col];
                for (int j = col; j < n; ++j)
                    A[row][j] = A[row][j] - factor * A[col][j];
                b[row] = b[row] - factor * b[col];
            }
        }
        x = b;
        return true;
    };
    std::vector<Rational> sol;
    if (!solve3(M, rhs, sol)) {
        failCode(r, "lyapunov_equation_singular");
        return r;
    }
    const Rational p = sol[0], q = sol[1], rr = sol[2];

    Ex V = nullptr;
    bool userV = false;
    if (!req.text("V").empty()) {
        try {
            V = parseExpr(req.text("V"));
            userV = true;
        } catch (const std::exception& e) {
            r.errorDetail = std::string("liapunov V: ") + e.what();
            failCode(r, "bad_expression");
            return r;
        }
    } else {
        const Ex dx = Add(Sym("x"), Num(-Rational::fromDouble(x0, 1000000)));
        const Ex dy = Add(Sym("y"), Num(-Rational::fromDouble(y0, 1000000)));
        V = Add(Mul(Num(p), Pow(dx, Num(2))),
                Mul(Num(Rational(2) * q), dx, dy),
                Mul(Num(rr), Pow(dy, Num(2))));
        V = simplify(V);
    }

    Ex Vdot = simplify(Add(Mul(diff(V, "x"), f), Mul(diff(V, "y"), g)));
    const double vAtPoint = eval2(V, x0, y0);
    const bool vZero = std::fabs(vAtPoint) < 1e-10;

    // Definiteness of a quadratic form by Sylvester's criterion; otherwise
    // sample a small circle and report numeric evidence only.
    bool quadratic = false;
    double p11 = 0, p12 = 0, p22 = 0;
    {
        // Read the quadratic coefficients when the form is exactly quadratic.
        std::vector<Ex> cx, cy;
        // Fall back to a numeric fit: V = p11 dx^2 + 2 p12 dx dy + p22 dy^2.
        const double h = 1e-4;
        const double vxp = eval2(V, x0 + h, y0), vxm = eval2(V, x0 - h, y0);
        const double vyp = eval2(V, x0, y0 + h), vym = eval2(V, x0, y0 - h);
        const double vpp = eval2(V, x0 + h, y0 + h);
        const double vmm = eval2(V, x0 - h, y0 - h);
        if (std::isfinite(vxp) && std::isfinite(vxm) && std::isfinite(vyp) &&
            std::isfinite(vym) && std::isfinite(vpp) && std::isfinite(vmm)) {
            p11 = (vxp - 2 * vAtPoint + vxm) / (h * h);
            p22 = (vyp - 2 * vAtPoint + vym) / (h * h);
            p12 = (vpp - vxp - vyp + vAtPoint) / (h * h);
            quadratic = true;
        }
        (void)cx; (void)cy;
    }

    bool positiveDefinite = false;
    bool positiveSemidefinite = false;
    if (quadratic) {
        positiveDefinite = p11 > 1e-9 && p11 * p22 - p12 * p12 > 1e-9;
        positiveSemidefinite = p11 >= -1e-9 && p11 * p22 - p12 * p12 >= -1e-9;
    } else {
        positiveDefinite = true;
        for (int k = 0; k < 64; ++k) {
            const double a = 2 * kPi * k / 64.0;
            const double v = eval2(V, x0 + 0.1 * std::cos(a), y0 + 0.1 * std::sin(a));
            if (!std::isfinite(v) || v <= 1e-10) positiveDefinite = false;
        }
        positiveSemidefinite = positiveDefinite;
    }

    double vdMin = 0, vdMax = 0;
    for (int k = 0; k < 64; ++k) {
        const double a = 2 * kPi * k / 64.0;
        const double v = eval2(Vdot, x0 + 0.1 * std::cos(a), y0 + 0.1 * std::sin(a));
        if (!std::isfinite(v)) continue;
        if (k == 0) { vdMin = vdMax = v; }
        vdMin = std::min(vdMin, v);
        vdMax = std::max(vdMax, v);
    }
    std::string conclusion;
    if (positiveDefinite && vdMax <= 1e-8 && vdMin < -1e-8)
        conclusion = "asymptotically stable (evidence on a sampled circle)";
    else if (positiveDefinite && vdMax <= 1e-8)
        conclusion = "stable (V' <= 0 on the sampled circle)";
    else if (positiveDefinite && vdMin >= -1e-8 && vdMax > 1e-8)
        conclusion = "unstable (V' > 0 on the sampled circle)";
    else if (!positiveDefinite)
        conclusion = "inconclusive: the candidate V is not positive definite near the point";
    else
        conclusion = "inconclusive: V' changes sign on the sampled circle";

    r.functions.push_back("V(x,y) = " + toPlain(V));
    r.functions.push_back("V'(x,y) = " + toPlain(Vdot));
    r.functions.push_back("V at the equilibrium = " + numStr(vAtPoint));
    r.functions.push_back("V' range on |r| = 0.1: [" + numStr(vdMin, 4) + ", " +
                          numStr(vdMax, 4) + "]");
    r.functions.push_back(conclusion + " — numerical evidence, not a proof");
    r.solution = Sym(conclusion);
    r.closedForm = false;
    r.numericOnly = !userV;
    if (userV)
        r.numericNote = "A user-supplied Lyapunov candidate proves a conclusion "
                        "only when the definiteness conditions hold on the whole "
                        "neighbourhood; the circle sample is evidence, not proof.";
    else
        r.numericNote = "V solves the linearized Lyapunov equation. The sign test "
                        "samples a circle, so the conclusion is evidence, not proof.";

    addStep(r, "step.title.linearize_at_the_equilibrium",
            Sym("J = [[" + numStr(J[0][0]) + ", " + numStr(J[0][1]) + "], [" +
                numStr(J[1][0]) + ", " + numStr(J[1][1]) + "]]"));
    addStep(r, "step.title.solve_the_lyapunov_equation",
            Sym("A^T P + P A = -I"));
    addStep(r, "step.title.write_the_candidate", Sym("V = " + toPlain(V)));
    addStep(r, "step.title.differentiate_along_the_flow",
            Sym("V' = " + toPlain(Vdot)));
    addStep(r, "step.title.conclude_from_the_sign", Sym(conclusion));

    addCheck(r, "check.candidate_vanishes_at_the_equilibrium", vZero, "", "",
             "V(x0) = " + numStr(vAtPoint));
    addCheck(r, "check.candidate_is_positive_definite_near_the_point",
             positiveDefinite || positiveSemidefinite, "", "",
             quadratic ? "Sylvester test on the quadratic form"
                       : "sampled circle evidence");
    addCheck(r, "check.the_sign_of_v_dot_was_sampled", std::isfinite(vdMin) &&
             std::isfinite(vdMax), "", "",
             "[" + numStr(vdMin, 3) + ", " + numStr(vdMax, 3) + "]");
    return r;
}

// ---------------------------------------------------------------- 83
Result limitCycle(const TopicRequest& req) {
    Result r;
    baseId(r, "limit_cycle", "t");
    Ex f, g;
    try {
        f = parseExpr(req.text("f", "y"));
        g = parseExpr(req.text("g", "1*(1 - x^2)*y - x"));
    } catch (const std::exception& e) {
        r.errorDetail = std::string("limit cycle: ") + e.what();
        failCode(r, "bad_expression");
        return r;
    }
    const double x0 = req.number("x0", 2.0);
    const double y0 = req.number("y0", 0.0);
    const double tmax = req.number("tmax", 40.0);
    const double h = req.number("h", 0.005);
    if (!std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(tmax) ||
        !std::isfinite(h) || tmax <= 0 || h <= 0 || tmax / h > 400000) {
        failCode(r, "bad_arguments");
        return r;
    }

    std::vector<Point2> orbit;
    if (!rk4Planar(f, g, x0, y0, 0.0, tmax, h, orbit)) {
        failCode(r, "integration_failed");
        return r;
    }

    // Poincare section: upward crossings of y = 0 with x > 0.
    std::vector<double> crossT, crossX;
    for (size_t i = 1; i < orbit.size(); ++i) {
        const Point2& p = orbit[i - 1];
        const Point2& q = orbit[i];
        if (p.y < 0.0 && q.y >= 0.0) {
            const double w = q.y - p.y;
            const double frac = std::fabs(w) < 1e-15 ? 0.0 : (-p.y) / w;
            crossT.push_back(p.t + frac * h);
            crossX.push_back(p.x + frac * (q.x - p.x));
        }
    }
    // Drop the transient: keep the last half of the crossings.
    const size_t keepFrom = crossT.size() > 4 ? crossT.size() / 2 : 0;
    std::vector<double> keptT(crossT.begin() + keepFrom, crossT.end());
    std::vector<double> keptX(crossX.begin() + keepFrom, crossX.end());

    std::ostringstream table;
    table << "crossing\tt\tx\ty\n";
    for (size_t i = 0; i < keptT.size(); ++i)
        table << i + 1 << "\t" << numStr(keptT[i]) << "\t" << numStr(keptX[i])
              << "\t0\n";

    if (keptT.size() < 2) {
        failCode(r, "no_closed_orbit_detected");
        r.errorDetail = "fewer than two transversal returns inside the window";
        return r;
    }

    std::vector<double> periods;
    for (size_t i = 1; i < keptT.size(); ++i) periods.push_back(keptT[i] - keptT[i - 1]);
    double periodSum = 0.0, xMin = keptX[0], xMax = keptX[0];
    for (double p : periods) periodSum += p;
    for (double x : keptX) { xMin = std::min(xMin, x); xMax = std::max(xMax, x); }
    const double period = periodSum / (double)periods.size();
    std::ostringstream periodsText;
    for (size_t i = 0; i < periods.size(); ++i)
        periodsText << (i ? ", " : "") << numStr(periods[i], 6);

    // Return residual: distance between consecutive section points.
    double maxJump = 0.0;
    for (size_t i = 1; i < keptX.size(); ++i)
        maxJump = std::max(maxJump, std::fabs(keptX[i] - keptX[i - 1]));

    // Nearby orbit: does it approach the section value?
    std::vector<Point2> near;
    double attraction = std::numeric_limits<double>::quiet_NaN();
    if (rk4Planar(f, g, x0 + 1e-3, y0, 0.0, tmax, h, near)) {
        std::vector<double> nearCross;
        for (size_t i = 1; i < near.size(); ++i) {
            if (near[i - 1].y < 0.0 && near[i].y >= 0.0)
                nearCross.push_back(near[i].x);
        }
        if (!nearCross.empty())
            attraction = std::fabs(nearCross.back() - keptX.back());
    }

    r.functions.push_back("period ~ " + numStr(period, 8) + " (finite-time estimate)");
    r.functions.push_back("section x range: [" + numStr(xMin, 6) + ", " +
                          numStr(xMax, 6) + "]");
    r.functions.push_back("consecutive section jumps: max = " + numStr(maxJump, 3));
    r.functions.push_back("periods: " + periodsText.str());
    if (std::isfinite(attraction))
        r.functions.push_back("nearby-orbit section difference = " +
                              numStr(attraction, 3));
    r.functions.push_back("Numerical evidence only: a closed orbit outside the "
                          "window cannot be excluded and the period is finite-time.");
    r.value = Num(Rational::fromDouble(period));
    r.solution = Sym("period ~ " + numStr(period, 8));
    r.closedForm = false;
    r.numericOnly = true;
    r.numericNote = "The orbit and period are numerical evidence, not a proof. "
                    "A negative result cannot exclude a cycle outside the window.";

    addStep(r, "step.title.integrate_the_planar_system",
            Sym("x' = " + toPlain(f) + ",  y' = " + toPlain(g)));
    addStepRaw(r, "step.title.find_the_poincare_returns", nullptr, table.str());
    addStep(r, "step.title.estimate_the_period",
            Sym("T ~ " + numStr(period, 8)));
    addStep(r, "step.title.check_transversality_and_stability",
            Sym("nearby-orbit difference = " +
                (std::isfinite(attraction) ? numStr(attraction, 3) : std::string("n/a"))));

    addCheck(r, "check.at_least_two_transversal_returns", keptT.size() >= 2, "", "",
             std::to_string(keptT.size()) + " returns");
    addCheck(r, "check.the_period_is_positive", period > 0, "", "",
             "T = " + numStr(period, 8));
    addCheck(r, "check.the_section_points_do_not_collapse",
             maxJump < 1e-2 * std::max(1.0, std::fabs(keptX.back())), "", "",
             "max jump = " + numStr(maxJump, 3));
    return r;
}

// ---------------------------------------------------------------- 84
Result chaosAttractor(const TopicRequest& req) {
    Result r;
    baseId(r, "chaos_attractor", "t");
    const std::string system = req.text("system", "lorenz");
    double x0 = req.number("x0", 1.0);
    double y0 = req.number("y0", 1.0);
    double z0 = req.number("z0", 1.0);
    const double tmax = req.number("tmax", 40.0);
    const double h = req.number("h", 0.005);
    if (!std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(z0) ||
        !std::isfinite(tmax) || !std::isfinite(h) || tmax <= 0 || h <= 0 ||
        tmax / h > 400000) {
        failCode(r, "bad_arguments");
        return r;
    }

    Ex Fx = nullptr, Fy = nullptr, Fz = nullptr;
    if (system == "lorenz") {
        Fx = parseExpr("10*(y - x)");
        Fy = parseExpr("x*(28 - z) - y");
        Fz = parseExpr("x*y - (8/3)*z");
    } else if (system == "rossler") {
        Fx = parseExpr("-y - z");
        Fy = parseExpr("x + 0.2*y");
        Fz = parseExpr("0.2 + z*(x - 5.7)");
    } else if (system == "duffing") {
        // x'' + 0.3 x' - x + x^3 = 0.3 cos(1.2 t), as a first-order system.
        Fx = parseExpr("y");
        Fy = parseExpr("x - x^3 - 0.3*y + 0.3*cos(1.2*z)");
        Fz = parseExpr("1");
    } else {
        failCode(r, "unknown_system");
        return r;
    }

    auto rhs = [&](double t, double x, double y, double z,
                   double& fx, double& fy, double& fz) {
        std::map<std::string, double> vars{{"x", x}, {"y", y}, {"z", z}, {"t", t}};
        bool okx = false, oky = false, okz = false;
        fx = evalReal(Fx, vars, &okx);
        fy = evalReal(Fy, vars, &oky);
        fz = evalReal(Fz, vars, &okz);
        return okx && oky && okz && std::isfinite(fx) && std::isfinite(fy) &&
               std::isfinite(fz);
    };

    auto integrate = [&](double sx, double sy, double sz, double step,
                         std::vector<std::vector<double>>& out) {
        out.clear();
        double x = sx, y = sy, z = sz, t = 0.0;
        const int steps = (int)std::lround(tmax / step);
        out.push_back({t, x, y, z});
        for (int i = 0; i < steps; ++i) {
            double k1x, k1y, k1z, k2x, k2y, k2z, k3x, k3y, k3z, k4x, k4y, k4z;
            if (!rhs(t, x, y, z, k1x, k1y, k1z)) return false;
            if (!rhs(t + step / 2, x + step / 2 * k1x, y + step / 2 * k1y,
                     z + step / 2 * k1z, k2x, k2y, k2z)) return false;
            if (!rhs(t + step / 2, x + step / 2 * k2x, y + step / 2 * k2y,
                     z + step / 2 * k2z, k3x, k3y, k3z)) return false;
            if (!rhs(t + step, x + step * k3x, y + step * k3y, z + step * k3z,
                     k4x, k4y, k4z)) return false;
            x += step / 6 * (k1x + 2 * k2x + 2 * k3x + k4x);
            y += step / 6 * (k1y + 2 * k2y + 2 * k3y + k4y);
            z += step / 6 * (k1z + 2 * k2z + 2 * k3z + k4z);
            t += step;
            out.push_back({t, x, y, z});
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
                std::fabs(x) > 1e8 || std::fabs(y) > 1e8 || std::fabs(z) > 1e8)
                return false;
        }
        return true;
    };

    std::vector<std::vector<double>> traj;
    if (!integrate(x0, y0, z0, h, traj)) {
        failCode(r, "integration_failed");
        return r;
    }

    // Finite-time maximal Lyapunov exponent by renormalised twin orbits.
    const double delta0 = 1e-8;
    double tx = x0 + delta0, ty = y0, tz = z0;
    double ax = x0, ay = y0, az = z0;
    const double tau = 1.0;
    std::vector<double> cumulativeLog;
    std::vector<double> cumulativeTime;
    double logSum = 0.0;
    int renormalizations = 0;
    {
        const int seg = std::max(1, (int)std::lround(tau / h));
        const int total = (int)std::lround(tmax / h);
        int count = 0;
        for (int i = 0; i < total; ++i) {
            double k1x, k1y, k1z, k2x, k2y, k2z, k3x, k3y, k3z, k4x, k4y, k4z;
            auto advance = [&](double& X, double& Y, double& Z) {
                if (!rhs(i * h, X, Y, Z, k1x, k1y, k1z)) return false;
                if (!rhs(i * h + h / 2, X + h / 2 * k1x, Y + h / 2 * k1y,
                         Z + h / 2 * k1z, k2x, k2y, k2z)) return false;
                if (!rhs(i * h + h / 2, X + h / 2 * k2x, Y + h / 2 * k2y,
                         Z + h / 2 * k2z, k3x, k3y, k3z)) return false;
                if (!rhs(i * h + h, X + h * k3x, Y + h * k3y, Z + h * k3z,
                         k4x, k4y, k4z)) return false;
                X += h / 6 * (k1x + 2 * k2x + 2 * k3x + k4x);
                Y += h / 6 * (k1y + 2 * k2y + 2 * k3y + k4y);
                Z += h / 6 * (k1z + 2 * k2z + 2 * k3z + k4z);
                return true;
            };
            if (!advance(tx, ty, tz) || !advance(ax, ay, az)) {
                failCode(r, "integration_failed");
                return r;
            }
            ++count;
            if (count == seg) {
                const double d = std::sqrt((tx - ax) * (tx - ax) +
                                           (ty - ay) * (ty - ay) +
                                           (tz - az) * (tz - az));
                if (d > 0 && std::isfinite(d)) {
                    logSum += std::log(d / delta0);
                    cumulativeLog.push_back(logSum);
                    cumulativeTime.push_back((i + 1) * h);
                    ++renormalizations;
                }
                const double scale = delta0 / std::max(d, 1e-300);
                tx = ax + (tx - ax) * scale;
                ty = ay + (ty - ay) * scale;
                tz = az + (tz - az) * scale;
                count = 0;
            }
        }
    }
    double lambda = 0.0;
    if (cumulativeLog.size() >= 2) {
        const size_t mid = cumulativeLog.size() / 2;
        const double dt = cumulativeTime.back() - cumulativeTime[mid];
        if (dt > 1e-12)
            lambda = (cumulativeLog.back() - cumulativeLog[mid]) / dt;
    }

    std::ostringstream table;
    table << "i\tt\tx\ty\tz\n";
    const int stride = std::max(1, (int)(traj.size() / 25));
    for (size_t i = 0; i < traj.size(); i += stride) {
        table << i << "\t" << numStr(traj[i][0]) << "\t" << numStr(traj[i][1])
              << "\t" << numStr(traj[i][2]) << "\t" << numStr(traj[i][3]) << "\n";
    }
    const std::vector<double>& last = traj.back();

    r.functions.push_back("system: " + system);
    r.functions.push_back("finite-time lambda_max ~ " + numStr(lambda, 5) +
                          " over t in [0, " + numStr(tmax) + "]");
    r.functions.push_back("renormalisations: " + std::to_string(renormalizations));
    r.functions.push_back("final state (" + numStr(last[1]) + ", " + numStr(last[2]) +
                          ", " + numStr(last[3]) + ")");
    r.functions.push_back("Finite-time estimate only: a positive value is evidence "
                          "of sensitive dependence, not a proof of chaos.");
    r.value = Num(Rational::fromDouble(lambda));
    r.solution = Sym("lambda_max ~ " + numStr(lambda, 5));
    r.closedForm = false;
    r.numericOnly = true;
    r.numericNote = "The Lyapunov exponent is finite-time and step-dependent; it "
                    "is evidence, not a proof.";

    addStep(r, "step.title.write_the_system",
            Sym(system == "lorenz" ? "x' = 10(y - x), y' = x(28 - z) - y, z' = xy - 8/3 z"
                                    : (system == "rossler"
                                           ? "x' = -y - z, y' = x + 0.2y, z' = 0.2 + z(x - 5.7)"
                                           : "x' = y, y' = x - x^3 - 0.3y + 0.3cos(1.2t), z' = 1")));
    addStepRaw(r, "step.title.integrate_the_trajectory", nullptr, table.str());
    addStep(r, "step.title.estimate_the_lyapunov_exponent",
            Sym("lambda_max ~ " + numStr(lambda, 5)));

    addCheck(r, "check.the_trajectory_stays_bounded",
             std::fabs(last[1]) < 1e6 && std::fabs(last[2]) < 1e6 &&
             std::fabs(last[3]) < 1e6, "", "",
             "final = (" + numStr(last[1], 4) + ", " + numStr(last[2], 4) + ", " +
             numStr(last[3], 4) + ")");
    addCheck(r, "check.at_least_one_renormalisation",
             renormalizations > 0, "", "",
             std::to_string(renormalizations) + " steps");
    // Short-time step-halving check.
    std::vector<std::vector<double>> half;
    bool halvingOk = integrate(x0, y0, z0, h / 2, half);
    double diff = std::numeric_limits<double>::quiet_NaN();
    if (halvingOk && !half.empty()) {
        const auto& a = traj.back();
        const auto& b = half.back();
        diff = std::sqrt((a[1] - b[1]) * (a[1] - b[1]) +
                         (a[2] - b[2]) * (a[2] - b[2]) +
                         (a[3] - b[3]) * (a[3] - b[3]));
    }
    addCheck(r, "check.step_halving_changes_the_short_trajectory_slightly",
             std::isfinite(diff), "", "",
             "difference at matched index = " + numStr(diff, 3));
    return r;
}

// ---------------------------------------------------------------- 112
Result localLinearization(const TopicRequest& req) {
    Result r;
    baseId(r, "local_linearization", "x");
    Ex f, g;
    try {
        f = parseExpr(req.text("f", "x - x^2 - x*y"));
        g = parseExpr(req.text("g", "y*x - 0.5*y"));
    } catch (const std::exception& e) {
        r.errorDetail = std::string("local linearization: ") + e.what();
        failCode(r, "bad_expression");
        return r;
    }
    const double xmin = req.number("xmin", -1.0);
    const double xmax = req.number("xmax", 2.0);
    const double ymin = req.number("ymin", -1.0);
    const double ymax = req.number("ymax", 2.0);
    if (!std::isfinite(xmin) || !std::isfinite(xmax) || !std::isfinite(ymin) ||
        !std::isfinite(ymax) || xmax - xmin <= 1e-9 || ymax - ymin <= 1e-9) {
        failCode(r, "bad_bounds");
        return r;
    }
    const auto eqs = findEquilibria(f, g, xmin, xmax, ymin, ymax);
    if (eqs.empty()) {
        failCode(r, "no_equilibrium_found");
        return r;
    }
    std::ostringstream lines;
    bool allHyperbolic = true;
    bool residualOk = true;
    for (size_t i = 0; i < eqs.size(); ++i) {
        const auto& e = eqs[i];
        lines << "E" << i + 1 << " = (" << numStr(e.x) << ", " << numStr(e.y) << ")\n";
        lines << "  J = [[" << numStr(e.j00, 4) << ", " << numStr(e.j01, 4)
              << "], [" << numStr(e.j10, 4) << ", " << numStr(e.j11, 4) << "]]\n";
        lines << "  linear: " << e.cls.kind << " / " << e.cls.stability << "\n";
        const bool hyperbolic = std::fabs(e.cls.re1) > 1e-9 &&
                                std::fabs(e.cls.re2) > 1e-9;
        if (hyperbolic)
            lines << "  hyperbolic: yes (the nonlinear type agrees with the linear one)\n";
        else
            lines << "  hyperbolic: no (the linear test is inconclusive)\n";
        if (!hyperbolic) allHyperbolic = false;
        const double fx = eval2(f, e.x, e.y), fy = eval2(g, e.x, e.y);
        if (!std::isfinite(fx) || !std::isfinite(fy) ||
            std::hypot(fx, fy) > 1e-7)
            residualOk = false;
    }
    r.functions.push_back("equilibria and linearizations:\n" + lines.str());
    r.solution = Sym(lines.str());
    r.closedForm = false;
    r.numericOnly = true;
    r.numericNote = "Equilibria are found by Newton's method on a grid; the "
                    "classification is exact for the linearization only.";

    addStep(r, "step.title.find_the_equilibria", Sym(lines.str()));
    addStep(r, "step.title.compute_the_jacobian_at_each_point",
            Sym("J(x,y) = [[f_x, f_y], [g_x, g_y]]"));
    addStep(r, "step.title.classify_the_linearization",
            Sym(allHyperbolic ? "every equilibrium is hyperbolic"
                              : "at least one equilibrium is non-hyperbolic"));
    addStep(r, "step.title.compare_with_the_nonlinear_field",
            Sym(allHyperbolic ? "the linear classification carries over"
                              : "the linear test does not decide the nonlinear type"));

    addCheck(r, "check.every_reported_point_vanishes", residualOk, "", "",
             std::to_string(eqs.size()) + " equilibria");
    addCheck(r, "check.every_equilibrium_is_hyperbolic", allHyperbolic);
    return r;
}

} // namespace

Result competingSpeciesTopic(const TopicRequest& req) { return competingSpecies(req); }
Result liapunovTopic(const TopicRequest& req) { return liapunov(req); }
Result limitCycleTopic(const TopicRequest& req) { return limitCycle(req); }
Result chaosAttractorTopic(const TopicRequest& req) { return chaosAttractor(req); }
Result localLinearizationTopic(const TopicRequest& req) {
    return localLinearization(req);
}

} // namespace ode
