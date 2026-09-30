// Persistent player data (replacement for the old GameManager). JSON on disk.
#pragma once
#include <string>

namespace ogd {

struct SaveData {
    int cube = 1;               // 1..13
    int mainColor = 0;
    int secondaryColor = 3;
    std::string username = "Player";

    bool load(const std::string& path);       // false if missing/corrupt (defaults kept)
    bool save(const std::string& path) const;
};

} // namespace ogd
