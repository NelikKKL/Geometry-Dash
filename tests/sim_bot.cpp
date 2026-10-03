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

struct Plan { std::vector<uint8_t> hold; };

// survival time (frames) of a plan over the horizon; horizon+1 means "alive / finished"
static int rollout(const Simulation& start, const std::vector<uint8_t>& plan, int horizon) {
    Simulation s = start;
    for (int f = 0; f < horizon; ++f) {
        s.setHolding(f < (int)plan.size() ? plan[f] : plan.back());
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
    const int ks[] = {1, 2, 3, 5, 8, 12, 20};
    // single press at ANY delay: the bot must not commit to "jump now" while a later takeoff still works
    for (int r = 0; r < horizon - 2; ++r) for (int k : ks) mk(r, k);
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
        if (const char* e = std::getenv("ORB")) cfg.orbBoost = std::atof(e);
        if (const char* e = std::getenv("SHIP_UP")) cfg.shipAccelUp = std::atof(e);
        if (const char* e = std::getenv("SHIP_DOWN")) cfg.shipAccelDown = std::atof(e);
        if (const char* e = std::getenv("SHIP_MAXUP")) cfg.shipMaxUp = std::atof(e);
        if (const char* e = std::getenv("SHIP_MAXDOWN")) cfg.shipMaxDown = std::atof(e);
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
            int best = 0, bestScore = -1;
            if (!carry.hold.empty()) {                    // yesterday's plan, shifted by one frame, usually still works
                Plan shifted; shifted.hold.assign(carry.hold.begin() + 1, carry.hold.end()); shifted.hold.push_back(0);
                if (rollout(sim, shifted.hold, H) > H) { plans.clear(); plans.push_back(shifted); bestScore = H + 1; }
            }
            for (size_t i = 0; i < plans.size() && bestScore <= H; ++i) {
                int sc = rollout(sim, plans[i].hold, H);
                if (sc > bestScore || (sc == bestScore && plans[best].hold[0] && !plans[i].hold[0])) { bestScore = sc; best = (int)i; }
                if (sc > H) break;                       // first fully surviving plan wins (structured ones come first)
            }
            if (bestScore <= H) { ++replans; carry.hold.clear(); }
            carry = plans[best];
            sim.setHolding(plans[best].hold[0]);
            sim.step(1.0);
            trace.push_back({frame, sim.player().x, sim.player().y, sim.player().vy, (int)plans[best].hold[0], (int)sim.player().onGround});
            ++frame;
        }
        const auto& p = sim.player();
        if (p.finished || (stopX > 0 && p.x >= stopX)) std::printf("%-28s COMPLETED  %5d frames (%.1fs), %d tight replans\n", argv[a], frame, frame / 60.0, replans);
        else {
            ++bad;
            std::printf("%-28s FAILED     at x=%.0f y=%.0f (%s, %.0f%%) after %d frames\n", argv[a], p.x, p.y,
                        p.mode == PlayMode::Ship ? "ship" : "cube", sim.progress() * 100, frame);
            if (std::getenv("TRACE")) for (size_t i = trace.size() > 70 ? trace.size() - 70 : 0; i < trace.size(); ++i)
                std::printf("   f%5d x=%7.1f y=%6.1f vy=%6.2f hold=%d ground=%d\n", trace[i].f, trace[i].x, trace[i].y, trace[i].vy, trace[i].h, trace[i].g);
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
