// Icon kit ("garage"): choose the cube icon and the two colours. Layout follows the original 1.x screen.
// Locked entries show a padlock; tapping one tells the player how to unlock it (see core/Unlocks).
#include <algorithm>
#include <cmath>
#include <memory>

#include "core/Palette.h"
#include "core/Unlocks.h"
#include "game/Levels.h"
#include "game/Popup.h"
#include "game/Scenes.h"
#include "game/Ui.h"
#include "game/World.h"

namespace ogd {
namespace {

constexpr float kCell = 64.f;           // tile size of icons and colour swatches
constexpr float kStep = 77.5f;          // distance between tiles
constexpr float kIconY = 281.f;
constexpr float kColorX0 = 213.f, kIconX0 = 175.f;
constexpr float kRowMainY = 149.f, kRowSecY = 65.f;
constexpr float kFloorTop = 206.f;
constexpr float kPreviewY = 439.f, kLineY = 384.f;
constexpr float kNameX = 645.f, kNameY = 607.f;
constexpr size_t kMaxName = 12;

std::string describe(const UnlockRule& r, const char* what) {
    switch (r.kind) {
    case UnlockKind::CompleteLevel: return std::string("Complete \"") + levelMeta(r.arg).name + "\"\nto unlock this " + what + ".";
    case UnlockKind::CompleteCount: return "Complete " + std::to_string(r.arg) + " levels\nto unlock this " + what + ".";
    case UnlockKind::CompleteAll: return std::string("Complete all ") + std::to_string(kLevelCount) + " levels\nto unlock this " + what + ".";
    default: return std::string("This ") + what + " is unlocked.";
    }
}

class GarageScene : public Scene {
public:
    GarageScene() : back_("GJ_arrow_03_001.png", 54.f, kH - 50.f, 1.f, [this] { leave(); }) {
        clampSelection();
    }

    ~GarageScene() override {
        if (editing_) SDL_StopTextInput();
    }

    void update(float dt) override {
        back_.update(dt);
        pulse_ = std::max(0.f, pulse_ - dt * 4.f);
        blink_ += dt;
        if (popup_) {
            popup_->update(dt);
            if (popup_->closed) popup_.reset();
        }
    }

    void draw() override {
        // grey gradient sky, dark floor
        drawVGradient(kFloorTop, kH, {106, 106, 106}, {173, 173, 173});
        E().fillRect(0, 0, kW, kFloorTop, {16, 12, 13}, 255);
        E().fillRect(0, kFloorTop - 3.f, kW, 3.f, {66, 65, 63}, 255);

        drawCornerArt(Corner::TopLeft);
        drawCornerArt(Corner::TopRight);
        back_.draw();

        // name and its hint
        E().drawSprite(E().sprite("GJ_nameTxt_001.png"), 371.f, 650.f);
        std::string shown = E().save.username;
        if (editing_ && std::fmod(blink_, 1.0f) < 0.5f) shown += "_";
        Font* big = E().font("bigFont.fnt");
        E().drawText(big, shown, kNameX, kNameY, fitScale(big, shown, 420.f, 1.15f));

        // preview cube standing on a glowing line
        drawGlowLine();
        const float ps = 1.7f + 0.3f * pulse_ * pulse_;
        drawIcon(E().save.cube, kW / 2.f, kPreviewY, ps, paletteColor(E().save.mainColor), paletteColor(E().save.secondaryColor));

        // icon panel
        E().drawPanel(E().sprite("square02_001.png"), 640.f, kIconY, 1034.f, 123.f, {0, 0, 0}, 140);
        for (int i = 1; i <= kIconCount; ++i) {
            const float x = kIconX0 + (i - 1) * kStep;
            if (isUnlocked(iconUnlockRule(i), E().save, kLevelCount)) {
                drawIcon(i, x, kIconY, 1.f, {175, 175, 175}, {255, 255, 255});
            } else {
                E().fillRect(x - kCell / 2, kIconY - kCell / 2, kCell, kCell, {60, 60, 60}, 255);
                E().fillRect(x - kCell / 2 + 4, kIconY - kCell / 2 + 4, kCell - 8, kCell - 8, {150, 150, 150}, 255);
                E().drawSprite(E().sprite("GJ_lock_001.png"), x, kIconY, 1.1f, 1.1f);
            }
            if (i == E().save.cube) E().drawSprite(E().sprite("GJ_select_001.png"), x, kIconY);
        }
        if (anyLocked()) E().drawSprite(E().sprite("GJ_unlockTxt_001.png"), 998.f, 345.f);

        // colour rows (main, secondary)
        drawColorRow(kRowMainY, E().save.mainColor);
        drawColorRow(kRowSecY, E().save.secondaryColor);

        if (popup_) popup_->draw();
    }

    void onDown(float x, float y) override {
        if (popup_) { popup_->onDown(x, y); return; }
        if (back_.onDown(x, y)) return;
        downOnName_ = nameHit(x, y);
        if (editing_ && !downOnName_) stopEditing();
    }

    void onUp(float x, float y) override {
        if (popup_) { popup_->onUp(x, y); return; }
        back_.onUp(x, y);
        if (downOnName_ && nameHit(x, y)) { startEditing(); downOnName_ = false; return; }
        downOnName_ = false;

        for (int i = 1; i <= kIconCount; ++i) {
            if (hit(x, y, kIconX0 + (i - 1) * kStep, kIconY)) { pickIcon(i); return; }
        }
        for (int c = 0; c < kPaletteSize; ++c) {
            const float cx = kColorX0 + c * kStep;
            if (hit(x, y, cx, kRowMainY)) { pickColor(c, true); return; }
            if (hit(x, y, cx, kRowSecY)) { pickColor(c, false); return; }
        }
    }

