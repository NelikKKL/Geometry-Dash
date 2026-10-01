#include "core/Level.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>

#include "core/Inflate.h"

namespace ogd {
namespace {

constexpr size_t kMaxStoredWarnings = 100;

struct Warn {
    LevelParseResult& r;
    void operator()(size_t obj, const std::string& msg) {
        ++r.warningCount;
        if (r.warnings.size() < kMaxStoredWarnings) r.warnings.push_back({obj, msg});
    }
};

bool parseInt(const char* b, const char* e, int& out) {
    if (b == e) return false;
    char tmp[24];
    size_t n = (size_t)(e - b);
    if (n >= sizeof tmp) return false;
    std::memcpy(tmp, b, n);
    tmp[n] = 0;
    char* end = nullptr;
    errno = 0;
    long v = std::strtol(tmp, &end, 10);
    if (end == tmp || *end || errno || v < INT_MIN || v > INT_MAX) return false;
    out = (int)v;
    return true;
}

bool parseFloat(const char* b, const char* e, float& out) {
    if (b == e) return false;
    char tmp[40];
    size_t n = (size_t)(e - b);
    if (n >= sizeof tmp) return false;
    std::memcpy(tmp, b, n);
    tmp[n] = 0;
    char* end = nullptr;
    double v = std::strtod(tmp, &end);
    if (end == tmp || *end) return false;
    out = (float)v;
    return true;
}

bool parseBool(const std::string& s) { return s == "1" || s == "true"; }

int clampByte(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

void applyHeader(LevelSettings& s) {
    auto get = [&](const char* k, int& v) {
        auto it = s.raw.find(k);
        if (it == s.raw.end()) return false;
        char* end = nullptr;
        long x = std::strtol(it->second.c_str(), &end, 10);
        if (end == it->second.c_str()) return false;
        v = (int)x;
        return true;
    };
    int r, g, b;
    if (get("kS1", r) && get("kS2", g) && get("kS3", b)) {
        s.hasBackground = true;
        s.background = {(uint8_t)clampByte(r), (uint8_t)clampByte(g), (uint8_t)clampByte(b)};
    }
    if (get("kS4", r) && get("kS5", g) && get("kS6", b)) {
        s.hasGround = true;
        s.ground = {(uint8_t)clampByte(r), (uint8_t)clampByte(g), (uint8_t)clampByte(b)};
    }
    int t;
    if (get("kA1", t)) s.musicTrack = t;
}

// Splits [b,e) on ',' into fields.
void splitFields(const char* b, const char* e, std::vector<std::pair<const char*, const char*>>& out) {
    out.clear();
    const char* s = b;
    for (const char* p = b; p <= e; ++p) {
        if (p == e || *p == ',') {
            out.emplace_back(s, p);
            s = p + 1;
        }
    }
}

} // namespace

double LevelObject::number(int key, double def) const {
    const std::string* v = find(key);
    if (!v) return def;
    char* end = nullptr;
    double d = std::strtod(v->c_str(), &end);
    return end == v->c_str() ? def : d;
}

int LevelSettings::intValue(const std::string& key, int def) const {
    auto it = raw.find(key);
    if (it == raw.end()) return def;
    char* end = nullptr;
    long v = std::strtol(it->second.c_str(), &end, 10);
    return end == it->second.c_str() ? def : (int)v;
}

std::pair<size_t, size_t> Level::range(float x0, float x1) const {
    auto lo = std::lower_bound(objects.begin(), objects.end(), x0, [](const LevelObject& o, float v) { return o.x < v; });
    auto hi = std::upper_bound(objects.begin(), objects.end(), x1, [](float v, const LevelObject& o) { return v < o.x; });
    if (hi < lo) hi = lo;
    return {(size_t)(lo - objects.begin()), (size_t)(hi - objects.begin())};
}

LevelParseResult parseLevel(const std::string& text) {
    LevelParseResult res;
    Warn warn{res};
    const char* data = text.data();
    const char* end = data + text.size();

    // trim whitespace / trailing NULs
    while (data < end && (*data == ' ' || *data == '\n' || *data == '\r' || *data == '\t')) ++data;
    while (end > data && (end[-1] == ' ' || end[-1] == '\n' || end[-1] == '\r' || end[-1] == '\t' || end[-1] == '\0')) --end;
    if (data == end) { res.error = "empty level data"; return res; }

    std::vector<std::pair<const char*, const char*>> f;
    size_t objIndex = 0;
    bool first = true;
    const char* seg = data;
    while (seg <= end) {
        const char* semi = (const char*)std::memchr(seg, ';', (size_t)(end - seg));
        const char* segEnd = semi ? semi : end;

        if (segEnd > seg) {
            // header? first segment whose first key starts with 'k'
            if (first && *seg == 'k') {
                splitFields(seg, segEnd, f);
                if (f.size() % 2) warn(SIZE_MAX, "header has an odd number of fields; last one ignored");
                for (size_t i = 0; i + 1 < f.size(); i += 2)
                    res.level.settings.raw[std::string(f[i].first, f[i].second)] = std::string(f[i + 1].first, f[i + 1].second);
                applyHeader(res.level.settings);
            } else {
                splitFields(seg, segEnd, f);
                if (f.size() % 2) warn(objIndex, "odd number of fields; last one ignored");
                LevelObject o;
                bool hasId = false, hasX = false, hasY = false, bad = false;
                for (size_t i = 0; i + 1 < f.size(); i += 2) {
                    int key;
                    if (!parseInt(f[i].first, f[i].second, key)) {
                        warn(objIndex, "non-numeric key '" + std::string(f[i].first, f[i].second) + "'");
                        continue;
                    }
                    const char *vb = f[i + 1].first, *ve = f[i + 1].second;
                    switch (key) {
                    case 1: hasId = parseInt(vb, ve, o.id); if (!hasId) bad = true; break;
                    case 2: hasX = parseFloat(vb, ve, o.x); if (!hasX) bad = true; break;
                    case 3: hasY = parseFloat(vb, ve, o.y); if (!hasY) bad = true; break;
                    case 4: o.flipX = parseBool(std::string(vb, ve)); break;
                    case 5: o.flipY = parseBool(std::string(vb, ve)); break;
                    case 6: if (!parseFloat(vb, ve, o.rotation)) warn(objIndex, "bad rotation value"); break;
                    default: o.extra.emplace_back(key, std::string(vb, ve)); break;
                    }
                }
                if (hasId && hasX && hasY && !bad) {
                    res.level.objects.push_back(std::move(o));
                } else {
                    ++res.skippedObjects;
                    warn(objIndex, bad ? "invalid id/x/y value; object skipped" : "object is missing id, x or y; skipped");
                }
                ++objIndex;
            }
        }
        first = false;
        if (!semi) break;
        seg = semi + 1;
    }

    if (res.level.objects.empty() && res.level.settings.raw.empty()) {
        res.error = "no objects or header found";
        return res;
    }
    std::stable_sort(res.level.objects.begin(), res.level.objects.end(),
                     [](const LevelObject& a, const LevelObject& b) { return a.x < b.x; });
    res.ok = true;
    return res;
}

bool decodeLevelString(const std::string& s, std::string& text, std::string* err) {
    // plain text: starts with a header key ("kS1,..", "kA..") or an object ("<digits>,")
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i;
    if (i >= s.size()) { if (err) *err = "empty input"; return false; }
    bool plain = (s[i] == 'k');
    if (!plain) {
        size_t j = i;
        if (j < s.size() && s[j] == '-') ++j;
        size_t d = j;
        while (j < s.size() && s[j] >= '0' && s[j] <= '9') ++j;
        plain = (j > d && j < s.size() && s[j] == ',');
    }
    if (plain) { text = s; return true; }

    std::vector<uint8_t> raw;
    if (!base64Decode(s, raw)) { if (err) *err = "not plain level text and not valid base64"; return false; }
    std::vector<uint8_t> out;
    if (!decompressAuto(raw.data(), raw.size(), out)) { if (err) *err = "corrupt or unsupported compressed level data"; return false; }
    text.assign(out.begin(), out.end());
    return true;
}

LevelParseResult parseLevelString(const std::string& s) {
    std::string text, err;
    if (!decodeLevelString(s, text, &err)) {
        LevelParseResult r;
        r.error = err;
        return r;
    }
    return parseLevel(text);
}

} // namespace ogd
