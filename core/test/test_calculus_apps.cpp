// Regression tests for the applications of the derivative and the direction
// field slope extraction (differential forms, Clairaut/Lagrange branches).
#include "calculus_apps.hpp"
#include "graph.hpp"
#include "odecore.h"
#include "solver.hpp"

#include <cmath>
#include <limits>
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

static std::string plain(const Ex& e) { return e ? toPlain(e) : std::string("<null>"); }

// The JSON API is what the Flutter UI actually calls, so the field tests go
// through ode_solve exactly like the app does.
static std::string api(const std::string& request) {
    char* raw = ode_solve(request.c_str());
    std::string out = raw ? raw : "";
    ode_free(raw);
    return out;
}

static bool apiOk(const std::string& request) {
    const std::string s = api(request);
    return s.find("\"ok\":true") != std::string::npos;
}

static void fieldCases() {
    // Every first-order form the UI offers must produce a direction field.
    const std::vector<std::string> equations = {
        "y' = y",
        "(2x+y) + (x+2y)y' = 0",
        "(x+y)dx + (x+2y)dy = 0",
        "cos(y)dy + sin(x)dx = 0",
        "(3*x^2 + 2*y)dx + (2*x - 3*y^2)dy = 0",
        "x*dx + y*dy = 0",
        "y = x*y' + (y')^2",
        "y = 2*x*y' + (y')^2",
        "dy/dx = -x/y",
    };
    for (const auto& eq : equations) {
        const std::string req =
            "{\"mode\":\"field\",\"input\":\"" + eq +
            "\",\"x\":\"x\",\"y\":\"y\",\"xmin\":-4,\"xmax\":4,\"ymin\":-4,\"ymax\":4,"
            "\"cols\":5,\"rows\":5,\"curves\":3}";
        expect(apiOk(req), "field plots: " + eq);
    }

    // A second-order equation is honestly rejected, not silently plotted.
    expect(!apiOk("{\"mode\":\"field\",\"input\":\"y'' + y = 0\",\"x\":\"x\",\"y\":\"y\"}"),
           "field rejects second-order equations");

    // The exact form must report the differential reading y' = -M/N.
    const std::string s = api(
        "{\"mode\":\"field\",\"input\":\"(x+y)dx + (x+2y)dy = 0\",\"x\":\"x\",\"y\":\"y\","
        "\"xmin\":-4,\"xmax\":4,\"ymin\":-4,\"ymax\":4,\"cols\":5,\"rows\":5,\"curves\":2}");
    expect(s.find("x + 2") != std::string::npos, "exact form reports y' = -M/N");
    // Trajectories are real curves, so both the slope grid and the curves exist.
    expect(s.find("\"curves\":[{\"ic_x\"") != std::string::npos,
           "exact form produces trajectory curves");
}

static void orthogonalApiCase() {
    // The Flutter UI sends mode "orthogonal" for the dedicated page. It must
    // not fall back to the envelope response with a warning-only orthogonal
    // field.
    const std::string s = api(
        "{\"mode\":\"orthogonal\",\"input\":\"x^2 + y^2 = c^2\",\"c\":\"c\","
        "\"x\":\"x\",\"y\":\"y\",\"lang\":\"en\"}");
    expect(s.find("\"ok\":true") != std::string::npos,
           "orthogonal API request succeeds");
    expect(s.find("\"type_id\":\"orthogonal\"") != std::string::npos,
           "orthogonal API request keeps its own type id");
    expect(s.find("\"orthogonal\":{") != std::string::npos,
           "orthogonal API request returns the orthogonal family");
    expect(s.find("\"orthogonal_explicit\":true") != std::string::npos,
           "orthogonal API request reports the explicit form");
    expect(s.find("\"orthogonal_slope\":{") != std::string::npos,
           "orthogonal API request exposes the solved slope for the plot");
    expect(s.find("\"family_branches\":[{") != std::string::npos,
           "orthogonal API request exposes the family branches for the plot");
}

