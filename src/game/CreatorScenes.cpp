// Creator hub and the screens behind it, rebuilt from CreatorLayer / MyLevelsLayer / EditLevelLayer / LevelCell /
// LevelBrowserLayer / SavedLevelsLayer of the 1.0 binary and from screenshots of the Android version.
// Layout numbers are design px (1280x720, y-up), measured on the screenshots (viewport 1.86:1 mapped onto 1280x720).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "core/Level.h"
#include "game/Boards.h"
#include "game/Dialog.h"
#include "game/Popup.h"
#include "game/Scenes.h"
#include "game/Songs.h"
#include "game/Ui.h"
#include "game/World.h"

namespace ogd {
namespace {

constexpr float kBackX = 52.f, kBackY = kH - 50.f;
const Color kFieldBlue{110, 150, 230};

// Scale that makes `name` exactly `h` design px tall (1 if the sprite is missing).
float scaleForHeight(const std::string& name, float h) {
    const Sprite s = E().sprite(name);
    return (s && s.srcH > 0.f) ? h / s.srcH : 1.f;
}
void drawFitH(const std::string& name, float x, float y, float h) {
    const float s = scaleForHeight(name, h);
    E().drawSprite(E().sprite(name), x, y, s, s);
}
void drawBlueBackground() { drawVGradient(0.f, (float)kH, {0, 40, 130}, {0, 102, 255}); }
void drawDarkPanel(float cx, float cy, float w, float h, Uint8 alpha = 105) {
    E().drawPanel(E().sprite("square02_001.png"), cx, cy, w, h, {0, 0, 0}, alpha);
}

// ----------------------------------------------------------------------------------------------- level info
const char* lengthName(const std::string& data) {
    if (data.empty()) return "Tiny";
    const LevelParseResult r = parseLevelString(data);
    if (!r.ok) return "Tiny";
    const float seconds = r.level.maxX() / 311.58f;     // 1x speed
    if (seconds < 10.f) return "Tiny";
    if (seconds < 30.f) return "Short";
    if (seconds < 60.f) return "Medium";
    return "Long";
}
std::string songTitle(int id) {
    for (const Song& s : kSongs) if (s.id == id) return s.title;
    return kSongs[1].title;
}

// ---------------------------------------------------------------------------------------- screen base
// Gradient background, optional corner art, back arrow, modal dialog handling.
class Screen : public Scene {
public:
    Screen(const char* arrowSprite, std::function<void()> onBack, bool topCorners, bool bottomCorners)
        : back_(arrowSprite, kBackX, kBackY, 1.f, std::move(onBack)), topCorners_(topCorners), bottomCorners_(bottomCorners) {}

    void update(float dt) override {
        if (dlg_) {
            dlg_->update(dt);
            if (dlg_->closed) {
                auto a = std::move(dlg_->after);
                dlg_.reset();
                if (a) a();
            }
            return;
        }
        back_.update(dt);
        updateScreen(dt);
    }
    void draw() override {
        drawBlueBackground();
        if (bottomCorners_) { drawCornerArt(Corner::BottomLeft); drawCornerArt(Corner::BottomRight); }
        if (topCorners_) { drawCornerArt(Corner::TopLeft); drawCornerArt(Corner::TopRight); }
        drawScreen();
        back_.draw();
        drawTop();
        if (dlg_) dlg_->draw();
    }
    void onDown(float x, float y) override {
        if (dlg_) { dlg_->onDown(x, y); return; }
        if (back_.onDown(x, y)) return;
        screenDown(x, y);
    }
    void onUp(float x, float y) override {
        if (dlg_) { dlg_->onUp(x, y); return; }
        back_.onUp(x, y);
        screenUp(x, y);
    }
    void onMove(float x, float y) override { if (!dlg_) screenMove(x, y); }
    void onWheel(float dy) override { if (!dlg_) screenWheel(dy); }
    void onKey(SDL_Keycode k, bool down) override {
        if (!down) return;
        if (dlg_) { if (k == SDLK_ESCAPE) dlg_->closed = true; return; }
        if (!screenKey(k) && k == SDLK_ESCAPE) { back_.onDown(back_.x, back_.y); back_.onUp(back_.x, back_.y); }
    }

protected:
    virtual void updateScreen(float) {}
    virtual void drawScreen() {}
    virtual void drawTop() {}                 // drawn after the back arrow, below dialogs
    virtual void screenDown(float, float) {}
    virtual void screenUp(float, float) {}
    virtual void screenMove(float, float) {}
    virtual void screenWheel(float) {}
    virtual bool screenKey(SDL_Keycode) { return false; }   // true = handled (Escape does not go back)

