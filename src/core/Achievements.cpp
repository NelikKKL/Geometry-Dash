#include "core/Achievements.h"

#include <cstdio>

#include "core/Plist.h"

namespace ogd {
namespace {
std::vector<Achievement> g_all;
}

bool loadAchievements(const std::string& text) {
    PValue v = parsePlist(text);
    if (!v.isDict()) return false;
    g_all.clear();
    for (auto& kv : v.dict) {
        if (!kv.second.isDict()) continue;
        Achievement a;
        a.id = kv.first;
        const PValue& d = kv.second;
        if (auto* p = d.get("title")) a.title = p->str();
        if (auto* p = d.get("icon")) a.icon = p->str();
        if (auto* p = d.get("achievedDescription")) a.achievedDesc = p->str();
        if (auto* p = d.get("unachievedDescription")) a.unachievedDesc = p->str();
        if (auto* p = d.get("order")) a.order = (int)p->num();
        g_all.push_back(std::move(a));
    }
    return !g_all.empty();
}

const std::vector<Achievement>& allAchievements() { return g_all; }

const Achievement* findAchievement(const std::string& id) {
    for (auto& a : g_all) if (a.id == id) return &a;
    return nullptr;
}

std::string levelAchievementId(int level, bool practice) {
    char b[48];
    std::snprintf(b, sizeof b, "geometry.ach.level%02d%c", level + 1, practice ? 'a' : 'b');
    return b;
}

std::string rewardText(const Achievement& a) {
    if (a.icon.rfind("icon_", 0) == 0) return "Unlocked new icon!";
    return "Unlocked new color!";
}

std::string iconAchievementId(int icon) {
    if (icon >= 5 && icon <= 11) return levelAchievementId(icon - 5, false);      // icon_05..11 <- level 1..7 (normal)
    if (icon == 12) return "geometry.ach.custom02";
    if (icon == 13) return "geometry.ach.rate";
    return "";
}

std::string colorAchievementId(int color, bool secondary) {
    if (!secondary) {
        if (color >= 4 && color <= 10) return levelAchievementId(color - 4, true);     // color_05..11 <- level 1..7 (practice)
        if (color == 11) return "geometry.ach.custom01";                               // color_12: a user created level
        return "";
    }
    switch (color) {                                                                   // color2_xx
    case 4: return "geometry.ach.moreGames";   // color2_5
    case 5: return "geometry.ach.jump01";      // color2_06
    case 6: return "geometry.ach.attempt01";   // color2_07
    case 7: return "geometry.ach.attempt02";   // color2_08
    case 8: return "geometry.ach.like";        // color2_09
    case 9: return "geometry.ach.rateDiff";    // color2_10
    case 10: return "geometry.ach.submit";     // color2_11
    case 11: return "geometry.ach.jump02";     // color2_12
    default: return "";
    }
}

std::vector<std::string> earnProgressAchievements(SaveData& save) {
    std::vector<std::string> got;
    auto check = [&](bool cond, const char* id) { if (cond && save.earnAchievement(id)) got.push_back(id); };
    check(save.totalJumps >= 1000, "geometry.ach.jump01");
    check(save.totalJumps >= 10000, "geometry.ach.jump02");
    check(save.totalAttempts >= 100, "geometry.ach.attempt01");
    check(save.totalAttempts >= 500, "geometry.ach.attempt02");
    return got;
}

} // namespace ogd
