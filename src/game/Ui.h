#pragma once
#include <functional>
#include <string>

#include "engine/Engine.h"

namespace ogd {

// GD-style "bouncy" button: grows on press, bounces back on release.
class Button {
public:
    Button(const std::string& spriteName, float x, float y, float baseScale, std::function<void()> cb);

    void setSprite(const std::string& spriteName) { spr_ = E().sprite(spriteName); }
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

// Wide green plate (GJ_button_01 scale-9) with a label: the "ButtonSprite" used by Options, tutorial, song cards.
class LabelButton {
public:
    LabelButton(std::string text, float x, float y, float w, float h, std::function<void()> cb,
                const char* fontName = "bigFont.fnt", float textScale = 0.8f, float textDy = 0.f);
    void update(float dt);
    void draw(float dy = 0.f) const;
    bool onDown(float px, float py, float dy = 0.f);
    void onUp(float px, float py, float dy = 0.f);
    std::string text;
    float x, y, w, h;

private:
    bool hit(float px, float py, float dy) const;
    void animateTo(float target, float dur);
    std::function<void()> cb_;
    std::string font_;
    float textScale_, textDy_;
    float cur_ = 1.f, from_ = 1.f, to_ = 1.f, t_ = 1.f, dur_ = 0.3f;
    bool pressed_ = false;
};

// Text with GD colour tags: <cg>green</c> <cy>yellow</c> <cl>blue</c> <cr>red</c>; '\n' breaks a line.
// Word-wrapped to maxWidth, every line centred on cx; y is the middle of the first line, lines go down by lineH.
void drawRichText(Font* f, const std::string& markup, float cx, float y, float scale, float maxWidth, float lineH);

// Largest scale <= maxScale at which `text` fits into maxWidth design px.
inline float fitScale(Font* f, const std::string& text, float maxWidth, float maxScale) {
    if (!f) return maxScale;
    const float w = f->bm.measure(text) * f->scale;
    return (w <= 0.f || w * maxScale <= maxWidth) ? maxScale : maxWidth / w;
}

} // namespace ogd