    void ask(const std::string& title, const std::string& body, std::vector<Dialog::Choice> choices, float panelH = 307.f) {
        dlg_ = std::make_unique<Dialog>(title, body, std::move(choices), panelH);
    }
    void alert(const std::string& title, const std::string& body) { ask(title, body, {{"OK", 110.f, nullptr}}); }
    bool dialogOpen() const { return (bool)dlg_; }

    Button back_;

private:
    bool topCorners_, bottomCorners_;
    std::unique_ptr<Dialog> dlg_;
};

// ---------------------------------------------------------------------------------------------- Creator
class CreatorScene : public Screen {
public:
    CreatorScene() : Screen("GJ_arrow_03_001.png", [] { app().goTo([] { return makeMenuScene(); }); }, true, true) {
        struct B { const char* spr; float x, y; std::function<void()> cb; };
        const B items[4] = {
            {"GJ_createBtn_001.png", 500.f, 507.f, [] { app().goTo([] { return makeMyLevelsScene(); }); }},
            {"GJ_savedBtn_001.png", 780.f, 507.f, [] { app().goTo([] { return makeOnlineLevelsScene(OnlineKind::Saved); }); }},
            {"GJ_featuredBtn_001.png", 500.f, 213.f, [] { app().goTo([] { return makeOnlineLevelsScene(OnlineKind::Featured); }); }},
            {"GJ_searchBtn_001.png", 780.f, 213.f, [] { app().goTo([] { return makeSearchScene(); }); }},
        };
        for (const B& b : items) {
            auto cb = b.cb;
            buttons_.emplace_back(b.spr, b.x, b.y, scaleForHeight(b.spr, 206.f), [cb] { E().playSfx("playSound_01.ogg"); cb(); });
        }
    }

protected:
    void updateScreen(float dt) override { for (auto& b : buttons_) b.update(dt); }
    void drawScreen() override { for (auto& b : buttons_) b.draw(); }
    void screenDown(float x, float y) override { for (auto& b : buttons_) if (b.onDown(x, y)) break; }
    void screenUp(float x, float y) override { for (auto& b : buttons_) b.onUp(x, y); }

private:
    std::vector<Button> buttons_;
};

// ------------------------------------------------------------------------------------- list screens
// Frame, title and list area shared by My Levels / Online Levels / Saved Levels.
void drawListFrame(const char* title) {
    Board::drawFrame(632.f, 77.f, Board::kInTop, Board::kInBottom, 0.f, false);
    Font* big = E().font("bigFont.fnt");
    E().drawText(big, title, kW / 2.f, 632.f - 12.f, fitScale(big, "Stats", 170.f, 10.f));
}
void drawListBackground() {
    E().fillRect(Board::kInL, Board::kInBottom, Board::kInR - Board::kInL, Board::kInTop - Board::kInBottom, {194, 114, 63}, 255);
}
void drawBigMessage(const std::string& markup, float y) {
    drawRichText(E().font("bigFont.fnt"), markup, kW / 2.f, y, 0.62f, 640.f, 53.f);
}

// ---------------------------------------------------------------------------------------------- My Levels
class MyLevelsScene : public Screen {
    static constexpr float kCellH = 156.f;
public:
    MyLevelsScene()
        : Screen("GJ_arrow_01_001.png", [] { app().goTo([] { return makeCreatorScene(); }); }, false, true),
          newBtn_("GJ_newBtn_001.png", 1215.f, 653.f, scaleForHeight("GJ_newBtn_001.png", 117.f), [this] { createLevel(); }) {}

protected:
    void updateScreen(float dt) override {
        newBtn_.update(dt);
        syncButtons();
        for (auto& b : viewBtns_) b.update(dt);
        if (!dragging_) scroll_ = std::max(0.f, std::min(maxScroll(), scroll_));
    }
    void drawScreen() override {
        drawListBackground();
        const auto& lv = E().save.customLevels;
        syncButtons();
        if (lv.empty()) {
            drawListFrame("My Levels");
            drawBigMessage("Tap <cg>New</c> to create a <cl>level</c>!", 347.f);
            return;
        }
        E().setClip(Board::kInL, Board::kInBottom, Board::kInR - Board::kInL, Board::kInTop - Board::kInBottom);
        for (size_t i = 0; i < lv.size(); ++i) {
            const float top = Board::kInTop - i * kCellH + scroll_;
            if (top < Board::kInBottom || top - kCellH > Board::kInTop) continue;
            drawCell(lv[i], (int)i, top);
        }
        E().clearClip();
        drawListFrame("My Levels");
    }
    void drawTop() override { newBtn_.draw(); }
    void screenDown(float x, float y) override {
        if (newBtn_.onDown(x, y)) return;
        downX_ = x; downY_ = y; lastY_ = y; moved_ = false;
        if (x >= Board::kInL && x <= Board::kInR && y >= Board::kInBottom && y <= Board::kInTop) {
            dragging_ = true;
            for (auto& b : viewBtns_) if (b.onDown(x, y)) { pressedView_ = true; break; }
        }
    }
    void screenMove(float, float y) override {
        if (!dragging_) return;
        if (std::fabs(y - downY_) > 12.f) moved_ = true;
        if (moved_) scroll_ += y - lastY_;
        lastY_ = y;
    }
    void screenUp(float x, float y) override {
        newBtn_.onUp(x, y);
        if (pressedView_) { if (!moved_) for (auto& b : viewBtns_) b.onUp(x, y); pressedView_ = false; }
        dragging_ = false;
    }
    void screenWheel(float dy) override { scroll_ = std::max(0.f, std::min(maxScroll(), scroll_ - dy * 40.f)); }

private:
    float maxScroll() const {
        return std::max(0.f, E().save.customLevels.size() * kCellH - (Board::kInTop - Board::kInBottom));
    }
    void createLevel() {
        E().save.customLevels.insert(E().save.customLevels.begin(), SaveData::CustomLevel());
        E().save.editingLevel = 0;
        E().persist();
        E().playSfx("playSound_01.ogg");
        app().goTo([] { return makeEditLevelScene(true); });
    }
    void drawCell(const SaveData::CustomLevel& c, int index, float top) {
        const float mid = top - kCellH / 2.f;
        E().fillRect(Board::kInL, top - kCellH, Board::kInR - Board::kInL, kCellH, (index % 2 == 0) ? Color{162, 88, 45} : Color{194, 114, 63}, 255);
        Font* big = E().font("bigFont.fnt");
        if (!c.name.empty()) E().drawText(big, c.name, 300.f, mid + 36.f, fitScale(big, c.name, 480.f, 0.85f), Align::Left);
        const float rowY = mid - 34.f;
        drawFitH("GJ_timeIcon_001.png", 301.f, rowY, 37.f);
        E().drawText(big, lengthName(c.data), 324.f, rowY, 0.45f, Align::Left);
        drawFitH("GJ_noteIcon_001.png", 419.f, rowY, 37.f);
        const std::string song = songTitle(c.song);
        E().drawText(big, song, 441.f, rowY, fitScale(big, song, 120.f, 0.45f), Align::Left);
        drawFitH("GJ_infoIcon_001.png", 573.f, rowY, 37.f);
        E().drawText(big, "Unverified", 595.f, rowY, 0.45f, Align::Left);

        viewBtns_[index].draw();
    }
    // One persistent View button per level (so press/release animation survives); moved to its row every frame.
    void syncButtons() {
        const size_t n = E().save.customLevels.size();
        if (viewBtns_.size() != n) {
            viewBtns_.clear();
            for (size_t i = 0; i < n; ++i)
                viewBtns_.emplace_back("View", 940.f, 0.f, 127.f, 63.f, [i] {
                    E().save.editingLevel = (int)i;
                    E().playSfx("playSound_01.ogg");
                    app().goTo([] { return makeEditLevelScene(false); });
                });
        }
        for (size_t i = 0; i < n; ++i) viewBtns_[i].y = Board::kInTop - i * kCellH + scroll_ - kCellH / 2.f + 1.f;
    }

