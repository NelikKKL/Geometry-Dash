// Geometry Dash level data parser (no SDL dependency).
//
// Format (as found in the 1.x game binary and in saved levels):
//
//   [header ;] object ; object ; ...
//   header : "kS1,40,kS2,62,kS3,255,kS4,0,kS5,19,kS6,200,kA1,0"   (string key , value , ...)
//   object : "1,8,2,525,3,15,6,-90"                                 (integer key , value , ...)
//
// Header keys seen in 1.x data: kS1..kS3 = background RGB, kS4..kS6 = ground RGB, kA1 = music track.
// Object keys: 1 id, 2 x, 3 y, 4 flipX, 5 flipY, 6 rotation (degrees). Colour triggers (ids 29/30) also carry
// 7/8/9 = RGB and 10 = fade duration. Coordinates are in GD units (a block is 30 units).
//
// Level strings may also be stored as base64(gzip(text)) ("H4sI..."); decodeLevelString() handles both.
#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ogd {

struct LevelColor {
    uint8_t r = 0, g = 0, b = 0;
};

constexpr int kObjColorTriggerBG = 29;      // the only ids that carry keys 7-10 in the 1.x data
constexpr int kObjColorTriggerGround = 30;

struct LevelObject {
    int id = 0;
    float x = 0, y = 0;
    float rotation = 0;
    bool flipX = false, flipY = false;
    std::vector<std::pair<int, std::string>> extra;  // every key except 1-6, in file order

    const std::string* find(int key) const {
        for (auto& kv : extra)
            if (kv.first == key) return &kv.second;
        return nullptr;
    }
    bool has(int key) const { return find(key) != nullptr; }
    double number(int key, double def = 0) const;
};

struct LevelSettings {
    std::map<std::string, std::string> raw;  // header key -> value
    bool hasBackground = false, hasGround = false;
    LevelColor background, ground;
    int musicTrack = -1;                     // kA1, -1 if absent

    int intValue(const std::string& key, int def) const;
};

struct LevelWarning {
    size_t object;       // 0-based index in file order (header not counted); SIZE_MAX = header
    std::string message;
};

struct Level {
    LevelSettings settings;
    std::vector<LevelObject> objects;  // stable-sorted by x (file order kept for equal x)

    bool empty() const { return objects.empty(); }
    float maxX() const { return objects.empty() ? 0.f : objects.back().x; }
    // Index range [first, last) of objects with x0 <= x <= x1 (binary search).
    std::pair<size_t, size_t> range(float x0, float x1) const;
};

struct LevelParseResult {
    bool ok = false;
    Level level;
    std::string error;                    // set when !ok
    std::vector<LevelWarning> warnings;   // first 100 only
    size_t warningCount = 0;              // total, including those not stored
    size_t skippedObjects = 0;
};

// Parses already-decoded level text. Lenient: malformed objects are skipped with a warning.
LevelParseResult parseLevel(const std::string& text);

// Auto-detects plain text vs base64(gzip|zlib|raw deflate), decodes, then parses.
LevelParseResult parseLevelString(const std::string& s);

// Returns the decoded text, or false (with err) if the data is neither plain level text nor valid base64+deflate.
bool decodeLevelString(const std::string& s, std::string& text, std::string* err = nullptr);

} // namespace ogd
