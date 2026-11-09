// Infinit Lab — JSON API on top of the native solver.
// SPDX-License-Identifier: Apache-2.0
#include "graph.hpp"
#include "calculus_apps.hpp"
#include "localize.hpp"
#include "odecore.h"
#include "solver.hpp"
#include "topics.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------- tiny JSON
struct JVal {
    enum Kind { Null, Bool, Num, Str, Arr, Obj } kind = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<JVal> arr;
    std::map<std::string, JVal> obj;

    const JVal* get(const std::string& key) const {
        auto it = obj.find(key);
        return it == obj.end() ? nullptr : &it->second;
    }
    std::string str(const std::string& key, const std::string& def = "") const {
        const JVal* v = get(key);
        return v && v->kind == Str ? v->s : def;
    }
    bool boolean(const std::string& key, bool def = false) const {
        const JVal* v = get(key);
        return v && v->kind == Bool ? v->b : def;
    }
};

struct DepthGuard {
    int& d;
    explicit DepthGuard(int& depth) : d(depth) {
        if (++d > 64) throw std::runtime_error("JSON too deeply nested");
    }
    ~DepthGuard() { --d; }
};

struct JsonParser {
    const std::string& s;
    size_t i = 0;
    int depth = 0;
    explicit JsonParser(const std::string& src) : s(src) {}

    void ws() { while (i < s.size() && std::isspace((unsigned char)s[i])) ++i; }
    bool eat(char c) { ws(); if (i < s.size() && s[i] == c) { ++i; return true; } return false; }

    std::string string() {
        std::string out;
        if (!eat('"')) throw std::runtime_error("invalid JSON string");
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return out;
            if (c == '\\') {
                if (i >= s.size()) break;
                char e = s[i++];
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        unsigned code = 0;
                        for (int k = 0; k < 4 && i < s.size(); ++k) {
                            char h = s[i++];
                            code <<= 4;
                            if (h >= '0' && h <= '9') code |= (unsigned)(h - '0');
                            else if (h >= 'a' && h <= 'f') code |= (unsigned)(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') code |= (unsigned)(h - 'A' + 10);
                        }
                        // encode as UTF-8 (surrogate pairs are passed through as-is;
                        // the UI sends complete code points).
                        if (code < 0x80) out += (char)code;
                        else if (code < 0x800) {
                            out += (char)(0xC0 | (code >> 6));
                            out += (char)(0x80 | (code & 0x3F));
                        } else {
                            out += (char)(0xE0 | (code >> 12));
                            out += (char)(0x80 | ((code >> 6) & 0x3F));
                            out += (char)(0x80 | (code & 0x3F));
                        }
                        break;
                    }
                    default: out += e; break;
                }
            } else {
                out += c;
            }
        }
        throw std::runtime_error("unterminated JSON string");
    }

    JVal value() {
        DepthGuard guard(depth);
        ws();
        if (i >= s.size()) throw std::runtime_error("unexpected end of JSON");
        JVal v;
        char c = s[i];
        if (c == '"') { v.kind = JVal::Str; v.s = string(); return v; }
        if (c == '{') {
            ++i;
            v.kind = JVal::Obj;
            ws();
            if (eat('}')) return v;
            while (true) {
                ws();
                std::string key = string();
                if (!eat(':')) throw std::runtime_error("expected ':' in JSON");
                v.obj[key] = value();
                ws();
                if (eat('}')) return v;
                if (!eat(',')) throw std::runtime_error("expected ',' in JSON");
            }
        }
        if (c == '[') {
            ++i;
            v.kind = JVal::Arr;
            ws();
            if (eat(']')) return v;
            while (true) {
                v.arr.push_back(value());
                ws();
                if (eat(']')) return v;
                if (!eat(',')) throw std::runtime_error("expected ',' in JSON array");
            }
        }
        if (s.compare(i, 4, "true") == 0) { i += 4; v.kind = JVal::Bool; v.b = true; return v; }
        if (s.compare(i, 5, "false") == 0) { i += 5; v.kind = JVal::Bool; v.b = false; return v; }
        if (s.compare(i, 4, "null") == 0) { i += 4; return v; }
        size_t start = i;
        if (s[i] == '-' || s[i] == '+') ++i;
        while (i < s.size() && (std::isdigit((unsigned char)s[i]) || s[i] == '.' || s[i] == 'e' || s[i] == 'E' || s[i] == '+' || s[i] == '-')) ++i;
        if (i == start) throw std::runtime_error("invalid JSON value");
        v.kind = JVal::Num;
        v.n = std::strtod(s.substr(start, i - start).c_str(), nullptr);
        return v;
    }
};

std::string esc(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\b': o += "\\b"; break;
            case '\f': o += "\\f"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    o += buf;
                } else {
                    o += (char)c;
                }
        }
    }
    return o;
}

