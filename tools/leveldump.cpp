// Inspect a level file: leveldump <file> [--objects]
// Accepts plain level text or base64(gzip|zlib) level strings.
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

#include "core/Level.h"

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: leveldump <file> [--objects]\n"); return 2; }
    std::ifstream f(argv[1], std::ios::binary);
    if (!f) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    std::stringstream ss; ss << f.rdbuf();
    const bool dumpObjects = argc > 2 && std::string(argv[2]) == "--objects";

    ogd::LevelParseResult r = ogd::parseLevelString(ss.str());
    if (!r.ok) { std::fprintf(stderr, "parse failed: %s\n", r.error.c_str()); return 1; }
    const auto& L = r.level;

    if (dumpObjects) {
        for (const auto& o : L.objects)
            std::printf("%d %g %g %g %d %d\n", o.id, o.x, o.y, o.rotation, (int)o.flipX, (int)o.flipY);
        return 0;
    }
    std::printf("objects:   %zu (skipped %zu, warnings %zu)\n", L.objects.size(), r.skippedObjects, r.warningCount);
    std::printf("length:    %g units  (%.1f s at 311.58 u/s)\n", L.maxX(), L.maxX() / 311.58);
    const auto& s = L.settings;
    std::printf("music:     %d\n", s.musicTrack);
    if (s.hasBackground) std::printf("background: %d,%d,%d\n", s.background.r, s.background.g, s.background.b);
    if (s.hasGround) std::printf("ground:     %d,%d,%d\n", s.ground.r, s.ground.g, s.ground.b);
    for (const auto& kv : s.raw) std::printf("  header %s = %s\n", kv.first.c_str(), kv.second.c_str());
    std::map<int, int> hist;
    for (const auto& o : L.objects) ++hist[o.id];
    std::printf("object ids (%zu distinct):", hist.size());
    for (const auto& kv : hist) std::printf(" %d x%d", kv.first, kv.second);
    std::printf("\n");
    for (const auto& w : r.warnings)
        std::printf("warning: object %zu: %s\n", w.object, w.message.c_str());
    return 0;
}
