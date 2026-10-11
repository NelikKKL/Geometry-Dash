#include "core/Unlocks.h"

#include "core/Achievements.h"

namespace ogd {

UnlockRule iconUnlockRule(int icon) {
    const std::string id = iconAchievementId(icon);
    if (id.empty()) return {UnlockKind::Free, 0, ""};
    return {UnlockKind::Achievement, 0, id};
}

UnlockRule colorUnlockRule(int color) {
    const std::string id = colorAchievementId(color);
    if (id.empty()) return {UnlockKind::Free, 0, ""};
    return {UnlockKind::Achievement, 0, id};
}

bool isUnlocked(const UnlockRule& rule, const SaveData& save, int levelCount) {
    if (save.debugUnlockAll) return true;
    switch (rule.kind) {
    case UnlockKind::Free: return true;
    case UnlockKind::CompleteLevel: return save.levelCompleted(rule.arg);
    case UnlockKind::CompleteCount: return save.completedLevels(levelCount) >= rule.arg;
    case UnlockKind::CompleteAll: return save.completedLevels(levelCount) >= levelCount;
    case UnlockKind::Achievement: return save.hasAchievement(rule.ach);
    }
    return false;
}

} // namespace ogd
