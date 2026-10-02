// Metadata for the official levels shipped in the 1.x game binary (level_<kA1>.txt).
#pragma once
#include <cstdio>
#include <string>

namespace ogd {

struct LevelMeta {
    const char* name;
    int difficulty;        // 1 easy, 2 normal, 3 hard, 4 harder, 5 insane -> difficulty_0N_btn_001.png
    const char* track;     // music file
};

constexpr int kLevelCount = 7;

inline const LevelMeta& levelMeta(int i) {
    static const LevelMeta kMeta[kLevelCount] = {
        {"Stereo Madness", 1, "StereoMadness.mp3"}, {"Back On Track", 1, "BackOnTrack.mp3"},
        {"Polargeist", 2, "Polargeist.mp3"},        {"Dry Out", 2, "DryOut.mp3"},
        {"Base After Base", 3, "BaseAfterBase.mp3"}, {"Can't Let Go", 3, "CantLetGo.mp3"},
        {"Jumper", 4, "Jumper.mp3"}};
    return kMeta[i < 0 ? 0 : (i >= kLevelCount ? kLevelCount - 1 : i)];
}

inline std::string levelFile(int i) {
    char b[32];
    std::snprintf(b, sizeof b, "level_%d.txt", i);
    return b;
}

} // namespace ogd
