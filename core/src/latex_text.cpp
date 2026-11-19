// Plain-notation to LaTeX conversion.
//
// A large part of the curriculum engine stores hand-written mathematical
// sentences in `Ex` symbol names (for example `Sym("y' = k y")`).  Those
// names have no structure the printer can walk, so `toLatex` used to echo the
// raw ASCII text and the UI ended up showing `alpha`, `sum_{j=0..k}` and
// `y''` as literal letters.  This file turns such strings into real LaTeX:
//
//   "y'' + (0) y' + (1) y = 0"    -> "y'' + \\left(0\\right) y' + ... = 0"
//   "sum_{j=0..k} [ a_j ]"        -> "\\sum_{j=0\\ldots k} \\left[ a_{j} \\right]"
//   "e^{(alpha + i beta)t}"       -> "e^{\\left(\\alpha + i\\,\\beta\\right)t}"
//
// The translator is intentionally forgiving: it only needs to produce valid,
// readable LaTeX, not a canonical parse tree.
#include "expr.hpp"

#include <cctype>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ode {
namespace {

const std::map<std::string, std::string>& greekLatex() {
    static const std::map<std::string, std::string> m = {
        {"alpha", "\\alpha"},     {"beta", "\\beta"},
        {"gamma", "\\gamma"},     {"delta", "\\delta"},
        {"epsilon", "\\epsilon"}, {"varepsilon", "\\varepsilon"},
        {"zeta", "\\zeta"},       {"eta", "\\eta"},
        {"theta", "\\theta"},     {"vartheta", "\\vartheta"},
        {"iota", "\\iota"},       {"kappa", "\\kappa"},
        {"lambda", "\\lambda"},   {"mu", "\\mu"},
        {"nu", "\\nu"},           {"xi", "\\xi"},
        {"omicron", "o"},         {"rho", "\\rho"},
        {"sigma", "\\sigma"},     {"varsigma", "\\varsigma"},
        {"tau", "\\tau"},         {"upsilon", "\\upsilon"},
        {"phi", "\\phi"},         {"varphi", "\\varphi"},
        {"chi", "\\chi"},         {"psi", "\\psi"},
        {"omega", "\\omega"},     {"pi", "\\pi"},
        {"Gamma", "\\Gamma"},     {"Delta", "\\Delta"},
        {"Theta", "\\Theta"},     {"Lambda", "\\Lambda"},
        {"Xi", "\\Xi"},           {"Pi", "\\Pi"},
        {"Sigma", "\\Sigma"},     {"Upsilon", "\\Upsilon"},
        {"Phi", "\\Phi"},         {"Psi", "\\Psi"},
        {"Omega", "\\Omega"},
        {"inf", "\\infty"},       {"infty", "\\infty"},
    };
    return m;
}

const std::map<std::string, std::string>& greekUnicode() {
    static const std::map<std::string, std::string> m = {
        {"alpha", "α"},     {"beta", "β"},      {"gamma", "γ"},
        {"delta", "δ"},     {"epsilon", "ε"},   {"varepsilon", "ε"},
        {"zeta", "ζ"},      {"eta", "η"},       {"theta", "θ"},
        {"vartheta", "ϑ"},  {"iota", "ι"},      {"kappa", "κ"},
        {"lambda", "λ"},    {"mu", "μ"},        {"nu", "ν"},
        {"xi", "ξ"},        {"omicron", "ο"},   {"rho", "ρ"},
        {"sigma", "σ"},     {"varsigma", "ς"},  {"tau", "τ"},
        {"upsilon", "υ"},   {"phi", "φ"},       {"varphi", "ϕ"},
        {"chi", "χ"},       {"psi", "ψ"},       {"omega", "ω"},
        {"pi", "π"},        {"Gamma", "Γ"},     {"Delta", "Δ"},
        {"Theta", "Θ"},     {"Lambda", "Λ"},    {"Xi", "Ξ"},
        {"Pi", "Π"},        {"Sigma", "Σ"},     {"Upsilon", "Υ"},
        {"Phi", "Φ"},       {"Psi", "Ψ"},       {"Omega", "Ω"},
    };
    return m;
}

const std::map<std::string, std::string>& functionLatex() {
    static const std::map<std::string, std::string> m = {
        {"sin", "\\sin"},   {"cos", "\\cos"},   {"tan", "\\tan"},
        {"cot", "\\cot"},   {"sec", "\\sec"},   {"csc", "\\csc"},
        {"sinh", "\\sinh"}, {"cosh", "\\cosh"}, {"tanh", "\\tanh"},
        {"asin", "\\arcsin"}, {"acos", "\\arccos"}, {"atan", "\\arctan"},
        {"ln", "\\ln"},     {"log", "\\log"},   {"exp", "\\exp"},
        {"det", "\\det"},   {"dim", "\\dim"},   {"max", "\\max"},
        {"min", "\\min"},   {"lim", "\\lim"},   {"sup", "\\sup"},
        {"arg", "\\arg"},   {"diag", "\\mathrm{diag}"},
        {"tr", "\\mathrm{tr}"}, {"Re", "\\mathrm{Re}"},
        {"Im", "\\mathrm{Im}"},
    };
    return m;
}

const std::set<std::string>& proseWords() {
    static const std::set<std::string> s = {
        "the", "and", "of", "to", "from", "with", "for", "on", "in", "is",
        "are", "was", "were", "not", "only", "where", "when", "this", "that",
        "then", "so", "but", "or", "at", "by", "through", "into", "over",
        "under", "using", "use", "used", "method", "order", "value", "values",
        "range", "estimate", "evidence", "numerical", "algorithm", "system",
        "unavailable", "elementary", "form", "whole", "line", "grid", "type",
        "unknown", "special", "admits", "condition", "conditions", "boundary",
        "equation", "classification", "recommended", "solver", "observed",
        "convergence", "discrete", "residual", "coexistence", "nullclines",
        "equilibria", "period", "section", "jumps", "consecutive", "nearby",
        "orbit", "periods", "computed", "check", "compare", "write", "solve",
        "find", "build", "record", "list", "step", "complex", "roots",
        "non-integer", "finite-time", "trace", "determinant", "decide",
        "decides", "here", "now", "note", "proof", "passes", "every",
        "satisfies", "matches", "recurrence", "interval", "point", "points",
        "first", "second", "third", "basis", "coefficients", "samples",
        "evaluate", "integrate", "derivative", "equation", "growth", "decay",
        "solution", "solutions", "initial", "final", "error", "model",
    };
    return s;
}

bool knownIdent(const std::string& name) {
    if (greekLatex().count(name) || functionLatex().count(name)) return true;
    static const std::set<std::string> k = {
        "int", "integral", "sum", "prod", "oint", "e", "sqrt", "abs",
    };
    if (k.count(name)) return true;
    if (name.size() >= 2 && name[0] == 'd') {
        const std::string rest = name.substr(1);
        if (rest.size() == 1 || greekLatex().count(rest)) return true;
    }
    return false;
}

std::string escapeText(const std::string& s) {
    std::string out;
    for (char ch : s) {
        switch (ch) {
            case '\\': out += "\\textbackslash{}"; break;
            case '{': out += "\\{"; break;
            case '}': out += "\\}"; break;
            case '_': out += "\\_"; break;
            case '%': out += "\\%"; break;
            case '&': out += "\\&"; break;
            case '#': out += "\\#"; break;
            case '$': out += "\\$"; break;
            default: out += ch;
        }
    }
    return out;
}

struct Tok {
    char kind = 'e';  // n = number, i = identifier, o = operator, e = end
    std::string s;
    bool space = false;
};
std::vector<Tok> lex(const std::string& s) {
    std::vector<Tok> out;
    size_t i = 0;
    bool space = false;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (std::isspace(c)) {
            space = true;
            ++i;
            continue;
        }
        if (std::isdigit(c)) {
            size_t j = i;
            while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) ++j;
            if (j + 1 < s.size() && s[j] == '.' &&
                std::isdigit(static_cast<unsigned char>(s[j + 1]))) {
                ++j;
                while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) ++j;
            }
            out.push_back({'n', s.substr(i, j - i), space});
            space = false;
            i = j;
            continue;
        }
        if (std::isalpha(c)) {
            size_t j = i;
            while (j < s.size() && std::isalpha(static_cast<unsigned char>(s[j]))) ++j;
            out.push_back({'i', s.substr(i, j - i), space});
            space = false;
            i = j;
            continue;
        }
        if (i + 1 < s.size()) {
            if (s.compare(i, 3, "...") == 0) {
                out.push_back({'o', "...", space});
                space = false;
                i += 3;
                continue;
            }
            const std::string two = s.substr(i, 2);
            static const std::set<std::string> multi = {
                "<=", ">=", "->", "=>", "..", "**", "||", ":=", "&&",
            };
            if (multi.count(two)) {
                out.push_back({'o', two, space});
                space = false;
                i += 2;
                continue;
            }
        }
        out.push_back({'o', std::string(1, s[i]), space});
        space = false;
        ++i;
    }
    out.push_back({'e', "", false});
    return out;
}

