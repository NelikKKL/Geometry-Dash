#include "core/ObjectTable.h"

namespace ogd {
namespace {

// Z layers
constexpr int ZBack = 0, ZDeco = 1, ZPit = 2, ZDSpike = 3, ZBlock = 4, ZSpike = 5, ZPad = kObjZFront;

//                                 id  kind                      sprite                 back                     z       w   h  ox  oy
const ObjInfo kTable[] = {
    {1,  ObjKind::Solid,  "square_01_001.png", nullptr, ZBlock, 30, 30, 0, 0},
    {2,  ObjKind::Solid,  "square_02_001.png", nullptr, ZBlock, 30, 30, 0, 0},
    {3,  ObjKind::Solid,  "square_03_001.png", nullptr, ZBlock, 30, 30, 0, 0},
    {4,  ObjKind::Solid,  "square_04_001.png", nullptr, ZBlock, 30, 30, 0, 0},
    {5,  ObjKind::Solid,  "square_05_001.png", nullptr, ZBlock, 30, 30, 0, 0},
    {6,  ObjKind::Solid,  "square_06_001.png", nullptr, ZBlock, 30, 30, 0, 0},
    {7,  ObjKind::Solid,  "square_07_001.png", nullptr, ZBlock, 30, 30, 0, 0},
    // hazards: hitboxes are deliberately a bit smaller than the sprite (forgiving, like the original)
    {8,  ObjKind::Hazard, "spike_01_001.png",  nullptr, ZSpike, 8, 16, 0, -4},
    {39, ObjKind::Hazard, "spike_02_001.png",  nullptr, ZSpike, 8, 8, 0, -1},
    {40, ObjKind::Solid,  "plank_01_001.png",  nullptr, ZBlock, 30, 14, 0, 0},

    {10, ObjKind::PortalGravityDown, "portal_01_front_001.png", "portal_01_back_001.png", ZPad, 20, 84, 0, 0},
    {11, ObjKind::PortalGravityUp,   "portal_02_front_001.png", "portal_02_back_001.png", ZPad, 20, 84, 0, 0},
    {12, ObjKind::PortalCube,        "portal_03_front_001.png", "portal_03_back_001.png", ZPad, 20, 84, 0, 0},
    {13, ObjKind::PortalShip,        "portal_04_front_001.png", "portal_04_back_001.png", ZPad, 20, 84, 0, 0},

    {35, ObjKind::Pad, "bump_01_001.png", nullptr, ZPad, 26, 8, 0, 0},
    {36, ObjKind::Orb, "ring_01_001.png", nullptr, ZPad, 30, 30, 0, 0},

    // decoration
    {9,  ObjKind::Deco, "pit_01_001.png",      nullptr, ZPit,    0, 0, 0, 0},   // ground "thorns": decoration (a solid strip of them covers floors that must be run on)
    {16, ObjKind::Deco, "pit_02_001.png",      nullptr, ZPit,    0, 0, 0, 0},
    {17, ObjKind::Deco, "pit_03_001.png",      nullptr, ZPit,    0, 0, 0, 0},
    {18, ObjKind::Deco, "d_spikes_01_001.png", nullptr, ZDSpike, 0, 0, 0, 0},
    {19, ObjKind::Deco, "d_spikes_02_001.png", nullptr, ZDSpike, 0, 0, 0, 0},
    {20, ObjKind::Deco, "d_spikes_03_001.png", nullptr, ZDSpike, 0, 0, 0, 0},
    {21, ObjKind::Deco, "d_spikes_04_001.png", nullptr, ZDSpike, 0, 0, 0, 0},

    {29, ObjKind::TriggerBG,     nullptr, nullptr, ZBack, 0, 0, 0, 0},
    {30, ObjKind::TriggerGround, nullptr, nullptr, ZBack, 0, 0, 0, 0},

    // present in the data, no confident sprite match
    {15, ObjKind::Unmapped, nullptr, nullptr, ZDeco, 0, 0, 0, 0},
    {22, ObjKind::Unmapped, nullptr, nullptr, ZDeco, 0, 0, 0, 0},
    {23, ObjKind::Unmapped, nullptr, nullptr, ZDeco, 0, 0, 0, 0},
    {24, ObjKind::Unmapped, nullptr, nullptr, ZDeco, 0, 0, 0, 0},
    {26, ObjKind::Unmapped, nullptr, nullptr, ZDeco, 0, 0, 0, 0},
    {27, ObjKind::Unmapped, nullptr, nullptr, ZDeco, 0, 0, 0, 0},
    {32, ObjKind::Unmapped, nullptr, nullptr, ZDeco, 0, 0, 0, 0},
    {33, ObjKind::Unmapped, nullptr, nullptr, ZDeco, 0, 0, 0, 0},
    {41, ObjKind::Unmapped, nullptr, nullptr, ZDeco, 0, 0, 0, 0},
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