static void obliqueApiCase() {
    const std::string s = api(
        "{\"mode\":\"oblique\",\"input\":\"y = c\",\"c\":\"c\",\"angle\":45,"
        "\"x\":\"x\",\"y\":\"y\",\"lang\":\"en\"}");
    expect(s.find("\"ok\":true") != std::string::npos,
           "oblique API request succeeds");
    expect(s.find("\"type_id\":\"oblique\"") != std::string::npos,
           "oblique API request keeps its own type id");
    expect(s.find("\"trajectory\":{") != std::string::npos,
           "oblique API request returns the trajectory family");
    expect(s.find("\"trajectory_explicit\":true") != std::string::npos,
           "oblique API request reports the explicit form");
    expect(s.find("\"trajectory_slope\":{") != std::string::npos,
           "oblique API request exposes the solved slope for the plot");
    expect(s.find("\"trajectory_angle\":45") != std::string::npos,
           "oblique API request echoes the crossing angle");
    expect(s.find("\"trajectory_mode\":\"oblique\"") != std::string::npos,
           "oblique API request reports the trajectory mode");

    // 120 and -60 degrees share tan = -sqrt(3); the exact surd must survive
    // instead of collapsing to a decimal rational approximation.
    for (int angle : {120, -60}) {
        const std::string t = api(
            "{\"mode\":\"oblique\",\"input\":\"y - c*x\",\"c\":\"c\",\"angle\":" +
            std::to_string(angle) + ",\"x\":\"x\",\"y\":\"y\",\"lang\":\"en\"}");
        expect(t.find("\"ok\":true") != std::string::npos,
               "oblique negative-angle request succeeds");
        expect(t.find("sqrt(3)") != std::string::npos,
               "oblique angle keeps the exact sqrt(3) slope");
        expect(t.find("1732/1000") == std::string::npos && t.find("1.732") == std::string::npos,
               "oblique angle does not use a decimal approximation");
    }

    // Crossing angles are defined modulo 180 degrees: -90 and 270 are the same
    // as +90 (orthogonal), and 180 is the same as 0.  A previous version fed
    // the angle straight into tan() and leaked a slope of about -1.6e16.
    for (int angle : {-90, 270}) {
        const std::string t = api(
            "{\"mode\":\"oblique\",\"input\":\"y = c*x\",\"c\":\"c\",\"angle\":" +
            std::to_string(angle) + ",\"x\":\"x\",\"y\":\"y\",\"lang\":\"en\"}");
        expect(t.find("\"ok\":true") != std::string::npos,
               "oblique right-angle request succeeds for every representative");
        expect(t.find("\"trajectory_angle\":90") != std::string::npos,
               "oblique right-angle request normalises the angle to 90");
        expect(t.find("16331239") == std::string::npos,
               "oblique right-angle request has no tan-pole coefficient");
    }
    for (int angle : {0, 180, 360}) {
        const std::string t = api(
            "{\"mode\":\"oblique\",\"input\":\"x = c\",\"c\":\"c\",\"angle\":" +
            std::to_string(angle) + ",\"x\":\"x\",\"y\":\"y\",\"lang\":\"en\"}");
        expect(t.find("\"ok\":true") != std::string::npos,
               "oblique full-turn request succeeds for a vertical family");
        expect(t.find("\"trajectory_angle\":0") != std::string::npos,
               "oblique full-turn request normalises the angle to 0");
    }
}

