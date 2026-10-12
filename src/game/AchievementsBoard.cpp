// Achievements list (AchievementsLayer): every achievement of AchievementsDesc.plist with its reward icon,
// title and description (what was done / what to do). Opened from the Achievements button of the main menu.
#include <algorithm>
#include <cmath>

#include "core/Achievements.h"
#include "core/Palette.h"
#include "game/Boards.h"

namespace ogd {
namespace {

class AchievementsBoard : public Board {
public:
    AchievementsBoard() : Board("Achievements") {
        list_ = allAchievements();
        std::sort(list_.begin(), list_.end(), [](const Achievement& a, const Achievement& b) { return a.order < b.order; });
    }

protected:
    static constexpr float kRowH = 110.f;
    float firstY() const { return kInTop - kRowH / 2; }
    float maxScroll() const { return std::max(0.f, (float)list_.size() * kRowH - (kInTop - kInBottom)); }

    void drawInterior() override {
        const float dy = slide();
        const Color dark{162, 88, 45}, light{194, 114, 63};
        E().fillRect(kInL, kInBottom + dy, kInR - kInL, kInTop - kInBottom, light, 255);
        Font* big = E().font("bigFont.fnt");
        Font* chat = E().font("chatFont.fnt");
        const Rgb mc = paletteRgb(E().save.mainColor), sc = paletteRgb(E().save.secondaryColor);
        for (size_t i = 0; i < list_.size(); ++i) {
            const Achievement& a = list_[i];
            const float cy = firstY() - (float)i * kRowH + scroll_ + dy;
            if (cy < kInBottom + dy - kRowH || cy > kInTop + dy + kRowH) continue;
            const bool done = E().save.hasAchievement(a.id);
            E().fillRect(kInL, cy - kRowH / 2, kInR - kInL, kRowH, (i % 2 == 0) ? dark : light, 255);
            // reward icon (grey and locked until earned)
            const float ix = 330.f;
            const Uint8 al = done ? 255 : 110;
            if (a.icon.rfind("icon_", 0) == 0) {
                char n1[48], n2[48];
                std::snprintf(n1, sizeof n1, "player_%02d_001.png", std::atoi(a.icon.c_str() + 5));
                std::snprintf(n2, sizeof n2, "player_%02d_2_001.png", std::atoi(a.icon.c_str() + 5));
                E().drawSprite(E().sprite(n2), ix, cy, 1.f, 1.f, 0, done ? Color{sc.r, sc.g, sc.b} : Color{110, 110, 110}, al);
                E().drawSprite(E().sprite(n1), ix, cy, 1.f, 1.f, 0, done ? Color{mc.r, mc.g, mc.b} : Color{110, 110, 110}, al);
            } else {
                const Rgb c = paletteRgb(std::max(0, std::atoi(a.icon.c_str() + a.icon.find('_') + 1) - 1));
                E().fillRect(ix - 30.f, cy - 30.f, 60.f, 60.f, {255, 255, 255}, al);
                E().fillRect(ix - 27.f, cy - 27.f, 54.f, 54.f, done ? Color{c.r, c.g, c.b} : Color{90, 90, 90}, al);
            }
            if (!done) E().drawSprite(E().sprite("GJ_lockGray_001.png"), ix + 24.f, cy - 24.f, 0.8f, 0.8f);
            const std::string& desc = done ? a.achievedDesc : a.unachievedDesc;
            E().drawText(big, a.title, 390.f, cy + 20.f, fitScale(big, a.title, 560.f, 0.7f), Align::Left,
                         done ? Color{255, 255, 255} : Color{200, 200, 200});
            E().drawText(chat, desc, 390.f, cy - 20.f, fitScale(chat, desc, 590.f, 1.0f), Align::Left,
                         done ? Color{255, 230, 90} : Color{235, 235, 235});
        }
        // progress counter under the title bar
        int n = 0;
        for (auto& a : list_) if (E().save.hasAchievement(a.id)) ++n;
        E().drawText(chat, std::to_string(n) + "/" + std::to_string(list_.size()), kInR - 14.f, kInBottom + 14.f + dy, 0.9f, Align::Right,
                     {255, 255, 255});
    }
    void interiorDown(float x, float y) override { (void)x; downY_ = y; downScroll_ = scroll_; down_ = true; }
    void interiorMove(float x, float y) override {
        (void)x;
        if (down_) scroll_ = std::max(0.f, std::min(maxScroll(), downScroll_ + (y - downY_)));
    }
    void interiorUp(float x, float y) override { (void)x; (void)y; down_ = false; }
    void interiorWheel(float dy) override { scroll_ = std::max(0.f, std::min(maxScroll(), scroll_ - dy * 40.f)); }

private:
    std::vector<Achievement> list_;
    float scroll_ = 0.f, downY_ = 0.f, downScroll_ = 0.f;
    bool down_ = false;
};

} // namespace

std::unique_ptr<Board> makeAchievementsBoard() { return std::make_unique<AchievementsBoard>(); }

} // namespace ogd
