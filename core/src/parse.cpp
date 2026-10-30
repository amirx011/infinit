// Infinit Lab — recursive descent parser
// SPDX-License-Identifier: MIT
#include "expr.hpp"
#include <cctype>
#include <regex>
#include <sstream>

namespace ode {

namespace {

enum class Tok { End, Num, Ident, Plus, Minus, Star, Slash, Caret, LParen, RParen, Comma, Pipe, Eq, Prime };

struct Token {
    Tok t = Tok::End;
    std::string text;
    Rational num;
    size_t pos = 0;
};

struct Lexer {
    std::string s;
    size_t i = 0;
    explicit Lexer(std::string src) : s(std::move(src)) {}

    Token next() {
        while (i < s.size() && std::isspace((unsigned char)s[i])) ++i;
        Token tk; tk.pos = i;
        if (i >= s.size()) { tk.t = Tok::End; return tk; }
        char c = s[i];
        if (std::isdigit((unsigned char)c) || (c == '.' && i + 1 < s.size() && std::isdigit((unsigned char)s[i + 1]))) {
            size_t b = i;
            while (i < s.size() && (std::isdigit((unsigned char)s[i]) || s[i] == '.')) ++i;
            std::string num = s.substr(b, i - b);
            size_t dot = num.find('.');
            if (dot == std::string::npos) {
                try { tk.num = Rational(std::stoll(num)); }
                catch (...) { throw std::runtime_error("عدد بسیار بزرگ: " + num); }
            } else {
                std::string ip = num.substr(0, dot), fp = num.substr(dot + 1);
                long long denom = 1;
                for (size_t k = 0; k < fp.size(); ++k) denom *= 10;
                long long intpart = ip.empty() ? 0 : std::stoll(ip);
                long long frac = fp.empty() ? 0 : std::stoll(fp);
                long long sign = 1;
                if (intpart < 0) sign = -1;
                tk.num = Rational(sign * (std::llabs(intpart) * denom + frac), denom);
            }
            tk.t = Tok::Num; tk.text = num; return tk;
        }
        if (std::isalpha((unsigned char)c) || c == '_') {
            size_t b = i;
            while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '_')) ++i;
            tk.t = Tok::Ident; tk.text = s.substr(b, i - b); return tk;
        }
        ++i;
        switch (c) {
            case '+': tk.t = Tok::Plus; break;
            case '-': tk.t = Tok::Minus; break;
            case '*':
                if (i < s.size() && s[i] == '*') { ++i; tk.t = Tok::Caret; }
                else tk.t = Tok::Star;
                break;
            case '/': tk.t = Tok::Slash; break;
            case '^': tk.t = Tok::Caret; break;
            case '(': case '[': case '{': tk.t = Tok::LParen; break;
            case ')': case ']': case '}': tk.t = Tok::RParen; break;
            case ',': tk.t = Tok::Comma; break;
            case '|': tk.t = Tok::Pipe; break;
            case '=': tk.t = Tok::Eq; if (i < s.size() && s[i] == '=') ++i; break;
            case '\'': {
                tk.t = Tok::Prime; break;
            }
            default:
                throw std::runtime_error(std::string("نویسه ناشناخته: '") + c + "'");
        }
        return tk;
    }
};

struct Parser {
    Lexer lx;
    Token cur;
    std::string indep;
    int depth = 0;

    Parser(const std::string& s, std::string indepVar) : lx(s), indep(std::move(indepVar)) { cur = lx.next(); }

    void adv() { cur = lx.next(); }
    bool accept(Tok t) { if (cur.t == t) { adv(); return true; } return false; }
    void expect(Tok t, const char* what) {
        if (cur.t != t) throw std::runtime_error(std::string("انتظار ") + what + " بود");
        adv();
    }

    bool startsAtom(Tok t) {
        return t == Tok::Num || t == Tok::Ident || t == Tok::LParen || t == Tok::Pipe;
    }

    Ex parse() {
        Ex e = sum();
        if (cur.t != Tok::End) throw std::runtime_error("عبارت اضافی در ورودی");
        return e;
    }

    Ex sum() {
        Ex e = product();
        while (cur.t == Tok::Plus || cur.t == Tok::Minus) {
            bool neg = cur.t == Tok::Minus;
            adv();
            Ex r = product();
            e = neg ? Add(e, Mul(Num(-1), r)) : Add(e, r);
        }
        return e;
    }

    Ex product() {
        Ex e = unary();
        while (true) {
            if (cur.t == Tok::Star || cur.t == Tok::Slash) {
                bool div = cur.t == Tok::Slash;
                adv();
                Ex r = unary();
                e = div ? Mul(e, Pow(r, Num(-1))) : Mul(e, r);
            } else if (startsAtom(cur.t) && !(cur.t == Tok::Pipe && false)) {
                // implicit multiplication: 2x, 3(x+1), (x+1)(x-1)
                Ex r = unary();
                e = Mul(e, r);
            } else break;
        }
        return e;
    }

