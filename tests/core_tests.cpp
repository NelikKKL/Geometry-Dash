// Dependency-free unit tests for src/core.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/Atlas.h"
#include "core/BMFont.h"
#include "core/PlayerPhysics.h"
#include "core/Plist.h"
#include "core/Save.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static void testPlist() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>a</key><integer>42</integer>
  <key>b</key><string>x &amp; y</string>
  <key>c</key><true/>
  <key>d</key><array><real>1.5</real><string>s</string></array>
  <key>e</key><dict/>
</dict></plist>)";
    auto v = ogd::parsePlist(xml);
    CHECK(v.isDict());
    CHECK(v.get("a") && v.get("a")->num() == 42);
    CHECK(v.get("b") && v.get("b")->str() == "x & y");
    CHECK(v.get("c") && v.get("c")->boolean());
    CHECK(v.get("d") && v.get("d")->arr.size() == 2 && v.get("d")->arr[0].num() == 1.5);
    CHECK(v.get("e") && v.get("e")->isDict());
    CHECK(ogd::parsePlist("garbage").type == ogd::PValue::None);
    auto n = ogd::parseNumbers("{{10,20},{-3.5,4}}");
    CHECK(n.size() == 4 && n[0] == 10 && n[2] == -3.5);
}

static void testAtlas() {
    // format 3 (cocos2d-x 3+/TexturePacker) and format 2
    const char* f3 = R"(<plist version="1.0"><dict><key>frames</key><dict>
<key>a.png</key><dict>
 <key>spriteOffset</key><string>{1,-2}</string>
 <key>spriteSize</key><string>{10,20}</string>
 <key>spriteSourceSize</key><string>{12,24}</string>
 <key>textureRect</key><string>{{100,200},{10,20}}</string>
 <key>textureRotated</key><true/>
</dict></dict>
<key>metadata</key><dict><key>format</key><integer>3</integer><key>textureFileName</key><string>S-hd.png</string></dict>
</dict></plist>)";
    auto s = ogd::parseSheet(f3);
    CHECK(s.ok && s.textureFile == "S-hd.png" && s.frames.size() == 1);
    CHECK(s.frames[0].x == 100 && s.frames[0].y == 200 && s.frames[0].w == 10 && s.frames[0].h == 20);
    CHECK(s.frames[0].rotated && s.frames[0].offX == 1 && s.frames[0].offY == -2);
    CHECK(s.frames[0].srcW == 12 && s.frames[0].srcH == 24);

    const char* f2 = R"(<plist version="1.0"><dict><key>frames</key><dict>
<key>b.png</key><dict>
 <key>frame</key><string>{{1,2},{3,4}}</string>
 <key>offset</key><string>{0,0}</string>
 <key>rotated</key><false/>
 <key>sourceSize</key><string>{3,4}</string>
</dict></dict>
<key>metadata</key><dict><key>format</key><integer>2</integer><key>textureFileName</key><string>T.png</string></dict>
</dict></plist>)";
    auto s2 = ogd::parseSheet(f2);
    CHECK(s2.ok && !s2.frames[0].rotated && s2.frames[0].w == 3 && s2.frames[0].h == 4);
    CHECK(!ogd::parseSheet("<plist><dict><key>x</key><integer>1</integer></dict></plist>").ok); // not a sheet
}

static void testFont() {
    const char* fnt =
        "info face=\"big\" size=59\n"
        "common lineHeight=64 base=50 scaleW=512 scaleH=512 pages=1\n"
        "page id=0 file=\"bigFont-hd.png\"\n"
        "chars count=2\n"
        "char id=65 x=0 y=0 width=30 height=40 xoffset=1 yoffset=2 xadvance=32 page=0 chnl=15\n"
        "char id=86 x=30 y=0 width=30 height=40 xoffset=0 yoffset=2 xadvance=30 page=0 chnl=15\n"
        "kernings count=1\n"
        "kerning first=65 second=86 amount=-4\n";
    auto f = ogd::parseBMFont(fnt);
    CHECK(f.ok && f.pageFile == "bigFont-hd.png" && f.lineHeight == 64);
    CHECK(f.chars.at(65).xadv == 32);
    CHECK(f.measure("AV") == 32 + 30 - 4);
    CHECK(f.measure("A?") == 32); // unknown glyph ignored
    CHECK(!ogd::parseBMFont("nothing").ok);
}

