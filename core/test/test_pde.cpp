// PDE curriculum topics: first-order transport, characteristics,
// classification and separation of variables.
//
// The tests cover several equation forms per topic and, where a closed form
// exists, compare the native answer with an independently evaluated formula.
#include "numeric.hpp"
#include "solver.hpp"
#include "topics.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <string>

using namespace ode;

static int failures = 0;

static void expect(bool condition, const std::string& what) {
    if (!condition) {
        ++failures;
        std::cout << "FAIL: " << what << "\n";
    }
}

static Result solve(const std::string& topic, const std::map<std::string, double>& nums,
                    const std::map<std::string, std::string>& strs = {}) {
    TopicRequest req;
    req.topic = topic;
    req.lang = "en";
    req.nums = nums;
    req.strs = strs;
    return solveTopic(req);
}

static bool allChecksOk(const Result& r) {
    if (r.checks.empty()) return false;
    for (const auto& c : r.checks) {
        if (!c.ok) return false;
    }
    return true;
}

static bool hasStep(const Result& r, const std::string& titleKey) {
    for (const auto& s : r.steps) {
        if (s.titleKey == titleKey) return true;
    }
    return false;
}

static bool hasCheck(const Result& r, const std::string& nameKey) {
    for (const auto& c : r.checks) {
        if (c.nameKey == nameKey) return true;
    }
    return false;
}

static std::string plain(const Ex& e) { return e ? toPlain(e) : std::string(); }

// Evaluate an expression in x and y and compare with an expected value.
static bool closeAt(const Ex& e, double x, double y, double expected, double tol = 1e-7) {
    std::map<std::string, double> vars{{"x", x}, {"y", y}};
    bool ok = false;
    const double got = evalReal(e, vars, &ok);
    return ok && std::isfinite(got) && std::fabs(got - expected) <= tol * (1.0 + std::fabs(expected));
}

static void testFirstOrder() {
    // Pure transport: the profile is translated, not distorted.
    {
        Result r = solve("pde_first_order", {{"a", 1}, {"b", 1}, {"c", 0}},
                         {{"f", "sin(x)"}});
        expect(r.ok, "transport u_x + u_y = 0 solves");
        expect(plain(r.solution) == "sin(x - y)", "transport of sin(x) gives sin(x - y)");
        expect(allChecksOk(r), "transport residual and initial curve agree");
        expect(hasStep(r, "step.title.write_the_characteristic_system"),
               "transport shows the characteristic system");
    }
    // Constant source with a negative characteristic slope and a polynomial.
    {
        Result r = solve("pde_first_order", {{"a", 2}, {"b", -3}, {"c", 1}},
                         {{"f", "x^2"}});
        expect(r.ok, "2 u_x - 3 u_y = 1 solves");
        expect(closeAt(r.solution, 0.0, -3.0, 5.0),
               "2 u_x - 3 u_y = 1 evaluates at (0,-3)");
        expect(closeAt(r.solution, 1.0, 0.0, 1.0), "source solution keeps u(x,0) = x^2");
        expect(allChecksOk(r), "constant-source checks pass");
    }
    // a = 0: the flow is purely in y.
    {
        Result r = solve("pde_first_order", {{"a", 0}, {"b", 2}, {"c", 3}},
                         {{"f", "exp(x)"}});
        expect(r.ok, "u_y-only equation solves");
        expect(closeAt(r.solution, 0.0, 4.0, 1.0 + 6.0),
               "u_y-only solution is exp(x) + 3y/2");
    }
    // Negative source and an even profile.
    {
        Result r = solve("pde_first_order", {{"a", 1}, {"b", 2}, {"c", -4}},
                         {{"f", "cos(x)"}});
        expect(r.ok, "negative-source transport solves");
        expect(closeAt(r.solution, 0.3, 1.1, std::cos(0.3 - 0.55) - 2.2),
               "negative-source transport matches cos(x - y/2) - 2y");
    }
    // Differentiated profile: the residual must still vanish.
    {
        Result r = solve("pde_first_order", {{"a", -1}, {"b", 3}, {"c", 2}},
                         {{"f", "x^3 - 2*x"}});
        expect(r.ok, "cubic profile solves");
        expect(allChecksOk(r), "cubic profile residual is zero");
        expect(hasCheck(r, "check.initial_curve"), "cubic profile checks the initial curve");
    }
    // Degenerate data: the initial line is itself a characteristic.
    {
        Result r = solve("pde_first_order", {{"a", 1}, {"b", 0}, {"c", 1}},
                         {{"f", "x"}});
        expect(!r.ok, "b = 0 is refused");
        expect(r.errorCode == "degenerate_characteristics",
               "b = 0 reports degenerate_characteristics");
    }
    // A misspelled function is an input error, not a silent zero.
    {
        Result r = solve("pde_first_order", {{"a", 1}, {"b", 1}, {"c", 0}},
                         {{"f", "wibble(x)"}});
        expect(!r.ok, "unknown initial profile is refused");
        expect(r.errorCode == "unknown_function", "unknown profile reports unknown_function");
    }
}

