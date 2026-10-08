// Options, Support and Soundtracks as hanging boards plus the 5-page tutorial.
// Port of OptionsLayer / SupportLayer / SongsLayer / SongInfoLayer / TutorialLayer (1.0 APK); the layout was measured
// from screenshots of the real game.
#include <algorithm>
#include <cmath>
#include <memory>

#include "game/Boards.h"
#include "game/Popup.h"
#include "game/Songs.h"
#include "game/Tutorial.h"

namespace ogd {
namespace {

// ----------------------------------------------------------------------------------------------- options
class OptionsBoard : public Board {
public:
    OptionsBoard() : Board("Options") {
        buttons_.reserve(4);
        const float ys[4] = {530.f, 447.f, 363.f, 280.f};
        const char* names[4] = {"Support", "Rate", "Download Soundtracks", "How to play"};
        for (int i = 0; i < 4; ++i)
            buttons_.emplace_back(names[i], 640.f, ys[i], 506.f, 60.f, [this, i] { act(i); }, "goldFont.fnt", 0.95f, -9.f);
        Sprite m = E().sprite("GJ_musicOnBtn_001.png");
        const float base = m.srcW > 0 ? 76.f / m.srcW : 1.f;
        music_ = std::make_unique<Button>("GJ_musicOnBtn_001.png", 590.f, 180.f, base, [this] {
            E().setMusicOn(!E().save.musicOn); E().persist(); refresh();
        });
        fx_ = std::make_unique<Button>("GJ_fxOnBtn_001.png", 691.f, 180.f, base, [this] {
            E().setFxOn(!E().save.fxOn); E().persist(); refresh();
        });
        refresh();
    }

protected:
    void updateInterior(float dt) override {
        for (auto& b : buttons_) b.update(dt);
        music_->update(dt); fx_->update(dt);
    }
    void updateOverlay(float dt) override {
        tut_.update(dt);
        if (popup_) { popup_->update(dt); if (popup_->closed) popup_.reset(); }
    }
    void drawInterior() override {
        const float dy = slide();
        E().fillRect(kInL, kInBottom + dy, kInR - kInL, kInTop - kInBottom, {0, 0, 0}, 150);
        for (auto& b : buttons_) b.draw(dy);
        music_->y = 180.f + dy; fx_->y = 180.f + dy;
        music_->draw(); fx_->draw();
    }
    void interiorDown(float x, float y) override {
        for (auto& b : buttons_) if (b.onDown(x, y)) return;
        if (music_->onDown(x, y)) return;
        fx_->onDown(x, y);
    }
    void interiorUp(float x, float y) override {
        for (auto& b : buttons_) b.onUp(x, y);
        music_->onUp(x, y); fx_->onUp(x, y);
    }
    bool overlayActive() const override { return tut_.active || popup_; }
    void drawOverlay() override {
        if (tut_.active) tut_.draw();
        if (popup_) popup_->draw();
    }
    void overlayDown(float x, float y) override {
        if (popup_) popup_->onDown(x, y); else tut_.down(x, y);
    }
    void overlayUp(float x, float y) override {
        if (popup_) popup_->onUp(x, y); else tut_.up(x, y);
    }
    void overlayBack() override {                          // TutorialLayer::keyBackClicked
        if (popup_) popup_->closed = true; else tut_.active = false;
    }

private:
    void refresh() {
        music_->setSprite(E().save.musicOn ? "GJ_musicOnBtn_001.png" : "GJ_musicOffBtn_001.png");
        fx_->setSprite(E().save.fxOn ? "GJ_fxOnBtn_001.png" : "GJ_fxOffBtn_001.png");
    }
    void act(int i);
    std::vector<LabelButton> buttons_;
    std::unique_ptr<Button> music_, fx_;
    Tutorial tut_;
    std::unique_ptr<Popup> popup_;
};

// ----------------------------------------------------------------------------------------------- support
class SupportBoard : public Board {
public:
    SupportBoard() : Board("Support") {
        email_ = std::make_unique<LabelButton>("Email Support", 640.f, 473.f, 506.f, 58.f,
                                               [] { SDL_OpenURL("http://www.robtopgames.com"); }, "goldFont.fnt", 0.95f, -9.f);
        Sprite r = E().sprite("robtoplogo_small.png"), c = E().sprite("cocos2DxLogo.png");
        rob_ = std::make_unique<Button>("robtoplogo_small.png", 736.f, 313.f, r.srcW > 0 ? 215.f / r.srcW : 1.f,
                                        [] { SDL_OpenURL("http://www.robtopgames.com"); });
        cocos_ = std::make_unique<Button>("cocos2DxLogo.png", 736.f, 200.f, c.srcW > 0 ? 192.f / c.srcW : 1.f,
                                          [] { SDL_OpenURL("http://www.cocos2d-x.org"); });
    }

protected:
    void updateInterior(float dt) override { email_->update(dt); rob_->update(dt); cocos_->update(dt); }
    void drawInterior() override {
        const float dy = slide();
        E().fillRect(kInL, kInBottom + dy, kInR - kInL, kInTop - kInBottom, {0, 0, 0}, 150);
        email_->draw(dy);
        Font* chat = E().font("chatFont.fnt");
        E().drawText(chat, "Developed By:", 522.f, 313.f + dy, 1.0f);
        E().drawText(chat, "Powered By:", 522.f, 200.f + dy, 1.0f);
        rob_->y = 313.f + dy; cocos_->y = 200.f + dy;
        rob_->draw(); cocos_->draw();
    }
    void interiorDown(float x, float y) override {
        if (email_->onDown(x, y)) return;
        if (rob_->onDown(x, y)) return;
        cocos_->onDown(x, y);
    }
    void interiorUp(float x, float y) override { email_->onUp(x, y); rob_->onUp(x, y); cocos_->onUp(x, y); }

private:
    std::unique_ptr<LabelButton> email_;
    std::unique_ptr<Button> rob_, cocos_;
};

// ----------------------------------------------------------------------------------------------- soundtracks
class SongsBoard : public Board {
public:
    SongsBoard() : Board("Soundtracks") {}

protected:
    static constexpr float kRowH = 135.f, kFirst = 528.f;
    float maxScroll() const { return std::max(0.f, kSongsListed * kRowH - (kInTop - kInBottom)); }

