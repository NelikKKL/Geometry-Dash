#include "game/Particles.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <random>

#include "core/Plist.h"

namespace ogd {
namespace {

std::mt19937& rng() {
    static std::mt19937 g(12345);
    return g;
}
float rnd() {   // RANDOM_M11
    return std::uniform_real_distribution<float>(-1.f, 1.f)(rng());
}
float clamp01(float v) { return std::max(0.f, std::min(1.f, v)); }
constexpr float kDeg = 3.14159265358979f / 180.f;

} // namespace

const EmitterDef& emitterDef(const std::string& name) {
    static std::map<std::string, EmitterDef> cache;
    auto it = cache.find(name);
    if (it != cache.end()) return it->second;
    EmitterDef d;
    std::string xml;
    if (E().readText(name, xml)) {
        PValue v = parsePlist(xml);
        if (v.isDict()) {
            auto n = [&](const char* k, float def = 0.f) { const PValue* p = v.get(k); return p ? (float)p->num() : def; };
            d.ok = true;
            d.type = (int)n("emitterType");
            d.duration = n("duration", -1.f);
            d.life = n("particleLifespan"); d.lifeVar = n("particleLifespanVariance");
            d.maxParticles = (int)n("maxParticles");
            d.angle = n("angle"); d.angleVar = n("angleVariance");
            d.speed = n("speed"); d.speedVar = n("speedVariance");
            d.gx = n("gravityx"); d.gy = n("gravityy");
            d.radialAccel = n("radialAcceleration"); d.radialAccelVar = n("radialAccelVariance");
            d.tangAccel = n("tangentialAcceleration"); d.tangAccelVar = n("tangentialAccelVariance");
            d.maxRadius = n("maxRadius"); d.maxRadiusVar = n("maxRadiusVariance");
            d.minRadius = n("minRadius"); d.minRadiusVar = n("minRadiusVariance");
            d.rotPerSec = n("rotatePerSecond"); d.rotPerSecVar = n("rotatePerSecondVariance");
            d.startSize = n("startParticleSize"); d.startSizeVar = n("startParticleSizeVariance");
            d.endSize = n("finishParticleSize", -1.f); d.endSizeVar = n("finishParticleSizeVariance");
            d.startSpin = n("rotationStart"); d.startSpinVar = n("rotationStartVariance");
            d.endSpin = n("rotationEnd"); d.endSpinVar = n("rotationEndVariance");
            const char* ch[4] = {"Red", "Green", "Blue", "Alpha"};
            for (int i = 0; i < 4; ++i) {
                d.sc[i] = n((std::string("startColor") + ch[i]).c_str(), 1.f);
                d.scv[i] = n((std::string("startColorVariance") + ch[i]).c_str());
                d.ec[i] = n((std::string("finishColor") + ch[i]).c_str(), 1.f);
                d.ecv[i] = n((std::string("finishColorVariance") + ch[i]).c_str());
            }
            d.posVarX = n("sourcePositionVariancex"); d.posVarY = n("sourcePositionVariancey");
            d.additive = !(n("blendFuncDestination", 1.f) == 771.f);
            const PValue* t = v.get("textureFileName");
            if (t && !t->str().empty()) d.texture = t->str();
        }
    }
    return cache.emplace(name, d).first->second;
}

Emitter::Emitter(const std::string& plist, float x, float y, Color tint, float scale)
    : def_(emitterDef(plist)), x_(x), y_(y), scale_(scale), tint_(tint) {
    if (!def_.ok) emitting_ = false;
}

void Emitter::add() {
    P p{};
    const float ttl = std::max(0.f, def_.life + def_.lifeVar * rnd());
    if (ttl <= 0.f) { return; }                      // CCParticleSystem::initParticle: dead on arrival
    p.ttl = p.life = ttl;
    p.ox = p.x = x_ + def_.posVarX * scale_ * rnd();
    p.oy = p.y = y_ + def_.posVarY * scale_ * rnd();
    float c0[4], c1[4];
    for (int i = 0; i < 4; ++i) {
        c0[i] = clamp01(def_.sc[i] + def_.scv[i] * rnd());
        c1[i] = clamp01(def_.ec[i] + def_.ecv[i] * rnd());
    }
    p.r = c0[0]; p.g = c0[1]; p.b = c0[2]; p.a = c0[3];
    p.dr = (c1[0] - c0[0]) / ttl; p.dg = (c1[1] - c0[1]) / ttl;
    p.db = (c1[2] - c0[2]) / ttl; p.da = (c1[3] - c0[3]) / ttl;
    p.size = std::max(0.f, def_.startSize + def_.startSizeVar * rnd());
    const float es = def_.endSize < 0.f ? p.size : std::max(0.f, def_.endSize + def_.endSizeVar * rnd());
    p.dsize = (es - p.size) / ttl;
    p.rot = def_.startSpin + def_.startSpinVar * rnd();
    p.drot = (def_.endSpin + def_.endSpinVar * rnd() - p.rot) / ttl;
    const float a = (def_.angle + def_.angleVar * rnd()) * kDeg;
    if (def_.type == 0) {
        const float s = def_.speed + def_.speedVar * rnd();
        p.vx = std::cos(a) * s * scale_; p.vy = std::sin(a) * s * scale_;
        p.radialA = def_.radialAccel + def_.radialAccelVar * rnd();
        p.tangA = def_.tangAccel + def_.tangAccelVar * rnd();
    } else {
        const float r0 = def_.maxRadius + def_.maxRadiusVar * rnd();
        const float r1 = def_.minRadius + def_.minRadiusVar * rnd();
        p.radius = r0; p.radiusDelta = (r1 - r0) / ttl;
        p.angle = a;
        p.spin = (def_.rotPerSec + def_.rotPerSecVar * rnd()) * kDeg;
    }
    ps_.push_back(p);
}

void Emitter::update(float dt) {
    if (emitting_ && (int)ps_.size() < def_.maxParticles) {
        // emit rate = total / lifespan; a lifespan of 0 fills the whole pool immediately
        const float rate = def_.life > 0.f ? def_.maxParticles / def_.life : 1e9f;
        counter_ += dt;
        const float per = 1.f / rate;
        int guard = def_.maxParticles;
        while ((int)ps_.size() < def_.maxParticles && counter_ > per && guard-- > 0) {
            const size_t before = ps_.size();
            add();
            if (ps_.size() == before && def_.life <= 0.f && def_.lifeVar <= 0.f) break;
            counter_ -= per;
        }
        if (def_.life <= 0.f) counter_ = 0.f;
    }
    if (emitting_) {
        elapsed_ += dt;
        if (def_.duration >= 0.f && elapsed_ > def_.duration) emitting_ = false;
    }
    for (size_t i = 0; i < ps_.size();) {
        P& p = ps_[i];
        p.ttl -= dt;
        if (p.ttl <= 0.f) { ps_[i] = ps_.back(); ps_.pop_back(); continue; }
        if (def_.type == 0) {
            float rx = p.x - p.ox, ry = p.y - p.oy;
            const float len = std::sqrt(rx * rx + ry * ry);
            if (len > 0.0001f) { rx /= len; ry /= len; }
            const float tx = -ry, ty = rx;
            p.vx += (rx * p.radialA + tx * p.tangA + def_.gx) * scale_ * dt;
            p.vy += (ry * p.radialA + ty * p.tangA + def_.gy) * scale_ * dt;
            p.x += p.vx * dt; p.y += p.vy * dt;
        } else {
            p.angle += p.spin * dt;
            p.radius += p.radiusDelta * dt;
            p.x = p.ox - std::cos(p.angle) * p.radius * scale_;
            p.y = p.oy - std::sin(p.angle) * p.radius * scale_;
        }
        p.r += p.dr * dt; p.g += p.dg * dt; p.b += p.db * dt; p.a += p.da * dt;
        p.size = std::max(0.f, p.size + p.dsize * dt);
        p.rot += p.drot * dt;
        ++i;
    }
}

void Emitter::draw(float camX, float camY) const {
    if (ps_.empty()) return;
    Sprite s = E().sprite(def_.texture);
    if (!s) s = E().sprite("square.png");        // starParticle.png / stone_debris.png are not shipped in the APK
    if (!s || s.w <= 0) return;
    for (const P& p : ps_) {
        const float k = p.size * scale_ / s.w;
        if (k <= 0.f) continue;
        const Color c{(Uint8)(clamp01(p.r) * tint_.r), (Uint8)(clamp01(p.g) * tint_.g), (Uint8)(clamp01(p.b) * tint_.b)};
        E().drawSprite(s, p.x - camX, p.y - camY, k, k, p.rot, c, (Uint8)(clamp01(p.a) * 255), false, false,
                       def_.additive);
    }
}

Emitter* ParticleSet::add(const std::string& plist, float x, float y, Color tint, float scale) {
    list_.push_back(std::make_unique<Emitter>(plist, x, y, tint, scale));
    return list_.back().get();
}

void ParticleSet::update(float dt) {
    for (auto& e : list_) e->update(dt);
    list_.erase(std::remove_if(list_.begin(), list_.end(), [](const std::unique_ptr<Emitter>& e) { return e->finished(); }),
                list_.end());
}

void ParticleSet::draw(float camX, float camY) const {
    for (auto& e : list_) e->draw(camX, camY);
}

} // namespace ogd
