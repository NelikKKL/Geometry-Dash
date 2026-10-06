#include "game/Boards.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/Save.h"
#include "game/Levels.h"

namespace ogd {
namespace {

constexpr float kScale = 1.072f;          // board sprites are drawn 7% larger than their texture, as on the device
constexpr float kTopBarY = 632.f, kBottomBarY = 77.f, kChainX0 = 304.f, kChainX1 = 976.f;

} // namespace

bool Board::inInterior(float x, float y, float slideY) {
    return x >= kInL && x <= kInR && y >= kInBottom + slideY && y <= kInTop + slideY;
}

Board::Board(const std::string& title)
    : title_(title), back_("GJ_arrow_03_001.png", 54.f, kH - 48.f, 1.f, [this] { closed = true; }) {}

void Board::update(float dt) {
    back_.update(dt);
    if (t_ < 1.f) {
        t_ = std::min(1.f, t_ + dt / 0.55f);
        slideY_ = (1.f - easeBounceOut(t_)) * 760.f;
    } else {
        slideY_ = 0.f;
    }
    updateInterior(dt);
}

void Board::draw() {
    const float dy = slideY_;

    // chains first: the board hangs from them
    Sprite chain = E().sprite("chain_01_001.png");
    for (float x : {kChainX0, kChainX1}) E().drawSprite(chain, x, kTopBarY + 35.f + chain.h / 2 + dy);

    // interior (clipped)
    E().setClip(kInL, kInBottom + dy, kInR - kInL, kInTop - kInBottom);
    drawInterior();
    E().clearClip();

    // frame
    Sprite side = E().sprite("GJ_table_side_001.png");
    const float sideH = side.h * kScale;
    for (int i = 0; i < 4; ++i) {
        const float y = kInTop - sideH / 2 - i * sideH + 6.f + dy;
        E().drawSprite(side, kInL - 26.f, y, kScale, kScale);
        E().drawSprite(side, kInR + 26.f, y, kScale, kScale, 0, {}, 255, true);
    }
    E().drawSprite(E().sprite("GJ_table_top_001.png"), kW / 2.f, kTopBarY + dy, kScale, kScale);
    E().drawSprite(E().sprite("GJ_table_bottom_001.png"), kW / 2.f, kBottomBarY + dy, kScale, kScale);
    Font* big = E().font("bigFont.fnt");
    E().drawText(big, title_, kW / 2.f, kTopBarY - 12.f + dy, fitScale(big, "Stats", 170.f, 10.f));

    back_.draw();
}

void Board::onDown(float x, float y) {
    if (back_.onDown(x, y)) return;
    if (settled() && inInterior(x, y, 0.f)) interiorDown(x, y);
}
void Board::onUp(float x, float y) {
    back_.onUp(x, y);
    if (settled()) interiorUp(x, y);
}
void Board::onMove(float x, float y) {
    if (settled()) interiorMove(x, y);
}
void Board::onWheel(float dy) {
    if (settled()) interiorWheel(dy);
}
void Board::onKey(SDL_Keycode k, bool down) {
    if (down && k == SDLK_ESCAPE) closed = true;
}

// ================================================================================================= Stats
namespace {

class StatsBoard : public Board {
public:
    StatsBoard() : Board("Stats") {}

protected:
    void drawInterior() override {
        const float dy = slide();
        // alternating brown rows (dark, light, dark, light), the rest of the board is light
        const Color dark{162, 88, 45}, light{194, 114, 63};
        E().fillRect(kInL, kInBottom + dy, kInR - kInL, kInTop - kInBottom, light, 255);
        const float edges[5] = {595.f, 509.f, 419.f, 329.f, 239.f};
        for (int i = 0; i < 4; ++i) {
            E().fillRect(kInL, edges[i + 1] + dy, kInR - kInL, edges[i] - edges[i + 1], (i % 2 == 0) ? dark : light, 255);
            if (i > 0) E().fillRect(kInL, edges[i] + dy - 1.f, kInR - kInL, 2.f, {140, 76, 38}, 255);
        }

        const SaveData& s = E().save;
        struct Row { const char* label; long long value; };
        const Row rows[4] = {{"Total Jumps:", s.totalJumps},
                             {"Total Attempts:", s.totalAttempts},
                             {"Completed Levels:", s.completedLevels(kLevelCount)},
                             {"Completed Online Levels:", 0}};
        const float ys[4] = {540.f, 450.f, 360.f, 270.f};
        Font* gold = E().font("goldFont.fnt");
        const float sc = fitScale(gold, "Total Jumps:", 246.f, 10.f);
        for (int i = 0; i < 4; ++i) {
            E().drawText(gold, rows[i].label, 303.f, ys[i] + dy, sc, Align::Left);
            E().drawText(gold, std::to_string(rows[i].value), 971.f, ys[i] + dy, sc, Align::Right);
        }
    }
};

// ============================================================================================ More Games
// One entry per RobTop game. The banners ship in the APK (promo_*.png). The original asked robtopgames.com for the
// list and its links; none of those games is listed there any more, so every entry opens the official site until a
// verified store link is put into `url`.
struct Game { const char* banner; const char* name; const char* url; };
const Game kGames[] = {
    {"promo_boom.png", "Boomlings", "https://robtopgames.com"},
    {"promo_mu.png", "Boomlings MatchUp", "https://robtopgames.com"},
    {"promo_mm.png", "Memory Mastermind", "https://robtopgames.com"},
};
constexpr int kGameCount = (int)(sizeof kGames / sizeof *kGames);
constexpr float kBannerW = 632.f, kGap = 5.f;

class MoreGamesBoard : public Board {
public:
    MoreGamesBoard() : Board("RobTop Games") {}

protected:
    void updateInterior(float dt) override {
        if (!dragging_) {                                        // kinetic scrolling after a flick
            offset_ += vel_ * dt;
            vel_ *= std::exp(-5.f * dt);
            if (std::fabs(vel_) < 5.f) vel_ = 0.f;
        }
        clampOffset();
    }

