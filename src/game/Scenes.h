#pragma once
#include <memory>
#include <string>

#include "game/App.h"

namespace ogd {

std::unique_ptr<Scene> makeLoadingScene();
std::unique_ptr<Scene> makeMenuScene();
std::unique_ptr<Scene> makePlayScene();

} // namespace ogd