std::string sanitize(const std::string& in) {
    std::string s = in;
    auto replaceAll = [&](const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
    };
    replaceAll("\xE2\x88\x92", "-");  // U+2212 minus
    replaceAll("\xC3\x97", "*");      // multiplication sign
    replaceAll("\xC2\xB7", "*");      // middle dot
    replaceAll("\xE2\x86\x92", "->"); // right arrow
    replaceAll("\xE2\x89\xA4", "<=");
    replaceAll("\xE2\x89\xA5", ">=");
    replaceAll("\xE2\x88\x9E", "inf");  // U+221E infinity
    // Pretty-printed expressions use real primes, superscripts and Greek
    // letters.  Fold them back into the linear notation the lexer knows.
    struct Rep { const char* from; const char* to; };
    static const Rep reps[] = {
        {"′", "'"},   {"″", "''"},  {"‴", "'''"}, {"⁗", "''''"},
        {"⁰", "^0"},  {"¹", "^1"},  {"²", "^2"},  {"³", "^3"},
        {"⁴", "^4"},  {"⁵", "^5"},  {"⁶", "^6"},  {"⁷", "^7"},
        {"⁸", "^8"},  {"⁹", "^9"},
        {"₀", "_0"},  {"₁", "_1"},  {"₂", "_2"},  {"₃", "_3"},
        {"₄", "_4"},  {"₅", "_5"},  {"₆", "_6"},  {"₇", "_7"},
        {"₈", "_8"},  {"₉", "_9"},
        {"α", "alpha"},   {"β", "beta"},    {"γ", "gamma"},
        {"δ", "delta"},   {"ε", "epsilon"}, {"ζ", "zeta"},
        {"η", "eta"},     {"θ", "theta"},   {"ι", "iota"},
        {"κ", "kappa"},   {"λ", "lambda"},  {"μ", "mu"},
        {"ν", "nu"},      {"ξ", "xi"},      {"ρ", "rho"},
        {"σ", "sigma"},   {"τ", "tau"},     {"υ", "upsilon"},
        {"φ", "phi"},     {"ϕ", "varphi"},  {"χ", "chi"},
        {"ψ", "psi"},     {"ω", "omega"},   {"π", "pi"},
        {"Γ", "Gamma"},   {"Δ", "Delta"},   {"Θ", "Theta"},
        {"Λ", "Lambda"},  {"Ξ", "Xi"},      {"Π", "Pi"},
        {"Σ", "Sigma"},   {"Υ", "Upsilon"}, {"Φ", "Phi"},
        {"Ψ", "Psi"},     {"Ω", "Omega"},
    };
    for (const Rep& rep : reps) replaceAll(rep.from, rep.to);
    // Plus/minus: accept the ASCII spelling and the Unicode sign, both folded
    // into an operator the converter maps to \pm.
    replaceAll("+/-", "@");
    replaceAll("\xC2\xB1", "@");
    // Scientific notation from the engine ("1.02379e-09") would otherwise be
    // read as the product e·(-9).  Fold it into "1.02379*10^(-9)".
    std::string folded;
    for (size_t i = 0; i < s.size();) {
        const bool afterMantissa =
            i > 0 &&
            (std::isdigit(static_cast<unsigned char>(s[i - 1])) ||
             (s[i - 1] == '.' && i > 1 &&
              std::isdigit(static_cast<unsigned char>(s[i - 2]))));
        if (!afterMantissa || !(s[i] == 'e' || s[i] == 'E')) {
            folded += s[i++];
            continue;
        }
        size_t j = i + 1;
        std::string exponent;
        if (j < s.size() && (s[j] == '+' || s[j] == '-')) exponent += s[j++];
        const size_t digitsStart = j;
        while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) ++j;
        if (j == digitsStart) {
            folded += s[i++];
            continue;
        }
        exponent += s.substr(digitsStart, j - digitsStart);
        // Drop the leading zeros the C++ formatter pads ("-09" -> "-9").
        try {
            exponent = std::to_string(std::stol(exponent));
        } catch (...) {
        }
        folded += "*10^(" + exponent + ")";
        i = j;
    }
    return folded;
}

