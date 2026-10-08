#include "game/PauseLayer.h"

#include <algorithm>

namespace ogd {

namespace {
constexpr float kBarW = 727.f, kBarH = 39.f;   // measured from screenshots (design px)

float diameterScale(const char* sprite, float diameter) {
    Sprite s = E().sprite(sprite);
    return s.srcW > 0 ? diameter / s.srcW : 1.f;
}
} // namespace

PauseLayer::PauseLayer(const std::string& levelName, int normalPercent, int practicePercent, bool practiceMode,
                       PauseCallbacks cb)
    : name_(levelName), normal_(normalPercent), practice_(practicePercent), practiceMode_(practiceMode),
      cb_(std::move(cb)) {
    buttons_.reserve(10);                      // callbacks capture `this`; no reallocation allowed
    // big row: Practice (or Normal) - Resume - Menu
    if (practiceMode_)
        buttons_.emplace_back("GJ_normalBtn_001.png", 445.f, 259.f, diameterScale("GJ_normalBtn_001.png", 124.f),
                              [this] { if (cb_.onNormal) cb_.onNormal(); });
    else
        buttons_.emplace_back("GJ_practiceBtn_001.png", 445.f, 259.f, diameterScale("GJ_practiceBtn_001.png", 124.f),
                              [this] { if (cb_.onPractice) cb_.onPractice(); });
    buttons_.emplace_back("GJ_playBtn2_001.png", 640.f, 259.f, diameterScale("GJ_playBtn2_001.png", 159.f),
                          [this] { if (cb_.onResume) cb_.onResume(); });
    buttons_.emplace_back("GJ_menuBtn_001.png", 834.f, 259.f, diameterScale("GJ_menuBtn_001.png", 124.f),
                          [this] { if (cb_.onQuit) cb_.onQuit(); });
    // small row: Music, FX, Auto, Help
    const float d = 76.f;
    musicIdx_ = buttons_.size();
    buttons_.emplace_back("GJ_musicOnBtn_001.png", 485.f, 83.f, diameterScale("GJ_musicOnBtn_001.png", d), [this] {
        E().setMusicOn(!E().save.musicOn); E().persist(); refreshToggles();
    });
    fxIdx_ = buttons_.size();
    buttons_.emplace_back("GJ_fxOnBtn_001.png", 588.f, 83.f, diameterScale("GJ_fxOnBtn_001.png", d), [this] {
        E().setFxOn(!E().save.fxOn); E().persist(); refreshToggles();
    });
    autoIdx_ = buttons_.size();
    buttons_.emplace_back("GJ_autoOffBtn_001.png", 691.f, 83.f, diameterScale("GJ_autoOffBtn_001.png", d), [this] {
        E().save.autoCheck = !E().save.autoCheck; E().persist(); refreshToggles();
    });
    buttons_.emplace_back("GJ_helpBtn_001.png", 794.f, 83.f, diameterScale("GJ_helpBtn_001.png", d),
                          [this] { tut_.open(); });
    refreshToggles();
}

void PauseLayer::refreshToggles() {
    buttons_[musicIdx_].setSprite(E().save.musicOn ? "GJ_musicOnBtn_001.png" : "GJ_musicOffBtn_001.png");
    buttons_[fxIdx_].setSprite(E().save.fxOn ? "GJ_fxOnBtn_001.png" : "GJ_fxOffBtn_001.png");
    buttons_[autoIdx_].setSprite(E().save.autoCheck ? "GJ_autoOnBtn_001.png" : "GJ_autoOffBtn_001.png");
}

void PauseLayer::update(float dt) {
    for (Button& b : buttons_) b.update(dt);
    tut_.update(dt);
}

void PauseLayer::drawBar(float cy, const std::string& label, int percent, Color fill) {
    Font* big = E().font("bigFont.fnt");
    Sprite bar = E().sprite("GJ_progressBar_001.png");
    const float lblScale = 236.f / std::max(1.f, big->bm.measure("Normal Mode") * big->scale);
    E().drawText(big, label, kW / 2.f, cy + 34.f, lblScale);
    if (bar && bar.w > 0) {
        const float sx = kBarW / bar.w, sy = kBarH / bar.h;
        E().drawSprite(bar, kW / 2.f, cy, sx, sy, 0, {0, 0, 0}, 125);          // dark track
        const float frac = std::max(0, std::min(100, percent)) / 100.f;
        if (frac > 0.f) {
            E().setClip(kW / 2.f - kBarW / 2, cy - kBarH, kBarW * frac, kBarH * 2.f);
            E().drawSprite(bar, kW / 2.f, cy, sx, sy, 0, fill);
            E().clearClip();
        }
    }
    const float pctScale = 90.f / std::max(1.f, big->bm.measure("100%") * big->scale);
    E().drawText(big, std::to_string(percent) + "%", kW / 2.f, cy - 11.f, pctScale);
}

void PauseLayer::draw() {
    E().fillRect(0, 0, kW, kH, {0, 0, 0}, 75);                               // PauseLayer::customSetup: opacity 0x4b
    E().drawPanel(E().sprite("square04_001.png"), kW / 2.f, 360.f, 1237.f, 674.f, {0, 0, 0}, 150);
    Font* big = E().font("bigFont.fnt");
    const float s0 = 580.f / std::max(1.f, big->bm.measure("Stereo Madness") * big->scale);
    E().drawText(big, name_, kW / 2.f, 620.f, fitScale(big, name_, 760.f, s0));
    drawBar(518.f, "Normal Mode", normal_, {0, 255, 0});
    drawBar(405.f, "Practice Mode", practice_, {0, 200, 255});
    for (Button& b : buttons_) b.draw();
    if (tut_.active) tut_.draw();
}

void PauseLayer::onDown(float x, float y) {
    if (tut_.active) { tut_.down(x, y); return; }
    for (Button& b : buttons_) if (b.onDown(x, y)) return;
}

void PauseLayer::onUp(float x, float y) {
    if (tut_.active) { tut_.up(x, y); return; }
    for (Button& b : buttons_) b.onUp(x, y);
}

} // namespace ogd