    Ex unary() {
        if (accept(Tok::Minus)) return Mul(Num(-1), unary());
        if (accept(Tok::Plus)) return unary();
        return power();
    }

    Ex power() {
        Ex b = atom();
        if (accept(Tok::Caret)) {
            Ex e = unary(); // right associative
            return Pow(b, e);
        }
        return b;
    }

    Ex atom() {
        if (++depth > 200) throw std::runtime_error("عبارت بسیار پیچیده است");
        Ex out;
        if (cur.t == Tok::Num) {
            out = Num(cur.num); adv();
        } else if (cur.t == Tok::LParen) {
            adv();
            out = sum();
            expect(Tok::RParen, "')'");
        } else if (cur.t == Tok::Pipe) {
            adv();
            Ex inner = sum();
            expect(Tok::Pipe, "'|'");
            out = Fn("abs", {inner});
        } else if (cur.t == Tok::Ident) {
            std::string name = cur.text;
            adv();
            if (cur.t == Tok::LParen) {
                adv();
                std::vector<Ex> args;
                if (cur.t != Tok::RParen) {
                    args.push_back(sum());
                    while (accept(Tok::Comma)) args.push_back(sum());
                }
                expect(Tok::RParen, "')'");
                std::string lower = name;
                for (auto& ch : lower) ch = (char)std::tolower((unsigned char)ch);
                if (lower == "sqrt") out = Sqrt(args[0]);
                else if (lower == "log") out = Fn("ln", args);
                else if (lower == "deriv" && args.size() == 3) {
                    // produced by preprocessDifferentials for dy/dx, d^2y/dx^2
                    std::string v = args[1]->isSym() ? args[1]->name : indep;
                    int order = args[2]->isNum() ? (int)args[2]->num.n : 1;
                    out = Deriv(args[0], v, order);
                }
                else out = Fn(lower, args);
            } else {
                std::string lower = name;
                for (auto& ch : lower) ch = (char)std::tolower((unsigned char)ch);
                if (lower == "pi" || lower == "π") out = Sym("pi");
                else if (lower == "e" && name == "e") out = Sym("e");
                else if (lower == "sqrt") throw std::runtime_error("sqrt به آرگومان نیاز دارد");
                else out = Sym(name);
            }
            if (cur.t == Tok::Prime) {
                int order = 0;
                while (cur.t == Tok::Prime) { ++order; adv(); }
                if (!out->isSym()) throw std::runtime_error("نماد مشتق نامعتبر است");
                out = Deriv(out, indep, order);
            }
        } else {
            throw std::runtime_error("عبارت نامعتبر است");
        }
        --depth;
        return out;
    }
};

std::string normalizeInput(std::string s) {
    s = replaceAll(s, "−", "-");
    s = replaceAll(s, "×", "*");
    s = replaceAll(s, "·", "*");
    s = replaceAll(s, "÷", "/");
    s = replaceAll(s, "√", "sqrt");
    s = replaceAll(s, "π", "pi");
    s = replaceAll(s, "′", "'");
    s = replaceAll(s, "’", "'");
    s = replaceAll(s, "²", "^2");
    s = replaceAll(s, "³", "^3");
    s = replaceAll(s, "⁴", "^4");
    s = replaceAll(s, "≤", "<=");
    return s;
}

// convert dy/dx, d^2y/dx^2, d/dx(...) into Deriv(f,x,n)
std::string preprocessDifferentials(const std::string& in, const std::string& indep) {
    std::string s = in;
    try {
        std::regex d2(R"(d\s*\^\s*(\d+)\s*([A-Za-z_][A-Za-z0-9_]*)\s*/\s*d\s*([A-Za-z_][A-Za-z0-9_]*)\s*\^\s*\d+)");
        s = std::regex_replace(s, d2, "Deriv($2,$3,$1)");
        std::regex d2op(R"(d\s*\^\s*(\d+)\s*/\s*d\s*([A-Za-z_][A-Za-z0-9_]*)\s*\^\s*\d+\s*\(([^()]*)\))");
        s = std::regex_replace(s, d2op, "Deriv($3,$2,$1)");
        std::regex d1(R"(d\s*([A-Za-z_][A-Za-z0-9_]*)\s*/\s*d\s*([A-Za-z_][A-Za-z0-9_]*))");
        s = std::regex_replace(s, d1, "Deriv($1,$2,1)");
        std::regex d1op(R"(d\s*/\s*d\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(([^()]*)\))");
        s = std::regex_replace(s, d1op, "Deriv($2,$1,1)");
    } catch (const std::regex_error&) {
        // leave as-is
    }
    (void)indep;
    return s;
}

static const char* kFunctionNames =
    "erfc|erf|asinh|acosh|atanh|asin|acos|atan|sinh|cosh|tanh|sin|cos|tan|cot|sec|csc|ln|log|exp|sqrt|abs";

