// Infinit Lab — numeric graph sampling, domain/range scans and direction fields.
// SPDX-License-Identifier: Apache-2.0
#include "graph.hpp"

#include "expr.hpp"
#include "numeric.hpp"
#include "poly.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace ode {
namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

Ex withConstants(const Ex& e, const std::map<std::string, double>& constants) {
    Ex out = e;
    for (const auto& kv : constants) {
        if (!std::isfinite(kv.second)) continue;
        out = substitute(out, kv.first, Num(Rational::fromDouble(kv.second)));
    }
    return out;
}

double at(const Ex& f, const std::string& x, double xv, bool* okp = nullptr) {
    std::map<std::string, double> vars;
    vars[x] = xv;
    return evalReal(f, vars, okp);
}

double at2(const Ex& f, const std::string& x, double xv, const std::string& y, double yv,
           bool* okp = nullptr) {
    std::map<std::string, double> vars;
    vars[x] = xv;
    vars[y] = yv;
    return evalReal(f, vars, okp);
}

bool finiteValue(double v) { return std::isfinite(v); }

// ------------------------------------------------------------- domain helpers
struct Traits {
    bool rational = false;
    bool root = false;
    bool logarithm = false;
    bool trigonometric = false;
};

bool containsTrigFunc(const Ex& e) {
    if (!e) return false;
    if (e->kind == Kind::Func &&
        (e->name == "sin" || e->name == "cos" || e->name == "tan" ||
         e->name == "cot" || e->name == "sec" || e->name == "csc"))
        return true;
    for (const auto& a : e->args)
        if (containsTrigFunc(a)) return true;
    return false;
}

void collectTraits(const Ex& e, Traits& t) {
    if (!e) return;
    if (e->kind == Kind::Pow) {
        if (e->args.size() == 2 && e->args[1]->isNum() && e->args[1]->num.n < 0) {
            t.rational = true;
            // 1/cos and 1/sin are sec and csc: trigonometric singularities.
            if (containsTrigFunc(e->args[0])) t.trigonometric = true;
        }
        if (e->args.size() == 2 && e->args[1]->isNum() && !e->args[1]->num.isInt()) {
            t.root = true;
        }
    }
    if (e->kind == Kind::Func) {
        if (e->name == "sqrt") t.root = true;
        if (e->name == "ln" || e->name == "log") t.logarithm = true;
        if (e->name == "tan" || e->name == "cot" || e->name == "sec" || e->name == "csc") {
            t.trigonometric = true;
        }
    }
    for (const auto& a : e->args) collectTraits(a, t);
}

std::string classify(const Traits& t) {
    // A reciprocal of a trig factor (sec, 1/cos, 1/tan, …) is trigonometric,
    // not rational, even though it is written as a negative power.
    if (t.trigonometric && !t.root && !t.logarithm) return "trigonometric";
    if (t.rational && (t.root || t.logarithm)) return "mixed";
    if (t.rational) return "rational";
    if (t.root) return "root";
    if (t.logarithm) return "logarithm";
    if (t.trigonometric) return "trigonometric";
    return "polynomial";
}

// Bisects between a valid and an invalid x. Returns the boundary estimate.
double refineBoundary(const Ex& f, const std::string& x, double good, double bad) {
    for (int i = 0; i < 60; ++i) {
        const double mid = 0.5 * (good + bad);
        bool ok = false;
        at(f, x, mid, &ok);
        if (ok) {
            good = mid;
        } else {
            bad = mid;
        }
        if (std::fabs(bad - good) <= 1e-12 * std::max(1.0, std::fabs(mid))) break;
    }
    return 0.5 * (good + bad);
}

bool definedAt(const Ex& f, const std::string& x, double xv) {
    bool ok = false;
    at(f, x, xv, &ok);
    return ok;
}

// Numeric limit at a finite singular point from one side.
double sideLimit(const Ex& f, const std::string& x, double point, double dir, bool* okp) {
    const double scale = std::max(1.0, std::fabs(point));
    double last = kNaN;
    for (int i = 0; i < 6; ++i) {
        const double d = scale * std::pow(10.0, -3.0 - 0.5 * i);
        bool ok = false;
        const double v = at(f, x, point + dir * d, &ok);
        if (!ok) {
            *okp = false;
            return kNaN;
        }
        last = v;
    }
    *okp = true;
    return last;
}

bool divergesAt(const Ex& f, const std::string& x, double point, double dir, int* sign) {
    const double scale = std::max(1.0, std::fabs(point));
    double v[5];
    for (int i = 0; i < 5; ++i) {
        const double d = scale * std::pow(10.0, -3.0 - 1.0 * i);
        bool ok = false;
        v[i] = at(f, x, point + dir * d, &ok);
        if (!ok || !std::isfinite(v[i])) return false;
    }
    const double d1 = std::fabs(v[1] - v[0]);
    const double d4 = std::fabs(v[4] - v[3]);
    if (std::fabs(v[4]) < 1e3 && d4 < d1 * 0.5) return false;
    bool grows = true;
    for (int i = 1; i < 5; ++i) {
        if (!(std::fabs(v[i]) > std::fabs(v[i - 1]) * 1.05)) grows = false;
    }
    if (grows && std::fabs(v[4]) > std::fabs(v[0]) * 1.2) {
        if (sign) *sign = v[4] < 0 ? -1 : 1;
        return true;
    }
    for (int i = 1; i < 5; ++i) {
        if (std::fabs(v[i]) > 1e3 && std::fabs(v[i]) > std::fabs(v[i - 1]) * 3.0) {
            if (sign) *sign = v[i] < 0 ? -1 : 1;
            return true;
        }
    }
    return false;
}

