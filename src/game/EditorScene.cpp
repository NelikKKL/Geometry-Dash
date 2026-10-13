// Level editor (first version): build / edit / delete objects on a 30-unit grid, undo / redo, pan and zoom,
// play-test and save. The Build palette lists every object the engine knows (ObjectTable); the toolbar uses the
// original's edit_* sprites. The editor level is kept in the save file (editor-level).
#include <algorithm>
#include <cmath>
#include <vector>

#include "core/Level.h"
#include "core/ObjectTable.h"
#include "game/App.h"
#include "game/Scenes.h"
#include "game/Ui.h"
#include "game/World.h"

namespace ogd {
namespace {

constexpr float kPx = 2.f;                       // GD units -> world px (same as the game)
constexpr float kFloorPx = 90.f * kPx;
constexpr float kCell = 30.f;                    // grid size in GD units
constexpr float kBarH = 112.f;                   // height of the bottom palette bar (screen px)
constexpr const char* kDefaultLevel = "kS1,40,kS2,62,kS3,255,kS4,0,kS5,19,kS6,200,kA1,0;";

enum class Mode { Build, Edit, Delete };

class EditorScene : public Scene {
public:
    EditorScene() {
        std::string txt = E().save.editorLevel.empty() ? std::string(kDefaultLevel) : E().save.editorLevel;
        LevelParseResult r = parseLevelString(txt);
        if (!r.ok) r = parseLevelString(kDefaultLevel);
        level_ = std::move(r.level);
        for (const ObjInfo* oi : allObjects()) palette_.push_back(oi->id);
        if (!palette_.empty()) brush_ = palette_[0];
        const auto& st = level_.settings;
        bg_ = st.hasBackground ? Color{st.background.r, st.background.g, st.background.b} : kBlue;
        ground_ = st.hasGround ? Color{st.ground.r, st.ground.g, st.ground.b} : kBlue;
        camX_ = -140.f;
        makeButtons();
    }

    void update(float dt) override {
        for (auto& b : buttons_) b.update(dt);
        for (auto& b : modeBtn_) b.update(dt);
        scrollX_ += (scrollTarget_ - scrollX_) * std::min(1.f, dt * 14.f);
    }

    void draw() override {
        E().clear({0, 0, 0});
        E().setZoom(zoom_);
        drawBackground(camX_ * 0.1f, bg_, 0.f);
        drawGround(camX_, ground_, camY_, kFloorPx);
        drawGrid();
        drawLevelObjects();
        if (mode_ == Mode::Build && hover_) drawGhost();
        E().setZoom(1.f);
        drawUi();
    }

    // ------------------------------------------------------------------------------- input
    void onDown(float x, float y) override {
        downX_ = x; downY_ = y; hoverX_ = x; hoverY_ = y; hover_ = y > kBarH; moved_ = false; pressed_ = true;
        for (auto& b : buttons_) if (b.onDown(x, y)) { uiPress_ = true; return; }
        for (auto& b : modeBtn_) if (b.onDown(x, y)) { uiPress_ = true; return; }
        uiPress_ = false;
        if (y < kBarH && mode_ == Mode::Build) { barPress_ = true; barStartScroll_ = scrollTarget_; return; }
        barPress_ = false;
        camStartX_ = camX_; camStartY_ = camY_;
    }

    void onMove(float x, float y) override {
        hoverX_ = x; hoverY_ = y; hover_ = y > kBarH;
        if (!pressed_ || uiPress_) return;
        if (std::hypot(x - downX_, y - downY_) > 14.f) moved_ = true;
        if (!moved_) return;
        if (barPress_) { scrollTarget_ = clampScroll(barStartScroll_ + (downX_ - x)); return; }
        camX_ = camStartX_ - (x - downX_) / zoom_;                          // drag = pan
        camY_ = std::max(0.f, camStartY_ - (y - downY_) / zoom_);
    }

    void onUp(float x, float y) override {
        pressed_ = false;
        for (auto& b : buttons_) b.onUp(x, y);
        for (auto& b : modeBtn_) b.onUp(x, y);
        if (uiPress_) { uiPress_ = false; return; }
        if (moved_) return;
        if (barPress_) { pickFromPalette(x, y); return; }
        if (y <= kBarH && mode_ == Mode::Build) return;
        tapWorld(x, y);
    }

    void onWheel(float dy) override { zoomBy(dy > 0 ? 1.12f : 1.f / 1.12f); }

