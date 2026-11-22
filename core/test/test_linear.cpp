// Linear systems and numerical methods for systems (topics 75–80).
//
// The checks use reference formulas and reference values, never the engine's
// own intermediate strings.
#include "expr.hpp"
#include "numeric.hpp"
#include "solver.hpp"
#include "topics.hpp"

#include <cmath>
#include <iostream>
#include <limits>
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

static Result solve(const std::string& topic,
                    const std::map<std::string, std::string>& strs,
                    const std::map<std::string, double>& nums = {}) {
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

static std::string allFunctions(const Result& r) {
    std::string all;
    for (const auto& f : r.functions) all += f + "\n";
    return all;
}

static std::string allStepMath(const Result& r) {
    std::string all;
    for (const auto& s : r.steps) {
        if (s.math) all += toPlain(s.math) + "\n";
        all += s.note + "\n";
    }
    return all;
}

static double evalValue(const Ex& e) {
    std::map<std::string, double> vars;
    bool ok = false;
    const double v = evalReal(e, vars, &ok);
    return ok ? v : std::numeric_limits<double>::quiet_NaN();
}

static void testSystemTheory() {
    {
        Result r = solve("system_theory", {{"matrix", "1, 1; 4, 1"}});
        expect(r.ok, "A = [[1,1],[4,1]] is analysed");
        expect(allChecksOk(r), "the fundamental matrix checks pass");
        expect(r.independent, "the solution set is independent");
        expect(allFunctions(r).find("3") != std::string::npos,
               "the eigenvalue 3 is reported");
        expect(allFunctions(r).find("-1") != std::string::npos,
               "the eigenvalue -1 is reported");
        // Abel's formula must read as an expression, not as exp((2) t).
        expect(allStepMath(r).find("exp(2*t)") != std::string::npos,
               "the Wronskian is printed as exp(2*t)");
    }
    {
        Result r = solve("system_theory", {{"matrix", "0, 1; -2, -3"}});
        expect(r.ok, "a second 2x2 system is analysed");
        expect(allChecksOk(r), "the second system checks pass");
    }
    {
        Result r = solve("system_theory", {{"matrix", "1, 0, 0; 0, 2, 0; 0, 0, 3"}});
        expect(r.ok, "a diagonal 3x3 system is analysed");
        expect(allChecksOk(r), "the diagonal system checks pass");
    }
}

static void testComplexEigen() {
    {
        Result r = solve("system_complex_eigen", {{"matrix", "1, -1; 1, 1"}});
        expect(r.ok, "the rotation-like system is analysed");
        expect(allChecksOk(r), "complex eigen checks pass");
        expect(allFunctions(r).find("x1(t)") != std::string::npos,
               "the first real solution is reported");
        expect(allFunctions(r).find("cos") != std::string::npos,
               "the real solutions use cosine");
    }
    {
        Result r = solve("system_complex_eigen", {{"matrix", "0, -1; 1, 0"}});
        expect(r.ok, "the pure rotation is analysed");
        expect(allFunctions(r).find("sin") != std::string::npos,
               "the pure rotation uses sine");
    }
    {
        Result r = solve("system_complex_eigen", {{"matrix", "1, 0; 0, 2"}});
        expect(!r.ok && r.errorCode == "no_complex_eigenvalues",
               "a real spectrum is refused by this tool");
    }
}

static void testFundamental() {
    {
        Result r = solve("system_fundamental", {{"matrix", "0, 1; -1, 0"}});
        expect(r.ok, "the rotation matrix has a fundamental matrix");
        expect(allChecksOk(r), "Phi(0) = I and Phi' = A Phi hold");
        expect(allFunctions(r).find("cos") != std::string::npos,
               "the rotation matrix produces cosine entries");
        expect(r.wronskian != nullptr, "Abel's formula is reported");
    }
    {
        Result r = solve("system_fundamental", {{"matrix", "2, 0; 0, 5"}});
        expect(r.ok, "a diagonal matrix has a fundamental matrix");
        expect(allChecksOk(r), "the diagonal fundamental matrix checks pass");
    }
}

static void testRepeated() {
    {
        Result r = solve("system_repeated", {{"matrix", "3, 1; 0, 3"}});
        expect(r.ok, "a defective repeated eigenvalue is analysed");
        expect(allChecksOk(r), "the defective system checks pass");
        expect(allFunctions(r).find("t") != std::string::npos,
               "the defective solution contains a t factor");
        const std::string text = allFunctions(r) + allStepMath(r);
        expect(text.find("= x = ") == std::string::npos,
               "no vector is prefixed with a stray x =");
        expect(allFunctions(r).find("x1(t) = [1, 0]*exp(3*t)") != std::string::npos,
               "the eigenvector line reads x1(t) = [1, 0]*exp(3*t)");
        expect(allStepMath(r).find("c_1*x1(t)") != std::string::npos &&
                   allStepMath(r).find("...") == std::string::npos,
               "the general solution names the basis instead of a placeholder");
    }
    {
        Result r = solve("system_repeated", {{"matrix", "3, 0; 0, 3"}});
        expect(r.ok, "a diagonal repeated eigenvalue is analysed");
        expect(allChecksOk(r), "the diagonal repeated system checks pass");
    }
    {
        Result r = solve("system_repeated", {{"matrix", "1, 0; 0, 2"}});
        expect(!r.ok && r.errorCode == "no_repeated_eigenvalue",
               "a simple spectrum is refused by this tool");
    }
}

static void testNonhomogeneous() {
    {
        Result r = solve("system_nonhomogeneous",
                         {{"matrix", "1, 0; 0, 2"}, {"f", "1, 0"}});
        expect(r.ok, "a constant forcing is solved");
        expect(allChecksOk(r), "the particular solution checks pass");
        expect(r.closedForm, "a polynomial forcing has a closed form");
        expect(allFunctions(r).find("x_p1") != std::string::npos,
               "the particular solution is reported");
    }
    {
        Result r = solve("system_nonhomogeneous",
                         {{"matrix", "1, 0; 0, 2"}, {"f", "exp(t), 0"}});
        expect(r.ok, "a resonant exponential forcing is solved");
        expect(allFunctions(r).find("x_p1") != std::string::npos,
               "the resonant particular solution is reported");
    }
}

static void testSystemRk4() {
    {
        Result r = solve("system_rk4",
                         {{"f", "x"}, {"g", "y"}, {"method", "rk4"}},
                         {{"t0", 0.0}, {"T", 1.0}, {"h", 0.05},
                          {"x0", 1.0}, {"y0", 1.0}});
        expect(r.ok, "the decoupled system integrates");
        expect(allChecksOk(r), "the RK4 checks pass");
        expect(!r.closedForm, "the numerical answer is not labelled exact");
        const double x = evalValue(r.value);
        expect(std::fabs(x - std::exp(1.0)) < 1e-6, "x(1) matches e");
    }
    {
        Result r = solve("system_rk4",
                         {{"f", "x - 0.5*x*y"}, {"g", "-y + 0.1*x*y"},
                          {"method", "euler"}},
                         {{"t0", 0.0}, {"T", 2.0}, {"h", 0.02},
                          {"x0", 2.0}, {"y0", 1.0}});
        expect(r.ok, "the predator-prey example integrates with Euler");
        expect(r.method == "euler", "the method is reported");
        expect(!r.closedForm, "Euler output is numerical");
    }
}

static void testAlgebraicLinearSolve() {
    {
        Result r = solve("linear_solve",
                         {{"matrix", "2, 1; 1, -1"}, {"rhs", "5; 1"},
                          {"method", "gauss"}});
        expect(r.ok, "Gauss solves the 2x2 system");
        expect(allChecksOk(r), "the Gauss answer satisfies both equations");
        expect(allFunctions(r) == "x_1 = 2\nx_2 = 1\n",
               "Gauss gives x = (2, 1)");
        bool back = false;
        for (const auto& s : r.steps)
            if (s.titleKey == "step.title.back_substitution") back = true;
        expect(back, "Gauss shows a back-substitution step");
    }
    {
        Result gauss = solve("linear_solve",
                             {{"matrix", "2, 1; 1, -1"}, {"rhs", "5; 1"},
                              {"method", "gauss"}});
        Result cramer = solve("linear_solve",
                              {{"matrix", "2, 1; 1, -1"}, {"rhs", "5; 1"},
                               {"method", "cramer"}});
        expect(cramer.ok, "Cramer solves the same 2x2 system");
        expect(allFunctions(gauss) == allFunctions(cramer),
               "both methods agree on the answer");
        bool rule = false;
        for (const auto& s : cramer.steps)
            if (s.titleKey == "step.title.apply_cramers_rule") rule = true;
        expect(rule, "Cramer shows the determinant rule, not row operations");
    }
    {
        // A dependent system keeps a particular answer but must warn.
        Result r = solve("linear_solve",
                         {{"matrix", "1, 2; 2, 4"}, {"rhs", "3; 6"},
                          {"method", "gauss"}});
        expect(r.ok, "a dependent system is not an error");
        expect(!r.warnings.empty(), "a dependent system warns about free variables");
        expect(allChecksOk(r), "the particular answer still satisfies the system");
    }
    {
        Result r = solve("linear_solve",
                         {{"matrix", "1, 1; 1, 1"}, {"rhs", "1; 2"},
                          {"method", "gauss"}});
        expect(!r.ok, "a contradictory system fails");
        expect(r.errorCode == "linear_system_inconsistent",
               "the failure is reported as inconsistent");
    }
    {
        Result r = solve("linear_solve",
                         {{"matrix", "1, 2, 3; 0, 1, 4; 5, 6, 0"},
                          {"rhs", "14; 14; 17"}, {"method", "cramer"}});
        expect(r.ok, "Cramer handles a 3x3 system");
        expect(allFunctions(r) == "x_1 = 1\nx_2 = 2\nx_3 = 3\n",
               "Cramer gives x = (1, 2, 3)");
        expect(allChecksOk(r), "the 3x3 answer satisfies every equation");
    }
}

static void testLinearAlgebraTopics() {
    // Rank, null space and the two other fundamental subspaces.
    {
        Result r = solve("linalg_rank",
                         {{"matrix", "1, 2, 3; 2, 4, 6; 1, 1, 1"}});
        expect(r.ok, "rank of a dependent 3x3 matrix");
        expect(toPlain(r.solution) == "rank(A) = 2,  nullity(A) = 1",
               "rank and nullity are 2 and 1");
        expect(allChecksOk(r), "the null-space and pivot-column checks pass");
        expect(allStepMath(r).find("v_1 = [[1]; [-2]; [1]]") !=
                   std::string::npos,
               "the null-space basis vector is reported");
    }
    {
        Result r = solve("linalg_rank", {{"matrix", "1, 0; 0, 1"}});
        expect(r.ok && r.warnings.empty(), "a full-rank matrix does not warn");
        expect(allStepMath(r).find("{0}") != std::string::npos,
               "a trivial null space is reported as {0}");
    }
    {
        Result r = solve("linalg_rank", {{"matrix", "a, b; 2*a, 2*b"}});
        expect(r.ok, "symbolic rank");
        expect(toPlain(r.solution) == "rank(A) = 1,  nullity(A) = 1",
               "the generic symbolic rank is 1");
        expect(!r.assumptions.empty(), "symbolic pivots are flagged");
    }

    // Determinant, including the cofactor certificate.
    {
        Result r = solve("linalg_det", {{"matrix", "1, 2; 3, 4"}});
        expect(r.ok && toPlain(r.solution) == "det(A) = -2",
               "the 2x2 determinant is -2");
        expect(allChecksOk(r),
               "the determinant matches cofactor expansion");
    }
    {
        Result r = solve("linalg_det", {{"matrix", "a, b; c, d"}});
        expect(r.ok && toPlain(r.solution) == "det(A) = a*d - b*c",
               "the symbolic determinant is ad - bc");
    }
    {
        Result r = solve("linalg_det", {{"matrix", "1, 2; 2, 4"}});
        expect(r.ok && !r.warnings.empty(),
               "a singular determinant warns instead of failing");
    }
    {
        Result r = solve("linalg_det", {{"matrix", "1, 2; 3"}});
        expect(!r.ok && r.errorCode == "linalg_ragged_matrix",
               "a ragged matrix is rejected");
    }
    {
        Result r = solve("linalg_det", {{"matrix", "1, 2, 3; 4, 5, 6"}});
        expect(!r.ok && r.errorCode == "linalg_not_square",
               "a rectangular determinant is rejected");
    }
    {
        // The elimination pivots are three fractions deep; expanding their
        // product has to reproduce the full 24-term cofactor expansion.
        Result r = solve("linalg_det",
                         {{"matrix", "a, b, c, d; f, g, h, j; k, l, m, n; "
                                     "p, q, r, s"}});
        expect(r.ok, "the 4x4 symbolic determinant is produced");
        expect(allChecksOk(r),
               "the 4x4 determinant matches cofactor expansion");
        const std::string sol = toPlain(r.solution);
        expect(sol.find("a*g*m*s") != std::string::npos &&
                   sol.find("d*h*l*p") != std::string::npos,
               "the 4x4 determinant keeps its leading and trailing terms");
        size_t terms = 0;
        std::string token;
        for (size_t i = 0; i <= sol.size(); ++i) {
            const char ch = i < sol.size() ? sol[i] : ' ';
            if (ch != ' ') { token += ch; continue; }
            if (!token.empty() && token != "+" && token != "-" && token != "=")
                ++terms;
            token.clear();
        }
        // The leading `det(A)` and `=` tokens are not terms.
        expect(terms == 25, "the 4x4 determinant has 24 terms");
    }

    // Inverse: numeric row reduction and the symbolic adjugate.
    {
        Result r = solve("linalg_inverse", {{"matrix", "2, 1; 1, 1"}});
        expect(r.ok && toPlain(r.solution) == "A^{-1} = [[1, -1]; [-1, 2]]",
               "the numeric inverse is exact");
        expect(allChecksOk(r), "A A^{-1} = I");
    }
    {
        Result r = solve("linalg_inverse", {{"matrix", "a, b; c, d"}});
        expect(r.ok, "the symbolic inverse is produced");
        expect(toPlain(r.solution) ==
                   "A^{-1} = 1/(a*d - b*c) * [[d, -b]; [-c, a]]",
               "the symbolic inverse is factored over the determinant");
        expect(allChecksOk(r), "the symbolic A A^{-1} = I holds");
    }
    {
        Result r = solve("linalg_inverse", {{"matrix", "1, 1; 1, 1"}});
        expect(r.ok && toPlain(r.solution) == "A^{-1} does not exist",
               "a singular matrix has no inverse");
        expect(!r.warnings.empty(), "the singular inverse warns");
    }
    {
        // The adjugate route keeps a 4x4 symbolic inverse at one common
        // denominator; Gauss-Jordan would nest fractions three levels deep.
        Result r = solve("linalg_inverse",
                         {{"matrix", "a, b, c, d; f, g, h, j; k, l, m, n; "
                                     "p, q, r, s"}});
        expect(r.ok, "the 4x4 symbolic inverse is produced");
        expect(allChecksOk(r), "the 4x4 symbolic A A^{-1} = I holds");
        const std::string sol = toPlain(r.solution);
        expect(sol.rfind("A^{-1} = 1/(", 0) == 0,
               "the 4x4 inverse is factored over the determinant");
        expect(sol.find("* [[") != std::string::npos,
               "the adjugate follows the determinant");
        expect(sol.size() < 2000,
               "the 4x4 inverse stays in a readable adjugate form");
    }

    // LU with and without a row swap.
    {
        Result r = solve("linalg_lu", {{"matrix", "4, 3; 6, 3"}});
        expect(r.ok, "the LU factorisation succeeds");
        expect(allChecksOk(r), "P A = L U is verified");
        expect(toPlain(r.solution).find("L = [[1, 0]; [3/2, 1]]") !=
                   std::string::npos,
               "the unit lower-triangular factor is reported");
        expect(toPlain(r.solution).find("U = [[4, 3]; [0, -3/2]]") !=
                   std::string::npos,
               "the upper-triangular factor is reported");
    }
    {
        Result r = solve("linalg_lu", {{"matrix", "0, 1; 1, 0"}});
        expect(r.ok, "LU with a row swap succeeds");
        expect(allChecksOk(r), "the permuted factorisation reconstructs A");
        expect(toPlain(r.solution).find("P = [[0, 1]; [1, 0]]") !=
                   std::string::npos,
               "the permutation is reported");
    }

    // Symbolic systems and free variables.
    {
        Result r = solve("linear_solve",
                         {{"matrix", "a, b; c, d"}, {"rhs", "1; 0"},
                          {"method", "gauss"}});
        expect(r.ok, "the symbolic system is solved by elimination");
        expect(allFunctions(r) ==
                   "x_1 = d/(a*d - b*c)\nx_2 = -c/(a*d - b*c)\n",
               "the symbolic solution is exact");
        expect(!r.assumptions.empty(), "symbolic pivots are flagged");
    }
    {
        Result r = solve("linear_solve",
                         {{"matrix", "1, 2, 3; 2, 4, 6"}, {"rhs", "1; 2"},
                          {"method", "gauss"}});
        expect(r.ok && !r.warnings.empty(),
               "an underdetermined system warns about free variables");
        expect(toPlain(r.solution).find("t_1") != std::string::npos,
               "free parameters appear in the general solution");
    }
    {
        Result r = solve("linear_solve",
                         {{"matrix", "a, b; c, d"}, {"rhs", "1; 0"},
                          {"method", "cramer"}});
        expect(r.ok, "the symbolic system is solved by Cramer");
        expect(allChecksOk(r), "the Cramer answer satisfies the system");
    }
}

static void testVectorSpaceTopics() {
    // Span membership: the coefficients reproduce the target.
    {
        Result r = solve("linalg_span",
                         {{"vectors", "1, 0; 0, 1"}, {"target", "3, 5"}});
        expect(r.ok && r.warnings.empty(), "the target lies in the span");
        expect(toPlain(r.solution) ==
                   "b = 3 v_1 + 5 v_2  lies in span{v_1, v_2},  dim span = 2",
               "the standard basis reproduces (3, 5)");
        expect(allChecksOk(r), "the combination check passes");
    }
    {
        Result r = solve("linalg_span",
                         {{"vectors", "1, 2; 2, 4"}, {"target", "1, 1"}});
        expect(r.ok && !r.warnings.empty(),
               "a vector outside the span warns instead of failing");
        expect(toPlain(r.solution) ==
                   "b is not in span{v_1, v_2},  dim span = 1",
               "the span of two dependent vectors is a line");
    }
    {
        Result r = solve("linalg_span", {{"vectors", "1, 2, 3; 2, 4, 6"}});
        expect(r.ok, "a span without a target is described");
        expect(toPlain(r.solution) ==
                   "span{v_1, v_2} has dimension 1,  basis = {[[1]; [2]; [3]]}",
               "the pivot vector is the reported basis");
    }
    {
        Result r = solve("linalg_span",
                         {{"vectors", "1, 0; 0, 1"}, {"target", "1, 2, 3"}});
        expect(!r.ok && r.errorCode == "linalg_dimension_mismatch",
               "a target of the wrong length is rejected");
    }

    // Independence: a free column gives an explicit relation.
    {
        Result r = solve("linalg_independence", {{"vectors", "1, 2; 2, 4"}});
        expect(r.ok && !r.warnings.empty(), "a dependent set warns");
        expect(toPlain(r.solution) ==
                   "v_1, v_2 are linearly dependent: -2 v_1 + v_2 = 0",
               "the dependence relation is -2 v_1 + v_2 = 0");
        expect(allChecksOk(r), "the relation evaluates to zero");
    }
    {
        Result r = solve("linalg_independence", {{"vectors", "1, 0; 0, 1"}});
        expect(r.ok && r.warnings.empty(), "a basis is independent");
        expect(toPlain(r.solution) ==
                   "v_1, v_2 are linearly independent",
               "the standard basis is independent");
    }
    {
        Result r = solve("linalg_independence", {{"vectors", "a, b; c, d"}});
        expect(r.ok, "symbolic independence is decided generically");
        expect(!r.assumptions.empty(), "symbolic pivots are flagged");
    }

    // Basis extraction.
    {
        Result r = solve("linalg_basis",
                         {{"vectors", "1, 2, 3; 2, 4, 6; 1, 1, 1"}});
        expect(r.ok, "a basis is extracted from a spanning set");
        expect(toPlain(r.solution) ==
                   "basis = {[[1]; [2]; [3]], [[1]; [1]; [1]]},  dim = 2",
               "the first and third vectors form the basis");
        expect(allChecksOk(r), "independence and spanning both hold");
    }
    {
        Result r = solve("linalg_basis", {{"vectors", "0, 0; 0, 0"}});
        expect(r.ok, "the zero span is described");
        expect(toPlain(r.solution) == "basis = { },  dim = 0",
               "the zero subspace has the empty basis");
        expect(allChecksOk(r), "the empty basis checks pass");
    }

    // Coordinates in an ordered basis.
    {
        Result r = solve("linalg_coordinates",
                         {{"basis", "1, 1; 0, 1"}, {"vector", "3, 5"}});
        expect(r.ok, "the coordinates are computed");
        expect(toPlain(r.solution) == "[v]_B = (3, 2)",
               "the coordinates solve B c = v");
        expect(allFunctions(r) == "c_1 = 3\nc_2 = 2\n",
               "the coordinate functions are reported");
        expect(allChecksOk(r), "the coordinates rebuild the vector");
    }
    {
        Result r = solve("linalg_coordinates",
                         {{"basis", "1, 1; 2, 2"}, {"vector", "3, 5"}});
        expect(!r.ok && r.errorCode == "linalg_not_a_basis",
               "a singular basis is rejected");
    }
    {
        Result r = solve("linalg_coordinates",
                         {{"basis", "1, 0; 0, 1"}, {"vector", "1, 2, 3"}});
        expect(!r.ok && r.errorCode == "linalg_dimension_mismatch",
               "a vector of the wrong length is rejected");
    }

    // Change of basis: P maps B-coordinates to C-coordinates.
    {
        Result r = solve("linalg_change_basis",
                         {{"basis_b", "1, 0; 0, 1"},
                          {"basis_c", "1, 1; 0, 1"}});
        expect(r.ok, "the transition matrix is computed");
        expect(toPlain(r.solution) == "P_{C<-B} = [[1, 0]; [-1, 1]]",
               "the transition matrix is C^{-1} B");
        expect(allChecksOk(r), "C P = B holds");
    }

    // Subspace axioms.
    {
        Result r = solve("linalg_subspace",
                         {{"vectors", "1, 0; 0, 1; 1, 1"}});
        expect(r.ok, "the span is verified as a subspace");
        expect(toPlain(r.solution) ==
                   "W is a subspace,  dim W = 2,  basis = {[[1]; [0]], "
                   "[[0]; [1]]}",
               "the plane has dimension 2");
        expect(r.checks.size() == 3 && allChecksOk(r),
               "zero, addition and scaling are all checked");
    }
}

static void testApplicationTopics() {
    // Orthogonal projection onto a plane in R^3.
    {
        Result r = solve("linalg_projection",
                         {{"basis", "1, 1, 0; 0, 1, 1"},
                          {"vector", "1, 2, 3"}});
        expect(r.ok, "the projection is computed");
        expect(toPlain(r.solution) ==
                   "proj_W(v) = (1/3, 8/3, 7/3),   v - proj_W(v) = (2/3, "
                   "-2/3, 2/3),   distance = 2/3*sqrt(3)",
               "the projection and residual match the reference values");
        expect(r.checks.size() == 2 && allChecksOk(r),
               "the residual is orthogonal and the projection is in W");
    }

    // A dependent basis makes the projection ambiguous.
    {
        Result r = solve("linalg_projection",
                         {{"basis", "1, 2; 2, 4"}, {"vector", "3, 4"}});
        expect(!r.ok && r.errorCode == "linalg_dependent_basis",
               "a dependent basis is rejected");
    }

    // Gram-Schmidt on two independent vectors.
    {
        Result r = solve("linalg_gram_schmidt", {{"vectors", "3, 4; 1, 0"}});
        expect(r.ok, "Gram-Schmidt succeeds");
        const std::string f = allFunctions(r);
        expect(f.find("u_2 = [[16/25]; [-12/25]]") != std::string::npos,
               "the second orthogonal vector is correct");
        expect(f.find("q_1 = [[3/5]; [4/5]]") != std::string::npos,
               "the first orthonormal vector is correct");
        expect(r.warnings.empty(), "independent vectors produce no warning");
        expect(r.checks.size() == 3 && allChecksOk(r),
               "orthogonality, unit length and spanning are checked");
    }

    // A dependent vector is dropped with a warning.
    {
        Result r = solve("linalg_gram_schmidt", {{"vectors", "1, 2; 2, 4"}});
        expect(r.ok, "Gram-Schmidt still returns the independent part");
        expect(r.warnings.size() == 1 &&
                   r.warnings.front() == "warning.linearly_dependent",
               "the dependent vector is reported");
        expect(r.functions.size() == 2,
               "only one orthogonal and one orthonormal vector remain");
    }

    // QR of a tall matrix.
    {
        Result r = solve("linalg_qr", {{"matrix", "1, 1; 1, 0; 0, 1"}});
        expect(r.ok, "the QR factorization succeeds");
        expect(r.checks.size() == 2 && allChecksOk(r),
               "Q R = A and Q^T Q = I are both verified");
    }
    {
        Result r = solve("linalg_qr", {{"matrix", "1, 2; 2, 4"}});
        expect(!r.ok && r.errorCode == "linalg_not_full_column_rank",
               "dependent columns are rejected by QR");
    }

    // Least squares with a nonzero residual.
    {
        Result r = solve("linalg_least_squares",
                         {{"matrix", "1, 0; 0, 1; 1, 1"}, {"rhs", "1; 2; 4"}});
        expect(r.ok, "the least-squares solution is computed");
        expect(toPlain(r.solution) ==
                   "x = (4/3, 7/3),   residual = (-1/3, -1/3, 1/3),   "
                   "||r|| = 1/3*sqrt(3)",
               "the normal equations give the reference solution");
        expect(r.checks.size() == 2 && allChecksOk(r),
               "the normal equations and the residual are verified");
    }
    {
        Result r = solve("linalg_least_squares",
                         {{"matrix", "1, 1; 1, 1"}, {"rhs", "1; 3"}});
        expect(!r.ok && r.errorCode == "linalg_not_full_column_rank",
               "a rank-deficient design matrix is rejected");
    }

    // Diagonalization with two distinct rational eigenvalues.
    {
        Result r = solve("linalg_diagonalize", {{"matrix", "2, 1; 1, 2"}});
        expect(r.ok, "the matrix is diagonalized");
        const std::string f = allFunctions(r);
        expect(f.find("D = [[1, 0]; [0, 3]]") != std::string::npos,
               "the diagonal holds the eigenvalues");
        expect(r.checks.size() == 2 && allChecksOk(r),
               "A P = P D and det(P) != 0 are checked");
    }
    {
        Result r = solve("linalg_diagonalize", {{"matrix", "1, 1; 0, 1"}});
        expect(!r.ok && r.errorCode == "linalg_not_diagonalizable",
               "a defective matrix is reported");
    }
    {
        Result r = solve("linalg_diagonalize", {{"matrix", "0, -1; 1, 0"}});
        expect(!r.ok && r.errorCode == "linalg_eigenvalues_not_rational",
               "complex eigenvalues are reported");
    }
    {
        Result r = solve("linalg_diagonalize", {{"matrix", "a, b; c, d"}});
        expect(!r.ok && r.errorCode == "linalg_numeric_required",
               "symbolic diagonalization asks for numbers");
    }

    // Matrix powers, positive and negative, with a diagonalization check.
    {
        Result r = solve("linalg_power",
                         {{"matrix", "2, 1; 1, 2"}, {"power", "3"}});
        expect(r.ok, "the power is computed");
        expect(toPlain(r.solution) == "A^3 = [[14, 13]; [13, 14]]",
               "A^3 matches the reference");
        expect(r.checks.size() == 1 && allChecksOk(r),
               "P D^3 P^{-1} matches direct multiplication");
    }
    {
        Result r = solve("linalg_power",
                         {{"matrix", "1, 1; 0, 1"}, {"power", "3"}});
        expect(r.ok, "a non-diagonalizable power still works");
        expect(toPlain(r.solution) == "A^3 = [[1, 3]; [0, 1]]",
               "direct multiplication handles the defective matrix");
        expect(r.checks.empty(),
               "no diagonal cross-check is offered for a defective matrix");
    }
    {
        Result r = solve("linalg_power",
                         {{"matrix", "2, 0; 0, 3"}, {"power", "-1"}});
        expect(r.ok, "a negative power uses the inverse");
        expect(toPlain(r.solution) == "A^-1 = [[1/2, 0]; [0, 1/3]]",
               "A^{-1} matches the reference");
    }
    {
        Result r = solve("linalg_power",
                         {{"matrix", "1, 1; 1, 1"}, {"power", "-1"}});
        expect(!r.ok && r.errorCode == "linalg_singular_matrix",
               "a negative power of a singular matrix is rejected");
    }
    {
        Result r = solve("linalg_power",
                         {{"matrix", "1, 1; 1, 1"}, {"power", "99"}});
        expect(!r.ok && r.errorCode == "linalg_bad_power",
               "an out-of-range exponent is rejected");
    }
}

static void testExtendedTopics() {
    // The orthogonal complement is the null space of the transpose.
    {
        Result r = solve("linalg_orthogonal_complement",
                         {{"matrix", "1, 2; 2, 4; 3, 6"}});
        expect(r.ok, "the orthogonal complement is computed");
        expect(toPlain(r.solution) ==
                   "N(A^T) = {v_1 = (-2, 1, 0),  v_2 = (-3, 0, 1)},   dim = 2",
               "the complement basis matches the reference");
        expect(r.checks.size() == 2 && allChecksOk(r),
               "orthogonality and independence are checked");
    }
    {
        Result r = solve("linalg_orthogonal_complement",
                         {{"matrix", "1, 0; 0, 1"}});
        expect(r.ok, "a full-rank square matrix has a trivial complement");
        expect(toPlain(r.solution) == "N(A^T) = {0},   dim = 0",
               "the complement is the zero space");
    }

    // A graph read through its incidence matrix.
    {
        Result r = solve("linalg_graph",
                         {{"nodes", "3"}, {"edges", "1-2, 2-3, 3-1"}});
        expect(r.ok, "the triangle graph is read");
        expect(toPlain(r.solution) ==
                   "components = 1,   rank(B) = 2,   cycle dimension = 1",
               "the triangle has one component and one cycle");
        expect(r.checks.size() == 2 && allChecksOk(r),
               "B x = 0 and B^T y = 0 are checked");
    }
    {
        Result r = solve("linalg_graph",
                         {{"nodes", "4"}, {"edges", "1-2, 3-4"}});
        expect(r.ok, "a disconnected graph is read");
        expect(toPlain(r.solution) ==
                   "components = 2,   rank(B) = 2,   cycle dimension = 0",
               "two edges give two components and no cycle");
    }
    {
        Result r = solve("linalg_graph",
                         {{"nodes", "3"}, {"edges", "1-4"}});
        expect(!r.ok && r.errorCode == "linalg_bad_matrix",
               "an edge outside the node range is rejected");
    }

    // The DFT and its inverse.
    {
        Result r = solve("linalg_fft", {{"signal", "1, 2, 3, 4"}});
        expect(r.ok, "the DFT of a length-4 signal is computed");
        expect(toPlain(r.solution) ==
                   "X_0 = 10,  X_1 = -2 + 2i,  X_2 = -2,  X_3 = -2 - 2i",
               "the transform matches the reference values");
        expect(r.checks.size() == 1 && allChecksOk(r),
               "the inverse DFT reconstructs the signal");
    }
    {
        Result r = solve("linalg_fft", {{"signal", "1, 2, 3, 4, 5"}});
        expect(!r.ok && r.errorCode == "linalg_fft_length",
               "a length other than 4 or 8 is rejected");
    }

    // Similarity preserves the trace and the determinant.
    {
        Result r = solve("linalg_similarity",
                         {{"matrix", "1, 2; 3, 4"}, {"change", "1, 1; 0, 1"}});
        expect(r.ok, "the similar matrix is computed");
        expect(toPlain(r.solution) == "B = P^{-1} A P = [[-2, -4]; [3, 7]]",
               "B matches the reference");
        expect(r.checks.size() == 2 && allChecksOk(r),
               "the trace and determinant invariants hold");
    }
    {
        Result r = solve("linalg_similarity",
                         {{"matrix", "1, 2; 3, 4"}, {"change", "1, 0; 0, 0"}});
        expect(!r.ok && r.errorCode == "linalg_singular_matrix",
               "a singular change of basis is rejected");
    }

    // The matrix exponential in three regimes.
    {
        Result r = solve("linalg_matrix_exp", {{"matrix", "0, 1; -1, 0"}});
        expect(r.ok, "the rotation exponential is computed");
        expect(toPlain(r.solution) ==
                   "e^{At} = [[cos(t), sin(t)]; [-sin(t), cos(t)]]",
               "a skew matrix gives a rotation");
        expect(r.checks.size() == 1 && allChecksOk(r),
               "the exponential is the identity at zero");
    }
    {
        Result r = solve("linalg_matrix_exp", {{"matrix", "1, 1; 0, 1"}});
        expect(r.ok, "the repeated eigenvalue case is computed");
        expect(toPlain(r.solution) ==
                   "e^{At} = [[exp(t), t*exp(t)]; [0, exp(t)]]",
               "a Jordan block adds the linear term");
    }
    {
        Result r = solve("linalg_matrix_exp",
                         {{"matrix", "1, 0, 0; 0, 2, 0; 0, 0, 3"}});
        expect(r.ok, "a diagonal matrix is exponentiated entry by entry");
        expect(toPlain(r.solution) ==
                   "e^{At} = [[exp(t), 0, 0]; [0, exp(2*t), 0]; [0, 0, "
                   "exp(3*t)]]",
               "the diagonal case is correct");
    }

    // Positive definite matrices and Cholesky.
    {
        Result r = solve("linalg_positive_definite",
                         {{"matrix", "4, 2; 2, 3"}});
        expect(r.ok, "the Cholesky factorization succeeds");
        expect(toPlain(r.solution) ==
                   "A = L L^T,   L = [[2, 0]; [1, sqrt(2)]]",
               "the Cholesky factor matches the reference");
        expect(r.checks.size() == 2 && allChecksOk(r),
               "L L^T = A and L is lower triangular");
    }
    {
        Result r = solve("linalg_positive_definite",
                         {{"matrix", "1, 2; 2, 1"}});
        expect(!r.ok && r.errorCode == "linalg_not_positive_definite",
               "an indefinite matrix is rejected");
    }
    {
        Result r = solve("linalg_positive_definite",
                         {{"matrix", "a, 0; 0, a"}});
        expect(r.ok, "a symbolic positive definite matrix is factored");
        expect(toPlain(r.solution) ==
                   "A = L L^T,   L = [[sqrt(a), 0]; [0, sqrt(a)]]",
               "the symbolic Cholesky factor is kept exact");
    }

    // The SVD, the rank and the pseudoinverse.
    {
        Result r = solve("linalg_svd", {{"matrix", "3, 0; 0, 2"}});
        expect(r.ok, "the SVD is computed");
        expect(toPlain(r.solution) ==
                   "sigma = (sigma_1 = 3,   sigma_2 = 2),   rank = 2,   "
                   "cond(A) = 1.5",
               "the singular values and condition number match");
        expect(r.checks.size() == 3 && allChecksOk(r),
               "U S V^T = A, U^T U = I and A A^+ A = A");
    }
    {
        Result r = solve("linalg_svd", {{"matrix", "1, 0; 0, 0"}});
        expect(r.ok, "a rank-deficient SVD is computed");
        expect(toPlain(r.solution) ==
                   "sigma = (sigma_1 = 1,   sigma_2 = 0),   rank = 1,   "
                   "cond(A) = inf",
               "the zero singular value makes the matrix singular");
    }
    {
        Result r = solve("linalg_svd", {{"matrix", "a, 0; 0, 1"}});
        expect(!r.ok && r.errorCode == "linalg_numeric_required",
               "a symbolic SVD asks for numbers");
    }

    // Norms and the condition number.
    {
        Result r = solve("linalg_norm", {{"matrix", "1, -2; 3, 4"}});
        expect(r.ok, "the norms are computed");
        const std::string f = allFunctions(r);
        expect(toPlain(r.solution).find("||A||_1 = 6") != std::string::npos,
               "the 1-norm is the largest column sum");
        expect(toPlain(r.solution).find("||A||_inf = 7") != std::string::npos,
               "the infinity-norm is the largest row sum");
        expect(r.checks.size() == 2 && allChecksOk(r),
               "the norm inequalities hold");
    }

    // Power iteration on a symmetric matrix.
    {
        Result r = solve("linalg_power_iteration",
                         {{"matrix", "2, 1; 1, 2"}, {"start", "1, 0"}});
        expect(r.ok, "power iteration converges");
        expect(toPlain(r.solution).find("lambda_1 = 3") != std::string::npos,
               "the dominant eigenvalue is 3");
        expect(r.warnings.empty(), "the iteration converges without a warning");
        expect(r.checks.size() == 1 && allChecksOk(r),
               "the eigenpair residual is small");
    }

    // The QR algorithm exposes the eigenvalues on the diagonal.
    {
        Result r = solve("linalg_qr_algorithm", {{"matrix", "2, 1; 1, 2"}});
        expect(r.ok, "the QR iteration runs");
        expect(toPlain(r.solution) == "lambda_1 = 3,   lambda_2 = 1",
               "the eigenvalues are read from the diagonal");
        expect(r.checks.size() == 1 && allChecksOk(r),
               "the off-diagonal part decreases");
    }
    {
        Result r = solve("linalg_qr_algorithm", {{"matrix", "1, 2; 3, 4"}});
        expect(!r.ok && r.errorCode == "linalg_not_symmetric",
               "the QR algorithm requires a symmetric matrix");
    }

    // Jacobi and Gauss-Seidel.
    {
        Result r = solve("linalg_iterative",
                         {{"matrix", "4, 1; 1, 3"}, {"rhs", "1, 2"}});
        expect(r.ok, "Jacobi converges");
        expect(toPlain(r.solution).find("x = (1/11, 7/11)") !=
                   std::string::npos,
               "the Jacobi solution matches the direct solve");
        expect(r.warnings.empty(), "no divergence warning");
    }
    {
        Result r = solve("linalg_iterative",
                         {{"matrix", "4, 1; 1, 3"},
                          {"rhs", "1, 2"},
                          {"method", "gauss_seidel"}});
        expect(r.ok, "Gauss-Seidel converges");
        expect(toPlain(r.solution).find("x = (1/11, 7/11)") !=
                   std::string::npos,
               "Gauss-Seidel reaches the same solution");
    }
    {
        Result r = solve("linalg_iterative",
                         {{"matrix", "1, 2; 2, 1"}, {"rhs", "3, 3"}});
        expect(r.ok, "a divergent iteration still returns its last iterate");
        expect(r.warnings.size() == 1 &&
                   r.warnings.front() == "warning.iterative_diverged",
               "divergence is reported");
        expect(toPlain(r.solution).find("x_") == 0,
               "the last iterate is labelled, not presented as the answer");
    }

    // Linear programming by the simplex method.
    {
        Result r = solve("linalg_lp",
                         {{"objective", "3, 2"},
                          {"constraints", "1, 1; 1, 0; 0, 1"},
                          {"bounds", "4, 2, 3"}});
        expect(r.ok, "the linear program is solved");
        expect(toPlain(r.solution) == "x = (2, 2),   z = 10",
               "the optimum matches the reference");
        expect(r.checks.size() == 1 && allChecksOk(r),
               "the vertex is feasible");
    }
    {
        Result r = solve("linalg_lp",
                         {{"objective", "1, 1"},
                          {"constraints", "-1, 0; 0, -1"},
                          {"bounds", "1, 1"}});
        expect(!r.ok && r.errorCode == "linalg_lp_unbounded",
               "an unbounded program is reported");
    }

    // 2x2 zero-sum games.
    {
        Result r = solve("linalg_game", {{"matrix", "3, 0; 0, 2"}});
        expect(r.ok, "the mixed game is solved");
        expect(toPlain(r.solution) ==
                   "value = 1.2,   row strategy = (0.4, 0.6),   column "
                   "strategy = (0.4, 0.6)",
               "the mixed strategies match the reference");
        expect(r.checks.size() == 1 && allChecksOk(r),
               "the mixed strategies attain the value");
    }
    {
        Result r = solve("linalg_game", {{"matrix", "3, 1; 2, 1"}});
        expect(r.ok, "the saddle point is found");
        expect(toPlain(r.solution) ==
                   "value = 1,   row strategy = (1, 0),   column strategy = "
                   "(0, 1)",
               "the pure equilibrium matches maximin = minimax");
    }

    // The spectral theorem.
    {
        Result r = solve("linalg_symmetric", {{"matrix", "2, 1; 1, 2"}});
        expect(r.ok, "the orthogonal diagonalization is computed");
        expect(r.checks.size() == 2 && allChecksOk(r),
               "Q^T Q = I and Q D Q^T = A");
        expect(allFunctions(r).find("Q = ") != std::string::npos,
               "the eigenvector matrix is reported");
    }
    {
        Result r = solve("linalg_symmetric", {{"matrix", "1, 2; 3, 4"}});
        expect(!r.ok && r.errorCode == "linalg_not_symmetric",
               "a non-symmetric matrix is rejected");
    }
}

int main() {
    testSystemTheory();
    testComplexEigen();
    testFundamental();
    testRepeated();
    testNonhomogeneous();
    testSystemRk4();
    testAlgebraicLinearSolve();
    testLinearAlgebraTopics();
    testVectorSpaceTopics();
    testApplicationTopics();
    testExtendedTopics();
    if (failures == 0) {
        std::cout << "all linear tests passed\n";
        return 0;
    }
    std::cout << failures << " linear test(s) failed\n";
    return 1;
}
