// Gameplay scene: renders a parsed level with real sprites and runs core/Sim.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <vector>

#include "core/Level.h"
#include "core/ObjectTable.h"
#include "core/Sim.h"
#include "game/Levels.h"
#include "game/PauseLayer.h"
#include "game/Scenes.h"
#include "game/Ui.h"
#include "game/World.h"

namespace ogd {

double g_debugStartX = 0.0;
bool g_debugStartShip = false;

namespace {

constexpr float kPx = 2.f;                // GD units -> design px
constexpr float kFloorPx = 206.f;         // screen Y of the floor line (camera at rest)
constexpr float kPlayerScreenX = 380.f;
constexpr float kPlanePx = kFloorPx + 300.f * kPx;

Color lerpColor(Color a, Color b, float t) {
    t = std::max(0.f, std::min(1.f, t));
    auto m = [&](Uint8 x, Uint8 y) { return (Uint8)(x + (y - x) * t); };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b)};
}

struct ColorFade {
    Color from, to, now;
    float t = 1.f, dur = 0.f;
    void set(Color c) { from = to = now = c; t = 1.f; }
    void start(Color target, float seconds) {
        from = now;
        to = target;
        dur = seconds;
        t = seconds > 0.f ? 0.f : 1.f;
        if (t >= 1.f) now = to;
    }
    void update(float dt) {
        if (t >= 1.f) return;
        t = std::min(1.f, t + dt / dur);
        now = lerpColor(from, to, t);
    }
};

struct Particle {
    float x, y, vx, vy, life, maxLife, size;
    Color c;
};

class PlayScene : public Scene {
public:
    explicit PlayScene(int index)
        : index_(index),
          back_("GJ_arrow_01_001.png", 50.f, kH - 50.f, 1.f, [this] { leave(); }),
          pauseBtn_("GJ_pauseBtn_001.png", kW - 50.f, kH - 50.f, 1.f, [this] { openPause(); }),
          checkBtn_("GJ_checkpointBtn_001.png", kW / 2.f - 80.f, 70.f, 0.9f, [this] { addCheckpoint(); }),
          removeBtn_("GJ_removeCheckBtn_001.png", kW / 2.f + 80.f, 70.f, 0.9f, [this] { removeCheckpoint(); }),
          replay_("GJ_replayBtn_001.png", 540.f, 300.f, 0.9f, [this] { restart(); }),
          menu_("GJ_menuBtn_001.png", 740.f, 300.f, 0.9f, [this] { leave(); }) {
        std::string txt;
        if (E().readText(levelFile(index_), txt)) {
            LevelParseResult r = parseLevelString(txt);
            if (r.ok) {
                if (r.warningCount) SDL_Log("level %d: %zu parse warnings", index_, r.warningCount);
                level_ = std::move(r.level);
                SimConfig cfg;
                if (g_debugStartX != 0.0) cfg.startX = g_debugStartX;
                cfg.startAsShip = g_debugStartShip;
                sim_ = std::make_unique<Simulation>(level_, cfg);
            } else {
                SDL_Log("level %d: %s", index_, r.error.c_str());
            }
        }
        main_ = paletteColor(E().save.mainColor);
        sec_ = paletteColor(E().save.secondaryColor);
        if (sim_) {
            const auto& st = level_.settings;
            baseBg_ = st.hasBackground ? Color{st.background.r, st.background.g, st.background.b} : kBlue;
            baseGround_ = st.hasGround ? Color{st.ground.r, st.ground.g, st.ground.b} : kBlue;
            for (size_t i = 0; i < level_.objects.size(); ++i) {
                const int id = level_.objects[i].id;
                if (id == kObjColorTriggerBG || id == kObjColorTriggerGround) triggers_.push_back(i);
            }
            resetRun();
            startMusic();
            countAttempt();
        }
    }

