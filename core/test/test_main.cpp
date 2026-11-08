#include "expr.hpp"
#include "poly.hpp"
#include "numeric.hpp"
#include <iostream>
#include <cassert>

using namespace ode;

static int failures = 0;
static void check(const std::string& label, const std::string& got, const std::string& want) {
    // compare after trimming spaces
    auto norm = [](std::string s) {
        std::string r;
        for (char c : s) if (!std::isspace((unsigned char)c)) r += c;
        return r;
    };
    bool ok = norm(got) == norm(want);
    std::cout << (ok ? "PASS " : "FAIL ") << label << "  got: " << got;
    if (!ok) std::cout << "   want: " << want;
    std::cout << "\n";
    if (!ok) ++failures;
}

int main() {
    // parsing / canonical form
    check("parse 2x+3x", toPlain(parseExpr("2x + 3x")), "5*x");
    check("parse (x+1)^2", toPlain(expand(parseExpr("(x+1)^2"))), "x^2 + 2*x + 1");
    check("diff x^3", toPlain(simplify(diff(parseExpr("x^3"), "x"))), "3*x^2");
    check("diff sin(x^2)", toPlain(simplify(diff(parseExpr("sin(x^2)"), "x"))), "2*x*cos(x^2)");
    check("diff ln(x)", toPlain(simplify(diff(parseExpr("ln(x)"), "x"))), "1/x");
    check("diff exp(2x)", toPlain(simplify(diff(parseExpr("exp(2x)"), "x"))), "2*exp(2*x)");
    check("subst", toPlain(simplify(substitute(parseExpr("x^2+y"), "x", Num(2)))), "y + 4");

    // integration
    check("int x^2", toPlain(simplify(integrate(parseExpr("x^2"), "x"))), "1/3*x^3");
    check("int 1/x", toPlain(simplify(integrate(parseExpr("1/x"), "x"))), "ln(abs(x))");
    check("int cos(3x)", toPlain(simplify(integrate(parseExpr("cos(3x)"), "x"))), "1/3*sin(3*x)");
    check("int 2x cos(x^2)", toPlain(simplify(integrate(parseExpr("2x*cos(x^2)"), "x"))), "sin(x^2)");
    {
        Ex F = integrate(parseExpr("x*exp(x)"), "x");
        Ex res = simplify(Add(diff(F, "x"), Mul(Num(-1), parseExpr("x*exp(x)"))));
        check("int x exp(x)", toPlain(res), "0");
    }
    check("int 1/(x^2+1)", toPlain(simplify(integrate(parseExpr("1/(x^2+1)"), "x"))), "atan(x)");
    check("int 1/(x(x+1))", toPlain(simplify(integrate(parseExpr("1/(x*(x+1))"), "x"))), "ln(abs(x)) - ln(abs(x+1))");
    check("int cos(x)^3", toPlain(simplify(integrate(parseExpr("cos(x)^3"), "x"))), "sin(x) - 1/3*sin(x)^3");
    {
        Ex F = integrate(parseExpr("cos(x)^4"), "x");
        Ex res = simplify(Add(diff(F, "x"), Mul(Num(-1), parseExpr("cos(x)^4"))));
        check("int cos(x)^4", numericallyZero(res, {"x"}, 8, 1e-6) ? "0" : toPlain(res), "0");
    }
    check("int 1/cos(x)", toPlain(simplify(integrate(parseExpr("1/cos(x)"), "x"))),
          "ln(abs(sec(x) + tan(x)))");
    check("int 1/tan(x)", toPlain(simplify(integrate(parseExpr("1/tan(x)"), "x"))), "ln(abs(sin(x)))");

    // Least common denominator: a repeated denominator must stay a single
    // factor instead of being squared by multiplying the denominators.
    check("together repeated denominator",
          toPlain(simplify(parseExpr("1/(x+1) + 1/(x+1)^2"))), "(x + 2)/(x + 1)^2");
    check("together repeated denominator x",
          toPlain(simplify(parseExpr("1/x + 1/x^2"))), "(x + 1)/x^2");
    check("together symbolic denominator",
          toPlain(simplify(parseExpr("1/(v*sqrt(3)-3) + 1/(v*sqrt(3)-3)^2"))),
          "(v*sqrt(3) - 2)/(v*sqrt(3) - 3)^2");
    {
        // Symbolic numerator over a linear denominator.
        Ex F = integrate(parseExpr("(a+b*x)/(x+1)"), "x");
        Ex res = simplify(Add(diff(F, "x"), Mul(Num(-1), parseExpr("(a+b*x)/(x+1)"))));
        check("int symbolic linear denominator",
              numericallyZero(res, {"x", "a", "b"}, 20, 1e-6) ? "0" : toPlain(res), "0");
    }
    {
        // Symbolic numerator over a quadratic with no real roots (atan form).
        Ex F = integrate(parseExpr("(a+b*x)/(x^2+1)"), "x");
        Ex res = simplify(Add(diff(F, "x"), Mul(Num(-1), parseExpr("(a+b*x)/(x^2+1)"))));
        check("int symbolic quadratic atan",
              numericallyZero(res, {"x", "a", "b"}, 20, 1e-6) ? "0" : toPlain(res), "0");
    }
    {
        // Symbolic numerator over a quadratic with two real roots (log form).
        Ex F = integrate(parseExpr("(a+b*x)/(x^2-1)"), "x");
        Ex res = simplify(Add(diff(F, "x"), Mul(Num(-1), parseExpr("(a+b*x)/(x^2-1)"))));
        check("int symbolic quadratic log",
              numericallyZero(res, {"x", "a", "b"}, 20, 1e-6) ? "0" : toPlain(res), "0");
    }
    check("cancel sign", toPlain(simplify(parseExpr("(y-x)/(x*(x-y))"))), "-1/x");
    check("cancel content", toPlain(simplify(parseExpr("(2-2*x*y)/(x^2*y-x)"))), "-2/x");
    {
        // Extracting the content of a powered factor must raise it to the
        // same power: 1/(-x/3 + C/x^2)^2 = 9/(x - 3C/x^2)^2.
        Ex s = simplify(parseExpr("(2*C/x^3 + 1/3)/(-1/3*x + C/x^2)^2"));
        Ex v = simplify(substitute(substitute(s, "C", Num(1)), "x", Num(2)));
        check("cancel content power", toPlain(v), "84/25");
    }
    // parsing edge cases
    check("scientific notation", toPlain(parseExpr("1e-3")), "1/1000");
    check("scientific notation uppercase", toPlain(parseExpr("2E5")), "200000");
    check("nested absolute value", toPlain(parseExpr("||x|-1|")), "abs(abs(x) - 1)");
    {
        bool threw = false;
        try { parseExpr(std::string(1200, '1')); } catch (const std::exception&) { threw = true; }
        check("over-long input rejected", threw ? "yes" : "no", "yes");
        threw = false;
        try { parseExpr("1.2345678901234567890123"); } catch (const std::exception&) { threw = true; }
        check("over-long decimal rejected", threw ? "yes" : "no", "yes");
    }
    check("exp product", toPlain(simplify(parseExpr("exp(x)*exp(2*x)"))), "exp(3*x)");
    check("sqrt(8)", toPlain(simplify(parseExpr("sqrt(8)"))), "2*sqrt(2)");
    check("sqrt(1/2)", toPlain(simplify(parseExpr("sqrt(1/2)"))), "1/2*sqrt(2)");
    check("int exp(-x^2)", toPlain(simplify(integrate(parseExpr("exp(-x^2)"), "x"))),
          "1/2*sqrt(pi)*erf(x)");
    check("diff erf(x)", toPlain(simplify(diff(parseExpr("erf(x)"), "x"))),
          "2*exp(-x^2)/sqrt(pi)");

    // polynomial
    Poly p;
    check("poly", toPoly(parseExpr("x^2 - 3x + 2"), "x", p) ? "ok" : "no", "ok");
    auto roots = solvePoly(parseExpr("x^2 - 3x + 2"), "x");
    std::cout << "roots: ";
    for (auto& r : roots) std::cout << toPlain(r) << " ";
    std::cout << "\n";

    std::cout << (failures ? "FAILURES: " + std::to_string(failures) : "ALL OK") << "\n";
    return failures ? 1 : 0;
}
