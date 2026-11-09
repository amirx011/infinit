// Infinit Lab — recursive descent parser
// SPDX-License-Identifier: Apache-2.0
#include "expr.hpp"
#include <cctype>
#include <cstring>
#include <regex>
#include <sstream>

namespace ode {

namespace {

enum class Tok { End, Num, Ident, Plus, Minus, Star, Slash, Caret, LParen, RParen, Comma, Pipe, Eq, Prime, Bang };

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
                if (num.size() > 18) throw EngineError("number_too_large", "number too large", "number too large: " + num);
                try { tk.num = Rational(std::stoll(num)); }
                catch (...) { throw EngineError("number_too_large", "number too large", "number too large: " + num); }
            } else {
                std::string ip = num.substr(0, dot), fp = num.substr(dot + 1);
                if (ip.size() > 18 || fp.size() > 18)
                    throw EngineError("number_too_large", "number too large", "number too large: " + num);
                long long denom = 1;
                for (size_t k = 0; k < fp.size(); ++k) denom *= 10;
                long long intpart = ip.empty() ? 0 : std::stoll(ip);
                long long frac = fp.empty() ? 0 : std::stoll(fp);
                long long sign = 1;
                if (intpart < 0) sign = -1;
                tk.num = Rational(sign * (std::llabs(intpart) * denom + frac), denom);
            }
            // Scientific notation: 1e-3, 2E5.  Only consume `e` when a valid
            // exponent follows, so `2*e` and `2e` keep their symbolic meaning.
            if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
                size_t j = i + 1;
                int expSign = 1;
                if (j < s.size() && (s[j] == '+' || s[j] == '-')) {
                    if (s[j] == '-') expSign = -1;
                    ++j;
                }
                if (j < s.size() && std::isdigit((unsigned char)s[j])) {
                    long long exp = 0;
                    while (j < s.size() && std::isdigit((unsigned char)s[j])) {
                        exp = exp * 10 + (s[j] - '0');
                        if (exp > 18) throw EngineError("number_too_large", "number too large", "number too large: " + num);
                        ++j;
                    }
                    i = j;
                    Rational scale(1);
                    for (long long k = 0; k < exp; ++k) scale = scale * Rational(10);
                    tk.num = expSign > 0 ? tk.num * scale : tk.num / scale;
                }
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
            case '!': {
                tk.t = Tok::Bang; break;
            }
            default:
                {
                    // Consume the whole UTF-8 code point.  Slicing one byte
                    // out of a multi-byte character produced invalid UTF-8,
                    // which crashed the Dart string decoder.
                    const size_t start = i - 1;
                    size_t len = 1;
                    const unsigned char lead = (unsigned char)c;
                    if ((lead & 0xE0) == 0xC0) len = 2;
                    else if ((lead & 0xF0) == 0xE0) len = 3;
                    else if ((lead & 0xF8) == 0xF0) len = 4;
                    for (size_t k = 1; k < len && i < s.size(); ++k) {
                        if (((unsigned char)s[i] & 0xC0) != 0x80) break;
                        ++i;
                    }
                    std::ostringstream detail;
                    detail << "unknown character at byte " << start << " (";
                    for (size_t k = start; k < i; ++k) {
                        if (k != start) detail << ' ';
                        detail << "0x" << std::hex << std::uppercase
                               << (unsigned)(unsigned char)s[k] << std::dec;
                    }
                    detail << ")";
                    throw EngineError("bad_characters", "invalid character",
                                      detail.str());
                }
        }
        return tk;
    }
};

struct Parser {
    Lexer lx;
    Token cur;
    std::string indep;
    int depth = 0;
    int absDepth = 0; // tracks open `|...|` groups so a closing `|` is not multiplied

    Parser(const std::string& s, std::string indepVar) : lx(s), indep(std::move(indepVar)) { cur = lx.next(); }

