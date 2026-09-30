#include <cstdio>
#include <string>

#include "game/App.h"
#include "game/Scenes.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

using namespace ogd;

static void tick() { app().frame(); }

int main(int argc, char** argv) {
#ifdef __EMSCRIPTEN__
    const std::string res = "/Resources", save = "/save/save.json";
#else
    const std::string res = argc > 1 ? argv[1] : "Resources";
    const std::string save = argc > 2 ? argv[2] : "save.json";
#endif
    (void)argc; (void)argv;

    if (!E().init(res, save)) return 1;

    if (!E().hasAsset("GJ_LaunchSheet.plist") && !E().hasAsset("GJ_LaunchSheet-hd.plist")) {
        std::fprintf(stderr,
                     "OpenGD: no game assets found in '%s'.\n"
                     "Copy the files from the assets folder of the Geometry Dash 1.x APK there "
                     "(see README, \"Assets\").\n", res.c_str());
        E().shutdown();
        return 2;
    }

    app().start([] { return makeLoadingScene(); });

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(tick, 0, 1);
#else
    while (app().running) tick();
#endif
    E().persist();
    E().shutdown();
    return 0;
}
