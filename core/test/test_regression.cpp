#include "odecore.h"
#include "calculus_apps.hpp"
#include "numeric.hpp"
#include "solver.hpp"
#include "topics.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <utility>
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
        expect(r.typeId == "bernoulli", "y'=y^2 classified as Bernoulli");
        expect(r.closedForm, "y'=y^2 closed form");
        expect(r.isExplicit && toPlain(r.explicitY).find("C - x") != std::string::npos,
               "y'=y^2 gives the explicit family 1/(C-x)");
    }
    // Second-order non-homogeneous equations: undetermined coefficients.
    {
        Result r = solve("y'' - 3*y' + 2*y = exp(x)");
        expect(r.ok, "non-homogeneous exp forcing solved");
        expect(r.typeId == "linear_cc", "non-homogeneous exp forcing classified");
        expect(r.checks.size() == 1 && r.checks[0].ok,
               "non-homogeneous exp forcing verified");
        expect(r.solution && toPlain(r.solution).find("exp(x)") != std::string::npos,
               "non-homogeneous exp forcing keeps the exponential");
    }
    {
        Result r = solve("y'' - 3*y' + 2*y = x^2");
        expect(r.ok, "non-homogeneous polynomial forcing solved");
        expect(r.checks.size() == 1 && r.checks[0].ok,
               "non-homogeneous polynomial forcing verified");
    }
    {
        Result r = solve("y'' + y = cos(x)");
        expect(r.ok, "resonant cosine forcing solved");
        expect(r.checks.size() == 1 && r.checks[0].ok,
               "resonant cosine forcing verified");
        expect(r.solution && toPlain(r.solution).find("x") != std::string::npos,
               "resonant cosine forcing gains a factor x");
    }
    {
        Result r = solve("y'' - y = exp(2*x) + x");
        expect(r.ok, "sum of forcing terms solved");
        expect(r.checks.size() == 1 && r.checks[0].ok,
               "sum of forcing terms verified");
    }
    {
        Result r = solve("y'' + 2*y' + y = exp(-x)");
        expect(r.ok, "repeated-root exponential forcing solved");
        expect(r.checks.size() == 1 && r.checks[0].ok,
               "repeated-root exponential forcing verified");
    }
    {
        Result r = solve("y'' + y = x*cos(x)");
        expect(r.ok, "polynomial-times-cosine forcing solved");
        expect(r.checks.size() == 1 && r.checks[0].ok,
               "polynomial-times-cosine forcing verified");
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
        Result r = solveOrthogonal("x^2 + y^2 = c^2");
        expect(r.ok, "dedicated orthogonal solver: circles");
        expect(r.typeId == "orthogonal", "dedicated orthogonal solver reports its own type");
        expect(r.orthogonal != nullptr, "circles produce orthogonal lines");
        expect(r.orthogonal && toPlain(r.orthogonal) == "C*x",
               "orthogonal trajectories of circles are y = Cx, got " +
                   (r.orthogonal ? toPlain(r.orthogonal) : std::string("<null>")));
        expect(r.closedForm, "orthogonal lines are a closed form");
        expect(!r.steps.empty(), "orthogonal solver emits derivation steps");
        expect(r.orthogonalSlope != nullptr,
               "orthogonal solver exposes the solved slope for plotting");
        expect(!r.familyBranches.empty(),
               "orthogonal solver exposes the original family for plotting");
        bool hasElimination = false, hasFamilyOde = false, hasOrthogonalOde = false;
        for (const auto& step : r.steps) {
            if (step.title.find("Eliminate") != std::string::npos) hasElimination = true;
            if (step.title.find("family ODE") != std::string::npos)
                hasFamilyOde = true;
            if (step.title.find("Orthogonal-trajectory equation") != std::string::npos)
                hasOrthogonalOde = true;
        }
        expect(hasElimination, "orthogonal steps show the c-elimination");
        expect(hasFamilyOde, "orthogonal steps show the family ODE");
        expect(hasOrthogonalOde, "orthogonal steps show the orthogonal ODE");
        expect(!r.steps.empty() && r.steps.back().title == "Orthogonal family",
               "orthogonal derivation ends with exactly one family answer step");
        for (const auto& step : r.steps)
            expect(step.title != "General solution",
                   "orthogonal steps do not duplicate the final answer");
    }
    {
        Result r = solveOrthogonal("y = c*x");
        expect(r.ok, "dedicated orthogonal solver: lines");
        expect(r.orthogonal != nullptr, "lines produce orthogonal circles");
        expect(r.orthogonal &&
                   toPlain(r.orthogonal) == "x^2 + y^2",
               "orthogonal trajectories of lines are circles, got " +
                   (r.orthogonal ? toPlain(r.orthogonal) : std::string("<null>")));
        expect(!r.orthogonalExplicit, "orthogonal circles are implicit");
    }
    {
        Result r = solveOrthogonal("y = c/x");
        expect(r.ok, "dedicated orthogonal solver: hyperbolas");
        expect(r.orthogonal != nullptr, "hyperbolas produce orthogonal hyperbolas");
        expect(r.orthogonal &&
                   toPlain(r.orthogonal) == "y^2 - x^2",
               "orthogonal trajectories of y = c/x, got " +
                   (r.orthogonal ? toPlain(r.orthogonal) : std::string("<null>")));
    }
    {
        Result r = solveOrthogonal("x = c");
        expect(r.ok, "dedicated orthogonal solver: vertical family");
        expect(r.orthogonal && toPlain(r.orthogonal) == "y",
               "vertical family produces horizontal trajectories");
        expect(r.orthogonalSlope && r.orthogonalSlope->isZero(),
               "horizontal trajectories expose slope 0 for the plot");
    }
    {
        Result r = solveOrthogonal("y = c");
        expect(r.ok, "dedicated orthogonal solver: horizontal family");
        expect(r.orthogonal && toPlain(r.orthogonal) == "x",
               "horizontal family produces vertical trajectories");
    }
    {
        // The family is quadratic in c, so the numeric root solver cannot be
        // used; the symbolic branch substitution has to eliminate c instead.
        Result r = solveOrthogonal("(y-c)^2 = x");
        expect(r.ok, "dedicated orthogonal solver: nonlinear parameter");
        expect(r.orthogonal != nullptr, "nonlinear-in-c family produces trajectories");
        expect(!r.warnings.empty(), "nonlinear-in-c family warns about the branch choice");
    }
    {
        Result r = solveOblique("y = c", "c", 45.0);
        expect(r.ok, "dedicated oblique solver: horizontal family");
        expect(r.typeId == "oblique", "oblique solver reports its own type");
        expect(r.trajectory != nullptr, "horizontal family produces oblique lines");
        expect(r.trajectory && toPlain(r.trajectory) == "C + x",
               "45-degree oblique trajectories of y = c are y = x + C, got " +
                   (r.trajectory ? toPlain(r.trajectory) : std::string("<null>")));
        expect(r.trajectoryExplicit, "oblique lines are explicit");
        expect(r.trajectorySlope && toPlain(r.trajectorySlope) == "1",
               "oblique slope is tan(45) = 1");
        expect(!r.steps.empty() && r.steps.back().title == "Oblique family",
               "oblique derivation ends with exactly one family answer step");
    }
    {
        Result r = solveOblique("x = c", "c", 45.0);
        expect(r.ok, "dedicated oblique solver: vertical family");
        expect(r.trajectory && toPlain(r.trajectory) == "C + x",
               "45-degree oblique trajectories of x = c are y = x + C");
        expect(r.trajectorySlope && toPlain(r.trajectorySlope) == "1",
               "vertical family exposes the cotangent slope");
    }
    {
        Result r = solveOblique("x^2 + y^2 = c^2", "c", 45.0);
        expect(r.ok, "dedicated oblique solver: circles at 45 degrees");
        expect(r.trajectory != nullptr, "circles produce oblique spirals");
        expect(r.closedForm, "oblique spirals have an implicit closed form");
        bool hasObliqueOde = false;
        for (const auto& step : r.steps)
            if (step.title.find("Oblique-trajectory equation") != std::string::npos)
                hasObliqueOde = true;
        expect(hasObliqueOde, "oblique steps show the oblique ODE");
        expect(!r.steps.empty() && r.steps.back().title == "Oblique family",
               "oblique circle derivation has one final answer");
        for (const auto& step : r.steps)
            expect(step.title != "General solution",
                   "oblique steps do not duplicate the final answer");
    }
    {
        Result r = solveOblique("x^2 + y^2 = c^2", "c", 90.0);
        expect(r.ok, "oblique solver at 90 degrees");
        expect(r.trajectory && toPlain(r.trajectory) == "C*x",
               "90-degree oblique trajectories reduce to the orthogonal family");
        expect(r.trajectoryAngle == 90.0, "right-angle request is normalised to 90");
        expect(r.orthogonal != nullptr,
               "right-angle oblique request also fills the orthogonal fields");
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
        // The homogeneous path also catches this scaled equation; its implicit
        // answer -1/2 ln|x^2 + xy + y^2| = C is the same potential.
        expect(r.typeId == "exact" || r.typeId == "integrating_factor" ||
                   r.typeId == "homogeneous",
               "scaled equation recognized after cancelling x");
        const std::string sol = r.solution ? toPlain(r.solution) : "";
        expect(sol.find("x^2 + y^2 + x*y") != std::string::npos ||
                   sol.find("y^2/x^2") != std::string::npos,
               "scaled equation has the potential x^2 + y^2 + x*y (or equivalent)");
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
    {
        Result r;
        Ex F = Add(Deriv(Sym("y"), "x", 1), Mul(Num(-1), Sym("y")));
        verifyExplicit(r, F, parseExpr("exp(x)"), "x", "y", true);
        expect(r.checks.size() == 1 && r.checks[0].ok,
               "verification accepts exp(x) for y' = y");
        Result bad;
        verifyExplicit(bad, F, parseExpr("x^2"), "x", "y", true);
        expect(bad.checks.size() == 1 && !bad.checks[0].ok,
               "verification rejects x^2 for y' = y");
    }
    {
        Result r = solveEnvelope("(x-c)^2+y^2=1");
        expect(r.ok, "horizontal envelope computed");
        expect(r.envelope.size() == 1, "one envelope curve for a horizontal envelope");
        expect(r.envelope.size() == 1 && r.envelope[0] &&
                   toPlain(r.envelope[0]).find("y^2 - 1") != std::string::npos,
               "horizontal envelope is y^2 = 1");
    }
    {
        Result r = solveWronskian({"1e-9*x", "1"});
        expect(r.ok && r.independent, "tiny Wronskian is not treated as a proof of dependence");
        expect(!r.warnings.empty(), "numerically tiny Wronskian produces a warning");
    }
    {
        Result r = solve("(y')^2 = 1");
        expect(r.ok, "quadratic-in-y' equation solved");
        expect(!r.warnings.empty(), "a single returned branch is disclosed");
    }

    // Step-by-step contract: one real mathematical action per step. These
    // counts are documented in INFINIT_TECHNICAL_AUDIT.md and guard against
    // reintroducing padded or duplicated setup steps.
    {
        Result r = solve("y' + y = x");
        expect(r.ok && r.steps.size() == 3, "linear equation emits 3 steps");
        for (const auto& step : r.steps)
            expect(step.title.find("Solve for y") == std::string::npos,
                   "linear steps do not include a separate solve-for-y' step");
    }
    {
        Result r = solve("(2*x+y) + (x+2*y)*y' = 0");
        expect(r.ok && r.steps.size() == 3, "exact equation emits 3 steps");
        expect(r.steps.size() >= 1 && r.steps[0].title == "Exactness test" &&
                   r.steps[0].math && toPlain(r.steps[0].math) == "1",
               "exactness step shows the shared partial derivative");
    }
    {
        Result r = solve("(2*y^2+3*x)dx + 2*x*y dy = 0");
        expect(r.ok && r.steps.size() == 5, "integrating-factor equation emits 5 steps");
    }
    {
        Result r = solve("y'' - 3*y' + 2*y = 0");
        expect(r.ok && r.steps.size() == 3, "homogeneous second-order equation emits 3 steps");
    }
    {
        Result r = solve("y'' + y = cos(x)");
        expect(r.ok && r.steps.size() == 6, "non-homogeneous second-order equation emits 6 steps");
    }
    {
        Result r = solveWronskian({"x", "x^2", "x^3"}, "x", "en");
        expect(r.ok && r.steps.size() == 4, "Wronskian emits 4 steps");
    }
    {
        Result r = solveEnvelope("y = c*x + c^2", "c", false, "x", "y", "en");
        expect(r.ok && r.steps.size() == 3, "envelope emits 3 steps");
    }

    // ------------------------------------------------ higher-order reductions
    // Missing dependent variable: y' = p, then integrate p once more.
    {
        Result r = solve("y'' = 6*x");
        expect(r.ok, "y''=6x solved through the missing-y path");
        expect(r.typeId == "direct_integration", "y''=6x keeps direct integration");
        expect(r.order == 2, "y''=6x reports order 2");
        expect(r.isExplicit && r.explicitY &&
                   toPlain(r.explicitY) == "C2 + x^3 + C*x",
               "y''=6x answer is x^3 + Cx + C2");
        expect(r.checks.size() == 1 && r.checks[0].ok, "y''=6x verified");
    }
    {
        Result r = solve("y'' = y'^2");
        expect(r.ok, "y''=y'^2 solved");
        expect(r.typeId == "reduction_missing_y", "y''=y'^2 classified as missing y");
        expect(r.checks.size() == 1 && r.checks[0].ok, "y''=y'^2 verified");
    }
    {
        Result r = solve("y'' = y'^3");
        expect(r.ok, "y''=y'^3 solved (negative-power isolation)");
        expect(r.isExplicit, "y''=y'^3 explicit");
        expect(r.checks.size() == 1 && r.checks[0].ok, "y''=y'^3 verified");
    }
    {
        Result r = solve("y'' = 1/y'");
        expect(r.ok, "y''=1/y' solved");
        expect(r.checks.size() == 1 && r.checks[0].ok, "y''=1/y' verified");
    }
    {
        Result r = solve("y'' = exp(y')");
        expect(r.ok, "y''=exp(y') solved");
        expect(r.checks.size() == 1 && r.checks[0].ok, "y''=exp(y') verified");
    }
    {
        Result r = solve("y'' = exp(-y')");
        expect(r.ok, "y''=exp(-y') solved");
        expect(r.checks.size() == 1 && r.checks[0].ok, "y''=exp(-y') verified");
    }
    {
        Result r = solve("y''' = y''");
        expect(r.ok, "y'''=y'' solved");
        expect(r.order == 3, "y'''=y'' reports order 3");
        expect(r.checks.size() == 1 && r.checks[0].ok, "y'''=y'' verified");
    }
    // Missing independent variable: y' = p(y), y'' = p·dp/dy.
    {
        Result r = solve("y'' = y*y'");
        expect(r.ok, "y''=y*y' solved");
        expect(r.typeId == "reduction_missing_x", "y''=y*y' classified as missing x");
        expect(r.checks.size() == 1 && r.checks[0].ok, "y''=y*y' verified");
    }
    {
        Result r = solve("y'' + y*y' = 0");
        expect(r.ok, "y''+y*y'=0 solved");
        expect(r.checks.size() == 1 && r.checks[0].ok,
               "y''+y*y'=0 real branch verified");
    }
    {
        Result r = solve("y'' = y^2");
        expect(r.ok, "y''=y^2 reduced");
        expect(r.typeId == "reduction_missing_x", "y''=y^2 classified as missing x");
        expect(!r.isExplicit, "y''=y^2 stays implicit (no elementary y(x))");
        expect(r.checks.size() == 1 && r.checks[0].ok, "y''=y^2 implicit relation verified");
    }
    {
        Result r = solve("y'' = y'^2 + y");
        expect(r.ok, "y''=y'^2+y reduced");
        expect(r.typeId == "reduction_missing_x", "y''=y'^2+y classified as missing x");
    }
    // A negative numeric base with an even root used to recurse forever in Pow.
    {
        Result r = solve("y'' = -y^2");
        expect(r.ok, "negative-base square root does not crash");
        expect(r.typeId == "reduction_missing_x", "y''=-y^2 classified as missing x");
    }

    // No closed form: keep the answer honest and expose a clean RK4 RHS.
    {
        Result r = solve("y'' = y' + y^2");
        expect(r.ok, "y''=y'+y^2 returns the numeric fallback");
        expect(r.numericOnly, "numeric fallback is labelled numeric");
        expect(r.typeId == "numeric_second_order", "numeric fallback has its own type");
        expect(r.numericRhs == "y^2 + y'",
               "numeric RHS is normalised, got " + r.numericRhs);
        expect(!r.closedForm, "numeric fallback has no closed form");
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
    raw = ode_solve("{\"mode\":\"wronskian\",\"functions\":\"sin(x)\"}");
    expect(raw != nullptr, "C ABI Wronskian accepts a single string");
    if (raw) {
        std::string response(raw);
        expect(response.find("\"ok\":true") != std::string::npos,
               "C ABI single-function Wronskian is ok");
        expect(response.find("\"independent\":true") != std::string::npos,
               "C ABI single-function Wronskian is independent");
        ode_free(raw);
    }

    // Laplace transform of t*e^(2t).  The generic constant-multiple branch
    // used to recurse on itself forever when a bare factor t was present.
    raw = ode_solve(
        "{\"mode\":\"topic\",\"topic\":\"laplace\",\"lang\":\"en\",\"f\":\"t*exp(2*t)\"}");
    expect(raw != nullptr, "C ABI Laplace response");
    if (raw) {
        std::string response(raw);
        expect(response.find("\"ok\":true") != std::string::npos,
               "Laplace t*exp(2t) is solved");
        expect(response.find("(s - 2)^-2") != std::string::npos,
               "Laplace t*exp(2t) = (s-2)^-2");
        ode_free(raw);
    }

    // M2: validations serialise keys, plain params and separate math.
    raw = ode_solve(
        "{\"mode\":\"topic\",\"topic\":\"fourier_series\",\"lang\":\"en\","
        "\"f\":\"x^2 + 3*exp(3*x)\",\"L\":3.141592653589793,\"n\":5}");
    expect(raw != nullptr, "C ABI Fourier response");
    if (raw) {
        std::string response(raw);
        expect(response.find("\"name_key\":\"check.constant_term\"") !=
                   std::string::npos,
               "C ABI check carries name_key");
        expect(response.find("\"detail_math\":\"a0/2 = ") != std::string::npos,
               "C ABI check carries separate detail math");
        expect(response.find("\"params\":{\"x\":") != std::string::npos,
               "C ABI check carries numeric params");
        expect(response.find("\"name\":\"\"") != std::string::npos,
               "C ABI migrated check has no name sentence");
        ode_free(raw);
    }

    // Parser fixtures (P1, P3, P4): superscripts, implicit multiplication and
    // glued functions must all canonicalise to the documented forms.
    {
        const std::pair<const char*, const char*> cases[] = {
            {"e^2t", "exp(2*t)"},
            {"e^5x", "exp(5*x)"},
            {"3e^3x", "3*exp(3*x)"},
            {"csc²x", "csc(x)^2"},
            {"csc²(x)", "csc(x)^2"},
            {"sec²x", "sec(x)^2"},
            {"sinx", "sin(x)"},
            {"csinx", "c*sin(x)"},
            {"x⁶y", "y*x^6"},
            {"4e^x y", "4*y*exp(x)"},
            {"2x", "2*x"},
            {"x × y", "x*y"},
            {"x + secx²x", "x + x*sec(x)^2"},
        };
        for (const auto& item : cases) {
            const std::string got = toPlain(parseExpr(item.first));
            expect(got == item.second,
                   std::string("parse ") + item.first + " -> " + item.second +
                       " (got " + got + ")");
        }
    }

    // P3: an invalid multi-byte character must raise a coded error instead of
    // truncating UTF-8 mid-character.
    {
        bool coded = false;
        try {
            parseExpr(u8"\u062a\u0627\u0628\u0639");
        } catch (const EngineError& e) {
            coded = e.code == "bad_characters";
        } catch (...) {
        }
        expect(coded, "Persian letters raise bad_characters");
    }

    // E1: a singular start point is reported with its location; moving the
    // start away from the pole still solves.
    {
        TopicRequest req;
        req.topic = "heun";
        req.strs["f"] = "x + csc(x)^2";
        req.nums["x0"] = 0;
        req.nums["y0"] = 1;
        req.nums["h"] = 0.1;
        req.nums["n"] = 10;
        Result r = solveTopic(req);
        expect(!r.ok, "Heun at a pole does not claim success");
        expect(r.errorCode == "singular", "Heun at a pole reports singular");
        expect(r.hasErrorPoint && std::fabs(r.errorPoint) < 1e-9,
               "Heun reports the singular point x=0");

        req.nums["x0"] = 1;
        Result shifted = solveTopic(req);
        expect(shifted.ok, "Heun away from the pole solves");
        expect(shifted.hasNumeric && std::fabs(shifted.numericValue - 3.60218) < 5e-4,
               "Heun x + csc^2(x) at x0=1 matches the known value");
    }

    // M3: topic derivation steps carry catalogue keys and keep formulas out
    // of the sentence fields.
    {
        TopicRequest req;
        req.topic = "linear_system";
        req.nums["a"] = 0;
        req.nums["b"] = 1;
        req.nums["c"] = -1;
        req.nums["d"] = 0;
        Result r = solveTopic(req);
        expect(r.ok, "linear system solves");
        bool keyedTitle = false, keyedNote = false, separateMath = false;
        for (const auto& s : r.steps) {
            expect(s.title.empty(), "topic steps carry no title sentence");
            if (!s.titleKey.empty()) keyedTitle = true;
            if (!s.noteKey.empty()) keyedNote = true;
            if (!s.titleKey.empty() && !s.noteMath.empty() && s.note.empty())
                separateMath = true;
        }
        expect(keyedTitle, "topic step titles are keys");
        expect(keyedNote, "topic step notes are keys");
        expect(separateMath, "formula notes travel as noteMath");
    }

    // E2: a non-integrable Fourier input is refused; a genuine jump keeps the
    // Gibbs note and interior-point validation.
    {
        TopicRequest req;
        req.topic = "fourier_series";
        req.strs["f"] = "1/cos(x)^2";
        req.nums["L"] = 3.141592653589793;
        req.nums["n"] = 5;
        Result r = solveTopic(req);
        expect(!r.ok, "Fourier refuses sec^2 on [-pi, pi]");
        expect(r.errorCode == "singular", "Fourier reports the singular point");
    }
    {
        TopicRequest req;
        req.topic = "fourier_series";
        req.strs["f"] = "6000000000";
        req.nums["L"] = 3.141592653589793;
        req.nums["n"] = 5;
        Result r = solveTopic(req);
        expect(!r.ok, "Fourier rejects an overflowing coefficient");
        expect(r.errorCode == "not_integrable",
               "overflowing coefficient reports not_integrable");
    }
    {
        TopicRequest req;
        req.topic = "fourier_series";
        req.strs["f"] = "x^2 + 3*exp(3*x)";
        req.nums["L"] = 3.141592653589793;
        req.nums["n"] = 5;
        Result r = solveTopic(req);
        expect(r.ok, "Fourier still solves a continuous interior function");
        bool gibbs = false;
        for (const auto& w : r.warnings) {
            if (w == "step.note.endpoint_jump") gibbs = true;
        }
        expect(gibbs, "Fourier warns about the endpoint jump and Gibbs");
        bool interior = false;
        bool constantTerm = false;
        for (const auto& c : r.checks) {
            expect(!c.nameKey.empty(), "every Fourier check has a name key");
            expect(c.name.empty(), "Fourier check carries no name sentence");
            expect(!c.detailKey.empty() || !c.detailMath.empty(),
                   "every Fourier check has a detail key or math");
            expect(c.detail.empty(), "Fourier check carries no detail sentence");
            if (c.nameKey == "check.partial_sum_at_x" &&
                c.params.find("\"x\":0") != std::string::npos) {
                interior = true;
            }
            if (c.nameKey == "check.constant_term") {
                constantTerm = true;
                expect(c.detailMath.rfind("a0/2 = ", 0) == 0,
                       "constant term carries a0/2 as separate math");
            }
        }
        expect(interior, "Fourier validates the partial sum at an interior point");
        expect(constantTerm, "Fourier reports the constant term by key");
    }

    // E3: a singular endpoint removes only the affected extremum, and the
    // interior minimum survives.
    {
        Result r = optimiseFunction("1/sin(x)^2 + 6*exp(5)*x^3", "x", 0, 2, "en");
        expect(r.ok, "optimisation with a singular endpoint completes");
        expect(!r.maximum, "no maximum is reported when f is unbounded above");
        expect(r.minimum != nullptr, "the interior minimum is still reported");
        bool ok = false;
        const double minimum = evalReal(r.minimum, {}, &ok);
        expect(ok && minimum > 27.0 && minimum < 33.0,
               "the interior minimum is near 30");
    }

    // E3/E4: an extra free symbol is named in a warning, never silently zero.
    {
        Result r = optimiseFunction("xy + 6*x^2*exp(5*x) + csc(x)^2", "x", 0, 2, "en");
        expect(r.ok, "optimisation with an extra symbol completes");
        bool named = false;
        for (const auto& w : r.warnings) {
            if (w.find("y") != std::string::npos) named = true;
        }
        expect(named, "optimisation warns that y was treated as a parameter");
        expect(!r.maximum, "extra-symbol case has no maximum");
    }
    {
        Result r = taylorSeries("x + 2*y", "x", 0, 2, "en");
        expect(r.ok, "Taylor series with an extra symbol completes");
        bool named = false;
        for (const auto& w : r.warnings) {
            if (w.find("y") != std::string::npos) named = true;
        }
        expect(named, "Taylor validation names the extra symbol y");
        expect(r.checks.size() >= 2, "Taylor validation uses non-zero samples");
    }

    // E10: unknown functions and malformed input carry stable error codes.
    {
        Result unknown = taylorSeries("foo(x)", "x", 0, 2, "en");
        expect(!unknown.ok, "unknown function is rejected");
        expect(unknown.errorCode == "unknown_function",
               "unknown function carries its code");
    }

    if (failures == 0) {
        std::cout << "All ODE regression tests passed.\n";
        return 0;
    }
    std::cout << failures << " regression failure(s).\n";
    return 1;
}