    void onKey(SDL_Keycode k, bool down) override {
        if (!down) return;
        if (popup_) { if (k == SDLK_RETURN || k == SDLK_ESCAPE) popup_->closed = true; return; }
        if (editing_) {
            if (k == SDLK_BACKSPACE) { if (!E().save.username.empty()) E().save.username.pop_back(); }
            else if (k == SDLK_RETURN || k == SDLK_ESCAPE) stopEditing();
            return;
        }
        if (k == SDLK_ESCAPE) leave();
        else if (k == SDLK_LEFT) pickIcon(std::max(1, E().save.cube - 1));
        else if (k == SDLK_RIGHT) pickIcon(std::min(kIconCount, E().save.cube + 1));
        else if (k == 'u') { E().save.debugUnlockAll = !E().save.debugUnlockAll; }   // hidden: unlock everything (testing)
    }

    void onText(const std::string& s) override {
        if (!editing_) return;
        for (unsigned char ch : s) {
            if (ch >= 32 && ch < 127 && E().save.username.size() < kMaxName) E().save.username += (char)ch;
        }
    }

private:
    static bool hit(float x, float y, float cx, float cy) { return std::fabs(x - cx) <= kCell / 2 && std::fabs(y - cy) <= kCell / 2; }
    static bool nameHit(float x, float y) { return std::fabs(x - kNameX) < 230.f && std::fabs(y - kNameY) < 40.f; }

    void clampSelection() {
        auto& s = E().save;
        s.cube = std::max(1, std::min(kIconCount, s.cube));
        s.mainColor = std::max(0, std::min(kPaletteSize - 1, s.mainColor));
        s.secondaryColor = std::max(0, std::min(kPaletteSize - 1, s.secondaryColor));
        // a save from a build with other unlock rules may point at a locked entry
        if (!isUnlocked(iconUnlockRule(s.cube), s, kLevelCount)) s.cube = 1;
        if (!isUnlocked(colorUnlockRule(s.mainColor), s, kLevelCount)) s.mainColor = 0;
        if (!isUnlocked(colorUnlockRule(s.secondaryColor), s, kLevelCount)) s.secondaryColor = 3;
    }

    bool anyLocked() const {
        for (int i = 1; i <= kIconCount; ++i) if (!isUnlocked(iconUnlockRule(i), E().save, kLevelCount)) return true;
        for (int c = 0; c < kPaletteSize; ++c) if (!isUnlocked(colorUnlockRule(c), E().save, kLevelCount)) return true;
        return false;
    }

    void pickIcon(int i) {
        const UnlockRule r = iconUnlockRule(i);
        if (!isUnlocked(r, E().save, kLevelCount)) { popup_ = std::make_unique<Popup>("Locked", describe(r, "icon")); return; }
        if (E().save.cube != i) { E().save.cube = i; pulse_ = 1.f; E().persist(); }
    }

    void pickColor(int c, bool main) {
        const UnlockRule r = colorUnlockRule(c);
        if (!isUnlocked(r, E().save, kLevelCount)) { popup_ = std::make_unique<Popup>("Locked", describe(r, "color")); return; }
        int& slot = main ? E().save.mainColor : E().save.secondaryColor;
        if (slot != c) { slot = c; pulse_ = 1.f; E().persist(); }
    }

    void startEditing() {
        editing_ = true;
        blink_ = 0.f;
        SDL_StartTextInput();
    }
    void stopEditing() {
        if (!editing_) return;
        editing_ = false;
        SDL_StopTextInput();
        if (E().save.username.empty()) E().save.username = "Player";
        E().persist();
    }

    void leave() {
        stopEditing();
        E().persist();
        app().goTo([] { return makeMenuScene(); });
    }

    static void drawIcon(int icon, float x, float y, float scale, Color main, Color sec) {
        char a[48], b[48];
        std::snprintf(a, sizeof a, "player_%02d_001.png", icon);
        std::snprintf(b, sizeof b, "player_%02d_2_001.png", icon);
        E().drawSprite(E().sprite(b), x, y, scale, scale, 0, sec);
        E().drawSprite(E().sprite(a), x, y, scale, scale, 0, main);
    }

    void drawColorRow(float y, int selected) {
        for (int c = 0; c < kPaletteSize; ++c) {
            const float x = kColorX0 + c * kStep;
            const Color col = paletteColor(c);
            E().fillRect(x - kCell / 2, y - kCell / 2, kCell, kCell, col, 255);
            if (!isUnlocked(colorUnlockRule(c), E().save, kLevelCount))
                E().drawSprite(E().sprite("GJ_lockGray_001.png"), x, y, 1.1f, 1.1f);
            if (c == selected) E().drawSprite(E().sprite("GJ_select_001.png"), x, y);
        }
    }

    // thin white line that fades out towards both ends (pixel-aligned segments, no overlap, so no seams)
    void drawGlowLine() {
        const int n = 96;
        const float x0 = 196.f, x1 = 1084.f;
        for (int i = 0; i < n; ++i) {
            const float a = std::sin((i + 0.5f) / n * 3.14159265f);
            const float xa = std::floor(x0 + (x1 - x0) * i / n), xb = std::floor(x0 + (x1 - x0) * (i + 1) / n);
            E().fillRect(xa, kLineY - 1.f, xb - xa, 3.f, {255, 255, 255}, (Uint8)(255 * std::pow(a, 0.6f)));
        }
    }

    Button back_;
    std::unique_ptr<Popup> popup_;
    float pulse_ = 0.f, blink_ = 0.f;
    bool editing_ = false, downOnName_ = false;
};

} // namespace

std::unique_ptr<Scene> makeGarageScene() { return std::make_unique<GarageScene>(); }

} // namespace ogd
