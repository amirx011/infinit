// Infinit Lab — public solver model
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "expr.hpp"
#include "localize.hpp"

namespace ode {

struct Step {
    // Legacy text fallback for sections that have not migrated yet.
    std::string title;
    Ex math;             // may be null
    std::string note;
    // Localisation keys.  When non-empty the UI resolves them from the
    // catalogue and ignores the legacy text fields.
    std::string titleKey;
    std::string noteKey;
    // Raw JSON object holding plain numeric params, e.g. {"n":4}.
    std::string params;
    // Optional formula for the note.  Math never goes inside a sentence.
    std::string noteMath;
};

struct Check {
    // Legacy text fallback for sections that have not migrated yet.
    std::string name;
    bool ok = false;
    std::string detail;
    // Localisation keys.  When non-empty the UI resolves them from the
    // catalogue and ignores the legacy text fields.
    std::string nameKey;
    std::string detailKey;
    // Raw JSON object holding plain numeric params, e.g. {"x":0.5}.
    std::string params;
    // Optional formula for the detail.  Math never goes inside a sentence.
    std::string detailMath;
};

// A point reported by the behaviour / optimisation analyses.
struct CriticalPointView {
    Ex x;                    // location
    Ex value;                // f(x)
    std::string kind;        // maximum / minimum / inflection / candidate
};

// One partial derivative (first or second order) with its display label.
struct PartialView {
    std::string label;       // f_x, f_y, f_xx, f_xy, ...
    Ex value;
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
    // Every first-order family the equation belongs to, primary included and
    // listed in catalogue order.  The UI shows the primary type and reports the
    // rest as "also ..." so an equation that is both separable and linear is
    // never presented as if it had a single form.
    std::vector<std::string> alsoForms;
    std::string typeFa;
    std::string error;
    // Stable machine-readable failure code (singular / bad_characters /
    // unknown_function / not_integrable) plus technical detail for logs.
    std::string errorCode;
    std::string errorDetail;
    // Raw JSON object with plain error params (for example {"n":2}).
    std::string errorParams;
    double errorPoint = 0.0;
    bool hasErrorPoint = false;
    // Canonical form of the parsed input when it differs from the raw text.
    std::string interpretedAs;
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
    Ex orthogonalSlope;        // right side of y' = f(x,y) after eliminating c
    Ex trajectory;             // solution of the oblique/orthogonal family ODE
    bool trajectoryExplicit = false;
    Ex trajectorySlope;        // right side of y' = f(x,y) for the trajectory
    double trajectoryAngle = 90.0; // requested crossing angle in degrees
    std::string trajectoryMode;    // orthogonal / oblique
    std::vector<Ex> familyBranches; // explicit y(x,c) branches of the original family
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

    // ------------------------------------------- asymptotes and curve sketching
    std::vector<std::string> asymptotes;         // printable equations
    std::vector<Ex> xIntercepts;
    Ex yIntercept;
    bool hasYIntercept = false;
    std::vector<std::string> sketch;             // summary lines

    // ------------------------------------------- Taylor / Maclaurin
    Ex polynomial;                               // the Taylor polynomial
    Ex remainder;                                // Lagrange remainder expression
    std::vector<Ex> coefficients;                // c_0 ... c_n

    // ------------------------------------------- definite-integral applications
    Ex integrand;                                // the integrand actually used
    std::string application;                     // area / volume / arc_length

    // ------------------------------------------- partial derivatives
    std::vector<PartialView> partials;           // first and second partials
    std::vector<Ex> gradient;                    // evaluated gradient vector
    Ex laplacian;
    Ex directional;                              // directional derivative
    Ex functionValue;                            // f at the point
    std::string pointLabel;                      // printable point

    // ------------------------------------------- series and convergence
    std::string seriesVerdict;                   // converges / diverges / inconclusive
    std::string seriesTest;                      // ratio / root / geometric / p-series / ...
    bool converges = false;
    Ex seriesSum;                                // exact sum when known
    double partialSum = 0.0;                     // numeric partial sum
    std::vector<double> partialSums;             // S_k for the partial-sum plot
    Ex functionValueA;                           // Cauchy: g(a)
    Ex functionValueB;                           // Cauchy: g(b)
    bool hypothesesContinuous = false;
    bool hypothesesDifferentiable = false;
    bool rolleApplies = false;

    bool closedForm = true;
    // The equation has no supported closed form, so the answer is a numerical
    // RK4 trajectory. The UI draws it and labels it as approximate instead of
    // pretending a symbolic solution exists.
    bool numericOnly = false;
    std::string numericRhs;
    std::string numericNote;
    std::string numericNoteParams;
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

// Append a key-based validation.  Math and sentences stay separate: a formula
// goes in `detailMath`, a sentence in `detailKey`, and `params` holds numbers.
inline void addCheck(Result& r, const std::string& nameKey, bool ok,
                     const std::string& detailKey = "", const std::string& params = "",
                     const std::string& detailMath = "") {
    Check c;
    c.nameKey = nameKey;
    c.ok = ok;
    c.detailKey = detailKey;
    c.params = params;
    c.detailMath = detailMath;
    r.checks.push_back(std::move(c));
}

// User-facing English text for a coded engine failure.
inline std::string englishForEngineError(const EngineError& e) {
    if (e.code == "bad_characters") return "The input contains an invalid character.";
    if (e.code == "singular") return "The function is undefined at a singular point.";
    if (e.code == "not_integrable") return "This function is not integrable on the interval.";
    if (e.code == "unknown_function") {
        return e.detail.empty() ? "Unknown function." : ("Unknown function: " +
               e.detail.substr(e.detail.find(':') == std::string::npos
                                   ? 0 : e.detail.find(':') + 2));
    }
    return localizeError(e.what(), true);
}

// Set a coded failure without ever putting a sentence in the response.  The
// technical text only goes to errorDetail (logs).
inline void setCodedFailure(Result& r, const std::exception& e,
                            const std::string& where = "") {
    if (const auto* coded = dynamic_cast<const EngineError*>(&e)) {
        r.errorCode = coded->code;
        r.errorDetail = coded->detail;
    } else {
        r.errorCode = "unavailable";
        r.errorDetail = where.empty() ? e.what() : where + ": " + e.what();
    }
    r.ok = false;
    r.verified = false;
    r.error.clear();
}

// Fill a Result from an exception without leaking Persian text into an English
// response.  EngineError carries its own user-facing sentence.
inline void setFailure(Result& r, bool en, const std::exception& e,
                       const std::string& enPrefix = "", const std::string& faPrefix = "") {
    (void)en;
    (void)enPrefix;
    (void)faPrefix;
    if (const auto* coded = dynamic_cast<const EngineError*>(&e)) {
        r.errorCode = coded->code;
        r.errorDetail = coded->detail;
        r.error.clear();
        return;
    }
    r.error = en ? enPrefix + localizeError(e.what(), true) : faPrefix + e.what();
}

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
// Dedicated orthogonal-trajectory solver: eliminates c, flips the slope and
// solves the resulting first-order ODE with the full step-by-step derivation.
Result solveOrthogonal(const std::string& input, const std::string& c = "c",
                       const std::string& x = "x", const std::string& y = "y",
                       const std::string& lang = "en");
// Oblique trajectories: curves crossing the family at a constant angle
// `angleDeg` (degrees).  The orthogonal family is the 90° special case.
Result solveOblique(const std::string& input, const std::string& c = "c",
                    double angleDeg = 45.0, const std::string& x = "x",
                    const std::string& y = "y", const std::string& lang = "en");
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
