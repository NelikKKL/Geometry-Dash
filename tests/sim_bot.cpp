// Look-ahead bot: tries to complete real level files in the simulation. Validates hitbox/physics tuning.
//   sim_bot <level.txt> [more levels...]        prints one line per level, exit code 1 if any level is not completed
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <vector>

#include "core/Sim.h"

using namespace ogd;

// A plan is either a fixed list of button states or (ship only) an altitude regulator: fly towards altitude `a` for
// `sw` frames, then towards `b`.
struct Plan { std::vector<uint8_t> hold; bool fb = false; int a = 0, b = 0, sw = 0; };

static bool regulate(const Simulation& s, int target) {
    const auto& p = s.player();
    return p.y + 7.0 * p.vy < target;                      // hold while the (predicted) altitude is below the target
}
static bool actionAt(const Simulation& s, const Plan& pl, int f) {
    if (pl.fb) return regulate(s, f < pl.sw ? pl.a : pl.b);
    return f < (int)pl.hold.size() ? pl.hold[f] : pl.hold.back();
}

// survival time (frames) of a plan over the horizon; horizon+1 means "alive / finished"
static int rollout(const Simulation& start, const Plan& plan, int horizon) {
    Simulation s = start;
    for (int f = 0; f < horizon; ++f) {
        s.setHolding(actionAt(s, plan, f));
        s.step(1.0);
        if (s.player().dead) return f;
        if (s.player().finished) return horizon + 1;
    }
    return horizon + 1;
}

static std::vector<Plan> structured(int horizon, bool ship) {
    std::vector<Plan> out;
    auto mk = [&](int rel, int hold, int rel2 = 0, int hold2 = 0) {
        Plan p; p.hold.assign(horizon, 0);
        for (int i = 0; i < hold && rel + i < horizon; ++i) p.hold[rel + i] = 1;
        for (int i = 0; i < hold2 && rel + hold + rel2 + i < horizon; ++i) p.hold[rel + hold + rel2 + i] = 1;
        out.push_back(p);
    };
    mk(0, 0);                                            // do nothing
    const int cubeKs[] = {1, 2, 3, 5, 8, 12, 20};
    const int shipKs[] = {1, 2, 3, 5, 8, 12, 20, 30, 45, 60, 80};
    const int* ks = ship ? shipKs : cubeKs;
    const int nks = ship ? 11 : 7;
    // single press at ANY delay: the bot must not commit to "jump now" while a later takeoff still works
    for (int r = 0; r < horizon - 2; ++r) for (int ki = 0; ki < nks; ++ki) mk(r, ks[ki]);
    if (ship) {
        for (int k : {2, 5, 10}) for (int r : {1, 3, 6, 10}) for (int k2 : {2, 4, 8}) mk(0, k, r, k2);
    } else {
        // press, then press again later (orbs / pads): any first delay, a range of gaps
        for (int a = 0; a <= 45; ++a) for (int gap : {3, 5, 7, 9, 11, 14, 18, 24}) for (int k2 : {1, 3}) mk(a, 1, gap, k2);
    }
    // plans whose first action is "release" come first
    std::stable_sort(out.begin(), out.end(), [](const Plan& a, const Plan& b) { return a.hold[0] < b.hold[0]; });
    return out;
}

