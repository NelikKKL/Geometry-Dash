#pragma once
#include <functional>
#include <memory>
#include <string>

#include "engine/Engine.h"

namespace ogd {

class Scene;
using SceneFactory = std::function<std::unique_ptr<Scene>()>;

class Scene {
public:
    virtual ~Scene() = default;
    virtual void update(float dt) { (void)dt; }
    virtual void draw() {}
    virtual void onDown(float x, float y) { (void)x; (void)y; }
    virtual void onUp(float x, float y) { (void)x; (void)y; }
    virtual void onKey(SDL_Keycode k, bool down) { (void)k; (void)down; }
    virtual void onText(const std::string& utf8) { (void)utf8; }   // while SDL text input is active
};

// Owns the running scene, handles fade transitions and the frame loop.
class App {
public:
    void start(SceneFactory first);
    void frame();                       // one iteration: events, update, draw
    void goTo(SceneFactory next);       // fade out -> swap -> fade in
    bool running = true;

private:
    void handleEvents();
    std::unique_ptr<Scene> scene_;
    SceneFactory pending_;
    float fade_ = 0.f;                  // 0..1 black overlay
    enum { Idle, Out, In } phase_ = Idle;
    Uint64 last_ = 0;
};

App& app();

} // namespace ogd
