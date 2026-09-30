#include "game/World.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace ogd {

void drawBackground(float scrollX, Color tint) {
    Sprite bg = E().sprite("game_bg_01_001.png");
    if (!bg) { E().clear(tint); return; }
    float w = bg.w;
    float start = -std::fmod(scrollX, w);
    if (start > 0) start -= w;
    for (float x = start; x < kW; x += w) E().drawSprite(bg, x + w / 2, kH / 2.f, 1, 1, 0, tint);
}

void drawGround(float scrollX, Color tint) {
    Sprite g = E().sprite("groundSquare_001.png");
    if (!g) return;
    const float bottom = -50.f;
    const float top = bottom + g.h;  // floor line; cube (size 60) rests here when centre Y = 236
    float start = -std::fmod(scrollX, g.w);
    if (start > 0) start -= g.w;
    for (float x = start; x < kW; x += g.w) E().drawSprite(g, x + g.w / 2, bottom + g.h / 2, 1, 1, 0, tint);

    Sprite line = E().sprite("floor.png");
    if (line) E().drawSprite(line, kW / 2.f, top, std::fmax(1.f, kW / line.w), 1);

    Sprite shadow = E().sprite("groundSquareShadow_001.png");
    if (shadow) {
        const float k = 1.6f;
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

// Approximation of the GD colour palette (first 12 entries); used for the saved player colours.
Color paletteColor(int i) {
    static const Color pal[] = {{125, 255, 0}, {0, 255, 0}, {0, 255, 125}, {0, 255, 255}, {0, 125, 255}, {0, 0, 255},
                                {125, 0, 255}, {255, 0, 255}, {255, 0, 125}, {255, 0, 0}, {255, 125, 0}, {255, 255, 0}};
    return pal[((i % 12) + 12) % 12];
}

} // namespace ogd