// Tends to infinity at an infinite edge? `dir` is +1 for +∞.
bool divergesAtInfinity(const Ex& f, const std::string& x, double edge, double dir, int* sign) {
    const double base = std::max(1.0, std::fabs(edge));
    double v[5];
    for (int i = 0; i < 5; ++i) {
        bool ok = false;
        v[i] = at(f, x, dir * base * std::pow(3.0, i + 1), &ok);
        if (!ok || !std::isfinite(v[i])) {
            // Overflow or an undefined far value. On an unbounded domain a
            // finite predecessor that could not be extended means growth.
            if (i > 0 && std::isfinite(v[i - 1])) {
                if (sign) *sign = v[i - 1] < 0 ? -1 : 1;
                return true;
            }
            return false;
        }
    }
    // Settling to a finite value is not divergence.
    const double d1 = std::fabs(v[1] - v[0]);
    const double d4 = std::fabs(v[4] - v[3]);
    if (d4 < d1 * 0.5) return false;
    bool grows = true;
    bool shrinks = true;
    for (int i = 1; i < 5; ++i) {
        if (!(std::fabs(v[i]) > std::fabs(v[i - 1]))) grows = false;
        if (!(std::fabs(v[i]) < std::fabs(v[i - 1]))) shrinks = false;
    }
    if (grows) {
        if (sign) *sign = v[4] < 0 ? -1 : 1;
        return true;
    }
    if (shrinks) return false;
    // Explosive growth that is not strictly monotone (e.g. oscillating with a
    // growing envelope) still diverges.
    for (int i = 1; i < 5; ++i) {
        if (std::fabs(v[i]) > 1e3 && std::fabs(v[i]) > std::fabs(v[i - 1]) * 3.0) {
            if (sign) *sign = v[i] < 0 ? -1 : 1;
            return true;
        }
    }
    return false;
}

double tailLimit(const Ex& f, const std::string& x, double edge, double dir, bool* finiteOut) {
    const double base = std::max(1.0, std::fabs(edge));
    bool ok = false;
    const double v1 = at(f, x, dir * base * 32.0, &ok);
    if (!ok || !std::isfinite(v1)) {
        *finiteOut = false;
        return kNaN;
    }
    const double v2 = at(f, x, dir * base * 1024.0, &ok);
    if (!ok || !std::isfinite(v2)) {
        *finiteOut = false;
        return kNaN;
    }
    if (std::fabs(v2) > 1e7) {
        *finiteOut = false;
        return kNaN;
    }
    // A tail that decays geometrically toward zero converges even though the
    // absolute gap test below can be too coarse for small values.
    if (std::fabs(v2) < 0.1 && std::fabs(v2) < 0.5 * std::fabs(v1)) {
        *finiteOut = true;
        return 0.0;
    }
    if (std::fabs(v2 - v1) <=
        1e-2 * std::max(1e-12, std::max(std::fabs(v1), std::fabs(v2)))) {
        *finiteOut = true;
        return v2;
    }
    *finiteOut = false;
    return kNaN;
}

} // namespace

SampleOutput sampleFunction(const std::string& input, const std::string& x, double xmin,
                            double xmax, int count,
                            const std::map<std::string, double>& constants) {
    SampleOutput out;
    try {
        Ex f = withConstants(parseExpr(input), constants);
        if (!(xmin < xmax)) {
            out.error = "plot.empty_window";
            return out;
        }
        count = std::max(2, std::min(count, 20000));
        out.xs.reserve(count);
        out.ys.reserve(count);
        for (int i = 0; i < count; ++i) {
            const double t = count == 1 ? 0.0 : (double)i / (double)(count - 1);
            const double xv = xmin + (xmax - xmin) * t;
            bool ok = false;
            double yv = at(f, x, xv, &ok);
            out.xs.push_back(xv);
            out.ys.push_back(ok && finiteValue(yv) ? yv : kNaN);
        }
        out.ok = true;
    } catch (const std::exception& e) {
        out.error = "plot.failed";
    }
    return out;
}

