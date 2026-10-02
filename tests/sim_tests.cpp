// Unit tests for the gameplay simulation (synthetic levels, no game assets needed).
#include <cmath>
#include <cstdio>
#include <string>

#include "core/Level.h"
#include "core/Sim.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

using namespace ogd;

static Level mk(const std::string& s) {
    auto r = parseLevel(s);
    if (!r.ok) { std::printf("bad test level: %s\n", r.error.c_str()); std::exit(2); }
    return r.level;
}

// Runs `frames` frames; holdFn(frame) gives the input. Returns the simulation state.
template <class F>
static void run(Simulation& sim, int frames, F holdFn) {
    for (int f = 0; f < frames && !sim.player().dead && !sim.player().finished; ++f) {
        sim.setHolding(holdFn(f));
        sim.step(1.0);
    }
}

static void testFlat() {
    Level L = mk("1,1,2,3000,3,-300");                 // one far-away irrelevant block below the floor
    Simulation sim(L);
    run(sim, 100000, [](int) { return false; });
    CHECK(!sim.player().dead && sim.player().finished);
    CHECK(std::fabs(sim.player().y - 15) < 1e-6 && sim.player().onGround);
    CHECK(sim.progress() >= 1.0);
    // speed: 311.58 units/s
    Simulation s2(L);
    run(s2, 60, [](int) { return false; });
    CHECK(std::fabs(s2.player().x - 311.58) < 0.5);
}

static void testJumpShape() {
    Level L = mk("1,1,2,100000,3,-300");
    Simulation sim(L);
    double peak = 15; int air = 0;
    sim.setHolding(true); sim.step(1.0); sim.setHolding(false);
    while (!sim.player().onGround && air < 200) { sim.step(1.0); peak = std::fmax(peak, sim.player().y); ++air; }
    CHECK(peak - 15 > 55 && peak - 15 < 80);         // ~2 blocks: clears a 60-unit tower only barely
    CHECK(air > 25 && air < 50);
    CHECK(std::fmod(sim.player().rotation, 90.0) == 0.0);
    // frame-rate independence: 30 fps (dt=2) gives about the same peak
    Simulation s2(L);
    s2.setHolding(true); s2.step(2.0); s2.setHolding(false);
    double p2 = 15;
    for (int i = 0; i < 100 && !s2.player().onGround; ++i) { s2.step(2.0); p2 = std::fmax(p2, s2.player().y); }
    CHECK(std::fabs(p2 - peak) < 4.0);
}

static void testSpike() {
    Level L = mk("1,8,2,600,3,15");
    int survive = 0, die = 0;
    for (int start = 60; start < 140; ++start) {
        Simulation sim(L);
        run(sim, 400, [&](int f) { return f >= start && f < start + 3; });
        (sim.player().dead ? die : survive)++;
    }
    CHECK(survive > 5 && die > 5);                       // jumpable, but not always
    Simulation walk(L);
    run(walk, 400, [](int) { return false; });
    CHECK(walk.player().dead && walk.player().x > 560 && walk.player().x < 620);
    // small spike (id 39) and flipped spike on the ceiling do not hurt a player running on the floor
    Level L2 = mk("1,8,2,600,3,285,5,1");
    Simulation s2(L2);
    run(s2, 400, [](int) { return false; });
    CHECK(!s2.player().dead && s2.player().finished);
}

static void testBlocks() {
    // a block in the way: running into it kills; jumping lands on top; walking off drops back to the floor
    Level L = mk("1,1,2,600,3,15");
    Simulation walk(L);
    run(walk, 400, [](int) { return false; });
    CHECK(walk.player().dead);

    bool landedOnTop = false;
    int ok = 0;
    for (int start = 60; start < 140; ++start) {
        Simulation sim(L);
        bool top = false;
        for (int f = 0; f < 400 && !sim.player().dead && !sim.player().finished; ++f) {
            sim.setHolding(f >= start && f < start + 3);
            sim.step(1.0);
            if (sim.player().onGround && std::fabs(sim.player().y - 45) < 1e-6) top = true;
        }
        if (!sim.player().dead && top && sim.player().finished && std::fabs(sim.player().y - 15) < 1e-6) { ++ok; landedOnTop = true; }
    }
    CHECK(landedOnTop && ok > 3);

    // underside hit: low ceiling block kills a jump
    Level C = mk("1,1,2,600,3,75;1,1,2,630,3,75;1,1,2,660,3,75");
    int dead = 0;
    for (int start = 60; start < 140; ++start) {
        Simulation sim(C);
        run(sim, 400, [&](int f) { return f >= start && f < start + 2; });
        dead += sim.player().dead;
    }
    CHECK(dead > 3);
    // running under it is fine
    Simulation under(C);
    run(under, 400, [](int) { return false; });
    CHECK(!under.player().dead);

    // slab (id 40, 14 high) is solid
    Level S = mk("1,40,2,600,3,7");
    Simulation sw(S);
    run(sw, 400, [](int) { return false; });
    CHECK(sw.player().dead);
}

