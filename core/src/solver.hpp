// Infinit Lab — public solver model
// SPDX-License-Identifier: MIT
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

    std::vector<Step> steps;
    std::vector<std::string> assumptions;
    std::vector<std::string> warnings;
    std::vector<Check> checks;
    bool closedForm = true;
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

// helpers shared with the API
Ex parseODE(const std::string& input, const std::string& x, std::string* dep);
bool isClosedForm(const Ex& e);
std::string typeNameFa(const std::string& id);
std::string typeNameEn(const std::string& id);

} // namespace ode