DomainOutput analyseDomain(const std::string& input, const std::string& x, double xmin,
                           double xmax, int samples) {
    DomainOutput out;
    try {
        Ex f = parseExpr(input);
        if (!(xmin < xmax)) {
            out.error = "plot.empty_window";
            return out;
        }
        samples = std::max(401, std::min(samples, 12001));
        if (samples % 2 == 0) ++samples;

        Traits traits;
        collectTraits(f, traits);
        out.kind = classify(traits);

        const double step = (xmax - xmin) / (double)(samples - 1);
        const double snapScale = std::max(1.0, std::max(std::fabs(xmin), std::fabs(xmax)));
        auto snap = [&](double v) {
            if (!std::isfinite(v)) return v;
            if (std::fabs(v) < 1e-9 * snapScale) return 0.0;
            const double mag = std::max(1.0, std::fabs(v));
            const double quantum = std::pow(10.0, std::floor(std::log10(mag)) - 6.0);
            return std::round(v / quantum) * quantum;
        };
        std::vector<double> xs(samples);
        std::vector<double> ys(samples, kNaN);
        std::vector<bool> valid(samples, false);
        for (int i = 0; i < samples; ++i) {
            xs[i] = xmin + step * i;
            bool ok = false;
            const double yv = at(f, x, xs[i], &ok);
            if (ok && finiteValue(yv)) {
                ys[i] = yv;
                valid[i] = true;
            }
        }

        // A pole that falls on a grid point can produce one enormous but
        // finite value (for example 1/(x−2)² at x = 2.000000000000002).
        // Treat such isolated spikes as undefined samples.
        for (int i = 1; i + 1 < samples; ++i) {
            if (!valid[i]) continue;
            const double before = valid[i - 1] ? std::fabs(ys[i - 1]) : 0.0;
            const double after = valid[i + 1] ? std::fabs(ys[i + 1]) : 0.0;
            const double neighbour = std::max(1.0, std::max(before, after));
            if (std::fabs(ys[i]) > 1e4 && std::fabs(ys[i]) > 100.0 * neighbour) {
                valid[i] = false;
            }
        }

        // Contiguous runs of defined samples.
        std::vector<std::pair<int, int>> runs;
        for (int i = 0; i < samples;) {
            if (!valid[i]) {
                ++i;
                continue;
            }
            int j = i;
            while (j + 1 < samples && valid[j + 1]) ++j;
            runs.emplace_back(i, j);
            i = j + 1;
        }

        for (size_t r = 0; r < runs.size(); ++r) {
            Interval in;
            const int s = runs[r].first;
            const int e = runs[r].second;
            if (s == 0) {
                in.loInfinite = true;
                in.loOpen = true;
            } else {
                in.lo = snap(refineBoundary(f, x, xs[s], xs[s - 1]));
                in.loOpen = !definedAt(f, x, in.lo);
            }
            if (e == samples - 1) {
                in.hiInfinite = true;
                in.hiOpen = true;
            } else {
                in.hi = snap(refineBoundary(f, x, xs[e], xs[e + 1]));
                in.hiOpen = !definedAt(f, x, in.hi);
            }
            if (in.loInfinite && in.hiInfinite) {
                in.loOpen = true;
                in.hiOpen = true;
            }
            out.domain.push_back(in);
        }

        // Classify the gaps between runs as removable holes or asymptotes.
        for (size_t r = 0; r + 1 < runs.size();) {
            const int badStart = runs[r].second + 1;
            const int badEnd = runs[r + 1].first - 1;
            const int width = badEnd - badStart + 1;
            const double centre = 0.5 * (xs[runs[r].second] + xs[runs[r + 1].first]);
            bool lok = false;
            bool rok = false;
            const double lv = sideLimit(f, x, centre, -1, &lok);
            const double rv = sideLimit(f, x, centre, +1, &rok);
            int ds = 1;
            const bool diverges = divergesAt(f, x, centre, -1, &ds) ||
                                  divergesAt(f, x, centre, +1, &ds);
            bool hole = !diverges && width <= 4 && lok && rok &&
                        std::fabs(lv - rv) <= 1e-3 * (1.0 + std::fabs(lv) + std::fabs(rv));

            if (hole) {
                out.holes.push_back(centre);
                if (lok && std::isfinite(lv)) out.rangeHoles.push_back(lv);
                // The two runs belong to one interval with a puncture.
                out.domain[r].hi = out.domain[r + 1].hi;
                out.domain[r].hiInfinite = out.domain[r + 1].hiInfinite;
                out.domain[r].hiOpen = out.domain[r + 1].hiOpen;
                out.domain.erase(out.domain.begin() + (long)r + 1);
                runs.erase(runs.begin() + (long)r + 1);
            } else {
                if (width <= 4) {
                    const double a = snap(centre);
                    out.asymptotes.push_back(a);
                    out.domain[r].hi = a;
                    out.domain[r].hiInfinite = false;
                    out.domain[r].hiOpen = true;
                    out.domain[r + 1].lo = a;
                    out.domain[r + 1].loInfinite = false;
                    out.domain[r + 1].loOpen = true;
                }
                // A wide gap is a missing interval of the domain (for example
                // sqrt(x² − 1)); its refined endpoints are already correct.
                ++r;
            }
        }

        // Periodic poles (tan and friends) can fall between grid points and
        // never produce an invalid sample. A large sign flip identifies them.
        for (int i = 1; i < samples; ++i) {
            if (!valid[i - 1] || !valid[i]) continue;
            const double a = ys[i - 1];
            const double b = ys[i];
            if (a * b >= 0) continue;
            if (std::fabs(a) < 50.0 || std::fabs(b) < 50.0) continue;
            if (std::fabs(a - b) < 100.0) continue;
            double xa = snap(0.5 * (xs[i - 1] + xs[i]));
            // The pole lies between two samples.  Refine it as a zero of 1/f
            // (cot for tan, cos for sec, …) so the reported location is the
            // actual singular point rather than the grid midpoint.
            {
                double lo = xs[i - 1], hi = xs[i];
                bool okLo = false, okHi = false;
                const double fLo = at(f, x, lo, &okLo);
                const double fHi = at(f, x, hi, &okHi);
                if (okLo && okHi && std::fabs(fLo) > 1e-12 && std::fabs(fHi) > 1e-12) {
                    double gLo = 1.0 / fLo, gHi = 1.0 / fHi;
                    if (gLo * gHi < 0) {
                        for (int it = 0; it < 60; ++it) {
                            const double mid = 0.5 * (lo + hi);
                            bool okMid = false;
                            const double fMid = at(f, x, mid, &okMid);
                            if (!okMid || std::fabs(fMid) < 1e-300) break;
                            const double gMid = 1.0 / fMid;
                            if (gLo * gMid <= 0) { hi = mid; gHi = gMid; }
                            else { lo = mid; gLo = gMid; }
                        }
                        xa = snap(0.5 * (lo + hi));
                    }
                }
            }
            bool duplicate = false;
            for (double old : out.asymptotes) {
                if (std::fabs(old - xa) <= step * 2.0) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) out.asymptotes.push_back(xa);
        }

        // ------------------------------------------------ range estimation
        double minY = std::numeric_limits<double>::infinity();
        double maxY = -std::numeric_limits<double>::infinity();
        bool minAttained = true;
        bool maxAttained = true;
        for (double v : ys) {
            if (!std::isfinite(v)) continue;
            minY = std::min(minY, v);
            maxY = std::max(maxY, v);
        }

        // A removable hole can carry the extreme value of the range even
        // though the scan never sees it (sin(x)/x peaks at 1 as x → 0 but the
        // sample at 0 is undefined). The hole limit is a supremum or infimum,
        // never an attained value, so it only extends the open end.
        for (double h : out.rangeHoles) {
            if (!std::isfinite(h)) continue;
            if (h < minY) {
                minY = h;
                minAttained = false;
            } else if (h > maxY) {
                maxY = h;
                maxAttained = false;
            }
        }

        bool unboundedBelow = false;
        bool unboundedAbove = false;

        if (!std::isfinite(minY)) minY = -1;
        if (!std::isfinite(maxY)) maxY = 1;
        if (maxY - minY < 1e-12) {
            minY -= 0.5;
            maxY += 0.5;
        }

        // Does the range straddle zero without attaining it?
        const double scale = std::max(1.0, std::max(std::fabs(minY), std::fabs(maxY)));
        bool hasNegative = false;
        bool hasPositive = false;
        bool hasZero = false;
        double negSup = -std::numeric_limits<double>::infinity();
        double posSup = std::numeric_limits<double>::infinity();
        bool negSupAttained = false;
        bool posSupAttained = false;
        for (double v : ys) {
            if (!std::isfinite(v)) continue;
            if (v < -1e-9 * scale) {
                hasNegative = true;
                if (v > negSup) {
                    negSup = v;
                    negSupAttained = true;
                }
            }
            if (v > 1e-9 * scale) {
                hasPositive = true;
                if (v < posSup) {
                    posSup = v;
                    posSupAttained = true;
                }
            }
            if (std::fabs(v) <= 1e-9 * scale) hasZero = true;
        }
        // A sign change between two neighbouring samples of one continuous
        // run proves the function attains zero there (cot(x) passes through
        // zero at every multiple of π even when no grid point lands on it),
        // so the range is connected and must not be split at zero.
        bool crossesZero = false;
        for (const auto& run : runs) {
            for (int i = run.first; i < run.second; ++i) {
                if (valid[i] && valid[i + 1] && ys[i] * ys[i + 1] < 0) {
                    crossesZero = true;
                    break;
                }
            }
            if (crossesZero) break;
        }
        out.splitAtZero = hasNegative && hasPositive && !hasZero && !crossesZero &&
                          out.domain.size() > 1;

        // A finite tail can tighten the open end of the range (exp(x) → 0 as
        // x → −∞, for example). A negative tail approaching zero is a
        // supremum of the negative branch.
        double firstFinite = kNaN;
        double lastFinite = kNaN;
        for (double v : ys) {
            if (!std::isfinite(v)) continue;
            if (!std::isfinite(firstFinite)) firstFinite = v;
            lastFinite = v;
        }
        auto tightenTail = [&](double lim, double edgeValue) {
            if (!std::isfinite(lim)) return;
            if (lim < -1e-9 * scale) {
                if (lim > negSup) {
                    negSup = lim;
                    negSupAttained = false;
                }
            } else if (lim > 1e-9 * scale) {
                if (lim < posSup) {
                    posSup = lim;
                    posSupAttained = false;
                }
            } else if (std::isfinite(edgeValue)) {
                if (edgeValue < 0 && 0.0 > negSup) {
                    negSup = 0.0;
                    negSupAttained = false;
                } else if (edgeValue > 0 && 0.0 < posSup) {
                    posSup = 0.0;
                    posSupAttained = false;
                }
            }
        };
        auto applyTail = [&](double lim, double edgeValue) {
            if (!std::isfinite(lim)) return;
            const double tol = 1e-9 * scale;
            if (lim <= minY + tol) {
                if (lim < minY) {
                    minY = lim;
                    minAttained = false;
                }
            }
            if (lim >= maxY - tol) {
                if (lim > maxY) {
                    maxY = lim;
                    maxAttained = false;
                }
            }
            tightenTail(lim, edgeValue);
        };
        for (const auto& in : out.domain) {
            if (in.loInfinite) {
                int sign = 1;
                const double edge = std::min(xmin, -1.0);
                if (divergesAtInfinity(f, x, edge, -1, &sign)) {
                    (sign < 0 ? unboundedBelow : unboundedAbove) = true;
                } else {
                    bool finite = false;
                    const double lim = tailLimit(f, x, edge, -1, &finite);
                    if (finite) applyTail(lim, firstFinite);
                }
            }
            if (in.hiInfinite) {
                int sign = 1;
                const double edge = std::max(xmax, 1.0);
                if (divergesAtInfinity(f, x, edge, +1, &sign)) {
                    (sign < 0 ? unboundedBelow : unboundedAbove) = true;
                } else {
                    bool finite = false;
                    const double lim = tailLimit(f, x, edge, +1, &finite);
                    if (finite) applyTail(lim, lastFinite);
                }
            }
        }
        for (double a : out.asymptotes) {
            int ls = 1;
            int rs = 1;
            if (divergesAt(f, x, a, -1, &ls)) {
                (ls < 0 ? unboundedBelow : unboundedAbove) = true;
            }
            if (divergesAt(f, x, a, +1, &rs)) {
                (rs < 0 ? unboundedBelow : unboundedAbove) = true;
            }
        }
        // One-sided behaviour at a finite domain edge: ln(x) at 0⁺ and
        // 1/sqrt(x) at 0⁺ diverge without any interior gap.
        for (const auto& in : out.domain) {
            if (!in.loInfinite) {
                int s = 1;
                if (divergesAt(f, x, in.lo, +1, &s)) {
                    (s < 0 ? unboundedBelow : unboundedAbove) = true;
                }
            }
            if (!in.hiInfinite) {
                int s = 1;
                if (divergesAt(f, x, in.hi, -1, &s)) {
                    (s < 0 ? unboundedBelow : unboundedAbove) = true;
                }
            }
        }

        if (!std::isfinite(minY)) minY = -1;
        if (!std::isfinite(maxY)) maxY = 1;
        if (maxY - minY < 1e-12) {
            minY -= 0.5;
            maxY += 0.5;
        }

        out.rangeUnboundedBelow = unboundedBelow;
        out.rangeUnboundedAbove = unboundedAbove;

        auto makeRange = [&](double lo, bool loInf, double hi, bool hiInf, bool loOpen,
                             bool hiOpen) {
            Interval in;
            in.lo = lo;
            in.hi = hi;
            in.loInfinite = loInf;
            in.hiInfinite = hiInf;
            in.loOpen = loInf || loOpen;
            in.hiOpen = hiInf || hiOpen;
            out.range.push_back(in);
        };

        // Base intervals: either one branch, or the negative and positive
        // branches separated by the value the function never attains.
        std::vector<Interval> base;
        auto addBase = [&](double lo, bool loInf, double hi, bool hiInf, bool loOpen,
                           bool hiOpen) {
            Interval in;
            in.lo = lo;
            in.hi = hi;
            in.loInfinite = loInf;
            in.hiInfinite = hiInf;
            in.loOpen = loInf || loOpen;
            in.hiOpen = hiInf || hiOpen;
            base.push_back(in);
        };
        if (out.splitAtZero) {
            addBase(minY, unboundedBelow, negSup, false,
                    unboundedBelow || !minAttained, !negSupAttained);
            addBase(posSup, false, maxY, unboundedAbove, !posSupAttained,
                    unboundedAbove || !maxAttained);
        } else {
            addBase(minY, unboundedBelow, maxY, unboundedAbove, !minAttained,
                    !maxAttained);
        }

        // Values omitted by removable holes split the range once more.
        for (const auto& in : base) {
            std::vector<double> cuts;
            for (double h : out.rangeHoles) {
                if (!std::isfinite(h)) continue;
                const double eps = 1e-9 * scale;
                if (in.loInfinite || h > in.lo + eps) {
                    if (in.hiInfinite || h < in.hi - eps) {
                        bool duplicate = false;
                        for (double c : cuts)
                            if (std::fabs(c - h) <= eps) duplicate = true;
                        if (!duplicate) cuts.push_back(h);
                    }
                }
            }
            std::sort(cuts.begin(), cuts.end());
            double lo = in.lo;
            bool loInf = in.loInfinite;
            bool loOpen = in.loOpen;
            for (double cut : cuts) {
                Interval piece;
                piece.lo = lo;
                piece.hi = cut;
                piece.loInfinite = loInf;
                piece.hiInfinite = false;
                piece.loOpen = loInf || loOpen;
                piece.hiOpen = true;
                if (piece.loInfinite || piece.hi - piece.lo > 1e-12) out.range.push_back(piece);
                lo = cut;
                loInf = false;
                loOpen = true;
            }
            Interval last;
            last.lo = lo;
            last.hi = in.hi;
            last.loInfinite = loInf;
            last.hiInfinite = in.hiInfinite;
            last.loOpen = loInf || loOpen;
            last.hiOpen = in.hiOpen;
            if (last.loInfinite || last.hiInfinite || last.hi - last.lo > 1e-12) {
                out.range.push_back(last);
            }
        }
        if (out.range.empty()) {
            makeRange(minY, unboundedBelow, maxY, unboundedAbove, unboundedBelow,
                      unboundedAbove);
        }

        // A lighter point set is enough for drawing but keeps the JSON small.
        const int stride = std::max(1, samples / 700);
        out.xs.reserve(samples / stride + 2);
        out.ys.reserve(samples / stride + 2);
        for (int i = 0; i < samples; i += stride) {
            out.xs.push_back(xs[i]);
            out.ys.push_back(ys[i]);
        }
        if (out.xs.empty() || out.xs.back() != xs.back()) {
            out.xs.push_back(xs.back());
            out.ys.push_back(ys.back());
        }
        out.ok = true;
    } catch (const std::exception& e) {
        out.error = "plot.failed";
    }
    return out;
}

