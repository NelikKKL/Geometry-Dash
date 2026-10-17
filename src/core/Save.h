// Persistent player data (replacement for the old GameManager). JSON on disk.
#pragma once
#include <array>
#include <string>
#include <vector>

namespace ogd {

struct SaveData {
    int cube = 1;               // 1..13
    int mainColor = 0;
    int secondaryColor = 3;
    std::string username = "Player";
    long long totalJumps = 0;     // shown on the Stats board
    long long totalAttempts = 0;
    std::array<int, 16> best{};  // best normal-mode progress per level, percent 0..100
    std::array<int, 16> practiceBest{};  // best practice-mode progress per level, percent 0..100

    // Settings (GameManager::toggleMusic / toggleFX in the original)
    bool musicOn = true;
    bool fxOn = true;
    bool autoCheck = true;   // PauseLayer::onAutoCheck: practice mode drops checkpoints on its own

    std::string editorLevel;      // legacy single editor level (migrated into customLevels on load)

    // "My Levels" (CreatorLayer > Create): the player's own levels. GJGameLevel in the original.
    struct CustomLevel {
        std::string name, desc;
        std::string data;         // serialized level string (see core/Level.h)
        int song = 0;             // official track id (kA1), 0 = Stereo Madness
        int version = 1;
    };
    std::vector<CustomLevel> customLevels;
    int editingLevel = -1;        // index in customLevels of the level open in EditLevelLayer / the editor (not saved)

    // First-run hints on the main menu ("Character Select", "Level Editor") disappear after the first visit.
    bool visitedGarage = false;
    bool visitedCreator = false;

    // Earned achievements (ids like "geometry.ach.level01a"); they unlock icons and colours (AchievementsDesc.plist).
    std::vector<std::string> achievements;
    bool hasAchievement(const std::string& id) const {
        for (auto& a : achievements) if (a == id) return true;
        return false;
    }
    bool earnAchievement(const std::string& id) {      // true when newly earned
        if (hasAchievement(id)) return false;
        achievements.push_back(id);
        return true;
    }

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
    bool recordPracticeBest(int level, int percent);
    int practiceBestOf(int level) const { return (level >= 0 && level < (int)practiceBest.size()) ? practiceBest[level] : 0; }

    bool load(const std::string& path);       // false if missing/corrupt (defaults kept)
    bool save(const std::string& path) const;
};

} // namespace ogd