    Button newBtn_;
    std::vector<LabelButton> viewBtns_;
    float scroll_ = 0.f, downX_ = 0.f, downY_ = 0.f, lastY_ = 0.f;
    bool dragging_ = false, moved_ = false, pressedView_ = false;
};

// ---------------------------------------------------------------------------------------------- Edit level
class EditLevelScene : public Screen {
    enum Field { None, Name, Desc };
    static constexpr size_t kMaxName = 20, kMaxDesc = 140;
public:
    explicit EditLevelScene(bool justCreated)
        : Screen("GJ_arrow_01_001.png", [this] { leave(); }, false, true), justCreated_(justCreated) {
        const SaveData::CustomLevel& c = level();
        name_ = c.name;
        desc_ = c.desc;
        const float d = 160.f;
        edit_ = std::make_unique<Button>("GJ_editBtn_001.png", 427.f, 304.f, scaleForHeight("GJ_editBtn_001.png", d), [this] { openEditor(); });
        play_ = std::make_unique<Button>("GJ_playBtn2_001.png", 640.f, 304.f, scaleForHeight("GJ_playBtn2_001.png", d), [this] { playLevel(); });
        share_ = std::make_unique<Button>("GJ_shareBtn_001.png", 853.f, 304.f, scaleForHeight("GJ_shareBtn_001.png", d), [this] { shareLevel(); });
        del_ = std::make_unique<Button>("GJ_deleteBtn_001.png", 1215.f, 653.f, scaleForHeight("GJ_deleteBtn_001.png", 98.f), [this] { askDelete(); });
        help_ = std::make_unique<Button>("GJ_helpBtn_001.png", 1215.f, 518.f, scaleForHeight("GJ_helpBtn_001.png", 98.f), [this] { askGuide(false); });
        if (justCreated_) askGuide(true);
    }
    ~EditLevelScene() override { SDL_StopTextInput(); }

protected:
    void updateScreen(float dt) override {
        for (Button* b : {edit_.get(), play_.get(), share_.get(), del_.get(), help_.get()}) b->update(dt);
        blink_ += dt;
    }
    void drawScreen() override {
        const SaveData::CustomLevel& c = level();
        Font* big = E().font("bigFont.fnt");
        Font* chat = E().font("chatFont.fnt");
        drawDarkPanel(640.f, 653.f, 785.f, 89.f);
        if (name_.empty() && field_ != Name) E().drawText(big, "Level Name", 640.f, 653.f, 1.0f, Align::Center, kFieldBlue);
        else E().drawText(big, name_ + caret(Name), 640.f, 653.f, fitScale(big, name_, 700.f, 1.0f));
        drawDarkPanel(640.f, 507.f, 785.f, 133.f);
        if (desc_.empty() && field_ != Desc) E().drawText(chat, "Description [Optional]", 640.f, 507.f, 1.0f, Align::Center, kFieldBlue);
        else drawRichText(chat, desc_ + caret(Desc), 640.f, 507.f + (desc_.size() > 60 ? 18.f : 0.f), 1.0f, 730.f, 36.f);

        edit_->draw(); play_->draw(); share_->draw(); del_->draw(); help_->draw();

        // info line: length / song / verified, then version and id
        drawFitH("GJ_timeIcon_001.png", 296.f, 124.f, 37.f);
        E().drawText(big, lengthName(c.data), 333.f, 124.f, 0.5f, Align::Left);
        drawFitH("GJ_noteIcon_001.png", 528.f, 124.f, 37.f);
        const std::string song = songTitle(c.song);
        E().drawText(big, song, 562.f, 124.f, fitScale(big, song, 230.f, 0.5f), Align::Left);
        drawFitH("GJ_infoIcon_001.png", 844.f, 124.f, 37.f);
        E().drawText(big, "Unverified", 880.f, 124.f, 0.5f, Align::Left);
        char v[32];
        std::snprintf(v, sizeof v, "Version: %d", c.version);
        Font* gold = E().font("goldFont.fnt");
        E().drawText(gold, v, 535.f, 21.f, 0.55f);
        E().drawText(gold, "ID: NA", 756.f, 21.f, 0.55f);
    }
    void screenDown(float x, float y) override {
        for (Button* b : {edit_.get(), play_.get(), share_.get(), del_.get(), help_.get()}) if (b->onDown(x, y)) return;
        if (std::fabs(x - 640.f) < 392.f && std::fabs(y - 653.f) < 45.f) startField(Name);
        else if (std::fabs(x - 640.f) < 392.f && std::fabs(y - 507.f) < 66.f) startField(Desc);
        else stopField();
    }
    void screenUp(float x, float y) override {
        for (Button* b : {edit_.get(), play_.get(), share_.get(), del_.get(), help_.get()}) b->onUp(x, y);
    }
    bool screenKey(SDL_Keycode k) override {
        if (field_ == None) return false;
        if (k == SDLK_BACKSPACE) { std::string& s = field_ == Name ? name_ : desc_; if (!s.empty()) s.pop_back(); }
        else if (k == SDLK_RETURN || k == SDLK_ESCAPE) stopField();
        return true;
    }
    void onText(const std::string& s) override {
        if (field_ == None) return;
        std::string& t = field_ == Name ? name_ : desc_;
        const size_t cap = field_ == Name ? kMaxName : kMaxDesc;
        for (unsigned char ch : s)
            if (ch >= 32 && ch < 127 && t.size() < cap && !(field_ == Name && (ch == ';' || ch == ','))) t += (char)ch;
    }

private:
    SaveData::CustomLevel& level() {
        auto& v = E().save.customLevels;
        if (v.empty()) v.emplace_back();
        int& i = E().save.editingLevel;
        i = std::max(0, std::min((int)v.size() - 1, i));
        return v[i];
    }
    std::string caret(Field f) const { return (field_ == f && std::fmod(blink_, 1.f) < 0.5f) ? "|" : (field_ == f ? " " : ""); }
    void startField(Field f) { field_ = f; blink_ = 0.f; SDL_StartTextInput(); }
    void stopField() { if (field_ == None) return; field_ = None; SDL_StopTextInput(); store(); }
    void store() { SaveData::CustomLevel& c = level(); c.name = name_; c.desc = desc_; E().persist(); }
    void leave() { stopField(); store(); app().goTo([] { return makeMyLevelsScene(); }); }