namespace {

bool hasDerivative(const Ex& e) {
    if (!e) return false;
    if (e->kind == Kind::Func && e->name == "Deriv") return true;
    for (const auto& a : e->args)
        if (hasDerivative(a)) return true;
    return false;
}

// A slope extractor either succeeds, does not apply to this equation, or
// applies and fails with an explanation worth showing the user.
enum class SlopeResult { Ok, NotApplicable, Failed };

// A differential form writes the equation as M(x,y) dx + N(x,y) dy = 0. The
// parser keeps dx and dy as ordinary symbols, so the slope is recovered as
// y' = -M/N. Only a first-degree form is accepted: anything quadratic in dx or
// dy is not a direction field in this window.
SlopeResult slopeFromDifferentialForm(const Ex& F, const std::string& x, const std::string& y,
                                      Ex& slope, std::string& error) {
    std::vector<std::string> syms;
    collectSymbols(F, syms);
    bool hasDx = false;
    bool hasDy = false;
    for (const auto& s : syms) {
        if (s == "dx") hasDx = true;
        if (s == "dy") hasDy = true;
    }
    if (!hasDx && !hasDy) return SlopeResult::NotApplicable;

    Ex marker = Sym("__q");
    Ex G = substituteSubexpr(F, Sym("dx"), marker);
    std::vector<Ex> cs;
    if (!polyCoeffs(expand(G), "__q", cs) || cs.size() > 2) {
        error = "The differential form is not linear in dx and dy.";
        return SlopeResult::Failed;
    }
    Ex M = cs.size() == 2 ? cs[1] : Num(0);
    Ex rest = cs[0];
    if (dependsOn(rest, "dx")) {
        error = "The differential form is not linear in dx and dy.";
        return SlopeResult::Failed;
    }
    std::vector<Ex> cs2;
    if (!polyCoeffs(expand(rest), "dy", cs2) || cs2.size() > 2) {
        error = "The differential form is not linear in dx and dy.";
        return SlopeResult::Failed;
    }
    Ex N = cs2.size() == 2 ? cs2[1] : Num(0);
    if (cs2.size() == 2 && !cs2[0]->isZero()) {
        error = "The differential form has terms independent of dx and dy.";
        return SlopeResult::Failed;
    }
    if (dependsOn(M, "dx") || dependsOn(M, "dy") || dependsOn(N, "dx") || dependsOn(N, "dy")) {
        error = "The differential form is not linear in dx and dy.";
        return SlopeResult::Failed;
    }
    if (N->isZero()) {
        // N = 0 turns the form into M(x,y) dx = 0, a curve rather than a field.
        error = "N(x,y) = 0, so the form cannot be solved for dy/dx.";
        return SlopeResult::Failed;
    }
    slope = simplify(Mul(Num(-1), M, Pow(N, Num(-1))));
    if (hasDerivative(slope)) {
        error = "The equation is not first order.";
        return SlopeResult::Failed;
    }
    return SlopeResult::Ok;
}

// y = x*y' + f(y') and similar implicit first-order forms are quadratic in y'.
// The two roots are the branches of the Clairaut/Lagrange family; when the
// coefficients are symbolic they cannot be reduced to rational roots, so the
// quadratic formula is kept symbolically and the numeric branch test in
// analyseField decides which sheet a trajectory sits on.
SlopeResult slopeFromQuadratic(const Ex& F, const std::string& x, const std::string& y, Ex& slope,
                               std::string& error) {
    Ex marker = Sym("__p");
    Ex G = substituteSubexpr(F, Deriv(Sym(y), x, 1), marker);
    std::vector<Ex> cs;
    if (!polyCoeffs(expand(G), "__p", cs) || cs.size() != 3) return SlopeResult::NotApplicable;
    if (cs[2]->isZero()) return SlopeResult::NotApplicable;
    Ex disc = simplify(Add({Mul(cs[1], cs[1]), Mul(Num(-4), cs[2], cs[0])}));
    std::vector<Ex> rational = solvePoly(
        Add({Mul(cs[2], Pow(marker, Num(2))), Mul(cs[1], marker), cs[0]}), "__p");
    if (!rational.empty()) {
        slope = rational.front();
    } else {
        slope = simplify(
            Mul(Add({Mul(Num(-1), cs[1]), Fn("sqrt", {disc})}), Pow(Mul(Num(2), cs[2]), Num(-1))));
    }
    if (hasDerivative(slope)) {
        error = "The equation is not first order.";
        return SlopeResult::Failed;
    }
    return SlopeResult::Ok;
}

SlopeResult extractSlope(const Ex& F, const std::string& x, const std::string& y, Ex& slope,
                         std::string& error) {
    Ex marker = Sym("__p");
    Ex G = substituteSubexpr(F, Deriv(Sym(y), x, 1), marker);
    std::vector<Ex> cs;
    if (!polyCoeffs(expand(G), "__p", cs)) {
        // Not polynomial in y'; a differential form might still describe it.
        return slopeFromDifferentialForm(F, x, y, slope, error);
    }
    if (cs.size() == 2 && !cs[1]->isZero()) {
        slope = simplify(Mul(Num(-1), cs[0], Pow(cs[1], Num(-1))));
        if (hasDerivative(slope)) {
            error = "The equation is not first order.";
            return SlopeResult::Failed;
        }
        return SlopeResult::Ok;
    }

    // No usable linear coefficient: try the quadratic branches, then the
    // differential-form reading, before giving up.
    std::string quadraticError;
    const SlopeResult quadratic = slopeFromQuadratic(F, x, y, slope, quadraticError);
    if (quadratic == SlopeResult::Ok) return quadratic;

    std::string formError;
    const SlopeResult form = slopeFromDifferentialForm(F, x, y, slope, formError);
    if (form == SlopeResult::Ok) return form;

    if (form == SlopeResult::Failed && !formError.empty()) error = formError;
    else if (quadratic == SlopeResult::Failed && !quadraticError.empty()) error = quadraticError;
    else error = "This direction field needs a first-order equation solved for y'.";
    return SlopeResult::Failed;
}

double slopeAt(const Ex& f, const std::string& x, const std::string& y, double xv, double yv,
               bool* okp) {
    return at2(f, x, xv, y, yv, okp);
}

// When y' cannot be isolated in closed form, the equation may still be
// quadratic in y'. That is exactly the Clairaut/Lagrange shape, so the field
// picks the branch whose value is closest to the slope already carried by the
// trajectory. Choosing the nearest branch keeps a trajectory on one sheet
// instead of jumping between them.
class SlopeFunction {
public:
    SlopeFunction(Ex closed, Ex quadA, Ex quadB, Ex quadC, std::string x, std::string y)
        : closed_(std::move(closed)), a_(std::move(quadA)), b_(std::move(quadB)),
          c_(std::move(quadC)), x_(std::move(x)), y_(std::move(y)) {}

