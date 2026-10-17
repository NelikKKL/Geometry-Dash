#include "game/Scenes.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "game/Boards.h"
#include "game/Notifier.h"
#include "game/Popup.h"
#include "game/Ui.h"
#include "game/World.h"

namespace ogd {
namespace {

// ===================================================================== Loading
class LoadingScene : public Scene {
public:
    LoadingScene() {
        E().loadSheet("GJ_LaunchSheet.plist");
        for (auto& n : E().sheetNames())
            if (n != "GJ_LaunchSheet.plist") queue_.push_back(n);
        static const char* splashes[] = {
            "Listen to the music to help time your jumps", "Back for more are ya?",
            "Use practice mode to learn the layout of a level", "Build your own levels using the level editor",
            "Go online to play other players levels!", "If at first you don't succeed, try, try again...",
            "Can you beat them all?", "Customize your character's icon and color!",
            "Spikes are not your friends, don't forget to jump",
            "Unlock new icons and colors by completing achievements!"};
        splash_ = splashes[rand() % (sizeof splashes / sizeof *splashes)];
    }

    void update(float dt) override {
        time_ += dt;
        Uint32 start = SDL_GetTicks();
        while (next_ < queue_.size() && SDL_GetTicks() - start < 12) E().loadSheet(queue_[next_++]);
        if (next_ >= queue_.size() && time_ > 0.4f && !leaving_) {
            leaving_ = true;
            app().goTo([] { return makeMenuScene(); });
        }
    }

    void draw() override {
        E().clear(kBlue);
        drawBackground(0);
        E().drawSprite(E().sprite("GJ_logo_001.png"), kW / 2.f, kH / 2.f, 1.05f, 1.05f);
        E().drawSprite(E().sprite("RobTopLogoBig_001.png"), kW / 2.f, kH / 2.f + 190);
        E().drawText(E().font("goldFont.fnt"), splash_, kW / 2.f, kH / 2.f - 200, 0.7f);

        Sprite groove = E().sprite("slidergroove.png");
        Sprite bar = E().sprite("sliderBar.png");
        const float gy = kH / 2.f - 200 + 70;
        E().drawSprite(groove, kW / 2.f, gy);
        float frac = queue_.empty() ? 1.f : (float)next_ / (float)queue_.size();
        float left = kW / 2.f - groove.w / 2 + 4;
        E().drawSpriteTiledX(bar, left, gy, (groove.w - 8) * frac);
    }

private:
    std::vector<std::string> queue_;
    size_t next_ = 0;
    float time_ = 0;
    bool leaving_ = false;
    std::string splash_;
};

// ======================================================================= Menu
class MenuScene : public Scene {
public:
    MenuScene() {
        if (E().audioOk) {
            E().playMusic("menuLoop.mp3", -1, MIX_MAX_VOLUME / 2);
        }
        respawn();

        // Layout measured on the real 1.0 main menu (Android screenshot, 1.86:1 viewport mapped onto 1280x720):
        // garage / play / creator sit on one row left-centre-right of the screen centre, the bottom row of three
        // round buttons is centred, RobTop's logo and "More games" hug the screen edges.
        struct Item { const char* spr; float x, y; std::function<void()> cb; };
        const Item mainRow[3] = {
            {"GJ_garageBtn_001.png", 403.f, 382.f, [] {
                 E().save.visitedGarage = true; E().persist();
                 E().playSfx("playSound_01.ogg"); app().goTo([] { return makeGarageScene(); }); }},
            {"GJ_playBtn_001.png", 641.f, 383.f, [] {
                 E().playSfx("playSound_01.ogg"); app().goTo([] { return makeLevelSelectScene(0); }); }},
            {"GJ_creatorBtn_001.png", 877.f, 382.f, [] {
                 E().save.visitedCreator = true; E().persist();
                 E().playSfx("playSound_01.ogg"); app().goTo([] { return makeCreatorScene(); }); }},
        };
        for (const Item& it : mainRow) buttons_.emplace_back(it.spr, it.x, it.y, 0.9f * 1.13f, it.cb);
        // first-run hints; each one disappears once its button has been used
        if (!E().save.visitedGarage) deco_.push_back({"GJ_chrSel_001.png", 296.f, 272.f, 1.13f});
        if (!E().save.visitedCreator) deco_.push_back({"GJ_lvlEdit_001.png", 987.f, 272.f, 1.13f});

        // bottom row: achievements / options / stats
        const char* bn[3] = {"GJ_achBtn_001.png", "GJ_optionsBtn_001.png", "GJ_statsBtn_001.png"};
        const float bx[3] = {519.f, 640.f, 761.f};
        for (int i = 0; i < 3; ++i) {
            std::function<void()> cb;
            if (i == 0) cb = [this] { board_ = makeAchievementsBoard(); };
            else if (i == 1) cb = [this] { board_ = makeOptionsBoard(); };
            else cb = [this] { board_ = makeStatsBoard(); };
            buttons_.emplace_back(bn[i], bx[i], 101.f, 1.f, cb);
        }
        buttons_.emplace_back("robtoplogo_small.png", 127.f, 67.f, 0.8f * 1.13f, [] { SDL_OpenURL("http://www.robtopgames.com"); });
        buttons_.emplace_back("GJ_moreGamesBtn_001.png", 1188.f, 100.f, 0.9f * 1.13f, [this] { board_ = makeMoreGamesBoard(); notifier().earn("geometry.ach.moreGames"); });
    }

