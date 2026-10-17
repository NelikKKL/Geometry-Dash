// Level editor, laid out like the 1.0 editor (EditorUI / EditorPauseLayer / LevelSettingsLayer / DrawGridLayer).
// Positions are design px (1280x720, y-up) measured on Android screenshots; behaviour follows the decompiled
// EditorUI where it could be read (tabs, 12-per-page button bars, swipe select, zoom, undo / redo, slider).
//
// Layout: undo / redo, scroll slider, settings + pause on top; BUILD / EDIT / DELETE tabs on the left, a 6x2 button
// bar with page arrows and page dots in the middle, SWIPE / DE-SELECT / zoom on the right.
// The level being edited is E().save.customLevels[E().save.editingLevel].
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include "core/Level.h"
#include "core/ObjectTable.h"
#include "game/App.h"
#include "game/Dialog.h"
#include "game/Popup.h"
#include "game/Scenes.h"
#include "game/Songs.h"
#include "game/Ui.h"
#include "game/World.h"

namespace ogd {
namespace {

constexpr float kPx = 2.f;                        // GD units -> world px
constexpr float kFloorPx = 90.f * kPx;            // world y of the floor line
constexpr float kCell = 30.f;                     // grid size in GD units
constexpr float kBarH = 203.f;                    // height of the bottom bar (screen px)
constexpr float kFloorScreenY = 273.f;            // where the floor line sits on the screen at the default camera
constexpr float kStartScreenX = 64.f;             // where x = 0 sits on the screen at the default camera
constexpr size_t kMaxObjects = 4000;              // "You cannot create more than 4000 objects in a single level."
constexpr const char* kDefaultLevel = "kS1,40,kS2,125,kS3,255,kS4,0,kS5,102,kS6,255,kA1,0;";
constexpr float kMoveBig = 30.f, kMoveSmall = 2.f;   // EditorUI::moveObjectCall uses 30 and 2

enum class Mode { Build, Edit, Delete };

// One entry of the Build bar: the icon frame shown on the button and the object id it creates.
struct PalItem { const char* frame; int id; };
const PalItem kPalette[] = {
    // page 1
    {"square_01_001.png", 1}, {"square_02_001.png", 2}, {"square_03_001.png", 3}, {"square_04_001.png", 4},
    {"square_05_001.png", 5}, {"square_06_001.png", 6}, {"square_07_001.png", 7}, {"spike_01_001.png", 8},
    {"spike_02_001.png", 39}, {"pit_01_001.png", 9}, {"plank_01_001.png", 40}, {"bump_01_001.png", 35},
    // page 2
    {"ring_01_001.png", 36}, {"portal_01_front_001.png", 10}, {"portal_02_front_001.png", 11},
    {"portal_03_front_001.png", 12}, {"portal_04_front_001.png", 13}, {"rod_01_001.png", 15},
    {"rod_02_001.png", 16}, {"rod_03_001.png", 17}, {"d_spikes_01_001.png", 18}, {"d_spikes_02_001.png", 19},
    {"d_spikes_03_001.png", 20}, {"d_spikes_04_001.png", 21},
    // page 3
    {"chain_01_001.png", 41},
    // page 4: edge effects, colour triggers, start position, ghost triggers (ids of the edge effects are a best guess)
    {"edit_eeNoneBtn_001.png", 22}, {"edit_eeFTBtn_001.png", 23}, {"edit_eeFBBtn_001.png", 24},
    {"edit_eeFLBtn_001.png", 25}, {"edit_eeFRBtn_001.png", 26}, {"edit_eeSUBtn_001.png", 27},
    {"edit_eTintBGBtn_001.png", 29}, {"edit_eTintGBtn_001.png", 30}, {"edit_eStartPosBtn_001.png", 31},
    {"edit_eGhostDBtn_001.png", 32}, {"edit_eGhostEBtn_001.png", 33},
};
constexpr int kPalCount = (int)(sizeof kPalette / sizeof *kPalette);
constexpr int kBuildPageStart[5] = {0, 12, 24, 25, kPalCount};     // 12 / 12 / 1 / 11 items, as in the original

// Edit bar (page 1 = 12 transform buttons, page 2 = Edit Object).
enum Act { Up, Down, Left, Right, UpBig, DownBig, LeftSmall, RightSmall, FlipX, FlipY, RotCW, RotCCW, EditObj };
struct EditItem { const char* icon; Act act; };
const EditItem kEditItems[] = {
    {"edit_upBtn_001.png", Up}, {"edit_downBtn_001.png", Down}, {"edit_leftBtn_001.png", Left}, {"edit_rightBtn_001.png", Right},
    {"edit_upBtn2_001.png", UpBig}, {"edit_downBtn2_001.png", DownBig},
    {"edit_leftBtn2_001.png", LeftSmall}, {"edit_rightBtn2_001.png", RightSmall},
    {"edit_flipXBtn_001.png", FlipX}, {"edit_flipYBtn_001.png", FlipY}, {"edit_cwBtn_001.png", RotCW}, {"edit_ccwBtn_001.png", RotCCW},
    {"edit_editObjBtn_001.png", EditObj},
};

const float kColX[6] = {414.f, 500.f, 586.f, 672.f, 758.f, 845.f};
constexpr float kRowY[2] = {153.f, 63.f};
constexpr float kCellW = 76.f, kCellH = 80.f;

float scaleForHeight(const char* name, float h) {
    const Sprite s = E().sprite(name);
    return (s && s.srcH > 0.f) ? h / s.srcH : 1.f;
}
float scaleToFit(const Sprite& s, float maxW, float maxH) {
    if (!s || s.w <= 0.f || s.h <= 0.f) return 1.f;
    return std::min(maxW / s.w, maxH / s.h);
}

// A button made of a plate (GJ_button_NN) and an icon on it: how the original builds the edit / create buttons.
struct Tile {
    float x = 0, y = 0, w = kCellW, h = kCellH;
    std::string icon;
    bool pressed = false;
    bool contains(float px, float py) const { return std::fabs(px - x) <= w / 2 && std::fabs(py - y) <= h / 2; }
    void draw(const char* plate, bool enabled = true, float iconMax = 56.f) const {
        const float k = pressed ? 1.12f : 1.f;
        const Uint8 a = enabled ? 255 : 110;
        E().drawPanel(E().sprite(plate), x, y, w * k, h * k, {}, a);
        const Sprite s = E().sprite(icon);
        if (!s) return;
        const float sc = std::min(1.f, scaleToFit(s, iconMax, iconMax)) * k;
        E().drawSprite(s, x, y, sc, sc, 0, {}, a);
    }
};

class EditorScene : public Scene {
public:
    EditorScene() {
        auto& levels = E().save.customLevels;
        if (levels.empty() || E().save.editingLevel < 0 || E().save.editingLevel >= (int)levels.size()) {
            levels.insert(levels.begin(), SaveData::CustomLevel());     // opened without a level (should not happen)
            E().save.editingLevel = 0;
        }
        const SaveData::CustomLevel& cl = levels[E().save.editingLevel];
        LevelParseResult r = parseLevelString(cl.data.empty() ? std::string(kDefaultLevel) : cl.data);
        if (!r.ok) r = parseLevelString(kDefaultLevel);
        level_ = std::move(r.level);
        objs_ = std::move(level_.objects);
        level_.objects.clear();
        sel_.assign(objs_.size(), 0);
        const auto& st = level_.settings;
        bg_ = st.hasBackground ? Color{st.background.r, st.background.g, st.background.b} : Color{40, 125, 255};
        ground_ = st.hasGround ? Color{st.ground.r, st.ground.g, st.ground.b} : Color{0, 102, 255};
        song_ = st.musicTrack >= 0 ? st.musicTrack : cl.song;
        camX_ = -kStartScreenX / zoom_;
        camY_ = camYMin();
        makeButtons();
    }
    ~EditorScene() override { if (musicOn_) E().stopMusic(); }

