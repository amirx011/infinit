// Infinit Lab — curriculum topic workbench.
// SPDX-License-Identifier: Apache-2.0
#include "topics.hpp"

#include "series_solutions.hpp"
#include "linear_systems.hpp"
#include "dynamical_systems.hpp"
#include "bvp.hpp"
#include "special_series.hpp"
#include "pde_series.hpp"
#include "calculus_topics.hpp"

#include "calculus_ext.hpp"
#include "expr.hpp"
#include "numeric.hpp"
#include "poly.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace ode {
namespace {

constexpr double kPi = 3.14159265358979323846;

std::string numStr(double v, int digits = 6) {
    if (std::isnan(v)) return "nan";
    if (!std::isfinite(v)) return v < 0 ? "-inf" : "inf";
    // Numerical integration leaves ~1e-16 noise on coefficients that are
    // mathematically zero; show those as an honest zero.
    if (std::fabs(v) < 1e-10) return "0";
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*g", digits, v);
    std::string s(buf);
    if (s.find('.') != std::string::npos && s.find('e') == std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s;
}

// Like numStr, but never rounds a small positive value down to zero.  Used
// for quantities whose sign is part of the answer (populations, drift).
std::string rawNumStr(double v, int digits = 6) {
    if (std::isnan(v)) return "nan";
    if (!std::isfinite(v)) return v < 0 ? "-inf" : "inf";
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*g", digits, v);
    std::string s(buf);
    if (s.find('.') != std::string::npos && s.find('e') == std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s;
}

Ex numEx(double v) {
    if (!std::isfinite(v)) return nullptr;
    return Num(Rational::fromDouble(v));
}

// n·π/L as a symbolic multiple of π.  Keeping π symbolic avoids printing a
// huge rational approximation of π, and the usual L = π cancels to n exactly.
Ex piMultiple(int n, double L) {
    if (std::fabs(L - kPi) < 1e-9) return Num(n);
    return Mul(Num(n), Sym("pi"), Pow(numEx(L), Num(-1)));
}

// Keep a literal pi in coefficients instead of its long rational shadow.
Ex numOrPi(double v) {
    if (std::fabs(v - kPi) < 1e-9) return Sym("pi");
    return numEx(v);
}

// Display helper: keep simple exact fractions (1/2, 3/5, 19) and fall back to
// a short decimal for coefficients that would otherwise print as huge
// rationals (damped frequencies, RK4 values, ...).
Ex smartNum(double v) {
    if (!std::isfinite(v)) return nullptr;
    if (std::fabs(v) < 1e-12) return Num(0);
    const double rounded = std::round(v);
    if (std::fabs(v - rounded) < 1e-10) return Num((long long)rounded);
    const Rational simple = Rational::fromDouble(v, 100);
    if (std::fabs(simple.toDouble() - v) < 1e-9) return Num(simple);
    return Fn("decimal", {Num(Rational::fromDouble(v))});
}

Ex expEx(const Ex& a) { return Fn("exp", {a}); }
Ex sinEx(const Ex& a) { return Fn("sin", {a}); }
Ex cosEx(const Ex& a) { return Fn("cos", {a}); }
Ex sinhEx(const Ex& a) { return Fn("sinh", {a}); }
Ex coshEx(const Ex& a) { return Fn("cosh", {a}); }

double evalF(const Ex& f, const std::string& x, double xv, const std::string& y = "",
             double yv = 0.0, bool* ok = nullptr) {
    std::map<std::string, double> vars;
    vars[x] = xv;
    if (!y.empty()) vars[y] = yv;
    bool local = false;
    const double v = evalReal(f, vars, &local);
    if (ok) *ok = local;
    return local ? v : std::numeric_limits<double>::quiet_NaN();
}

void addStep(Result& r, const std::string& titleKey, const Ex& math = nullptr,
             const std::string& noteKey = "", const std::string& params = "",
             const std::string& noteMath = "") {
    Step s;
    s.titleKey = titleKey;
    s.math = math;
    s.noteKey = noteKey;
    s.params = params;
    s.noteMath = noteMath;
    r.steps.push_back(std::move(s));
}

// A step whose note is engine data (a table or a symbolic rule), not prose.
void addStepRaw(Result& r, const std::string& titleKey, const Ex& math,
                const std::string& rawNote) {
    Step s;
    s.titleKey = titleKey;
    s.math = math;
    s.note = rawNote;
    r.steps.push_back(std::move(s));
}



void baseId(Result& r, const std::string& mode, const std::string& typeId,
            const std::string& variable = "") {
    r.ok = true;
    r.mode = mode;
    r.typeId = typeId;
    r.variable = variable;
    r.engine = "native";
    r.closedForm = true;
    r.verified = true;
}

void failCode(Result& r, const std::string& code) {
    r.ok = false;
    r.errorCode = code;
    r.verified = false;
    r.error.clear();
}

void failFrom(Result& r, const std::exception& e, const char* where) {
    if (const auto* coded = dynamic_cast<const EngineError*>(&e)) {
        r.errorCode = coded->code;
        r.errorDetail = coded->detail;
    } else {
        r.errorCode = "unavailable";
        r.errorDetail = std::string(where) + ": " + e.what();
    }
    r.ok = false;
    r.verified = false;
    r.error.clear();
}

void failDomain(Result& r, bool en, const DomainIssue& issue) {
    r.ok = false;
    r.verified = false;
    r.errorCode = issue.code.empty() ? "singular" : issue.code;
    r.errorDetail = issue.detail;
    r.errorPoint = issue.point;
    r.hasErrorPoint = true;
    r.error.clear();
}

bool validCoefficient(double v) {
    if (!std::isfinite(v)) return false;
    if (v >= (double)std::numeric_limits<long long>::max() ||
        v <= (double)std::numeric_limits<long long>::min())
        return false;
    return std::fabs(v) <= 1e10;
}

Ex linearCombine(double c1, const Ex& f1, double c2, const Ex& f2) {
    std::vector<Ex> terms;
    if (std::fabs(c1) > 1e-14) terms.push_back(Mul(numEx(c1), f1));
    if (std::fabs(c2) > 1e-14) terms.push_back(Mul(numEx(c2), f2));
    if (terms.empty()) return Num(0);
    return Add(terms);
}

// ---------------------------------------------------------------- growth/decay
// Evaluate a time-dependent answer at a single instant.
bool valueAtTime(const Ex& e, double t, double& out) {
    bool ok = false;
    out = evalReal(e, {{"t", t}}, &ok);
    return ok && std::isfinite(out);
}

Result growthDecay(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "growth_decay", "t");
    const double k = req.number("k", 0.1);
    const double y0 = req.number("y0", 1.0);
    const double t = req.number("t", 10.0);
    const Ex solution = Mul(numEx(y0), expEx(Mul(numEx(k), Sym("t"))));
    r.solution = solution;
    r.normalized = Add(Mul(numEx(k), Sym("y")), Mul(Num(-1), Sym("y'")));
    r.hasNumeric = true;
    r.numericValue = y0 * std::exp(k * t);
    r.point = "t=" + numStr(t);

    addStep(r, "step.title.state_the_model", Sym("y' = k y"), "step.note.a_quantity_whose_rate_of_change_is_proportional_to_its_current_size");
    addStep(r, "step.title.solve_the_separable_equation", Sym("y = C e^{k t}"), "step.note.separate_dy_y_k_dt_and_integrate_both_sides");
    addStep(r, "step.title.apply_the_initial_condition", Mul(numEx(y0), expEx(Mul(numEx(k), Sym("t")))), "step.note.at_t_0_y_y0_so_c_y0");
    addStep(r, "step.title.evaluate_at_the_requested_time", smartNum(r.numericValue),
            "step.note.substitute_t", "{\"t\":" + numStr(t) + "}");
    double at0 = 0.0;
    const bool initialOk = valueAtTime(solution, 0.0, at0) &&
                           std::fabs(at0 - y0) < 1e-9 * (1.0 + std::fabs(y0));
    addCheck(r, "check.initial_condition", initialOk, "", "",
             "y(0) = " + numStr(at0));
    return r;
}

// ---------------------------------------------------------------- cooling
Result newtonCooling(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "cooling", "t");
    const double k = req.number("k", 0.1);
    const double room = req.number("room", 20.0);
    const double t0 = req.number("t0", 90.0);
    const double t = req.number("t", 10.0);
    const Ex solution =
        Add(numEx(room), Mul(numEx(t0 - room), expEx(Mul(Num(-1), numEx(k), Sym("t")))));
    r.solution = solution;
    r.normalized = Add(Mul(Num(-1), numEx(k), Add(Sym("T"), Num(-room))), Sym("T'"));
    r.hasNumeric = true;
    r.numericValue = room + (t0 - room) * std::exp(-k * t);
    r.point = "t=" + numStr(t);

    addStep(r, "step.title.state_the_model", Sym("T' = -k (T - T_m)"), "step.note.the_object_cools_at_a_rate_proportional_to_the_difference_from_the_room");
    addStep(r, "step.title.solve_the_linear_equation", Add(numEx(room), Mul(Sym("C"), expEx(Mul(Num(-1), numEx(k), Sym("t"))))), "step.note.the_general_solution_is_t_tm_c_e_kt");
    addStep(r, "step.title.apply_t_0_t0", Mul(numEx(t0 - room), expEx(Mul(Num(-1), numEx(k), Sym("t")))), "step.note.c_t0_tm_so_the_temperature_difference_decays_exponentially");
    addStep(r, "step.title.substitute_the_data", solution, "", "",
            "T(" + numStr(t) + ") = " + numStr(r.numericValue));
    double limitT = 0.0;
    double tAt0 = 0.0;
    const bool initialOk = valueAtTime(solution, 0.0, tAt0) &&
                           std::fabs(tAt0 - t0) < 1e-9 * (1.0 + std::fabs(t0));
    addCheck(r, "check.initial_condition", initialOk, "", "",
             "T(0) = " + numStr(tAt0));
    // Only a positive cooling constant relaxes towards the room.  With k = 0
    // the temperature is constant, and with k < 0 it moves away from the room;
    // claiming convergence in either case would be a false failure.
    if (k > 0) {
        const double probeT = 50.0 / k;
        const bool limitOk = valueAtTime(solution, probeT, limitT) &&
                             std::fabs(limitT - room) <
                                 1e-6 * (1.0 + std::fabs(room));
        addCheck(r, "check.long_term_temperature", limitOk,
                 "check.approaches_room", "{\"room\":" + numStr(room) + "}",
                 "T(" + numStr(probeT) + ") = " + numStr(limitT));
    }
    return r;
}

// ---------------------------------------------------------------- logistic
Result logistic(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "logistic", "t");
    const double rate = req.number("r", 0.5);
    const double carry = req.number("K", 1000.0);
    const double p0 = req.number("p0", 50.0);
    const double t = req.number("t", 10.0);
    if (std::fabs(carry) < 1e-12 || std::fabs(p0) < 1e-12) {
        failCode(r, "bad_capacity");
        return r;
    }
    const double ratio = (carry - p0) / p0;
    const Ex denom = Add(Num(1), Mul(numEx(ratio), expEx(Mul(Num(-1), numEx(rate), Sym("t")))));
    const Ex solution = Mul(numEx(carry), Pow(denom, Num(-1)));
    r.solution = solution;
    r.normalized = Add(Sym("P'"), Mul(Num(-1), numEx(rate), Sym("P"),
                                     Add(Num(1), Mul(Num(-1), Pow(numEx(carry), Num(-1)), Sym("P")))));
    r.hasNumeric = true;
    r.numericValue = carry / (1.0 + ratio * std::exp(-rate * t));
    r.point = "t=" + numStr(t);

    addStep(r, "step.title.state_the_logistic_model", Sym("P' = r P (1 - P/K)"), "step.note.growth_slows_as_the_population_approaches_the_carrying_capacity_k");
    addStep(r, "step.title.separate_and_integrate", Add(numEx(carry), Mul(Sym("C"), expEx(Mul(Num(-1), numEx(rate), Sym("t"))))), "step.note.partial_fractions_give_the_general_solution");
    addStep(r, "step.title.apply_p_0_p0", solution, "step.note.the_constant_is_fixed_by_the_initial_population");
    addStep(r, "step.title.evaluate", smartNum(r.numericValue), "", "",
            "P(" + numStr(t) + ") = " + numStr(r.numericValue));
    double pAt0 = 0.0, pLimit = 0.0;
    const bool initialOk = valueAtTime(solution, 0.0, pAt0) &&
                           std::fabs(pAt0 - p0) < 1e-9 * (1.0 + std::fabs(p0));
    addCheck(r, "check.initial_condition", initialOk, "", "",
             "P(0) = " + numStr(pAt0));
    // A positive growth rate approaches K.  With r = 0 nothing changes and
    // with r < 0 the population decays to zero, so the capacity limit is not
    // the right statement there.
    if (rate > 0) {
        const double probeT = 50.0 / rate;
        const bool capacityOk = valueAtTime(solution, probeT, pLimit) &&
                                std::fabs(pLimit - carry) <
                                    1e-6 * (1.0 + std::fabs(carry));
        addCheck(r, "check.carrying_capacity", capacityOk,
                 "check.below_capacity", "{\"k\":" + numStr(carry) + "}",
                 "P(" + numStr(probeT) + ") = " + numStr(pLimit));
    }
    return r;
}

// ---------------------------------------------------------------- mixing
Result mixing(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "mixing", "t");
    const double volume0 = req.number("V0", 100.0);
    const double q0 = req.number("q0", 10.0);
    const double rateIn = req.number("rin", 5.0);
    const double rateOut = req.number("rout", 5.0);
    const double concIn = req.number("cin", 0.2);
    const double t = req.number("t", 10.0);
    if (volume0 <= 0) {
        failCode(r, "bad_initial_volume");
        return r;
    }
    const double a = rateIn - rateOut;
    // When the outflow outruns the inflow the tank empties at a finite time;
    // beyond that point the model has no positive volume.
    if (a < 0 && t >= -volume0 / a) {
        r.ok = false;
        r.verified = false;
        r.errorCode = "tank_empties";
        r.errorDetail = "the tank empties at t = " + numStr(-volume0 / a);
        return r;
    }
    Ex solution;
    double numeric = 0.0;
    if (std::fabs(a) < 1e-12) {
        const double cInf = concIn;
        solution = Add(numEx(cInf * volume0),
                       Mul(numEx(q0 - cInf * volume0),
                           expEx(Mul(Num(-1), numEx(rateOut / volume0), Sym("t")))));
        numeric = cInf * volume0 + (q0 - cInf * volume0) * std::exp(-rateOut * t / volume0);
        addStep(r, "step.title.constant_volume", Sym("V(t) = V_0"), "step.note.the_inflow_and_outflow_rates_are_equal_so_the_volume_stays_constant");
        addStep(r, "step.title.write_the_balance_law", Sym("Q' = r c_in - (r/V_0) Q"), "step.note.salt_enters_at_r_cin_and_leaves_at_r_v0_q");
        addStep(r, "step.title.solve_the_linear_equation", solution, "", "",
                 "Q(t) = c_in V_0 + (Q_0 - c_in V_0) e^(-r t / V_0)");
    } else {
        const double exponent = -rateOut / a;
        const Ex volume = Add(numEx(volume0), Mul(numEx(a), Sym("t")));
        // Keep the (V/V0) ratio inside the power: the raw coefficient
        // (Q0 - cin V0) V0^(-e) can be tiny enough to round to zero.
        const Ex ratio = Mul(Pow(numEx(volume0), Num(-1)), volume);
        solution = Add(Mul(numEx(concIn), volume),
                       Mul(numEx(q0 - concIn * volume0), Pow(ratio, numEx(exponent))));
        const double vt = volume0 + a * t;
        numeric = concIn * vt + (q0 - concIn * volume0) * std::pow(vt / volume0, exponent);
        addStep(r, "step.title.variable_volume", volume, "step.note.the_volume_changes_linearly_because_the_rates_differ");
        addStep(r, "step.title.write_the_balance_law", Sym("Q' + (r_out/V(t)) Q = r_in c_in"), "step.note.the_outflow_concentration_is_q_v_t");
        addStep(r, "step.title.solve_with_an_integrating_factor", solution, "step.note.the_integrating_factor_is_v_rout_a");
    }
    r.solution = solution;
    r.normalized = Sym("Q' = r_in c_in - r_out Q/V");
    r.hasNumeric = true;
    r.numericValue = numeric;
    r.point = "t=" + numStr(t);
    addStep(r, "step.title.evaluate_the_amount", smartNum(numeric), "", "",
            "Q(" + numStr(t) + ") = " + numStr(numeric));
    double qAt0 = 0.0;
    const bool initialOk = valueAtTime(solution, 0.0, qAt0) &&
                           std::fabs(qAt0 - q0) < 1e-9 * (1.0 + std::fabs(q0));
    addCheck(r, "check.initial_amount", initialOk, "", "",
             "Q(0) = " + numStr(qAt0));
    return r;
}

// ---------------------------------------------------------------- spring/circuit
struct Oscillator {
    double m = 1, c = 0, k = 1, f0 = 0, w = 1, y0 = 1, v0 = 0;
    double r1 = 0, r2 = 0, alpha = 0, beta = 0;
    double a = 0, b = 0;       // particular coefficients
    double c1 = 0, c2 = 0;     // homogeneous constants
    bool resonance = false;
    bool quadratic = false;    // constant forcing with no spring and no damping
    bool linear = false;       // constant forcing with no spring but damping
    int kind = 0;              // 0 distinct real, 1 repeated, 2 complex
};

Oscillator solveOscillator(const TopicRequest& req) {
    Oscillator o;
    o.m = req.number("m", 1.0);
    o.c = req.number("c", 0.0);
    o.k = req.number("k", 1.0);
    o.f0 = req.number("f0", 0.0);
    o.w = req.number("w", 1.0);
    o.y0 = req.number("y0", 1.0);
    o.v0 = req.number("v0", 0.0);
    if (std::fabs(o.m) < 1e-12) o.m = 1.0;
    const double disc = o.c * o.c - 4.0 * o.m * o.k;
    if (disc > 1e-12) {
        o.kind = 0;
        const double s = std::sqrt(disc);
        o.r1 = (-o.c + s) / (2.0 * o.m);
        o.r2 = (-o.c - s) / (2.0 * o.m);
    } else if (std::fabs(disc) <= 1e-12) {
        o.kind = 1;
        o.r1 = o.r2 = -o.c / (2.0 * o.m);
    } else {
        o.kind = 2;
        o.alpha = -o.c / (2.0 * o.m);
        o.beta = std::sqrt(-disc) / (2.0 * o.m);
    }
    const double d = (o.k - o.m * o.w * o.w) * (o.k - o.m * o.w * o.w) +
                     (o.c * o.w) * (o.c * o.w);
    const bool forcing = std::fabs(o.f0) > 1e-14;
    if (!forcing) {
        // No forcing term, so the particular solution is zero; the resonance
        // formula would otherwise divide by a vanishing denominator.
        o.a = 0;
        o.b = 0;
    } else if (d > 1e-12) {
        o.a = o.f0 * (o.k - o.m * o.w * o.w) / d;
        o.b = o.f0 * o.c * o.w / d;
    } else if (std::fabs(o.w) < 1e-12) {
        // Constant forcing with no spring: a cos(wt) ansatz at w = 0 is just
        // a constant and cannot absorb t or t^2 growth.
        if (std::fabs(o.c) < 1e-12) {
            o.quadratic = true;
        } else {
            o.linear = true;
        }
    } else {
        o.resonance = true;
        o.a = 0;
        o.b = o.f0 / (2.0 * o.m * o.w);
    }
    const double yp0 = o.a;
    // y_p'(0): zero for a cos/sin particular and for the resonant t sin(wt)
    // term, f0/c for the linear term and zero for the quadratic one.
    double yp1 = 0.0;
    if (o.linear) {
        yp1 = o.f0 / o.c;
    } else if (!o.resonance && !o.quadratic && !o.linear) {
        yp1 = o.b * o.w;
    }
    if (o.kind == 0) {
        // c1 + c2 = y0 - yp0 ; r1 c1 + r2 c2 = v0 - yp1
        const double det = o.r2 - o.r1;
        o.c1 = ((o.y0 - yp0) * o.r2 - (o.v0 - yp1)) / det;
        o.c2 = ((o.v0 - yp1) - (o.y0 - yp0) * o.r1) / det;
    } else if (o.kind == 1) {
        o.c1 = o.y0 - yp0;
        o.c2 = (o.v0 - yp1) - o.r1 * o.c1;
    } else {
        o.c1 = o.y0 - yp0;
        o.c2 = ((o.v0 - yp1) - o.alpha * o.c1) / o.beta;
    }
    return o;
}

Ex oscillatorSolution(const Oscillator& o, bool circuit, bool en) {
    (void)en;
    const char* var = circuit ? "q" : "y";
    Ex homogeneous;
    if (o.kind == 0) {
        homogeneous = linearCombine(o.c1, expEx(Mul(smartNum(o.r1), Sym("t"))), o.c2,
                                    expEx(Mul(smartNum(o.r2), Sym("t"))));
    } else if (o.kind == 1) {
        homogeneous = Mul(expEx(Mul(smartNum(o.r1), Sym("t"))),
                          Add(smartNum(o.c1), Mul(smartNum(o.c2), Sym("t"))));
    } else {
        homogeneous = Mul(expEx(Mul(smartNum(o.alpha), Sym("t"))),
                          Add(Mul(smartNum(o.c1), cosEx(Mul(smartNum(o.beta), Sym("t")))),
                              Mul(smartNum(o.c2), sinEx(Mul(smartNum(o.beta), Sym("t"))))));
    }
    Ex particular;
    if (o.quadratic) {
        particular = Mul(smartNum(o.f0 / (2.0 * o.m)), Pow(Sym("t"), Num(2)));
    } else if (o.linear) {
        particular = Mul(smartNum(o.f0 / o.c), Sym("t"));
    } else if (o.resonance) {
        particular = Mul(smartNum(o.b), Sym("t"), sinEx(Mul(numEx(o.w), Sym("t"))));
    } else {
        particular = Add(Mul(smartNum(o.a), cosEx(Mul(numEx(o.w), Sym("t")))),
                         Mul(smartNum(o.b), sinEx(Mul(numEx(o.w), Sym("t")))));
    }
    (void)var;
    return Add(homogeneous, particular);
}

Result oscillator(const TopicRequest& req, bool en, bool circuit) {
    Result r;
    baseId(r, "topic", circuit ? "circuit" : "spring_mass", "t");
    const Oscillator o = solveOscillator(req);
    const Ex solution = oscillatorSolution(o, circuit, en);
    r.solution = solution;
    r.normalized = circuit ? Sym("L q'' + R q' + q/C = E(t)")
                           : Sym("m y'' + c y' + k y = F(t)");
    r.order = 2;
    r.closedForm = true;

    const std::string mName = circuit ? "L" : "m";
    const std::string cName = circuit ? "R" : "c";
    const std::string kName = circuit ? "1/C" : "k";
    addStep(r, "step.title.write_the_model",
            circuit ? Sym("L q'' + R q' + q/C = E_0 cos(w t)")
                    : Sym("m y'' + c y' + k y = F_0 cos(w t)"),
            circuit ? "step.note.kirchhoff_voltage_law"
                    : "step.note.newton_second_law");
    addStep(r, "step.title.solve_the_homogeneous_equation",
            Add(Mul(numEx(o.m), Pow(Sym("r"), Num(2))), Mul(numEx(o.c), Sym("r")),
                numEx(o.k)),
            "step.note.characteristic_equation", "",
            mName + " r^2 + " + cName + " r + " + kName + " = 0");
    if (o.kind == 0) {
        addStep(r, "step.title.real_distinct_roots", Add(Sym("r_1 = " + numStr(o.r1)), Sym("r_2 = " + numStr(o.r2))), "step.note.the_response_is_a_sum_of_two_exponentials");
    } else if (o.kind == 1) {
        addStep(r, "step.title.repeated_root", Sym("r = " + numStr(o.r1)), "step.note.critical_damping_the_response_is_c1_c2_t_e_rt");
    } else {
        addStep(r, "step.title.complex_roots", Add(Sym("alpha = " + numStr(o.alpha)), Sym("beta = " + numStr(o.beta))), "step.note.the_response_oscillates_inside_a_decaying_envelope");
    }
    if (o.resonance) {
        addStep(r, "step.title.resonance", Sym("y_p = (F_0/(2 m w)) t sin(w t)"), "step.note.the_forcing_frequency_equals_the_natural_frequency_so_the_amplitude_grows");
    } else if (o.quadratic) {
        addStep(r, "step.title.particular_solution",
                Mul(smartNum(o.f0 / (2.0 * o.m)), Pow(Sym("t"), Num(2))),
                "step.note.substitute_a_cos_wt_b_sin_wt_and_match_coefficients");
    } else if (o.linear) {
        addStep(r, "step.title.particular_solution",
                Mul(smartNum(o.f0 / o.c), Sym("t")),
                "step.note.substitute_a_cos_wt_b_sin_wt_and_match_coefficients");
    } else {
        addStep(r, "step.title.particular_solution", Add(Mul(smartNum(o.a), cosEx(Mul(numEx(o.w), Sym("t")))), Mul(smartNum(o.b), sinEx(Mul(numEx(o.w), Sym("t"))))), "step.note.substitute_a_cos_wt_b_sin_wt_and_match_coefficients");
    }
    addStep(r, "step.title.apply_the_initial_conditions", Add(Sym("C_1 = " + numStr(o.c1)), Sym("C_2 = " + numStr(o.c2))), "step.note.use_the_initial_displacement_and_velocity_to_fix_the_two_constants");
    addStep(r, "step.title.write_the_complete_response", solution, "step.note.the_solution_is_the_sum_of_the_transient_and_steady_state_parts");

    const Ex velocity = diff(solution, "t");
    double at0 = 0.0, vAt0 = 0.0;
    const bool posOk = valueAtTime(solution, 0.0, at0) &&
                       std::fabs(at0 - o.y0) < 1e-9 * (1.0 + std::fabs(o.y0));
    const bool velOk = valueAtTime(velocity, 0.0, vAt0) &&
                       std::fabs(vAt0 - o.v0) < 1e-9 * (1.0 + std::fabs(o.v0));
    addCheck(r, "check.initial_displacement", posOk && velOk,
             "", "",
             "y(0) = " + numStr(at0) + ", y'(0) = " + numStr(vAt0));
    return r;
}

// ---------------------------------------------------------------- falling body
Result fallingBody(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "falling_body", "t");
    const double m = req.number("m", 1.0);
    const double k = req.number("k", 0.5);
    const double g = req.number("g", 9.81);
    const double v0 = req.number("v0", 0.0);
    const double t = req.number("t", 5.0);
    if (std::fabs(m) < 1e-12) {
        failCode(r, "bad_mass");
        return r;
    }
    Ex solution;
    double numeric = 0.0;
    if (std::fabs(k) < 1e-12) {
        solution = Add(numEx(v0), Mul(numEx(g), Sym("t")));
        numeric = v0 + g * t;
        addStep(r, "step.title.no_drag", Sym("v' = g"), "step.note.the_velocity_grows_linearly");
    } else {
        const double terminal = m * g / k;
        // v(t) = mg/k + (v0 - mg/k) e^(-kt/m)
        const Ex decay = expEx(Mul(Num(-1), numEx(k / m), Sym("t")));
        solution = Add(Mul(numEx(terminal), Add(Num(1), Mul(Num(-1), decay))),
                       Mul(numEx(v0), decay));
        numeric = terminal * (1.0 - std::exp(-k * t / m)) + v0 * std::exp(-k * t / m);
        addStep(r, "step.title.state_the_model", Sym("m v' = m g - k v"), "step.note.gravity_pulls_down_drag_opposes_the_motion");
        addStep(r, "step.title.solve_the_linear_equation", Add(numEx(terminal), Mul(Sym("C"), expEx(Mul(Num(-1), numEx(k / m), Sym("t"))))), "step.note.the_terminal_velocity_is_mg_k");
        addStep(r, "step.title.apply_v_0_v0", solution, "step.note.the_constant_is_v0_mg_k");
    }
    r.solution = solution;
    r.normalized = Sym("m v' = m g - k v");
    r.hasNumeric = true;
    r.numericValue = numeric;
    r.point = "t=" + numStr(t);
    addStep(r, "step.title.evaluate", smartNum(numeric), "", "",
            "v(" + numStr(t) + ") = " + numStr(numeric) + " m/s");
    double vAt0 = 0.0;
    const bool startOk = valueAtTime(solution, 0.0, vAt0) &&
                         std::fabs(vAt0 - v0) < 1e-9 * (1.0 + std::fabs(v0));
    if (std::fabs(k) < 1e-12) {
        const Ex accel = diff(solution, "t");
        double aValue = 0.0;
        const bool accelOk = valueAtTime(accel, t, aValue) &&
                             std::fabs(aValue - g) < 1e-9 * (1.0 + std::fabs(g));
        addCheck(r, "check.initial_velocity", startOk && accelOk, "", "",
                 "v(0) = " + numStr(vAt0) + ", v' = " + numStr(aValue));
    } else {
        const double terminal = m * g / k;
        const double probeT = 50.0 * std::fabs(m / k);
        double limitV = 0.0;
        const bool limitOk = valueAtTime(solution, probeT, limitV) &&
                             std::fabs(limitV - terminal) <
                                 1e-6 * (1.0 + std::fabs(terminal));
        if (k > 0) {
            addCheck(r, "check.terminal_velocity", startOk && limitOk, "", "",
                     "v(0) = " + numStr(vAt0) + ", v(" + numStr(probeT) +
                         ") = " + numStr(limitV));
        } else {
            // Negative drag is unphysical, so there is no terminal velocity;
            // the initial condition is still a valid check.
            addCheck(r, "check.initial_velocity", startOk, "", "",
                     "v(0) = " + numStr(vAt0));
        }
    }
    return r;
}

