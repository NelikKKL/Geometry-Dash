#include "game/World.h"

#include "core/Palette.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace ogd {

void drawBackground(float scrollX, Color tint, float yOffset, float scale) {
    Sprite bg = E().sprite("game_bg_01_001.png");
    if (!bg) { E().clear(tint); return; }
    float w = bg.w * scale;
    float start = -std::fmod(scrollX, w);
    if (start > 0) start -= w;
    for (float x = start; x < kW; x += w) E().drawSprite(bg, x + w / 2, kH / 2.f - yOffset * 0.1f, scale, scale, 0, tint);
}

void drawGround(float scrollX, Color tint, float yOffset, float scale) {
    Sprite g = E().sprite("groundSquare_001.png");
    if (!g) return;
    const float w = g.w * scale, h = g.h * scale;
    const float bottom = -50.f - yOffset;
    const float top = bottom + h;  // floor line; cube (size 60) rests here when centre Y = 236
    float start = -std::fmod(scrollX, w);
    if (start > 0) start -= w;
    for (float x = start; x < kW; x += w) E().drawSprite(g, x + w / 2, bottom + h / 2, scale, scale, 0, tint);

    Sprite line = E().sprite("floor.png");
    if (line) E().drawSprite(line, kW / 2.f, top, std::fmax(1.f, kW / line.w), 1);

    Sprite shadow = E().sprite("groundSquareShadow_001.png");
    if (shadow) {
        const float k = 1.6f * scale;
        E().drawSprite(shadow, shadow.srcW * k / 2, top - shadow.srcH * k / 2, k, k);
        E().drawSprite(shadow, kW - shadow.srcW * k / 2, top - shadow.srcH * k / 2, k, k, 0, {}, 255, true);
    }
}

void drawCube(int cube, const PlayerState& p, float camX, Color main, Color secondary) {
    char a[48], b[48];
    std::snprintf(a, sizeof a, "player_%02d_001.png", cube);
    std::snprintf(b, sizeof b, "player_%02d_2_001.png", cube);
    float x = (float)p.x - camX, y = (float)p.y;
    E().drawSprite(E().sprite(b), x, y, 1, 1, (float)p.rotation, secondary);
    E().drawSprite(E().sprite(a), x, y, 1, 1, (float)p.rotation, main);
}

Color randomColor() {
    return {(Uint8)(rand() % 256), (Uint8)(rand() % 256), (Uint8)(rand() % 256)};
}

Color paletteColor(int i) {
    const Rgb c = paletteRgb(i);
    return {c.r, c.g, c.b};
}

void drawVGradient(float yBottom, float yTop, Color bottom, Color top) {
    const int steps = std::max(2, (int)((yTop - yBottom) / 4.f));
    const float h = (yTop - yBottom) / steps;
    for (int i = 0; i < steps; ++i) {
        const float t = (i + 0.5f) / steps;
        auto m = [&](Uint8 a, Uint8 b) { return (Uint8)(a + (b - a) * t); };
        E().fillRect(0, yBottom + i * h, kW, h + 1.f, {m(bottom.r, top.r), m(bottom.g, top.g), m(bottom.b, top.b)}, 255);
    }
}

void drawCornerArt(Corner c, float scale) {
    Sprite a = E().sprite("GJ_sideArt_001.png");
    if (!a) return;
    const bool right = (c == Corner::BottomRight || c == Corner::TopRight);
    const bool top = (c == Corner::TopLeft || c == Corner::TopRight);
    const float cx = right ? kW - a.srcW * scale / 2 : a.srcW * scale / 2;
    const float cy = top ? kH - a.srcH * scale / 2 : a.srcH * scale / 2;
    E().drawSprite(a, cx, cy, scale, scale, 0, {}, 255, right, top);
}

void drawCeiling(float scrollX, Color tint, float yOffset, float planeHeightPx, float scale) {
    Sprite g = E().sprite("groundSquare_001.png");
    if (!g) return;
    const float w = g.w * scale, h = g.h * scale;
    const float edge = planeHeightPx - yOffset;           // the visible ceiling line (screen y, y-up)
    if (edge > kH + h) return;
    float start = -std::fmod(scrollX, w);
    if (start > 0) start -= w;
    for (float x = start; x < kW; x += w) E().drawSprite(g, x + w / 2, edge + h / 2, scale, -scale, 0, tint);
    Sprite line = E().sprite("floor.png");
    if (line) E().drawSprite(line, kW / 2.f, edge, std::fmax(1.f, kW / line.w), 1);
}

} // namespace ogd
