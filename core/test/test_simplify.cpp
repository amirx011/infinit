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

    // A 4x4 determinant written as the product of its elimination pivots.  The
    // last pivot is three fractions deep, and the cancellation that turns the
    // product into the 24-term polynomial only shows after expansion.
    expect("a*(g - b*f/a)*(m - c*k/a - (h - c*f/a)*(l - b*k/a)/(g - b*f/a))"
           "*(s - d*p/a - (j - d*f/a)*(q - b*p/a)/(g - b*f/a)"
           " - (n - d*k/a - (j - d*f/a)*(l - b*k/a)/(g - b*f/a))"
           "*(r - c*p/a - (h - c*f/a)*(q - b*p/a)/(g - b*f/a))"
           "/(m - c*k/a - (h - c*f/a)*(l - b*k/a)/(g - b*f/a)))",
           "a*g*m*s - a*g*n*r - a*h*l*s + a*h*n*q + a*j*l*r - a*j*m*q"
           " - b*f*m*s + b*f*n*r + b*h*k*s - b*h*n*p - b*j*k*r + b*j*m*p"
           " + c*f*l*s - c*f*n*q - c*g*k*s + c*g*n*p + c*j*k*q - c*j*l*p"
           " - d*f*l*r + d*f*m*q + d*g*k*r - d*g*m*p - d*h*k*q + d*h*l*p");

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
    // The same preference applies to a whole expression, not only a numerator.
    expect("-(x^2 - 2*ln(abs(y)))", "-x^2 + 2*ln(abs(y))");
    // sqrt(1 - x^2/4) has no sign to pull out of the root, so the two
    // denominators must meet on sqrt(4 - x^2) instead of a complex split.
    expect("1/sqrt(4-x^2) + 3/sqrt(1-x^2/4)", "7/sqrt((-x^2 + 4))");
    expect("1/sqrt(1-x^2/4) + 1/sqrt(4-x^2)", "3/sqrt((-x^2 + 4))");
    // A fraction inside a denominator is combined locally instead of being
    // left as 1/(C/x^2 - x/3).
    expect("1/(-1/3*x + C/x^2)", "-3*x^2/(x^3 - 3*C)");
    expect("1/x + 1/(-1/3*x + C/x^2)", "1/x - 3*x^2/(x^3 - 3*C)");
    // A coefficient does not need a visible multiplication sign in front of a
    // fraction: 2·1/(x²+16) belongs in one numerator.
    expect("1/(x^2+4) + 2/(x^2+16)", "1/(x^2 + 4) + 2/(x^2 + 16)");
    expect("1/2*s/(s^2+4)", "s/(2*(s^2 + 4))");
    // A repeated composite factor inside a numerator is pulled out even when
    // no polynomial factor cancels; this is the oscillatory Riccati solution.
    expect(
        "(-C*cos(1/2*sqrt(3)*ln(x)) - sqrt(3)*cos(1/2*sqrt(3)*ln(x)) + "
        "sin(1/2*sqrt(3)*ln(x))*(C*sqrt(3) - 1))/"
        "(2*x*(sin(1/2*sqrt(3)*ln(x)) + C*cos(1/2*sqrt(3)*ln(x))))",
        "(cos(1/2*sqrt(3)*ln(x))*(-C - sqrt(3)) + "
        "sin(1/2*sqrt(3)*ln(x))*(C*sqrt(3) - 1))/"
        "(2*x*(sin(1/2*sqrt(3)*ln(x)) + C*cos(1/2*sqrt(3)*ln(x))))");
    // Odd and even functions pull the sign out of their argument.
    expect("asin(-1/2*x)", "-asin(1/2*x)");
    expect("abs(-x)", "abs(x)");

    // A negative denominator leading coefficient becomes a leading minus.
    expect("(x+1)/(-x+1)", "-(x + 1)/(x - 1)");

    // Repeated differentiation of a product must end in the collected form,
    // not in a nested product that repeats every bracket.  This is what the
    // derivative page shows for each order, and each order feeds the next.
    {
        auto nthDerivative = [](const std::string& input, int order, bool strong) {
            Ex d = parseExpr(input);
            for (int k = 0; k < order; ++k) {
                d = diff(d, "x");
                d = strong ? simplifyStrong(simplify(d)) : simplify(d);
            }
            return d;
        };
        struct Case {
            const char* input;
            int order;
            const char* want;
        } cases[] = {
            {"A*(3*x^2+x^3)*e^x", 1, "A*x*exp(x)*(x^2 + 6*x + 6)"},
            {"A*(3*x^2+x^3)*e^x", 2,
             "A*exp(x)*(x^3 + 9*x^2 + 18*x + 6)"},
            {"A*(3*x^2+x^3)*e^x", 3,
             "A*exp(x)*(x^3 + 12*x^2 + 36*x + 24)"},
            {"x^3*e^x", 2, "x*exp(x)*(x^2 + 6*x + 6)"},
        };
        for (auto& c : cases) {
            Ex out = nthDerivative(c.input, c.order, true);
            Ex plain = nthDerivative(c.input, c.order, false);
            std::vector<std::string> syms;
            collectSymbols(out, syms);
            const bool equal =
                numericallyZero(Add(plain, Mul(Num(-1), out)), syms, 12, 1e-5);
            const bool printed = toPlain(out) == c.want;
            if (!equal || !printed) {
                ++failures;
                std::cout << "FAIL: d^" << c.order << "/dx^" << c.order << " "
                          << c.input << "\n  got:  " << toPlain(out);
                if (!printed) std::cout << "\n  want: " << c.want;
                if (!equal) std::cout << "\n  the result is not equal to the derivative";
                std::cout << "\n";
            }
        }
    }

    // A common factor can sit inside a sum that is itself a factor of a
    // product.  The top-level pass never reaches it, so the two sine terms
    // used to stay separate and the polynomial factor kept its canonical
    // (unsorted) order.
    expect("e^x*(sin(x)*(x^2+x) + sin(x)*(2*x+1))",
           "exp(x)*sin(x)*(x^2 + 3*x + 1)");
    {
        Ex in = parseExpr("(x^5+3*x^4+2*x^3+x^2+x+1)*e^x*sin(x)");
        Ex exact = diff(in, "x");
        Ex out = simplifyStrong(simplify(exact));
        const std::string want =
            "exp(x)*(cos(x)*(x^5 + 3*x^4 + 2*x^3 + x^2 + x + 1) + "
            "sin(x)*(x^5 + 8*x^4 + 14*x^3 + 7*x^2 + 3*x + 2))";
        const bool printed = toPlain(out) == want;
        const bool equal = numericallyZero(
            Add(out, Mul(Num(-1), exact)), {"x"}, 8, 1e-7);
        if (!printed || !equal) {
            ++failures;
            std::cout << "FAIL: product derivative presentation\n  got:  "
                      << toPlain(out) << "\n  want: " << want << "\n";
        }
    }

    if (failures) {
        std::cout << failures << " simplification check(s) failed\n";
        return 1;
    }
    std::cout << "simplification checks passed\n";
    return 0;
}
