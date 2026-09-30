// AngelCode BMFont (text format) parser. No SDL dependency.
#pragma once
#include <string>
#include <unordered_map>

namespace ogd {

struct BMChar {
    int x = 0, y = 0, w = 0, h = 0;
    int xoff = 0, yoff = 0, xadv = 0;
};

struct BMFont {
    std::string pageFile;
    int lineHeight = 0;
    int base = 0;
    std::unordered_map<int, BMChar> chars;
    std::unordered_map<long long, int> kerning; // (first<<32)|second -> amount
    bool ok = false;

    int kern(int a, int b) const {
        auto it = kerning.find(((long long)a << 32) | (unsigned)b);
        return it == kerning.end() ? 0 : it->second;
    }
    // Width of a single line in font units (unscaled).
    float measure(const std::string& text) const;
};

BMFont parseBMFont(const std::string& text);

} // namespace ogd