    void adv() { cur = lx.next(); }
    bool accept(Tok t) { if (cur.t == t) { adv(); return true; } return false; }
    void expect(Tok t, const char* what) {
        if (cur.t != t) throw EngineError("unexpected_token", "unexpected token", std::string("expected ") + what);
        adv();
    }

    bool startsAtom(Tok t) {
        return t == Tok::Num || t == Tok::Ident || t == Tok::LParen || t == Tok::Pipe;
    }

    Ex parse() {
        Ex e = sum();
        if (cur.t != Tok::End) throw EngineError("trailing_input", "unexpected trailing input");
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
            } else if (startsAtom(cur.t) && !(cur.t == Tok::Pipe && absDepth > 0)) {
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
            // Student notation `e^2t` means e^(2t), not (e^2)*t.  Apply the
            // rule only for the base e and only when the exponent starts with
            // a number, so `e^x y` keeps standard precedence.
            if (b->isSym("e") && e->isNum()) {
                while (cur.t == Tok::Ident) e = Mul(e, unary());
            }
            return Pow(b, e);
        }
        return b;
    }

    Ex atom() {
        if (++depth > 200) throw EngineError("too_complex", "expression too deeply nested");
        Ex out;
        if (cur.t == Tok::Num) {
            out = Num(cur.num); adv();
        } else if (cur.t == Tok::LParen) {
            adv();
            out = sum();
            expect(Tok::RParen, "')'");
        } else if (cur.t == Tok::Pipe) {
            adv();
            ++absDepth;
            Ex inner = sum();
            --absDepth;
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
                else {
                    if (!isKnownFunctionName(lower) && lower != "deriv" &&
                        lower != "integral") {
                        throw EngineError("unknown_function",
                                          "unknown function",
                                          "unknown function: " + name);
                    }
                    out = Fn(lower, args);
                }
            } else {
                std::string lower = name;
                for (auto& ch : lower) ch = (char)std::tolower((unsigned char)ch);
                if (lower == "pi" || lower == "π") out = Sym("pi");
                else if (lower == "e" && name == "e") out = Sym("e");
                else if (lower == "sqrt") throw EngineError("sqrt_argument", "sqrt requires an argument");
                else out = Sym(name);
            }
            if (cur.t == Tok::Prime) {
                int order = 0;
                while (cur.t == Tok::Prime) { ++order; adv(); }
                if (!out->isSym()) throw EngineError("invalid_derivative", "invalid derivative symbol");
                out = Deriv(out, indep, order);
            }
        } else {
            throw EngineError("invalid_expression", "invalid expression");
        }
        // Postfix factorial binds tighter than a power: n! is fact(n).
        while (accept(Tok::Bang)) out = Fn("fact", {out});
        --depth;
        return out;
    }
};

namespace {
// Decode one superscript digit or sign at s[i].  Returns the digit value, -1
// for a sign (with `sign` set to +1/-1) and -2 when this is not a superscript.
int superscriptAt(const std::string& s, size_t i, size_t& len, int& sign) {
    static const char* digits[] = {
        "\xE2\x81\xB0", "\xE2\x81\xB1", "\xE2\x81\xB2", "\xE2\x81\xB3", "\xE2\x81\xB4",
        "\xE2\x81\xB5", "\xE2\x81\xB6", "\xE2\x81\xB7", "\xE2\x81\xB8", "\xE2\x81\xB9"};
    for (int d = 0; d < 10; ++d) {
        const size_t n = std::strlen(digits[d]);
        if (s.compare(i, n, digits[d]) == 0) { len = n; return d; }
    }
    // Superscript one, two and three also live in Latin-1.
    if (s.compare(i, 2, "\xC2\xB9") == 0) { len = 2; return 1; }
    if (s.compare(i, 2, "\xC2\xB2") == 0) { len = 2; return 2; }
    if (s.compare(i, 2, "\xC2\xB3") == 0) { len = 2; return 3; }
    if (s.compare(i, 3, "\xE2\x81\xBA") == 0) { len = 3; sign = 1; return -1; }
    if (s.compare(i, 3, "\xE2\x81\xBB") == 0) { len = 3; sign = -1; return -1; }
    return -2;
}
} // namespace