    bool isQuadratic() const { return a_ != nullptr; }

    double at(double xv, double yv, double hint, bool* okp) const {
        if (!a_) return at2(closed_, x_, xv, y_, yv, okp);
        bool oka = false, okb = false, okc = false;
        const double A = at2(a_, x_, xv, y_, yv, &oka);
        const double B = at2(b_, x_, xv, y_, yv, &okb);
        const double C = at2(c_, x_, xv, y_, yv, &okc);
        if (!oka || !okb || !okc || !std::isfinite(A) || !std::isfinite(B) || !std::isfinite(C)) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        std::vector<double> roots;
        if (std::fabs(A) < 1e-12) {
            if (std::fabs(B) > 1e-12) roots.push_back(-C / B);
        } else {
            const double disc = B * B - 4 * A * C;
            if (disc >= 0) {
                const double s = std::sqrt(disc);
                roots.push_back((-B + s) / (2 * A));
                roots.push_back((-B - s) / (2 * A));
            }
        }
        if (roots.empty()) return std::numeric_limits<double>::quiet_NaN();
        double best = roots.front();
        double bestGap = std::numeric_limits<double>::infinity();
        for (double r : roots) {
            const double gap = std::fabs(r - hint);
            if (gap < bestGap) {
                bestGap = gap;
                best = r;
            }
        }
        if (okp) *okp = true;
        return best;
    }

