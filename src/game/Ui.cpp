#include "game/Ui.h"

#include <cmath>

namespace ogd {

float easeBounceOut(float t) {
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    if (t < 1 / 2.75f) return 7.5625f * t * t;
    if (t < 2 / 2.75f) { t -= 1.5f / 2.75f; return 7.5625f * t * t + 0.75f; }
    if (t < 2.5f / 2.75f) { t -= 2.25f / 2.75f; return 7.5625f * t * t + 0.9375f; }
    t -= 2.625f / 2.75f;
    return 7.5625f * t * t + 0.984375f;
}

Button::Button(const std::string& spriteName, float x_, float y_, float baseScale, std::function<void()> cb)
    : x(x_), y(y_), spr_(E().sprite(spriteName)), base_(baseScale), cb_(std::move(cb)) {}

void Button::animateTo(float target, float dur) {
    from_ = cur_;
    to_ = target;
    t_ = 0;
    dur_ = dur;
}

void Button::update(float dt) {
    if (t_ < 1.f) {
        t_ = std::fmin(1.f, t_ + dt / dur_);
        cur_ = from_ + (to_ - from_) * easeBounceOut(t_);
    }
}

bool Button::hit(float px, float py) const {
    float s = base_ * cur_;
    return std::fabs(px - x) <= spr_.srcW * s / 2 && std::fabs(py - y) <= spr_.srcH * s / 2;
}

bool Button::onDown(float px, float py) {
    if (!hit(px, py)) return false;
    pressed_ = true;
    animateTo(1.26f, 0.3f);
    return true;
}

void Button::onUp(float px, float py) {
    if (!pressed_) return;
    pressed_ = false;
    bool fire = hit(px, py);
    animateTo(1.f, 0.4f);
    if (fire && cb_) cb_();
}

void Button::draw() {
    float s = base_ * cur_;
    E().drawSprite(spr_, x, y, s, s, 0, tint, 255, flipX);
    if (!label_.empty()) E().drawText(E().font("bigFont.fnt"), label_, x, y + 2, labelScale_ * s);
}

} // namespace ogd
