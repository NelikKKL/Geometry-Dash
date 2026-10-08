// Pause menu (port of PauseLayer from the 1.0 APK; layout measured from screenshots of the real game):
// rounded dark panel, level name, Normal / Practice progress bars, Practice-Resume-Menu buttons and a row of
// Music, FX, Auto-checkpoint and Help switches.
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "game/Tutorial.h"
#include "game/Ui.h"

namespace ogd {

struct PauseCallbacks {
    std::function<void()> onResume, onRestart, onQuit, onPractice, onNormal;
};

class PauseLayer {
public:
    PauseLayer(const std::string& levelName, int normalPercent, int practicePercent, bool practiceMode,
               PauseCallbacks cb);
    void update(float dt);
    void draw();
    void onDown(float x, float y);
    void onUp(float x, float y);
    bool helpOpen() const { return tut_.active; }
    void closeHelp() { tut_.active = false; }

private:
    void drawBar(float cy, const std::string& label, int percent, Color fill);
    void refreshToggles();

    std::string name_;
    int normal_, practice_;
    bool practiceMode_;
    PauseCallbacks cb_;
    std::vector<Button> buttons_;
    size_t musicIdx_ = 0, fxIdx_ = 0, autoIdx_ = 0;
    Tutorial tut_;
};

} // namespace ogd
