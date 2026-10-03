// What the player must do to unlock cube icons and colours (a design decision of this project: the original
// ties them to achievements, which do not exist here yet). Pure data + one predicate, no SDL.
#pragma once
#include "core/Save.h"

namespace ogd {

constexpr int kIconCount = 13;   // cube icons 1..13

enum class UnlockKind { Free, CompleteLevel, CompleteCount, CompleteAll };

struct UnlockRule {
    UnlockKind kind = UnlockKind::Free;
    int arg = 0;                 // level index for CompleteLevel, number of levels for CompleteCount
};

// icon: 1..kIconCount, colour: 0..kPaletteSize-1
UnlockRule iconUnlockRule(int icon);
UnlockRule colorUnlockRule(int color);

// levelCount = number of official levels
bool isUnlocked(const UnlockRule& rule, const SaveData& save, int levelCount);

} // namespace ogd
