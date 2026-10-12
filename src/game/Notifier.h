// AchievementNotifier: the brown banner that slides down from the top when an achievement is earned.
// Lives above every scene (App draws it last), shows queued banners one after another.
#pragma once
#include <string>
#include <vector>

namespace ogd {

class Notifier {
public:
    // Earn `id` (no-op when already earned or unknown) and queue its banner; delay = seconds before it appears.
    void earn(const std::string& id, float delay = 0.f);
    // Queue a banner for an achievement that was already recorded in the save (progress achievements).
    void announce(const std::string& id, float delay = 0.f);
    void update(float dt);
    void draw();

private:
    struct Item { std::string id; float delay; };
    std::vector<Item> queue_;
    std::string current_;
    float t_ = 0.f;
    bool active_ = false;
};

Notifier& notifier();

// Checks jump / attempt counters and announces what they earned (call after the counters change).
void checkProgressAchievements();

} // namespace ogd