std::string q(const std::string& s) { return "\"" + esc(s) + "\""; }

std::string numJson(double v) {
    if (!std::isfinite(v)) return "null";
    std::ostringstream o;
    o << std::setprecision(10) << v;
    return o.str();
}

std::string numArrayJson(const std::vector<double>& v) {
    std::string out = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += ",";
        out += numJson(v[i]);
    }
    out += "]";
    return out;
}

std::string intervalArrayJson(const std::vector<ode::Interval>& v) {
    std::string out = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += ",";
        const auto& in = v[i];
        out += "{\"lo\":";
        out += in.loInfinite ? "null" : numJson(in.lo);
        out += ",\"hi\":";
        out += in.hiInfinite ? "null" : numJson(in.hi);
        out += ",\"lo_inf\":";
        out += in.loInfinite ? "true" : "false";
        out += ",\"hi_inf\":";
        out += in.hiInfinite ? "true" : "false";
        out += ",\"lo_open\":";
        out += in.loOpen ? "true" : "false";
        out += ",\"hi_open\":";
        out += in.hiOpen ? "true" : "false";
        out += "}";
    }
    out += "]";
    return out;
}

std::map<std::string, double> constantsFrom(const JVal& req) {
    std::map<std::string, double> out;
    if (const JVal* c = req.get("constants")) {
        if (c->kind == JVal::Obj) {
            for (const auto& kv : c->obj) {
                if (kv.second.kind == JVal::Num && std::isfinite(kv.second.n)) {
                    out[kv.first] = kv.second.n;
                }
            }
        }
    }
    return out;
}

double numberArg(const JVal& req, const char* key, double fallback) {
    if (const JVal* v = req.get(key)) {
        if (v->kind == JVal::Num) return v->n;
    }
    return fallback;
}

std::string graphError(const std::string& mode, const std::string& error) {
    return "{\"ok\":false,\"mode\":" + q(mode) + ",\"error\":" + q(error) + "}";
}