static void testCharacteristics() {
    // Growth along the characteristics: u = e^y sin(x-y).
    {
        Result r = solve("pde_characteristics",
                         {{"a", 1}, {"b", 1}, {"p", 1}, {"q", 0}},
                         {{"f", "sin(x)"}});
        expect(r.ok, "u_x + u_y = u solves");
        expect(closeAt(r.solution, 0.4, 0.7, std::exp(0.7) * std::sin(-0.3)),
               "growth solution is e^y sin(x-y)");
        expect(allChecksOk(r), "growth residual and initial curve agree");
    }
    // Affine source with p ≠ 0: u + q/p grows exponentially.
    {
        Result r = solve("pde_characteristics",
                         {{"a", 2}, {"b", 1}, {"p", 3}, {"q", 1}},
                         {{"f", "x^2"}});
        expect(r.ok, "2 u_x + u_y = 3u + 1 solves");
        expect(closeAt(r.solution, 0.0, 1.0,
                       (4.0 + 1.0 / 3.0) * std::exp(3.0) - 1.0 / 3.0),
               "affine growth matches (f + 1/3) e^(3y) - 1/3");
        expect(allChecksOk(r), "affine growth checks pass");
    }
    // p = 0 reduces to the transport equation with source q.
    {
        Result r = solve("pde_characteristics",
                         {{"a", 1}, {"b", 1}, {"p", 0}, {"q", 2}},
                         {{"f", "x"}});
        expect(r.ok, "p = 0 characteristics solve");
        expect(closeAt(r.solution, 1.0, 3.0, 1.0 - 3.0 + 6.0),
               "p = 0 solution is f(x-y) + 2y");
    }
    // Decay (p < 0) and q = 0.
    {
        Result r = solve("pde_characteristics",
                         {{"a", -1}, {"b", 2}, {"p", -0.5}, {"q", 0}},
                         {{"f", "cos(x)"}});
        expect(r.ok, "decaying characteristics solve");
        expect(closeAt(r.solution, 0.2, 1.5,
                       std::cos(0.2 + 0.75) * std::exp(-0.375)),
               "decay solution is e^(-y/4) cos(x + y/2)");
    }
    // Negative q with a shifted profile.
    {
        Result r = solve("pde_characteristics",
                         {{"a", 3}, {"b", 1}, {"p", 2}, {"q", -4}},
                         {{"f", "x^2 + 1"}});
        expect(r.ok, "negative affine source solves");
        expect(allChecksOk(r), "negative affine source checks pass");
    }
    // Degenerate characteristic direction.
    {
        Result r = solve("pde_characteristics",
                         {{"a", 1}, {"b", 0}, {"p", 1}, {"q", 0}},
                         {{"f", "x"}});
        expect(!r.ok && r.errorCode == "degenerate_characteristics",
               "characteristics refuse b = 0");
    }
}

