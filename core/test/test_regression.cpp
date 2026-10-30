#include "odecore.h"
#include "solver.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace ode;

static int failures = 0;

static void expect(bool condition, const std::string& what) {
    if (!condition) {
        ++failures;
        std::cout << "FAIL: " << what << "\n";
    }
}

static Result solve(const std::string& input) { return solveODE(input); }

int main() {
    {
        Result r = solve("y' = y");
        expect(r.ok, "y'=y solved");
        expect(r.typeId == "linear", "y'=y classified linear");
        expect(r.isExplicit, "y'=y explicit");
        expect(r.checks.size() == 1 && r.checks[0].ok, "y'=y verified");
    }
    {
        Result r = solve("y' = y^2");
        expect(r.ok, "y'=y^2 solved");
        expect(r.typeId == "separable", "y'=y^2 separable");
        expect(r.closedForm, "y'=y^2 closed form");
    }
    {
        Result r = solve("y' = 1 + y^2");
        expect(r.ok, "y'=1+y^2 solved");
        expect(r.isExplicit, "y'=1+y^2 explicit");
        expect(r.explicitY && toPlain(r.explicitY).find("tan") != std::string::npos,
               "y'=1+y^2 gives tan");
    }
    {
        Result r = solve("y' = (x+y)^2");
        expect(r.ok, "y'=(x+y)^2 solved");
        expect(r.typeId == "linear_sub", "y'=(x+y)^2 linear substitution");
        expect(r.isExplicit, "y'=(x+y)^2 explicit");
    }
    {
        Result r = solve("2*y + x*y' = 0");
        expect(r.ok, "2y+xy'=0 solved");
        expect(r.typeId == "linear", "2y+xy'=0 linear");
    }
    {
        Result r = solve("(2x+y) + (x+2y)*y' = 0");
        expect(r.ok, "(2x+y)+(x+2y)y'=0 solved");
        expect(r.typeId == "exact", "(2x+y)+(x+2y)y'=0 exact");
        expect(r.checks.size() == 1 && r.checks[0].ok, "exact equation verified");
    }
    {
        Result r = solve("(2*x*y+3) + (x^2-1)*y' = 0");
        expect(r.ok, "exact with polynomial coefficients solved");
        expect(r.typeId == "exact", "exact equation prefers the potential function");
    }
    {
        Result r = solve("y' = 1/(x^2-x+1)");
        expect(r.ok, "quadratic reciprocal integrated");
        expect(r.closedForm, "quadratic reciprocal is closed form");
        expect(r.solution && toPlain(r.solution).find("sqrt(3)") != std::string::npos,
               "quadratic reciprocal uses sqrt(3)");
    }
    {
        Ex x = Sym("x");
        expect(toPretty(Mul(Num(2), Pow(x, Num(-1)))) == "2/x",
               "printer renders 2/x");
        expect(toPretty(Mul(Num(-1), Pow(x, Num(-1)))) == "\u22121/x",
               "printer renders -1/x");
        expect(toPlain(Mul(Num(Rational(1, 2)), Pow(x, Num(-1)))) == "1/(2*x)",
               "printer folds rational coefficient into denominator");
        Result r = solve("y' = y**2");
        expect(r.ok, "double-star power accepted");
    }
    {
        Result r = solve("(x+y)dx + (x+2y)dy = 0");
        expect(r.ok, "differential form solved");
        expect(r.typeId == "exact", "differential form exact");
    }
    {
        Result r = solve("y'' - 3y' + 2y = 0");
        expect(r.ok, "constant-coefficient order 2 solved");
        expect(r.order == 2, "constant-coefficient order is 2");
        expect(r.checks.size() == 1 && r.checks[0].ok, "order-2 solution verified");
    }
    {
        Result r = solve("y'' + y = 0");
        expect(r.ok, "y''+y=0 solved");
        expect(r.typeId == "linear_cc", "y''+y=0 constant coefficients");
    }
    {
        Result r = solve("y'' = 6x");
        expect(r.ok, "y''=6x solved");
        expect(r.typeId == "direct_integration", "y''=6x direct integration");
    }
    {
        Result r = solve("y = x*y' + (y')^2");
        expect(r.ok, "Clairaut solved");
        expect(r.typeId == "clairaut", "Clairaut classified");
        expect(r.singular.size() == 1, "Clairaut singular solution present");
        expect(r.singular.size() == 1 &&
                   toPlain(r.singular[0]).find("1/4") != std::string::npos,
               "Clairaut envelope is y = -x^2/4");
    }
    {
        Result r = solve("y = 2*x*y' + (y')^2");
        expect(r.ok, "Lagrange solved");
        expect(r.typeId == "lagrange", "Lagrange classified");
        expect(r.paramX && r.paramY, "Lagrange parametric solution present");
    }
    {
        Result r = solve("y' = y^2 - 2/x^2");
        expect(r.ok, "Riccati with particular solution solved");
        expect(r.typeId == "riccati", "Riccati classified");
        expect(r.checks.size() == 1 && r.checks[0].ok, "Riccati solution verified");
    }
    {
        Result r = solve("y = x*y' + y'*ln(y')");
        expect(r.ok, "transcendental Clairaut solved");
        expect(r.typeId == "clairaut", "transcendental Clairaut classified");
        expect(r.paramX && r.paramY, "Clairaut envelope is parametric");
        expect(r.paramSingular, "Clairaut parametric envelope marked as singular");
    }
    {
        Result r = solve("y = x*y' + y'*sin(y')");
        expect(r.ok, "sine Clairaut solved");
        expect(r.typeId == "clairaut", "sine Clairaut classified");
        expect(r.paramSingular, "sine Clairaut parametric envelope flagged");
    }
    {
        Result r = solve("y = x*(y')^2 + (y')^3");
        expect(r.ok, "Lagrange with two singular branches solved");
        expect(r.typeId == "lagrange", "Lagrange classified");
        expect(r.paramX && r.paramY, "Lagrange general solution is parametric");
        expect(r.singular.size() == 2, "Lagrange p-g=0 branches found");
        expect(r.paramX && toPlain(r.paramX).find("C") != std::string::npos &&
               toPlain(r.paramX).find("Integral") == std::string::npos,
               "Lagrange parametric solution is simplified");
    }
    {
        Result r = solve("y = xy' + y'sin y'");
        expect(r.ok, "natural notation Clairaut solved");
        expect(r.typeId == "clairaut", "natural notation Clairaut classified");
    }
    {
        Result r = solve("y = xy' + y'ln y'");
        expect(r.ok, "natural logarithm Clairaut solved");
        expect(r.typeId == "clairaut", "natural logarithm Clairaut classified");
    }
    {
        Result r = solve("y = xy'^2 + y'^3");
        expect(r.ok, "natural notation Lagrange solved");
        expect(r.typeId == "lagrange", "natural notation Lagrange classified");
    }
    {
        Result r = solve("y' = 1+x^2-2xy+y^2");
        expect(r.ok, "natural notation Riccati solved");
        expect(r.typeId == "riccati", "natural notation Riccati classified");
        expect(r.isExplicit, "natural notation Riccati explicit");
    }
    {
        OdeOptions o;
        o.particular = "sin(x)";
        Result r = solveODE("2y'cosx = 2cos^2x - ysinx + y^2", o);
        expect(r.ok, "trigonometric Riccati solved");
        expect(r.typeId == "riccati", "trigonometric Riccati classified");
        expect(r.solution && toPlain(r.solution).find("sin(x)") != std::string::npos,
               "trigonometric Riccati uses y1 = sin(x)");
        expect(!r.closedForm, "trigonometric Riccati reports an exact quadrature");
    }
    {
        Result r = solveWronskian({"sin(x)", "cos(x)"});
        expect(r.ok, "Wronskian computed");
        expect(r.wronskian && toPlain(r.wronskian) == "-1", "W(sin,cos) = -1");
        expect(r.independent, "sin/cos independent");
    }
    {
        Result r = solveWronskian({"x", "2x"});
        expect(r.ok, "dependent Wronskian computed");
        expect(r.wronskian && r.wronskian->isZero(), "W(x,2x) = 0");
        expect(!r.independent, "x,2x dependent");
    }
    {
        Result r = solveEnvelope("y = c*x + c^2");
        expect(r.ok, "line family envelope");
        expect(r.envelope.size() == 1, "line family has one envelope");
    }
    {
        Result r = solveEnvelope("x^2 + y^2 = c^2");
        expect(r.ok, "concentric circles analysed");
        expect(r.envelope.empty(), "concentric circles have no envelope");
        expect(!r.warnings.empty(), "no-envelope case warns");
    }
    {
        Result r = solveEnvelope("x^2 + y^2 = c^2", "c", true);
        expect(r.ok, "orthogonal trajectories requested");
        expect(r.orthogonal != nullptr, "orthogonal trajectories computed");
        expect(r.orthogonal && toPlain(r.orthogonal).find("x") != std::string::npos,
               "orthogonal trajectories involve x");
    }
    {
        Result r = solveEnvelope("y = c*x", "c", true);
        expect(r.ok, "line family with orthogonal trajectories");
        expect(r.orthogonal != nullptr, "orthogonal circles computed for line family");
    }

    {
        Result r = solve("x*y*y' - y^2 + x^2*e^(y/x) = 0");
        expect(r.ok, "homogeneous equation with exp(y/x) solved");
        expect(r.typeId == "homogeneous", "homogeneous exp(y/x) classified");
        expect(r.closedForm, "homogeneous exp(y/x) has closed form");
    }
    {
        Result r = solve("(x*y^2 + 2*x*e^y - 1) + (x^2*y + x^2*e^y)*y' = 0");
        expect(r.ok, "exact equation with exponential coefficients solved");
        expect(r.typeId == "exact", "exponential-coefficient equation exact");
        expect(r.solution && toPlain(r.solution).find("Integral") == std::string::npos,
               "exact exponential potential has no leftover integral");
    }
    {
        Result r = solve("(x*y^2 + 2*x*e^y - 1)dx + (x^2*y + x^2*e^y)dy = 0");
        expect(r.ok, "differential form with exponentials solved");
        expect(r.typeId == "exact", "differential exponential form exact");
    }
    {
        Result r = solve("x*(2*x+y) + x*(x+2*y)*y' = 0");
        expect(r.ok, "scaled exact equation solved");
        expect(r.typeId == "exact" || r.typeId == "integrating_factor",
               "scaled equation recognized as exact after cancelling x");
        expect(r.solution && toPlain(r.solution).find("x^2 + y^2 + x*y") != std::string::npos,
               "scaled equation has the potential x^2 + y^2 + x*y");
    }
    {
        Result r = solve("y' = exp(x+y)");
        expect(r.ok, "separable exponential equation solved");
        expect(r.typeId == "separable", "exp(x+y) detected as separable");
        expect(r.closedForm, "exp(x+y) closed form");
    }
    {
        Result r = solve("(x^2+y^2)dx - 2*x*y dy = 0");
        expect(r.ok, "differential form falling back to the explicit path");
        expect(r.solution != nullptr, "differential form fallback produced a solution");
    }
    {
        Result r = solve("(2*x*y+y)dx + (x^2+x)dy = 0");
        expect(r.ok, "exactness test survives coefficient distribution");
        expect(r.typeId == "exact", "distributed exact equation classified exact");
    }
    {
        Result r = solve("x*y' = y + sqrt(x^2+y^2)");
        expect(r.ok, "homogeneous equation with a radical");
        expect(r.typeId == "homogeneous", "radical equation classified homogeneous");
    }
    {
        Result r = solve("y' + y = x*sqrt(y)");
        expect(r.ok, "Bernoulli equation with a fractional exponent");
        expect(r.typeId == "bernoulli", "fractional Bernoulli classified");
    }
    {
        Result r = solve("y' = exp(-x^2)");
        expect(r.ok, "Gaussian quadrature solved with erf");
        expect(r.closedForm, "erf solution is closed form");
        expect(r.checks.size() == 1 && r.checks[0].ok, "erf solution verified numerically");
    }
    {
        Result r = solve("dy/dx = x + y");
        expect(r.ok, "dy/dx notation accepted");
        expect(r.typeId == "linear", "dy/dx equation classified linear");
    }
    {
        Result r = solve("d^2y/dx^2 = 6*x");
        expect(r.ok, "d^2y/dx^2 notation accepted");
        expect(r.typeId == "direct_integration", "second derivative notation classified");
    }
    {
        Result r = integrateExpression("2*x*cos(x^2)", "x");
        expect(r.ok, "substitution integral solved");
        expect(r.method == "substitution", "substitution detected");
        expect(r.checks.size() == 1 && r.checks[0].ok, "substitution result verified");
    }
    {
        Result r = integrateExpression("x*exp(x)", "x");
        expect(r.ok, "by-parts integral solved");
        expect(r.method == "parts", "by-parts detected");
        expect(r.checks.size() == 1 && r.checks[0].ok, "by-parts result verified");
    }
    {
        Result r = integrateExpression("exp(x)*sin(x)", "x");
        expect(r.ok, "cyclic by-parts integral solved");
        expect(r.closedForm, "cyclic by-parts integral is closed form");
        expect(r.checks.size() == 1 && r.checks[0].ok, "cyclic integral verified");
    }
    {
        Result r = derivativeExpression("x^3", "x", 2);
        expect(r.ok, "second derivative computed");
        expect(r.order == 2, "derivative order is 2");
        expect(r.solution && toPlain(r.solution) == "6*x", "second derivative of x^3 is 6x");
    }

    // C ABI smoke test
    char* raw = ode_solve("{\"mode\":\"ode\",\"input\":\"y' = y\"}");
    expect(raw != nullptr, "C ABI returns a response");
    if (raw) {
        std::string response(raw);
        expect(response.find("\"ok\":true") != std::string::npos, "C ABI result is ok");
        expect(response.find("\"steps\"") != std::string::npos, "C ABI includes steps");
        ode_free(raw);
    }
    raw = ode_solve("{\"mode\":\"wronskian\",\"functions\":[\"x\",\"2x\"]}");
    expect(raw != nullptr, "C ABI Wronskian response");
    if (raw) {
        std::string response(raw);
        expect(response.find("\"independent\":false") != std::string::npos,
               "C ABI Wronskian dependence flag");
        ode_free(raw);
    }

    if (failures == 0) {
        std::cout << "All ODE regression tests passed.\n";
        return 0;
    }
    std::cout << failures << " regression failure(s).\n";
    return 1;
}
