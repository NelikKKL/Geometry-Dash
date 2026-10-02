// Shared drawing helpers: tinted background, ground, cube.
#pragma once
#include "core/PlayerPhysics.h"
#include "engine/Engine.h"

namespace ogd {

constexpr Color kBlue{0, 102, 255};

// yOffset: camera height in px (positive = camera moved up, the world shifts down)
void drawBackground(float scrollX, Color tint = kBlue, float yOffset = 0);
void drawGround(float scrollX, Color tint = kBlue, float yOffset = 0);
void drawCeiling(float scrollX, Color tint, float yOffset, float planeHeightPx);   // mirrored ground at y = planeHeightPx
void drawCube(int cube, const PlayerState& p, float camX, Color main, Color secondary);

Color randomColor();
Color paletteColor(int index);

} // namespace ogd
