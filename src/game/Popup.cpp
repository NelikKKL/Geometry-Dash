#include "game/Popup.h"

#include <vector>

namespace ogd {

Popup::Popup(const std::string& title, const std::string& text)
    : title_(title), text_(text), ok_("GJ_button_01.png", kW / 2.f, 255, 1.f, [this] { closed = true; }) {
    ok_.setLabel("OK", 0.9f);
}

void Popup::draw() {
    E().fillRect(0, 0, kW, kH, {0, 0, 0}, 120);
    E().drawPanel(E().sprite("GJ_square01.png"), kW / 2.f, kH / 2.f, 620, 320);
    E().drawText(E().font("goldFont.fnt"), title_, kW / 2.f, kH / 2.f + 105, 1.0f);
    // text may contain '\n'
    std::vector<std::string> lines;
    for (size_t a = 0;;) {
        size_t b = text_.find('\n', a);
        lines.push_back(text_.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (b == std::string::npos) break;
        a = b + 1;
    }
    const float step = 36.f, y0 = kH / 2.f + 15.f + (lines.size() - 1) * step / 2.f;
    for (size_t i = 0; i < lines.size(); ++i)
        E().drawText(E().font("chatFont.fnt"), lines[i], kW / 2.f, y0 - i * step, 1.0f);
    ok_.draw();
}

} // namespace ogd
