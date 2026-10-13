#pragma once
#include <functional>
#include <memory>
#include <string>

#include "game/App.h"

namespace ogd {

std::unique_ptr<Scene> makeLoadingScene();
std::unique_ptr<Scene> makeMenuScene();
std::unique_ptr<Scene> makeLevelSelectScene(int page = 0);
std::unique_ptr<Scene> makeGarageScene();
std::unique_ptr<Scene> makePlayScene(int level);
std::unique_ptr<Scene> makePlayTestScene(const std::string& levelText, std::function<std::unique_ptr<Scene>()> back);   // editor play-test
std::unique_ptr<Scene> makeEditorScene();

// Debug/testing hook: start every run at this x (GD units). 0 = normal.
extern double g_debugStartX;
extern bool g_debugStartShip;

} // namespace ogd
