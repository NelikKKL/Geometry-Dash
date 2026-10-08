#include "game/Ui.h"

#include <cmath>
#include <vector>

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

namespace ogd {

LabelButton::LabelButton(std::string t, float x_, float y_, float w_, float h_, std::function<void()> cb,
                         const char* fontName, float textScale, float textDy)
    : text(std::move(t)), x(x_), y(y_), w(w_), h(h_), cb_(std::move(cb)), font_(fontName), textScale_(textScale),
      textDy_(textDy) {}

void LabelButton::animateTo(float target, float dur) { from_ = cur_; to_ = target; t_ = 0; dur_ = dur; }

void LabelButton::update(float dt) {
    if (t_ < 1.f) {
        t_ = std::fmin(1.f, t_ + dt / dur_);
        cur_ = from_ + (to_ - from_) * easeBounceOut(t_);
    }
}

bool LabelButton::hit(float px, float py, float dy) const {
    return std::fabs(px - x) <= w * cur_ / 2 && std::fabs(py - (y + dy)) <= h * cur_ / 2;
}

bool LabelButton::onDown(float px, float py, float dy) {
    if (!hit(px, py, dy)) return false;
    pressed_ = true;
    animateTo(1.12f, 0.3f);
    return true;
}

void LabelButton::onUp(float px, float py, float dy) {
    if (!pressed_) return;
    pressed_ = false;
    const bool fire = hit(px, py, dy);
    animateTo(1.f, 0.4f);
    if (fire && cb_) cb_();
}

void LabelButton::draw(float dy) const {
    E().drawPanel(E().sprite("GJ_button_01.png"), x, y + dy, w * cur_, h * cur_);
    Font* f = E().font(font_);
    E().drawText(f, text, x, y + dy + textDy_, fitScale(f, text, w * 0.9f, textScale_) * cur_);
}

namespace {
struct Seg { std::string text; Color col; };
Color tagColor(const std::string& tag) {
    if (tag == "cg") return {64, 227, 72};
    if (tag == "cy") return {255, 255, 0};
    if (tag == "cl") return {96, 171, 239};
    if (tag == "cr") return {255, 90, 90};
    return {255, 255, 255};
}
} // namespace

void drawRichText(Font* f, const std::string& markup, float cx, float y, float scale, float maxWidth, float lineH) {
    if (!f) return;
    // 1. split into coloured words, '\n' = hard break
    struct Word { std::string t; Color c; bool nl; bool glue; };
    std::vector<Word> words;
    Color cur{255, 255, 255};
    std::string w;
    bool spaceBefore = true, wGlue = false;      // glue: the word follows a colour tag with no space in between
    auto flush = [&] { if (!w.empty()) { words.push_back({w, cur, false, wGlue}); w.clear(); } };
    for (size_t i = 0; i < markup.size(); ++i) {
        const char ch = markup[i];
        if (ch == '<') {
            const size_t e = markup.find('>', i);
            if (e != std::string::npos) {
                const std::string tag = markup.substr(i + 1, e - i - 1);
                if (tag == "/c") { flush(); cur = {255, 255, 255}; }
                else { flush(); cur = tagColor(tag); }
                i = e;
                continue;
            }
        }
        if (ch == '\n') { flush(); words.push_back({"", cur, true, false}); spaceBefore = true; }
        else if (ch == ' ') { flush(); spaceBefore = true; }
        else {
            if (w.empty()) wGlue = !spaceBefore && !words.empty() && !words.back().nl;
            w += ch;
            spaceBefore = false;
        }
    }
    flush();
    auto width = [&](const std::string& s) { return f->bm.measure(s) * f->scale * scale; };
    const float space = width(" ");
    // 2. greedy line fill
    std::vector<std::vector<Word>> lines(1);
    float lw = 0;
    for (const Word& wd : words) {
        if (wd.nl) { lines.emplace_back(); lw = 0; continue; }
        const float ww = width(wd.t);
        if (!lines.back().empty() && !wd.glue && lw + space + ww > maxWidth) { lines.emplace_back(); lw = 0; }
        lw += (lines.back().empty() || wd.glue ? 0.f : space) + ww;
        lines.back().push_back(wd);
    }
    // 3. draw
    float ly = y;
    for (const auto& ln : lines) {
        float total = 0;
        for (size_t i = 0; i < ln.size(); ++i) total += width(ln[i].t) + (i && !ln[i].glue ? space : 0.f);
        float x = cx - total / 2;
        for (size_t i = 0; i < ln.size(); ++i) {
            const Word& wd = ln[i];
            if (i && !wd.glue) x += space;
            E().drawText(f, wd.t, x, ly, scale, Align::Left, wd.c);
            x += width(wd.t);
        }
        ly -= lineH;
    }
}

} // namespace ogd
