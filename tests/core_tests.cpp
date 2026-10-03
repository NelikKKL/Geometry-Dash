// Dependency-free unit tests for src/core.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <fstream>
#include <sstream>
#include <vector>

#include "core/Atlas.h"
#include "core/Inflate.h"
#include "core/Level.h"
#include "core/ObjectTable.h"
#include "core/Palette.h"
#include "core/Unlocks.h"
#include "core/BMFont.h"
#include "core/PlayerPhysics.h"
#include "core/Plist.h"
#include "core/Save.h"

#ifndef OGD_TEST_DATA
#define OGD_TEST_DATA "tests/data"
#endif
static std::string slurp(const std::string& name) {
    std::ifstream f(std::string(OGD_TEST_DATA) + "/" + name, std::ios::binary);
    std::stringstream ss; ss << f.rdbuf(); return ss.str();
}

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
    ogd::SaveData bs; CHECK(bs.recordBest(2, 40) && !bs.recordBest(2, 30) && bs.recordBest(2, 100) && bs.bestOf(2) == 100);
    CHECK(!bs.recordBest(-1, 5) && !bs.recordBest(99, 5) && bs.bestOf(99) == 0);
    bs.recordBest(0, 7); CHECK(bs.save(path));
    ogd::SaveData bl; CHECK(bl.load(path) && bl.bestOf(2) == 100 && bl.bestOf(0) == 7 && bl.bestOf(1) == 0);
    ogd::SaveData c;
    CHECK(!c.load("does_not_exist.json") && c.cube == 1);
    // out-of-range cube is clamped; corrupt file keeps defaults
    FILE* f = std::fopen(path.c_str(), "w"); std::fputs("{\"player-cube\": 99}", f); std::fclose(f);
    ogd::SaveData d; CHECK(d.load(path) && d.cube == 13);
    f = std::fopen(path.c_str(), "w"); std::fputs("{not json", f); std::fclose(f);
    ogd::SaveData e; CHECK(!e.load(path) && e.cube == 1);
    std::remove(path.c_str());
}


static void testInflate() {
    using namespace ogd;
    // base64: both alphabets, whitespace, missing padding
    std::vector<uint8_t> b;
    CHECK(base64Decode("aGVsbG8=", b) && std::string(b.begin(), b.end()) == "hello");
    CHECK(base64Decode("aGVsbG8", b) && std::string(b.begin(), b.end()) == "hello");
    CHECK(base64Decode("a G\nVsbG8=", b) && std::string(b.begin(), b.end()) == "hello");
    CHECK(base64Decode("-_-_", b) && b.size() == 3 && b[0] == 0xFB && b[1] == 0xFF && b[2] == 0xBF);
    CHECK(!base64Decode("ab$d", b));

    CHECK(crc32((const uint8_t*)"123456789", 9) == 0xCBF43926u);

    const std::string plain = slurp("level_plain.txt");
    CHECK(plain.size() > 5000);
    const char* files[] = {"level_gzip.b64", "level_zlib.b64", "level_stored.b64", "level_rawdeflate.b64"};
    for (const char* f : files) {                 // dynamic-Huffman, stored and raw streams
        std::string text, err;
        bool ok = decodeLevelString(slurp(f), text, &err);
        CHECK(ok);
        CHECK(text == plain);
        if (!ok || text != plain) std::printf("   (while decoding %s: %s)\n", f, err.c_str());
    }
    std::string t;                                 // fixed-Huffman block
    CHECK(decodeLevelString(slurp("short_zlib.b64"), t) && t == slurp("short_plain.txt"));

    // corruption must be rejected, never crash
    std::string gz = slurp("level_gzip.b64");
    std::vector<uint8_t> raw; base64Decode(gz, raw);
    std::vector<uint8_t> out;
    CHECK(gunzip(raw.data(), raw.size(), out) && out.size() == plain.size());
    auto bad = raw; bad[bad.size() - 6] ^= 0x01;                       // flip a CRC bit
    CHECK(!gunzip(bad.data(), bad.size(), out));
    bad = raw; bad[40] ^= 0xFF;                                         // damage the deflate stream
    CHECK(!gunzip(bad.data(), bad.size(), out));
    CHECK(!gunzip(raw.data(), raw.size() / 2, out));                    // truncated
    CHECK(!gunzip(raw.data(), 5, out));
    // decompression bomb guard
    CHECK(!gunzip(raw.data(), raw.size(), out, 1000));
    for (size_t cut = 0; cut < raw.size(); cut += 37) { std::vector<uint8_t> o2; gunzip(raw.data(), cut, o2); }  // no crash
    std::string e2;
    CHECK(!decodeLevelString("@@@@", t, &e2) && !e2.empty());
    CHECK(!decodeLevelString("   ", t));
}