    void onKey(SDL_Keycode k, bool down) override {
        if (!down) return;
        switch (k) {
        case SDLK_ESCAPE: exitEditor(); break;
        case SDLK_LEFT: camX_ -= 120.f; break;
        case SDLK_RIGHT: camX_ += 120.f; break;
        case SDLK_UP: camY_ += 120.f; break;
        case SDLK_DOWN: camY_ = std::max(0.f, camY_ - 120.f); break;
        case SDLK_z: undo(); break;
        case SDLK_y: redo(); break;
        case SDLK_SPACE: playTest(); break;
        default: break;
        }
    }

private:
    // ----------------------------------------------------------------------------- coordinates
    float worldX(float sx) const { return camX_ + sx / zoom_; }
    float worldY(float sy) const { return camY_ + sy / zoom_; }
    float unitsX(float sx) const { return worldX(sx) / kPx; }
    float unitsY(float sy) const { return (worldY(sy) - kFloorPx) / kPx; }
    static float snap(float u) { return std::floor(u / kCell) * kCell + kCell / 2.f; }

    // -------------------------------------------------------------------------------- actions
    void pushUndo() {
        undo_.push_back(level_.objects);
        if (undo_.size() > 100) undo_.erase(undo_.begin());
        redo_.clear();
    }
    void undo() {
        if (undo_.empty()) return;
        redo_.push_back(level_.objects);
        level_.objects = std::move(undo_.back());
        undo_.pop_back();
        hasSel_ = false;
    }
    void redo() {
        if (redo_.empty()) return;
        undo_.push_back(level_.objects);
        level_.objects = std::move(redo_.back());
        redo_.pop_back();
        hasSel_ = false;
    }
    void zoomBy(float f) {
        const float cx = worldX(kW / 2.f), cy = worldY(kH / 2.f);
        zoom_ = std::max(0.5f, std::min(2.6f, zoom_ * f));
        camX_ = cx - (kW / 2.f) / zoom_;
        camY_ = std::max(0.f, cy - (kH / 2.f) / zoom_);
    }
    void save() {
        level_.sortByX();
        E().save.editorLevel = serializeLevel(level_);
        E().persist();
    }
    void exitEditor() {
        save();
        app().goTo([] { return makeMenuScene(); });
    }
    void playTest() {
        save();
        const std::string text = E().save.editorLevel;
        app().goTo([text] { return makePlayTestScene(text, [] { return makeEditorScene(); }); });
    }

    int objectAt(float ux, float uy) const {            // topmost object whose cell contains the point
        for (int i = (int)level_.objects.size() - 1; i >= 0; --i) {
            const LevelObject& o = level_.objects[i];
            if (std::fabs(ux - o.x) <= kCell / 2.f && std::fabs(uy - o.y) <= kCell / 2.f) return i;
        }
        return -1;
    }

    void tapWorld(float sx, float sy) {
        const float ux = unitsX(sx), uy = unitsY(sy);
        if (mode_ == Mode::Build) {
            const float cx = snap(ux), cy = std::max(kCell / 2.f, snap(uy));
            for (const LevelObject& o : level_.objects)
                if (o.id == brush_ && o.x == cx && o.y == cy) return;       // already there
            pushUndo();
            LevelObject o;
            o.id = brush_; o.x = cx; o.y = cy;
            level_.objects.push_back(o);
            level_.sortByX();
        } else if (mode_ == Mode::Delete) {
            const int i = objectAt(ux, uy);
            if (i >= 0) { pushUndo(); level_.objects.erase(level_.objects.begin() + i); hasSel_ = false; }
        } else {
            const int i = objectAt(ux, uy);
            if (i < 0) { hasSel_ = false; return; }
            hasSel_ = true;
            selId_ = level_.objects[i].id; selX_ = level_.objects[i].x; selY_ = level_.objects[i].y;
        }
    }

    int selectedIndex() const {
        if (!hasSel_) return -1;
        for (size_t i = 0; i < level_.objects.size(); ++i) {
            const LevelObject& o = level_.objects[i];
            if (o.id == selId_ && o.x == selX_ && o.y == selY_) return (int)i;
        }
        return -1;
    }
    void editSelected(int what) {                       // 0 rotate cw, 1 rotate ccw, 2 flipX, 3 flipY, 4 delete, 5/6/7/8 nudge
        const int i = selectedIndex();
        if (i < 0) return;
        pushUndo();
        LevelObject& o = level_.objects[i];
        switch (what) {
        case 0: o.rotation = std::fmod(o.rotation + 90.f, 360.f); break;
        case 1: o.rotation = std::fmod(o.rotation + 270.f, 360.f); break;
        case 2: o.flipX = !o.flipX; break;
        case 3: o.flipY = !o.flipY; break;
        case 4: level_.objects.erase(level_.objects.begin() + i); hasSel_ = false; return;
        case 5: o.y += kCell; selY_ = o.y; break;
        case 6: o.y = std::max(kCell / 2.f, o.y - kCell); selY_ = o.y; break;
        case 7: o.x -= kCell; selX_ = o.x; level_.sortByX(); break;
        case 8: o.x += kCell; selX_ = o.x; level_.sortByX(); break;
        }
    }