    void update(float dt) override {
        if (pendingClose_) {                              // pause menu was dismissed by one of its callbacks / Esc
            pendingClose_ = false;
            pause_.reset();
            E().resumeMusic();
        }
        back_.update(dt);
        if (!sim_) return;
        if (pause_) { pause_->update(dt); return; }   // the world is frozen while paused
        pauseBtn_.update(dt);
        if (practice_) { checkBtn_.update(dt); removeBtn_.update(dt); }
        updateParticles(dt);

        switch (state_) {
        case State::Playing: {
            sim_->step(dt * 60.0);
            countJumps();
            applyTriggers();
            bg_.update(dt);
            ground_.update(dt);
            updateCamera(dt);
            if (sim_->player().dead) onDeath();
            else if (sim_->player().finished) onComplete();
            break;
        }
        case State::Dead:
            bg_.update(dt);
            ground_.update(dt);
            timer_ -= dt;
            if (timer_ <= 0.f) restart();
            break;
        case State::Complete:
            replay_.update(dt);
            menu_.update(dt);
            break;
        }
    }

    void draw() override {
        if (!sim_) {
            E().clear(kBlue);
            E().drawText(E().font("bigFont.fnt"), "Level data not found", kW / 2.f, kH / 2.f + 30, 0.9f);
            E().drawText(E().font("chatFont.fnt"), "Pack your APK with tools/akrile-tool.mjs (it extracts the levels)", kW / 2.f, kH / 2.f - 30, 0.9f);
            back_.draw();
            return;
        }
        drawBackground(camX_ * 0.1f, bg_.now, camY_);
        drawGround(camX_, ground_.now, camY_);
        const auto& p = sim_->player();
        if (p.mode == PlayMode::Ship || p.mirrored || camY_ > 40.f) drawCeiling(camX_, ground_.now, camY_, kPlanePx);

        collectVisible();
        drawObjects(true);
        if (state_ != State::Dead) drawPlayer();
        drawObjects(false);

        // "Attempt N" lives in the world near the start
        E().drawText(E().font("bigFont.fnt"), "Attempt " + std::to_string(attempts_), 940.f - camX_, 470.f - camY_, 1.0f);

        for (const Particle& q : particles_) {
            const float a = std::max(0.f, q.life / q.maxLife);
            E().fillRect(q.x - camX_ - q.size / 2, q.y - camY_ - q.size / 2, q.size, q.size, q.c, (Uint8)(255 * a));
        }

        if (!pause_) drawHud();
        if (state_ == State::Playing && !pause_) {
            pauseBtn_.draw();
            if (practice_) { checkBtn_.draw(); removeBtn_.draw(); }
        }

        if (pause_) pause_->draw();
        if (state_ == State::Complete) {
            E().fillRect(0, 0, kW, kH, {0, 0, 0}, 110);
            E().drawSprite(E().sprite("GJ_levelComplete_001.png"), kW / 2.f, 540.f);
            E().drawText(E().font("bigFont.fnt"), "Attempts: " + std::to_string(attempts_), kW / 2.f, 430.f, 0.8f);
            replay_.draw();
            menu_.draw();
        }
    }

    void onDown(float x, float y) override {
        if (pause_) { pause_->onDown(x, y); return; }
        if (!sim_) { back_.onDown(x, y); return; }
        if (state_ == State::Playing) {
            if (pauseBtn_.onDown(x, y)) return;
            if (practice_ && (checkBtn_.onDown(x, y) || removeBtn_.onDown(x, y))) return;
        }
        if (state_ == State::Complete) {
            if (replay_.onDown(x, y)) return;
            if (menu_.onDown(x, y)) return;
        }
        press(true);
    }
    void onUp(float x, float y) override {
        if (pause_) { pause_->onUp(x, y); return; }
        back_.onUp(x, y);
        pauseBtn_.onUp(x, y);
        if (practice_) { checkBtn_.onUp(x, y); removeBtn_.onUp(x, y); }
        if (state_ == State::Complete) { replay_.onUp(x, y); menu_.onUp(x, y); }
        press(false);
    }
    void onKey(SDL_Keycode k, bool down) override {
        if (pause_) {                                   // Esc closes the pause menu (PauseLayer::keyBackClicked)
            if (k == SDLK_ESCAPE && down) closePause();
            return;
        }
        if (k == SDLK_SPACE || k == SDLK_UP || k == SDLK_w) press(down);
        else if (k == SDLK_ESCAPE && down) {
            if (sim_ && state_ == State::Playing) openPause();
            else if (!sim_ || state_ == State::Complete) leave();   // during the death animation Esc is ignored
        }
        else if (practice_ && state_ == State::Playing && down) {
            if (k == SDLK_z) addCheckpoint();
            else if (k == SDLK_x) removeCheckpoint();
        }
    }

private:
    enum class State { Playing, Dead, Complete };