static void testLevel() {
    using namespace ogd;
    auto r = parseLevel("kS1,40,kS2,62,kS3,255,kS4,0,kS5,19,kS6,200,kA1,2;"
                        "1,8,2,525,3,15;1,8,2,555,3,15,5,1;1,39,2,100,3,6,6,-90,4,1;"
                        "1,29,2,135,3,135,7,158,8,3,9,255,10,10.0;");
    CHECK(r.ok && r.warningCount == 0 && r.level.objects.size() == 4);
    CHECK(r.level.settings.hasBackground && r.level.settings.background.r == 40 && r.level.settings.background.b == 255);
    CHECK(r.level.settings.hasGround && r.level.settings.ground.g == 19 && r.level.settings.ground.b == 200);
    CHECK(r.level.settings.musicTrack == 2);
    CHECK(r.level.settings.intValue("kS6", -1) == 200 && r.level.settings.intValue("nope", -1) == -1);
    // sorted by x
    CHECK(r.level.objects[0].x == 100 && r.level.objects[1].x == 135 && r.level.objects[3].x == 555);
    const auto& o = r.level.objects[0];
    CHECK(o.id == 39 && o.y == 6 && o.rotation == -90 && o.flipX && !o.flipY);
    const auto& trig = r.level.objects[1];
    CHECK(trig.id == kObjColorTriggerBG && trig.number(7) == 158 && trig.number(10) == 10.0 && trig.has(9) && !trig.has(11));
    CHECK(trig.number(99, -5) == -5);
    CHECK(r.level.objects[3].flipY && !r.level.objects[3].flipX);
    CHECK(r.level.maxX() == 555);

    // no header (like the extra level in the game binary)
    auto nh = parseLevel("1,29,2,45,3,105,7,23,8,241,9,0,10,0.5;1,1,2,255,3,15");
    CHECK(nh.ok && nh.level.objects.size() == 2 && !nh.level.settings.hasBackground && nh.level.settings.musicTrack == -1);

    // stable sort keeps file order for equal x
    auto st = parseLevel("1,1,2,50,3,15;1,2,2,50,3,45;1,3,2,10,3,15;1,4,2,50,3,75;");
    CHECK(st.ok && st.level.objects[0].id == 3 && st.level.objects[1].id == 1 && st.level.objects[2].id == 2 && st.level.objects[3].id == 4);

    // range query
    auto rg = parseLevel("1,1,2,10,3,0;1,1,2,20,3,0;1,1,2,20,3,0;1,1,2,30,3,0;1,1,2,40,3,0;");
    auto q = rg.level.range(20, 30);
    CHECK(q.first == 1 && q.second == 4);
    q = rg.level.range(21, 29); CHECK(q.first == q.second);
    q = rg.level.range(-100, 1000); CHECK(q.first == 0 && q.second == 5);
    q = rg.level.range(50, 60); CHECK(q.first == q.second);
    q = rg.level.range(30, 10); CHECK(q.first == q.second);                // inverted range

    // leniency: bad objects are skipped, good ones kept, warnings reported
    auto bad = parseLevel("1,8,2,100,3,15;;1,8,2,abc,3,15;1,8,3,15;x,1,2,3;1,8,2,200,3,15,6;1,9,2,300,3,15");
    CHECK(bad.ok);
    CHECK(bad.level.objects.size() == 3);       // ids 8@100, 8@200 (odd trailing field ignored), 9@300
    CHECK(bad.skippedObjects == 3);   // bad x, missing x, and "x,1,2,3" (no id/y)
    CHECK(bad.warningCount >= 4);
    CHECK(!bad.warnings.empty() && bad.warnings[0].object != 0);
    // whitespace / trailing NUL / CRLF
    auto ws = parseLevel(std::string("\r\n kA1,1;1,8,2,5,3,15;\r\n") + '\0');
    CHECK(ws.ok && ws.level.objects.size() == 1 && ws.level.settings.musicTrack == 1);
    // hard failures
    CHECK(!parseLevel("").ok && !parseLevel("   \n").ok && !parseLevel(";;;").ok);
    CHECK(parseLevel("kA1,5").ok && parseLevel("kA1,5").level.objects.empty());   // header-only is valid
    // warnings are capped
    std::string many; for (int i = 0; i < 1000; ++i) many += "x;";
    many += "1,1,2,1,3,1";
    auto cap = parseLevel(many);
    CHECK(cap.ok && cap.warningCount >= 1000 && cap.warnings.size() == 100);

    // full pipeline: every encoding of the fixture parses to the same level
    auto ref = parseLevelString(slurp("level_plain.txt"));
    CHECK(ref.ok && ref.level.objects.size() == 401 && ref.warningCount == 0 && ref.level.settings.musicTrack == 3);
    for (const char* f : {"level_gzip.b64", "level_zlib.b64", "level_stored.b64", "level_rawdeflate.b64"}) {
        auto x = parseLevelString(slurp(f));
        bool same = x.ok && x.level.objects.size() == ref.level.objects.size();
        for (size_t i = 0; same && i < x.level.objects.size(); ++i)
            same = x.level.objects[i].id == ref.level.objects[i].id && x.level.objects[i].x == ref.level.objects[i].x &&
                   x.level.objects[i].y == ref.level.objects[i].y;
        CHECK(same);
    }
    CHECK(!parseLevelString("@@@").ok);
}