    // ------------------------------------------------------------------------------- palette
    static constexpr float kItem = 84.f;
    float paletteWidth() const { return (float)palette_.size() * kItem; }
    float clampScroll(float v) const { return std::max(0.f, std::min(std::max(0.f, paletteWidth() - (kW - 330.f)), v)); }
    float itemX(size_t i) const { return 190.f + (float)i * kItem + kItem / 2 - scrollX_; }

    void pickFromPalette(float x, float y) {
        (void)y;
        for (size_t i = 0; i < palette_.size(); ++i)
            if (std::fabs(x - itemX(i)) < kItem / 2 && x > 170.f && x < kW - 150.f) { brush_ = palette_[i]; return; }
    }

    // --------------------------------------------------------------------------------- drawing
    void drawGrid() {
        const float x0 = std::floor(camX_ / (kCell * kPx)) * kCell * kPx;
        const float visW = kW / zoom_, visH = kH / zoom_;
        for (float wx = x0; wx < camX_ + visW + 60.f; wx += kCell * kPx)
            E().fillRect(wx - camX_ - 0.5f, 0.f, 1.f, visH, {255, 255, 255}, wx == 0.f ? 120 : 28);
        for (float wy = kFloorPx; wy < camY_ + visH + 60.f; wy += kCell * kPx)
            E().fillRect(0.f, wy - camY_ - 0.5f, visW, 1.f, {255, 255, 255}, 28);
        // start line
        E().fillRect(-camX_ - 1.5f, 0.f, 3.f, visH, {120, 255, 120}, 150);
    }

    void drawObjectAt(const LevelObject& o, Uint8 alpha = 255, Color tint = {}) {
        const ObjInfo* info = objectInfo(o.id);
        if (!info) return;
        const float sx = o.x * kPx - camX_, sy = kFloorPx + o.y * kPx - camY_;
        if (info->spriteBack) E().drawSprite(E().sprite(info->spriteBack), sx, sy, 1.f, 1.f, o.rotation, tint, alpha, o.flipX, o.flipY);
        if (info->sprite) E().drawSprite(E().sprite(info->sprite), sx, sy, 1.f, 1.f, o.rotation, tint, alpha, o.flipX, o.flipY);
    }

    void drawLevelObjects() {
        const float visW = kW / zoom_;
        auto r = level_.range((camX_ - 200.f) / kPx, (camX_ + visW + 200.f) / kPx);
        std::vector<size_t> idx;
        for (size_t i = r.first; i < r.second; ++i) idx.push_back(i);
        std::stable_sort(idx.begin(), idx.end(), [&](size_t a, size_t b) {
            const ObjInfo* ia = objectInfo(level_.objects[a].id); const ObjInfo* ib = objectInfo(level_.objects[b].id);
            return (ia ? ia->z : 0) < (ib ? ib->z : 0);
        });
        for (size_t i : idx) drawObjectAt(level_.objects[i]);
        const int si = selectedIndex();
        if (si >= 0) {                                   // selection: bright frame
            const LevelObject& o = level_.objects[si];
            const float sx = o.x * kPx - camX_, sy = kFloorPx + o.y * kPx - camY_, h = kCell * kPx / 2.f;
            E().fillRect(sx - h, sy - h, h * 2, 3.f, {120, 255, 120}, 255);
            E().fillRect(sx - h, sy + h - 3.f, h * 2, 3.f, {120, 255, 120}, 255);
            E().fillRect(sx - h, sy - h, 3.f, h * 2, {120, 255, 120}, 255);
            E().fillRect(sx + h - 3.f, sy - h, 3.f, h * 2, {120, 255, 120}, 255);
        }
    }

    void drawGhost() {
        LevelObject g;
        g.id = brush_;
        g.x = snap(unitsX(hoverX_));
        g.y = std::max(kCell / 2.f, snap(unitsY(hoverY_)));
        drawObjectAt(g, 140);
    }

