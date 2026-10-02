#include "core/Sim.h"

#include <algorithm>
#include <cmath>

namespace ogd {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr float kEps = 0.01f;

bool overlaps(const Simulation::Box& a, const Simulation::Box& b) {
    return a.minx < b.maxx - kEps && a.maxx > b.minx + kEps && a.miny < b.maxy - kEps && a.maxy > b.miny + kEps;
}

Simulation::Box makeBox(const LevelObject& o, const ObjInfo& info) {
    // centre offset, flipped then rotated clockwise (y-up)
    double ox = info.hitOffX * (o.flipX ? -1 : 1), oy = info.hitOffY * (o.flipY ? -1 : 1);
    const double rad = o.rotation * kPi / 180.0;
    const double c = std::cos(rad), s = std::sin(rad);
    const double rx = ox * c + oy * s, ry = -ox * s + oy * c;
    // AABB of the rotated rectangle (exact for multiples of 90 degrees)
    double w = std::fabs(info.hitW * c) + std::fabs(info.hitH * s);
    double h = std::fabs(info.hitW * s) + std::fabs(info.hitH * c);
    const double cx = o.x + rx, cy = o.y + ry;
    return {(float)(cx - w / 2), (float)(cx + w / 2), (float)(cy - h / 2), (float)(cy + h / 2)};
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
    endX_ = level.maxX() + cfg_.endPadding;
    reset();
}

void Simulation::reset() {
    p_ = SimPlayer();
    p_.x = cfg_.startX;
    if (cfg_.startAsShip) p_.mode = PlayMode::Ship;
    p_.y = cfg_.playerSize / 2;
    ly_ = p_.y;
    lvy_ = 0;
    rot_ = 0;
    holding_ = pressPending_ = false;
    used_.assign(level_->objects.size(), 0);
}

void Simulation::setHolding(bool h) {
    if (h && !holding_) pressPending_ = true;
    holding_ = h;
}

double Simulation::progress() const {
    return std::max(0.0, std::min(1.0, (p_.x - cfg_.startX) / (endX_ - cfg_.startX)));
}

Simulation::Box Simulation::localBox(const Box& b) const {
    if (!p_.mirrored) return b;
    return {b.minx, b.maxx, (float)cfg_.planeHeight - b.maxy, (float)cfg_.planeHeight - b.miny};
}

Simulation::Box Simulation::bodyBox() const {
    const float r = (float)cfg_.playerSize / 2;
    return {(float)p_.x - r, (float)p_.x + r, (float)ly_ - r, (float)ly_ + r};
}

Simulation::Box Simulation::hazardBox() const {
    const float r = (float)cfg_.hazardBox / 2;
    return {(float)p_.x - r, (float)p_.x + r, (float)ly_ - r, (float)ly_ + r};
}

void Simulation::flipGravity(bool up) {
    if (p_.mirrored == up) return;
    p_.mirrored = up;
    ly_ = cfg_.planeHeight - ly_;
    lvy_ = -lvy_;
    p_.onGround = false;
}

bool Simulation::supported() const {
    const double bottom = ly_ - cfg_.playerSize / 2;
    if (bottom <= 0.001) return true;
    auto r = level_->range((float)p_.x - 45.f, (float)p_.x + 45.f);
    const Box body = bodyBox();
    for (size_t i = r.first; i < r.second; ++i) {
        const ObjInfo* info = objectInfo(level_->objects[i].id);
        if (!info || info->kind != ObjKind::Solid) continue;
        const Box s = localBox((*boxes_)[i]);
        if (body.minx < s.maxx - kEps && body.maxx > s.minx + kEps && std::fabs(bottom - s.maxy) < 0.05) return true;
    }
    return false;
}

// Returns false if the player died. prevLocalY = local centre y before this substep's vertical move.
bool Simulation::resolveSolids(double prevLocalY) {
    const double half = cfg_.playerSize / 2;
    const double prevBottom = prevLocalY - half, prevTop = prevLocalY + half;
    auto r = level_->range((float)p_.x - 45.f, (float)p_.x + 45.f);
    for (size_t i = r.first; i < r.second; ++i) {
        const ObjInfo* info = objectInfo(level_->objects[i].id);
        if (!info || info->kind != ObjKind::Solid) continue;
        const Box s = localBox((*boxes_)[i]);
        Box body = bodyBox();
        const float in = (float)cfg_.solidInset;
        body = {body.minx + in, body.maxx - in, body.miny + in, body.maxy - in};
        if (!overlaps(body, s)) continue;

        if (prevBottom >= s.maxy - cfg_.landSlack && lvy_ <= 0.0001) {          // landed on top
            ly_ = s.maxy + half;
            lvy_ = 0;
            p_.onGround = true;
        } else if (p_.mode == PlayMode::Ship && prevTop <= s.miny + cfg_.landSlack && lvy_ >= -0.0001) {  // bumped head
            ly_ = s.miny - half;
            if (lvy_ > 0) lvy_ = 0;
        } else {                                                                   // side / underside hit
            die();
            return false;
        }
    }
    return true;
}

bool Simulation::touchesHazard() const {
    auto r = level_->range((float)p_.x - 45.f, (float)p_.x + 45.f);
    const Box hb = hazardBox();
    for (size_t i = r.first; i < r.second; ++i) {
        const ObjInfo* info = objectInfo(level_->objects[i].id);
        if (!info || info->kind != ObjKind::Hazard) continue;
        if (overlaps(hb, localBox((*boxes_)[i]))) return true;
    }
    return false;
}

void Simulation::touchTriggers() {
    auto r = level_->range((float)p_.x - 45.f, (float)p_.x + 45.f);
    // portals / pads / orbs use the world-space body box
    Box body = {(float)p_.x - 15.f, (float)p_.x + 15.f, 0, 0};
    const double wy = p_.mirrored ? cfg_.planeHeight - ly_ : ly_;
    body.miny = (float)wy - 15.f;
    body.maxy = (float)wy + 15.f;
    for (size_t i = r.first; i < r.second; ++i) {
        const ObjInfo* info = objectInfo(level_->objects[i].id);
        if (!info) continue;
        const Box& b = (*boxes_)[i];
        switch (info->kind) {
        case ObjKind::PortalCube:
            if (overlaps(body, b)) { p_.mode = PlayMode::Cube; }
            break;
        case ObjKind::PortalShip:
            if (overlaps(body, b)) { p_.mode = PlayMode::Ship; }
            break;
        case ObjKind::PortalGravityDown:
            if (overlaps(body, b)) flipGravity(false);
            break;
        case ObjKind::PortalGravityUp:
            if (overlaps(body, b)) flipGravity(true);
            break;
        case ObjKind::Pad:
            if (!used_[i] && overlaps(body, b)) {
                used_[i] = 1;
                lvy_ = (p_.mirrored ? 1 : 1) * cfg_.padBoost;   // local space: always "up" away from the floor
                p_.onGround = false;
            }
            break;
        case ObjKind::Orb:
            if (pressPending_ && !used_[i] && overlaps(body, b)) {
                used_[i] = 1;
                pressPending_ = false;
                lvy_ = cfg_.orbBoost;
                p_.onGround = false;
            }
            break;
        default: break;
        }
    }
}

void Simulation::cubeMove(double h) {
    const double jdt = h * cfg_.speed;
    if (p_.onGround && !supported()) p_.onGround = false;
    if (p_.onGround && holding_) {
        p_.onGround = false;
        lvy_ = cfg_.jump;
    } else if (!p_.onGround) {
        lvy_ = std::max(-cfg_.maxFall, lvy_ - cfg_.gravity * jdt);
    }
    p_.x += h * cfg_.speed * cfg_.xVel;
    const double prev = ly_;
    ly_ += h * cfg_.speed * lvy_;

    if (ly_ - cfg_.playerSize / 2 < 0) {           // floor
        ly_ = cfg_.playerSize / 2;
        if (lvy_ < 0) lvy_ = 0;
        p_.onGround = true;
    }
    if (!resolveSolids(prev)) return;
    if (!p_.onGround) rot_ += cfg_.rotateDegPerFrame * h;
    else rot_ = 90.0 * std::round(rot_ / 90.0);
}

void Simulation::shipMove(double h) {
    const double jdt = h * cfg_.speed;
    lvy_ += (holding_ ? cfg_.shipAccel : -cfg_.shipAccel) * jdt;
    lvy_ = std::max(-cfg_.shipMaxV, std::min(cfg_.shipMaxV, lvy_));
    p_.x += h * cfg_.speed * cfg_.xVel;
    const double prev = ly_;
    ly_ += h * cfg_.speed * lvy_;

    const double half = cfg_.playerSize / 2;
    if (ly_ - half < 0) { ly_ = half; if (lvy_ < 0) lvy_ = 0; }
    if (ly_ + half > cfg_.planeHeight) { ly_ = cfg_.planeHeight - half; if (lvy_ > 0) lvy_ = 0; }
    p_.onGround = false;
    if (!resolveSolids(prev)) return;
    rot_ = -std::atan2(lvy_, cfg_.xVel) * 180.0 / kPi;
}

void Simulation::substep(double h) {
    if (p_.mode == PlayMode::Cube) cubeMove(h); else shipMove(h);
    if (p_.dead) return;
    touchTriggers();
    if (touchesHazard()) { die(); return; }
    if (p_.x >= endX_) p_.finished = true;
}

void Simulation::step(double dtFrames) {
    if (p_.dead || p_.finished) return;
    if (dtFrames > 2.0) dtFrames = 2.0;
    if (dtFrames <= 0) return;
    const double h = dtFrames / 4.0;
    for (int i = 0; i < 4 && !p_.dead && !p_.finished; ++i) substep(h);
    pressPending_ = false;

    // world-space view of the local state
    p_.y = p_.mirrored ? cfg_.planeHeight - ly_ : ly_;
    p_.vy = p_.mirrored ? -lvy_ : lvy_;
    p_.rotation = p_.mirrored ? -rot_ : rot_;
}

} // namespace ogd