// Optional: real level files (extracted locally from your own APK; never committed).
// Run with OGD_LEVELS_DIR=/path/with/level_*.txt
static void testRealLevels() {
    const char* dir = std::getenv("OGD_LEVELS_DIR");
    if (!dir) { std::printf("   (skipping real-level test: OGD_LEVELS_DIR not set)\n"); return; }
    int files = 0; size_t objects = 0;
    for (int i = 0; i < 64; ++i) {
        char name[64];
        std::snprintf(name, sizeof name, "%s/level_%d.txt", dir, i);
        std::ifstream f(name, std::ios::binary);
        if (!f) continue;
        std::stringstream ss; ss << f.rdbuf();
        std::string txt = ss.str();
        auto r = ogd::parseLevelString(txt);
        size_t segs = 0; for (char c : txt) segs += (c == ';');
        CHECK(r.ok && r.warningCount == 0);
        CHECK(r.level.objects.size() >= segs - 1 && r.level.objects.size() <= segs + 1);   // header may or may not exist
        CHECK(r.level.maxX() > 1000);
        for (const auto& o : r.level.objects) if (!ogd::objectInfo(o.id)) { std::printf("   unknown object id %d in %s\n", o.id, name); CHECK(false); break; }
        ++files; objects += r.level.objects.size();
    }
    std::printf("   real levels: %d files, %zu objects\n", files, objects);
    CHECK(files > 0);
}

static void testUnlocks() {
    using namespace ogd;
    SaveData s;
    for (int i = 1; i <= 4; ++i) CHECK(isUnlocked(iconUnlockRule(i), s, 7));
    for (int c = 0; c <= 3; ++c) CHECK(isUnlocked(colorUnlockRule(c), s, 7));
    int lockedIcons = 0, lockedColors = 0;
    for (int i = 1; i <= kIconCount; ++i) lockedIcons += !isUnlocked(iconUnlockRule(i), s, 7);
    for (int c = 0; c < kPaletteSize; ++c) lockedColors += !isUnlocked(colorUnlockRule(c), s, 7);
    CHECK(lockedIcons == 9 && lockedColors == 8);          // 4+9 icons and 4+8 colours, like the original garage
    CHECK(iconUnlockRule(5).kind == UnlockKind::CompleteLevel && iconUnlockRule(5).arg == 0);
    CHECK(!isUnlocked(iconUnlockRule(5), s, 7));
    s.recordBest(0, 99); CHECK(!isUnlocked(iconUnlockRule(5), s, 7));
    s.recordBest(0, 100); CHECK(isUnlocked(iconUnlockRule(5), s, 7) && isUnlocked(colorUnlockRule(4), s, 7));
    CHECK(!isUnlocked(iconUnlockRule(6), s, 7) && s.completedLevels(7) == 1);
    for (int l = 1; l < 4; ++l) s.recordBest(l, 100);
    CHECK(isUnlocked(iconUnlockRule(12), s, 7) && !isUnlocked(iconUnlockRule(13), s, 7));
    for (int l = 4; l < 7; ++l) s.recordBest(l, 100);
    CHECK(isUnlocked(iconUnlockRule(13), s, 7) && isUnlocked(colorUnlockRule(11), s, 7));
    SaveData d; d.debugUnlockAll = true; CHECK(isUnlocked(iconUnlockRule(13), d, 7));
    // palette wraps and matches the colours seen in the original garage
    CHECK(paletteRgb(0).r == 125 && paletteRgb(0).g == 255 && paletteRgb(3).b == 255 && paletteRgb(12).r == 125 && paletteRgb(-1).g == 255);
}

int main() {
    testPlist(); testAtlas(); testFont(); testPhysics(); testSave(); testInflate(); testLevel(); testRealLevels(); testUnlocks();
    if (failures) { std::printf("%d check(s) failed\n", failures); return 1; }
    std::printf("all core tests passed\n");
    return 0;
}
