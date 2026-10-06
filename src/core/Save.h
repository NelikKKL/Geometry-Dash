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
    long long totalJumps = 0;     // shown on the Stats board
    long long totalAttempts = 0;
    std::array<int, 16> best{};  // best normal-mode progress per level, percent 0..100

    int bestOf(int level) const { return (level >= 0 && level < (int)best.size()) ? best[level] : 0; }
    bool levelCompleted(int level) const { return bestOf(level) >= 100; }
    int completedLevels(int levelCount) const {
        int n = 0;
        for (int i = 0; i < levelCount; ++i) n += levelCompleted(i);
        return n;
    }
    bool debugUnlockAll = false;  // never saved; toggled by the hidden U key in the garage

    // Records progress; returns true if it is a new best.
    bool recordBest(int level, int percent);

    bool load(const std::string& path);       // false if missing/corrupt (defaults kept)
    bool save(const std::string& path) const;
};

} // namespace ogd