    void press(bool down) {
        held_ = down;
        if (sim_ && state_ == State::Playing) sim_->setHolding(down);
    }

    void startMusic() {
        if (E().audioOk) E().playMusic(levelMeta(index_).track, 0);
    }

    void resetRun() {
        sim_->reset();
        state_ = State::Playing;
        bg_.set(baseBg_);
        ground_.set(baseGround_);
        nextTrigger_ = 0;
        particles_.clear();
        camY_ = 0.f;
        camX_ = -kPlayerScreenX;
        lastJumps_ = 0;
        if (held_) sim_->setHolding(true);
    }

    void restart() {
        if (practice_ && !checkpoints_.empty()) {      // practice: respawn at the last checkpoint, no new attempt
            restoreCheckpoint(checkpoints_.back());
            startMusic();
            return;
        }
        ++attempts_;
        resetRun();
        startMusic();
        countAttempt();
    }

    // ---- practice mode -------------------------------------------------------
    struct Checkpoint {
        Simulation sim;
        ColorFade bg, ground;
        size_t nextTrigger;
        float camX, camY;
    };

    void addCheckpoint() {
        if (!sim_ || state_ != State::Playing || sim_->player().dead) return;
        checkpoints_.push_back({*sim_, bg_, ground_, nextTrigger_, camX_, camY_});
    }
    void removeCheckpoint() {
        if (!checkpoints_.empty()) checkpoints_.pop_back();
    }
    void restoreCheckpoint(const Checkpoint& c) {
        *sim_ = c.sim;
        bg_ = c.bg; ground_ = c.ground;
        nextTrigger_ = c.nextTrigger;
        camX_ = c.camX; camY_ = c.camY;
        particles_.clear();
        state_ = State::Playing;
        lastJumps_ = sim_->jumpCount();
        sim_->setHolding(held_);
    }

    // ---- pause -----------------------------------------------------------------
    void openPause() {
        if (!sim_ || state_ != State::Playing || pause_) return;
        held_ = false;
        sim_->setHolding(false);
        E().pauseMusic();
        PauseCallbacks cb;
        cb.onResume = [this] { closePause(); };
        cb.onRestart = [this] { closePause(); if (practice_) clearCheckpoints(); restartFresh(); };
        cb.onQuit = [this] { closePause(); leave(); };
        cb.onPractice = [this] {                        // PauseLayer::onPracticeMode: switch on, then resume
            practice_ = true;
            clearCheckpoints();
            addCheckpoint();                            // so a death right away does not throw the run back to 0%
            closePause();
        };
        cb.onNormal = [this] {                          // PauseLayer::onNormalMode
            practice_ = false;
            clearCheckpoints();
            closePause();
            restartFresh();
        };
        pause_ = std::make_unique<PauseLayer>(levelMeta(index_).name, E().save.bestOf(index_),
                                              E().save.practiceBestOf(index_), practice_, std::move(cb));
    }
    void closePause() {
        if (!pause_) return;
        pendingClose_ = true;                           // freed after the callback returns (we're inside its button)
    }
    void clearCheckpoints() { checkpoints_.clear(); }
    void restartFresh() {
        ++attempts_;
        resetRun();
        startMusic();
        countAttempt();
    }

    // Stats board counters (not counted for debug starts)
    void countAttempt() { if (g_debugStartX == 0.0) ++E().save.totalAttempts; }
    void countJumps() {
        const long long j = sim_->jumpCount();
        if (g_debugStartX == 0.0 && j > lastJumps_) E().save.totalJumps += j - lastJumps_;
        lastJumps_ = j;
    }

