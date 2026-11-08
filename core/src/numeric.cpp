// Infinit Lab — numeric certificates
#include "numeric.hpp"
#include "numeric_c.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include <set>

namespace ode {

struct Compiler {
    std::vector<OdeInstr> code;
    std::map<std::string, int> vars;
    bool ok = true;

    int varIndex(const std::string& name) {
        auto it = vars.find(name);
        if (it != vars.end()) return it->second;
        int idx = (int)vars.size();
        vars[name] = idx;
        return idx;
    }

    void emit(int op, double val = 0.0, int idx = 0) { code.push_back({op, val, idx}); }

    void compile(const Ex& a) {
        if (!ok || !a) { ok = false; return; }
        switch (a->kind) {
            case Kind::Num:
                emit(ODE_CONST, a->num.toDouble());
                return;
            case Kind::Sym:
                if (a->name == "pi") { emit(ODE_CONST, M_PI); return; }
                if (a->name == "e") { emit(ODE_CONST, M_E); return; }
                emit(ODE_VAR, 0, varIndex(a->name));
                return;
            case Kind::Add:
                compile(a->args[0]);
                for (size_t i = 1; i < a->args.size(); ++i) {
                    compile(a->args[i]);
                    emit(ODE_ADD);
                }
                return;
            case Kind::Mul:
                compile(a->args[0]);
                for (size_t i = 1; i < a->args.size(); ++i) {
                    compile(a->args[i]);
                    emit(ODE_MUL);
                }
                return;
            case Kind::Pow: {
                // negative integer exponents -> 1/(base^|n|)
                if (a->args[1]->isNum() && a->args[1]->num.isInt() && a->args[1]->num.n < 0) {
                    Ex pos = Num(-a->args[1]->num);
                    emit(ODE_CONST, 1.0);
                    compile(a->args[0]);
                    compile(pos);
                    emit(ODE_POW);
                    emit(ODE_DIV);
                    return;
                }
                compile(a->args[0]);
                compile(a->args[1]);
                emit(ODE_POW);
                return;
            }
            case Kind::Func: {
                const std::string& n = a->name;
                if (n == "Deriv" || n == "Integral") { ok = false; return; }
                static const std::map<std::string, int> ops = {
                    {"sin", ODE_SIN}, {"cos", ODE_COS}, {"tan", ODE_TAN}, {"cot", ODE_COT},
                    {"sec", ODE_SEC}, {"csc", ODE_CSC}, {"asin", ODE_ASIN}, {"acos", ODE_ACOS},
                    {"atan", ODE_ATAN}, {"exp", ODE_EXP}, {"ln", ODE_LN}, {"log", ODE_LN},
                    {"sqrt", ODE_SQRT}, {"abs", ODE_ABS}, {"sinh", ODE_SINH}, {"cosh", ODE_COSH},
                    {"tanh", ODE_TANH}, {"asinh", ODE_ASINH}, {"acosh", ODE_ACOSH}, {"atanh", ODE_ATANH},
                    {"erf", ODE_ERF}, {"erfc", ODE_ERFC}, {"fact", ODE_FACT}
                };
                auto it = ops.find(n);
                if (it == ops.end()) { ok = false; return; }
                if (a->args.size() != 1) { ok = false; return; }
                compile(a->args[0]);
                emit(it->second);
                return;
            }
        }
        ok = false;
    }
};

double evalReal(const Ex& e, const std::map<std::string, double>& vars, bool* okp) {
    Compiler c;
    c.compile(e);
    if (!c.ok) { if (okp) *okp = false; return std::nan(""); }
    std::vector<double> vals(c.vars.size(), 0.0);
    for (auto& kv : vars) {
        auto it = c.vars.find(kv.first);
        if (it != c.vars.end()) vals[it->second] = kv.second;
    }
    double v = ode_eval_rpn(c.code.data(), (int)c.code.size(), vals.data());
    if (okp) *okp = std::isfinite(v);
    return v;
}

static double sampleCoord(int i, int which) {
    // deterministic, avoids 0 and 1, all positive
    static const double xs[] = {0.37, 1.71, 2.53, 0.91, 3.17, 4.29, 1.13, 2.77, 0.63, 5.11};
    static const double ys[] = {1.13, 0.77, 2.31, 3.71, 0.53, 2.93, 4.07, 1.57, 3.37, 0.83};
    return which == 0 ? xs[i % 10] : ys[i % 10];
}

bool numericallyZero(const Ex& e, const std::vector<std::string>& syms, int samples, double tol) {
    int finite = 0;
    // Try both signs of every sample.  A solution such as
    // y = C2 + C + x - (C+x)ln(-C-x) is only real when -C-x > 0, so an
    // all-positive sample would leave the logarithm undefined and the
    // verification inconclusive.
    for (int i = 0; i < samples; ++i) {
        for (int sign = 0; sign < 2; ++sign) {
            std::map<std::string, double> vars;
            int k = 0;
            for (auto& s : syms) {
                double value = sampleCoord(i + k, k % 2);
                if (sign) value = -value;
                vars[s] = value;
                ++k;
            }
            bool ok = false;
            double v = evalReal(e, vars, &ok);
            if (!ok) continue;
            ++finite;
            if (std::fabs(v) > tol) return false;
        }
    }
    return finite > 0;
}

bool numericallyEqual(const Ex& a, const Ex& b, const std::vector<std::string>& syms, int samples, double tol) {
    return numericallyZero(Add(a, Mul(Num(-1), b)), syms, samples, tol);
}

// ---------------------------------------------------------------- domain scan
namespace {

bool evalPoint(const Ex& f, const std::string& var, double xv,
               const std::map<std::string, double>& fixed, double* out) {
    std::map<std::string, double> vars = fixed;
    vars[var] = xv;
    bool ok = false;
    const double v = evalReal(f, vars, &ok);
    if (!ok || !std::isfinite(v)) return false;
    *out = v;
    return true;
}

bool evalAbs(const Ex& f, const std::string& var, double xv,
             const std::map<std::string, double>& fixed, double* out) {
    double v = 0;
    if (!evalPoint(f, var, xv, fixed, &v)) return false;
    *out = std::fabs(v);
    return true;
}

// Denominator expressions that can make f undefined.  tan/sec use cos and
// cot/csc use sin, matching the C evaluator.
void collectDenominators(const Ex& e, std::vector<Ex>& out) {
    if (!e) return;
    if (e->isPow() && e->args.size() == 2 && e->args[1]->isNum() &&
        e->args[1]->num.isInt() && e->args[1]->num.n < 0) {
        out.push_back(e->args[0]);
    }
    if (e->isFunc() && e->args.size() == 1) {
        if (e->name == "tan" || e->name == "sec") out.push_back(Fn("cos", {e->args[0]}));
        else if (e->name == "cot" || e->name == "csc") out.push_back(Fn("sin", {e->args[0]}));
    }
    for (const auto& arg : e->args) collectDenominators(arg, out);
}

// Golden-section minimisation of |g| on [a, b]; used for even-multiplicity
// zeros of a denominator (sin(x)^2 touches zero without changing sign).
double minimiseAbs(const Ex& g, const std::string& var, const std::map<std::string, double>& fixed,
                   double a, double b) {
    const double phi = 0.6180339887498949;
    double c = b - phi * (b - a), d = a + phi * (b - a);
    double fc = 0, fd = 0;
    if (!evalAbs(g, var, c, fixed, &fc)) fc = std::numeric_limits<double>::infinity();
    if (!evalAbs(g, var, d, fixed, &fd)) fd = std::numeric_limits<double>::infinity();
    for (int it = 0; it < 80; ++it) {
        if (fc < fd) {
            b = d; d = c; fd = fc;
            c = b - phi * (b - a);
            if (!evalAbs(g, var, c, fixed, &fc)) fc = std::numeric_limits<double>::infinity();
        } else {
            a = c; c = d; fc = fd;
            d = a + phi * (b - a);
            if (!evalAbs(g, var, d, fixed, &fd)) fd = std::numeric_limits<double>::infinity();
        }
    }
    return 0.5 * (a + b);
}

void addRoot(std::vector<double>& roots, double r, double lo, double hi) {
    if (!std::isfinite(r) || r < lo - 1e-9 || r > hi + 1e-9) return;
    for (double existing : roots) {
        if (std::fabs(existing - r) < 1e-6) return;
    }
    roots.push_back(r);
}

// Zeros of a denominator on [lo, hi]: sign changes (odd multiplicity) plus
// near-zero minima (even multiplicity), refined by bisection/minimisation.
std::vector<double> denominatorZeros(const Ex& g, const std::string& var,
                                     const std::map<std::string, double>& fixed,
                                     double lo, double hi, int samples) {
    std::vector<double> roots;
    double vlo = 0, vhi = 0;
    if (evalAbs(g, var, lo, fixed, &vlo) && vlo < 1e-10) addRoot(roots, lo, lo, hi);
    if (evalAbs(g, var, hi, fixed, &vhi) && vhi < 1e-10) addRoot(roots, hi, lo, hi);
    const int n = std::max(400, samples);
    const double h = (hi - lo) / n;
    double prevX = lo, prev = 0;
    bool havePrev = evalAbs(g, var, lo, fixed, &prev);
    for (int i = 1; i <= n; ++i) {
        const double x = lo + h * i;
        double v = 0;
        const bool ok = evalAbs(g, var, x, fixed, &v);
        if (!ok) { havePrev = false; prevX = x; continue; }
        if (havePrev) {
            // Odd-multiplicity zero: sign change of g (not of |g|).  Evaluate
            // the signed value for the bracket.
            double gl = 0, gr = 0;
            if (evalPoint(g, var, prevX, fixed, &gl) && evalPoint(g, var, x, fixed, &gr) &&
                gl * gr < 0) {
                double a = prevX, b = x, fa = gl;
                for (int it = 0; it < 90; ++it) {
                    const double mid = 0.5 * (a + b);
                    double fm = 0;
                    if (!evalPoint(g, var, mid, fixed, &fm)) break;
                    if (fa * fm < 0) b = mid;
                    else { a = mid; fa = fm; }
                }
                addRoot(roots, 0.5 * (a + b), lo, hi);
            }
            // Even-multiplicity zero: |g| dips to zero between samples.
            if (v < 1e-3 || prev < 1e-3) {
                const double m = minimiseAbs(g, var, fixed, prevX, x);
                double vm = 0;
                if (evalAbs(g, var, m, fixed, &vm) && vm < 1e-8) addRoot(roots, m, lo, hi);
            }
        }
        prevX = x;
        prev = v;
        havePrev = true;
    }
    return roots;
}

} // namespace

bool checkDomain(const Ex& f, const std::string& var,
                 const std::map<std::string, double>& fixed, double lo, double hi,
                 DomainIssue* issue, int samples) {
    if (issue) *issue = DomainIssue{};
    if (!f || !(lo < hi)) return false;
    const int n = std::max(64, samples);
    const double h = (hi - lo) / n;

    // 1. Dense finite-value scan, including runaway growth.
    for (int i = 0; i <= n; ++i) {
        const double x = lo + h * i;
        double v = 0;
        if (!evalPoint(f, var, x, fixed, &v) || std::fabs(v) > 1e10) {
            if (issue) {
                issue->found = true;
                issue->code = "singular";
                issue->point = x;
                issue->detail = "f is not finite at " + var + " = " + std::to_string(x);
            }
            return true;
        }
    }

    // 2. Denominator zeros, which may fall between the samples.
    std::vector<Ex> denominators;
    collectDenominators(f, denominators);
    const double eps = std::max(1e-7, (hi - lo) * 1e-7);
    for (const Ex& g : denominators) {
        for (double p : denominatorZeros(g, var, fixed, lo, hi, n)) {
            double left = 0, right = 0;
            const bool okL = p - eps >= lo && evalPoint(f, var, p - eps, fixed, &left);
            const bool okR = p + eps <= hi && evalPoint(f, var, p + eps, fixed, &right);
            const bool blowL = okL && std::fabs(left) > 1e8;
            const bool blowR = okR && std::fabs(right) > 1e8;
            if (!blowL && !blowR) continue;  // removable hole
            if (issue) {
                issue->found = true;
                issue->code = "singular";
                issue->point = p;
                issue->detail = toPlain(f) + " is undefined at " + var + " = " +
                                std::to_string(p);
            }
            return true;
        }
    }
    return false;
}

} // namespace ode
