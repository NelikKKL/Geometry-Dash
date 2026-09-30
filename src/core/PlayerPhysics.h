// Cube physics ported from the original OpenGD PlayerObject
// (constants are GD 1x units per 60fps frame, scaled x2 for the 1280x720 design space).
// Header-only, no SDL dependency, unit-tested.
#pragma once
#include <cmath>

namespace ogd {

constexpr double kHdScale    = 2.0;
constexpr double kGroundY    = 236.0;  // cube centre Y when standing on the ground
constexpr double kPlayerSize = 60.0;

struct PlayerState {
    double x = 0, y = kGroundY;
    double yVel = 0;
    double rotation = 0;   // degrees, clockwise
    bool onGround = true;
    bool holding = false;
    bool dead = false;
    bool locked = false;
};

struct PlayerParams {
    double xVel = 5.770002;
    double gravity = 0.958199;
    double jumpHeight = 11.180032;
    double speed = 0.9;
    double rotateSpeed = 180.0 / 0.43333; // deg/s while airborne
};

// dtFrames: elapsed time in 60fps frames. 4 sub-steps, like the original PlayLayer.
inline void stepPlayer(PlayerState& p, double dtFrames, const PlayerParams& k = PlayerParams()) {
    if (p.dead || p.locked) return;
    if (dtFrames > 2.0) dtFrames = 2.0;
    const double h = dtFrames / 4.0;
    for (int i = 0; i < 4; ++i) {
        if (p.y <= kGroundY) {
            p.onGround = true;
            p.y = kGroundY;
            if (p.yVel < 0) p.yVel = 0;
            p.rotation = 90.0 * std::round(p.rotation / 90.0);
        }
        const double jdt = h * 0.9; // updateJump(dt * 0.9)
        if (p.holding && p.onGround) {
            p.onGround = false;
            p.yVel = k.jumpHeight;
        } else if (!p.onGround) {
            p.yVel -= k.gravity * jdt;
        }
        p.x += h * k.speed * k.xVel * kHdScale;
        p.y += h * k.speed * p.yVel * kHdScale;
        if (!p.onGround) p.rotation += k.rotateSpeed * (h / 60.0);
    }
}

} // namespace ogd
