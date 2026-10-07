#include "game/OptionsLayer.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

#include "game/Levels.h"
#include "game/Popup.h"

namespace ogd {
namespace {

constexpr float kSlideTime = 0.5f;        // GJDropDownLayer::showLayer / hideLayer: CCMoveTo 0.5 s + CCEaseInOut
constexpr Uint8 kBackdropAlpha = 125;     // GJDropDownLayer::init: layer colour (0,0,0,0x7d)
constexpr Uint8 kTutorialAlpha = 150;     // TutorialLayer::init: colour (0,0,0,0x96)
constexpr float kPanelH = 600.f;

float easeInOut(float t) {                // cocos CCEaseInOut(rate 2)
    t = std::max(0.f, std::min(1.f, t));
    return t < 0.5f ? 0.5f * std::pow(2.f * t, 2.f) : 1.f - 0.5f * std::pow(2.f * (1.f - t), 2.f);
}

// Text button on the green GJ_button_01 plate with the same bounce as ogd::Button.
struct TextButton {
    std::string label;
    float x, y, w, h, labelScale;
    std::function<void()> cb;
    float cur = 1.f, from = 1.f, to = 1.f, t = 1.f, dur = 0.3f;
    bool pressed = false;

    TextButton(std::string l, float x_, float y_, float w_, float h_, std::function<void()> c, float ls = 0.8f)
        : label(std::move(l)), x(x_), y(y_), w(w_), h(h_), labelScale(ls), cb(std::move(c)) {}

    void animateTo(float target, float d) { from = cur; to = target; t = 0; dur = d; }
    void update(float dt) {
        if (t < 1.f) { t = std::fmin(1.f, t + dt / dur); cur = from + (to - from) * easeBounceOut(t); }
    }
    bool hit(float px, float py, float dy) const {
        return std::fabs(px - x) <= w * cur / 2 && std::fabs(py - (y + dy)) <= h * cur / 2;
    }
    bool down(float px, float py, float dy) {
        if (!hit(px, py, dy)) return false;
        pressed = true; animateTo(1.12f, 0.3f);
        return true;
    }
    void up(float px, float py, float dy) {
        if (!pressed) return;
        pressed = false;
        const bool fire = hit(px, py, dy);
        animateTo(1.f, 0.4f);
        if (fire && cb) cb();
    }
    void draw(float dy) const {
        E().drawPanel(E().sprite("GJ_button_01.png"), x, y + dy, w * cur, h * cur);
        Font* f = E().font("bigFont.fnt");
        E().drawText(f, label, x, y + dy + 2.f, fitScale(f, label, w * 0.86f, labelScale) * cur);
    }
};

enum class Page { Main, Support, Songs };

} // namespace

struct OptionsLayer::Impl {
    Page page = Page::Main, pending = Page::Main;
    enum class Phase { SlideIn, Shown, SlideOut } phase = Phase::SlideIn;
    float t = 0.f;
    bool closeAfter = true;             // slide-out ends by closing the whole thing (vs. opening another page)

    // tutorial overlay
    bool tutorial = false;
    int tutorialPage = 1;

    std::vector<TextButton> main, support;
    std::unique_ptr<Button> back, musicBtn, fxBtn;
    std::unique_ptr<TextButton> tutNext;
    std::unique_ptr<Popup> popup;
    OptionsLayer* owner = nullptr;

    // px the panel is raised above its resting place (0 = fully shown, kPanelH + 20 = fully hidden)
    float lift() const {
        const float full = kPanelH + 20.f;
        if (phase == Phase::SlideIn) return (1.f - easeInOut(t)) * full;
        if (phase == Phase::SlideOut) return easeInOut(t) * full;
        return 0.f;
    }

    void startSlideOut(bool closeWhole, Page next = Page::Main) {
        if (phase != Phase::Shown) return;
        phase = Phase::SlideOut; t = 0.f;
        closeAfter = closeWhole; pending = next;
    }

