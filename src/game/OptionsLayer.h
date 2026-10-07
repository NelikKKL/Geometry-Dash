// Options drop-down (port of OptionsLayer / GJDropDownLayer from the 1.0 APK).
// Main page: Support, Rate, Download Soundtracks, How to play + Music / FX switches.
// Support and Soundtracks are separate drop-downs that open after the Options panel has slid away
// (OptionsLayer::layerHidden); How to play is the 5-page TutorialLayer on top.
#pragma once
#include <memory>

#include "game/Ui.h"

namespace ogd {

class OptionsLayer {
public:
    OptionsLayer();
    ~OptionsLayer();
    void update(float dt);
    void draw();
    void onDown(float x, float y);
    void onUp(float x, float y);
    void onKey(SDL_Keycode k, bool down);
    bool closed = false;

private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

std::unique_ptr<OptionsLayer> makeOptionsLayer();

} // namespace ogd
