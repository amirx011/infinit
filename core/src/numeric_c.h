/* Infinit Lab — tiny C evaluator for numeric certificates */
#ifndef ODE_NUMERIC_C_H
#define ODE_NUMERIC_C_H

#ifdef __cplusplus
extern "C" {
#endif

enum OdeOp {
    ODE_CONST = 0,
    ODE_VAR = 1,
    ODE_ADD = 2,
    ODE_SUB = 3,
    ODE_MUL = 4,
    ODE_DIV = 5,
    ODE_POW = 6,
    ODE_NEG = 7,
    ODE_SIN = 8,
    ODE_COS = 9,
    ODE_TAN = 10,
    ODE_ASIN = 11,
    ODE_ACOS = 12,
    ODE_ATAN = 13,
    ODE_EXP = 14,
    ODE_LN = 15,
    ODE_SQRT = 16,
    ODE_ABS = 17,
    ODE_SINH = 18,
    ODE_COSH = 19,
    ODE_TANH = 20,
    ODE_ASINH = 21,
    ODE_ACOSH = 22,
    ODE_ATANH = 23,
    ODE_SEC = 24,
    ODE_CSC = 25,
    ODE_COT = 26,
    ODE_ERF = 27,
    ODE_ERFC = 28,
    ODE_FACT = 29
};

typedef struct {
    int op;
    double value; /* for CONST */
    int index;    /* for VAR */
} OdeInstr;

/* Evaluate reverse-polish program. Returns NAN on domain errors. */
double ode_eval_rpn(const OdeInstr* code, int n, const double* vars);

#ifdef __cplusplus
}
#endif
#endif
