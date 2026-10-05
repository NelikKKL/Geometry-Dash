// Unit tests for the gameplay simulation (synthetic levels, no game assets needed).
#include <cmath>
#include <cstdio>
#include <string>

#include "core/Level.h"
#include "core/Sim.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

using namespace ogd;

// Every test level gets a far-away dummy object so that the level does not end early.
static const char* kFar = ";1,1,2,100000,3,-300";

static Level mk(const std::string& s) {
    auto r = parseLevel(s);
    if (!r.ok) { std::printf("bad test level: %s\n", r.error.c_str()); std::exit(2); }
    return r.level;
}
static Level mkFar(const std::string& s) { return mk(s + kFar); }

template <class F>
static void run(Simulation& sim, int frames, F holdFn) {
    for (int f = 0; f < frames && !sim.player().dead && !sim.player().finished; ++f) {
        sim.setHolding(holdFn(f));
        sim.step(1.0);
    }
}

static double jumpPeak() {
    Level L = mk("1,1,2,100000,3,-300");
    Simulation sim(L);
    double peak = 15;
    sim.setHolding(true); sim.step(1.0); sim.setHolding(false);
    for (int i = 0; i < 200 && !sim.player().onGround; ++i) { sim.step(1.0); peak = std::fmax(peak, sim.player().y); }
    return peak;
}

static void testFlat() {
    Level L = mk("1,1,2,3000,3,-300");
    Simulation sim(L);
    run(sim, 100000, [](int) { return false; });
    CHECK(!sim.player().dead && sim.player().finished);
    CHECK(std::fabs(sim.player().y - 15) < 1e-6 && sim.player().onGround);
    CHECK(sim.progress() >= 1.0);
    Simulation s2(L);
    run(s2, 60, [](int) { return false; });
    CHECK(std::fabs(s2.player().x - (-20 + 311.58)) < 0.5);       // 311.58 units/s, start x = -20
}

static void testJumpShape() {
    const double peak = jumpPeak();
    CHECK(peak - 15 > 55 && peak - 15 < 80);                        // ~2 blocks: clears a 60-unit tower only barely
    Level L = mk("1,1,2,100000,3,-300");
    Simulation sim(L);
    int air = 0;
    sim.setHolding(true); sim.step(1.0); sim.setHolding(false);
    while (!sim.player().onGround && air < 200) { sim.step(1.0); ++air; }
    CHECK(air > 20 && air < 45);
    CHECK(std::fmod(sim.player().rotation, 90.0) == 0.0);
    Simulation s2(L);                                               // frame-rate independence: 30 fps gives the same peak
    s2.setHolding(true); s2.step(2.0); s2.setHolding(false);
    double p2 = 15;
    for (int i = 0; i < 100 && !s2.player().onGround; ++i) { s2.step(2.0); p2 = std::fmax(p2, s2.player().y); }
    CHECK(std::fabs(p2 - peak) < 4.0);
}

static void testHazards() {
    // spike (id 8): jumpable, but not always
    Level L = mkFar("1,8,2,600,3,15");
    int survive = 0, die = 0;
    for (int start = 60; start < 140; ++start) {
        Simulation sim(L);
        run(sim, 400, [&](int f) { return f >= start && f < start + 3; });
        (sim.player().dead ? die : survive)++;
    }
    CHECK(survive > 5 && die > 5);
    Simulation walk(L);
    run(walk, 400, [](int) { return false; });
    CHECK(walk.player().dead && walk.player().x > 560 && walk.player().x < 620);

    // every hazard id kills a cube that runs into it: 8 spike, 39 small spike, 9 ground thorns
    for (int id : {8, 39, 9}) {
        Level H = mkFar("1," + std::to_string(id) + ",2,600,3," + (id == 9 ? "2" : (id == 39 ? "6" : "15")));
        Simulation s(H);
        run(s, 400, [](int) { return false; });
        CHECK(s.player().dead);
    }
    // decoration never kills: id 5 fill block, 15-17 rods, 18-21 white thorns, 41 chain
    for (int id : {5, 15, 16, 17, 18, 19, 20, 21, 41}) {
        Level H = mkFar("1," + std::to_string(id) + ",2,600,3,15");
        Simulation s(H);
        run(s, 400, [](int) { return false; });
        CHECK(!s.player().dead);
    }
    // an upside-down spike on the ceiling does not touch a cube on the floor
    Level C = mkFar("1,8,2,600,3,285,5,1");
    Simulation sc(C);
    run(sc, 400, [](int) { return false; });
    CHECK(!sc.player().dead);
}