class TextToLatex {
public:
    explicit TextToLatex(const std::string& s) : toks_(lex(sanitize(s))) {}

    std::string run() {
        std::string out = expression(std::set<std::string>{});
        return out.empty() ? "" : out;
    }

private:
    std::vector<Tok> toks_;
    size_t p_ = 0;
    int scriptDepth_ = 0;

    const Tok& peek(size_t k = 0) const {
        static const Tok end;
        return p_ + k < toks_.size() ? toks_[p_ + k] : end;
    }
    bool isOp(const std::string& s) const {
        return peek().kind == 'o' && peek().s == s;
    }
    bool atStop(const std::set<std::string>& stops) const {
        if (peek().kind == 'e') return true;
        if (peek().kind != 'o') return false;
        return stops.count(peek().s) > 0;
    }
    static std::set<std::string> with(const std::set<std::string>& a,
                                      const std::string& b) {
        std::set<std::string> s = a;
        s.insert(b);
        return s;
    }

    bool startsAtom() const {
        const Tok& t = peek();
        if (t.kind == 'n' || t.kind == 'i') return true;
        if (t.kind != 'o') return false;
        return t.s == "(" || t.s == "[" || t.s == "{" || t.s == "||" ||
               t.s == "|";
    }

    static std::string binOp(const std::string& op) {
        if (op == "<=") return "\\le";
        if (op == ">=") return "\\ge";
        if (op == "->") return "\\to";
        if (op == "=>") return "\\Rightarrow";
        if (op == "~") return "\\sim";
        if (op == "@") return "\\pm";
        if (op == ".." || op == "...") return "\\ldots";
        if (op == ":") return ":";
        return op;
    }

