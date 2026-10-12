#include "game/Notifier.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "core/Achievements.h"
#include "core/Palette.h"
#include "game/Ui.h"

namespace ogd {
namespace {
float easeOutCubic(float t) { t = std::max(0.f, std::min(1.f, t)); return 1.f - (1.f - t) * (1.f - t) * (1.f - t); }
constexpr float kIn = 0.4f, kHold = 2.6f, kOut = 0.4f;
} // namespace

Notifier& notifier() {
    static Notifier n;
    return n;
}

void Notifier::earn(const std::string& id, float delay) {
    if (!findAchievement(id)) return;
    if (E().save.earnAchievement(id)) {
        E().persist();
        announce(id, delay);
    }
}

void Notifier::announce(const std::string& id, float delay) {
    if (!findAchievement(id)) return;
    queue_.push_back({id, delay});
}

void Notifier::update(float dt) {
    if (!active_) {
        for (auto& q : queue_) q.delay -= dt;                       // delays count down while waiting too
        if (!queue_.empty() && queue_.front().delay <= 0.f) {
            current_ = queue_.front().id;
            queue_.erase(queue_.begin());
            active_ = true;
            t_ = 0.f;
            E().playSfx("achievement_01.ogg");
        }
        return;
    }
    t_ += dt;
    if (t_ >= kIn + kHold + kOut) active_ = false;
}

void Notifier::draw() {
    if (!active_) return;
    const Achievement* a = findAchievement(current_);
    if (!a) return;
    const float slide = t_ < kIn ? easeOutCubic(t_ / kIn) : (t_ > kIn + kHold ? 1.f - easeOutCubic((t_ - kIn - kHold) / kOut) : 1.f);
    const float cx = kW / 2.f, h = 120.f, cy = kH + h / 2.f - (h + 6.f) * slide;
    E().drawPanel(E().sprite("GJ_square01.png"), cx, cy, 625.f, h);
    const float ix = cx - 235.f;
    if (a->icon.rfind("icon_", 0) == 0) {
        char nm[48], nm2[48];
        std::snprintf(nm, sizeof nm, "player_%02d_001.png", std::atoi(a->icon.c_str() + 5));
        std::snprintf(nm2, sizeof nm2, "player_%02d_2_001.png", std::atoi(a->icon.c_str() + 5));
        const Rgb m = paletteRgb(E().save.mainColor), s = paletteRgb(E().save.secondaryColor);
        E().drawSprite(E().sprite(nm2), ix, cy, 1.1f, 1.1f, 0, {s.r, s.g, s.b});
        E().drawSprite(E().sprite(nm), ix, cy, 1.1f, 1.1f, 0, {m.r, m.g, m.b});
    } else {
        const std::string tail = a->icon.substr(a->icon.find('_') + 1);                    // color_05 / color2_5 -> palette index 4
        const Rgb c = paletteRgb(std::max(0, std::atoi(tail.c_str()) - 1));
        E().fillRect(ix - 34.f, cy - 34.f, 68.f, 68.f, {255, 255, 255}, 255);
        E().fillRect(ix - 30.f, cy - 30.f, 60.f, 60.f, {c.r, c.g, c.b}, 255);
    }
    Font* big = E().font("bigFont.fnt");
    E().drawText(big, a->title, cx - 175.f, cy + 17.f, fitScale(big, a->title, 370.f, 0.85f), Align::Left);
    E().drawText(E().font("chatFont.fnt"), rewardText(*a), cx - 175.f, cy - 22.f, 1.1f, Align::Left);
}

void checkProgressAchievements() {
    const auto got = earnProgressAchievements(E().save);
    for (const std::string& id : got) notifier().announce(id);
    if (!got.empty()) E().persist();
}

} // namespace ogd