std::string graphResultJson(const JVal& req, const std::string& mode) {
    if (mode == "sample") {
        const auto sample = ode::sampleFunction(
            req.str("input"), req.str("x", "x"), numberArg(req, "xmin", -10.0),
            numberArg(req, "xmax", 10.0), (int)numberArg(req, "samples", 600),
            constantsFrom(req));
        if (!sample.ok) return graphError(mode, sample.error);
        return "{\"ok\":true,\"mode\":\"sample\",\"xs\":" + numArrayJson(sample.xs) +
               ",\"ys\":" + numArrayJson(sample.ys) + "}";
    }
    if (mode == "domain") {
        const auto report = ode::analyseDomain(
            req.str("input"), req.str("x", "x"), numberArg(req, "xmin", -10.0),
            numberArg(req, "xmax", 10.0), (int)numberArg(req, "samples", 2401));
        if (!report.ok) return graphError(mode, report.error);
        std::ostringstream o;
        o << "{\"ok\":true,\"mode\":\"domain\",\"kind\":" << q(report.kind)
          << ",\"domain\":" << intervalArrayJson(report.domain)
          << ",\"range\":" << intervalArrayJson(report.range)
          << ",\"holes\":" << numArrayJson(report.holes)
          << ",\"asymptotes\":" << numArrayJson(report.asymptotes)
          << ",\"range_holes\":" << numArrayJson(report.rangeHoles)
          << ",\"range_unbounded_below\":"
          << (report.rangeUnboundedBelow ? "true" : "false")
          << ",\"range_unbounded_above\":"
          << (report.rangeUnboundedAbove ? "true" : "false")
          << ",\"split_at_zero\":" << (report.splitAtZero ? "true" : "false")
          << ",\"xs\":" << numArrayJson(report.xs)
          << ",\"ys\":" << numArrayJson(report.ys) << "}";
        return o.str();
    }
    if (mode == "field") {
        std::vector<std::pair<double, double>> ics;
        if (const JVal* arr = req.get("ics")) {
            if (arr->kind == JVal::Arr) {
                for (const auto& v : arr->arr) {
                    if (v.kind == JVal::Arr && v.arr.size() >= 2 &&
                        v.arr[0].kind == JVal::Num && v.arr[1].kind == JVal::Num) {
                        ics.emplace_back(v.arr[0].n, v.arr[1].n);
                    }
                }
            }
        }
        const auto field = ode::analyseField(
            req.str("input"), req.str("x", "x"), req.str("y", "y"),
            numberArg(req, "xmin", -6.0), numberArg(req, "xmax", 6.0),
            numberArg(req, "ymin", -6.0), numberArg(req, "ymax", 6.0),
            (int)numberArg(req, "cols", 21), (int)numberArg(req, "rows", 21),
            (int)numberArg(req, "curves", 6), ics);
        if (!field.ok) return graphError(mode, field.error);
        std::ostringstream o;
        o << "{\"ok\":true,\"mode\":\"field\",\"rhs\":" << q(field.rhs)
          << ",\"xmin\":" << numJson(field.xmin) << ",\"xmax\":" << numJson(field.xmax)
          << ",\"ymin\":" << numJson(field.ymin) << ",\"ymax\":" << numJson(field.ymax)
          << ",\"cols\":" << field.cols << ",\"rows\":" << field.rows << ",\"slopes\":[";
        for (int r = 0; r < field.rows; ++r) {
            if (r) o << ",";
            o << "[";
            for (int c = 0; c < field.cols; ++c) {
                if (c) o << ",";
                o << numJson(field.slopes[(size_t)r * field.cols + c]);
            }
            o << "]";
        }
        o << "],\"curves\":[";
        for (size_t i = 0; i < field.curves.size(); ++i) {
            if (i) o << ",";
            const auto& curve = field.curves[i];
            o << "{\"ic_x\":" << numJson(curve.icX) << ",\"ic_y\":" << numJson(curve.icY)
              << ",\"xs\":" << numArrayJson(curve.xs)
              << ",\"ys\":" << numArrayJson(curve.ys) << "}";
        }
        o << "]}";
        return o.str();
    }
    if (mode == "ode_trajectory") {
        std::vector<ode::TrajectorySeed> seeds;
        if (const JVal* arr = req.get("ics")) {
            if (arr->kind == JVal::Arr) {
                for (const auto& v : arr->arr) {
                    if (v.kind == JVal::Arr && v.arr.size() >= 3 &&
                        v.arr[0].kind == JVal::Num && v.arr[1].kind == JVal::Num &&
                        v.arr[2].kind == JVal::Num) {
                        seeds.push_back({v.arr[0].n, v.arr[1].n, v.arr[2].n});
                    }
                }
            }
        }
        const auto traj = ode::analyseTrajectory(
            req.str("input"), req.str("x", "x"), req.str("y", "y"),
            numberArg(req, "xmin", -6.0), numberArg(req, "xmax", 6.0),
            (int)numberArg(req, "curves", 6), seeds);
        if (!traj.ok) return graphError(mode, traj.error);
        std::ostringstream o;
        o << "{\"ok\":true,\"mode\":\"ode_trajectory\",\"rhs\":" << q(traj.rhs)
          << ",\"order\":" << traj.order
          << ",\"xmin\":" << numJson(traj.xmin) << ",\"xmax\":" << numJson(traj.xmax)
          << ",\"curves\":[";
        for (size_t i = 0; i < traj.curves.size(); ++i) {
            if (i) o << ",";
            const auto& curve = traj.curves[i];
            o << "{\"x0\":" << numJson(curve.icX) << ",\"y0\":" << numJson(curve.icY)
              << ",\"xs\":" << numArrayJson(curve.xs)
              << ",\"ys\":" << numArrayJson(curve.ys) << "}";
        }
        o << "]}";
        return o.str();
    }
    if (mode == "surface") {
        const auto surf = ode::sampleSurface(
            req.str("input"), req.str("x", "x"), req.str("y", "y"),
            numberArg(req, "xmin", -4.0), numberArg(req, "xmax", 4.0),
            numberArg(req, "ymin", -4.0), numberArg(req, "ymax", 4.0),
            (int)numberArg(req, "nx", 48), (int)numberArg(req, "ny", 48));
        if (!surf.ok) return graphError(mode, surf.error);
        std::ostringstream o;
        o << "{\"ok\":true,\"mode\":\"surface\",\"xmin\":" << numJson(surf.xmin)
          << ",\"xmax\":" << numJson(surf.xmax) << ",\"ymin\":" << numJson(surf.ymin)
          << ",\"ymax\":" << numJson(surf.ymax) << ",\"nx\":" << surf.nx
          << ",\"ny\":" << surf.ny << ",\"zs\":[";
        for (int r = 0; r < surf.ny; ++r) {
            if (r) o << ",";
            o << "[";
            for (int c = 0; c < surf.nx; ++c) {
                if (c) o << ",";
                o << numJson(surf.zs[(size_t)r * surf.nx + c]);
            }
            o << "]";
        }
        o << "]}";
        return o.str();
    }
    return graphError(mode, "Unknown graph mode");
}


std::string exJson(const ode::Ex& e) {
    if (!e) return "null";
    return "{\"plain\":" + q(ode::toPlain(e)) + ",\"pretty\":" + q(ode::toPretty(e)) +
           ",\"latex\":" + q(ode::toLatex(e)) + "}";
}

std::string exArrayJson(const std::vector<ode::Ex>& v) {
    std::string out = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += ",";
        out += exJson(v[i]);
    }
    out += "]";
    return out;
}