static void domainCase() {
    // 1/cos(x) is trigonometric, with poles at pi/2 + k*pi.  The reciprocal
    // must not be classified as rational, and the pole must be refined to
    // pi/2 instead of left at a grid approximation.
    DomainOutput d = analyseDomain("1/cos(x)", "x", 0.0, 3.0, 1201);
    expect(d.error.empty(), "domain: 1/cos(x) succeeds");
    expect(d.kind == "trigonometric", "domain: 1/cos(x) is trigonometric");
    expect(d.asymptotes.size() == 1, "domain: 1/cos(x) has one pole in (0,3)");
    if (d.asymptotes.size() == 1)
        expect(std::fabs(d.asymptotes[0] - 1.5707963267948966) < 1e-4,
               "domain: the pole is refined to pi/2");

    // cot(x) is continuous on its runs and crosses zero at every multiple of
    // pi, so its range is one interval, not two branches split at zero.
    DomainOutput cot = analyseDomain("cot(x)", "x", -5.0, 5.0, 2401);
    expect(cot.error.empty(), "domain: cot(x) succeeds");
    expect(cot.range.size() == 1, "domain: cot(x) range is one interval");
    if (cot.range.size() == 1) {
        expect(cot.range[0].loInfinite && cot.range[0].hiInfinite,
               "domain: cot(x) range is (-inf, inf)");
    }

    // A removable hole can hold the extreme value: sin(x)/x reaches 1 as
    // x -> 0 even though x = 0 is undefined, so the supremum is 1.
    DomainOutput sinc = analyseDomain("sin(x)/x", "x", -5.0, 5.0, 2401);
    expect(sinc.error.empty(), "domain: sin(x)/x succeeds");
    bool maxIsOne = false;
    for (const auto& in : sinc.range) {
        if (!in.hiInfinite && std::fabs(in.hi - 1.0) < 1e-9) maxIsOne = true;
    }
    expect(maxIsOne, "domain: sin(x)/x range supremum is 1 from the hole limit");
}

static void behaviourCase() {
    Result r = analyseFunction("x^3 - 3*x", "x", -3, 3, "en");
    expect(r.ok, "behaviour: x^3-3x solved");
    expect(r.critical.size() == 2, "behaviour: two critical points");
    bool hasMax = false, hasMin = false;
    for (const auto& c : r.critical) {
        if (c.kind == "maximum") hasMax = true;
        if (c.kind == "minimum") hasMin = true;
    }
    expect(hasMax && hasMin, "behaviour: maximum at -1 and minimum at 1");
    expect(r.inflections.size() == 1, "behaviour: one inflection point");
    if (r.inflections.size() == 1) {
        expect(plain(r.inflections[0].x) == "0", "behaviour: inflection at x = 0");
    }
    expect(r.pieces.size() == 3, "behaviour: three monotonicity pieces");
    bool rising = false, falling = false;
    for (const auto& p : r.pieces) {
        if (p.sign > 0) rising = true;
        if (p.sign < 0) falling = true;
    }
    expect(rising && falling, "behaviour: both trends detected");
    expect(r.steps.size() == 6, "behaviour: solution path has 6 steps");
}

static void tangentCase() {
    Result r = tangentAndNormal("x^2", "x", "1", "en");
    expect(r.ok, "tangent: x^2 at 1");
    // y - 1 = 2(x - 1)  =>  y = 2x - 1
    expect(plain(r.tangent) == "2*x - 1" || plain(r.tangent) == "-1 + 2*x",
           "tangent line is 2x - 1, got " + plain(r.tangent));
    // Normal slope is -1/2 and it passes through (1, 1): y = 3/2 - x/2.
    expect(r.normalSlope && plain(r.normalSlope) == "-1/2", "normal slope is -1/2");
    expect(r.slope && plain(r.slope) == "2", "tangent slope is 2");

    // A horizontal tangent has a vertical normal, which has no finite slope.
    Result flat = tangentAndNormal("x^2", "x", "0", "en");
    expect(flat.ok, "tangent: horizontal case solved");
    expect(flat.normalSlope == nullptr, "vertical normal has no slope");
    expect(plain(flat.tangent) == "0", "horizontal tangent is y = 0");
    expect(r.steps.size() == 4, "tangent: solution path has 4 steps");
}

static void optimisationCase() {
    Result r = optimiseFunction("x^3 - 3*x", "x", -2, 2, "en");
    expect(r.ok, "optimise: x^3-3x on [-2,2]");
    // Values: f(-2) = -2, f(-1) = 2, f(1) = -2, f(2) = 2.
    expect(r.maximum && plain(r.maximum) == "2", "optimise: maximum value 2");
    expect(r.minimum && plain(r.minimum) == "-2", "optimise: minimum value -2");
    // All four candidates are compared, including both endpoints.
    expect(r.candidates.size() == 4, "optimise: compares two critical and two endpoint values");
    expect(r.steps.size() == 4, "optimise: solution path has 4 steps");
}