    void update(float dt) override {
        if (popup_) {
            popup_->update(dt);
            if (popup_->closed) popup_.reset();
            return;
        }
        if (board_) {
            board_->update(dt);
            if (board_->closed) {
                std::unique_ptr<Board> n = std::move(board_->next);   // Options -> Support / Soundtracks
                board_ = std::move(n);
            }
        } else {
            for (auto& b : buttons_) b.update(dt);
        }

        scroll_ += 623.f * dt;
        bgScroll_ += 62.f * dt;
        jumpTimer_ -= dt;
        if (jumpTimer_ <= 0) { holdTime_ = 0.12f; jumpTimer_ = 1.2f + (rand() % 200) / 100.f; }
        player_.holding = holdTime_ > 0;
        holdTime_ -= dt;
        stepPlayer(player_, dt * 60.0);
        if (player_.x > kW + 100) respawn();
    }

    void draw() override {
        drawBackground(bgScroll_);
        drawGround(scroll_);
        drawCube(cube_, player_, 0, main_, sec_);

        E().drawSprite(E().sprite("GJ_logo_001.png"), kW / 2.f, kH - 110.f);
        for (auto& d : deco_) E().drawSprite(E().sprite(d.spr), d.x, d.y, d.s, d.s);
        for (auto& b : buttons_) b.draw();
        if (board_) board_->draw();
        if (popup_) popup_->draw();
    }

    void onDown(float x, float y) override {
        if (popup_) { popup_->onDown(x, y); return; }
        if (board_) { board_->onDown(x, y); return; }
        for (auto& b : buttons_) if (b.onDown(x, y)) break;
    }
    void onUp(float x, float y) override {
        if (popup_) { popup_->onUp(x, y); return; }
        if (board_) { board_->onUp(x, y); return; }
        for (auto& b : buttons_) b.onUp(x, y);
    }
    void onMove(float x, float y) override { if (board_) board_->onMove(x, y); }
    void onWheel(float dy) override { if (board_) board_->onWheel(dy); }
    void onKey(SDL_Keycode k, bool down) override { if (board_) board_->onKey(k, down); }

private:
    struct Deco { std::string spr; float x, y, s; };

    void alert(const std::string& t, const std::string& m) { popup_ = std::make_unique<Popup>(t, m); }

    void respawn() {
        player_ = PlayerState();
        player_.x = -300;
        cube_ = 1 + rand() % 13;
        main_ = randomColor();
        sec_ = randomColor();
    }

    std::vector<Button> buttons_;
    std::vector<Deco> deco_;
    std::unique_ptr<Popup> popup_;
    std::unique_ptr<Board> board_;
    PlayerState player_;
    int cube_ = 1;
    Color main_, sec_;
    float scroll_ = 0, bgScroll_ = 0, jumpTimer_ = 1.5f, holdTime_ = 0;
};

} // namespace

std::unique_ptr<Scene> makeLoadingScene() { return std::make_unique<LoadingScene>(); }
std::unique_ptr<Scene> makeMenuScene() { return std::make_unique<MenuScene>(); }

} // namespace ogd