std::string strArrayJson(const std::vector<std::string>& v) {
    std::string out = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += ",";
        out += q(v[i]);
    }
    out += "]";
    return out;
}

std::string resultJson(const ode::Result& r) {
    std::ostringstream o;
    o << "{";
    o << "\"ok\":" << (r.ok ? "true" : "false");
    o << ",\"mode\":" << q(r.mode);
    o << ",\"type_id\":" << q(r.typeId);
    o << ",\"type_fa\":" << q(r.typeFa.empty() && !r.typeId.empty() ? ode::typeNameFa(r.typeId) : r.typeFa);
    o << ",\"type_en\":" << q(ode::typeNameEn(r.typeId));
    o << ",\"method\":" << q(r.method);
    o << ",\"variable\":" << q(r.variable);
    o << ",\"order\":" << r.order;
    o << ",\"engine\":" << q(r.engine);
    o << ",\"error\":" << q(r.error);
    o << ",\"error_code\":" << q(r.errorCode);
    o << ",\"error_detail\":" << q(r.errorDetail);
    o << ",\"error_point\":"
      << (r.hasErrorPoint ? numJson(r.errorPoint) : "null");
    o << ",\"interpreted_as\":" << q(r.interpretedAs);
    o << ",\"closed_form\":" << (r.closedForm ? "true" : "false");
    o << ",\"time_ms\":" << r.timeMs;
    o << ",\"normalized\":" << exJson(r.normalized);
    o << ",\"solution\":" << exJson(r.solution);
    o << ",\"is_explicit\":" << (r.isExplicit ? "true" : "false");
    o << ",\"explicit_y\":" << exJson(r.explicitY);
    o << ",\"param_x\":" << exJson(r.paramX);
    o << ",\"param_y\":" << exJson(r.paramY);
    o << ",\"param_name\":" << q(r.paramName);
    o << ",\"param_singular\":" << (r.paramSingular ? "true" : "false");
    o << ",\"singular\":" << exArrayJson(r.singular);
    o << ",\"wronskian\":" << exJson(r.wronskian);
    o << ",\"independent\":" << (r.independent ? "true" : "false");
    o << ",\"envelope\":" << exArrayJson(r.envelope);
    o << ",\"orthogonal\":" << exJson(r.orthogonal);
    o << ",\"orthogonal_explicit\":" << (r.orthogonalExplicit ? "true" : "false");
    o << ",\"orthogonal_slope\":" << exJson(r.orthogonalSlope);
    o << ",\"trajectory\":" << exJson(r.trajectory);
    o << ",\"trajectory_explicit\":" << (r.trajectoryExplicit ? "true" : "false");
    o << ",\"trajectory_slope\":" << exJson(r.trajectorySlope);
    o << ",\"trajectory_angle\":" << numJson(r.trajectoryAngle);
    o << ",\"trajectory_mode\":" << q(r.trajectoryMode);
    o << ",\"family_branches\":" << exArrayJson(r.familyBranches);
    o << ",\"functions\":" << strArrayJson(r.functions);
    o << ",\"family\":" << q(r.family);

    // limits and continuity
    o << ",\"point\":" << q(r.point);
    o << ",\"side\":" << q(r.side);
    o << ",\"limit_exists\":" << (r.limitExists ? "true" : "false");
    o << ",\"limit_infinite\":" << (r.limitInfinite ? "true" : "false");
    o << ",\"limit_sign\":" << r.limitSign;
    o << ",\"left_exists\":" << (r.leftExists ? "true" : "false");
    o << ",\"right_exists\":" << (r.rightExists ? "true" : "false");
    o << ",\"left_infinite\":" << (r.leftInfinite ? "true" : "false");
    o << ",\"right_infinite\":" << (r.rightInfinite ? "true" : "false");
    o << ",\"left_sign\":" << r.leftSign;
    o << ",\"right_sign\":" << r.rightSign;
    o << ",\"left_value\":" << exJson(r.leftValue);
    o << ",\"right_value\":" << exJson(r.rightValue);
    o << ",\"value_defined\":" << (r.valueDefined ? "true" : "false");
    o << ",\"value_at\":" << exJson(r.valueAt);
    o << ",\"continuous\":" << (r.continuous ? "true" : "false");
    o << ",\"discontinuity\":" << q(r.discontinuity);
    o << ",\"laws\":" << strArrayJson(r.laws);

    // calculus extras
    o << ",\"dimension\":" << r.dimension;
    o << ",\"definite\":" << (r.definite ? "true" : "false");
    o << ",\"implicit\":" << (r.implicit ? "true" : "false");
    o << ",\"improper\":" << (r.improper ? "true" : "false");
    o << ",\"divergent\":" << (r.divergent ? "true" : "false");
    o << ",\"singularities\":" << strArrayJson(r.singularities);
    bool verified = r.ok && r.verified;
    for (const auto& c : r.checks) if (!c.ok) verified = false;
    o << ",\"verified\":" << (verified ? "true" : "false");
    o << ",\"has_numeric\":" << (r.hasNumeric ? "true" : "false");
    o << ",\"numeric_value\":" << std::setprecision(17)
      << (r.hasNumeric ? r.numericValue : 0.0);
    o << ",\"vars\":" << strArrayJson(r.vars);
    o << ",\"lower\":" << strArrayJson(r.lower);
    o << ",\"upper\":" << strArrayJson(r.upper);

    o << ",\"steps\":[";
    for (size_t i = 0; i < r.steps.size(); ++i) {
        if (i) o << ",";
        const auto& s = r.steps[i];
        o << "{\"title\":" << q(s.title) << ",\"note\":" << q(s.note)
          << ",\"math\":" << exJson(s.math) << "}";
    }
    o << "]";

    o << ",\"checks\":[";
    for (size_t i = 0; i < r.checks.size(); ++i) {
        if (i) o << ",";
        const auto& c = r.checks[i];
        o << "{\"name\":" << q(c.name) << ",\"ok\":"
          << (c.ok ? "true" : "false") << ",\"detail\":" << q(c.detail);
        if (!c.nameKey.empty()) o << ",\"name_key\":" << q(c.nameKey);
        if (!c.detailKey.empty()) o << ",\"detail_key\":" << q(c.detailKey);
        if (!c.params.empty()) o << ",\"params\":" << c.params;
        if (!c.detailMath.empty()) o << ",\"detail_math\":" << q(c.detailMath);
        o << "}";
    }
    o << "]";

    o << ",\"warnings\":" << strArrayJson(r.warnings);
    o << ",\"assumptions\":" << strArrayJson(r.assumptions);

    // applications of the derivative
    o << ",\"derivative\":" << exJson(r.derivative);
    o << ",\"second_derivative\":" << exJson(r.secondDerivative);
    o << ",\"value\":" << exJson(r.value);
    o << ",\"slope\":" << exJson(r.slope);
    o << ",\"tangent\":" << exJson(r.tangent);
    o << ",\"normal\":" << exJson(r.normal);
    o << ",\"tangent_slope\":" << exJson(r.tangentSlope);
    o << ",\"normal_slope\":" << exJson(r.normalSlope);
    o << ",\"value_a\":" << exJson(r.valueA);
    o << ",\"value_b\":" << exJson(r.valueB);
    o << ",\"maximum\":" << exJson(r.maximum);
    o << ",\"minimum\":" << exJson(r.minimum);
    o << ",\"maximum_at\":" << exJson(r.maximumAt);
    o << ",\"minimum_at\":" << exJson(r.minimumAt);
    o << ",\"function_f\":" << exJson(r.functionF);
    o << ",\"function_g\":" << exJson(r.functionG);
    o << ",\"hypotheses_continuous\":" << (r.hypothesesContinuous ? "true" : "false");
    o << ",\"hypotheses_differentiable\":"
      << (r.hypothesesDifferentiable ? "true" : "false");
    o << ",\"rolle_applies\":" << (r.rolleApplies ? "true" : "false");
    o << ",\"points\":" << exArrayJson(r.points);
    o << ",\"point_values\":" << exArrayJson(r.pointValues);
    o << ",\"function_value_a\":" << exJson(r.functionValueA);
    o << ",\"function_value_b\":" << exJson(r.functionValueB);

    auto pointsJson = [](const std::vector<ode::CriticalPointView>& pts) {
        std::string out = "[";
        for (size_t i = 0; i < pts.size(); ++i) {
            if (i) out += ",";
            out += "{\"x\":" + exJson(pts[i].x) + ",\"value\":" + exJson(pts[i].value) +
                   ",\"kind\":" + q(pts[i].kind) + "}";
        }
        out += "]";
        return out;
    };
    o << ",\"critical\":" << pointsJson(r.critical);
    o << ",\"candidates\":" << pointsJson(r.candidates);
    o << ",\"inflections\":" << pointsJson(r.inflections);

    o << ",\"pieces\":[";
    for (size_t i = 0; i < r.pieces.size(); ++i) {
        if (i) o << ",";
        const auto& p = r.pieces[i];
        o << "{\"lo\":" << numJson(p.lo) << ",\"hi\":" << numJson(p.hi)
          << ",\"lo_inf\":" << (p.loInfinite ? "true" : "false")
          << ",\"hi_inf\":" << (p.hiInfinite ? "true" : "false") << ",\"sign\":" << p.sign
          << ",\"trend\":" << q(p.trend) << "}";
    }
    o << "]";

    // asymptotes and curve sketching
    o << ",\"asymptotes\":" << strArrayJson(r.asymptotes);
    o << ",\"x_intercepts\":" << exArrayJson(r.xIntercepts);
    o << ",\"y_intercept\":" << exJson(r.yIntercept);
    o << ",\"has_y_intercept\":" << (r.hasYIntercept ? "true" : "false");
    o << ",\"sketch\":" << strArrayJson(r.sketch);

    // Taylor / Maclaurin
    o << ",\"polynomial\":" << exJson(r.polynomial);
    o << ",\"remainder\":" << exJson(r.remainder);
    o << ",\"coefficients\":" << exArrayJson(r.coefficients);

    // definite-integral applications
    o << ",\"integrand\":" << exJson(r.integrand);
    o << ",\"application\":" << q(r.application);

    // partial derivatives
    o << ",\"partials\":[";
    for (size_t i = 0; i < r.partials.size(); ++i) {
        if (i) o << ",";
        o << "{\"label\":" << q(r.partials[i].label)
          << ",\"value\":" << exJson(r.partials[i].value) << "}";
    }
    o << "]";
    o << ",\"gradient\":" << exArrayJson(r.gradient);
    o << ",\"laplacian\":" << exJson(r.laplacian);
    o << ",\"directional\":" << exJson(r.directional);
    o << ",\"function_value\":" << exJson(r.functionValue);
    o << ",\"point_label\":" << q(r.pointLabel);

    // series convergence
    o << ",\"series_verdict\":" << q(r.seriesVerdict);
    o << ",\"series_test\":" << q(r.seriesTest);
    o << ",\"converges\":" << (r.converges ? "true" : "false");
    o << ",\"series_sum\":" << exJson(r.seriesSum);
    o << ",\"partial_sum\":" << numJson(r.partialSum);
    o << ",\"partial_sums\":" << numArrayJson(r.partialSums);

    o << ",\"numeric_only\":" << (r.numericOnly ? "true" : "false");
    o << ",\"numeric_rhs\":" << q(r.numericRhs);
    o << ",\"numeric_note\":" << q(r.numericNote);

    o << "}";
    return o.str();
}

