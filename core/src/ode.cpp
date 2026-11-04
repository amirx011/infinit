// Infinit Lab — ODE classification and exact solvers
// SPDX-License-Identifier: Apache-2.0
#include "expr.hpp"
#include "poly.hpp"
#include "solver.hpp"
#include "numeric.hpp"
#include "localize.hpp"
#include <algorithm>
#include <chrono>
#include <complex>
#include <set>
#include <functional>
#include <utility>

namespace ode {

// ---------------------------------------------------------------- helpers
static bool isZero(const Ex& e) {
    try { return simplify(e)->isZero(); } catch (...) { return false; }
}

static Ex Cst(int i = 1) {
    if (i <= 1) return Sym("C");
    return Sym("C" + std::to_string(i));
}

static size_t exprSize(const Ex& a) {
    if (!a) return 0;
    size_t n = 1;
    for (auto& x : a->args) n += exprSize(x);
    return n;
}

static bool dependsOnY(const Ex& e, const std::string& y) { return dependsOn(e, y); }

static bool isLinearIn(const Ex& e, const std::string& y) {
    // e is polynomial of degree <=1 in y
    std::vector<Ex> c;
    if (!polyCoeffs(expand(e), y, c)) return false;
    return c.size() <= 2;
}

struct ExLess {
    bool operator()(const Ex& a, const Ex& b) const { return compareExpr(a, b) < 0; }
};

struct TermPow {
    Ex exp;   // exponent of y
    Ex coeff; // coefficient
};

// decompose expanded expression into sum of coeff * y^exp
static bool powerTerms(const Ex& e0, const std::string& y, std::vector<TermPow>& out) {
    Ex e = expand(e0);
    std::vector<Ex> terms = e->isAdd() ? e->args : std::vector<Ex>{e};
    std::map<Ex, Ex, ExLess> m;
    for (auto& t : terms) {
        std::vector<Ex> facs = t->isMul() ? t->args : std::vector<Ex>{t};
        Ex coeff = Num(1), ex = Num(0);
        for (auto& f : facs) {
            if (f->isSym(y)) { ex = Add(ex, Num(1)); continue; }
            if (f->isPow() && f->args[0]->isSym(y) && f->args[1]->isNum()) {
                ex = Add(ex, f->args[1]);
                continue;
            }
            if (dependsOn(f, y)) return false;
            coeff = Mul(coeff, f);
        }
        auto it = m.find(ex);
        if (it == m.end()) m.emplace(ex, coeff);
        else it->second = Add(it->second, coeff);
    }
    out.clear();
    for (auto& kv : m) {
        if (kv.second->isZero()) continue;
        out.push_back({kv.first, kv.second});
    }
    return true;
}

static Ex coeffOf(const std::vector<TermPow>& ts, long long exp) {
    for (auto& t : ts) if (t.exp->isNum() && t.exp->num == Rational(exp)) return t.coeff;
    return Num(0);
}

static std::string normFa(std::string s) {
    std::string o;
    o.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        if (c == 0xE2 && i + 2 < s.size() && (unsigned char)s[i + 1] == 0x80 &&
            ((unsigned char)s[i + 2] == 0x8C || (unsigned char)s[i + 2] == 0x8D)) {
            i += 3; continue; // ZWNJ / ZWJ
        }
        if (c == 0xD9 && i + 1 < s.size()) {
            unsigned char d = (unsigned char)s[i + 1];
            if (d == 0x8A) { o += "\u06cc"; i += 2; continue; } // Arabic yeh -> Persian yeh
            if (d == 0x83) { o += "\u06a9"; i += 2; continue; } // Arabic kaf -> Persian kaf
            if (d >= 0x8B && d <= 0x9F) { i += 2; continue; }   // combining marks
        }
        o += s[i++];
    }
    return o;
}

