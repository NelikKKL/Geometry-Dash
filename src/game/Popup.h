#pragma once
#include <memory>
#include <string>

#include "game/Ui.h"

namespace ogd {

// Modal "OK" dialog. Blocks input to the scene below while visible.
class Popup {
public:
    Popup(const std::string& title, const std::string& text);
    void update(float dt) { ok_.update(dt); }
    void draw();
    void onDown(float x, float y) { ok_.onDown(x, y); }
    void onUp(float x, float y) { ok_.onUp(x, y); }
    bool closed = false;

private:
    std::string title_, text_;
    Button ok_;
};

} // namespace ogd