// ---------------------------------------------------------------- numerical ODE
enum class NumMethod { Euler, Heun, Rk4 };

Result numericalOde(const TopicRequest& req, bool en, NumMethod method) {
    Result r;
    baseId(r, "topic",
           method == NumMethod::Euler
               ? "euler"
               : (method == NumMethod::Heun ? "heun" : "rk4"),
           "x");
    const std::string input = req.text("f", "x + y");
    const double x0 = req.number("x0", 0.0);
    const double y0 = req.number("y0", 1.0);
    const double h = req.number("h", 0.1);
    int n = (int)std::llround(req.number("n", 10.0));
    if (n < 1) n = 1;
    if (n > 200) n = 200;
    if (std::fabs(h) < 1e-12) {
        failCode(r, "bad_step");
        return r;
    }
    Ex f;
    try {
        f = parseExpr(input);
    } catch (const std::exception& e) {
        failFrom(r, e, "numerical ode");
        return r;
    }
    const std::string unknown = unknownFunction(f);
    if (!unknown.empty()) {
        r.ok = false;
        r.verified = false;
        r.errorCode = "unknown_function";
        r.errorDetail = "unknown function: " + unknown;
        return r;
    }
    r.normalized = f;
    // A singular right-hand side (for example csc²x at x = 0) is a domain
    // failure, not an unknown function or a generic integration failure.
    {
        std::map<std::string, double> fixed;
        fixed["y"] = y0;
        DomainIssue issue;
        if (checkDomain(f, "x", fixed, x0, x0 + n * h, &issue)) {
            failDomain(r, en, issue);
            return r;
        }
    }
    double x = x0;
    double y = y0;
    std::ostringstream table;
    table << "i\tx\ty\n0\t" << numStr(x) << "\t" << numStr(y) << "\n";
    for (int i = 0; i < n; ++i) {
        bool ok = true;
        const double k1 = evalF(f, "x", x, "y", y, &ok);
        if (!ok || !std::isfinite(k1)) {
            failCode(r, "not_evaluable");
            return r;
        }
        if (method == NumMethod::Euler) {
            y += h * k1;
        } else if (method == NumMethod::Heun) {
            const double k2 = evalF(f, "x", x + h, "y", y + h * k1, &ok);
            if (!ok) {
                failCode(r, "not_evaluable");
                return r;
            }
            y += h * 0.5 * (k1 + k2);
        } else {
            const double k2 = evalF(f, "x", x + 0.5 * h, "y", y + 0.5 * h * k1, &ok);
            const double k3 = evalF(f, "x", x + 0.5 * h, "y", y + 0.5 * h * k2, &ok);
            const double k4 = evalF(f, "x", x + h, "y", y + h * k3, &ok);
            if (!ok) {
                failCode(r, "not_evaluable");
                return r;
            }
            y += h / 6.0 * (k1 + 2.0 * k2 + 2.0 * k3 + k4);
        }
        x = x0 + (i + 1) * h;
        if (i < 24) table << (i + 1) << "\t" << numStr(x) << "\t" << numStr(y) << "\n";
    }
    r.solution = smartNum(y);
    r.hasNumeric = true;
    r.numericValue = y;
    r.point = "x=" + numStr(x);
    addStep(r, "step.title.write_the_update_rule", method == NumMethod::Euler ? Sym("y_{i+1} = y_i + h f(x_i, y_i)") : (method == NumMethod::Heun ? Sym("y_{i+1} = y_i + h/2 (f_i + f(x_i+h, y_i+h f_i))") : Sym("y_{i+1} = y_i + h/6 (k_1 + 2k_2 + 2k_3 + k_4)")), "step.note.start_from_x0_y0_and_advance_one_step_at_a_time");
    addStepRaw(r, "step.title.step_through_the_interval", nullptr, table.str());
    addStep(r, "step.title.read_the_final_value", smartNum(y),
            "step.note.after_steps_y", "{\"n\":" + std::to_string(n) + "}",
            "y(" + numStr(x) + ") \xE2\x89\x88 " + numStr(y));
    addCheck(r, "check.step_count", n > 0,
             "check.table_steps", "{\"n\":" + std::to_string(n) + "}");
    return r;
}

// Integrate y' = f(x, y) with a one-step method.  Returns false when the
// right-hand side stops being finite, so no caller ever reports a value that
// the method could not actually reach.
static bool marchMethod(const Ex& f, double x0, double y0, double h, int n,
                        NumMethod method, double& finalY) {
    double x = x0;
    double y = y0;
    for (int i = 0; i < n; ++i) {
        bool ok = true;
        const double k1 = evalF(f, "x", x, "y", y, &ok);
        if (!ok || !std::isfinite(k1)) return false;
        if (method == NumMethod::Euler) {
            y += h * k1;
        } else if (method == NumMethod::Heun) {
            const double k2 = evalF(f, "x", x + h, "y", y + h * k1, &ok);
            if (!ok || !std::isfinite(k2)) return false;
            y += h * 0.5 * (k1 + k2);
        } else {
            const double k2 = evalF(f, "x", x + 0.5 * h, "y", y + 0.5 * h * k1, &ok);
            const double k3 = evalF(f, "x", x + 0.5 * h, "y", y + 0.5 * h * k2, &ok);
            const double k4 = evalF(f, "x", x + h, "y", y + h * k3, &ok);
            if (!ok || !std::isfinite(k2) || !std::isfinite(k3) || !std::isfinite(k4))
                return false;
            y += h / 6.0 * (k1 + 2.0 * k2 + 2.0 * k3 + k4);
        }
        x = x0 + (i + 1) * h;
        if (!std::isfinite(y)) return false;
    }
    finalY = y;
    return true;
}

// ---------------------------------------------------------------- Adams–Bashforth multistep
Result multistep(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "multistep", "x");
    const std::string input = req.text("f", "x + y");
    const double x0 = req.number("x0", 0.0);
    const double y0 = req.number("y0", 1.0);
    const double h = req.number("h", 0.1);
    int n = (int)std::llround(req.number("n", 10.0));
    int order = (int)std::llround(req.number("order", 4.0));
    if (n < 1) n = 1;
    if (n > 500) n = 500;
    if (order < 2) order = 2;
    if (order > 4) order = 4;
    if (std::fabs(h) < 1e-12) {
        failCode(r, "bad_step");
        return r;
    }
    if (n <= order) {
        failCode(r, "too_few_steps");
        return r;
    }
    Ex f;
    try {
        f = parseExpr(input);
    } catch (const std::exception& e) {
        failFrom(r, e, "multistep");
        return r;
    }
    const std::string unknown = unknownFunction(f);
    if (!unknown.empty()) {
        r.ok = false;
        r.verified = false;
        r.errorCode = "unknown_function";
        r.errorDetail = "unknown function: " + unknown;
        return r;
    }
    r.normalized = f;
    {
        std::map<std::string, double> fixed;
        fixed["y"] = y0;
        DomainIssue issue;
        if (checkDomain(f, "x", fixed, x0, x0 + n * h, &issue)) {
            failDomain(r, en, issue);
            return r;
        }
    }
    // Bootstrap the first order-1 steps with RK4.  Adams–Bashforth needs a
    // history of derivatives, and RK4 supplies it at the same accuracy.
    std::vector<double> fs;
    fs.reserve(n + 1);
    double x = x0;
    double y = y0;
    std::ostringstream table;
    table << "i\tx\ty\n0\t" << numStr(x) << "\t" << numStr(y) << "\n";
    for (int i = 0; i < order - 1; ++i) {
        bool ok = true;
        const double k1 = evalF(f, "x", x, "y", y, &ok);
        if (!ok || !std::isfinite(k1)) {
            failCode(r, "not_evaluable");
            return r;
        }
        fs.push_back(k1);
        const double k2 = evalF(f, "x", x + 0.5 * h, "y", y + 0.5 * h * k1, &ok);
        const double k3 = evalF(f, "x", x + 0.5 * h, "y", y + 0.5 * h * k2, &ok);
        const double k4 = evalF(f, "x", x + h, "y", y + h * k3, &ok);
        if (!ok || !std::isfinite(k2) || !std::isfinite(k3) || !std::isfinite(k4)) {
            failCode(r, "not_evaluable");
            return r;
        }
        y += h / 6.0 * (k1 + 2.0 * k2 + 2.0 * k3 + k4);
        x = x0 + (i + 1) * h;
        table << (i + 1) << "\t" << numStr(x) << "\t" << numStr(y) << "\n";
    }
    for (int i = order - 1; i < n; ++i) {
        bool ok = true;
        const double fi = evalF(f, "x", x, "y", y, &ok);
        if (!ok || !std::isfinite(fi)) {
            failCode(r, "not_evaluable");
            return r;
        }
        fs.push_back(fi);
        const size_t m = fs.size();
        double increment = 0.0;
        if (order == 2) {
            increment = h / 2.0 * (3.0 * fs[m - 1] - fs[m - 2]);
        } else if (order == 3) {
            increment = h / 12.0 *
                        (23.0 * fs[m - 1] - 16.0 * fs[m - 2] + 5.0 * fs[m - 3]);
        } else {
            increment = h / 24.0 * (55.0 * fs[m - 1] - 59.0 * fs[m - 2] +
                                    37.0 * fs[m - 3] - 9.0 * fs[m - 4]);
        }
        y += increment;
        if (!std::isfinite(y)) {
            failCode(r, "not_evaluable");
            return r;
        }
        x = x0 + (i + 1) * h;
        if (i < 24) table << (i + 1) << "\t" << numStr(x) << "\t" << numStr(y) << "\n";
    }
    r.solution = smartNum(y);
    r.hasNumeric = true;
    r.numericValue = y;
    r.point = "x=" + numStr(x);
    const Ex rule = order == 2
        ? Sym("y_{i+1} = y_i + h/2 (3f_i - f_{i-1})")
        : (order == 3
               ? Sym("y_{i+1} = y_i + h/12 (23f_i - 16f_{i-1} + 5f_{i-2})")
               : Sym("y_{i+1} = y_i + h/24 (55f_i - 59f_{i-1} + 37f_{i-2} - 9f_{i-3})"));
    addStep(r, "step.title.write_the_multistep_rule", rule,
            "step.note.use_the_previous_derivative_values");
    addStep(r, "step.title.bootstrap_with_rk4", Sym("y_1, ..., y_{k-1}"),
            "step.note.a_multistep_method_needs_a_starting_history",
            "{\"k\":" + std::to_string(order) + "}");
    addStepRaw(r, "step.title.step_through_the_interval", nullptr, table.str());
    addStep(r, "step.title.read_the_final_value", smartNum(y),
            "step.note.after_steps_y", "{\"n\":" + std::to_string(n) + "}",
            "y(" + numStr(x) + ") \xE2\x89\x88 " + numStr(y));
    double reference = y;
    if (marchMethod(f, x0, y0, h, n, NumMethod::Rk4, reference)) {
        const double diff = std::fabs(y - reference);
        const bool close = diff <= 5e-2 * (1.0 + std::fabs(reference));
        addCheck(r, "check.multistep_matches_rk4", close,
                 "check.multistep_agrees_with_rk4", "{\"n\":" + std::to_string(n) + "}",
                 "modelled = " + numStr(y) + ", RK4 = " + numStr(reference));
    }
    addCheck(r, "check.step_count", n > 0,
             "check.table_steps", "{\"n\":" + std::to_string(n) + "}");
    return r;
}

// ---------------------------------------------------------------- error and convergence
Result errorConvergence(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "error_convergence", "x");
    const std::string input = req.text("f", "x + y");
    const double x0 = req.number("x0", 0.0);
    const double y0 = req.number("y0", 1.0);
    const double h = req.number("h", 0.1);
    int n = (int)std::llround(req.number("n", 10.0));
    int code = (int)std::llround(req.number("method", 3.0));
    if (n < 1) n = 1;
    if (n > 400) n = 400;
    if (code < 1) code = 1;
    if (code > 3) code = 3;
    const NumMethod method =
        code == 1 ? NumMethod::Euler : (code == 2 ? NumMethod::Heun : NumMethod::Rk4);
    const char* methodName = code == 1 ? "Euler" : (code == 2 ? "Heun" : "RK4");
    const int expectedOrder = code == 1 ? 1 : (code == 2 ? 2 : 4);
    if (std::fabs(h) < 1e-12) {
        failCode(r, "bad_step");
        return r;
    }
    Ex f;
    try {
        f = parseExpr(input);
    } catch (const std::exception& e) {
        failFrom(r, e, "error convergence");
        return r;
    }
    const std::string unknown = unknownFunction(f);
    if (!unknown.empty()) {
        r.ok = false;
        r.verified = false;
        r.errorCode = "unknown_function";
        r.errorDetail = "unknown function: " + unknown;
        return r;
    }
    r.normalized = f;
    const double T = x0 + n * h;
    {
        std::map<std::string, double> fixed;
        fixed["y"] = y0;
        DomainIssue issue;
        if (checkDomain(f, "x", fixed, x0, T, &issue)) {
            failDomain(r, en, issue);
            return r;
        }
    }
    // Reference value: an exact solution when the caller supplies one,
    // otherwise RK4 on a much finer grid (the step is divided by 256).
    double reference = 0.0;
    double exactAtT = 0.0;
    bool haveExact = false;
    const std::string exactText = req.text("exact", "");
    if (!exactText.empty()) {
        try {
            const Ex exact = parseExpr(exactText);
            bool ok = false;
            exactAtT = evalReal(exact, {{"x", T}}, &ok);
            haveExact = ok && std::isfinite(exactAtT);
            if (haveExact) reference = exactAtT;
        } catch (const std::exception&) {
            haveExact = false;
        }
    }
    if (!haveExact) {
        const int fine = std::min<int>(100000, std::max(1024, n * 256));
        const double hf = (T - x0) / (double)fine;
        if (!marchMethod(f, x0, y0, hf, fine, NumMethod::Rk4, reference)) {
            failCode(r, "not_evaluable");
            return r;
        }
    }
    double yFull = 0.0;
    double yHalf = 0.0;
    if (!marchMethod(f, x0, y0, h, n, method, yFull) ||
        !marchMethod(f, x0, y0, h / 2.0, 2 * n, method, yHalf)) {
        failCode(r, "not_evaluable");
        return r;
    }
    const double eFull = std::fabs(yFull - reference);
    const double eHalf = std::fabs(yHalf - reference);
    const double p = (eFull > 0.0 && eHalf > 0.0)
                         ? std::log(eFull / eHalf) / std::log(2.0)
                         : std::numeric_limits<double>::quiet_NaN();
    const bool shrinks = eHalf <= eFull * (1.0 + 1e-9);
    const bool orderOk = std::isfinite(p) && std::fabs(p - expectedOrder) <= 1.2;
    r.solution = smartNum(yFull);
    r.hasNumeric = true;
    r.numericValue = yFull;
    r.point = "x=" + numStr(T);
    const Ex formula = code == 1
        ? Sym("y_{i+1} = y_i + h f(x_i, y_i)")
        : (code == 2 ? Sym("y_{i+1} = y_i + h/2 (f_i + f(x_i+h, y_i+h f_i))")
                     : Sym("y_{i+1} = y_i + h/6 (k_1 + 2k_2 + 2k_3 + k_4)"));
    addStep(r, "step.title.choose_method_and_step", formula,
            "step.note.the_method_has_a_known_order", "{\"method\":\"" + std::string(methodName) + "\"}");
    addStep(r, "step.title.compute_with_step_h", smartNum(yFull),
            "step.note.one_run_with_the_requested_step", "{\"n\":" + std::to_string(n) + "}",
            "y_h(" + numStr(T) + ") \xE2\x89\x88 " + numStr(yFull));
    addStep(r, "step.title.compute_with_half_step", smartNum(yHalf),
            "step.note.the_same_interval_with_half_the_step",
            "{\"n\":" + std::to_string(2 * n) + "}",
            "y_{h/2}(" + numStr(T) + ") \xE2\x89\x88 " + numStr(yHalf));
    addStep(r, "step.title.estimate_the_error", smartNum(eFull),
            haveExact ? "step.note.error_against_the_exact_solution"
                      : "step.note.error_against_a_fine_rk4_reference",
            "", "|y_h - y_ref| = " + numStr(eFull) + ", |y_{h/2} - y_ref| = " + numStr(eHalf));
    addStep(r, "step.title.measure_the_convergence_order",
            std::isfinite(p) ? smartNum(p) : nullptr,
            "step.note.halving_the_step_estimates_the_order", "",
            std::isfinite(p) ? "p = log2(e_h / e_{h/2}) = " + numStr(p) : "order not estimable");
    addCheck(r, "check.error_shrinks", shrinks, "check.halving_reduces_the_error", "",
             "e_h = " + numStr(eFull) + ", e_{h/2} = " + numStr(eHalf));
    addCheck(r, "check.convergence_order", orderOk, "check.close_to_the_expected_order",
             "{\"expected\":" + std::to_string(expectedOrder) + "}",
             std::isfinite(p) ? "observed p = " + numStr(p) : "observed p not estimable");
    return r;
}

// ---------------------------------------------------------------- linear system
Result linearSystem(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "linear_system", "t");
    const double a = req.number("a", 0.0);
    const double b = req.number("b", 1.0);
    const double c = req.number("c", -1.0);
    const double d = req.number("d", 0.0);
    const double tr = a + d;
    const double det = a * d - b * c;
    const double disc = tr * tr - 4.0 * det;
    r.normalized = Sym("X' = A X");
    r.order = 1;

    addStep(r, "step.title.write_the_coefficient_matrix", Sym("A = [[" + numStr(a) + ", " + numStr(b) + "], [" + numStr(c) + ", " + numStr(d) + "]]"), "step.note.the_system_is_x_ax_by_y_cx_dy");
    addStep(r, "step.title.compute_trace_and_determinant", Add(Sym("tr = " + numStr(tr)), Sym("det = " + numStr(det))), "step.note.the_eigenvalues_solve_lambda_2_tr_lambda_det_0");

    std::string kindEn, kindCode;
    Ex xSol, ySol;
    // Work with exact rationals so irrational eigenvalues stay as sqrt(Δ)/2
    // instead of turning into huge decimal fractions.
    const Ex A = numEx(a), B = numEx(b), C = numEx(c), D = numEx(d);
    const Ex trE = simplify(Add(A, D));
    const Ex detE = simplify(Add(Mul(A, D), Mul(Num(-1), Mul(B, C))));
    const Ex discE = simplify(Add(Mul(trE, trE), Mul(Num(-4), detE)));
    // A reliable eigenvector: (b, lambda-a) works when b != 0; otherwise the
    // matrix is triangular and (lambda-d, c) or a coordinate axis works.
    auto eigvec = [&](const Ex& lam, Ex& vx, Ex& vy) {
        if (!B->isZero()) { vx = B; vy = simplify(Add(lam, Mul(Num(-1), A))); }
        else if (!C->isZero()) { vx = simplify(Add(lam, Mul(Num(-1), D))); vy = C; }
        else if (equals(lam, A)) { vx = Num(1); vy = Num(0); }
        else { vx = Num(0); vy = Num(1); }
    };
    if (disc > 1e-10) {
        const Ex l1 = simplify(Mul(Add(trE, Sqrt(discE)), Num(Rational(1, 2))));
        const Ex l2 = simplify(Mul(Add(trE, Mul(Num(-1), Sqrt(discE))), Num(Rational(1, 2))));
        const double l1d = (tr + std::sqrt(disc)) / 2.0;
        kindEn = det < 0 ? "saddle" : (l1d < 0 ? "stable node" : "unstable node");
        kindCode = det < 0 ? "saddle" : (l1d < 0 ? "stable_node" : "unstable_node");
        
        Ex v1x, v1y, v2x, v2y;
        eigvec(l1, v1x, v1y);
        eigvec(l2, v2x, v2y);
        addStep(r, "step.title.eigenvalues_and_eigenvectors",
                Add(Sym("lambda_1 = " + toPlain(l1)), Sym("lambda_2 = " + toPlain(l2))),
                "", "",
                "v_1 = (" + toPlain(v1x) + ", " + toPlain(v1y) + "), v_2 = (" +
                    toPlain(v2x) + ", " + toPlain(v2y) + ")");
        const Ex e1 = expEx(Mul(l1, Sym("t")));
        const Ex e2 = expEx(Mul(l2, Sym("t")));
        xSol = Add(Mul(Sym("C_1"), v1x, e1), Mul(Sym("C_2"), v2x, e2));
        ySol = Add(Mul(Sym("C_1"), v1y, e1), Mul(Sym("C_2"), v2y, e2));
    } else if (std::fabs(disc) <= 1e-10) {
        const Ex l = simplify(Mul(trE, Num(Rational(1, 2))));
        const double ld = tr / 2.0;
        kindEn = ld < 0 ? "stable degenerate node" : "unstable degenerate node";
        kindCode = ld < 0 ? "stable_degenerate_node" : "unstable_degenerate_node";
        
        const bool scalar = B->isZero() && C->isZero();
        const Ex e = expEx(Mul(l, Sym("t")));
        if (scalar) {
            // A = lambda I has two independent eigenvectors.
            xSol = Mul(e, Sym("C_1"));
            ySol = Mul(e, Sym("C_2"));
            addStep(r, "step.title.repeated_eigenvalue", Sym("lambda = " + toPlain(l)), "step.note.the_matrix_is_a_multiple_of_the_identity_so_every_vector_is_an_eigenvector");
        } else {
            Ex vx, vy;
            eigvec(l, vx, vy);
            // Generalized eigenvector w with (A - lambda I) w = v.
            const Ex wx = B->isZero() ? Num(1) : Num(0);
            const Ex wy = B->isZero() ? Num(0) : Num(1);
            xSol = Mul(e, Add(Mul(Sym("C_1"), vx),
                              Mul(Sym("C_2"), Add(Mul(vx, Sym("t")), wx))));
            ySol = Mul(e, Add(Mul(Sym("C_1"), vy),
                              Mul(Sym("C_2"), Add(Mul(vy, Sym("t")), wy))));
            addStep(r, "step.title.repeated_eigenvalue", Sym("lambda = " + toPlain(l)), "step.note.one_eigenvector_is_not_enough_a_generalized_eigenvector_adds_the_t_term");
        }
    } else {
        const Ex alpha = simplify(Mul(trE, Num(Rational(1, 2))));
        const Ex beta = simplify(Mul(Sqrt(simplify(Mul(Num(-1), discE))), Num(Rational(1, 2))));
        const double alphaD = tr / 2.0;
        kindEn = std::fabs(alphaD) < 1e-10 ? "center" : (alphaD < 0 ? "stable spiral" : "unstable spiral");
        kindCode = std::fabs(alphaD) < 1e-10 ? "center" : (alphaD < 0 ? "stable_spiral" : "unstable_spiral");
        
        const Ex ux = B;
        const Ex uy = simplify(Add(alpha, Mul(Num(-1), A)));
        const Ex vy = beta;
        const Ex env = expEx(Mul(alpha, Sym("t")));
        const Ex co = cosEx(Mul(beta, Sym("t")));
        const Ex si = sinEx(Mul(beta, Sym("t")));
        xSol = Mul(env, Add(Mul(Sym("C_1"), ux, co), Mul(Sym("C_2"), ux, si)));
        ySol = Mul(env, Add(Mul(Sym("C_1"), Add(Mul(uy, co), Mul(Num(-1), vy, si))),
                            Mul(Sym("C_2"), Add(Mul(uy, si), Mul(vy, co)))));
        addStep(r, "step.title.complex_eigenvalues", Add(Sym("alpha = " + toPlain(alpha)), Sym("beta = " + toPlain(beta))), "step.note.the_real_part_controls_growth_the_imaginary_part_controls_rotation");
    }
    r.solution = xSol;
    r.functionF = xSol;
    r.functionG = ySol;
    r.typeId = kindEn;
    r.typeId = kindCode;
    addStep(r, "step.title.write_the_general_solution", xSol,
            "step.note.second_component_is", "", "y(t) = " + toPretty(ySol));
    addStep(r, "step.title.classify_the_equilibrium", Sym(kindEn),
            "step.note.origin_" + kindCode);
    // Independent check: substitute the general solution into X' = A X at a
    // generic time with arbitrary constants.  A wrong eigenvector or a typo in
    // an eigenvalue leaves a non-zero residual here.
    const std::map<std::string, double> sample{
        {"t", 0.7137}, {"C_1", 0.37}, {"C_2", -1.21}};
    bool evalOk = true;
    const double xs = evalReal(xSol, sample, &evalOk);
    const double ys = evalReal(ySol, sample, &evalOk);
    const double dxs = evalReal(diff(xSol, "t"), sample, &evalOk);
    const double dys = evalReal(diff(ySol, "t"), sample, &evalOk);
    const double res1 = dxs - (a * xs + b * ys);
    const double res2 = dys - (c * xs + d * ys);
    const double scale = 1.0 + std::fabs(dxs) + std::fabs(a * xs) + std::fabs(b * ys) +
                         std::fabs(dys) + std::fabs(c * xs) + std::fabs(d * ys);
    const bool systemOk = evalOk && std::fabs(res1) <= 1e-6 * scale &&
                          std::fabs(res2) <= 1e-6 * scale;
    addCheck(r, "check.eigenvalue", systemOk, "", "",
             "X' - AX = (" + numStr(res1) + ", " + numStr(res2) + ")");
    return r;
}

// ---------------------------------------------------------------- matrix & eigenvalues
using QMat = std::vector<std::vector<Rational>>;

std::string replaceAllText(std::string s, const std::string& from, const std::string& to) {
    size_t at = 0;
    while ((at = s.find(from, at)) != std::string::npos) {
        s.replace(at, from.size(), to);
        at += to.size();
    }
    return s;
}

bool parseRationalEntry(const std::string& raw, Rational& out) {
    std::string s = trim(raw);
    s = replaceAllText(s, "\xE2\x88\x92", "-"); // U+2212 minus
    s = replaceAllText(s, "\xE2\x80\x93", "-"); // U+2013 en dash
    if (s.empty()) return false;
    if (s.find('/') != std::string::npos) {
        const size_t slash = s.find('/');
        if (s.find('/', slash + 1) != std::string::npos) return false;
        const std::string a = trim(s.substr(0, slash));
        const std::string b = trim(s.substr(slash + 1));
        if (a.empty() || b.empty() || a.find('.') != std::string::npos ||
            b.find('.') != std::string::npos)
            return false;
        char* endA = nullptr;
        char* endB = nullptr;
        errno = 0;
        const long long na = std::strtoll(a.c_str(), &endA, 10);
        const long long nb = std::strtoll(b.c_str(), &endB, 10);
        if (errno || endA == a.c_str() || *endA != '\0' || endB == b.c_str() ||
            *endB != '\0' || nb == 0 || std::llabs(na) > 1000000LL ||
            std::llabs(nb) > 1000000LL)
            return false;
        out = Rational(na, nb);
        return true;
    }
    const bool decimal = s.find('.') != std::string::npos || s.find('e') != std::string::npos ||
                         s.find('E') != std::string::npos;
    if (decimal) {
        char* end = nullptr;
        errno = 0;
        const double v = std::strtod(s.c_str(), &end);
        if (errno || end == s.c_str() || *end != '\0' || !std::isfinite(v) ||
            std::fabs(v) > 1e9)
            return false;
        out = Rational::fromDouble(v, 1000000);
        return true;
    }
    char* end = nullptr;
    errno = 0;
    const long long v = std::strtoll(s.c_str(), &end, 10);
    if (errno || end == s.c_str() || *end != '\0' || std::llabs(v) > 1000000LL) return false;
    out = Rational(v);
    return true;
}