    void update(float dt) override {
        if (dialog_) {
            dialog_->update(dt);
            if (dialog_->closed) { auto a = std::move(dialog_->after); dialog_.reset(); if (a) a(); }
            return;
        }
        if (overlay_ != Overlay::None) { updateOverlay(dt); return; }
        for (auto* b : {&undoB_, &redoB_, &gearB_, &pauseB_, &prevB_, &nextB_}) b->update(dt);
        blink_ += dt;
    }

    void draw() override {
        E().clear({0, 0, 0});
        E().setZoom(zoom_);
        drawBackground(camX_ * 0.1f, {135, 135, 135}, 0.f);
        drawGround(camX_, {150, 150, 150}, camY_, kFloorPx);
        drawGrid();
        drawObjects();
        if (mode_ == Mode::Build && overlay_ == Overlay::None && !dialog_ && hover_ && hoverY_ > kBarH && !swiping_) drawGhost();
        if (swiping_ && swipeDrag_) drawSwipeRect();
        E().setZoom(1.f);
        drawUi();
        if (overlay_ != Overlay::None) drawOverlay();
        if (dialog_) dialog_->draw();
    }

    // ----------------------------------------------------------------------------------- input
    void onDown(float x, float y) override {
        downX_ = hoverX_ = x; downY_ = hoverY_ = y; hover_ = true; moved_ = false; pressed_ = true; swipeDrag_ = false;
        if (dialog_) { dialog_->onDown(x, y); return; }
        if (overlay_ != Overlay::None) { overlayDown(x, y); return; }
        uiPress_ = true;
        for (auto* b : {&undoB_, &redoB_, &gearB_, &pauseB_}) if (b->onDown(x, y)) return;
        if (pageCount() > 1) { if (prevB_.onDown(x, y) || nextB_.onDown(x, y)) return; }
        if (sliderDown(x, y)) return;
        if (tabDown(x, y) || sideDown(x, y)) return;
        if (barDown(x, y)) return;
        uiPress_ = false;
        camStartX_ = camX_; camStartY_ = camY_;
    }

    void onMove(float x, float y) override {
        hoverX_ = x; hoverY_ = y; hover_ = true;
        if (dialog_ || overlay_ != Overlay::None) { overlayMove(x, y); return; }
        if (sliding_) { sliderTo(x); return; }
        if (!pressed_ || uiPress_) return;
        if (std::hypot(x - downX_, y - downY_) > 14.f) moved_ = true;
        if (!moved_) return;
        if (swiping_) { swipeDrag_ = true; return; }
        camX_ = camStartX_ - (x - downX_) / zoom_;                          // drag = pan
        camY_ = camStartY_ - (y - downY_) / zoom_;
        clampCamera();
    }

    void onUp(float x, float y) override {
        const bool wasPressed = pressed_;
        pressed_ = false;
        if (dialog_) { dialog_->onUp(x, y); return; }
        if (overlay_ != Overlay::None) { overlayUp(x, y); return; }
        for (auto* b : {&undoB_, &redoB_, &gearB_, &pauseB_, &prevB_, &nextB_}) b->onUp(x, y);
        sliding_ = false;
        const bool wasUi = uiPress_;
        uiPress_ = false;
        releaseTiles(x, y);
        if (wasUi || !wasPressed) return;
        if (swiping_ && swipeDrag_) { selectRect(downX_, downY_, x, y); swipeDrag_ = false; return; }
        if (moved_) return;
        if (y <= kBarH) return;
        tapWorld(x, y);
    }

    void onWheel(float dy) override { if (!dialog_ && overlay_ == Overlay::None) zoomBy(dy > 0 ? 1.12f : 1.f / 1.12f); }

    void onKey(SDL_Keycode k, bool down) override {
        if (!down) return;
        if (dialog_) { if (k == SDLK_ESCAPE) dialog_->closed = true; return; }
        if (overlay_ != Overlay::None) { if (k == SDLK_ESCAPE) closeOverlay(); return; }
        switch (k) {
        case SDLK_ESCAPE: overlay_ = Overlay::Pause; break;
        case SDLK_LEFT: camX_ -= 120.f; clampCamera(); break;
        case SDLK_RIGHT: camX_ += 120.f; break;
        case SDLK_UP: camY_ += 120.f; break;
        case SDLK_DOWN: camY_ -= 120.f; clampCamera(); break;
        case SDLK_z: undo(); break;
        case SDLK_y: redo(); break;
        case SDLK_BACKSPACE: deleteSelected(); break;
        default: break;
        }
    }

private:
    enum class Overlay { None, Pause, Settings, Color };