static void testBlocks() {
    Level L = mkFar("1,1,2,600,3,15");
    Simulation walk(L);
    run(walk, 400, [](int) { return false; });
    CHECK(walk.player().dead);                                     // running into a wall

    int ok = 0;                                                     // jump on top, walk off the far edge, land on the floor
    for (int start = 60; start < 140; ++start) {
        Simulation sim(L);
        bool top = false;
        for (int f = 0; f < 400 && !sim.player().dead && !sim.player().finished; ++f) {
            sim.setHolding(f >= start && f < start + 3);
            sim.step(1.0);
            if (sim.player().onGround && std::fabs(sim.player().y - 45) < 1e-6) top = true;
        }
        if (!sim.player().dead && top && std::fabs(sim.player().y - 15) < 1e-6) ++ok;
    }
    CHECK(ok > 3);

    Level C = mkFar("1,1,2,600,3,75;1,1,2,630,3,75;1,1,2,660,3,75");   // low ceiling: a jump into it kills, walking under is fine
    int dead = 0;
    for (int start = 60; start < 140; ++start) {
        Simulation sim(C);
        run(sim, 400, [&](int f) { return f >= start && f < start + 2; });
        dead += sim.player().dead;
    }
    CHECK(dead > 3);
    Simulation under(C);
    run(under, 400, [](int) { return false; });
    CHECK(!under.player().dead);

    Level S = mkFar("1,40,2,600,3,7");                              // slab (id 40, 14 high) is solid
    Simulation sw(S);
    run(sw, 400, [](int) { return false; });
    CHECK(sw.player().dead);
}

// The Back On Track spot: a spike, then a column with a one-block window at y 60..90.
static void testOneBlockWindow() {
    Level L = mkFar("1,8,2,700,3,15;1,1,2,730,3,15;1,1,2,730,3,45;1,1,2,730,3,105;1,1,2,730,3,135");
    int working = 0;
    for (int start = 100; start < 140; ++start) {
        Simulation sim(L);
        run(sim, 400, [&](int f) { return f >= start && f < start + 3; });
        working += !sim.player().dead;
    }
    CHECK(working >= 8);                                            // >= ~130 ms of timing tolerance
    std::printf("   one-block window: %d working takeoff frames\n", working);
}

// Walking off a platform keeps "on ground" for ~2 frames, so a jump pressed just after the edge still works.
static void testCoyote() {
    Level L = mkFar("1,1,2,600,3,15");
    auto vyAfterEdge = [&](int waitFrames) {
        for (int start = 100; start < 130; ++start) {
            Simulation sim(L);
            bool onTop = false;
            int sinceEdge = -1;
            for (int f = 0; f < 300 && !sim.player().dead; ++f) {
                bool press = false;
                if (onTop && sim.player().x - 15 > 615) { if (sinceEdge < 0) sinceEdge = 0; else ++sinceEdge; }
                if (sinceEdge >= waitFrames) press = true;
                sim.setHolding(press || (f >= start && f < start + 3));
                sim.step(1.0);
                if (sim.player().onGround && std::fabs(sim.player().y - 45) < 1e-6) onTop = true;
                if (sinceEdge >= waitFrames && sim.player().y > 45.5 && sim.player().vy > 5) return 1;    // jumped from thin air
                if (sinceEdge >= waitFrames + 1) break;
            }
        }
        return 0;
    };
    CHECK(vyAfterEdge(1) == 1);                                     // 1 frame after leaving the block: still a valid jump
    CHECK(vyAfterEdge(5) == 0);                                     // 5 frames later: no jump any more
}

static void testShip() {
    Level L = mkFar("1,13,2,300,3,15");
    Simulation sim(L);
    run(sim, 200, [](int) { return false; });
    CHECK(sim.player().mode == PlayMode::Ship && !sim.player().dead);
    CHECK(std::fabs(sim.player().y - 3) < 1e-6);                    // ship rests 3 units above the floor line
    run(sim, 30, [](int) { return true; });
    const double y30 = sim.player().y;
    CHECK(y30 > 45);                                                // holding climbs
    CHECK(sim.player().rotation < -10);                             // nose up
    run(sim, 400, [](int) { return true; });
    CHECK(sim.player().y <= 297.0 + 1e-6 && sim.player().y > 290 && !sim.player().dead);   // pinned at the ceiling
    run(sim, 100, [](int) { return false; });
    CHECK(sim.player().y < 297);
    // vertical speed never exceeds the documented limits (+8 up, -6.4 down)
    Simulation s3(L);
    double maxUp = 0, maxDown = 0;
    for (int f = 0; f < 600; ++f) {
        s3.setHolding((f / 90) % 2 == 0); s3.step(1.0);
        if (s3.player().mode != PlayMode::Ship) continue;           // the first frames are still cube (jump = 11.18)
        maxUp = std::fmax(maxUp, s3.player().vy); maxDown = std::fmin(maxDown, s3.player().vy);
    }
    CHECK(maxUp <= 8.0 + 1e-9 && maxDown >= -6.4 - 1e-9 && maxUp > 5 && maxDown < -4);

    Level M = mkFar("1,13,2,300,3,15;1,12,2,800,3,15");
    Simulation s2(M);
    run(s2, 70, [](int) { return false; });
    CHECK(s2.player().mode == PlayMode::Ship);
    run(s2, 150, [](int) { return false; });
    CHECK(s2.player().mode == PlayMode::Cube);

    Level W = mkFar("1,13,2,300,3,15;1,1,2,700,3,15;1,1,2,700,3,45");   // flying into the face of a wall is fatal
    Simulation sw(W);
    run(sw, 400, [](int) { return false; });
    CHECK(sw.player().dead);

    std::string bar = "1,13,2,300,3,15";                              // a low ceiling is only a bump
    for (int i = 0; i < 40; ++i) bar += ";1,1,2," + std::to_string(700 + 30 * i) + ",3,75";
    Level B = mkFar(bar);
    Simulation sb(B);
    run(sb, 190, [](int f) { return f > 128; });
    CHECK(!sb.player().dead && sb.player().y <= 46.5 && sb.player().y > 40);
}