    std::string expression(const std::set<std::string>& stops) {
        std::string out;
        bool first = true;
        while (true) {
            const size_t before = p_;
            const std::string term = product(stops);
            out += term;
            if (first && !term.empty()) first = false;
            if (p_ == before) break;
            const Tok& t = peek();
            if (t.kind == 'o' && !stops.count(t.s)) {
                static const std::set<std::string> bin = {
                    "+", "-", "=", "<", ">", "<=", ">=", "~", ",", ";", "->", "=>",
                    "..", "...", ":", ".", "@"};
                if (bin.count(t.s)) {
                    const std::string op = t.s;
                    ++p_;
                    if (op == ",") {
                        out += ",\\;";
                    } else if (op == ";") {
                        out += ";\\;";
                    } else {
                        out += " " + binOp(op) + " ";
                    }
                    continue;
                }
            }
            break;
        }
        return out;
    }

    std::string product(const std::set<std::string>& stops) {
        if (isOp("...") || isOp("..")) {
            ++p_;
            return "\\ldots";
        }
        std::string prefix;
        std::string last;
        bool have = false;

        std::string sign;
        while (peek().kind == 'o' && (peek().s == "+" || peek().s == "-")) {
            sign += peek().s;
            ++p_;
        }

        std::string first = postfix(stops);
        if (first.empty()) return sign;
        last = (sign == "-") ? "-" + first : first;
        have = true;

        static const std::set<std::string> bin = {
            "+", "-", "=", "<", ">", "<=", ">=", "~", ",", ";", "->", "=>",
            "..", "...", ":", ".", "@"};

        while (true) {
            const Tok& t = peek();
            if (t.kind == 'e' || atStop(stops)) break;
            bool consumed = false;
            if (t.kind == 'o') {
                if (t.s == "*") {
                    ++p_;
                    std::string rhs = postfix(stops);
                    auto numeric = [](const std::string& x) {
                        return !x.empty() &&
                               x.find_first_not_of("0123456789.") ==
                                   std::string::npos;
                    };
                    const bool rhsNumber =
                        !rhs.empty() &&
                        std::isdigit(static_cast<unsigned char>(rhs[0]));
                    prefix += numeric(last) && (numeric(rhs) || rhsNumber)
                                  ? last + "\\times "
                                  : last + "\\,";
                    last = rhs;
                    consumed = true;
                } else if (t.s == "**") {
                    ++p_;
                    std::string rhs = postfix(stops);
                    last = "{" + last + "}^{" + rhs + "}";
                    consumed = true;
                } else if (t.s == "/") {
                    ++p_;
                    std::string rhs = postfix(stops);
                    // "stable / node" is prose with a slash, not a fraction:
                    // a fraction bar between two words would read as a
                    // quantity divided by another quantity.
                    if (isTextNode(last) || isTextNode(rhs)) {
                        last += "\\,/\\," + rhs;
                        consumed = true;
                        continue;
                    }
                    std::string num = last;
                    std::string sg;
                    if (!num.empty() && num[0] == '-') {
                        sg = "-";
                        num.erase(0, 1);
                    }
                    last = sg + "\\frac{" + num + "}{" + rhs + "}";
                    consumed = true;
                } else if (bin.count(t.s)) {
                    break;
                }
            }
            if (consumed) continue;
            if (!startsAtom()) break;
            if (t.kind == 'n' && !t.space && last.size() == 1 &&
                std::isalpha(static_cast<unsigned char>(last[0]))) {
                ++p_;
                last += "_{" + t.s + "}";
                continue;
            }
            std::string rhs = postfix(stops);
            if (rhs.empty()) break;
            if (rhs.rfind("\\left(", 0) == 0 || rhs.rfind("\\left[", 0) == 0 ||
                rhs.rfind("\\left\\{", 0) == 0) {
                prefix += isTextNode(last) ? last + "\\;" : last;
            } else if (t.space && isTextNode(last) && isTextNode(rhs)) {
                last = mergeText(last, rhs);
                continue;
            } else {
                prefix += last + "\\,";
            }
            last = rhs;
            have = true;
        }
        (void)have;
        return prefix + last;
    }

