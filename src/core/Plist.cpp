#include "core/Plist.h"
#include <cstdlib>

namespace ogd {
namespace {

struct Cur {
    const std::string& s;
    size_t i = 0;
};

std::string unescape(const std::string& t) {
    if (t.find('&') == std::string::npos) return t;
    std::string o;
    for (size_t i = 0; i < t.size(); ++i) {
        if (t[i] == '&') {
            bool done = false;
            struct E { const char* ent; char c; };
            static const E table[] = {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};
            for (auto& e : table) {
                size_t n = std::string(e.ent).size();
                if (t.compare(i, n, e.ent) == 0) { o += e.c; i += n - 1; done = true; break; }
            }
            if (done) continue;
        }
        o += t[i];
    }
    return o;
}

bool readTag(Cur& c, std::string& name, bool& closing, bool& self) {
    for (;;) {
        size_t p = c.s.find('<', c.i);
        if (p == std::string::npos) return false;
        if (c.s.compare(p, 4, "<!--") == 0) {
            size_t e = c.s.find("-->", p);
            if (e == std::string::npos) return false;
            c.i = e + 3;
            continue;
        }
        if (p + 1 < c.s.size() && (c.s[p + 1] == '?' || c.s[p + 1] == '!')) {
            size_t e = c.s.find('>', p);
            if (e == std::string::npos) return false;
            c.i = e + 1;
            continue;
        }
        size_t e = c.s.find('>', p);
        if (e == std::string::npos) return false;
        std::string inner = c.s.substr(p + 1, e - p - 1);
        closing = !inner.empty() && inner[0] == '/';
        if (closing) inner.erase(0, 1);
        self = !inner.empty() && inner.back() == '/';
        if (self) inner.pop_back();
        size_t sp = inner.find_first_of(" \t\r\n");
        if (sp != std::string::npos) inner.resize(sp);
        name = inner;
        c.i = e + 1;
        return true;
    }
}

std::string readText(Cur& c) {
    size_t e = c.s.find('<', c.i);
    if (e == std::string::npos) e = c.s.size();
    std::string t = c.s.substr(c.i, e - c.i);
    c.i = e;
    return unescape(t);
}

bool parseValue(Cur& c, const std::string& tag, bool self, PValue& out) {
    std::string n; bool cl = false, sf = false;
    if (tag == "dict") {
        out.type = PValue::Dict;
        if (self) return true;
        for (;;) {
            if (!readTag(c, n, cl, sf)) return false;
            if (cl && n == "dict") return true;
            if (n != "key") return false;
            std::string key = sf ? "" : readText(c);
            if (!sf && !(readTag(c, n, cl, sf) && cl && n == "key")) return false;
            std::string vt; bool vcl = false, vself = false;
            if (!readTag(c, vt, vcl, vself) || vcl) return false;
            PValue v;
            if (!parseValue(c, vt, vself, v)) return false;
            out.dict.emplace_back(std::move(key), std::move(v));
        }
    }
    if (tag == "array") {
        out.type = PValue::Array;
        if (self) return true;
        for (;;) {
            std::string vt; bool vcl = false, vself = false;
            if (!readTag(c, vt, vcl, vself)) return false;
            if (vcl && vt == "array") return true;
            if (vcl) return false;
            PValue v;
            if (!parseValue(c, vt, vself, v)) return false;
            out.arr.push_back(std::move(v));
        }
    }
    if (tag == "true" || tag == "false") {
        out.type = PValue::Bool;
        out.b = (tag == "true");
        return true;
    }
    if (tag == "string" || tag == "integer" || tag == "real" || tag == "data" || tag == "date") {
        std::string text = self ? "" : readText(c);
        if (!self && !(readTag(c, n, cl, sf) && cl && n == tag)) return false;
        if (tag == "integer") { out.type = PValue::Int; out.n = std::strtod(text.c_str(), nullptr); }
        else if (tag == "real") { out.type = PValue::Real; out.n = std::strtod(text.c_str(), nullptr); }
        else { out.type = PValue::String; out.s = text; }
        return true;
    }
    return false;
}

} // namespace

PValue parsePlist(const std::string& xml) {
    Cur c{xml};
    std::string n; bool cl = false, sf = false;
    bool found = false;
    while (readTag(c, n, cl, sf)) {
        if (n == "plist" && !cl) { found = true; break; }
    }
    if (!found) return PValue{};
    std::string vt; bool vcl = false, vself = false;
    if (!readTag(c, vt, vcl, vself) || vcl) return PValue{};
    PValue v;
    if (!parseValue(c, vt, vself, v)) return PValue{};
    return v;
}

std::vector<double> parseNumbers(const std::string& s) {
    std::vector<double> r;
    const char* p = s.c_str();
    while (*p) {
        if ((*p >= '0' && *p <= '9') || *p == '-' || *p == '.' || *p == '+') {
            char* end = nullptr;
            double v = std::strtod(p, &end);
            if (end != p) { r.push_back(v); p = end; continue; }
        }
        ++p;
    }
    return r;
}

} // namespace ogd