    void openEditor() { stopField(); store(); E().playSfx("playSound_01.ogg"); app().goTo([] { return makeEditorScene(); }); }
    void playLevel() {
        stopField(); store();
        const std::string data = level().data;
        if (data.empty()) { alert("Empty Level", "Build something in the editor first."); return; }
        E().playSfx("playSound_01.ogg");
        app().goTo([data] { return makePlayTestScene(data, [] { return makeEditLevelScene(false); }); });
    }
    void shareLevel() {
        stopField(); store();
        if (level().data.empty()) { alert("Share", "You can't share an empty level."); return; }
        if (level().name.empty()) { alert("Share", "You must name your level before sharing it."); return; }
        alert("Share", "Sharing needs the online servers, which this browser version does not have.");
    }
    void askDelete() {
        stopField();
        ask("Delete Level", "Are you sure you want to <cr>delete</c> this level?",
            {{"NO", 105.f, nullptr}, {"YES", 135.f, [this] {
                 auto& v = E().save.customLevels;
                 const int i = E().save.editingLevel;
                 if (i >= 0 && i < (int)v.size()) v.erase(v.begin() + i);
                 E().save.editingLevel = -1;
                 E().persist();
                 app().goTo([] { return makeMyLevelsScene(); });
             }}});
    }
    void askGuide(bool recommended) {
        stopField();
        const std::string text = recommended
            ? "It is recommended that you view the editor quick start guide before creating a level. Open the editor guide now?"
            : "Do you want to open the editor guide now?";
        ask("Editor Guide", text, {{"NO", 105.f, nullptr}, {"YES", 135.f, [this] {
                 ask("Quick Start",
                     "<cg>Build</c> places objects, <cg>Edit</c> moves and rotates the selected ones, <cg>Delete</c> removes them.\n"
                     "Tap the pause button to save, play-test or exit. Use the slider on top to scroll through the level.",
                     {{"OK", 110.f, nullptr}}, 330.f);
             }}}, justCreated_ ? 340.f : 307.f);
    }