    std::string postfix(const std::set<std::string>& stops) {
        std::string a = atom(stops);
        if (a.empty()) return a;
        while (true) {
            if (isOp("'")) {
                ++p_;
                a += "'";
                continue;
            }
            if (isOp("!")) {
                ++p_;
                a += "!";
                continue;
            }
            if (isOp("^") || isOp("_")) {
                const std::string op = peek().s;
                ++p_;
                std::string arg = script(stops, op == "_");
                a += (op == "^" ? "^{" : "_{") + arg + "}";
                continue;
            }
            break;
        }
        const bool big = startsWith(a, "\\int") || startsWith(a, "\\sum") ||
                         startsWith(a, "\\prod") || startsWith(a, "\\lim") ||
                         startsWith(a, "\\oint");
        if (big) {
            std::string body;
            bool firstBody = true;
            while (true) {
                if (startsAtom() && !atStop(stops)) {
                    std::string part = postfix(stops);
                    if (part.empty()) break;
                    body += firstBody ? part : "\\," + part;
                    firstBody = false;
                    continue;
                }
                if (isOp("*")) {
                    ++p_;
                    body += "\\,";
                    continue;
                }
                if (isOp("/")) {
                    ++p_;
                    std::string den = postfix(stops);
                    std::string num = body;
                    std::string sg;
                    if (!num.empty() && num[0] == '-') {
                        sg = "-";
                        num.erase(0, 1);
                    }
                    body = sg + "\\frac{" + num + "}{" + den + "}";
                    continue;
                }
                break;
            }
            if (!body.empty()) a += "\\," + body;
        }
        return a;
    }

    static bool startsWith(const std::string& s, const std::string& pre) {
        return s.rfind(pre, 0) == 0;
    }

    static bool isTextNode(const std::string& s) {
        return s.size() > 6 && s.rfind("\\text{", 0) == 0 && s.back() == '}';
    }

    static std::string mergeText(const std::string& a, const std::string& b) {
        return "\\text{" + a.substr(6, a.size() - 7) + " " +
               b.substr(6, b.size() - 7) + "}";
    }

    std::string script(const std::set<std::string>& stops, bool subscript) {
        if (isOp("{")) {
            ++p_;
            ++scriptDepth_;
            std::string inner = expression(with(stops, "}"));
            --scriptDepth_;
            if (isOp("}")) ++p_;
            return inner;
        }
        if (isOp("(")) {
            ++p_;
            ++scriptDepth_;
            std::string inner = expression(with(stops, ")"));
            --scriptDepth_;
            if (isOp(")")) ++p_;
            return inner;
        }
        std::string sign;
        if (isOp("+") || isOp("-")) {
            sign = peek().s;
            ++p_;
        }
        std::string out;
        bool took = false;
        if (subscript) {
            while ((peek().kind == 'n' || peek().kind == 'i') && !peek().space) {
                out += peek().kind == 'n' ? peek().s
                                          : ident(peek().s, true);
                ++p_;
                took = true;
            }
        } else {
            // A lone sign (`f^- + f^+`) must not swallow the next term.
            if (!sign.empty() && (isOp("+") || isOp("-"))) return sign;
            if (peek().kind == 'n') {
                out = peek().s;
                ++p_;
                took = true;
            } else if (peek().kind == 'i') {
                out = ident(peek().s, true);
                ++p_;
                took = true;
            }
        }
        return sign + out;
    }

    std::string atom(const std::set<std::string>& stops) {
        const Tok& t = peek();
        if (t.kind == 'n') {
            ++p_;
            return t.s;
        }
        if (t.kind == 'i') {
            const std::string name = t.s;
            ++p_;
            if ((name == "sqrt" || name == "abs") && isOp("(")) {
                ++p_;
                std::string inner = expression(with(stops, ")"));
                if (isOp(")")) ++p_;
                return name == "sqrt" ? "\\sqrt{" + inner + "}"
                                      : "\\left|" + inner + "\\right|";
            }
            // Named functions consume a parenthesised argument so that
            // `J/sin(nu pi)` keeps the argument inside the denominator.
            auto fn = functionLatex().find(name);
            if (fn != functionLatex().end() && isOp("(")) {
                ++p_;
                std::string inner = expression(with(stops, ")"));
                if (isOp(")")) ++p_;
                return fn->second + "\\left(" + inner + "\\right)";
            }
            if (name.size() >= 3 && !knownIdent(name)) {
                std::string full = name;
                while (isOp("_") && !peek(1).space &&
                       (peek(1).kind == 'i' || peek(1).kind == 'n')) {
                    full += "_" + peek(1).s;
                    p_ += 2;
                }
                return "\\text{" + escapeText(full) + "}";
            }
            // log2(...) is the base-two logarithm, not "log times 2".
            if (name == "log" && peek().kind == 'n' && !peek().space) {
                const std::string base = peek().s;
                ++p_;
                return "\\log_{" + base + "}";
            }
            // The Laplace transform is written L{...} all over the engine.
            if (name == "L" && isOp("{")) return "\\mathcal{L}";
            return ident(name, scriptDepth_ > 0);
        }
        if (t.kind != 'o') return "";

        if (t.s == "(") {
            ++p_;
            std::string inner = expression(with(stops, ")"));
            if (isOp(")")) ++p_;
            return "\\left(" + inner + "\\right)";
        }
        if (t.s == "[") {
            if (peek(1).kind == 'o' && peek(1).s == "[") return matrix(stops);
            ++p_;
            std::string inner = expression(with(stops, "]"));
            if (isOp("]")) ++p_;
            return "\\left[" + inner + "\\right]";
        }
        if (t.s == "{") {
            ++p_;
            std::string inner = expression(with(stops, "}"));
            if (isOp("}")) ++p_;
            return "\\left\\{" + inner + "\\right\\}";
        }
        if (t.s == "||") {
            ++p_;
            std::string inner = expression(with(stops, "||"));
            if (isOp("||")) ++p_;
            return "\\left\\lVert " + inner + "\\right\\rVert";
        }
        if (t.s == "|") {
            ++p_;
            std::string inner = expression(with(stops, "|"));
            if (isOp("|")) ++p_;
            return "\\left|" + inner + "\\right|";
        }
        return "";
    }

