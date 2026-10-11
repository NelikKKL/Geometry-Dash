// Level select screen. Layout follows the original 1.x screen (measured from a device screenshot).
#include <algorithm>
#include <cmath>
#include <vector>

#include "core/Level.h"
#include "game/Boards.h"
#include "game/Levels.h"
#include "game/Scenes.h"
#include "game/Ui.h"
#include "game/World.h"

namespace ogd {
namespace {

constexpr float kCardX = 640.f, kCardY = 495.f, kCardW = 732.f, kCardH = 212.f;
constexpr float kGroundShift = 93.f;       // floor line at y = 113 on this screen

// Page turn (measured from a screen recording of the real game): the whole page content slides by one page width with
// CCEaseElasticOut, about 1.0 s long, period 0.48 -- it shoots past its place, swings back and settles. The background
// colour follows the slide.
constexpr float kPageD = 1139.f, kTurnTime = 1.0f, kTurnPeriod = 0.48f;
float elasticOut(float t, float p) {
    if (t <= 0.f) return 0.f;
    if (t >= 1.f) return 1.f;
    return std::pow(2.f, -10.f * t) * std::sin((t - p / 4.f) * 2.f * 3.14159265f / p) + 1.f;
}

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
        }
        for (int i = 0; i < kLevelCount; ++i) bg_[i] = ground_[i] = levelPageColor(i);   // the screen ignores the level's own colours
        fromPage_ = page_;
        shownBg_ = bg_[page_];
        shownGround_ = ground_[page_];

        if (E().audioOk && !Mix_PlayingMusic()) E().playMusic("menuLoop.mp3", -1);

        buttons_.emplace_back("GJ_arrow_01_001.png", 54.f, kH - 48.f, 1.f, [] { app().goTo([] { return makeMenuScene(); }); });
        buttons_.emplace_back("navArrowBtn_001.png", 55.f, kH / 2.f, 1.f, [this] { turn(-1); });
        buttons_.back().flipX = true;
        buttons_.emplace_back("navArrowBtn_001.png", kW - 55.f, kH / 2.f, 1.f, [this] { turn(+1); });
    }

    void update(float dt) override {
        for (auto& b : buttons_) b.update(dt);
        if (animT_ < 1.f) animT_ = std::min(1.f, animT_ + dt / kTurnTime);
        const float e = animT_ < 1.f ? std::max(0.f, std::min(1.f, elasticOut(animT_, kTurnPeriod))) : 1.f;
        shownBg_ = lerpColor(bg_[fromPage_], bg_[page_], e);
        shownGround_ = lerpColor(ground_[fromPage_], ground_[page_], e);
        scroll_ += 120.f * dt;
        if (board_) {
            board_->update(dt);
            if (board_->closed) board_.reset();
        }
    }

    void draw() override {
        drawPageBackground();
        drawPageGround();

        drawCornerArt(Corner::BottomLeft);
        drawCornerArt(Corner::BottomRight);
        E().drawSprite(E().sprite("GJ_topBar_001.png"), kW / 2.f, kH - 36.f, 1.09f, 1.09f);

        Font* big = E().font("bigFont.fnt");
        // page content: the page we are leaving is one page width behind the new one
        const float off = animT_ < 1.f ? dir_ * kPageD * (1.f - elasticOut(animT_, kTurnPeriod)) : 0.f;
        if (animT_ < 1.f && std::fabs(off - dir_ * kPageD) < 900.f) drawContent(fromPage_, off - dir_ * kPageD);
        drawContent(page_, off);

        for (auto& b : buttons_) b.draw();

        E().drawText(big, "Download the soundtracks", kW / 2.f, 67.f, fitScale(big, "Download the soundtracks", 497.f, 10.f),
                     Align::Center, {}, 255);
        for (int i = 0; i < kDots; ++i)                       // 8 dots: the last one belongs to the soundtracks page
            E().drawSprite(E().sprite("smallDot.png"), kW / 2.f + (i - (kDots - 1) / 2.f) * 34.4f, 33.f, 1.f, 1.f, 0,
                           {}, i == page_ ? 255 : 110);

        if (board_) board_->draw();
    }

    void onDown(float x, float y) override {
        if (board_) { board_->onDown(x, y); return; }
        if (inSoundtracks(x, y)) { soundtracksPressed_ = true; return; }
        for (auto& b : buttons_) if (b.onDown(x, y)) return;
        if (inCard(x, y)) cardPressed_ = true;
    }
    void onMove(float x, float y) override { if (board_) board_->onMove(x, y); }
    void onWheel(float dy) override { if (board_) board_->onWheel(dy); }
    void onUp(float x, float y) override {
        if (board_) { board_->onUp(x, y); return; }
        if (soundtracksPressed_ && inSoundtracks(x, y)) board_ = makeSongsBoard();   // LevelSelectLayer::onDownload
        soundtracksPressed_ = false;
        for (auto& b : buttons_) b.onUp(x, y);
        if (cardPressed_ && inCard(x, y)) play();
        cardPressed_ = false;
    }
    void onKey(SDL_Keycode k, bool down) override {
        if (board_) { board_->onKey(k, down); return; }
        if (!down) return;
        if (k == SDLK_LEFT || k == SDLK_a) turn(-1);
        else if (k == SDLK_RIGHT || k == SDLK_d) turn(+1);
        else if (k == SDLK_RETURN || k == SDLK_SPACE) play();
        else if (k == SDLK_ESCAPE) app().goTo([] { return makeMenuScene(); });
    }

