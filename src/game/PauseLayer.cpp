#include "game/PauseLayer.h"

#include <algorithm>

namespace ogd {

namespace {
constexpr Uint8 kBackdropAlpha = 75;      // PauseLayer::customSetup: setOpacity(0x4b)
constexpr Uint8 kBarBackAlpha = 125;      // setupProgressBars: background bar opacity 125
constexpr float kBarFillScaleX = 0.992f;  // ... fill bar scale 0.992 x 0.86
constexpr float kBarFillScaleY = 0.86f;
} // namespace

PauseLayer::PauseLayer(const std::string& levelName, int normalPercent, int practicePercent, bool practiceMode,
                       PauseCallbacks cb)
    : name_(levelName), normal_(normalPercent), practice_(practicePercent), practiceMode_(practiceMode),
      cb_(std::move(cb)) {
    const float cy = 300.f;
    buttons_.reserve(8);                       // callbacks capture `this`, vector must not move buttons around
    buttons_.emplace_back("GJ_menuBtn_001.png", 400.f, cy, 1.f, [this] { if (cb_.onQuit) cb_.onQuit(); });
    buttons_.emplace_back("GJ_replayBtn_001.png", 540.f, cy, 1.f, [this] { if (cb_.onRestart) cb_.onRestart(); });
    buttons_.emplace_back("GJ_playBtn2_001.png", 700.f, cy, 1.f, [this] { if (cb_.onResume) cb_.onResume(); });
    if (practiceMode_)
        buttons_.emplace_back("GJ_normalBtn_001.png", 860.f, cy, 1.f, [this] { if (cb_.onNormal) cb_.onNormal(); });
    else
        buttons_.emplace_back("GJ_practiceBtn_001.png", 860.f, cy, 1.f, [this] { if (cb_.onPractice) cb_.onPractice(); });

    musicIdx_ = buttons_.size();
    buttons_.emplace_back(E().save.musicOn ? "GJ_musicOnBtn_001.png" : "GJ_musicOffBtn_001.png", 560.f, 150.f, 0.9f,
                          [this] { toggleMusic(); });
    fxIdx_ = buttons_.size();
    buttons_.emplace_back(E().save.fxOn ? "GJ_fxOnBtn_001.png" : "GJ_fxOffBtn_001.png", 720.f, 150.f, 0.9f,
                          [this] { toggleFx(); });
}

void PauseLayer::toggleMusic() {
    E().setMusicOn(!E().save.musicOn);
    buttons_[musicIdx_].setSprite(E().save.musicOn ? "GJ_musicOnBtn_001.png" : "GJ_musicOffBtn_001.png");
    E().persist();
}

void PauseLayer::toggleFx() {
    E().setFxOn(!E().save.fxOn);
    buttons_[fxIdx_].setSprite(E().save.fxOn ? "GJ_fxOnBtn_001.png" : "GJ_fxOffBtn_001.png");
    E().persist();
}

void PauseLayer::update(float dt) {
    for (Button& b : buttons_) b.update(dt);
}

void PauseLayer::drawBar(float cy, const std::string& label, int percent, Color fill) {
    Font* big = E().font("bigFont.fnt");
    Sprite bar = E().sprite("GJ_progressBar_001.png");
    const float cx = kW / 2.f;
    E().drawText(big, label, cx, cy + 36.f, 0.5f);
    E().drawSprite(bar, cx, cy, 1.f, 1.f, 0, {0, 0, 0}, kBarBackAlpha);
    const float w = bar.w * kBarFillScaleX;
    const float frac = std::max(0, std::min(100, percent)) / 100.f;
    if (frac > 0.f) {
        E().setClip(cx - w / 2.f, cy - bar.h, w * frac, bar.h * 2.f);
        E().drawSprite(bar, cx, cy, kBarFillScaleX, kBarFillScaleY, 0, fill);
        E().clearClip();
    }
    E().drawText(big, std::to_string(percent) + "%", cx, cy, 0.5f);
}

void PauseLayer::draw() {
    E().fillRect(0, 0, kW, kH, {0, 0, 0}, kBackdropAlpha);
    Font* big = E().font("bigFont.fnt");
    E().drawText(big, name_, kW / 2.f, 650.f, fitScale(big, name_, 800.f, 1.0f));
    drawBar(550.f, "Normal Mode", normal_, {0, 255, 0});
    drawBar(445.f, "Practice Mode", practice_, {0, 200, 255});
    for (Button& b : buttons_) b.draw();
    E().drawText(big, "Music", 560.f, 100.f, 0.4f);
    E().drawText(big, "FX", 720.f, 100.f, 0.4f);
}

void PauseLayer::onDown(float x, float y) {
    for (Button& b : buttons_) if (b.onDown(x, y)) return;
}

void PauseLayer::onUp(float x, float y) {
    for (Button& b : buttons_) b.onUp(x, y);
}

} // namespace ogd
