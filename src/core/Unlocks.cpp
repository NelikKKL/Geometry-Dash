#include "core/Unlocks.h"

namespace ogd {

UnlockRule iconUnlockRule(int icon) {
    if (icon <= 4) return {UnlockKind::Free, 0};
    if (icon <= 11) return {UnlockKind::CompleteLevel, icon - 5};    // icons 5..11 <- levels 0..6
    if (icon == 12) return {UnlockKind::CompleteCount, 4};
    return {UnlockKind::CompleteAll, 0};
}

UnlockRule colorUnlockRule(int color) {
    if (color <= 3) return {UnlockKind::Free, 0};
    if (color <= 10) return {UnlockKind::CompleteLevel, color - 4};  // colours 4..10 <- levels 0..6
    return {UnlockKind::CompleteAll, 0};
}

bool isUnlocked(const UnlockRule& rule, const SaveData& save, int levelCount) {
    if (save.debugUnlockAll) return true;
    switch (rule.kind) {
    case UnlockKind::Free: return true;
    case UnlockKind::CompleteLevel: return save.levelCompleted(rule.arg);
    case UnlockKind::CompleteCount: return save.completedLevels(levelCount) >= rule.arg;
    case UnlockKind::CompleteAll: return save.completedLevels(levelCount) >= levelCount;
    }
    return false;
}

} // namespace ogd
