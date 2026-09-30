#include "game/Popup.h"

namespace ogd {

Popup::Popup(const std::string& title, const std::string& text)
    : title_(title), text_(text), ok_("GJ_button_01.png", kW / 2.f, 255, 1.f, [this] { closed = true; }) {
    ok_.setLabel("OK", 0.9f);
}

void Popup::draw() {
    E().fillRect(0, 0, kW, kH, {0, 0, 0}, 120);
    E().drawPanel(E().sprite("GJ_square01.png"), kW / 2.f, kH / 2.f, 620, 320);
    E().drawText(E().font("goldFont.fnt"), title_, kW / 2.f, kH / 2.f + 105, 1.0f);
    E().drawText(E().font("chatFont.fnt"), text_, kW / 2.f, kH / 2.f + 15, 1.0f);
    ok_.draw();
}

} // namespace ogd
