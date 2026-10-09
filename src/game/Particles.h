// cocos2d particle emitters (.plist files from the APK: explodeEffect, landEffect, portalEffect01..04 ...).
// Follows CCParticleSystem: gravity mode (emitterType 0) and radius mode (emitterType 1), colour / size /
// rotation interpolation over the lifetime, duration, "free" position type (particles stay in the world).
// The plist's sourcePosition is the emitter node's position in the original, so it is ignored here.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "engine/Engine.h"

namespace ogd {

struct EmitterDef {
    bool ok = false;
    int type = 0;                  // 0 gravity, 1 radius
    float duration = -1, life = 1, lifeVar = 0;
    int maxParticles = 0;
    float angle = 0, angleVar = 0;
    float speed = 0, speedVar = 0;
    float gx = 0, gy = 0;
    float radialAccel = 0, radialAccelVar = 0, tangAccel = 0, tangAccelVar = 0;
    float maxRadius = 0, maxRadiusVar = 0, minRadius = 0, minRadiusVar = 0;
    float rotPerSec = 0, rotPerSecVar = 0;
    float startSize = 0, startSizeVar = 0, endSize = -1, endSizeVar = 0;
    float startSpin = 0, startSpinVar = 0, endSpin = 0, endSpinVar = 0;
    float sc[4]{1, 1, 1, 1}, scv[4]{}, ec[4]{1, 1, 1, 1}, ecv[4]{};   // start / end colour (+variance), rgba 0..1
    float posVarX = 0, posVarY = 0;
    bool additive = true;          // blendFunc 770 / 1
    std::string texture = "square.png";
};

const EmitterDef& emitterDef(const std::string& plistName);   // cached; ok=false when missing

class Emitter {
public:
    Emitter(const std::string& plist, float x, float y, Color tint = {255, 255, 255}, float scale = 2.f);
    void setPos(float x, float y) { x_ = x; y_ = y; }
    void stop() { emitting_ = false; }                 // let living particles fade out
    void start() { emitting_ = true; }
    bool emitting() const { return emitting_; }
    bool finished() const { return !emitting_ && ps_.empty(); }
    void update(float dt);
    void draw(float camX, float camY) const;
    size_t count() const { return ps_.size(); }

private:
    struct P {
        float x, y, ox, oy;                 // position, emission origin
        float vx, vy, radialA, tangA;
        float angle, radius, radiusDelta, spin;   // radius mode
        float ttl, life;
        float r, g, b, a, dr, dg, db, da;
        float size, dsize, rot, drot;
    };
    void add();
    EmitterDef def_;
    float x_, y_, scale_, elapsed_ = 0, counter_ = 0;
    Color tint_;
    bool emitting_ = true;
    std::vector<P> ps_;
};

// Fire-and-forget emitters (death burst, landing, pad hit, fireworks ...).
class ParticleSet {
public:
    Emitter* add(const std::string& plist, float x, float y, Color tint = {255, 255, 255});
    void update(float dt);
    void draw(float camX, float camY) const;
    void clear() { list_.clear(); }

private:
    std::vector<std::unique_ptr<Emitter>> list_;
};

} // namespace ogd