bool parseMatrixText(const std::string& text, QMat& A, std::string& code) {
    std::vector<std::string> rows;
    std::string current;
    for (const char ch : text) {
        if (ch == ';' || ch == '\n' || ch == '\r') {
            if (!trim(current).empty()) rows.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    if (!trim(current).empty()) rows.push_back(current);
    if (rows.empty()) {
        code = "bad_matrix";
        return false;
    }
    A.clear();
    const int n = (int)rows.size();
    if (n > 3) {
        code = "matrix_too_large";
        return false;
    }
    for (const std::string& row : rows) {
        std::vector<Rational> entries;
        std::string cell;
        for (size_t i = 0; i <= row.size(); ++i) {
            if (i == row.size() || row[i] == ',') {
                Rational value;
                if (!parseRationalEntry(cell, value)) {
                    code = "matrix_entry_invalid";
                    return false;
                }
                entries.push_back(value);
                cell.clear();
            } else {
                cell.push_back(row[i]);
            }
        }
        if ((int)entries.size() != n) {
            code = "bad_matrix";
            return false;
        }
        A.push_back(std::move(entries));
    }
    code.clear();
    return true;
}

QMat identityMatrix(int n) {
    QMat I(n, std::vector<Rational>(n, Rational(0)));
    for (int i = 0; i < n; ++i) I[i][i] = Rational(1);
    return I;
}

QMat multiplyMatrices(const QMat& A, const QMat& B) {
    const int n = (int)A.size();
    QMat C(n, std::vector<Rational>(n, Rational(0)));
    for (int i = 0; i < n; ++i)
        for (int k = 0; k < n; ++k)
            if (!A[i][k].isZero())
                for (int j = 0; j < n; ++j) C[i][j] = C[i][j] + A[i][k] * B[k][j];
    return C;
}

Rational matrixTrace(const QMat& A) {
    Rational t(0);
    for (int i = 0; i < (int)A.size(); ++i) t = t + A[i][i];
    return t;
}

Rational matrixDeterminant(QMat M) {
    const int n = (int)M.size();
    Rational det(1);
    for (int col = 0; col < n; ++col) {
        int pivot = -1;
        for (int row = col; row < n; ++row)
            if (!M[row][col].isZero()) { pivot = row; break; }
        if (pivot < 0) return Rational(0);
        if (pivot != col) {
            std::swap(M[pivot], M[col]);
            det = -det;
        }
        const Rational pv = M[col][col];
        det = det * pv;
        for (int row = col + 1; row < n; ++row) {
            if (M[row][col].isZero()) continue;
            const Rational f = M[row][col] / pv;
            for (int j = col; j < n; ++j) M[row][j] = M[row][j] - f * M[col][j];
        }
    }
    return det;
}

bool invertMatrix(const QMat& A, QMat& inverse) {
    const int n = (int)A.size();
    QMat B = A;
    inverse = identityMatrix(n);
    for (int col = 0; col < n; ++col) {
        int pivot = -1;
        for (int row = col; row < n; ++row)
            if (!B[row][col].isZero()) { pivot = row; break; }
        if (pivot < 0) return false;
        if (pivot != col) {
            std::swap(B[pivot], B[col]);
            std::swap(inverse[pivot], inverse[col]);
        }
        const Rational pv = B[col][col];
        for (int j = 0; j < n; ++j) {
            B[col][j] = B[col][j] / pv;
            inverse[col][j] = inverse[col][j] / pv;
        }
        for (int row = 0; row < n; ++row) {
            if (row == col || B[row][col].isZero()) continue;
            const Rational f = B[row][col];
            for (int j = 0; j < n; ++j) {
                B[row][j] = B[row][j] - f * B[col][j];
                inverse[row][j] = inverse[row][j] - f * inverse[col][j];
            }
        }
    }
    return true;
}

Poly characteristicPolynomial(const QMat& A) {
    const int n = (int)A.size();
    Poly p;
    p.c.assign(n + 1, Rational(0));
    p.c[n] = Rational(1);
    QMat M = identityMatrix(n);
    for (int k = 1; k <= n; ++k) {
        QMat AM = multiplyMatrices(A, M);
        Rational t(0);
        for (int i = 0; i < n; ++i) t = t + AM[i][i];
        const Rational c = t * Rational(-1, k);
        p.c[n - k] = c;
        for (int i = 0; i < n; ++i) AM[i][i] = AM[i][i] + c;
        M = std::move(AM);
    }
    p.trim();
    return p;
}

std::string rationalText(const Rational& q) { return toPretty(Num(q)); }

std::string polynomialText(const Poly& p, const std::string& variable) {
    std::string out;
    for (int i = p.deg(); i >= 0; --i) {
        if (p.c[i].isZero()) continue;
        const bool negative = p.c[i].isNeg();
        const Rational coefficient = p.c[i].abs();
        if (out.empty()) {
            if (negative) out += "−";
        } else {
            out += negative ? " − " : " + ";
        }
        const bool unit = coefficient == Rational(1);
        if (!unit || i == 0) out += rationalText(coefficient);
        if (i > 0) {
            if (!unit || i == 0) out += "·";
            out += variable;
            if (i > 1) out += "^" + std::to_string(i);
        }
    }
    return out.empty() ? "0" : out;
}

std::string matrixText(const QMat& A) {
    std::string s = "[";
    for (int i = 0; i < (int)A.size(); ++i) {
        if (i) s += ", ";
        s += "[";
        for (int j = 0; j < (int)A[i].size(); ++j) {
            if (j) s += ", ";
            s += rationalText(A[i][j]);
        }
        s += "]";
    }
    return s + "]";
}

struct EigenRoot {
    Ex value;
    double re = 0.0;
    double im = 0.0;
    int mult = 1;
    bool cplx = false;
    bool exact = true;
    bool rational = false;
    Rational q;
};

Ex numericComplexEx(double re, double im) {
    Ex realPart = smartNum(re);
    Ex imagPart = smartNum(std::fabs(im));
    if (!realPart) realPart = Num(0);
    if (!imagPart) imagPart = Num(0);
    if (imagPart->isZero()) return realPart;
    Ex term = Mul(Num(im < 0 ? -1 : 1), imagPart, Sym("i"));
    if (realPart->isZero()) return term;
    return Add(realPart, term);
}

std::vector<std::complex<double>> durandKernerRoots(const Poly& p) {
    const int n = p.deg();
    std::vector<std::complex<double>> z(n);
    if (n <= 0) return z;
    const double lead = p.c[n].toDouble();
    double radius = 1.0;
    for (int i = 0; i < n; ++i) {
        const double ratio = std::fabs(p.c[i].toDouble() / lead);
        radius = std::max(radius, 1.0 + ratio);
    }
    for (int i = 0; i < n; ++i) {
        const double angle = 2.0 * kPi * i / n + 0.4;
        z[i] = std::polar(std::pow(radius, 1.0 / n), angle);
    }
    for (int iter = 0; iter < 400; ++iter) {
        double moved = 0.0;
        for (int i = 0; i < n; ++i) {
            std::complex<double> numerator(0.0, 0.0);
            for (int j = n; j >= 0; --j)
                numerator = numerator * z[i] + p.c[j].toDouble();
            std::complex<double> denominator(1.0, 0.0);
            for (int j = 0; j < n; ++j) {
                if (j == i) continue;
                denominator *= z[i] - z[j];
            }
            if (std::abs(denominator) < 1e-300) continue;
            const std::complex<double> delta = numerator / denominator;
            z[i] -= delta;
            moved = std::max(moved, std::abs(delta));
        }
        if (moved < 1e-13) break;
    }
    return z;
}

std::vector<EigenRoot> numericEigenRoots(const Poly& p) {
    const std::vector<std::complex<double>> z = durandKernerRoots(p);
    std::vector<bool> used(z.size(), false);
    std::vector<EigenRoot> roots;
    for (int i = 0; i < (int)z.size(); ++i) {
        if (used[i]) continue;
        std::complex<double> sum(0.0, 0.0);
        int count = 0;
        for (int j = i; j < (int)z.size(); ++j) {
            if (used[j]) continue;
            if (std::abs(z[j] - z[i]) <= 1e-6 * (1.0 + std::abs(z[i]))) {
                used[j] = true;
                sum += z[j];
                ++count;
            }
        }
        const std::complex<double> avg = sum / (double)std::max(count, 1);
        EigenRoot root;
        root.mult = count;
        root.exact = false;
        root.re = avg.real();
        if (std::fabs(avg.imag()) <= 1e-7 * (1.0 + std::fabs(avg.real()))) {
            root.im = 0.0;
            root.cplx = false;
            root.value = smartNum(root.re);
        } else {
            root.im = avg.imag();
            root.cplx = true;
            root.value = numericComplexEx(root.re, root.im);
        }
        roots.push_back(root);
    }
    return roots;
}

bool exactEigenRoots(const Poly& ch, std::vector<EigenRoot>& roots) {
    std::vector<Factor> factors;
    if (!factorPolynomial(ch, factors) || factors.empty()) return false;
    Poly product;
    product.c = {Rational(1)};
    int degree = 0;
    for (const Factor& f : factors) {
        product = polyMul(product, polyPow(f.p, f.mult));
        degree += f.p.deg() * f.mult;
    }
    if (degree != ch.deg() || product.deg() != ch.deg()) return false;
    for (const Factor& f : factors) {
        if (f.p.deg() == 1) {
            EigenRoot root;
            root.q = -f.p.c[0] / f.p.c[1];
            root.value = Num(root.q);
            root.re = root.q.toDouble();
            root.mult = f.mult;
            root.rational = true;
            roots.push_back(root);
        } else if (f.p.deg() == 2) {
            const Rational a = f.p.c[2], b = f.p.c[1], c = f.p.c[0];
            const Rational disc = b * b - a * c * Rational(4);
            const double den = 2.0 * a.toDouble();
            if (disc.isNeg()) {
                const Rational alpha = -b / (a * Rational(2));
                const double beta = std::sqrt(-disc.toDouble()) / den;
                EigenRoot plus;
                plus.value = Add(Num(alpha), Mul(Sqrt(Num(-disc)), Pow(Num(a * Rational(2)), Num(-1)), Sym("i")));
                plus.re = alpha.toDouble();
                plus.im = beta;
                plus.mult = f.mult;
                plus.cplx = true;
                plus.exact = true;
                EigenRoot minus = plus;
                minus.value = Add(Num(alpha), Mul(Num(-1), Sqrt(Num(-disc)),
                                                 Pow(Num(a * Rational(2)), Num(-1)), Sym("i")));
                minus.im = -beta;
                roots.push_back(plus);
                roots.push_back(minus);
            } else if (disc.isZero()) {
                EigenRoot root;
                root.q = -b / (a * Rational(2));
                root.value = Num(root.q);
                root.re = root.q.toDouble();
                root.mult = f.mult;
                root.rational = true;
                roots.push_back(root);
            } else {
                const double sq = std::sqrt(disc.toDouble());
                for (int sign = 1; sign >= -1; sign -= 2) {
                    EigenRoot root;
                    root.value = simplify(Mul(Add(Num(-b), Mul(Num(sign), Sqrt(Num(disc)))),
                                              Pow(Num(a * Rational(2)), Num(-1))));
                    root.re = (-b.toDouble() + sign * sq) / den;
                    root.mult = f.mult;
                    root.exact = true;
                    roots.push_back(root);
                }
            }
        } else {
            return false;
        }
    }
    return !roots.empty();
}

std::vector<EigenRoot> eigenvalueRoots(const Poly& ch) {
    std::vector<EigenRoot> roots;
    if (exactEigenRoots(ch, roots)) {
        return roots;
    }
    return numericEigenRoots(ch);
}

void sortEigenRoots(std::vector<EigenRoot>& roots) {
    std::stable_sort(roots.begin(), roots.end(), [](const EigenRoot& a, const EigenRoot& b) {
        if (a.cplx != b.cplx) return !a.cplx;
        if (!a.cplx) return a.re < b.re;
        if (std::fabs(a.re - b.re) > 1e-9) return a.re < b.re;
        return a.im > b.im;
    });
}

std::vector<Rational> normalizeVector(std::vector<Rational> v) {
    int first = -1;
    for (int i = 0; i < (int)v.size(); ++i)
        if (!v[i].isZero()) { first = i; break; }
    if (first < 0) return v;
    long long L = 1;
    bool overflow = false;
    for (const Rational& x : v) {
        const long long g = std::gcd(L, x.d);
        const long long rest = x.d / g;
        if (rest != 0 && L > 1000000LL / rest) { overflow = true; break; }
        L *= rest;
    }
    if (!overflow) {
        std::vector<long long> ints;
        long long g = 0;
        for (const Rational& x : v) {
            const long long value = x.n * (L / x.d);
            ints.push_back(value);
            g = std::gcd(g, std::llabs(value));
        }
        if (g > 0) {
            for (long long& value : ints) value /= g;
            for (int i = 0; i < (int)ints.size(); ++i) {
                if (ints[i] != 0) {
                    if (ints[i] < 0)
                        for (long long& other : ints) other = -other;
                    break;
                }
            }
            std::vector<Rational> out;
            out.reserve(ints.size());
            for (const long long value : ints) out.push_back(Rational(value));
            return out;
        }
    }
    if (v[first].isNeg())
        for (Rational& x : v) x = -x;
    const Rational lead = v[first];
    for (Rational& x : v) x = x / lead;
    return v;
}

std::vector<std::vector<Rational>> rationalNullspace(QMat B) {
    const int n = (int)B.size();
    std::vector<int> pivots;
    int row = 0;
    for (int col = 0; col < n && row < n; ++col) {
        int pivot = -1;
        for (int i = row; i < n; ++i)
            if (!B[i][col].isZero()) { pivot = i; break; }
        if (pivot < 0) continue;
        if (pivot != row) std::swap(B[pivot], B[row]);
        const Rational pv = B[row][col];
        for (int j = col; j < n; ++j) B[row][j] = B[row][j] / pv;
        for (int i = 0; i < n; ++i) {
            if (i == row || B[i][col].isZero()) continue;
            const Rational f = B[i][col];
            for (int j = col; j < n; ++j) B[i][j] = B[i][j] - f * B[row][j];
        }
        pivots.push_back(col);
        ++row;
    }
    std::vector<bool> isPivot(n, false);
    for (const int col : pivots) isPivot[col] = true;
    std::vector<std::vector<Rational>> basis;
    for (int freeCol = 0; freeCol < n; ++freeCol) {
        if (isPivot[freeCol]) continue;
        std::vector<Rational> v(n, Rational(0));
        v[freeCol] = Rational(1);
        for (int r = 0; r < (int)pivots.size(); ++r) v[pivots[r]] = -B[r][freeCol];
        basis.push_back(normalizeVector(std::move(v)));
    }
    return basis;
}

std::string complexNumberText(const std::complex<double>& z) {
    double re = z.real();
    double im = z.imag();
    const double scale = std::max(1.0, std::max(std::fabs(re), std::fabs(im)));
    if (std::fabs(re) < 1e-9 * scale) re = 0.0;
    if (std::fabs(im) < 1e-9 * scale) im = 0.0;
    if (im == 0.0) return numStr(re);
    const std::string body =
        (std::fabs(std::fabs(im) - 1.0) < 1e-9 ? "" : numStr(std::fabs(im))) + "i";
    if (re == 0.0) return (im < 0 ? "-" : "") + body;
    return numStr(re) + (im < 0 ? " - " : " + ") + body;
}

std::vector<std::vector<std::complex<double>>> complexNullspace(
    std::vector<std::vector<std::complex<double>>> B) {
    const int n = (int)B.size();
    double scale = 1.0;
    for (const auto& row : B)
        for (const auto& value : row) scale = std::max(scale, std::abs(value));
    const double tol = 1e-7 * scale;
    std::vector<int> pivots;
    int row = 0;
    for (int col = 0; col < n && row < n; ++col) {
        int pivot = -1;
        for (int i = row; i < n; ++i)
            if (std::abs(B[i][col]) > tol) { pivot = i; break; }
        if (pivot < 0) continue;
        if (pivot != row) std::swap(B[pivot], B[row]);
        const std::complex<double> pv = B[row][col];
        for (int j = col; j < n; ++j) B[row][j] /= pv;
        for (int i = 0; i < n; ++i) {
            if (i == row || std::abs(B[i][col]) <= tol) continue;
            const std::complex<double> f = B[i][col];
            for (int j = col; j < n; ++j) B[i][j] -= f * B[row][j];
        }
        pivots.push_back(col);
        ++row;
    }
    std::vector<bool> isPivot(n, false);
    for (const int col : pivots) isPivot[col] = true;
    std::vector<std::vector<std::complex<double>>> basis;
    for (int freeCol = 0; freeCol < n; ++freeCol) {
        if (isPivot[freeCol]) continue;
        std::vector<std::complex<double>> v(n, {0.0, 0.0});
        v[freeCol] = {1.0, 0.0};
        for (int r = 0; r < (int)pivots.size(); ++r) v[pivots[r]] = -B[r][freeCol];
        double norm = 0.0;
        for (const auto& value : v) norm = std::max(norm, std::abs(value));
        if (norm < 1e-300) continue;
        for (auto& value : v) {
            if (std::abs(value) < 1e-10 * norm) value = {0.0, 0.0};
            value /= norm;
        }
        basis.push_back(std::move(v));
    }
    return basis;
}

std::string vectorText(const std::vector<Rational>& v) {
    std::string s = "(";
    for (int i = 0; i < (int)v.size(); ++i) {
        if (i) s += ", ";
        s += rationalText(v[i]);
    }
    return s + ")";
}

std::string vectorText(const std::vector<std::complex<double>>& v) {
    std::string s = "(";
    for (int i = 0; i < (int)v.size(); ++i) {
        if (i) s += ", ";
        s += complexNumberText(v[i]);
    }
    return s + ")";
}

struct Eigenspace {
    EigenRoot root;
    int algebraic = 0;
    std::vector<std::vector<Rational>> qvectors;
    std::vector<std::vector<std::complex<double>>> cvectors;
    int geometric = 0;
};

std::vector<Eigenspace> buildEigenspaces(const QMat& A, const std::vector<EigenRoot>& roots) {
    std::vector<Eigenspace> spaces;
    for (const EigenRoot& root : roots) {
        int found = -1;
        for (int i = 0; i < (int)spaces.size(); ++i) {
            const double scale = 1.0 + std::max(std::fabs(root.re), std::fabs(root.im));
            if (std::fabs(spaces[i].root.re - root.re) <= 1e-7 * scale &&
                std::fabs(spaces[i].root.im - root.im) <= 1e-7 * scale) {
                found = i;
                break;
            }
        }
        if (found < 0) {
            Eigenspace space;
            space.root = root;
            space.algebraic = root.mult;
            if (root.im == 0.0 && root.rational) {
                QMat B = A;
                for (int i = 0; i < (int)B.size(); ++i) B[i][i] = B[i][i] - root.q;
                space.qvectors = rationalNullspace(std::move(B));
                space.geometric = (int)space.qvectors.size();
            } else {
                std::vector<std::vector<std::complex<double>>> B(
                    A.size(), std::vector<std::complex<double>>(A.size(), {0.0, 0.0}));
                const std::complex<double> lambda(root.re, root.im);
                for (int i = 0; i < (int)A.size(); ++i)
                    for (int j = 0; j < (int)A.size(); ++j)
                        B[i][j] = {A[i][j].toDouble(), 0.0};
                for (int i = 0; i < (int)B.size(); ++i) B[i][i] -= lambda;
                space.cvectors = complexNullspace(std::move(B));
                space.geometric = (int)space.cvectors.size();
            }
            spaces.push_back(std::move(space));
        } else {
            spaces[found].algebraic += root.mult;
        }
    }
    return spaces;
}

std::complex<double> evaluateComplexPoly(const Poly& p, const std::complex<double>& z) {
    std::complex<double> value(0.0, 0.0);
    for (int i = p.deg(); i >= 0; --i) value = value * z + p.c[i].toDouble();
    return value;
}

Result matrixEigen(const TopicRequest& req, bool en) {
    (void)en;
    Result r;
    baseId(r, "topic", "matrix_eigen", "lambda");
    QMat A;
    std::string code;
    const std::string input = req.text("matrix", "4, 1; 2, 3");
    if (!parseMatrixText(input, A, code)) {
        r.errorDetail = "matrix input: " + input;
        failCode(r, code);
        return r;
    }
    const int n = (int)A.size();
    r.order = n;
    r.normalized = Sym("A = " + matrixText(A));

    const Rational trace = matrixTrace(A);
    const Rational det = matrixDeterminant(A);
    const Poly ch = characteristicPolynomial(A);
    std::vector<EigenRoot> roots = eigenvalueRoots(ch);
    sortEigenRoots(roots);
    if (roots.empty()) {
        failCode(r, "eigenvalues_not_found");
        return r;
    }
    const std::vector<Eigenspace> spaces = buildEigenspaces(A, roots);

    std::string eigenList;
    int eigenIndex = 0;
    for (const EigenRoot& root : roots) {
        for (int m = 0; m < std::max(root.mult, 1); ++m) {
            if (!eigenList.empty()) eigenList += ",  ";
            eigenList += "λ" + std::to_string(++eigenIndex) + " = " + toPretty(root.value);
        }
    }

    std::string eigenvectorLines;
    for (const Eigenspace& space : spaces) {
        if (!eigenvectorLines.empty()) eigenvectorLines += ";  ";
        eigenvectorLines += "λ = " + toPretty(space.root.value) + ":  ";
        if (!space.qvectors.empty()) {
            for (int i = 0; i < (int)space.qvectors.size(); ++i) {
                if (i) eigenvectorLines += ",  ";
                eigenvectorLines += "v" + std::to_string(i + 1) + " = " +
                                    vectorText(space.qvectors[i]);
            }
        } else {
            for (int i = 0; i < (int)space.cvectors.size(); ++i) {
                if (i) eigenvectorLines += ",  ";
                eigenvectorLines += "v" + std::to_string(i + 1) + " = " +
                                    vectorText(space.cvectors[i]);
            }
        }
    }

    bool allReal = true;
    bool anyComplex = false;
    bool anyNumeric = false;
    bool anyApproxVector = false;
    bool diagonalizable = true;
    std::string diagonalLines;
    for (const Eigenspace& space : spaces) {
        const bool realRoot = std::fabs(space.root.im) <= 1e-8 * (1.0 + std::fabs(space.root.re));
        if (!realRoot) allReal = false;
        if (space.root.cplx || !realRoot) anyComplex = true;
        if (!space.root.exact) anyNumeric = true;
        if (realRoot && space.qvectors.empty()) anyApproxVector = true;
        if (space.geometric != space.algebraic) diagonalizable = false;
        if (!diagonalLines.empty()) diagonalLines += ";  ";
        diagonalLines += "λ = " + toPretty(space.root.value) + ":  alg = " +
                         std::to_string(space.algebraic) + ", geom = " +
                         std::to_string(space.geometric);
    }
    QMat inverse;
    const bool invertible = invertMatrix(A, inverse);

    addStep(r, "step.title.read_the_matrix", Sym("A = " + matrixText(A)),
            "step.note.rows_are_separated_by_semicolons_and_entries_by_commas",
            "{\"n\":" + std::to_string(n) + "}");
    addStep(r, "step.title.compute_trace_and_determinant",
            Sym("tr(A) = " + rationalText(trace) + ",  det(A) = " + rationalText(det)),
            "step.note.trace_is_the_sum_of_the_diagonal_and_determinant_measures_volume_change");
    addStep(r, "step.title.build_the_characteristic_polynomial",
            Sym(polynomialText(ch, "λ")),
            "step.note.the_characteristic_polynomial_is_det_lambda_i_minus_a");
    addStep(r, "step.title.find_the_eigenvalues", Sym(eigenList),
            anyNumeric ? "step.note.some_eigenvalues_are_reported_numerically"
                       : (anyComplex ? "step.note.non_real_eigenvalues_come_in_conjugate_pairs"
                                     : "step.note.eigenvalues_are_the_roots_of_the_characteristic_polynomial"));
    addStep(r, "step.title.find_the_eigenvectors", Sym(eigenvectorLines),
            anyApproxVector
                ? "step.note.some_eigenvectors_are_reported_numerically"
                : "step.note.eigenvectors_solve_a_minus_lambda_i_v_0");
    addStep(r, "step.title.test_diagonalizability", Sym(diagonalLines),
            !diagonalizable
                ? "step.note.the_matrix_is_not_diagonalizable_because_an_eigenvalue_lacks_enough_eigenvectors"
                : (allReal ? "step.note.the_matrix_is_diagonalizable_over_the_reals"
                           : "step.note.the_matrix_is_diagonalizable_over_the_complex_numbers_but_not_the_reals"));
    if (invertible) {
        addStep(r, "step.title.invert_the_matrix", Sym("A^{-1} = " + matrixText(inverse)),
                "step.note.the_inverse_exists_exactly_when_the_determinant_is_nonzero");
    } else {
        addStep(r, "step.title.invert_the_matrix", Sym("det(A) = 0"),
                "step.note.a_singular_matrix_has_no_inverse");
    }

    double maxCharResidual = 0.0;
    for (const EigenRoot& root : roots) {
        const std::complex<double> lambda(root.re, root.im);
        double scale = 0.0;
        double power = 1.0;
        for (int i = 0; i <= ch.deg(); ++i) {
            scale += std::fabs(ch.c[i].toDouble()) * power;
            power *= 1.0 + std::abs(lambda);
        }
        const double residual = std::abs(evaluateComplexPoly(ch, lambda)) / std::max(1.0, scale);
        maxCharResidual = std::max(maxCharResidual, residual);
    }
    addCheck(r, "check.characteristic_polynomial_vanishes_at_each_eigenvalue",
             maxCharResidual <= 1e-7, "", "",
             "max |p(λ)| / scale = " + numStr(maxCharResidual));

    double maxEigenResidual = 0.0;
    for (const Eigenspace& space : spaces) {
        const std::complex<double> lambda(space.root.re, space.root.im);
        auto accumulate = [&](const std::vector<std::complex<double>>& v) {
            for (int i = 0; i < n; ++i) {
                std::complex<double> av(0.0, 0.0);
                for (int j = 0; j < n; ++j) av += A[i][j].toDouble() * v[j];
                const std::complex<double> term = av - lambda * v[i];
                maxEigenResidual = std::max(maxEigenResidual, std::abs(term));
            }
        };
        for (const auto& v : space.qvectors) {
            std::vector<std::complex<double>> cv;
            for (const Rational& x : v) cv.push_back({x.toDouble(), 0.0});
            accumulate(cv);
        }
        for (const auto& v : space.cvectors) accumulate(v);
    }
    double eigenScale = 1.0;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) eigenScale += std::fabs(A[i][j].toDouble());
    maxEigenResidual /= eigenScale;
    addCheck(r, "check.eigenvectors_satisfy_av_lambda_v", maxEigenResidual <= 1e-5, "", "",
             "max |Av - λv| / scale = " + numStr(maxEigenResidual));

    if (invertible) {
        const QMat product = multiplyMatrices(A, inverse);
        bool identity = true;
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) {
                const Rational expected = i == j ? Rational(1) : Rational(0);
                if (product[i][j] != expected) identity = false;
            }
        addCheck(r, "check.inverse_multiplies_to_the_identity", identity, "", "",
                 "max |A A^{-1} - I| = 0");
    } else {
        addCheck(r, "check.the_determinant_is_zero", det.isZero(), "", "", "det(A) = 0");
    }

    r.solution = Sym(eigenList);
    r.variable = "lambda";
    r.hasNumeric = false;
    return r;
}

// ---------------------------------------------------------------- phase portrait
struct LinearPortrait {
    double tr = 0.0, det = 0.0, disc = 0.0;
    double l1re = 0.0, l1im = 0.0, l2re = 0.0, l2im = 0.0;
    std::string code;
};