// True when two spellings differ only by whitespace.  The interpretation
// notice is only useful when the parser actually changed something.
bool sameIgnoringWhitespace(std::string a, std::string b) {
    auto strip = [](std::string& s) {
        s.erase(std::remove_if(s.begin(), s.end(),
                               [](unsigned char c) { return std::isspace(c); }),
                s.end());
    };
    strip(a);
    strip(b);
    return a == b;
}

ode::Result handleRaw(const JVal& req) {
    std::string mode = req.str("mode", "ode");
    if (mode == "parse") {
        ode::Result r;
        r.mode = "parse";
        r.typeId = "parse";
        r.variable = req.str("x", "x");
        try {
            ode::Ex parsed = ode::parseODEExpr(req.str("input"), r.variable, nullptr,
                                               {r.variable, "e", "pi"});
            r.normalized = parsed;
            r.solution = parsed;
            const std::string canonical = ode::toPlain(parsed);
            if (!sameIgnoringWhitespace(canonical, req.str("input")))
                r.interpretedAs = canonical;
            r.ok = true;
        } catch (const std::exception& e) {
            ode::setFailure(r, req.str("lang", "en") != "fa", e);
        }
        return r;
    }
    if (mode == "wronskian") {
        std::vector<std::string> fns;
        if (const JVal* arr = req.get("functions")) {
            if (arr->kind == JVal::Arr) {
                for (auto& v : arr->arr)
                    if (v.kind == JVal::Str) fns.push_back(v.s);
            } else if (arr->kind == JVal::Str) {
                fns.push_back(arr->s);
            }
        }
        return ode::solveWronskian(fns, req.str("x", "x"), req.str("lang", "en"));
    }
    if (mode == "integral") {
        auto strList = [&](const char* key) {
            std::vector<std::string> out;
            if (const JVal* a = req.get(key)) {
                if (a->kind == JVal::Arr)
                    for (auto& v : a->arr)
                        if (v.kind == JVal::Str) out.push_back(v.s);
                if (a->kind == JVal::Str) out.push_back(a->s);
            }
            return out;
        };
        std::vector<std::string> vars = strList("vars");
        if (vars.empty()) {
            int dim = 1;
            if (const JVal* d = req.get("dimension"))
                if (d->kind == JVal::Num) dim = (int)d->n;
            if (dim < 1) dim = 1;
            if (dim > 3) dim = 3;
            static const char* defaults[] = {"x", "y", "z"};
            for (int i = 0; i < dim; ++i) vars.push_back(defaults[i]);
        }
        std::vector<std::string> lower = strList("lower");
        std::vector<std::string> upper = strList("upper");
        if (lower.size() < vars.size()) lower.resize(vars.size(), "0");
        if (upper.size() < vars.size()) upper.resize(vars.size(), "1");
        return ode::integrateMulti(req.str("input"), vars, req.boolean("definite", false), lower,
                                   upper, req.str("lang", "en"));
    }
    if (mode == "limit") {
        return ode::limitExpression(req.str("input"), req.str("x", "x"), req.str("point", "0"),
                                    req.str("side", "both"), req.str("lang", "en"));
    }
    if (mode == "continuity") {
        return ode::continuityExpression(req.str("input"), req.str("x", "x"),
                                         req.str("point", "0"), req.str("lang", "en"));
    }
    if (mode == "derivative") {
        int order = 1;
        if (const JVal* v = req.get("order")) {
            if (v->kind == JVal::Num) order = (int)v->n;
        }
        return ode::derivativeExpression(req.str("input"), req.str("x", "x"), order,
                                         req.str("lang", "en"));
    }
    if (mode == "orthogonal") {
        return ode::solveOrthogonal(req.str("input"), req.str("c", "c"),
                                    req.str("x", "x"), req.str("y", "y"),
                                    req.str("lang", "en"));
    }
    if (mode == "oblique") {
        return ode::solveOblique(req.str("input"), req.str("c", "c"),
                                 numberArg(req, "angle", 45.0), req.str("x", "x"),
                                 req.str("y", "y"), req.str("lang", "en"));
    }
    if (mode == "envelope") {
        return ode::solveEnvelope(req.str("input"), req.str("c", "c"),
                                  req.boolean("orthogonal", false),
                                  req.str("x", "x"), req.str("y", "y"),
                                  req.str("lang", "en"));
    }
    // ------------------------------------------- calculus extensions
    if (mode == "taylor") {
        int order = 4;
        if (const JVal* v = req.get("order")) {
            if (v->kind == JVal::Num) order = (int)v->n;
        }
        return ode::taylorSeries(req.str("input"), req.str("x", "x"), numberArg(req, "a", 0.0),
                                 order, req.str("lang", "en"));
    }
    if (mode == "integral_apps") {
        return ode::integralApplication(req.str("kind", "area"), req.str("input"),
                                        req.str("g", ""), req.str("x", "x"),
                                        numberArg(req, "a", 0.0), numberArg(req, "b", 1.0),
                                        req.str("axis", "x"), req.str("lang", "en"));
    }
    if (mode == "partial") {
        auto textList = [&](const char* key) {
            std::vector<std::string> out;
            if (const JVal* v = req.get(key)) {
                if (v->kind == JVal::Str) {
                    std::string s = v->s;
                    for (char& ch : s) if (ch == ',') ch = ' ';
                    std::istringstream in(s);
                    std::string token;
                    while (in >> token) out.push_back(token);
                } else if (v->kind == JVal::Arr) {
                    for (auto& item : v->arr)
                        if (item.kind == JVal::Str) out.push_back(item.s);
                }
            }
            return out;
        };
        auto numberList = [&](const char* key) {
            std::vector<double> out;
            if (const JVal* v = req.get(key)) {
                if (v->kind == JVal::Str) {
                    std::string s = v->s;
                    for (char& ch : s) if (ch == ',') ch = ' ';
                    std::istringstream in(s);
                    double value = 0;
                    while (in >> value) out.push_back(value);
                } else if (v->kind == JVal::Arr) {
                    for (auto& item : v->arr)
                        if (item.kind == JVal::Num) out.push_back(item.n);
                }
            }
            return out;
        };
        return ode::partialDerivatives(req.str("input"), textList("vars"), numberList("point"),
                                       numberList("direction"), req.str("lang", "en"));
    }
    if (mode == "series") {
        int start = 1;
        if (const JVal* v = req.get("start")) {
            if (v->kind == JVal::Num) start = (int)v->n;
        }
        return ode::seriesAnalysis(req.str("input"), req.str("n", "n"), start,
                                   req.str("lang", "en"));
    }
    if (mode == "topic") {
        ode::TopicRequest topic;
        topic.topic = req.str("topic");
        topic.lang = req.str("lang", "en");
        for (const auto& item : req.obj) {
            if (item.second.kind == JVal::Num) topic.nums[item.first] = item.second.n;
            else if (item.second.kind == JVal::Str) topic.strs[item.first] = item.second.s;
        }
        return ode::solveTopic(topic);
    }

    // ------------------------------------------- applications of the derivative
    if (mode == "behaviour" || mode == "optimize") {
        const double lo = numberArg(req, "xmin", mode == "optimize" ? -5.0 : -10.0);
        const double hi = numberArg(req, "xmax", mode == "optimize" ? 5.0 : 10.0);
        if (mode == "optimize") {
            return ode::optimiseFunction(req.str("input"), req.str("x", "x"), lo, hi,
                                         req.str("lang", "en"));
        }
        return ode::analyseFunction(req.str("input"), req.str("x", "x"), lo, hi,
                                    req.str("lang", "en"));
    }
    if (mode == "tangent") {
        return ode::tangentAndNormal(req.str("input"), req.str("x", "x"), req.str("at", "1"),
                                     req.str("lang", "en"));
    }
    if (mode == "related_rates") {
        // A missing numeric field stays NaN, which keeps the answer symbolic.
        auto optionalNumber = [&](const char* key) -> double {
            if (const JVal* v = req.get(key)) {
                if (v->kind == JVal::Num && std::isfinite(v->n)) return v->n;
            }
            return std::numeric_limits<double>::quiet_NaN();
        };
        return ode::relatedRates(req.str("input"), req.str("var", "x"), req.str("rate_var", "y"),
                                 req.str("t", "t"), optionalNumber("q_value"),
                                 optionalNumber("w_value"), optionalNumber("given_rate"),
                                 req.str("lang", "en"));
    }
    if (mode == "mvt") {
        return ode::meanValueTheorems(req.str("input"), req.str("x", "x"),
                                      numberArg(req, "a", 0.0), numberArg(req, "b", 1.0),
                                      req.str("lang", "en"));
    }
    if (mode == "cauchy") {
        return ode::cauchyMeanValue(req.str("input"), req.str("g", "x^2"), req.str("x", "x"),
                                    numberArg(req, "a", 0.0), numberArg(req, "b", 1.0),
                                    req.str("lang", "en"));
    }
    if (mode == "lhopital") {
        return ode::lhopitalLimit(req.str("input"), req.str("g", "x"), req.str("x", "x"),
                                  req.str("point", "0"), req.str("lang", "en"));
    }
    ode::OdeOptions opt;
    opt.x = req.str("x", "x");
    opt.y = req.str("y", "y");
    opt.particular = req.str("particular");
    opt.lang = req.str("lang", "en");
    return ode::solveODE(req.str("input"), opt);
}

