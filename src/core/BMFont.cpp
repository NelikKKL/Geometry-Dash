#include "core/BMFont.h"
#include <cstdlib>
#include <sstream>

namespace ogd {
namespace {

using KV = std::unordered_map<std::string, std::string>;

KV kv(const std::string& line) {
    KV m;
    size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && line[i] == ' ') ++i;
        size_t eq = line.find('=', i);
        if (eq == std::string::npos) break;
        std::string key = line.substr(i, eq - i);
        i = eq + 1;
        std::string val;
        if (i < line.size() && line[i] == '"') {
            size_t e = line.find('"', i + 1);
            if (e == std::string::npos) e = line.size();
            val = line.substr(i + 1, e - i - 1);
            i = e + 1;
        } else {
            size_t e = line.find(' ', i);
            if (e == std::string::npos) e = line.size();
            val = line.substr(i, e - i);
            i = e;
        }
        m[key] = val;
    }
    return m;
}

int I(const KV& m, const char* k) {
    auto it = m.find(k);
    return it == m.end() ? 0 : std::atoi(it->second.c_str());
}

} // namespace

BMFont parseBMFont(const std::string& text) {
    BMFont f;
    std::istringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.compare(0, 5, "char ") == 0) {
            KV m = kv(line.substr(5));
            BMChar c;
            int id = I(m, "id");
            c.x = I(m, "x"); c.y = I(m, "y"); c.w = I(m, "width"); c.h = I(m, "height");
            c.xoff = I(m, "xoffset"); c.yoff = I(m, "yoffset"); c.xadv = I(m, "xadvance");
            f.chars[id] = c;
        } else if (line.compare(0, 8, "kerning ") == 0) {
            KV m = kv(line.substr(8));
            f.kerning[((long long)I(m, "first") << 32) | (unsigned)I(m, "second")] = I(m, "amount");
        } else if (line.compare(0, 7, "common ") == 0) {
            KV m = kv(line.substr(7));
            f.lineHeight = I(m, "lineHeight");
            f.base = I(m, "base");
        } else if (line.compare(0, 5, "page ") == 0) {
            KV m = kv(line.substr(5));
            if (f.pageFile.empty()) f.pageFile = m["file"];
        }
    }
    f.ok = !f.chars.empty() && !f.pageFile.empty();
    return f;
}

float BMFont::measure(const std::string& text) const {
    float w = 0;
    int prev = 0;
    for (unsigned char ch : text) {
        auto it = chars.find(ch);
        if (it == chars.end()) continue;
        w += it->second.xadv + (prev ? kern(prev, ch) : 0);
        prev = ch;
    }
    return w;
}

} // namespace ogd