    // -------------------------------------------------------------------------- coordinates
    float camYMin() const { return kFloorPx - kFloorScreenY / zoom_; }
    void clampCamera() {
        camY_ = std::max(camYMin(), camY_);
        camX_ = std::max(-kStartScreenX / zoom_ - 100.f, camX_);
    }
    float worldX(float sx) const { return camX_ + sx / zoom_; }
    float worldY(float sy) const { return camY_ + sy / zoom_; }
    float unitsX(float sx) const { return worldX(sx) / kPx; }
    float unitsY(float sy) const { return (worldY(sy) - kFloorPx) / kPx; }
    static float snap(float u) { return std::floor(u / kCell) * kCell + kCell / 2.f; }
    float screenX(float ux) const { return (ux * kPx - camX_) * zoom_; }
    float screenY(float uy) const { return (kFloorPx + uy * kPx - camY_) * zoom_; }

    // ------------------------------------------------------------------------------- level
    float levelEndX() const {
        float m = 0.f;
        for (const LevelObject& o : objs_) m = std::max(m, o.x);
        return m;
    }
    float sliderMin() const { return -kStartScreenX / zoom_; }
    float sliderMax() const { return std::max(1500.f, levelEndX() * kPx + 600.f); }

    void pushUndo() {
        undo_.push_back(objs_);
        if (undo_.size() > 60) undo_.erase(undo_.begin());
        redo_.clear();
    }
    void restore(std::vector<LevelObject> o) { objs_ = std::move(o); sel_.assign(objs_.size(), 0); }
    void undo() { if (undo_.empty()) return; redo_.push_back(objs_); restore(std::move(undo_.back())); undo_.pop_back(); }
    void redo() { if (redo_.empty()) return; undo_.push_back(objs_); restore(std::move(redo_.back())); redo_.pop_back(); }

    void zoomBy(float f) {
        const float cx = worldX(kW / 2.f), cy = worldY(kH / 2.f);
        zoom_ = std::max(0.5f, std::min(2.6f, zoom_ * f));
        camX_ = cx - (kW / 2.f) / zoom_;
        camY_ = cy - (kH / 2.f) / zoom_;
        clampCamera();
    }

    // The level as a string: header from the settings dialog, objects sorted by x.
    std::string serialize() {
        auto& raw = level_.settings.raw;
        raw["kS1"] = std::to_string(bg_.r); raw["kS2"] = std::to_string(bg_.g); raw["kS3"] = std::to_string(bg_.b);
        raw["kS4"] = std::to_string(ground_.r); raw["kS5"] = std::to_string(ground_.g); raw["kS6"] = std::to_string(ground_.b);
        raw["kA1"] = std::to_string(song_);
        Level out;
        out.settings = level_.settings;
        out.objects = objs_;
        out.sortByX();
        return serializeLevel(out);
    }
    void save() {
        SaveData::CustomLevel& cl = E().save.customLevels[E().save.editingLevel];
        cl.data = serialize();
        cl.song = song_;
        E().persist();
    }
    void saveAndPlay() {
        save();
        const std::string text = E().save.customLevels[E().save.editingLevel].data;
        app().goTo([text] { return makePlayTestScene(text, [] { return makeEditorScene(); }); });
    }
    void leave() { app().goTo([] { return makeEditLevelScene(false); }); }
    void saveAndExit() { save(); leave(); }

    // --------------------------------------------------------------------------------- objects
    const char* iconFor(int id) const {
        for (const PalItem& p : kPalette) if (p.id == id) return p.frame;
        return nullptr;
    }
    int objectAt(float ux, float uy) const {            // topmost object whose cell contains the point
        for (int i = (int)objs_.size() - 1; i >= 0; --i)
            if (std::fabs(ux - objs_[i].x) <= kCell / 2.f && std::fabs(uy - objs_[i].y) <= kCell / 2.f) return i;
        return -1;
    }
    int selectionCount() const { int n = 0; for (char c : sel_) n += c; return n; }
    void deselectAll() { std::fill(sel_.begin(), sel_.end(), 0); }

    void place(int id, float cx, float cy) {
        if (objs_.size() >= kMaxObjects) {
            dialog_ = std::make_unique<Dialog>("Max Objects", "You cannot create more than <cy>4000</c> <cg>objects</c> in a single level.",
                                               std::vector<Dialog::Choice>{{"OK", 110.f, nullptr}});
            return;
        }
        for (const LevelObject& o : objs_) if (o.id == id && o.x == cx && o.y == cy) return;   // already there
        pushUndo();
        LevelObject o;
        o.id = id; o.x = cx; o.y = cy;
        if (id == kObjColorTriggerBG || id == kObjColorTriggerGround) {      // colour triggers start with the current colour
            const Color c = id == kObjColorTriggerBG ? bg_ : ground_;
            o.extra = {{7, std::to_string(c.r)}, {8, std::to_string(c.g)}, {9, std::to_string(c.b)}, {10, "0.5"}};
        }
        objs_.push_back(std::move(o));
        sel_.push_back(0);
    }

    void tapWorld(float sx, float sy) {
        const float ux = unitsX(sx), uy = unitsY(sy);
        if (mode_ == Mode::Build) {
            if (ux < 0.f) return;
            place(brushId(), snap(ux), std::max(kCell / 2.f, snap(uy)));
        } else {
            const int i = objectAt(ux, uy);
            if (i < 0) { deselectAll(); return; }
            const bool was = sel_[i] != 0;
            if (!swiping_) deselectAll();      // without Swipe a tap selects just this object; with it, taps toggle
            sel_[i] = was ? 0 : 1;
        }
    }
    void selectRect(float x0, float y0, float x1, float y1) {
        const float ax = unitsX(std::min(x0, x1)), bx = unitsX(std::max(x0, x1));
        const float ay = unitsY(std::min(y0, y1)), by = unitsY(std::max(y0, y1));
        if (mode_ == Mode::Build) return;
        for (size_t i = 0; i < objs_.size(); ++i)
            if (objs_[i].x >= ax && objs_[i].x <= bx && objs_[i].y >= ay && objs_[i].y <= by) sel_[i] = 1;
    }
    void deleteSelected() {
        if (selectionCount() == 0) return;
        pushUndo();
        std::vector<LevelObject> keep;
        for (size_t i = 0; i < objs_.size(); ++i) if (!sel_[i]) keep.push_back(objs_[i]);
        objs_ = std::move(keep);
        sel_.assign(objs_.size(), 0);
    }
    void deleteAll() {
        if (objs_.empty()) return;
        pushUndo();
        objs_.clear();
        sel_.clear();
    }

