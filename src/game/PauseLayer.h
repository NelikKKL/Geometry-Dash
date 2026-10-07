// Pause menu (port of PauseLayer from the 1.0 APK: dim backdrop, normal/practice progress bars,
// Menu / Restart / Resume / Practice buttons, Music and FX toggles).
#pragma once
#include <functional>
#include <string>
#include <vector>

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

private:
    void drawBar(float cy, const std::string& label, int percent, Color fill);
    void toggleMusic();
    void toggleFx();

    std::string name_;
    int normal_, practice_;
    bool practiceMode_;
    PauseCallbacks cb_;
    std::vector<Button> buttons_;
    size_t musicIdx_ = 0, fxIdx_ = 0;
};

} // namespace ogd
