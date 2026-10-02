#pragma once
#include <functional>
#include <string>

#include "engine/Engine.h"

namespace ogd {

// GD-style "bouncy" button: grows on press, bounces back on release.
class Button {
public:
    Button(const std::string& spriteName, float x, float y, float baseScale, std::function<void()> cb);

    void setLabel(const std::string& text, float scale = 0.8f) { label_ = text; labelScale_ = scale; }
    void update(float dt);
    void draw();
    bool onDown(float x, float y);   // true if the press started on this button
    void onUp(float x, float y);

    float x, y;
    bool flipX = false;
    Color tint;

private:
    bool hit(float px, float py) const;
    void animateTo(float target, float dur);

    Sprite spr_;
    float base_;
    float cur_ = 1.f, from_ = 1.f, to_ = 1.f, t_ = 1.f, dur_ = 0.3f;
    bool pressed_ = false;
    std::string label_;
    float labelScale_ = 0.8f;
    std::function<void()> cb_;
};

float easeBounceOut(float t);

} // namespace ogd