    const Ex& closed() const { return closed_; }

private:
    Ex closed_;
    Ex a_;
    Ex b_;
    Ex c_;
    std::string x_;
    std::string y_;
};

void integrateTrajectory(const SlopeFunction& f, double x0, double y0, double xEnd,
                         FieldCurve& curve, double yLimit) {
    const int steps = 260;
    const double h = (xEnd - x0) / (double)steps;
    if (!std::isfinite(h) || h == 0) return;
    double xv = x0;
    double yv = y0;
    double hint = 0;
    curve.xs.push_back(xv);
    curve.ys.push_back(yv);
    for (int i = 0; i < steps; ++i) {
        bool ok1 = false, ok2 = false, ok3 = false, ok4 = false;
        const double k1 = f.at(xv, yv, hint, &ok1);
        const double k2 = f.at(xv + h / 2, yv + h * k1 / 2, k1, &ok2);
        const double k3 = f.at(xv + h / 2, yv + h * k2 / 2, k2, &ok3);
        const double k4 = f.at(xv + h, yv + h * k3, k3, &ok4);
        if (!ok1 || !ok2 || !ok3 || !ok4) break;
        const double nextY = yv + h * (k1 + 2 * k2 + 2 * k3 + k4) / 6;
        xv += h;
        yv = nextY;
        hint = k4;
        if (!std::isfinite(xv) || !std::isfinite(yv) || std::fabs(yv) > yLimit) break;
        curve.xs.push_back(xv);
        curve.ys.push_back(yv);
    }
    curve.ok = curve.xs.size() > 1;
}

} // namespace

