/* Infinit Lab — C numeric evaluator (math.h only) */
#include "numeric_c.h"
#include <math.h>

double ode_eval_rpn(const OdeInstr* code, int n, const double* vars) {
    double st[512];
    int sp = 0;
    if (!code || n <= 0) return NAN;
    for (int i = 0; i < n; ++i) {
        int op = code[i].op;
        if (op == ODE_CONST) {
            if (sp >= 511) return NAN;
            st[sp++] = code[i].value;
        } else if (op == ODE_VAR) {
            if (sp >= 511) return NAN;
            st[sp++] = vars ? vars[code[i].index] : 0.0;
        } else if (op == ODE_NEG) {
            if (sp < 1) return NAN;
            st[sp - 1] = -st[sp - 1];
        } else if (op >= ODE_SIN && op <= ODE_FACT) {
            double u;
            if (sp < 1) return NAN;
            u = st[sp - 1];
            switch (op) {
                case ODE_SIN: st[sp - 1] = sin(u); break;
                case ODE_COS: st[sp - 1] = cos(u); break;
                case ODE_TAN: st[sp - 1] = tan(u); break;
                case ODE_ASIN: st[sp - 1] = asin(u); break;
                case ODE_ACOS: st[sp - 1] = acos(u); break;
                case ODE_ATAN: st[sp - 1] = atan(u); break;
                case ODE_EXP: st[sp - 1] = exp(u); break;
                case ODE_LN: st[sp - 1] = log(u); break;
                case ODE_SQRT: st[sp - 1] = sqrt(u); break;
                case ODE_ABS: st[sp - 1] = fabs(u); break;
                case ODE_SINH: st[sp - 1] = sinh(u); break;
                case ODE_COSH: st[sp - 1] = cosh(u); break;
                case ODE_TANH: st[sp - 1] = tanh(u); break;
                case ODE_ASINH: st[sp - 1] = asinh(u); break;
                case ODE_ACOSH: st[sp - 1] = acosh(u); break;
                case ODE_ATANH: st[sp - 1] = atanh(u); break;
                case ODE_SEC: st[sp - 1] = 1.0 / cos(u); break;
                case ODE_CSC: st[sp - 1] = 1.0 / sin(u); break;
                case ODE_COT: st[sp - 1] = cos(u) / sin(u); break;
                case ODE_ERF: st[sp - 1] = erf(u); break;
                case ODE_ERFC: st[sp - 1] = erfc(u); break;
                case ODE_FACT:
                    /* factorial for non-negative integers, Gamma(u+1) otherwise */
                    if (u < 0.0 && u == floor(u)) st[sp - 1] = NAN;
                    else st[sp - 1] = tgamma(u + 1.0);
                    break;
                default: break;
            }
        } else {
            double a, b;
            if (sp < 2) return NAN;
            b = st[--sp];
            a = st[--sp];
            switch (op) {
                case ODE_ADD: st[sp++] = a + b; break;
                case ODE_SUB: st[sp++] = a - b; break;
                case ODE_MUL: st[sp++] = a * b; break;
                case ODE_DIV: st[sp++] = a / b; break;
                case ODE_POW: st[sp++] = pow(a, b); break;
                default: return NAN;
            }
        }
    }
    return sp == 1 ? st[0] : NAN;
}