    void drawInterior() override {
        const float dy = slide();
        E().fillRect(kInL, kInBottom + dy, kInR - kInL, kInTop - kInBottom, {0, 13, 32}, 255);
        for (int i = 0; i < kGameCount; ++i) {
            const float h = bannerH(i);
            const float top = kInTop - i * (h + kGap) + offset_;
            const float cy = top - h / 2 + dy;
            if (cy + h / 2 < kInBottom + dy || cy - h / 2 > kInTop + dy) continue;     // off screen
            Sprite b = E().sprite(kGames[i].banner);
            const Uint8 a = (pressed_ == i && !moved_) ? 190 : 255;
            if (b) {
                E().drawSprite(b, kW / 2.f, cy, kBannerW / b.w, kBannerW / b.w, 0, {}, a);
            } else {                                                                      // banner art missing from the bundle
                E().fillRect(kW / 2.f - kBannerW / 2, cy - h / 2, kBannerW, h, {20, 60, 110}, a);
                Font* big = E().font("bigFont.fnt");
                E().drawText(big, kGames[i].name, kW / 2.f, cy, fitScale(big, kGames[i].name, 560.f, 1.f));
            }
        }
    }

    void interiorDown(float x, float y) override {
        (void)x;
        dragging_ = true; moved_ = false; vel_ = 0.f;
        downY_ = lastY_ = y;
        pressed_ = itemAt(x, y);
    }
    void interiorMove(float x, float y) override {
        (void)x;
        if (!dragging_) return;
        const float d = y - lastY_;
        offset_ += d;                                            // finger up = content up
        vel_ = d * 60.f;
        lastY_ = y;
        if (std::fabs(y - downY_) > 10.f) moved_ = true;
        clampOffset();
    }
    void interiorUp(float x, float y) override {
        if (!dragging_) return;
        dragging_ = false;
        if (!moved_ && pressed_ >= 0 && itemAt(x, y) == pressed_) open(pressed_);
        pressed_ = -1;
    }
    void interiorWheel(float dy) override {
        offset_ -= dy * 60.f;                                    // wheel up = towards the top
        vel_ = 0.f;
        clampOffset();
    }

private:
    float bannerH(int i) const {
        Sprite b = E().sprite(kGames[i].banner);
        return b ? b.h * (kBannerW / b.w) : 246.f;
    }
    float contentH() const {
        float h = 0.f;
        for (int i = 0; i < kGameCount; ++i) h += bannerH(i) + (i ? kGap : 0.f);
        return h;
    }
    void clampOffset() {
        const float maxOff = std::max(0.f, contentH() - (kInTop - kInBottom));
        if (offset_ < 0.f) { offset_ = 0.f; vel_ = 0.f; }
        if (offset_ > maxOff) { offset_ = maxOff; vel_ = 0.f; }
    }
    int itemAt(float x, float y) const {
        if (!inInterior(x, y, 0.f) || std::fabs(x - kW / 2.f) > kBannerW / 2) return -1;
        for (int i = 0; i < kGameCount; ++i) {
            const float h = bannerH(i);
            const float top = kInTop - i * (h + kGap) + offset_;
            if (y <= top && y >= top - h) return i;
        }
        return -1;
    }
    void open(int i) { SDL_OpenURL(kGames[i].url); }

    float offset_ = 0.f, vel_ = 0.f, downY_ = 0.f, lastY_ = 0.f;
    bool dragging_ = false, moved_ = false;
    int pressed_ = -1;
};

} // namespace

std::unique_ptr<Board> makeStatsBoard() { return std::make_unique<StatsBoard>(); }
std::unique_ptr<Board> makeMoreGamesBoard() { return std::make_unique<MoreGamesBoard>(); }

} // namespace ogd
