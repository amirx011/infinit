// Infinit Lab — symbol-by-symbol differentiation of the expression tree.
//
// The parser builds an AST (Expr nodes: Num, Sym, Add, Mul, Pow, Func). Here we
// walk that tree and apply one derivation rule per node:
//
//   Add  -> linearity (sum rule)
//   Mul  -> product rule (one term per factor that depends on the variable)
//   Pow  -> power rule / exponential rule / logarithmic differentiation
//   Func -> table derivative composed with the inner derivative (chain rule)
//
// Because every rule recurses on the children of the node, the chain rule and
// the product rule come out of the tree structure itself; no pattern matching
// on strings is involved.
// SPDX-License-Identifier: Apache-2.0
#include "calculus_ext.hpp"

#include <algorithm>
#include <sstream>

namespace ode {
namespace {

std::string clip(const std::string& s, size_t maxLen = 72) {
    if (s.size() <= maxLen) return s;
    return s.substr(0, maxLen - 1) + "…";
}

void note(std::vector<DerivationNote>* notes, const char* rule, const Ex& subject, int depth) {
    if (!notes) return;
    if (depth > 3) return;              // keep the trace readable
    if (notes->size() >= 14) return;
    const std::string text = clip(toPlain(subject));
    for (const auto& n : *notes) {
        if (n.rule == rule && n.subject == text) return;
    }
    notes->push_back({rule, text, depth});
}

} // namespace

Ex diffAst(const Ex& a, const std::string& v, std::vector<DerivationNote>* notes, int depth) {
    if (!a) return Num(0);
    switch (a->kind) {
        case Kind::Num:
            note(notes, "constant", a, depth);
            return Num(0);

        case Kind::Sym:
            if (a->isSym(v)) {
                note(notes, "variable", a, depth);
                return Num(1);
            }
            note(notes, "constant", a, depth);
            return Num(0);

        case Kind::Add: {
            note(notes, "sum", a, depth);
            std::vector<Ex> terms;
            terms.reserve(a->args.size());
            for (const auto& child : a->args) terms.push_back(diffAst(child, v, notes, depth + 1));
            return Add(terms);
        }

        case Kind::Mul: {
            size_t dependent = 0, last = 0;
            for (size_t i = 0; i < a->args.size(); ++i) {
                if (dependsOn(a->args[i], v)) {
                    ++dependent;
                    last = i;
                }
            }
            if (dependent == 0) {
                note(notes, "constant", a, depth);
                return Num(0);
            }
            if (dependent == 1) {
                // constant multiple rule: (c·u)' = c·u'
                note(notes, "constant_multiple", a, depth);
                std::vector<Ex> factors = a->args;
                factors[last] = diffAst(factors[last], v, notes, depth + 1);
                return Mul(factors);
            }
            // product rule over every factor that carries the variable
            note(notes, a->args.size() == 2 ? "product" : "product_general", a, depth);
            std::vector<Ex> terms;
            for (size_t i = 0; i < a->args.size(); ++i) {
                if (!dependsOn(a->args[i], v)) continue;
                std::vector<Ex> factors = a->args;
                factors[i] = diffAst(factors[i], v, notes, depth + 1);
                terms.push_back(Mul(factors));
            }
            return Add(terms);
        }

        case Kind::Pow: {
            const Ex& base = a->args[0];
            const Ex& exponent = a->args[1];
            const bool baseConst = !dependsOn(base, v);
            const bool expConst = !dependsOn(exponent, v);
            if (expConst) {
                // power rule: n·b^(n-1)·b'
                note(notes, "power", a, depth);
                Ex inner = diffAst(base, v, notes, depth + 1);
                return Mul(exponent, Pow(base, Add(exponent, Num(-1))), inner);
            }
            if (baseConst) {
                // exponential rule: b^e·ln(b)·e'
                note(notes, "exponential", a, depth);
                Ex inner = diffAst(exponent, v, notes, depth + 1);
                return Mul(a, Fn("ln", {base}), inner);
            }
            // general case: logarithmic differentiation
            note(notes, "general_power", a, depth);
            Ex de = diffAst(exponent, v, notes, depth + 1);
            Ex db = diffAst(base, v, notes, depth + 1);
            return Mul(a, Add(Mul(de, Fn("ln", {base})), Mul(exponent, db, Pow(base, Num(-1)))));
        }

        case Kind::Func: {
            // Deriv nodes carry their own order (d^n/dx^n f).
            if (a->name == "Deriv") {
                if (a->args.size() >= 2 && a->args[1]->isSym(v)) {
                    note(notes, "derivative_node", a, depth);
                    return Deriv(a->args[0], v, derivOrder(a) + 1);
                }
                return Num(0);
            }
            if (a->name == "Integral" && a->args.size() == 2 && a->args[1]->isSym(v)) {
                note(notes, "fundamental_theorem", a, depth);
                return a->args[0];
            }
            if (a->args.empty()) return Num(0);
            const Ex& u = a->args[0];
            // Chain rule: d/dv f(u) = f'(u) * u'
            Ex du = diffAst(u, v, notes, depth + 1);
            if (du->isZero()) return Num(0);
            const std::string& f = a->name;
            Ex outer;
            if (f == "sin") outer = Fn("cos", {u});
            else if (f == "cos") outer = Mul(Num(-1), Fn("sin", {u}));
            else if (f == "tan") outer = Pow(Fn("cos", {u}), Num(-2));
            else if (f == "cot") outer = Mul(Num(-1), Pow(Fn("sin", {u}), Num(-2)));
            else if (f == "sec") outer = Mul(Fn("sec", {u}), Fn("tan", {u}));
            else if (f == "csc") outer = Mul(Num(-1), Fn("csc", {u}), Fn("cot", {u}));
            else if (f == "asin")
                outer = Pow(Add(Num(1), Mul(Num(-1), Pow(u, Num(2)))), Num(Rational(-1, 2)));
            else if (f == "acos")
                outer = Mul(Num(-1),
                            Pow(Add(Num(1), Mul(Num(-1), Pow(u, Num(2)))), Num(Rational(-1, 2))));
            else if (f == "atan") outer = Pow(Add(Num(1), Pow(u, Num(2))), Num(-1));
            else if (f == "sinh") outer = Fn("cosh", {u});
            else if (f == "cosh") outer = Fn("sinh", {u});
            else if (f == "tanh") outer = Pow(Fn("cosh", {u}), Num(-2));
            else if (f == "asinh")
                outer = Pow(Add(Pow(u, Num(2)), Num(1)), Num(Rational(-1, 2)));
            else if (f == "acosh")
                outer = Mul(Pow(Add(u, Num(-1)), Num(Rational(-1, 2))),
                            Pow(Add(u, Num(1)), Num(Rational(-1, 2))));
            else if (f == "atanh")
                outer = Pow(Add(Num(1), Mul(Num(-1), Pow(u, Num(2)))), Num(-1));
            else if (f == "ln" || f == "log") {
                if (u->isFunc("abs")) {
                    note(notes, "log_abs", a, depth);
                    return Mul(diffAst(u->args[0], v, notes, depth + 1), Pow(u->args[0], Num(-1)));
                }
                outer = Pow(u, Num(-1));
            } else if (f == "exp") {
                note(notes, "exponential", a, depth);
                outer = Fn("exp", {u});
            } else if (f == "erf")
                outer = Mul(Num(2), Pow(Sym("pi"), Num(Rational(-1, 2))),
                            Fn("exp", {Mul(Num(-1), Pow(u, Num(2)))}));
            else if (f == "erfc")
                outer = Mul(Num(-2), Pow(Sym("pi"), Num(Rational(-1, 2))),
                            Fn("exp", {Mul(Num(-1), Pow(u, Num(2)))}));
            else if (f == "abs") {
                note(notes, "absolute_value", a, depth);
                return Mul(du, u, Pow(a, Num(-1)));
            } else if (f == "sqrt") {
                note(notes, "power", a, depth);
                outer = Mul(Num(Rational(1, 2)), Pow(u, Num(Rational(-1, 2))));
            } else {
                note(notes, "formal_derivative", a, depth);
                return Deriv(Fn(f, a->args), v, 1);
            }
            note(notes, "chain", a, depth);
            return Mul(outer, du);
        }
    }
    return Num(0);
}

// ---------------------------------------------------------------- implicit form
//
// F(x, y) = 0 defines y(x).  Differentiating totally with respect to x:
//     F_x + F_y · y' = 0          =>  y'  = -F_x / F_y
// and applying the total derivative  D = ∂/∂x + y'·∂/∂y  to y' gives y'', then
// applying D once more gives y'''.  Because y' and y'' are already written in
// terms of x and y, the same operator closes the recursion at every order.
Ex implicitDerivativeOf(const Ex& F, const std::string& x, const std::string& y, int order,
                        Ex* first) {
    if (order < 1) order = 1;
    Ex Fx = simplify(diffAst(F, x));
    Ex Fy = simplify(diffAst(F, y));
    if (Fy->isZero()) throw std::runtime_error("F_y = 0: the equation does not define y implicitly");
    Ex yp = simplify(Mul(Num(-1), Mul(Fx, Pow(Fy, Num(-1)))));
    if (first) *first = yp;
    if (order == 1) return yp;

    Ex cur = yp;
    for (int k = 2; k <= order; ++k) {
        Ex total = Add(diffAst(cur, x), Mul(yp, diffAst(cur, y)));
        cur = simplify(total);
    }
    return cur;
}

} // namespace ode