    void transform(Act a) {
        if (selectionCount() == 0) return;
        pushUndo();
        float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
        for (size_t i = 0; i < objs_.size(); ++i) if (sel_[i]) {
            minX = std::min(minX, objs_[i].x); maxX = std::max(maxX, objs_[i].x);
            minY = std::min(minY, objs_[i].y); maxY = std::max(maxY, objs_[i].y);
        }
        const float cx = (minX + maxX) / 2.f, cy = (minY + maxY) / 2.f;
        float dx = 0.f, dy = 0.f;
        switch (a) {
        case Up: dy = kMoveBig; break;
        case Down: dy = -kMoveBig; break;
        case Left: dx = -kMoveBig; break;
        case Right: dx = kMoveBig; break;
        case UpBig: dy = kMoveSmall; break;
        case DownBig: dy = -kMoveSmall; break;
        case LeftSmall: dx = -kMoveSmall; break;
        case RightSmall: dx = kMoveSmall; break;
        default: break;
        }
        // keep the whole selection above the floor and right of the start line
        if (dy < 0.f && minY + dy < kCell / 2.f) dy = kCell / 2.f - minY;
        if (dx < 0.f && minX + dx < 1.f) dx = 1.f - minX;
        for (size_t i = 0; i < objs_.size(); ++i) {
            if (!sel_[i]) continue;
            LevelObject& o = objs_[i];
            o.x += dx; o.y += dy;
            const float rx = o.x - cx, ry = o.y - cy;
            switch (a) {
            case FlipX: o.x = cx - rx; o.flipX = !o.flipX; break;
            case FlipY: o.y = std::max(kCell / 2.f, cy - ry); o.flipY = !o.flipY; break;
            case RotCW: o.x = cx + ry; o.y = std::max(kCell / 2.f, cy - rx); o.rotation = std::fmod(o.rotation + 90.f, 360.f); break;
            case RotCCW: o.x = cx - ry; o.y = std::max(kCell / 2.f, cy + rx); o.rotation = std::fmod(o.rotation + 270.f, 360.f); break;
            default: break;
            }
        }
    }

    // colour triggers among the selection (the only objects "Edit Object" can change)
    bool selectionHasTrigger() const {
        for (size_t i = 0; i < objs_.size(); ++i)
            if (sel_[i] && (objs_[i].id == kObjColorTriggerBG || objs_[i].id == kObjColorTriggerGround)) return true;
        return false;
    }

    // ------------------------------------------------------------------------------ UI layout
    int pageCount() const { return mode_ == Mode::Build ? 4 : (mode_ == Mode::Edit ? 2 : 1); }
    int itemsOnPage() const {
        if (mode_ == Mode::Build) return kBuildPageStart[page_ + 1] - kBuildPageStart[page_];
        if (mode_ == Mode::Edit) return page_ == 0 ? 12 : 1;
        return 0;
    }
    Tile itemTile(int slot) const {
        Tile t;
        t.x = kColX[slot % 6];
        t.y = kRowY[slot / 6];
        if (mode_ == Mode::Build) t.icon = kPalette[kBuildPageStart[page_] + slot].frame;
        else if (mode_ == Mode::Edit) t.icon = kEditItems[page_ == 0 ? slot : 12].icon;
        return t;
    }
    int brushId() const { return brush_ >= 0 && brush_ < kPalCount ? kPalette[brush_].id : 1; }

    static constexpr float kSlideL = 425.f, kSlideR = 855.f, kSlideY = 675.f;
    float sliderFrac() const { return std::max(0.f, std::min(1.f, (camX_ - sliderMin()) / std::max(1.f, sliderMax() - sliderMin()))); }
    bool sliderDown(float x, float y) {
        if (std::fabs(y - kSlideY) > 45.f || x < kSlideL - 40.f || x > kSlideR + 40.f) return false;
        sliding_ = true;
        sliderTo(x);
        return true;
    }
    void sliderTo(float x) {
        const float f = std::max(0.f, std::min(1.f, (x - kSlideL) / (kSlideR - kSlideL)));
        camX_ = sliderMin() + f * (sliderMax() - sliderMin());
    }

    bool tabDown(float x, float y) {
        for (int i = 0; i < 3; ++i)
            if (std::fabs(x - 90.f) < 80.f && std::fabs(y - tabY(i)) < 31.f) {
                if ((int)mode_ != i) { mode_ = (Mode)i; page_ = 0; if (mode_ == Mode::Build) deselectAll(); }
                return true;
            }
        return false;
    }
    static float tabY(int i) { return i == 0 ? 169.f : (i == 1 ? 104.f : 39.f); }

    bool sideDown(float x, float y) {
        struct R { float x, y; int id; };
        const R r[4] = {{1132.f, 149.f, 0}, {1227.f, 149.f, 1}, {1132.f, 52.f, 2}, {1227.f, 52.f, 3}};
        for (const R& b : r)
            if (std::fabs(x - b.x) < 42.f && std::fabs(y - b.y) < 39.f) {
                if (b.id == 0) { swiping_ = !swiping_; }
                else if (b.id == 1) zoomBy(1.2f);
                else if (b.id == 2) deselectAll();
                else zoomBy(1.f / 1.2f);
                return true;
            }
        return false;
    }