    std::string matrix(const std::set<std::string>& stops) {
        ++p_;  // outer [
        std::string body;
        bool firstRow = true;
        while (isOp("[")) {
            ++p_;
            std::string row;
            bool firstCell = true;
            while (true) {
                std::set<std::string> cellStops = with(with(stops, ","), "]");
                std::string cell = expression(cellStops);
                if (!firstCell) row += " & ";
                row += cell;
                firstCell = false;
                if (isOp(",")) {
                    ++p_;
                    continue;
                }
                if (isOp("]")) {
                    ++p_;
                    break;
                }
                break;
            }
            if (!firstRow) body += " \\\\ ";
            body += row;
            firstRow = false;
            // Both `,` and `;` separate rows; engine matrices print as
            // "[[a, b]; [c, d]]".
            if (isOp(",") || isOp(";")) {
                ++p_;
                continue;
            }
            if (isOp("]")) {
                ++p_;
                break;
            }
            break;
        }
        return "\\begin{bmatrix}" + body + "\\end{bmatrix}";
    }

    static std::string ident(const std::string& name, bool inScript) {
        auto g = greekLatex().find(name);
        if (g != greekLatex().end()) return g->second;
        auto f = functionLatex().find(name);
        if (f != functionLatex().end()) return f->second;
        if (name == "int" || name == "integral") return "\\int";
        if (name == "sum") return "\\sum";
        if (name == "prod") return "\\prod";
        if (name == "oint") return "\\oint";
        if (name == "e") return "e";
        // Differentials: dx, dt, dtau, ...
        if (name.size() >= 2 && name[0] == 'd') {
            const std::string rest = name.substr(1);
            auto gr = greekLatex().find(rest);
            if (gr != greekLatex().end()) return "\\mathrm{d}" + gr->second;
            if (rest.size() == 1) return "\\mathrm{d}" + rest;
        }
        if (inScript) return name;
        if (name.size() == 1) return name;
        // Inside an exponent two letters are a product, not an English word:
        // e^{at} is e raised to a·t, never e to the word "at".
        if (inScript && name.size() == 2) {
            return std::string(1, name[0]) + "\\," + std::string(1, name[1]);
        }
        static const std::set<std::string> shortText = {
            "no", "on", "is", "in", "of", "to", "at", "by", "an", "or",
            "as", "be", "do", "if", "it", "so", "we", "us", "my", "up"};
        if (shortText.count(name)) return "\\text{" + name + "}";
        if (name.size() >= 3) return "\\text{" + name + "}";
        // Two-letter unknown identifiers are products of single letters.
        std::string out;
        for (size_t i = 0; i < name.size(); ++i) {
            if (i) out += "\\,";
            out += std::string(1, name[i]);
        }
        return out;
    }
};

}  // namespace

std::string toLatexText(const std::string& text) {
    TextToLatex conv(text);
    return conv.run();
}

bool isMathText(const std::string& text) {
    if (text.find('\t') != std::string::npos ||
        text.find('\n') != std::string::npos)
        return false;
    bool hasOperator = false;
    std::string word;
    auto checkWord = [&]() -> bool {
        if (word.empty()) return true;
        if (word.size() >= 3) {
            std::string lower = word;
            for (char& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (proseWords().count(lower)) return false;
        }
        word.clear();
        return true;
    };
    for (char ch : text) {
        if (std::isalpha(static_cast<unsigned char>(ch))) {
            word += ch;
        } else {
            if (!checkWord()) return false;
            if (std::string("=<>^_'+-*/|~()").find(ch) != std::string::npos)
                hasOperator = true;
        }
    }
    if (!checkWord()) return false;
    if (!hasOperator) return false;
    return true;
}

static std::string escapeLabel(const std::string& s) {
    std::string out;
    for (char ch : s) {
        switch (ch) {
            case '\\': out += "\\textbackslash{}"; break;
            case '{': out += "\\{"; break;
            case '}': out += "\\}"; break;
            case '_': out += "\\_"; break;
            case '%': out += "\\%"; break;
            case '&': out += "\\&"; break;
            case '#': out += "\\#"; break;
            case '$': out += "\\$"; break;
            default: out += ch;
        }
    }
    return out;
}

namespace {

std::string trimCopy(const std::string& s) {
    size_t begin = 0;
    size_t end = s.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(s[begin])))
        ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])))
        --end;
    return s.substr(begin, end - begin);
}