    void leave() {
        if (leaving_) return;
        leaving_ = true;
        E().stopMusic();
        E().persist();
        const int page = index_;
        app().goTo([page] { return makeLevelSelectScene(page); });
    }

    void applyTriggers() {
        const double x = sim_->player().x;
        while (nextTrigger_ < triggers_.size() && level_.objects[triggers_[nextTrigger_]].x <= x) {
            const LevelObject& o = level_.objects[triggers_[nextTrigger_++]];
            auto byte = [&](int key) { return (Uint8)std::max(0.0, std::min(255.0, o.number(key, 0))); };
            const Color c{byte(7), byte(8), byte(9)};
            const float dur = (float)o.number(10, 0);
            (o.id == kObjColorTriggerBG ? bg_ : ground_).start(c, dur);
        }
    }

    void updateCamera(float dt) {
        const auto& p = sim_->player();
        camX_ = (float)p.x * kPx - kPlayerScreenX;
        // cube: follow only when the player gets high; ship / gravity sections: show floor and ceiling together
        float target;
        if (p.mode == PlayMode::Ship) target = 120.f;
        else target = std::max(0.f, kFloorPx + (float)p.y * kPx - 470.f);
        if (p.mirrored) target = std::max(target, 120.f);
        target = std::min(target, 180.f);
        camY_ += (target - camY_) * std::min(1.f, dt * 5.f);
    }

    void onDeath() {
        state_ = State::Dead;
        timer_ = 1.0f;
        const auto& p = sim_->player();
        E().stopMusic();
        E().playSfx("explode_11.ogg");
        const int pct = (int)(sim_->progress() * 100.0);
        if (g_debugStartX == 0.0) {
            if (practice_) E().save.recordPracticeBest(index_, pct);
            else E().save.recordBest(index_, pct);
            E().persist();
        }
        // burst of squares
        const float cx = (float)p.x * kPx, cy = kFloorPx + (float)p.y * kPx;
        for (int i = 0; i < 30; ++i) {
            const float ang = (float)(rand() % 3600) / 10.f * 3.14159265f / 180.f;
            const float sp = 150.f + (float)(rand() % 550);
            Particle q;
            q.x = cx; q.y = cy;
            q.vx = std::cos(ang) * sp; q.vy = std::sin(ang) * sp;
            q.maxLife = q.life = 0.5f + (float)(rand() % 50) / 100.f;
            q.size = 8.f + (float)(rand() % 14);
            q.c = (i & 1) ? main_ : sec_;
            particles_.push_back(q);
        }
    }

    void onComplete() {
        state_ = State::Complete;
        E().stopMusic();
        E().playSfx("endStart_02.ogg");
        if (g_debugStartX == 0.0) {
            if (practice_) E().save.recordPracticeBest(index_, 100);
            else E().save.recordBest(index_, 100);
            E().persist();
        }
    }

