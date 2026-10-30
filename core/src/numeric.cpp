// ODE Lab — numeric certificates
#include "numeric.hpp"
#include "numeric_c.h"
#include <cmath>
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
                    {"erf", ODE_ERF}, {"erfc", ODE_ERFC}
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
    for (int i = 0; i < samples; ++i) {
        std::map<std::string, double> vars;
        int k = 0;
        for (auto& s : syms) vars[s] = sampleCoord(i + k, k % 2), ++k;
        bool ok = false;
        double v = evalReal(e, vars, &ok);
        if (!ok) continue;
        ++finite;
        if (std::fabs(v) > tol) return false;
    }
    return finite > 0;
}

bool numericallyEqual(const Ex& a, const Ex& b, const std::vector<std::string>& syms, int samples, double tol) {
    return numericallyZero(Add(a, Mul(Num(-1), b)), syms, samples, tol);
}

} // namespace ode