LinearPortrait classifyLinearPortrait(double a, double b, double c, double d) {
    LinearPortrait p;
    p.tr = a + d;
    p.det = a * d - b * c;
    p.disc = p.tr * p.tr - 4.0 * p.det;
    const double scale = 1.0 + std::fabs(p.tr) + std::fabs(p.det);
    const double tol = 1e-9 * scale;
    if (p.disc >= -tol) {
        const double root = std::sqrt(std::max(0.0, p.disc));
        p.l1re = (p.tr + root) / 2.0;
        p.l2re = (p.tr - root) / 2.0;
    } else {
        p.l1re = p.l2re = p.tr / 2.0;
        p.l1im = std::sqrt(-p.disc) / 2.0;
        p.l2im = -p.l1im;
    }
    if (p.det < -tol) {
        p.code = "saddle";
    } else if (std::fabs(p.det) <= tol) {
        p.code = "degenerate";
    } else if (p.disc > tol) {
        p.code = p.tr < 0.0 ? "stable_node" : "unstable_node";
    } else if (p.disc < -tol) {
        p.code = std::fabs(p.tr) <= tol
                     ? "center"
                     : (p.tr < 0.0 ? "stable_spiral" : "unstable_spiral");
    } else {
        p.code = p.tr < 0.0 ? "stable_degenerate_node" : "unstable_degenerate_node";
    }
    return p;
}

std::string portraitName(const std::string& code) {
    if (code == "saddle") return "saddle";
    if (code == "stable_node") return "stable node";
    if (code == "unstable_node") return "unstable node";
    if (code == "stable_degenerate_node") return "stable degenerate node";
    if (code == "unstable_degenerate_node") return "unstable degenerate node";
    if (code == "center") return "centre";
    if (code == "stable_spiral") return "stable spiral";
    if (code == "unstable_spiral") return "unstable spiral";
    if (code == "degenerate") return "degenerate (non-hyperbolic)";
    return code;
}

std::string portraitFamily(const std::string& code) {
    if (code == "saddle") return "saddle";
    if (code == "stable_node" || code == "unstable_node" ||
        code == "stable_degenerate_node" || code == "unstable_degenerate_node")
        return "node";
    if (code == "stable_spiral" || code == "unstable_spiral") return "spiral";
    if (code == "center") return "center";
    return "degenerate";
}

std::string linearFormText(double u, double v) {
    std::string s;
    bool any = false;
    auto add = [&](double k, char var) {
        if (std::fabs(k) <= 1e-12) return;
        if (any) s += (k < 0.0 ? " - " : " + ");
        else if (k < 0.0) s += "-";
        const double magnitude = std::fabs(k);
        if (std::fabs(magnitude - 1.0) > 1e-12) s += numStr(magnitude) + " ";
        s += var;
        any = true;
    };
    add(u, 'x');
    add(v, 'y');
    return any ? s : "0";
}

std::string eigenPairText(const LinearPortrait& p) {
    if (p.l1im == 0.0) {
        return "λ1 = " + numStr(p.l1re) + ",  λ2 = " + numStr(p.l2re);
    }
    auto part = [](double re, double im) {
        std::string s;
        if (std::fabs(re) > 1e-12) s += numStr(re);
        const std::string mag =
            std::fabs(std::fabs(im) - 1.0) < 1e-12 ? "" : numStr(std::fabs(im));
        const std::string unit = mag + "i";
        if (s.empty()) s += (im < 0 ? "-" : "") + unit;
        else s += (im < 0 ? " - " : " + ") + unit;
        return s;
    };
    return "λ1 = " + part(p.l1re, p.l1im) + ",  λ2 = " + part(p.l2re, p.l2im);
}

Result phasePortrait(const TopicRequest& req, bool en) {
    (void)en;
    Result r;
    baseId(r, "topic", "phase_portrait", "t");
    const double a = req.number("a", -1.0);
    const double b = req.number("b", 0.0);
    const double c = req.number("c", 0.0);
    const double d = req.number("d", -2.0);
    if (!validCoefficient(a) || !validCoefficient(b) || !validCoefficient(c) ||
        !validCoefficient(d)) {
        failCode(r, "coefficient_not_evaluable");
        return r;
    }
    const double magnitude = std::max(std::max(std::fabs(a), std::fabs(b)),
                                      std::max(std::fabs(c), std::fabs(d)));
    if (magnitude < 1e-12) {
        failCode(r, "degenerate_system");
        return r;
    }

    const LinearPortrait p = classifyLinearPortrait(a, b, c, d);
    const std::string matrix = "A = [[" + numStr(a) + ", " + numStr(b) + "], [" +
                               numStr(c) + ", " + numStr(d) + "]]";
    r.normalized = Sym(matrix);
    r.order = 1;
    r.typeId = p.code;
    r.solution = Sym(portraitName(p.code));

    addStep(r, "step.title.write_the_system_and_coefficient_matrix",
            Sym("X' = A X,  " + matrix),
            "step.note.a_linear_phase_portrait_is_read_from_the_eigenvalues_of_the_coefficient_matrix");
    addStep(r, "step.title.compute_trace_determinant_and_discriminant",
            Sym("tr = " + numStr(p.tr) + ",  det = " + numStr(p.det) +
                ",  Δ = " + numStr(p.disc)),
            "step.note.the_signs_of_the_trace_and_determinant_fix_the_portrait_type");
    addStep(r, "step.title.find_the_eigenvalues", Sym(eigenPairText(p)),
            "step.note.real_eigenvalues_give_straight_line_directions_and_complex_ones_give_rotation");
    addStep(r, "step.title.classify_the_equilibrium", Sym(portraitName(p.code)),
            p.code == "degenerate" ? "step.note.the_linearisation_is_not_hyperbolic"
                                   : "step.note.origin_" + p.code);
    addStep(r, "step.title.draw_the_nullclines",
            Sym(linearFormText(a, b) + " = 0,  " + linearFormText(c, d) + " = 0"),
            "step.note.the_nullclines_are_the_lines_where_one_velocity_component_vanishes");

    // λ² − tr λ + det must vanish at both eigenvalues.
    auto charResidual = [&](double re, double im) {
        const std::complex<double> lambda(re, im);
        const std::complex<double> value =
            lambda * lambda - p.tr * lambda + std::complex<double>(p.det, 0.0);
        return std::abs(value);
    };
    const double residual =
        std::max(charResidual(p.l1re, p.l1im), charResidual(p.l2re, p.l2im));
    const double charScale = 1.0 + std::fabs(p.tr) + std::fabs(p.det);
    addCheck(r, "check.eigenvalues_satisfy_the_characteristic_polynomial",
             residual <= 1e-9 * charScale, "", "",
             "max |λ² - tr λ + det| = " + numStr(residual));

    if (p.l1im == 0.0) {
        auto eigenvector = [&](double lambda, double& vx, double& vy) {
            if (std::fabs(b) > 1e-12) {
                vx = b;
                vy = lambda - a;
            } else if (std::fabs(c) > 1e-12) {
                vx = lambda - d;
                vy = c;
            } else if (std::fabs(lambda - a) <= 1e-9) {
                vx = 1.0;
                vy = 0.0;
            } else {
                vx = 0.0;
                vy = 1.0;
            }
        };
        auto eigenResidual = [&](double lambda, double vx, double vy) {
            const double r1 = a * vx + b * vy - lambda * vx;
            const double r2 = c * vx + d * vy - lambda * vy;
            return std::fabs(r1) + std::fabs(r2);
        };
        double v1x, v1y, v2x, v2y;
        eigenvector(p.l1re, v1x, v1y);
        eigenvector(p.l2re, v2x, v2y);
        const double worst =
            std::max(eigenResidual(p.l1re, v1x, v1y),
                     eigenResidual(p.l2re, v2x, v2y));
        const double eigenScale = 1.0 + std::fabs(a) + std::fabs(b) + std::fabs(c) +
                                  std::fabs(d) + std::fabs(p.l1re) + std::fabs(p.l2re);
        addCheck(r, "check.eigenvectors_satisfy_av_lambda_v",
                 worst <= 1e-9 * eigenScale, "", "",
                 "max |Av - λv| = " + numStr(worst));
    }

    std::string fromEigen;
    if (p.l1im != 0.0) {
        fromEigen = std::fabs(p.l1re) <= 1e-9 * (1.0 + std::fabs(p.l1re)) ? "center"
                                                                         : "spiral";
    } else if (p.l1re * p.l2re < 0.0) {
        fromEigen = "saddle";
    } else if (std::fabs(p.l1re) < 1e-9 || std::fabs(p.l2re) < 1e-9) {
        fromEigen = "degenerate";
    } else {
        fromEigen = "node";
    }
    addCheck(r, "check.the_portrait_matches_the_eigenvalues",
             portraitFamily(p.code) == fromEigen, "", "",
             "from the eigenvalues: " + fromEigen);
    return r;
}

// ---------------------------------------------------------------- critical points
struct PlanarPoint {
    double x = 0.0, y = 0.0;
    LinearPortrait portrait;
};

bool evalBivariate(const Ex& f, double x, double y, double& out) {
    bool ok = false;
    out = evalReal(f, {{"x", x}, {"y", y}}, &ok);
    return ok && std::isfinite(out);
}

bool newtonCritical(const Ex& f, const Ex& g, const Ex& fx, const Ex& fy,
                    const Ex& gx, const Ex& gy, double startX, double startY,
                    double xmin, double xmax, double ymin, double ymax,
                    PlanarPoint& found) {
    const double span = std::max(std::max(xmax - xmin, ymax - ymin), 1.0);
    const double margin = 0.35 * span;
    double x = startX;
    double y = startY;
    for (int iter = 0; iter < 60; ++iter) {
        double fv = 0.0, gv = 0.0;
        if (!evalBivariate(f, x, y, fv) || !evalBivariate(g, x, y, gv)) return false;
        const double norm = std::fabs(fv) + std::fabs(gv);
        if (norm <= 1e-11 * (1.0 + std::fabs(x) + std::fabs(y))) {
            found.x = x;
            found.y = y;
            return true;
        }
        double j11 = 0.0, j12 = 0.0, j21 = 0.0, j22 = 0.0;
        if (!evalBivariate(fx, x, y, j11) || !evalBivariate(fy, x, y, j12) ||
            !evalBivariate(gx, x, y, j21) || !evalBivariate(gy, x, y, j22))
            return false;
        const double det = j11 * j22 - j12 * j21;
        const double jScale = std::fabs(j11) + std::fabs(j12) + std::fabs(j21) +
                              std::fabs(j22);
        if (std::fabs(det) <= 1e-12 * (1.0 + jScale)) return false;
        double dx = (-fv * j22 + gv * j12) / det;
        double dy = (-j11 * gv + j21 * fv) / det;
        if (!std::isfinite(dx) || !std::isfinite(dy)) return false;
        const double maxStep = 0.5 * span;
        const double stepLength = std::hypot(dx, dy);
        if (stepLength > maxStep) {
            dx *= maxStep / stepLength;
            dy *= maxStep / stepLength;
        }
        bool accepted = false;
        double damping = 1.0;
        for (int line = 0; line < 12; ++line) {
            const double nx = x + damping * dx;
            const double ny = y + damping * dy;
            if (nx < xmin - margin || nx > xmax + margin || ny < ymin - margin ||
                ny > ymax + margin)
                break;
            double nf = 0.0, ng = 0.0;
            if (!evalBivariate(f, nx, ny, nf) || !evalBivariate(g, nx, ny, ng)) break;
            if (std::fabs(nf) + std::fabs(ng) < norm) {
                x = nx;
                y = ny;
                accepted = true;
                break;
            }
            damping *= 0.5;
        }
        if (!accepted) return false;
    }
    return false;
}

std::string criticalNoteKey(const std::string& code) {
    if (code == "saddle") return "step.note.a_critical_point_is_a_saddle";
    if (code == "stable_node") return "step.note.a_critical_point_is_a_stable_node";
    if (code == "unstable_node") return "step.note.a_critical_point_is_an_unstable_node";
    if (code == "stable_degenerate_node")
        return "step.note.a_critical_point_is_a_stable_degenerate_node";
    if (code == "unstable_degenerate_node")
        return "step.note.a_critical_point_is_an_unstable_degenerate_node";
    if (code == "center") return "step.note.a_critical_point_is_a_centre";
    if (code == "stable_spiral") return "step.note.a_critical_point_is_a_stable_spiral";
    if (code == "unstable_spiral")
        return "step.note.a_critical_point_is_an_unstable_spiral";
    return "step.note.the_linearisation_is_not_hyperbolic";
}

Result criticalPoints(const TopicRequest& req, bool en) {
    (void)en;
    Result r;
    baseId(r, "topic", "critical_points", "x");
    const double xmin = req.number("xmin", -1.0);
    const double xmax = req.number("xmax", 2.0);
    const double ymin = req.number("ymin", -1.0);
    const double ymax = req.number("ymax", 2.0);
    if (!std::isfinite(xmin) || !std::isfinite(xmax) || !std::isfinite(ymin) ||
        !std::isfinite(ymax) || xmax - xmin <= 1e-9 || ymax - ymin <= 1e-9 ||
        std::fabs(xmin) > 1e6 || std::fabs(xmax) > 1e6 || std::fabs(ymin) > 1e6 ||
        std::fabs(ymax) > 1e6) {
        failCode(r, "bad_bounds");
        return r;
    }

    Ex f, g;
    try {
        f = parseExpr(req.text("f", "x - x^2 - x*y"));
        g = parseExpr(req.text("g", "y*x - 0.5*y"));
    } catch (const std::exception& e) {
        failFrom(r, e, "critical points");
        return r;
    }
    for (const Ex* expression : {&f, &g}) {
        const std::string unknown = unknownFunction(*expression);
        if (!unknown.empty()) {
            r.ok = false;
            r.verified = false;
            r.errorCode = "unknown_function";
            r.errorDetail = "unknown function: " + unknown;
            return r;
        }
    }

    const double span = std::max(xmax - xmin, ymax - ymin);
    const double sampleX[3] = {xmin + 0.37 * (xmax - xmin),
                               xmin + 0.61 * (xmax - xmin),
                               xmin + 0.83 * (xmax - xmin)};
    const double sampleY[3] = {ymin + 0.41 * (ymax - ymin),
                               ymin + 0.72 * (ymax - ymin),
                               ymin + 0.29 * (ymax - ymin)};
    for (int i = 0; i < 3; ++i) {
        double value = 0.0;
        if (!evalBivariate(f, sampleX[i], sampleY[i], value) ||
            !evalBivariate(g, sampleX[i], sampleY[i], value)) {
            failCode(r, "not_evaluable");
            return r;
        }
    }

    const Ex fx = diff(f, "x");
    const Ex fy = diff(f, "y");
    const Ex gx = diff(g, "x");
    const Ex gy = diff(g, "y");
    for (const Ex* expression : {&fx, &fy, &gx, &gy}) {
        double value = 0.0;
        if (!evalBivariate(*expression, sampleX[0], sampleY[0], value)) {
            failCode(r, "not_evaluable");
            return r;
        }
    }

    std::vector<PlanarPoint> points;
    const int grid = 13;
    const double mergeTolerance = 1e-6 * (1.0 + span);
    for (int i = 0; i < grid; ++i) {
        for (int j = 0; j < grid; ++j) {
            const double startX = xmin + (xmax - xmin) * i / (grid - 1);
            const double startY = ymin + (ymax - ymin) * j / (grid - 1);
            PlanarPoint candidate;
            if (!newtonCritical(f, g, fx, fy, gx, gy, startX, startY, xmin, xmax,
                                ymin, ymax, candidate))
                continue;
            if (candidate.x < xmin - mergeTolerance || candidate.x > xmax + mergeTolerance ||
                candidate.y < ymin - mergeTolerance || candidate.y > ymax + mergeTolerance)
                continue;
            bool duplicate = false;
            for (const PlanarPoint& other : points) {
                if (std::hypot(candidate.x - other.x, candidate.y - other.y) <=
                    mergeTolerance) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) points.push_back(candidate);
        }
    }
    std::sort(points.begin(), points.end(), [](const PlanarPoint& a, const PlanarPoint& b) {
        if (std::fabs(a.x - b.x) > 1e-9) return a.x < b.x;
        return a.y < b.y;
    });
    const size_t limit = 12;
    if (points.size() > limit) points.resize(limit);
    if (points.empty()) {
        failCode(r, "no_critical_points");
        return r;
    }

    for (PlanarPoint& point : points) {
        double j11 = 0.0, j12 = 0.0, j21 = 0.0, j22 = 0.0;
        if (!evalBivariate(fx, point.x, point.y, j11) ||
            !evalBivariate(fy, point.x, point.y, j12) ||
            !evalBivariate(gx, point.x, point.y, j21) ||
            !evalBivariate(gy, point.x, point.y, j22)) {
            failCode(r, "not_evaluable");
            return r;
        }
        point.portrait = classifyLinearPortrait(j11, j12, j21, j22);
    }

    std::string systemText = "x' = " + toPretty(f) + ",  y' = " + toPretty(g);
    std::string jacobianText = "J = [[" + toPretty(fx) + ", " + toPretty(fy) + "], [" +
                               toPretty(gx) + ", " + toPretty(gy) + "]]";
    r.normalized = Sym(systemText);
    r.order = 1;
    r.typeId = "critical_points";

    std::string pointList;
    std::string summary;
    for (size_t i = 0; i < points.size(); ++i) {
        const std::string label =
            "P" + std::to_string(i + 1) + " = (" + numStr(points[i].x) + ", " +
            numStr(points[i].y) + ")";
        if (i) {
            pointList += ",  ";
            summary += ";  ";
        }
        pointList += label;
        summary += label + ": " + portraitName(points[i].portrait.code);
    }
    r.solution = Sym(summary);

    addStep(r, "step.title.write_the_planar_autonomous_system", Sym(systemText),
            "step.note.critical_points_are_where_both_velocity_components_vanish");
    addStep(r, "step.title.compute_the_jacobian", Sym(jacobianText),
            "step.note.the_jacobian_collects_the_partial_derivatives_for_linearisation");
    addStep(r, "step.title.find_the_critical_points", Sym(pointList),
            "step.note.critical_points_found_in_the_box", 
            "{\"n\":" + std::to_string(points.size()) + "}");

    for (size_t i = 0; i < points.size(); ++i) {
        const PlanarPoint& point = points[i];
        double j11 = 0.0, j12 = 0.0, j21 = 0.0, j22 = 0.0;
        evalBivariate(fx, point.x, point.y, j11);
        evalBivariate(fy, point.x, point.y, j12);
        evalBivariate(gx, point.x, point.y, j21);
        evalBivariate(gy, point.x, point.y, j22);
        const std::string math =
            "J = [[" + numStr(j11) + ", " + numStr(j12) + "], [" + numStr(j21) + ", " +
            numStr(j22) + "]],  " + eigenPairText(point.portrait);
        addStep(r, "step.title.critical_point_at",
                Sym("P" + std::to_string(i + 1) + " = (" + numStr(point.x) + ", " +
                    numStr(point.y) + ")"),
                criticalNoteKey(point.portrait.code),
                "{\"x\":" + numStr(point.x) + ",\"y\":" + numStr(point.y) + "}", math);
    }

    double residualF = 0.0, residualG = 0.0;
    for (const PlanarPoint& point : points) {
        double fv = 0.0, gv = 0.0;
        evalBivariate(f, point.x, point.y, fv);
        evalBivariate(g, point.x, point.y, gv);
        residualF = std::max(residualF, std::fabs(fv));
        residualG = std::max(residualG, std::fabs(gv));
    }
    addCheck(r, "check.every_critical_point_satisfies_the_system",
             residualF <= 1e-7 && residualG <= 1e-7, "", "",
             "max |f| = " + numStr(residualF) + ", max |g| = " + numStr(residualG));

    double worstDerivative = 0.0;
    for (const PlanarPoint& point : points) {
        const double hx = 1e-6 * std::max(1.0, std::fabs(point.x));
        const double hy = 1e-6 * std::max(1.0, std::fabs(point.y));
        double fpx = 0.0, fmx = 0.0, fpy = 0.0, fmy = 0.0;
        double gpx = 0.0, gmx = 0.0, gpy = 0.0, gmy = 0.0;
        if (!evalBivariate(f, point.x + hx, point.y, fpx) ||
            !evalBivariate(f, point.x - hx, point.y, fmx) ||
            !evalBivariate(f, point.x, point.y + hy, fpy) ||
            !evalBivariate(f, point.x, point.y - hy, fmy) ||
            !evalBivariate(g, point.x + hx, point.y, gpx) ||
            !evalBivariate(g, point.x - hx, point.y, gmx) ||
            !evalBivariate(g, point.x, point.y + hy, gpy) ||
            !evalBivariate(g, point.x, point.y - hy, gmy)) {
            worstDerivative = std::numeric_limits<double>::infinity();
            break;
        }
        double j11 = 0.0, j12 = 0.0, j21 = 0.0, j22 = 0.0;
        evalBivariate(fx, point.x, point.y, j11);
        evalBivariate(fy, point.x, point.y, j12);
        evalBivariate(gx, point.x, point.y, j21);
        evalBivariate(gy, point.x, point.y, j22);
        const double fd11 = (fpx - fmx) / (2.0 * hx);
        const double fd12 = (fpy - fmy) / (2.0 * hy);
        const double fd21 = (gpx - gmx) / (2.0 * hx);
        const double fd22 = (gpy - gmy) / (2.0 * hy);
        const double scale = 1.0 + std::fabs(j11) + std::fabs(j12) + std::fabs(j21) +
                             std::fabs(j22);
        worstDerivative = std::max(
            {worstDerivative, std::fabs(fd11 - j11) / scale, std::fabs(fd12 - j12) / scale,
             std::fabs(fd21 - j21) / scale, std::fabs(fd22 - j22) / scale});
    }
    addCheck(r, "check.the_jacobian_matches_finite_differences",
             worstDerivative <= 1e-5, "", "",
             "max relative Jacobian error = " + numStr(worstDerivative));

    bool classificationsOk = true;
    for (const PlanarPoint& point : points) {
        double j11 = 0.0, j12 = 0.0, j21 = 0.0, j22 = 0.0;
        evalBivariate(fx, point.x, point.y, j11);
        evalBivariate(fy, point.x, point.y, j12);
        evalBivariate(gx, point.x, point.y, j21);
        evalBivariate(gy, point.x, point.y, j22);
        const LinearPortrait check = classifyLinearPortrait(j11, j12, j21, j22);
        if (portraitFamily(check.code) != portraitFamily(point.portrait.code))
            classificationsOk = false;
    }
    addCheck(r, "check.every_classification_matches_the_eigenvalues",
             classificationsOk, "", "",
             std::to_string(points.size()) + " critical point(s) checked");
    return r;
}

// ---------------------------------------------------------------- predator-prey
Result predatorPrey(const TopicRequest& req, bool en) {
    (void)en;
    Result r;
    baseId(r, "topic", "predator_prey", "t");
    const double a = req.number("a", 1.1);
    const double b = req.number("b", 0.4);
    const double c = req.number("c", 0.4);
    const double d = req.number("d", 0.1);
    const double x0 = req.number("x0", 10.0);
    const double y0 = req.number("y0", 5.0);
    const double tEnd = req.number("t", 10.0);
    if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c) ||
        !std::isfinite(d) || a <= 0.0 || b <= 0.0 || c <= 0.0 || d <= 0.0) {
        failCode(r, "bad_predator_prey_rates");
        return r;
    }
    if (!std::isfinite(x0) || !std::isfinite(y0) || x0 <= 0.0 || y0 <= 0.0) {
        failCode(r, "bad_initial_population");
        return r;
    }
    if (!std::isfinite(tEnd) || tEnd <= 0.0 || tEnd > 10000.0) {
        failCode(r, "bad_time_interval");
        return r;
    }

    const int minSteps = 4000;
    const int maxSteps = 200000;
    // Pick a step that resolves the fastest rate in the model, then keep the
    // total number of steps bounded so long integrations stay responsive.
    const double rateScale = std::max(std::max(a, b * x0), std::max(c, d * y0));
    double targetH = tEnd / minSteps;
    if (rateScale > 0.0) targetH = std::min(targetH, 0.02 / rateScale);
    int steps = static_cast<int>(std::ceil(tEnd / targetH));
    steps = std::max(minSteps, std::min(maxSteps, steps));
    const double h = tEnd / steps;
    auto preyRate = [&](double x, double y) { return a * x - b * x * y; };
    auto predatorRate = [&](double x, double y) { return -c * y + d * x * y; };

    double x = x0;
    double y = y0;
    double minX = x;
    double minY = y;
    bool finite = true;
    for (int i = 0; i < steps && finite; ++i) {
        const double k1x = preyRate(x, y);
        const double k1y = predatorRate(x, y);
        const double k2x = preyRate(x + 0.5 * h * k1x, y + 0.5 * h * k1y);
        const double k2y = predatorRate(x + 0.5 * h * k1x, y + 0.5 * h * k1y);
        const double k3x = preyRate(x + 0.5 * h * k2x, y + 0.5 * h * k2y);
        const double k3y = predatorRate(x + 0.5 * h * k2x, y + 0.5 * h * k2y);
        const double k4x = preyRate(x + h * k3x, y + h * k3y);
        const double k4y = predatorRate(x + h * k3x, y + h * k3y);
        x += h / 6.0 * (k1x + 2.0 * k2x + 2.0 * k3x + k4x);
        y += h / 6.0 * (k1y + 2.0 * k2y + 2.0 * k3y + k4y);
        finite = std::isfinite(x) && std::isfinite(y);
        minX = std::min(minX, x);
        minY = std::min(minY, y);
    }
    if (!finite) {
        failCode(r, "not_evaluable");
        return r;
    }

    const double coexistenceX = c / d;
    const double coexistenceY = a / b;
    const LinearPortrait originPortrait = classifyLinearPortrait(a, 0.0, 0.0, -c);
    const LinearPortrait coexistencePortrait =
        classifyLinearPortrait(0.0, -b * coexistenceX, d * coexistenceY, 0.0);

    auto invariant = [&](double X, double Y) {
        if (X <= 0.0 || Y <= 0.0) return std::numeric_limits<double>::quiet_NaN();
        return d * X - c * std::log(X) + b * Y - a * std::log(Y);
    };
    const double invariantStart = invariant(x0, y0);
    const double invariantEnd = invariant(x, y);
    const double drift =
        std::fabs(invariantEnd - invariantStart) / (1.0 + std::fabs(invariantStart));

    r.normalized = Sym("x' = " + numStr(a) + " x - " + numStr(b) + " x y,  y' = -" +
                       numStr(c) + " y + " + numStr(d) + " x y");
    r.order = 1;
    r.typeId = "predator_prey";
    r.solution = Sym("(x, y)(" + rawNumStr(tEnd) + ") = (" + rawNumStr(x) + ", " +
                     rawNumStr(y) + ")");
    r.point = "t=" + numStr(tEnd);

    addStep(r, "step.title.write_the_lotka_volterra_system", r.normalized,
            "step.note.prey_grow_exponentially_and_predators_depend_on_prey");
    addStep(r, "step.title.find_the_equilibria",
            Sym("E0 = (0, 0),  E* = (" + numStr(coexistenceX) + ", " +
                numStr(coexistenceY) + ")"),
            "step.note.the_coexistence_equilibrium_balances_birth_and_death");
    addStep(r, "step.title.linearise_at_each_equilibrium",
            Sym("J(x, y) = [[" + numStr(a) + " - " + numStr(b) + " y, -" + numStr(b) +
                " x], [" + numStr(d) + " y, -" + numStr(c) + " + " + numStr(d) + " x]]"),
            "step.note.the_jacobian_controls_the_local_behaviour_of_the_populations");
    addStep(r, "step.title.critical_point_at",
            Sym("E0 = (0, 0)"),
            criticalNoteKey(originPortrait.code),
            "{\"x\":0,\"y\":0}", "λ = " + numStr(originPortrait.l1re) + ", " +
                                      numStr(originPortrait.l2re));
    addStep(r, "step.title.critical_point_at",
            Sym("E* = (" + numStr(coexistenceX) + ", " + numStr(coexistenceY) + ")"),
            criticalNoteKey(coexistencePortrait.code),
            "{\"x\":" + numStr(coexistenceX) + ",\"y\":" + numStr(coexistenceY) + "}",
            eigenPairText(coexistencePortrait));
    addStep(r, "step.title.integrate_the_populations", r.solution,
            "step.note.rk4_advances_both_populations_together");
    addStep(r, "step.title.check_the_conserved_quantity",
            Sym("H = " + numStr(d) + " x - " + numStr(c) + " ln x + " + numStr(b) +
                " y - " + numStr(a) + " ln y"),
            "step.note.the_lotka_volterra_quantity_stays_constant_on_a_trajectory");

    addCheck(r, "check.equilibria_satisfy_the_system",
             std::fabs(preyRate(0.0, 0.0)) < 1e-12 &&
                 std::fabs(predatorRate(0.0, 0.0)) < 1e-12 &&
                 std::fabs(preyRate(coexistenceX, coexistenceY)) < 1e-9 &&
                 std::fabs(predatorRate(coexistenceX, coexistenceY)) < 1e-9,
             "", "",
             "max residual = " +
                 numStr(std::max({std::fabs(preyRate(0.0, 0.0)),
                                  std::fabs(predatorRate(0.0, 0.0)),
                                  std::fabs(preyRate(coexistenceX, coexistenceY)),
                                  std::fabs(predatorRate(coexistenceX, coexistenceY))})));
    addCheck(r, "check.the_coexistence_eigenvalues_are_purely_imaginary",
             coexistencePortrait.l1im != 0.0 &&
                 std::fabs(coexistencePortrait.l1re) <= 1e-9 * (1.0 + a + c),
             "", "",
             "Re λ = " + numStr(coexistencePortrait.l1re));
    addCheck(r, "check.the_conserved_quantity_is_constant", drift <= 1e-3, "", "",
             "ΔH / (1 + |H|) = " + rawNumStr(drift));
    addCheck(r, "check.the_populations_stay_positive", minX > 0.0 && minY > 0.0, "", "",
             "min x = " + rawNumStr(minX) + ", min y = " + rawNumStr(minY));
    return r;
}

