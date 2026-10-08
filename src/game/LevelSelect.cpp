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
        fade_ = std::min(1.f, fade_ + dt / 0.25f);
        const float k = std::min(1.f, dt * 8.f);
        shownBg_ = lerpColor(shownBg_, bg_[page_], k);
        shownGround_ = lerpColor(shownGround_, ground_[page_], k);
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

        const LevelMeta& m = levelMeta(page_);
        const Uint8 a = (Uint8)(fade_ * 255);
        Font* big = E().font("bigFont.fnt");

        // level card: difficulty face + title
        E().drawPanel(E().sprite("square02_001.png"), kCardX, kCardY, kCardW, kCardH, {0, 0, 0}, (Uint8)(110 * fade_));
        const float ts = fitScale(big, m.name, 545.f, 1.09f);
        const float titleW = big->bm.measure(m.name) * big->scale * ts;
        char face[40];
        std::snprintf(face, sizeof face, "diffIcon_0%d_btn_001.png", m.difficulty);
        E().drawSprite(E().sprite(face), 688.f - titleW / 2.f - 70.f, kCardY, 1.15f, 1.15f, 0, {}, a);   // face sits 70 px left of the centred title
        E().drawText(big, m.name, 688.f, kCardY - 17.f, ts, Align::Center, {}, a);

        // progress bars: scale the label font so that "Normal Mode" is ~236 px wide, as on the device
        const float labelScale = fitScale(big, "Normal Mode", 236.f, 10.f);
        const float pctScale = fitScale(big, "0%", 51.f, 10.f);
        drawMode("Normal Mode", 327.f, 293.f, 283.f, E().save.bestOf(page_), labelScale, pctScale, a);
        drawMode("Practice Mode", 213.f, 180.f, 170.f, 0, labelScale, pctScale, a);

        if (!available_[page_])
            E().drawText(E().font("chatFont.fnt"), "Level data not found - repack the APK with tools/akrile-tool.mjs", kW / 2.f, 600.f, 0.8f, Align::Center, {255, 140, 140}, a);

        for (auto& b : buttons_) b.draw();

        E().drawText(big, "Download the soundtracks", kW / 2.f, 67.f, fitScale(big, "Download the soundtracks", 497.f, 10.f),
                     Align::Center, {}, a);
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

    void drawMode(const char* label, float labelY, float barY, float pctY, int percent, float labelScale, float pctScale, Uint8 a) {
        Font* big = E().font("bigFont.fnt");
        E().drawText(big, label, kW / 2.f, labelY, labelScale, Align::Center, {}, a);
        Sprite bar = E().sprite("GJ_progressBar_001.png");
        const float sx = kCardW / std::max(1.f, bar.w), sy = 45.f / std::max(1.f, bar.h);
        E().drawSprite(bar, kW / 2.f, barY, sx, sy, 0, {0, 0, 0}, (Uint8)(140 * fade_));
        if (percent > 0) E().drawSpriteCropped(bar, kW / 2.f - kCardW / 2, barY, percent / 100.f, {0, 255, 0}, sx);
        E().drawText(big, std::to_string(percent) + "%", kW / 2.f, pctY, pctScale, Align::Center, {}, a);
    }

    void turn(int d) {                        // the arrows wrap around: Stereo Madness <- -> Jumper
        page_ = (page_ + d + kLevelCount) % kLevelCount;
        fade_ = 0.f;
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
    float fade_ = 1.f, scroll_ = 0;
    bool leaving_ = false, cardPressed_ = false;
    bool available_[kLevelCount] = {};
    Color bg_[kLevelCount], ground_[kLevelCount], shownBg_, shownGround_;
    std::vector<Button> buttons_;
};

} // namespace

std::unique_ptr<Scene> makeLevelSelectScene(int page) { return std::make_unique<LevelSelectScene>(page); }

} // namespace ogd