    bool justCreated_;
    std::string name_, desc_;
    Field field_ = None;
    float blink_ = 0.f;
    std::unique_ptr<Button> edit_, play_, share_, del_, help_;
};

// ------------------------------------------------------------------------------------ Online / Saved
class OnlineLevelsScene : public Screen {
public:
    explicit OnlineLevelsScene(OnlineKind kind)
        : Screen("GJ_arrow_01_001.png", [] { app().goTo([] { return makeCreatorScene(); }); }, false, true), kind_(kind) {}

protected:
    void updateScreen(float dt) override { t_ += dt; }
    void drawScreen() override {
        drawListBackground();
        drawListFrame(kind_ == OnlineKind::Saved ? "Saved Levels" : "Online Levels");
        if (kind_ == OnlineKind::Saved) drawBigMessage("You have not <cg>downloaded</c> any\n<cl>levels</c> yet!", 373.f);
        else if (t_ > 1.0f) drawBigMessage("No <cg>Internet</c> connection!", 347.f);   // there are no servers in the browser version
    }

private:
    OnlineKind kind_;
    float t_ = 0.f;
};

// ------------------------------------------------------------------------------------------------ Search
class SearchScene : public Screen {
public:
    SearchScene() : Screen("GJ_arrow_01_001.png", [] { app().goTo([] { return makeCreatorScene(); }); }, false, true) {
        auto go = [] { app().goTo([] { return makeOnlineLevelsScene(OnlineKind::Results); }); };
        auto mk = [&](const char* text, float x, float y, float w, float h, const char* plate, float ts) {
            buttons_.emplace_back(text, x, y, w, h, go, "bigFont.fnt", ts);
            buttons_.back().plate = plate;
        };
        mk("Search", 916.f, 630.f, 190.f, 60.f, "GJ_button_02.png", 0.9f);
        mk("Most Downloaded", 446.f, 461.f, 360.f, 62.f, "GJ_button_01.png", 0.6f);
        mk("Most Liked", 834.f, 461.f, 360.f, 62.f, "GJ_button_01.png", 0.7f);
        mk("Trending", 446.f, 371.f, 360.f, 62.f, "GJ_button_01.png", 0.7f);
        mk("Most Recent", 834.f, 371.f, 360.f, 62.f, "GJ_button_01.png", 0.7f);
    }

protected:
    void updateScreen(float dt) override { for (auto& b : buttons_) b.update(dt); }
    void drawScreen() override {
        Font* big = E().font("bigFont.fnt");
        drawDarkPanel(640.f, 630.f, 785.f, 90.f, 120);
        E().drawText(big, "Search for a level or ID", 268.f, 630.f, 0.6f, Align::Left, kFieldBlue);

        E().drawText(big, "Quick Search", 640.f, 535.f, 0.62f);
        drawDarkPanel(640.f, 416.f, 785.f, 193.f, 120);

        E().drawText(big, "Filters", 640.f, 277.f, 0.62f);
        drawDarkPanel(640.f, 215.f, 785.f, 108.f, 120);
        static const char* faces[6] = {"NA", "Easy", "Normal", "Hard", "Harder", "Insane"};
        static const float fx[6] = {307.f, 426.f, 558.f, 691.f, 823.f, 965.f};
        for (int i = 0; i < 6; ++i) {
            char n[40];
            std::snprintf(n, sizeof n, "difficulty_%02d_btn_001.png", i);
            drawFitH(n, fx[i], 215.f, 66.f);
            E().drawText(big, faces[i], fx[i], 165.f, 0.4f, Align::Center, {170, 170, 170});
        }
        drawDarkPanel(640.f, 65.f, 785.f, 68.f, 120);
        drawFitH("GJ_timeIcon_001.png", 319.f, 65.f, 45.f);
        static const char* lens[4] = {"Tiny", "Short", "Medium", "Long"};
        static const float lx[4] = {434.f, 589.f, 771.f, 938.f};
        for (int i = 0; i < 4; ++i) E().drawText(big, lens[i], lx[i], 65.f, 0.62f, Align::Center, {170, 170, 170});
        for (auto& b : buttons_) b.draw();
    }
    void screenDown(float x, float y) override { for (auto& b : buttons_) if (b.onDown(x, y)) break; }
    void screenUp(float x, float y) override { for (auto& b : buttons_) b.onUp(x, y); }

private:
    std::vector<LabelButton> buttons_;
};

} // namespace

std::unique_ptr<Scene> makeCreatorScene() { return std::make_unique<CreatorScene>(); }
std::unique_ptr<Scene> makeMyLevelsScene() { return std::make_unique<MyLevelsScene>(); }
std::unique_ptr<Scene> makeEditLevelScene(bool justCreated) { return std::make_unique<EditLevelScene>(justCreated); }
std::unique_ptr<Scene> makeSearchScene() { return std::make_unique<SearchScene>(); }
std::unique_ptr<Scene> makeOnlineLevelsScene(OnlineKind kind) { return std::make_unique<OnlineLevelsScene>(kind); }

} // namespace ogd