    void refreshToggles() {
        musicBtn->setSprite(E().save.musicOn ? "GJ_musicOnBtn_001.png" : "GJ_musicOffBtn_001.png");
        fxBtn->setSprite(E().save.fxOn ? "GJ_fxOnBtn_001.png" : "GJ_fxOffBtn_001.png");
    }
};

OptionsLayer::OptionsLayer() : d_(new Impl) {
    Impl& d = *d_;
    d.owner = this;
    const float lx = 400.f;
    d.main.reserve(8);
    d.main.emplace_back("Support", lx, 500.f, 440.f, 74.f, [this] { d_->startSlideOut(false, Page::Support); });
    d.main.emplace_back("Rate", lx, 410.f, 440.f, 74.f, [this] {
        // GameManager::rateGame opens the store page; a browser build has none
        d_->popup = std::make_unique<Popup>("Rate", "Thanks for playing!\nThere is no store page for the web port.");
    });
    d.main.emplace_back("Download Soundtracks", lx, 320.f, 440.f, 74.f, [this] { d_->startSlideOut(false, Page::Songs); }, 0.7f);
    d.main.emplace_back("How to play", lx, 230.f, 440.f, 74.f, [this] { d_->tutorial = true; d_->tutorialPage = 1; });

    d.support.reserve(4);
    d.support.emplace_back("Email Support", kW / 2.f, 470.f, 520.f, 80.f, [] { SDL_OpenURL("http://www.robtopgames.com"); });
    d.support.emplace_back("RobTop Games", 410.f, 300.f, 380.f, 74.f, [] { SDL_OpenURL("http://www.robtopgames.com"); });
    d.support.emplace_back("cocos2d-x", 870.f, 300.f, 380.f, 74.f, [] { SDL_OpenURL("http://www.cocos2d-x.org"); });

    d.back = std::make_unique<Button>("GJ_arrow_01_001.png", 54.f, kH - 48.f, 1.f, [this] {
        if (d_->tutorial) return;
        d_->startSlideOut(d_->page == Page::Main, Page::Main);   // sub-pages return to Options, Options closes
    });
    d.musicBtn = std::make_unique<Button>("GJ_musicOnBtn_001.png", 860.f, 470.f, 1.2f, [this] {
        E().setMusicOn(!E().save.musicOn); E().persist(); d_->refreshToggles();
    });
    d.fxBtn = std::make_unique<Button>("GJ_fxOnBtn_001.png", 1060.f, 470.f, 1.2f, [this] {
        E().setFxOn(!E().save.fxOn); E().persist(); d_->refreshToggles();
    });
    d.tutNext = std::make_unique<TextButton>("Next", kW / 2.f, 190.f, 240.f, 70.f, [this] {
        if (d_->tutorialPage < 5) ++d_->tutorialPage;          // TutorialLayer::onNext
        else d_->tutorial = false;
    });
    d.refreshToggles();
}

OptionsLayer::~OptionsLayer() = default;

void OptionsLayer::update(float dt) {
    Impl& d = *d_;
    if (d.popup) { d.popup->update(dt); if (d.popup->closed) d.popup.reset(); return; }
    d.back->update(dt); d.musicBtn->update(dt); d.fxBtn->update(dt); d.tutNext->update(dt);
    for (auto& b : d.main) b.update(dt);
    for (auto& b : d.support) b.update(dt);
    switch (d.phase) {
    case Impl::Phase::SlideIn:
        d.t = std::min(1.f, d.t + dt / kSlideTime);
        if (d.t >= 1.f) d.phase = Impl::Phase::Shown;
        break;
    case Impl::Phase::SlideOut:
        d.t = std::min(1.f, d.t + dt / kSlideTime);
        if (d.t >= 1.f) {
            if (d.closeAfter) { closed = true; break; }
            d.page = d.pending; d.t = 0.f; d.phase = Impl::Phase::SlideIn;   // layerHidden -> open the next drop-down
        }
        break;
    case Impl::Phase::Shown: break;
    }
}

void OptionsLayer::draw() {
    Impl& d = *d_;
    const float dy = d.lift();                          // panel raised by dy px
    E().fillRect(0, 0, kW, kH, {0, 0, 0}, (Uint8)(kBackdropAlpha * (1.f - dy / (kPanelH + 20.f))));
    const float bottom = kH - kPanelH + dy;

    // panel: blue gradient with a dark bottom rim
    E().fillRect(0, bottom, kW, kPanelH, {0, 62, 168}, 255);
    E().drawSpriteTiledX(E().sprite("GJ_gradientBG.png"), 0, bottom + kPanelH / 2, (float)kW, {0, 90, 220});
    E().fillRect(0, bottom - 8.f, kW, 8.f, {0, 20, 70}, 255);

    Font* big = E().font("bigFont.fnt");
    const char* title = d.page == Page::Main ? "Options" : (d.page == Page::Support ? "Support" : "Soundtracks");
    E().drawText(big, title, kW / 2.f, kH - 70.f + dy, 1.0f);
    d.back->y = kH - 48.f + dy;
    d.back->draw();

    if (d.page == Page::Main) {
        for (auto& b : d.main) b.draw(dy);
        d.musicBtn->y = 470.f + dy; d.fxBtn->y = 470.f + dy;
        d.musicBtn->draw(); d.fxBtn->draw();
        E().drawText(big, "Music", 860.f, 390.f + dy, 0.55f);
        E().drawText(big, "FX", 1060.f, 390.f + dy, 0.55f);
    } else if (d.page == Page::Support) {
        for (auto& b : d.support) b.draw(dy);
    } else {
        Font* gold = E().font("goldFont.fnt");
        Font* chat = E().font("chatFont.fnt");
        for (int i = 0; i < kLevelCount; ++i) {
            const float y = 540.f - i * 58.f + dy;
            E().fillRect(340.f, y - 27.f, 600.f, 54.f, {0, 40, 120}, i % 2 ? 90 : 150);
            E().drawText(gold, levelMeta(i).name, 360.f, y + 6.f, 0.6f, Align::Left);
            E().drawText(chat, levelMeta(i).track, 920.f, y - 4.f, 0.8f, Align::Right);
        }
    }

    if (d.tutorial) {
        E().fillRect(0, 0, kW, kH, {0, 0, 0}, kTutorialAlpha);
        E().drawPanel(E().sprite("GJ_square01.png"), kW / 2.f, kH / 2.f + 40.f, 840.f, 520.f);
        char n[32]; std::snprintf(n, sizeof n, "tutorial_%02d_001.png", d.tutorialPage);
        Sprite s = E().sprite(n);
        if (s) {
            const float k = std::min(1.f, std::min(760.f / std::max(1.f, s.w), 400.f / std::max(1.f, s.h)));
            E().drawSprite(s, kW / 2.f, kH / 2.f + 70.f, k, k);
        }
        d.tutNext->label = d.tutorialPage < 5 ? "Next" : "OK";
        d.tutNext->draw(0.f);
    }
    if (d.popup) d.popup->draw();
}

void OptionsLayer::onDown(float x, float y) {
    Impl& d = *d_;
    if (d.popup) { d.popup->onDown(x, y); return; }
    if (d.tutorial) { d.tutNext->down(x, y, 0.f); return; }
    if (d.phase != Impl::Phase::Shown) return;
    if (d.back->onDown(x, y)) return;
    if (d.page == Page::Main) {
        for (auto& b : d.main) if (b.down(x, y, 0.f)) return;
        if (d.musicBtn->onDown(x, y)) return;
        d.fxBtn->onDown(x, y);
    } else if (d.page == Page::Support) {
        for (auto& b : d.support) if (b.down(x, y, 0.f)) return;
    }
}

void OptionsLayer::onUp(float x, float y) {
    Impl& d = *d_;
    if (d.popup) { d.popup->onUp(x, y); return; }
    if (d.tutorial) { d.tutNext->up(x, y, 0.f); return; }
    d.back->onUp(x, y);
    for (auto& b : d.main) b.up(x, y, 0.f);
    for (auto& b : d.support) b.up(x, y, 0.f);
    d.musicBtn->onUp(x, y);
    d.fxBtn->onUp(x, y);
}

void OptionsLayer::onKey(SDL_Keycode k, bool down) {
    Impl& d = *d_;
    if (!down || k != SDLK_ESCAPE) return;
    if (d.popup) { d.popup->closed = true; return; }
    if (d.tutorial) { d.tutorial = false; return; }          // TutorialLayer::keyBackClicked
    d.startSlideOut(d.page == Page::Main, Page::Main);
}

std::unique_ptr<OptionsLayer> makeOptionsLayer() { return std::make_unique<OptionsLayer>(); }

} // namespace ogd
