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
std::unique_ptr<Scene> makeEditorScene();                      // edits E().save.customLevels[E().save.editingLevel]
// Creator hub (CreatorLayer) and the screens behind it, see CreatorScenes.cpp
std::unique_ptr<Scene> makeCreatorScene();
std::unique_ptr<Scene> makeMyLevelsScene();
std::unique_ptr<Scene> makeEditLevelScene(bool justCreated = false);   // E().save.editingLevel
std::unique_ptr<Scene> makeSearchScene();
enum class OnlineKind { Featured, Saved, Results };
std::unique_ptr<Scene> makeOnlineLevelsScene(OnlineKind kind);

// Debug/testing hook: start every run at this x (GD units). 0 = normal.
extern double g_debugStartX;
extern bool g_debugStartShip;

} // namespace ogd
