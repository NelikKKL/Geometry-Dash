// Gameplay simulation (no SDL): cube + ship physics, solid/hazard collisions, portals, pads, rings.
// A port of the OpenGD fork's PlayerObject / PlayLayer::checkCollisions logic (see README "Physics source").
// All coordinates are GD units (block = 30, floor top y = 0, player centre y = 15 when standing on the floor).
//
// Gravity-up sections are simulated in a mirrored "local" space (y -> 300 - y), so every rule is written once.
#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "core/Level.h"
#include "core/ObjectTable.h"

namespace ogd {

enum class PlayMode : uint8_t { Cube, Ship };

struct SimConfig {
    double xVel = 5.770002;        // per frame, times speed
    double speed = 0.9;
    double gravity = 0.958199;
    double jump = 11.180032;
    double maxFall = 15.0;
    double rotateDegPerFrame = 180.0 / 0.41 / 60.0;   // RotateBy(0.41 s, 180) while airborne

    // Ship (from PlayerObject::updateJump). shipScale multiplies the acceleration (tuning knob, 1 = original).
    double shipScale = 1.0;
    double shipUpLimit = 8.0;
    double shipDownLimit = 6.4;

    double padBoost = 16.0;        // yellow pad: propellPlayer(1) -> 16
    double ringBoost = 11.180032;  // yellow ring: the jump height

    double planeHeight = 300.0;    // distance floor <-> ceiling (gravity-up mirror plane)
    double playerSize = 30.0;      // outer box, used for landing, hazards, portals, pads, rings
    double innerSize = 7.5;        // inner box: touching a block with it = death
    double cubeLandMod = 10.0;     // a falling cube lands if its centre is >= block top + mod (i.e. bottom >= top - 5)
    double shipLandMod = 6.0;
    double shipFloor = 3.0;        // ship centre may not go below this (cube: 15)
    double startX = -20.0;
    bool startAsShip = false;      // testing / debugging only
};

struct SimPlayer {
    double x = 0, y = 15;          // world, centre
    double vy = 0;                 // world, internal units (positive = up)
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
    void setHolding(bool h);       // true = finger/space down
    void step(double dtFrames);    // dtFrames: elapsed time in 60 fps frames (clamped to 2)

    const SimPlayer& player() const { return p_; }
    bool holding() const { return holding_; }
    double endX() const { return endX_; }
    const std::vector<uint8_t>& usedFlags() const { return used_; }   // per level object: pad/ring/portal already triggered
    long long jumpCount() const { return jumps_; }   // cube jumps + ring jumps since reset (Stats board)
    double progress() const;       // 0..1 (x / x of the last object, like the original percentage)
    const Level& level() const { return *level_; }
    const SimConfig& config() const { return cfg_; }

    struct Box { float minx, maxx, miny, maxy; };
    // Hitbox of object `index` in world units (valid for Solid/Hazard/Portal/Pad/Orb kinds).
    Box objectBox(size_t index) const { return (*boxes_)[index]; }

private:
    void substep(double h);
    void updateJump(double dtSlow);
    void collide();
    void collideSolid(const Box& s);
    void hitGround() { lvy_ = 0; onGround_ = true; queuedHold_ = false; }
    void setMode(PlayMode m);
    void flipGravity(bool up);
    Box localBox(const Box& b) const;
    Box outerBox() const;
    Box innerBox() const;
    void die() { p_.dead = true; }
    bool falling() const { return lvy_ < cfg_.gravity; }

    const Level* level_;
    SimConfig cfg_;
    std::shared_ptr<const std::vector<Box>> boxes_;
    std::vector<uint8_t> used_;
    double endX_ = 0;

    SimPlayer p_;
    double ly_ = 15;               // local y (mirrored when gravity is up)
    double lvy_ = 0;               // local vy
    bool onGround_ = true, rising_ = false;
    bool holding_ = false, queuedHold_ = false;
    bool touchedRing_ = false;
    double rot_ = 0;
    long long jumps_ = 0;
};

} // namespace ogd