    bool barDown(float x, float y) {
        if (y > kBarH) return false;
        pressedTile_ = -1;
        if (mode_ == Mode::Delete) {
            if (std::fabs(x - 540.f) < 100.f && std::fabs(y - 130.f) < 30.f) { pressedTile_ = 100; return true; }
            if (std::fabs(x - 740.f) < 100.f && std::fabs(y - 130.f) < 30.f) { pressedTile_ = 101; return true; }
            return true;
        }
        for (int s = 0; s < itemsOnPage(); ++s)
            if (itemTile(s).contains(x, y)) { pressedTile_ = s; return true; }
        return true;
    }
    void releaseTiles(float x, float y) {
        const int t = pressedTile_;
        pressedTile_ = -1;
        if (t < 0) return;
        if (t >= 100) {
            if (t == 100 && std::fabs(x - 540.f) < 100.f && std::fabs(y - 130.f) < 30.f) deleteSelected();
            if (t == 101 && std::fabs(x - 740.f) < 100.f && std::fabs(y - 130.f) < 30.f)
                dialog_ = std::make_unique<Dialog>("Delete All", "Are you sure you want to <cr>Delete</c> all objects? (<cy>This action cannot be undone</c>)",
                    std::vector<Dialog::Choice>{{"NO", 105.f, nullptr}, {"YES", 135.f, [this] { deleteAll(); }}}, 340.f);
            return;
        }
        if (!itemTile(t).contains(x, y)) return;
        if (mode_ == Mode::Build) brush_ = kBuildPageStart[page_] + t;
        else if (mode_ == Mode::Edit) {
            const Act a = kEditItems[page_ == 0 ? t : 12].act;
            if (a == EditObj) { if (selectionHasTrigger()) openColorForSelection(); }
            else transform(a);
        }
    }

    void setPage(int p) { const int n = pageCount(); page_ = ((p % n) + n) % n; }

    // ----------------------------------------------------------------------------- drawing
    void drawGrid() {
        const float visW = kW / zoom_, visH = kH / zoom_;
        const float floorY = kFloorPx - camY_;
        const float x0 = std::max(0.f, std::floor(camX_ / (kCell * kPx)) * kCell * kPx);
        const float w = visW + camX_ - 0.f;                       // horizontal lines start at x = 0 and run off the screen
        for (float wx = x0; wx < camX_ + visW + 60.f; wx += kCell * kPx)
            E().fillRect(wx - camX_ - 0.5f, floorY, 1.f, visH - floorY, wx == 0.f ? Color{140, 120, 255} : Color{0, 0, 0}, wx == 0.f ? 170 : 85);
        for (float wy = kFloorPx; wy < camY_ + visH + 60.f; wy += kCell * kPx)
            E().fillRect(-camX_, wy - camY_ - 0.5f, std::max(0.f, w), 1.f, {0, 0, 0}, 85);
    }

    void drawObjectAt(const LevelObject& o, Uint8 alpha = 255, Color tint = {}) {
        const float sx = o.x * kPx - camX_, sy = kFloorPx + o.y * kPx - camY_;
        if (sx < -150.f || sx > kW / zoom_ + 150.f) return;
        const ObjInfo* info = objectInfo(o.id);
        if (info && (info->sprite || info->spriteBack)) {
            if (info->spriteBack) E().drawSprite(E().sprite(info->spriteBack), sx, sy, 1.f, 1.f, o.rotation, tint, alpha, o.flipX, o.flipY);
            if (info->sprite) E().drawSprite(E().sprite(info->sprite), sx, sy, 1.f, 1.f, o.rotation, tint, alpha, o.flipX, o.flipY);
            return;
        }
        if (const char* icon = iconFor(o.id)) {                   // triggers and the start marker show their button icon
            const Sprite s = E().sprite(icon);
            if (!s) return;
            const float k = std::min(1.f, scaleToFit(s, kCell * kPx - 6.f, kCell * kPx - 6.f));
            E().drawSprite(s, sx, sy, k, k, o.rotation, tint, alpha, o.flipX, o.flipY);
        }
    }

    void drawObjects() {
        std::vector<size_t> idx;
        for (size_t i = 0; i < objs_.size(); ++i) {
            const float sx = objs_[i].x * kPx - camX_;
            if (sx > -150.f && sx < kW / zoom_ + 150.f) idx.push_back(i);
        }
        std::stable_sort(idx.begin(), idx.end(), [&](size_t a, size_t b) {
            const ObjInfo* ia = objectInfo(objs_[a].id); const ObjInfo* ib = objectInfo(objs_[b].id);
            return (ia ? ia->z : 0) < (ib ? ib->z : 0);
        });
        for (size_t i : idx) drawObjectAt(objs_[i], 255, sel_[i] ? Color{120, 255, 120} : Color{});
    }

    void drawGhost() {
        LevelObject g;
        g.id = brushId();
        g.x = snap(unitsX(hoverX_));
        g.y = std::max(kCell / 2.f, snap(unitsY(hoverY_)));
        drawObjectAt(g, 120);
    }
    void drawSwipeRect() {
        const float ax = worldX(std::min(downX_, hoverX_)) - camX_, bx = worldX(std::max(downX_, hoverX_)) - camX_;
        const float ay = worldY(std::min(downY_, hoverY_)) - camY_, by = worldY(std::max(downY_, hoverY_)) - camY_;
        E().fillRect(ax, ay, bx - ax, by - ay, {120, 255, 120}, 60);
    }