FieldOutput analyseField(const std::string& input, const std::string& x, const std::string& y,
                         double xmin, double xmax, double ymin, double ymax, int cols,
                         int rows, int curveCount,
                         const std::vector<std::pair<double, double>>& ics) {
    FieldOutput out;
    try {
        if (!(xmin < xmax) || !(ymin < ymax)) {
            out.error = "plot.empty_window";
            return out;
        }
        std::string dep;
        Ex F = parseODEExpr(input, x, &dep, {x, y, "c"});
        const std::string depVar = dep.empty() ? y : dep;
        Ex slope;
        std::string why;
        // A bare expression with no derivative and no '=' is the right-hand
        // side itself: `y - x^2 + 1` means y' = y - x^2 + 1.  An actual
        // equation still has to be solved for y'.
        const bool bareRhs = input.find('=') == std::string::npos && !hasDerivative(F);
        if (bareRhs) {
            slope = simplify(F);
        } else if (extractSlope(F, x, depVar, slope, why) != SlopeResult::Ok) {
            out.error = "plot.unsupported_form";
            return out;
        }
        // A quadratic in y' has two solution branches (Clairaut and Lagrange
        // belong here). Keeping both branches numerically and following the
        // nearest one stops a trajectory from jumping between sheets.
        SlopeFunction field(slope, nullptr, nullptr, nullptr, x, depVar);
        Ex marker = Sym("__p");
        Ex G = substituteSubexpr(F, Deriv(Sym(depVar), x, 1), marker);
        std::vector<Ex> cs;
        const bool quadratic = polyCoeffs(expand(G), "__p", cs) && cs.size() == 3 &&
                               !cs[2]->isZero();
        if (quadratic) {
            field = SlopeFunction(nullptr, cs[2], cs[1], cs[0], x, depVar);
            out.rhs = "(" + toPretty(cs[2]) + ")(y′)² + (" + toPretty(cs[1]) + ")y′ + (" +
                      toPretty(cs[0]) + ") = 0";
        } else {
            out.rhs = toPretty(slope);
        }
        out.xmin = xmin;
        out.xmax = xmax;
        out.ymin = ymin;
        out.ymax = ymax;
        cols = std::max(4, std::min(cols, 61));
        rows = std::max(4, std::min(rows, 61));
        out.cols = cols;
        out.rows = rows;

        out.slopes.reserve((size_t)cols * rows);
        for (int r = 0; r < rows; ++r) {
            const double yv = ymin + (ymax - ymin) * (double)r / (double)(rows - 1);
            for (int c = 0; c < cols; ++c) {
                const double xv = xmin + (xmax - xmin) * (double)c / (double)(cols - 1);
                bool ok = false;
                const double m = field.at(xv, yv, 0, &ok);
                out.slopes.push_back(ok && std::isfinite(m) ? m : kNaN);
            }
        }

        std::vector<std::pair<double, double>> seeds = ics;
        curveCount = std::max(0, std::min(curveCount, 9));
        if (seeds.empty() && curveCount > 0) {
            const double xMid = 0.5 * (xmin + xmax);
            for (int i = 0; i < curveCount; ++i) {
                const double t = (double)(i + 1) / (double)(curveCount + 1);
                seeds.emplace_back(xMid, ymin + (ymax - ymin) * t);
            }
        }
        const double yLimit = std::max(std::fabs(ymin), std::fabs(ymax)) * 3.0 + 8.0;
        for (const auto& seed : seeds) {
            FieldCurve curve;
            curve.icX = seed.first;
            curve.icY = seed.second;
            integrateTrajectory(field, seed.first, seed.second, xmax, curve, yLimit);
            if (curve.ok) {
                FieldCurve back;
                integrateTrajectory(field, seed.first, seed.second, xmin, back, yLimit);
                if (back.ok) {
                    for (size_t i = 1; i < back.xs.size(); ++i) {
                        curve.xs.push_back(back.xs[i]);
                        curve.ys.push_back(back.ys[i]);
                    }
                    std::vector<size_t> order(curve.xs.size());
                    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
                    std::sort(order.begin(), order.end(),
                              [&](size_t a, size_t b) { return curve.xs[a] < curve.xs[b]; });
                    std::vector<double> sx;
                    std::vector<double> sy;
                    sx.reserve(order.size());
                    sy.reserve(order.size());
                    for (size_t idx : order) {
                        sx.push_back(curve.xs[idx]);
                        sy.push_back(curve.ys[idx]);
                    }
                    curve.xs.swap(sx);
                    curve.ys.swap(sy);
                }
                out.curves.push_back(curve);
            }
        }
        out.ok = true;
    } catch (const std::exception& e) {
        out.error = "plot.failed";
    }
    return out;
}

// Highest derivative order of `dep` with respect to `x` in the expression.
static int maxDerivativeOrder(const Ex& e, const std::string& dep, const std::string& x) {
    if (isDeriv(e) && e->args.size() == 3 && e->args[0]->isSym(dep) && e->args[1]->isSym(x)) {
        return derivOrder(e);
    }
    int best = 0;
    for (const auto& a : e->args) best = std::max(best, maxDerivativeOrder(a, dep, x));
    return best;
}

