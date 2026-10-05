// Object id -> behaviour, sprite, draw order and hitbox for GD 1.x levels (no SDL dependency).
//
// Source of truth: the OpenGD fork (Content/Custom/object.json for types / sprites / z-order and Source/LongData.cpp for
// hitboxes). Hitboxes are {w, h, offsetX, offsetY} in GD units relative to the object centre, before flip/rotation.
//   * solid blocks: 1,2,3,4,6,7 (30x30) and 40 (30x14 slab); id 5 is DECORATION (the black fill block)
//   * hazards: 8 spike (6x12), 39 small spike (6x5.6), 9 ground thorns (9x10.8)
//   * decoration (no collision): 5, 15-17 (rods), 18-21 (white thorn tops), 41 (chain)
//   * portals 10 (gravity down) 11 (gravity up) 12 (cube) 13 (ship), pad 35, ring 36, colour triggers 29/30
//   * 22-28, 32, 33 are invisible fade/trail triggers: ignored
#pragma once
#include <cstdint>

namespace ogd {

enum class ObjKind : uint8_t {
    Ignored,       // invisible trigger or unknown: not drawn, no collision
    Deco,          // drawn only
    Solid,         // blocks and slabs
    Hazard,        // spikes, thorns: touching = death
    PortalCube, PortalShip, PortalGravityDown, PortalGravityUp,
    Pad,           // yellow jump pad
    Orb,           // yellow jump ring (needs a tap)
    TriggerBG, TriggerGround,
};

// Draw order = layer * 100 + z-order (same keys as the original). The player is drawn at kObjZPlayer: everything
// above it (portal fronts) is drawn after the player.
constexpr int kObjZPlayer = 506;

struct ObjInfo {
    int id;
    ObjKind kind;
    const char* sprite;      // sprite frame name, nullptr = not drawn
    const char* spriteBack;  // drawn behind the player (portals), may be nullptr
    int z;                   // layer * 100 + z-order
    bool tintMain;           // tinted with the player's main colour (detail objects)
    // hitbox in GD units, relative to the object centre, BEFORE flip/rotation
    float hitW, hitH, hitOffX, hitOffY;
};

// nullptr for ids that are not in the table at all.
const ObjInfo* objectInfo(int id);

} // namespace ogd
