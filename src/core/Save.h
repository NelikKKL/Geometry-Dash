// Persistent player data (replacement for the old GameManager). JSON on disk.
#pragma once
#include <array>
#include <string>

namespace ogd {

struct SaveData {
    int cube = 1;               // 1..13
    int mainColor = 0;
    int secondaryColor = 3;
    std::string username = "Player";
    std::array<int, 16> best{};  // best normal-mode progress per level, percent 0..100

    int bestOf(int level) const { return (level >= 0 && level < (int)best.size()) ? best[level] : 0; }
    // Records progress; returns true if it is a new best.
    bool recordBest(int level, int percent);

    bool load(const std::string& path);       // false if missing/corrupt (defaults kept)
    bool save(const std::string& path) const;
};

} // namespace ogd