private:
    // Floor without the dark corner shadows the play screen has.
    void drawPageGround() {
        Sprite g = E().sprite("groundSquare_001.png");
        if (!g || g.w <= 0) return;
        const float bottom = -50.f - kGroundShift, top = bottom + g.h;
        float start = -std::fmod(scroll_, g.w);
        if (start > 0) start -= g.w;
        for (float x = start; x < kW; x += g.w) E().drawSprite(g, x + g.w / 2, bottom + g.h / 2, 1, 1, 0, shownGround_);
        Sprite line = E().sprite("floor.png");
        if (line && line.w > 0) E().drawSprite(line, kW / 2.f, top, std::fmax(1.f, kW / line.w), 1);
    }

    // Smooth vertical gradient in the page colour with the tile pattern only faintly visible (sampled from the real screen).
    void drawPageBackground() {
        Sprite grad = E().sprite("GJ_gradientBG.png");
        const Color c{(Uint8)(shownBg_.r * 0.92f), (Uint8)(shownBg_.g * 0.92f), (Uint8)(shownBg_.b * 0.92f)};
        if (grad && grad.w > 0) E().drawSprite(grad, kW / 2.f, kH / 2.f, kW / grad.w, kH / grad.h, 0, c);
        else E().clear(c);
        Sprite bg = E().sprite("game_bg_01_001.png");
        if (!bg || bg.w <= 0) return;
        float start = -std::fmod(scroll_ * 0.1f, bg.w);
        if (start > 0) start -= bg.w;
        for (float x = start; x < kW; x += bg.w) E().drawSprite(bg, x + bg.w / 2, kH / 2.f, 1, 1, 0, shownBg_, 38);
    }

    static bool inSoundtracks(float x, float y) { return std::fabs(x - kW / 2.f) < 260.f && std::fabs(y - 67.f) < 22.f; }
    static bool inCard(float x, float y) { return std::fabs(x - kCardX) < kCardW / 2 && std::fabs(y - kCardY) < kCardH / 2; }

    void drawContent(int page, float xo) {
        const LevelMeta& m = levelMeta(page);
        const Uint8 a = 255;
        Font* big = E().font("bigFont.fnt");
        // level card: difficulty face + title
        E().drawPanel(E().sprite("square02_001.png"), kCardX + xo, kCardY, kCardW, kCardH, {0, 0, 0}, 110);
        const float ts = fitScale(big, m.name, 545.f, 1.09f);
        const float titleW = big->bm.measure(m.name) * big->scale * ts;
        char face[40];
        std::snprintf(face, sizeof face, "diffIcon_0%d_btn_001.png", m.difficulty);
        E().drawSprite(E().sprite(face), 688.f + xo - titleW / 2.f - 70.f, kCardY, 1.15f, 1.15f, 0, {}, a);   // face sits 70 px left of the centred title
        E().drawText(big, m.name, 688.f + xo, kCardY - 17.f, ts, Align::Center, {}, a);

        // progress bars: scale the label font so that "Normal Mode" is ~236 px wide, as on the device
        const float labelScale = fitScale(big, "Normal Mode", 236.f, 10.f);
        const float pctScale = fitScale(big, "0%", 51.f, 10.f);
        drawMode("Normal Mode", 327.f, 293.f, 283.f, E().save.bestOf(page), labelScale, pctScale, a, {0, 255, 0}, xo);
        drawMode("Practice Mode", 213.f, 180.f, 170.f, E().save.practiceBestOf(page), labelScale, pctScale, a, {0, 255, 255}, xo);

        if (!available_[page])
            E().drawText(E().font("chatFont.fnt"), "Level data not found - repack the APK with tools/akrile-tool.mjs", kW / 2.f + xo, 600.f, 0.8f, Align::Center, {255, 140, 140}, a);
    }

    void drawMode(const char* label, float labelY, float barY, float pctY, int percent, float labelScale, float pctScale, Uint8 a, Color fill, float xo) {
        Font* big = E().font("bigFont.fnt");
        E().drawText(big, label, kW / 2.f + xo, labelY, labelScale, Align::Center, {}, a);
        Sprite bar = E().sprite("GJ_progressBar_001.png");
        const float sx = kCardW / std::max(1.f, bar.w), sy = 45.f / std::max(1.f, bar.h);
        E().drawSprite(bar, kW / 2.f + xo, barY, sx, sy, 0, {0, 0, 0}, 140);
        if (percent > 0) E().drawSpriteCropped(bar, kW / 2.f - kCardW / 2 + xo, barY, percent / 100.f, fill, sx);
        E().drawText(big, std::to_string(percent) + "%", kW / 2.f + xo, pctY, pctScale, Align::Center, {}, a);
    }

    void turn(int d) {                        // the arrows wrap around: Stereo Madness <- -> Jumper
        fromPage_ = page_;
        page_ = (page_ + d + kLevelCount) % kLevelCount;
        dir_ = d;
        animT_ = 0.f;
    }
    void play() {
        if (!available_[page_] || leaving_) return;
        leaving_ = true;
        E().playSfx("playSound_01.ogg");
        const int level = page_;
        app().goTo([level] { return makePlayScene(level); });
    }

    static constexpr int kDots = 8;
    int page_;
    std::unique_ptr<Board> board_;
    bool soundtracksPressed_ = false;
    float animT_ = 1.f, scroll_ = 0;
    int fromPage_ = 0, dir_ = 0;
    bool leaving_ = false, cardPressed_ = false;
    bool available_[kLevelCount] = {};
    Color bg_[kLevelCount], ground_[kLevelCount], shownBg_, shownGround_;
    std::vector<Button> buttons_;
};

} // namespace

std::unique_ptr<Scene> makeLevelSelectScene(int page) { return std::make_unique<LevelSelectScene>(page); }

} // namespace ogd