    void drawUi() {
        // bottom bar
        const Sprite bar = E().sprite("edit_barBG_001.png");
        if (bar && bar.srcW > 0.f) E().drawSprite(bar, kW / 2.f, kBarH / 2.f, kW / bar.srcW, kBarH / bar.srcH);
        else E().fillRect(0.f, 0.f, kW, kBarH, {30, 30, 30}, 220);
        for (float x : {181.f, 1075.f}) E().drawSprite(E().sprite("edit_vLine_001.png"), x, kBarH / 2.f, 1.f, kBarH / std::max(1.f, E().sprite("edit_vLine_001.png").srcH));

        // top row
        for (auto* b : {&undoB_, &redoB_, &gearB_, &pauseB_}) b->draw();
        drawSlider();

        // tabs
        static const char* off[3] = {"edit_buildBtn_001.png", "edit_editBtn_001.png", "edit_deleteBtn_001.png"};
        static const char* on[3] = {"edit_buildSBtn_001.png", "edit_editSBtn_001.png", "edit_deleteSBtn_001.png"};
        for (int i = 0; i < 3; ++i) {
            const Sprite s = E().sprite((int)mode_ == i ? on[i] : off[i]);
            const float k = s.srcW > 0.f ? 160.f / s.srcW : 1.f;
            E().drawSprite(s, 90.f, tabY(i), k, k);
        }

        // right column: swipe / zoom / deselect
        Tile sw; sw.x = 1132.f; sw.y = 149.f; sw.w = 83.f; sw.h = 79.f; sw.icon = "edit_swipeBtn_001.png";
        sw.draw(swiping_ ? "GJ_button_02.png" : "GJ_button_01.png", true, 74.f);
        Tile zi = sw; zi.x = 1227.f; zi.icon = "edit_zoomInBtn_001.png"; zi.draw("GJ_button_01.png", true, 64.f);
        Tile ds = sw; ds.y = 52.f; ds.icon = "edit_deselectBtn_001.png"; ds.draw("GJ_button_01.png", true, 74.f);
        Tile zo = zi; zo.y = 52.f; zo.icon = "edit_zoomOutBtn_001.png"; zo.draw("GJ_button_01.png", true, 64.f);

        // button bar
        if (mode_ == Mode::Delete) {
            drawDeleteBar();
        } else {
            for (int s = 0; s < itemsOnPage(); ++s) {
                Tile t = itemTile(s);
                t.pressed = pressedTile_ == s;
                if (mode_ == Mode::Build) {
                    t.draw(kBuildPageStart[page_] + s == brush_ ? "GJ_button_02.png" : "GJ_button_04.png");
                } else {
                    const Act a = kEditItems[page_ == 0 ? s : 12].act;
                    const bool enabled = a == EditObj ? selectionHasTrigger() : selectionCount() > 0;
                    t.draw("GJ_button_01.png", enabled);
                }
            }
            if (pageCount() > 1) { prevB_.draw(); nextB_.draw(); }
        }
        drawDots();
    }

    void drawDeleteBar() {
        const bool any = selectionCount() > 0;
        LabelButton del("Delete", 540.f, 130.f, 200.f, 60.f, nullptr, "goldFont.fnt", 0.9f);
        LabelButton all("Delete All", 740.f, 130.f, 200.f, 60.f, nullptr, "goldFont.fnt", 0.9f);
        del.plate = any ? "GJ_button_01.png" : "GJ_button_03.png";
        del.draw(); all.draw();
    }

    void drawDots() {
        const int n = pageCount();
        if (n <= 1) return;
        const Sprite dot = E().sprite("smallDot.png");
        for (int i = 0; i < n; ++i) {
            const float x = kW / 2.f + (i - (n - 1) / 2.f) * 34.5f;
            const bool cur = i == page_;
            const Color c = cur ? Color{255, 255, 255} : Color{150, 150, 150};
            if (dot) { const float k = 14.f / std::max(1.f, dot.srcH); E().drawSprite(dot, x, 11.f, k, k, 0, c); }
            else E().fillRect(x - 6.f, 5.f, 12.f, 12.f, c, 255);
        }
    }

    void drawSlider() {
        const Sprite groove = E().sprite("slidergroove_02.png");
        if (groove && groove.srcW > 0.f) E().drawSprite(groove, 639.f, kSlideY, 448.f / groove.srcW, 1.f);
        else E().fillRect(415.f, kSlideY - 6.f, 448.f, 12.f, {20, 20, 20}, 230);
        const float x = kSlideL + sliderFrac() * (kSlideR - kSlideL);
        const char* thumb = sliding_ ? "GJ_moveSBtn.png" : "GJ_moveBtn.png";
        const float k = scaleForHeight(thumb, 80.f);
        E().drawSprite(E().sprite(thumb), x, kSlideY, k, k);
    }

    void makeButtons() {
        auto mk = [&](Button& b, const char* spr, float x, float y, std::function<void()> cb) {
            b = Button(spr, x, y, scaleForHeight(spr, 80.f), std::move(cb));
        };
        mk(undoB_, "GJ_undoBtn_001.png", 54.f, 675.f, [this] { undo(); });
        mk(redoB_, "GJ_redoBtn_001.png", 161.f, 675.f, [this] { redo(); });
        mk(gearB_, "GJ_optionsBtn02_001.png", 1129.f, 675.f, [this] { overlay_ = Overlay::Settings; });
        mk(pauseB_, "GJ_pauseBtn_001.png", 1236.f, 675.f, [this] { overlay_ = Overlay::Pause; });
        prevB_ = Button("GJ_arrow_02_001.png", 342.f, 108.f, scaleForHeight("GJ_arrow_02_001.png", 70.f), [this] { setPage(page_ - 1); });
        nextB_ = Button("GJ_arrow_02_001.png", 917.f, 108.f, scaleForHeight("GJ_arrow_02_001.png", 70.f), [this] { setPage(page_ + 1); });
        nextB_.flipX = true;
        // pause menu
        const char* names[4] = {"Resume", "Save and Play", "Save and Exit", "Exit"};
        const float ys[4] = {502.f, 406.f, 312.f, 217.f};
        for (int i = 0; i < 4; ++i)
            pauseBtns_.emplace_back(names[i], 640.f, ys[i], 507.f, 60.f, [this, i] { onPauseChoice(i); }, "goldFont.fnt", 0.95f);
        okB_ = std::make_unique<LabelButton>("OK", 640.f, 90.f, 110.f, 58.f, [this] {
            if (overlay_ == Overlay::Color) { commitColor(); overlay_ = colorTarget_ == 2 ? Overlay::None : Overlay::Settings; }
            else closeOverlay();
        }, "goldFont.fnt", 0.95f);
    }

    // ---------------------------------------------------------------------------- overlays
    void closeOverlay() { overlay_ = Overlay::None; }
    void onPauseChoice(int i) {
        if (i == 0) closeOverlay();
        else if (i == 1) saveAndPlay();
        else if (i == 2) saveAndExit();
        else dialog_ = std::make_unique<Dialog>("Exit", "<cy>Exit</c> without saving? All changes will be <cr>lost</c>!",
                std::vector<Dialog::Choice>{{"NO", 105.f, nullptr}, {"YES", 135.f, [this] { leave(); }}}, 320.f);
    }
    void updateOverlay(float dt) {
        for (auto& b : pauseBtns_) b.update(dt);
        okB_->update(dt);
    }

