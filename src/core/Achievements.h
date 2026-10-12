// Achievements from the APK (assets/AchievementsDesc.plist). Each one rewards an icon or a colour:
//   icon_05 .. icon_13   cube icons 5..13
//   color_05 .. color_12 main colours 5..12 (0-based index 4..11)
//   color2_xx            secondary colours (jumps / attempts / online features, not available here)
#pragma once
#include <string>
#include <vector>

#include "core/Save.h"

namespace ogd {

struct Achievement {
    std::string id, title, icon, achievedDesc, unachievedDesc;
    int order = 0;
};

// Parses AchievementsDesc.plist text once (call again to reload); returns false if it is not a valid plist.
bool loadAchievements(const std::string& plistText);
const std::vector<Achievement>& allAchievements();
const Achievement* findAchievement(const std::string& id);

// "geometry.ach.level03a" (a = practice mode, b = normal mode) for a 0-based level index
std::string levelAchievementId(int level, bool practice);
// Reward kind of an achievement: "Unlocked new color!" / "Unlocked new icon!"
std::string rewardText(const Achievement& a);

// Achievement that unlocks cube icon `icon` (1-based) / main colour `color` (0-based); empty = free or not obtainable
std::string iconAchievementId(int icon);
std::string colorAchievementId(int color, bool secondary = false);

// Counters that earn achievements on their own: jump01/02 (1000 / 10000 jumps), attempt01/02 (100 / 500 attempts).
// Returns the ids that were newly earned by this call.
std::vector<std::string> earnProgressAchievements(SaveData& save);

} // namespace ogd
