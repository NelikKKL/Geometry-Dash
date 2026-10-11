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

std::string colorAchievementId(int color) {
    if (color >= 4 && color <= 10) return levelAchievementId(color - 4, true);     // color_05..11 <- level 1..7 (practice)
    if (color == 11) return "geometry.ach.custom01";
    return "";
}

} // namespace ogd
