// Shared drawing helpers: tinted background, ground, cube.
#pragma once
#include "core/PlayerPhysics.h"
#include "engine/Engine.h"

namespace ogd {

constexpr Color kBlue{0, 102, 255};

void drawBackground(float scrollX, Color tint = kBlue);
void drawGround(float scrollX, Color tint = kBlue);
void drawCube(int cube, const PlayerState& p, float camX, Color main, Color secondary);

Color randomColor();
Color paletteColor(int index);

} // namespace ogd
