// FLAlertLayer look-alike used by the creator screens and the editor: dimmed screen, panel, gold title, rich text,
// one or two buttons. The owning scene removes the dialog once `closed` is set and then runs `after` (so the action
// may open another dialog or change scenes without destroying the dialog it is running from).
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "game/Ui.h"

namespace ogd {

class Dialog {
public:
    struct Choice { std::string label; float w; std::function<void()> action; };
    Dialog(std::string title, std::string body, std::vector<Choice> choices, float panelH = 307.f)
        : title_(std::move(title)), body_(std::move(body)), panelH_(panelH) {
        const float n = (float)choices.size();
        float total = 0.f;
        for (auto& c : choices) total += c.w;
        total += (n - 1.f) * 40.f;
        float x = kW / 2.f - total / 2.f;
        const float y = kH / 2.f - panelH_ / 2.f + 64.f;      // measured: buttons sit 64 px above the panel's bottom edge
        for (auto& c : choices) {
            auto act = c.action;
            buttons_.emplace_back(c.label, x + c.w / 2.f, y, c.w, 58.f, [this, act] { closed = true; after = act; },
                                  "goldFont.fnt", 0.9f);
            x += c.w + 40.f;
        }
    }
    void update(float dt) { for (auto& b : buttons_) b.update(dt); }
    void draw() const {
        E().fillRect(0, 0, (float)kW, (float)kH, {0, 0, 0}, 120);
        E().drawPanel(E().sprite("GJ_square01.png"), kW / 2.f, kH / 2.f, 636.f, panelH_);
        const float top = kH / 2.f + panelH_ / 2.f;
        E().drawText(E().font("goldFont.fnt"), title_, kW / 2.f, top - 79.f, 1.0f);
        drawRichText(E().font("chatFont.fnt"), body_, kW / 2.f, top - 125.f, 1.0f, 600.f, 45.f);
        for (const auto& b : buttons_) b.draw();
    }
    void onDown(float x, float y) { for (auto& b : buttons_) if (b.onDown(x, y)) break; }
    void onUp(float x, float y) { for (auto& b : buttons_) b.onUp(x, y); }
    bool closed = false;
    std::function<void()> after;       // run by the owning screen once the dialog has been removed

private:
    std::string title_, body_;
    float panelH_;
    std::vector<LabelButton> buttons_;
};

} // namespace ogd