TrajectoryOutput analyseTrajectory(const std::string& input, const std::string& x,
                                   const std::string& y, double xmin, double xmax,
                                   int curveCount,
                                   const std::vector<TrajectorySeed>& seedsIn) {
    TrajectoryOutput out;
    try {
        if (!(xmin < xmax)) {
            out.error = "plot.empty_window";
            return out;
        }
        std::string dep;
        Ex F = parseODEExpr(input, x, &dep, {x, y, "c"});
        const std::string depVar = dep.empty() ? y : dep;
        const int order = maxDerivativeOrder(F, depVar, x);
        if (order < 2 || order > 6) {
            out.error = "plot.numeric_order";
            return out;
        }
        out.order = order;

        Ex marker = Sym("__q");
        Ex G = substituteSubexpr(F, Deriv(Sym(depVar), x, order), marker);
        std::vector<Ex> cs;
        if (!polyCoeffs(expand(G), "__q", cs) || cs.size() != 2 || cs[1]->isZero()) {
            out.error = "plot.linear_requirement";
            return out;
        }
        Ex rhs = simplify(Mul(Num(-1), cs[0], Pow(cs[1], Num(-1))));

        // Replace y', y'', ... with the state variables of the first-order system.
        Ex rhsState = rhs;
        for (int k = 1; k < order; ++k) {
            rhsState = substituteSubexpr(rhsState, Deriv(Sym(depVar), x, k),
                                         Sym("__d" + std::to_string(k)));
        }
        if (maxDerivativeOrder(rhsState, depVar, x) > 0) {
            out.error = "plot.isolate_derivative";
            return out;
        }
        out.rhs = toPlain(rhs);
        out.xmin = xmin;
        out.xmax = xmax;

        auto system = [&](double xv, const std::vector<double>& z,
                          std::vector<double>& dz) -> bool {
            dz.assign((size_t)order, 0.0);
            for (int i = 0; i + 1 < order; ++i) dz[(size_t)i] = z[(size_t)i + 1];
            std::map<std::string, double> vars;
            vars[x] = xv;
            vars[depVar] = z[0];
            for (int k = 1; k < order; ++k) vars["__d" + std::to_string(k)] = z[(size_t)k];
            bool ok = false;
            const double top = evalReal(rhsState, vars, &ok);
            if (!ok || !std::isfinite(top)) return false;
            dz[(size_t)order - 1] = top;
            return true;
        };

        const int steps = 360;
        const double stateLimit = 1e6;
        auto integrate = [&](const TrajectorySeed& seed, double xEnd, FieldCurve& curve) {
            const double h = (xEnd - seed.x0) / (double)steps;
            if (!std::isfinite(h) || h == 0) return;
            std::vector<double> z((size_t)order, 0.0);
            z[0] = seed.y0;
            if (order >= 2) z[1] = seed.v0;
            double xv = seed.x0;
            curve.xs.push_back(xv);
            curve.ys.push_back(z[0]);
            std::vector<double> k1, k2, k3, k4, tmp;
            for (int i = 0; i < steps; ++i) {
                if (!system(xv, z, k1)) break;
                tmp.resize((size_t)order);
                for (int j = 0; j < order; ++j) tmp[(size_t)j] = z[(size_t)j] + 0.5 * h * k1[(size_t)j];
                if (!system(xv + h / 2, tmp, k2)) break;
                for (int j = 0; j < order; ++j) tmp[(size_t)j] = z[(size_t)j] + 0.5 * h * k2[(size_t)j];
                if (!system(xv + h / 2, tmp, k3)) break;
                for (int j = 0; j < order; ++j) tmp[(size_t)j] = z[(size_t)j] + h * k3[(size_t)j];
                if (!system(xv + h, tmp, k4)) break;
                bool finite = true;
                for (int j = 0; j < order; ++j) {
                    z[(size_t)j] += h * (k1[(size_t)j] + 2 * k2[(size_t)j] +
                                         2 * k3[(size_t)j] + k4[(size_t)j]) /
                                    6.0;
                    if (!std::isfinite(z[(size_t)j]) || std::fabs(z[(size_t)j]) > stateLimit) {
                        finite = false;
                        break;
                    }
                }
                if (!finite) break;
                xv += h;
                curve.xs.push_back(xv);
                curve.ys.push_back(z[0]);
            }
            curve.ok = curve.xs.size() > 1;
        };

        std::vector<TrajectorySeed> seeds = seedsIn;
        curveCount = std::max(1, std::min(curveCount, 12));
        if (seeds.empty()) {
            const double xMid = 0.5 * (xmin + xmax);
            for (int i = 0; i < curveCount; ++i) {
                const double t = (double)(i + 1) / (double)(curveCount + 1);
                seeds.push_back({xMid, -2.0 + 4.0 * t, 0.0});
            }
            // A few slopes make the two-parameter family visible instead of a
            // stack of parallel translates.
            seeds.push_back({xMid, 0.0, 1.5});
            seeds.push_back({xMid, 0.0, -1.5});
        }

        for (const auto& seed : seeds) {
            FieldCurve curve;
            curve.icX = seed.x0;
            curve.icY = seed.y0;
            integrate(seed, xmax, curve);
            if (curve.ok) {
                FieldCurve back;
                integrate(seed, xmin, back);
                if (back.ok) {
                    for (size_t i = 1; i < back.xs.size(); ++i) {
                        curve.xs.push_back(back.xs[i]);
                        curve.ys.push_back(back.ys[i]);
                    }
                    std::vector<size_t> orderIdx(curve.xs.size());
                    for (size_t i = 0; i < orderIdx.size(); ++i) orderIdx[i] = i;
                    std::sort(orderIdx.begin(), orderIdx.end(),
                              [&](size_t a, size_t b) { return curve.xs[a] < curve.xs[b]; });
                    std::vector<double> sx;
                    std::vector<double> sy;
                    sx.reserve(orderIdx.size());
                    sy.reserve(orderIdx.size());
                    for (size_t idx : orderIdx) {
                        sx.push_back(curve.xs[idx]);
                        sy.push_back(curve.ys[idx]);
                    }
                    curve.xs.swap(sx);
                    curve.ys.swap(sy);
                }
                out.curves.push_back(curve);
            }
        }
        out.ok = !out.curves.empty();
        if (!out.ok && out.error.empty()) {
            out.error = "plot.domain_exit";
        }
    } catch (const std::exception& e) {
        out.error = "plot.failed";
    }
    return out;
}

// A point that falls exactly on a removable singularity (sin(r)/r at the
// origin, for example) evaluates to NaN, which would leave a hole in an
// otherwise smooth surface.  Probe a shrinking neighbourhood in several
// directions and adopt the common limit when all probes agree.
bool removableLimit(const Ex& f, const std::string& x, const std::string& y,
                    double xv, double yv, double scale, double& value) {
    static const double dirs[8][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, -1}, {1, -1}, {-1, 1}};
    double delta = scale > 0 ? scale : 1.0;
    for (int level = 0; level < 5; ++level) {
        double lo = std::numeric_limits<double>::infinity();
        double hi = -lo;
        double sum = 0.0;
        int good = 0;
        for (const auto& d : dirs) {
            bool ok = false;
            const double v = at2(f, x, xv + d[0] * delta, y, yv + d[1] * delta, &ok);
            if (!ok || !std::isfinite(v)) continue;
            lo = std::min(lo, v);
            hi = std::max(hi, v);
            sum += v;
            ++good;
        }
        if (good >= 6) {
            const double mean = sum / good;
            if (hi - lo <= 1e-3 * (1.0 + std::fabs(mean))) {
                value = mean;
                return true;
            }
        }
        delta *= 0.1;
    }
    return false;
}

SurfaceOutput sampleSurface(const std::string& input, const std::string& x,
                            const std::string& y, double xmin, double xmax, double ymin,
                            double ymax, int nx, int ny) {
    SurfaceOutput out;
    try {
        if (!(xmin < xmax) || !(ymin < ymax)) {
            out.error = "plot.empty_window";
            return out;
        }
        Ex f = parseODEExpr(input, x, nullptr, {x, y, "e", "pi"});
        nx = std::max(8, std::min(nx, 160));
        ny = std::max(8, std::min(ny, 160));
        out.xmin = xmin;
        out.xmax = xmax;
        out.ymin = ymin;
        out.ymax = ymax;
        out.nx = nx;
        out.ny = ny;
        out.zs.reserve((size_t)nx * ny);
        const double step = std::max((xmax - xmin) / std::max(1, nx - 1),
                                     (ymax - ymin) / std::max(1, ny - 1));
        for (int r = 0; r < ny; ++r) {
            const double yv = ymin + (ymax - ymin) * (double)r / (double)(ny - 1);
            for (int c = 0; c < nx; ++c) {
                const double xv = xmin + (xmax - xmin) * (double)c / (double)(nx - 1);
                bool ok = false;
                double zv = at2(f, x, xv, y, yv, &ok);
                if (!ok || !std::isfinite(zv)) {
                    double limit = 0.0;
                    if (removableLimit(f, x, y, xv, yv, step * 1e-2, limit)) {
                        zv = limit;
                        ok = true;
                    }
                }
                out.zs.push_back(ok && std::isfinite(zv) ? zv : kNaN);
            }
        }
        out.ok = true;
    } catch (const std::exception& e) {
        out.error = "plot.failed";
    }
    return out;
}

} // namespace ode
