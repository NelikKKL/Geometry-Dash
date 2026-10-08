// Metadata for the official levels shipped in the 1.x game binary (level_<kA1>.txt).
#pragma once
#include <cstdio>
#include <string>

#include "engine/Engine.h"

namespace ogd {

struct LevelMeta {
    const char* name;
    int difficulty;        // 1 easy (blue), 2 normal (green), 3 hard (yellow), 4 harder (orange), 5 insane (pink) -> diffIcon_0N_btn_001.png
    const char* track;     // music file
};

constexpr int kLevelCount = 7;

inline const LevelMeta& levelMeta(int i) {
    static const LevelMeta kMeta[kLevelCount] = {
        {"Stereo Madness", 1, "StereoMadness.mp3"}, {"Back On Track", 2, "BackOnTrack.mp3"},
        {"Polargeist", 3, "Polargeist.mp3"},        {"Dry Out", 4, "DryOut.mp3"},
        {"Base After Base", 4, "BaseAfterBase.mp3"}, {"Cant Let Go", 5, "CantLetGo.mp3"},
        {"Jumper", 5, "Jumper.mp3"}};
    return kMeta[i < 0 ? 0 : (i >= kLevelCount ? kLevelCount - 1 : i)];
}

// Background / ground colour of each page on the level-select screen (sampled from screenshots of the real game).
inline Color levelPageColor(int i) {
    static const Color k[kLevelCount] = {{0, 0, 255}, {255, 0, 255}, {255, 0, 125}, {255, 0, 0},
                                         {255, 125, 0}, {255, 255, 0}, {0, 255, 0}};
    return k[i < 0 ? 0 : (i >= kLevelCount ? kLevelCount - 1 : i)];
}

inline std::string levelFile(int i) {
    char b[32];
    std::snprintf(b, sizeof b, "level_%d.txt", i);
    return b;
}

} // namespace ogd