static void meanValueCase() {
    Result r = meanValueTheorems("x^2", "x", 0, 2, "en");
    expect(r.ok, "mvt: x^2 on [0,2]");
    // The secant slope is 2 and f'(c) = 2c = 2 gives c = 1.
    expect(r.slope && plain(r.slope) == "2", "mvt: secant slope is 2");
    expect(r.points.size() == 1, "mvt: exactly one interior point");
    if (r.points.size() == 1) {
        expect(plain(r.points[0]) == "1", "mvt: c = 1, got " + plain(r.points[0]));
    }
    expect(r.hypothesesContinuous, "mvt: continuity hypothesis checked");
    expect(r.hypothesesDifferentiable, "mvt: differentiability hypothesis checked");
    expect(!r.rolleApplies, "mvt: Rolle does not apply when f(a) != f(b)");
    expect(r.steps.size() == 4, "mvt: solution path has 4 steps");

    // Rolle: x^2 - 4x + 3 has f(1) = f(3) = 0, so c = 2.
    Result rolle = meanValueTheorems("x^2 - 4*x + 3", "x", 1, 3, "en");
    expect(rolle.ok, "mvt: Rolle case solved");
    expect(rolle.rolleApplies, "mvt: Rolle detected f(a) = f(b)");
    expect(rolle.points.size() == 1 && plain(rolle.points[0]) == "2",
           "mvt: Rolle point is 2, got " +
               (rolle.points.empty() ? std::string("<none>") : plain(rolle.points[0])));
}

static void parserImplicitProductCase() {
    Ex e = parseODEExpr("2XY", "x", nullptr, {"x", "e", "pi"});
    expect(plain(e) == "2*X*Y", "implicit product: 2XY -> 2*X*Y, got " + plain(e));
    Ex f = parseODEExpr("2xy", "x", nullptr, {"x", "y", "c"});
    expect(plain(f) == "2*x*y", "implicit product: 2xy -> 2*x*y, got " + plain(f));
    Ex g = parseODEExpr("exp(x)", "x", nullptr, {"x", "e", "pi"});
    expect(plain(g) == "exp(x)", "parser keeps exp(x) intact, got " + plain(g));
    Ex h = parseExpr("atan(x)");
    expect(plain(h) == "atan(x)", "parser keeps atan(x) intact, got " + plain(h));
    Ex i = parseExpr("alpha");
    expect(plain(i) == "alpha", "parser keeps multi-letter variable alpha, got " + plain(i));
}

static void rootBlowupCase() {
    // The reported crash: a constant target used to enumerate all 4000 samples.
    Result mvt = meanValueTheorems("2XY", "x", 0, 1, "en");
    expect(mvt.ok, "mvt: 2XY solved");
    expect(mvt.points.size() == 1, "mvt: 2XY reports one representative point, got " +
                                       std::to_string(mvt.points.size()));
    if (!mvt.points.empty()) {
        expect(plain(mvt.points[0]) == "1/2", "mvt: 2XY midpoint is 1/2");
    }

    Result linear = meanValueTheorems("x", "x", 0, 1, "en");
    expect(linear.ok && linear.points.size() == 1,
           "mvt: linear function reports one representative point");

    Result constant = meanValueTheorems("5", "x", 0, 1, "en");
    expect(constant.ok && constant.points.size() == 1,
           "mvt: constant function reports one representative point");

    Result expCase = meanValueTheorems("exp(x)", "x", 0, 1, "en");
    expect(expCase.ok, "mvt: exp(x) solved");
    expect(expCase.normalized && plain(expCase.normalized) == "exp(x)",
           "mvt: exp(x) normalized without splitting");

    Result behaviour = analyseFunction("5", "x", 0, 1, "en");
    expect(behaviour.ok && behaviour.critical.empty(),
           "behaviour: constant has no critical points");
    expect(behaviour.inflections.empty(), "behaviour: constant has no inflections");

    Result opt = optimiseFunction("5", "x", 0, 1, "en");
    expect(opt.ok, "optimise: constant solved");
    expect(opt.candidates.size() == 2,
           "optimise: constant compares only endpoints, got " +
               std::to_string(opt.candidates.size()));

    Result cauchy = cauchyMeanValue("2XY", "x", "x", 0, 1, "en");
    expect(cauchy.ok && cauchy.points.size() == 1,
           "cauchy: constant f reports one representative point");
}

