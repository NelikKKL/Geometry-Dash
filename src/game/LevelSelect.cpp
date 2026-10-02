// Level select screen: one page per official level, arrows / swipe-free navigation, best progress bar.
#include <algorithm>
#include <cmath>
#include <vector>

#include "core/Level.h"
#include "game/Levels.h"
#include "game/Scenes.h"
#include "game/Ui.h"
#include "game/World.h"

namespace ogd {
namespace {

Color lerpColor(Color a, Color b, float t) {
    auto m = [&](Uint8 x, Uint8 y) { return (Uint8)(x + (y - x) * t); };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b)};
}

class LevelSelectScene : public Scene {
public:
    explicit LevelSelectScene(int page) : page_(std::max(0, std::min(kLevelCount - 1, page))) {
        // read the level headers once for the per-page colours; also tells us which level files exist
        for (int i = 0; i < kLevelCount; ++i) {
            std::string txt;
            bg_[i] = ground_[i] = kBlue;
            if (!E().readText(levelFile(i), txt)) continue;
            LevelParseResult r = parseLevelString(txt);
            if (!r.ok) continue;
            available_[i] = true;
            if (r.level.settings.hasBackground) {
                const auto& c = r.level.settings.background;
                bg_[i] = {c.r, c.g, c.b};
            }
            if (r.level.settings.hasGround) {
                const auto& c = r.level.settings.ground;
                ground_[i] = {c.r, c.g, c.b};
            }
        }
        shownBg_ = bg_[page_];
        shownGround_ = ground_[page_];

        if (E().audioOk && !Mix_PlayingMusic()) E().playMusic("menuLoop.mp3", -1);

        buttons_.emplace_back("GJ_arrow_01_001.png", 55.f, kH - 50.f, 1.f, [] { app().goTo([] { return makeMenuScene(); }); });
        buttons_.emplace_back("navArrowBtn_001.png", 70.f, kH / 2.f, 1.f, [this] { turn(-1); });
        buttons_.back().flipX = true;
        buttons_.back().tint = {160, 255, 40};
        buttons_.emplace_back("navArrowBtn_001.png", kW - 70.f, kH / 2.f, 1.f, [this] { turn(+1); });
        buttons_.back().tint = {160, 255, 40};
        buttons_.emplace_back("GJ_playBtn2_001.png", kW / 2.f, 292.f, 0.62f, [this] { play(); });
    }

    void update(float dt) override {
        for (auto& b : buttons_) b.update(dt);
        fade_ = std::min(1.f, fade_ + dt / 0.25f);
        const float k = std::min(1.f, dt * 8.f);
        shownBg_ = lerpColor(shownBg_, bg_[page_], k);
        shownGround_ = lerpColor(shownGround_, ground_[page_], k);
        scroll_ += 120.f * dt;
    }

    void draw() override {
        drawBackground(scroll_ * 0.1f, shownBg_);
        drawGround(scroll_, shownGround_);

        const LevelMeta& m = levelMeta(page_);
        const Uint8 a = (Uint8)(fade_ * 255);

        // card
        E().drawPanel(E().sprite("square02_001.png"), kW / 2.f, 400.f, 780.f, 330.f, {0, 0, 0}, 150);
        E().drawText(E().font("bigFont.fnt"), m.name, kW / 2.f, 520.f, 0.95f, Align::Center, {}, a);

        char face[40];
        std::snprintf(face, sizeof face, "difficulty_0%d_btn_001.png", m.difficulty);
        E().drawSprite(E().sprite(face), 410.f, 405.f, 1.35f, 1.35f, 0, {}, a);

        // best progress (normal mode)
        const int best = E().save.bestOf(page_);
        const float cx = 800.f, cy = 402.f, sc = 0.62f;
        Sprite groove = E().sprite("GJ_progressBar_001.png");
        const float w = groove.w * sc;
        E().drawText(E().font("goldFont.fnt"), "Normal Mode", cx, cy + 42.f, 0.7f, Align::Center, {}, a);
        E().drawSprite(groove, cx, cy, sc, sc, 0, {0, 0, 0}, (Uint8)(120 * fade_));
        if (best > 0) {
            E().drawSpriteCropped(groove, cx - w / 2, cy, best / 100.f, {0, 255, 0}, sc);
        }
        char pct[16];
        std::snprintf(pct, sizeof pct, "%d%%", best);
        E().drawText(E().font("bigFont.fnt"), pct, cx, cy + 1.f, 0.5f, Align::Center, {}, a);

        if (!available_[page_])
            E().drawText(E().font("chatFont.fnt"), "Level data not found - repack the APK with tools/akrile-tool.mjs", kW / 2.f, 190.f, 0.8f, Align::Center, {255, 120, 120}, a);

        for (auto& b : buttons_) b.draw();

        // page dots
        for (int i = 0; i < kLevelCount; ++i)
            E().drawSprite(E().sprite("smallDot.png"), kW / 2.f + (i - (kLevelCount - 1) / 2.f) * 34.f, 150.f, 1.f, 1.f, 0,
                           {}, i == page_ ? 255 : 100);
    }

    void onDown(float x, float y) override {
        for (auto& b : buttons_) if (b.onDown(x, y)) return;
        // tapping the card also starts the level
        if (std::fabs(x - kW / 2.f) < 390 && std::fabs(y - 400.f) < 165) cardPressed_ = true;
    }
    void onUp(float x, float y) override {
        for (auto& b : buttons_) b.onUp(x, y);
        if (cardPressed_ && std::fabs(x - kW / 2.f) < 390 && std::fabs(y - 400.f) < 165) play();
        cardPressed_ = false;
    }
    void onKey(SDL_Keycode k, bool down) override {
        if (!down) return;
        if (k == SDLK_LEFT || k == SDLK_a) turn(-1);
        else if (k == SDLK_RIGHT || k == SDLK_d) turn(+1);
        else if (k == SDLK_RETURN || k == SDLK_SPACE) play();
        else if (k == SDLK_ESCAPE) app().goTo([] { return makeMenuScene(); });
    }

private:
    void turn(int d) {
        const int n = std::max(0, std::min(kLevelCount - 1, page_ + d));
        if (n == page_) return;
        page_ = n;
        fade_ = 0.f;
    }
    void play() {
        if (!available_[page_] || leaving_) return;
        leaving_ = true;
        E().playSfx("playSound_01.ogg");
        const int level = page_;
        app().goTo([level] { return makePlayScene(level); });
    }

    int page_;
    float fade_ = 1.f, scroll_ = 0;
    bool leaving_ = false, cardPressed_ = false;
    bool available_[kLevelCount] = {};
    Color bg_[kLevelCount], ground_[kLevelCount], shownBg_, shownGround_;
    std::vector<Button> buttons_;
};

} // namespace

std::unique_ptr<Scene> makeLevelSelectScene(int page) { return std::make_unique<LevelSelectScene>(page); }

} // namespace ogd
