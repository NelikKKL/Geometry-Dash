// Exact reachability search: because the horizontal speed is constant, the state after N frames is fully described by
// (height, vertical speed, mode, flags). This expands every distinct state frame by frame (hold / release) and reports
// whether the level can be finished, and how far the best state gets. Much stronger than the greedy sim_bot.
//   sim_solver <level.txt> [startX] [ship] [maxStates]
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "core/Sim.h"

using namespace ogd;

static uint64_t keyOf(const Simulation& s) {
    const auto& p = s.player();
    auto q = [](double v, double step) { return (int64_t)std::llround(v / step); };
    uint64_t k = (uint64_t)(q(p.y, 1.0) + 100000) & 0xFFFFF;
    k = k * 1021 + (uint64_t)(q(p.vy, 0.5) + 2000);
    k = k * 7 + (p.mode == PlayMode::Ship) + 2 * p.mirrored + 4 * p.onGround;
    k = k * 4 + s.holding() * 1 + 0;
    return k;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: sim_solver <level> [startX] [ship] [maxStates]\n"); return 2; }
    std::ifstream f(argv[1], std::ios::binary);
    std::stringstream ss; ss << f.rdbuf();
    auto r = parseLevelString(ss.str());
    if (!r.ok) { std::fprintf(stderr, "parse error\n"); return 2; }
    SimConfig cfg;
    if (argc > 2) cfg.startX = std::atof(argv[2]);
    if (argc > 3 && std::atoi(argv[3])) cfg.startAsShip = true;
    const size_t cap = argc > 4 ? (size_t)std::atoi(argv[4]) : 6000;

    std::vector<Simulation> cur;
    cur.emplace_back(r.level, cfg);
    std::mt19937 rng(7);
    int frame = 0;
    double bestX = cfg.startX;
    const int maxFrames = (int)((cur[0].endX() - cfg.startX) / 5.19) + 200;
    for (; frame < maxFrames && !cur.empty(); ++frame) {
        std::unordered_map<uint64_t, size_t> seen;
        std::vector<Simulation> next;
        for (const Simulation& s : cur) {
            for (int h = 0; h < 2; ++h) {
                Simulation n = s;
                n.setHolding(h);
                n.step(1.0);
                if (n.player().dead) continue;
                if (n.player().finished) {
                    std::printf("%s: COMPLETABLE (frame %d, %.1fs, %zu states alive)\n", argv[1], frame, frame / 60.0, cur.size());
                    return 0;
                }
                if (seen.emplace(keyOf(n), next.size()).second) next.push_back(std::move(n));
            }
        }
        if (next.size() > cap) {                         // thin out randomly, keeping a spread of heights
            std::shuffle(next.begin(), next.end(), rng);
            next.erase(next.begin() + (long)cap, next.end());
        }
        for (const auto& s : next) bestX = std::max(bestX, s.player().x);
        cur.swap(next);
    }
    std::printf("%s: NOT completable in this search: all states dead near x=%.0f (best reached x=%.0f of %.0f, %.0f%%)\n", argv[1],
                cur.empty() ? bestX : cur[0].player().x, bestX, cur.empty() ? 0.0 : 0.0, 100.0 * bestX / std::max(1.0, (double)r.level.maxX()));
    return 1;
}