    void updateOverlay(float dt) override {
        for (auto& b : card_) b.update(dt);
        for (auto& b : closeCard_) b.update(dt);
    }
    void drawInterior() override {
        const float dy = slide();
        const Color dark{162, 88, 45}, light{194, 114, 63};
        E().fillRect(kInL, kInBottom + dy, kInR - kInL, kInTop - kInBottom, light, 255);
        Font* big = E().font("bigFont.fnt");
        Font* gold = E().font("goldFont.fnt");
        const float ts = 408.f / std::max(1.f, big->bm.measure("Stereo Madness") * big->scale);
        const float gs = 300.f / std::max(1.f, gold->bm.measure("By ForeverBound") * gold->scale);
        for (int i = 0; i < kSongsListed; ++i) {
            const float cy = kFirst - i * kRowH + scroll_ + dy;
            if (cy < kInBottom + dy - kRowH || cy > kInTop + dy + kRowH) continue;
            E().fillRect(kInL, cy - kRowH / 2, kInR - kInL, kRowH, (i % 2 == 0) ? dark : light, 255);
            const Song& sg = listedSong(i);
            E().drawText(big, sg.title, 282.f, cy + 13.f, ts, Align::Left);
            E().drawText(gold, std::string("By ") + kArtists[sg.artist].name, 282.f, cy - 28.f, gs, Align::Left);
            LabelButton v("View", 940.f, cy + 7.f, 127.f, 62.f, nullptr);
            v.draw();
        }
    }
    void interiorDown(float x, float y) override { downY_ = y; downScroll_ = scroll_; dragging_ = false; down_ = true; (void)x; }
    void interiorMove(float x, float y) override {
        (void)x;
        if (!down_) return;
        if (std::fabs(y - downY_) > 10.f) dragging_ = true;
        if (dragging_) scroll_ = std::max(0.f, std::min(maxScroll(), downScroll_ + (y - downY_)));
    }
    void interiorUp(float x, float y) override {
        if (!down_) return;
        down_ = false;
        if (dragging_) return;
        for (int i = 0; i < kSongsListed; ++i) {
            const float cy = kFirst - i * kRowH + scroll_ + 7.f;
            if (std::fabs(x - 940.f) <= 64.f && std::fabs(y - cy) <= 32.f && inInterior(x, y, 0.f)) { openCard(i); return; }
        }
    }
    void interiorWheel(float dy) override { scroll_ = std::max(0.f, std::min(maxScroll(), scroll_ - dy * 40.f)); }

