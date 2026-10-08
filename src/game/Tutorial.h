// TutorialLayer: the 5-page "How to play" popup (Options > How to play, pause menu > Help).
#pragma once
#include <algorithm>
#include <cstdio>

#include "game/Ui.h"

namespace ogd {

class Tutorial {
public:
    Tutorial()
        : close_("GJ_closeBtn_001.png", 209.f, 630.f, 1.f, [this] { active = false; }),
          next_("Next", 981.f, 180.f, 134.f, 87.f, [this] {
              if (page_ < 5) ++page_; else active = false;           // TutorialLayer::onNext
          }) {}

    void open() { active = true; page_ = 1; }
    void update(float dt) { close_.update(dt); next_.update(dt); }
    void draw() {
        E().fillRect(0, 0, kW, kH, {0, 0, 0}, 150);                 // TutorialLayer::init: colour (0,0,0,150)
        E().drawPanel(E().sprite("GJ_square01.png"), 641.f, 360.f, 898.f, 580.f);
        Font* big = E().font("bigFont.fnt");
        const float ts = 459.f / std::max(1.f, big->bm.measure("How to play") * big->scale);
        E().drawText(big, "How to play", 640.f, 573.f, ts);

        struct Page { const char* text; float y, lineH; };
        static const Page pages[5] = {
            {"<cg>Tap</c> the screen to jump.\n<cg>Hold</c> down to keep jumping.", 473.f, 63.f},
            {"<cg>Hold</c> to fly up.\n<cg>Release</c> to fly down.", 473.f, 63.f},
            {"You can enter <cl>Practice Mode</c> from\nthe pause menu.\nPractice mode lets you place\n<cg>checkpoints</c>.", 499.f, 54.f},
            {"You can place checkpoints manually,\nor use the auto-checkpoint feature.\nTap the delete button to remove your\nlast checkpoint.", 505.f, 56.f},
            {"<cy>Jump rings</c> activate when you are on\ntop of them.\n<cg>Tap</c> while touching a ring to\nperform a ring jump.", 499.f, 54.f},
        };
        const Page& pg = pages[page_ - 1];
        const float rs = 768.f / std::max(1.f, big->bm.measure("You can place checkpoints manually,") * big->scale);
        drawRichText(big, pg.text, 640.f, pg.y, rs, 860.f, pg.lineH);   // 768 px = width of the longest real line

        char n[32];
        std::snprintf(n, sizeof n, "tutorial_%02d_001.png", page_);
        Sprite s = E().sprite(n);
        if (s && s.w > 0) {
            const float k = 653.f / s.w;
            E().drawSprite(s, 538.f, 193.f, k, k);
        }
        next_.text = page_ < 5 ? "Next" : "Exit";
        next_.draw();
        close_.draw();
    }
    void down(float x, float y) { if (!close_.onDown(x, y)) next_.onDown(x, y); }
    void up(float x, float y) { close_.onUp(x, y); next_.onUp(x, y); }
    bool active = false;

private:
    int page_ = 1;
    Button close_;
    LabelButton next_;
};

} // namespace ogd