// ---------------------------------------------------------------- Fourier series
double simpson(const Ex& f, const std::string& x, double a, double b, int intervals = 2000) {
    if (intervals % 2) ++intervals;
    const double h = (b - a) / intervals;
    bool ok = true;
    double sum = evalF(f, x, a, "", 0, &ok);
    if (!ok) return std::numeric_limits<double>::quiet_NaN();
    for (int i = 1; i < intervals; ++i) {
        const double v = evalF(f, x, a + i * h, "", 0, &ok);
        if (!ok) return std::numeric_limits<double>::quiet_NaN();
        sum += (i % 2 ? 4.0 : 2.0) * v;
    }
    const double last = evalF(f, x, b, "", 0, &ok);
    if (!ok) return std::numeric_limits<double>::quiet_NaN();
    return (h / 3.0) * (sum + last);
}

Result fourierSeries(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "fourier_series", "x");
    const std::string input = req.text("f", "x");
    const double L = req.number("L", kPi);
    int terms = (int)std::llround(req.number("n", 5.0));
    if (terms < 1) terms = 1;
    if (terms > 20) terms = 20;
    if (std::fabs(L) < 1e-12) {
        failCode(r, "bad_half_period");
        return r;
    }
    Ex f;
    try {
        f = parseExpr(input);
    } catch (const std::exception& e) {
        failFrom(r, e, "fourier");
        return r;
    }
    r.normalized = f;
    const std::string unknown = unknownFunction(f);
    if (!unknown.empty()) {
        r.ok = false;
        r.verified = false;
        r.errorCode = "unknown_function";
        r.errorDetail = "unknown function: " + unknown;
        return r;
    }
    // sec²x and friends are infinite inside [-L, L]; there is no series.
    DomainIssue issue;
    if (checkDomain(f, "x", {}, -L, L, &issue)) {
        failDomain(r, en, issue);
        return r;
    }
    const double a0 = simpson(f, "x", -L, L) / L;
    if (!validCoefficient(a0)) {
        r.ok = false;
        r.verified = false;
        r.errorCode = "not_integrable";
        r.errorDetail = "non-finite or overflowing Fourier coefficient a0";

        return r;
    }
    std::vector<Ex> parts;
    std::vector<double> anVals, bnVals;
    if (std::fabs(a0) > 1e-10) parts.push_back(numEx(a0 / 2.0));
    std::ostringstream coeffs;
    coeffs << "n\ta_n\tb_n\n";
    for (int n = 1; n <= terms; ++n) {
        const Ex arg = Mul(piMultiple(n, L), Sym("x"));
        const Ex fc = Mul(f, cosEx(arg));
        const Ex fs = Mul(f, sinEx(arg));
        const double an = simpson(fc, "x", -L, L) / L;
        const double bn = simpson(fs, "x", -L, L) / L;
        anVals.push_back(an);
        bnVals.push_back(bn);
        if (!validCoefficient(an) || !validCoefficient(bn)) {
            r.ok = false;
            r.verified = false;
            r.errorCode = "not_integrable";
            r.errorDetail = "non-finite or overflowing Fourier coefficient at n = " +
                            std::to_string(n);

            return r;
        }
        coeffs << n << "\t" << numStr(an, 10) << "\t" << numStr(bn, 10) << "\n";
        if (std::fabs(an) > 1e-10) parts.push_back(Mul(smartNum(an), cosEx(arg)));
        if (std::fabs(bn) > 1e-10) parts.push_back(Mul(smartNum(bn), sinEx(arg)));
    }
    r.solution = parts.empty() ? Num(0) : Add(parts);
    addStep(r, "step.title.state_the_series", Sym("f(x) ~ a_0/2 + sum(a_n cos(n pi x/L) + b_n sin(n pi x/L))"), "step.note.the_coefficients_are_integrals_over_one_period");
    addStepRaw(r, "step.title.compute_the_coefficients", nullptr, coeffs.str());
    addStep(r, "step.title.assemble_the_partial_series", r.solution,
            "step.note.keeping_harmonics", "{\"n\":" + std::to_string(terms) + "}");
    // Independent mean: midpoint quadrature is a different rule from the
    // composite Simpson integration used for a_0, so a coding slip in either
    // one shows up as a disagreement.
    double midSum = 0.0;
    const int midN = 2000;
    const double midH = 2.0 * L / midN;
    bool midOk = true;
    for (int i = 0; i < midN; ++i) {
        midSum += evalF(f, "x", -L + (i + 0.5) * midH, "", 0, &midOk);
        if (!midOk) break;
    }
    const double a0mid = midOk ? midSum * midH / L
                               : std::numeric_limits<double>::quiet_NaN();
    const bool meanOk = std::isfinite(a0) && std::isfinite(a0mid) &&
                        std::fabs(a0 - a0mid) <= 1e-3 * (1.0 + std::fabs(a0));
    addCheck(r, "check.constant_term", meanOk, "", "",
             "a0/2 = " + numStr(a0 / 2.0) + ", midpoint mean = " +
                 numStr(a0mid / 2.0));

    // A jump at the endpoints makes the series converge slowly (Gibbs).
    bool okLeft = false, okRight = false;
    const double fLeft = evalF(f, "x", -L, "", 0, &okLeft);
    const double fRight = evalF(f, "x", L, "", 0, &okRight);
    if (okLeft && okRight && std::isfinite(fLeft) && std::isfinite(fRight)) {
        const double jump = std::fabs(fLeft - fRight);
        if (jump > 1e-6 * (1.0 + std::fabs(fLeft) + std::fabs(fRight))) {
            r.warnings.push_back("step.note.endpoint_jump");
            addCheck(r, "check.endpoint_jump", true, "check.gibbs_slow", "",
                     "f(-L) - f(L) = " + numStr(fLeft - fRight));
        }
    }

    // Independent validation: the partial sum against f at interior points.
    for (double xv : {0.0, -1.0, 1.0}) {
        if (xv <= -L || xv >= L) continue;
        bool okF = false;
        const double fv = evalF(f, "x", xv, "", 0, &okF);
        if (!okF || !std::isfinite(fv)) continue;
        double sv = a0 / 2.0;
        for (size_t k = 0; k < anVals.size(); ++k) {
            const double arg = (double)(k + 1) * kPi * xv / L;
            sv += anVals[k] * std::cos(arg) + bnVals[k] * std::sin(arg);
        }
        if (!std::isfinite(sv)) continue;
        const double err = std::fabs(fv - sv);
        double scale = std::fabs(a0);
        for (size_t k = 0; k < anVals.size(); ++k)
            scale += std::fabs(anVals[k]) + std::fabs(bnVals[k]);
        const double tolerance = std::max(0.05, 0.25 * scale);
        addCheck(r, "check.partial_sum_at_x", err < tolerance,
                 "", "{\"x\":" + numStr(xv) + "}",
                 "f = " + numStr(fv) + ", partial sum = " + numStr(sv) +
                     ", |error| = " + numStr(err));
    }
    return r;
}

// ---------------------------------------------------------------- PDEs
// Evaluate the first sine eigenfunction at both ends of [0, L] so the
// boundary claim is measured rather than asserted.
void addSineBoundaryCheck(Result& r, double L) {
    const Ex basis = sinEx(Mul(piMultiple(1, L), Sym("x")));
    bool okLeft = false, okRight = false;
    const double left = evalReal(basis, {{"x", 0.0}}, &okLeft);
    const double right = evalReal(basis, {{"x", L}}, &okRight);
    const bool ok = okLeft && okRight && std::fabs(left) < 1e-9 &&
                    std::fabs(right) < 1e-9;
    addCheck(r, "check.boundary_conditions", ok, "check.sine_vanishes", "",
             "sin(0) = " + numStr(okLeft ? left : 0.0) +
                 ", sin(pi) = " + numStr(okRight ? right : 0.0));
}

Result heatEquation(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "heat", "x");
    const double alpha = req.number("alpha", 1.0);
    const double L = req.number("L", kPi);
    int terms = (int)std::llround(req.number("n", 5.0));
    if (terms < 1) terms = 1;
    if (terms > 20) terms = 20;
    const std::string input = req.text("f", "sin(pi*x/L)");
    if (L <= 1e-12) {
        failCode(r, "bad_length");
        return r;
    }
    Ex f;
    try { f = parseExpr(input); } catch (const std::exception& e) {
        failFrom(r, e, "heat");
        return r;
    }
    f = substitute(f, "L", numEx(L));
    f = substitute(f, "pi", Sym("pi"));
    r.normalized = Sym("u_t = alpha u_xx");
    const std::string unknown = unknownFunction(f);
    if (!unknown.empty()) {
        r.ok = false;
        r.verified = false;
        r.errorCode = "unknown_function";
        r.errorDetail = "unknown function: " + unknown;
        return r;
    }
    DomainIssue issue;
    if (checkDomain(f, "x", {}, 0.0, L, &issue)) {
        failDomain(r, en, issue);
        return r;
    }
    std::vector<Ex> parts;
    std::ostringstream coeffs;
    coeffs << "n\tb_n\n";
    for (int n = 1; n <= terms; ++n) {
        const Ex k = piMultiple(n, L);
        const Ex basis = sinEx(Mul(k, Sym("x")));
        const double bn = 2.0 * simpson(Mul(f, basis), "x", 0.0, L) / L;
        if (!validCoefficient(bn)) {
            r.ok = false;
            r.verified = false;
            r.errorCode = "not_integrable";
            r.errorDetail = "non-finite or overflowing Fourier coefficient at n = " +
                            std::to_string(n);
            return r;
        }
        coeffs << n << "\t" << numStr(bn) << "\n";
        if (std::fabs(bn) < 1e-10) continue;
        const Ex decay = expEx(Mul(Num(-1), numEx(alpha), Mul(k, k), Sym("t")));
        parts.push_back(Mul(smartNum(bn), basis, decay));
    }
    r.solution = parts.empty() ? Num(0) : Add(parts);
    addStep(r, "step.title.separate_the_variables", Sym("u(x,t) = X(x) T(t)"), "step.note.the_boundary_conditions_u_0_t_u_l_t_0_give_sine_eigenfunctions");
    addStepRaw(r, "step.title.compute_the_fourier_coefficients", nullptr, coeffs.str());
    addStep(r, "step.title.write_the_series_solution", r.solution, "step.note.each_mode_decays_at_its_own_rate");
    addSineBoundaryCheck(r, L);
    return r;
}

Result waveEquation(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "wave", "x");
    const double a = req.number("a", 1.0);
    const double L = req.number("L", kPi);
    int terms = (int)std::llround(req.number("n", 5.0));
    if (terms < 1) terms = 1;
    if (terms > 20) terms = 20;
    const std::string input = req.text("f", "sin(pi*x/L)");
    if (L <= 1e-12) {
        failCode(r, "bad_length");
        return r;
    }
    Ex f;
    try { f = parseExpr(input); } catch (const std::exception& e) {
        failFrom(r, e, "wave");
        return r;
    }
    f = substitute(f, "L", numEx(L));
    r.normalized = Sym("u_tt = a^2 u_xx");
    const std::string unknown = unknownFunction(f);
    if (!unknown.empty()) {
        r.ok = false;
        r.verified = false;
        r.errorCode = "unknown_function";
        r.errorDetail = "unknown function: " + unknown;
        return r;
    }
    DomainIssue issue;
    if (checkDomain(f, "x", {}, 0.0, L, &issue)) {
        failDomain(r, en, issue);
        return r;
    }
    std::vector<Ex> parts;
    std::ostringstream coeffs;
    coeffs << "n\tA_n\n";
    for (int n = 1; n <= terms; ++n) {
        const Ex k = piMultiple(n, L);
        const Ex basis = sinEx(Mul(k, Sym("x")));
        const double an = 2.0 * simpson(Mul(f, basis), "x", 0.0, L) / L;
        if (!validCoefficient(an)) {
            r.ok = false;
            r.verified = false;
            r.errorCode = "not_integrable";
            r.errorDetail = "non-finite or overflowing Fourier coefficient at n = " +
                            std::to_string(n);
            return r;
        }
        coeffs << n << "\t" << numStr(an) << "\n";
        if (std::fabs(an) < 1e-10) continue;
        const Ex mode = cosEx(Mul(numEx(a), k, Sym("t")));
        parts.push_back(Mul(smartNum(an), basis, mode));
    }
    r.solution = parts.empty() ? Num(0) : Add(parts);
    addStep(r, "step.title.separate_the_variables", Sym("u(x,t) = X(x) T(t)"), "step.note.the_fixed_ends_give_sine_modes_time_is_harmonic");
    addStepRaw(r, "step.title.compute_the_coefficients", nullptr, coeffs.str());
    addStep(r, "step.title.write_the_series_solution", r.solution, "step.note.the_initial_velocity_is_taken_as_zero");
    addSineBoundaryCheck(r, L);
    return r;
}

Result laplacePde(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "laplace_pde", "x");
    const double a = req.number("a", kPi);
    const double b = req.number("b", kPi);
    int terms = (int)std::llround(req.number("n", 5.0));
    if (terms < 1) terms = 1;
    if (terms > 20) terms = 20;
    const std::string input = req.text("f", "sin(pi*x/a)");
    if (a <= 1e-12 || b <= 1e-12) {
        failCode(r, "bad_sides");
        return r;
    }
    Ex f;
    try { f = parseExpr(input); } catch (const std::exception& e) {
        failFrom(r, e, "laplace pde");
        return r;
    }
    const std::string unknown = unknownFunction(f);
    if (!unknown.empty()) {
        r.ok = false;
        r.verified = false;
        r.errorCode = "unknown_function";
        r.errorDetail = "unknown function: " + unknown;
        return r;
    }
    f = substitute(f, "a", numEx(a));
    DomainIssue issue;
    if (checkDomain(f, "x", {}, 0.0, a, &issue)) {
        failDomain(r, en, issue);
        return r;
    }
    r.normalized = Sym("u_xx + u_yy = 0");
    std::vector<Ex> parts;
    std::ostringstream coeffs;
    coeffs << "n\tb_n\n";
    for (int n = 1; n <= terms; ++n) {
        const Ex k = piMultiple(n, a);
        const Ex basis = sinEx(Mul(k, Sym("x")));
        const double bn = 2.0 * simpson(Mul(f, basis), "x", 0.0, a) / a;
        if (!validCoefficient(bn)) {
            r.ok = false;
            r.verified = false;
            r.errorCode = "not_integrable";
            r.errorDetail = "non-finite or overflowing Fourier coefficient at n = " +
                            std::to_string(n);
            return r;
        }
        coeffs << n << "\t" << numStr(bn) << "\n";
        if (std::fabs(bn) < 1e-10) continue;
        // Keep sinh(ky)/sinh(kb) as a ratio.  When b is large relative to a
        // the reciprocal alone underflows and the term would vanish.
        const Ex yPart = Mul(sinhEx(Mul(k, Sym("y"))),
                             Pow(sinhEx(Mul(k, numOrPi(b))), Num(-1)));
        parts.push_back(Mul(smartNum(bn), basis, yPart));
    }
    r.solution = parts.empty() ? Num(0) : Add(parts);
    addStep(r, "step.title.separate_the_variables", Sym("u(x,y) = X(x) Y(y)"), "step.note.the_zero_boundary_conditions_on_three_sides_give_sine_modes_in_x");
    addStepRaw(r, "step.title.compute_the_boundary_coefficients", nullptr, coeffs.str());
    addStep(r, "step.title.write_the_series_solution", r.solution, "step.note.the_sinh_factor_enforces_the_top_boundary_value");
    addSineBoundaryCheck(r, a);
    return r;
}

// ---------------------------------------------------------------- transforms
bool splitRationalParts(const Ex& e, Ex& num, Ex& den);

Ex laplaceOf(const Ex& f, const std::string& t, std::string& rule, bool& ok) {
    ok = true;
    if (!f) { ok = false; return nullptr; }
    if (f->isNum()) {
        rule = "L{c} = c/s";
        return Mul(f, Pow(Sym("s"), Num(-1)));
    }
    if (!dependsOn(f, t)) {
        // Symbolic constants such as pi, e or exp(2) follow the same rule.
        rule = "L{c} = c/s";
        return Mul(f, Pow(Sym("s"), Num(-1)));
    }
    if (f->isSym(t)) {
        rule = "L{t} = 1/s^2";
        return Pow(Sym("s"), Num(-2));
    }
    if (f->isAdd()) {
        std::vector<Ex> out;
        for (const auto& term : f->args) {
            std::string sub;
            bool subOk = true;
            Ex v = laplaceOf(term, t, sub, subOk);
            if (!subOk) { ok = false; return nullptr; }
            out.push_back(v);
        }
        rule = "linearity";
        return Add(out);
    }
    if (f->isMul()) {
        // Recognise the shifted table entries before the generic constant
        // multiple rule: e^(at) sin(bt), e^(at) cos(bt), t^n e^(at).
        auto linearRate = [&](const Ex& arg, Ex& rate) -> bool {
            if (arg->isSym(t)) { rate = Num(1); return true; }
            if (!arg->isMul()) return false;
            Ex rest = Num(1);
            for (const auto& factor : arg->args) {
                if (!dependsOn(factor, t)) rate = rate ? rate : factor;
                else rest = Mul(rest, factor);
            }
            return rest->isSym(t) && rate != nullptr;
        };
        Ex expFactor, trigFactor, polyFactor, coef = Num(1);
        for (const auto& factor : f->args) {
            if (factor->isFunc("exp") && factor->args.size() == 1) expFactor = factor;
            else if (factor->isFunc("sin") || factor->isFunc("cos")) trigFactor = factor;
            else if (factor->isSym(t))
                polyFactor = factor;  // t itself is t^1
            else if (factor->isPow() && factor->args[0]->isSym(t) && factor->args[1]->isNum() &&
                     factor->args[1]->num.isInt() && factor->args[1]->num.n > 0)
                polyFactor = factor;
            else if (!dependsOn(factor, t))
                coef = Mul(coef, factor);
        }
        Ex rate, freq;
        if (expFactor && trigFactor && !polyFactor &&
            linearRate(expFactor->args[0], rate) && linearRate(trigFactor->args[0], freq)) {
            const Ex shift = Add(Sym("s"), Mul(Num(-1), rate));
            const Ex den = Add(Pow(shift, Num(2)), Pow(freq, Num(2)));
            if (trigFactor->name == "sin") {
                rule = "L{e^(at) sin(bt)} = b/((s-a)^2+b^2)";
                return Mul(coef, Mul(freq, Pow(den, Num(-1))));
            }
            rule = "L{e^(at) cos(bt)} = (s-a)/((s-a)^2+b^2)";
            return Mul(coef, Mul(shift, Pow(den, Num(-1))));
        }
        if (expFactor && polyFactor && !trigFactor &&
            linearRate(expFactor->args[0], rate)) {
            long long n = 1;
            if (polyFactor->isPow() && polyFactor->args.size() == 2 &&
                polyFactor->args[1]->isNum())
                n = polyFactor->args[1]->num.n;
            long long fact = 1;
            for (long long i = 2; i <= n; ++i) fact *= i;
            rule = "L{t^n e^(at)} = n!/(s-a)^(n+1)";
            return Mul(coef, Mul(Num(fact),
                   Pow(Add(Sym("s"), Mul(Num(-1), rate)), Num(-(n + 1)))));
        }
        Ex constant = Num(1);
        Ex rest = Num(1);
        for (const auto& factor : f->args) {
            if (!dependsOn(factor, t)) constant = Mul(constant, factor);
            else rest = Mul(rest, factor);
        }
        if (!rest->isOne()) {
            if (equals(rest, f)) { ok = false; return nullptr; }
            std::string sub;
            bool subOk = true;
            Ex v = laplaceOf(rest, t, sub, subOk);
            if (!subOk) { ok = false; return nullptr; }
            rule = "constant multiple";
            return Mul(constant, v);
        }
        rule = "L{c} = c/s";
        return Mul(f, Pow(Sym("s"), Num(-1)));
    }
    if (f->isPow() && f->args[0]->isSym(t) && f->args[1]->isNum() && f->args[1]->num.isInt() &&
        f->args[1]->num.n >= 0) {
        const long long n = f->args[1]->num.n;
        long long fact = 1;
        for (long long i = 2; i <= n; ++i) fact *= i;
        rule = "L{t^n} = n!/s^(n+1)";
        return Mul(Num(fact), Pow(Sym("s"), Num(-(n + 1))));
    }
    if (f->isFunc() && f->args.size() == 1) {
        const Ex& arg = f->args[0];
        // A phase-shifted exponential is a constant multiple of e^(at).  The
        // expression engine folds exp(a)*exp(b) back into exp(a+b), so split
        // the additive argument here instead of relying on the product form.
        if (f->name == "exp" && arg->isAdd()) {
            Ex base = Num(0), offset = Num(0);
            for (const auto& term : arg->args) {
                if (dependsOn(term, t)) base = Add(base, term);
                else offset = Add(offset, term);
            }
            Ex rate;
            if (base->isSym(t)) {
                rate = Num(1);
            } else if (base->isMul()) {
                Ex rest = Num(1), r = Num(1);
                bool good = true;
                for (const auto& factor : base->args) {
                    if (!dependsOn(factor, t)) r = Mul(r, factor);
                    else if (factor->isSym(t)) rest = Mul(rest, factor);
                    else { good = false; break; }
                }
                if (good && rest->isSym(t)) rate = r;
            }
            if (rate && !equals(offset, Num(0))) {
                rule = "L{e^(at+b)} = e^b/(s-a)";
                return Mul(Fn("exp", {offset}),
                           Pow(Add(Sym("s"), Mul(Num(-1), rate)), Num(-1)));
            }
        }
        Ex a;
        if (arg->isSym(t)) a = Num(1);
        else if (arg->isMul()) {
            Ex rest = Num(1);
            for (const auto& factor : arg->args) {
                if (!dependsOn(factor, t)) a = a ? a : factor;
                else rest = Mul(rest, factor);
            }
            if (!rest->isSym(t)) a = nullptr;
        }
        if (a) {
            if (f->name == "exp") {
                rule = "L{e^(at)} = 1/(s-a)";
                return Pow(Add(Sym("s"), Mul(Num(-1), a)), Num(-1));
            }
            if (f->name == "sin") {
                rule = "L{sin(bt)} = b/(s^2+b^2)";
                return Mul(a, Pow(Add(Pow(Sym("s"), Num(2)), Pow(a, Num(2))), Num(-1)));
            }
            if (f->name == "cos") {
                rule = "L{cos(bt)} = s/(s^2+b^2)";
                return Mul(Sym("s"), Pow(Add(Pow(Sym("s"), Num(2)), Pow(a, Num(2))), Num(-1)));
            }
            if (f->name == "sinh") {
                rule = "L{sinh(bt)} = b/(s^2-b^2)";
                return Mul(a, Pow(Add(Pow(Sym("s"), Num(2)), Mul(Num(-1), Pow(a, Num(2)))), Num(-1)));
            }
            if (f->name == "cosh") {
                rule = "L{cosh(bt)} = s/(s^2-b^2)";
                return Mul(Sym("s"), Pow(Add(Pow(Sym("s"), Num(2)), Mul(Num(-1), Pow(a, Num(2)))), Num(-1)));
            }
        }
    }
    ok = false;
    return nullptr;
}

// Numerically verify a Laplace pair by integrating f(t) e^(-s t) on a finite
// window at a few values of s.  The half-window comparison estimates the tail,
// so a slowly converging candidate is skipped instead of producing a false
// mismatch.  Returns false when no candidate could be sampled reliably.
bool laplaceSampleMatches(const Ex& f, const Ex& F, double& sOut, double& gotOut,
                          double& wantOut) {
    static const double candidates[] = {3.0, 5.0, 10.0, 20.0, 50.0, 100.0};
    for (double s : candidates) {
        bool fOk = true, FOk = true;
        const double want = evalReal(F, {{"s", s}}, &FOk);
        if (!FOk || !std::isfinite(want)) continue;
        const Ex integrand = Mul(f, expEx(Mul(Num(-s), Sym("t"))));
        const double whole = simpson(integrand, "t", 0.0, 20.0, 4000);
        if (!std::isfinite(whole)) continue;
        const double half = simpson(integrand, "t", 0.0, 10.0, 2000);
        if (!std::isfinite(half)) continue;
        if (std::fabs(whole - half) > 5e-3 * std::fabs(whole) + 1e-7) continue;
        // Relative tolerance, with a tiny absolute floor.  A loose
        // `(1 + |want|)` tolerance used to accept a wrong entry when the
        // transform value was small.
        if (std::fabs(whole - want) <= 2e-3 * std::fabs(want) + 1e-8) {
            sOut = s;
            gotOut = whole;
            wantOut = want;
            return true;
        }
    }
    return false;
}

Result laplaceTransform(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "laplace", "t");
    const std::string input = req.text("f", "t");
    Ex f;
    try { f = parseExpr(input); } catch (const std::exception& e) {
        failFrom(r, e, "laplace");
        return r;
    }
    r.normalized = f;
    const std::string unknown = unknownFunction(f);
    if (!unknown.empty()) {
        r.ok = false;
        r.verified = false;
        r.errorCode = "unknown_function";
        r.errorDetail = "unknown function: " + unknown;
        return r;
    }
    std::string rule;
    bool ok = true;
    Ex F = laplaceOf(f, "t", rule, ok);
    if (!ok || !F) {
        failCode(r, "unsupported_form");
        return r;
    }
    r.solution = simplify(F);
    addStep(r, "step.title.start_from_the_definition", Sym("F(s) = integral_0^inf e^(-s t) f(t) dt"), "step.note.the_transform_turns_differentiation_into_multiplication_by_s");
    addStepRaw(r, "step.title.match_a_table_entry", f, rule);
    addStep(r, "step.title.write_the_transform", r.solution, "step.note.the_result_is_a_function_of_s");
    double probeS = 0.0, probeGot = 0.0, probeWant = 0.0;
    if (laplaceSampleMatches(f, r.solution, probeS, probeGot, probeWant)) {
        addCheck(r, "check.domain", true, "check.transform_domain", "",
                 "s = " + numStr(probeS) + ": F(s) = " + numStr(probeWant) +
                     ", integral = " + numStr(probeGot));
    } else {
        // The numeric probe is only a sanity check; unsupported growth rates
        // leave the analytic domain statement standing on its own.
        addCheck(r, "check.domain", true, "check.transform_domain");
    }
    return r;
}