static std::string naturalizeImplicit(std::string s, const std::vector<std::string>& vars) {
    std::string vc;
    for (auto& v : vars) {
        if (v.size() == 1 && std::isalpha((unsigned char)v[0])) vc += v[0];
    }
    if (vc.empty()) return s;
    std::string cls = "[" + vc + "]";
    try {
        // x sin y / x siny  ->  x*sin y / x*siny
        std::regex beforeFn("(" + cls + ")('*)\\s*(?=(" + kFunctionNames + "))", std::regex::icase);
        s = std::regex_replace(s, beforeFn, "$1$2*");
        for (int pass = 0; pass < 4; ++pass) {
            std::string prev = s;
            // cos^2(x)  ->  (cos(x))^2
            std::regex fnPowParen("(^|[^A-Za-z_])(" + std::string(kFunctionNames) +
                                  ")\\s*\\^\\s*(-?[0-9]+)\\s*\\(([^()]*)\\)", std::regex::icase);
            s = std::regex_replace(s, fnPowParen, "$1$2($4)^$3");
            // cos^2x / sin^2 x  ->  (cos(x))^2
            std::regex fnPowVar("(^|[^A-Za-z_])(" + std::string(kFunctionNames) +
                                ")\\s*\\^\\s*(-?[0-9]+)\\s*(" + cls + "+)('*)", std::regex::icase);
            s = std::regex_replace(s, fnPowVar, "$1$2($4$5)^$3");
            // sinx / sin x / ln y'  ->  sin(x) / ln(y')
            std::regex fnArg("(^|[^A-Za-z_])(" + std::string(kFunctionNames) +
                             ")\\s*(" + cls + "+)('*)", std::regex::icase);
            s = std::regex_replace(s, fnArg, "$1$2($3$4)");
            // xy / x y' / cx  ->  x*y / x*y' / c*x
            std::regex varVar("(" + cls + ")('*)(?=" + cls + ")", std::regex::icase);
            s = std::regex_replace(s, varVar, "$1$2*");
            if (s == prev) break;
        }
    } catch (const std::regex_error&) {
        return s;
    }
    return s;
}

Ex parseWith(const std::string& raw, const std::string& indep, std::string* depOut,
             const std::vector<std::string>& implicitVars = {}) {
    std::string s = preprocessDifferentials(normalizeInput(raw), indep);
    if (!implicitVars.empty()) s = naturalizeImplicit(s, implicitVars);
    s = replaceAll(s, "==", "=");
    // split on top-level '='
    int par = 0; size_t eq = std::string::npos;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '(' || s[i] == '[' || s[i] == '{') ++par;
        else if (s[i] == ')' || s[i] == ']' || s[i] == '}') --par;
        else if (s[i] == '=' && par == 0) { eq = i; break; }
    }
    if (eq == std::string::npos) {
        Parser p(s, indep);
        Ex e = p.parse();
        if (depOut) {
            std::vector<std::string> syms; collectSymbols(e, syms);
            for (auto& x : syms) if (x != indep && x.rfind("C", 0) != 0) { *depOut = x; break; }
        }
        return e;
    }
    std::string l = s.substr(0, eq), r = s.substr(eq + 1);
    if (trim(l).empty() || trim(r).empty()) throw std::runtime_error("دو طرف معادله لازم است");
    Parser pl(l, indep), pr(r, indep);
    Ex L = pl.parse(), R = pr.parse();
    if (depOut) {
        std::vector<std::string> syms; collectSymbols(L, syms); collectSymbols(R, syms);
        for (auto& x : syms) {
            if (x == indep) continue;
            if (dependsOn(L, x) || dependsOn(R, x)) { *depOut = x; break; }
        }
    }
    return Add(L, Mul(Num(-1), R));
}

} // namespace

Ex parseExpr(const std::string& s) {
    return parseWith(s, "x", nullptr);
}
Ex parseEquation(const std::string& s, std::string* lhsOut, std::string* rhsOut) {
    std::string norm = normalizeInput(s);
    int par = 0; size_t eq = std::string::npos;
    for (size_t i = 0; i < norm.size(); ++i) {
        if (norm[i] == '(') ++par; else if (norm[i] == ')') --par;
        else if (norm[i] == '=' && par == 0) { eq = i; break; }
    }
    Ex e = parseWith(s, "x", nullptr);
    if (eq != std::string::npos) {
        if (lhsOut) *lhsOut = trim(norm.substr(0, eq));
        if (rhsOut) *rhsOut = trim(norm.substr(eq + 1));
    } else {
        if (lhsOut) *lhsOut = trim(norm);
        if (rhsOut) *rhsOut = "0";
    }
    return e;
}
Ex parseODEExpr(const std::string& s, const std::string& indep, std::string* depOut,
                const std::vector<std::string>& implicitVars) {
    return parseWith(s, indep, depOut, implicitVars);
}

} // namespace ode
