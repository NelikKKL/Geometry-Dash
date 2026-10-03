// Gameplay simulation (no SDL): cube + ship physics, solid/hazard collisions, portals, pads, orbs.
// All coordinates are GD units (block = 30, floor top y = 0, player centre y = 15 when standing on the floor).
//
// Gravity-up sections are simulated in a mirrored "local" space (y -> 300 - y), so every rule is written once.
// Physics constants for the cube come from the original OpenGD code; hitbox sizes, ship physics and
// pad/orb strengths are approximations tuned so that the official levels can be completed (see tests/sim_bot).
#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "core/Level.h"
#include "core/ObjectTable.h"

namespace ogd {

enum class PlayMode : uint8_t { Cube, Ship };

struct SimConfig {
    double xVel = 5.770002;        // units per frame * speed
    double speed = 0.9;
    double gravity = 0.958199;
    double jump = 11.180032;
    double maxFall = 15.0;
    double rotateDegPerFrame = (180.0 / 0.43333) / 60.0;

    // Ship: 0.257 corresponds to the documented ship gravity of 25 blocks/s^2 (GeometryPhysics project); thrust
    // while holding is assumed equal. A speed cap equal to xVel means the steepest climb is exactly 45 degrees.
    double shipAccelUp = 0.257;
    double shipAccelDown = 0.257;
    double shipMaxUp = 5.770002;
    double shipMaxDown = 5.770002;

    double padBoost = 16.0;
    double orbBoost = 11.18;

    double planeHeight = 300.0;    // distance floor <-> ceiling (gravity-up mirror plane)
    double playerSize = 30.0;
    double hazardBox = 12.0;       // player box used against hazards (smaller than the body)
    double solidInset = 1.5;       // body shrinks by this on every side when tested against blocks (gap tolerance)
    double landSlack = 5.0;        // how far below a block's top edge the cube may be and still land on it
    double endPadding = 300.0;     // level ends this far beyond the last object
    double startX = 0.0;
    bool startAsShip = false;      // testing / debugging only
};

struct SimPlayer {
    double x = 0, y = 15;          // world, centre
    double vy = 0;                 // world, units/frame*0.9 (positive = up)
    double rotation = 0;           // degrees clockwise, as drawn
    PlayMode mode = PlayMode::Cube;
    bool mirrored = false;         // gravity points up
    bool onGround = true;
    bool dead = false;
    bool finished = false;
};

class Simulation {
public:
    explicit Simulation(const Level& level, const SimConfig& cfg = SimConfig());

    void reset();
    void setHolding(bool h);       // true = finger/space down; a false->true edge also triggers orbs
    void step(double dtFrames);    // dtFrames: elapsed time in 60 fps frames (clamped to 2)

    const SimPlayer& player() const { return p_; }
    bool holding() const { return holding_; }
    double endX() const { return endX_; }
    double progress() const;       // 0..1
    const Level& level() const { return *level_; }
    const SimConfig& config() const { return cfg_; }

    struct Box { float minx, maxx, miny, maxy; };
    // Hitbox of object `index` in world units (valid for Solid/Hazard/Portal/Pad/Orb kinds).
    Box objectBox(size_t index) const { return (*boxes_)[index]; }

private:
    void substep(double h);
    void cubeMove(double h);
    void shipMove(double h);
    bool resolveSolids(double prevLocalY);
    bool touchesHazard() const;
    void touchTriggers();
    bool supported() const;
    Box localBox(const Box& b) const;
    Box bodyBox() const;           // local space
    Box hazardBox() const;         // local space
    void flipGravity(bool up);
    void die() { p_.dead = true; }

    const Level* level_;
    SimConfig cfg_;
    std::shared_ptr<const std::vector<Box>> boxes_;
    std::vector<uint8_t> used_;
    double endX_ = 0;

    SimPlayer p_;
    double ly_ = 15;               // local y (mirrored when gravity is up)
    double lvy_ = 0;               // local vy
    bool holding_ = false, pressPending_ = false;
    double rot_ = 0;
};

} // namespace ogd
