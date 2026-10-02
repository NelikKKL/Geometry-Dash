#include "core/Save.h"
#include <algorithm>
#include <fstream>
#include "json.hpp"

namespace ogd {

bool SaveData::load(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;
    try {
        auto j = nlohmann::json::parse(f);
        cube = j.value("player-cube", cube);
        if (cube < 1) cube = 1;
        if (cube > 13) cube = 13;
        mainColor = j.value("player-main-color", mainColor);
        secondaryColor = j.value("player-secondary-color", secondaryColor);
        username = j.value("player-username", username);
        if (j.contains("level-best") && j["level-best"].is_array()) {
            size_t i = 0;
            for (auto& v : j["level-best"]) {
                if (i >= best.size()) break;
                if (v.is_number_integer()) best[i] = std::max(0, std::min(100, v.get<int>()));
                ++i;
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool SaveData::recordBest(int level, int percent) {
    if (level < 0 || level >= (int)best.size()) return false;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    if (percent <= best[level]) return false;
    best[level] = percent;
    return true;
}

bool SaveData::save(const std::string& path) const {
    nlohmann::json j = {
        {"player-cube", cube},
        {"player-main-color", mainColor},
        {"player-secondary-color", secondaryColor},
        {"player-username", username},
        {"level-best", best},
    };
    std::ofstream f(path);
    if (!f) return false;
    f << j.dump(2);
    return (bool)f;
}

} // namespace ogd
