#include "solver.hpp"
#include <iostream>
#include <vector>

using namespace ode;

static void show(const std::string& input, OdeOptions opt = {}) {
    Result r = solveODE(input, opt);
    std::cout << "\n=== " << input << " ===\n";
    std::cout << "ok=" << r.ok << " type=" << r.typeFa << " (" << r.typeId << ") order=" << r.order << "\n";
    if (!r.error.empty()) std::cout << "error: " << r.error << "\n";
    if (r.isExplicit) std::cout << "y = " << toPretty(r.explicitY) << "\n";
    else if (r.solution) std::cout << toPretty(r.solution) << " = C\n";
    if (r.isExplicit && r.solution) std::cout << "raw: " << toPlain(r.solution) << "\n";
    if (r.paramX) std::cout << "x(p) = " << toPretty(r.paramX) << "\n";
    if (r.paramY) std::cout << "y(p) = " << toPretty(r.paramY) << "\n";
    for (auto& s : r.singular) std::cout << "singular: " << toPretty(s) << "\n";
    for (auto& s : r.steps) std::cout << "  · " << s.title << (s.math ? ("  [" + toPlain(s.math) + "]") : "") << " " << s.note << "\n";
    for (auto& c : r.checks) std::cout << "  check " << c.name << ": " << (c.ok ? "OK" : "FAIL") << " — " << c.detail << "\n";
    for (auto& w : r.warnings) std::cout << "  warn: " << w << "\n";
}

int main() {
    show("y' = x/y");
    show("y' = (x^2+y^2)/(x*y)");
    show("y' = y");
    show("y' + y/x = x^2");
    show("y' = y^2");
    show("y' = 1 + y^2");
    show("y' = x*y + y^2");
    show("y = x*y' + (y')^2");
    show("y = 2*x*y' + (y')^2");
    show("(2x+y) + (x+2y)*y' = 0");
    show("(x+y)dx + (x+2y)dy = 0");
    show("2*y + x*y' = 0");
    show("y'' - 3y' + 2y = 0");
    show("y'' + y = 0");
    show("y'' = 6x");
    show("y'' - y = 0");
    show("y' = (x+y)^2");

    std::cout << "\n--- Wronskian ---\n";
    for (auto fns : {std::vector<std::string>{"sin(x)", "cos(x)"},
                     std::vector<std::string>{"x", "2x"},
                     std::vector<std::string>{"1", "x", "x^2"}}) {
        Result r = solveWronskian(fns, "x");
        std::cout << "W = " << toPretty(r.wronskian) << " independent=" << r.independent << "\n";
    }
    std::cout << "\n--- Envelope ---\n";
    Result e = solveEnvelope("y = c*x + c^2", "c");
    std::cout << "ok=" << e.ok << " err=" << e.error << "\n";
    for (auto& c : e.envelope) std::cout << "curve: " << toPretty(c) << " = 0\n";
    Result e2 = solveEnvelope("x^2 + y^2 = c^2", "c");
    std::cout << "ok=" << e2.ok << " err=" << e2.error << "\n";
    for (auto& c : e2.envelope) std::cout << "curve: " << toPretty(c) << " = 0\n";
}
