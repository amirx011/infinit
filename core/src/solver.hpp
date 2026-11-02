// Infinit Lab — public solver model
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "expr.hpp"

namespace ode {

struct Step {
    std::string title;
    Ex math;             // may be null
    std::string note;
};

struct Check {
    std::string name;
    bool ok = false;
    std::string detail;
};

// A point reported by the behaviour / optimisation analyses.
struct CriticalPointView {
    Ex x;                    // location
    Ex value;                // f(x)
    std::string kind;        // maximum / minimum / inflection / candidate
};

// One monotonicity interval of f, with the sign of f' on it.
struct MonotonePiece {
    double lo = 0;
    double hi = 0;
    bool loInfinite = false;
    bool hiInfinite = false;
    int sign = 0;            // -1, 0 or +1
    std::string trend;       // increasing / decreasing / constant
};

struct Result {
    bool ok = false;
    std::string mode;          // ode / envelope / wronskian
    std::string typeId;
    std::string typeFa;
    std::string error;
    std::string engine = "native";
    std::string method;        // integration method, e.g. substitution / parts
    std::string variable;      // calculus variable
    int order = 0;

    Ex normalized;             // F(x,y,y',...) = 0
    Ex solution;               // implicit u(x,y)=C, or explicit y(x)
    bool isExplicit = false;
    Ex explicitY;              // valid when isExplicit
    Ex paramX, paramY;         // parametric solution (t = parameter)
    std::string paramName = "p";
    bool paramSingular = false; // Clairaut envelope given parametrically
    std::vector<Ex> singular;

    Ex wronskian;              // W
    bool independent = false;
    std::vector<Ex> envelope;
    Ex orthogonal;             // solution of the orthogonal-trajectory ODE
    bool orthogonalExplicit = false;
    std::vector<std::string> functions;
    std::string family;

    // calculus extras: multiple / definite integrals and implicit derivatives
    int dimension = 1;                 // 1, 2 or 3
    bool definite = false;             // definite integral with bounds
    bool implicit = false;             // implicit derivative (equation input)
    bool hasNumeric = false;           // numeric evaluation available
    double numericValue = 0.0;         // numeric value of the definite integral
    std::vector<std::string> vars;     // integration variables, innermost first
    std::vector<std::string> lower;    // bounds (printable)
    std::vector<std::string> upper;

    // limits and continuity
    std::string point;                 // the point the variable approaches
    std::string side = "both";         // both / left / right
    bool limitExists = false;
    bool limitInfinite = false;        // diverges to +-infinity
    int limitSign = 1;                 // sign of the divergence
    bool leftExists = false;
    bool rightExists = false;
    bool leftInfinite = false;
    bool rightInfinite = false;
    int leftSign = 1;
    int rightSign = 1;
    Ex leftValue;                      // finite one-sided values
    Ex rightValue;
    bool valueDefined = false;         // f(point) exists
    Ex valueAt;                        // f(point)
    bool continuous = false;
    std::string discontinuity;         // removable / jump / infinite / oscillatory / domain
    std::vector<std::string> laws;     // limit laws that fired

    std::vector<Step> steps;
    std::vector<std::string> assumptions;
    std::vector<std::string> warnings;
    std::vector<Check> checks;

    // ------------------------------------------- applications of the derivative
    Ex derivative;                // f'
    Ex secondDerivative;          // f''
    Ex value;                     // f(a)
    Ex slope;                     // slope at a point, or the secant slope
    Ex tangent;                   // tangent line (solved for y)
    Ex normal;                    // normal line (solved for y), or x = a
    Ex tangentSlope;              // f'(a)
    Ex normalSlope;               // -1/f'(a); null when the tangent is horizontal
    Ex valueA;                    // f(a)
    Ex valueB;                    // f(b)
    Ex maximum;                   // largest value on the closed interval
    Ex minimum;                   // smallest value on the closed interval
    Ex maximumAt;                 // point where the maximum occurs
    Ex minimumAt;                 // point where the minimum occurs
    Ex functionF;                 // Cauchy MVT: first function
    Ex functionG;                 // Cauchy MVT: second function
    std::vector<CriticalPointView> critical;     // behaviour: critical points
    std::vector<CriticalPointView> candidates;   // optimisation: compared points
    std::vector<CriticalPointView> inflections;  // behaviour: inflection points
    std::vector<MonotonePiece> pieces;           // behaviour: monotonicity pieces
    std::vector<Ex> points;                      // MVT / Cauchy: the points c
    std::vector<Ex> pointValues;                 // f(c) for each point in `points`
    Ex functionValueA;                           // Cauchy: g(a)
    Ex functionValueB;                           // Cauchy: g(b)
    bool hypothesesContinuous = false;
    bool hypothesesDifferentiable = false;
    bool rolleApplies = false;

    bool closedForm = true;
    // True when every reported check passed.  `ok` says the solver produced an
    // answer; `verified` says the answer survived independent verification.
    bool verified = true;
    // Definite integrals: the integrand is singular or a bound is infinite.
    bool improper = false;
    // The requested quantity does not exist (divergent integral / limit).
    bool divergent = false;
    std::vector<std::string> singularities;
    long long timeMs = 0;
};

struct OdeOptions {
    std::string x = "x";
    std::string y = "y";
    std::string particular;   // optional known particular solution (Riccati)
    std::string lang = "en";  // "en" or "fa"
    int simplifyAttempts = 0; // internal retry depth for the simplification pass
    bool steps = true;
    bool verify = true;
};

Result solveODE(const std::string& input, const OdeOptions& opt = {});
Result solveEnvelope(const std::string& input, const std::string& c = "c", bool orthogonal = false,
                     const std::string& x = "x", const std::string& y = "y",
                     const std::string& lang = "en");
Result solveWronskian(const std::vector<std::string>& functions, const std::string& x = "x",
                      const std::string& lang = "en");
Result integrateExpression(const std::string& input, const std::string& x = "x",
                           const std::string& lang = "en");
Result derivativeExpression(const std::string& input, const std::string& x = "x", int order = 1,
                            const std::string& lang = "en");
// Limit of an expression as a variable approaches a point (side: both/left/right).
Result limitExpression(const std::string& input, const std::string& x, const std::string& point,
                       const std::string& side = "both", const std::string& lang = "en");
// Continuity of an expression at a point (one-sided limits + value of the function).
Result continuityExpression(const std::string& input, const std::string& x,
                            const std::string& point, const std::string& lang = "en");
// Multi-dimensional (1..3) and/or definite integration.
Result integrateMulti(const std::string& input, const std::vector<std::string>& vars,
                      bool definite, const std::vector<std::string>& lower,
                      const std::vector<std::string>& upper, const std::string& lang = "en");

// Verify an explicit candidate against F(x, y, y′, …) = 0.  Derivative nodes
// are replaced by the derivatives of `yExpr`; the check is appended to
// `r.checks` unless one is already present.  Exposed for the solver tests.
void verifyExplicit(Result& r, const Ex& F, const Ex& yExpr, const std::string& x,
                    const std::string& y, bool en = true);

// helpers shared with the API
Ex parseODE(const std::string& input, const std::string& x, std::string* dep);
bool isClosedForm(const Ex& e);
std::string typeNameFa(const std::string& id);
std::string typeNameEn(const std::string& id);

} // namespace ode
