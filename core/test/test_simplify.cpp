// Deep simplification checks.
//
// Every case is verified twice: the printed form must match the expected
// string, and the expression must stay numerically equal to its input, so a
// wrong cancellation can never pass as "prettier".
#include "expr.hpp"
#include "numeric.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace ode;

static int failures = 0;

static void expect(const std::string& input, const std::string& want) {
    Ex in = parseExpr(input);
    Ex out = simplifyStrong(in);
    std::vector<std::string> syms;
    collectSymbols(in, syms);
    const bool equal = numericallyZero(Add(in, Mul(Num(-1), out)), syms, 12, 1e-5);
    const bool printed = toPlain(out) == want;
    if (!equal || !printed) {
        ++failures;
        std::cout << "FAIL: " << input << "\n  got:  " << toPlain(out);
        if (!printed) std::cout << "\n  want: " << want;
        if (!equal) std::cout << "\n  the result is not equal to the input";
        std::cout << "\n";
    }
}

int main() {
    // One-step cancel: the numerator already factors.
    expect("(x^2-1)/(x-1)", "x + 1");
    expect("(x^3-y^3)/(x-y)", "x^2 + x*y + y^2");
    expect("(2*x+2)/(2*x-2)", "(x + 1)/(x - 1)");
    expect("(x^2-4)/(x^2-4*x+4)", "(x + 2)/(x - 2)");

    // Several steps: expand, combine, cancel, then pick the short form.
    expect("((x+1)^2-x^2-2*x-1)/(x-1)", "0");
    expect("1/(1+1/x)", "x/(x + 1)");
    expect("(1/x+1)/(1/x-1)", "-(x + 1)/(x - 1)");
    expect("(1 - 1/x^2)/(1 - 1/x)", "1/x + 1");
    expect("1/(1+1/(1+1/x))", "(x + 1)/(2*x + 1)");
    expect("2*(x+1)/(x^2-1)", "2/(x - 1)");

    // Long chains that only close after polynomial division.
    expect("((x+1)^3 - 3*x*(x+1) - 1)/(x+1)", "x^3/(x + 1)");
    expect("(1/(x+1) - 1/(x-1))*(x^2-1)", "-2");
    expect("((x^2-1)/(x^2+2*x+1))*((x+1)/(x-1))^2", "(x + 1)/(x - 1)");
    expect("(x^4-1)/(x^2+1)/(x-1)", "x + 1");
    expect("(x^2*y - y)/(x*y + y)", "x - 1");
    expect("(x/(x+y) + y/(x-y))/((x^2+y^2)/(x^2-y^2))", "1");
    expect("((x+1)^2/(x-1)^2)*((x-1)^3/(x+1))", "x^2 - 1");
    expect("(3/(x-1) - 3/(x+1))/(6/(x^2-1))", "1");

    // Exponential and logarithm rules.
    expect("exp(2*ln(x)) + exp(ln(x^2))", "2*x^2");
    expect("ln(exp(x)) + ln(1)", "x");

    // A quotient whose cancellation needs several variables.
    expect("((x-2*y)^2-1)/(x-2*y-1)", "x - 2*y + 1");

    // Common factors are pulled out of a sum when that reads better.
    expect("x*sin(x) + sin(x)*cos(x)", "sin(x)*(x + cos(x))");

    // Polynomials are multiplied out and ordered by descending degree.
    expect("1/2*x*(x + 2)", "1/2*x^2 + x");
    expect("x*(x^2 - 3*x + 2)", "x^3 - 3*x^2 + 2*x");
    expect("3*x^2*y + 6*x*y^2", "3*y*x^2 + 6*x*y^2");

    // A series with fractional powers runs from the lowest power up.
    expect("x^(5/2) + x^(1/2) - x^(3/2)", "sqrt(x) - x^(3/2) + x^(5/2)");

    // A negated sum in a numerator is flipped, not left as `-(-a+b)/(...)`.
    expect("-(-s^2 + 1)/(s^2 + 1)^2", "(s^2 - 1)/(s^2 + 1)^2");
    expect("-(-(s - 2)^2 + 1)/((s - 2)^2 + 1)^2",
           "((s - 2)^2 - 1)/((s - 2)^2 + 1)^2");

    // A negative denominator leading coefficient becomes a leading minus.
    expect("(x+1)/(-x+1)", "-(x + 1)/(x - 1)");

    if (failures) {
        std::cout << failures << " simplification check(s) failed\n";
        return 1;
    }
    std::cout << "simplification checks passed\n";
    return 0;
}