std::string normalizeInput(std::string s) {
    s = replaceAll(s, "−", "-");
    s = replaceAll(s, "×", "*");
    s = replaceAll(s, "·", "*");
    s = replaceAll(s, "⋅", "*");
    s = replaceAll(s, "÷", "/");
    s = replaceAll(s, "√", "sqrt");
    s = replaceAll(s, "π", "pi");
    s = replaceAll(s, "′", "'");
    s = replaceAll(s, "’", "'");
    s = replaceAll(s, "≤", "<=");
    // Superscript runs become one exponent: x⁶y -> x^6y, x⁻² -> x^-2,
    // x²³ -> x^23.  A run must contain at least one digit.
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        int sign = 0;
        std::string digits;
        size_t j = i;
        while (j < s.size()) {
            size_t len = 0;
            const int d = superscriptAt(s, j, len, sign);
            if (d == -2) break;
            if (d >= 0) digits += (char)('0' + d);
            j += len;
        }
        if (!digits.empty()) {
            if (!out.empty() && out.back() == '^') out.pop_back();
            out += '^';
            if (sign < 0) out += '-';
            out += digits;
            i = j;
            continue;
        }
        // A sign without digits is malformed but must still be copied whole so
        // the output stays valid UTF-8.
        if (j > i) { out += s.substr(i, j - i); i = j; continue; }
        out += s[i++];
    }
    return out;
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

// Names that must survive the implicit-product splitter as a unit. Function
// names were already handled above; the constants, differential symbols and
// Greek variable names would otherwise turn into a product of letters.
static const char* kProtectedNames[] = {
    "erfc", "erf", "asinh", "acosh", "atanh", "asin", "acos", "atan",
    "sinh", "cosh", "tanh", "sin", "cos", "tan", "cot", "sec", "csc",
    "ln", "log", "exp", "sqrt", "abs", "Deriv", "Integral",
    "pi", "inf", "dx", "dy", "dt",
    "alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta", "theta",
    "iota", "kappa", "lambda", "mu", "nu", "xi", "omicron", "rho",
    "sigma", "tau", "upsilon", "phi", "chi", "psi", "omega"
};

bool asciiEqualFold(const std::string& a, const char* b) {
    size_t i = 0;
    for (; i < a.size() && b[i]; ++i) {
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    }
    return i == a.size() && b[i] == '\0';
}

bool isProtectedName(const std::string& s) {
    for (const char* p : kProtectedNames) {
        if (asciiEqualFold(s, p)) return true;
    }
    if (s.size() >= 2 && s[0] == 'C') {
        bool digits = true;
        for (size_t i = 1; i < s.size(); ++i) {
            if (!std::isdigit((unsigned char)s[i])) { digits = false; break; }
        }
        if (digits) return true;
    }
    return false;
}