int main(int argc, char** argv) {
    int bad = 0;
    for (int a = 1; a < argc; ++a) {
        std::ifstream f(argv[a], std::ios::binary);
        std::stringstream ss; ss << f.rdbuf();
        auto r = parseLevelString(ss.str());
        if (!r.ok) { std::printf("%s: parse error\n", argv[a]); ++bad; continue; }
        SimConfig cfg;
        if (const char* e = std::getenv("PAD")) cfg.padBoost = std::atof(e);
        if (const char* e = std::getenv("ORB")) cfg.ringBoost = std::atof(e);
        if (const char* e = std::getenv("SHIP_SCALE")) cfg.shipScale = std::atof(e);
        if (const char* e = std::getenv("STARTX")) cfg.startX = std::atof(e);
        if (std::getenv("SHIP")) cfg.startAsShip = true;
        const double stopX = std::getenv("STOPX") ? std::atof(std::getenv("STOPX")) : 0.0;   // count reaching this x as success
        Simulation sim(r.level, cfg);
        std::mt19937 rng(12345);
        const int H = std::getenv("H") ? std::atoi(std::getenv("H")) : 70;
        auto cubePlans = structured(H, false);
        auto shipPlans = structured(H, true);
        int frame = 0, replans = 0;
        Plan carry;
        struct T { int f; double x, y, vy; int h, g; }; std::vector<T> trace;
        const int cap = (int)(sim.endX() / 5.19) + 600;
        while (!sim.player().dead && !sim.player().finished && !(stopX > 0 && sim.player().x >= stopX) && frame < cap) {
            const bool ship = sim.player().mode == PlayMode::Ship;
            std::vector<Plan> plans = ship ? shipPlans : cubePlans;
            // random plans: piecewise-constant segments
            for (int i = 0; i < (ship ? 120 : 60); ++i) {
                Plan p; p.hold.assign(H, 0);
                int t = 0; bool h = rng() & 1;
                while (t < H) { int len = 2 + rng() % 10; for (int j = 0; j < len && t < H; ++j) p.hold[t++] = h; h = !h; }
                plans.push_back(std::move(p));
            }
            if (ship) {
                for (int a = 20; a <= 280; a += 20) { Plan q; q.fb = true; q.a = q.b = a; q.sw = 0; plans.push_back(q); }
                for (int a = 20; a <= 280; a += 20) for (int b = 20; b <= 280; b += 20) if (a != b)
                    for (int sw : {30, 60, 100, 150}) { Plan q; q.fb = true; q.a = a; q.b = b; q.sw = sw; plans.push_back(q); }
            }
            int best = 0, bestScore = -1;
            if (!carry.hold.empty() || carry.fb) {                    // yesterday's plan, shifted by one frame, usually still works
                Plan shifted = carry;
                if (carry.fb) shifted.sw = std::max(0, carry.sw - 1);
                else { shifted.hold.assign(carry.hold.begin() + 1, carry.hold.end()); shifted.hold.push_back(0); }
                if (rollout(sim, shifted, H) > H) { plans.clear(); plans.push_back(shifted); bestScore = H + 1; }
            }
            for (size_t i = 0; i < plans.size() && bestScore <= H; ++i) {
                int sc = rollout(sim, plans[i], H);
                if (sc > bestScore || (sc == bestScore && actionAt(sim, plans[best], 0) && !actionAt(sim, plans[i], 0))) { bestScore = sc; best = (int)i; }
                if (sc > H) break;                       // first fully surviving plan wins (structured ones come first)
            }
            if (bestScore <= H) { ++replans; carry.hold.clear(); carry.fb = false; }
            carry = plans[best];
            const bool act = actionAt(sim, plans[best], 0);
            sim.setHolding(act);
            sim.step(1.0);
            trace.push_back({frame, sim.player().x, sim.player().y, sim.player().vy, (int)act, (int)sim.player().onGround});
            ++frame;
        }
        const auto& p = sim.player();
        if (p.finished || (stopX > 0 && p.x >= stopX)) std::printf("%-28s COMPLETED  %5d frames (%.1fs), %d tight replans\n", argv[a], frame, frame / 60.0, replans);
        else {
            ++bad;
            std::printf("%-28s FAILED     at x=%.0f y=%.0f (%s, %.0f%%) after %d frames\n", argv[a], p.x, p.y,
                        p.mode == PlayMode::Ship ? "ship" : "cube", sim.progress() * 100, frame);
            if (std::getenv("TRACE")) {
                const size_t tn = std::getenv("TRACE_N") ? (size_t)std::atoi(std::getenv("TRACE_N")) : 70;
                for (size_t i = trace.size() > tn ? trace.size() - tn : 0; i < trace.size(); i += (tn > 100 ? 6 : 1))
                    std::printf("   f%5d x=%7.1f y=%6.1f vy=%6.2f hold=%d ground=%d\n", trace[i].f, trace[i].x, trace[i].y, trace[i].vy, trace[i].h, trace[i].g);
            }
            auto rg = sim.level().range((float)p.x - 40, (float)p.x + 40);
            for (size_t i = rg.first; i < rg.second; ++i) {
                const auto& o = sim.level().objects[i];
                auto b = sim.objectBox(i);
                const ObjInfo* info = objectInfo(o.id);
                if (info && (info->kind == ObjKind::Solid || info->kind == ObjKind::Hazard))
                    std::printf("     near: id %d at (%.0f,%.0f) rot %.0f fy %d  box x[%.0f..%.0f] y[%.0f..%.0f]\n", o.id, o.x, o.y, o.rotation, o.flipY, b.minx, b.maxx, b.miny, b.maxy);
            }
        }
    }
    return bad ? 1 : 0;
}