static void testPhysics() {
    using namespace ogd;
    PlayerState p;
    // idle on ground: stays put vertically, moves right at ~623 px/s
    for (int i = 0; i < 60; ++i) stepPlayer(p, 1.0);
    CHECK(p.y == kGroundY && p.onGround);
    CHECK(std::fabs(p.x - 623.0) < 1.0);

    // jump: leaves ground, peaks, lands again
    PlayerState q;
    q.holding = true;
    stepPlayer(q, 1.0);
    q.holding = false;
    CHECK(!q.onGround && q.y > kGroundY);
    double peak = q.y;
    int frames = 0;
    while (!q.onGround && frames < 200) { stepPlayer(q, 1.0); peak = std::fmax(peak, q.y); ++frames; }
    CHECK(q.onGround && q.y == kGroundY);
    CHECK(peak > kGroundY + 100 && peak < kGroundY + 300);   // GD cube jump is ~ 2 blocks high (60px each in 2x)
    CHECK(frames > 20 && frames < 60);
    CHECK(std::fmod(q.rotation, 90.0) == 0.0);               // snaps to a multiple of 90 on landing

    // holding keeps bouncing
    PlayerState r; r.holding = true; int jumps = 0; double prevV = 0;
    for (int i = 0; i < 600; ++i) { stepPlayer(r, 1.0); if (r.yVel > 10.0 && prevV <= 10.0) ++jumps; prevV = r.yVel; }
    CHECK(jumps >= 5);

    // frame-rate independence (roughly): 30fps vs 60fps peak within 10%
    auto peakAt = [](double dt) {
        PlayerState s; s.holding = true; stepPlayer(s, dt); s.holding = false;
        double pk = s.y; for (int i = 0; i < 400 && !s.onGround; ++i) { stepPlayer(s, dt); pk = std::fmax(pk, s.y); }
        return pk;
    };
    CHECK(std::fabs(peakAt(1.0) - peakAt(2.0)) < 0.1 * (peakAt(1.0) - kGroundY));

    // dead/locked players don't move
    PlayerState d; d.dead = true; stepPlayer(d, 1.0); CHECK(d.x == 0);
}

static void testSave() {
    std::string path = "ogd_test_save.json";
    ogd::SaveData a; a.cube = 7; a.mainColor = 5; a.username = "Ты";
    CHECK(a.save(path));
    ogd::SaveData b;
    CHECK(b.load(path) && b.cube == 7 && b.mainColor == 5 && b.username == "Ты" && b.secondaryColor == 3);
    std::remove(path.c_str());
    ogd::SaveData c;
    CHECK(!c.load("does_not_exist.json") && c.cube == 1);
    // out-of-range cube is clamped; corrupt file keeps defaults
    FILE* f = std::fopen(path.c_str(), "w"); std::fputs("{\"player-cube\": 99}", f); std::fclose(f);
    ogd::SaveData d; CHECK(d.load(path) && d.cube == 13);
    f = std::fopen(path.c_str(), "w"); std::fputs("{not json", f); std::fclose(f);
    ogd::SaveData e; CHECK(!e.load(path) && e.cube == 1);
    std::remove(path.c_str());
}

int main() {
    testPlist(); testAtlas(); testFont(); testPhysics(); testSave();
    if (failures) { std::printf("%d check(s) failed\n", failures); return 1; }
    std::printf("all core tests passed\n");
    return 0;
}
