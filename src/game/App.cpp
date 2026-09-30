#include "game/App.h"

#include <algorithm>

namespace ogd {

App& app() {
    static App a;
    return a;
}

void App::start(SceneFactory first) {
    scene_ = first();
    last_ = SDL_GetPerformanceCounter();
}

void App::goTo(SceneFactory next) {
    if (phase_ != Idle) return;
    pending_ = std::move(next);
    phase_ = Out;
}

void App::handleEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT: running = false; break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            if (e.button.button != SDL_BUTTON_LEFT || phase_ != Idle || !scene_) break;
            float lx = 0, ly = 0;
            SDL_RenderWindowToLogical(E().ren, e.button.x, e.button.y, &lx, &ly);
            float gy = kH - ly;  // to y-up
            if (e.type == SDL_MOUSEBUTTONDOWN) scene_->onDown(lx, gy);
            else scene_->onUp(lx, gy);
            break;
        }
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (e.key.repeat || !scene_) break;
            scene_->onKey(e.key.keysym.sym, e.type == SDL_KEYDOWN);
            break;
        default: break;
        }
    }
}

void App::frame() {
    Uint64 now = SDL_GetPerformanceCounter();
    float dt = (float)((double)(now - last_) / (double)SDL_GetPerformanceFrequency());
    last_ = now;
    dt = std::min(dt, 0.05f);

    handleEvents();

    if (scene_) {
        scene_->update(dt);
        scene_->draw();
    }

    // fade transition
    const float half = 0.25f;
    if (phase_ == Out) {
        fade_ += dt / half;
        if (fade_ >= 1.f) {
            fade_ = 1.f;
            scene_ = pending_();
            pending_ = nullptr;
            phase_ = In;
        }
    } else if (phase_ == In) {
        fade_ -= dt / half;
        if (fade_ <= 0.f) { fade_ = 0.f; phase_ = Idle; }
    }
    if (fade_ > 0.f) E().fillRect(0, 0, kW, kH, {0, 0, 0}, (Uint8)(fade_ * 255));

    E().present();
}

} // namespace ogd