ode::Result handle(const JVal& req) {
    ode::Result r = handleRaw(req);
    const std::string mode = req.str("mode", "ode");
    if (mode == "parse") return r;
    std::string text;
    std::string indep = req.str("x", "x");
    if (mode == "taylor" || mode == "optimize" || mode == "behaviour" ||
        mode == "envelope" || mode == "orthogonal" || mode == "oblique" ||
        mode == "derivative" || mode == "integral" || mode == "integral_apps" ||
        mode == "limit" || mode == "continuity" || mode == "tangent" ||
        mode == "mvt" || mode == "cauchy" || mode == "lhopital" ||
        mode == "partial" || mode == "series") {
        text = req.str("input");
    } else if (mode == "topic") {
        text = req.str("f");
        if (text.empty()) text = req.str("F");
        indep = "x";
    } else {
        return r;
    }
    if (text.empty()) return r;
    try {
        ode::Ex parsed = (mode == "topic")
                             ? ode::parseExpr(text)
                             : ode::parseODEExpr(text, indep, nullptr, {indep, "e", "pi"});
        const std::string canonical = ode::toPlain(parsed);
        if (!sameIgnoringWhitespace(canonical, text)) r.interpretedAs = canonical;
    } catch (...) {
        // The solver already reported the parse failure; no notice to add.
    }
    return r;
}

