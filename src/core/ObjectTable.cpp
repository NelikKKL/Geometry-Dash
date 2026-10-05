#include "core/ObjectTable.h"

namespace ogd {
namespace {

//          id kind                 sprite                         back                      z    tint   w    h   ox     oy
const ObjInfo kTable[] = {
    {1,  ObjKind::Solid,  "square_01_001.png", nullptr, 502, false, 30, 30, -15, -15},
    {2,  ObjKind::Solid,  "square_02_001.png", nullptr, 502, false, 30, 30, -15, -15},
    {3,  ObjKind::Solid,  "square_03_001.png", nullptr, 502, false, 30, 30, -15, -15},
    {4,  ObjKind::Solid,  "square_04_001.png", nullptr, 502, false, 30, 30, -15, -15},
    {5,  ObjKind::Deco,   "square_05_001.png", nullptr, 93,  false, 0,  0,  0,   0},    // layer 1, z -7
    {6,  ObjKind::Solid,  "square_06_001.png", nullptr, 502, false, 30, 30, -15, -15},
    {7,  ObjKind::Solid,  "square_07_001.png", nullptr, 502, false, 30, 30, -15, -15},
    {8,  ObjKind::Hazard, "spike_01_001.png",  nullptr, 502, false, 6,  12, -3,  -6},
    {9,  ObjKind::Hazard, "pit_02_001.png",    nullptr, 502, false, 9,  10.8f, -4.5f, -5.4f},
    {39, ObjKind::Hazard, "spike_02_001.png",  nullptr, 502, false, 6,  5.6f, -3, -2.8f},
    {40, ObjKind::Solid,  "plank_01_001.png",  nullptr, 502, false, 30, 14, -15, -7},

    {10, ObjKind::PortalGravityDown, "portal_01_front_001.png", "portal_01_back_001.png", 510, false, 25, 75, -12.5f, -37.5f},
    {11, ObjKind::PortalGravityUp,   "portal_02_front_001.png", "portal_02_back_001.png", 510, false, 25, 75, -12.5f, -37.5f},
    {12, ObjKind::PortalCube,        "portal_03_front_001.png", "portal_03_back_001.png", 510, false, 34, 86, -17,    -43},
    {13, ObjKind::PortalShip,        "portal_04_front_001.png", "portal_04_back_001.png", 510, false, 34, 86, -17,    -43},

    {35, ObjKind::Pad, "bump_01_001.png", nullptr, 312, false, 25, 4, -12.5f, -2},
    {36, ObjKind::Orb, "ring_01_001.png", nullptr, 312, false, 36, 36, -18, -18},

    // decoration: rods behind everything, white thorn tops and chains in the detail layer
    {15, ObjKind::Deco, "rod_01_001.png",      nullptr, 94, false, 0, 0, 0, 0},
    {16, ObjKind::Deco, "rod_02_001.png",      nullptr, 94, false, 0, 0, 0, 0},
    {17, ObjKind::Deco, "rod_03_001.png",      nullptr, 94, false, 0, 0, 0, 0},
    {18, ObjKind::Deco, "d_spikes_01_001.png", nullptr, 309, true,  0, 0, 0, 0},
    {19, ObjKind::Deco, "d_spikes_02_001.png", nullptr, 309, true,  0, 0, 0, 0},
    {20, ObjKind::Deco, "d_spikes_03_001.png", nullptr, 309, true,  0, 0, 0, 0},
    {21, ObjKind::Deco, "d_spikes_04_001.png", nullptr, 309, true,  0, 0, 0, 0},
    {41, ObjKind::Deco, "chain_01_001.png",    nullptr, 309, true,  0, 0, 0, 0},

    {29, ObjKind::TriggerBG,     nullptr, nullptr, 0, false, 0, 0, 0, 0},
    {30, ObjKind::TriggerGround, nullptr, nullptr, 0, false, 0, 0, 0, 0},

    // invisible fade / trail triggers
    {22, ObjKind::Ignored, nullptr, nullptr, 0, false, 0, 0, 0, 0}, {23, ObjKind::Ignored, nullptr, nullptr, 0, false, 0, 0, 0, 0},
    {24, ObjKind::Ignored, nullptr, nullptr, 0, false, 0, 0, 0, 0}, {25, ObjKind::Ignored, nullptr, nullptr, 0, false, 0, 0, 0, 0},
    {26, ObjKind::Ignored, nullptr, nullptr, 0, false, 0, 0, 0, 0}, {27, ObjKind::Ignored, nullptr, nullptr, 0, false, 0, 0, 0, 0},
    {28, ObjKind::Ignored, nullptr, nullptr, 0, false, 0, 0, 0, 0}, {32, ObjKind::Ignored, nullptr, nullptr, 0, false, 0, 0, 0, 0},
    {33, ObjKind::Ignored, nullptr, nullptr, 0, false, 0, 0, 0, 0},
};

} // namespace

const ObjInfo* objectInfo(int id) {
    static const ObjInfo* index[64] = {nullptr};
    static bool built = false;
    if (!built) {
        for (const ObjInfo& o : kTable)
            if (o.id >= 0 && o.id < 64) index[o.id] = &o;
        built = true;
    }
    return (id >= 0 && id < 64) ? index[id] : nullptr;
}

} // namespace ogd
