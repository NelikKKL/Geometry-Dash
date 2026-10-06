#include "core/Sim.h"

#include <algorithm>
#include <cmath>

namespace ogd {
namespace {

constexpr double kPi = 3.14159265358979323846;

// The original uses Rect::intersectsRect, which counts touching edges as intersecting.
bool touches(const Simulation::Box& a, const Simulation::Box& b) {
    return a.minx <= b.maxx && a.maxx >= b.minx && a.miny <= b.maxy && a.maxy >= b.miny;
}

// Hitbox rect (origin offset + size, relative to the centre) transformed by flip and rotation -> AABB.
Simulation::Box makeBox(const LevelObject& o, const ObjInfo& info) {
    const double rad = o.rotation * kPi / 180.0;
    const double c = std::cos(rad), s = std::sin(rad);
    double minx = 1e18, maxx = -1e18, miny = 1e18, maxy = -1e18;
    for (int i = 0; i < 4; ++i) {
        double px = info.hitOffX + ((i & 1) ? info.hitW : 0);
        double py = info.hitOffY + ((i & 2) ? info.hitH : 0);
        if (o.flipX) px = -px;
        if (o.flipY) py = -py;
        const double rx = px * c + py * s, ry = -px * s + py * c;       // clockwise rotation, y-up
        minx = std::min(minx, rx); maxx = std::max(maxx, rx);
        miny = std::min(miny, ry); maxy = std::max(maxy, ry);
    }
    return {(float)(o.x + minx), (float)(o.x + maxx), (float)(o.y + miny), (float)(o.y + maxy)};
}

bool collidable(ObjKind k) {
    switch (k) {
    case ObjKind::Solid: case ObjKind::Hazard: case ObjKind::PortalCube: case ObjKind::PortalShip:
    case ObjKind::PortalGravityDown: case ObjKind::PortalGravityUp: case ObjKind::Pad: case ObjKind::Orb:
        return true;
    default: return false;
    }
}

} // namespace

Simulation::Simulation(const Level& level, const SimConfig& cfg) : level_(&level), cfg_(cfg) {
    auto boxes = std::make_shared<std::vector<Box>>(level.objects.size(), Box{0, 0, 0, 0});
    for (size_t i = 0; i < level.objects.size(); ++i) {
        const LevelObject& o = level.objects[i];
        const ObjInfo* info = objectInfo(o.id);
        if (info && collidable(info->kind)) (*boxes)[i] = makeBox(o, *info);
    }
    boxes_ = boxes;
    endX_ = std::max(570.0, (double)level.maxX());      // the original: x of the last object, at least 570
    reset();
}

void Simulation::reset() {
    p_ = SimPlayer();
    p_.x = cfg_.startX;
    if (cfg_.startAsShip) p_.mode = PlayMode::Ship;
    ly_ = cfg_.playerSize / 2;
    p_.y = ly_;
    lvy_ = 0;
    rot_ = 0;
    onGround_ = true;
    rising_ = false;
    holding_ = queuedHold_ = touchedRing_ = false;
    jumps_ = 0;
    used_.assign(level_->objects.size(), 0);
}

void Simulation::setHolding(bool h) {
    if (h && !holding_) queuedHold_ = true;             // pushButton
    if (!h) queuedHold_ = false;                         // releaseButton
    holding_ = h;
}

double Simulation::progress() const {
    return std::max(0.0, std::min(1.0, p_.x / endX_));
}

Simulation::Box Simulation::localBox(const Box& b) const {
    if (!p_.mirrored) return b;
    return {b.minx, b.maxx, (float)cfg_.planeHeight - b.maxy, (float)cfg_.planeHeight - b.miny};
}

Simulation::Box Simulation::outerBox() const {
    const float r = (float)cfg_.playerSize / 2;
    return {(float)p_.x - r, (float)p_.x + r, (float)ly_ - r, (float)ly_ + r};
}

Simulation::Box Simulation::innerBox() const {
    const float r = (float)cfg_.innerSize / 2;
    return {(float)p_.x - r, (float)p_.x + r, (float)ly_ - r, (float)ly_ + r};
}

void Simulation::setMode(PlayMode m) {
    if (p_.mode == m) return;
    p_.mode = m;
    if (m == PlayMode::Ship) lvy_ /= 2.0;                // setGamemode(Ship): m_dYVel /= 2
    else rot_ = 0;
    onGround_ = false;
}

void Simulation::flipGravity(bool up) {
    if (p_.mirrored == up) return;
    p_.mirrored = up;
    ly_ = cfg_.planeHeight - ly_;
    lvy_ = -lvy_ / 2.0;                                   // flipGravity: m_dYVel /= 2 (world sign is kept)
}

// PlayerObject::updateJump. dtSlow = dt * 0.9
void Simulation::updateJump(double dt) {
    const double g = cfg_.gravity;
    if (p_.mode == PlayMode::Ship) {
        double accel = 0.8;
        if (holding_) accel = -1.0;
        if (!holding_ && !falling()) accel = 1.2;
        double boost = 0.4;
        if (holding_ && falling()) boost = 0.5;
        lvy_ -= g * dt * accel * boost * cfg_.shipScale;
        lvy_ = std::max(-cfg_.shipDownLimit, std::min(cfg_.shipUpLimit, lvy_));
        return;
    }
    if (holding_ && onGround_) {
        rising_ = true;
        onGround_ = false;
        lvy_ = cfg_.jump;
        ++jumps_;
        if (!touchedRing_) queuedHold_ = false;
    } else if (rising_) {
        lvy_ -= g * dt;
        if (falling()) { rising_ = false; onGround_ = false; }
    } else {
        if (lvy_ < -g * 2.0) onGround_ = false;          // leaving a platform: ~2 frames of "coyote time"
        lvy_ -= g * dt;
        lvy_ = std::max(lvy_, -cfg_.maxFall);
    }
}

// PlayerObject::collidedWithObject for a solid block `s` (local space).
void Simulation::collideSolid(const Box& s) {
    const bool ship = p_.mode == PlayMode::Ship;
    const double mod = ship ? cfg_.shipLandMod : cfg_.cubeLandMod;
    const double half = cfg_.playerSize / 2;

    if (lvy_ < 0.0 && ly_ >= s.maxy + mod) {                      // falling onto the top
        ly_ = s.maxy + half;
        hitGround();
    } else if (ship && lvy_ > 0.0 && ly_ <= s.miny + 24.0 && ly_ <= (s.miny + s.maxy) / 2 + half) {   // ship bumps its head
        ly_ = s.miny - half;
        hitGround();
    }
    if (touches(innerBox(), s)) die();                            // the small inner box decides death
}

void Simulation::collide() {
    const double half = cfg_.playerSize / 2;
    if (p_.mode == PlayMode::Cube) {
        if (ly_ < half) {                                          // floor (local)
            if (p_.mirrored) { /* local floor = world ceiling plane: land on it */ }
            ly_ = half;
            hitGround();
        }
        if (p_.mirrored && ly_ > cfg_.planeHeight - half) { die(); return; }   // flipped cube reaching the world floor
    } else {
        if (ly_ < cfg_.shipFloor) { ly_ = cfg_.shipFloor; lvy_ = 0; if (!p_.mirrored) onGround_ = true; }
        const double top = cfg_.planeHeight - cfg_.shipFloor;
        if (ly_ > top) { ly_ = top; lvy_ = 0; }
    }

    touchedRing_ = false;
    auto r = level_->range((float)p_.x - 60.f, (float)p_.x + 60.f);
    const Box outer = outerBox();
    for (size_t i = r.first; i < r.second; ++i) {
        const ObjInfo* info = objectInfo(level_->objects[i].id);
        if (!info) continue;
        switch (info->kind) {
        case ObjKind::Hazard:
            if (touches(outerBox(), localBox((*boxes_)[i]))) { die(); return; }
            break;
        case ObjKind::Solid: {
            const Box s = localBox((*boxes_)[i]);
            if (touches(outerBox(), s)) { collideSolid(s); if (p_.dead) return; }
            break;
        }
        case ObjKind::PortalCube:
            if (touches(outer, localBox((*boxes_)[i]))) setMode(PlayMode::Cube);
            break;
        case ObjKind::PortalShip:
            if (touches(outer, localBox((*boxes_)[i]))) setMode(PlayMode::Ship);
            break;
        case ObjKind::PortalGravityDown:
            if (touches(outer, localBox((*boxes_)[i]))) flipGravity(false);
            break;
        case ObjKind::PortalGravityUp:
            if (touches(outer, localBox((*boxes_)[i]))) flipGravity(true);
            break;
        case ObjKind::Pad:
            if (!used_[i] && touches(outer, localBox((*boxes_)[i]))) {
                used_[i] = 1;
                rising_ = true;                                    // propellPlayer(1)
                onGround_ = false;
                lvy_ = cfg_.padBoost;
            }
            break;
        case ObjKind::Orb:
            if (!used_[i] && touches(outer, localBox((*boxes_)[i]))) {
                touchedRing_ = true;
                if (queuedHold_ && holding_) {                    // ringJump
                    used_[i] = 1;
                    rising_ = true;
                    queuedHold_ = false;
                    onGround_ = false;
                    lvy_ = cfg_.ringBoost;
                    ++jumps_;
                }
            }
            break;
        default: break;
        }
    }
    if (p_.mode == PlayMode::Ship) queuedHold_ = false;
}

void Simulation::substep(double h) {
    const double dtSlow = h * 0.9;
    updateJump(dtSlow);
    ly_ += dtSlow * lvy_;
    p_.x += h * cfg_.xVel * cfg_.speed;
    collide();
    if (p_.dead) return;

    if (p_.mode == PlayMode::Cube) {
        if (!onGround_) rot_ += cfg_.rotateDegPerFrame * h;
        else rot_ = 90.0 * std::round(rot_ / 90.0);
    } else {
        rot_ = -std::atan2(lvy_, cfg_.xVel) * 180.0 / kPi;
    }
    if (p_.x >= endX_) p_.finished = true;
}

void Simulation::step(double dtFrames) {
    if (p_.dead || p_.finished) return;
    if (dtFrames > 2.0) dtFrames = 2.0;
    if (dtFrames <= 0) return;
    const double h = dtFrames / 4.0;
    for (int i = 0; i < 4 && !p_.dead && !p_.finished; ++i) substep(h);

    p_.onGround = onGround_;
    p_.y = p_.mirrored ? cfg_.planeHeight - ly_ : ly_;
    p_.vy = p_.mirrored ? -lvy_ : lvy_;
    p_.rotation = p_.mirrored ? -rot_ : rot_;
}

} // namespace ogd