    bool overlayActive() const override { return cardRow_ >= 0; }
    void drawOverlay() override {
        const Song& sg = listedSong(cardRow_);
        E().fillRect(0, 0, kW, kH, {0, 0, 0}, 150);
        E().drawPanel(E().sprite("GJ_square01.png"), kW / 2.f, kH / 2.f, 640.f, 520.f);
        Font* gold = E().font("goldFont.fnt");
        Font* chat = E().font("chatFont.fnt");
        E().drawText(gold, sg.title, kW / 2.f, 540.f, fitScale(gold, sg.title, 560.f, 0.9f));
        E().drawText(chat, std::string("By: ") + kArtists[sg.artist].name, kW / 2.f, 490.f, 1.1f);
        for (auto& b : card_) b.draw();
        for (auto& b : closeCard_) b.draw();
    }
    void overlayDown(float x, float y) override {
        for (auto& b : card_) if (b.onDown(x, y)) return;
        for (auto& b : closeCard_) if (b.onDown(x, y)) return;
        if (std::fabs(x - kW / 2.f) > 320.f || std::fabs(y - kH / 2.f) > 260.f) cardRow_ = -1;
    }
    void overlayUp(float x, float y) override {
        for (auto& b : card_) b.onUp(x, y);
        for (auto& b : closeCard_) b.onUp(x, y);
    }
    void overlayBack() override { cardRow_ = -1; }                       // SongInfoLayer::keyBackClicked

private:
    void openCard(int row) {
        cardRow_ = row;
        const Song& sg = listedSong(row);
        const Artist& ar = kArtists[sg.artist];
        card_.clear(); card_.reserve(5);
        float y = 410.f;
        auto add = [&](const char* label, const char* url) {
            if (!url) return;
            card_.emplace_back(label, kW / 2.f, y, 420.f, 62.f, [url] { SDL_OpenURL(url); }, "bigFont.fnt", 0.7f);
            y -= 76.f;
        };
        add("Download", sg.url);                 // SongInfoLayer::onDownload -> urlForAudio
        add("Newgrounds", ar.ng);                // onNG
        add("YouTube", ar.yt);                   // onYT
        add("Facebook", ar.fb);                  // onFB
        closeCard_.clear();
    }
    float scroll_ = 0.f, downY_ = 0.f, downScroll_ = 0.f;
    bool down_ = false, dragging_ = false;
    int cardRow_ = -1;
    std::vector<LabelButton> card_, closeCard_;
};

void OptionsBoard::act(int i) {
    if (i == 0) { next = std::make_unique<SupportBoard>(); closed = true; }
    else if (i == 1) popup_ = std::make_unique<Popup>("Rate", "Thanks for playing!\nThere is no store page for the web port.");
    else if (i == 2) { next = std::make_unique<SongsBoard>(); closed = true; }
    else tut_.open();
}

} // namespace

std::unique_ptr<Board> makeSongsBoard() { return std::make_unique<SongsBoard>(); }
std::unique_ptr<Board> makeOptionsBoard() { return std::make_unique<OptionsBoard>(); }

} // namespace ogd