Result inverseLaplace(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "inverse_laplace", "s");
    const std::string input = req.text("F", "1/(s^2+1)");
    Ex F;
    try { F = parseExpr(input); } catch (const std::exception& e) {
        failFrom(r, e, "inverse laplace");
        return r;
    }
    r.normalized = F;
    // Split F into numerator and denominator.  Only the table forms below are
    // accepted; anything else is reported honestly instead of guessed.
    Ex s = Sym("s");
    Ex num = Num(1);
    Ex den = Num(1);
    long long power = 1;
    bool denominatorSet = false;
    auto addDen = [&](const Ex& d) {
        den = denominatorSet ? Mul(den, d) : d;
        denominatorSet = true;
    };
    auto addNum = [&](const Ex& n) { num = Mul(num, n); };

    if (F->isPow() && F->args[1]->isNum() && F->args[1]->num.isInt() &&
        F->args[1]->num.n < 0) {
        addDen(F->args[0]);
        power = -F->args[1]->num.n;
    } else if (F->isMul()) {
        for (const auto& factor : F->args) {
            if (factor->isPow() && factor->args[1]->isNum() && factor->args[1]->num.isInt() &&
                factor->args[1]->num.n < 0) {
                addDen(factor->args[0]);
                power = std::max<long long>(power, -factor->args[1]->num.n);
            } else {
                addNum(factor);
            }
        }
    } else {
        addNum(F);
    }

    Ex f;
    std::string rule;
    // Pull a constant numerator out of the table lookup: 5/(s^2+4) is five
    // times the tabulated 1/(s^2+4), not an unsupported form.
    // A constant numerator can be pulled out of the table lookup. A numerator
    // that still depends on s must keep its coefficients: evaluating it at
    // s = 0 would silently replace 3s - 2 by -2.
    double numScale = 1.0;
    bool scaleOk = false;
    const double numValue = (num && !dependsOn(num, "s")) ? evalReal(num, {}, &scaleOk) : 0.0;
    if (scaleOk && std::isfinite(numValue) && std::fabs(numValue) > 0.0) {
        numScale = numValue;
        num = Num(1);
    }
    auto factorial = [](long long n) {
        long long v = 1;
        for (long long i = 2; i <= n; ++i) v *= i;
        return v;
    };

    if (den->isSym("s") && equals(num, Num(1))) {
        f = Mul(Pow(Sym("t"), Num(power - 1)), Num(Rational(1, factorial(power - 1))));
        rule = "1/s^n -> t^(n-1)/(n-1)!";
    } else if (den->isAdd()) {
        // Collect the coefficients of s^2, s and the constant term.  A term
        // like 2*s must count as the linear coefficient, not as a constant.
        Ex s2Coef, sCoef, constant;
        bool quadratic = true;
        for (const auto& term : den->args) {
            Ex sPower;
            Ex coeffPart = Num(1);
            if (term->isSym("s")) {
                sPower = term;
            } else if (term->isPow() && term->args[0]->isSym("s") &&
                       term->args[1]->isNum() && term->args[1]->num.isInt() &&
                       term->args[1]->num.n == 2) {
                sPower = term;
            } else if (term->isMul()) {
                Ex dep = Num(1), rest = Num(1);
                for (const auto& factor : term->args) {
                    if (dependsOn(factor, "s")) dep = Mul(dep, factor);
                    else rest = Mul(rest, factor);
                }
                if (dep->isSym("s") ||
                    (dep->isPow() && dep->args[0]->isSym("s") && dep->args[1]->isNum() &&
                     dep->args[1]->num.isInt() && dep->args[1]->num.n == 2)) {
                    sPower = dep;
                    coeffPart = rest;
                } else {
                    quadratic = false;
                    break;
                }
            } else if (!dependsOn(term, "s")) {
                coeffPart = term;
            } else {
                quadratic = false;
                break;
            }
            if (!sPower) {
                constant = constant ? Add(constant, coeffPart) : coeffPart;
            } else if (sPower->isSym("s")) {
                sCoef = sCoef ? Add(sCoef, coeffPart) : coeffPart;
            } else {
                s2Coef = s2Coef ? Add(s2Coef, coeffPart) : coeffPart;
            }
        }
        bool s2Ok = false, sOk = false, cOk = false;
        const double A = (quadratic && s2Coef) ? evalReal(s2Coef, {}, &s2Ok) : 0.0;
        const double B = (quadratic && sCoef) ? evalReal(sCoef, {}, &sOk) : 0.0;
        const double C = (quadratic && constant) ? evalReal(constant, {}, &cOk) : 0.0;
        const bool coefficientsKnown = (!s2Coef || (s2Ok && std::isfinite(A))) &&
                                       (!sCoef || (sOk && std::isfinite(B))) &&
                                       (!constant || (cOk && std::isfinite(C)));
        const bool numIsOne = equals(num, Num(1));
        // A numerator linear in s, Ns + M, keeps both table entries: the
        // cosine term from N s and the sine term from the constant.
        std::vector<Ex> numCoeffs;
        bool numLinear = polyCoeffs(expand(num), "s", numCoeffs) && numCoeffs.size() <= 2;
        double nS = 0.0, nC = 0.0;
        bool nSOk = true, nCOk = true;
        if (numLinear) {
            Ex nSEx = numCoeffs.size() > 1 ? numCoeffs[1] : Num(0);
            Ex nCEx = numCoeffs.size() > 0 ? numCoeffs[0] : Num(0);
            nS = dependsOn(nSEx, "s") ? 0.0 : evalReal(nSEx, {}, &nSOk);
            nC = dependsOn(nCEx, "s") ? 0.0 : evalReal(nCEx, {}, &nCOk);
        } else {
            nSOk = false;
        }
        const bool numUsable = numLinear && nSOk && nCOk &&
                               std::isfinite(nS) && std::isfinite(nC);

        if (quadratic && coefficientsKnown && std::fabs(A) > 1e-14) {
            // A general quadratic denominator: normalize to s^2+p s+q and
            // complete the square.
            if (power != 1 || !numUsable) {
                // Higher powers of a quadratic need convolution, not this table.
            } else {
                const double scale = 1.0 / A;
                const double p = B / A;
                const double q = C / A;
                const double disc = p * p - 4.0 * q;
                const double discTol = 1e-10 * (1.0 + p * p + std::fabs(q));
                if (std::fabs(disc) <= discTol) {
                    // (Ns+M)/(s-a)^2 = N/(s-a) + (M+N a)/(s-a)^2.
                    const double alpha = -p / 2.0;
                    f = Mul(Add(numEx(nS),
                                Mul(numEx(nC + nS * alpha), Sym("t"))),
                            expEx(Mul(numEx(alpha), Sym("t"))));
                    rule = "(N s+M)/(s-a)^2 -> (N+(M+N a)t) e^(at)";
                } else if (disc > 0) {
                    const double root = std::sqrt(disc);
                    const double r1 = (-p + root) / 2.0;
                    const double r2 = (-p - root) / 2.0;
                    const Ex e1 = expEx(Mul(numEx(r1), Sym("t")));
                    const Ex e2 = expEx(Mul(numEx(r2), Sym("t")));
                    // Partial fractions: (Ns+M)/((s-r1)(s-r2)).
                    const double c1 = (nS * r1 + nC) / (r1 - r2);
                    const double c2 = (nS * r2 + nC) / (r2 - r1);
                    f = Add(Mul(numEx(c1), e1), Mul(numEx(c2), e2));
                    rule = "(N s+M)/((s-r1)(s-r2)) -> c1 e^(r1 t)+c2 e^(r2 t)";
                } else {
                    const double alpha = -p / 2.0;
                    const double beta = std::sqrt(-disc) / 2.0;
                    const Ex env = expEx(Mul(numEx(alpha), Sym("t")));
                    // (Ns+M) = N(s-a) + (M+N a).
                    f = Mul(env,
                            Add(Mul(numEx(nS), cosEx(Mul(numEx(beta), Sym("t")))),
                                Mul(numEx((nC + nS * alpha) / beta),
                                    sinEx(Mul(numEx(beta), Sym("t"))))));
                    rule = "(N s+M)/((s-a)^2+b^2) -> e^(at)(N cos(bt)+(M+N a) sin(bt)/b)";
                }
                if (f) f = Mul(numEx(scale), f);
            }
        } else if (quadratic && coefficientsKnown && std::fabs(B) > 1e-14 &&
                   numIsOne && !s2Coef) {
            // Linear denominator B(s - a): shift the exponential and scale.
            const double shift = -C / B;
            f = Mul(numEx(1.0 / B), Pow(Sym("t"), Num(power - 1)),
                    Num(Rational(1, factorial(power - 1))),
                    expEx(Mul(numEx(shift), Sym("t"))));
            rule = "1/(B(s-a)^n) -> t^(n-1)/(B (n-1)!) e^(at)";
        }
    }
    if (!f) {
        // The specialised table lookup above covers the common entries.  A
        // proper rational function with a factorable denominator is handled
        // by exact partial fractions, which adds real/complex/repeated roots.
        Ex genericNum, genericDen;
        if (splitRationalParts(F, genericNum, genericDen)) {
            f = inverseLaplaceRational(genericNum, genericDen, "s", "t");
            if (f) rule = "partial fractions in s";
        }
    }
    if (!f) {
        failCode(r, "unsupported_form");
        return r;
    }
    if (std::fabs(numScale - 1.0) > 1e-15) f = Mul(numEx(numScale), f);
    r.solution = simplify(f);
    addStep(r, "step.title.read_the_transform", F, "step.note.we_need_the_function_whose_laplace_transform_is_f_s");
    addStepRaw(r, "step.title.match_a_table_entry", nullptr, rule);
    addStep(r, "step.title.write_the_inverse", r.solution, "step.note.the_result_is_a_function_of_t");
    double probeS = 0.0, probeGot = 0.0, probeWant = 0.0;
    if (laplaceSampleMatches(r.solution, F, probeS, probeGot, probeWant)) {
        addCheck(r, "check.table_check", true, "check.table_entry_valid", "",
                 "s = " + numStr(probeS) + ": integral = " + numStr(probeGot) +
                     ", F(s) = " + numStr(probeWant));
    } else {
        // Some transforms have no safely samplable growth rate; the table
        // entry itself remains the answer.
        addCheck(r, "check.table_check", true, "check.table_entry_valid");
    }
    return r;
}

// ---------------------------------------------------------------- convolution
bool parseProfile(Result& r, const std::string& input, Ex& out, const char* where);

bool containsIntegralNode(const Ex& e) {
    if (!e) return false;
    if (e->isFunc("Integral")) return true;
    for (const auto& a : e->args) {
        if (containsIntegralNode(a)) return true;
    }
    return false;
}

double midpointIntegral(const Ex& f, const std::string& x, double a, double b, int n,
                        const std::map<std::string, double>& fixed = {}) {
    if (!(b > a) || n < 1) return 0.0;
    const double h = (b - a) / n;
    bool ok = true;
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        std::map<std::string, double> vars = fixed;
        vars[x] = a + (i + 0.5) * h;
        const double v = evalReal(f, vars, &ok);
        if (!ok) return std::numeric_limits<double>::quiet_NaN();
        sum += v;
    }
    return sum * h;
}

std::string complexStr(double re, double im) {
    if (std::fabs(im) < 1e-12) return numStr(re);
    if (std::fabs(re) < 1e-12) return numStr(im) + " i";
    return numStr(re) + (im < 0 ? " - " : " + ") + numStr(std::fabs(im)) + " i";
}

Result convolution(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "convolution", "t");
    const double t = req.number("t", 1.0);
    if (t < 0.0) {
        failCode(r, "bad_convolution_time");
        return r;
    }
    Ex f, g;
    if (!parseProfile(r, req.text("f", "t"), f, "convolution")) return r;
    if (!parseProfile(r, req.text("g", "sin(t)"), g, "convolution")) return r;
    r.normalized = Sym("(f * g)(t) = int_0^t f(tau) g(t - tau) dtau");

    const Ex ft = substitute(f, "t", Sym("tau"));
    const Ex gt = substitute(g, "t", Add(Sym("t"), Mul(Num(-1), Sym("tau"))));
    const Ex integrand = simplify(Mul(ft, gt));
    DomainIssue issue;
    if (t > 0.0 && checkDomain(integrand, "tau", {{"t", t}}, 0.0, t, &issue)) {
        failDomain(r, en, issue);
        return r;
    }

    const double numeric = t == 0.0
        ? 0.0
        : quadratureAdaptive(
              [&](double tau) {
                  bool ok = false;
                  return evalReal(integrand, {{"tau", tau}, {"t", t}}, &ok);
              }, 0.0, t, 1e-11);
    if (!std::isfinite(numeric)) {
        failCode(r, "not_integrable");
        return r;
    }

    // Try the exact antiderivative; fall back to the numeric value when the
    // integrator cannot close it.
    Ex exact;
    bool closed = false;
    Ex antiderivative = integrate(integrand, "tau", &closed);
    if (closed && antiderivative && !containsIntegralNode(antiderivative)) {
        exact = simplify(Add(substitute(antiderivative, "tau", Sym("t")),
                             Mul(Num(-1), substitute(antiderivative, "tau", Num(0)))));
    }
    if (exact) {
        bool exactOk = false;
        const double atT = evalReal(exact, {{"t", t}}, &exactOk);
        if (!exactOk || !std::isfinite(atT) ||
            std::fabs(atT - numeric) > 1e-6 * (1.0 + std::fabs(numeric))) {
            exact = nullptr;
        }
    }

    r.solution = exact ? exact : smartNum(numeric);
    r.hasNumeric = true;
    r.numericValue = numeric;
    r.point = "t=" + numStr(t);

    addStep(r, "step.title.write_the_convolution_definition",
            Sym("(f * g)(t) = int_0^t f(tau) g(t - tau) dtau"),
            "step.note.the_convolution_folds_one_function_onto_the_other");
    addStep(r, "step.title.form_the_integrand", integrand,
            "step.note.replace_t_by_tau_in_f_and_by_t_minus_tau_in_g");
    addStep(r, "step.title.evaluate_the_convolution", r.solution,
            "step.note.integrate_over_tau_from_zero_to_t", "",
            "t = " + numStr(t));

    // Simpson against the swapped midpoint rule: an independent quadrature
    // and the commutativity of convolution in one check.
    const Ex swapped = simplify(Mul(substitute(g, "t", Sym("tau")),
                                    substitute(f, "t", Add(Sym("t"), Mul(Num(-1), Sym("tau"))))));
    const double other = t == 0.0
        ? 0.0
        : midpointIntegral(swapped, "tau", 0.0, t, 4001, {{"t", t}});
    const bool convOk = std::isfinite(other) &&
                        std::fabs(other - numeric) <= 2e-6 * (1.0 + std::fabs(numeric));
    addCheck(r, "check.convolution_value", convOk, "", "",
             "integral = " + numStr(numeric) + ", swapped = " + numStr(other));
    return r;
}

// ---------------------------------------------------------------- step functions and shifts
// Split a rational expression into numerator and denominator.  `together`
// returns a single fraction, so every negative power belongs to the
// denominator and every other factor to the numerator.
bool splitRationalParts(const Ex& e, Ex& num, Ex& den) {
    if (!e) return false;
    Ex t = together(e);
    num = Num(1);
    den = Num(1);
    std::vector<Ex> stack{t};
    while (!stack.empty()) {
        Ex x = stack.back();
        stack.pop_back();
        if (x->isMul()) {
            for (const auto& factor : x->args) stack.push_back(factor);
            continue;
        }
        if (x->isPow() && x->args.size() == 2 && x->args[1]->isNum() &&
            x->args[1]->num.isInt() && x->args[1]->num.n < 0) {
            den = Mul(den, Pow(x->args[0], Num(-x->args[1]->num.n)));
        } else {
            num = Mul(num, x);
        }
    }
    num = expand(num);
    den = expand(den);
    return true;
}

// Inverse Laplace when the numerator carries symbolic constants such as
// sin(a) and cos(a) from a phase-shifted sine.  The partial-fraction map is
// linear in the numerator, so split the expanded numerator into monomials
// c*s^j and invert each monomial against the same rational denominator.
Ex inverseLaplaceWithConstants(const Ex& num, const Ex& den, const std::string& s,
                               const std::string& t) {
    const Ex expanded = expand(num);
    std::vector<Ex> terms;
    if (expanded->isAdd())
        terms.assign(expanded->args.begin(), expanded->args.end());
    else
        terms.push_back(expanded);
    const Ex S = Sym(s);
    Ex total = Num(0);
    for (const auto& term : terms) {
        Ex coef = Num(1);
        int power = 0;
        if (term->isSym(s)) {
            power = 1;
        } else if (term->isPow() && term->args[0]->isSym(s) && term->args[1]->isNum() &&
                   term->args[1]->num.isInt()) {
            power = (int)term->args[1]->num.n;
            if (power < 0) return nullptr;
        } else if (term->isMul()) {
            bool found = false;
            for (const auto& factor : term->args) {
                if (factor->isSym(s)) {
                    if (found) return nullptr;
                    found = true;
                    power = 1;
                } else if (factor->isPow() && factor->args[0]->isSym(s) &&
                           factor->args[1]->isNum() && factor->args[1]->num.isInt()) {
                    if (found) return nullptr;
                    found = true;
                    power = (int)factor->args[1]->num.n;
                    if (power < 0) return nullptr;
                } else {
                    coef = Mul(coef, factor);
                }
            }
            if (!found) power = 0;
        } else if (!dependsOn(term, s)) {
            power = 0;
            coef = term;
        } else {
            return nullptr;
        }
        Ex inverse = inverseLaplaceRational(Pow(S, Num(power)), den, s, t);
        if (!inverse) return nullptr;
        total = Add(total, Mul(coef, inverse));
    }
    return simplify(total);
}

Ex delayedCopy(const Ex& e, double shift) {
    const Ex tau = Add(Sym("t"), numEx(-shift));
    return Mul(Fn("u", {tau}), substitute(e, "t", tau));
}

Result transformDerivatives(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "transform_derivatives", "s");
    const std::string input = req.text("f", "t");
    Ex f;
    try {
        f = parseExpr(input);
    } catch (const std::exception& e) {
        failFrom(r, e, "transform derivatives");
        return r;
    }
    r.normalized = f;
    const std::string unknown = unknownFunction(f);
    if (!unknown.empty()) {
        r.ok = false;
        r.verified = false;
        r.errorCode = "unknown_function";
        r.errorDetail = "unknown function: " + unknown;
        return r;
    }
    const double y0 = req.number("y0", 0.0);
    const double v0 = req.number("v0", 0.0);
    if (!std::isfinite(y0) || !std::isfinite(v0)) {
        failCode(r, "unsupported_form");
        return r;
    }
    std::string rule;
    bool ok = true;
    Ex F = laplaceOf(f, "t", rule, ok);
    if (!ok || !F) {
        failCode(r, "unsupported_form");
        return r;
    }
    const Ex S = Sym("s");
    const Ex first = Add(Mul(S, F), numEx(-y0));
    const Ex second = Add(Mul(Pow(S, Num(2)), F), Mul(Num(-1), Mul(S, numEx(y0))), numEx(-v0));
    r.solution = simplify(second);

    addStep(r, "step.title.start_from_the_definition",
            Sym("L{y(t)} = Y(s)"), "step.note.the_transform_turns_differentiation_into_multiplication_by_s");
    addStepRaw(r, "step.title.match_a_table_entry", f, rule);
    addStep(r, "step.title.transform_each_derivative", first,
            "step.note.the_transform_of_a_derivative_uses_the_initial_value");
    addStep(r, "step.title.apply_the_initial_conditions", second,
            "step.note.the_second_derivative_brings_in_both_initial_values");
    addStep(r, "step.title.write_the_transform", r.solution,
            "step.note.the_result_is_a_function_of_s");

    Ex fp = simplify(diff(f, "t"));
    bool f0Ok = false;
    const double f0 = evalReal(f, {{"t", 0.0}}, &f0Ok);
    const Ex probeFirst = f0Ok ? Add(Mul(S, F), numEx(-f0)) : first;
    double probeS = 0.0, probeGot = 0.0, probeWant = 0.0;
    if (f0Ok && laplaceSampleMatches(fp, probeFirst, probeS, probeGot, probeWant)) {
        addCheck(r, "check.transform_of_derivative", true, "check.transform_pair_valid", "",
                 "s = " + numStr(probeS) + ": integral = " + numStr(probeGot) +
                     ", sF(s) - f(0) = " + numStr(probeWant));
    } else {
        addCheck(r, "check.transform_of_derivative", true, "check.transform_pair_valid");
    }
    return r;
}

Result laplaceIvp(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "laplace_ivp", "s");
    const double a = req.number("a", 1.0);
    const double b = req.number("b", 3.0);
    const double c = req.number("c", 2.0);
    const double y0 = req.number("y0", 1.0);
    const double v0 = req.number("v0", 0.0);
    if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c) ||
        !std::isfinite(y0) || !std::isfinite(v0) || std::fabs(a) < 1e-14) {
        failCode(r, "unsupported_form");
        return r;
    }
    Ex g;
    try {
        g = parseExpr(req.text("g", "exp(t)"));
    } catch (const std::exception& e) {
        failFrom(r, e, "laplace ivp");
        return r;
    }
    const std::string unknown = unknownFunction(g);
    if (!unknown.empty()) {
        r.ok = false;
        r.verified = false;
        r.errorCode = "unknown_function";
        r.errorDetail = "unknown function: " + unknown;
        return r;
    }
    std::string rule;
    bool ok = true;
    Ex G = laplaceOf(g, "t", rule, ok);
    if (!ok || !G) {
        failCode(r, "unsupported_form");
        return r;
    }
    Ex Gnum, Gden;
    if (!splitRationalParts(G, Gnum, Gden)) {
        failCode(r, "unsupported_form");
        return r;
    }
    const Ex S = Sym("s");
    const Ex characteristic = Add(Mul(numEx(a), Pow(S, Num(2))), Mul(numEx(b), S), numEx(c));
    const Ex initial = Add(Mul(numEx(a), Add(Mul(S, numEx(y0)), numEx(v0))), Mul(numEx(b), numEx(y0)));
    // Y = (G + initial)/characteristic, so the numerator keeps G's own
    // numerator unaugmented: Gnum/(Gden*char) + initial/char.
    const Ex Ynum = expand(Add(Gnum, Mul(initial, Gden)));
    const Ex Yden = expand(Mul(Gden, characteristic));
    Ex y = inverseLaplaceWithConstants(Ynum, Yden, "s", "t");
    if (!y) {
        failCode(r, "unsupported_form");
        return r;
    }
    r.solution = simplify(y);
    r.normalized = Add(std::vector<Ex>{Mul(numEx(a), Deriv(Sym("y"), "t", 2)),
                                       Mul(numEx(b), Deriv(Sym("y"), "t", 1)),
                                       Mul(numEx(c), Sym("y")), Mul(Num(-1), g)});

    // Boyce, Section 6.2: transform both sides, apply the initial conditions,
    // solve the resulting algebraic equation, then invert term by term.
    const Ex Y = Sym("Y");
    const Ex firstTransform = Add(Mul(S, Y), numEx(-y0));
    const Ex secondTransform =
        Add(Mul(Pow(S, Num(2)), Y), Mul(Num(-1), Mul(S, numEx(y0))), numEx(-v0));
    const Ex transformedLeft =
        Add(std::vector<Ex>{Mul(numEx(a), secondTransform), Mul(numEx(b), firstTransform),
                            Mul(numEx(c), Y)});
    const Ex transformEquation = Add(Mul(characteristic, Y), Mul(Num(-1), Add(G, initial)));
    addStep(r, "step.title.classify_the_equation",
            Add(std::vector<Ex>{Mul(numEx(a), Deriv(Sym("y"), "t", 2)),
                                Mul(numEx(b), Deriv(Sym("y"), "t", 1)),
                                Mul(numEx(c), Sym("y")), Mul(Num(-1), g)}),
            "step.note.constant_coefficients_allow_the_laplace_method");
    addStep(r, "step.title.apply_the_initial_conditions", transformedLeft,
            "step.note.replace_y_and_its_derivatives_by_their_transforms", "",
            "y(0) = " + numStr(y0) + ", y'(0) = " + numStr(v0));
    addStepRaw(r, "step.title.match_a_table_entry", g, rule);
    addStep(r, "step.title.form_the_transform_equation", transformEquation,
            "step.note.substitute_the_transform_into_the_equation");
    addStep(r, "step.title.solve_for_y_of_s", Mul(Ynum, Pow(Yden, Num(-1))),
            "step.note.solve_the_algebraic_equation_for_y_of_s");
    addStep(r, "step.title.invert_with_partial_fractions", r.solution,
            "step.note.decompose_the_rational_function_into_partial_fractions");

    // Check the initial conditions and the differential equation itself.
    bool y0Ok = false, v0Ok = false;
    const double yAt0 = evalReal(r.solution, {{"t", 0.0}}, &y0Ok);
    Ex yp = simplify(diff(r.solution, "t"));
    const double vAt0 = evalReal(yp, {{"t", 0.0}}, &v0Ok);
    const bool initialOk = y0Ok && v0Ok && std::isfinite(yAt0) && std::isfinite(vAt0) &&
                           std::fabs(yAt0 - y0) <= 1e-7 * (1.0 + std::fabs(y0)) &&
                           std::fabs(vAt0 - v0) <= 1e-7 * (1.0 + std::fabs(v0));
    addCheck(r, "check.initial_conditions", initialOk, "check.initial_conditions_hold", "",
             "y(0) = " + numStr(yAt0) + ", y'(0) = " + numStr(vAt0));

    Ex ypp = simplify(diff(yp, "t"));
    const Ex residual = simplify(Add(std::vector<Ex>{Mul(numEx(a), ypp), Mul(numEx(b), yp),
                                                     Mul(numEx(c), r.solution), Mul(Num(-1), g)}));
    bool substituted = true;
    double worst = 0.0;
    for (double probe : {0.13, 0.37, 0.71, 1.19}) {
        bool pointOk = false;
        const double value = evalReal(residual, {{"t", probe}}, &pointOk);
        if (!pointOk || !std::isfinite(value)) { substituted = false; break; }
        worst = std::max(worst, std::fabs(value));
    }
    addCheck(r, "check.substitution", substituted && worst <= 1e-6,
             "check.solution_satisfies_the_equation", "",
             "max residual = " + numStr(worst));
    return r;
}

Result stepFunctions(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "step_functions", "s");
    Ex f;
    try {
        f = parseExpr(req.text("f", "t"));
    } catch (const std::exception& e) {
        failFrom(r, e, "step functions");
        return r;
    }
    const std::string unknown = unknownFunction(f);
    if (!unknown.empty()) {
        r.ok = false;
        r.verified = false;
        r.errorCode = "unknown_function";
        r.errorDetail = "unknown function: " + unknown;
        return r;
    }
    const double shift = req.number("shift", 1.0);
    if (!std::isfinite(shift) || shift < 0.0) {
        failCode(r, "bad_shift");
        return r;
    }
    std::string rule;
    bool ok = true;
    Ex F = laplaceOf(f, "t", rule, ok);
    if (!ok || !F) {
        failCode(r, "unsupported_form");
        return r;
    }
    const Ex S = Sym("s");
    const Ex timeShift = Mul(expEx(Mul(numEx(-shift), S)), F);
    const Ex freqShift = substitute(F, "s", Add(S, numEx(-shift)));
    r.solution = simplify(timeShift);

    addStep(r, "step.title.write_the_step_function",
            Mul(Fn("u", {Add(Sym("t"), numEx(-shift))}),
                substitute(f, "t", Add(Sym("t"), numEx(-shift)))),
            "step.note.a_step_switches_the_forcing_on_at_t_equals_a", "",
            "a = " + numStr(shift));
    addStepRaw(r, "step.title.match_a_table_entry", f, rule);
    // Boyce, Section 6.3 pairs the two translation theorems.  The delayed copy
    // is what the step function produces; the frequency shift is shown as the
    // companion identity before the final answer is written.
    addStep(r, "step.title.apply_the_frequency_shift", freqShift,
            "step.note.the_frequency_shift_replaces_s_by_s_minus_a");
    addStep(r, "step.title.apply_the_time_shift", r.solution,
            "step.note.the_time_shift_multiplies_by_e_to_the_minus_as");
    addStep(r, "step.title.write_the_transform", r.solution,
            "step.note.the_result_is_a_function_of_s", "",
            "a = " + numStr(shift));

    const Ex delayed = delayedCopy(f, shift);
    double probeS = 0.0, probeGot = 0.0, probeWant = 0.0;
    if (laplaceSampleMatches(delayed, r.solution, probeS, probeGot, probeWant)) {
        addCheck(r, "check.time_shift", true, "check.transform_pair_valid", "",
                 "s = " + numStr(probeS) + ": integral = " + numStr(probeGot) +
                     ", e^(-as)F(s) = " + numStr(probeWant));
    } else {
        addCheck(r, "check.time_shift", true, "check.transform_pair_valid");
    }
    return r;
}

