// ODE Lab — JSON API on top of the native solver.
// SPDX-License-Identifier: MIT
#include "odecore.h"
#include "solver.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    o << ",\"functions\":" << strArrayJson(r.functions);
    o << ",\"family\":" << q(r.family);

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
        o << "{\"name\":" << q(r.checks[i].name) << ",\"ok\":"
          << (r.checks[i].ok ? "true" : "false") << ",\"detail\":" << q(r.checks[i].detail) << "}";
    }
    o << "]";

    o << ",\"warnings\":" << strArrayJson(r.warnings);
    o << ",\"assumptions\":" << strArrayJson(r.assumptions);
    o << "}";
    return o.str();
}

ode::Result handle(const std::string& request) {
    JsonParser parser(request);
    JVal req = parser.value();
    std::string mode = req.str("mode", "ode");
    if (mode == "wronskian") {
        std::vector<std::string> fns;
        if (const JVal* arr = req.get("functions")) {
            if (arr->kind == JVal::Arr)
                for (auto& v : arr->arr) if (v.kind == JVal::Str) fns.push_back(v.s);
            else if (arr->kind == JVal::Str)
                fns.push_back(arr->s);
        }
        return ode::solveWronskian(fns, req.str("x", "x"), req.str("lang", "en"));
    }
    if (mode == "integral") {
        return ode::integrateExpression(req.str("input"), req.str("x", "x"), req.str("lang", "en"));
    }
    if (mode == "derivative") {
        int order = 1;
        if (const JVal* v = req.get("order")) {
            if (v->kind == JVal::Num) order = (int)v->n;
        }
        return ode::derivativeExpression(req.str("input"), req.str("x", "x"), order,
                                         req.str("lang", "en"));
    }
    if (mode == "envelope" || mode == "orthogonal") {
        return ode::solveEnvelope(req.str("input"), req.str("c", "c"),
                                  req.boolean("orthogonal", mode == "orthogonal"),
                                  req.str("x", "x"), req.str("y", "y"),
                                  req.str("lang", "en"));
    }
    ode::OdeOptions opt;
    opt.x = req.str("x", "x");
    opt.y = req.str("y", "y");
    opt.particular = req.str("particular");
    opt.lang = req.str("lang", "en");
    return ode::solveODE(req.str("input"), opt);
}

std::string errorJson(const std::string& message) {
    return std::string("{\"ok\":false,\"error\":") + q(message) + ",\"mode\":\"ode\"}";
}

} // namespace

extern "C" {

ODE_API const char* ode_core_version(void) { return "0.1.0"; }

ODE_API char* ode_solve(const char* request_json) {
    std::string out;
    try {
        if (!request_json) throw std::runtime_error("empty request");
        out = resultJson(handle(request_json));
    } catch (const std::exception& e) {
        out = errorJson(std::string("درخواست نامعتبر است: ") + e.what());
    } catch (...) {
        out = errorJson("خطای ناشناخته در موتور محاسبات");
    }
    char* buf = (char*)std::malloc(out.size() + 1);
    if (!buf) return nullptr;
    std::memcpy(buf, out.c_str(), out.size() + 1);
    return buf;
}

ODE_API void ode_free(char* text) { std::free(text); }

} // extern "C"
