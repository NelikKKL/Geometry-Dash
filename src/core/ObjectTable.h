// Object id -> behaviour + sprite table for GD 1.x levels (no SDL dependency).
//
// How each entry was established (see README "Object table"):
//   * ids 1-7 map to square_01..07 in order; 8 = spike_01, 39 = spike_02 (small), 40 = plank_01 (slab)
//   * ids 10-13 = portal_01..04 (blue/yellow gravity, green cube, pink ship) - by sprite colour and by the order
//     in which they appear inside the levels; 35 = bump (pad), 36 = ring (orb): Stereo Madness has none of them,
//     Back on Track has pads, which matches the original game
//   * ids 18-21 = d_spikes_01..04: they always sit directly above a row of id 9 (black ground thorns).
//     id 9 is DECORATION: in Base After Base it covers the whole floor of stretches that must be run on
// Ids marked "unmapped" are decoration that occurs in the data but could not be matched to a sprite with
// confidence; they are skipped by the renderer (and never collide).
#pragma once
#include <cstdint>

namespace ogd {

enum class ObjKind : uint8_t {
    Unmapped,      // known to exist, not drawn, no collision
    Deco,          // drawn only
    Solid,         // blocks and slabs
    Hazard,        // spikes, pits: touching = death
    PortalCube, PortalShip, PortalGravityDown, PortalGravityUp,
    Pad,           // yellow jump pad
    Orb,           // yellow jump orb (needs a tap while overlapping)
    TriggerBG, TriggerGround,
};

// Objects with z >= kObjZFront are drawn in front of the player (portal fronts, pads, orbs).
constexpr int kObjZFront = 6;

struct ObjInfo {
    int id;
    ObjKind kind;
    const char* sprite;      // sprite frame name, nullptr = not drawn
    const char* spriteBack;  // drawn behind the player (portals), may be nullptr
    int z;                   // draw order, low first
    // hitbox in GD units, relative to the object centre, BEFORE flip/rotation
    float hitW, hitH, hitOffX, hitOffY;
};

// nullptr for ids that are not in the table at all.
const ObjInfo* objectInfo(int id);

} // namespace ogd