    // round buttons of the pause menu, left to right: music, BPM, help
    struct Round { const char* spr; float x; };
    void pauseRounds(Round r[3]) const {
        r[0] = {musicOn_ ? "GJ_audioOnBtn_001.png" : "GJ_audioOffBtn_001.png", 547.f};
        r[1] = {"GJ_BPMOffBtn_001.png", 640.f};
        r[2] = {"GJ_helpBtn_001.png", 733.f};
    }

    void drawOverlay() {
        E().fillRect(0, 0, kW, kH, {0, 0, 0}, 110);
        if (overlay_ == Overlay::Pause) {
            for (auto& b : pauseBtns_) b.draw();
            Round r[3];
            pauseRounds(r);
            for (const Round& b : r) { const float k = scaleForHeight(b.spr, 82.f); E().drawSprite(E().sprite(b.spr), b.x, 90.f, k, k); }
        } else if (overlay_ == Overlay::Settings) {
            drawSettings();
        } else if (overlay_ == Overlay::Color) {
            drawColorPicker();
        }
    }
    void overlayDown(float x, float y) {
        if (overlay_ == Overlay::Pause) {
            for (auto& b : pauseBtns_) if (b.onDown(x, y)) return;
            Round r[3];
            pauseRounds(r);
            for (int i = 0; i < 3; ++i)
                if (std::fabs(x - r[i].x) < 41.f && std::fabs(y - 90.f) < 41.f) { roundDown_ = i; return; }
        } else if (overlay_ == Overlay::Settings) {
            settingsDown(x, y);
        } else if (overlay_ == Overlay::Color) {
            colorDown(x, y);
        }
    }
    void overlayMove(float x, float) { if (overlay_ == Overlay::Color && dragChannel_ >= 0) colorSlide(x); }
    void overlayUp(float x, float y) {
        if (overlay_ == Overlay::Pause) {
            for (auto& b : pauseBtns_) b.onUp(x, y);
            if (roundDown_ >= 0) {
                Round r[3];
                pauseRounds(r);
                if (std::fabs(x - r[roundDown_].x) < 41.f && std::fabs(y - 90.f) < 41.f) onRound(roundDown_);
                roundDown_ = -1;
            }
        } else if (overlay_ == Overlay::Settings) {
            okB_->onUp(x, y);
            settingsUp(x, y);
        } else if (overlay_ == Overlay::Color) {
            okB_->onUp(x, y);
            dragChannel_ = -1;
        }
    }
    void onRound(int i) {
        if (i == 0) {
            musicOn_ = !musicOn_;
            if (musicOn_) {
                const char* file = "StereoMadness.mp3";
                for (const Song& s : kSongs) if (s.id == song_ && s.file) file = s.file;
                E().playMusic(file, -1);
            } else E().stopMusic();
        } else if (i == 1) {
            dialog_ = std::make_unique<Dialog>("BPM", "The beat guide lines are not available in this version yet.",
                std::vector<Dialog::Choice>{{"OK", 110.f, nullptr}});
        } else {
            dialog_ = std::make_unique<Dialog>("Help",
                "<cg>Build</c> places objects, <cg>Edit</c> moves, flips and rotates the selected ones, <cg>Delete</c> removes them.\n"
                "<cl>Swipe</c> selects with a drag. The slider on top scrolls the level.",
                std::vector<Dialog::Choice>{{"OK", 110.f, nullptr}}, 360.f);
        }
    }

    // --- level settings: background / ground colour and song (LevelSettingsLayer)
    int songIndexCount() const { return 10; }                 // tracks 0..9
    void settingsDown(float x, float y) {
        okB_->onDown(x, y);
        if (std::fabs(x - 489.f) < 40.f && std::fabs(y - 383.f) < 40.f) { colorTarget_ = 0; colorTriggerIdx_ = -1; settingsPress_ = 10; }
        else if (std::fabs(x - 790.f) < 40.f && std::fabs(y - 383.f) < 40.f) { colorTarget_ = 1; colorTriggerIdx_ = -1; settingsPress_ = 11; }
        else if (std::fabs(x - 382.f) < 45.f && std::fabs(y - 180.f) < 45.f) settingsPress_ = 12;
        else if (std::fabs(x - 898.f) < 45.f && std::fabs(y - 180.f) < 45.f) settingsPress_ = 13;
        else settingsPress_ = -1;
    }
    void settingsUp(float x, float y) {
        const int p = settingsPress_;
        settingsPress_ = -1;
        if (p < 0) return;
        const float cx[4] = {489.f, 790.f, 382.f, 898.f}, cy[4] = {383.f, 383.f, 180.f, 180.f};
        if (std::fabs(x - cx[p - 10]) > 50.f || std::fabs(y - cy[p - 10]) > 50.f) return;
        if (p == 10 || p == 11) { openColor(p == 10 ? bg_ : ground_); }
        else {
            const int n = songIndexCount();
            song_ = ((song_ + (p == 13 ? 1 : -1)) % n + n) % n;
        }
    }
    void drawSettings() {
        E().drawPanel(E().sprite("square02_001.png"), 640.f, 360.f, 647.f, 676.f, {0, 0, 0}, 200);
        Font* gold = E().font("goldFont.fnt");
        Font* big = E().font("bigFont.fnt");
        E().drawText(gold, "Select Color:", 640.f, 591.f, 1.0f);
        E().drawText(big, "BG:", 492.f, 480.f, 0.9f);
        E().drawText(big, "Ground:", 790.f, 487.f, 0.7f);
        const Sprite sq = E().sprite("whiteSquare60_001.png");
        const float k = 66.f / std::max(1.f, sq.srcW);
        E().drawSprite(sq, 489.f, 383.f, k, k, 0, bg_);
        E().drawSprite(sq, 790.f, 383.f, k, k, 0, ground_);
        E().drawText(gold, "Select Song:", 640.f, 253.f, 1.0f);
        const Sprite l = E().sprite("edit_leftBtn_001.png"), r = E().sprite("edit_rightBtn_001.png");
        E().drawSprite(l, 382.f, 180.f, 1.f, 1.f, 0, {255, 255, 255});
        E().drawSprite(r, 898.f, 180.f, 1.f, 1.f, 0, {255, 255, 255});
        std::string title = "Stereo Madness";
        for (const Song& s : kSongs) if (s.id == song_) title = s.title;
        char line[96];
        std::snprintf(line, sizeof line, "%02d: %s", song_ + 1, title.c_str());
        E().drawText(big, line, 640.f, 167.f, fitScale(big, line, 440.f, 0.75f));
        okB_->draw();
    }

