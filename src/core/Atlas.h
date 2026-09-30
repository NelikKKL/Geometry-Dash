// Sprite-sheet (TexturePacker / cocos2d plist) parsing. No SDL dependency.
#pragma once
#include <string>
#include <vector>

namespace ogd {

struct SheetFrame {
    std::string name;
    float x = 0, y = 0;        // top-left in texture
    float w = 0, h = 0;        // *unrotated* sprite size (footprint in texture is h x w when rotated)
    bool rotated = false;      // stored rotated 90deg clockwise in the atlas
    float offX = 0, offY = 0;  // trimmed-rect centre offset from original centre (y-up)
    float srcW = 0, srcH = 0;  // original (untrimmed) size
};

struct Sheet {
    std::string textureFile;   // e.g. "GJ_LaunchSheet-hd.png"
    std::vector<SheetFrame> frames;
    bool ok = false;
};

Sheet parseSheet(const std::string& plistXml);

} // namespace ogd