static void testClassification() {
    // Elliptic, parabolic and hyperbolic representative equations.
    {
        Result r = solve("pde_classify", {{"A", 1}, {"B", 0}, {"C", 1}});
        expect(r.ok, "Laplace equation solves");
        expect(plain(r.solution) == "elliptic", "Laplace equation is elliptic");
        expect(allChecksOk(r), "elliptic discriminant and canonical checks pass");
        expect(hasStep(r, "step.title.write_the_canonical_form"),
               "elliptic equation shows its canonical form");
    }
    {
        Result r = solve("pde_classify", {{"A", 1}, {"B", 2}, {"C", 1}});
        expect(r.ok, "parabolic equation solves");
        expect(plain(r.solution) == "parabolic", "u_xx + 2u_xy + u_yy is parabolic");
        expect(allChecksOk(r), "parabolic checks pass");
    }
    // A rotated parabolic form: the zero direction misses a coarse sampling
    // grid, so the check must use the exact eigenvalue instead.
    {
        Result r = solve("pde_classify", {{"A", 1}, {"B", 1}, {"C", 0.25}});
        expect(r.ok && plain(r.solution) == "parabolic",
               "u_xx + u_xy + u_yy/4 is parabolic");
        expect(allChecksOk(r),
               "rotated parabolic check does not miss the zero direction");
        bool canonical = false;
        for (const auto& s : r.steps) {
            if (s.titleKey == "step.title.write_the_canonical_form" &&
                toPlain(s.math).find("u_etaeta") != std::string::npos) {
                canonical = true;
            }
        }
        expect(canonical, "parabolic canonical form quotes u_etaeta");
    }
    {
        Result r = solve("pde_classify", {{"A", 1}, {"B", 4}, {"C", 1}});
        expect(r.ok, "hyperbolic equation solves");
        expect(plain(r.solution) == "hyperbolic", "u_xx + 4u_xy + u_yy is hyperbolic");
        expect(allChecksOk(r), "hyperbolic checks pass");
        expect(hasStep(r, "step.title.find_the_characteristic_curves"),
               "hyperbolic equation shows both characteristic families");
    }
    // Wave-like equation with a positive discriminant.
    {
        Result r = solve("pde_classify", {{"A", 1}, {"B", 0}, {"C", -1}});
        expect(r.ok && plain(r.solution) == "hyperbolic", "u_xx - u_yy is hyperbolic");
        expect(allChecksOk(r), "wave-like discriminant checks pass");
        bool coeff = false;
        for (const auto& s : r.steps) {
            if (s.titleKey == "step.title.write_the_canonical_form" &&
                toPlain(s.math).find("-4 u_xieta") != std::string::npos) {
                coeff = true;
            }
        }
        expect(coeff, "canonical form quotes the transformed mixed coefficient");
    }
    // A = 0 but B ≠ 0 is still hyperbolic.
    {
        Result r = solve("pde_classify", {{"A", 0}, {"B", 2}, {"C", 1}});
        expect(r.ok && plain(r.solution) == "hyperbolic",
               "2u_xy + u_yy is hyperbolic");
        expect(allChecksOk(r), "A = 0 canonical check passes");
    }
    // A = B = 0 leaves only u_yy.
    {
        Result r = solve("pde_classify", {{"A", 0}, {"B", 0}, {"C", 1}});
        expect(r.ok && plain(r.solution) == "parabolic",
               "u_yy = 0 is parabolic");
        expect(allChecksOk(r), "degenerate parabolic check passes");
    }
    // Lower-order terms do not change the type.
    {
        Result r = solve("pde_classify",
                         {{"A", 1}, {"B", 0}, {"C", 1}, {"D", 5}, {"E", -2}, {"F", 7}});
        expect(r.ok && plain(r.solution) == "elliptic",
               "lower-order terms keep the equation elliptic");
        expect(allChecksOk(r), "lower-order terms keep all checks green");
    }
    // Elliptic with unequal coefficients.
    {
        Result r = solve("pde_classify", {{"A", 2}, {"B", 0}, {"C", 8}});
        expect(r.ok && plain(r.solution) == "elliptic", "2u_xx + 8u_yy is elliptic");
        expect(allChecksOk(r), "unequal elliptic coefficients check out");
    }
    // No principal part at all.
    {
        Result r = solve("pde_classify",
                         {{"A", 0}, {"B", 0}, {"C", 0}, {"D", 1}, {"E", 1}, {"F", 1}});
        expect(!r.ok && r.errorCode == "bad_principal_part",
               "zero principal part is refused");
    }
}

static void testSeparation() {
    // Laplace: sinh and cosh in y.
    {
        Result r = solve("pde_separation",
                         {{"A", 1}, {"C", 1}, {"L", 3.141592653589793}, {"n", 1}});
        expect(r.ok, "separation for Laplace solves");
        const std::string s = plain(r.solution);
        expect(s.find("sinh(y)") != std::string::npos, "Laplace modes use sinh(y)");
        expect(s.find("cosh(y)") != std::string::npos, "Laplace modes use cosh(y)");
        expect(s.find("sin(x)") != std::string::npos, "Laplace modes use sin(x)");
        expect(allChecksOk(r), "mode residual and eigenfunction checks pass");
    }
    // A/C < 0: oscillatory in y.
    {
        Result r = solve("pde_separation",
                         {{"A", -1}, {"C", 1}, {"L", 3.141592653589793}, {"n", 1}});
        expect(r.ok, "oscillatory separation solves");
        const std::string s = plain(r.solution);
        expect(s.find("cos(y)") != std::string::npos, "A/C < 0 uses cos(y)");
        expect(s.find("sin(y)") != std::string::npos, "A/C < 0 uses sin(y)");
        expect(allChecksOk(r), "oscillatory checks pass");
    }
    // Unequal coefficients and two modes.
    {
        Result r = solve("pde_separation",
                         {{"A", 4}, {"C", 1}, {"L", 2}, {"n", 2}});
        expect(r.ok, "two-mode separation solves");
        const std::string s = plain(r.solution);
        expect(s.find("A_1") != std::string::npos && s.find("A_2") != std::string::npos,
               "two modes carry independent coefficients");
        expect(s.find("sinh(2") != std::string::npos,
               "frequency ratio A/C = 4 gives sinh(2 ...)");
    }
    // L is not pi: the eigenfunctions must still vanish at both ends.
    {
        Result r = solve("pde_separation", {{"A", 1}, {"C", 4}, {"L", 1.5}, {"n", 3}});
        expect(r.ok, "separation with L = 1.5 solves");
        const std::string s = plain(r.solution);
        expect(s.find("A_3") != std::string::npos, "three modes are shown");
        expect(allChecksOk(r), "boundary values vanish for L = 1.5");
    }
    // Truncated series with a single mode still satisfies the PDE.
    {
        Result r = solve("pde_separation",
                         {{"A", 1}, {"C", 1}, {"L", 3.141592653589793}, {"n", 1}});
        expect(r.ok && hasCheck(r, "check.pde_residual"),
               "the first mode carries an independent residual check");
    }
    // Degenerate coefficients and lengths.
    {
        Result r = solve("pde_separation", {{"A", 1}, {"C", 0}, {"L", 1}, {"n", 1}});
        expect(!r.ok && r.errorCode == "bad_principal_part",
               "C = 0 is refused by separation");
    }
    {
        Result r = solve("pde_separation", {{"A", 1}, {"C", 1}, {"L", 0}, {"n", 1}});
        expect(!r.ok && r.errorCode == "bad_length", "L = 0 is refused by separation");
    }
}