std::vector<std::string> splitNoteLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (char ch : text) {
        if (ch == '\r') continue;
        if (ch == '\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current += ch;
        }
    }
    lines.push_back(current);
    return lines;
}

std::vector<std::string> splitCells(const std::string& line) {
    std::vector<std::string> cells;
    std::string current;
    for (char ch : line) {
        if (ch == '\t') {
            cells.push_back(trimCopy(current));
            current.clear();
        } else {
            current += ch;
        }
    }
    cells.push_back(trimCopy(current));
    return cells;
}

// A table row such as "i\tx\ty" or "0\t0.25\t1.5": at least two columns and
// every cell is a number or a short symbol, never a sentence.
bool isMathRow(const std::string& line, size_t* columns) {
    if (line.find('\t') == std::string::npos) return false;
    const auto cells = splitCells(line);
    if (cells.size() < 2) return false;
    if (columns) *columns = cells.size();
    for (const auto& cell : cells) {
        if (cell.empty()) continue;
        if (cell.size() > 24) return false;
        bool hasAlnum = false;
        for (char ch : cell) {
            const unsigned char u = static_cast<unsigned char>(ch);
            if (std::isalnum(u)) {
                hasAlnum = true;
                continue;
            }
            if (std::string(" ._^+-*/()[]{}'|,=<>&!:%").find(ch) == std::string::npos)
                return false;
        }
        if (!hasAlnum) return false;
    }
    return true;
}

bool isTableBlock(const std::vector<std::string>& lines, size_t* columns) {
    size_t expected = 0;
    bool any = false;
    for (const auto& raw : lines) {
        if (trimCopy(raw).empty()) continue;
        size_t width = 0;
        if (!isMathRow(raw, &width)) return false;
        if (!any) {
            expected = width;
            any = true;
        } else if (width != expected) {
            return false;
        }
    }
    if (!any) return false;
    if (columns) *columns = expected;
    return true;
}

std::string tableLatex(const std::vector<std::string>& lines, size_t columns) {
    std::string spec(columns, 'c');
    std::string body;
    bool firstRow = true;
    for (const auto& raw : lines) {
        if (trimCopy(raw).empty()) continue;
        const auto cells = splitCells(raw);
        if (!firstRow) body += " \\\\ ";
        for (size_t i = 0; i < columns; ++i) {
            if (i) body += " & ";
            body += i < cells.size() ? toLatexText(cells[i]) : std::string();
        }
        firstRow = false;
    }
    return "\\begin{array}{" + spec + "}" + body + "\\end{array}";
}

// "max |error| = 0.003" or "observed p = 1.999": a label and a value.  The
// label may contain words, which `isMathText` rejects as prose, so accept it
// only when the right side is clearly a number or an expression.  Comma
// separated lists ("f = 0.2, P = 0.1, |error| = 0.1") are checked per part.
bool isLabeledValuePart(const std::string& part) {
    const size_t eq = part.find('=');
    if (eq == std::string::npos || eq == 0 || eq + 1 >= part.size())
        return false;
    const std::string label = trimCopy(part.substr(0, eq));
    const std::string value = trimCopy(part.substr(eq + 1));
    if (label.empty() || value.empty() || value.size() > 48) return false;
    size_t words = 0;
    std::string word;
    for (char ch : label) {
        if (std::isalpha(static_cast<unsigned char>(ch))) {
            word += ch;
            continue;
        }
        if (!word.empty()) {
            ++words;
            word.clear();
        }
        if (std::string(" |()-_'.,").find(ch) == std::string::npos)
            return false;
    }
    if (!word.empty()) ++words;
    if (words == 0 || words > 3) return false;
    bool valueHasDigit = false;
    for (char ch : value) {
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            valueHasDigit = true;
            break;
        }
    }
    if (!valueHasDigit) return false;
    return isMathText(value) || value.find(' ') == std::string::npos;
}

bool isLabeledValue(const std::string& text) {
    if (text.size() > 96) return false;
    bool sawEquals = false;
    std::string part;
    auto check = [&]() -> bool {
        const std::string trimmed = trimCopy(part);
        part.clear();
        if (trimmed.empty()) return true;
        if (isLabeledValuePart(trimmed)) {
            sawEquals = true;
            return true;
        }
        return isMathText(trimmed);
    };
    for (char ch : text) {
        if (ch == ',') {
            if (!check()) return false;
        } else {
            part += ch;
        }
    }
    if (!check()) return false;
    return sawEquals;
}