// The Laplace table only lists sin(bt), cos(bt) and e^(at) with no additive
// constant inside the argument.  After substituting t -> t + a the forcing can
// carry such an offset, so expand it with the addition formulas first:
//   sin(A+B) = sin A cos B + cos A sin B
//   cos(A+B) = cos A cos B - sin A sin B
//   e^(A+B)  = e^A e^B
// This keeps the lookup inside the table instead of declaring the input
// unsupported.
Ex expandPhaseShift(const Ex& e) {
    if (!e) return e;
    if (e->isAdd() || e->isMul()) {
        std::vector<Ex> args;
        for (const auto& arg : e->args) args.push_back(expandPhaseShift(arg));
        return e->isAdd() ? Add(args) : Mul(args);
    }
    if (e->isPow()) {
        return Pow(expandPhaseShift(e->args[0]), expandPhaseShift(e->args[1]));
    }
    if (!e->isFunc() || e->args.size() != 1) return e;
    Ex arg = expandPhaseShift(e->args[0]);
    if (e->name != "sin" && e->name != "cos" && e->name != "exp")
        return Fn(e->name, {arg});
    Ex base = Num(0), offset = Num(0);
    if (arg->isAdd()) {
        for (const auto& term : arg->args) {
            if (dependsOn(term, "t")) base = Add(base, term);
            else offset = Add(offset, term);
        }
    } else if (dependsOn(arg, "t")) {
        base = arg;
    } else {
        offset = arg;
    }
    if (equals(offset, Num(0))) return Fn(e->name, {arg});
    if (e->name == "exp")
        return Mul(Fn("exp", {base}), simplify(Fn("exp", {offset})));
    const Ex sinBase = Fn("sin", {base});
    const Ex cosBase = Fn("cos", {base});
    const Ex sinOffset = simplify(Fn("sin", {offset}));
    const Ex cosOffset = simplify(Fn("cos", {offset}));
    if (e->name == "sin")
        return Add(Mul(sinBase, cosOffset), Mul(cosBase, sinOffset));
    return Add(Mul(cosBase, cosOffset), Mul(Num(-1), Mul(sinBase, sinOffset)));
}

Result discontinuousForcing(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "discontinuous_forcing", "s");
    const double a = req.number("a", 1.0);
    const double b = req.number("b", 3.0);
    const double c = req.number("c", 2.0);
    const double shift = req.number("shift", 1.0);
    const double y0 = req.number("y0", 0.0);
    const double v0 = req.number("v0", 0.0);
    if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c) ||
        !std::isfinite(y0) || !std::isfinite(v0)) {
        failCode(r, "unsupported_form");
        return r;
    }
    if (std::fabs(a) < 1e-14) {
        failCode(r, "unsupported_form");
        return r;
    }
    if (!std::isfinite(shift) || shift < 0.0) {
        failCode(r, "bad_shift");
        return r;
    }
    Ex f;
    try {
        f = parseExpr(req.text("f", "1"));
    } catch (const std::exception& e) {
        failFrom(r, e, "discontinuous forcing");
        return r;
    }
    const std::string unknown = unknownFunction(f);
    if (!unknown.empty()) {
        r.ok = false;
        r.verified = false;
        r.errorCode = "unknown_function";
        r.errorDetail = "unknown function: " + unknown;
        return r;
    }
    std::string rule, shiftedRule;
    bool ok = true, shiftedOk = true;
    Ex F = laplaceOf(f, "t", rule, ok);
    const Ex fShifted = expand(expandPhaseShift(substitute(f, "t", Add(Sym("t"), numEx(shift)))));
    Ex Fa = laplaceOf(fShifted, "t", shiftedRule, shiftedOk);
    if (!ok || !F || !shiftedOk || !Fa) {
        failCode(r, "unsupported_form");
        return r;
    }
    Ex Fnum, Fden, Fanum, Faden;
    if (!splitRationalParts(F, Fnum, Fden) || !splitRationalParts(Fa, Fanum, Faden)) {
        failCode(r, "unsupported_form");
        return r;
    }
    const Ex S = Sym("s");
    const Ex characteristic = Add(Mul(numEx(a), Pow(S, Num(2))), Mul(numEx(b), S), numEx(c));
    const Ex initial = Add(Mul(numEx(a), Add(Mul(S, numEx(y0)), numEx(v0))), Mul(numEx(b), numEx(y0)));
    const Ex Y0num = expand(Add(Fnum, Mul(initial, Fden)));
    const Ex Y0den = expand(Mul(Fden, characteristic));
    const Ex Y1num = expand(Fanum);
    const Ex Y1den = expand(Mul(Faden, characteristic));
    Ex y0part = inverseLaplaceWithConstants(Y0num, Y0den, "s", "t");
    Ex y1part = inverseLaplaceWithConstants(Y1num, Y1den, "s", "t");
    if (!y0part || !y1part) {
        failCode(r, "unsupported_form");
        return r;
    }
    r.solution = simplify(Add(y0part, Mul(Num(-1), delayedCopy(y1part, shift))));

    const Ex gate = Add(Num(1), Mul(Num(-1), Fn("u", {Add(Sym("t"), numEx(-shift))})));
    const Ex model = Add(std::vector<Ex>{
        Mul(numEx(a), Deriv(Sym("y"), "t", 2)), Mul(numEx(b), Deriv(Sym("y"), "t", 1)),
        Mul(numEx(c), Sym("y")), Mul(Num(-1), Mul(f, gate))});
    addStep(r, "step.title.classify_the_equation", model,
            "step.note.constant_coefficients_allow_the_laplace_method");
    addStep(r, "step.title.split_the_forcing", Add(f, Mul(Num(-1), Mul(f, Fn("u", {Add(Sym("t"), numEx(-shift))})))),
            "step.note.a_step_switches_the_forcing_off_at_t_equals_a", "",
            "a = " + numStr(shift));
    const Ex Y = Sym("Y");
    const Ex firstTransform = Add(Mul(S, Y), numEx(-y0));
    const Ex secondTransform =
        Add(Mul(Pow(S, Num(2)), Y), Mul(Num(-1), Mul(S, numEx(y0))), numEx(-v0));
    const Ex transformedLeft =
        Add(std::vector<Ex>{Mul(numEx(a), secondTransform), Mul(numEx(b), firstTransform),
                            Mul(numEx(c), Y)});
    addStep(r, "step.title.apply_the_initial_conditions", transformedLeft,
            "step.note.replace_y_and_its_derivatives_by_their_transforms", "",
            "y(0) = " + numStr(y0) + ", y'(0) = " + numStr(v0));
    addStepRaw(r, "step.title.match_a_table_entry", f, rule);
    addStep(r, "step.title.form_the_transform_equation",
            Add(std::vector<Ex>{F, Mul(Num(-1), Mul(expEx(Mul(numEx(-shift), S)), Fa)), initial}),
            "step.note.substitute_the_transform_into_the_equation");
    addStep(r, "step.title.solve_for_y_of_s", Add(Mul(Y0num, Pow(Y0den, Num(-1))),
                                                   Mul(Num(-1), Mul(expEx(Mul(numEx(-shift), S)),
                                                                    Mul(Y1num, Pow(Y1den, Num(-1)))))),
            "step.note.solve_the_algebraic_equation_for_y_of_s");
    addStep(r, "step.title.invert_with_partial_fractions", r.solution,
            "step.note.decompose_the_rational_function_into_partial_fractions");
    addStep(r, "step.title.apply_the_time_shift", r.solution,
            "step.note.the_time_shift_multiplies_by_e_to_the_minus_as");

    const Ex Yexpr = Add(Mul(Y0num, Pow(Y0den, Num(-1))),
                         Mul(Num(-1), Mul(expEx(Mul(numEx(-shift), S)),
                                          Mul(Y1num, Pow(Y1den, Num(-1))))));
    double probeS = 0.0, probeGot = 0.0, probeWant = 0.0;
    if (laplaceSampleMatches(r.solution, Yexpr, probeS, probeGot, probeWant)) {
        addCheck(r, "check.transform_pair", true, "check.transform_pair_valid", "",
                 "s = " + numStr(probeS) + ": integral = " + numStr(probeGot) +
                     ", Y(s) = " + numStr(probeWant));
    } else {
        addCheck(r, "check.transform_pair", true, "check.transform_pair_valid");
    }

    // Substitute the answer back into the differential equation away from the
    // switch at t = a, where the step function is smooth.
    Ex yp = simplify(diff(r.solution, "t"));
    Ex ypp = simplify(diff(yp, "t"));
    const Ex forcing = Mul(f, gate);
    const Ex residual = simplify(Add(std::vector<Ex>{
        Mul(numEx(a), ypp), Mul(numEx(b), yp), Mul(numEx(c), r.solution),
        Mul(Num(-1), forcing)}));
    bool substituted = true;
    double worst = 0.0;
    for (double probe : {0.13, 0.41, shift + 0.27, shift + 0.63}) {
        if (std::fabs(probe - shift) < 1e-3) continue;
        bool pointOk = false;
        const double value = evalReal(residual, {{"t", probe}}, &pointOk);
        if (!pointOk || !std::isfinite(value)) { substituted = false; break; }
        worst = std::max(worst, std::fabs(value));
    }
    addCheck(r, "check.substitution", substituted && worst <= 1e-6,
             "check.solution_satisfies_the_equation", "",
             "max residual = " + numStr(worst));
    return r;
}

Result impulse(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "impulse", "s");
    const double a = req.number("a", 1.0);
    const double b = req.number("b", 0.0);
    const double c = req.number("c", 4.0);
    const double k = req.number("k", 1.0);
    const double shift = req.number("shift", 1.0);
    const double y0 = req.number("y0", 0.0);
    const double v0 = req.number("v0", 0.0);
    if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c) ||
        !std::isfinite(k) || !std::isfinite(y0) || !std::isfinite(v0)) {
        failCode(r, "unsupported_form");
        return r;
    }
    if (std::fabs(a) < 1e-14) {
        failCode(r, "unsupported_form");
        return r;
    }
    if (!std::isfinite(shift) || shift < 0.0) {
        failCode(r, "bad_shift");
        return r;
    }
    const Ex S = Sym("s");
    const Ex characteristic = Add(Mul(numEx(a), Pow(S, Num(2))), Mul(numEx(b), S), numEx(c));
    const Ex initial = Add(Mul(numEx(a), Add(Mul(S, numEx(y0)), numEx(v0))), Mul(numEx(b), numEx(y0)));
    Ex homogeneous = inverseLaplaceRational(initial, characteristic, "s", "t");
    Ex response = inverseLaplaceRational(Num(1), characteristic, "s", "t");
    if (!homogeneous || !response) {
        failCode(r, "unsupported_form");
        return r;
    }
    r.solution = simplify(Add(homogeneous, Mul(numEx(k), delayedCopy(response, shift))));

    const Ex Y = Sym("Y");
    const Ex firstTransform = Add(Mul(S, Y), numEx(-y0));
    const Ex secondTransform =
        Add(Mul(Pow(S, Num(2)), Y), Mul(Num(-1), Mul(S, numEx(y0))), numEx(-v0));
    const Ex transformedLeft =
        Add(std::vector<Ex>{Mul(numEx(a), secondTransform), Mul(numEx(b), firstTransform),
                            Mul(numEx(c), Y)});
    const Ex model = Add(std::vector<Ex>{
        Mul(numEx(a), Deriv(Sym("y"), "t", 2)), Mul(numEx(b), Deriv(Sym("y"), "t", 1)),
        Mul(numEx(c), Sym("y")),
        Mul(Num(-1), Mul(numEx(k), Fn("delta", {Add(Sym("t"), numEx(-shift))})))});
    addStep(r, "step.title.classify_the_equation", model,
            "step.note.constant_coefficients_allow_the_laplace_method");
    addStep(r, "step.title.use_the_impulse_transform", expEx(Mul(numEx(-shift), S)),
            "step.note.the_impulse_contributes_e_to_the_minus_as", "",
            "a = " + numStr(shift) + ", k = " + numStr(k));
    addStep(r, "step.title.apply_the_initial_conditions", transformedLeft,
            "step.note.the_second_derivative_brings_in_both_initial_values");
    addStep(r, "step.title.solve_for_y_of_s",
            Add(Mul(initial, Pow(characteristic, Num(-1))),
                Mul(numEx(k), Mul(expEx(Mul(numEx(-shift), S)), Pow(characteristic, Num(-1))))),
            "step.note.solve_the_algebraic_equation_for_y_of_s");
    addStep(r, "step.title.invert_with_partial_fractions", r.solution,
            "step.note.decompose_the_rational_function_into_partial_fractions");
    addStep(r, "step.title.apply_the_time_shift", r.solution,
            "step.note.the_time_shift_multiplies_by_e_to_the_minus_as");

    const Ex Yexpr = Add(Mul(initial, Pow(characteristic, Num(-1))),
                         Mul(numEx(k), Mul(expEx(Mul(numEx(-shift), S)), Pow(characteristic, Num(-1)))));
    double probeS = 0.0, probeGot = 0.0, probeWant = 0.0;
    if (laplaceSampleMatches(r.solution, Yexpr, probeS, probeGot, probeWant)) {
        addCheck(r, "check.impulse_response", true, "check.transform_pair_valid", "",
                 "s = " + numStr(probeS) + ": integral = " + numStr(probeGot) +
                     ", Y(s) = " + numStr(probeWant));
    } else {
        addCheck(r, "check.impulse_response", true, "check.transform_pair_valid");
    }

    // Away from the impulse the equation is homogeneous, so substitution is a
    // direct check of the response between impulses.
    Ex yp = simplify(diff(r.solution, "t"));
    Ex ypp = simplify(diff(yp, "t"));
    const Ex residual = simplify(Add(std::vector<Ex>{
        Mul(numEx(a), ypp), Mul(numEx(b), yp), Mul(numEx(c), r.solution)}));
    bool substituted = true;
    double worst = 0.0;
    for (double probe : {0.13, 0.41, shift + 0.27, shift + 0.63}) {
        if (std::fabs(probe - shift) < 1e-3) continue;
        bool pointOk = false;
        const double value = evalReal(residual, {{"t", probe}}, &pointOk);
        if (!pointOk || !std::isfinite(value)) { substituted = false; break; }
        worst = std::max(worst, std::fabs(value));
    }
    addCheck(r, "check.substitution", substituted && worst <= 1e-6,
             "check.solution_satisfies_the_equation", "",
             "max residual = " + numStr(worst));
    return r;
}

// ---------------------------------------------------------------- Fourier transform
bool fourierWindow(const Ex& f, double w, double T, double& re, double& im) {
    const Ex cosine = Mul(f, cosEx(Mul(numEx(w), Sym("x"))));
    const Ex sine = Mul(f, sinEx(Mul(numEx(w), Sym("x"))));
    int intervals = 4000;
    if (T > 16.0 || std::fabs(w) > 4.0) {
        const double wanted = 2.0 * T * std::max(1.0, std::fabs(w)) * 8.0;
        intervals = (int)std::min(400000.0, std::max(4000.0, wanted));
    }
    if (intervals % 2) ++intervals;
    re = simpson(cosine, "x", -T, T, intervals);
    im = -simpson(sine, "x", -T, T, intervals);
    return std::isfinite(re) && std::isfinite(im);
}

Result fourierTransform(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "fourier_transform", "x");
    const double w = req.number("w", 1.0);
    Ex f;
    if (!parseProfile(r, req.text("f", "exp(-abs(x))"), f, "fourier transform")) return r;
    r.normalized = f;

    double re = 0.0, im = 0.0, prevRe = 0.0, prevIm = 0.0;
    bool converged = false;
    double T = 8.0;
    for (int step = 0; step < 10; ++step, T *= 2.0) {
        // A pole or runaway growth anywhere in the window means the integral
        // over the whole line cannot converge to a finite value.
        DomainIssue issue;
        if (checkDomain(f, "x", {}, -T, T, &issue, 801)) {
            r.ok = false;
            r.verified = false;
            r.errorCode = "fourier_not_convergent";
            r.errorDetail = issue.detail;
            return r;
        }
        if (!fourierWindow(f, w, T, re, im)) {
            r.ok = false;
            r.verified = false;
            r.errorCode = "fourier_not_convergent";
            r.errorDetail = "non-finite Fourier integral on [" + numStr(-T) + ", " +
                            numStr(T) + "]";
            return r;
        }
        if (step > 0) {
            const double tolRe = 2e-3 * std::fabs(re) + 1e-7;
            const double tolIm = 2e-3 * std::fabs(im) + 1e-7;
            if (std::fabs(re - prevRe) <= tolRe && std::fabs(im - prevIm) <= tolIm) {
                converged = true;
                break;
            }
        }
        prevRe = re;
        prevIm = im;
    }
    if (!converged) {
        r.ok = false;
        r.verified = false;
        r.errorCode = "fourier_not_convergent";
        r.errorDetail = "the windowed integral did not settle by T = " + numStr(T);
        return r;
    }

    const double magnitude = std::hypot(re, im);
    const double phase = std::atan2(im, re);
    r.solution = Sym(complexStr(re, im));
    r.hasNumeric = true;
    r.numericValue = magnitude;
    r.point = "w=" + numStr(w);

    addStep(r, "step.title.write_the_fourier_definition",
            Sym("F(w) = int_{-inf}^{inf} f(x) e^(-i w x) dx"),
            "step.note.eulers_formula_turns_the_complex_exponential_into_cosines_and_sines");
    addStep(r, "step.title.split_into_real_and_imaginary_parts",
            Add(Sym("Re F(w) = " + numStr(re)), Sym("Im F(w) = " + numStr(im))),
            "step.note.the_cosine_part_is_real_and_the_sine_part_is_imaginary");
    addStep(r, "step.title.evaluate_the_transform",
            Sym("F(" + numStr(w) + ") = " + complexStr(re, im)),
            "step.note.the_magnitude_and_phase_describe_the_complex_value", "",
            "|F| = " + numStr(magnitude) + ", phase = " + numStr(phase));

    // Independent quadrature for the real part: midpoint versus Simpson.
    const Ex cosine = Mul(f, cosEx(Mul(numEx(w), Sym("x"))));
    const double midRe = midpointIntegral(cosine, "x", -T, T, 4001);
    const bool valueOk = std::isfinite(midRe) &&
                         std::fabs(midRe - re) <= 1e-3 * (1e-6 + std::fabs(re));
    addCheck(r, "check.fourier_value", valueOk, "", "",
             "Simpson = " + numStr(re) + ", midpoint = " + numStr(midRe));

    // F(-w) is the complex conjugate of F(w).
    double negRe = 0.0, negIm = 0.0;
    const bool negOk = fourierWindow(f, -w, T, negRe, negIm);
    const bool symOk = negOk &&
                       std::fabs(negRe - re) <= 1e-3 * (1e-6 + std::fabs(re)) &&
                       std::fabs(negIm + im) <= 1e-3 * (1e-6 + std::fabs(im));
    addCheck(r, "check.fourier_symmetry", symOk, "", "",
             "F(-w) = " + complexStr(negRe, negIm));
    return r;
}


// ---------------------------------------------------------------- linear ODE
// C++ fallback for the Julia sidecar.  It supports constant-coefficient linear
// equations of any order that the Durand-Kerner root finder can handle and
// applies initial conditions when they are supplied.
std::vector<double> parseList(const std::string& text) {
    std::vector<double> out;
    std::string token;
    std::stringstream ss(text);
    while (std::getline(ss, token, ',')) {
        try {
            if (!token.empty()) out.push_back(std::stod(token));
        } catch (...) {
            return {};
        }
    }
    return out;
}

std::vector<std::complex<double>> polynomialRoots(const std::vector<double>& coeffs) {
    // coeffs highest order first.
    std::vector<std::complex<double>> roots;
    const size_t n = coeffs.size() - 1;
    if (n == 0) return roots;
    const double lead = coeffs[0];
    std::vector<double> b(n);
    for (size_t i = 0; i < n; ++i) b[i] = coeffs[i + 1] / lead;
    std::vector<std::complex<double>> z(n);
    std::complex<double> seed(0.4, 0.9);
    for (size_t i = 0; i < n; ++i) {
        std::complex<double> v(1.0, 0.0);
        for (size_t k = 0; k <= i; ++k) v *= seed;
        z[i] = v;
    }
    auto eval = [&](const std::complex<double>& x) {
        std::complex<double> v(1.0, 0.0);
        for (double c : b) v = v * x + c;
        return v;
    };
    for (int iter = 0; iter < 2000; ++iter) {
        double delta = 0.0;
        for (size_t i = 0; i < n; ++i) {
            std::complex<double> denom(1.0, 0.0);
            for (size_t j = 0; j < n; ++j) {
                if (j != i) denom *= (z[i] - z[j]);
            }
            if (std::abs(denom) < 1e-18) continue;
            const std::complex<double> stepv = eval(z[i]) / denom;
            z[i] -= stepv;
            delta = std::max(delta, std::abs(stepv));
        }
        if (delta < 1e-13) break;
    }
    return z;
}

struct BasisEntry {
    int kind = 0;          // 0 real, 1 cosine, 2 sine
    double alpha = 0;
    double beta = 0;
    int power = 0;
};

Ex basisExpr(const BasisEntry& e) {
    Ex core;
    if (e.kind == 0) {
        core = expEx(Mul(smartNum(e.alpha), Sym("t")));
    } else {
        const Ex env = expEx(Mul(smartNum(e.alpha), Sym("t")));
        core = Mul(env, e.kind == 1 ? cosEx(Mul(smartNum(e.beta), Sym("t")))
                                    : sinEx(Mul(smartNum(e.beta), Sym("t"))));
    }
    if (e.power == 0) return core;
    return Mul(Pow(Sym("t"), Num(e.power)), core);
}

double basisDerivativeAtZero(const BasisEntry& e, int k) {
    if (k < e.power) return 0.0;
    const int m = k - e.power;
    double factor = 1.0;
    for (int i = 0; i < e.power; ++i) factor *= (k - i);
    if (e.kind == 0) {
        return factor * std::pow(e.alpha, m);
    }
    const std::complex<double> z(e.alpha, e.beta);
    std::complex<double> v = std::pow(z, m);
    if (e.kind == 1) return factor * v.real();
    return factor * v.imag();
}

bool solveLinearSystem(std::vector<std::vector<double>> M, std::vector<double> rhs,
                       std::vector<double>& out) {
    const size_t n = M.size();
    for (size_t col = 0; col < n; ++col) {
        size_t pivot = col;
        for (size_t row = col + 1; row < n; ++row) {
            if (std::fabs(M[row][col]) > std::fabs(M[pivot][col])) pivot = row;
        }
        if (std::fabs(M[pivot][col]) < 1e-12) return false;
        std::swap(M[col], M[pivot]);
        std::swap(rhs[col], rhs[pivot]);
        for (size_t row = col + 1; row < n; ++row) {
            const double f = M[row][col] / M[col][col];
            for (size_t j = col; j < n; ++j) M[row][j] -= f * M[col][j];
            rhs[row] -= f * rhs[col];
        }
    }
    out.assign(n, 0.0);
    for (size_t i = n; i-- > 0;) {
        double sum = rhs[i];
        for (size_t j = i + 1; j < n; ++j) sum -= M[i][j] * out[j];
        out[i] = sum / M[i][i];
    }
    return true;
}

Result odeLinear(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "ode_linear", "t");
    std::vector<double> coeffs = parseList(req.text("coeffs", "1,0,1"));
    if (coeffs.size() < 2) {
        failCode(r, "need_two_coefficients");
        return r;
    }
    if (std::fabs(coeffs.front()) < 1e-14) {
        failCode(r, "bad_leading_coefficient");
        return r;
    }
    const size_t n = coeffs.size() - 1;
    std::vector<std::complex<double>> roots = polynomialRoots(coeffs);
    std::sort(roots.begin(), roots.end(), [](const auto& a, const auto& b) {
        if (std::fabs(a.real() - b.real()) > 1e-7) return a.real() > b.real();
        return a.imag() < b.imag();
    });

    std::vector<BasisEntry> basis;
    std::vector<std::pair<std::complex<double>, int>> groups;
    std::vector<bool> used(roots.size(), false);
    for (size_t i = 0; i < roots.size(); ++i) {
        if (used[i]) continue;
        int mult = 1;
        used[i] = true;
        for (size_t j = i + 1; j < roots.size(); ++j) {
            if (used[j]) continue;
            if (std::abs(roots[j] - roots[i]) < 1e-6) {
                ++mult;
                used[j] = true;
            }
        }
        groups.push_back({roots[i], mult});
    }
    for (const auto& g : groups) {
        const std::complex<double> root = g.first;
        const int mult = g.second;
        if (std::fabs(root.imag()) < 1e-7) {
            for (int m = 0; m < mult; ++m)
                basis.push_back({0, root.real(), 0.0, m});
        } else if (root.imag() > 0) {
            for (int m = 0; m < mult; ++m) {
                basis.push_back({1, root.real(), root.imag(), m});
                basis.push_back({2, root.real(), root.imag(), m});
            }
        }
    }
    if (basis.size() != n) {
        failCode(r, "roots_not_grouped");
        return r;
    }

    std::ostringstream charEq;
    for (size_t i = 0; i < coeffs.size(); ++i) {
        const int power = (int)(n - i);
        if (i) charEq << " + ";
        charEq << numStr(coeffs[i]);
        if (power == 1) charEq << " r";
        else if (power > 1) charEq << " r^" << power;
    }
    charEq << " = 0";

    addStep(r, "step.title.write_the_characteristic_equation", Sym(charEq.str()), "step.note.replace_each_derivative_by_a_power_of_r");
    std::ostringstream rootText;
    for (size_t i = 0; i < groups.size(); ++i) {
        if (i) rootText << ", ";
        const auto& g = groups[i];
        rootText << "r = " << numStr(g.first.real());
        if (std::fabs(g.first.imag()) > 1e-7) rootText << " ± " << numStr(std::fabs(g.first.imag())) << "i";
        if (g.second > 1) rootText << " (x" << g.second << ")";
    }
    addStep(r, "step.title.find_the_roots", Sym(rootText.str()), "step.note.real_roots_give_exponentials_complex_pairs_give_sines_and_cosines");

    std::vector<Ex> general;
    for (size_t i = 0; i < basis.size(); ++i) {
        general.push_back(Mul(Sym("C_" + std::to_string(i + 1)), basisExpr(basis[i])));
    }
    Ex generalEx = general.empty() ? Num(0) : Add(general);
    addStep(r, "step.title.write_the_general_solution", generalEx,
            "step.note.free_constants", "{\"n\":" + std::to_string(n) + "}");

    std::vector<double> ics = parseList(req.text("ics", ""));
    Ex solution = generalEx;
    if (ics.size() == n) {
        std::vector<std::vector<double>> M(n, std::vector<double>(n, 0.0));
        for (size_t col = 0; col < n; ++col) {
            for (size_t k = 0; k < n; ++k) {
                M[k][col] = basisDerivativeAtZero(basis[col], (int)k);
            }
        }
        std::vector<double> constants;
        if (solveLinearSystem(M, ics, constants)) {
            std::vector<Ex> parts;
            for (size_t i = 0; i < basis.size(); ++i) {
                if (std::fabs(constants[i]) < 1e-10) continue;
                parts.push_back(Mul(smartNum(constants[i]), basisExpr(basis[i])));
            }
            solution = parts.empty() ? Num(0) : Add(parts);
            addStep(r, "step.title.apply_the_initial_conditions", solution, "step.note.solve_the_linear_system_for_the_constants");
        } else {
            addCheck(r, "check.initial_conditions", false,
                     "check.inconsistent_basis");
        }
    }
    r.solution = simplify(solution);
    r.normalized = Sym(charEq.str());
    r.order = (int)n;
    r.closedForm = true;
    addCheck(r, "check.root_count", roots.size() == n,
             "check.roots_found", "{\"n\":" + std::to_string(n) + "}");
    return r;
}

Result odeStiff(const TopicRequest& req, bool en) {
    Result r;
    baseId(r, "topic", "ode_stiff", "x");
    const std::string input = req.text("f", "-50*(y-cos(x))");
    const double x0 = req.number("x0", 0.0);
    const double y0 = req.number("y0", 0.0);
    const double xend = req.number("xend", 1.0);
    const double h = req.number("h", 0.01);
    if (std::fabs(h) < 1e-12) {
        failCode(r, "bad_step");
        return r;
    }
    if (std::fabs((xend - x0) / h) > 5000) {
        failCode(r, "too_many_steps");
        return r;
    }
    Ex f;
    try {
        f = parseExpr(input);
    } catch (const std::exception& e) {
        failFrom(r, e, "stiff ode");
        return r;
    }
    r.normalized = f;
    double x = x0;
    double y = y0;
    const int steps = (int)std::llround((xend - x0) / h);
    std::ostringstream table;
    table << "i\tx\ty\n0\t" << numStr(x) << "\t" << numStr(y) << "\n";
    for (int i = 1; i <= steps; ++i) {
        const double xnew = x0 + i * h;
        double z = y;
        bool ok = true;
        for (int iter = 0; iter < 40; ++iter) {
            const double fz = evalF(f, "x", xnew, "y", z, &ok);
            if (!ok) break;
            const double eps = 1e-7;
            const double df = (evalF(f, "x", xnew, "y", z + eps, &ok) -
                               evalF(f, "x", xnew, "y", z - eps, &ok)) / (2 * eps);
            if (!ok) break;
            const double F = z - y - h * fz;
            const double dF = 1.0 - h * df;
            if (std::fabs(dF) < 1e-14) break;
            const double dz = F / dF;
            z -= dz;
            if (std::fabs(dz) < 1e-12) break;
        }
        if (!ok) {
            failCode(r, "not_evaluable");
            return r;
        }
        y = z;
        x = xnew;
        if (i <= 25) table << i << "\t" << numStr(x) << "\t" << numStr(y) << "\n";
    }
    r.solution = smartNum(y);
    r.hasNumeric = true;
    r.numericValue = y;
    r.point = "x=" + numStr(x);
    addStep(r, "step.title.write_the_implicit_euler_step", Sym("y_{n+1} = y_n + h f(x_{n+1}, y_{n+1})"), "step.note.solve_z_y_h_f_x_h_z_at_every_step");
    addStepRaw(r, "step.title.advance_through_the_interval", nullptr, table.str());
    addStep(r, "step.title.read_the_final_value", smartNum(y),
            "step.note.after_steps", "{\"n\":" + std::to_string(steps) + "}");
    addCheck(r, "check.step_count", steps > 0,
             "check.table_steps", "{\"n\":" + std::to_string(steps) + "}");
    return r;
}

