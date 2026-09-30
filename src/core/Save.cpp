#include "core/Save.h"
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
        return true;
    } catch (...) {
        return false;
    }
}

bool SaveData::save(const std::string& path) const {
    nlohmann::json j = {
        {"player-cube", cube},
        {"player-main-color", mainColor},
        {"player-secondary-color", secondaryColor},
        {"player-username", username},
    };
    std::ofstream f(path);
    if (!f) return false;
    f << j.dump(2);
    return (bool)f;
}

} // namespace ogd