    // --- colour chooser (RGB sliders); used for BG / ground and for colour triggers
    void openColor(Color c) { rgb_[0] = c.r; rgb_[1] = c.g; rgb_[2] = c.b; overlay_ = Overlay::Color; }
    void openColorForSelection() {
        for (size_t i = 0; i < objs_.size(); ++i)
            if (sel_[i] && (objs_[i].id == kObjColorTriggerBG || objs_[i].id == kObjColorTriggerGround)) {
                colorTarget_ = 2;
                colorTriggerIdx_ = (int)i;
                openColor({(Uint8)objs_[i].number(7, 255), (Uint8)objs_[i].number(8, 255), (Uint8)objs_[i].number(9, 255)});
                return;
            }
    }
    static constexpr float kCpL = 440.f, kCpR = 840.f;
    float cpY(int ch) const { return 470.f - ch * 70.f; }
    void colorDown(float x, float y) {
        okB_->onDown(x, y);
        for (int c = 0; c < 3; ++c)
            if (std::fabs(y - cpY(c)) < 30.f && x > kCpL - 25.f && x < kCpR + 25.f) { dragChannel_ = c; colorSlide(x); return; }
    }
    void colorSlide(float x) {
        const float f = std::max(0.f, std::min(1.f, (x - kCpL) / (kCpR - kCpL)));
        rgb_[dragChannel_] = (int)std::lround(f * 255.f);
    }
    void commitColor() {
        const Color c{(Uint8)rgb_[0], (Uint8)rgb_[1], (Uint8)rgb_[2]};
        if (colorTarget_ == 0) bg_ = c;
        else if (colorTarget_ == 1) ground_ = c;
        else if (colorTriggerIdx_ >= 0 && colorTriggerIdx_ < (int)objs_.size()) {
            pushUndo();
            LevelObject& o = objs_[colorTriggerIdx_];
            auto set = [&](int key, int v) {
                for (auto& kv : o.extra) if (kv.first == key) { kv.second = std::to_string(v); return; }
                o.extra.emplace_back(key, std::to_string(v));
            };
            set(7, c.r); set(8, c.g); set(9, c.b);
        }
    }
    void drawColorPicker() {
        E().drawPanel(E().sprite("GJ_square01.png"), 640.f, 360.f, 560.f, 470.f);
        Font* gold = E().font("goldFont.fnt");
        Font* big = E().font("bigFont.fnt");
        E().drawText(gold, "Select Color:", 640.f, 545.f, 0.9f);
        const char* names[3] = {"R", "G", "B"};
        for (int c = 0; c < 3; ++c) {
            const float y = cpY(c) - 10.f;
            E().fillRect(kCpL, y - 4.f, kCpR - kCpL, 8.f, {20, 20, 40}, 255);
            const float x = kCpL + rgb_[c] / 255.f * (kCpR - kCpL);
            Color col{(Uint8)(c == 0 ? 255 : 60), (Uint8)(c == 1 ? 255 : 60), (Uint8)(c == 2 ? 255 : 60)};
            E().fillRect(x - 9.f, y - 17.f, 18.f, 34.f, col, 255);
            E().drawText(big, names[c], kCpL - 40.f, y, 0.6f);
            E().drawText(big, std::to_string(rgb_[c]), kCpR + 45.f, y, 0.5f);
        }
        const Sprite sq = E().sprite("whiteSquare60_001.png");
        const float k = 60.f / std::max(1.f, sq.srcW);
        E().drawSprite(sq, 640.f, 235.f, k, k, 0, {(Uint8)rgb_[0], (Uint8)rgb_[1], (Uint8)rgb_[2]});
        okB_->draw();
    }

    // ------------------------------------------------------------------------------- state
    Level level_;                                     // header only; the objects live in objs_
    std::vector<LevelObject> objs_;
    std::vector<char> sel_;                           // selected flags, parallel to objs_
    std::vector<std::vector<LevelObject>> undo_, redo_;
    Color bg_, ground_;
    int song_ = 0;

    Mode mode_ = Mode::Build;
    int page_ = 0, brush_ = 0, pressedTile_ = -1;
    bool swiping_ = false, swipeDrag_ = false, sliding_ = false;
    float camX_ = 0.f, camY_ = 0.f, zoom_ = 1.125f;
    float camStartX_ = 0.f, camStartY_ = 0.f, downX_ = 0.f, downY_ = 0.f, hoverX_ = 0.f, hoverY_ = 0.f, blink_ = 0.f;
    bool pressed_ = false, moved_ = false, uiPress_ = false, hover_ = false, musicOn_ = false;

    Button undoB_{"GJ_undoBtn_001.png", 0, 0, 1, nullptr}, redoB_{"GJ_redoBtn_001.png", 0, 0, 1, nullptr};
    Button gearB_{"GJ_optionsBtn02_001.png", 0, 0, 1, nullptr}, pauseB_{"GJ_pauseBtn_001.png", 0, 0, 1, nullptr};
    Button prevB_{"GJ_arrow_02_001.png", 0, 0, 1, nullptr}, nextB_{"GJ_arrow_02_001.png", 0, 0, 1, nullptr};
    std::vector<LabelButton> pauseBtns_;
    std::unique_ptr<LabelButton> okB_;
    std::unique_ptr<Dialog> dialog_;
    Overlay overlay_ = Overlay::None;
    int roundDown_ = -1, settingsPress_ = -1;
    int colorTarget_ = 0, colorTriggerIdx_ = -1, dragChannel_ = -1;
    int rgb_[3] = {0, 0, 0};
};

} // namespace

std::unique_ptr<Scene> makeEditorScene() { return std::make_unique<EditorScene>(); }

} // namespace ogd