    void drawUi() {
        for (auto& b : buttons_) b.draw();
        for (size_t i = 0; i < modeBtn_.size(); ++i) {
            modeBtn_[i].setSprite(modeSprite(i, (int)mode_ == (int)i));
            modeBtn_[i].draw();
        }
        Font* chat = E().font("chatFont.fnt");
        E().drawText(chat, std::to_string(level_.objects.size()) + " objects", kW / 2.f, kH - 28.f, 0.9f, Align::Center, {255, 255, 255}, 200);
        if (mode_ == Mode::Build) {
            E().fillRect(0.f, 0.f, kW, kBarH, {0, 0, 0}, 150);
            E().setClip(170.f, 0.f, kW - 320.f, kBarH);
            for (size_t i = 0; i < palette_.size(); ++i) {
                const float x = itemX(i);
                if (x < 130.f || x > kW - 130.f) continue;
                const ObjInfo* info = objectInfo(palette_[i]);
                if (palette_[i] == brush_) E().fillRect(x - kItem / 2 + 4.f, 8.f, kItem - 8.f, kBarH - 16.f, {255, 255, 255}, 70);
                if (!info) continue;
                auto thumb = [&](const char* name) {
                    Sprite s = E().sprite(name);
                    if (!s || s.w <= 0) return;
                    const float k = std::min(1.f, 62.f / std::max(s.w, s.h));
                    E().drawSprite(s, x, kBarH / 2.f, k, k);
                };
                if (info->spriteBack) thumb(info->spriteBack);
                if (info->sprite) thumb(info->sprite);
            }
            E().clearClip();
        } else if (mode_ == Mode::Edit) {
            E().fillRect(0.f, 0.f, kW, kBarH, {0, 0, 0}, 150);
            for (auto& b : editBtns_) b.draw();
            if (!hasSel_) E().drawText(chat, "Tap an object to select it", kW / 2.f, kBarH / 2.f - 6.f, 1.0f, Align::Center, {255, 255, 255}, 200);
        }
    }

    static const char* modeSprite(size_t i, bool on) {
        static const char* off[3] = {"edit_buildBtn_001.png", "edit_editBtn_001.png", "edit_deleteBtn_001.png"};
        static const char* sel[3] = {"edit_buildSBtn_001.png", "edit_editSBtn_001.png", "edit_deleteSBtn_001.png"};
        return on ? sel[i] : off[i];
    }

    void makeButtons() {
        buttons_.reserve(8);
        buttons_.emplace_back("GJ_arrow_01_001.png", 50.f, kH - 50.f, 1.f, [this] { exitEditor(); });
        buttons_.emplace_back("GJ_playBtn2_001.png", kW - 60.f, kH - 60.f, 0.55f, [this] { playTest(); });
        buttons_.emplace_back("GJ_undoBtn_001.png", kW - 150.f, kH - 56.f, 1.f, [this] { undo(); });
        buttons_.emplace_back("GJ_redoBtn_001.png", kW - 220.f, kH - 56.f, 1.f, [this] { redo(); });
        buttons_.emplace_back("edit_zoomInBtn_001.png", kW - 300.f, kH - 56.f, 1.f, [this] { zoomBy(1.2f); });
        buttons_.emplace_back("edit_zoomOutBtn_001.png", kW - 370.f, kH - 56.f, 1.f, [this] { zoomBy(1.f / 1.2f); });
        modeBtn_.reserve(3);
        for (size_t i = 0; i < 3; ++i)
            modeBtn_.emplace_back(modeSprite(i, i == 0), 105.f, 560.f - (float)i * 92.f, 0.9f, [this, i] { mode_ = (Mode)i; hasSel_ = false; });
        editBtns_.reserve(8);
        const char* names[8] = {"edit_cwBtn_001.png", "edit_ccwBtn_001.png", "edit_flipXBtn_001.png", "edit_flipYBtn_001.png",
                                "edit_upBtn_001.png", "edit_downBtn_001.png", "edit_leftBtn_001.png", "edit_rightBtn_001.png"};
        const int what[8] = {0, 1, 2, 3, 5, 6, 7, 8};
        for (int i = 0; i < 8; ++i)
            editBtns_.emplace_back(names[i], 230.f + i * 90.f, kBarH / 2.f, 0.9f, [this, w = what[i]] { editSelected(w); });
        editBtns_.emplace_back("edit_deleteBtn_001.png", 230.f + 8 * 90.f, kBarH / 2.f, 0.9f, [this] { editSelected(4); });
    }

    // ---------------------------------------------------------------------------------- state
    Level level_;
    std::vector<int> palette_;
    int brush_ = 1;
    Mode mode_ = Mode::Build;
    Color bg_, ground_;
    float camX_ = 0.f, camY_ = 0.f, zoom_ = 1.125f;
    float camStartX_ = 0.f, camStartY_ = 0.f, downX_ = 0.f, downY_ = 0.f, hoverX_ = 0.f, hoverY_ = 0.f;
    bool pressed_ = false, moved_ = false, uiPress_ = false, barPress_ = false, hover_ = false;
    float scrollX_ = 0.f, scrollTarget_ = 0.f, barStartScroll_ = 0.f;
    bool hasSel_ = false;
    int selId_ = 0;
    float selX_ = 0.f, selY_ = 0.f;
    std::vector<std::vector<LevelObject>> undo_, redo_;
    std::vector<Button> buttons_, modeBtn_, editBtns_;
};

} // namespace

std::unique_ptr<Scene> makeEditorScene() { return std::make_unique<EditorScene>(); }

} // namespace ogd