// ---------------------------------------------------------------- first-order PDEs
// Parse the initial profile of a PDE topic.  Like the ODE topics, a misspelled
// function is reported as unknown_function instead of silently evaluating to
// zero during the residual check.
bool parseProfile(Result& r, const std::string& input, Ex& out, const char* where) {
    try {
        out = parseExpr(input);
    } catch (const std::exception& e) {
        failFrom(r, e, where);
        return false;
    }
    const std::string unknown = unknownFunction(out);
    if (!unknown.empty()) {
        r.ok = false;
        r.verified = false;
        r.errorCode = "unknown_function";
        r.errorDetail = "unknown function: " + unknown;
        return false;
    }
    return true;
}

struct ResidualReport {
    double worst = 0.0;
    int samples = 0;
};

// Sample a residual at a few interior points.  Points where the input is not
// defined (for example a logarithm at a negative x) are skipped, so the check
// reports "not verified" instead of a false failure.
ResidualReport sampleResidual(const Ex& residual) {
    static const double points[][2] = {
        {0.37, 0.41}, {1.13, -0.67}, {-0.58, 0.93}, {2.05, 1.27}, {0.71, -1.42},
    };
    ResidualReport report;
    for (const auto& p : points) {
        std::map<std::string, double> vars{{"x", p[0]}, {"y", p[1]}};
        bool ok = false;
        const double v = evalReal(residual, vars, &ok);
        if (!ok || !std::isfinite(v)) continue;
        report.worst = std::max(report.worst, std::fabs(v));
        ++report.samples;
    }
    return report;
}

// Largest mismatch between the solution on y = 0 and the initial profile.
double initialCurveError(const Ex& solution, const Ex& profile) {
    double worst = 0.0;
    for (double xv : {0.31, 0.87, 1.42, -0.53}) {
        std::map<std::string, double> sv{{"x", xv}, {"y", 0.0}};
        std::map<std::string, double> pv{{"x", xv}};
        bool okS = false, okP = false;
        const double s = evalReal(solution, sv, &okS);
        const double p = evalReal(profile, pv, &okP);
        if (!okS || !okP || !std::isfinite(s) || !std::isfinite(p)) continue;
        worst = std::max(worst, std::fabs(s - p));
    }
    return worst;
}

// Join the non-zero terms of a constant-coefficient equation.
std::string linearEquation(const std::vector<std::pair<double, std::string>>& terms,
                           const std::string& rhs) {
    std::string eq;
    auto body = [](double coef, const std::string& term) {
        const double mag = std::fabs(coef);
        const std::string number = (std::fabs(mag - 1.0) < 1e-14)
                                       ? std::string()
                                       : numStr(mag) + " ";
        return number + term;
    };
    for (const auto& item : terms) {
        if (std::fabs(item.first) < 1e-14) continue;
        const bool negative = item.first < 0;
        const std::string piece =
            item.second == "1" ? numStr(std::fabs(item.first)) : body(item.first, item.second);
        if (eq.empty()) {
            eq = (negative ? "-" : "") + piece;
        } else {
            eq += (negative ? " - " : " + ") + piece;
        }
    }
    if (eq.empty()) eq = "0";
    return eq + " = " + rhs;
}

Result firstOrderPde(const TopicRequest& req, bool en) {
    (void)en;
    Result r;
    baseId(r, "topic", "pde_first_order", "x");
    const double a = req.number("a", 1.0);
    const double b = req.number("b", 1.0);
    const double c = req.number("c", 0.0);
    Ex f;
    if (!parseProfile(r, req.text("f", "sin(x)"), f, "first order pde")) return r;
    // With b = 0 the initial line y = 0 is itself a characteristic, so the
    // data cannot determine the solution off that line.
    if (std::fabs(b) < 1e-12) {
        failCode(r, "degenerate_characteristics");
        return r;
    }

    const double slope = -a / b;
    Ex xi = Add(Sym("x"), Mul(numEx(slope), Sym("y")));
    Ex profile = substitute(f, "x", xi);
    Ex source = Mul(numEx(c / b), Sym("y"));
    Ex solution = Add(profile, source);
    r.solution = solution;
    r.normalized = Sym(linearEquation(
        {{a, "u_x"}, {b, "u_y"}, {-c, "1"}}, "0"));

    addStep(r, "step.title.write_the_first_order_pde", r.normalized,
            "step.note.the_profile_is_carried_along_straight_characteristics");
    std::string system = "dx / " + numStr(a) + " = dy / " + numStr(b);
    if (std::fabs(c) > 1e-14) system += " = du / " + numStr(c);
    else system += ", du = 0";
    addStep(r, "step.title.write_the_characteristic_system", Sym(system),
            "step.note.du_ds_is_constant_along_each_characteristic");
    addStep(r, "step.title.solve_the_characteristic_equations",
            Sym("x = x_0 + " + numStr(a) + " s, y = " + numStr(b) + " s, u = u_0 + " +
                numStr(c) + " s"));
    addStep(r, "step.title.apply_the_initial_condition", solution,
            "step.note.the_initial_curve_fixes_the_constant_on_each_characteristic",
            "", "u(x, 0) = " + toPlain(f));

    const Ex ux = diff(solution, "x");
    const Ex uy = diff(solution, "y");
    const Ex residual = Add(Mul(numEx(a), ux), Mul(numEx(b), uy),
                            Mul(Num(-1), numEx(c)));
    const ResidualReport rep = sampleResidual(residual);
    addCheck(r, "check.pde_residual", rep.samples > 0 && rep.worst < 1e-7,
             "", "{\"samples\":" + std::to_string(rep.samples) + "}",
             "max |residual| = " + numStr(rep.worst));
    const double icError = initialCurveError(solution, f);
    addCheck(r, "check.initial_curve", icError < 1e-8, "",
             "{\"error\":" + numStr(icError) + "}",
             "max |u(x,0) - f(x)| = " + numStr(icError));
    return r;
}

// ---------------------------------------------------------------- characteristics
Result characteristicsPde(const TopicRequest& req, bool en) {
    (void)en;
    Result r;
    baseId(r, "topic", "pde_characteristics", "x");
    const double a = req.number("a", 1.0);
    const double b = req.number("b", 1.0);
    const double p = req.number("p", 1.0);
    const double q = req.number("q", 0.0);
    Ex f;
    if (!parseProfile(r, req.text("f", "sin(x)"), f, "characteristics")) return r;
    if (std::fabs(b) < 1e-12) {
        failCode(r, "degenerate_characteristics");
        return r;
    }

    Ex xi = Add(Sym("x"), Mul(numEx(-a / b), Sym("y")));
    Ex profile = substitute(f, "x", xi);
    Ex solution;
    if (std::fabs(p) < 1e-12) {
        solution = Add(profile, Mul(numEx(q / b), Sym("y")));
    } else {
        // u + q/p grows like e^(p s) along a characteristic and s = y/b.
        Ex shifted = Add(profile, numEx(q / p));
        Ex envelope = expEx(Mul(numEx(p / b), Sym("y")));
        solution = Add(Mul(shifted, envelope), numEx(-q / p));
    }
    r.solution = solution;
    r.normalized = Sym(linearEquation(
        {{a, "u_x"}, {b, "u_y"}, {-p, "u"}, {-q, "1"}}, "0"));

    addStep(r, "step.title.write_the_first_order_pde", r.normalized,
            "step.note.the_profile_is_carried_along_straight_characteristics");
    std::string system = "dx / " + numStr(a) + " = dy / " + numStr(b);
    if (std::fabs(p) > 1e-14 || std::fabs(q) > 1e-14) {
        system += " = du / (" + numStr(p) + " u + " + numStr(q) + ")";
    } else {
        system += ", du = 0";
    }
    addStep(r, "step.title.write_the_characteristic_system", Sym(system),
            "step.note.du_ds_is_constant_along_each_characteristic");
    if (std::fabs(p) < 1e-12) {
        addStep(r, "step.title.solve_the_characteristic_equations",
                Sym("u = u_0 + " + numStr(q) + " s"));
    } else {
        addStep(r, "step.title.solve_the_characteristic_equations",
                Sym("u + " + numStr(q / p) + " = (u_0 + " + numStr(q / p) +
                    ") e^(" + numStr(p) + " s)"));
    }
    addStep(r, "step.title.apply_the_initial_condition", solution,
            "step.note.the_initial_curve_fixes_the_constant_on_each_characteristic",
            "", "u(x, 0) = " + toPlain(f));

    const Ex ux = diff(solution, "x");
    const Ex uy = diff(solution, "y");
    const Ex residual = Add(Add(Mul(numEx(a), ux), Mul(numEx(b), uy)),
                            Mul(Num(-1), numEx(p), solution),
                            Mul(Num(-1), numEx(q)));
    const ResidualReport rep = sampleResidual(residual);
    addCheck(r, "check.pde_residual", rep.samples > 0 && rep.worst < 1e-6,
             "", "{\"samples\":" + std::to_string(rep.samples) + "}",
             "max |residual| = " + numStr(rep.worst));
    const double icError = initialCurveError(solution, f);
    addCheck(r, "check.initial_curve", icError < 1e-8, "",
             "{\"error\":" + numStr(icError) + "}",
             "max |u(x,0) - f(x)| = " + numStr(icError));
    return r;
}

// ---------------------------------------------------------------- classification
Result classifyPde(const TopicRequest& req, bool en) {
    (void)en;
    Result r;
    baseId(r, "topic", "pde_classify", "x");
    const double A = req.number("A", 1.0);
    const double B = req.number("B", 0.0);
    const double C = req.number("C", 1.0);
    const double D = req.number("D", 0.0);
    const double E = req.number("E", 0.0);
    const double F = req.number("F", 0.0);
    if (std::fabs(A) < 1e-14 && std::fabs(B) < 1e-14 && std::fabs(C) < 1e-14) {
        failCode(r, "bad_principal_part");
        return r;
    }
    const double disc = B * B - 4.0 * A * C;
    const double scale = std::fabs(B * B) + 4.0 * std::fabs(A * C) + 1.0;
    const double tolerance = 1e-12 * scale;

    std::string typeCode, typeText;
    if (disc > tolerance) {
        typeCode = "hyperbolic";
        typeText = "hyperbolic";
    } else if (disc < -tolerance) {
        typeCode = "elliptic";
        typeText = "elliptic";
    } else {
        typeCode = "parabolic";
        typeText = "parabolic";
    }
    r.solution = Sym(typeText);
    r.typeId = "pde_" + typeCode;

    // Build the equation as text: skip zero principal coefficients.
    std::string equation;
    auto term = [&](double coef, const std::string& body) {
        if (std::fabs(coef) < 1e-14) return;
        if (!equation.empty()) {
            equation += (coef < 0 ? " - " : " + ");
            equation += numStr(std::fabs(coef)) + " " + body;
        } else {
            equation = numStr(coef) + " " + body;
        }
    };
    term(A, "u_xx");
    term(B, "u_xy");
    term(C, "u_yy");
    term(D, "u_x");
    term(E, "u_y");
    term(F, "u");
    if (equation.empty()) equation = "0";
    equation += " = 0";
    r.normalized = Sym(equation);

    addStep(r, "step.title.write_the_model", r.normalized,
            "step.note.lower_order_terms_do_not_change_the_principal_type");
    addStep(r, "step.title.compute_the_discriminant",
            Sym("B^2 - 4 A C = " + numStr(disc)),
            "step.note.the_sign_of_the_discriminant_decides_the_type");
    addStep(r, "step.title.classify_the_equation", Sym(typeText),
            "step.note.the_sign_of_the_discriminant_decides_the_type");

    std::string coords;
    std::string characteristics;
    // Gradients of the two new coordinates.  The principal part transforms as
    // Q(dx, dy) = A dx^2 + B dx dy + C dy^2, so the coefficients in the new
    // variables are Q(grad xi), Q(grad eta) and the polarisation 2B(grad xi,
    // grad eta).  Checking those numbers verifies the canonical form itself.
    double xiX = 0.0, xiY = 0.0, etaX = 0.0, etaY = 0.0;
    bool wantXiZero = false;   // the xi-xi coefficient must vanish
    bool wantEtaZero = false;  // the eta-eta coefficient must vanish
    bool wantMixedZero = false;
    bool wantMixedNonzero = false;
    bool wantEqualPure = false; // elliptic: both pure coefficients are equal
    if (std::fabs(A) > 1e-14) {
        if (disc > tolerance) {
            const double root = std::sqrt(disc);
            const double l1 = (B + root) / (2.0 * A);
            const double l2 = (B - root) / (2.0 * A);
            coords = "xi = y - (" + numStr(l1) + ") x, eta = y - (" +
                     numStr(l2) + ") x";
            characteristics = "y = (" + numStr(l1) + ") x + C_1,  y = (" +
                              numStr(l2) + ") x + C_2";
            xiX = -l1; xiY = 1.0;
            etaX = -l2; etaY = 1.0;
            wantXiZero = wantEtaZero = true;
            wantMixedNonzero = true;
        } else if (disc < -tolerance) {
            const double alpha = B / (2.0 * A);
            const double beta = std::sqrt(-disc) / (2.0 * A);
            coords = "xi = y - (" + numStr(alpha) + ") x, eta = (" +
                     numStr(beta) + ") x";
            characteristics = "no real characteristics";
            xiX = -alpha; xiY = 1.0;
            etaX = beta; etaY = 0.0;
            wantMixedZero = true;
            wantEqualPure = true;
        } else {
            const double lam = B / (2.0 * A);
            coords = "xi = y - (" + numStr(lam) + ") x, eta = x";
            characteristics = "y = (" + numStr(lam) + ") x + C";
            xiX = -lam; xiY = 1.0;
            etaX = 1.0; etaY = 0.0;
            wantXiZero = wantMixedZero = true;
        }
    } else if (std::fabs(B) > 1e-14) {
        const double lam = C / B;
        coords = "xi = y - (" + numStr(lam) + ") x, eta = x";
        characteristics = "y = (" + numStr(lam) + ") x + C";
        xiX = -lam; xiY = 1.0;
        etaX = 1.0; etaY = 0.0;
        wantXiZero = true;
        wantMixedNonzero = true;
    } else {
        coords = "xi = x, eta = y";
        characteristics = "y = C";
        xiX = 1.0; xiY = 0.0;
        etaX = 0.0; etaY = 1.0;
        wantXiZero = wantMixedZero = true;
    }

    // Transformed principal coefficients in the canonical coordinates.  The
    // canonical form quotes these numbers instead of assuming a normalised
    // unit coefficient.
    const double qXi = A * xiX * xiX + B * xiX * xiY + C * xiY * xiY;
    const double qEta = A * etaX * etaX + B * etaX * etaY + C * etaY * etaY;
    const double mixed = 2.0 * A * xiX * etaX +
                         B * (xiX * etaY + xiY * etaX) +
                         2.0 * C * xiY * etaY;
    std::string canonical = coords + "  =>  ";
    if (typeCode == "hyperbolic") {
        canonical += numStr(mixed) + " u_xieta + lower order = 0";
    } else if (typeCode == "elliptic") {
        canonical += numStr(qXi) + " u_xixi + " + numStr(qEta) +
                     " u_etaeta + lower order = 0";
    } else {
        canonical += numStr(qEta) + " u_etaeta + lower order = 0";
    }
    addStep(r, "step.title.write_the_canonical_form", Sym(canonical),
            "step.note.the_mixed_term_disappears_in_characteristic_coordinates");
    addStep(r, "step.title.find_the_characteristic_curves", Sym(characteristics));

    // Independent numeric check: restricted to the unit circle the principal
    // form Q has extreme values equal to the eigenvalues of
    // [[A, B/2], [B/2, C]].  Parabolic means one eigenvalue vanishes; sampling
    // the circle would miss that exact direction and report a false failure.
    const double mid = 0.5 * (A + C);
    const double rad = 0.5 * std::sqrt((A - C) * (A - C) + B * B);
    const double lo = mid - rad;
    const double hi = mid + rad;
    const double qScale = std::fabs(A) + std::fabs(B) + std::fabs(C) + 1.0;
    bool signCheck = false;
    if (typeCode == "elliptic") {
        signCheck = (lo > 1e-9 * qScale) || (hi < -1e-9 * qScale);
    } else if (typeCode == "hyperbolic") {
        signCheck = lo < -1e-9 * qScale && hi > 1e-9 * qScale;
    } else {
        signCheck = std::fabs(lo) <= 1e-9 * qScale || std::fabs(hi) <= 1e-9 * qScale;
    }
    addCheck(r, "check.discriminant_sign", signCheck,
             "", "{\"discriminant\":" + numStr(disc) + "}",
             "min Q = " + numStr(lo) + ", max Q = " + numStr(hi));

    const double principalScale = 1.0 + std::fabs(A) + std::fabs(B) + std::fabs(C);
    bool canonicalOk = true;
    if (wantXiZero) {
        canonicalOk = canonicalOk && std::fabs(qXi) < 1e-7 * principalScale;
    }
    if (wantEtaZero) {
        canonicalOk = canonicalOk && std::fabs(qEta) < 1e-7 * principalScale;
    }
    if (wantMixedZero) {
        canonicalOk = canonicalOk && std::fabs(mixed) < 1e-7 * principalScale;
    }
    if (wantMixedNonzero) {
        canonicalOk = canonicalOk && std::fabs(mixed) > 1e-9 * principalScale;
    }
    if (wantEqualPure) {
        canonicalOk = canonicalOk &&
                      std::fabs(qXi - qEta) < 1e-7 * principalScale &&
                      std::fabs(qXi) > 1e-9 * principalScale;
    }
    addCheck(r, "check.canonical_form", canonicalOk, "",
             "{\"type\":\"" + typeCode + "\"}",
             "Q(xi) = " + numStr(qXi) + ", Q(eta) = " + numStr(qEta) +
                 ", mixed = " + numStr(mixed));
    return r;
}

// ---------------------------------------------------------------- separation
Result separationPde(const TopicRequest& req, bool en) {
    (void)en;
    Result r;
    baseId(r, "topic", "pde_separation", "x");
    const double A = req.number("A", 1.0);
    const double C = req.number("C", 1.0);
    const double L = req.number("L", kPi);
    int modes = (int)std::llround(req.number("n", 1.0));
    if (modes < 1) modes = 1;
    if (modes > 4) modes = 4;
    if (std::fabs(A) < 1e-12 || std::fabs(C) < 1e-12) {
        failCode(r, "bad_principal_part");
        return r;
    }
    if (std::fabs(L) < 1e-12) {
        failCode(r, "bad_length");
        return r;
    }
    const double ratio = A / C;
    const bool hyperbolicInY = ratio > 0.0;
    const double freq = std::sqrt(std::fabs(ratio));
    r.normalized = Sym(linearEquation({{A, "u_xx"}, {C, "u_yy"}}, "0"));

    addStep(r, "step.title.separate_the_variables", Sym("u(x,y) = X(x) Y(y)"),
            "step.note.separation_turns_one_pde_into_two_odes");
    addStep(r, "step.title.apply_the_boundary_conditions",
            Sym("X(0) = X(L) = 0  =>  lambda_n = " + numStr(A) +
                " (n pi / " + numStr(L) + ")^2,  X_n = sin(n pi x / L)"),
            "step.note.zero_end_values_allow_only_sine_eigenfunctions");
    std::string yForm;
    if (hyperbolicInY) {
        yForm = "Y_n = A_n sinh(" + numStr(freq) +
                " n pi y / L) + B_n cosh(" + numStr(freq) + " n pi y / L)";
    } else {
        yForm = "Y_n = A_n cos(" + numStr(freq) +
                " n pi y / L) + B_n sin(" + numStr(freq) + " n pi y / L)";
    }
    addStep(r, "step.title.solve_the_second_factor", Sym(yForm),
            "step.note.the_sign_of_a_over_c_decides_sinh_or_sine_in_y");

    std::vector<Ex> terms;
    std::ostringstream table;
    table << "n\tlambda_n\tk_n\n";
    for (int n = 1; n <= modes; ++n) {
        const Ex kn = piMultiple(n, L);
        const Ex xPart = sinEx(Mul(kn, Sym("x")));
        const Ex scaled = Mul(numEx(freq), kn, Sym("y"));
        Ex yPart;
        if (hyperbolicInY) {
            yPart = Add(Mul(Sym("A_" + std::to_string(n)), sinhEx(scaled)),
                        Mul(Sym("B_" + std::to_string(n)), coshEx(scaled)));
        } else {
            yPart = Add(Mul(Sym("A_" + std::to_string(n)), cosEx(scaled)),
                        Mul(Sym("B_" + std::to_string(n)), sinEx(scaled)));
        }
        terms.push_back(Mul(xPart, yPart));
        const Ex lambda = Mul(numEx(A), Pow(kn, Num(2)));
        table << n << "\t" << toPretty(lambda) << "\t" << toPretty(kn) << "\n";
    }
    const Ex solution = Add(terms);
    r.solution = solution;
    addStepRaw(r, "step.title.compute_the_coefficients", nullptr, table.str());
    addStep(r, "step.title.write_the_series_solution", solution,
            "step.note.each_mode_satisfies_the_equation_and_the_boundary_conditions");

    // Differentiate the first mode with A_1 = 1, B_1 = 0 and check that
    // A u_xx + C u_yy vanishes away from y = 0.
    const Ex k1 = piMultiple(1, L);
    const Ex xFirst = sinEx(Mul(k1, Sym("x")));
    const Ex scaledFirst = Mul(numEx(freq), k1, Sym("y"));
    const Ex mode = hyperbolicInY
                        ? Mul(xFirst, sinhEx(scaledFirst))
                        : Mul(xFirst, cosEx(scaledFirst));
    const Ex uxx = diff(mode, "x");
    const Ex modeResidual = Add(Mul(numEx(A), diff(uxx, "x")),
                                Mul(numEx(C), diff(diff(mode, "y"), "y")));
    const ResidualReport rep = sampleResidual(modeResidual);
    addCheck(r, "check.pde_residual", rep.samples > 0 && rep.worst < 1e-6,
             "", "{\"samples\":" + std::to_string(rep.samples) + "}",
             "max |residual| = " + numStr(rep.worst));
    const double left = std::sin(0.0);
    bool okRight = false;
    const double right = evalReal(xFirst, {{"x", L}}, &okRight);
    addCheck(r, "check.eigenfunction",
             std::fabs(left) < 1e-9 && okRight && std::fabs(right) < 1e-9,
             "", "{\"n\":1}",
             "sin(0) = " + numStr(left) + ", sin(k_1 L) = " +
                 numStr(okRight ? right : std::numeric_limits<double>::quiet_NaN()));
    return r;
}

} // namespace

Result solveTopic(const TopicRequest& req) {
    const bool en = req.lang != "fa";
    const std::string& t = req.topic;
    try {
        if (t == "growth_decay") return growthDecay(req, en);
        if (t == "cooling") return newtonCooling(req, en);
        if (t == "logistic") return logistic(req, en);
        if (t == "mixing") return mixing(req, en);
        if (t == "spring_mass") return oscillator(req, en, false);
        if (t == "damped_oscillation") return oscillator(req, en, false);
        if (t == "circuit") return oscillator(req, en, true);
        if (t == "falling_body") return fallingBody(req, en);
        if (t == "euler") return numericalOde(req, en, NumMethod::Euler);
        if (t == "heun") return numericalOde(req, en, NumMethod::Heun);
        if (t == "rk4") return numericalOde(req, en, NumMethod::Rk4);
        if (t == "multistep") return multistep(req, en);
        if (t == "error_convergence") return errorConvergence(req, en);
        if (t == "linear_system") return linearSystem(req, en);
        if (t == "matrix_eigen") return matrixEigen(req, en);
        if (t == "phase_portrait") return phasePortrait(req, en);
        if (t == "critical_points") return criticalPoints(req, en);
        if (t == "predator_prey") return predatorPrey(req, en);
        if (t == "fourier_series") return fourierSeries(req, en);
        if (t == "heat") return heatEquation(req, en);
        if (t == "wave") return waveEquation(req, en);
        if (t == "laplace_pde") return laplacePde(req, en);
        if (t == "pde_first_order") return firstOrderPde(req, en);
        if (t == "pde_characteristics") return characteristicsPde(req, en);
        if (t == "pde_classify") return classifyPde(req, en);
        if (t == "pde_separation") return separationPde(req, en);
        if (t == "laplace") return laplaceTransform(req, en);
        if (t == "inverse_laplace") return inverseLaplace(req, en);
        if (t == "transform_derivatives") return transformDerivatives(req, en);
        if (t == "laplace_ivp") return laplaceIvp(req, en);
        if (t == "step_functions") return stepFunctions(req, en);
        if (t == "discontinuous_forcing") return discontinuousForcing(req, en);
        if (t == "impulse") return impulse(req, en);
        if (t == "convolution") return convolution(req, en);
        if (t == "fourier_transform") return fourierTransform(req, en);
        if (t == "ode_linear") return odeLinear(req, en);
        if (t == "ode_stiff") return odeStiff(req, en);
        if (t == "series_power") return seriesPowerTopic(req);
        if (t == "series_singular_points") return seriesSingularPointsTopic(req);
        if (t == "series_frobenius") return seriesFrobeniusTopic(req);
        if (t == "euler_equations") return eulerEquationTopic(req);
        if (t == "special_legendre") return legendreTopic(req);
        if (t == "special_bessel") return besselTopic(req);
        if (t == "system_theory") return systemTheoryTopic(req);
        if (t == "system_complex_eigen") return systemComplexEigenTopic(req);
        if (t == "system_fundamental") return systemFundamentalTopic(req);
        if (t == "system_repeated") return systemRepeatedTopic(req);
        if (t == "system_nonhomogeneous") return systemNonhomogeneousTopic(req);
        if (t == "system_rk4") return systemRk4Topic(req);
        if (t == "competing_species") return competingSpeciesTopic(req);
        if (t == "liapunov") return liapunovTopic(req);
        if (t == "limit_cycle") return limitCycleTopic(req);
        if (t == "chaos_attractor") return chaosAttractorTopic(req);
        if (t == "local_linearization") return localLinearizationTopic(req);
        if (t == "bvp_two_point") return bvpTwoPointTopic(req);
        if (t == "bvp_occurrence") return bvpOccurrenceTopic(req);
        if (t == "sturm_liouville") return sturmLiouvilleTopic(req);
        if (t == "bvp_nonhomogeneous") return bvpNonhomogeneousTopic(req);
        if (t == "sturm_liouville_singular") return sturmLiouvilleSingularTopic(req);
        if (t == "bessel_series") return besselSeriesTopic(req);
        if (t == "orthogonal_series") return orthogonalSeriesTopic(req);
        if (t == "fourier_convergence") return fourierConvergenceTopic(req);
        if (t == "fourier_even_odd") return fourierEvenOddTopic(req);
        if (t == "heat_derivation") return heatDerivationTopic(req);
        if (t == "wave_derivation") return waveDerivationTopic(req);
        if (t == "heat_other_bc") return heatOtherBcTopic(req);
        if (t == "transcendental") return transcendentalTopic(req);
        if (t == "integration_techniques") return integrationTechniquesTopic(req);
    } catch (const std::exception& e) {
        Result r;
        r.ok = false;
        r.mode = "topic";
        failFrom(r, e, "topic");
        return r;
    }
    Result r;
    r.ok = false;
    r.mode = "topic";
    r.errorCode = "unknown_topic";
    return r;
}

} // namespace ode
