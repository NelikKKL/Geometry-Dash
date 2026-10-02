// Runs the real game scenes natively on the fake SDL and dumps frames.
//   render_harness <resources_dir> "<script>"
// script commands (separated by ';'):  run <seconds> | click <x> <y> | down <x> <y> | up <x> <y>
//                                       key <space|esc|left|right|enter> <down|up> | startx <units> | shot <file.ppm>
// Screen coordinates: origin top-left, 1280x720.
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

#include "game/App.h"
#include "game/Scenes.h"

using namespace ogd;

static void frames(double seconds) {
    const int n = (int)(seconds * 60 + 0.5);
    for (int i = 0; i < n; ++i) { fake_advance(1.0 / 60.0); app().frame(); }
}
static void mouse(bool down, int x, int y) {
    SDL_Event e{}; e.button.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP; e.button.button = SDL_BUTTON_LEFT; e.button.x = x; e.button.y = y;
    fake_push_event(e);
}
static SDL_Keycode keyOf(const std::string& n) {
    if (n == "space") return SDLK_SPACE;
    if (n == "esc") return SDLK_ESCAPE;
    if (n == "left") return SDLK_LEFT;
    if (n == "right") return SDLK_RIGHT;
    if (n == "enter") return SDLK_RETURN;
    E().shutdown();
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: render_harness <resources> \"<script>\"\n"); return 2; }
    if (!E().init(argv[1], "/tmp/render_harness_save.json")) return 1;
    if (!E().hasAsset("GJ_LaunchSheet.plist") && !E().hasAsset("GJ_LaunchSheet-hd.plist")) { std::fprintf(stderr, "no assets\n"); return 1; }
    app().start([] { return makeLoadingScene(); });
    std::stringstream script(argv[2]);
    std::string cmdline;
    while (std::getline(script, cmdline, ';')) {
        std::stringstream c(cmdline); std::string cmd; c >> cmd;
        if (cmd == "run") { double s; c >> s; frames(s); }
        else if (cmd == "click") { int x, y; c >> x >> y; mouse(true, x, y); frames(0.1); mouse(false, x, y); frames(0.1); }
        else if (cmd == "down") { int x, y; c >> x >> y; mouse(true, x, y); frames(1.0 / 60); }
        else if (cmd == "up") { int x, y; c >> x >> y; mouse(false, x, y); frames(1.0 / 60); }
        else if (cmd == "key") { std::string k, d; c >> k >> d; SDL_Event e{}; e.key.type = d == "down" ? SDL_KEYDOWN : SDL_KEYUP; e.key.keysym.sym = keyOf(k); fake_push_event(e); frames(1.0 / 60); }
        else if (cmd == "ship") { g_debugStartShip = true; }
        else if (cmd == "startx") { double x; c >> x; g_debugStartX = x; }
        else if (cmd == "shot") { std::string f; c >> f; if (!fake_dump_ppm(fake_renderer(), f.c_str())) std::fprintf(stderr, "cannot write %s\n", f.c_str()); }
    }
    E().shutdown();
    return 0;
}