// A named mathematical symbol (function or Greek letter) next to an operator
// marks a line that is prose with an embedded formula rather than plain text.
bool hasNamedMathSymbol(const std::string& text) {
    bool operatorFound = false;
    for (char ch : text) {
        if (std::string("=<>^_+-*/|").find(ch) != std::string::npos) {
            operatorFound = true;
            break;
        }
    }
    if (!operatorFound) return false;
    auto known = [](std::string word) {
        for (char& ch : word)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return greekLatex().count(word) > 0 ||
               functionLatex().count(word) > 0 || word == "int" ||
               word == "integral" || word == "sum" || word == "prod" ||
               word == "oint";
    };
    std::string word;
    for (char ch : text) {
        if (std::isalpha(static_cast<unsigned char>(ch))) {
            word += ch;
        } else {
            if (!word.empty() && known(word)) return true;
            word.clear();
        }
    }
    return !word.empty() && known(word);
}

bool isMathLineSingle(const std::string& text) {
    if (isMathText(text)) return true;
    // Prose that carries a real formula: "complex roots: use cos(beta ln|x|)
    // and sin(beta ln|x|)".  A named function or Greek symbol plus an operator
    // is enough; the words become upright text in the output.
    if (hasNamedMathSymbol(text)) return true;
    const size_t colon = text.find(':');
    if (colon != std::string::npos && colon <= 40) {
        const std::string label = text.substr(0, colon);
        bool labelOk = true;
        for (char ch : label) {
            if (!std::isalpha(static_cast<unsigned char>(ch)) && ch != ' ' &&
                ch != '(' && ch != ')' && ch != '-') {
                labelOk = false;
                break;
            }
        }
        if (labelOk && isMathText(text.substr(colon + 1))) return true;
    }
    return isLabeledValue(text);
}

std::string toLatexLineSingle(const std::string& text) {
    if (isMathText(text)) return toLatexText(text);
    if (hasNamedMathSymbol(text)) return toLatexText(text);
    const size_t colon = text.find(':');
    if (colon != std::string::npos && colon <= 40) {
        const std::string label = text.substr(0, colon);
        const std::string rest = text.substr(colon + 1);
        bool labelOk = true;
        for (char ch : label) {
            if (!std::isalpha(static_cast<unsigned char>(ch)) && ch != ' ' &&
                ch != '(' && ch != ')' && ch != '-') {
                labelOk = false;
                break;
            }
        }
        if (labelOk && isMathText(rest)) {
            return "\\text{" + escapeLabel(trimCopy(label)) + ":}\\;" +
                   toLatexText(rest);
        }
    }
    if (isLabeledValue(text)) return toLatexText(text);
    return "";
}

}  // namespace

bool isMathLine(const std::string& text) {
    if (text.find('\n') == std::string::npos &&
        text.find('\t') == std::string::npos)
        return isMathLineSingle(text);
    const auto lines = splitNoteLines(text);
    size_t columns = 0;
    if (isTableBlock(lines, &columns)) return true;
    bool any = false;
    for (const auto& raw : lines) {
        const std::string line = trimCopy(raw);
        if (line.empty()) continue;
        if (!isMathLineSingle(line)) return false;
        any = true;
    }
    return any;
}

std::string toLatexLine(const std::string& text) {
    if (text.find('\n') == std::string::npos &&
        text.find('\t') == std::string::npos)
        return toLatexLineSingle(text);
    const auto lines = splitNoteLines(text);
    size_t columns = 0;
    if (isTableBlock(lines, &columns)) return tableLatex(lines, columns);
    std::string body;
    bool first = true;
    for (const auto& raw : lines) {
        const std::string line = trimCopy(raw);
        if (line.empty()) continue;
        const std::string latex = toLatexLineSingle(line);
        if (latex.empty()) return "";
        if (!first) body += " \\\\ ";
        body += latex;
        first = false;
    }
    if (first) return "";
    return "\\begin{array}{l}" + body + "\\end{array}";
}

std::string greekSymbolLatex(const std::string& name) {
    auto it = greekLatex().find(name);
    return it == greekLatex().end() ? std::string() : it->second;
}

std::string greekSymbolUnicode(const std::string& name) {
    auto it = greekUnicode().find(name);
    return it == greekUnicode().end() ? std::string() : it->second;
}

std::string greekTextToUnicode(const std::string& text) {
    std::string out;
    std::string word;
    auto flush = [&]() {
        if (word.empty()) return;
        auto it = greekUnicode().find(word);
        out += it == greekUnicode().end() ? word : it->second;
        word.clear();
    };
    for (char ch : text) {
        if (std::isalpha(static_cast<unsigned char>(ch))) {
            word += ch;
        } else {
            flush();
            out += ch;
        }
    }
    flush();
    return out;
}

}  // namespace ode