static void testGravityPortal() {
    Level L = mkFar("1,11,2,300,3,15");
    Simulation sim(L);
    run(sim, 300, [](int) { return false; });
    CHECK(sim.player().mirrored && !sim.player().dead);
    CHECK(std::fabs(sim.player().y - 285) < 1e-6 && sim.player().onGround);
    sim.setHolding(true); sim.step(1.0); sim.setHolding(false);
    CHECK(sim.player().y < 285 && sim.player().vy < 0);             // jumping from the ceiling goes down
    Level M = mkFar("1,11,2,300,3,15;1,10,2,900,3,285");
    Simulation s2(M);
    run(s2, 600, [](int) { return false; });
    CHECK(!s2.player().mirrored && std::fabs(s2.player().y - 15) < 1e-6);
}

static void testPadRing() {
    const double peak = jumpPeak();
    Level P = mkFar("1,35,2,600,3,2");
    Simulation sp(P);
    double padPeak = 15;
    run(sp, 400, [&](int) { padPeak = std::fmax(padPeak, sp.player().y); return false; });
    CHECK(padPeak > peak + 20 && !sp.player().dead);

    Level O = mkFar("1,36,2,600,3,60");
    Simulation passive(O);
    double lowPeak = 15;
    run(passive, 300, [&](int) { lowPeak = std::fmax(lowPeak, passive.player().y); return false; });
    CHECK(lowPeak < 20);                                            // never touched: the ring is above the cube

    // A tap while the cube is in the air and still held when it touches the ring triggers it. Count how many
    // second-tap moments work: the timing must be generous.
    int worked = 0;
    for (int tap2 = 10; tap2 < 60; ++tap2) {
        Simulation s(O);
        double hi = 15;
        for (int f = 0; f < 300 && !s.player().dead; ++f) {
            const bool h = (f >= 90 && f < 93) || (f >= 90 + tap2 && f < 90 + tap2 + 25);
            s.setHolding(h); s.step(1.0);
            hi = std::fmax(hi, s.player().y);
        }
        worked += hi > peak + 12;
    }
    CHECK(worked >= 12);
    std::printf("   ring: %d of 50 second-tap moments trigger it\n", worked);

    // holding the button from the ground does NOT trigger a ring in mid-air (the tap was used by the jump)
    Simulation held(O);
    double hh = 15;
    for (int f = 0; f < 300 && !held.player().dead; ++f) { held.setHolding(f >= 92 && f < 130); held.step(1.0); hh = std::fmax(hh, held.player().y); }
    CHECK(hh < peak + 8);
}

static void testCopyDeterminism() {
    Level L = mkFar("1,8,2,600,3,15;1,1,2,900,3,15;1,13,2,1500,3,15");
    Simulation a(L);
    for (int i = 0; i < 80; ++i) { a.setHolding(i % 7 < 3); a.step(1.0); }
    Simulation b = a;
    for (int i = 0; i < 80; ++i) {
        bool h = (i % 5) < 2;
        a.setHolding(h); a.step(1.0);
        b.setHolding(h); b.step(1.0);
    }
    CHECK(a.player().x == b.player().x && a.player().y == b.player().y && a.player().dead == b.player().dead);
    a.reset();
    CHECK(a.player().x == -20 && !a.player().dead && a.player().mode == PlayMode::Cube);
}

int main() {
    testFlat(); testJumpShape(); testHazards(); testBlocks(); testOneBlockWindow(); testCoyote(); testShip();
    testGravityPortal(); testPadRing(); testCopyDeterminism();
    if (failures) { std::printf("%d check(s) failed\n", failures); return 1; }
    std::printf("all sim tests passed\n");
    return 0;
}