static void cauchyCase() {
    Result r = cauchyMeanValue("x^2", "x^3", "x", 0, 1, "en");
    expect(r.ok, "cauchy: x^2 and x^3 on [0,1]");
    // f' = 2x, g' = 3x^2; ratio (1-0)/(1-0) = 1 gives 2c/(3c^2) = 1, so c = 2/3.
    expect(r.points.size() == 1, "cauchy: one interior point");
    if (r.points.size() == 1) {
        expect(plain(r.points[0]) == "2/3", "cauchy: c = 2/3, got " + plain(r.points[0]));
    }
    expect(r.steps.size() == 3, "cauchy: solution path has 3 steps");
    // g(a) = g(b) must be reported as a failed hypothesis, not solved.
    Result bad = cauchyMeanValue("x", "x^2", "x", -1, 1, "en");
    expect(!bad.ok, "cauchy: rejects g(a) = g(b)");
}

static void lhopitalCase() {
    Result r = lhopitalLimit("sin(x)", "x", "x", "0", "en");
    expect(r.ok, "lhopital: sin(x)/x at 0");
    expect(r.solution && plain(r.solution) == "1", "lhopital: limit is 1");
    expect(r.limitExists, "lhopital: limit exists");
    expect(r.steps.size() == 3, "lhopital: solution path has 3 steps");

    // A quotient that is not indeterminate must not be treated as L'Hopital.
    Result direct = lhopitalLimit("x^2", "x", "x", "3", "en");
    expect(direct.ok, "lhopital: direct substitution case solved");
    expect(direct.solution && plain(direct.solution) == "3", "lhopital: limit is 3");

    // 1/x at 0 has different one-sided limits, so the two-sided limit does not
    // exist; that must be reported as a failure of existence, not as an answer.
    Result inf = lhopitalLimit("1", "x", "x", "0", "en");
    expect(inf.ok, "lhopital: 1/x at 0 analysed");
    expect(!inf.limitExists, "lhopital: two-sided limit of 1/x at 0 does not exist");
}

static void relatedRatesCase() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    Result r = relatedRates("x^2 + y^2 = 25", "x", "y", "t", nan, nan, nan, "en");
    expect(r.ok, "related rates: circle relation solved");
    // Differentiating gives 2x x' + 2y y' = 0, so the isolated rate is -y/x.
    expect(r.derivative != nullptr, "related rates: differentiated relation shown");
    expect(r.steps.size() == 2, "related rates: symbolic path has 2 steps");

    // At x = 3, y = 4 with y' = 2: q' = -(y/x) w' = -(4/3)(2) = -8/3.
    Result numeric = relatedRates("x^2 + y^2 = 25", "x", "y", "t", 3.0, 4.0, 2.0, "en");
    expect(numeric.ok, "related rates: numeric instant solved");
    expect(numeric.hasNumeric, "related rates: numeric answer available");
    expect(std::fabs(numeric.numericValue + 8.0 / 3.0) < 1e-9,
           "related rates: rate is -8/3, got " + std::to_string(numeric.numericValue));
    expect(numeric.steps.size() == 3, "related rates: numeric path has 3 steps");
}

int main() {
    fieldCases();
    orthogonalApiCase();
    obliqueApiCase();
    domainCase();
    behaviourCase();
    tangentCase();
    optimisationCase();
    parserImplicitProductCase();
    rootBlowupCase();
    meanValueCase();
    cauchyCase();
    lhopitalCase();
    relatedRatesCase();

    if (failures == 0) {
        std::cout << "All calculus-application tests passed.\n";
        return 0;
    }
    std::cout << failures << " test(s) failed.\n";
    return 1;
}