std::string errorJson(const std::string& code, const std::string& detail = "") {
    std::string out = "{\"ok\":false,\"mode\":\"ode\"";
    if (!code.empty()) out += ",\"error_code\":" + q(code);
    if (!detail.empty()) out += ",\"error_detail\":" + q(detail);
    out += "}";
    return out;
}

} // namespace

extern "C" {

ODE_API const char* ode_core_version(void) { return "1.0.0"; }

ODE_API char* ode_solve(const char* request_json) {
    std::string out;
    try {
        if (!request_json) throw ode::EngineError("empty_request", "empty request");
        const std::string body(request_json);
        JsonParser parser(body);
        JVal req = parser.value();
        const std::string mode = req.str("mode", "ode");
        if (mode == "sample" || mode == "domain" || mode == "field" ||
            mode == "ode_trajectory" || mode == "surface") {
            out = graphResultJson(req, mode);
        } else {
            out = resultJson(handle(req));
        }
    } catch (const ode::EngineError& e) {
        out = errorJson(e.code.empty() ? "unavailable" : e.code, e.detail);
    } catch (const std::exception& e) {
        out = errorJson("bad_request", e.what());
    } catch (...) {
        out = errorJson("unavailable");
    }
    char* buf = (char*)std::malloc(out.size() + 1);
    if (!buf) return nullptr;
    std::memcpy(buf, out.c_str(), out.size() + 1);
    return buf;
}

ODE_API void ode_free(char* text) { std::free(text); }

} // extern "C"
