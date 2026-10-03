// Player colour palette of GD 1.x (12 entries; main and secondary colours pick from the same list).
#pragma once
#include <cstdint>

namespace ogd {

constexpr int kPaletteSize = 12;

struct Rgb {
    uint8_t r, g, b;
};

inline Rgb paletteRgb(int index) {
    static const Rgb kPalette[kPaletteSize] = {
        {125, 255, 0}, {0, 255, 0},  {0, 255, 125}, {0, 255, 255}, {0, 125, 255}, {0, 0, 255},
        {125, 0, 255}, {255, 0, 255}, {255, 0, 125}, {255, 0, 0},   {255, 125, 0}, {255, 255, 0}};
    index %= kPaletteSize;
    if (index < 0) index += kPaletteSize;
    return kPalette[index];
}

} // namespace ogd