bool containsProtectedMulti(const std::string& run) {
    for (const char* p : kProtectedNames) {
        const size_t len = std::strlen(p);
        if (len < 2 || len > run.size()) continue;
        for (size_t start = 0; start + len <= run.size(); ++start) {
            bool match = true;
            for (size_t k = 0; k < len; ++k) {
                if (std::tolower((unsigned char)run[start + k]) !=
                    std::tolower((unsigned char)p[k])) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
    }
    return false;
}

// Longest known function name starting at `pos` and ending no later than
// `end`.  Longest wins so `sinh` is not read as `sin` followed by `h`.
size_t functionLengthAt(const std::string& s, size_t pos, size_t end) {
    static const char* names[] = {
        "asinh", "acosh", "atanh", "erfc", "asin", "acos", "atan", "sinh",
        "cosh", "tanh", "sqrt", "sin", "cos", "tan", "cot", "sec", "csc",
        "erf", "exp", "log", "abs", "ln"};
    size_t best = 0;
    for (const char* name : names) {
        const size_t len = std::strlen(name);
        if (len <= best || len > end - pos) continue;
        bool match = true;
        for (size_t k = 0; k < len; ++k) {
            if (std::tolower((unsigned char)s[pos + k]) !=
                std::tolower((unsigned char)name[k])) {
                match = false;
                break;
            }
        }
        if (match) best = len;
    }
    return best;
}

// `csinx` is one identifier to the lexer.  Insert a product before a function
// name glued to a preceding letter (`c*sinx`), then the function pass adds the
// parentheses.  Whole names such as `asinh` and `sinh` are left intact.
std::string separateGluedFunctions(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    size_t i = 0;
    while (i < s.size()) {
        if (!(std::isalpha((unsigned char)s[i]) || s[i] == '_')) {
            out += s[i++];
            continue;
        }
        size_t j = i + 1;
        while (j < s.size() && (std::isalnum((unsigned char)s[j]) || s[j] == '_')) ++j;
        size_t pos = i;
        while (pos < j) {
            const size_t here = functionLengthAt(s, pos, j);
            if (here) {
                out += s.substr(pos, here);
                pos += here;
                continue;
            }
            size_t next = std::string::npos;
            for (size_t k = pos + 1; k < j; ++k) {
                if (functionLengthAt(s, k, j)) { next = k; break; }
            }
            if (next == std::string::npos) {
                out += s.substr(pos, j - pos);
                break;
            }
            out += s.substr(pos, next - pos);
            out += '*';
            pos = next;
        }
        i = j;
    }
    return out;
}

// The lexer reads `xy` as one identifier. This pass splits such a run into
// single-letter factors (`x*y`) while keeping function names, constants,
// differentials and multi-letter variable names whole.
std::string splitImplicitRuns(const std::string& s, const std::vector<std::string>& vars) {
    std::string out;
    out.reserve(s.size() * 2);
    size_t i = 0;
    while (i < s.size()) {
        const char c = s[i];
        if (!(std::isalpha((unsigned char)c) || c == '_')) {
            out += c;
            ++i;
            continue;
        }
        size_t j = i + 1;
        while (j < s.size() && (std::isalnum((unsigned char)s[j]) || s[j] == '_')) ++j;
        const std::string run = s.substr(i, j - i);
        i = j;

        bool protectedWhole = isProtectedName(run);
        for (const auto& v : vars) {
            if (run == v) { protectedWhole = true; break; }
        }
        bool hasDigitOrUnderscore = false;
        for (char ch : run) {
            if (std::isdigit((unsigned char)ch) || ch == '_') {
                hasDigitOrUnderscore = true;
                break;
            }
        }
        // An identifier followed by '(' is a function name, even when it is an
        // unknown one: `foo(x)` must be reported as an unknown function, not
        // split into `f*o*o(x)`.
        const bool callLike = j < s.size() && s[j] == '(';
        if (protectedWhole || callLike || hasDigitOrUnderscore) {
            out += run;
            continue;
        }

        bool allImplicit = !vars.empty();
        for (char ch : run) {
            bool found = false;
            for (const auto& v : vars) {
                if (v.size() == 1 &&
                    std::tolower((unsigned char)v[0]) == std::tolower((unsigned char)ch)) {
                    found = true;
                    break;
                }
            }
            if (!found) { allImplicit = false; break; }
        }
        // Short runs are almost always implicit products. Longer runs are only
        // split when every letter is a declared variable, or when they embed a
        // known name (`xpi` -> `x*pi`); this keeps `alpha` and `theta` intact.
        if (!(run.size() <= 3 || allImplicit || containsProtectedMulti(run))) {
            out += run;
            continue;
        }

        std::vector<std::string> pieces;
        size_t k = 0;
        while (k < run.size()) {
            size_t bestLen = 0;
            for (const char* p : kProtectedNames) {
                const size_t len = std::strlen(p);
                if (len == 0 || len > run.size() - k) continue;
                bool match = true;
                for (size_t m = 0; m < len; ++m) {
                    if (std::tolower((unsigned char)run[k + m]) !=
                        std::tolower((unsigned char)p[m])) {
                        match = false;
                        break;
                    }
                }
                if (match && len > bestLen) bestLen = len;
            }
            for (const auto& v : vars) {
                if (v.size() <= bestLen || v.size() > run.size() - k) continue;
                bool match = true;
                for (size_t m = 0; m < v.size(); ++m) {
                    if (run[k + m] != v[m]) { match = false; break; }
                }
                if (match) bestLen = v.size();
            }
            if (bestLen > 0) {
                pieces.push_back(run.substr(k, bestLen));
                k += bestLen;
            } else {
                pieces.push_back(run.substr(k, 1));
                ++k;
            }
        }
        for (size_t p = 0; p < pieces.size(); ++p) {
            if (p) out += '*';
            out += pieces[p];
        }
    }
    return out;
}

static std::string naturalizeImplicit(std::string s, const std::vector<std::string>& vars) {
    s = separateGluedFunctions(s);
    std::vector<std::string> effective = vars;
    if (effective.empty()) {
        // parseExpr has no declared variable list. These are the letters the
        // app's input fields use; without them `2xy` would remain one symbol.
        effective = {"x", "y", "z", "t", "a", "b", "c", "u", "v", "w", "r", "s", "e", "pi"};
    }
    std::string vc;
    for (auto& v : effective) {
        if (v.size() == 1 && std::isalpha((unsigned char)v[0])) vc += v[0];
    }
    if (!vc.empty()) {
        std::string cls = "[" + vc + "]";
        try {
            // x sin y / x siny  ->  x*sin y / x*siny. The lookahead keeps the
            // leading letter of atan/asin/acos/... from becoming a factor.
            std::regex beforeFn("(^|[^A-Za-z_])(?!(?:" + std::string(kFunctionNames) + "))(" +
                                    cls + ")('*)\\s*(?=(" + kFunctionNames + "))",
                                std::regex::icase);
            s = std::regex_replace(s, beforeFn, "$1$2$3*");
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
                if (s == prev) break;
            }
        } catch (const std::regex_error&) {
            return s;
        }
    }
    return splitImplicitRuns(s, effective);
}

namespace {
// The differential symbols are bookkeeping, not variables: a direction field
// must never treat `dx` or `dy` as the dependent variable.
bool isDifferentialSymbol(const std::string& name) { return name == "dx" || name == "dy"; }
} // namespace

Ex parseWith(const std::string& raw, const std::string& indep, std::string* depOut,
             const std::vector<std::string>& implicitVars = {}) {
    // Bound the input: the regex preprocessor and the recursive parser should
    // not be exposed to arbitrarily large or adversarial strings.
    if (raw.size() > 1000) throw EngineError("too_long", "expression too long");
    std::string s = preprocessDifferentials(normalizeInput(raw), indep);
    s = naturalizeImplicit(s, implicitVars);
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
            for (auto& x : syms) {
                if (x == indep || isDifferentialSymbol(x)) continue;
                if (x.rfind("C", 0) == 0) continue;
                *depOut = x;
                break;
            }
        }
        return e;
    }
    std::string l = s.substr(0, eq), r = s.substr(eq + 1);
    if (trim(l).empty() || trim(r).empty()) throw EngineError("equation_sides", "both sides of the equation are required");
    Parser pl(l, indep), pr(r, indep);
    Ex L = pl.parse(), R = pr.parse();
    if (depOut) {
        std::vector<std::string> syms; collectSymbols(L, syms); collectSymbols(R, syms);
        for (auto& x : syms) {
            if (x == indep || isDifferentialSymbol(x)) continue;
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