// ---------------------------------------------------------------- series PDEs
// The heat, wave and Laplace topics share the same sine-series machinery as
// the Fourier topic; these tests pin down that it validates its input and
// reports real boundary values.
static void testSeriesPdes() {
    const double pi = 3.141592653589793;
    {
        Result r = solve("heat", {{"alpha", 1}, {"L", pi}, {"n", 3}},
                         {{"f", "sin(pi*x/L)"}});
        expect(r.ok, "heat equation with its default profile solves");
        expect(plain(r.solution).find("exp(-t)") != std::string::npos,
               "the first heat mode decays as exp(-t)");
        expect(allChecksOk(r), "heat boundary check is computed");
    }
    {
        Result r = solve("heat", {{"alpha", 1}, {"L", pi}, {"n", 3}},
                         {{"f", "tan(x)"}});
        expect(!r.ok && (r.errorCode == "singular" || r.errorCode == "not_integrable"),
               "heat refuses a profile with a pole");
    }
    {
        Result r = solve("wave", {{"a", 1}, {"L", pi}, {"n", 3}},
                         {{"f", "x^2"}});
        expect(r.ok, "wave equation with x^2 solves");
        expect(plain(r.solution).find("cos(t)") != std::string::npos,
               "the first wave mode oscillates as cos(t)");
        expect(allChecksOk(r), "wave boundary check is computed");
    }
    {
        Result r = solve("wave", {{"a", 1}, {"L", pi}, {"n", 3}},
                         {{"f", "sec(x)"}});
        expect(!r.ok && (r.errorCode == "singular" || r.errorCode == "not_integrable"),
               "wave refuses a profile with a pole");
    }
    {
        Result r = solve("laplace_pde", {{"a", pi}, {"b", pi}, {"n", 2}},
                         {{"f", "sin(pi*x/a)"}});
        expect(r.ok, "Laplace on a square solves");
        expect(plain(r.solution).find("sinh") != std::string::npos,
               "the Laplace solution uses sinh in y");
        expect(allChecksOk(r), "Laplace boundary check is computed");
    }
    {
        Result r = solve("laplace_pde", {{"a", pi}, {"b", pi}, {"n", 2}},
                         {{"f", "1/(x - 1)"}});
        expect(!r.ok && (r.errorCode == "singular" || r.errorCode == "not_integrable"),
               "Laplace refuses a profile with an interior pole");
    }
    {
        Result r = solve("heat", {{"alpha", 1}, {"L", -1}, {"n", 2}},
                         {{"f", "x"}});
        expect(!r.ok && r.errorCode == "bad_length",
               "a negative heat length is refused");
    }
    {
        Result r = solve("laplace_pde", {{"a", 0}, {"b", 1}, {"n", 2}},
                         {{"f", "x"}});
        expect(!r.ok && r.errorCode == "bad_sides",
               "a zero Laplace side is refused");
    }
}

int main() {
    testFirstOrder();
    testCharacteristics();
    testClassification();
    testSeparation();
    testSeriesPdes();
    if (failures == 0) {
        std::cout << "PDE topics: all tests passed\n";
    } else {
        std::cout << "PDE topics: " << failures << " failure(s)\n";
    }
    return failures == 0 ? 0 : 1;
}