static std::string replaceAllStr(std::string s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

static std::string enTitle(const std::string& fa) {
    static const std::map<std::string, std::string> m = {
        {"شناسایی نوع", "Type identification"},
        {"جواب عمومی", "General solution"},
        {"جواب منفرد (پوش)", "Singular solution (envelope)"},
        {"جواب منفرد (پوش، پارامتری)", "Singular solution (envelope, parametric)"},
        {"معادله خطی بر حسب x(p)", "Linear equation in x(p)"},
        {"حل معادله خطی", "Solve the linear equation"},
        {"حذف p از y = x·g(p)+f(p)", "Eliminate p from y = x·g(p)+f(p)"},
        {"ضرایب", "Coefficients"},
        {"یک جواب خصوصی", "A particular solution"},
        {"جایگذاری y = y₁ + 1/z", "Substitute y = y₁ + 1/z"},
        {"حل خطی برای z", "Solve the linear equation for z"},
        {"تبدیل ریکاتی", "Riccati transformation"},
        {"معادله خطی مرتبه دوم", "Second-order linear equation"},
        {"حل معادله مرتبه دوم و بازگشت به y", "Solve the second-order equation and return to y"},
        {"تغییر متغیر", "Change of variables"},
        {"حل خطی برای v", "Solve the linear equation for v"},
        {"بازگشت به y", "Return to y"},
        {"عامل انتگرال‌ساز", "Integrating factor"},
        {"انتگرال‌گیری", "Integrate"},
        {"جداسازی متغیرها", "Separate the variables"},
        {"تجزیه", "Factorisation"},
        {"جداسازی", "Separation"},
        {"انتگرال دو طرف", "Integrate both sides"},
        {"جواب ضمنی", "Implicit solution"},
        {"آزمون کامل بودن", "Exactness test"},
        {"تابع پتانسیل", "Potential function"},
        {"معادله کامل جدید", "New exact equation"},
        {"معادله مشخصه", "Characteristic equation"},
        {"ریشه‌های مشخصه", "Characteristic roots"},
        {"انتگرال‌گیری دوباره", "Integrate again"},
        {"شکل دیفرانسیلی", "Differential form"},
        {"حل برای y′", "Solve for y′"},
        {"انتگرال‌گیری مستقیم", "Direct integration"},
        {"ساخت ماتریس رونسکی", "Build the Wronskian matrix"},
        {"توابع پایه", "Basis functions"},
        {"دترمینان رونسکی", "Wronskian determinant"},
        {"نتیجه", "Conclusion"},
        {"خانواده منحنی‌ها", "Family of curves"},
        {"مشتق نسبت به پارامتر", "Derivative with respect to the parameter"},
        {"حذف c", "Eliminate c"},
        {"بررسی پوش", "Envelope check"},
        {"شیب خانواده", "Family slope"},
        {"معادله مسیرهای متعامد", "Orthogonal-trajectory equation"},
        {"حل مسیرهای متعامد", "Solve the orthogonal trajectories"},
        {"معادله مسیرهای قائم", "Orthogonal-trajectory equation"},
        {"حل مسیرهای قائم", "Solve the orthogonal trajectories"},
        {"حداکثر ۶ تابع پشتیبانی می‌شود", "At most six functions are supported"}
    };
    static const std::map<std::string, std::string> norm = [] {
        std::map<std::string, std::string> out;
        for (auto& kv : m) out[normFa(kv.first)] = kv.second;
        return out;
    }();
    auto it = norm.find(normFa(fa));
    return it == norm.end() ? "" : it->second;
}

static std::string enNote(const std::string& faRaw) {
    std::string fa = normFa(faRaw);
    static const std::map<std::string, std::string> m = {
        {"معادله به شکل کلرو است: y = x·y′ + f(y′).", "Clairaut form: y = x·y′ + f(y′)."},
        {"معادله به شکل لاگرانژ است: y = x·g(y′) + f(y′).", "Lagrange form: y = x·g(y′) + f(y′)."},
        {"با قرار دادن y′ = C در معادله، خانواده خطوط y = Cx + f(C) به دست می‌آید.",
         "Setting y′ = C gives the line family y = Cx + f(C)."},
        {"با حذف پارامتر p بین x = -f′(p) و y = f(p) - p f′(p) پوش خانواده به دست می‌آید.",
         "Eliminating p between x = -f′(p) and y = f(p) - p f′(p) gives the envelope."},
        {"جواب منفرد به صورت پارامتری x(p) = -f′(p)، y(p) = f(p) - p f′(p) است.",
         "The singular solution is parametric: x(p) = -f′(p), y(p) = f(p) - p f′(p)."},
        {"با مشتق‌گیری نسبت به x و قرار دادن p = y′: dx/dp − g′(p)/(p−g(p))·x = f′(p)/(p−g(p)).",
         "Differentiate with respect to x and set p = y′: dx/dp − g′(p)/(p−g(p))·x = f′(p)/(p−g(p))."},
        {"x بر حسب پارامتر p به دست آمد.", "x is obtained as a function of the parameter p."},
        {"جواب عمومی به شکل پارامتری است.", "The general solution is parametric."},
        {"معادله ریکاتی است: y′ = P(x)y² + Q(x)y + R(x).", "Riccati equation: y′ = P(x)y² + Q(x)y + R(x)."},
        {"با جایگذاری در معادله بررسی و تأیید شد.", "Verified by substitution into the equation."},
        {"معادله به خطی z′ + (2P y₁ + Q)z = −P تبدیل می‌شود.",
         "The equation becomes linear: z′ + (2P y₁ + Q)z = −P."},
        {"با y = −u′/(P u)، معادله به u″ − (Q + P′/P)u′ + P R u = 0 تبدیل می‌شود.",
         "With y = −u′/(P u), the equation becomes u″ − (Q + P′/P)u′ + P R u = 0."},
        {"جواب u به دست آمد و با y = −u′/(P u) به جواب ریکاتی برمی‌گردیم.",
         "The solution u is obtained, then y = −u′/(P u) returns the Riccati solution."},
        {"با v = y^(1−n)، معادله به v′ + (1−n)P v = (1−n)Q تبدیل می‌شود.",
         "With v = y^(1−n), the equation becomes v′ + (1−n)P v = (1−n)Q."},
        {"معادله خطی مرتبه اول است: y′ + P(x)y = Q(x).",
         "First-order linear equation: y′ + P(x)y = Q(x)."},
        {"μ(x) = exp(∫P dx).", "Integrating factor: μ(x) = exp(∫P dx)."},
        {"y = μ⁻¹(∫μQ dx + C).", "Then y = μ⁻¹(∫μQ dx + C)."},
        {"معادله همگن است: y′ = F(y/x).", "Homogeneous equation: y′ = F(y/x)."},
        {"با y = vx داریم y′ = v + x·v′ پس x·v′ = F(v) − v.",
         "With y = vx we get y′ = v + x·v′, hence x·v′ = F(v) − v."},
        {"در این حالت F(v) = v و خانواده خطوط y = Cx به دست می‌آید.",
         "Here F(v) = v, giving the line family y = Cx."},
        {"dv/(F(v) − v) = dx/x.", "dv/(F(v) − v) = dx/x."},
        {"با v = y/x جواب ضمنی به دست می‌آید.", "Substituting v = y/x gives the implicit solution."},
        {"معادله جداشدنی است: y′ = f(x)·g(y).", "Separable equation: y′ = f(x)·g(y)."},
        {"dy/g(y) = f(x) dx.", "dy/g(y) = f(x) dx."},
        {"معادله به شکل y′ = f(ax + by) است.", "Equation of the form y′ = f(ax + by)."},
        {"با u = ax + y، معادله به u′ = a + f(u) (جداشدنی) تبدیل می‌شود.",
         "With u = ax + y, the equation becomes u′ = a + f(u) (separable)."},
        {"∂M/∂y = ∂N/∂x، پس معادله کامل (دقیق) است.",
         "∂M/∂y = ∂N/∂x, so the equation is exact."},
        {"u(x,y) = ∫M dx + φ(y) و φ′(y) = N − ∂/∂y∫M dx.",
         "u(x,y) = ∫M dx + φ(y), with φ′(y) = N − ∂/∂y∫M dx."},
        {"با ضرب در μ، معادله کامل می‌شود.", "Multiplying by μ makes the equation exact."},
        {"u(x,y) = C جواب عمومی است.", "The general solution is u(x,y) = C."},
        {"با فرض y = e^(rx)، معادله مشخصه به دست می‌آید.",
         "Assuming y = e^(rx) gives the characteristic equation."},
        {"ریشه‌های حقیقی/مختلط با تکراپذیری در جواب لحاظ شده‌اند.",
         "Real/complex roots and their multiplicities are included in the solution."},
        {"ترکیب خطی توابع پایه با ثابت‌های دلخواه.", "A linear combination of the basis functions with arbitrary constants."},
        {"معادله از نوع y″ = f(x) است (متغیر y و y′ غایب‌اند).",
         "The equation is of the form y″ = f(x) (y and y′ are absent)."},
        {"M dx + N dy = 0 با M = X و N = Y", "M dx + N dy = 0"},
        {"معادله را به شکل صریح y′ = f(x,y) نوشتیم.", "Rewrote the equation explicitly as y′ = f(x,y)."},
        {"چون y′ تنها تابعی از x است، y = ∫f(x)dx + C.",
         "Since y′ depends only on x, y = ∫f(x)dx + C."},
        {"برای حل ریکاتی، یک جواب خصوصی پیدا نشد. تبدیل به معادله خطی مرتبه دوم داده شد.",
         "No particular Riccati solution was found; the equation was transformed to second order."},
        {"انتگرال‌های جواب مقدماتی نیستند؛ جواب به صورت کوادراتور دقیق ارائه شده است.",
         "The solution contains non-elementary integrals; it is presented as an exact quadrature."},
        {"جواب شامل انتگرال غیرمقدماتی است و به صورت کوادراتور دقیق ارائه شده است.",
         "The solution contains a non-elementary integral and is presented as an exact quadrature."},
        {"این خانواده پوش ندارد: شرط ∂F/∂c = 0 فقط یک عضو خانواده (یا یک منحنی تبهگن) می‌دهد.",
         "This family has no envelope: ∂F/∂c = 0 gives only a family member (or a degenerate curve)."},
        {"مقدار c = 0 مستقل از x و y است؛ منحنی حاصل عضو خانواده است، نه پوش.",
         "c = 0 is independent of x and y; the curve is a family member, not an envelope."},
        {"این خانواده پوش ندارد.", "This family has no envelope."},
        {"شیب خانواده در این حالت تعریف‌نشده است؛ مسیرهای متعامد محاسبه نشد.",
         "The family slope is undefined here; orthogonal trajectories were not computed."},
        {"حذف c از شیب خانواده ممکن نشد؛ مسیرهای متعامد تنها به صورت معادله دیفرانسیل ارائه می‌شود.",
         "c could not be eliminated from the slope; orthogonal trajectories are given only as an ODE."},
        {"مسیرهای متعامد با روش‌های موجود حل نشد.",
         "The orthogonal trajectories could not be solved with the available methods."},
        {"خانواده مسیرهای متعامد.", "Family of orthogonal trajectories."},
        {"خانواده ضمنی مسیرهای متعامد.", "Implicit family of orthogonal trajectories."},
        {"حداقل یک تابع لازم است.", "At least one function is required."},
        {"حداکثر ۶ تابع پشتیبانی می‌شود.", "At most six functions are supported."},
        {"تابع معتبری وارد نشد.", "No valid function was entered."},
        {"استقلال خطی روی بازه‌ای بررسی می‌شود که توابع و W در آن تعریف‌شده و پیوسته باشند.",
         "Linear independence is checked on an interval where the functions and W are defined and continuous."},
        {"W ≢ 0 است؛ بنابراین توابع روی هر بازه‌ای که W در آن صفر نشود، مستقل خطی‌اند.",
         "W ≢ 0, so the functions are linearly independent on any interval where W is nonzero."},
        {"W ≡ 0 است؛ بنابراین توابع روی بازه موردنظر وابسته خطی‌اند (یا حداقل یک نقطه تبهگن دارند).",
         "W ≡ 0, so the functions are linearly dependent on the interval (or have a degenerate point)."},
        {"دترمینان صفر شد.", "The determinant is zero."},
        {"دترمینان صفر نیست.", "The determinant is not zero."},
        {"بخش ناهمگن در این نسخه تنها برای معادلات مرتبه اول پوشش داده شده است.",
         "Non-homogeneous terms are currently supported only for first-order equations."},
        {"این شکل دیفرانسیلی با روش‌های کامل/عامل انتگرال‌ساز حل نشد.",
         "This differential form was not solved by the exact/integrating-factor methods."},
        {"این معادله مرتبه بالا در قالب‌های پشتیبانی‌شده نیست.",
         "This higher-order equation is not in a supported form."},
        {"معادله نسبت به y′ خطی نیست و در قالب‌های پوشش‌داده‌شده قرار نمی‌گیرد.",
         "The equation is not linear in y′ and is outside the supported forms."},
        {"معادله با روش‌های استاندارد این نسخه (جداشدنی، همگن، کامل، عامل انتگرال‌ساز، خطی، برنولی، کلرو، لاگرانژ، ریکاتی) حل نشد.",
         "The equation was not solved by the standard methods of this version (separable, homogeneous, exact, integrating factor, linear, Bernoulli, Clairaut, Lagrange, Riccati)."},
        {"این معادله خطی مرتبه بالا ضرایب ثابت ندارد؛ روش‌های ضرایب متغیر در این نسخه پوشش داده نشده است.",
         "This higher-order linear equation does not have constant coefficients; variable-coefficient methods are not covered in this version."},
        {"ریشه‌های مشخصه به‌صورت بسته یافت نشدند.",
         "The characteristic roots could not be found in closed form."},
        {"خطا در تحلیل معادله: ", "Equation analysis error: "},
        {"ضرایب عددی بیش از حد بزرگ شدند: ", "Numeric coefficients grew too large: "},

        {"سطر i شامل مشتق مرتبه i توابع است (i = 0,…, n−1).",
         "Row i contains the i-th derivatives of the functions (i = 0,…, n−1)."},
        {"F(x,y,c) = 0.", "F(x,y,c) = 0."},
        {"برای حذف c، شرط مماس بودن: ∂F/∂c = 0.",
         "To eliminate c, use the tangency condition ∂F/∂c = 0."},
        {"هر نقطه از پوش باید در ازای پارامتری مانند c هم F=0 و هم F_c=0 را برآورده کند؛ این شرط با ساخت بالا تضمین می‌شود.",
         "Every envelope point must satisfy F = 0 and F_c = 0 for some parameter c; the construction above enforces this."},
        {"پوش در نقاطی که F و F_c هموارند و حذف c معتبر است، تعریف می‌شود.",
         "The envelope is defined where F and F_c are smooth and eliminating c is valid."}
    };
    static const std::map<std::string, std::string> norm = [] {
        std::map<std::string, std::string> out;
        for (auto& kv : m) out[normFa(kv.first)] = kv.second;
        return out;
    }();
    auto it = norm.find(fa);
    if (it != norm.end()) return it->second;

    // Dynamic notes: translate the fixed part and keep the mathematical text.
    if (fa.rfind("P = ", 0) == 0)
        return "Coefficients: " + replaceAllStr(fa, " ، ", ", ");
    if (fa.rfind("f(x) = ", 0) == 0)
        return "Factorisation: " + replaceAllStr(fa, " ، ", ", ");
    if (fa.rfind("u″ + (", 0) == 0)
        return "The linear equation is " + fa;
    if (fa.rfind("جواب ضمنی: ", 0) == 0)
        return "Implicit solution: " + fa.substr(std::string("جواب ضمنی: ").size());
    if (fa.rfind("y′ = ", 0) == 0 && fa.find("معادل برنولی") != std::string::npos)
        return replaceAllStr(fa, " (معادل برنولی).", " (Bernoulli form).");
    if (fa.rfind("معادله برنولی است:", 0) == 0) {
        std::string rest = fa.substr(std::string("معادله برنولی است: ").size());
        return "Bernoulli equation: " + replaceAllStr(rest, " با n = ", " with n = ");
    }
    if (fa.rfind("M dx + N dy = 0", 0) == 0) {
        std::string t = replaceAllStr(fa, " با M = ", " with M = ");
        return replaceAllStr(t, " و N = ", " and N = ");
    }
    if (fa.rfind("معادله خطی همگن با ضرایب ثابت از مرتبه ", 0) == 0) {
        std::string rest = fa.substr(std::string("معادله خطی همگن با ضرایب ثابت از مرتبه ").size());
        const std::string tail = " است.";
        if (rest.size() > tail.size() && rest.compare(rest.size() - tail.size(), tail.size(), tail) == 0)
            rest = rest.substr(0, rest.size() - tail.size());
        return "Homogeneous linear equation with constant coefficients of order " + rest + ".";
    }
    if (fa.rfind("مقدار c = ", 0) == 0) {
        if (fa.find("مستقل") != std::string::npos)
            return fa.substr(0, fa.find("مستقل")) + "is independent of x and y; the curve is a family member, not an envelope.";
        if (fa.find("تبهگن") != std::string::npos)
            return fa.substr(0, fa.find("منحنی")) + "gives a degenerate curve, not an envelope.";
    }
    if (fa.rfind("با جایگذاری c = ", 0) == 0)
        return replaceAllStr(fa, "با جایگذاری ", "Substituting ");
    if (fa.rfind("این خانواده پوش ندارد", 0) == 0)
        return "This family has no envelope.";
    return "";
}

struct Builder {
    Result& r;
    OdeOptions opt;
    bool en = true;
    std::string x, y;
    Ex X, Y, P;

    Builder(Result& res, const OdeOptions& o) : r(res), opt(o), x(o.x), y(o.y) {
        en = o.lang != "fa";
        X = Sym(x); Y = Sym(y);
        P = Deriv(Y, x, 1);
    }

    void step(const std::string& title, const Ex& math, const std::string& note = "",
              const std::string& titleEn = "", const std::string& noteEn = "") {
        if (!opt.steps) return;
        std::string t = title, n = note;
        if (en) {
            if (!titleEn.empty()) t = titleEn;
            else { std::string x = enTitle(title); if (!x.empty()) t = x; }
            if (!noteEn.empty()) n = noteEn;
            else { std::string x = enNote(note); if (!x.empty()) n = x; }
        }
        r.steps.push_back({t, math, n});
    }
    void text(const std::string& title, const std::string& note,
              const std::string& titleEn = "", const std::string& noteEn = "") {
        step(title, nullptr, note, titleEn, noteEn);
    }
    void warn(const std::string& w, const std::string& wEn = "") {
        std::string text = w;
        if (en) {
            if (!wEn.empty()) text = wEn;
            else { std::string x = enNote(w); if (!x.empty()) text = x; }
        }
        r.warnings.push_back(text);
    }
    void assume(const std::string& a, const std::string& aEn = "") {
        std::string text = a;
        if (en) {
            if (!aEn.empty()) text = aEn;
            else { std::string x = enNote(a); if (!x.empty()) text = x; }
        }
        r.assumptions.push_back(text);
    }
    void check(const std::string& name, bool ok, const std::string& detail,
               const std::string& nameEn = "", const std::string& detailEn = "") {
        std::string n = name, d = detail;
        if (en) {
            if (!nameEn.empty()) n = nameEn;
            else { std::string x = enNote(name); if (!x.empty()) n = x; }
            if (!detailEn.empty()) d = detailEn;
            else { std::string x = enNote(detail); if (!x.empty()) d = x; }
        }
        r.checks.push_back({n, ok, d});
    }
    void fail(const std::string& message, const std::string& messageEn = "") {
        r.ok = false;
        if (!en) { r.error = message; return; }
        if (!messageEn.empty()) { r.error = messageEn; return; }
        std::string x = enNote(message);
        r.error = x.empty() ? localizeError(message, true) : x;
    }
};

static Ex expOf(const Ex& e) { return Fn("exp", {e}); }
static Ex lnAbs(const Ex& e) { return Fn("ln", {Fn("abs", {e})}); }

// solve y' + P y = Q exactly
static Ex solveLinearODE(const std::string& x, const Ex& Pcoef, const Ex& Q, Ex* muOut = nullptr,
                         Ex* intOut = nullptr) {
    Ex X = Sym(x);
    bool closed1 = false, closed2 = false;
    Ex intP = integrate(Pcoef, x, &closed1);
    // standard convention: x > 0 for the integrating factor
    {
        std::function<Ex(const Ex&)> pos = [&](const Ex& e) -> Ex {
            if (!e) return e;
            // μ is only defined up to sign, so drop every absolute value.
            if (e->isFunc("abs") && e->args.size() == 1) return e->args[0];
            if (e->args.empty()) return e;
            std::vector<Ex> v;
            for (auto& a : e->args) v.push_back(pos(a));
            switch (e->kind) {
                case Kind::Add: return Add(v);
                case Kind::Mul: return Mul(v);
                case Kind::Pow: return Pow(v[0], v[1]);
                case Kind::Func: return Fn(e->name, v);
                default: return e;
            }
        };
        intP = pos(intP);
    }
    Ex mu = simplify(expOf(intP));
    Ex rhs = simplify(Mul(mu, Q));
    Ex intRhs = integrate(rhs, x, &closed2);
    Ex sol = simplify(Mul(Add(Cst(1), intRhs), Pow(mu, Num(-1))));
    // Distribute the integrating factor when it collapses the answer
    // (e.g. e^{x²/2+x}(C + 2e^{-x²/2-x}) -> C·e^{x²/2+x} + 2).
    if (exprSize(sol) < 600) {
        Ex ex = simplify(expand(sol));
        if (exprSize(ex) < exprSize(sol)) sol = ex;
    }
    if (muOut) *muOut = mu;
    if (intOut) *intOut = intRhs;
    (void)closed1; (void)closed2; (void)X;
    return sol;
}

// try to turn an implicit equation expr = C into y = ...
static Ex numeratorOf(const Ex& e) {
    if (!e) return e;
    if (e->isPow() && e->args[1]->isNum() && e->args[1]->num.isInt() && e->args[1]->num.n < 0) return Num(1);
    if (e->isMul()) {
        std::vector<Ex> v;
        for (auto& f : e->args) {
            Ex n = numeratorOf(f);
            if (!n->isOne()) v.push_back(n);
        }
        return v.empty() ? Num(1) : Mul(v);
    }
    return e;
}

static Ex tryExplicit(const Ex& eq, const std::string& x, const std::string& y) {
    Ex s = simplify(eq);
    if (!dependsOn(s, y)) return nullptr;
    Ex lin = solveLinear(s, y);
    if (lin) return simplify(lin);
    // Clear denominators and retry: e.g. −x − 1/y = C becomes linear in y.
    try {
        Ex frac = together(s);
        Ex num = numeratorOf(frac);
        if (num && !equals(num, s)) {
            Ex l2 = solveLinear(num, y);
            if (l2) return simplify(l2);
        }
    } catch (...) {}
    // logarithm form: sum of terms; isolate k*ln|y| (+ const in y) = rest(x)
    Ex e = expand(s);
    std::vector<Ex> terms = e->isAdd() ? e->args : std::vector<Ex>{e};
    Ex ypart = Num(0), rest = Num(0);
    for (auto& t : terms) {
        if (dependsOn(t, y)) ypart = Add(ypart, t);
        else rest = Add(rest, t);
    }
    if (ypart->isZero()) return nullptr;
    // pattern: k*ln(abs(y))
    Rational k(1);
    bool ok = false;
    std::vector<Ex> yf = ypart->isMul() ? ypart->args : std::vector<Ex>{ypart};
    for (auto& f : yf) {
        if (f->isNum()) { k = k * f->num; continue; }
        if (f->isFunc("ln") && f->args[0]->isFunc("abs") && f->args[0]->args[0]->isSym(y)) { ok = true; continue; }
        if (f->isFunc("ln") && f->args[0]->isSym(y)) { ok = true; continue; }
        ok = false;
        break;
    }
    if (ok && !k.isZero()) {
        // k ln|y| + rest = 0 -> |y| = exp(-rest/k) -> y = C exp(...)
        Ex arg = simplify(Mul(Num(-1), rest, Pow(Num(k), Num(-1))));
        return simplify(Mul(Cst(1), expOf(arg)));
    }
    // single invertible function of y: k·F(y) + rest = 0
    try {
        Rational kk(1);
        std::string fname;
        Ex farg;
        std::vector<Ex> mf = ypart->isMul() ? ypart->args : std::vector<Ex>{ypart};
        bool good = true;
        for (auto& f : mf) {
            if (f->isNum()) { kk = kk * f->num; continue; }
            if (f->isFunc() && f->args.size() == 1 && dependsOn(f->args[0], y) && fname.empty()) {
                fname = f->name;
                farg = f->args[0];
                continue;
            }
            good = false;
            break;
        }
        static const std::map<std::string, std::string> inv = {
            {"atan", "tan"}, {"tan", "atan"}, {"asin", "sin"}, {"sin", "asin"},
            {"acos", "cos"}, {"cos", "acos"}, {"asinh", "sinh"}, {"sinh", "asinh"},
            {"acosh", "cosh"}, {"cosh", "acosh"}, {"atanh", "tanh"}, {"tanh", "atanh"}
        };
        auto it = inv.find(fname);
        if (good && it != inv.end() && !kk.isZero()) {
            Ex rhs = simplify(expand(Mul(Num(-1), rest, Pow(Num(kk), Num(-1)))));
            Ex target = simplify(Fn(it->second, {rhs}));
            Ex ysol = solveLinear(Add(farg, Mul(Num(-1), target)), y);
            if (ysol) return simplify(ysol);
        }
    } catch (...) {}
    return nullptr;
}

// ---------------------------------------------------------------- simplification retry
// When none of the supported forms matches, try a small set of exact
// normalisations (expand, together, clear denominators) and run the full
// classifier again on each distinct candidate.
static Result trySimplifiedFallback(const Ex& F, const OdeOptions& opt) {
    if (opt.simplifyAttempts >= 2) return Result{};
    std::vector<Ex> candidates;
    std::set<std::string> dividedKeys;
    auto add = [&](const Ex& e) {
        if (!e) return;
        try {
            Ex s = simplify(e);
            std::string key = toPlain(s);
            if (key.empty()) return;
            for (auto& old : candidates)
                if (toPlain(old) == key) return;
            candidates.push_back(s);
        } catch (...) {}
    };
    // Dividing a common factor out of F=0 preserves the solution curves
    // (except on the zero set of the factor).  This is often what turns a
    // scaled exact/linear equation into its recognizable form.
    auto addDivisions = [&](const Ex& e) {
        if (!e || !e->isMul()) return;
        for (auto& factor : e->args) {
            if (factor->isNum() || (factor->isPow() && factor->args[1]->isNum() &&
                                    factor->args[1]->num.isNeg()))
                continue;
            try {
                Ex q = simplify(Mul(e, Pow(factor, Num(-1))));
                std::string key = toPlain(q);
                if (key.empty() || key == toPlain(F)) continue;
                dividedKeys.insert(key);
                add(q);
            } catch (...) {}
        }
    };
    try {
        Ex s0 = simplify(F);
        add(s0);
        addDivisions(s0);
        Ex e0 = simplify(expand(F));
        add(e0);
        addDivisions(e0);
        Ex t0 = together(s0);
        add(t0);
        addDivisions(t0);
        Ex n0 = numeratorOf(t0);
        add(n0);
        addDivisions(n0);
        add(expand(n0));
        Ex t1 = together(e0);
        add(t1);
        addDivisions(t1);
        add(numeratorOf(t1));
    } catch (...) {
        return Result{};
    }

    const std::string original = toPlain(F);
    for (auto& candidate : candidates) {
        std::string text = toPlain(candidate);
        if (text == original || text.size() > 2000) continue;
        OdeOptions o = opt;
        ++o.simplifyAttempts;
        Result rr = solveODE(text, o);
        if (rr.ok) {
            const bool en = opt.lang != "fa";
            const std::string simplificationTitle = en ? "Simplification" : "ساده‌سازی";
            if (rr.steps.empty() || rr.steps.front().title != simplificationTitle) {
                rr.steps.insert(rr.steps.begin(), Step{
                    simplificationTitle, candidate,
                    en ? "The equation was simplified before retrying the supported forms."
                       : "معادله پیش از تلاش دوباره برای قالب‌های پشتیبانی‌شده ساده شد."});
            }
            if (dividedKeys.count(text))
                rr.warnings.push_back(en
                    ? "Simplification divided by a factor; check the case where that factor is zero."
                    : "در ساده‌سازی بر یک عامل تقسیم شد؛ حالت صفرشدن آن عامل را بررسی کنید.");
            return rr;
        }
    }
    return Result{};
}

// ---------------------------------------------------------------- verification
void verifyExplicit(Result& r, const Ex& F, const Ex& yExpr, const std::string& x,
                    const std::string& y, bool en) {
    if (!r.checks.empty()) return;
    try {
        // The caller normally sets r.order, but the helper is also used
        // directly in tests; derive the required order from F when needed.
        int neededOrder = r.order;
        std::function<void(const Ex&)> scan = [&](const Ex& e) {
            if (!e) return;
            if (isDeriv(e) && e->args[1]->isSym(x)) neededOrder = std::max(neededOrder, derivOrder(e));
            for (auto& c : e->args) scan(c);
        };
        scan(F);
        std::map<std::string, Ex> sub;
        sub[y] = yExpr;
        Ex cur = yExpr;
        for (int k = 1; k <= neededOrder; ++k) {
            cur = simplify(diff(cur, x));
            sub["__d" + std::to_string(k)] = cur;
        }
        Ex Fx = F;
        // replace Deriv nodes by their explicit values
        std::function<Ex(const Ex&)> repl = [&](const Ex& e) -> Ex {
            if (!e) return e;
            if (isDeriv(e) && e->args[0]->isSym(y) && e->args[1]->isSym(x)) {
                int o = derivOrder(e);
                auto it = sub.find("__d" + std::to_string(o));
                return it != sub.end() ? it->second : e;
            }
            if (e->args.empty()) return e;
            std::vector<Ex> v;
            for (auto& a : e->args) v.push_back(repl(a));
            switch (e->kind) {
                case Kind::Add: return Add(v);
                case Kind::Mul: return Mul(v);
                case Kind::Pow: return Pow(v[0], v[1]);
                case Kind::Func: return Fn(e->name, v);
                default: return e;
            }
        };
        Ex res = simplify(repl(Fx));
        sub.erase(y);
        bool ok = res->isZero();
        if (!ok) {
            Ex chk = simplify(expand(substitute(res, y, yExpr)));
            ok = chk->isZero() || numericallyZero(chk, {x}, 8, 1e-6);
        }
        r.checks.push_back({en ? "Substitution into the ODE" : "جایگذاری در معادله", ok,
                            en ? (ok ? "Differentiating the answer satisfies the equation." : "Symbolic verification was inconclusive; check the domain.") : (ok ? "پاسخ با مشتق‌گیری، در معادله صدق می‌کند." : "بررسی نمادین قطعی نشد؛ لطفاً دامنه را بررسی کنید.")});
    } catch (...) {
        r.checks.push_back({en ? "Substitution into the ODE" : "جایگذاری در معادله", false,
                            en ? "Automatic verification was not possible." : "بررسی خودکار ممکن نشد."});
    }
}

static void verifyImplicit(Result& r, const Ex& u, const Ex& M, const Ex& N, const std::string& x,
                           const std::string& y, bool en = true) {
    try {
        Ex ux = simplify(diff(u, x));
        Ex uy = simplify(diff(u, y));
        Ex rel = simplify(Add(Mul(ux, N), Mul(Num(-1), Mul(uy, M))));
        bool ok = rel->isZero() || numericallyZero(rel, {x, y}, 8, 1e-6);
        r.checks.push_back({en ? "Exactness condition" : "شرط دیفرانسیل کامل", ok,
                            en ? (ok ? "du is proportional to M dx + N dy; the solution family is valid."
                                     : "The symbolic relation was not confirmed; branch points may exist.")
                               : (ok ? "du متناسب با M dx + N dy است؛ خانواده جواب‌ها معتبر است."
                                     : "رابطه نمادین تأیید نشد؛ ممکن است نقاط شاخه‌دار وجود داشته باشد.")});
    } catch (...) {
        r.checks.push_back({en ? "Exactness condition" : "شرط دیفرانسیل کامل", false,
                            en ? "Automatic verification was not possible." : "بررسی خودکار ممکن نشد."});
    }
}

// ---------------------------------------------------------------- differential form
static bool extractMN(const Ex& F, const std::string& x, const std::string& y, Ex& M, Ex& N) {
    if (!F) return false;
    std::vector<std::string> syms;
    collectSymbols(F, syms);
    bool hasDx = false, hasDy = false;
    for (auto& s : syms) { if (s == "dx") hasDx = true; if (s == "dy") hasDy = true; }
    if (!hasDx || !hasDy) return false;
    Ex t = Sym("__q");
    Ex G = substituteSubexpr(F, Sym("dx"), t);
    std::vector<Ex> cs;
    if (!polyCoeffs(expand(G), "__q", cs) || cs.size() != 2) return false;
    M = cs[1];
    Ex rest = cs[0];
    if (dependsOn(rest, "dx")) return false;
    std::vector<Ex> cs2;
    if (!polyCoeffs(expand(rest), "dy", cs2)) return false;
    if (cs2.size() > 2) return false;
    N = cs2.size() == 2 ? cs2[1] : Num(0);
    if (cs2.size() >= 1 && !cs2[0]->isZero()) return false;
    if (!M || !N) return false;
    if (dependsOn(M, "dx") || dependsOn(M, "dy") || dependsOn(N, "dy")) return false;
    return true;
}

// ---------------------------------------------------------------- particular solution (Riccati)
static Ex findParticular(const Ex& Pc, const Ex& Q, const Ex& R, const std::string& x,
                         const std::string& y, const Ex& yp) {
    Ex X = Sym(x);
    if (R->isZero()) return Num(0);
    // constant coefficients -> real equilibrium roots
    if (Pc->isNum() && Q->isNum() && R->isNum()) {
        auto roots = solvePoly(Add({Mul(Pc, Pow(Sym(y), Num(2))), Mul(Q, Sym(y)), R}), y);
        for (auto& rr : roots) if (rr && rr->isNum()) return rr;
    }
    std::vector<Ex> cands = {
        Num(0), Num(1), Num(-1), Num(2), Num(-2),
        X, Mul(Num(-1), X), Add(X, Num(1)), Add(X, Num(-1)),
        Pow(X, Num(2)), Pow(X, Num(-1)),
        expOf(X), expOf(Mul(Num(-1), X)),
        Fn("sin", {X}), Fn("cos", {X}), Fn("tan", {X}), Fn("ln", {X}),
        Sqrt(X), Mul(Num(2), X), Add(Mul(Num(2), X), Num(1))
    };
    for (auto& c : cands) {
        try {
            Ex rhs = Add(Mul(Pc, Pow(c, Num(2))), Mul(Q, c), R);
            // A particular solution must satisfy the ODE itself, so its
            // derivative (not a formal substitution of y′) is the left side.
            Ex lhs = simplify(diff(c, x));
            Ex res = simplify(Add(lhs, Mul(Num(-1), rhs)));
            if (!dependsOn(res, y) && (res->isZero() || numericallyZero(res, {x}, 6, 1e-7))) return c;
        } catch (...) {}
    }
    // linear ansatz y = a x + b, constants a,b (search small rationals)
    for (int a = -3; a <= 3; ++a) {
        for (int b = -3; b <= 3; ++b) {
            Ex c = Add(Mul(Num(a), X), Num(b));
            try {
                Ex rhs = Add(Mul(Pc, Pow(c, Num(2))), Mul(Q, c), R);
                Ex lhs = simplify(diff(c, x));
                Ex res = simplify(Add(lhs, Mul(Num(-1), rhs)));
                if (!dependsOn(res, y) && (res->isZero() || numericallyZero(res, {x}, 6, 1e-7))) return c;
            } catch (...) {}
        }
    }
    (void)y;
    return nullptr;
}

// ---------------------------------------------------------------- first order
static void setSolution(Result& r, const std::string& id, const std::string& fa, const Ex& sol,
                        bool explicitForm, const Ex& yExpr = nullptr) {
    r.ok = true;
    r.typeId = id;
    r.typeFa = fa;
    r.solution = sol;
    r.isExplicit = explicitForm;
    r.explicitY = explicitForm ? (yExpr ? yExpr : sol) : nullptr;
    r.closedForm = !hasUnevaluatedIntegral(sol) && (!r.explicitY || !hasUnevaluatedIntegral(r.explicitY));
}

static bool tryClairautLagrange(Builder& b, const Ex& F, const Ex& p) {
    // Clairaut/Lagrange equations are nonlinear in y′ (degree ≥ 2, or
    // transcendental). A degree-1 equation such as (2x+y)+(x+2y)y′=0 is a
    // linear/exact equation and must be handled by those solvers.
    {
        Ex t = Sym("__p");
        Ex G = substituteSubexpr(F, p, t);
        std::vector<Ex> pc;
        if (polyCoeffs(expand(G), "__p", pc) && pc.size() <= 2) return false;
    }
    Ex ysol = solveLinear(F, b.y);
    if (!ysol) return false;
    Ex ye = expand(ysol);
    std::vector<Ex> cs;
    if (!polyCoeffs(ye, b.x, cs) || cs.size() != 2) return false;
    Ex g = simplify(cs[1]);
    Ex f = simplify(cs[0]);
    if (dependsOn(g, b.x) || dependsOn(f, b.x)) return false;
    if (g->isZero()) return false;
    bool gHasP = false;
    std::function<void(const Ex&)> scanP = [&](const Ex& e) {
        if (!e) return;
        if (isDeriv(e) && e->args[0]->isSym(b.y)) gHasP = true;
        for (auto& c : e->args) scanP(c);
    };
    scanP(g);
    if (!gHasP) return false;
    bool clairaut = equals(g, p);
    if (!clairaut) {
        // y = x·g(p) + f(p). If both g and f are at most linear in p the
        // equation is really a linear first-order ODE (e.g. 2y + x y′ = 0),
        // so let the linear/separable solvers handle it.
        std::vector<Ex> gc, fc;
        Ex gg0 = substituteSubexpr(g, p, Sym("p"));
        Ex ff0 = substituteSubexpr(f, p, Sym("p"));
        bool gLin = polyCoeffs(expand(gg0), "p", gc) && gc.size() <= 2;
        bool fLin = polyCoeffs(expand(ff0), "p", fc) && fc.size() <= 2;
        if (gLin && fLin) return false;
    }
    const size_t stepMark = b.r.steps.size();
    const size_t warnMark = b.r.warnings.size();
    b.r.order = 1;
    Ex tp = Sym("p");
    Ex gg = substituteSubexpr(g, p, tp);
    Ex ff = substituteSubexpr(f, p, tp);
    Ex gp0 = simplify(diff(gg, "p"));
    Ex fp0 = simplify(diff(ff, "p"));

    b.step("شناسایی نوع", Add({Mul(Sym(b.x), gg), ff}),
           clairaut
               ? "معادله به شکل y = x·p + f(p) با p = y′ است، یعنی کلرو؛ f(p) = " +
                     toPretty(ff) + "."
               : "معادله به شکل y = x·g(p) + f(p) با p = y′ است، یعنی لاگرانژ؛ g(p) = " +
                     toPretty(gg) + " و f(p) = " + toPretty(ff) + ".",
           "Type identification",
           clairaut
               ? "The equation has the Clairaut form y = xp + f(p) with p = y′; f(p) = " +
                     toPretty(ff) + "."
               : "The equation has the Lagrange form y = x g(p) + f(p) with p = y′; g(p) = " +
                     toPretty(gg) + " and f(p) = " + toPretty(ff) + ".");
    b.step("جایگذاری p = y′",
           Add({Sym(b.y), Mul(Num(-1), Mul(Sym(b.x), gg)), Mul(Num(-1), ff)}),
           "با p = y′، معادله به شکل y = x·" + std::string(clairaut ? "p" : "g(p)") +
               " + f(p) نوشته می‌شود.",
           "Substitute p = y′",
           "With p = y′, the equation reads y = x·" + std::string(clairaut ? "p" : "g(p)") +
               " + f(p).");

    if (clairaut) {
        b.step("مشتق‌گیری نسبت به x",
               Mul(Add({Sym(b.x), fp0}), Deriv(tp, b.x, 1)),
               "مشتق‌گیری از دو طرف و استفاده از y′ = p می‌دهد (x + f′(p))·dp/dx = 0.",
               "Differentiate with respect to x",
               "Differentiating both sides and using y′ = p gives (x + f′(p))·dp/dx = 0.");

        Ex gen = Add(Mul(Cst(1), Sym(b.x)), substituteSubexpr(f, p, Cst(1)));
        b.step("جواب عمومی", gen,
               "از عامل اول dp/dx = 0 داریم p = C، پس خانوادهٔ خطوط y = Cx + f(C) به دست می‌آید.",
               "General solution",
               "The first factor dp/dx = 0 gives p = C, hence the line family y = Cx + f(C).");

        b.r.singular.clear();
        try {
            Ex fp = simplify(diff(ff, "p"));
            if (fp->isZero()) {
                b.step("جواب منفرد", nullptr,
                       "چون f′(p) = 0 است، شرط x + f′(p) = 0 هیچ مقدار متناهی برای p نمی‌دهد؛ "
                       "جواب منفرد وجود ندارد.",
                       "Singular solution",
                       "Since f′(p) = 0, the condition x + f′(p) = 0 gives no finite p; there is no "
                       "singular solution.");
            } else {
                Ex px = Mul(Num(-1), fp);
                Ex py = simplify(Add(ff, Mul(Num(-1), Mul(tp, fp))));
                Ex elim = solveLinear(Add(px, Mul(Num(-1), Sym(b.x))), "p");
                if (elim) {
                    Ex sing = simplify(substituteSubexpr(py, tp, elim));
                    b.r.singular.push_back(sing);
                    b.step("جواب منفرد (پوش)", sing,
                           "از عامل دوم x + f′(p) = 0 داریم x = " + toPretty(px) +
                               " و y = f(p) − p·f′(p) = " + toPretty(py) + "؛ حذف p با p = " +
                               toPretty(elim) + " پوش را می‌دهد.",
                           "Singular solution (envelope)",
                           "The second factor x + f′(p) = 0 gives x = " + toPretty(px) +
                               " and y = f(p) − p f′(p) = " + toPretty(py) + "; eliminating p with p = " +
                               toPretty(elim) + " gives the envelope.");
                } else {
                    b.r.paramX = px;
                    b.r.paramY = py;
                    b.r.paramName = "p";
                    b.r.paramSingular = true;
                    b.step("جواب منفرد (پوش، پارامتری)", nullptr,
                           "عامل دوم x + f′(p) = 0 جواب منفرد را به‌صورت پارامتری x(p) = " +
                               toPretty(px) + "، y(p) = " + toPretty(py) + " می‌دهد.",
                           "Singular solution (envelope, parametric)",
                           "The second factor x + f′(p) = 0 gives the singular solution "
                           "parametrically: x(p) = " + toPretty(px) + ", y(p) = " + toPretty(py) + ".");
                }
            }
        } catch (...) {}
        setSolution(b.r, "clairaut", "کلرو", gen, true, gen);
        b.r.normalized = F;
        verifyExplicit(b.r, F, gen, b.x, b.y, b.en);
        return true;
    }

    // Lagrange: dx/dp - g'/(p-g) x = f'/(p-g)
    try {
        Ex gp = simplify(diff(gg, "p"));
        Ex fp = simplify(diff(ff, "p"));
        Ex den = simplify(Add(tp, Mul(Num(-1), gg)));
        if (den->isZero()) {
            b.r.steps.resize(stepMark);
            b.r.warnings.resize(warnMark);
            return false;
        }
        Ex Pc = simplify(Mul(Num(-1), gp, Pow(den, Num(-1))));
        Ex Qc = simplify(Mul(fp, Pow(den, Num(-1))));
        Ex mu, intRhs;
        Ex xp = solveLinearODE("p", Pc, Qc, &mu, &intRhs);
        (void)intRhs;
        Ex yp = simplify(Add(Mul(xp, gg), ff));
        b.step("معادلهٔ خطی بر حسب x(p)",
               Add({Deriv(Sym("x"), "p", 1), Mul(Pc, Sym("x")), Mul(Num(-1), Qc)}),
               "مشتق‌گیری و بازآرایی، معادلهٔ خطی dx/dp + (" + toPretty(Pc) + ")·x = " +
                   toPretty(Qc) + " را برای x(p) می‌دهد.",
               "Linear equation in x(p)",
               "Differentiating and rearranging gives the linear equation dx/dp + (" +
                   toPretty(Pc) + ")x = " + toPretty(Qc) + " for x(p).");
        b.step("حل معادلهٔ خطی", xp,
               "با عامل انتگرال‌ساز μ = " + toPretty(mu) + "، x(p) به دست می‌آید.",
               "Solve the linear equation",
               "With the integrating factor μ = " + toPretty(mu) + ", x(p) is obtained.");
        b.step("جواب پارامتری", yp,
               "با جایگذاری x(p) در y = x·g(p) + f(p)، جواب عمومی به شکل پارامتری نوشته می‌شود.",
               "Parametric solution",
               "Substituting x(p) into y = x g(p) + f(p) writes the general solution parametrically.");
        b.r.paramX = xp;
        b.r.paramY = yp;
        b.r.paramName = "p";
        b.r.ok = true;
        b.r.typeId = "lagrange";
        b.r.typeFa = "لاگرانژ";
        b.r.closedForm = !hasUnevaluatedIntegral(xp) && !hasUnevaluatedIntegral(yp);
        b.r.normalized = F;
        // singular: p0 with g(p0)=p0
        auto roots = solvePoly(den, "p");
        for (auto& rr : roots) {
            Ex yy = simplify(Add(Mul(rr, Sym(b.x)), substituteSubexpr(ff, tp, rr)));
            b.r.singular.push_back(yy);
        }
        return true;
    } catch (...) {
        b.r.steps.resize(stepMark);
        b.r.warnings.resize(warnMark);
        return false;
    }
}

static bool tryRiccati(Builder& b, const Ex& yp, const std::vector<TermPow>& ts) {
    size_t stepMark = b.r.steps.size();
    size_t warnMark = b.r.warnings.size();
    Ex Pc = coeffOf(ts, 2);
    if (Pc->isZero()) return false;
    Ex Q = coeffOf(ts, 1);
    Ex R = coeffOf(ts, 0);
    for (auto& t : ts) {
        if (!t.exp->isNum() || t.exp->num.isInt() == false) return false;
        long long n = t.exp->num.n;
        if (n < 0 || n > 2) return false;
    }
    b.r.order = 1;
    b.step("شناسایی نوع", Add({Mul(Pc, Pow(b.Y, Num(2))), Mul(Q, b.Y), R}),
           "طرف راست در y درجهٔ دوم است، پس معادله ریکاتی است: y′ = P·y² + Q·y + R با P = " +
               toPretty(Pc) + "، Q = " + toPretty(Q) + " و R = " + toPretty(R) + ".",
           "Type identification",
           "The right side is quadratic in y, so this is Riccati: y′ = P y² + Q y + R with P = " +
               toPretty(Pc) + ", Q = " + toPretty(Q) + " and R = " + toPretty(R) + ".");
    Ex y1;
    if (!b.opt.particular.empty()) {
        std::string dep;
        y1 = parseODEExpr(b.opt.particular, b.x, &dep, {b.x, b.y, "c"});
    } else {
        y1 = findParticular(Pc, Q, R, b.x, b.y, yp);
    }
    if (y1) {
        b.step("یک جواب خصوصی", y1, "با جایگذاری در معادله بررسی و تأیید شد.");
        Ex A = simplify(Add(Mul(Num(2), Pc, y1), Q));
        Ex B = simplify(Mul(Num(-1), Pc));
        Ex z = solveLinearODE(b.x, A, B);
        Ex ysol = simplify(Add(y1, Pow(z, Num(-1))));
        b.step("جایگذاری y = y₁ + 1/z",
               Add({Deriv(Sym("z"), b.x, 1), Mul(A, Sym("z")), Mul(Num(-1), B)}),
               "با y = y₁ + 1/z و داشتن یک جواب خصوصی، معادلهٔ غیرخطی به معادلهٔ خطی "
               "z′ + (2P·y₁ + Q)·z = −P تبدیل می‌شود.",
               "Substitute y = y₁ + 1/z",
               "With y = y₁ + 1/z and one known particular solution, the nonlinear equation "
               "becomes the linear equation z′ + (2P·y₁ + Q)z = −P.");
        b.step("حل خطی برای z", z);
        b.step("جواب عمومی", ysol);
        b.r.normalized = yp;
        setSolution(b.r, "riccati", "ریکاتی", ysol, true, ysol);
        if (b.r.closedForm) {
            verifyExplicit(b.r, Add(b.P, Mul(Num(-1), Add(Mul(Pc, Pow(Sym(b.y), Num(2))), Mul(Q, Sym(b.y)), R))),
                           ysol, b.x, b.y, b.en);
        } else {
            b.warn("جواب شامل انتگرال غیرمقدماتی است و به صورت کوادراتور دقیق ارائه شده است.",
                   "The solution contains a non-elementary integral and is presented as an exact quadrature.");
        }
        return true;
    }
    // transform to second order linear: y = -u'/(P u)
    try {
        Ex Pp = simplify(diff(Pc, b.x));
        Ex a1 = simplify(Mul(Num(-1), Add(Q, Mul(Pp, Pow(Pc, Num(-1))))));
        Ex a0 = simplify(Mul(Pc, R));
        b.step("تبدیل ریکاتی", nullptr,
               "با y = −u′/(P u)، معادله به u″ − (Q + P′/P)u′ + P R u = 0 تبدیل می‌شود.");
        b.step("معادلهٔ خطی مرتبه دوم", nullptr,
               "u″ + (" + toPretty(a1) + ")u′ + (" + toPretty(a0) + ")u = 0");
        if (a1->isNum() && a0->isNum()) {
            Rational A1 = a1->num, A0 = a0->num;
            Rational disc = A1 * A1 - A0 * Rational(4);
            Ex X = Sym(b.x);
            Ex u = nullptr;
            if (disc.isZero()) {
                Rational r = -A1 * Rational(1, 2);
                u = Add(Mul(Cst(1), expOf(Mul(Num(r), X))),
                        Mul(Cst(2), X, expOf(Mul(Num(r), X))));
            } else if (disc.isNeg()) {
                Rational alpha = -A1 * Rational(1, 2);
                Ex beta = simplify(Mul(Sqrt(Num(-disc)), Num(Rational(1, 2))));
                Ex env = expOf(Mul(Num(alpha), X));
                u = Add(Mul(Cst(1), env, Fn("cos", {Mul(beta, X)})),
                        Mul(Cst(2), env, Fn("sin", {Mul(beta, X)})));
            } else {
                Ex sq = Sqrt(Num(disc));
                Ex r1 = simplify(Mul(Add(Num(-A1), sq), Num(Rational(1, 2))));
                Ex r2 = simplify(Mul(Add(Num(-A1), Mul(Num(-1), sq)), Num(Rational(1, 2))));
                u = Add(Mul(Cst(1), expOf(Mul(r1, X))),
                        Mul(Cst(2), expOf(Mul(r2, X))));
            }
            if (u) {
                Ex up = simplify(diff(u, b.x));
                Ex ysol = simplify(Mul(Num(-1), up, Pow(Mul(Pc, u), Num(-1))));
                b.step("حل معادلهٔ مرتبه دوم و بازگشت به y", ysol,
                       "جواب u به دست آمد و با y = −u′/(P u) به جواب ریکاتی برمی‌گردیم.");
                setSolution(b.r, "riccati", "ریکاتی", ysol, true, ysol);
                return true;
            }
        }
        b.warn("برای حل ریکاتی، یک جواب خصوصی پیدا نشد. تبدیل به معادلهٔ خطی مرتبهٔ دوم داده شد.");
        b.r.steps.resize(stepMark);
        b.r.warnings.resize(warnMark);
        return false;
    } catch (...) {
        b.r.steps.resize(stepMark);
        b.r.warnings.resize(warnMark);
        return false;
    }
}

static bool tryBernoulli(Builder& b, const Ex& yp, const std::vector<TermPow>& ts) {
    if (!coeffOf(ts, 0)->isZero()) return false;
    Ex pcoef = coeffOf(ts, 1);
    Ex nEx = nullptr, qcoef = nullptr;
    int count = 0;
    for (auto& t : ts) {
        if (!t.exp->isNum()) return false;
        if (t.exp->num == Rational(0)) continue;
        if (t.exp->num == Rational(1)) continue;
        nEx = t.exp; qcoef = t.coeff;
        ++count;
    }
    if (!nEx || count != 1) return false;
    Rational n = nEx->num;
    // y' = -pcoef*y + qcoef*y^n  => y' + P y = Q y^n with P = pcoef, Q = qcoef
    Ex Pc = pcoef;
    Ex Q = qcoef;
    b.r.order = 1;
    b.step("شناسایی نوع", Add({Mul(Pc, b.Y), Mul(Q, Pow(b.Y, nEx))}),
           "معادله به شکل برنولی y′ + P(x)·y = Q(x)·y^n است با n = " + rationalToString(n) +
               "، P = " + toPretty(Pc) + " و Q = " + toPretty(Q) + ".",
           "Type identification",
           "This is the Bernoulli form y′ + P(x)y = Q(x)y^n with n = " + rationalToString(n) +
               ", P = " + toPretty(Pc) + " and Q = " + toPretty(Q) + ".");
    // v = y^(1-n)
    Ex oneMinusN = Num(Rational(1) - n);
    Ex A = simplify(Mul(Num(n - Rational(1)), Pc));   // (1-n)*(-P)
    Ex B = simplify(Mul(oneMinusN, Q));
    Ex vz = solveLinearODE(b.x, A, B);
    Ex ysol = simplify(Pow(vz, Pow(oneMinusN, Num(-1))));
    b.step("تغییر متغیر v = y^(1−n)",
           Add({Deriv(Sym("v"), b.x, 1), Mul(A, Sym("v")), Mul(Num(-1), B)}),
           "با v = y^(1−n) معادله به شکل خطی v′ + " + toPretty(A) + "·v = " + toPretty(B) +
               " تبدیل می‌شود.",
           "Substitute v = y^(1−n)",
           "With v = y^(1−n) the equation becomes linear: v′ + " + toPretty(A) + "v = " +
               toPretty(B) + ".");
    b.step("حل خطی برای v", vz,
           "این معادلهٔ خطی با عامل انتگرال‌ساز حل می‌شود و v(x) به دست می‌آید.",
           "Solve the linear equation for v",
           "The linear equation is solved with an integrating factor to obtain v(x).");
    b.step("بازگشت به y", ysol,
           "با y = v^(1/(1−n)) به متغیر اصلی برمی‌گردیم.",
           "Return to y",
           "The substitution y = v^(1/(1−n)) returns to the original variable.");
    b.r.singular.push_back(Num(0));
    setSolution(b.r, "bernoulli", "برنولی", ysol, true, ysol);
    if (b.r.closedForm)
        verifyExplicit(b.r, Add(b.P, Mul(Num(-1), Add(Mul(Pc, Sym(b.y)), Mul(Q, Pow(Sym(b.y), nEx))))),
                       ysol, b.x, b.y, b.en);
    else
        b.warn("انتگرال‌های جواب مقدماتی نیستند؛ جواب به صورت کوادراتور دقیق ارائه شده است.");
    return true;
}

static bool tryLinear(Builder& b, const Ex& yp, const std::vector<TermPow>& ts) {
    for (auto& t : ts) {
        if (!t.exp->isNum() || !t.exp->num.isInt()) return false;
        long long n = t.exp->num.n;
        if (n < 0 || n > 1) return false;
    }
    Ex pcoef = coeffOf(ts, 1);
    Ex q = coeffOf(ts, 0);
    Ex P = simplify(Mul(Num(-1), pcoef));
    b.r.order = 1;
    b.step("شناسایی نوع", Add({b.P, Mul(P, b.Y), Mul(Num(-1), q)}),
           "پس از مرتب کردن: y′ + P(x)·y = Q(x) با P = " + toPretty(P) + " و Q = " +
               toPretty(q) + ".",
           "Type identification",
           "After rearrangement: y′ + P(x)y = Q(x) with P = " + toPretty(P) + " and Q = " +
               toPretty(q) + ".");
    Ex mu, intRhs;
    Ex ysol = solveLinearODE(b.x, P, q, &mu, &intRhs);
    b.step("عامل انتگرال‌ساز", mu,
           "μ = exp(∫P dx)؛ ضرب در μ دو طرف را به (μ·y)′ = μ·Q تبدیل می‌کند.",
           "Integrating factor",
           "μ = exp(∫P dx); multiplying by μ turns both sides into (μy)′ = μQ.");
    b.step("جواب عمومی", ysol,
           "انتگرال دو طرف ∫μQ dx = " + toPretty(intRhs) +
               " را می‌دهد؛ تقسیم بر μ، y را جدا می‌کند و C آزاد می‌ماند.",
           "General solution",
           "Integrating gives ∫μQ dx = " + toPretty(intRhs) +
               "; dividing by μ isolates y and leaves C free.");
    setSolution(b.r, "linear", "خطی", ysol, true, ysol);
    if (b.r.closedForm)
        verifyExplicit(b.r, Add(b.P, Mul(Num(-1), Add(Mul(pcoef, Sym(b.y)), q))), ysol, b.x, b.y, b.en);
    else
        b.warn("جواب شامل انتگرال غیرمقدماتی است و به صورت کوادراتور دقیق ارائه شده است.");
    return true;
}

static std::vector<Ex> subCandidates(const Ex& a, const Ex& whole) {
    std::vector<Ex> out;
    std::function<void(const Ex&)> rec = [&](const Ex& e) {
        if (!e || e->args.empty()) return;
        if (!equals(e, whole)) {
            bool dup = false;
            for (auto& c : out) if (equals(c, e)) { dup = true; break; }
            if (!dup) out.push_back(e);
        }
        for (auto& x : e->args) rec(x);
    };
    rec(a);
    return out;
}

static bool tryHomogeneous(Builder& b, const Ex& yp) {
    if (!dependsOn(yp, b.y)) return false;
    Ex v = Sym("v");
    Ex sub = substitute(yp, b.y, Mul(v, Sym(b.x)));
    sub = expand(simplify(sub));
    if (dependsOn(sub, b.x)) return false;
    b.r.order = 1;
    b.step("شناسایی نوع", sub,
           "اگر در طرف راست y = v·x گذاشته شود، عبارت تنها به v بستگی پیدا می‌کند؛ پس معادله "
           "همگن است و به نسبت y/x وابسته است.",
           "Type identification",
           "Replacing y by v·x leaves an expression that depends on v alone, so the equation is "
           "homogeneous: it depends only on the ratio y/x.");
    b.step("تغییر متغیر y = v·x", Add({v, Mul(Sym(b.x), Deriv(v, b.x, 1))}),
           "با y = v·x قاعدهٔ ضرب می‌دهد y′ = v + x·v′، پس معادله به x·v′ = F(v) − v تبدیل می‌شود.",
           "Substitute y = v·x",
           "With y = v·x the product rule gives y′ = v + x·v′, so the equation becomes "
           "x·v′ = F(v) − v.");
    Ex Fv = sub;
    Ex num = simplify(Add(Fv, Mul(Num(-1), v)));
    if (num->isZero()) {
        Ex sol = Mul(Cst(1), Sym(b.x));
        b.step("جواب عمومی", sol, "در این حالت F(v) = v و خانوادهٔ خطوط y = Cx به دست میآید.");
        setSolution(b.r, "homogeneous", "همگن", sol, true, sol);
        return true;
    }
    Ex integral = integrate(Pow(num, Num(-1)), "v");
    Ex sol = Add(integral, Mul(Num(-1), lnAbs(Sym(b.x))));
    b.step("جداسازی متغیرها", num,
           "معادلهٔ به‌دست‌آمده جداشدنی است: dv/(F(v) − v) = dx/x.",
           "Separate the variables",
           "The transformed equation is separable: dv/(F(v) − v) = dx/x.");
    b.step("انتگرال‌گیری", sol,
           "دو طرف انتگرال می‌شوند و یک ثابت مشترک می‌گیرند.",
           "Integrate",
           "Both sides are integrated and share a single constant.");
    b.step("بازگشت به y", substituteSubexpr(sol, v, Mul(Sym(b.y), Pow(Sym(b.x), Num(-1)))),
           "با جایگذاری v = y/x دوباره به متغیر اصلی برمی‌گردیم و جواب ضمنی حاصل می‌شود.",
           "Return to y",
           "Substituting v = y/x returns to the original variable and gives the implicit solution.");
    // constant/singular solutions from F(v)-v=0
    auto roots = solvePoly(num, "v");
    for (auto& rr : roots) {
        if (!dependsOn(rr, "v")) {
            b.r.singular.push_back(simplify(Mul(Sym(b.x), rr)));
        }
    }
    // try explicit y
    Ex implicit = substituteSubexpr(sol, v, Mul(Sym(b.y), Pow(Sym(b.x), Num(-1))));
    Ex ex = tryExplicit(Add(implicit, Mul(Num(-1), Cst(1))), b.x, b.y);
    setSolution(b.r, "homogeneous", "همگن", implicit, ex != nullptr, ex);
    b.r.normalized = yp;
    // verify via exact relation u_x N - u_y M, M=-F, N=1
    verifyImplicit(b.r, implicit, Mul(Num(-1), yp), Num(1), b.x, b.y, b.en);
    return true;
}

static bool trySeparable(Builder& b, const Ex& yp) {
    if (!dependsOn(yp, b.y)) return false;
    Ex yp_x = simplify(diff(yp, b.x));
    Ex yp_y = simplify(diff(yp, b.y));
    Ex yp_xy = simplify(diff(yp_x, b.y));
    Ex test = simplify(Add(Mul(yp, yp_xy), Mul(Num(-1), Mul(yp_x, yp_y))));
    if (!test->isZero()) return false;
    // g(y) = yp at x=1, f(x) = yp/g
    Ex ype = expand(yp);
    // Normalise at x = 0 when possible: it keeps g(y) free of stray constants.
    Ex g = nullptr;
    for (int pt : {0, 1, 2}) {
        try {
            Ex cand = expand(simplify(substitute(ype, b.x, Num(pt))));
            if (!cand->isZero() && !dependsOn(cand, b.x)) { g = cand; break; }
        } catch (...) {}
    }
    if (!g) return false;
    Ex f = simplify(Mul(ype, Pow(g, Num(-1))));
    if (dependsOn(f, b.y)) return false;
    // Strip a constant scale from g(y): the solution family is unchanged, but
    // the displayed split becomes sin(x)·cos(y) instead of sin(x)/sin(1).
    if (g->isMul()) {
        std::vector<Ex> cst, rest;
        for (auto& fac : g->args) {
            if (dependsOn(fac, b.y)) rest.push_back(fac);
            else cst.push_back(fac);
        }
        if (!cst.empty() && !rest.empty()) {
            Ex c = cst.size() == 1 ? cst[0] : Mul(cst);
            if (!c->isZero()) {
                g = rest.size() == 1 ? rest[0] : Mul(rest);
                f = simplify(Mul(f, c));
            }
        }
    }
    b.r.order = 1;
    b.step("شناسایی نوع", Mul(f, g),
           "طرف راست حاصل‌ضرب دو عامل است: f(x) = " + toPretty(f) + " و g(y) = " +
               toPretty(g) + "؛ پس معادله جداشدنی است.",
           "Type identification",
           "The right side factors as f(x) = " + toPretty(f) + " and g(y) = " + toPretty(g) +
               ", so the equation is separable.");
    Ex Ig = integrate(Pow(g, Num(-1)), b.y);
    Ex If = integrate(f, b.x);
    Ex sol = Add(Ig, Mul(Num(-1), If));
    b.step("جداسازی متغیرها", Add({Mul(Pow(g, Num(-1)), b.P), Mul(Num(-1), f)}),
           "با تقسیم بر g(y) داریم dy/g(y) = f(x) dx؛ هر طرف تنها یک متغیر دارد.",
           "Separate the variables",
           "Dividing by g(y) gives dy/g(y) = f(x) dx; each side now has one variable only.");
    b.step("انتگرال دو طرف", sol,
           "انتگرال هر طرف نسبت به متغیر خودش، با یک ثابت مشترک C، جواب ضمنی را می‌دهد.",
           "Integrate both sides",
           "Integrating each side with respect to its own variable, with one shared constant C, "
           "gives the implicit solution.");
    // singular constant solutions
    auto roots = solvePoly(g, b.y);
    for (auto& rr : roots) {
        if (dependsOn(rr, b.y)) continue;
        bool dup = false;
        for (auto& s : b.r.singular) if (equals(s, rr)) { dup = true; break; }
        if (!dup) b.r.singular.push_back(rr);
    }
    Ex ex = tryExplicit(Add(sol, Mul(Num(-1), Cst(1))), b.x, b.y);
    setSolution(b.r, "separable", "جداشدنی", sol, ex != nullptr, ex);
    b.r.normalized = yp;
    verifyImplicit(b.r, sol, Mul(Num(-1), yp), Num(1), b.x, b.y, b.en);
    return true;
}

static bool tryLinearSub(Builder& b, const Ex& yp0) {
    // y' = f(a x + y)
    Ex yp = expand(yp0);
    Ex yx = simplify(diff(yp, b.x));
    Ex yy = simplify(diff(yp, b.y));
    if (yy->isZero() || yx->isZero()) return false;
    Ex a = simplify(Mul(yx, Pow(yy, Num(-1))));
    if (dependsOn(a, b.x) || dependsOn(a, b.y)) return false;
    Ex u = Add(Mul(a, Sym(b.x)), Sym(b.y));
    // Expanded f contains no literal (a·x+y) subexpression, so substitute
    // y = u − a·x and simplify before checking dependence.
    Ex fu = simplify(substitute(yp, b.y, Add(Sym("u"), Mul(Num(-1), Mul(a, Sym(b.x))))));
    fu = simplify(expand(fu));
    if (dependsOn(fu, b.x) || dependsOn(fu, b.y)) return false;
    // u' = a + f(u)
    Ex rhs = simplify(Add(a, fu));
    Ex Ig = integrate(Pow(rhs, Num(-1)), "u");
    Ex solU = Add(Ig, Mul(Num(-1), Sym(b.x)));
    Ex sol = substituteSubexpr(solU, Sym("u"), u);
    b.r.order = 1;
    b.step("شناسایی نوع", rhs,
           "طرف راست تنها از ترکیب خطی ax + by ساخته شده است، پس با یک تغییر متغیر ساده "
           "جداشدنی می‌شود.",
           "Type identification",
           "The right side is built from the linear combination ax + by alone, so a single "
           "substitution turns it separable.");
    b.step("تغییر متغیر u = a·x + y", u,
           "با u = a·x + y داریم u′ = a + y′، پس معادله به u′ = a + f(u) تبدیل می‌شود که "
           "جداشدنی است.",
           "Substitute u = a·x + y",
           "With u = a·x + y we get u′ = a + y′, so the equation becomes u′ = a + f(u), which "
           "is separable.");
    b.step("انتگرال‌گیری", solU,
           "دو طرف را انتگرال می‌گیریم و ثابت مشترک را نگه می‌داریم.",
           "Integrate both sides",
           "Both sides are integrated, keeping a single shared constant.");
    b.step("جواب ضمنی", sol,
           "با برگرداندن u = a·x + y جواب اصلی به دست می‌آید.",
           "Implicit solution",
           "Restoring u = a·x + y gives the solution of the original equation.");
    Ex ex = tryExplicit(Add(sol, Mul(Num(-1), Cst(1))), b.x, b.y);
    setSolution(b.r, "linear_sub", "جایگذاری خطی", sol, ex != nullptr, ex);
    b.r.normalized = yp;
    return true;
}

// μ is only defined up to sign; removing |·| keeps the derivatives clean.
static Ex stripAbs(const Ex& e) {
    if (!e) return e;
    if (e->isFunc("abs") && e->args.size() == 1) return stripAbs(e->args[0]);
    if (e->args.empty()) return e;
    std::vector<Ex> v;
    for (auto& a : e->args) v.push_back(stripAbs(a));
    switch (e->kind) {
        case Kind::Add: return Add(v);
        case Kind::Mul: return Mul(v);
        case Kind::Pow: return Pow(v[0], v[1]);
        case Kind::Func: return Fn(e->name, v);
        default: return e;
    }
}

// exact / integrating factor from M dx + N dy = 0
static bool tryExact(Builder& b, const Ex& F, const Ex& M, const Ex& N, bool fromForm) {
    Ex My = simplify(diff(M, b.y));
    Ex Nx = simplify(diff(N, b.x));
    Ex d = simplify(Add(My, Mul(Num(-1), Nx)));
    b.r.order = 1;
    if (d->isZero()) {
        Ex u0 = integrate(M, b.x);
        Ex phiP = simplify(Add(N, Mul(Num(-1), diff(u0, b.y))));
        Ex phi = integrate(phiP, b.y);
        Ex u = simplify(Add(u0, phi));

        b.step("آزمون کامل بودن", My,
               "∂M/∂y = " + toPretty(My) + " و ∂N/∂x = " + toPretty(Nx) +
                   "؛ چون برابرند، معادله کامل است و تابع پتانسیل u با ∂u/∂x = M و "
                   "∂u/∂y = N وجود دارد.",
               "Exactness test",
               "∂M/∂y = " + toPretty(My) + " and ∂N/∂x = " + toPretty(Nx) +
                   "; they agree, so the equation is exact and a potential u exists with "
                   "∂u/∂x = M and ∂u/∂y = N.");
        b.step("تابع پتانسیل", u,
               "از M نسبت به x انتگرال می‌گیریم (ثابت φ(y))؛ سپس با مقایسهٔ ∂u/∂y با N "
               "مقدار φ′(y) = " + toPretty(phiP) + " و φ(y) = " + toPretty(phi) + " را می‌یابیم.",
               "Potential function",
               "Integrate M with respect to x (the constant is φ(y)); comparing ∂u/∂y with N "
               "gives φ′(y) = " + toPretty(phiP) + " and φ(y) = " + toPretty(phi) + ".");
        b.step("جواب عمومی", u,
               "منحنی‌های تراز پتانسیل جواب‌ها هستند، پس u(x,y) = C.",
               "General solution",
               "The level curves of the potential are the solutions, so u(x,y) = C.");
        Ex ex = tryExplicit(Add(u, Mul(Num(-1), Cst(1))), b.x, b.y);
        setSolution(b.r, "exact", "کامل", u, ex != nullptr, ex);
        b.r.normalized = F;
        verifyImplicit(b.r, u, M, N, b.x, b.y, b.en);
        return true;
    }
    // integrating factors
    struct Try { Ex mu; std::string kind; };
    std::vector<Try> tries;
    try {
        Ex f1 = simplify(Mul(d, Pow(N, Num(-1))));
        if (!dependsOn(f1, b.y)) {
            Ex mu = stripAbs(simplify(expOf(integrate(f1, b.x))));
            tries.push_back({mu, "μ(x) = exp(∫ (M_y − N_x)/N dx)"});
        }
    } catch (...) {}
    try {
        Ex f2 = simplify(Mul(Num(-1), d, Pow(M, Num(-1))));
        if (!dependsOn(f2, b.x)) {
            Ex mu = stripAbs(simplify(expOf(integrate(f2, b.y))));
            tries.push_back({mu, "μ(y) = exp(∫ (N_x − M_y)/M dy)"});
        }
    } catch (...) {}
    try {
        Ex den = simplify(Add(Mul(Sym(b.x), M), Mul(Num(-1), Mul(Sym(b.y), N))));
        // u = xy: μ'/μ = -(M_y - N_x)/(xM - yN), so the sign is negated here.
        Ex f3 = simplify(Mul(Num(-1), d, Pow(den, Num(-1))));
        Ex sub = substituteSubexpr(f3, Mul(Sym(b.x), Sym(b.y)), Sym("u"));
        if (!dependsOn(sub, b.x) && !dependsOn(sub, b.y)) {
            Ex mu = stripAbs(simplify(expOf(integrate(sub, "u"))));
            mu = substituteSubexpr(mu, Sym("u"), Mul(Sym(b.x), Sym(b.y)));
            tries.push_back({mu, "μ(xy) = exp(∫ f(u) du), u = xy"});
        }
    } catch (...) {}
    try {
        Ex den = simplify(Add(Mul(Num(-1), M), N)); // N-M
        Ex f4 = simplify(Mul(d, Pow(den, Num(-1))));
        Ex u = Add(Sym(b.x), Sym(b.y));
        Ex sub = substituteSubexpr(f4, u, Sym("u"));
        if (!dependsOn(sub, b.x) && !dependsOn(sub, b.y)) {
            Ex mu = stripAbs(simplify(expOf(integrate(sub, "u"))));
            mu = substituteSubexpr(mu, Sym("u"), u);
            tries.push_back({mu, "μ(x+y) = exp(∫ f(u) du), u = x+y"});
        }
    } catch (...) {}
    for (auto& tr : tries) {
        if (!tr.mu) continue;
        Ex M2 = simplify(Mul(tr.mu, M));
        Ex N2 = simplify(Mul(tr.mu, N));
        if (!isZero(Add(simplify(diff(M2, b.y)), Mul(Num(-1), simplify(diff(N2, b.x)))))) continue;
        Ex u0 = integrate(M2, b.x);
        Ex phiP = simplify(Add(N2, Mul(Num(-1), diff(u0, b.y))));
        Ex phi = integrate(phiP, b.y);
        Ex u = simplify(Add(u0, phi));
        b.step("آزمون کامل بودن", d,
               "∂M/∂y = " + toPretty(My) + " و ∂N/∂x = " + toPretty(Nx) +
                   "؛ چون برابر نیستند، معادلهٔ اولیه کامل نیست و باید در یک عامل انتگرال‌ساز ضرب شود.",
               "Exactness test",
               "∂M/∂y = " + toPretty(My) + " and ∂N/∂x = " + toPretty(Nx) +
                   "; they differ, so the original equation is not exact and needs an integrating factor.");
        b.step("عامل انتگرال‌ساز", tr.mu, tr.kind, "Integrating factor", tr.kind);
        b.step("تابع پتانسیل", u,
               "با ضرب در μ معادله کامل می‌شود؛ از جملهٔ نخست نسبت به x انتگرال می‌گیریم (ثابت "
               "φ(y))، سپس با مقایسهٔ ∂u/∂y با جملهٔ دوم φ′(y) = " + toPretty(phiP) +
                   " و φ(y) = " + toPretty(phi) + " را می‌یابیم.",
               "Potential function",
               "Multiplying by μ makes the equation exact; integrating the first term with "
               "respect to x (constant φ(y)) and comparing ∂u/∂y with the second term gives "
               "φ′(y) = " + toPretty(phiP) + " and φ(y) = " + toPretty(phi) + ".");
        b.step("جواب عمومی", u,
               "خانوادهٔ جواب‌ها همان منحنی‌های تراز این تابع است: u(x,y) = C.",
               "General solution",
               "The solution family is the level curves of this potential: u(x,y) = C.");
        Ex ex = tryExplicit(Add(u, Mul(Num(-1), Cst(1))), b.x, b.y);
        setSolution(b.r, "integrating_factor", "کامل‌شدنی با عامل انتگرال‌ساز", u, ex != nullptr, ex);
        b.r.normalized = F;
        verifyImplicit(b.r, u, M2, N2, b.x, b.y, b.en);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------- higher order
static bool extractLinearODE(const Ex& F, const std::string& y, const std::string& x,
                             std::vector<Ex>& a, Ex& rhs) {
    Ex e = expand(F);
    std::vector<Ex> terms = e->isAdd() ? e->args : std::vector<Ex>{e};
    a.clear();
    rhs = Num(0);
    for (auto& t : terms) {
        // find exactly one y-related factor
        std::vector<Ex> facs = t->isMul() ? t->args : std::vector<Ex>{t};
        int order = -1;
        Ex coef = Num(1);
        bool bad = false;
        for (auto& f : facs) {
            if (isDeriv(f) && f->args[0]->isSym(y) && f->args[1]->isSym(x)) {
                if (order != -1) { bad = true; break; }
                order = derivOrder(f);
                if (order < 1 || order > 8) { bad = true; break; }
                continue;
            }
            if (f->isSym(y)) {
                if (order != -1) { bad = true; break; }
                order = 0;
                continue;
            }
            if (!dependsOn(f, y)) { coef = Mul(coef, f); continue; }
            bad = true;
            break;
        }
        if (bad) return false;
        if (order == -1) {
            if (dependsOn(coef, y)) return false;
            rhs = Add(rhs, Mul(Num(-1), coef));
            continue;
        }
        if ((int)a.size() <= order) a.resize(order + 1, Num(0));
        a[order] = Add(a[order], coef);
    }
    if (a.empty()) return false;
    while (a.size() > 1 && a.back()->isZero()) a.pop_back();
    return true;
}

static Ex constCoeffSolution(const std::vector<Ex>& a, const std::string& x, Builder& b,
                              bool emitGeneral = true) {
    int n = (int)a.size() - 1;
    if (n < 1) return nullptr;
    for (auto& c : a) if (!c->isNum()) return nullptr;
    Poly ch;
    for (auto& c : a) ch.c.push_back(c->num);
    // characteristic polynomial in r
    Ex charExpr = polyToExpr(ch, "r");
    std::vector<Factor> facs;
    if (!factorPolynomial(ch, facs)) {
        // fall back: rational roots only
        facs.clear();
    }
    // build roots with multiplicity
    struct Root { Ex re, im; int mult; bool cplx; };
    std::vector<Root> roots;
    for (auto& f : facs) {
        if (f.p.deg() == 1) {
            Rational r = -f.p.c[0] / f.p.c[1];
            roots.push_back({Num(r), Num(0), f.mult, false});
        } else if (f.p.deg() == 2) {
            Rational p = f.p.c[1], q = f.p.c[0];
            Rational disc = p * p - q * Rational(4);
            if (disc.isNeg()) {
                Rational alpha = -p * Rational(1, 2);
                Ex beta = simplify(Mul(Sqrt(Num(-disc)), Num(Rational(1, 2))));
                roots.push_back({Num(alpha), beta, f.mult, true});
            } else {
                Ex s = Sqrt(Num(disc));
                roots.push_back({simplify(Mul(Add(Num(-p), s), Num(Rational(1, 2)))), Num(0), f.mult, false});
                if (!disc.isZero())
                    roots.push_back({simplify(Mul(Add(Num(-p), Mul(Num(-1), s)), Num(Rational(1, 2)))), Num(0), f.mult, false});
            }
        } else {
            return nullptr;
        }
    }
    Ex X = Sym(x);
    std::vector<Ex> terms;
    int ci = 1;
    for (auto& rr : roots) {
        for (int m = 0; m < rr.mult; ++m) {
            Ex polyFactor = Pow(X, Num(m));
            if (rr.cplx) {
                Ex alpha = rr.re, beta = rr.im;
                Ex env = expOf(Mul(alpha, X));
                Ex cs = Mul(Cst(ci++), polyFactor, env, Fn("cos", {Mul(beta, X)}));
                Ex sn = Mul(Cst(ci++), polyFactor, env, Fn("sin", {Mul(beta, X)}));
                terms.push_back(cs);
                terms.push_back(sn);
            } else {
                terms.push_back(Mul(Cst(ci++), polyFactor, expOf(Mul(rr.re, X))));
            }
        }
    }
    if (terms.empty()) return nullptr;
    Ex sol = Add(terms);

    std::string rootFa = "ریشه‌ها: ";
    std::string rootEn = "Roots: ";
    bool firstRoot = true;
    for (auto& rt : roots) {
        Ex term = rt.cplx ? Add({rt.re, Mul(rt.im, Sym("i"))}) : rt.re;
        if (!firstRoot) { rootFa += "، "; rootEn += ", "; }
        firstRoot = false;
        rootFa += "r = " + toPretty(term);
        rootEn += "r = " + toPretty(term);
        if (rt.mult > 1) {
            rootFa += " (تکرار " + std::to_string(rt.mult) + ")";
            rootEn += " (multiplicity " + std::to_string(rt.mult) + ")";
        }
    }
    rootFa += ". ";
    rootEn += ". ";
    if (roots.size() == 1 && roots[0].cplx) {
        rootFa += "این جفت مختلط جواب‌های e^(ax)·cos(bx) و e^(ax)·sin(bx) را می‌دهد.";
        rootEn += "This complex pair yields e^(ax)cos(bx) and e^(ax)sin(bx).";
    } else {
        rootFa += "هر ریشهٔ حقیقی ساده جواب پایهٔ e^(rx) را می‌دهد؛ ریشهٔ تکراری یک عامل x "
                  "به توان تکرار می‌گیرد.";
        rootEn += "Each simple real root gives e^(rx); a repeated root picks up a factor x to "
                  "the power of its multiplicity.";
    }
    b.step("معادلهٔ مشخصه و ریشه‌ها", charExpr, rootFa,
           "Characteristic equation and roots", rootEn);
    if (emitGeneral) b.step("جواب عمومی", sol, "ترکیب خطی توابع پایه با ثابت‌های دلخواه.");
    return sol;
}

// ------------------------------------------------- non-homogeneous const coeff
// A small exact linear solver for the undetermined-coefficient systems. Every
// entry is a rational number, so Gaussian elimination stays exact.
static bool solveRationalSystem(std::vector<std::vector<Ex>> M, std::vector<Ex> rhs,
                                std::vector<Ex>& out) {
    const int m = (int)M.size();
    if (m == 0 || M[0].empty()) return false;
    const int n = (int)M[0].size();
    for (int col = 0; col < n; ++col) {
        int piv = -1;
        for (int r = col; r < m; ++r) {
            if (M[r][col] && !simplify(M[r][col])->isZero()) { piv = r; break; }
        }
        if (piv < 0) return false; // rank deficient ansatz
        if (piv != col) {
            std::swap(M[piv], M[col]);
            std::swap(rhs[piv], rhs[col]);
        }
        Ex pv = simplify(M[col][col]);
        if (pv->isZero()) return false;
        for (int j = col; j < n; ++j) M[col][j] = simplify(Mul(M[col][j], Pow(pv, Num(-1))));
        rhs[col] = simplify(Mul(rhs[col], Pow(pv, Num(-1))));
        for (int r = 0; r < m; ++r) {
            if (r == col) continue;
            Ex f = simplify(M[r][col]);
            if (f->isZero()) continue;
            for (int j = col; j < n; ++j) M[r][j] = simplify(Add(M[r][j], Mul(Num(-1), f, M[col][j])));
            rhs[r] = simplify(Add(rhs[r], Mul(Num(-1), f, rhs[col])));
        }
    }
    for (int r = n; r < m; ++r) {
        if (!simplify(rhs[r])->isZero()) return false; // inconsistent overdetermined system
    }
    out.assign(n, Num(0));
    for (int i = 0; i < n; ++i) out[i] = simplify(rhs[i]);
    return true;
}

static long long binomialSmall(int n, int k) {
    if (k < 0 || k > n) return 0;
    long long r = 1;
    for (int i = 1; i <= k; ++i) r = r * (n - k + i) / i;
    return r;
}

static Ex intPow(const Ex& base, int e) {
    if (e == 0) return Num(1);
    return Pow(base, Num(e));
}

static Ex diffN(const Ex& g, const std::string& x, int n) {
    Ex r = g;
    for (int i = 0; i < n; ++i) r = diff(r, x);
    return r;
}

// L_k[g] where L = sum a_j D^j and y = e^(k x) g. This avoids carrying the
// exponential through every derivative.
static Ex shiftedOperator(const std::vector<Ex>& a, const Ex& k, const Ex& g,
                          const std::string& x) {
    Ex total = Num(0);
    for (int j = 0; j < (int)a.size(); ++j) {
        if (!a[j] || a[j]->isZero()) continue;
        Ex inner = Num(0);
        for (int i = 0; i <= j; ++i) {
            Ex term = Mul(Num(binomialSmall(j, i)), intPow(k, j - i), diffN(g, x, i));
            inner = Add(inner, term);
        }
        total = Add(total, Mul(a[j], inner));
    }
    return simplify(expand(total));
}

// Multiplicity of k + i b as a root of the characteristic polynomial. The
// inputs are rational, so a numeric probe only has to decide which derivative
// is the first non-zero one.
static int characteristicMultiplicity(const std::vector<Ex>& a, const Ex& k, const Ex& b) {
    const int n = (int)a.size() - 1;
    if (n < 1 || !k->isNum() || !b->isNum()) return 0;
    const std::complex<double> z(k->num.toDouble(), b->num.toDouble());
    std::vector<double> c(n + 1, 0.0);
    for (int i = 0; i <= n; ++i) c[i] = a[i]->isNum() ? a[i]->num.toDouble() : 0.0;
    for (int j = 0; j <= n; ++j) {
        std::complex<double> val(0.0, 0.0);
        double scale = 0.0;
        for (int m = j; m <= n; ++m) {
            double fact = 1.0;
            for (int q = 0; q < j; ++q) fact *= (double)(m - q);
            std::complex<double> term = c[m] * fact * std::pow(z, m - j);
            val += term;
            scale += std::abs(term);
        }
        if (std::abs(val) > 1e-8 * (1.0 + scale)) return j;
    }
    return n + 1;
}

// Split a forcing term into e^(k x) * poly(x) * cos/sin(b x). Anything that is
// not of this shape is reported as unsupported so the caller can explain it.
static bool decomposeForcingTerm(const Ex& term, const std::string& x,
                                 Ex& k, Ex& b, int& trig, Ex& poly) {
    Ex t = expand(term);
    std::vector<Ex> facs = t->isMul() ? t->args : std::vector<Ex>{t};
    Rational ksum(0);
    bool haveExp = false, haveTrig = false;
    std::vector<Ex> polyFacs;
    b = Num(0);
    trig = 0;
    for (auto& f : facs) {
        if (f->isFunc("exp") && f->args.size() == 1) {
            Ex arg = expand(f->args[0]);
            std::vector<Ex> ac;
            if (!polyCoeffs(arg, x, ac) || ac.size() > 2) return false;
            Rational c0(0), c1(0);
            if (!ac[0]->isNum()) return false;
            c0 = ac[0]->num;
            if (ac.size() == 2) {
                if (!ac[1]->isNum()) return false;
                c1 = ac[1]->num;
            }
            if (!c0.isZero()) return false; // e^(k x + const) is not supported
            ksum = ksum + c1;
            haveExp = true;
            continue;
        }
        if ((f->isFunc("sin") || f->isFunc("cos")) && f->args.size() == 1) {
            if (haveTrig) return false;
            Ex arg = expand(f->args[0]);
            std::vector<Ex> ac;
            if (!polyCoeffs(arg, x, ac) || ac.size() > 2) return false;
            Rational c0(0), c1(0);
            if (!ac[0]->isNum()) return false;
            c0 = ac[0]->num;
            if (ac.size() == 2) {
                if (!ac[1]->isNum()) return false;
                c1 = ac[1]->num;
            }
            if (!c0.isZero() || c1.isZero()) return false;
            b = Num(c1);
            trig = f->isFunc("cos") ? 1 : 2;
            haveTrig = true;
            continue;
        }
        polyFacs.push_back(f);
    }
    if (haveExp) k = Num(ksum);
    else k = Num(0);
    if (polyFacs.empty()) poly = Num(1);
    else if (polyFacs.size() == 1) poly = polyFacs[0];
    else poly = Mul(polyFacs);
    poly = expand(poly);
    std::vector<Ex> pc;
    if (!polyCoeffs(poly, x, pc)) return false;
    for (auto& c : pc) if (!c->isNum()) return false;
    return true;
}

static bool extractTrigCoeffs(const Ex& h, const std::string& x, const Ex& b, int maxDeg,
                              std::vector<Ex>& cc, std::vector<Ex>& ss) {
    Ex C = Sym("__C"), S = Sym("__S");
    Ex X = Sym(x);
    Ex sub = substituteSubexpr(h, Fn("cos", {Mul(b, X)}), C);
    sub = substituteSubexpr(sub, Fn("sin", {Mul(b, X)}), S);
    sub = expand(sub);
    std::vector<Ex> pc;
    if (!polyCoeffs(sub, x, pc) || (int)pc.size() > maxDeg + 1) return false;
    cc.assign(maxDeg + 1, Num(0));
    ss.assign(maxDeg + 1, Num(0));
    for (int p = 0; p < (int)pc.size(); ++p) {
        Ex e = expand(pc[p]);
        Ex c0 = simplify(substitute(substitute(e, "__C", Num(1)), "__S", Num(0)));
        Ex c1 = simplify(substitute(substitute(e, "__C", Num(0)), "__S", Num(1)));
        if (dependsOn(c0, "__C") || dependsOn(c0, "__S") ||
            dependsOn(c1, "__C") || dependsOn(c1, "__S")) return false;
        cc[p] = c0;
        ss[p] = c1;
    }
    return true;
}

static Ex particularForTerm(const std::vector<Ex>& a, const Ex& k, const Ex& b, int trig,
                            const Ex& poly, const std::string& x, Builder& bld, bool en) {
    std::vector<Ex> pc;
    if (!polyCoeffs(poly, x, pc) || pc.empty()) return nullptr;
    for (auto& c : pc) if (!c->isNum()) return nullptr;
    const int d = (int)pc.size() - 1;
    const int s = characteristicMultiplicity(a, k, b);
    if (s < 0 || s > (int)a.size()) return nullptr;
    const int maxDeg = s + d;
    Ex X = Sym(x);
    Ex trigArg = Mul(b, X);

    const Ex expFactor = k->isZero() ? Num(1) : expOf(Mul(k, X));
    const std::string expLabel = k->isZero() ? "" : "e^(" + toPretty(k) + "·x)*";
    std::vector<Ex> basis;
    std::vector<std::string> labels;
    for (int j = s; j <= s + d; ++j) {
        Ex xj = Pow(X, Num(j));
        if (b->isZero()) {
            basis.push_back(xj);
            labels.push_back(expLabel + "x^" + std::to_string(j));
        } else {
            basis.push_back(Mul(xj, Fn("cos", {trigArg})));
            labels.push_back(expLabel + "x^" + std::to_string(j) + "*cos(" + toPretty(trigArg) + ")");
            basis.push_back(Mul(xj, Fn("sin", {trigArg})));
            labels.push_back(expLabel + "x^" + std::to_string(j) + "*sin(" + toPretty(trigArg) + ")");
        }
    }
    if (basis.empty()) return nullptr;

    const int ncol = (int)basis.size();
    const int nrow = b->isZero() ? (maxDeg + 1) : (2 * (maxDeg + 1));
    std::vector<std::vector<Ex>> M(nrow, std::vector<Ex>(ncol, Num(0)));
    std::vector<Ex> rhs(nrow, Num(0));

    for (int col = 0; col < ncol; ++col) {
        Ex h = shiftedOperator(a, k, basis[col], x);
        if (b->isZero()) {
            std::vector<Ex> hc;
            if (!polyCoeffs(h, x, hc)) return nullptr;
            for (int p = 0; p < (int)hc.size() && p <= maxDeg; ++p) M[p][col] = hc[p];
        } else {
            std::vector<Ex> cc, ss;
            if (!extractTrigCoeffs(h, x, b, maxDeg, cc, ss)) return nullptr;
            for (int p = 0; p <= maxDeg; ++p) {
                M[p][col] = cc[p];
                M[maxDeg + 1 + p][col] = ss[p];
            }
        }
    }
    for (int p = 0; p <= d; ++p) {
        if (b->isZero()) {
            rhs[p] = pc[p];
        } else if (trig == 1) {
            rhs[p] = pc[p];
        } else {
            rhs[maxDeg + 1 + p] = pc[p];
        }
    }

    std::vector<Ex> coef;
    if (!solveRationalSystem(M, rhs, coef)) return nullptr;
    Ex part = Num(0);
    for (int i = 0; i < ncol; ++i) part = Add(part, Mul(coef[i], expFactor, basis[i]));
    part = simplify(expand(part));

    std::string ansatz = "y_p = ";
    for (int i = 0; i < ncol; ++i) {
        if (i) ansatz += " + ";
        ansatz += (i < (int)labels.size() ? labels[i] : "term");
    }
    if (en) {
        bld.step("Forcing term and ansatz", poly,
                 "The forcing term splits into e^(kx), polynomial and sine/cosine pieces. "
                 "Because k + i b has multiplicity " + std::to_string(s) +
                 " as a characteristic root, the ansatz is " + ansatz + ".");
        bld.step("Solve for the coefficients", part,
                 "Substituting the ansatz into L[y] = forcing term and matching like terms "
                 "gives a linear system for the unknown coefficients.");
    } else {
        bld.step("جملهٔ ناهمگن و فرض ضرایب نامعین", poly,
                 "جملهٔ ناهمگن از قطعه‌های e^(kx)، چندجمله‌ای و سینوس/کسینوس ساخته شده است. "
                 "چون k + i b ریشهٔ مشخصه با تکرار " + std::to_string(s) +
                 " است، فرض جواب خصوصی " + ansatz + " است.");
        bld.step("حل ضرایب", part,
                 "با جایگذاری فرض در L[y] = جملهٔ ناهمگن و برابر قرار دادن جمله‌های مشابه، "
                 "یک دستگاه خطی برای ضرایب مجهول به دست می‌آید.");
    }
    return part;
}

static Ex particularSolution(const std::vector<Ex>& a, const Ex& rhs, const std::string& x,
                             Builder& b, bool en) {
    Ex expanded = expand(rhs);
    std::vector<Ex> terms = expanded->isAdd() ? expanded->args : std::vector<Ex>{expanded};
    Ex total = Num(0);
    for (auto& term : terms) {
        if (term->isZero()) continue;
        Ex k, bb, poly;
        int trig = 0;
        if (!decomposeForcingTerm(term, x, k, bb, trig, poly)) return nullptr;
        Ex part = particularForTerm(a, k, bb, trig, poly, x, b, en);
        if (!part) return nullptr;
        total = Add(total, part);
    }
    return simplify(expand(total));
}

static bool solveHigherOrder(Builder& b, const Ex& F) {
    std::vector<Ex> a;
    Ex rhs;
    if (!extractLinearODE(F, b.y, b.x, a, rhs)) return false;
    int n = (int)a.size() - 1;
    if (n < 2) return false;
    b.r.order = n;
    b.r.normalized = F;
    bool constCoef = true;
    for (auto& c : a) if (!c->isNum()) constCoef = false;
    if (!constCoef) {
        b.r.ok = false;
        b.r.typeId = "linear_higher";
        b.r.typeFa = "خطی مرتبهٔ بالا";
        b.fail("این معادلهٔ خطی مرتبهٔ بالا ضرایب ثابت ندارد؛ روشهای ضرایب متغیر در این نسخه پوشش داده نشده است.");
        return true;
    }
    if (n == 2 && a.size() == 3 && a[0]->isZero() && a[1]->isZero() && !rhs->isZero()) {
        Ex f = simplify(Mul(rhs, Pow(a[2], Num(-1))));
        Ex y1 = integrate(f, b.x);
        Ex y = Add(integrate(y1, b.x), Mul(Cst(1), Sym(b.x)), Cst(2));
        b.step("شناسایی نوع", F,
               "در معادله هیچ جمله‌ای شامل y یا y′ نیست و تنها y″ و تابعی از x مانده‌اند؛ پس "
               "می‌توان دو بار مستقیماً انتگرال گرفت.",
               "Type identification",
               "No term contains y or y′, only y″ and a function of x, so the equation can be "
               "integrated twice directly.");
        b.step("انتگرال اول", y1,
               "یک بار انتگرال می‌گیریم تا y′ به دست آید؛ این انتگرال یک ثابت C۱ می‌آورد.",
               "First integration",
               "One integration gives y′; it introduces a first constant C1.");
        b.step("انتگرال دوم", y,
               "بار دوم انتگرال می‌گیریم تا خود y به دست آید؛ این بار یک ثابت دوم اضافه می‌شود.",
               "Second integration",
               "A second integration gives y itself and introduces a second constant.");
        setSolution(b.r, "direct_integration", "انتگرال‌گیری مستقیم مرتبهٔ دوم", simplify(y), true, simplify(y));
        verifyExplicit(b.r, F, simplify(y), b.x, b.y, b.en);
        return true;
    }
    if (!rhs->isZero()) {
        b.step("شناسایی نوع", F,
               "معادلهٔ خطی ناهمگن با ضرایب ثابت از مرتبهٔ " + std::to_string(n) +
               " است؛ جواب عمومی = جواب همگن + یک جواب خصوصی.",
               "Type identification",
               "A non-homogeneous linear equation with constant coefficients of order " +
               std::to_string(n) + "; the general solution is y = y_h + y_p.");
        Ex hom = constCoeffSolution(a, b.x, b, false);
        if (!hom) {
            b.r.ok = false;
            b.r.typeId = "linear_higher";
            b.r.typeFa = "خطی مرتبهٔ بالا";
            b.fail("ریشه‌های مشخصه به‌صورت بسته یافت نشدند.");
            return true;
        }
        b.step("جواب همگن", hom,
               "جواب عمومی معادلهٔ همگن متناظر، که ثابت‌های دلخواه دارد.",
               "Homogeneous solution",
               "The general solution of the corresponding homogeneous equation, carrying "
               "the arbitrary constants.");
        Ex part = particularSolution(a, rhs, b.x, b, b.en);
        if (!part) {
            b.r.ok = false;
            b.r.typeId = "linear_higher";
            b.r.typeFa = "خطی مرتبهٔ بالا (ناهمگن)";
            b.fail("این شکل از جملهٔ ناهمگن پوشش داده نشده است. شکل‌های پشتیبانی‌شده: "
                   "چندجمله‌ای، e^(kx)، sin(bx)، cos(bx) و مجموع یا حاصل‌ضرب آن‌ها با چندجمله‌ای.",
                   "This forcing term is not covered. Supported forms: polynomials, e^(kx), "
                   "sin(bx), cos(bx), and sums or polynomial products of these.");
            return true;
        }
        Ex sol = simplify(Add(hom, part));
        b.step("جواب عمومی", sol,
               "جواب همگن و جواب خصوصی را جمع می‌کنیم؛ این جواب همهٔ معادله را برآورده می‌کند.",
               "General solution",
               "Adding the homogeneous and particular solutions satisfies the full equation.");
        setSolution(b.r, "linear_cc", "خطی ناهمگن با ضرایب ثابت", sol, true, sol);
        verifyExplicit(b.r, F, sol, b.x, b.y, b.en);
        return true;
    }
    b.step("شناسایی نوع", F,
           "معادلهٔ خطی همگن با ضرایب ثابت از مرتبهٔ " + std::to_string(n) + " است.");
    Ex sol = constCoeffSolution(a, b.x, b);
    if (!sol) {
        b.r.ok = false;
        b.r.typeId = "linear_higher";
        b.r.typeFa = "خطی مرتبهٔ بالا";
        b.fail("ریشههای مشخصه بهصورت بسته یافت نشدند.");
        return true;
    }
    setSolution(b.r, "linear_cc", "خطی همگن با ضرایب ثابت", sol, true, sol);
    verifyExplicit(b.r, F, sol, b.x, b.y, b.en);
    return true;
}

// ---------------------------------------------------------------- API
Ex parseODE(const std::string& input, const std::string& x, std::string* dep) {
    return parseODEExpr(input, x, dep, {x, "y", "c"});
}

bool isClosedForm(const Ex& e) { return !hasUnevaluatedIntegral(e); }

std::string typeNameFa(const std::string& id) {
    static const std::map<std::string, std::string> m = {
        {"separable", "جداشدنی"},
        {"homogeneous", "همگن"},
        {"exact", "کامل (دقیق)"},
        {"integrating_factor", "کاملشدنی با عامل انتگرالساز"},
        {"linear", "خطی مرتبهٔ اول"},
        {"bernoulli", "برنولی"},
        {"clairaut", "کلرو"},
        {"lagrange", "لاگرانژ"},
        {"riccati", "ریکاتی"},
        {"linear_sub", "جایگذاری خطی"},
        {"linear_cc", "خطی همگن با ضرایب ثابت"},
        {"linear_higher", "خطی مرتبهٔ بالا"},
        {"wronskian", "رونسکی"},
        {"direct_integration", "انتگرال‌گیری مستقیم"},
        {"quadrature", "انتگرال‌گیری مستقیم"},
        {"derivative", "مشتق"},
        {"derivative_implicit", "مشتق ضمنی"},
        {"integral", "انتگرال نامعین"},
        {"integral_definite", "انتگرال معین"},
        {"limit", "حد"},
        {"continuity", "پیوستگی"},
        {"envelope", "پوش خانوادهٔ منحنیها"},
        {"orthogonal", "مسیرهای قائم"},
        {"oblique", "مسیرهای مایل"},
    };
    auto it = m.find(id);
    return it == m.end() ? id : it->second;
}

std::string typeNameEn(const std::string& id) {
    static const std::map<std::string, std::string> m = {
        {"separable", "Separable"},
        {"homogeneous", "Homogeneous"},
        {"exact", "Exact"},
        {"integrating_factor", "Integrating factor"},
        {"linear", "First-order linear"},
        {"bernoulli", "Bernoulli"},
        {"clairaut", "Clairaut"},
        {"lagrange", "Lagrange"},
        {"riccati", "Riccati"},
        {"linear_sub", "Linear substitution"},
        {"linear_cc", "Linear, constant coefficients"},
        {"linear_higher", "Higher-order linear"},
        {"direct_integration", "Direct integration (2nd order)"},
        {"quadrature", "Direct integration"},
        {"wronskian", "Wronskian"},
        {"envelope", "Envelope of a family"},
        {"orthogonal", "Orthogonal trajectories"},
        {"oblique", "Oblique trajectories"},
        {"integral", "Indefinite integral"},
        {"derivative", "Derivative"}
    };
    auto it = m.find(id);
    return it == m.end() ? id : it->second;
}

Result solveODE(const std::string& input, const OdeOptions& opt) {
    auto t0 = std::chrono::steady_clock::now();
    Result r;
    r.mode = "ode";
    r.engine = "native";
    Builder b(r, opt);
    try {
        std::string dep;
        Ex F = parseODEExpr(input, opt.x, &dep, {opt.x, opt.y, "c"});
        if (!dep.empty() && dep != opt.y && dep != "dx" && dep != "dy") {
            b.y = dep;
            b.Y = Sym(b.y);
            b.P = Deriv(b.Y, b.x, 1);
        }
        r.normalized = F;
        std::vector<std::string> syms;
        collectSymbols(F, syms);
        bool diffForm = false;
        for (auto& s : syms) if (s == "dx" || s == "dy") diffForm = true;

        Ex M, N;
        if (diffForm) {
            if (!extractMN(F, b.x, b.y, M, N)) {
                b.fail("شکل دیفرانسیلی معادله خوانده نشد. نمونه: (x+y)dx + (x+2y)dy = 0",
                       "Could not read the differential form. Example: (x+y)dx + (x+2y)dy = 0");
                r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                return r;
            }
            b.step("شکل دیفرانسیلی", nullptr, "M dx + N dy = 0 با M = " + toPretty(M) + " و N = " + toPretty(N));
            r.order = 1;
            // try exact / IF directly
            if (tryExact(b, F, M, N, true)) {
                r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                return r;
            }
            // try as explicit y' = -M/N
            Ex yp = simplify(Mul(Num(-1), M, Pow(N, Num(-1))));
            std::vector<TermPow> tv;
            powerTerms(yp, b.y, tv);
            if (tryRiccati(b, yp, tv)) {
                r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                return r;
            }
            // Not exact and not Riccati: run the full explicit first-order
            // pipeline on y' = -M/N (separable, homogeneous, linear, ...).
            try {
                Result rr = solveODE("y' = " + toPlain(yp), opt);
                if (rr.ok) {
                    const bool en = opt.lang != "fa";
                    rr.steps.insert(rr.steps.begin(), Step{
                        en ? "Differential form" : "شکل دیفرانسیلی",
                        nullptr,
                        en ? "M dx + N dy = 0 is not exact; rewriting it as y' = -M/N."
                           : "شکل M dx + N dy = 0 کامل نیست؛ به y' = -M/N بازنویسی می‌شود."});
                    rr.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                    rr.normalized = F;
                    return rr;
                }
            } catch (...) {}
            b.fail("این شکل دیفرانسیلی با روشهای کامل/عامل انتگرالساز حل نشد.");
            r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            return r;
        }

        // derivative order
        int maxOrder = 0;
        std::function<void(const Ex&)> scan = [&](const Ex& e) {
            if (!e) return;
            if (isDeriv(e) && e->args[1]->isSym(b.x)) maxOrder = std::max(maxOrder, derivOrder(e));
            for (auto& c : e->args) scan(c);
        };
        scan(F);
        if (maxOrder == 0) {
            b.fail("معادله هیچ مشتقی ندارد. نمونه: y' = x + y",
                   "The equation contains no derivative. Example: y' = x + y");
            r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            return r;
        }
        r.order = maxOrder;

        if (maxOrder >= 2) {
            if (solveHigherOrder(b, F)) {
                r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                return r;
            }
            // y'' = f(x)
            std::vector<Ex> cs;
            if (maxOrder == 2 && polyCoeffs(expand(F), "__p2", cs)) {
                // not applicable; try direct: F is linear in y''
            }
            r.ok = false;
            r.typeFa = "مرتبهٔ بالا";
            b.fail("این معادلهٔ مرتبهٔ بالا در قالبهای پشتیبانیشده نیست.");
            r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            return r;
        }

        // ---- order 1
        Ex p = Deriv(b.Y, b.x, 1);
        if (tryClairautLagrange(b, F, p)) {
            r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            return r;
        }
        // solve for y'
        Ex t = Sym("__p");
        Ex G = substituteSubexpr(F, p, t);
        std::vector<Ex> cs;
        Ex yp;
        if (polyCoeffs(expand(G), "__p", cs) && cs.size() >= 2) {
            if (cs.size() == 2 && !cs[1]->isZero()) {
                yp = simplify(Mul(Num(-1), cs[0], Pow(cs[1], Num(-1))));
            } else if (cs.size() == 3 && !cs[2]->isZero()) {
                // nonlinear in y' (quadratic) — try symbolic quadratic formula
                Ex disc = simplify(Add(Pow(cs[1], Num(2)), Mul(Num(-4), cs[2], cs[0])));
                Ex sq = Sqrt(disc);
                Ex root1 = simplify(Mul(Add(Mul(Num(-1), cs[1]), sq), Num(Rational(1, 2)), Pow(cs[2], Num(-1))));
                Ex root2 = simplify(Mul(Add(Mul(Num(-1), cs[1]), Mul(Num(-1), sq)), Num(Rational(1, 2)), Pow(cs[2], Num(-1))));
                // solve each branch independently (without steps duplication)
                const bool twoBranches = !equals(root1, root2);
                Result best;
                for (auto& br : {root1, root2}) {
                    OdeOptions o2 = opt;
                    o2.steps = opt.steps;
                    Result rr = solveODE("y' = " + toPlain(br), o2);
                    if (rr.ok) { best = rr; break; }
                }
                if (best.ok) {
                    if (twoBranches) {
                        best.warnings.push_back(opt.lang != "fa"
                            ? "The equation is quadratic in y'; only one solution branch is returned."
                            : "معادله نسبت به y′ درجهٔ دوم است؛ تنها یک شاخه از جواب بازگردانده می‌شود.");
                    }
                    best.engine = "native";
                    best.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                    return best;
                }
                yp = nullptr;
            } else {
                yp = nullptr;
            }
        }
        if (!yp) {
            b.fail("معادله نسبت به y′ خطی نیست و در قالبهای پوششدادهشده قرار نمیگیرد.");
            r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            return r;
        }
        r.normalized = F;

        std::vector<TermPow> ts;
        if (!powerTerms(yp, b.y, ts)) ts.clear();
        // An exact equation written as M(x,y) + N(x,y)y′ = 0 should be solved
        // by the potential function, not by the linear integrating factor
        // (which often leaves a needlessly complicated integral).
        if (cs.size() == 2) {
            Ex M0 = cs[0], N0 = cs[1];
            Ex d0 = simplify(Add(simplify(diff(M0, b.y)), Mul(Num(-1), simplify(diff(N0, b.x)))));
            if (d0->isZero() && (dependsOn(M0, b.y) || dependsOn(N0, b.y))) {
                if (tryExact(b, F, M0, N0, false)) {
                    r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                    return r;
                }
            }
        }
        // Linear and separable forms are the most direct; try them first so
        // that e.g. y′ = 1 + y² is solved by separation instead of an awkward
        // Riccati transformation.
        if (!ts.empty() && tryLinear(b, yp, ts)) {
            r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            return r;
        }
        if (trySeparable(b, yp)) {
            r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            return r;
        }
        if (tryHomogeneous(b, yp)) {
            r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            return r;
        }
        if (!ts.empty()) {
            if (tryBernoulli(b, yp, ts)) {
                r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                return r;
            }
            if (tryRiccati(b, yp, ts)) {
                r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                return r;
            }
        }
        if (tryLinearSub(b, yp)) {
            r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            return r;
        }
        // exact / integrating factor from coefficients in y'
        if (cs.size() == 2) {
            Ex M = cs[0], N = cs[1];
            if (tryExact(b, F, M, N, false)) {
                r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                return r;
            }
        }
        // pure quadrature
        if (!dependsOn(yp, b.y)) {
            Ex sol = Add(integrate(yp, b.x), Cst(1));
            b.step("انتگرالگیری مستقیم", sol, "چون y′ تنها تابعی از x است، y = ∫f(x)dx + C.");
            setSolution(b.r, "quadrature", "انتگرالگیری مستقیم", sol, true, sol);
            b.r.normalized = F;
            return b.r;
        }
        {
            Result simplified = trySimplifiedFallback(F, opt);
            if (simplified.ok) {
                simplified.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - t0).count();
                return simplified;
            }
        }
        r.ok = false;
        r.typeFa = "تشخیص‌داده‌نشده";
        b.fail("پس از ساده‌سازی، معادله در هیچ‌یک از قالب‌های پشتیبانی‌شده قرار نگرفت (جداشدنی، همگن، کامل، عامل انتگرال‌ساز، خطی، برنولی، کلرو، لاگرانژ، ریکاتی).",
               "After simplification, the equation did not match any supported form (separable, homogeneous, exact, integrating factor, linear, Bernoulli, Clairaut, Lagrange, Riccati).");
        r.normalized = F;
    } catch (const OverflowError& e) {
        b.fail(std::string("ضرایب عددی بیش از حد بزرگ شدند: ") + e.what(),
               std::string("Numeric coefficients grew too large: ") + localizeError(e.what(), true));
    } catch (const std::exception& e) {
        b.fail(std::string("خطا در تحلیل معادله: ") + e.what(),
               std::string("Equation analysis error: ") + localizeError(e.what(), true));
    }
    r.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    return r;
}

// ---------------------------------------------------------------- Wronskian
static Ex determinant(std::vector<std::vector<Ex>> m) {
    int n = (int)m.size();
    if (n == 1) return m[0][0];
    if (n == 2) return simplify(Add(Mul(m[0][0], m[1][1]), Mul(Num(-1), Mul(m[0][1], m[1][0]))));
    Ex total = Num(0);
    for (int col = 0; col < n; ++col) {
        std::vector<std::vector<Ex>> sub;
        for (int i = 1; i < n; ++i) {
            std::vector<Ex> row;
            for (int j = 0; j < n; ++j) if (j != col) row.push_back(m[i][j]);
            sub.push_back(row);
        }
        Ex cof = Mul(m[0][col], determinant(sub));
        if (col % 2) cof = Mul(Num(-1), cof);
        total = Add(total, cof);
    }
    return simplify(total);
}

Result solveWronskian(const std::vector<std::string>& functions, const std::string& x,
                      const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "wronskian";
    r.typeId = "wronskian";
    r.typeFa = "رونسکی و استقلال خطی";
    try {
        if (functions.empty()) {
            r.error = en ? "At least one function is required." : "حداقل یک تابع لازم است.";
            return r;
        }
        if (functions.size() > 6) {
            r.error = "حداکثر ۶ تابع پشتیبانی میشود.";
            return r;
        }
        Ex X = Sym(x);
        std::vector<Ex> fs;
        for (auto& s : functions) {
            std::string t = trim(s);
            if (t.empty()) continue;
            fs.push_back(parseExpr(t));
        }
        if (fs.empty()) {
            r.error = en ? "No valid function was entered." : "تابع معتبری وارد نشد.";
            return r;
        }
        int n = (int)fs.size();
        std::vector<std::vector<Ex>> M(n, std::vector<Ex>(n));
        for (int j = 0; j < n; ++j) {
            Ex cur = fs[j];
            for (int i = 0; i < n; ++i) {
                M[i][j] = (i == 0) ? cur : simplify(diff(cur, x));
                cur = M[i][j];
            }
        }
        for (int j = 0; j < n; ++j) {
            for (int i = 1; i < n; ++i) M[i][j] = simplify(M[i][j]);
        }
        Ex W = simplify(expand(determinant(M)));
        r.wronskian = W;
        r.functions = functions;
        r.order = n - 1;
        Ex expansion = nullptr;
        if (n == 1) {
            expansion = M[0][0];
        } else if (n == 2) {
            expansion = Add(Mul(M[0][0], M[1][1]),
                            Mul(Num(-1), Mul(M[0][1], M[1][0])));
        } else if (n == 3) {
            std::vector<Ex> terms;
            for (int col = 0; col < 3; ++col) {
                std::vector<std::vector<Ex>> sub;
                for (int i = 1; i < 3; ++i) {
                    std::vector<Ex> row;
                    for (int j = 0; j < 3; ++j) if (j != col) row.push_back(M[i][j]);
                    sub.push_back(row);
                }
                Ex minor = determinant(sub);
                Ex term = Mul(M[0][col], minor);
                if (col % 2) term = Mul(Num(-1), term);
                terms.push_back(term);
            }
            expansion = Add(terms);
        }
        if (expansion) expansion = simplify(expand(expansion));

        std::string matrixFa = "ستون‌ها: ";
        std::string matrixEn = "Columns: ";
        for (int j = 0; j < n; ++j) {
            if (j) { matrixFa += "، "; matrixEn += ", "; }
            matrixFa += toPretty(fs[j]);
            matrixEn += toPretty(fs[j]);
        }
        matrixFa += ". سطرها: ";
        matrixEn += ". Rows: ";
        for (int i = 0; i < n; ++i) {
            matrixFa += "[";
            matrixEn += "[";
            for (int j = 0; j < n; ++j) {
                if (j) { matrixFa += ", "; matrixEn += ", "; }
                matrixFa += toPretty(M[i][j]);
                matrixEn += toPretty(M[i][j]);
            }
            matrixFa += "] ";
            matrixEn += "] ";
        }
        r.steps.push_back({en ? "Build the Wronskian matrix" : "ساخت ماتریس رونسکی",
                           nullptr, en ? matrixEn : matrixFa});
        r.steps.push_back({en ? "Expand the determinant" : "بسط دترمینان", expansion,
                           en ? "Cofactor expansion along the first row."
                              : "بسط هم‌عامل در امتداد سطر اول."});
        r.steps.push_back({en ? "Wronskian determinant" : "دترمینان رونسکی", W,
                           "W = det[ f_i^(j) ]."});
        // Only a symbolic zero proves linear dependence.  A numerically tiny
        // W can come from small coefficients (e.g. 1e-9*x), so it is reported
        // as a caveat rather than asserted as a proof.
        const bool zero = W->isZero();
        if (!zero && numericallyZero(W, {x}, 8, 1e-7)) {
            r.warnings.push_back(en
                ? "W is numerically zero at the sampled points but was not proven identically zero; the functions are treated as independent."
                : "W در نقاط نمونه‌برداری‌شده به‌صورت عددی صفر است اما صفر بودن یکسان آن اثبات نشد؛ توابع مستقل در نظر گرفته می‌شوند.");
        }
        r.independent = !zero;
        // Cross-check the symbolic determinant against a direct numeric one.
        bool numericCheck = true;
        int checked = 0;
        for (double xv : {0.37, 1.13, 2.71}) {
            std::vector<std::vector<double>> dm(n, std::vector<double>(n, 0.0));
            bool ok = true;
            for (int i = 0; i < n && ok; ++i) {
                for (int j = 0; j < n; ++j) {
                    std::map<std::string, double> vars;
                    vars[x] = xv;
                    bool o = false;
                    dm[i][j] = evalReal(M[i][j], vars, &o);
                    if (!o || !std::isfinite(dm[i][j])) { ok = false; break; }
                }
            }
            if (!ok) continue;
            double det = 1.0;
            for (int col = 0; col < n; ++col) {
                int piv = col;
                for (int row = col + 1; row < n; ++row)
                    if (std::fabs(dm[row][col]) > std::fabs(dm[piv][col])) piv = row;
                if (std::fabs(dm[piv][col]) < 1e-12) { det = 0.0; break; }
                if (piv != col) { std::swap(dm[piv], dm[col]); det = -det; }
                det *= dm[col][col];
                for (int row = col + 1; row < n; ++row) {
                    const double f = dm[row][col] / dm[col][col];
                    for (int k = col; k < n; ++k) dm[row][k] -= f * dm[col][k];
                }
            }
            std::map<std::string, double> vars;
            vars[x] = xv;
            bool okW = false;
            const double wv = evalReal(W, vars, &okW);
            if (!okW || !std::isfinite(wv) ||
                std::fabs(wv - det) > 1e-6 * (1.0 + std::fabs(det))) {
                numericCheck = false;
            }
            ++checked;
        }
        if (checked > 0) {
            r.checks.push_back({en ? "Numeric determinant cross-check"
                                   : "بررسی عددی دترمینان",
                                numericCheck,
                                en ? "The symbolic W matches the numeric determinant at sampled points."
                                   : "W نمادین در نقاط نمونه با دترمینان عددی مطابقت دارد."});
        }
        r.steps.push_back({en ? "Conclusion" : "نتیجه", W,
                           en ? (zero
                                 ? "W ≡ 0, so the functions are linearly dependent (or the set has a degenerate point) and cannot form a fundamental set."
                                 : "W ≢ 0, so the functions are linearly independent on any interval where W ≠ 0 and form a fundamental set there.")
                              : (zero
                                 ? "W ≡ 0 است؛ پس توابع وابستهٔ خطی‌اند (یا مجموعه نقطهٔ تبهگن دارد) و مجموعهٔ پایه نمی‌سازند."
                                 : "W ≢ 0 است؛ پس توابع روی هر بازه‌ای که W ≠ 0 باشد مستقل خطی‌اند و مجموعهٔ پایه می‌سازند.")});
        r.assumptions.push_back(en ? "Linear independence is checked on an interval where the functions and W are defined and continuous."
                                   : "استقلال خطی روی بازه‌ای بررسی می‌شود که توابع و W در آن تعریف‌شده و پیوسته باشند.");
        r.checks.push_back({en ? "W ≢ 0" : "W ≢ 0", !zero,
                            en ? (zero ? "The determinant is zero." : "The determinant is not zero.")
                               : (zero ? "دترمینان صفر شد." : "دترمینان صفر نیست.")});
        r.solution = W;
        r.ok = true;
    } catch (const std::exception& e) {
        r.error = (en ? std::string("Wronskian computation error: ") : std::string("خطا در محاسبهٔ رونسکی: ")) +
                  localizeError(e.what(), en);
    }
    return r;
}

// ---------------------------------------------------------------- envelope / family
Result solveEnvelope(const std::string& input, const std::string& c, bool orthogonal,
                     const std::string& x, const std::string& y, const std::string& lang) {
    const bool en = lang != "fa";
    Result r;
    r.mode = "envelope";
    r.typeId = "envelope";
    r.typeFa = "پوش خانوادهٔ منحنیها";
    try {
        std::string dep;
        Ex F = parseODEExpr(input, x, &dep, {x, y, c});
        r.family = input;
        r.normalized = F;
        Ex Fc = simplify(diff(F, c));
        r.steps.push_back({en ? "Family of curves" : "خانوادهٔ منحنی‌ها", F,
                           en ? "F(x,y,c) = 0, where c is the parameter."
                              : "F(x,y,c) = 0 که c پارامتر خانواده است."});
        std::vector<Ex> cvals;
        Ex lin = solveLinear(Fc, c);
        if (lin) {
            cvals.push_back(lin);
        } else {
            auto roots = solvePoly(Fc, c);
            for (auto& rr : roots) cvals.push_back(rr);
        }
        if (cvals.empty()) {
            // resultant-free fallback: solve linear in c if possible
            std::vector<Ex> cs;
            if (polyCoeffs(expand(Fc), c, cs) && cs.size() == 2)
                cvals.push_back(simplify(Mul(Num(-1), cs[0], Pow(cs[1], Num(-1)))));
        }
        if (cvals.empty()) {
            // No envelope candidate.  The family can still be meaningful, and
            // orthogonal trajectories are computed from F itself below.
            r.warnings.push_back(en
                ? "c could not be eliminated from ∂F/∂c; the envelope was not computed."
                : "حذف پارامتر c از شرط ∂F/∂c ممکن نشد؛ پوش محاسبه نشد.");
        }
        for (auto& cv : cvals) {
            // If ∂F/∂c = 0 pins c to a value independent of x and y, the
            // resulting curve is just one member of the family (degenerate),
            // never an envelope.
            if (!dependsOn(cv, x) && !dependsOn(cv, y)) {
                r.warnings.push_back(en
                    ? ("c = " + toPretty(cv) + " is independent of x and y; the curve is a family member, not an envelope.")
                    : ("مقدار c = " + toPretty(cv) + " مستقل از x و y است؛ منحنی حاصل عضو خانواده است، نه پوش."));
                continue;
            }
            Ex curve = simplify(substitute(F, c, cv));
            // degenerate point (e.g. c=0 in concentric circles) is not an envelope
            // A curve that depends on at least one of x/y is a valid envelope;
            // horizontal or vertical envelopes (e.g. y^2 - 1 = 0) are allowed.
            if (curve->isZero() || (!dependsOn(curve, x) && !dependsOn(curve, y))) {
                r.warnings.push_back(en
                    ? ("c = " + toPretty(cv) + " gives a degenerate curve, not an envelope.")
                    : ("مقدار c = " + toPretty(cv) + " منحنی تبهگن می‌دهد و پوش نیست."));
                continue;
            }
            r.envelope.push_back(curve);
            r.steps.push_back({en ? "Eliminate the parameter c" : "حذف پارامتر c", curve,
                               en ? ("The tangency condition ∂F/∂c = 0 gives c = " + toPretty(cv) +
                                     "; substituting it into F gives this candidate envelope.")
                                  : ("شرط مماس بودن ∂F/∂c = 0 مقدار c = " + toPretty(cv) +
                                     " را می‌دهد؛ با جایگذاری آن در F، این نامزد پوش به دست می‌آید.")});
            // The envelope must be tangent to every member of the family. Compare
            // the implicit slopes of the candidate and of the family at c = c(x,y).
            {
                Ex Fx = simplify(substitute(diff(F, x), c, cv));
                Ex Fy = simplify(substitute(diff(F, y), c, cv));
                Ex Cx = simplify(diff(curve, x));
                Ex Cy = simplify(diff(curve, y));
                bool tangencyKnown = false;
                bool tangent = false;
                if (!Fy->isZero() && !Cy->isZero()) {
                    Ex sF = simplify(Mul(Num(-1), Fx, Pow(Fy, Num(-1))));
                    Ex sC = simplify(Mul(Num(-1), Cx, Pow(Cy, Num(-1))));
                    sF = simplify(substitute(sF, c, cv));
                    Ex gap = simplify(Add({sF, Mul(Num(-1), sC)}));
                    tangent = gap->isZero() || numericallyZero(gap, {x, y}, 8, 1e-7);
                    tangencyKnown = true;
                } else if (Fy->isZero() && Cy->isZero()) {
                    tangent = true; // both tangents are vertical
                    tangencyKnown = true;
                }
                if (tangencyKnown) {
                    r.checks.push_back({en ? "Envelope is tangent to the family"
                                           : "پوش بر خانواده مماس است",
                                        tangent,
                                        en ? "The implicit slopes of the envelope and of the family agree at the contact point."
                                           : "شیب ضمنی پوش و خانواده در نقطهٔ تماس یکسان است."});
                    r.steps.push_back({en ? "Tangency check" : "بررسی مماس بودن", nullptr,
                                       en ? (tangent ? "The candidate is tangent to every family member, so it is an envelope."
                                                     : "The candidate is not tangent to the family; it is not a valid envelope.")
                                          : (tangent ? "نامزد بر هر عضو خانواده مماس است، پس پوش معتبر است."
                                                     : "نامزد بر خانواده مماس نیست و پوش معتبری نیست.")});
                }
            }
        }
        r.assumptions.push_back(en
            ? "The envelope is defined where F and F_c are smooth and eliminating c is valid."
            : "پوش در نقاطی که F و F_c هموارند و حذف c معتبر است، تعریف می‌شود.");
        r.solution = r.envelope.empty() ? nullptr : r.envelope[0];
        r.isExplicit = false;
        r.ok = true;
        if (r.envelope.empty())
            r.steps.push_back({en ? "Conclusion" : "نتیجه", nullptr,
                               en ? "This family has no envelope: ∂F/∂c = 0 gives only a family member (or a degenerate curve)."
                                  : "این خانواده پوش ندارد: شرط ∂F/∂c = 0 فقط یک عضو خانواده (یا یک منحنی تبهگن) می‌دهد."});
        r.closedForm = true;
        if (orthogonal) {
            // slope of family: y' = -F_x/F_y (with c eliminated if possible)
            Ex Fx = simplify(diff(F, x));
            Ex Fy = simplify(diff(F, y));
            if (Fy->isZero()) {
                r.warnings.push_back("شیب خانواده در این حالت تعریفنشده است؛ مسیرهای قائم محاسبه نشد.");
            } else {
                Ex slope = simplify(Mul(Num(-1), Fx, Pow(Fy, Num(-1))));
                Ex cElim = nullptr;
                Ex clin = solveLinear(F, c);
                if (clin) cElim = clin;
                else {
                    auto rts = solvePoly(F, c);
                    if (!rts.empty()) cElim = rts[0];
                }
                if (cElim) slope = simplify(substitute(slope, c, cElim));
                if (dependsOn(slope, c)) {
                    r.warnings.push_back("حذف c از شیب خانواده ممکن نشد؛ مسیرهای قائم تنها بهصورت معادلهٔ دیفرانسیل ارائه میشود.");
                    r.steps.push_back({"معادلهٔ مسیرهای قائم", nullptr,
                                       "y′ = −1 / (" + toPretty(slope) + ")"});
                } else {
                    Ex ortho = simplify(Mul(Num(-1), Pow(slope, Num(-1))));
                    r.steps.push_back({en ? "Family slope" : "شیب خانواده", slope});
                    r.steps.push_back({en ? "Orthogonal-trajectory equation" : "معادلهٔ مسیرهای قائم", nullptr,
                                       "y′ = " + toPretty(ortho)});
                    OdeOptions o; o.x = x; o.y = y; o.lang = lang;
                    Result rr = solveODE("y' = " + toPlain(ortho), o);
                    if (rr.ok) {
                        r.orthogonal = rr.isExplicit && rr.explicitY ? rr.explicitY : rr.solution;
                        r.orthogonalExplicit = rr.isExplicit;
                        r.steps.push_back({en ? "Solve the orthogonal trajectories" : "حل مسیرهای قائم", rr.solution,
                                           en ? (rr.isExplicit ? "Family of orthogonal trajectories."
                                                               : "Implicit family of orthogonal trajectories.")
                                              : (rr.isExplicit ? "خانوادهٔ مسیرهای قائم."
                                                               : "خانوادهٔ ضمنی مسیرهای قائم.")});
                        r.assumptions.insert(r.assumptions.end(), rr.assumptions.begin(), rr.assumptions.end());
                        r.checks.insert(r.checks.end(), rr.checks.begin(), rr.checks.end());
                    } else {
                        r.warnings.push_back("مسیرهای قائم با روشهای موجود حل نشد.");
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        r.error = en ? std::string("Envelope analysis error: ") + localizeError(e.what(), true)
                     : std::string("خطا در تحلیل خانوادهٔ منحنیها: ") + e.what();
    }
    return r;
}

// ---------------------------------------------------------------- orthogonal
// Solve F(x,y,c) = 0 for a variable when F is at most quadratic in it. The
// coefficients may depend on the other symbols, which covers families such as
// (y-c)^2 = x and x^2 + y^2 = c^2 that the rational-root solver rejects because
// its coefficients are not numeric.
static std::vector<Ex> quadraticBranches(const Ex& F, const std::string& var) {
    std::vector<Ex> out;
    std::vector<Ex> a;
    if (!polyCoeffs(expand(F), var, a)) return out;
    while (a.size() > 1 && a.back()->isZero()) a.pop_back();
    if (a.size() == 2 && !a[1]->isZero()) {
        out.push_back(simplify(Mul(Num(-1), a[0], Pow(a[1], Num(-1)))));
        return out;
    }
    if (a.size() == 3 && !a[2]->isZero()) {
        Ex disc = simplify(Add(Pow(a[1], Num(2)), Mul(Num(-4), a[2], a[0])));
        Ex sq = Sqrt(disc);
        Ex den = Pow(Mul(Num(2), a[2]), Num(-1));
        Ex r1 = simplify(Mul(Add(Mul(Num(-1), a[1]), sq), den));
        Ex r2 = simplify(Mul(Add(Mul(Num(-1), a[1]), Mul(Num(-1), sq)), den));
        out.push_back(r1);
        if (!equals(r1, r2)) out.push_back(r2);
    }
    return out;
}

// ---------------------------------------------------------------- trajectories
// tan(alpha) as an exact expression for the common angles and a bounded
// rational approximation otherwise.  Exact 30/45/60 keep the derivation
// readable; arbitrary angles still solve numerically through the same engine.
static Ex tanDegrees(double angleDeg) {
    const double rad = angleDeg * 3.14159265358979323846 / 180.0;
    const double t = std::tan(rad);
    if (std::fabs(t) < 1e-14) return Num(0);
    if (std::fabs(t - 1.0) < 1e-12) return Num(1);
    if (std::fabs(t + 1.0) < 1e-12) return Num(-1);
    const double s3 = std::sqrt(3.0);
    if (std::fabs(t - s3) < 1e-10) return Sqrt(Num(3));
    if (std::fabs(t - 1.0 / s3) < 1e-10) {
        return simplify(Mul(Sqrt(Num(3)), Pow(Num(3), Num(-1))));
    }
    return Num(Rational::fromDouble(t, 1000));
}

// Slope of the trajectory family from the family slope m.  A 90° crossing is
// the orthogonal case y′ = −1/m; otherwise tan α = (m_t − m)/(1 + m_t m)
// gives m_t = (m + tan α)/(1 − m tan α).
static Ex trajectorySlopeFrom(const Ex& familySlope, double angleDeg, bool orthogonal,
                              bool& vertical) {
    vertical = false;
    const bool rightAngle = orthogonal || std::fabs(angleDeg - 90.0) < 1e-9;
    if (rightAngle) {
        if (familySlope->isZero()) {
            vertical = true;
            return nullptr;
        }
        return simplify(Mul(Num(-1), Pow(familySlope, Num(-1))));
    }
    if (std::fabs(angleDeg) < 1e-9) return familySlope;
    Ex t = tanDegrees(angleDeg);
    Ex denom = simplify(Add(Num(1), Mul(Num(-1), familySlope, t)));
    if (denom->isZero()) {
        vertical = true;
        return nullptr;
    }
    return simplify(Mul(Add(familySlope, t), Pow(denom, Num(-1))));
}

static Result solveTrajectory(const std::string& input, const std::string& c,
                              double angleDeg, const std::string& x,
                              const std::string& y, const std::string& lang,
                              bool orthogonal) {
    const bool en = lang != "fa";
    const bool rightAngle = orthogonal || std::fabs(angleDeg - 90.0) < 1e-9;
    Result r;
    r.mode = orthogonal ? "orthogonal" : "oblique";
    r.typeId = r.mode;
    r.typeFa = orthogonal ? "مسیرهای قائم" : "مسیرهای مایل";
    r.trajectoryMode = r.mode;
    r.trajectoryAngle = rightAngle ? 90.0 : angleDeg;
    r.family = input;

    const std::string eqTitle = en
        ? (orthogonal ? "Orthogonal-trajectory equation" : "Oblique-trajectory equation")
        : (orthogonal ? "معادلهٔ مسیرهای قائم" : "معادلهٔ مسیرهای مایل");
    const std::string familyTitle = en
        ? (orthogonal ? "Orthogonal family" : "Oblique family")
        : (orthogonal ? "خانوادهٔ قائم" : "خانوادهٔ مایل");
    const std::string trajTitle = en
        ? (orthogonal ? "Orthogonal trajectories" : "Oblique trajectories")
        : (orthogonal ? "مسیرهای قائم" : "مسیرهای مایل");
    auto familyNote = [&](bool explicitY) {
        if (en) {
            if (orthogonal)
                return explicitY ? std::string("Explicit orthogonal trajectories.")
                                 : std::string("Implicit orthogonal trajectories.");
            return explicitY ? std::string("Explicit oblique trajectories.")
                             : std::string("Implicit oblique trajectories.");
        }
        if (orthogonal)
            return explicitY ? std::string("مسیرهای قائم صریح.")
                             : std::string("مسیرهای قائم ضمنی.");
        return explicitY ? std::string("مسیرهای مایل صریح.")
                         : std::string("مسیرهای مایل ضمنی.");
    };
    auto publish = [&]() {
        if (rightAngle) {
            r.orthogonal = r.trajectory;
            r.orthogonalExplicit = r.trajectoryExplicit;
            r.orthogonalSlope = r.trajectorySlope;
        }
    };
    auto finishWith = [&](const Ex& traj, bool explicitY, const Ex& slope) {
        r.trajectory = traj;
        r.trajectoryExplicit = explicitY;
        r.trajectorySlope = slope;
        r.solution = traj;
        r.ok = true;
        r.closedForm = true;
        publish();
    };

    try {
        std::string dep;
        Ex F = parseODEExpr(input, x, &dep, {x, y, c});
        r.normalized = F;
        // Explicit y(x,c) branches let the UI overlay the original family on
        // the trajectory direction field. Linear and quadratic families are
        // covered; a vertical family has no y-branch and is simply skipped.
        r.familyBranches = quadraticBranches(F, y);
        Ex Fx = simplify(diff(F, x));
        Ex Fy = simplify(diff(F, y));

        if (Fy->isZero()) {
            if (Fx->isZero()) {
                r.error = en ? "The family is degenerate: F does not depend on x or y."
                             : "خانواده تبهگن است: F به x یا y بستگی ندارد.";
                return r;
            }
            r.steps.push_back({en ? "Vertical family" : "خانوادهٔ قائم", F,
                               en ? "F_y = 0, so every member is a vertical line and the family has no finite slope y′."
                                  : "F_y = 0 است، پس هر عضو یک خط قائم است و شیب متناهی y′ ندارد."});
            if (rightAngle) {
                r.steps.push_back({trajTitle, Sym(y),
                                   en ? "A vertical line is perpendicular to a horizontal one, so the trajectory family is y = C."
                                      : "خط قائم بر خط افقی عمود است، پس خانوادهٔ مسیرها y = C است."});
                finishWith(Sym(y), false, Num(0));
                return r;
            }
            if (std::fabs(angleDeg) < 1e-9) {
                r.steps.push_back({trajTitle, Sym(x),
                                   en ? "A zero crossing angle leaves the vertical family unchanged: x = C."
                                      : "زاویهٔ صفر خانوادهٔ قائم را بدون تغییر می‌گذارد: x = C."});
                finishWith(Sym(x), false, nullptr);
                return r;
            }
            Ex cot = simplify(Pow(tanDegrees(angleDeg), Num(-1)));
            r.steps.push_back({eqTitle, Add({Deriv(Sym(y), x, 1), Mul(Num(-1), cot)}),
                               en ? "The family is vertical; a curve crossing it at angle α has slope cot α."
                                  : "خانواده قائم است؛ منحنی‌ای که با زاویهٔ α آن را قطع می‌کند شیب cot α دارد."});
            OdeOptions o;
            o.x = x;
            o.y = y;
            o.lang = lang;
            Result rr = solveODE("y' = " + toPlain(cot), o);
            if (!rr.ok) {
                r.warnings.push_back(en ? "The oblique-trajectory ODE could not be solved by the available methods."
                                        : "معادلهٔ دیفرانسیل مسیرهای مایل با روش‌های موجود حل نشد.");
                r.ok = true;
                r.closedForm = false;
                return r;
            }
            bool renamedFinal = false;
            if (!rr.steps.empty()) {
                Step& last = rr.steps.back();
                if (last.title == "General solution" || last.title == "جواب عمومی") {
                    last.title = familyTitle;
                    if (!last.note.empty()) last.note += " ";
                    last.note += familyNote(rr.isExplicit);
                    renamedFinal = true;
                }
            }
            for (auto& s : rr.steps) r.steps.push_back(s);
            r.assumptions.insert(r.assumptions.end(), rr.assumptions.begin(), rr.assumptions.end());
            r.checks.insert(r.checks.end(), rr.checks.begin(), rr.checks.end());
            Ex traj = rr.isExplicit && rr.explicitY ? rr.explicitY : rr.solution;
            if (!renamedFinal) r.steps.push_back({familyTitle, traj, familyNote(rr.isExplicit)});
            r.trajectory = traj;
            r.trajectoryExplicit = rr.isExplicit;
            r.trajectorySlope = cot;
            r.solution = traj;
            r.closedForm = rr.closedForm;
            r.ok = true;
            publish();
            return r;
        }

        Ex slope = simplify(Mul(Num(-1), Fx, Pow(Fy, Num(-1))));

        Ex cElim = nullptr;
        if (dependsOn(slope, c)) {
            std::vector<Ex> branches = quadraticBranches(F, c);
            if (branches.empty()) {
                Ex clin = solveLinear(F, c);
                if (clin) branches.push_back(clin);
            }
            // Keep only branches that actually remove c from the slope. A branch
            // that leaves c behind cannot describe a single trajectory family.
            std::vector<Ex> usable;
            for (auto& br : branches) {
                Ex cand = simplify(substitute(slope, c, br));
                if (!dependsOn(cand, c)) usable.push_back(br);
            }
            if (usable.empty()) {
                r.steps.push_back({eqTitle, nullptr,
                                   orthogonal
                                       ? "y′ = -1 / (" + toPretty(slope) + ")"
                                       : "y′ = (" + toPretty(slope) +
                                             " + tan α) / (1 - " + toPretty(slope) +
                                             "·tan α)"});
                r.warnings.push_back(en
                    ? "c could not be eliminated from the family slope; the trajectory equation still contains c."
                    : "حذف c از شیب خانواده ممکن نشد؛ معادلهٔ مسیرها هنوز c دارد.");
                r.ok = true;
                r.closedForm = false;
                return r;
            }
            if (usable.size() > 1) {
                r.warnings.push_back(en
                    ? "The family equation has several c-branches; the first one that removes c is used."
                    : "معادلهٔ خانواده چند شاخه برای c دارد؛ نخستین شاخه‌ای که c را حذف می‌کند استفاده شد.");
            }
            cElim = usable.front();
            slope = simplify(substitute(slope, c, cElim));
        }

        if (dependsOn(slope, c)) {
            r.steps.push_back({eqTitle, nullptr,
                               orthogonal
                                   ? "y′ = -1 / (" + toPretty(slope) + ")"
                                   : "y′ = (" + toPretty(slope) +
                                         " + tan α) / (1 - " + toPretty(slope) +
                                         "·tan α)"});
            r.warnings.push_back(en
                ? "The trajectory equation still contains c and was not solved as one family."
                : "معادلهٔ مسیرها هنوز c دارد و به‌صورت یک خانوادهٔ واحد حل نشد.");
            r.ok = true;
            r.closedForm = false;
            return r;
        }

        std::string elimNote;
        if (cElim) {
            elimNote = en
                ? "Form the system F = 0 and F_x + F_y·y′ = 0. From F = 0, c = " +
                      toPretty(cElim) +
                      "; substituting this into the derivative equation removes c and gives the "
                      "family ODE."
                : "دستگاه F = 0 و F_x + F_y·y′ = 0 را تشکیل می‌دهیم. از F = 0 داریم c = " +
                      toPretty(cElim) +
                      "؛ با جایگذاری آن در معادلهٔ مشتق، c حذف می‌شود و معادلهٔ دیفرانسیل "
                      "خانواده به دست می‌آید.";
        } else {
            elimNote = en
                ? "Form the system F = 0 and F_x + F_y·y′ = 0. Differentiating and simplifying "
                  "removes c, so the derivative equation is already the family ODE."
                : "دستگاه F = 0 و F_x + F_y·y′ = 0 را تشکیل می‌دهیم. مشتق‌گیری و ساده‌سازی، c "
                  "را حذف می‌کند و معادلهٔ دیفرانسیل خانواده به دست می‌آید.";
        }
        r.steps.push_back({en ? "Eliminate c and form the family ODE"
                              : "حذف c و تشکیل معادلهٔ دیفرانسیل خانواده",
                           Add({Deriv(Sym(y), x, 1), Mul(Num(-1), slope)}), elimNote});

        if (slope->isZero()) {
            if (rightAngle) {
                r.steps.push_back({trajTitle, Sym(x),
                                   en ? "The family slope is 0, so the family is horizontal and the "
                                         "trajectory family is the vertical lines x = C."
                                      : "شیب خانواده صفر است، پس خانواده افقی است و مسیرها "
                                        "خطوط قائم x = C هستند."});
                finishWith(Sym(x), false, nullptr);
                return r;
            }
            if (std::fabs(angleDeg) < 1e-9) {
                r.steps.push_back({trajTitle, Sym(y),
                                   en ? "A zero crossing angle leaves the horizontal family unchanged: y = C."
                                      : "زاویهٔ صفر خانوادهٔ افقی را بدون تغییر می‌گذارد: y = C."});
                finishWith(Sym(y), false, Num(0));
                return r;
            }
        }

        bool vertical = false;
        Ex trajSlope = trajectorySlopeFrom(slope, angleDeg, orthogonal, vertical);
        if (vertical) {
            r.steps.push_back({trajTitle, Sym(x),
                               en ? "The trajectory slope is infinite, so the family consists of the vertical lines x = C."
                                  : "شیب مسیرها نامتناهی است، پس خانواده از خطوط قائم x = C تشکیل می‌شود."});
            finishWith(Sym(x), false, nullptr);
            return r;
        }
        r.trajectorySlope = trajSlope;

        std::string angleNote;
        if (orthogonal) {
            angleNote = en
                ? "Perpendicular slopes multiply to -1, so y′_traj = -1/y′_family. "
                  "Substituting the family slope gives this first-order equation."
                : "حاصل‌ضرب شیب دو خط عمود -1 است، پس y′_traj = -1/y′_family. با "
                  "جایگذاری شیب خانواده، این معادلهٔ مرتبهٔ اول به دست می‌آید.";
        } else {
            const std::string t = toPretty(tanDegrees(angleDeg));
            angleNote = en
                ? "The crossing angle α satisfies tan α = (m_t − m)/(1 + m_t m), so "
                  "m_t = (m + tan α)/(1 − m tan α) with tan α = " + t +
                  ". Substituting the family slope gives this first-order equation."
                : "زاویهٔ تقاطع α در رابطهٔ tan α = (m_t − m)/(1 + m_t m) صدق می‌کند، پس "
                  "m_t = (m + tan α)/(1 − m tan α) با tan α = " + t +
                  ". با جایگذاری شیب خانواده، این معادلهٔ مرتبهٔ اول به دست می‌آید.";
        }
        r.steps.push_back({eqTitle, Add({Deriv(Sym(y), x, 1), Mul(Num(-1), trajSlope)}),
                           angleNote});

        OdeOptions o;
        o.x = x;
        o.y = y;
        o.lang = lang;
        Result rr = solveODE("y' = " + toPlain(trajSlope), o);
        if (!rr.ok) {
            r.warnings.push_back(en
                ? "The trajectory ODE could not be solved by the available methods."
                : "معادلهٔ دیفرانسیل مسیرها با روش‌های موجود حل نشد.");
            r.ok = true;
            r.closedForm = false;
            return r;
        }
        bool renamedFinal = false;
        if (!rr.steps.empty()) {
            Step& last = rr.steps.back();
            if (last.title == "General solution" || last.title == "جواب عمومی") {
                last.title = familyTitle;
                if (!last.note.empty()) last.note += " ";
                last.note += familyNote(rr.isExplicit);
                renamedFinal = true;
            }
        }
        for (auto& s : rr.steps) r.steps.push_back(s);
        r.assumptions.insert(r.assumptions.end(), rr.assumptions.begin(), rr.assumptions.end());
        r.checks.insert(r.checks.end(), rr.checks.begin(), rr.checks.end());
        Ex traj = rr.isExplicit && rr.explicitY ? rr.explicitY : rr.solution;
        if (!renamedFinal) r.steps.push_back({familyTitle, traj, familyNote(rr.isExplicit)});
        r.trajectory = traj;
        r.trajectoryExplicit = rr.isExplicit;
        r.trajectorySlope = trajSlope;
        r.solution = traj;
        r.closedForm = rr.closedForm;
        r.ok = true;
        publish();
        r.assumptions.push_back(en
            ? "F is smooth and the family slope is finite where the trajectories are computed; c was eliminated before solving."
            : "F هموار است و در نقاطی که مسیرها محاسبه می‌شوند شیب خانواده متناهی است؛ c پیش از حل حذف شد.");
    } catch (const std::exception& e) {
        r.error = en ? std::string("Trajectory error: ") + localizeError(e.what(), true)
                     : std::string("خطا در مسیرها: ") + e.what();
    }
    return r;
}

Result solveOrthogonal(const std::string& input, const std::string& c,
                       const std::string& x, const std::string& y,
                       const std::string& lang) {
    return solveTrajectory(input, c, 90.0, x, y, lang, true);
}

Result solveOblique(const std::string& input, const std::string& c, double angleDeg,
                    const std::string& x, const std::string& y,
                    const std::string& lang) {
    return solveTrajectory(input, c, angleDeg, x, y, lang, false);
}

} // namespace ode