    void updateParticles(float dt) {
        for (Particle& q : particles_) {
            q.life -= dt;
            q.vy -= 1500.f * dt;
            q.x += q.vx * dt;
            q.y += q.vy * dt;
        }
        particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle& q) { return q.life <= 0.f; }), particles_.end());
    }

    // objects near the camera, sorted by draw layer
    void collectVisible() {
        visible_.clear();
        auto r = level_.range((camX_ - 220.f) / kPx, (camX_ + kW + 220.f) / kPx);
        for (size_t i = r.first; i < r.second; ++i) {
            const ObjInfo* info = objectInfo(level_.objects[i].id);
            if (info && info->sprite) visible_.push_back(i);
        }
        std::stable_sort(visible_.begin(), visible_.end(), [&](size_t a, size_t b) {
            return objectInfo(level_.objects[a].id)->z < objectInfo(level_.objects[b].id)->z;
        });
    }

    void drawObject(const LevelObject& o, const char* spriteName) {
        const float sx = o.x * kPx - camX_;
        const float sy = kFloorPx + o.y * kPx - camY_;
        E().drawSprite(E().sprite(spriteName), sx, sy, 1.f, 1.f, o.rotation, {}, 255, o.flipX, o.flipY);
    }

    // behind = everything that goes under the player; otherwise portal fronts, pads and orbs
    void drawObjects(bool behind) {
        if (behind) {
            for (size_t i : visible_) {                       // portal backs first
                const LevelObject& o = level_.objects[i];
                const ObjInfo* info = objectInfo(o.id);
                if (info->spriteBack) drawObject(o, info->spriteBack);
            }
        }
        for (size_t i : visible_) {
            const LevelObject& o = level_.objects[i];
            const ObjInfo* info = objectInfo(o.id);
            if ((info->z < kObjZPlayer) == behind) drawObject(o, info->sprite);
        }
    }

    void drawPlayer() {
        const auto& p = sim_->player();
        const float x = (float)p.x * kPx - camX_;
        const float y = kFloorPx + (float)p.y * kPx - camY_;
        const float flip = p.mirrored ? -1.f : 1.f;
        char a[48], b[48];
        const int cube = std::max(1, std::min(13, E().save.cube));
        std::snprintf(a, sizeof a, "player_%02d_001.png", cube);
        std::snprintf(b, sizeof b, "player_%02d_2_001.png", cube);
        if (p.mode == PlayMode::Cube) {
            E().drawSprite(E().sprite(b), x, y, 1.f, flip, (float)p.rotation, sec_);
            E().drawSprite(E().sprite(a), x, y, 1.f, flip, (float)p.rotation, main_);
        } else {
            const float rad = (float)p.rotation * 3.14159265f / 180.f;
            const float off = 8.f * flip;                     // icon sits on top of the ship, rotated with it
            const float lift = 16.f * flip;                   // the ship centre is 3 units above the floor: raise the artwork
            const float ix = x + off * std::sin(rad), iy = y + lift + off * std::cos(rad);
            E().drawSprite(E().sprite(b), ix, iy, 0.6f, 0.6f * flip, (float)p.rotation, sec_);
            E().drawSprite(E().sprite(a), ix, iy, 0.6f, 0.6f * flip, (float)p.rotation, main_);
            E().drawSprite(E().sprite("ship_01_001.png"), x, y + lift, 1.f, flip, (float)p.rotation, main_);
        }
    }

    void drawHud() {
        Sprite groove = E().sprite("slidergroove.png");
        Sprite bar = E().sprite("sliderBar.png");
        const float cy = kH - 28.f;
        E().drawSprite(groove, kW / 2.f, cy);
        const float frac = (float)sim_->progress();
        E().drawSpriteTiledX(bar, kW / 2.f - groove.w / 2 + 4, cy, (groove.w - 8) * frac);
        const int pct = (int)(frac * 100.f);
        E().drawText(E().font("bigFont.fnt"), std::to_string(pct) + "%", kW / 2.f + groove.w / 2 + 55.f, cy, 0.5f);
    }

    int index_;
    Level level_;
    std::unique_ptr<Simulation> sim_;
    Button back_, pauseBtn_, checkBtn_, removeBtn_, replay_, menu_;
    std::unique_ptr<PauseLayer> pause_;
    bool pendingClose_ = false, practice_ = false;
    std::vector<Checkpoint> checkpoints_;
    State state_ = State::Playing;
    std::vector<size_t> triggers_, visible_;
    size_t nextTrigger_ = 0;
    ColorFade bg_, ground_;
    Color baseBg_ = kBlue, baseGround_ = kBlue, main_, sec_;
    std::vector<Particle> particles_;
    float camX_ = -kPlayerScreenX, camY_ = 0.f, timer_ = 0.f;
    int attempts_ = 1;
    long long lastJumps_ = 0;
    bool held_ = false, leaving_ = false;
};

} // namespace

std::unique_ptr<Scene> makePlayScene(int level) { return std::make_unique<PlayScene>(level); }

} // namespace ogd