static void testShip() {
    // portal at x=300, then free flight
    Level L = mk("1,13,2,300,3,15;1,1,2,100000,3,-300");
    Simulation sim(L);
    run(sim, 200, [](int) { return false; });
    CHECK(sim.player().mode == PlayMode::Ship && !sim.player().dead);
    CHECK(std::fabs(sim.player().y - 15) < 1e-6);        // resting on the floor, not dead
    run(sim, 20, [](int) { return true; });              // hold: climbs
    CHECK(sim.player().y > 60);
    CHECK(sim.player().rotation < -10);                  // nose up (counter-clockwise)
    run(sim, 400, [](int) { return true; });             // keep holding: stops at the ceiling plane
    CHECK(sim.player().y <= 285.0 + 1e-6 && sim.player().y > 280 && !sim.player().dead);
    run(sim, 100, [](int) { return false; });            // release: falls back
    CHECK(sim.player().y < 285);
    // cube portal switches back
    Level M = mk("1,13,2,300,3,15;1,12,2,800,3,15;1,1,2,100000,3,-300");
    Simulation s2(M);
    run(s2, 70, [](int) { return false; });              // x ~ 363: past the ship portal only
    CHECK(s2.player().mode == PlayMode::Ship);
    run(s2, 150, [](int) { return false; });             // x ~ 1140: past the cube portal
    CHECK(s2.player().mode == PlayMode::Cube);
    // flying into the face of a wall is fatal ...
    Level W = mk("1,13,2,300,3,15;1,1,2,700,3,15;1,1,2,700,3,45;1,1,2,100000,3,-300");
    Simulation sw(W);
    run(sw, 400, [](int) { return false; });
    CHECK(sw.player().dead);
    // ... but a low ceiling is only a bump: holding up under a long bar keeps the ship alive, pinned below it
    std::string bar = "1,13,2,300,3,15;1,1,2,100000,3,-300";
    for (int i = 0; i < 40; ++i) bar += ";1,1,2," + std::to_string(700 + 30 * i) + ",3,75";
    Level B = mk(bar);
    Simulation sb(B);
    run(sb, 190, [](int f) { return f > 128; });         // start climbing just before the bar (x ~ 665)
    CHECK(!sb.player().dead && sb.player().y <= 46.6 && sb.player().y > 40);
}

static void testGravityPortal() {
    Level L = mk("1,11,2,300,3,15;1,1,2,100000,3,-300");   // yellow portal: gravity up
    Simulation sim(L);
    run(sim, 300, [](int) { return false; });
    CHECK(sim.player().mirrored && !sim.player().dead);
    CHECK(std::fabs(sim.player().y - 285) < 1e-6 && sim.player().onGround);   // "falls" up onto the ceiling plane
    // jumping while on the ceiling goes downward
    sim.setHolding(true); sim.step(1.0); sim.setHolding(false);
    CHECK(sim.player().y < 285 && sim.player().vy < 0);
    // blue portal restores normal gravity
    Level M = mk("1,11,2,300,3,15;1,10,2,900,3,285;1,1,2,100000,3,-300");
    Simulation s2(M);
    run(s2, 600, [](int) { return false; });
    CHECK(!s2.player().mirrored && std::fabs(s2.player().y - 15) < 1e-6);
}

static void testPadOrb() {
    Level base = mk("1,1,2,100000,3,-300");
    double jumpPeak = 15;
    { Simulation s(base); s.setHolding(true); s.step(1.0); s.setHolding(false);
      for (int i = 0; i < 100 && !s.player().onGround; ++i) { s.step(1.0); jumpPeak = std::fmax(jumpPeak, s.player().y); } }
    // pad (id 35) on the floor
    Level P = mk("1,35,2,600,3,2;1,1,2,100000,3,-300");
    Simulation sp(P);
    double padPeak = 15;
    run(sp, 400, [&](int) { padPeak = std::fmax(padPeak, sp.player().y); return false; });
    CHECK(padPeak > jumpPeak + 10 && !sp.player().dead);
    // orb (id 36): only reacts to a press while overlapping
    Level O = mk("1,36,2,600,3,60;1,1,2,100000,3,-300");
    Simulation passive(O);
    double lowPeak = 15;
    run(passive, 300, [&](int) { lowPeak = std::fmax(lowPeak, passive.player().y); return false; });
    CHECK(lowPeak < 20);                                 // never touched: ring is above the cube
    Simulation active(O);
    double hi = 15; bool jumped = false;
    for (int f = 0; f < 300; ++f) {
        // jump so that we arrive near the ring, then tap again inside it
        bool h = (f >= 95 && f < 98) || (f >= 112 && f < 114);
        active.setHolding(h);
        active.step(1.0);
        hi = std::fmax(hi, active.player().y);
        if (active.player().vy > 5 && active.player().y > 60) jumped = true;
    }
    CHECK(hi > 60);
    (void)jumped;
}

static void testCopyDeterminism() {
    Level L = mk("1,8,2,600,3,15;1,1,2,900,3,15;1,13,2,1500,3,15;1,1,2,100000,3,-300");
    Simulation a(L);
    for (int i = 0; i < 80; ++i) { a.setHolding(i % 7 < 3); a.step(1.0); }
    Simulation b = a;                                    // copy mid-run
    for (int i = 0; i < 80; ++i) {
        bool h = (i % 5) < 2;
        a.setHolding(h); a.step(1.0);
        b.setHolding(h); b.step(1.0);
    }
    CHECK(a.player().x == b.player().x && a.player().y == b.player().y && a.player().dead == b.player().dead);
    a.reset();
    CHECK(a.player().x == 0 && !a.player().dead && a.player().mode == PlayMode::Cube);
}

int main() {
    testFlat(); testJumpShape(); testSpike(); testBlocks(); testShip(); testGravityPortal(); testPadOrb(); testCopyDeterminism();
    if (failures) { std::printf("%d check(s) failed\n", failures); return 1; }
    std::printf("all sim tests passed\n");
    return 0;
}
