#include "game/Scenes.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

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
        E().drawSpriteCropped(bar, left, gy, frac * ((groove.w - 8) / std::fmax(1.f, bar.w)));
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
            if (Mix_Music* m = E().music("menuLoop.mp3")) { Mix_VolumeMusic(MIX_MAX_VOLUME / 2); Mix_PlayMusic(m, -1); }
        }
        respawn();

        // main row: garage / play / creator. Replicates cocos layout: Menu(scale 1.13 @ centre+(82,47)) >
        // inner menu(scale 0.9 @ (-70,0)) > items aligned horizontally with padding 50.
        const char* names[3] = {"GJ_garageBtn_001.png", "GJ_playBtn_001.png", "GJ_creatorBtn_001.png"};
        float widths[3], total = 50.f * 2;
        for (int i = 0; i < 3; ++i) total += (widths[i] = E().sprite(names[i]).srcW);
        float cursor = -total / 2, lx[3];
        for (int i = 0; i < 3; ++i) { lx[i] = cursor + widths[i] / 2; cursor += widths[i] + 50; }
        auto toScreen = [](float mx, float my) { return std::pair<float, float>(kW / 2.f + 82 + 1.13f * mx, kH / 2.f + 47 + 1.13f * my); };

        for (int i = 0; i < 3; ++i) {
            auto p = toScreen(-70 + 0.9f * lx[i], 0);
            std::function<void()> cb;
            if (i == 1) cb = [] { app().goTo([] { return makePlayScene(); }); };
            else if (i == 0) cb = [this] { alert("Garage", "Icon garage is not implemented yet."); };
            else cb = [this] { alert("Creator", "The level editor is not implemented yet."); };
            buttons_.emplace_back(names[i], p.first, p.second, 0.9f * 1.13f, cb);
        }
        auto c1 = toScreen(lx[0] - 70, -70), c2 = toScreen(lx[2] + 50, -50);
        deco_.push_back({"GJ_chrSel_001.png", c1.first, c1.second, 1.13f});
        deco_.push_back({"GJ_lvlEdit_001.png", c2.first, c2.second, 1.13f});

        // bottom row
        const char* bn[3] = {"GJ_achBtn_001.png", "GJ_optionsBtn_001.png", "GJ_statsBtn_001.png"};
        const char* bt[3] = {"Achievements", "Options", "Stats"};
        float bw[3], btotal = 10.f * 2;
        for (int i = 0; i < 3; ++i) btotal += (bw[i] = E().sprite(bn[i]).srcW);
        float bc = kW / 2.f - 30 - btotal / 2;
        for (int i = 0; i < 3; ++i) {
            std::string t = bt[i];
            buttons_.emplace_back(bn[i], bc + bw[i] / 2, 100.f, 1.f, [this, t] { alert(t, "Not implemented yet."); });
            bc += bw[i] + 10;
        }
        buttons_.emplace_back("robtoplogo_small.png", 170.f, 90.f, 0.8f * 1.13f, [] { SDL_OpenURL("http://www.robtopgames.com"); });
        buttons_.emplace_back("GJ_moreGamesBtn_001.png", kW - 156.f, 130.f, 0.9f * 1.13f, [this] { alert("More games", "Coming soon."); });
    }

    void update(float dt) override {
        if (popup_) {
            popup_->update(dt);
            if (popup_->closed) popup_.reset();
            return;
        }
        for (auto& b : buttons_) b.update(dt);

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
        if (popup_) popup_->draw();
    }

    void onDown(float x, float y) override {
        if (popup_) { popup_->onDown(x, y); return; }
        for (auto& b : buttons_) if (b.onDown(x, y)) break;
    }
    void onUp(float x, float y) override {
        if (popup_) { popup_->onUp(x, y); return; }
        for (auto& b : buttons_) b.onUp(x, y);
    }

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
    PlayerState player_;
    int cube_ = 1;
    Color main_, sec_;
    float scroll_ = 0, bgScroll_ = 0, jumpTimer_ = 1.5f, holdTime_ = 0;
};

// ======================================================================= Play
class PlayScene : public Scene {
public:
    PlayScene() : back_("GJ_arrow_01_001.png", 50.f, kH - 50.f, 1.f, [] { app().goTo([] { return makeMenuScene(); }); }) {
        player_.x = 0;
        main_ = paletteColor(E().save.mainColor);
        sec_ = paletteColor(E().save.secondaryColor);
        if (E().audioOk) Mix_HaltMusic();
    }

    void update(float dt) override {
        back_.update(dt);
        stepPlayer(player_, dt * 60.0);
        camX_ = (float)player_.x - kStartOffset;
    }

    void draw() override {
        drawBackground(camX_ * 0.1f);
        drawGround(camX_);
        // "Attempt 1" label lives in world space near the start
        E().drawText(E().font("bigFont.fnt"), "Attempt 1", 940.f - camX_, 470.f, 1.0f);
        drawCube(std::min(13, std::max(1, E().save.cube)), player_, camX_, main_, sec_);
        back_.draw();
    }

    void onDown(float x, float y) override {
        if (back_.onDown(x, y)) return;
        player_.holding = true;
    }
    void onUp(float x, float y) override {
        back_.onUp(x, y);
        player_.holding = false;
    }
    void onKey(SDL_Keycode k, bool down) override {
        if (k == SDLK_SPACE || k == SDLK_UP || k == SDLK_w) player_.holding = down;
        else if (k == SDLK_ESCAPE && down) app().goTo([] { return makeMenuScene(); });
    }

private:
    static constexpr float kStartOffset = 380.f;  // player's on-screen X
    Button back_;
    PlayerState player_;
    Color main_, sec_;
    float camX_ = -kStartOffset;
};

} // namespace

std::unique_ptr<Scene> makeLoadingScene() { return std::make_unique<LoadingScene>(); }
std::unique_ptr<Scene> makeMenuScene() { return std::make_unique<MenuScene>(); }
std::unique_ptr<Scene> makePlayScene() { return std::make_unique<PlayScene>(); }

} // namespace ogd
